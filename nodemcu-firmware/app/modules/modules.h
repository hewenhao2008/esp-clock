/**
 * External modules library
 */

#ifndef __MODULES_H__
#define __MODULES_H__

#if defined(LUA_USE_MODULES_GPIO)
#define MODULES_GPIO       "gpio"
#define ROM_MODULES_GPIO   \
    _ROM(MODULES_GPIO, luaopen_gpio, gpio_map)
#else
#define ROM_MODULES_GPIO
#endif

#if defined(LUA_USE_MODULES_PWM)
#define MODULES_PWM       "pwm"
#define ROM_MODULES_PWM   \
    _ROM(MODULES_PWM, luaopen_pwm, pwm_map)
#else
#define ROM_MODULES_PWM
#endif

#if defined(LUA_USE_MODULES_WIFI)
#define MODULES_WIFI       "wifi"
#define ROM_MODULES_WIFI   \
    _ROM(MODULES_WIFI, luaopen_wifi, wifi_map)
#else
#define ROM_MODULES_WIFI
#endif

#if defined(LUA_USE_MODULES_NET)
#define MODULES_NET       "net"
#define ROM_MODULES_NET   \
    _ROM(MODULES_NET, luaopen_net, net_map)
#else
#define ROM_MODULES_NET
#endif

#if defined(LUA_USE_MODULES_MQTT)
#define MODULES_MQTT       "mqtt"
#define ROM_MODULES_MQTT   \
    _ROM(MODULES_MQTT, luaopen_mqtt, mqtt_map)
#else
#define ROM_MODULES_MQTT
#endif

#if defined(LUA_USE_MODULES_I2C)
#define MODULES_I2C       "i2c"
#define ROM_MODULES_I2C   \
    _ROM(MODULES_I2C, luaopen_i2c, i2c_map)
#else
#define ROM_MODULES_I2C
#endif

#if defined(LUA_USE_MODULES_SPI)
#define MODULES_SPI       "spi"
#define ROM_MODULES_SPI   \
    _ROM(MODULES_SPI, luaopen_spi, spi_map)
#else
#define ROM_MODULES_SPI
#endif

#if defined(LUA_USE_MODULES_LPD8806)
#define MODULES_LPD8806   "lpd8806"
#define ROM_MODULES_LPD8806   \
    _ROM(MODULES_LPD8806, luaopen_lpd8806, lpd8806_map)
#else
#define ROM_MODULES_LPD8806
#endif

#if defined(LUA_USE_MODULES_LPD_TICKER)
#define MODULES_LPD_TICKER   "ticker"
#define ROM_MODULES_LPD_TICKER   \
    _ROM(MODULES_LPD_TICKER, luaopen_ticker, lpdticker_map)
#else
#define ROM_MODULES_LPD_TICKER
#endif

#if defined(LUA_USE_MODULES_FILE_SERVER)
#define MODULES_FILE_SERVER   "file_server"
#define ROM_MODULES_FILE_SERVER   \
    _ROM(MODULES_FILE_SERVER, luaopen_file_server, file_server_map)
#else
#define ROM_MODULES_FILE_SERVER
#endif

#if defined(LUA_USE_MODULES_DHT)
#define MODULES_DHT   "dht"
#define ROM_MODULES_DHT   \
    _ROM(MODULES_DHT, luaopen_dht, dht_map)
#else
#define ROM_MODULES_DHT
#endif

#if defined(LUA_USE_MODULES_TMR)
#define MODULES_TMR       "tmr"
#define ROM_MODULES_TMR   \
    _ROM(MODULES_TMR, luaopen_tmr, tmr_map)
#else
#define ROM_MODULES_TMR
#endif

#if defined(LUA_USE_MODULES_NODE)
#define MODULES_NODE       "node"
#define ROM_MODULES_NODE   \
    _ROM(MODULES_NODE, luaopen_node, node_map)
#else
#define ROM_MODULES_NODE
#endif

#if defined(LUA_USE_MODULES_FILE)
#define MODULES_FILE       "file"
#define ROM_MODULES_FILE   \
    _ROM(MODULES_FILE, luaopen_file, file_map)
#else
#define ROM_MODULES_FILE
#endif

#if defined(LUA_USE_MODULES_ADC)
#define MODULES_ADC       "adc"
#define ROM_MODULES_ADC   \
    _ROM(MODULES_ADC, luaopen_adc, adc_map)
#else
#define ROM_MODULES_ADC
#endif

#if defined(LUA_USE_MODULES_UART)
#define MODULES_UART       "uart"
#define ROM_MODULES_UART   \
    _ROM(MODULES_UART, luaopen_uart, uart_map)
#else
#define ROM_MODULES_UART
#endif

#if defined(LUA_USE_MODULES_OW)
#define MODULES_OW       "ow"
#define ROM_MODULES_OW   \
    _ROM(MODULES_OW, luaopen_ow, ow_map)
#else
#define ROM_MODULES_OW
#endif

#if defined(LUA_USE_MODULES_BIT)
#define MODULES_BIT       "bit"
#define ROM_MODULES_BIT   \
    _ROM(MODULES_BIT, luaopen_bit, bit_map)
#else
#define ROM_MODULES_BIT
#endif

#if defined(LUA_USE_MODULES_WS2812)
#define MODULES_WS2812 "ws2812"
#define ROM_MODULES_WS2812 \
		_ROM(MODULES_WS2812, luaopen_ws2812, ws2812_map)
#else
#define ROM_MODULES_WS2812
#endif


#define LUA_MODULES_ROM     \
        ROM_MODULES_GPIO    \
        ROM_MODULES_PWM		\
        ROM_MODULES_WIFI	\
        ROM_MODULES_MQTT    \
        ROM_MODULES_I2C     \
        ROM_MODULES_SPI     \
        ROM_MODULES_TMR     \
        ROM_MODULES_NODE    \
        ROM_MODULES_FILE    \
        ROM_MODULES_NET     \
        ROM_MODULES_ADC     \
        ROM_MODULES_UART    \
        ROM_MODULES_OW      \
        ROM_MODULES_LPD8806 \
        ROM_MODULES_FILE_SERVER \
        ROM_MODULES_DHT     \
        ROM_MODULES_LPD_TICKER \
        ROM_MODULES_BIT		\
        ROM_MODULES_WS2812

#endif

