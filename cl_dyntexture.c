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

static dyntexture_t * _CL_FindDynTexture( const char *name ) {
	unsigned i;
	dyntexture_t *dyntexture = NULL;
	// some sanity checks - and make sure its actually a dynamic texture path
	if( !name || strncmp( name, DYNAMIC_TEXTURE_PATH_PREFIX, sizeof( DYNAMIC_TEXTURE_PATH_PREFIX ) - 1 ) != 0 ) {
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
	dyntexture_t *dyntexture = _CL_FindDynTexture( name );
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

	dyntexture = _CL_FindDynTexture( name );
	// TODO: assert dyntexture != NULL!
	if( dyntexture->texture != texture ) {
		cachepic = Draw_CachePic( name, false );
		skinframe = R_SkinFrame_Find( name, 0, 0, 0, 0, false );
		// this is kind of hacky
		// TODO: assert cachepic and skinframe should be valid pointers...

		// TODO: assert cachepic->tex = dyntexture->texture
		cachepic->tex = texture;
		// update cachepic's size, too
		cachepic->width = R_TextureWidth( texture );
		cachepic->height = R_TextureHeight( texture );
		// TODO: assert skinframe->base = dyntexture->texture
		skinframe->base = texture;
		// simply reset the compare* attributes of skinframe
		skinframe->comparecrc = 0;
		skinframe->comparewidth = skinframe->compareheight = 0;

		dyntexture->texture = texture;
	}
}

void CL_UnlinkDynTexture( const char *name ) {
	CL_LinkDynTexture( name, DEFAULT_DYNTEXTURE );
}

