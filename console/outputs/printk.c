#include "nyxis.h"
#include "console/outputs/outputs_base.h"
#include <stdarg.h>

#include "console/outputs/printk.h"

// 커서 상태 (전역)
static u32 g_cursor_x = 0;
static u32 g_cursor_y = 0;

// 화면 설정 (필요시 외부에서 설정)
static u32 g_screen_width = 1024;
static u32 g_screen_height = 768;

static u32* g_printk_fb = nNULL;
static u32  g_printk_pixels_per_scanline = 0;

Nstatus printk_init(u32 *framebuffer, u32 pixels_per_scanline, u32 screen_width, u32 screen_height)
{
    if (!framebuffer || !pixels_per_scanline || !screen_width || !screen_height)
        return NinvalidArg;

    g_printk_fb = framebuffer;
    g_printk_pixels_per_scanline = pixels_per_scanline;
    g_screen_width = screen_width;
    g_screen_height = screen_height;

    return NSTATUS_OK;
}

#define FONT_W 16
#define FONT_H 16
#define PRINTK_BUFFER_SIZE 512

static Nstatus printk_puts(const char *string)
{
    if (!string)
        return NinvalidArg;

    if (!g_printk_fb || !g_printk_pixels_per_scanline)
        return NnotFound;

    while (*string)
    {
        char c = *string++;

        if (c == '\n') {
            g_cursor_x = 0;
            g_cursor_y += FONT_H;
            continue;
        }

        if (g_cursor_x + FONT_W >= g_screen_width) {
            g_cursor_x = 0;
            g_cursor_y += FONT_H;
        }

        if (g_cursor_y + FONT_H >= g_screen_height) {
            g_cursor_y = 0;
        }

        Nstatus status = draw_char_fast(
            g_printk_fb,
            g_printk_pixels_per_scanline,
            g_cursor_x,
            g_cursor_y,
            (utf8)c,
            0xFFFFFF,
            0x000000
        );

        if (NSTATUS_IS_ERR(status))
            return status;

        g_cursor_x += FONT_W;
    }

    return NSTATUS_OK;
}

static u32 uint_to_string(u32 value, char *out, u32 base, u32 min_width, u32 pad_zero)
{
    char temp[32];
    u32 len = 0;

    if (value == 0) {
        temp[len++] = '0';
    } else {
        while (value > 0) {
            u32 digit = value % base;
            temp[len++] = (char)(digit < 10 ? '0' + digit : 'a' + digit - 10);
            value /= base;
        }
    }

    while (len < min_width)
        temp[len++] = pad_zero ? '0' : ' ';

    u32 out_len = 0;
    for (i32 i = (i32)len - 1; i >= 0; i--)
        out[out_len++] = temp[i];

    return out_len;
}

static u32 int_to_string(i32 value, char *out)
{
    if (value < 0) {
        out[0] = '-';
        u32 len = uint_to_string((u32)(-(i64)value), out + 1, 10, 0, 0);
        return len + 1;
    }

    return uint_to_string((u32)value, out, 10, 0, 0);
}

static u32 status_to_string(Nstatus status, char *out)
{
    u32 pos = 0;
    u32 value = (u32)status;

    out[pos++] = '0';
    out[pos++] = 'x';

    for (i32 shift = 28; shift >= 0; shift -= 4) {
        u32 digit = (value >> shift) & 0xF;
        out[pos++] = (char)(digit < 10 ? '0' + digit : 'a' + digit - 10);
    }

    return pos;
}

static Nstatus printk_vformat(const char *format, va_list args)
{
    if (!format)
        return NinvalidArg;

    char buffer[PRINTK_BUFFER_SIZE];
    u32 length = 0;

    while (*format)
    {
        if (*format != '%') {
            if (length + 1 >= PRINTK_BUFFER_SIZE)
                return Noverflow;

            buffer[length++] = *format++;
            continue;
        }

        format++;
        if (*format == '%') {
            if (length + 1 >= PRINTK_BUFFER_SIZE)
                return Noverflow;

            buffer[length++] = '%';
            format++;
            continue;
        }

        char temp[64];
        u32 temp_len = 0;

        switch (*format) {
            case 's': {
                const char *text = va_arg(args, const char *);
                if (!text)
                    text = "(null)";

                while (*text) {
                    if (length + 1 >= PRINTK_BUFFER_SIZE)
                        return Noverflow;
                    buffer[length++] = *text++;
                }
                break;
            }
            case 'd': {
                i32 value = va_arg(args, int);
                temp_len = int_to_string(value, temp);
                break;
            }
            case 'u': {
                u32 value = va_arg(args, unsigned int);
                temp_len = uint_to_string(value, temp, 10, 0, 0);
                break;
            }
            case 'x': {
                u32 value = va_arg(args, unsigned int);
                temp_len = uint_to_string(value, temp, 16, 0, 1);
                break;
            }
            case 'r': {
                Nstatus status = va_arg(args, Nstatus);
                temp_len = status_to_string(status, temp);
                break;
            }
            default: {
                if (length + 1 >= PRINTK_BUFFER_SIZE)
                    return Noverflow;
                buffer[length++] = '%';
                if (*format) {
                    if (length + 1 >= PRINTK_BUFFER_SIZE)
                        return Noverflow;
                    buffer[length++] = *format;
                }
                break;
            }
        }

        if (temp_len) {
            if (length + temp_len >= PRINTK_BUFFER_SIZE)
                return Noverflow;

            for (u32 i = 0; i < temp_len; i++)
                buffer[length++] = temp[i];
        }

        if (*format)
            format++;
    }

    if (length >= PRINTK_BUFFER_SIZE)
        return Noverflow;

    buffer[length] = '\0';
    return printk_puts(buffer);
}

Nstatus printk(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    Nstatus status = printk_vformat(format, args);
    va_end(args);
    return status;
}

