// Andreas Kirsch 07

#ifdef SUPPORT_GECKO

#ifndef CL_GECKO_H
#define CL_GECKO_H

#define DEFAULT_GECKO_WIDTH	512
#define DEFAULT_GECKO_HEIGHT	DEFAULT_GECKO_WIDTH

#define CLGECKOPREFIX			CLDYNTEXTUREPREFIX "gecko/"
#define MAX_GECKO_INSTANCES	16

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

clgecko_t * CL_Gecko_CreateBrowser( const char *name );
clgecko_t * CL_Gecko_FindBrowser( const char *name );
void CL_Gecko_DestroyBrowser( clgecko_t *instance );

void CL_Gecko_NavigateToURI( clgecko_t *instance, const char *URI );
void CL_Gecko_Event_CursorMove( clgecko_t *instance, float x, float y );

// returns whether the key/button event was handled or not
qboolean CL_Gecko_Event_Key( clgecko_t *instance, int key, clgecko_buttoneventtype_t eventtype );

#endif

#endif

