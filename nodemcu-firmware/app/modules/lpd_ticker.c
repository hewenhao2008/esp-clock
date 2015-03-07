/*
 * lpd_ticker.c
 *
 *  Created on: Feb 21, 2015
 *      Author: yowidin
 */

#include "lualib.h"
#include "lauxlib.h"
#include "lpd8806.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"
#include "c_string.h"
#include "c_stdlib.h"

#define GET_LINE_0(x) ((x) >> 20) & 0xF
#define GET_LINE_1(x) ((x) >> 16) & 0xF
#define GET_LINE_2(x) ((x) >> 12) & 0xF
#define GET_LINE_3(x) ((x) >>  8) & 0xF
#define GET_LINE_4(x) ((x) >>  4) & 0xF
#define GET_LINE_5(x) ((x)      ) & 0xF

//! Number of rows in ticker
#define NUM_ROWS    6

//! Number of columns in ticker
#define NUM_COLS    10

//! Global Lua state - used to handle callback functions
static lua_State *g_pLua = NULL;

/**
 * Font ASCII table.
 * Each entry represents a single 4x6 character (4 bits per line)
 */
uint32_t ascii_table[] = {
      0,        // 0x00
      0,        // 0x01
      0,        // 0x02
      0,        // 0x03
      0,        // 0x04
      0,        // 0x05
      0,        // 0x06
      0,        // 0x07
      0,        // 0x08
      0,        // 0x09
      0,        // 0x0A
      0,        // 0x0B
      0,        // 0x0C
      0,        // 0x0D
      0,        // 0x0E
      0,        // 0x0F
      0,        // 0x10
      0,        // 0x11
      0,        // 0x12
      0,        // 0x13
      0,        // 0x14
      0,        // 0x15
      0,        // 0x16
      0,        // 0x17
      0,        // 0x18
      0,        // 0x19
      0,        // 0x1A
      0,        // 0x1B
      0,        // 0x1C
      0,        // 0x1D
      0,        // 0x1E
      0,        // 0x1F
      0,        // 0x20
      2236448,  // 0x21 !
      5570560,  // 0x22 "
      5723984,  // 0x23 #
      2319202,  // 0x24 $
      4269072,  // 0x25 %
      2438512,  // 0x26 &
      6553600,  // 0x27 '
      2376736,  // 0x28 (
      4334144,  // 0x29 )
      5403216,  // 0x2A *
      160256,   // 0x2B +
      50,       // 0x2C ,
      28672,    // 0x2D -
      32,       // 0x2E .
      1123392,  // 0x2F /
      3495264,  // 0x30 0
      2499184,  // 0x31 1
      6366320,  // 0x32 2
      6365536,  // 0x33 3
      1405200,  // 0x34 4
      7627104,  // 0x35 5
      2385184,  // 0x36 6
      7414304,  // 0x37 7
      2434336,  // 0x38 8
      2437408,  // 0x39 9
      8224,     // 0x3A :
      8292,     // 0x3B ;
      1196560,  // 0x3C <
      28784,    // 0x3D =
      4330048,  // 0x3E >
      6365216,  // 0x3F ?
      7689328,  // 0x40 @
      2454864,  // 0x41 A
      6645088,  // 0x42 B
      3425328,  // 0x43 C
      6640992,  // 0x44 D
      7627888,  // 0x45 E
      7627840,  // 0x46 F
      3429680,  // 0x47 G
      5600592,  // 0x48 H
      7479920,  // 0x49 I
      1119520,  // 0x4A J
      5596496,  // 0x4B K
      4473968,  // 0x4C L
      5731664,  // 0x4D M
      5723472,  // 0x4E N
      2446624,  // 0x4F O
      6644800,  // 0x50 P
      2446640,  // 0x51 Q
      6645072,  // 0x52 R
      3436896,  // 0x53 S
      7479840,  // 0x54 T
      5592432,  // 0x55 U
      5592352,  // 0x56 V
      5601104,  // 0x57 W
      5580112,  // 0x58 X
      5579296,  // 0x59 Y
      7414896,  // 0x5A Z
      6571104,  // 0x5B [
      4464912,  // 0x5C \ .
      6431328,  // 0x5D ]
      2424832,  // 0x5E ^
      15,       // 0x5F _
      6422528,  // 0x60 `
      13680,    // 0x61 a
      4482400,  // 0x62 b
      13360,    // 0x63 c
      1127728,  // 0x64 d
      30256,    // 0x65 e
      1208864,  // 0x66 f
      29975,    // 0x67 g
      4482384,  // 0x68 h
      2105888,  // 0x69 i
      2105894,  // 0x6A j
      4478544,  // 0x6B k
      2236960,  // 0x6C l
      30544,    // 0x6D m
      25936,    // 0x6E n
      9504,     // 0x6F o
      25956,    // 0x70 p
      13617,    // 0x71 q
      25664,    // 0x72 r
      12896,    // 0x73 s
      160304,   // 0x74 t
      21872,    // 0x75 u
      21792,    // 0x76 v
      22384,    // 0x77 w
      21072,    // 0x78 x
      21796,    // 0x79 y
      25136,    // 0x7A z
      3301936,  // 0x7B {
      2236960,  // 0x7C |
      6435424,  // 0x7D }
      5898240,  // 0x7E ~
};

/**
 * Maps LED "snake" shape into matrix with upper
 * left [0; 0] corner
 */
const uint8_t led_mapping[] = {
    50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
    49, 48, 47, 46, 45, 44, 43, 42, 41, 40,
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    29, 28, 27, 26, 25, 24, 23, 22, 21, 20,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
     9,  8,  7,  6,  5,  4,  3,  2,  1,  0
};


/**
 * Ticker-related data
 */
typedef struct ticker {
   lpd_userdata *m_LedStrip;    //!< LED Strip associated with the ticker
   int           m_StripRef;    //!< LED Strip reference (for lua counter)
   unsigned m_Speed;            //!< Ticker scrolling speed (scroll interval in ms)
   unsigned m_Length;           //!< Ticker character buffer length (in columns * 3)
   unsigned m_Used;             //!< Occupied buffer length (in columns * 3)
   ETSTimer m_ScrollTimer;      //!< Ticker scroll timer
   uint8_t *m_OutputBuffer;     //!< Cached ticker output
   unsigned m_Position;         //!< Current position
   double   m_Brightness;       //!< Ticker brightness [0:1]
   int      m_ScrollCallback;   //!< Scroll completion callback
} ticker_t;

static void add_letter(ticker_t *pTicker, uint32_t mask, unsigned r, unsigned g, unsigned b) {
   static uint8_t lines[6];
   lines[0] = GET_LINE_0(mask);
   lines[1] = GET_LINE_1(mask);
   lines[2] = GET_LINE_2(mask);
   lines[3] = GET_LINE_3(mask);
   lines[4] = GET_LINE_4(mask);
   lines[5] = GET_LINE_5(mask);

   unsigned column, row, value, start;
   for (row = 0; row < 6; ++row){
      start = pTicker->m_Used + pTicker->m_Length * row;
      for (column = 0; column < 4; ++column) {
         value = (lines[row] >> (3 - column)) & 0x1;
         pTicker->m_OutputBuffer[start + column * 3 + 0] = value * r;
         pTicker->m_OutputBuffer[start + column * 3 + 1] = value * g;
         pTicker->m_OutputBuffer[start + column * 3 + 2] = value * b;
      }
   }
   pTicker->m_Used += 12;
}

/**
 * Forward function declaration
 * @copydoc ticker_scroll_cb
 */
static void ICACHE_FLASH_ATTR ticker_scroll_cb(ticker_t *pTicker);

/**
 * Clear ticker's text buffer
 * @param   L   Lua state
 */
static int ticker_clear(lua_State *L) {
   ticker_t *pTicker = (ticker_t *)luaL_checkudata(L, 1, "ticker.tbl");
   luaL_argcheck(L, pTicker, 1, "ticker.tbl expected");
   if(!pTicker) {
      NODE_DBG("userdata is nil.\n");
      return 0;
   }

   os_timer_disarm(&pTicker->m_ScrollTimer);

   pTicker->m_Used = 0;
   pTicker->m_Position = 0;

   clear(pTicker->m_LedStrip, 0, 0, 0);
   update(pTicker->m_LedStrip);

   // Add some empty space at te start
   unsigned column, row, start;
   for (row = 0; row < 6; ++row){
      start = pTicker->m_Length * row;
      for (column = 0; column < 10; ++column) {
         pTicker->m_OutputBuffer[start + column * 3 + 0] = 0;
         pTicker->m_OutputBuffer[start + column * 3 + 1] = 0;
         pTicker->m_OutputBuffer[start + column * 3 + 2] = 0;
      }
   }
   pTicker->m_Used += 30;

   os_timer_setfn(&pTicker->m_ScrollTimer, ticker_scroll_cb, pTicker);
   os_timer_arm(&pTicker->m_ScrollTimer, pTicker->m_Speed, 1);

   return 0;
}


/**
 * Set ticker brightness
 * @param   L   Lua state
 * @example     Lua: t:set_brightness(0.5)
 */
static int ticker_set_brightness(lua_State *L){
   ticker_t *pTicker = (ticker_t *)luaL_checkudata(L, 1, "ticker.tbl");
   luaL_argcheck(L, pTicker, 1, "ticker.tbl expected");
   if(!pTicker) {
      NODE_DBG("userdata is nil.\n");
      return 0;
   }

   pTicker->m_Brightness = lua_tonumber(L, 2);

   return 0;
}

/**
 * Concatenate a string with  current ticker's text
 * @param   L   Lua state
 * @example     Lua: t:add_text("Hello", 255, 0, 0)
 * @example     Lua: t:add_text({72, 101, 108, 108, 111}, 255, 0, 0)
 */
static int ticker_add_text(lua_State *L) {
   ticker_t *pTicker = (ticker_t *)luaL_checkudata(L, 1, "ticker.tbl");
   luaL_argcheck(L, pTicker, 1, "ticker.tbl expected");
   if(!pTicker) {
      NODE_DBG("userdata is nil.\n");
      return 0;
   }

   unsigned r, g, b, len, i, letter;
   r = luaL_checkinteger(L, 3);
   g = luaL_checkinteger(L, 4);
   b = luaL_checkinteger(L, 5);

   if (lua_isstring(L, 2)) {
      const char *str = lua_tostring(L, 2);
      len = lua_strlen(L, 2);
      for (i = 0; i < len; ++i) {
         if (pTicker->m_Length - pTicker->m_Used < 12)
            return luaL_error(L, "Ticker buffer is full.");

         add_letter(pTicker, ascii_table[str[i]], r, g, b);
      }
   }
   else
   if (lua_istable(L, 2)) {
      len = lua_objlen(L, 2);
      for (i = 0; i < len; ++i) {
         if (pTicker->m_Length - pTicker->m_Used < 12)
            return luaL_error(L, "Ticker buffer is full.");

         lua_rawgeti(L, 2, i + 1);
         letter = luaL_checkinteger(L, -1) % (sizeof(ascii_table) / sizeof(ascii_table[0]));
         lua_pop(L, 1);

         add_letter(pTicker, ascii_table[letter], r, g, b);
      }
   }
   else
      return luaL_error(L, "String or table argument expected.");

   return 0;
}

/**
 * Set a numeric mask for a given character.
 * Doesn't update text in buffer.
 * Since Lua does not support 32 bit integers we have to work
 * with 6 rows.
 * @param   L   Lua state
 * @example     Lua: t:set_char_mask("a", {0, 7, 5, 3, 0, 0})
 * @example     Lua: t:set_char_mask(97,  {0, 7, 5, 3, 0, 0})
 */
static int ticker_set_char_mask(lua_State *L) {
   uint32_t mask = 0;
   unsigned index = 0, i, n;

   ticker_t *pTicker = (ticker_t *)luaL_checkudata(L, 1, "ticker.tbl");
   luaL_argcheck(L, pTicker, 1, "ticker.tbl expected");
   if(!pTicker) {
      NODE_DBG("userdata is nil.\n");
      return 0;
   }

   if (lua_isstring(L, 2)) {
      index = lua_tostring(L, 2)[0];
   }
   else
   if (lua_isnumber(L, 2)) {
      index = lua_tointeger(L, 2) % (sizeof(ascii_table) / sizeof(ascii_table[0]));
   }
   else
      return luaL_error(L, "String or numeric argument expected.");

   if (!lua_istable(L, 3) || lua_objlen(L, 3) != 6)
      return luaL_error(L, "Table with 6 rows exactly expected.");

   // Compose the mask
   for (i = 0; i < 6; ++i) {
      // Index
      lua_rawgeti(L, 3, i + 1);
      mask = mask | (luaL_checkinteger(L, -1) << (4 * i));
      lua_pop(L, 1);
   }

   ascii_table[index] = mask;
   return 0;
}

/**
 * Set ticker text (clear previous text as well)
 * @param   L   Lua state
 * @example     Lua: t:set_text("Hello", 255, 0, 0)
 */
static int ticker_set_text(lua_State *L) {
   ticker_clear(L);
   ticker_add_text(L);

   return 0;
}

/**
 * Add a single character to the ticker buffer
 *
 * @param   L   Lua Sate
 * @return      Number of return parameters on stack (always 0)
 * @example     Lua: = t.add_letter("a", 255, 0, 0)
 * @example     Lua: = t.add_letter(97, 255, 0, 0)
 */
static int ticker_add_letter(lua_State *L) {
   uint32_t mask = 0;

   ticker_t *pTicker = (ticker_t *)luaL_checkudata(L, 1, "ticker.tbl");
   luaL_argcheck(L, pTicker, 1, "ticker.tbl expected");
   if(!pTicker) {
      NODE_DBG("userdata is nil.\n");
      return 0;
   }

   if (pTicker->m_Length - pTicker->m_Used < 12)
      return luaL_error(L, "Ticker buffer is full.");

   if (lua_isstring(L, 2)) {
      mask = ascii_table[lua_tostring(L, 2)[0]];
   }
   else
   if (lua_isnumber(L, 2)) {
      mask = lua_tointeger(L, 2) % (sizeof(ascii_table) / sizeof(ascii_table[0]));
      mask = ascii_table[mask];
   }
   else
      return luaL_error(L, "String or numeric argument expected.");

   unsigned r, g, b;
   r = luaL_checkinteger(L, 3);
   g = luaL_checkinteger(L, 4);
   b = luaL_checkinteger(L, 5);

   add_letter(pTicker, mask, r, g, b);

   return 0;
}

/**
 * Ticker callback function.
 * Updates current ticker position
 * @param   pTicker   Ticker to be updated
 */
static void ICACHE_FLASH_ATTR ticker_scroll_cb(ticker_t *pTicker) {
   if (!pTicker->m_Used) return;

   unsigned column, row, r, g, b, index;
   for (row = 0; row < 6; ++row) {
      for (column = 0; column < 10; ++column) {
         r = pTicker->m_OutputBuffer[(((pTicker->m_Position + column) * 3 + 0) % pTicker->m_Used) + row * pTicker->m_Length];
         g = pTicker->m_OutputBuffer[(((pTicker->m_Position + column) * 3 + 1) % pTicker->m_Used) + row * pTicker->m_Length];
         b = pTicker->m_OutputBuffer[(((pTicker->m_Position + column) * 3 + 2) % pTicker->m_Used) + row * pTicker->m_Length];

         r = ((double)r * pTicker->m_Brightness);
         g = ((double)g * pTicker->m_Brightness);
         b = ((double)b * pTicker->m_Brightness);

         set_color(pTicker->m_LedStrip, led_mapping[column + row * 10], r, g, b);
      }
   }

   update(pTicker->m_LedStrip);

   // Scroll finish detected
   if ((pTicker->m_Position * 3) == pTicker->m_Used) {
      pTicker->m_Position = 0;

      if(g_pLua && pTicker->m_ScrollCallback != LUA_NOREF){
         lua_rawgeti(g_pLua, LUA_REGISTRYINDEX, pTicker->m_ScrollCallback);
         lua_call(g_pLua, 0, 0);
      }
   }

   ++pTicker->m_Position;
}

/**
 * Update the ticker scrolling speed.
 * @param   L   Lua state
 * @example     Lua: t:set_speed(speed)
 */
static int ticker_set_speed(lua_State *L) {
   ticker_t *pTicker = (ticker_t *)luaL_checkudata(L, 1, "ticker.tbl");
   luaL_argcheck(L, pTicker, 1, "ticker.tbl expected");
   if(!pTicker) {
      NODE_DBG("userdata is nil.\n");
      return 0;
   }

   pTicker->m_Speed = luaL_checkinteger(L, 2);

   os_timer_disarm(&pTicker->m_ScrollTimer);
   os_timer_setfn(&pTicker->m_ScrollTimer, ticker_scroll_cb, pTicker);
   os_timer_arm(&pTicker->m_ScrollTimer, pTicker->m_Speed, 1);

   return 0;
}

/**
 * Start the ticker timer.
 * i.e. start scrolling
 * @param   L   Lua state
 * @example     Lua: t:start()
 */
static int ticker_start(lua_State *L) {
   ticker_t *pTicker = (ticker_t *)luaL_checkudata(L, 1, "ticker.tbl");
   luaL_argcheck(L, pTicker, 1, "ticker.tbl expected");
   if(!pTicker) {
      NODE_DBG("userdata is nil.\n");
      return 0;
   }

   os_timer_setfn(&pTicker->m_ScrollTimer, ticker_scroll_cb, pTicker);
   os_timer_arm(&pTicker->m_ScrollTimer, pTicker->m_Speed, 1);

   return 0;
}

/**
 * Stop the ticker timer.
 * i.e. stop all scrolling
 * @param   L   Lua state
 * @example     Lua: t:stop()
 */
static int ticker_stop(lua_State *L) {
   ticker_t *pTicker = (ticker_t *)luaL_checkudata(L, 1, "ticker.tbl");
   luaL_argcheck(L, pTicker, 1, "ticker.tbl expected");
   if(!pTicker) {
      NODE_DBG("userdata is nil.\n");
      return 0;
   }

   os_timer_disarm(&pTicker->m_ScrollTimer);
   return 0;
}

/**
 * Setup a LED ticker
 * @param   L   Lua Sate
 * @return      Number of return parameters on stack (always 1)
 * @example     Lua: t = ticker.setup(lpd [scroll_speed, length, scroll_callback])
 */
static int ticker_setup(lua_State *L) {
   lpd_userdata *pStrip = NULL;
   ticker_t *pTicker = NULL;
   unsigned speed  = 100;
   unsigned length = 60;

   NODE_DBG("ticker_setup is called.\n");

   pStrip = (lpd_userdata *)luaL_checkudata(L, 1, "lpd8806.lpd");
   luaL_argcheck(L, pStrip, 1, "lpd8806.lpd expected");
   if(!pStrip) {
      NODE_DBG("userdata is nil.\n");
      return 0;
   }

   if (lua_isnumber(L, 2)) speed  = lua_tointeger(L, 2);
   if (lua_isnumber(L, 3)) length = lua_tointeger(L, 3);

   // Create an object
   pTicker = (ticker_t *)lua_newuserdata(L, sizeof(ticker_t));
   if (!pTicker) return luaL_error(L, "Out of memory (Ticker struct).");

   // Initialize ticker structure
   pTicker->m_LedStrip      = pStrip;
   pTicker->m_Speed         = speed;
   pTicker->m_Length        = length * 12;
   pTicker->m_OutputBuffer  = NULL;
   pTicker->m_Used          = 0;
   pTicker->m_Position      = 0;
   pTicker->m_Brightness    = 1.0f;
   pTicker->m_StripRef      = LUA_NOREF;
   pTicker->m_ScrollCallback= LUA_NOREF;

   size_t size = sizeof(uint8_t) * pTicker->m_Length * NUM_ROWS;

   pTicker->m_OutputBuffer = c_zalloc(size);
   if (!pTicker->m_OutputBuffer)
      return luaL_error(L, "Out of memory (data).");

   c_memset(pTicker->m_OutputBuffer, 0, size);

   // ticker_t in now on top of the stack
   // We need a reference to the strip - its in the second slot
   lua_pushvalue(L, 1);
   pTicker->m_StripRef = luaL_ref(L, LUA_REGISTRYINDEX);

   // Handle optional scroll finish callback
   if (lua_type(L, 4) == LUA_TFUNCTION || lua_type(L, 4) == LUA_TLIGHTFUNCTION) {
      lua_pushvalue(L, 4);  // copy function to the top of the stack
      pTicker->m_ScrollCallback = luaL_ref(L, LUA_REGISTRYINDEX);
   }

   // Set metatable
   luaL_getmetatable(L, "ticker.tbl");
   lua_setmetatable(L, -2);

   // Store Lua reference
   g_pLua = L;

   // Fire a update timer
   os_timer_disarm(&pTicker->m_ScrollTimer);
   os_timer_setfn(&pTicker->m_ScrollTimer, ticker_scroll_cb, pTicker);
   os_timer_arm(&pTicker->m_ScrollTimer, pTicker->m_Speed, 1);

   return 1;
}

static int ticker_destroy(lua_State *L) {
   NODE_DBG("ticker_destroy is called.\n");

   ticker_t *pData = (ticker_t *)luaL_checkudata(L, 1, "ticker.tbl");
   luaL_argcheck(L, pData, 1, "ticker.tbl expected");
   if(!pData) {
      NODE_DBG("userdata is nil.\n");
      return 0;
   }

   os_timer_disarm(&pData->m_ScrollTimer);

   // Release ticker buffer
   if (pData->m_OutputBuffer) {
      c_free(pData->m_OutputBuffer);
      pData->m_OutputBuffer = NULL;
   }

   lua_gc(L, LUA_GCSTOP, 0);
   if (pData->m_StripRef != LUA_NOREF) {
      luaL_unref(L, LUA_REGISTRYINDEX, pData->m_StripRef);
      pData->m_StripRef = LUA_NOREF;
   }

   if (pData->m_ScrollCallback != LUA_NOREF) {
      luaL_unref(L, LUA_REGISTRYINDEX, pData->m_ScrollCallback);
      pData->m_ScrollCallback = LUA_NOREF;
   }
   lua_gc(L, LUA_GCRESTART, 0);

   return 0;
}


// Module function map
#define MIN_OPT_LEVEL   2
#include "lrodefs.h"

/**
 * LPD object functions
 */
static const LUA_REG_TYPE ticker_map[] = {
  { LSTRKEY("set_text"),      LFUNCVAL(ticker_set_text) },
  { LSTRKEY("add_text"),      LFUNCVAL(ticker_add_text) },
  { LSTRKEY("add_letter"),    LFUNCVAL(ticker_add_letter) },
  { LSTRKEY("start"),         LFUNCVAL(ticker_start) },
  { LSTRKEY("stop"),          LFUNCVAL(ticker_stop) },
  { LSTRKEY("clear"),         LFUNCVAL(ticker_clear) },
  { LSTRKEY("set_speed"),     LFUNCVAL(ticker_set_speed) },
  { LSTRKEY("set_char_mask"), LFUNCVAL(ticker_set_char_mask) },
  { LSTRKEY("set_brightness"),LFUNCVAL(ticker_set_brightness) },
  { LSTRKEY("__gc"),          LFUNCVAL(ticker_destroy) },
#if LUA_OPTIMIZE_MEMORY > 0
  { LSTRKEY("__index"),      LROVAL(ticker_map) },
#endif
  { LNILKEY, LNILVAL }
};

/**
 * LPD Namespace functions
 */
const LUA_REG_TYPE lpdticker_map[] = {
  { LSTRKEY("setup"),         LFUNCVAL(ticker_setup) },
#if LUA_OPTIMIZE_MEMORY > 0
  { LSTRKEY("__metatable"),   LROVAL(lpdticker_map) },
#endif
  { LNILKEY, LNILVAL }
};

/**
 * Initializer function
 * @param L Lua state to be initialized
 */
LUALIB_API int luaopen_ticker(lua_State *L) {
#if LUA_OPTIMIZE_MEMORY > 0
   luaL_rometatable(L, "ticker.tbl", (void *)ticker_map);  // create metatable for ticker.lpd
   return 0;
#else // #if LUA_OPTIMIZE_MEMORY > 0
   luaL_register(L, AUXLIB_LPD_TICKER, lpdticker_map);

   // Set it as its own metatable
   lua_pushvalue(L, -1);
   lua_setmetatable(L, -2);

   // create metatable
   luaL_newmetatable(L, "ticker.tbl");
   // metatable.__index = metatable
   lua_pushliteral(L, "__index");
   lua_pushvalue(L,-2);
   lua_rawset(L,-3);
   // Setup the methods inside metatable
   luaL_register(L, NULL, ticker_map);

   return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0
}
