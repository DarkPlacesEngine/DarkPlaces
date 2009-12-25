#include "quakedef.h"
#include "utf8lib.h"

/*
================================================================================
Initialization of UTF-8 support and new cvars.
================================================================================
*/
// for compatibility this defaults to 0
cvar_t    utf8_enable = {CVAR_SAVE, "utf8_enable", "0", "Enable UTF-8 support. For compatibility, this is disabled by default in most games."};

void   u8_Init(void)
{
	Cvar_RegisterVariable(&utf8_enable);
}

/*
================================================================================
UTF-8 encoding and decoding functions follow.
================================================================================
*/

/** Analyze the next character and return various information if requested.
 * @param _s      An utf-8 string.
 * @param _start  Filled with the start byte-offset of the next valid character
 * @param _len    Fileed with the length of the next valid character
 * @param _ch     Filled with the unicode value of the next character
 * @param _maxlen Maximum number of bytes to read from _s
 * @return        Whether or not another valid character is in the string
 */
#define U8_ANALYZE_INFINITY 7
static qboolean u8_analyze(const char *_s, size_t *_start, size_t *_len, Uchar *_ch, size_t _maxlen)
{
	const unsigned char *s = (const unsigned char*)_s;
	unsigned char bt, bc;
	size_t i;
	size_t bits, j;
	Uchar ch;

	i = 0;
findchar:

	// <0xC2 is always an overlong encoding, they're invalid, thus skipped
	while (i < _maxlen && s[i] && s[i] >= 0x80 && s[i] <= 0xC2) {
		//fprintf(stderr, "skipping\n");
		++i;
	}
	if(i >= _maxlen)
		return false;
	//fprintf(stderr, "checking\n");

	// If we hit the end, well, we're out and invalid
	if (!s[i])
		return false;
	//fprintf(stderr, "checking ascii\n");

	// ascii characters
	if (s[i] < 0x80)
	{
		if (_start) *_start = i;
		if (_len) *_len = 1;
		if (_ch) *_ch = (Uchar)s[i];
		//fprintf(stderr, "valid ascii\n");
		return true;
	}
	//fprintf(stderr, "checking length\n");

	// Figure out the next char's length
	bc = s[i];
	bits = 1;
	// count the 1 bits, they're the # of bytes
	for (bt = 0x40; bt && (bc & bt); bt >>= 1, ++bits);
	if (!bt)
	{
		//fprintf(stderr, "superlong\n");
		++i;
		goto findchar;
	}
	if(i + bits > _maxlen)
		return false;
	// turn bt into a mask and give ch a starting value
	--bt;
	ch = (s[i] & bt);
	// check the byte sequence for invalid bytes
	for (j = 1; j < bits; ++j)
	{
		// valid bit value: 10xx xxxx
		//if (s[i+j] < 0x80 || s[i+j] >= 0xC0)
		if ( (s[i+j] & 0xC0) != 0x80 )
		{
			//fprintf(stderr, "sequence of %i f'd at %i by %x\n", bits, j, (unsigned int)s[i+j]);
			// this byte sequence is invalid, skip it
			i += j;
			// find a character after it
			goto findchar;
		}
		// at the same time, decode the character
		ch = (ch << 6) | (s[i+j] & 0x3F);
	}

	// Now check the decoded byte for an overlong encoding
	if ( (bits >= 2 && ch < 0x80) ||
	     (bits >= 3 && ch < 0x800) ||
	     (bits >= 4 && ch < 0x10000) ||
	     ch >= 0x10FFFF // RFC 3629
		)
	{
		i += bits;
		//fprintf(stderr, "overlong: %i bytes for %x\n", bits, ch);
		goto findchar;
	}

	if (_start)
		*_start = i;
	if (_len)
		*_len = bits;
	if (_ch)
		*_ch = ch;
	//fprintf(stderr, "valid utf8\n");
	return true;
}

/** Get the number of characters in an UTF-8 string.
 * @param _s    An utf-8 encoded null-terminated string.
 * @return      The number of unicode characters in the string.
 */
size_t u8_strlen(const char *_s)
{
	size_t st, ln;
	size_t len = 0;
	const unsigned char *s = (const unsigned char*)_s;

	if (!utf8_enable.integer)
		return strlen(_s);

	while (*s)
	{
		// ascii char, skip u8_analyze
		if (*s < 0x80)
		{
			++len;
			++s;
			continue;
		}

		// invalid, skip u8_analyze
		if (*s <= 0xC2)
		{
			++s;
			continue;
		}

		if (!u8_analyze((const char*)s, &st, &ln, NULL, U8_ANALYZE_INFINITY))
			break;
		// valid character, skip after it
		s += st + ln;
		++len;
	}
	return len;
}

/** Get the number of characters in a part of an UTF-8 string.
 * @param _s    An utf-8 encoded null-terminated string.
 * @param n     The maximum number of bytes.
 * @return      The number of unicode characters in the string.
 */
size_t u8_strnlen(const char *_s, size_t n)
{
	size_t st, ln;
	size_t len = 0;
	const unsigned char *s = (const unsigned char*)_s;

	if (!utf8_enable.integer)
	{
		len = strlen(_s);
		return (len < n) ? len : n;
	}

	while (*s && n)
	{
		// ascii char, skip u8_analyze
		if (*s < 0x80)
		{
			++len;
			++s;
			--n;
			continue;
		}

		// invalid, skip u8_analyze
		if (*s <= 0xC2)
		{
			++s;
			--n;
			continue;
		}

		if (!u8_analyze((const char*)s, &st, &ln, NULL, n))
			break;
		// valid character, see if it's still inside the range specified by n:
		if (n < st + ln)
			return len;
		++len;
		n -= st + ln;
		s += st + ln;
	}
	return len;
}

/** Get the number of bytes used in a string to represent an amount of characters.
 * @param _s    An utf-8 encoded null-terminated string.
 * @param n     The number of characters we want to know the byte-size for.
 * @return      The number of bytes used to represent n characters.
 */
size_t u8_bytelen(const char *_s, size_t n)
{
	size_t st, ln;
	size_t len = 0;
	const unsigned char *s = (const unsigned char*)_s;

	if (!utf8_enable.integer)
		return n;

	while (*s && n)
	{
		// ascii char, skip u8_analyze
		if (*s < 0x80)
		{
			++len;
			++s;
			--n;
			continue;
		}

		// invalid, skip u8_analyze
		if (*s <= 0xC2)
		{
			++s;
			++len;
			continue;
		}

		if (!u8_analyze((const char*)s, &st, &ln, NULL, U8_ANALYZE_INFINITY))
			break;
		--n;
		s += st + ln;
		len += st + ln;
	}
	return len;
}

/** Get the byte-index for a character-index.
 * @param _s      An utf-8 encoded string.
 * @param i       The character-index for which you want the byte offset.
 * @param len     If not null, character's length will be stored in there.
 * @return        The byte-index at which the character begins, or -1 if the string is too short.
 */
int u8_byteofs(const char *_s, size_t i, size_t *len)
{
	size_t st, ln;
	size_t ofs = 0;
	const unsigned char *s = (const unsigned char*)_s;

	if (!utf8_enable.integer)
	{
		if (len) *len = 1;
		return i;
	}

	st = ln = 0;
	do
	{
		ofs += ln;
		if (!u8_analyze((const char*)s + ofs, &st, &ln, NULL, U8_ANALYZE_INFINITY))
			return -1;
		ofs += st;
	} while(i-- > 0);
	if (len)
		*len = ln;
	return ofs;
}

/** Get the char-index for a byte-index.
 * @param _s      An utf-8 encoded string.
 * @param i       The byte offset for which you want the character index.
 * @param len     If not null, the offset within the character is stored here.
 * @return        The character-index, or -1 if the string is too short.
 */
int u8_charidx(const char *_s, size_t i, size_t *len)
{
	size_t st, ln;
	size_t ofs = 0;
	size_t pofs = 0;
	int idx = 0;
	const unsigned char *s = (const unsigned char*)_s;

	if (!utf8_enable.integer)
	{
		if (len) *len = 0;
		return i;
	}

	while (ofs < i && s[ofs])
	{
		// ascii character, skip u8_analyze
		if (s[ofs] < 0x80)
		{
			pofs = ofs;
			++idx;
			++ofs;
			continue;
		}

		// invalid, skip u8_analyze
		if (s[ofs] <= 0xC2)
		{
			++ofs;
			continue;
		}

		if (!u8_analyze((const char*)s+ofs, &st, &ln, NULL, U8_ANALYZE_INFINITY))
			return -1;
		// see if next char is after the bytemark
		if (ofs + st > i)
		{
			if (len)
				*len = i - pofs;
			return idx;
		}
		++idx;
		pofs = ofs + st;
		ofs += st + ln;
		// see if bytemark is within the char
		if (ofs > i)
		{
			if (len)
				*len = i - pofs;
			return idx;
		}
	}
	if (len) *len = 0;
	return idx;
}

/** Get the byte offset of the previous byte.
 * The result equals:
 * prevchar_pos = u8_byteofs(text, u8_charidx(text, thischar_pos, NULL) - 1, NULL)
 * @param _s      An utf-8 encoded string.
 * @param i       The current byte offset.
 * @return        The byte offset of the previous character
 */
size_t u8_prevbyte(const char *_s, size_t i)
{
	size_t st, ln;
	const unsigned char *s = (const unsigned char*)_s;
	size_t lastofs = 0;
	size_t ofs = 0;

	if (!utf8_enable.integer)
	{
		if (i > 0)
			return i-1;
		return 0;
	}

	while (ofs < i && s[ofs])
	{
		// ascii character, skip u8_analyze
		if (s[ofs] < 0x80)
		{
			lastofs = ofs++;
			continue;
		}

		// invalid, skip u8_analyze
		if (s[ofs] <= 0xC2)
		{
			++ofs;
			continue;
		}

		if (!u8_analyze((const char*)s+ofs, &st, &ln, NULL, U8_ANALYZE_INFINITY))
			return lastofs;
		if (ofs + st > i)
			return lastofs;
		if (ofs + st + ln >= i)
			return ofs + st;

		lastofs = ofs;
		ofs += st + ln;
	}
	return lastofs;
}

static int char_usefont[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // specials
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // specials
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // shift+digit line
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // digits
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // caps
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // caps
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // small
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // small
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // specials
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // faces
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


/** Fetch a character from an utf-8 encoded string.
 * @param _s      The start of an utf-8 encoded multi-byte character.
 * @param _end    Will point to after the first multi-byte character.
 * @return        The 32-bit integer representation of the first multi-byte character or 0 for invalid characters.
 */
Uchar u8_getchar(const char *_s, const char **_end)
{
	size_t st, ln;
	Uchar ch;

	if (!utf8_enable.integer)
	{
		if (_end)
			*_end = _s + 1;
		/* Careful: if we disable utf8 but not freetype, we wish to see freetype chars
		 * for normal letters. So use E000+x for special chars, but leave the freetype stuff for the
		 * rest:
		 */
		if (!char_usefont[(unsigned int)*(const unsigned char*)_s])
			return 0xE000 + (Uchar)*(const unsigned char*)_s;
		return (Uchar)*(const unsigned char*)_s;
	}
	
	if (!u8_analyze(_s, &st, &ln, &ch, U8_ANALYZE_INFINITY))
		return 0;
	if (_end)
		*_end = _s + st + ln;
	return ch;
}

/** Fetch a character from an utf-8 encoded string.
 * @param _s      The start of an utf-8 encoded multi-byte character.
 * @param _end    Will point to after the first multi-byte character.
 * @return        The 32-bit integer representation of the first multi-byte character or 0 for invalid characters.
 */
Uchar u8_getnchar(const char *_s, const char **_end, size_t _maxlen)
{
	size_t st, ln;
	Uchar ch;

	if (!utf8_enable.integer)
	{
		if (_end)
			*_end = _s + 1;
		/* Careful: if we disable utf8 but not freetype, we wish to see freetype chars
		 * for normal letters. So use E000+x for special chars, but leave the freetype stuff for the
		 * rest:
		 */
		if (!char_usefont[(unsigned int)*(const unsigned char*)_s])
			return 0xE000 + (Uchar)*(const unsigned char*)_s;
		return (Uchar)*(const unsigned char*)_s;
	}
	
	if (!u8_analyze(_s, &st, &ln, &ch, _maxlen))
		return 0;
	if (_end)
		*_end = _s + st + ln;
	return ch;
}

/** Encode a wide-character into utf-8.
 * @param w        The wide character to encode.
 * @param to       The target buffer the utf-8 encoded string is stored to.
 * @param maxlen   The maximum number of bytes that fit into the target buffer.
 * @return         Number of bytes written to the buffer not including the terminating null.
 *                 Less or equal to 0 if the buffer is too small.
 */
int u8_fromchar(Uchar w, char *to, size_t maxlen)
{
	if (maxlen < 1)
		return -2;

	if (!w)
		return -5;

	if (w >= 0xE000 && !utf8_enable.integer)
		w -= 0xE000;

	if (w < 0x80 || !utf8_enable.integer)
	{
		to[0] = (char)w;
		if (maxlen < 2)
			return -1;
		to[1] = 0;
		return 1;
	}
	// for a little speedup
	if (w < 0x800)
	{
		if (maxlen < 3)
		{
			to[0] = 0;
			return -1;
		}
		to[2] = 0;
		to[1] = 0x80 | (w & 0x3F); w >>= 6;
		to[0] = 0xC0 | w;
		return 2;
	}
	if (w < 0x10000)
	{
		if (maxlen < 4)
		{
			to[0] = 0;
			return -1;
		}
		to[3] = 0;
		to[2] = 0x80 | (w & 0x3F); w >>= 6;
		to[1] = 0x80 | (w & 0x3F); w >>= 6;
		to[0] = 0xE0 | w;
		return 3;
	}

	// RFC 3629
	if (w <= 0x10FFFF)
	{
		if (maxlen < 5)
		{
			to[0] = 0;
			return -1;
		}
		to[4] = 0;
		to[3] = 0x80 | (w & 0x3F); w >>= 6;
		to[2] = 0x80 | (w & 0x3F); w >>= 6;
		to[1] = 0x80 | (w & 0x3F); w >>= 6;
		to[0] = 0xE0 | w;
		return 4;
	}
	return -1;
}

/** uses u8_fromchar on a static buffer
 * @param ch        The unicode character to convert to encode
 * @param l         The number of bytes without the terminating null.
 * @return          A statically allocated buffer containing the character's utf8 representation, or NULL if it fails.
 */
char *u8_encodech(Uchar ch, size_t *l)
{
	static char buf[16];
	size_t len;
	len = u8_fromchar(ch, buf, sizeof(buf));
	if (len > 0)
	{
		if (l) *l = len;
		return buf;
	}
	return NULL;
}

/** Convert a utf-8 multibyte string to a wide character string.
 * @param wcs       The target wide-character buffer.
 * @param mb        The utf-8 encoded multibyte string to convert.
 * @param maxlen    The maximum number of wide-characters that fit into the target buffer.
 * @return          The number of characters written to the target buffer.
 */
size_t u8_mbstowcs(Uchar *wcs, const char *mb, size_t maxlen)
{
	size_t i;
	Uchar ch;
	if (maxlen < 1)
		return 0;
	for (i = 0; *mb && i < maxlen-1; ++i)
	{
		ch = u8_getchar(mb, &mb);
		if (!ch)
			break;
		wcs[i] = ch;
	}
	wcs[i] = 0;
	return i;
}

/** Convert a wide-character string to a utf-8 multibyte string.
 * @param mb      The target buffer the utf-8 string is written to.
 * @param wcs     The wide-character string to convert.
 * @param maxlen  The number bytes that fit into the multibyte target buffer.
 * @return        The number of bytes written, not including the terminating \0
 */
size_t u8_wcstombs(char *mb, const Uchar *wcs, size_t maxlen)
{
	size_t i;
	const char *start = mb;
	if (maxlen < 2)
		return 0;
	for (i = 0; wcs[i] && i < maxlen-1; ++i)
	{
		int len;
		if ( (len = u8_fromchar(wcs[i], mb, maxlen - i)) < 0)
			return (mb - start);
		mb += len;
	}
	*mb = 0;
	return (mb - start);
}

/*
============
UTF-8 aware COM_StringLengthNoColors

calculates the visible width of a color coded string.

*valid is filled with TRUE if the string is a valid colored string (that is, if
it does not end with an unfinished color code). If it gets filled with FALSE, a
fix would be adding a STRING_COLOR_TAG at the end of the string.

valid can be set to NULL if the caller doesn't care.

For size_s, specify the maximum number of characters from s to use, or 0 to use
all characters until the zero terminator.
============
*/
size_t
COM_StringLengthNoColors(const char *s, size_t size_s, qboolean *valid);
size_t
u8_COM_StringLengthNoColors(const char *s, size_t size_s, qboolean *valid)
{
	const char *end;
	size_t len = 0;

	if (!utf8_enable.integer)
		return COM_StringLengthNoColors(s, size_s, valid);

	end = size_s ? (s + size_s) : NULL;

	for(;;)
	{
		switch((s == end) ? 0 : *s)
		{
			case 0:
				if(valid)
					*valid = TRUE;
				return len;
			case STRING_COLOR_TAG:
				++s;
				switch((s == end) ? 0 : *s)
				{
					case STRING_COLOR_RGB_TAG_CHAR:
						if (s+1 != end && isxdigit(s[1]) &&
							s+2 != end && isxdigit(s[2]) &&
							s+3 != end && isxdigit(s[3]) )
						{
							s+=3;
							break;
						}
						++len; // STRING_COLOR_TAG
						++len; // STRING_COLOR_RGB_TAG_CHAR
						break;
					case 0: // ends with unfinished color code!
						++len;
						if(valid)
							*valid = FALSE;
						return len;
					case STRING_COLOR_TAG: // escaped ^
						++len;
						break;
					case '0': case '1': case '2': case '3': case '4':
					case '5': case '6': case '7': case '8': case '9': // color code
						break;
					default: // not a color code
						++len; // STRING_COLOR_TAG
						++len; // the character
						break;
				}
				break;
			default:
				++len;
				break;
		}

		// start of a wide character
		if (*s & 0xC0)
		{
			for (++s; *s >= 0x80 && *s <= 0xC0; ++s);
			continue;
		}
		// part of a wide character, we ignore that one
		if (*s <= 0xBF)
			--len;
		++s;
	}
	// never get here
}

/** Pads a utf-8 string
 * @param out     The target buffer the utf-8 string is written to.
 * @param outsize The size of the target buffer, including the final NUL
 * @param in      The input utf-8 buffer
 * @param leftalign Left align the output string (by default right alignment is done)
 * @param minwidth The minimum output width
 * @param maxwidth The maximum output width
 * @return        The number of bytes written, not including the terminating \0
 */
size_t u8_strpad(char *out, size_t outsize, const char *in, qboolean leftalign, size_t minwidth, size_t maxwidth)
{
	if(!utf8_enable.integer)
	{
		return dpsnprintf(out, outsize, "%*.*s", leftalign ? -(int) minwidth : (int) minwidth, (int) maxwidth, in);
	}
	else
	{
		size_t l = u8_bytelen(in, maxwidth);
		size_t actual_width = u8_strnlen(in, l);
		int pad = (actual_width >= minwidth) ? 0 : (minwidth - actual_width);
		int prec = l;
		int lpad = leftalign ? 0 : pad;
		int rpad = leftalign ? pad : 0;
		return dpsnprintf(out, outsize, "%*s%.*s%*s", lpad, "", prec, in, rpad, "");
	}
}
