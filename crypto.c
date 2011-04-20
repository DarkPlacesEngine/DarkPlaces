// TODO key loading, generating, saving
#include "quakedef.h"
#include "crypto.h"
#include "common.h"

#include "hmac.h"
#include "libcurl.h"

cvar_t crypto_developer = {CVAR_SAVE, "crypto_developer", "0", "print extra info about crypto handshake"};
cvar_t crypto_servercpupercent = {CVAR_SAVE, "crypto_servercpupercent", "10", "allowed crypto CPU load in percent for server operation (0 = no limit, faster)"};
cvar_t crypto_servercpumaxtime = {CVAR_SAVE, "crypto_servercpumaxtime", "0.01", "maximum allowed crypto CPU time per frame (0 = no limit)"};
cvar_t crypto_servercpudebug = {CVAR_SAVE, "crypto_servercpudebug", "0", "print statistics about time usage by crypto"};
static double crypto_servercpu_accumulator = 0;
static double crypto_servercpu_lastrealtime = 0;
cvar_t crypto_aeslevel = {CVAR_SAVE, "crypto_aeslevel", "1", "whether to support AES encryption in authenticated connections (0 = no, 1 = supported, 2 = requested, 3 = required)"};
int crypto_keyfp_recommended_length;
static const char *crypto_idstring = NULL;
static char crypto_idstring_buf[512];

#define PROTOCOL_D0_BLIND_ID FOURCC_D0PK
#define PROTOCOL_VLEN (('v' << 0) | ('l' << 8) | ('e' << 16) | ('n' << 24))

// BEGIN stuff shared with crypto-keygen-standalone
#define FOURCC_D0PK (('d' << 0) | ('0' << 8) | ('p' << 16) | ('k' << 24))
#define FOURCC_D0SK (('d' << 0) | ('0' << 8) | ('s' << 16) | ('k' << 24))
#define FOURCC_D0PI (('d' << 0) | ('0' << 8) | ('p' << 16) | ('i' << 24))
#define FOURCC_D0SI (('d' << 0) | ('0' << 8) | ('s' << 16) | ('i' << 24))
#define FOURCC_D0IQ (('d' << 0) | ('0' << 8) | ('i' << 16) | ('q' << 24))
#define FOURCC_D0IR (('d' << 0) | ('0' << 8) | ('i' << 16) | ('r' << 24))
#define FOURCC_D0ER (('d' << 0) | ('0' << 8) | ('e' << 16) | ('r' << 24))
#define FOURCC_D0IC (('d' << 0) | ('0' << 8) | ('i' << 16) | ('c' << 24))

static unsigned long Crypto_LittleLong(const char *data)
{
	return
		((unsigned char) data[0]) |
		(((unsigned char) data[1]) << 8) |
		(((unsigned char) data[2]) << 16) |
		(((unsigned char) data[3]) << 24);
}

static void Crypto_UnLittleLong(char *data, unsigned long l)
{
	data[0] = l & 0xFF;
	data[1] = (l >> 8) & 0xFF;
	data[2] = (l >> 16) & 0xFF;
	data[3] = (l >> 24) & 0xFF;
}

static size_t Crypto_ParsePack(const char *buf, size_t len, unsigned long header, const char **lumps, size_t *lumpsize, size_t nlumps)
{
	size_t i;
	size_t pos;
	pos = 0;
	if(header)
	{
		if(len < 4)
			return 0;
		if(Crypto_LittleLong(buf) != header)
			return 0;
		pos += 4;
	}
	for(i = 0; i < nlumps; ++i)
	{
		if(pos + 4 > len)
			return 0;
		lumpsize[i] = Crypto_LittleLong(&buf[pos]);
		pos += 4;
		if(pos + lumpsize[i] > len)
			return 0;
		lumps[i] = &buf[pos];
		pos += lumpsize[i];
	}
	return pos;
}

static size_t Crypto_UnParsePack(char *buf, size_t len, unsigned long header, const char *const *lumps, const size_t *lumpsize, size_t nlumps)
{
	size_t i;
	size_t pos;
	pos = 0;
	if(header)
	{
		if(len < 4)
			return 0;
		Crypto_UnLittleLong(buf, header);
		pos += 4;
	}
	for(i = 0; i < nlumps; ++i)
	{
		if(pos + 4 + lumpsize[i] > len)
			return 0;
		Crypto_UnLittleLong(&buf[pos], lumpsize[i]);
		pos += 4;
		memcpy(&buf[pos], lumps[i], lumpsize[i]);
		pos += lumpsize[i];
	}
	return pos;
}
// END stuff shared with xonotic-keygen

#define USE_AES

#ifdef CRYPTO_STATIC

#include <d0_blind_id/d0_blind_id.h>

#define d0_blind_id_dll 1
#define Crypto_OpenLibrary() true
#define Crypto_CloseLibrary()

#define qd0_blind_id_new d0_blind_id_new
#define qd0_blind_id_free d0_blind_id_free
//#define qd0_blind_id_clear d0_blind_id_clear
#define qd0_blind_id_copy d0_blind_id_copy
//#define qd0_blind_id_generate_private_key d0_blind_id_generate_private_key
//#define qd0_blind_id_generate_private_key_fastreject d0_blind_id_generate_private_key_fastreject
//#define qd0_blind_id_read_private_key d0_blind_id_read_private_key
#define qd0_blind_id_read_public_key d0_blind_id_read_public_key
//#define qd0_blind_id_write_private_key d0_blind_id_write_private_key
//#define qd0_blind_id_write_public_key d0_blind_id_write_public_key
#define qd0_blind_id_fingerprint64_public_key d0_blind_id_fingerprint64_public_key
//#define qd0_blind_id_generate_private_id_modulus d0_blind_id_generate_private_id_modulus
#define qd0_blind_id_read_private_id_modulus d0_blind_id_read_private_id_modulus
//#define qd0_blind_id_write_private_id_modulus d0_blind_id_write_private_id_modulus
#define qd0_blind_id_generate_private_id_start d0_blind_id_generate_private_id_start
#define qd0_blind_id_generate_private_id_request d0_blind_id_generate_private_id_request
//#define qd0_blind_id_answer_private_id_request d0_blind_id_answer_private_id_request
#define qd0_blind_id_finish_private_id_request d0_blind_id_finish_private_id_request
//#define qd0_blind_id_read_private_id_request_camouflage d0_blind_id_read_private_id_request_camouflage
//#define qd0_blind_id_write_private_id_request_camouflage d0_blind_id_write_private_id_request_camouflage
#define qd0_blind_id_read_private_id d0_blind_id_read_private_id
//#define qd0_blind_id_read_public_id d0_blind_id_read_public_id
#define qd0_blind_id_write_private_id d0_blind_id_write_private_id
//#define qd0_blind_id_write_public_id d0_blind_id_write_public_id
#define qd0_blind_id_authenticate_with_private_id_start d0_blind_id_authenticate_with_private_id_start
#define qd0_blind_id_authenticate_with_private_id_challenge d0_blind_id_authenticate_with_private_id_challenge
#define qd0_blind_id_authenticate_with_private_id_response d0_blind_id_authenticate_with_private_id_response
#define qd0_blind_id_authenticate_with_private_id_verify d0_blind_id_authenticate_with_private_id_verify
#define qd0_blind_id_fingerprint64_public_id d0_blind_id_fingerprint64_public_id
#define qd0_blind_id_sessionkey_public_id d0_blind_id_sessionkey_public_id
#define qd0_blind_id_INITIALIZE d0_blind_id_INITIALIZE
#define qd0_blind_id_SHUTDOWN d0_blind_id_SHUTDOWN
#define qd0_blind_id_util_sha256 d0_blind_id_util_sha256
#define qd0_blind_id_sign_with_private_id_sign d0_blind_id_sign_with_private_id_sign
#define qd0_blind_id_sign_with_private_id_sign_detached d0_blind_id_sign_with_private_id_sign_detached

#else

// d0_blind_id interface
#define D0_EXPORT
#ifdef __GNUC__
#define D0_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define D0_WARN_UNUSED_RESULT
#endif
#define D0_BOOL int

typedef struct d0_blind_id_s d0_blind_id_t;
typedef D0_BOOL (*d0_fastreject_function) (const d0_blind_id_t *ctx, void *pass);
static D0_EXPORT D0_WARN_UNUSED_RESULT d0_blind_id_t *(*qd0_blind_id_new) (void);
static D0_EXPORT void (*qd0_blind_id_free) (d0_blind_id_t *a);
//static D0_EXPORT void (*qd0_blind_id_clear) (d0_blind_id_t *ctx);
static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_copy) (d0_blind_id_t *ctx, const d0_blind_id_t *src);
//static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_generate_private_key) (d0_blind_id_t *ctx, int k);
//static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_generate_private_key_fastreject) (d0_blind_id_t *ctx, int k, d0_fastreject_function reject, void *pass);
//static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_read_private_key) (d0_blind_id_t *ctx, const char *inbuf, size_t inbuflen);
static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_read_public_key) (d0_blind_id_t *ctx, const char *inbuf, size_t inbuflen);
//static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_write_private_key) (const d0_blind_id_t *ctx, char *outbuf, size_t *outbuflen);
//static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_write_public_key) (const d0_blind_id_t *ctx, char *outbuf, size_t *outbuflen);
static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_fingerprint64_public_key) (const d0_blind_id_t *ctx, char *outbuf, size_t *outbuflen);
//static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_generate_private_id_modulus) (d0_blind_id_t *ctx);
static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_read_private_id_modulus) (d0_blind_id_t *ctx, const char *inbuf, size_t inbuflen);
//static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_write_private_id_modulus) (const d0_blind_id_t *ctx, char *outbuf, size_t *outbuflen);
static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_generate_private_id_start) (d0_blind_id_t *ctx);
static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_generate_private_id_request) (d0_blind_id_t *ctx, char *outbuf, size_t *outbuflen);
//static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_answer_private_id_request) (const d0_blind_id_t *ctx, const char *inbuf, size_t inbuflen, char *outbuf, size_t *outbuflen);
static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_finish_private_id_request) (d0_blind_id_t *ctx, const char *inbuf, size_t inbuflen);
//static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_read_private_id_request_camouflage) (d0_blind_id_t *ctx, const char *inbuf, size_t inbuflen);
//static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_write_private_id_request_camouflage) (const d0_blind_id_t *ctx, char *outbuf, size_t *outbuflen);
static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_read_private_id) (d0_blind_id_t *ctx, const char *inbuf, size_t inbuflen);
//static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_read_public_id) (d0_blind_id_t *ctx, const char *inbuf, size_t inbuflen);
static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_write_private_id) (const d0_blind_id_t *ctx, char *outbuf, size_t *outbuflen);
//static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_write_public_id) (const d0_blind_id_t *ctx, char *outbuf, size_t *outbuflen);
static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_authenticate_with_private_id_start) (d0_blind_id_t *ctx, D0_BOOL is_first, D0_BOOL send_modulus, const char *message, size_t msglen, char *outbuf, size_t *outbuflen);
static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_authenticate_with_private_id_challenge) (d0_blind_id_t *ctx, D0_BOOL is_first, D0_BOOL recv_modulus, const char *inbuf, size_t inbuflen, char *outbuf, size_t *outbuflen, D0_BOOL *status);
static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_authenticate_with_private_id_response) (d0_blind_id_t *ctx, const char *inbuf, size_t inbuflen, char *outbuf, size_t *outbuflen);
static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_authenticate_with_private_id_verify) (d0_blind_id_t *ctx, const char *inbuf, size_t inbuflen, char *msg, size_t *msglen, D0_BOOL *status);
static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_fingerprint64_public_id) (const d0_blind_id_t *ctx, char *outbuf, size_t *outbuflen);
static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_sessionkey_public_id) (const d0_blind_id_t *ctx, char *outbuf, size_t *outbuflen); // can only be done after successful key exchange, this performs a modpow; key length is limited by SHA_DIGESTSIZE for now; also ONLY valid after successful d0_blind_id_authenticate_with_private_id_verify/d0_blind_id_fingerprint64_public_id
static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_INITIALIZE) (void);
static D0_EXPORT void (*qd0_blind_id_SHUTDOWN) (void);
static D0_EXPORT void (*qd0_blind_id_util_sha256) (char *out, const char *in, size_t n);
static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_sign_with_private_id_sign) (d0_blind_id_t *ctx, D0_BOOL is_first, D0_BOOL send_modulus, const char *message, size_t msglen, char *outbuf, size_t *outbuflen);
static D0_EXPORT D0_WARN_UNUSED_RESULT D0_BOOL (*qd0_blind_id_sign_with_private_id_sign_detached) (d0_blind_id_t *ctx, D0_BOOL is_first, D0_BOOL send_modulus, const char *message, size_t msglen, char *outbuf, size_t *outbuflen);
static dllfunction_t d0_blind_id_funcs[] =
{
	{"d0_blind_id_new", (void **) &qd0_blind_id_new},
	{"d0_blind_id_free", (void **) &qd0_blind_id_free},
	//{"d0_blind_id_clear", (void **) &qd0_blind_id_clear},
	{"d0_blind_id_copy", (void **) &qd0_blind_id_copy},
	//{"d0_blind_id_generate_private_key", (void **) &qd0_blind_id_generate_private_key},
	//{"d0_blind_id_generate_private_key_fastreject", (void **) &qd0_blind_id_generate_private_key_fastreject},
	//{"d0_blind_id_read_private_key", (void **) &qd0_blind_id_read_private_key},
	{"d0_blind_id_read_public_key", (void **) &qd0_blind_id_read_public_key},
	//{"d0_blind_id_write_private_key", (void **) &qd0_blind_id_write_private_key},
	//{"d0_blind_id_write_public_key", (void **) &qd0_blind_id_write_public_key},
	{"d0_blind_id_fingerprint64_public_key", (void **) &qd0_blind_id_fingerprint64_public_key},
	//{"d0_blind_id_generate_private_id_modulus", (void **) &qd0_blind_id_generate_private_id_modulus},
	{"d0_blind_id_read_private_id_modulus", (void **) &qd0_blind_id_read_private_id_modulus},
	//{"d0_blind_id_write_private_id_modulus", (void **) &qd0_blind_id_write_private_id_modulus},
	{"d0_blind_id_generate_private_id_start", (void **) &qd0_blind_id_generate_private_id_start},
	{"d0_blind_id_generate_private_id_request", (void **) &qd0_blind_id_generate_private_id_request},
	//{"d0_blind_id_answer_private_id_request", (void **) &qd0_blind_id_answer_private_id_request},
	{"d0_blind_id_finish_private_id_request", (void **) &qd0_blind_id_finish_private_id_request},
	//{"d0_blind_id_read_private_id_request_camouflage", (void **) &qd0_blind_id_read_private_id_request_camouflage},
	//{"d0_blind_id_write_private_id_request_camouflage", (void **) &qd0_blind_id_write_private_id_request_camouflage},
	{"d0_blind_id_read_private_id", (void **) &qd0_blind_id_read_private_id},
	//{"d0_blind_id_read_public_id", (void **) &qd0_blind_id_read_public_id},
	{"d0_blind_id_write_private_id", (void **) &qd0_blind_id_write_private_id},
	//{"d0_blind_id_write_public_id", (void **) &qd0_blind_id_write_public_id},
	{"d0_blind_id_authenticate_with_private_id_start", (void **) &qd0_blind_id_authenticate_with_private_id_start},
	{"d0_blind_id_authenticate_with_private_id_challenge", (void **) &qd0_blind_id_authenticate_with_private_id_challenge},
	{"d0_blind_id_authenticate_with_private_id_response", (void **) &qd0_blind_id_authenticate_with_private_id_response},
	{"d0_blind_id_authenticate_with_private_id_verify", (void **) &qd0_blind_id_authenticate_with_private_id_verify},
	{"d0_blind_id_fingerprint64_public_id", (void **) &qd0_blind_id_fingerprint64_public_id},
	{"d0_blind_id_sessionkey_public_id", (void **) &qd0_blind_id_sessionkey_public_id},
	{"d0_blind_id_INITIALIZE", (void **) &qd0_blind_id_INITIALIZE},
	{"d0_blind_id_SHUTDOWN", (void **) &qd0_blind_id_SHUTDOWN},
	{"d0_blind_id_util_sha256", (void **) &qd0_blind_id_util_sha256},
	{"d0_blind_id_sign_with_private_id_sign", (void **) &qd0_blind_id_sign_with_private_id_sign},
	{"d0_blind_id_sign_with_private_id_sign_detached", (void **) &qd0_blind_id_sign_with_private_id_sign_detached},
	{NULL, NULL}
};
// end of d0_blind_id interface

static dllhandle_t d0_blind_id_dll = NULL;
static qboolean Crypto_OpenLibrary (void)
{
	const char* dllnames [] =
	{
#if defined(WIN32)
		"libd0_blind_id-0.dll",
#elif defined(MACOSX)
		"libd0_blind_id.0.dylib",
#else
		"libd0_blind_id.so.0",
		"libd0_blind_id.so", // FreeBSD
#endif
		NULL
	};

	// Already loaded?
	if (d0_blind_id_dll)
		return true;

	// Load the DLL
	return Sys_LoadLibrary (dllnames, &d0_blind_id_dll, d0_blind_id_funcs);
}

static void Crypto_CloseLibrary (void)
{
	Sys_UnloadLibrary (&d0_blind_id_dll);
}

#endif

#ifdef CRYPTO_RIJNDAEL_STATIC

#include <d0_blind_id/d0_rijndael.h>

#define d0_rijndael_dll 1
#define Crypto_Rijndael_OpenLibrary() true
#define Crypto_Rijndael_CloseLibrary()

#define qd0_rijndael_setup_encrypt d0_rijndael_setup_encrypt
#define qd0_rijndael_setup_decrypt d0_rijndael_setup_decrypt
#define qd0_rijndael_encrypt d0_rijndael_encrypt
#define qd0_rijndael_decrypt d0_rijndael_decrypt

#else

// no need to do the #define dance here, as the upper part declares out macros either way

D0_EXPORT int (*qd0_rijndael_setup_encrypt) (unsigned long *rk, const unsigned char *key,
  int keybits);
D0_EXPORT int (*qd0_rijndael_setup_decrypt) (unsigned long *rk, const unsigned char *key,
  int keybits);
D0_EXPORT void (*qd0_rijndael_encrypt) (const unsigned long *rk, int nrounds,
  const unsigned char plaintext[16], unsigned char ciphertext[16]);
D0_EXPORT void (*qd0_rijndael_decrypt) (const unsigned long *rk, int nrounds,
  const unsigned char ciphertext[16], unsigned char plaintext[16]);
#define D0_RIJNDAEL_KEYLENGTH(keybits) ((keybits)/8)
#define D0_RIJNDAEL_RKLENGTH(keybits)  ((keybits)/8+28)
#define D0_RIJNDAEL_NROUNDS(keybits)   ((keybits)/32+6)
static dllfunction_t d0_rijndael_funcs[] =
{
	{"d0_rijndael_setup_decrypt", (void **) &qd0_rijndael_setup_decrypt},
	{"d0_rijndael_setup_encrypt", (void **) &qd0_rijndael_setup_encrypt},
	{"d0_rijndael_decrypt", (void **) &qd0_rijndael_decrypt},
	{"d0_rijndael_encrypt", (void **) &qd0_rijndael_encrypt},
	{NULL, NULL}
};
// end of d0_blind_id interface

static dllhandle_t d0_rijndael_dll = NULL;
static qboolean Crypto_Rijndael_OpenLibrary (void)
{
	const char* dllnames [] =
	{
#if defined(WIN32)
		"libd0_rijndael-0.dll",
#elif defined(MACOSX)
		"libd0_rijndael.0.dylib",
#else
		"libd0_rijndael.so.0",
		"libd0_rijndael.so", // FreeBSD
#endif
		NULL
	};

	// Already loaded?
	if (d0_rijndael_dll)
		return true;

	// Load the DLL
	return Sys_LoadLibrary (dllnames, &d0_rijndael_dll, d0_rijndael_funcs);
}

static void Crypto_Rijndael_CloseLibrary (void)
{
	Sys_UnloadLibrary (&d0_rijndael_dll);
}

#endif

// various helpers
void sha256(unsigned char *out, const unsigned char *in, int n)
{
	qd0_blind_id_util_sha256((char *) out, (const char *) in, n);
}

static size_t Crypto_LoadFile(const char *path, char *buf, size_t nmax)
{
	qfile_t *f = NULL;
	fs_offset_t n;
	if(*fs_userdir)
		f = FS_SysOpen(va("%s%s", fs_userdir, path), "rb", false);
	if(!f)
		f = FS_SysOpen(va("%s%s", fs_basedir, path), "rb", false);
	if(!f)
		return 0;
	n = FS_Read(f, buf, nmax);
	if(n < 0)
		n = 0;
	FS_Close(f);
	return (size_t) n;
}

static qboolean PutWithNul(char **data, size_t *len, const char *str)
{
	// invariant: data points to insertion point
	size_t l = strlen(str);
	if(l >= *len)
		return false;
	memcpy(*data, str, l+1);
	*data += l+1;
	*len -= l+1;
	return true;
}

static const char *GetUntilNul(const char **data, size_t *len)
{
	// invariant: data points to next character to take
	const char *data_save = *data;
	size_t n;
	const char *p;

	if(!*data)
		return NULL;

	if(!*len)
	{
		*data = NULL;
		return NULL;
	}

	p = (const char *) memchr(*data, 0, *len);
	if(!p) // no terminating NUL
	{
		*data = NULL;
		*len = 0;
		return NULL;
	}
	else
	{
		n = (p - *data) + 1;
		*len -= n;
		*data += n;
		if(*len == 0)
			*data = NULL;
		return (const char *) data_save;
	}
	*data = NULL;
	return NULL;
}

// d0pk reading
static d0_blind_id_t *Crypto_ReadPublicKey(char *buf, size_t len)
{
	d0_blind_id_t *pk = NULL;
	const char *p[2];
	size_t l[2];
	if(Crypto_ParsePack(buf, len, FOURCC_D0PK, p, l, 2))
	{
		pk = qd0_blind_id_new();
		if(pk)
			if(qd0_blind_id_read_public_key(pk, p[0], l[0]))
				if(qd0_blind_id_read_private_id_modulus(pk, p[1], l[1]))
					return pk;
	}
	if(pk)
		qd0_blind_id_free(pk);
	return NULL;
}

// d0si reading
static qboolean Crypto_AddPrivateKey(d0_blind_id_t *pk, char *buf, size_t len)
{
	const char *p[1];
	size_t l[1];
	if(Crypto_ParsePack(buf, len, FOURCC_D0SI, p, l, 1))
	{
		if(qd0_blind_id_read_private_id(pk, p[0], l[0]))
			return true;
	}
	return false;
}

#define MAX_PUBKEYS 16
static d0_blind_id_t *pubkeys[MAX_PUBKEYS];
static char pubkeys_fp64[MAX_PUBKEYS][FP64_SIZE+1];
static qboolean pubkeys_havepriv[MAX_PUBKEYS];
static char pubkeys_priv_fp64[MAX_PUBKEYS][FP64_SIZE+1];
static char challenge_append[1400];
static size_t challenge_append_length;

static int keygen_i = -1;
static char keygen_buf[8192];

#define MAX_CRYPTOCONNECTS 16
#define CRYPTOCONNECT_NONE 0
#define CRYPTOCONNECT_PRECONNECT 1
#define CRYPTOCONNECT_CONNECT 2
#define CRYPTOCONNECT_RECONNECT 3
#define CRYPTOCONNECT_DUPLICATE 4
typedef struct server_cryptoconnect_s
{
	double lasttime;
	lhnetaddress_t address;
	crypto_t crypto;
	int next_step;
}
server_cryptoconnect_t;
static server_cryptoconnect_t cryptoconnects[MAX_CRYPTOCONNECTS];

static int cdata_id = 0;
typedef struct
{
	d0_blind_id_t *id;
	int s, c;
	int next_step;
	char challenge[2048];
	char wantserver_idfp[FP64_SIZE+1];
	qboolean wantserver_aes;
	int cdata_id;
}
crypto_data_t;

// crypto specific helpers
#define CDATA ((crypto_data_t *) crypto->data)
#define MAKE_CDATA if(!crypto->data) crypto->data = Z_Malloc(sizeof(crypto_data_t))
#define CLEAR_CDATA if(crypto->data) { if(CDATA->id) qd0_blind_id_free(CDATA->id); Z_Free(crypto->data); } crypto->data = NULL

static crypto_t *Crypto_ServerFindInstance(lhnetaddress_t *peeraddress, qboolean allow_create)
{
	crypto_t *crypto; 
	int i, best;

	if(!d0_blind_id_dll)
		return NULL; // no support

	for(i = 0; i < MAX_CRYPTOCONNECTS; ++i)
		if(LHNETADDRESS_Compare(peeraddress, &cryptoconnects[i].address))
			break;
	if(i < MAX_CRYPTOCONNECTS && (allow_create || cryptoconnects[i].crypto.data))
	{
		crypto = &cryptoconnects[i].crypto;
		cryptoconnects[i].lasttime = realtime;
		return crypto;
	}
	if(!allow_create)
		return NULL;
	best = 0;
	for(i = 1; i < MAX_CRYPTOCONNECTS; ++i)
		if(cryptoconnects[i].lasttime < cryptoconnects[best].lasttime)
			best = i;
	crypto = &cryptoconnects[best].crypto;
	cryptoconnects[best].lasttime = realtime;
	memcpy(&cryptoconnects[best].address, peeraddress, sizeof(cryptoconnects[best].address));
	CLEAR_CDATA;
	return crypto;
}

qboolean Crypto_ServerFinishInstance(crypto_t *out, crypto_t *crypto)
{
	// no check needed here (returned pointers are only used in prefilled fields)
	if(!crypto || !crypto->authenticated)
	{
		Con_Printf("Passed an invalid crypto connect instance\n");
		memset(out, 0, sizeof(*out));
		return false;
	}
	CLEAR_CDATA;
	memcpy(out, crypto, sizeof(*out));
	memset(crypto, 0, sizeof(crypto));
	return true;
}

crypto_t *Crypto_ServerGetInstance(lhnetaddress_t *peeraddress)
{
	// no check needed here (returned pointers are only used in prefilled fields)
	return Crypto_ServerFindInstance(peeraddress, false);
}

typedef struct crypto_storedhostkey_s
{
	struct crypto_storedhostkey_s *next;
	lhnetaddress_t addr;
	int keyid;
	char idfp[FP64_SIZE+1];
	int aeslevel;
}
crypto_storedhostkey_t;
static crypto_storedhostkey_t *crypto_storedhostkey_hashtable[CRYPTO_HOSTKEY_HASHSIZE];

static void Crypto_InitHostKeys(void)
{
	int i;
	for(i = 0; i < CRYPTO_HOSTKEY_HASHSIZE; ++i)
		crypto_storedhostkey_hashtable[i] = NULL;
}

static void Crypto_ClearHostKeys(void)
{
	int i;
	crypto_storedhostkey_t *hk, *hkn;
	for(i = 0; i < CRYPTO_HOSTKEY_HASHSIZE; ++i)
	{
		for(hk = crypto_storedhostkey_hashtable[i]; hk; hk = hkn)
		{
			hkn = hk->next;
			Z_Free(hk);
		}
		crypto_storedhostkey_hashtable[i] = NULL;
	}
}

static qboolean Crypto_ClearHostKey(lhnetaddress_t *peeraddress)
{
	char buf[128];
	int hashindex;
	crypto_storedhostkey_t **hkp;
	qboolean found = false;

	LHNETADDRESS_ToString(peeraddress, buf, sizeof(buf), 1);
	hashindex = CRC_Block((const unsigned char *) buf, strlen(buf)) % CRYPTO_HOSTKEY_HASHSIZE;
	for(hkp = &crypto_storedhostkey_hashtable[hashindex]; *hkp && LHNETADDRESS_Compare(&((*hkp)->addr), peeraddress); hkp = &((*hkp)->next));

	if(*hkp)
	{
		crypto_storedhostkey_t *hk = *hkp;
		*hkp = hk->next;
		Z_Free(hk);
		found = true;
	}

	return found;
}

static void Crypto_StoreHostKey(lhnetaddress_t *peeraddress, const char *keystring, qboolean complain)
{
	char buf[128];
	int hashindex;
	crypto_storedhostkey_t *hk;
	int keyid;
	char idfp[FP64_SIZE+1];
	int aeslevel;

	if(!d0_blind_id_dll)
		return;
	
	// syntax of keystring:
	// aeslevel id@key id@key ...

	if(!*keystring)
		return;
	aeslevel = bound(0, *keystring - '0', 3);
	while(*keystring && *keystring != ' ')
		++keystring;

	keyid = -1;
	while(*keystring && keyid < 0)
	{
		// id@key
		const char *idstart, *idend, *keystart, *keyend;
		++keystring; // skip the space
		idstart = keystring;
		while(*keystring && *keystring != ' ' && *keystring != '@')
			++keystring;
		idend = keystring;
		if(!*keystring)
			break;
		++keystring;
		keystart = keystring;
		while(*keystring && *keystring != ' ')
			++keystring;
		keyend = keystring;

		if(idend - idstart == FP64_SIZE && keyend - keystart == FP64_SIZE)
		{
			for(keyid = 0; keyid < MAX_PUBKEYS; ++keyid)
				if(pubkeys[keyid])
					if(!memcmp(pubkeys_fp64[keyid], keystart, FP64_SIZE))
					{
						memcpy(idfp, idstart, FP64_SIZE);
						idfp[FP64_SIZE] = 0;
						break;
					}
			if(keyid >= MAX_PUBKEYS)
				keyid = -1;
		}
	}

	if(keyid < 0)
		return;

	LHNETADDRESS_ToString(peeraddress, buf, sizeof(buf), 1);
	hashindex = CRC_Block((const unsigned char *) buf, strlen(buf)) % CRYPTO_HOSTKEY_HASHSIZE;
	for(hk = crypto_storedhostkey_hashtable[hashindex]; hk && LHNETADDRESS_Compare(&hk->addr, peeraddress); hk = hk->next);

	if(hk)
	{
		if(complain)
		{
			if(hk->keyid != keyid || memcmp(hk->idfp, idfp, FP64_SIZE+1))
				Con_Printf("Server %s tried to change the host key to a value not in the host cache. Connecting to it will fail. To accept the new host key, do crypto_hostkey_clear %s\n", buf, buf);
			if(hk->aeslevel > aeslevel)
				Con_Printf("Server %s tried to reduce encryption status, not accepted. Connecting to it will fail. To accept, do crypto_hostkey_clear %s\n", buf, buf);
		}
		hk->aeslevel = max(aeslevel, hk->aeslevel);
		return;
	}

	// great, we did NOT have it yet
	hk = (crypto_storedhostkey_t *) Z_Malloc(sizeof(*hk));
	memcpy(&hk->addr, peeraddress, sizeof(hk->addr));
	hk->keyid = keyid;
	memcpy(hk->idfp, idfp, FP64_SIZE+1);
	hk->next = crypto_storedhostkey_hashtable[hashindex];
	hk->aeslevel = aeslevel;
	crypto_storedhostkey_hashtable[hashindex] = hk;
}

qboolean Crypto_RetrieveHostKey(lhnetaddress_t *peeraddress, int *keyid, char *keyfp, size_t keyfplen, char *idfp, size_t idfplen, int *aeslevel)
{
	char buf[128];
	int hashindex;
	crypto_storedhostkey_t *hk;

	if(!d0_blind_id_dll)
		return false;

	LHNETADDRESS_ToString(peeraddress, buf, sizeof(buf), 1);
	hashindex = CRC_Block((const unsigned char *) buf, strlen(buf)) % CRYPTO_HOSTKEY_HASHSIZE;
	for(hk = crypto_storedhostkey_hashtable[hashindex]; hk && LHNETADDRESS_Compare(&hk->addr, peeraddress); hk = hk->next);

	if(!hk)
		return false;

	if(keyid)
		*keyid = hk->keyid;
	if(keyfp)
		strlcpy(keyfp, pubkeys_fp64[hk->keyid], keyfplen);
	if(idfp)
		strlcpy(idfp, hk->idfp, idfplen);
	if(aeslevel)
		*aeslevel = hk->aeslevel;

	return true;
}
int Crypto_RetrieveLocalKey(int keyid, char *keyfp, size_t keyfplen, char *idfp, size_t idfplen) // return value: -1 if more to come, +1 if valid, 0 if end of list
{
	if(keyid < 0 || keyid >= MAX_PUBKEYS)
		return 0;
	if(keyfp)
		*keyfp = 0;
	if(idfp)
		*idfp = 0;
	if(!pubkeys[keyid])
		return -1;
	if(keyfp)
		strlcpy(keyfp, pubkeys_fp64[keyid], keyfplen);
	if(idfp)
		if(pubkeys_havepriv[keyid])
			strlcpy(idfp, pubkeys_priv_fp64[keyid], keyfplen);
	return 1;
}
// end

// init/shutdown code
static void Crypto_BuildChallengeAppend(void)
{
	char *p, *lengthptr, *startptr;
	size_t n;
	int i;
	p = challenge_append;
	n = sizeof(challenge_append);
	Crypto_UnLittleLong(p, PROTOCOL_VLEN);
	p += 4;
	n -= 4;
	lengthptr = p;
	Crypto_UnLittleLong(p, 0);
	p += 4;
	n -= 4;
	Crypto_UnLittleLong(p, PROTOCOL_D0_BLIND_ID);
	p += 4;
	n -= 4;
	startptr = p;
	for(i = 0; i < MAX_PUBKEYS; ++i)
		if(pubkeys_havepriv[i])
			PutWithNul(&p, &n, pubkeys_fp64[i]);
	PutWithNul(&p, &n, "");
	for(i = 0; i < MAX_PUBKEYS; ++i)
		if(!pubkeys_havepriv[i] && pubkeys[i])
			PutWithNul(&p, &n, pubkeys_fp64[i]);
	Crypto_UnLittleLong(lengthptr, p - startptr);
	challenge_append_length = p - challenge_append;
}

static void Crypto_LoadKeys(void)
{
	char buf[8192];
	size_t len, len2;
	int i;

	// load keys
	// note: we are just a CLIENT
	// so we load:
	//   PUBLIC KEYS to accept (including modulus)
	//   PRIVATE KEY of user

	crypto_idstring = NULL;
	dpsnprintf(crypto_idstring_buf, sizeof(crypto_idstring_buf), "%d", d0_rijndael_dll ? crypto_aeslevel.integer : 0);
	for(i = 0; i < MAX_PUBKEYS; ++i)
	{
		memset(pubkeys_fp64[i], 0, sizeof(pubkeys_fp64[i]));
		memset(pubkeys_priv_fp64[i], 0, sizeof(pubkeys_fp64[i]));
		pubkeys_havepriv[i] = false;
		len = Crypto_LoadFile(va("key_%d.d0pk", i), buf, sizeof(buf));
		if((pubkeys[i] = Crypto_ReadPublicKey(buf, len)))
		{
			len2 = FP64_SIZE;
			if(qd0_blind_id_fingerprint64_public_key(pubkeys[i], pubkeys_fp64[i], &len2)) // keeps final NUL
			{
				Con_Printf("Loaded public key key_%d.d0pk (fingerprint: %s)\n", i, pubkeys_fp64[i]);
				len = Crypto_LoadFile(va("key_%d.d0si", i), buf, sizeof(buf));
				if(len)
				{
					if(Crypto_AddPrivateKey(pubkeys[i], buf, len))
					{
						len2 = FP64_SIZE;
						if(qd0_blind_id_fingerprint64_public_id(pubkeys[i], pubkeys_priv_fp64[i], &len2)) // keeps final NUL
						{
							Con_Printf("Loaded private ID key_%d.d0si for key_%d.d0pk (fingerprint: %s)\n", i, i, pubkeys_priv_fp64[i]);
							pubkeys_havepriv[i] = true;
							strlcat(crypto_idstring_buf, va(" %s@%s", pubkeys_priv_fp64[i], pubkeys_fp64[i]), sizeof(crypto_idstring_buf));
						}
						else
						{
							// can't really happen
							// but nothing leaked here
						}
					}
				}
			}
			else
			{
				// can't really happen
				qd0_blind_id_free(pubkeys[i]);
				pubkeys[i] = NULL;
			}
		}
	}
	crypto_idstring = crypto_idstring_buf;

	keygen_i = -1;
	Crypto_BuildChallengeAppend();

	// find a good prefix length for all the keys we know (yes, algorithm is not perfect yet, may yield too long prefix length)
	crypto_keyfp_recommended_length = 0;
	memset(buf+256, 0, MAX_PUBKEYS + MAX_PUBKEYS);
	while(crypto_keyfp_recommended_length < FP64_SIZE)
	{
		memset(buf, 0, 256);
		for(i = 0; i < MAX_PUBKEYS; ++i)
			if(pubkeys[i])
			{
				if(!buf[256 + i])
					++buf[(unsigned char) pubkeys_fp64[i][crypto_keyfp_recommended_length]];
				if(pubkeys_havepriv[i])
					if(!buf[256 + MAX_PUBKEYS + i])
						++buf[(unsigned char) pubkeys_priv_fp64[i][crypto_keyfp_recommended_length]];
			}
		for(i = 0; i < MAX_PUBKEYS; ++i)
			if(pubkeys[i])
			{
				if(!buf[256 + i])
					if(buf[(unsigned char) pubkeys_fp64[i][crypto_keyfp_recommended_length]] < 2)
						buf[256 + i] = 1;
				if(pubkeys_havepriv[i])
					if(!buf[256 + MAX_PUBKEYS + i])
						if(buf[(unsigned char) pubkeys_priv_fp64[i][crypto_keyfp_recommended_length]] < 2)
							buf[256 + MAX_PUBKEYS + i] = 1;
			}
		++crypto_keyfp_recommended_length;
		for(i = 0; i < MAX_PUBKEYS; ++i)
			if(pubkeys[i])
			{
				if(!buf[256 + i])
					break;
				if(pubkeys_havepriv[i])
					if(!buf[256 + MAX_PUBKEYS + i])
						break;
			}
		if(i >= MAX_PUBKEYS)
			break;
	}
	if(crypto_keyfp_recommended_length < 7)
		crypto_keyfp_recommended_length = 7;
}

static void Crypto_UnloadKeys(void)
{
	int i;
	keygen_i = -1;
	for(i = 0; i < MAX_PUBKEYS; ++i)
	{
		if(pubkeys[i])
			qd0_blind_id_free(pubkeys[i]);
		pubkeys[i] = NULL;
		pubkeys_havepriv[i] = false;
		memset(pubkeys_fp64[i], 0, sizeof(pubkeys_fp64[i]));
		memset(pubkeys_priv_fp64[i], 0, sizeof(pubkeys_fp64[i]));
		challenge_append_length = 0;
	}
	crypto_idstring = NULL;
}

void Crypto_Shutdown(void)
{
	crypto_t *crypto;
	int i;

	Crypto_Rijndael_CloseLibrary();

	if(d0_blind_id_dll)
	{
		// free memory
		for(i = 0; i < MAX_CRYPTOCONNECTS; ++i)
		{
			crypto = &cryptoconnects[i].crypto;
			CLEAR_CDATA;
		}
		memset(cryptoconnects, 0, sizeof(cryptoconnects));
		crypto = &cls.crypto;
		CLEAR_CDATA;

		Crypto_UnloadKeys();

		qd0_blind_id_SHUTDOWN();

		Crypto_CloseLibrary();
	}
}

void Crypto_Init(void)
{
	if(!Crypto_OpenLibrary())
		return;

	if(!qd0_blind_id_INITIALIZE())
	{
		Crypto_Rijndael_CloseLibrary();
		Crypto_CloseLibrary();
		Con_Printf("libd0_blind_id initialization FAILED, cryptography support has been disabled\n");
		return;
	}

	Crypto_Rijndael_OpenLibrary(); // if this fails, it's uncritical

	Crypto_InitHostKeys();
	Crypto_LoadKeys();
}
// end

// keygen code
static void Crypto_KeyGen_Finished(int code, size_t length_received, unsigned char *buffer, void *cbdata)
{
	const char *p[1];
	size_t l[1];
	static char buf[8192];
	static char buf2[8192];
	size_t bufsize, buf2size;
	qfile_t *f = NULL;
	d0_blind_id_t *ctx, *ctx2;
	D0_BOOL status;
	size_t len2;

	if(!d0_blind_id_dll)
	{
		Con_Print("libd0_blind_id DLL not found, this command is inactive.\n");
		keygen_i = -1;
		return;
	}

	if(keygen_i >= MAX_PUBKEYS || !pubkeys[keygen_i])
	{
		Con_Printf("overflow of keygen_i\n");
		keygen_i = -1;
		return;
	}
	if(keygen_i < 0)
	{
		Con_Printf("Unexpected response from keygen server:\n");
		Com_HexDumpToConsole(buffer, length_received);
		return;
	}
	if(!Crypto_ParsePack((const char *) buffer, length_received, FOURCC_D0IR, p, l, 1))
	{
		if(length_received >= 5 && Crypto_LittleLong((const char *) buffer) == FOURCC_D0ER)
		{
			Con_Printf("Error response from keygen server: %.*s\n", (int)(length_received - 5), buffer + 5);
		}
		else
		{
			Con_Printf("Invalid response from keygen server:\n");
			Com_HexDumpToConsole(buffer, length_received);
		}
		keygen_i = -1;
		return;
	}
	if(!qd0_blind_id_finish_private_id_request(pubkeys[keygen_i], p[0], l[0]))
	{
		Con_Printf("d0_blind_id_finish_private_id_request failed\n");
		keygen_i = -1;
		return;
	}

	// verify the key we just got (just in case)
	ctx = qd0_blind_id_new();
	if(!ctx)
	{
		Con_Printf("d0_blind_id_new failed\n");
		keygen_i = -1;
		return;
	}
	ctx2 = qd0_blind_id_new();
	if(!ctx2)
	{
		Con_Printf("d0_blind_id_new failed\n");
		qd0_blind_id_free(ctx);
		keygen_i = -1;
		return;
	}
	if(!qd0_blind_id_copy(ctx, pubkeys[keygen_i]))
	{
		Con_Printf("d0_blind_id_copy failed\n");
		qd0_blind_id_free(ctx);
		qd0_blind_id_free(ctx2);
		keygen_i = -1;
		return;
	}
	if(!qd0_blind_id_copy(ctx2, pubkeys[keygen_i]))
	{
		Con_Printf("d0_blind_id_copy failed\n");
		qd0_blind_id_free(ctx);
		qd0_blind_id_free(ctx2);
		keygen_i = -1;
		return;
	}
	bufsize = sizeof(buf);
	if(!qd0_blind_id_authenticate_with_private_id_start(ctx, 1, 1, "hello world", 11, buf, &bufsize))
	{
		Con_Printf("d0_blind_id_authenticate_with_private_id_start failed\n");
		qd0_blind_id_free(ctx);
		qd0_blind_id_free(ctx2);
		keygen_i = -1;
		return;
	}
	buf2size = sizeof(buf2);
	if(!qd0_blind_id_authenticate_with_private_id_challenge(ctx2, 1, 1, buf, bufsize, buf2, &buf2size, &status) || !status)
	{
		Con_Printf("d0_blind_id_authenticate_with_private_id_challenge failed (server does not have the requested private key)\n");
		qd0_blind_id_free(ctx);
		qd0_blind_id_free(ctx2);
		keygen_i = -1;
		return;
	}
	bufsize = sizeof(buf);
	if(!qd0_blind_id_authenticate_with_private_id_response(ctx, buf2, buf2size, buf, &bufsize))
	{
		Con_Printf("d0_blind_id_authenticate_with_private_id_response failed\n");
		qd0_blind_id_free(ctx);
		qd0_blind_id_free(ctx2);
		keygen_i = -1;
		return;
	}
	buf2size = sizeof(buf2);
	if(!qd0_blind_id_authenticate_with_private_id_verify(ctx2, buf, bufsize, buf2, &buf2size, &status) || !status)
	{
		Con_Printf("d0_blind_id_authenticate_with_private_id_verify failed (server does not have the requested private key)\n");
		qd0_blind_id_free(ctx);
		qd0_blind_id_free(ctx2);
		keygen_i = -1;
		return;
	}
	qd0_blind_id_free(ctx);
	qd0_blind_id_free(ctx2);

	// we have a valid key now!
	// make the rest of crypto.c know that
	len2 = FP64_SIZE;
	if(qd0_blind_id_fingerprint64_public_id(pubkeys[keygen_i], pubkeys_priv_fp64[keygen_i], &len2)) // keeps final NUL
	{
		Con_Printf("Received private ID key_%d.d0pk (fingerprint: %s)\n", keygen_i, pubkeys_priv_fp64[keygen_i]);
		pubkeys_havepriv[keygen_i] = true;
		strlcat(crypto_idstring_buf, va(" %s@%s", pubkeys_priv_fp64[keygen_i], pubkeys_fp64[keygen_i]), sizeof(crypto_idstring_buf));
		crypto_idstring = crypto_idstring_buf;
		Crypto_BuildChallengeAppend();
	}
	// write the key to disk
	p[0] = buf;
	l[0] = sizeof(buf);
	if(!qd0_blind_id_write_private_id(pubkeys[keygen_i], buf, &l[0]))
	{
		Con_Printf("d0_blind_id_write_private_id failed\n");
		keygen_i = -1;
		return;
	}
	if(!(buf2size = Crypto_UnParsePack(buf2, sizeof(buf2), FOURCC_D0SI, p, l, 1)))
	{
		Con_Printf("Crypto_UnParsePack failed\n");
		keygen_i = -1;
		return;
	}

	if(*fs_userdir)
	{
		FS_CreatePath(va("%skey_%d.d0si", fs_userdir, keygen_i));
		f = FS_SysOpen(va("%skey_%d.d0si", fs_userdir, keygen_i), "wb", false);
	}
	if(!f)
	{
		FS_CreatePath(va("%skey_%d.d0si", fs_basedir, keygen_i));
		f = FS_SysOpen(va("%skey_%d.d0si", fs_basedir, keygen_i), "wb", false);
	}
	if(!f)
	{
		Con_Printf("Cannot open key_%d.d0si\n", keygen_i);
		keygen_i = -1;
		return;
	}
	FS_Write(f, buf2, buf2size);
	FS_Close(f);

	Con_Printf("Saved to key_%d.d0si\n", keygen_i);
	keygen_i = -1;
}

static void Crypto_KeyGen_f(void)
{
	int i;
	const char *p[1];
	size_t l[1];
	static char buf[8192];
	static char buf2[8192];
	size_t buf2l, buf2pos;
	if(!d0_blind_id_dll)
	{
		Con_Print("libd0_blind_id DLL not found, this command is inactive.\n");
		return;
	}
	if(Cmd_Argc() != 3)
	{
		Con_Printf("usage:\n%s id url\n", Cmd_Argv(0));
		return;
	}
	i = atoi(Cmd_Argv(1));
	if(!pubkeys[i])
	{
		Con_Printf("there is no public key %d\n", i);
		return;
	}
	if(pubkeys_havepriv[i])
	{
		Con_Printf("there is already a private key for %d\n", i);
		return;
	}
	if(keygen_i >= 0)
	{
		Con_Printf("there is already a keygen run on the way\n");
		return;
	}
	keygen_i = i;
	if(!qd0_blind_id_generate_private_id_start(pubkeys[keygen_i]))
	{
		Con_Printf("d0_blind_id_start failed\n");
		keygen_i = -1;
		return;
	}
	p[0] = buf;
	l[0] = sizeof(buf);
	if(!qd0_blind_id_generate_private_id_request(pubkeys[keygen_i], buf, &l[0]))
	{
		Con_Printf("d0_blind_id_generate_private_id_request failed\n");
		keygen_i = -1;
		return;
	}
	buf2pos = strlen(Cmd_Argv(2));
	memcpy(buf2, Cmd_Argv(2), buf2pos);
	if(!(buf2l = Crypto_UnParsePack(buf2 + buf2pos, sizeof(buf2) - buf2pos - 1, FOURCC_D0IQ, p, l, 1)))
	{
		Con_Printf("Crypto_UnParsePack failed\n");
		keygen_i = -1;
		return;
	}
	if(!(buf2l = base64_encode((unsigned char *) (buf2 + buf2pos), buf2l, sizeof(buf2) - buf2pos - 1)))
	{
		Con_Printf("base64_encode failed\n");
		keygen_i = -1;
		return;
	}
	buf2l += buf2pos;
	buf[buf2l] = 0;
	if(!Curl_Begin_ToMemory(buf2, 0, (unsigned char *) keygen_buf, sizeof(keygen_buf), Crypto_KeyGen_Finished, NULL))
	{
		Con_Printf("curl failed\n");
		keygen_i = -1;
		return;
	}
	Con_Printf("key generation in progress\n");
}
// end

// console commands
static void Crypto_Reload_f(void)
{
	Crypto_ClearHostKeys();
	Crypto_UnloadKeys();
	Crypto_LoadKeys();
}

static void Crypto_Keys_f(void)
{
	int i;
	if(!d0_blind_id_dll)
	{
		Con_Print("libd0_blind_id DLL not found, this command is inactive.\n");
		return;
	}
	for(i = 0; i < MAX_PUBKEYS; ++i)
	{
		if(pubkeys[i])
		{
			Con_Printf("%2d: public key key_%d.d0pk (fingerprint: %s)\n", i, i, pubkeys_fp64[i]);
			if(pubkeys_havepriv[i])
				Con_Printf("    private ID key_%d.d0si (fingerprint: %s)\n", i, pubkeys_priv_fp64[i]);
		}
	}
}

static void Crypto_HostKeys_f(void)
{
	int i;
	crypto_storedhostkey_t *hk;
	char buf[128];

	if(!d0_blind_id_dll)
	{
		Con_Print("libd0_blind_id DLL not found, this command is inactive.\n");
		return;
	}
	for(i = 0; i < CRYPTO_HOSTKEY_HASHSIZE; ++i)
	{
		for(hk = crypto_storedhostkey_hashtable[i]; hk; hk = hk->next)
		{
			LHNETADDRESS_ToString(&hk->addr, buf, sizeof(buf), 1);
			Con_Printf("%d %s@%.*s %s\n",
					hk->aeslevel,
					hk->idfp,
					crypto_keyfp_recommended_length, pubkeys_fp64[hk->keyid],
					buf);
		}
	}
}

static void Crypto_HostKey_Clear_f(void)
{
	lhnetaddress_t addr;
	int i;

	if(!d0_blind_id_dll)
	{
		Con_Print("libd0_blind_id DLL not found, this command is inactive.\n");
		return;
	}

	for(i = 1; i < Cmd_Argc(); ++i)
	{
		LHNETADDRESS_FromString(&addr, Cmd_Argv(i), 26000);
		if(Crypto_ClearHostKey(&addr))
		{
			Con_Printf("cleared host key for %s\n", Cmd_Argv(i));
		}
	}
}

void Crypto_Init_Commands(void)
{
	if(d0_blind_id_dll)
	{
		Cmd_AddCommand("crypto_reload", Crypto_Reload_f, "reloads cryptographic keys");
		Cmd_AddCommand("crypto_keygen", Crypto_KeyGen_f, "generates and saves a cryptographic key");
		Cmd_AddCommand("crypto_keys", Crypto_Keys_f, "lists the loaded keys");
		Cmd_AddCommand("crypto_hostkeys", Crypto_HostKeys_f, "lists the cached host keys");
		Cmd_AddCommand("crypto_hostkey_clear", Crypto_HostKey_Clear_f, "clears a cached host key");
		Cvar_RegisterVariable(&crypto_developer);
		if(d0_rijndael_dll)
			Cvar_RegisterVariable(&crypto_aeslevel);
		else
			crypto_aeslevel.integer = 0; // make sure
		Cvar_RegisterVariable(&crypto_servercpupercent);
		Cvar_RegisterVariable(&crypto_servercpumaxtime);
		Cvar_RegisterVariable(&crypto_servercpudebug);
	}
}
// end

// AES encryption
static void aescpy(unsigned char *key, const unsigned char *iv, unsigned char *dst, const unsigned char *src, size_t len)
{
	const unsigned char *xorpos = iv;
	unsigned char xorbuf[16];
	unsigned long rk[D0_RIJNDAEL_RKLENGTH(DHKEY_SIZE * 8)];
	size_t i;
	qd0_rijndael_setup_encrypt(rk, key, DHKEY_SIZE * 8);
	while(len > 16)
	{
		for(i = 0; i < 16; ++i)
			xorbuf[i] = src[i] ^ xorpos[i];
		qd0_rijndael_encrypt(rk, D0_RIJNDAEL_NROUNDS(DHKEY_SIZE * 8), xorbuf, dst);
		xorpos = dst;
		len -= 16;
		src += 16;
		dst += 16;
	}
	if(len > 0)
	{
		for(i = 0; i < len; ++i)
			xorbuf[i] = src[i] ^ xorpos[i];
		for(; i < 16; ++i)
			xorbuf[i] = xorpos[i];
		qd0_rijndael_encrypt(rk, D0_RIJNDAEL_NROUNDS(DHKEY_SIZE * 8), xorbuf, dst);
	}
}
static void seacpy(unsigned char *key, const unsigned char *iv, unsigned char *dst, const unsigned char *src, size_t len)
{
	const unsigned char *xorpos = iv;
	unsigned char xorbuf[16];
	unsigned long rk[D0_RIJNDAEL_RKLENGTH(DHKEY_SIZE * 8)];
	size_t i;
	qd0_rijndael_setup_decrypt(rk, key, DHKEY_SIZE * 8);
	while(len > 16)
	{
		qd0_rijndael_decrypt(rk, D0_RIJNDAEL_NROUNDS(DHKEY_SIZE * 8), src, xorbuf);
		for(i = 0; i < 16; ++i)
			dst[i] = xorbuf[i] ^ xorpos[i];
		xorpos = src;
		len -= 16;
		src += 16;
		dst += 16;
	}
	if(len > 0)
	{
		qd0_rijndael_decrypt(rk, D0_RIJNDAEL_NROUNDS(DHKEY_SIZE * 8), src, xorbuf);
		for(i = 0; i < len; ++i)
			dst[i] = xorbuf[i] ^ xorpos[i];
	}
}

// NOTE: we MUST avoid the following begins of the packet:
//   1. 0xFF, 0xFF, 0xFF, 0xFF
//   2. 0x80, 0x00, length/256, length%256
// this luckily does NOT affect AES mode, where the first byte always is in the range from 0x00 to 0x0F
const void *Crypto_EncryptPacket(crypto_t *crypto, const void *data_src, size_t len_src, void *data_dst, size_t *len_dst, size_t len)
{
	unsigned char h[32];
	int i;
	if(crypto->authenticated)
	{
		if(crypto->use_aes)
		{
			// AES packet = 1 byte length overhead, 15 bytes from HMAC-SHA-256, data, 0..15 bytes padding
			// 15 bytes HMAC-SHA-256 (112bit) suffice as the attacker can't do more than forge a random-looking packet
			// HMAC is needed to not leak information about packet content
			if(developer_networking.integer)
			{
				Con_Print("To be encrypted:\n");
				Com_HexDumpToConsole((const unsigned char *) data_src, len_src);
			}
			if(len_src + 32 > len || !HMAC_SHA256_32BYTES(h, (const unsigned char *) data_src, len_src, crypto->dhkey, DHKEY_SIZE))
			{
				Con_Printf("Crypto_EncryptPacket failed (not enough space: %d bytes in, %d bytes out)\n", (int) len_src, (int) len);
				return NULL;
			}
			*len_dst = ((len_src + 15) / 16) * 16 + 16; // add 16 for HMAC, then round to 16-size for AES
			((unsigned char *) data_dst)[0] = *len_dst - len_src;
			memcpy(((unsigned char *) data_dst)+1, h, 15);
			aescpy(crypto->dhkey, (const unsigned char *) data_dst, ((unsigned char *) data_dst) + 16, (const unsigned char *) data_src, len_src);
			//                    IV                                dst                                src                               len
		}
		else
		{
			// HMAC packet = 16 bytes HMAC-SHA-256 (truncated to 128 bits), data
			if(len_src + 16 > len || !HMAC_SHA256_32BYTES(h, (const unsigned char *) data_src, len_src, crypto->dhkey, DHKEY_SIZE))
			{
				Con_Printf("Crypto_EncryptPacket failed (not enough space: %d bytes in, %d bytes out)\n", (int) len_src, (int) len);
				return NULL;
			}
			*len_dst = len_src + 16;
			memcpy(data_dst, h, 16);
			memcpy(((unsigned char *) data_dst) + 16, (unsigned char *) data_src, len_src);

			// handle the "avoid" conditions:
			i = BuffBigLong((unsigned char *) data_dst);
			if(
				(i == (int)0xFFFFFFFF) // avoid QW control packet
				||
				(i == (int)0x80000000 + (int)*len_dst) // avoid NQ control packet
			)
				*(unsigned char *)data_dst ^= 0x80; // this will ALWAYS fix it
		}
		return data_dst;
	}
	else
	{
		*len_dst = len_src;
		return data_src;
	}
}

const void *Crypto_DecryptPacket(crypto_t *crypto, const void *data_src, size_t len_src, void *data_dst, size_t *len_dst, size_t len)
{
	unsigned char h[32];
	int i;

	// silently handle non-crypto packets
	i = BuffBigLong((unsigned char *) data_src);
	if(
		(i == (int)0xFFFFFFFF) // avoid QW control packet
		||
		(i == (int)0x80000000 + (int)len_src) // avoid NQ control packet
	)
		return NULL;

	if(crypto->authenticated)
	{
		if(crypto->use_aes)
		{
			if(len_src < 16 || ((len_src - 16) % 16))
			{
				Con_Printf("Crypto_DecryptPacket failed (not enough space: %d bytes in, %d bytes out)\n", (int) len_src, (int) len);
				return NULL;
			}
			*len_dst = len_src - ((unsigned char *) data_src)[0];
			if(len < *len_dst || *len_dst > len_src - 16)
			{
				Con_Printf("Crypto_DecryptPacket failed (not enough space: %d bytes in, %d->%d bytes out)\n", (int) len_src, (int) *len_dst, (int) len);
				return NULL;
			}
			seacpy(crypto->dhkey, (unsigned char *) data_src, (unsigned char *) data_dst, ((const unsigned char *) data_src) + 16, *len_dst);
			//                    IV                          dst                         src                                      len
			if(!HMAC_SHA256_32BYTES(h, (const unsigned char *) data_dst, *len_dst, crypto->dhkey, DHKEY_SIZE))
			{
				Con_Printf("HMAC fail\n");
				return NULL;
			}
			if(memcmp(((const unsigned char *) data_src)+1, h, 15)) // ignore first byte, used for length
			{
				Con_Printf("HMAC mismatch\n");
				return NULL;
			}
			if(developer_networking.integer)
			{
				Con_Print("Decrypted:\n");
				Com_HexDumpToConsole((const unsigned char *) data_dst, *len_dst);
			}
			return data_dst; // no need to copy
		}
		else
		{
			if(len_src < 16)
			{
				Con_Printf("Crypto_DecryptPacket failed (not enough space: %d bytes in, %d bytes out)\n", (int) len_src, (int) len);
				return NULL;
			}
			*len_dst = len_src - 16;
			if(len < *len_dst)
			{
				Con_Printf("Crypto_DecryptPacket failed (not enough space: %d bytes in, %d->%d bytes out)\n", (int) len_src, (int) *len_dst, (int) len);
				return NULL;
			}
			//memcpy(data_dst, data_src + 16, *len_dst);
			if(!HMAC_SHA256_32BYTES(h, ((const unsigned char *) data_src) + 16, *len_dst, crypto->dhkey, DHKEY_SIZE))
			{
				Con_Printf("HMAC fail\n");
				Com_HexDumpToConsole((const unsigned char *) data_src, len_src);
				return NULL;
			}

			if(memcmp((const unsigned char *) data_src, h, 16)) // ignore first byte, used for length
			{
				// undo the "avoid conditions"
				if(
						(i == (int)0x7FFFFFFF) // avoided QW control packet
						||
						(i == (int)0x00000000 + (int)len_src) // avoided NQ control packet
				  )
				{
					// do the avoidance on the hash too
					h[0] ^= 0x80;
					if(memcmp((const unsigned char *) data_src, h, 16)) // ignore first byte, used for length
					{
						Con_Printf("HMAC mismatch\n");
						Com_HexDumpToConsole((const unsigned char *) data_src, len_src);
						return NULL;
					}
				}
				else
				{
					Con_Printf("HMAC mismatch\n");
					Com_HexDumpToConsole((const unsigned char *) data_src, len_src);
					return NULL;
				}
			}
			return ((const unsigned char *) data_src) + 16; // no need to copy, so data_dst is not used
		}
	}
	else
	{
		*len_dst = len_src;
		return data_src;
	}
}
// end

const char *Crypto_GetInfoResponseDataString(void)
{
	crypto_idstring_buf[0] = '0' + crypto_aeslevel.integer;
	return crypto_idstring;
}

// network protocol
qboolean Crypto_ServerAppendToChallenge(const char *data_in, size_t len_in, char *data_out, size_t *len_out, size_t maxlen_out)
{
	// cheap op, all is precomputed
	if(!d0_blind_id_dll)
		return false; // no support
	// append challenge
	if(maxlen_out <= *len_out + challenge_append_length)
		return false;
	memcpy(data_out + *len_out, challenge_append, challenge_append_length);
	*len_out += challenge_append_length;
	return false;
}

static int Crypto_ServerError(char *data_out, size_t *len_out, const char *msg, const char *msg_client)
{
	if(!msg_client)
		msg_client = msg;
	Con_DPrintf("rejecting client: %s\n", msg);
	if(*msg_client)
		dpsnprintf(data_out, *len_out, "reject %s", msg_client);
	*len_out = strlen(data_out);
	return CRYPTO_DISCARD;
}

static int Crypto_SoftServerError(char *data_out, size_t *len_out, const char *msg)
{
	*len_out = 0;
	Con_DPrintf("%s\n", msg);
	return CRYPTO_DISCARD;
}

static int Crypto_ServerParsePacket_Internal(const char *data_in, size_t len_in, char *data_out, size_t *len_out, lhnetaddress_t *peeraddress)
{
	// if "connect": reject if in the middle of crypto handshake
	crypto_t *crypto = NULL;
	char *data_out_p = data_out;
	const char *string = data_in;
	int aeslevel;
	D0_BOOL aes;
	D0_BOOL status;

	if(!d0_blind_id_dll)
		return CRYPTO_NOMATCH; // no support

	if (len_in > 8 && !memcmp(string, "connect\\", 8) && d0_rijndael_dll && crypto_aeslevel.integer >= 3)
	{
		const char *s;
		int i;
		// sorry, we have to verify the challenge here to not reflect network spam

		if (!(s = SearchInfostring(string + 4, "challenge")))
			return CRYPTO_NOMATCH; // will be later accepted if encryption was set up
		// validate the challenge
		for (i = 0;i < MAX_CHALLENGES;i++)
			if(challenge[i].time > 0)
				if (!LHNETADDRESS_Compare(peeraddress, &challenge[i].address) && !strcmp(challenge[i].string, s))
					break;
		// if the challenge is not recognized, drop the packet
		if (i == MAX_CHALLENGES) // challenge mismatch is silent
			return CRYPTO_DISCARD; // pre-challenge: rather be silent

		crypto = Crypto_ServerFindInstance(peeraddress, false);
		if(!crypto || !crypto->authenticated)
			return Crypto_ServerError(data_out, len_out, "This server requires authentication and encryption to be supported by your client", NULL);
	}
	else if(len_in > 5 && !memcmp(string, "d0pk\\", 5) && ((LHNETADDRESS_GetAddressType(peeraddress) == LHNETADDRESSTYPE_LOOP) || sv_public.integer > -3))
	{
		const char *cnt, *s, *p;
		int id;
		int clientid = -1, serverid = -1;
		cnt = SearchInfostring(string + 4, "id");
		id = (cnt ? atoi(cnt) : -1);
		cnt = SearchInfostring(string + 4, "cnt");
		if(!cnt)
			return CRYPTO_DISCARD; // pre-challenge: rather be silent
		GetUntilNul(&data_in, &len_in);
		if(!data_in)
			return CRYPTO_DISCARD; // pre-challenge: rather be silent
		if(!strcmp(cnt, "0"))
		{
			int i;
			if (!(s = SearchInfostring(string + 4, "challenge")))
				return CRYPTO_DISCARD; // pre-challenge: rather be silent
			// validate the challenge
			for (i = 0;i < MAX_CHALLENGES;i++)
				if(challenge[i].time > 0)
					if (!LHNETADDRESS_Compare(peeraddress, &challenge[i].address) && !strcmp(challenge[i].string, s))
						break;
			// if the challenge is not recognized, drop the packet
			if (i == MAX_CHALLENGES) // challenge mismatch is silent
				return CRYPTO_DISCARD; // pre-challenge: rather be silent

			if (!(s = SearchInfostring(string + 4, "aeslevel")))
				aeslevel = 0; // not supported
			else
				aeslevel = bound(0, atoi(s), 3);
			switch(bound(0, d0_rijndael_dll ? crypto_aeslevel.integer : 0, 3))
			{
				default: // dummy, never happens, but to make gcc happy...
				case 0:
					if(aeslevel >= 3)
						return Crypto_ServerError(data_out, len_out, "This server requires encryption to be not required (crypto_aeslevel <= 2)", NULL);
					aes = false;
					break;
				case 1:
					aes = (aeslevel >= 2);
					break;
				case 2:
					aes = (aeslevel >= 1);
					break;
				case 3:
					if(aeslevel <= 0)
						return Crypto_ServerError(data_out, len_out, "This server requires encryption to be supported (crypto_aeslevel >= 1, and d0_rijndael library must be present)", NULL);
					aes = true;
					break;
			}

			p = GetUntilNul(&data_in, &len_in);
			if(p && *p)
			{
				for(i = 0; i < MAX_PUBKEYS; ++i)
				{
					if(pubkeys[i])
						if(!strcmp(p, pubkeys_fp64[i]))
							if(pubkeys_havepriv[i])
								if(serverid < 0)
									serverid = i;
				}
				if(serverid < 0)
					return Crypto_ServerError(data_out, len_out, "Invalid server key", NULL);
			}
			p = GetUntilNul(&data_in, &len_in);
			if(p && *p)
			{
				for(i = 0; i < MAX_PUBKEYS; ++i)
				{
					if(pubkeys[i])
						if(!strcmp(p, pubkeys_fp64[i]))
							if(clientid < 0)
								clientid = i;
				}
				if(clientid < 0)
					return Crypto_ServerError(data_out, len_out, "Invalid client key", NULL);
			}

			crypto = Crypto_ServerFindInstance(peeraddress, true);
			if(!crypto)
				return Crypto_ServerError(data_out, len_out, "Could not create a crypto connect instance", NULL);
			MAKE_CDATA;
			CDATA->cdata_id = id;
			CDATA->s = serverid;
			CDATA->c = clientid;
			memset(crypto->dhkey, 0, sizeof(crypto->dhkey));
			CDATA->challenge[0] = 0;
			crypto->client_keyfp[0] = 0;
			crypto->client_idfp[0] = 0;
			crypto->server_keyfp[0] = 0;
			crypto->server_idfp[0] = 0;
			crypto->use_aes = aes != 0;

			if(CDATA->s >= 0)
			{
				// I am the server, and my key is ok... so let's set server_keyfp and server_idfp
				strlcpy(crypto->server_keyfp, pubkeys_fp64[CDATA->s], sizeof(crypto->server_keyfp));
				strlcpy(crypto->server_idfp, pubkeys_priv_fp64[CDATA->s], sizeof(crypto->server_idfp));

				if(!CDATA->id)
					CDATA->id = qd0_blind_id_new();
				if(!CDATA->id)
				{
					CLEAR_CDATA;
					return Crypto_ServerError(data_out, len_out, "d0_blind_id_new failed", "Internal error");
				}
				if(!qd0_blind_id_copy(CDATA->id, pubkeys[CDATA->s]))
				{
					CLEAR_CDATA;
					return Crypto_ServerError(data_out, len_out, "d0_blind_id_copy failed", "Internal error");
				}
				PutWithNul(&data_out_p, len_out, va("d0pk\\cnt\\1\\id\\%d\\aes\\%d", CDATA->cdata_id, crypto->use_aes));
				if(!qd0_blind_id_authenticate_with_private_id_start(CDATA->id, true, false, "XONOTIC", 8, data_out_p, len_out)) // len_out receives used size by this op
				{
					CLEAR_CDATA;
					return Crypto_ServerError(data_out, len_out, "d0_blind_id_authenticate_with_private_id_start failed", "Internal error");
				}
				CDATA->next_step = 2;
				data_out_p += *len_out;
				*len_out = data_out_p - data_out;
				return CRYPTO_DISCARD;
			}
			else if(CDATA->c >= 0)
			{
				if(!CDATA->id)
					CDATA->id = qd0_blind_id_new();
				if(!CDATA->id)
				{
					CLEAR_CDATA;
					return Crypto_ServerError(data_out, len_out, "d0_blind_id_new failed", "Internal error");
				}
				if(!qd0_blind_id_copy(CDATA->id, pubkeys[CDATA->c]))
				{
					CLEAR_CDATA;
					return Crypto_ServerError(data_out, len_out, "d0_blind_id_copy failed", "Internal error");
				}
				PutWithNul(&data_out_p, len_out, va("d0pk\\cnt\\5\\id\\%d\\aes\\%d", CDATA->cdata_id, crypto->use_aes));
				if(!qd0_blind_id_authenticate_with_private_id_challenge(CDATA->id, true, false, data_in, len_in, data_out_p, len_out, &status))
				{
					CLEAR_CDATA;
					return Crypto_ServerError(data_out, len_out, "d0_blind_id_authenticate_with_private_id_challenge failed", "Internal error");
				}
				CDATA->next_step = 6;
				data_out_p += *len_out;
				*len_out = data_out_p - data_out;
				return CRYPTO_DISCARD;
			}
			else
			{
				CLEAR_CDATA;
				return Crypto_ServerError(data_out, len_out, "Missing client and server key", NULL);
			}
		}
		else if(!strcmp(cnt, "2"))
		{
			size_t fpbuflen;
			crypto = Crypto_ServerFindInstance(peeraddress, false);
			if(!crypto)
				return CRYPTO_NOMATCH; // pre-challenge, rather be silent
			if(id >= 0)
				if(CDATA->cdata_id != id)
					return Crypto_SoftServerError(data_out, len_out, va("Got d0pk\\id\\%d when expecting %d", id, CDATA->cdata_id));
			if(CDATA->next_step != 2)
				return Crypto_SoftServerError(data_out, len_out, va("Got d0pk\\cnt\\%s when expecting %d", cnt, CDATA->next_step));

			PutWithNul(&data_out_p, len_out, va("d0pk\\cnt\\3\\id\\%d", CDATA->cdata_id));
			if(!qd0_blind_id_authenticate_with_private_id_response(CDATA->id, data_in, len_in, data_out_p, len_out))
			{
				CLEAR_CDATA;
				return Crypto_ServerError(data_out, len_out, "d0_blind_id_authenticate_with_private_id_response failed", "Internal error");
			}
			fpbuflen = DHKEY_SIZE;
			if(!qd0_blind_id_sessionkey_public_id(CDATA->id, (char *) crypto->dhkey, &fpbuflen))
			{
				CLEAR_CDATA;
				return Crypto_ServerError(data_out, len_out, "d0_blind_id_sessionkey_public_id failed", "Internal error");
			}
			if(CDATA->c >= 0)
			{
				if(!qd0_blind_id_copy(CDATA->id, pubkeys[CDATA->c]))
				{
					CLEAR_CDATA;
					return Crypto_ServerError(data_out, len_out, "d0_blind_id_copy failed", "Internal error");
				}
				CDATA->next_step = 4;
			}
			else
			{
				// session key is FINISHED (no server part is to be expected)! By this, all keys are set up
				crypto->authenticated = true;
				CDATA->next_step = 0;
			}
			data_out_p += *len_out;
			*len_out = data_out_p - data_out;
			return CRYPTO_DISCARD;
		}
		else if(!strcmp(cnt, "4"))
		{
			crypto = Crypto_ServerFindInstance(peeraddress, false);
			if(!crypto)
				return CRYPTO_NOMATCH; // pre-challenge, rather be silent
			if(id >= 0)
				if(CDATA->cdata_id != id)
					return Crypto_SoftServerError(data_out, len_out, va("Got d0pk\\id\\%d when expecting %d", id, CDATA->cdata_id));
			if(CDATA->next_step != 4)
				return Crypto_SoftServerError(data_out, len_out, va("Got d0pk\\cnt\\%s when expecting %d", cnt, CDATA->next_step));
			PutWithNul(&data_out_p, len_out, va("d0pk\\cnt\\5\\id\\%d", CDATA->cdata_id));
			if(!qd0_blind_id_authenticate_with_private_id_challenge(CDATA->id, true, false, data_in, len_in, data_out_p, len_out, &status))
			{
				CLEAR_CDATA;
				return Crypto_ServerError(data_out, len_out, "d0_blind_id_authenticate_with_private_id_challenge failed", "Internal error");
			}
			CDATA->next_step = 6;
			data_out_p += *len_out;
			*len_out = data_out_p - data_out;
			return CRYPTO_DISCARD;
		}
		else if(!strcmp(cnt, "6"))
		{
			static char msgbuf[32];
			size_t msgbuflen = sizeof(msgbuf);
			size_t fpbuflen;
			int i;
			unsigned char dhkey[DHKEY_SIZE];
			crypto = Crypto_ServerFindInstance(peeraddress, false);
			if(!crypto)
				return CRYPTO_NOMATCH; // pre-challenge, rather be silent
			if(id >= 0)
				if(CDATA->cdata_id != id)
					return Crypto_SoftServerError(data_out, len_out, va("Got d0pk\\id\\%d when expecting %d", id, CDATA->cdata_id));
			if(CDATA->next_step != 6)
				return Crypto_SoftServerError(data_out, len_out, va("Got d0pk\\cnt\\%s when expecting %d", cnt, CDATA->next_step));

			if(!qd0_blind_id_authenticate_with_private_id_verify(CDATA->id, data_in, len_in, msgbuf, &msgbuflen, &status))
			{
				CLEAR_CDATA;
				return Crypto_ServerError(data_out, len_out, "d0_blind_id_authenticate_with_private_id_verify failed (authentication error)", "Authentication error");
			}
			if(status)
				strlcpy(crypto->client_keyfp, pubkeys_fp64[CDATA->c], sizeof(crypto->client_keyfp));
			else
				crypto->client_keyfp[0] = 0;
			memset(crypto->client_idfp, 0, sizeof(crypto->client_idfp));
			fpbuflen = FP64_SIZE;
			if(!qd0_blind_id_fingerprint64_public_id(CDATA->id, crypto->client_idfp, &fpbuflen))
			{
				CLEAR_CDATA;
				return Crypto_ServerError(data_out, len_out, "d0_blind_id_fingerprint64_public_id failed", "Internal error");
			}
			fpbuflen = DHKEY_SIZE;
			if(!qd0_blind_id_sessionkey_public_id(CDATA->id, (char *) dhkey, &fpbuflen))
			{
				CLEAR_CDATA;
				return Crypto_ServerError(data_out, len_out, "d0_blind_id_sessionkey_public_id failed", "Internal error");
			}
			// XOR the two DH keys together to make one
			for(i = 0; i < DHKEY_SIZE; ++i)
				crypto->dhkey[i] ^= dhkey[i];

			// session key is FINISHED (no server part is to be expected)! By this, all keys are set up
			crypto->authenticated = true;
			CDATA->next_step = 0;
			// send a challenge-less challenge
			PutWithNul(&data_out_p, len_out, "challenge ");
			*len_out = data_out_p - data_out;
			--*len_out; // remove NUL terminator
			return CRYPTO_MATCH;
		}
		return CRYPTO_NOMATCH; // pre-challenge, rather be silent
	}
	return CRYPTO_NOMATCH;
}

int Crypto_ServerParsePacket(const char *data_in, size_t len_in, char *data_out, size_t *len_out, lhnetaddress_t *peeraddress)
{
	int ret;
	double t = 0;
	static double complain_time = 0;
	const char *cnt;
	qboolean do_time = false;
	qboolean do_reject = false;
	if(crypto_servercpupercent.value > 0 || crypto_servercpumaxtime.value > 0)
		if(len_in > 5 && !memcmp(data_in, "d0pk\\", 5))
		{
			do_time = true;
			cnt = SearchInfostring(data_in + 4, "cnt");
			if(cnt)
				if(!strcmp(cnt, "0"))
					do_reject = true;
		}
	if(do_time)
	{
		// check if we may perform crypto...
		if(crypto_servercpupercent.value > 0)
		{
			crypto_servercpu_accumulator += (realtime - crypto_servercpu_lastrealtime) * crypto_servercpupercent.value * 0.01;
			if(crypto_servercpumaxtime.value)
				if(crypto_servercpu_accumulator > crypto_servercpumaxtime.value)
					crypto_servercpu_accumulator = crypto_servercpumaxtime.value;
		}
		else
		{
			if(crypto_servercpumaxtime.value > 0)
				if(realtime != crypto_servercpu_lastrealtime)
					crypto_servercpu_accumulator = crypto_servercpumaxtime.value;
		}
		crypto_servercpu_lastrealtime = realtime;
		if(do_reject && crypto_servercpu_accumulator < 0)
		{
			if(realtime > complain_time + 5)
				Con_Printf("crypto: cannot perform requested crypto operations; denial service attack or crypto_servercpupercent/crypto_servercpumaxtime are too low\n");
			*len_out = 0;
			return CRYPTO_DISCARD;
		}
		t = Sys_DoubleTime();
	}
	ret = Crypto_ServerParsePacket_Internal(data_in, len_in, data_out, len_out, peeraddress);
	if(do_time)
	{
		t = Sys_DoubleTime() - t;
		if(crypto_servercpudebug.integer)
			Con_Printf("crypto: accumulator was %.1f ms, used %.1f ms for crypto, ", crypto_servercpu_accumulator * 1000, t * 1000);
		crypto_servercpu_accumulator -= t;
		if(crypto_servercpudebug.integer)
			Con_Printf("is %.1f ms\n", crypto_servercpu_accumulator * 1000);
	}
	return ret;
}

static int Crypto_ClientError(char *data_out, size_t *len_out, const char *msg)
{
	dpsnprintf(data_out, *len_out, "reject %s", msg);
	*len_out = strlen(data_out);
	return CRYPTO_REPLACE;
}

static int Crypto_SoftClientError(char *data_out, size_t *len_out, const char *msg)
{
	*len_out = 0;
	Con_Printf("%s\n", msg);
	return CRYPTO_DISCARD;
}

int Crypto_ClientParsePacket(const char *data_in, size_t len_in, char *data_out, size_t *len_out, lhnetaddress_t *peeraddress)
{
	crypto_t *crypto = &cls.crypto;
	const char *string = data_in;
	const char *s;
	D0_BOOL aes;
	char *data_out_p = data_out;
	D0_BOOL status;

	if(!d0_blind_id_dll)
		return CRYPTO_NOMATCH; // no support

	// if "challenge": verify challenge, and discard message, send next crypto protocol message instead
	// otherwise, just handle actual protocol messages

	if (len_in == 6 && !memcmp(string, "accept", 6) && cls.connect_trying && d0_rijndael_dll)
	{
		int wantserverid = -1;
		Crypto_RetrieveHostKey(&cls.connect_address, &wantserverid, NULL, 0, NULL, 0, NULL);
		if(!crypto || !crypto->authenticated)
		{
			if(wantserverid >= 0)
				return Crypto_ClientError(data_out, len_out, "Server tried an unauthenticated connection even though a host key is present");
			if(crypto_aeslevel.integer >= 3)
				return Crypto_ClientError(data_out, len_out, "This server requires encryption to be not required (crypto_aeslevel <= 2)");
		}
		return CRYPTO_NOMATCH;
	}
	else if (len_in >= 1 && string[0] == 'j' && cls.connect_trying && d0_rijndael_dll && crypto_aeslevel.integer >= 3)
	{
		int wantserverid = -1;
		Crypto_RetrieveHostKey(&cls.connect_address, &wantserverid, NULL, 0, NULL, 0, NULL);
		if(!crypto || !crypto->authenticated)
		{
			if(wantserverid >= 0)
				return Crypto_ClientError(data_out, len_out, "Server tried an unauthenticated connection even though a host key is present");
			if(crypto_aeslevel.integer >= 3)
				return Crypto_ClientError(data_out, len_out, "This server requires encryption to be not required (crypto_aeslevel <= 2)");
		}
		return CRYPTO_NOMATCH;
	}
	else if (len_in >= 13 && !memcmp(string, "infoResponse\x0A", 13))
	{
		s = SearchInfostring(string + 13, "d0_blind_id");
		if(s)
			Crypto_StoreHostKey(peeraddress, s, true);
		return CRYPTO_NOMATCH;
	}
	else if (len_in >= 15 && !memcmp(string, "statusResponse\x0A", 15))
	{
		char save = 0;
		const char *p;
		p = strchr(string + 15, '\n');
		if(p)
		{
			save = *p;
			* (char *) p = 0; // cut off the string there
		}
		s = SearchInfostring(string + 15, "d0_blind_id");
		if(s)
			Crypto_StoreHostKey(peeraddress, s, true);
		if(p)
		{
			* (char *) p = save;
			// invoking those nasal demons again (do not run this on the DS9k)
		}
		return CRYPTO_NOMATCH;
	}
	else if(len_in > 10 && !memcmp(string, "challenge ", 10) && cls.connect_trying)
	{
		const char *vlen_blind_id_ptr = NULL;
		size_t len_blind_id_ptr = 0;
		unsigned long k, v;
		const char *challenge = data_in + 10;
		const char *p;
		int i;
		int clientid = -1, serverid = -1, wantserverid = -1;
		qboolean server_can_auth = true;
		char wantserver_idfp[FP64_SIZE+1];
		int wantserver_aeslevel;

		// if we have a stored host key for the server, assume serverid to already be selected!
		// (the loop will refuse to overwrite this one then)
		wantserver_idfp[0] = 0;
		Crypto_RetrieveHostKey(&cls.connect_address, &wantserverid, NULL, 0, wantserver_idfp, sizeof(wantserver_idfp), &wantserver_aeslevel);
		// requirement: wantserver_idfp is a full ID if wantserverid set

		// if we leave, we have to consider the connection
		// unauthenticated; NOTE: this may be faked by a clever
		// attacker to force an unauthenticated connection; so we have
		// a safeguard check in place when encryption is required too
		// in place, or when authentication is required by the server
		crypto->authenticated = false;

		GetUntilNul(&data_in, &len_in);
		if(!data_in)
			return (wantserverid >= 0) ? Crypto_ClientError(data_out, len_out, "Server tried an unauthenticated connection even though a host key is present") :
				(d0_rijndael_dll && crypto_aeslevel.integer >= 3) ? Crypto_ServerError(data_out, len_out, "This server requires encryption to be not required (crypto_aeslevel <= 2)", NULL) :
				CRYPTO_NOMATCH;

		// FTEQW extension protocol
		while(len_in >= 8)
		{
			k = Crypto_LittleLong(data_in);
			v = Crypto_LittleLong(data_in + 4);
			data_in += 8;
			len_in -= 8;
			switch(k)
			{
				case PROTOCOL_VLEN:
					if(len_in >= 4 + v)
					{
						k = Crypto_LittleLong(data_in);
						data_in += 4;
						len_in -= 4;
						switch(k)
						{
							case PROTOCOL_D0_BLIND_ID:
								vlen_blind_id_ptr = data_in;
								len_blind_id_ptr = v;
								break;
						}
						data_in += v;
						len_in -= v;
					}
					break;
				default:
					break;
			}
		}

		if(!vlen_blind_id_ptr)
			return (wantserverid >= 0) ? Crypto_ClientError(data_out, len_out, "Server tried an unauthenticated connection even though authentication is required") :
				(d0_rijndael_dll && crypto_aeslevel.integer >= 3) ? Crypto_ServerError(data_out, len_out, "This server requires encryption to be not required (crypto_aeslevel <= 2)", NULL) :
				CRYPTO_NOMATCH;

		data_in = vlen_blind_id_ptr;
		len_in = len_blind_id_ptr;

		// parse fingerprints
		//   once we found a fingerprint we can auth to (ANY), select it as clientfp
		//   once we found a fingerprint in the first list that we know, select it as serverfp

		for(;;)
		{
			p = GetUntilNul(&data_in, &len_in);
			if(!p)
				break;
			if(!*p)
			{
				if(!server_can_auth)
					break; // other protocol message may follow
				server_can_auth = false;
				if(clientid >= 0)
					break;
				continue;
			}
			for(i = 0; i < MAX_PUBKEYS; ++i)
			{
				if(pubkeys[i])
				if(!strcmp(p, pubkeys_fp64[i]))
				{
					if(pubkeys_havepriv[i])
						if(clientid < 0)
							clientid = i;
					if(server_can_auth)
						if(serverid < 0)
							if(wantserverid < 0 || i == wantserverid)
								serverid = i;
				}
			}
			if(clientid >= 0 && serverid >= 0)
				break;
		}

		// if stored host key is not found:
		if(wantserverid >= 0 && serverid < 0)
			return Crypto_ClientError(data_out, len_out, "Server CA does not match stored host key, refusing to connect");

		if(serverid >= 0 || clientid >= 0)
		{
			// TODO at this point, fill clientside crypto struct!
			MAKE_CDATA;
			CDATA->cdata_id = ++cdata_id;
			CDATA->s = serverid;
			CDATA->c = clientid;
			memset(crypto->dhkey, 0, sizeof(crypto->dhkey));
			strlcpy(CDATA->challenge, challenge, sizeof(CDATA->challenge));
			crypto->client_keyfp[0] = 0;
			crypto->client_idfp[0] = 0;
			crypto->server_keyfp[0] = 0;
			crypto->server_idfp[0] = 0;
			memcpy(CDATA->wantserver_idfp, wantserver_idfp, sizeof(crypto->server_idfp));

			if(CDATA->wantserver_idfp[0]) // if we know a host key, honor its encryption setting
			switch(bound(0, d0_rijndael_dll ? crypto_aeslevel.integer : 0, 3))
			{
				default: // dummy, never happens, but to make gcc happy...
				case 0:
					if(wantserver_aeslevel >= 3)
						return Crypto_ServerError(data_out, len_out, "This server requires encryption to be not required (crypto_aeslevel <= 2)", NULL);
					CDATA->wantserver_aes = false;
					break;
				case 1:
					CDATA->wantserver_aes = (wantserver_aeslevel >= 2);
					break;
				case 2:
					CDATA->wantserver_aes = (wantserver_aeslevel >= 1);
					break;
				case 3:
					if(wantserver_aeslevel <= 0)
						return Crypto_ServerError(data_out, len_out, "This server requires encryption to be supported (crypto_aeslevel >= 1, and d0_rijndael library must be present)", NULL);
					CDATA->wantserver_aes = true;
					break;
			}

			// build outgoing message
			// append regular stuff
			PutWithNul(&data_out_p, len_out, va("d0pk\\cnt\\0\\id\\%d\\aeslevel\\%d\\challenge\\%s", CDATA->cdata_id, d0_rijndael_dll ? crypto_aeslevel.integer : 0, challenge));
			PutWithNul(&data_out_p, len_out, serverid >= 0 ? pubkeys_fp64[serverid] : "");
			PutWithNul(&data_out_p, len_out, clientid >= 0 ? pubkeys_fp64[clientid] : "");

			if(clientid >= 0)
			{
				// I am the client, and my key is ok... so let's set client_keyfp and client_idfp
				strlcpy(crypto->client_keyfp, pubkeys_fp64[CDATA->c], sizeof(crypto->client_keyfp));
				strlcpy(crypto->client_idfp, pubkeys_priv_fp64[CDATA->c], sizeof(crypto->client_idfp));
			}

			if(serverid >= 0)
			{
				if(!CDATA->id)
					CDATA->id = qd0_blind_id_new();
				if(!CDATA->id)
				{
					CLEAR_CDATA;
					return Crypto_ClientError(data_out, len_out, "d0_blind_id_new failed");
				}
				if(!qd0_blind_id_copy(CDATA->id, pubkeys[CDATA->s]))
				{
					CLEAR_CDATA;
					return Crypto_ClientError(data_out, len_out, "d0_blind_id_copy failed");
				}
				CDATA->next_step = 1;
				*len_out = data_out_p - data_out;
			}
			else if(clientid >= 0)
			{
				// skip over server auth, perform client auth only
				if(!CDATA->id)
					CDATA->id = qd0_blind_id_new();
				if(!CDATA->id)
				{
					CLEAR_CDATA;
					return Crypto_ClientError(data_out, len_out, "d0_blind_id_new failed");
				}
				if(!qd0_blind_id_copy(CDATA->id, pubkeys[CDATA->c]))
				{
					CLEAR_CDATA;
					return Crypto_ClientError(data_out, len_out, "d0_blind_id_copy failed");
				}
				if(!qd0_blind_id_authenticate_with_private_id_start(CDATA->id, true, false, "XONOTIC", 8, data_out_p, len_out)) // len_out receives used size by this op
				{
					CLEAR_CDATA;
					return Crypto_ClientError(data_out, len_out, "d0_blind_id_authenticate_with_private_id_start failed");
				}
				CDATA->next_step = 5;
				data_out_p += *len_out;
				*len_out = data_out_p - data_out;
			}
			else
				*len_out = data_out_p - data_out;

			return CRYPTO_DISCARD;
		}
		else
		{
			if(wantserver_idfp[0]) // if we know a host key, honor its encryption setting
			if(wantserver_aeslevel >= 3)
				return Crypto_ClientError(data_out, len_out, "Server insists on encryption, but neither can authenticate to the other");
			return (d0_rijndael_dll && crypto_aeslevel.integer >= 3) ? Crypto_ServerError(data_out, len_out, "This server requires encryption to be not required (crypto_aeslevel <= 2)", NULL) :
				CRYPTO_NOMATCH;
		}
	}
	else if(len_in > 5 && !memcmp(string, "d0pk\\", 5) && cls.connect_trying)
	{
		const char *cnt;
		int id;
		cnt = SearchInfostring(string + 4, "id");
		id = (cnt ? atoi(cnt) : -1);
		cnt = SearchInfostring(string + 4, "cnt");
		if(!cnt)
			return Crypto_ClientError(data_out, len_out, "d0pk\\ message without cnt");
		GetUntilNul(&data_in, &len_in);
		if(!data_in)
			return Crypto_ClientError(data_out, len_out, "d0pk\\ message without attachment");

		if(!strcmp(cnt, "1"))
		{
			if(id >= 0)
				if(CDATA->cdata_id != id)
					return Crypto_SoftServerError(data_out, len_out, va("Got d0pk\\id\\%d when expecting %d", id, CDATA->cdata_id));
			if(CDATA->next_step != 1)
				return Crypto_SoftClientError(data_out, len_out, va("Got d0pk\\cnt\\%s when expecting %d", cnt, CDATA->next_step));

			cls.connect_nextsendtime = max(cls.connect_nextsendtime, realtime + 1); // prevent "hammering"

			if((s = SearchInfostring(string + 4, "aes")))
				aes = atoi(s);
			else
				aes = false;
			// we CANNOT toggle the AES status any more!
			// as the server already decided
			if(CDATA->wantserver_idfp[0]) // if we know a host key, honor its encryption setting
			if(!aes && CDATA->wantserver_aes)
			{
				CLEAR_CDATA;
				return Crypto_ClientError(data_out, len_out, "Stored host key requires encryption, but server did not enable encryption");
			}
			if(aes && (!d0_rijndael_dll || crypto_aeslevel.integer <= 0))
			{
				CLEAR_CDATA;
				return Crypto_ClientError(data_out, len_out, "Server insists on encryption too hard");
			}
			if(!aes && (d0_rijndael_dll && crypto_aeslevel.integer >= 3))
			{
				CLEAR_CDATA;
				return Crypto_ClientError(data_out, len_out, "Server insists on plaintext too hard");
			}
			crypto->use_aes = aes != 0;

			PutWithNul(&data_out_p, len_out, va("d0pk\\cnt\\2\\id\\%d", CDATA->cdata_id));
			if(!qd0_blind_id_authenticate_with_private_id_challenge(CDATA->id, true, false, data_in, len_in, data_out_p, len_out, &status))
			{
				CLEAR_CDATA;
				return Crypto_ClientError(data_out, len_out, "d0_blind_id_authenticate_with_private_id_challenge failed");
			}
			CDATA->next_step = 3;
			data_out_p += *len_out;
			*len_out = data_out_p - data_out;
			return CRYPTO_DISCARD;
		}
		else if(!strcmp(cnt, "3"))
		{
			static char msgbuf[32];
			size_t msgbuflen = sizeof(msgbuf);
			size_t fpbuflen;

			if(id >= 0)
				if(CDATA->cdata_id != id)
					return Crypto_SoftServerError(data_out, len_out, va("Got d0pk\\id\\%d when expecting %d", id, CDATA->cdata_id));
			if(CDATA->next_step != 3)
				return Crypto_SoftClientError(data_out, len_out, va("Got d0pk\\cnt\\%s when expecting %d", cnt, CDATA->next_step));

			cls.connect_nextsendtime = max(cls.connect_nextsendtime, realtime + 1); // prevent "hammering"

			if(!qd0_blind_id_authenticate_with_private_id_verify(CDATA->id, data_in, len_in, msgbuf, &msgbuflen, &status))
			{
				CLEAR_CDATA;
				return Crypto_ClientError(data_out, len_out, "d0_blind_id_authenticate_with_private_id_verify failed (server authentication error)");
			}
			if(status)
				strlcpy(crypto->server_keyfp, pubkeys_fp64[CDATA->s], sizeof(crypto->server_keyfp));
			else
				crypto->server_keyfp[0] = 0;
			memset(crypto->server_idfp, 0, sizeof(crypto->server_idfp));
			fpbuflen = FP64_SIZE;
			if(!qd0_blind_id_fingerprint64_public_id(CDATA->id, crypto->server_idfp, &fpbuflen))
			{
				CLEAR_CDATA;
				return Crypto_ClientError(data_out, len_out, "d0_blind_id_fingerprint64_public_id failed");
			}
			if(CDATA->wantserver_idfp[0])
			if(memcmp(CDATA->wantserver_idfp, crypto->server_idfp, sizeof(crypto->server_idfp)))
			{
				CLEAR_CDATA;
				return Crypto_ClientError(data_out, len_out, "Server ID does not match stored host key, refusing to connect");
			}
			fpbuflen = DHKEY_SIZE;
			if(!qd0_blind_id_sessionkey_public_id(CDATA->id, (char *) crypto->dhkey, &fpbuflen))
			{
				CLEAR_CDATA;
				return Crypto_ClientError(data_out, len_out, "d0_blind_id_sessionkey_public_id failed");
			}

			// cache the server key
			Crypto_StoreHostKey(&cls.connect_address, va("%d %s@%s", crypto->use_aes ? 1 : 0, crypto->server_idfp, pubkeys_fp64[CDATA->s]), false);

			if(CDATA->c >= 0)
			{
				// client will auth next
				PutWithNul(&data_out_p, len_out, va("d0pk\\cnt\\4\\id\\%d", CDATA->cdata_id));
				if(!qd0_blind_id_copy(CDATA->id, pubkeys[CDATA->c]))
				{
					CLEAR_CDATA;
					return Crypto_ClientError(data_out, len_out, "d0_blind_id_copy failed");
				}
				if(!qd0_blind_id_authenticate_with_private_id_start(CDATA->id, true, false, "XONOTIC", 8, data_out_p, len_out)) // len_out receives used size by this op
				{
					CLEAR_CDATA;
					return Crypto_ClientError(data_out, len_out, "d0_blind_id_authenticate_with_private_id_start failed");
				}
				CDATA->next_step = 5;
				data_out_p += *len_out;
				*len_out = data_out_p - data_out;
				return CRYPTO_DISCARD;
			}
			else
			{
				// session key is FINISHED (no server part is to be expected)! By this, all keys are set up
				crypto->authenticated = true;
				CDATA->next_step = 0;
				// assume we got the empty challenge to finish the protocol
				PutWithNul(&data_out_p, len_out, "challenge ");
				*len_out = data_out_p - data_out;
				--*len_out; // remove NUL terminator
				return CRYPTO_REPLACE;
			}
		}
		else if(!strcmp(cnt, "5"))
		{
			size_t fpbuflen;
			unsigned char dhkey[DHKEY_SIZE];
			int i;

			if(id >= 0)
				if(CDATA->cdata_id != id)
					return Crypto_SoftServerError(data_out, len_out, va("Got d0pk\\id\\%d when expecting %d", id, CDATA->cdata_id));
			if(CDATA->next_step != 5)
				return Crypto_SoftClientError(data_out, len_out, va("Got d0pk\\cnt\\%s when expecting %d", cnt, CDATA->next_step));

			cls.connect_nextsendtime = max(cls.connect_nextsendtime, realtime + 1); // prevent "hammering"

			if(CDATA->s < 0) // only if server didn't auth
			{
				if((s = SearchInfostring(string + 4, "aes")))
					aes = atoi(s);
				else
					aes = false;
				if(CDATA->wantserver_idfp[0]) // if we know a host key, honor its encryption setting
				if(!aes && CDATA->wantserver_aes)
				{
					CLEAR_CDATA;
					return Crypto_ClientError(data_out, len_out, "Stored host key requires encryption, but server did not enable encryption");
				}
				if(aes && (!d0_rijndael_dll || crypto_aeslevel.integer <= 0))
				{
					CLEAR_CDATA;
					return Crypto_ClientError(data_out, len_out, "Server insists on encryption too hard");
				}
				if(!aes && (d0_rijndael_dll && crypto_aeslevel.integer >= 3))
				{
					CLEAR_CDATA;
					return Crypto_ClientError(data_out, len_out, "Server insists on plaintext too hard");
				}
				crypto->use_aes = aes != 0;
			}

			PutWithNul(&data_out_p, len_out, va("d0pk\\cnt\\6\\id\\%d", CDATA->cdata_id));
			if(!qd0_blind_id_authenticate_with_private_id_response(CDATA->id, data_in, len_in, data_out_p, len_out))
			{
				CLEAR_CDATA;
				return Crypto_ClientError(data_out, len_out, "d0_blind_id_authenticate_with_private_id_response failed");
			}
			fpbuflen = DHKEY_SIZE;
			if(!qd0_blind_id_sessionkey_public_id(CDATA->id, (char *) dhkey, &fpbuflen))
			{
				CLEAR_CDATA;
				return Crypto_ClientError(data_out, len_out, "d0_blind_id_sessionkey_public_id failed");
			}
			// XOR the two DH keys together to make one
			for(i = 0; i < DHKEY_SIZE; ++i)
				crypto->dhkey[i] ^= dhkey[i];
			// session key is FINISHED! By this, all keys are set up
			crypto->authenticated = true;
			CDATA->next_step = 0;
			data_out_p += *len_out;
			*len_out = data_out_p - data_out;
			return CRYPTO_DISCARD;
		}
		return Crypto_SoftClientError(data_out, len_out, "Got unknown d0_blind_id message from server");
	}

	return CRYPTO_NOMATCH;
}

size_t Crypto_SignData(const void *data, size_t datasize, int keyid, void *signed_data, size_t signed_size)
{
	if(keyid < 0 || keyid >= MAX_PUBKEYS)
		return 0;
	if(!pubkeys_havepriv[keyid])
		return 0;
	if(qd0_blind_id_sign_with_private_id_sign(pubkeys[keyid], true, false, (const char *)data, datasize, (char *)signed_data, &signed_size))
		return signed_size;
	return 0;
}

size_t Crypto_SignDataDetached(const void *data, size_t datasize, int keyid, void *signed_data, size_t signed_size)
{
	if(keyid < 0 || keyid >= MAX_PUBKEYS)
		return 0;
	if(!pubkeys_havepriv[keyid])
		return 0;
	if(qd0_blind_id_sign_with_private_id_sign_detached(pubkeys[keyid], true, false, (const char *)data, datasize, (char *)signed_data, &signed_size))
		return signed_size;
	return 0;
}
