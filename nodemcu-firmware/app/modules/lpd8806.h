/*
 * lpd8806.h
 *
 * Defines LPD8806 related functions and structures
 *
 *  Created on: Feb 21, 2015
 *      Author: yowidin
 */

#ifndef APP_MODULES_LPD8806_H_
#define APP_MODULES_LPD8806_H_

/**
 * An LED Strip object
 */
typedef struct lpd_userdata {
   uint8_t *m_OutputBuffer; //!< Cached state of the LED strip
   unsigned m_Mosi;         //!< MOSI GPIO pin index
   unsigned m_Clk;          //!< CLK GPIO pin index
   unsigned m_Length;       //!< Length of the LED strip
   unsigned m_Type;         //!< SPI Transfer type
   unsigned m_Delay;        //!< SPI bit-bang delay
} lpd_userdata;

void update(lpd_userdata *pData);
void set_color(lpd_userdata *pData, unsigned index, unsigned r, unsigned g, unsigned b);
void set_color_n(lpd_userdata *pData, unsigned length, unsigned *indices,
                 unsigned *r, unsigned *g, unsigned *b);
void clear(lpd_userdata *pData, unsigned r, unsigned g, unsigned b);


#endif /* APP_MODULES_LPD8806_H_ */
