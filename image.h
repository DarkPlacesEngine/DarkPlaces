
void Image_GammaRemapRGB(qbyte *in, qbyte *out, int pixels, qbyte *gammar, qbyte *gammag, qbyte *gammab);
void Image_Copy8bitRGBA(qbyte *in, qbyte *out, int pixels, int *pal);
int image_makemask (qbyte *in, qbyte *out, int size);
qbyte *loadimagepixels (char* filename, qboolean complain, int matchwidth, int matchheight);
rtexture_t *loadtextureimage (rtexturepool_t *pool, char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap, qboolean precache);
qbyte *loadimagepixelsmask (char* filename, qboolean complain, int matchwidth, int matchheight);
rtexture_t *loadtextureimagemask (rtexturepool_t *pool, char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap, qboolean precache);
rtexture_t *image_masktex;
rtexture_t *loadtextureimagewithmask (rtexturepool_t *pool, char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap, qboolean precache);
void Image_WriteTGARGB_preflipped (char *filename, int width, int height, qbyte *data);
void Image_WriteTGARGB (char *filename, int width, int height, qbyte *data);
void Image_WriteTGARGBA (char *filename, int width, int height, qbyte *data);
qboolean Image_CheckAlpha(qbyte *data, int size, qboolean rgba);
