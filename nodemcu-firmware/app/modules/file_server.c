/*
 * file_server.c
 *
 * Module for starting a file server - a server that receives
 * scripts, saves and compiles them and also executes small Lua commands
 *
 *  Created on: Feb 25, 2015
 *      Author: Dennis Sitelew
 */

#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"
#include "c_string.h"
#include "c_stdlib.h"
#include "espconn.h"
#include "flash_fs.h"

#define FS_OBJECT           "file_server.fs"
#define MAX_BUFFER_SIZE     64
#define LOCAL_ADDRESS       "0.0.0.0"
#define FS_INVALID_FILE     (FS_OPEN_OK - 1)
#define min(a,b)            ((a)<(b)?(a):(b))

extern lua_Load gLoad;              //!< External Lua interpreter buffer (used in terminal input)
extern void dojob(lua_Load *load);  //!< External processing function (used in terminal input)
extern spiffs fs;                   //!< SPI File system reference

/**
 * File server state enumeration
 */
typedef enum fs_state {
   NO_COMMAND = 1,  //!< No command received
   NO_SIZE    = 2,  //!< Command received but data size is yet unknown
   NO_DATA    = 3,  //!< Command and size are known, waiting for data
} fs_state_t;

/**
 * File server commands enumeration
 */
typedef enum fs_command {
   IDLE     = '-',  //!< Waiting for the next command
   OPEN     = 'O',  //!< File open command received waiting for data
   WRITE    = 'W',  //!< Writing data
   CLOSE    = 'C',  //!< File close command received
   RUN      = 'G'   //!< Execute a script command received
} fs_command_t;


/**
 * A file server object
 */
typedef struct fs_userdata {
   struct espconn   *m_pConnection;     //!< ESP Connection
   char m_DataBuffer[MAX_BUFFER_SIZE];  //!< Received data buffer
   unsigned         m_DataLength;       //!< Actual size of the received data
   unsigned         m_ReadPoint;        //!< Read point within the data buffer
   unsigned         m_Payload;          //!< Bytes to be received to complete a packet
   fs_state_t       m_State;            //!< Current state of the file server
   fs_command_t     m_Command;          //!< Current command
   struct espconn   *m_pClient;         //!< Client connection socket (only one is allowed)
} fs_userdata_t;

static fs_userdata_t *g_pServer = NULL;
static volatile int file_fd = FS_OPEN_OK - 1;
/**
 * Client disconnected callback.
 * @param pClient   ESP Connection
 */
static void fs_client_disconnected(struct espconn *pClient) {
   // c_printf("fs_client_disconnected\n");

   if (!pClient)
      return;

   fs_userdata_t *pServer = (fs_userdata_t *)pClient->reverse;
   if (!pServer)
      return;

   pClient->reverse = NULL;

   pServer->m_pClient = NULL;
   pServer->m_DataLength = 0;
   pServer->m_ReadPoint = 0;
   pServer->m_Payload = 0;
   pServer->m_State = NO_COMMAND;
   pServer->m_Command = IDLE;

   if (file_fd != FS_INVALID_FILE) {
      fs_flush(file_fd);
      fs_close(file_fd);
      file_fd = FS_INVALID_FILE;
   }
}

/**
 * Client reconnection callback
 * @param pClient   ESP Connection
 * @param err       Error (currently undocumented)
 */
static void fs_server_reconnected(struct espconn *pClient, sint8_t err) {
   // c_printf("fs_server_reconnected\n");
   fs_client_disconnected(pClient);
}

static mem_move(char *pDest, char *pSource, size_t length) {
   size_t i = 0;
   for (; i != length; ++i) {
      pDest[i] = pSource[i];
   }
}

static size_t atoi(char *pStr, size_t lenght) {
   size_t result = 0, i;
   unsigned short digit;

   for (i = 0; i != lenght; ++i) {
      if (pStr[i] >= '0' && pStr[i] <= '9')
          digit = pStr[i] - '0';
      else
         return 0;

      result *= 10;
      result += digit;
   }
   return result;
}

/**
 * Data received callback.
 * @param pClient   ESP Client
 * @param pData     Received data
 * @param len       Received data length
 */
static void fs_data_received(struct espconn *pClient, char *pData, unsigned short len){
   // c_printf("fs_data_received\n");
   if (!pClient)
      return;

   fs_userdata_t *pServer = (fs_userdata_t *)pClient->reverse;
   if (!pServer)
      return;

   // Close connection if not enough buffer space
   size_t available = MAX_BUFFER_SIZE - pServer->m_DataLength;
   size_t copied = min(len, available);
   size_t i = 0, index;

   if (copied == 0) {
      espconn_disconnect(pClient);
      return;
   }

   // Copy as much data as possible into internal buffer and adjust sizes and pointers
   c_memcpy(pServer->m_DataBuffer + pServer->m_DataLength, pData, copied);
   pServer->m_DataLength += copied;
   pData += copied;
   len -= copied;

   // Ready to get a new command
   if (pServer->m_State == NO_COMMAND){
      if (pServer->m_DataLength >= 2){
         pServer->m_ReadPoint = 2;
         pServer->m_Command = IDLE;

         switch (pServer->m_DataBuffer[0]) {
            case OPEN:  pServer->m_Command = OPEN;  break;
            case WRITE: pServer->m_Command = WRITE; break;
            case CLOSE: pServer->m_Command = CLOSE; break;
            case RUN:   pServer->m_Command = RUN;   break;
         }

         if (pServer->m_Command != IDLE)
            pServer->m_State = NO_SIZE;
      }
      else {
         return;
      }
   }

   // Try to get data size
   if (pServer->m_State == NO_SIZE) {
      index = -1;
      for (i = pServer->m_ReadPoint; i != pServer->m_DataLength; ++i) {
         if (pServer->m_DataBuffer[i] == '|') {
            index = i;
            break;
         }
      }

      if (index != -1) {
         pServer->m_Payload = atoi(pServer->m_DataBuffer + pServer->m_ReadPoint, index - pServer->m_ReadPoint);
         pServer->m_ReadPoint = index + 1;
         pServer->m_State = NO_DATA;
      }
      else {
         // No size data available - return
         return;
      }
   }

   // We have both size and command - get data
   if (pServer->m_DataLength - pServer->m_ReadPoint >= pServer->m_Payload){
      // Make a NULL-terminated string inside of the buffer
      uint16 packet_len = pServer->m_ReadPoint + pServer->m_Payload;
      char backup = pServer->m_DataBuffer[packet_len];
      pServer->m_DataBuffer[packet_len] = 0;

      char *pPayload = pServer->m_DataBuffer + pServer->m_ReadPoint;
      size_t payload_len = strlen(pPayload);
      bool success = false;

      switch (pServer->m_Command) {
         case OPEN: {
            if (file_fd != FS_INVALID_FILE) {
               fs_flush(file_fd);
               fs_close(file_fd);

               file_fd = FS_INVALID_FILE;
            }

            if (payload_len <= FS_NAME_MAX_LENGTH) {
               file_fd = fs_open(pPayload, fs_mode2flag("w"));
               fs_close(file_fd);
               SPIFFS_remove(&fs, (char *)pPayload);

               file_fd = fs_open(pPayload, fs_mode2flag("w+"));
               if (file_fd < FS_OPEN_OK){
                  file_fd = FS_INVALID_FILE;
               }
               else {
                  success = true;
               }
            }
         }
         break;

         case WRITE: {
            if (file_fd != FS_INVALID_FILE) {
               success = (fs_write(file_fd, pPayload, payload_len) == payload_len);
            }
         }
         break;

         case CLOSE: {
            if (file_fd != FS_INVALID_FILE) {
               size_t i = 0;
               for (; i < 10 && !success; ++i) {
                  success = (fs_flush(file_fd) == 0);
               }
               fs_close(file_fd);

               file_fd = FS_INVALID_FILE;
            }
         }
         break;

         case RUN: {
            lua_Load *load = &gLoad;
            if (load->line_position == 0){
               // c_printf("executing: %s\n", pPayload);
               c_memcpy(load->line, pPayload, payload_len);
               load->line[payload_len + 1] = 0;
               load->line_position = c_strlen(load->line) + 1;
               load->done = 1;

               dojob(load);

               success = true;
            }
         }
         break;
      }

      // Restore NULL-terminator and send confirmation or failure message
      pServer->m_DataBuffer[packet_len] = backup;
      espconn_sent(pClient, (unsigned char *)(success ? "Y|" : "N|"), 2);
      espconn_sent(pClient, (unsigned char *)pServer->m_DataBuffer, packet_len);

      if (!success) {
         espconn_disconnect(pClient);
      }

      // Still have data in buffer
      pServer->m_DataLength = pServer->m_DataLength - packet_len;
      if (pServer->m_DataLength > 0) {
         mem_move(pServer->m_DataBuffer,
                  pServer->m_DataBuffer + packet_len,
                  pServer->m_DataLength);
      }

      // Reset state machine
      pServer->m_Payload = 0;
      pServer->m_ReadPoint = 0;
      pServer->m_Command = IDLE;
      pServer->m_State = NO_COMMAND;
   }

   // Call this function again if still have unprocessed data
   if (len) {
      fs_data_received(pClient, pData, len);
   }
}

/**
 * Client connected callback.
 * @param pClient   ESP Connection
 */
static void fs_client_connected(struct espconn *pClient) {
   // c_printf("fs_client_connected\n");
   if (!pClient)
      return;

   // Only single client allowed
   if (g_pServer->m_pClient != NULL) {
      NODE_ERR("MAX_CONNECT\n");
      pClient->reverse = NULL;   // Do not accept this connection
      if (pClient->proto.tcp->remote_port || pClient->proto.tcp->local_port)
         espconn_disconnect(pClient);
      return;
   }

   g_pServer->m_pClient = pClient;
   pClient->reverse = g_pServer;

   espconn_regist_recvcb  (pClient, (espconn_recv_callback)fs_data_received);
   espconn_regist_disconcb(pClient, (espconn_connect_callback)fs_client_disconnected);
   espconn_regist_reconcb (pClient, (espconn_reconnect_callback)fs_server_reconnected);
}

/**
 * Setup a file server
 * @param   L   Lua Sate
 * @return      Number of return parameters on stack (always 1)
 * @example     Lua: fs = file_server.start(port, [client_timeout = 10])
 */
static int file_server_start(lua_State *L) {
   // c_printf("file_server_start\n");
   uint16_t port = 20123;
   uint16_t timeout = 30;
   struct espconn *pConnection = NULL;
   ip_addr_t ipaddr;

   // Load and check parameters
   port = luaL_checkinteger(L, 1);
   if (lua_isnumber(L, 3)) {
      timeout = (uint16_t)lua_tointeger(L, 2);
      if (timeout  < 1 || timeout  > 28800){
         return luaL_error(L, "wrong arg type");
      }
   }

   // Create an object
   g_pServer = (fs_userdata_t *)lua_newuserdata(L, sizeof(fs_userdata_t));
   if (!g_pServer)
      return luaL_error(L, "not enough memory");

   // Initialize file server structure
   g_pServer->m_pConnection = pConnection = (struct espconn *)c_zalloc(sizeof(struct espconn));
   if (!pConnection)
      return luaL_error(L, "not enough memory");

   g_pServer->m_DataLength = 0;
   g_pServer->m_ReadPoint = 0;
   g_pServer->m_Payload = 0;
   g_pServer->m_State = NO_COMMAND;
   g_pServer->m_Command = IDLE;
   g_pServer->m_pClient = NULL;

   pConnection->proto.tcp = NULL;
   pConnection->reverse = NULL;

   pConnection->proto.tcp = (esp_tcp *)c_zalloc(sizeof(esp_tcp));
   if (!pConnection->proto.tcp){
      c_free(pConnection);
      pConnection = g_pServer->m_pConnection = NULL;
      return luaL_error(L, "not enough memory");
   }

   pConnection->type = ESPCONN_TCP;
   pConnection->state = ESPCONN_NONE;
   pConnection->reverse = g_pServer; // For the callback function
   pConnection->proto.tcp->local_port = port;

   ipaddr.addr = ipaddr_addr(LOCAL_ADDRESS);
   c_memcpy(pConnection->proto.tcp->local_ip, &ipaddr.addr, sizeof(ipaddr.addr));

   espconn_regist_connectcb(pConnection, (espconn_connect_callback)fs_client_connected);
   espconn_accept(pConnection);
   espconn_regist_time(pConnection, timeout, 0);

   // Assign metatable to the file server object
   luaL_getmetatable(L, FS_OBJECT);
   lua_setmetatable(L, -2);
   return 1;
}

/**
 * Stops the file server and destroys all associated data
 * @param pData file server to be destroyed
 */
static void stop(fs_userdata_t *pData){
   // c_printf("stop\n");
   if (pData->m_pConnection) {
      if (pData->m_pConnection->proto.tcp) {
         c_free(pData->m_pConnection->proto.tcp);
         pData->m_pConnection->proto.tcp = NULL;
      }
   }

   g_pServer = NULL;
}

/**
 * Stop a file server
 *
 * Explicitly stops the given file server. Same as setting the file server's
 * variable to nil
 *
 * @param   L   Lua Sate
 * @return      Number of return parameters on stack (always 0)
 * @example     Lua: = fs.stop()
 */
static int file_server_stop(lua_State *L) {
   // c_printf("file_server_stop\n");
   fs_userdata_t *pData = (fs_userdata_t *)luaL_checkudata(L, 1, FS_OBJECT);
   luaL_argcheck(L, pData, 1, FS_OBJECT" expected");
   if (!pData) {
      NODE_DBG("userdata is nil.\n");
      return 0;
   }

   stop(pData);

   return 0;
}

// Module function map
#define MIN_OPT_LEVEL   2
#include "lrodefs.h"

/**
 * file_server object functions
 */
static const LUA_REG_TYPE file_server_obj_map[] = {
  { LSTRKEY("stop"),          LFUNCVAL(file_server_stop) },
  { LSTRKEY("__gc"),          LFUNCVAL(file_server_stop) },
#if LUA_OPTIMIZE_MEMORY > 0
  { LSTRKEY("__index"),       LROVAL(file_server_obj_map) },
#endif
  { LNILKEY, LNILVAL }
};

/**
 * file_server namespace functions
 */
const LUA_REG_TYPE file_server_map[] = {
  { LSTRKEY("start"),       LFUNCVAL(file_server_start) },
#if LUA_OPTIMIZE_MEMORY > 0
  { LSTRKEY("__metatable"), LROVAL(file_server_map) },
#endif
  { LNILKEY, LNILVAL }
};

/**
 * Initializer function
 * @param L Lua state to be initialized
 */
LUALIB_API int luaopen_file_server(lua_State *L) {
#if LUA_OPTIMIZE_MEMORY > 0
   luaL_rometatable(L, FS_OBJECT, (void *)file_server_obj_map);
   return 0;
#else // #if LUA_OPTIMIZE_MEMORY > 0
   luaL_register(L, AUXLIB_FILE_SERVER, file_server_map);

   // Set it as its own metatable
   lua_pushvalue(L, -1);
   lua_setmetatable(L, -2);

   // create metatable
   luaL_newmetatable(L, FS_OBJECT);
   // metatable.__index = metatable
   lua_pushliteral(L, "__index");
   lua_pushvalue(L,-2);
   lua_rawset(L,-3);
   // Setup the methods inside metatable
   luaL_register(L, NULL, file_server_obj_map);

   return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0
}
