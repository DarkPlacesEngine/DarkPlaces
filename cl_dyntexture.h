// Andreas 'Black' Kirsch 07
#ifndef CL_DYNTEXTURE_H
#define CL_DYNTEXTURE_H

#define DYNAMIC_TEXTURE_PATH_PREFIX			"_dynamic/"
#define MAX_DYNAMIC_TEXTURE_COUNT			64

// return a valid texture handle for a dynamic texture (might be filler texture if it hasnt been initialized yet)
// textureflags will be ignored though for now [11/22/2007 Black]
rtexture_t * CL_GetDynTexture( const char *name );

// link a texture handle as dynamic texture and update texture handles in the renderer and draw_* accordingly
void CL_LinkDynTexture( const char *name, rtexture_t *texture );

// unlink a texture handle from its name
void CL_UnlinkDynTexture( const char *name );

#endif