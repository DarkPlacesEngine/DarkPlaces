
#ifndef IMAGE_H
#define IMAGE_H

// swizzle components (even converting number of components) and flip images
// (warning: input must be different than output due to non-linear read/write)
// (tip: inputcomponentindices can contain values | 0x80000000 to tell it to
// store them directly into output, so 255 | 0x80000000 would write 255)
void Image_CopyMux(qbyte *outpixels, const qbyte *inpixels, int width, int height, int flipx, int flipy, int flipdiagonal, int numincomponents, int numoutcomponents, int *inputcomponentindices);

// applies gamma correction to RGB pixels, in can be the same as out
void Image_GammaRemapRGB(const qbyte *in, qbyte *out, int pixels, const qbyte *gammar, const qbyte *gammag, const qbyte *gammab);

// converts 8bit image data to RGBA, in can not be the same as out
void Image_Copy8bitRGBA(const qbyte *in, qbyte *out, int pixels, const unsigned int *pal);

// makes a RGBA mask from RGBA input, in can be the same as out
int image_makemask (const qbyte *in, qbyte *out, int size);

// loads a texture, as pixel data
qbyte *loadimagepixels (const char *filename, qboolean complain, int matchwidth, int matchheight);

// loads a texture, as a texture
rtexture_t *loadtextureimage (rtexturepool_t *pool, const char *filename, int matchwidth, int matchheight, qboolean complain, int flags);

// loads a texture's alpha mask, as pixel data
qbyte *loadimagepixelsmask (const char *filename, qboolean complain, int matchwidth, int matchheight);

// loads a texture's alpha mask, as a texture
rtexture_t *loadtextureimagemask (rtexturepool_t *pool, const char *filename, int matchwidth, int matchheight, qboolean complain, int flags);

// loads a texture and it's alpha mask at once (NULL if it has no translucent pixels)
rtexture_t *image_masktex;
rtexture_t *image_nmaptex;
rtexture_t *loadtextureimagewithmask (rtexturepool_t *pool, const char *filename, int matchwidth, int matchheight, qboolean complain, int flags);
rtexture_t *loadtextureimagewithmaskandnmap (rtexturepool_t *pool, const char *filename, int matchwidth, int matchheight, qboolean complain, int flags, float bumpscale);
rtexture_t *loadtextureimagebumpasnmap (rtexturepool_t *pool, const char *filename, int matchwidth, int matchheight, qboolean complain, int flags, float bumpscale);

// writes a RGB TGA that is already upside down (which TGA wants)
qboolean Image_WriteTGARGB_preflipped (const char *filename, int width, int height, const qbyte *data);

// writes a RGB TGA
void Image_WriteTGARGB (const char *filename, int width, int height, const qbyte *data);

// writes a RGBA TGA
void Image_WriteTGARGBA (const char *filename, int width, int height, const qbyte *data);

// returns true if the image has some translucent pixels
qboolean Image_CheckAlpha(const qbyte *data, int size, qboolean rgba);

// resizes the image (in can not be the same as out)
void Image_Resample (const void *indata, int inwidth, int inheight, int indepth, void *outdata, int outwidth, int outheight, int outdepth, int bytesperpixel, int quality);

// scales the image down by a power of 2 (in can be the same as out)
void Image_MipReduce(const qbyte *in, qbyte *out, int *width, int *height, int *depth, int destwidth, int destheight, int destdepth, int bytesperpixel);

// only used by menuplyr coloring
qbyte *LoadLMPAs8Bit (qbyte *f, int matchwidth, int matchheight);

void Image_HeightmapToNormalmap(const unsigned char *inpixels, unsigned char *outpixels, int width, int height, int clamp, float bumpscale);

typedef struct imageskin_s
{
	qbyte *basepixels;int basepixels_width;int basepixels_height;
	qbyte *nmappixels;int nmappixels_width;int nmappixels_height;
	qbyte *glowpixels;int glowpixels_width;int glowpixels_height;
	qbyte *glosspixels;int glosspixels_width;int glosspixels_height;
	qbyte *pantspixels;int pantspixels_width;int pantspixels_height;
	qbyte *shirtpixels;int shirtpixels_width;int shirtpixels_height;
	qbyte *maskpixels;int maskpixels_width;int maskpixels_height;
}
imageskin_t;

int image_loadskin(imageskin_t *s, char *name);
void image_freeskin(imageskin_t *s);

#endif

