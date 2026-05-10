#ifndef PRINTK_H
#define PRINTK_H

#include "include/nyxis.h"

#define FONT_W 16
#define FONT_H 16

Nstatus printk(const char *format, ...);
Nstatus printk_init(u32 *framebuffer, u32 pixels_per_scanline, u32 screen_width, u32 screen_height);

#endif