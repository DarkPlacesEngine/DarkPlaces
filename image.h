
#ifndef IMAGE_H
#define IMAGE_H

// applies gamma correction to RGB pixels, in can be the same as out
void Image_GammaRemapRGB(qbyte *in, qbyte *out, int pixels, qbyte *gammar, qbyte *gammag, qbyte *gammab);

// converts 8bit image data to RGBA, in can not be the same as out
void Image_Copy8bitRGBA(qbyte *in, qbyte *out, int pixels, int *pal);

// makes a RGBA mask from RGBA input, in can be the same as out
int image_makemask (qbyte *in, qbyte *out, int size);

// loads a texture, as pixel data
qbyte *loadimagepixels (char* filename, qboolean complain, int matchwidth, int matchheight);

// loads a texture, as a texture
rtexture_t *loadtextureimage (rtexturepool_t *pool, char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap, qboolean precache);

// loads a texture's alpha mask, as pixel data
qbyte *loadimagepixelsmask (char* filename, qboolean complain, int matchwidth, int matchheight);

// loads a texture's alpha mask, as a texture
rtexture_t *loadtextureimagemask (rtexturepool_t *pool, char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap, qboolean precache);

// loads a texture and it's alpha mask at once (NULL if it has no translucent pixels)
rtexture_t *image_masktex;
rtexture_t *loadtextureimagewithmask (rtexturepool_t *pool, char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap, qboolean precache);

// writes a RGB TGA that is already upside down (which TGA wants)
void Image_WriteTGARGB_preflipped (char *filename, int width, int height, qbyte *data);

// writes a RGB TGA
void Image_WriteTGARGB (char *filename, int width, int height, qbyte *data);

// writes a RGBA TGA
void Image_WriteTGARGBA (char *filename, int width, int height, qbyte *data);

// returns true if the image has some translucent pixels
qboolean Image_CheckAlpha(qbyte *data, int size, qboolean rgba);

// resizes the image (in can not be the same as out)
void Image_Resample (void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight, int bytesperpixel, int quality);

// scales the image down by a power of 2 (in can be the same as out)
void Image_MipReduce(qbyte *in, qbyte *out, int *width, int *height, int destwidth, int destheight, int bytesperpixel);

#endif
