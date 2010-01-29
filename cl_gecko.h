// Andreas Kirsch 07

#ifndef CL_GECKO_H
#define CL_GECKO_H

#include "cl_dyntexture.h"

#define CLGECKOPREFIX			CLDYNTEXTUREPREFIX "gecko/"

typedef enum clgecko_buttoneventtype_e {
	CLG_BET_DOWN,
	CLG_BET_UP,
	CLG_BET_DOUBLECLICK,
	// use for up + down (but dont use both)
	CLG_BET_PRESS
} clgecko_buttoneventtype_t;

typedef struct clgecko_s clgecko_t;

void CL_Gecko_Frame( void );
void CL_Gecko_Init( void );
void CL_Gecko_Shutdown( void );

clgecko_t * CL_Gecko_CreateBrowser( const char *name, int ownerProg );
clgecko_t * CL_Gecko_FindBrowser( const char *name );
void CL_Gecko_DestroyBrowser( clgecko_t *instance );

void CL_Gecko_NavigateToURI( clgecko_t *instance, const char *URI );
// x and y between 0.0 and 1.0 (0.0 is top-left?)
void CL_Gecko_Event_CursorMove( clgecko_t *instance, float x, float y );

// returns whether the key/button event was handled or not
qboolean CL_Gecko_Event_Key( clgecko_t *instance, keynum_t key, clgecko_buttoneventtype_t eventtype );

void CL_Gecko_Resize( clgecko_t *instance, int width, int height );
// get the ratio between gecko instance's size in the texture and the actual texture size..
void CL_Gecko_GetTextureExtent( clgecko_t *instance, float* pwidth, float* pheight );
#endif

