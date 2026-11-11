#ifndef PTX_STRUTIL_H
#define PTX_STRUTIL_H

#include <stddef.h>
#include <stdint.h>

/* Growable byte buffer, used for building the copied selection text. */
typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} StrBuf;

/* Append n bytes of s, keeping the buffer NUL-terminated. Grows as needed. */
void strbuf_append(StrBuf *b, const char *s, size_t n);

/* Encode a codepoint as UTF-8 into out (needs >= 4 bytes). Returns the number
 * of bytes written, or 0 if the codepoint is not a valid scalar value. */
int utf8_encode(uint32_t cp, char *out);

#endif /* PTX_STRUTIL_H */
