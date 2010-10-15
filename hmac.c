#include "quakedef.h"
#include "hmac.h"

qboolean hmac(
	hashfunc_t hfunc, int hlen, int hblock,
	unsigned char *out,
	const unsigned char *in, int n,
	const unsigned char *key, int k
)
{
	static unsigned char hashbuf[32];
	static unsigned char k_xor_ipad[128];
	static unsigned char k_xor_opad[128];
	static unsigned char catbuf[65600]; // 65535 bytes max quake packet size + 64 for the hash
	int i;

	if(sizeof(hashbuf) < (size_t) hlen)
		return false;
	if(sizeof(k_xor_ipad) < (size_t) hblock)
		return false;
	if(sizeof(k_xor_ipad) < (size_t) hlen)
		return false;
	if(sizeof(catbuf) < (size_t) hblock + (size_t) hlen)
		return false;
	if(sizeof(catbuf) < (size_t) hblock + (size_t) n)
		return false;

	if(k > hblock)
	{
		// hash the key if it is too long
		hfunc(k_xor_opad, key, k);
		key = k_xor_opad;
		k = hlen;
	}

	if(k < hblock)
	{
		// zero pad the key if it is too short
		if(key != k_xor_opad)
			memcpy(k_xor_opad, key, k);
		for(i = k; i < hblock; ++i)
			k_xor_opad[i] = 0;
		key = k_xor_opad;
		k = hblock;
	}

	for(i = 0; i < hblock; ++i)
	{
		k_xor_ipad[i] = key[i] ^ 0x36;
		k_xor_opad[i] = key[i] ^ 0x5c;
	}

	memcpy(catbuf, k_xor_ipad, hblock);
	memcpy(catbuf + hblock, in, n);
	hfunc(hashbuf, catbuf, hblock + n);
	memcpy(catbuf, k_xor_opad, hblock);
	memcpy(catbuf + hblock, hashbuf, hlen);
	hfunc(out, catbuf, hblock + hlen);
	return true;
}
