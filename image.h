
extern void Image_Copy8bitRGBA(byte *in, byte *out, int pixels, int *pal);
extern void Image_CopyRGBAGamma(byte *in, byte *out, int pixels);
extern int image_makemask (byte *in, byte *out, int size);
extern byte* loadimagepixelsmask (char* filename, qboolean complain, int matchwidth, int matchheight);
extern int loadtextureimagemask (char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap);
extern int image_masktexnum;
extern int loadtextureimagewithmask (char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap);
