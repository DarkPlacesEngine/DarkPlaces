
void Image_Copy8bitRGBA(byte *in, byte *out, int pixels, int *pal);
void Image_CopyRGBAGamma(byte *in, byte *out, int pixels);
int image_makemask (byte *in, byte *out, int size);
byte* loadimagepixels (char* filename, qboolean complain, int matchwidth, int matchheight);
rtexture_t *loadtextureimage (char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap, qboolean precache);
byte* loadimagepixelsmask (char* filename, qboolean complain, int matchwidth, int matchheight);
rtexture_t *loadtextureimagemask (char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap, qboolean precache);
rtexture_t *image_masktex;
rtexture_t *loadtextureimagewithmask (char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap, qboolean precache);
void Image_WriteTGARGB_preflipped (char *filename, int width, int height, byte *data);
void Image_WriteTGARGB (char *filename, int width, int height, byte *data);
void Image_WriteTGARGBA (char *filename, int width, int height, byte *data);
qboolean Image_CheckAlpha(byte *data, int size, qboolean rgba);
