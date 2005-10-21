
#include "quakedef.h"

void fractalnoise(qbyte *noise, int size, int startgrid)
{
	int x, y, g, g2, amplitude, min, max, size1 = size - 1, sizepower, gridpower;
	int *noisebuf;
#define n(x,y) noisebuf[((y)&size1)*size+((x)&size1)]

	for (sizepower = 0;(1 << sizepower) < size;sizepower++);
	if (size != (1 << sizepower))
	{
		Con_Printf("fractalnoise: size must be power of 2\n");
		return;
	}

	for (gridpower = 0;(1 << gridpower) < startgrid;gridpower++);
	if (startgrid != (1 << gridpower))
	{
		Con_Printf("fractalnoise: grid must be power of 2\n");
		return;
	}

	startgrid = bound(0, startgrid, size);

	amplitude = 0xFFFF; // this gets halved before use
	noisebuf = (int *)Mem_Alloc(tempmempool, size*size*sizeof(int));
	memset(noisebuf, 0, size*size*sizeof(int));

	for (g2 = startgrid;g2;g2 >>= 1)
	{
		// brownian motion (at every smaller level there is random behavior)
		amplitude >>= 1;
		for (y = 0;y < size;y += g2)
			for (x = 0;x < size;x += g2)
				n(x,y) += (rand()&amplitude);

		g = g2 >> 1;
		if (g)
		{
			// subdivide, diamond-square algorithm (really this has little to do with squares)
			// diamond
			for (y = 0;y < size;y += g2)
				for (x = 0;x < size;x += g2)
					n(x+g,y+g) = (n(x,y) + n(x+g2,y) + n(x,y+g2) + n(x+g2,y+g2)) >> 2;
			// square
			for (y = 0;y < size;y += g2)
				for (x = 0;x < size;x += g2)
				{
					n(x+g,y) = (n(x,y) + n(x+g2,y) + n(x+g,y-g) + n(x+g,y+g)) >> 2;
					n(x,y+g) = (n(x,y) + n(x,y+g2) + n(x-g,y+g) + n(x+g,y+g)) >> 2;
				}
		}
	}
	// find range of noise values
	min = max = 0;
	for (y = 0;y < size;y++)
		for (x = 0;x < size;x++)
		{
			if (n(x,y) < min) min = n(x,y);
			if (n(x,y) > max) max = n(x,y);
		}
	max -= min;
	max++;
	// normalize noise and copy to output
	for (y = 0;y < size;y++)
		for (x = 0;x < size;x++)
			*noise++ = (qbyte) (((n(x,y) - min) * 256) / max);
	Mem_Free(noisebuf);
#undef n
}

// unnormalized, used for explosions mainly, does not allocate/free memory (hence the name quick)
void fractalnoisequick(qbyte *noise, int size, int startgrid)
{
	int x, y, g, g2, amplitude, size1 = size - 1, sizepower, gridpower;
#define n(x,y) noise[((y)&size1)*size+((x)&size1)]

	for (sizepower = 0;(1 << sizepower) < size;sizepower++);
	if (size != (1 << sizepower))
	{
		Con_Printf("fractalnoise: size must be power of 2\n");
		return;
	}

	for (gridpower = 0;(1 << gridpower) < startgrid;gridpower++);
	if (startgrid != (1 << gridpower))
	{
		Con_Printf("fractalnoise: grid must be power of 2\n");
		return;
	}

	startgrid = bound(0, startgrid, size);

	amplitude = 255; // this gets halved before use
	memset(noise, 0, size*size);

	for (g2 = startgrid;g2;g2 >>= 1)
	{
		// brownian motion (at every smaller level there is random behavior)
		amplitude >>= 1;
		for (y = 0;y < size;y += g2)
			for (x = 0;x < size;x += g2)
				n(x,y) += (rand()&amplitude);

		g = g2 >> 1;
		if (g)
		{
			// subdivide, diamond-square algorithm (really this has little to do with squares)
			// diamond
			for (y = 0;y < size;y += g2)
				for (x = 0;x < size;x += g2)
					n(x+g,y+g) = (qbyte) (((int) n(x,y) + (int) n(x+g2,y) + (int) n(x,y+g2) + (int) n(x+g2,y+g2)) >> 2);
			// square
			for (y = 0;y < size;y += g2)
				for (x = 0;x < size;x += g2)
				{
					n(x+g,y) = (qbyte) (((int) n(x,y) + (int) n(x+g2,y) + (int) n(x+g,y-g) + (int) n(x+g,y+g)) >> 2);
					n(x,y+g) = (qbyte) (((int) n(x,y) + (int) n(x,y+g2) + (int) n(x-g,y+g) + (int) n(x+g,y+g)) >> 2);
				}
		}
	}
#undef n
}

