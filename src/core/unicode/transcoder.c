#include "core/unicode/transcoder.h"
#include <string.h>

// Helper to decode a single UTF-8 sequence
static u32 s_utf8_decode_next(const u8 **pp)
{
    const u8 *p = *pp;
    u32 cp = *p++;
    if (cp < 0x80) goto end;
    if ((cp & 0xE0) == 0xC0) { cp &= 0x1F; cp <<= 6; cp |= (*p++ & 0x3F); }
    else if ((cp & 0xF0) == 0xE0) { cp &= 0x0F; cp <<= 6; cp |= (*p++ & 0x3F); cp <<= 6; cp |= (*p++ & 0x3F); }
    else if ((cp & 0xF8) == 0xF0) { cp &= 0x07; cp <<= 6; cp |= (*p++ & 0x3F); cp <<= 6; cp |= (*p++ & 0x3F); cp <<= 6; cp |= (*p++ & 0x3F); }
end:
    *pp = p;
    return cp;
}

size_t dpp_utf8_to_utf16(const u8 *src, u16 *dest)
{
    size_t count = 0;
    while (*src) {
        u32 cp = s_utf8_decode_next(&src);
        if (cp <= 0xFFFF) {
            dest[count++] = (u16)cp;
        } else {
            cp -= 0x10000;
            dest[count++] = (u16)((cp >> 10) + 0xD800);
            dest[count++] = (u16)((cp & 0x3FF) + 0xDC00);
        }
    }
    dest[count] = 0; // Null terminator
    return count;
}

size_t dpp_utf8_to_utf32(const u8 *src, u32 *dest)
{
    size_t count = 0;
    while (*src) {
        dest[count++] = s_utf8_decode_next(&src);
    }
    dest[count] = 0; // Null terminator
    return count;
}
