#ifdef SUPPORT_GECKO

// includes everything!
#include <OffscreenGecko/browser.h>

#ifdef _MSC_VER
#	pragma comment( lib, "OffscreenGecko" )
#endif

#include "quakedef.h"
#include "cl_dyntexture.h"
#include "cl_gecko.h"

static rtexturepool_t *cl_geckotexturepool;
static OSGK_Embedding *cl_geckoembedding;

struct clgecko_s {
	qboolean active;
	char name[ MAX_QPATH + 32 ];

	OSGK_Browser *browser;
	
	rtexture_t *texture;
};

static clgecko_t cl_geckoinstances[ MAX_GECKO_INSTANCES ];

static clgecko_t * cl_gecko_findunusedinstance( void ) {
	int i;
	for( i = 0 ; i < MAX_GECKO_INSTANCES ; i++ ) {
		clgecko_t *instance = &cl_geckoinstances[ i ];
		if( !instance->active ) {
			return instance;
		}
	}
	return NULL;
}

clgecko_t * CL_Gecko_FindBrowser( const char *name ) {
	int i;

	if( !name || !*name || strncmp( name, CLGECKOPREFIX, sizeof( CLGECKOPREFIX ) - 1 ) != 0 ) {
		if( developer.integer > 0 ) {
			Con_Printf( "CL_Gecko_FindBrowser: Bad gecko texture name '%s'!\n", name );
		}
		return NULL;
	}

	for( i = 0 ; i < MAX_GECKO_INSTANCES ; i++ ) {
		clgecko_t *instance = &cl_geckoinstances[ i ];
		if( instance->active && strcmp( instance->name, name ) == 0 ) {
			return instance;
		}
	}

	return NULL;
}

static void cl_gecko_updatecallback( rtexture_t *texture, clgecko_t *instance ) {
	const unsigned char *data;
	if( instance->browser ) {
		// TODO: OSGK only supports BGRA right now
		data = osgk_browser_lock_data( instance->browser, NULL );
		R_UpdateTexture( texture, data, 0, 0, DEFAULT_GECKO_WIDTH, DEFAULT_GECKO_HEIGHT );
		osgk_browser_unlock_data( instance->browser, data );
	}
}

static void cl_gecko_linktexture( clgecko_t *instance ) {
	// TODO: assert that instance->texture == NULL
	instance->texture = R_LoadTexture2D( cl_geckotexturepool, instance->name, DEFAULT_GECKO_WIDTH, DEFAULT_GECKO_HEIGHT, NULL, TEXTYPE_BGRA, TEXF_ALPHA, NULL );
	R_MakeTextureDynamic( instance->texture, cl_gecko_updatecallback, instance );
	CL_LinkDynTexture( instance->name, instance->texture );
}

static void cl_gecko_unlinktexture( clgecko_t *instance ) {
	if( instance->texture ) {
		CL_UnlinkDynTexture( instance->name );
		R_FreeTexture( instance->texture );
		instance->texture = NULL;
	}
}

clgecko_t * CL_Gecko_CreateBrowser( const char *name ) {
	// TODO: verify that we dont use a name twice
	clgecko_t *instance = cl_gecko_findunusedinstance();
	// TODO: assert != NULL
	
	instance->active = true;
	strlcpy( instance->name, name, sizeof( instance->name ) );
	instance->browser = osgk_browser_create( cl_geckoembedding, DEFAULT_GECKO_WIDTH, DEFAULT_GECKO_HEIGHT );
	// TODO: assert != NULL

	cl_gecko_linktexture( instance );

	return instance;
}

void CL_Gecko_DestroyBrowser( clgecko_t *instance ) {
   if( !instance || !instance->active ) {
		return;
	}

	instance->active = false;
	cl_gecko_unlinktexture( instance );

	osgk_release( instance->browser );
	instance->browser = NULL;
}

void CL_Gecko_Frame( void ) {
	int i;
	for( i = 0 ; i < MAX_GECKO_INSTANCES ; i++ ) {
		clgecko_t *instance = &cl_geckoinstances[ i ];
		if( instance->active ) {
			if( instance->browser && osgk_browser_query_dirty( instance->browser ) == 1 ) {
				R_MarkDirtyTexture( instance->texture );
			}
		}
	}
}

static void cl_gecko_start( void )
{
	int i;
	cl_geckotexturepool = R_AllocTexturePool();

	// recreate textures on module start
	for( i = 0 ; i < MAX_GECKO_INSTANCES ; i++ ) {
		clgecko_t *instance = &cl_geckoinstances[ i ];
		if( instance->active ) {
			cl_gecko_linktexture( instance );
		}
	}
}

static void cl_gecko_shutdown( void )
{
	int i;
	for( i = 0 ; i < MAX_GECKO_INSTANCES ; i++ ) {
		clgecko_t *instance = &cl_geckoinstances[ i ];
		if( instance->active ) {
			cl_gecko_unlinktexture( instance );
		}
	}
	R_FreeTexturePool( &cl_geckotexturepool );
}

static void cl_gecko_newmap( void )
{
	// DO NOTHING
}

void CL_Gecko_Shutdown( void ) {
	int i;
	for( i = 0 ; i < MAX_GECKO_INSTANCES ; i++ ) {
		clgecko_t *instance = &cl_geckoinstances[ i ];
		if( instance->active ) {
			cl_gecko_unlinktexture( instance );
		}		
	}
	osgk_release( cl_geckoembedding );
}

static void cl_gecko_create_f( void ) {
	char name[MAX_QPATH];

	if (Cmd_Argc() != 2)
	{
		Con_Print("usage: gecko_create <name>\npcreates a browser (full texture path " CLGECKOPREFIX "<name>)\n");
		return;
	}

	// TODO: use snprintf instead
	sprintf(name, CLGECKOPREFIX "%s", Cmd_Argv(1));
	CL_Gecko_CreateBrowser( name );
}

static void cl_gecko_destroy_f( void ) {
	char name[MAX_QPATH];

	if (Cmd_Argc() != 2)
	{
		Con_Print("usage: gecko_destroy <name>\ndestroys a browser (full texture path " CLGECKOPREFIX "<name>)\n");
		return;
	}

	// TODO: use snprintf instead
	sprintf(name, CLGECKOPREFIX "%s", Cmd_Argv(1));
	CL_Gecko_DestroyBrowser( CL_Gecko_FindBrowser( name ) );
}

static void cl_gecko_navigate_f( void ) {
	char name[MAX_QPATH];
	const char *URI;

	if (Cmd_Argc() != 3)
	{
		Con_Print("usage: gecko_navigate <name> <URI>\nnavigates to a certain URI (full texture path " CLGECKOPREFIX "<name>)\n");
		return;
	}

	// TODO: use snprintf instead
	sprintf(name, CLGECKOPREFIX "%s", Cmd_Argv(1));
	URI = Cmd_Argv( 2 );
	CL_Gecko_NavigateToURI( CL_Gecko_FindBrowser( name ), URI );
}

void CL_Gecko_Init( void )
{
	OSGK_EmbeddingOptions *options = osgk_embedding_options_create();
	osgk_embedding_options_add_search_path( options, "./xulrunner/" );
   cl_geckoembedding = osgk_embedding_create_with_options( options, NULL );
	osgk_release( options );
	
	if( cl_geckoembedding == NULL ) {
		Con_Printf( "CL_Gecko_Init: Couldn't retrieve gecko embedding object!\n" );
	}
	
	Cmd_AddCommand( "gecko_create", cl_gecko_create_f, "Create a gecko browser instance" );
	Cmd_AddCommand( "gecko_destroy", cl_gecko_destroy_f, "Destroy a gecko browser instance" );
	Cmd_AddCommand( "gecko_navigate", cl_gecko_navigate_f, "Navigate a gecko browser to an URI" );

	R_RegisterModule( "CL_Gecko", cl_gecko_start, cl_gecko_shutdown, cl_gecko_newmap );
}

void CL_Gecko_NavigateToURI( clgecko_t *instance, const char *URI ) {
	if( instance && instance->active ) {
		osgk_browser_navigate( instance->browser, URI );
	}
}

// TODO: write this function
void CL_Gecko_Event_CursorMove( clgecko_t *instance, float x, float y );
qboolean CL_Gecko_Event_Key( clgecko_t *instance, int key, clgecko_buttoneventtype_t eventtype );

#endif