#ifndef DPP_UNICODE_TRANSCODER_H
#define DPP_UNICODE_TRANSCODER_H

#include "core/types.h"

// Returns the number of 16-bit code units written to dest.
// dest must be large enough.
size_t dpp_utf8_to_utf16(const u8 *src, u16 *dest);

// Returns the number of 32-bit code units written to dest.
// dest must be large enough.
size_t dpp_utf8_to_utf32(const u8 *src, u32 *dest);

#endif
