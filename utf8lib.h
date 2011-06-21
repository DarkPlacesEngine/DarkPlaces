/*
 * UTF-8 utility functions for DarkPlaces
 */
#ifndef UTF8LIB_H__
#define UTF8LIB_H__

#include "qtypes.h"

// types for unicode strings
// let them be 32 bit for now
// normally, whcar_t is 16 or 32 bit, 16 on linux I think, 32 on haiku and maybe windows
#ifdef _MSC_VER
typedef __int32 U_int32;
#else
#include <stdint.h>
#include <sys/types.h>
typedef int32_t U_int32;
#endif

// Uchar, a wide character
typedef U_int32 Uchar;

// Initialize UTF8, this registers cvars which allows for UTF8 to be disabled
// completely.
// When UTF8 is disabled, every u8_ function will work exactly as you'd expect
// a non-utf8 version to work: u8_strlen() will wrap to strlen()
// u8_byteofs() and u8_charidx() will simply return whatever is passed as index parameter
// u8_getchar() will will just return the next byte, u8_fromchar will write one byte, ...
extern cvar_t    utf8_enable;
void   u8_Init(void);

size_t u8_strlen(const char*);
size_t u8_strnlen(const char*, size_t);
int    u8_byteofs(const char*, size_t, size_t*);
int    u8_charidx(const char*, size_t, size_t*);
size_t u8_bytelen(const char*, size_t);
size_t u8_prevbyte(const char*, size_t);
Uchar  u8_getchar_utf8_enabled(const char*, const char**);
Uchar  u8_getnchar_utf8_enabled(const char*, const char**, size_t);
int    u8_fromchar(Uchar, char*, size_t);
size_t u8_wcstombs(char*, const Uchar*, size_t);
size_t u8_COM_StringLengthNoColors(const char *s, size_t size_s, qboolean *valid);

// returns a static buffer, use this for inlining
char  *u8_encodech(Uchar ch, size_t*);

size_t u8_strpad(char *out, size_t outsize, const char *in, qboolean leftalign, size_t minwidth, size_t maxwidth);

/* Careful: if we disable utf8 but not freetype, we wish to see freetype chars
 * for normal letters. So use E000+x for special chars, but leave the freetype stuff for the
 * rest:
 */
extern Uchar u8_quake2utf8map[256];
// these defines get a bit tricky, as c and e may be aliased to the same variable
#define u8_getchar(c,e) (utf8_enable.integer ? u8_getchar_utf8_enabled(c,e) : (u8_quake2utf8map[(unsigned char)(*(e) = (c) + 1)[-1]]))
#define u8_getchar_noendptr(c) (utf8_enable.integer ? u8_getchar_utf8_enabled(c,NULL) : (u8_quake2utf8map[(unsigned char)*(c)]))
#define u8_getchar_check(c,e) ((e) ? u8_getchar((c),(e)) : u8_getchar_noendptr((c)))
#define u8_getnchar(c,e,n) (utf8_enable.integer ? u8_getnchar_utf8_enabled(c,e,n) : ((n) <= 0 ? ((*(e) = c), 0) : (u8_quake2utf8map[(unsigned char)(*(e) = (c) + 1)[-1]])))
#define u8_getnchar_noendptr(c,n) (utf8_enable.integer ? u8_getnchar_utf8_enabled(c,NULL,n) : ((n) <= 0 ? 0  : (u8_quake2utf8map[(unsigned char)*(c)])))
#define u8_getnchar_check(c,e,n) ((e) ? u8_getchar((c),(e),(n)) : u8_getchar_noendptr((c),(n)))

Uchar u8_toupper(Uchar ch);
Uchar u8_tolower(Uchar ch);

#endif // UTF8LIB_H__
