
#include <stdlib.h>

void fractalnoise(unsigned char *noise, int size)
{
	int x, y, g, g2, amplitude, amplitude2, min, max, size1 = size - 1;
	int *noisebuf;
#define n(x,y) noisebuf[((y)&size1)*size+((x)&size1)]
	noisebuf = calloc(size*size, sizeof(int));

	amplitude = 32767;
	g2 = size;
	n(0,0) = 0;
	for (;(g = g2 >> 1) >= 1;g2 >>= 1)
	{
		// subdivide, diamond-square algorythm (really this has little to do with squares)
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
		// brownian motion theory
		amplitude >>= 1;
		amplitude2 = amplitude >> 1;
		for (y = 0;y < size;y += g)
			for (x = 0;x < size;x += g)
				n(x,y) += (rand()&amplitude) - amplitude2;
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
	// normalize noise and copy to output
	for (y = 0;y < size;y++)
		for (x = 0;x < size;x++)
			*noise++ = (n(x,y) - min) * 255 / max;
	free(noisebuf);
#undef n
}