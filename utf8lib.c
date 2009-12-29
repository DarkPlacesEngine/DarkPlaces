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
	while (i < _maxlen && s[i] && s[i] >= 0x80 && s[i] < 0xC2) {
		//fprintf(stderr, "skipping\n");
		++i;
	}

	//fprintf(stderr, "checking\n");
	// If we hit the end, well, we're out and invalid
	if(i >= _maxlen || !s[i]) {
		if (_start) *_start = i;
		if (_len) *_len = 0;
		return false;
	}

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
	if(i + bits > _maxlen) {
		/*
		if (_start) *_start = i;
		if (_len) *_len = 0;
		return false;
		*/
		++i;
		goto findchar;
	}
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
		if (*s < 0xC2)
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
		if (*s < 0xC2)
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

	if (!utf8_enable.integer) {
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
		if (*s < 0xC2)
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
		if (strlen(_s) < i)
		{
			if (len) *len = 0;
			return -1;
		}

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
		if (s[ofs] < 0xC2)
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
		if (s[ofs] < 0xC2)
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

Uchar u8_quake2utf8map[256] = {
	0xE000, 0xE001, 0xE002, 0xE003, 0xE004, 0xE005, 0xE006, 0xE007, 0xE008, 0xE009, 0xE00A, 0xE00B, 0xE00C, 0xE00D, 0xE00E, 0xE00F, // specials
	0xE010, 0xE011, 0xE012, 0xE013, 0xE014, 0xE015, 0xE016, 0xE017, 0xE018, 0xE019, 0xE01A, 0xE01B, 0xE01C, 0xE01D, 0xE01E, 0xE01F, // specials
	0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F, // shift+digit line
	0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F, // digits
	0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F, // caps
	0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F, // caps
	0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F, // small
	0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x007F, // small
	0xE080, 0xE081, 0xE082, 0xE083, 0xE084, 0xE085, 0xE086, 0xE087, 0xE088, 0xE089, 0xE08A, 0xE08B, 0xE08C, 0xE08D, 0xE08E, 0xE08F, // specials
	0xE090, 0xE091, 0xE092, 0xE093, 0xE094, 0xE095, 0xE096, 0xE097, 0xE098, 0xE099, 0xE09A, 0xE09B, 0xE09C, 0xE09D, 0xE09E, 0xE09F, // faces
	0xE0A0, 0xE0A1, 0xE0A2, 0xE0A3, 0xE0A4, 0xE0A5, 0xE0A6, 0xE0A7, 0xE0A8, 0xE0A9, 0xE0AA, 0xE0AB, 0xE0AC, 0xE0AD, 0xE0AE, 0xE0AF,
	0xE0B0, 0xE0B1, 0xE0B2, 0xE0B3, 0xE0B4, 0xE0B5, 0xE0B6, 0xE0B7, 0xE0B8, 0xE0B9, 0xE0BA, 0xE0BB, 0xE0BC, 0xE0BD, 0xE0BE, 0xE0BF,
	0xE0C0, 0xE0C1, 0xE0C2, 0xE0C3, 0xE0C4, 0xE0C5, 0xE0C6, 0xE0C7, 0xE0C8, 0xE0C9, 0xE0CA, 0xE0CB, 0xE0CC, 0xE0CD, 0xE0CE, 0xE0CF,
	0xE0D0, 0xE0D1, 0xE0D2, 0xE0D3, 0xE0D4, 0xE0D5, 0xE0D6, 0xE0D7, 0xE0D8, 0xE0D9, 0xE0DA, 0xE0DB, 0xE0DC, 0xE0DD, 0xE0DE, 0xE0DF,
	0xE0E0, 0xE0E1, 0xE0E2, 0xE0E3, 0xE0E4, 0xE0E5, 0xE0E6, 0xE0E7, 0xE0E8, 0xE0E9, 0xE0EA, 0xE0EB, 0xE0EC, 0xE0ED, 0xE0EE, 0xE0EF,
	0xE0F0, 0xE0F1, 0xE0F2, 0xE0F3, 0xE0F4, 0xE0F5, 0xE0F6, 0xE0F7, 0xE0F8, 0xE0F9, 0xE0FA, 0xE0FB, 0xE0FC, 0xE0FD, 0xE0FE, 0xE0FF,
};

/** Fetch a character from an utf-8 encoded string.
 * @param _s      The start of an utf-8 encoded multi-byte character.
 * @param _end    Will point to after the first multi-byte character.
 * @return        The 32-bit integer representation of the first multi-byte character or 0 for invalid characters.
 */
Uchar u8_getchar_utf8_enabled(const char *_s, const char **_end)
{
	size_t st, ln;
	Uchar ch;

	if (!u8_analyze(_s, &st, &ln, &ch, U8_ANALYZE_INFINITY))
		ch = 0;
	if (_end)
		*_end = _s + st + ln;
	return ch;
}

/** Fetch a character from an utf-8 encoded string.
 * @param _s      The start of an utf-8 encoded multi-byte character.
 * @param _end    Will point to after the first multi-byte character.
 * @return        The 32-bit integer representation of the first multi-byte character or 0 for invalid characters.
 */
Uchar u8_getnchar_utf8_enabled(const char *_s, const char **_end, size_t _maxlen)
{
	size_t st, ln;
	Uchar ch;

	if (!u8_analyze(_s, &st, &ln, &ch, _maxlen))
		ch = 0;
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
		return 0;

	if (!w)
		return 0;

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
	return 0;
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
		/*
		int len;
		if ( (len = u8_fromchar(wcs[i], mb, maxlen - i)) < 0)
			return (mb - start);
		mb += len;
		*/
		mb += u8_fromchar(wcs[i], mb, maxlen - i);
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
u8_COM_StringLengthNoColors(const char *_s, size_t size_s, qboolean *valid)
{
	const unsigned char *s = (const unsigned char*)_s;
	const unsigned char *end;
	size_t len = 0;

	if (!utf8_enable.integer)
		return COM_StringLengthNoColors(_s, size_s, valid);

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
