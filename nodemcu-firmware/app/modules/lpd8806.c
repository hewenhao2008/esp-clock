// Module for interfacing with a LPD8806 LED strip

//#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "lpd8806.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"
#include "c_string.h"
#include "c_stdlib.h"

//! SPI Bit-bang delay
#define DELAY_US    1

/**
 * List of the supported SPI modes, where
 * CPOL - Clock Polarity
 * CPHA - Clock Phase (Edge)
 */
enum spi_mode {
   CPOL0_CPHA1 = 0, //!< CPOL = 0, CPHA = 1
   CPOL0_CPHA0 = 1, //!< CPOL = 0, CPHA = 0
   CPOL1_CPHA0 = 2, //!< CPOL = 1, CPHA = 0
   CPOL1_CPHA1 = 3  //!< CPOL = 1, CPHA = 1
};

/**
 * Gamma correction table.
 * Since the LPD8806 strip is only able to represent 7-bit colors
 * we have to convert 8 bit into 7 bit color values.
 */
const uint8_t gammaTable[] = {
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,
     1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,
     2,   2,   2,   2,   2,   3,   3,   3,   3,   3,   3,   3,   3,   4,   4,   4,
     4,   4,   4,   4,   5,   5,   5,   5,   5,   6,   6,   6,   6,   6,   7,   7,
     7,   7,   7,   8,   8,   8,   8,   9,   9,   9,   9,  10,  10,  10,  10,  11,
    11,  11,  12,  12,  12,  13,  13,  13,  13,  14,  14,  14,  15,  15,  16,  16,
    16,  17,  17,  17,  18,  18,  18,  19,  19,  20,  20,  21,  21,  21,  22,  22,
    23,  23,  24,  24,  24,  25,  25,  26,  26,  27,  27,  28,  28,  29,  29,  30,
    30,  31,  32,  32,  33,  33,  34,  34,  35,  35,  36,  37,  37,  38,  38,  39,
    40,  40,  41,  41,  42,  43,  43,  44,  45,  45,  46,  47,  47,  48,  49,  50,
    50,  51,  52,  52,  53,  54,  55,  55,  56,  57,  58,  58,  59,  60,  61,  62,
    62,  63,  64,  65,  66,  67,  67,  68,  69,  70,  71,  72,  73,  74,  74,  75,
    76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,
    92,  93,  94,  95,  96,  97,  98,  99, 100, 101, 102, 104, 105, 106, 107, 108,
   109, 110, 111, 113, 114, 115, 116, 117, 118, 120, 121, 122, 123, 125, 126, 127
};

/**
 * Transfer a single data byte via bit-bang SPI protocol
 * @param byte  byte to be transfered
 */
static void transfer_byte(unsigned mosi, unsigned clk, uint8_t byte, unsigned delay, unsigned type) {
   unsigned level;
   uint8_t bit;

   platform_gpio_write(clk, type != CPOL0_CPHA0 && type != CPOL0_CPHA1);

   for (bit = 0x80; bit; bit >>= 1) {
      level = (byte & bit) ? PLATFORM_GPIO_HIGH : PLATFORM_GPIO_LOW;
      platform_gpio_write(mosi, level);

      switch (type) {
         case CPOL0_CPHA0: {
            os_delay_us(delay);
            platform_gpio_write(clk, PLATFORM_GPIO_HIGH);
            os_delay_us(delay);
            platform_gpio_write(clk, PLATFORM_GPIO_LOW);
         }
         break;

         case CPOL0_CPHA1: {
            platform_gpio_write(clk, PLATFORM_GPIO_HIGH);
            os_delay_us(delay);
            platform_gpio_write(clk, PLATFORM_GPIO_LOW);
            os_delay_us(delay);
         }
         break;

         case CPOL1_CPHA0: {
            os_delay_us(delay);
            platform_gpio_write(clk, PLATFORM_GPIO_LOW);
            os_delay_us(delay);
            platform_gpio_write(clk, PLATFORM_GPIO_HIGH);
         }
         break;

         case CPOL1_CPHA1: {
            platform_gpio_write(clk, PLATFORM_GPIO_LOW);
            os_delay_us(delay);
            platform_gpio_write(clk, PLATFORM_GPIO_HIGH);
            os_delay_us(delay);
         }
         break;
      }
   }
}

/**
 * Update a given LED strip.
 * i.e. push cached LED's state into LED strip
 * @param pData an LED Strip object
 */
void update(lpd_userdata *pData) {
   uint16_t length = pData->m_Length * 3;
   uint16_t latch = ((pData->m_Length + 31) / 32) * 3;
   unsigned mosi = pData->m_Mosi;
   unsigned clk = pData->m_Clk;
   unsigned delay = pData->m_Delay;
   unsigned type = pData->m_Type;
   uint16_t i;

   os_intr_lock();
   for (i = 0; i < length; ++i)
      transfer_byte(mosi, clk, pData->m_OutputBuffer[i], delay, type);

   for (i = 0; i < latch; ++i)
      transfer_byte(mosi, clk, 0, delay, type);
   os_intr_unlock();
}

/**
 * Update a single LED of a given LED strip.
 * i.e. write a new RGB value into strip's cache.
 *
 * @param pData an LED Strip object
 * @param index index of an LED to be changed
 * @param r     R color component
 * @param g     G color component
 * @param b     B color component
 */
void set_color(lpd_userdata *pData, unsigned index, unsigned r, unsigned g, unsigned b) {
   index = index % pData->m_Length;

   unsigned rn = gammaTable[r];
   unsigned gn = gammaTable[g];
   unsigned bn = gammaTable[b];

   pData->m_OutputBuffer[3 * index + 0] = gn | 0x80;
   pData->m_OutputBuffer[3 * index + 1] = rn | 0x80;
   pData->m_OutputBuffer[3 * index + 2] = bn | 0x80;
}

/**
 * Update a a list of LEDs of a given LED strip.
 * i.e. write a new RGB value into strip's cache.
 * @param pData     an LED Strip object
 * @param length    number of LEDs to be updated
 * @param indecies  array of LED indecies
 * @param r         array of R color components
 * @param g         array of G color components
 * @param b         array of B color components
 */
void set_color_n(lpd_userdata *pData, unsigned length, unsigned *indices,
                        unsigned *r, unsigned *g, unsigned *b)
{
   unsigned i, index, rn, gn, bn;
   for (i = 0; i < length; ++i) {
      index = indices[i] % pData->m_Length;

      rn = gammaTable[r[i]];
      gn = gammaTable[g[i]];
      bn = gammaTable[b[i]];

      pData->m_OutputBuffer[3 * index + 0] = gn | 0x80;
      pData->m_OutputBuffer[3 * index + 1] = rn | 0x80;
      pData->m_OutputBuffer[3 * index + 2] = bn | 0x80;
   }
}

/**
 * Clear a given LED strip
 * @param   pData   an LED Strip object
 * @param   r       R color component
 * @param   g       G color component
 * @param   b       B color component
 */
void clear(lpd_userdata *pData, unsigned r, unsigned g, unsigned b) {
   unsigned length = pData->m_Length;
   unsigned i = 0;

   for (; i < length; ++i)
      set_color(pData, i, r, g, b);
}

/**
 * Setup an LED strip
 * @param   L   Lua Sate
 * @return      Number of return parameters on stack (always 1)
 * @example     Lua: lpd = lpd8806.setup(mosi, clk, length [, type, delay])
 */
static int lpd_setup(lua_State *L) {
   unsigned mosi;
   unsigned clk;
   unsigned length;
   unsigned type = 0;
   unsigned delay = 2;
   int result;
   lpd_userdata *pData = NULL;

   NODE_DBG("lpd_setup is called.\n");

   // Load and check parameters
   mosi     = luaL_checkinteger(L, 1);
   clk      = luaL_checkinteger(L, 2);
   length   = luaL_checkinteger(L, 3);

   if (lua_isnumber(L, 4)) type  = lua_tointeger(L, 4);
   if (lua_isnumber(L, 5)) delay = lua_tointeger(L, 5);

   MOD_CHECK_ID(gpio, mosi);
   MOD_CHECK_ID(gpio, clk);

   result = platform_gpio_mode(mosi, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
   if (result < 0)
      return luaL_error(L, "Invalid MOSI pin.");

   result = platform_gpio_mode(clk, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
   if (result < 0)
      return luaL_error(L, "Invalid CLK pin.");

   // create an object
   pData = (lpd_userdata *)lua_newuserdata(L, sizeof(lpd_userdata));
   if (!pData)
      return luaL_error(L, "Out of memory (struct).");

   // Initialize LED structure
   pData->m_Mosi    = mosi;
   pData->m_Clk     = clk;
   pData->m_Length  = length;
   pData->m_OutputBuffer = NULL;
   pData->m_Type    = type;
   pData->m_Delay   = delay;

   pData->m_OutputBuffer = c_zalloc(sizeof(uint8_t) * pData->m_Length * 3);
   if (!pData->m_OutputBuffer)
      return luaL_error(L, "Out of memory (data).");

   // Set metatable
   luaL_getmetatable(L, "lpd8806.lpd");
   lua_setmetatable(L, -2);

   // Bring strip into predictable state (cases single flash)
   // clear(pData, 255, 255, 255); update(pData);
   clear(pData,   0,   0,   0); update(pData);

   NODE_DBG("LPD8806: Init info: MOSI=%d, CLK=%d, Length=%d\r\n", pData->m_Mosi, pData->m_Clk, pData->m_Length);
   return 1;
}

/**
 * Update a single LED of a given LED strip.
 * i.e. write a new RGB value into strip's cache.
 *
 * It can either receive single index and single value for each color component
 * or table of indices and separate table for each color component.
 *
 * @param   L   Lua Sate
 * @return      Number of return parameters on stack (always 0)
 * @example     Lua: = lpd.set_color(0, 255, 0, 0)
 *                     lpd.set_color({0, 1}, {255, 0}, {0, 255}, {0, 0})
 */
static int lpd_set_color(lua_State *L) {
   unsigned r = 0, g = 0, b = 0, index = 0, len = 0, i;
   lpd_userdata *pData = NULL;

   pData = (lpd_userdata *)luaL_checkudata(L, 1, "lpd8806.lpd");
   luaL_argcheck(L, pData, 1, "lpd8806.lpd expected");
   if(!pData) {
      NODE_DBG("userdata is nil.\n");
      return 0;
   }

   // Table arguments (arrays of indices and colors)
   if (lua_istable(L, 2)) {
      // Ensure that all tables have the same size
      len = lua_istable(L, 3) && lua_istable(L, 4) && lua_istable(L, 5) &&
            (lua_objlen(L, 2) == lua_objlen(L, 3)) &&
            (lua_objlen(L, 2) == lua_objlen(L, 4)) &&
            (lua_objlen(L, 2) == lua_objlen(L, 5));

      if (!len)
         return luaL_error(L, "Tables sizes do not match");

      len = lua_objlen(L, 2);

      for (i = 0; i < len; ++i) {
         // Index
         lua_rawgeti(L, 2, i + 1);
         index = luaL_checkinteger(L, -1);
         lua_pop(L, 1);

         // R
         lua_rawgeti(L, 3, i + 1);
         r = luaL_checkinteger(L, -1);
         lua_pop(L, 1);

         // G
         lua_rawgeti(L, 4, i + 1);
         g = luaL_checkinteger(L, -1);
         lua_pop(L, 1);

         // B
         lua_rawgeti(L, 5, i + 1);
         b = luaL_checkinteger(L, -1);
         lua_pop(L, 1);

         // Apply changes
         set_color(pData, index, r, g, b);
      }
   }
   else {
      // Single index call
      index = luaL_checkinteger(L, 2);

      r = luaL_checkinteger(L, 3);
      g = luaL_checkinteger(L, 4);
      b = luaL_checkinteger(L, 5);

      set_color(pData, index, r, g, b);
   }

   return 0;
}

/**
 * Update a given LED strip. i.e. push cached LED's state into LED strip
 * @param   L   Lua Sate
 * @return      Number of return parameters on stack (always 0)
 * @example     Lua: = lpd.update()
 */
static int lpd_update(lua_State *L) {
   lpd_userdata *pData = (lpd_userdata *)luaL_checkudata(L, 1, "lpd8806.lpd");
   luaL_argcheck(L, pData, 1, "lpd8806.lpd expected");
   if(!pData) {
      NODE_DBG("userdata is nil.\n");
      return 0;
   }

   update(pData);

   return 0;
}

/**
 * Clear a given LED strip
 * @param   L   Lua State
 * @return      Number of return parameters on stack (always 0)
 * @example     Lua: = lpd.clear(r, g, b)
 */
static int lpd_clear(lua_State *L) {
   unsigned r = 0, g = 0, b = 0;
   lpd_userdata *pData = NULL;

   pData = (lpd_userdata *)luaL_checkudata(L, 1, "lpd8806.lpd");
   luaL_argcheck(L, pData, 1, "lpd8806.lpd expected");
   if(!pData) {
      NODE_DBG("userdata is nil.\n");
      return 0;
   }

   if (lua_isnumber(L, 2)) r = lua_tointeger(L, 2);
   if (lua_isnumber(L, 3)) g = lua_tointeger(L, 3);
   if (lua_isnumber(L, 4)) b = lua_tointeger(L, 4);

   clear(pData, r, g, b);
   update(pData);

   return 0;
}

/**
 * Returns length of a given LED strip
 * @param   L   Lua state
 * @return      Number of LED's in the strip
 * @example     Lua: length = lpd.get_length()
 */
static int lpd_get_length(lua_State *L) {
   lpd_userdata *pData = (lpd_userdata *)luaL_checkudata(L, 1, "lpd8806.lpd");
   luaL_argcheck(L, pData, 1, "lpd8806.lpd expected");
   if(!pData) {
      NODE_DBG("userdata is nil.\n");
      return 0;
   }
   lua_pushinteger(L, pData->m_Length);
   return 1;
}

/**
 * Function called by the Lua garbage collector then last reference on
 * lpd object is removed.
 *
 * @param L Lua state
 */
static int lpd_destroy(lua_State *L) {
   NODE_DBG("lpd_destroy is called.\n");

   lpd_userdata *pData = (lpd_userdata *)luaL_checkudata(L, 1, "lpd8806.lpd");
   luaL_argcheck(L, pData, 1, "lpd8806.lpd expected");
   if(!pData) {
      NODE_DBG("userdata is nil.\n");
      return 0;
   }

   // Release LED buffer
   if (pData->m_OutputBuffer) {
      c_free(pData->m_OutputBuffer);
      pData->m_OutputBuffer = NULL;
   }

   return 0;
}

// Module function map
#define MIN_OPT_LEVEL   2
#include "lrodefs.h"

/**
 * LPD object functions
 */
static const LUA_REG_TYPE lpd_map[] = {
  { LSTRKEY("set_color"),     LFUNCVAL(lpd_set_color) },
  { LSTRKEY("update"),        LFUNCVAL(lpd_update) },
  { LSTRKEY("clear"),         LFUNCVAL(lpd_clear) },
  { LSTRKEY("get_length"),    LFUNCVAL(lpd_get_length) },
  { LSTRKEY("__gc"),          LFUNCVAL(lpd_destroy) },
#if LUA_OPTIMIZE_MEMORY > 0
  { LSTRKEY("__index"),       LROVAL(lpd_map) },
#endif
  { LNILKEY, LNILVAL }
};

/**
 * LPD Namespace functions
 */
const LUA_REG_TYPE lpd8806_map[] = {
  { LSTRKEY("setup"),         LFUNCVAL(lpd_setup) },
#if LUA_OPTIMIZE_MEMORY > 0
  { LSTRKEY("__metatable"),   LROVAL(lpd8806_map) },
#endif
  { LNILKEY, LNILVAL }
};

/**
 * Initializer function
 * @param L Lua state to be initialized
 */
LUALIB_API int luaopen_lpd8806(lua_State *L) {
#if LUA_OPTIMIZE_MEMORY > 0
   luaL_rometatable(L, "lpd8806.lpd", (void *)lpd_map);  // create metatable for lpd8806.lpd
   return 0;
#else // #if LUA_OPTIMIZE_MEMORY > 0
   luaL_register(L, AUXLIB_LPD8806, lpd8806_map);

   // Set it as its own metatable
   lua_pushvalue(L, -1);
   lua_setmetatable(L, -2);

   // create metatable
   luaL_newmetatable(L, "lpd8806.lpd");
   // metatable.__index = metatable
   lua_pushliteral(L, "__index");
   lua_pushvalue(L,-2);
   lua_rawset(L,-3);
   // Setup the methods inside metatable
   luaL_register(L, NULL, lpd_map);

   return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0
}

