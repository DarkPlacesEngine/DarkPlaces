// Andreas 'Black' Kirsch 07
#ifndef CL_DYNTEXTURE_H
#define CL_DYNTEXTURE_H

#define CLDYNTEXTUREPREFIX			"_dynamic/"

// always path fully specified names to the dynamic texture functions! (ie. with the _dynamic/ prefix, etc.!)

// return a valid texture handle for a dynamic texture (might be filler texture if it hasnt been initialized yet)
// or NULL if its not a valid dynamic texture name
rtexture_t * CL_GetDynTexture( const char *name );

// link a texture handle as dynamic texture and update texture handles in the renderer and draw_* accordingly
void CL_LinkDynTexture( const char *name, rtexture_t *texture );

// unlink a texture handle from its name
void CL_UnlinkDynTexture( const char *name );

#endif

