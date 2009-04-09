#ifndef HMAC_H
#define HMAC_H

typedef void (*hashfunc_t) (unsigned char *out, unsigned char *in, int n);
void hmac(
	hashfunc_t hfunc, int hlen, int hblock,
	unsigned char *out,
	unsigned char *in, int n,
	unsigned char *key, int k
);

#define HMAC_MDFOUR_16BYTES(out, in, n, key, k) hmac(mdfour, 16, 64, out, in, n, key, k)

#endif
