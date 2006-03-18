
#ifndef IMAGE_H
#define IMAGE_H


extern int image_width, image_height;


// swizzle components (even converting number of components) and flip images
// (warning: input must be different than output due to non-linear read/write)
// (tip: component indices can contain values | 0x80000000 to tell it to
// store them directly into output, so 255 | 0x80000000 would write 255)
void Image_CopyMux(unsigned char *outpixels, const unsigned char *inpixels, int inputwidth, int inputheight, qboolean inputflipx, qboolean inputflipy, qboolean inputflipdiagonal, int numoutputcomponents, int numinputcomponents, int *outputinputcomponentindices);

// applies gamma correction to RGB pixels, in can be the same as out
void Image_GammaRemapRGB(const unsigned char *in, unsigned char *out, int pixels, const unsigned char *gammar, const unsigned char *gammag, const unsigned char *gammab);

// converts 8bit image data to RGBA, in can not be the same as out
void Image_Copy8bitRGBA(const unsigned char *in, unsigned char *out, int pixels, const unsigned int *pal);

unsigned char *LoadTGA (const unsigned char *f, int filesize, int matchwidth, int matchheight);

// loads a texture, as pixel data
unsigned char *loadimagepixels (const char *filename, qboolean complain, int matchwidth, int matchheight);

// loads a texture, as a texture
rtexture_t *loadtextureimage (rtexturepool_t *pool, const char *filename, int matchwidth, int matchheight, qboolean complain, int flags);

// writes a RGB TGA that is already upside down (which TGA wants)
qboolean Image_WriteTGARGB_preflipped (const char *filename, int width, int height, const unsigned char *data, unsigned char *buffer);

// writes a RGBA TGA
void Image_WriteTGARGBA (const char *filename, int width, int height, const unsigned char *data);

// resizes the image (in can not be the same as out)
void Image_Resample (const void *indata, int inwidth, int inheight, int indepth, void *outdata, int outwidth, int outheight, int outdepth, int bytesperpixel, int quality);

// scales the image down by a power of 2 (in can be the same as out)
void Image_MipReduce(const unsigned char *in, unsigned char *out, int *width, int *height, int *depth, int destwidth, int destheight, int destdepth, int bytesperpixel);

// only used by menuplyr coloring
unsigned char *LoadLMP (const unsigned char *f, int filesize, int matchwidth, int matchheight, qboolean loadAs8Bit);

void Image_HeightmapToNormalmap(const unsigned char *inpixels, unsigned char *outpixels, int width, int height, int clamp, float bumpscale);

typedef struct imageskin_s
{
	unsigned char *basepixels;int basepixels_width;int basepixels_height;
	unsigned char *nmappixels;int nmappixels_width;int nmappixels_height;
	unsigned char *glowpixels;int glowpixels_width;int glowpixels_height;
	unsigned char *glosspixels;int glosspixels_width;int glosspixels_height;
	unsigned char *pantspixels;int pantspixels_width;int pantspixels_height;
	unsigned char *shirtpixels;int shirtpixels_width;int shirtpixels_height;
	unsigned char *maskpixels;int maskpixels_width;int maskpixels_height;
}
imageskin_t;

int image_loadskin(imageskin_t *s, const char *name);
void image_freeskin(imageskin_t *s);

#endif

