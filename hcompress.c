
// LordHavoc: my little compression library

#if 0
#include <stdlib.h>

#ifndef byte
typedef unsigned char byte;
#endif

#define HCOMPRESS_CORRUPT -1

typedef struct
{
	int position;
	int size;
	byte *data;
} hcblock;

typedef struct
{
	int identifer;
	int compressedsize;
	int decompressedsize;
	int compressedcrc;
	int decompressedcrc;
	byte data[0];
} storagehcblock;

int hc_readimmediate(hcblock *b)
{
	return b->data[b->position++];
}

int hc_readsize(hcblock *b)
{
	b->position += 2;
	return b->data[b->position - 2];
}

int hc_readoffset(hcblock *b)
{
	b->position += 2;
	return b->data[b->position - 2];
}

int hc_readbit(hcblock *b)
{
	return b->data[b->position++];
}

void hc_writeimmediate(hcblock *b, int num)
{
	b->data[b->size++] = num;
}

void hc_writesize(hcblock *b, int num)
{
	b->data[b->size] = num;
	b->size += 2;
}

void hc_writeoffset(hcblock *b, int num)
{
	b->data[b->size] = num;
	b->size += 2;
}

void hc_writebit(hcblock *b, int num)
{
	b->data[b->size++] = num;
}

int hcompress_decompress(void *inaddr, void *outaddr, int insize, int outsize)
{
	/*
	byte *in, *out;
	hcblock b;
	b.position = 0;
	b.size = 0;
	b.
	b = inaddr;
	if (
	int commandbits, commandbyte, count, temp;
	in = inaddr;
	out = outaddr;
	while (outsize && insize)
	{
		hc_readbit(
		if (!commandbits)
		{
			if (!insize)
				return HCOMPRESS_CORRUPT;
			commandbyte = *in++;
			commandbits = 8;
			insize--;
			if (!insize)
				return HCOMPRESS_CORRUPT;
		}
		if (commandbyte)
		{
			for (;commandbits && outsize;commandbits--,commandbyte >>= 1)
			{
				if (commandbyte & 1) // reference
				{
					if (insize < 2)
						return HCOMPRESS_CORRUPT;
					size = (in[0] >> 4) + 3;
					if (size > outsize)
						return HCOMPRESS_CORRUPT;
					insize -= 2;
					outsize -= size;
					tempout = out - ((((in[0] << 8) | in[1]) & 0xFFF) + 1);
					if ((int) tempout < (int) outaddr)
						return HCOMPRESS_CORRUPT;
					while (size--)
						*out++ = *tempout++;
				}
				else
				{
					if (!insize || !outsize)
						return HCOMPRESS_CORRUPT;
					*out++ = *in++;
					insize--;
					outsize--;
				}
			}
		}
		else // copy 8 bytes straight
		{
			if (insize < 8 || outsize < 8)
				return HCOMPRESS_CORRUPT;
			*out++ = *in++;
			*out++ = *in++;
			*out++ = *in++;
			*out++ = *in++;
			*out++ = *in++;
			*out++ = *in++;
			*out++ = *in++;
			*out++ = *in++;
			insize -= 8;
			outsize -= 8;
		}
	}
	if (insize || outsize)
		return HCOMPRESS_CORRUPT;
	return ((int) out - (int) outaddr);
	*/
	return HCOMPRESS_CORRUPT;
}

int hcompress_compress(void *indata, void *outdata, int size)
{
	byte *in, *out;
	struct hctoken
	{
		unsigned short size; // if size == 0, offset holds the immediate value
		unsigned short offset;
	} *token;
	int offsetcount[65536];
	int sizecount[256];
	int tokens = 0;
	int position = 0;
	int c, i, j, l, bestsize, bestposition, maxlen;
	int *h;
	byte *c1, *c2;
	struct
	{
		int start; // start of the chain
		int length; // length of the chain
	} hashindex[256][256];
	int *hashtable;
	token = qmalloc(size*sizeof(struct hctoken));
	hashtable = qmalloc(size*sizeof(int));
	in = indata;
	memset(&hashindex, 0, sizeof(hashindex));
	// count the chain lengths
	for (i = 0;i < size-1;i++)
		hashindex[in[i]][in[i+1]].length++;
	hashindex[in[i]][0].length++;
	// assign starting positions for each chain
	c = 0;
	for (i = 0;i < 256;i++)
	{
		for (j = 0;j < 256;j++)
		{
			hashindex[i][j].start = c;
			c += hashindex[i][j].length;
		}
	}
	// enter the data into the chains
	for (i = 0;i < size-1;i++)
		hashtable[hashindex[in[i]][in[i+1]].start++] = i;
	hashtable[hashindex[in[i]][0].start++] = i;
	// adjust start positions back to what they should be
	for (i = 0;i < 256;i++)
		for (j = 0;j < 256;j++)
			hashindex[i][j].start -= hashindex[i][j].length;
	// now the real work
	out = outdata;
	while (position < size)
	{
		c = *in++;
		if (position + 1 == size) // this is the last byte
		{
			h = &hashtable[hashindex[c][0].start];
			l = hashindex[c][0].length;
		}
		else
		{
			h = &hashtable[hashindex[c][*in].start];
			l = hashindex[c][0].length;
		}
		if (l)
		{
			if (*h < position - 65535) // too old, nudge up the chain to avoid finding this one again
			{
				if (position + 1 == size)
				{
					hashindex[c][0].start++;
					hashindex[c][0].length--;
				}
				else
				{
					hashindex[c][*in].start++;
					hashindex[c][*in].length--;
				}
				h++;
				l--;
			}
			if (l)
			{
				bestsize = 0;
				bestposition = 0;
				while (l--)
				{
					c1 = &in[*h];
					c2 = &in[position];
					maxlen = size - position;
					if (maxlen > 258)
						maxlen = 258;
					for (i = 0;i < maxlen;i++)
						if (*c1++ != *c2++)
							break;
					if (i > bestsize)
					{
						bestsize = i;
						bestposition = *h;
					}
					h++;
				}
				if (bestsize >= 3)
				{
					// write a reference
					token[tokens].size = bestsize;
					token[tokens++].offset = position - bestposition; // offset backward
					sizecount[bestsize - 3]++;
					offsetcount[position - bestposition]++;
				}
				else
				{
					// write an immediate
					token[tokens].size = 0;
					token[tokens++].offset = c;
				}
			}
			else
			{
				// no remaining occurances, write an immediate
				token[tokens].size = 0;
				token[tokens++].offset = c;
			}
		}
		else
		{
			// no remaining occurances, write an immediate
			token[tokens].size = 0;
			token[tokens++].offset = c;
		}
	}
	return HCOMPRESS_CORRUPT;

	/*
	int i, index, insize = size, outsize = 0, commandbyte = 0, commandbits = 0;
	struct hcompress_hashchain
	{
		short prev, next;
		int key;
	} hashchain[4096];
	short hashindex[65536];
	int hashvalue[4096];
	struct
	{
		byte type;
		unsigned short data;
	} ref[8];
	for (i = 0;i < 65536;i++)
		hashindex[i] = -1;
	for (i = 0;i < 4096;i++)
	{
		hashchain[i].next = -1;
		hashchain[i].key = -1;
	}
	in = indata;
	out = outdata;
	while(insize)
	{
		if (insize >= 3) // enough data left to compress
		{
			key = in[0] | (in[1] << 8);
			if (hashindex[
			for (
			index = ((int) in + 1) & 0xFFF;
			if (hash[index].key >= 0)
			{
				if (hashindex[hash[index].key] == index)
					hashindex[hash[index].key] = -1;
				if (hash[index].prev >= 0)
					hash[hash[index].prev].next = hash[index].next;
			}
			hash[index].key = key;
			hash[index].next = hashindex[key];
			hashindex[key] = index;
		}
		else
		{
			while (insize--)
			{
				ref[commandbits].type = 0;
				ref[commandbits++].data = *in++;
			}
		}
	}
	*/
}
#endif