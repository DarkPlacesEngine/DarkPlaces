// Andreas Kirsch 07

#include "quakedef.h"
#include "cl_dyntexture.h"

typedef struct dyntexture_s {
	// everything after DYNAMIC_TEXTURE_PATH_PREFIX
	char name[ MAX_QPATH + 32 ];
	// texture pointer (points to r_texture_white at first)
	rtexture_t *texture;
} dyntexture_t;

static dyntexture_t dyntextures[ MAX_DYNAMIC_TEXTURE_COUNT ];
static unsigned dyntexturecount;

#define DEFAULT_DYNTEXTURE r_texture_grey128

static dyntexture_t * cl_finddyntexture( const char *name, qboolean warnonfailure ) {
	unsigned i;
	dyntexture_t *dyntexture = NULL;

	// sanity checks - make sure its actually a dynamic texture path
	if( !name || !*name || strncmp( name, CLDYNTEXTUREPREFIX, sizeof( CLDYNTEXTUREPREFIX ) - 1 ) != 0 ) {
		// TODO: print a warning or something
		if (warnonfailure)
			Con_Printf( "cl_finddyntexture: Bad dynamic texture name '%s'\n", name );
		return NULL;
	}

	for( i = 0 ; i < dyntexturecount ; i++ ) {
		dyntexture = &dyntextures[ i ];
		if( dyntexture->name && strcmp( dyntexture->name, name ) == 0 ) {
			return dyntexture;
		}
	}

	if( dyntexturecount == MAX_DYNAMIC_TEXTURE_COUNT ) {
		// TODO: warn or expand the array, etc.
		return NULL;
	}
	dyntexture = &dyntextures[ dyntexturecount++ ];
	strlcpy( dyntexture->name, name, sizeof( dyntexture->name ) );
	dyntexture->texture = DEFAULT_DYNTEXTURE;
	return dyntexture;
}

rtexture_t * CL_GetDynTexture( const char *name ) {
	dyntexture_t *dyntexture = cl_finddyntexture( name, false );
	if( dyntexture ) {
		return dyntexture->texture;
	} else {
		return NULL;
	}
}

void CL_LinkDynTexture( const char *name, rtexture_t *texture ) {
	dyntexture_t *dyntexture;
	cachepic_t *cachepic;
	skinframe_t *skinframe;

	dyntexture = cl_finddyntexture( name, true );
	if( !dyntexture ) {
		Con_Printf( "CL_LinkDynTexture: internal error in cl_finddyntexture!\n" );
		return;
	}
	// TODO: assert dyntexture != NULL!
	if( dyntexture->texture != texture ) {
		dyntexture->texture = texture;

		cachepic = Draw_CachePic_Flags( name, CACHEPICFLAG_NOTPERSISTENT );
		// TODO: assert cachepic and skinframe should be valid pointers...
		// TODO: assert cachepic->tex = dyntexture->texture
		cachepic->tex = texture;
		// update cachepic's size, too
		cachepic->width = R_TextureWidth( texture );
		cachepic->height = R_TextureHeight( texture );

		// update skinframes
		skinframe = NULL;
		while( (skinframe = R_SkinFrame_FindNextByName( skinframe, name )) != NULL ) {
			skinframe->base = texture;
			// simply reset the compare* attributes of skinframe
			skinframe->comparecrc = 0;
			skinframe->comparewidth = skinframe->compareheight = 0;
			// this is kind of hacky
		}
	}
}

void CL_UnlinkDynTexture( const char *name ) {
	CL_LinkDynTexture( name, DEFAULT_DYNTEXTURE );
}

