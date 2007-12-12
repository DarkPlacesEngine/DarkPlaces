#ifdef SUPPORT_GECKO

// includes everything!
#include <OffscreenGecko/browser.h>

#ifdef _MSC_VER
#	pragma comment( lib, "OffscreenGecko" )
#endif

#include "quakedef.h"
#include "cl_dyntexture.h"
#include "cl_gecko.h"
#include "timing.h"

#define DEFAULT_SIZE	  512

static rtexturepool_t *cl_geckotexturepool;
static OSGK_Embedding *cl_geckoembedding;

struct clgecko_s {
	qboolean active;
	char name[ MAX_QPATH + 32 ];

	OSGK_Browser *browser;
	int w, h;
	int texW, texH;
	
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
		TIMING_TIMESTATEMENT(data = osgk_browser_lock_data( instance->browser, NULL ));
		R_UpdateTexture( texture, data, 0, 0, instance->w, instance->h);
		osgk_browser_unlock_data( instance->browser, data );
	}
}

static void cl_gecko_linktexture( clgecko_t *instance ) {
	// TODO: assert that instance->texture == NULL
	instance->texture = R_LoadTexture2D( cl_geckotexturepool, instance->name, 
		instance->texW, instance->texH, NULL, TEXTYPE_BGRA, TEXF_ALPHA | TEXF_PERSISTENT, NULL );
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
	
	if( cl_geckoembedding == NULL ) {
		OSGK_EmbeddingOptions *options = osgk_embedding_options_create();
		osgk_embedding_options_add_search_path( options, "./xulrunner/" );
		cl_geckoembedding = osgk_embedding_create_with_options( options, NULL );
		osgk_release( options );
        	
		if( cl_geckoembedding == NULL ) {
			Con_Printf( "CL_Gecko_CreateBrowser: Couldn't retrieve gecko embedding object!\n" );
			return NULL;
		}
	}

	instance->active = true;
	strlcpy( instance->name, name, sizeof( instance->name ) );
	instance->browser = osgk_browser_create( cl_geckoembedding, DEFAULT_SIZE, DEFAULT_SIZE );
	// TODO: assert != NULL

	instance->texW = DEFAULT_SIZE;
	instance->texH = DEFAULT_SIZE;
	instance->w = DEFAULT_SIZE;
	instance->h = DEFAULT_SIZE;
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

	if (cl_geckoembedding != NULL)
	{
	    osgk_release( cl_geckoembedding );
	    cl_geckoembedding = NULL;
	}
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
		Con_Print("usage: gecko_destroy <name>\ndestroys a browser\n");
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
		Con_Print("usage: gecko_navigate <name> <URI>\nnavigates to a certain URI\n");
		return;
	}

	// TODO: use snprintf instead
	sprintf(name, CLGECKOPREFIX "%s", Cmd_Argv(1));
	URI = Cmd_Argv( 2 );
	CL_Gecko_NavigateToURI( CL_Gecko_FindBrowser( name ), URI );
}

static void cl_gecko_injecttext_f( void ) {
	char name[MAX_QPATH];
	const char *text;
	clgecko_t *instance;
	const char *p;

	if (Cmd_Argc() < 3)
	{
		Con_Print("usage: gecko_injecttext <name> <text>\ninjects a certain text into the browser\n");
		return;
	}

	// TODO: use snprintf instead
	sprintf(name, CLGECKOPREFIX "%s", Cmd_Argv(1));
	instance = CL_Gecko_FindBrowser( name );
	if( !instance ) {
		Con_Printf( "cl_gecko_injecttext_f: gecko instance '%s' couldn't be found!\n", name );
		return;
	}

	text = Cmd_Argv( 2 );

	for( p = text ; *p ; p++ ) {
		unsigned key = *p;
		switch( key ) {
			case ' ':
				key = K_SPACE;
				break;
			case '\\':
				key = *++p;
				switch( key ) {
				case 'n':
					key = K_ENTER;
					break;
				case '\0':
					--p;
					key = '\\';
					break;
				}
				break;
		}

		CL_Gecko_Event_Key( instance, key, CLG_BET_PRESS );
	}
}

void CL_Gecko_Init( void )
{
	Cmd_AddCommand( "gecko_create", cl_gecko_create_f, "Create a gecko browser instance" );
	Cmd_AddCommand( "gecko_destroy", cl_gecko_destroy_f, "Destroy a gecko browser instance" );
	Cmd_AddCommand( "gecko_navigate", cl_gecko_navigate_f, "Navigate a gecko browser to a URI" );
	Cmd_AddCommand( "gecko_injecttext", cl_gecko_injecttext_f, "Injects text into a browser" );

	R_RegisterModule( "CL_Gecko", cl_gecko_start, cl_gecko_shutdown, cl_gecko_newmap );
}

void CL_Gecko_NavigateToURI( clgecko_t *instance, const char *URI ) {
	if( instance && instance->active ) {
		osgk_browser_navigate( instance->browser, URI );
	}
}

void CL_Gecko_Event_CursorMove( clgecko_t *instance, float x, float y ) {
	// TODO: assert x, y \in [0.0, 1.0]
	int mappedx, mappedy;

	if( !instance || !instance->browser ) {
		return;
	}

	mappedx = x * instance->w;
	mappedy = y * instance->h;
	osgk_browser_event_mouse_move( instance->browser, mappedx, mappedy );
}

typedef struct geckokeymapping_s {
	keynum_t keycode;
	unsigned int geckokeycode;
} geckokeymapping_t;

static geckokeymapping_t geckokeymappingtable[] = {
	{ K_BACKSPACE, OSGKKey_Backspace },
	{ K_TAB, OSGKKey_Tab },
	{ K_ENTER, OSGKKey_Return },
	{ K_SHIFT, OSGKKey_Shift },
	{ K_CTRL, OSGKKey_Control },
	{ K_ALT, OSGKKey_Alt },
	{ K_CAPSLOCK, OSGKKey_CapsLock },
	{ K_ESCAPE, OSGKKey_Escape },
	{ K_SPACE, OSGKKey_Space },
	{ K_PGUP, OSGKKey_PageUp },
	{ K_PGDN, OSGKKey_PageDown },
	{ K_END, OSGKKey_End },
	{ K_HOME, OSGKKey_Home },
	{ K_LEFTARROW, OSGKKey_Left },
	{ K_UPARROW, OSGKKey_Up },
	{ K_RIGHTARROW, OSGKKey_Right },
	{ K_DOWNARROW, OSGKKey_Down },
	{ K_INS, OSGKKey_Insert },
	{ K_DEL, OSGKKey_Delete },
	{ K_F1, OSGKKey_F1 },
	{ K_F2, OSGKKey_F2 },
	{ K_F3, OSGKKey_F3 },
	{ K_F4, OSGKKey_F4 },
	{ K_F5, OSGKKey_F5 },
	{ K_F6, OSGKKey_F6 },
	{ K_F7, OSGKKey_F7 },
	{ K_F8, OSGKKey_F8 },
	{ K_F9, OSGKKey_F9 },
	{ K_F10, OSGKKey_F10 },
	{ K_F11, OSGKKey_F11 },
	{ K_F12, OSGKKey_F12 },
	{ K_NUMLOCK, OSGKKey_NumLock },
	{ K_SCROLLOCK, OSGKKey_ScrollLock }
};

qboolean CL_Gecko_Event_Key( clgecko_t *instance, int key, clgecko_buttoneventtype_t eventtype ) {
	if( !instance || !instance->browser ) {
		return false;
	}

	// determine whether its a keyboard event
	if( key < K_OTHERDEVICESBEGIN ) {

		OSGK_KeyboardEventType mappedtype;
		unsigned int mappedkey = key;
		
		int i;
		// yes! then convert it if necessary!
		for( i = 0 ; i < sizeof( geckokeymappingtable ) / sizeof( *geckokeymappingtable ) ; i++ ) {
			const geckokeymapping_t * const mapping = &geckokeymappingtable[ i ];
			if( key == mapping->keycode ) {
				mappedkey = mapping->geckokeycode;
				break;
			}
		}

		// convert the eventtype
		// map the type
		switch( eventtype ) {
		case CLG_BET_DOWN:
			mappedtype = keDown;
			break;
		case CLG_BET_UP:
			mappedtype = keUp;
			break;
		case CLG_BET_DOUBLECLICK:
			// TODO: error message
			break;
		case CLG_BET_PRESS:
			mappedtype = kePress;
		}

		return osgk_browser_event_key( instance->browser, mappedkey, mappedtype ) != 0;
	} else if( K_MOUSE1 <= key && key <= K_MOUSE3 ) {
		OSGK_MouseButtonEventType mappedtype;
		OSGK_MouseButton mappedbutton;

		mappedbutton = (OSGK_MouseButton) (mbLeft + (key - K_MOUSE1));

		switch( eventtype ) {
		case CLG_BET_DOWN:
			mappedtype = meDown;
			break;
		case CLG_BET_UP:
			mappedtype = meUp;
			break;
		case CLG_BET_DOUBLECLICK:
			mappedtype = meDoubleClick;
			break;
		case CLG_BET_PRESS:
			// hihi, hacky hacky
			osgk_browser_event_mouse_button( instance->browser, mappedbutton, meDown );
			mappedtype = meUp;
			break;
		}

		osgk_browser_event_mouse_button( instance->browser, mappedbutton, mappedtype );
		return true;
	}
	// TODO: error?
	return false;
}

void CL_Gecko_Resize( clgecko_t *instance, int w, int h ) {
	int newW, newH;

	if( !instance || !instance->browser ) {
		return;
	}

	newW = 1; while( newW < w ) newW *= 2;
	newH = 1; while( newH < h ) newH *= 2;
	if ((newW != instance->texW) || (newH != instance->texH))
	{
	  cl_gecko_unlinktexture( instance );
	  instance->texW = newW;
	  instance->texH = newH;
	  cl_gecko_linktexture( instance );
	}
	else
	{
	  /* The gecko area will only cover a part of the texture; to avoid
	     'old' pixels bleeding in at the border clear the texture. */
	  R_ClearTexture( instance->texture );
	}

	osgk_browser_resize( instance->browser, w, h);
	instance->w = w;
	instance->h = h;
}

void CL_Gecko_GetTextureExtent( clgecko_t *instance, float* u, float* v )
{
	if( !instance || !instance->browser ) {
		return;
	}

	*u = (float)instance->w / instance->texW;
	*v = (float)instance->h / instance->texH;
}

#endif
