/*
 * dht.c
 *
 *  Created on: Feb 10, 2015
 *      Author: Dennis Sitelew
 */


// Module for interfacing with a DHT temperature sensors family

// how many timing transitions we need to keep track of. 2 * number bits + extra
#define MAXTIMINGS 85

//#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"
#include "c_string.h"
#include "c_stdlib.h"

#define DHT11 11
#define DHT22 22
#define DHT21 21
#define AM2301 21

#define DIRECT_READ(pin)         (0x1 & GPIO_INPUT_GET(GPIO_ID_PIN(pin_num[pin])))
#define DIRECT_MODE_INPUT(pin)   GPIO_DIS_OUTPUT(pin_num[pin])
#define DIRECT_MODE_OUTPUT(pin)
#define DIRECT_WRITE_LOW(pin)    (GPIO_OUTPUT_SET(GPIO_ID_PIN(pin_num[pin]), 0))
#define DIRECT_WRITE_HIGH(pin)   (GPIO_OUTPUT_SET(GPIO_ID_PIN(pin_num[pin]), 1))

/**
 * An DHT Sensor object
 */
typedef struct dht_userdata {
   uint8_t m_Data[6];
   uint8_t m_Pin;
   uint8_t m_Type;
   uint8_t m_Count;
   uint8_t m_FirstReading;
   unsigned long m_LastReadTime;
} dht_userdata_t;

inline uint8_t reader(uint8_t pin, uint8_t value) {
   uint8_t c = 255;
   for (; c-- && platform_gpio_read(pin) != value; );
   return c;
}

/**
 * Read humidity value for the given sensor
 * @param pData a sensor object
 */
static uint8_t read(dht_userdata_t *pData) {
   uint8_t last_state = PLATFORM_GPIO_HIGH;
   uint8_t counter = 0;
   uint8_t j = 0, i;
   uint8_t pin = pData->m_Pin;
   uint8_t *data = pData->m_Data;
   uint8_t step = pData->m_Count;
   unsigned long current_time;

   // Check if sensor was read less than two seconds ago and return early to use last reading
   current_time = 0x7FFFFFFF & system_get_time();
   if (current_time < pData->m_LastReadTime) {
      // i.e. there was a rollover
      pData->m_LastReadTime = 0;
   }

   if (!pData->m_FirstReading && ((current_time - pData->m_LastReadTime) < 2000000)) {
      return true; // Return last correct measurement
   }

   pData->m_FirstReading = 0;
   pData->m_LastReadTime = current_time;
   c_memset(data, 0, sizeof(pData->m_Data));

   // Pull the pin high and wait 250 milliseconds
   DIRECT_MODE_INPUT(pin);
   DIRECT_WRITE_HIGH(pin);
   os_delay_us(250000);

   // Now pull it low for ~20 milliseconds
   DIRECT_MODE_OUTPUT(pin);
   DIRECT_WRITE_LOW(pin);
   os_delay_us(20000);
   os_intr_lock();
   DIRECT_WRITE_HIGH(pin);
   os_delay_us(40);
   DIRECT_MODE_INPUT(pin);

   // read in timings
   for (i=0; i< MAXTIMINGS; ++i) {
      counter = 0;
      while (DIRECT_READ(pin) == last_state) {
         ++counter;
         os_delay_us(1);
         if (counter == 255) {
            break;
         }
      }
      last_state = DIRECT_READ(pin);

      if (counter == 255) break;

      // ignore first 3 transitions
      if ((i >= 4) && (i % 2 == 0)) {
         // shove each bit into the storage bytes
         data[j / 8] <<= 1;
         if (counter > step)
            data[j / 8] |= 1;
         j++;
      }
   }

   os_intr_unlock();

   if ((j >= 40) && (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF))){
      return 1;
   }
   return 0;
}

/**
 * Read temperature value for the given sensor
 * @param pData a sensor object
 */
static bool read_temperature(dht_userdata_t *pData, float *result) {
   if (read(pData)) {
      switch (pData->m_Type) {
        case DHT11:
           *result = pData->m_Data[2];
           return true;

        case DHT22:
        case DHT21:
           *result = pData->m_Data[2] & 0x7F;
           *result *= 256;
           *result += pData->m_Data[3];
           *result /= 10;
           if (pData->m_Data[2] & 0x80)
              *result *= -1;
          return true;
      }
   }
   return false;
}

/**
 * Read humidity value for the given sensor
 * @param pData a sensor object
 */
static bool read_humidity(dht_userdata_t *pData, float *result) {
   if (read(pData)) {
      switch (pData->m_Type) {
         case DHT11:
            *result = pData->m_Data[0];
            return true;

         case DHT22:
         case DHT21:
            *result = pData->m_Data[0];
            *result *= 256;
            *result += pData->m_Data[1];
            *result /= 10;
            return true;
      }
   }
   return false;
}

/**
 * Setup a DHT sensor
 * @param   L   Lua Sate
 * @return      Number of return parameters on stack (always 1)
 * @example     Lua: dht = dht.setup(pin, type [, count])
 */
static int dht_setup(lua_State *L) {
   unsigned pin;
   unsigned type;
   unsigned count = 20;

   int result;
   dht_userdata_t *pData = NULL;

   NODE_DBG("dht_setup is called.\n");

   // Load and check parameters
   pin  = luaL_checkinteger(L, 1);
   type = luaL_checkinteger(L, 2);

   if (lua_isnumber(L, 3)) count = lua_tointeger(L, 3);

   MOD_CHECK_ID(gpio, pin);

   result = platform_gpio_mode(pin, PLATFORM_GPIO_INPUT, PLATFORM_GPIO_PULLUP);
   if (result < 0)
      return luaL_error(L, "Invalid DHT pin.");
   platform_gpio_write(pin, PLATFORM_GPIO_HIGH);

   if (type != DHT11 && type != DHT22 && type != DHT21 && type != AM2301)
      return luaL_error(L, "Invalid sensor type.");

   // create a object
   pData = (dht_userdata_t *)lua_newuserdata(L, sizeof(dht_userdata_t));
   if (!pData)
      return luaL_error(L, "Out of memory (dht struct).");

   // Initialize dht structure
   c_memset(pData, 0, sizeof(dht_userdata_t));
   pData->m_FirstReading = 1;
   pData->m_Pin = pin;
   pData->m_Count = count;
   pData->m_Type = type;

   // Set metatable
   luaL_getmetatable(L, "dht.dht");
   lua_setmetatable(L, -2);

   NODE_DBG("DHT: Init info: PIN=%d, CNT=%d, TYPE=%d\r\n", pData->m_Pin, pData->m_Count, pData->m_Type);
   return 1;
}

/**
 * Read dht sensor data
 * @param   L   Lua Sate
 * @return      Number of return parameters on stack (always 2 - temperature and humidity)
 * @example     Lua: = dht.read()
 */
static int dht_read(lua_State *L) {
   dht_userdata_t *pData = (dht_userdata_t *)luaL_checkudata(L, 1, "dht.dht");
   luaL_argcheck(L, pData, 1, "dht.dht expected");
   if (!pData) {
      NODE_DBG("userdata is nil.\n");
      return 0;
   }

   float t, h;
   if (!read_temperature(pData, &t) || !read_humidity(pData, &h)) {
      lua_pushnil(L);
      lua_pushnil(L);
      return 0;
   }

   lua_pushnumber(L, t);
   lua_pushnumber(L, h);
   return 2;
}

// Module function map
#define MIN_OPT_LEVEL   2
#include "lrodefs.h"

/**
 * DHT object functions
 */
static const LUA_REG_TYPE dht_obj_map[] = {
  { LSTRKEY("read"),          LFUNCVAL(dht_read) },
#if LUA_OPTIMIZE_MEMORY > 0
  { LSTRKEY("__index"),       LROVAL(dht_obj_map) },
#endif
  { LNILKEY, LNILVAL }
};

/**
 * DHT Namespace functions
 */
const LUA_REG_TYPE dht_map[] = {
  { LSTRKEY("setup"),       LFUNCVAL(dht_setup) },
#if LUA_OPTIMIZE_MEMORY > 0
  { LSTRKEY( "DHT11" ),     LNUMVAL( DHT11 ) },
  { LSTRKEY( "DHT22" ),     LNUMVAL( DHT22 ) },
  { LSTRKEY( "DHT21" ),     LNUMVAL( DHT21 ) },
  { LSTRKEY( "AM2301" ),    LNUMVAL( AM2301 ) },
  { LSTRKEY("__metatable"), LROVAL(dht_map) },
#endif
  { LNILKEY, LNILVAL }
};

/**
 * Initializer function
 * @param L Lua state to be initialized
 */
LUALIB_API int luaopen_dht(lua_State *L) {
#if LUA_OPTIMIZE_MEMORY > 0
   luaL_rometatable(L, "dht.dht", (void *)dht_obj_map);  // create metatable for dht.dht
   return 0;
#else // #if LUA_OPTIMIZE_MEMORY > 0
   luaL_register(L, AUXLIB_DHT, dht_map);

   // Set it as its own metatable
   lua_pushvalue(L, -1);
   lua_setmetatable(L, -2);

   // Module constants
   MOD_REG_NUMBER( L, "DHT11", DHT11 );
   MOD_REG_NUMBER( L, "DHT22", DHT22 );
   MOD_REG_NUMBER( L, "DHT21", DHT21 );
   MOD_REG_NUMBER( L, "AM2301", AM2301 );

   // create metatable
   luaL_newmetatable(L, "dht.dht");
   // metatable.__index = metatable
   lua_pushliteral(L, "__index");
   lua_pushvalue(L,-2);
   lua_rawset(L,-3);
   // Setup the methods inside metatable
   luaL_register(L, NULL, dht_obj_map);

   return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0
}

