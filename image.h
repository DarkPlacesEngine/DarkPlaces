
extern void Image_Copy8bitRGBA(byte *in, byte *out, int pixels, int *pal);
extern void Image_CopyRGBAGamma(byte *in, byte *out, int pixels);
extern int image_makemask (byte *in, byte *out, int size);
extern byte* loadimagepixels (char* filename, qboolean complain, int matchwidth, int matchheight);
extern rtexture_t *loadtextureimage (char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap, qboolean precache);
extern byte* loadimagepixelsmask (char* filename, qboolean complain, int matchwidth, int matchheight);
extern rtexture_t *loadtextureimagemask (char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap, qboolean precache);
extern rtexture_t *image_masktex;
extern rtexture_t *loadtextureimagewithmask (char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap, qboolean precache);
extern void Image_WriteTGARGB_preflipped (char *filename, int width, int height, byte *data);
extern void Image_WriteTGARGB (char *filename, int width, int height, byte *data);
extern void Image_WriteTGARGBA (char *filename, int width, int height, byte *data);
