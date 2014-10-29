/*
 * FB driver for the ILI9341 LCD display controller
 *
 * This display uses 9-bit SPI: Data/Command bit + 8 data bits
 * For platforms that doesn't support 9-bit, the driver is capable
 * of emulating this using 8-bit transfer.
 * This is done by transfering eight 9-bit words in 9 bytes.
 *
 * Copyright (C) 2013 Christian Vogelgsang
 * Based on adafruit22fb.c by Noralf Tronnes
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>

#include "fbtft.h"

#define DRVNAME		"fb_ili9341"
#define WIDTH		240
#define HEIGHT		320
#define TXBUFLEN	(4 * PAGE_SIZE)
#define DEFAULT_GAMMA	"1F 1A 18 0A 0F 06 45 87 32 0A 07 02 07 05 00\n" \
			"00 25 27 05 10 09 3A 78 4D 05 18 0D 38 3A 1F"


static int init_display(struct fbtft_par *par)
{
	fbtft_par_dbg(DEBUG_INIT_DISPLAY, par, "%s()\n", __func__);

	par->fbtftops.reset(par);
	
	/* startup sequence for MI0283QT-9A */
	write_reg(par, 0x01); /* software reset */
	mdelay(5);
	write_reg(par, 0x28); /* display off */
	/* --------------------------------------------------------- */
#if 1	
	write_reg(par, 0xCF, 0x00, 0xDB, 0x30); 			// Power control B
	write_reg(par, 0xb1, 0x00, 0x18);					// Frame Rate Control (In Normal Mode/Full Colors) 
	write_reg(par, 0xED, 0x64, 0x03, 0x12, 0x81);		// Power on sequence control
	write_reg(par, 0xE8, 0x85, 0x00, 0x70); 			// Driver timing control A
	
	write_reg(par, 0xCB, 0x39, 0X2C, 0x00, 0x34, 0x02); 	// Power control A
	write_reg(par, 0xF7, 0x20); 							// Pump ratio control
	write_reg(par, 0xEA, 0x00, 0x00);						// Driver timing control B
		
	write_reg(par, 0xC0, 0x22); 			// Power Control 1
	write_reg(par, 0xC1, 0x12); 			// Power Control 2
	write_reg(par, 0xC5, 0x5C, 0x4c);		// VCOM Control 1
	write_reg(par, 0xC7, 0x8F); 			// VCOM Control 2
	write_reg(par, 0x36, 0x48); 			// Memory Access Control 
	write_reg(par, 0x3A, 0x55); 			// COLMOD: Pixel Format Set  16bit pixel 
	//write_reg(par, 0x3A, 0x66); 			// COLMOD: Pixel Format Set  18bit pixel 

	write_reg(par, 0xB0, 0xE0);			// RGB Interface Signal Control  RCM[1:0]=11
	write_reg(par, 0xF6, 0x00, 0x00, 0x06);
	write_reg(par, 0xB5, 0x04, 0x02, 0x0A, 0x14);
	
	//write_reg(par, 0xb6, 0x0a,0xa2);			// Display Function Control 	
	write_reg(par, 0xf2, 0x02); 			// 3Gamma Function Disable	
	write_reg(par, 0x26, 0x01); 			// Gamma curve selected 

	write_reg(par, 0xE0, 0x0f,0x20,0x19,0x0F,0x10,0x08,0x4A,0xF6,0x3A,0x0F,0x14,0x09,0x18,0x0B,0x08);	//Set Gamma 
	write_reg(par, 0xE1, 0x00,0x1E,0x1E,0x05,0x0F,0x04,0x31,0x33,0x43,0x04,0x0B,0x06,0x27,0x34,0x0F);	 //Set Gamma 
	
	write_reg(par, 0x11); // sleep out //
	mdelay(120);
	write_reg(par, 0x29); /* display on */
	mdelay(20);

	write_reg(par, 0x2A, 0x00, 0x00, 0x00, 0xEF);
	write_reg(par, 0x2B, 0x00, 0x00, 0x01, 0x3F);
	printk("------------------davinci 3.8.0 test---------------------\n");
	//write_reg(par, 0x2C);
#endif

	return 0;
}

static void set_addr_win(struct fbtft_par *par, int xs, int ys, int xe, int ye)
{
	fbtft_par_dbg(DEBUG_SET_ADDR_WIN, par,
		"%s(xs=%d, ys=%d, xe=%d, ye=%d)\n", __func__, xs, ys, xe, ye);

	/* Column address set */
	write_reg(par, 0x2A,
		(xs >> 8) & 0xFF, xs & 0xFF, (xe >> 8) & 0xFF, xe & 0xFF);

	/* Row adress set */
	write_reg(par, 0x2B,
		(ys >> 8) & 0xFF, ys & 0xFF, (ye >> 8) & 0xFF, ye & 0xFF);

	/* Memory write */
	write_reg(par, 0x2C);
}

#define MEM_Y   (7) /* MY row address order */
#define MEM_X   (6) /* MX column address order */
#define MEM_V   (5) /* MV row / column exchange */
#define MEM_L   (4) /* ML vertical refresh order */
#define MEM_H   (2) /* MH horizontal refresh order */
#define MEM_BGR (3) /* RGB-BGR Order */
static int set_var(struct fbtft_par *par)
{
	fbtft_par_dbg(DEBUG_INIT_DISPLAY, par, "%s()\n", __func__);

	switch (par->info->var.rotate) {
	case 0:
		write_reg(par, 0x36, (1 << MEM_X) | (par->bgr << MEM_BGR));
		break;
	case 270:
		write_reg(par, 0x36,
			(1<<MEM_V) | (1 << MEM_L) | (par->bgr << MEM_BGR));
		break;
	case 180:
		write_reg(par, 0x36, (1 << MEM_Y) | (par->bgr << MEM_BGR));
		break;
	case 90:
		write_reg(par, 0x36, (1 << MEM_Y) | (1 << MEM_X) |
				     (1 << MEM_V) | (par->bgr << MEM_BGR));
		break;
	}

	return 0;
}

/*
  Gamma string format:
    Positive: Par1 Par2 [...] Par15
    Negative: Par1 Par2 [...] Par15
*/
#define CURVE(num, idx)  curves[num*par->gamma.num_values + idx]
static int set_gamma(struct fbtft_par *par, unsigned long *curves)
{
	int i;

	fbtft_par_dbg(DEBUG_INIT_DISPLAY, par, "%s()\n", __func__);

	for (i = 0; i < par->gamma.num_curves; i++)
		write_reg(par, 0xE0 + i,
			CURVE(i, 0), CURVE(i, 1), CURVE(i, 2),
			CURVE(i, 3), CURVE(i, 4), CURVE(i, 5),
			CURVE(i, 6), CURVE(i, 7), CURVE(i, 8),
			CURVE(i, 9), CURVE(i, 10), CURVE(i, 11),
			CURVE(i, 12), CURVE(i, 13), CURVE(i, 14));

	return 0;
}
#undef CURVE


static struct fbtft_display display = {
	.buswidth = 9,
	.regwidth = 8,
	.width = WIDTH,
	.height = HEIGHT,
	.txbuflen = TXBUFLEN,
	.gamma_num = 2,
	.gamma_len = 15,
	.gamma = DEFAULT_GAMMA,
	.fbtftops = {
		.init_display = init_display,
		.set_addr_win = set_addr_win,
		.set_var = set_var,
		.set_gamma = set_gamma,
	},
};


FBTFT_REGISTER_DRIVER(DRVNAME, &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("platform:" DRVNAME);

MODULE_DESCRIPTION("FB driver for the ILI9341 LCD display controller");
MODULE_AUTHOR("Christian Vogelgsang");
MODULE_LICENSE("GPL");
