/* --- 8< --- 8< ---   OffscreenGecko headers   --- >8 --- >8 --- */
/* NOTE: Documentation comments have been omitted. Documentation is
   available in the OffscreenGecko SDK download. */

/* OffscreenGecko/defs.h */

#define OSGK_CLASSTYPE_DEF	struct
#define OSGK_CLASSTYPE_REF	struct

#include <assert.h>
#include <stdlib.h>
#define OSGK_ASSERT(x)	assert(x)

typedef unsigned int OSGK_GeckoResult;

#if defined(__cplusplus) || defined(__GNUC__)
#  define OSGK_INLINE	inline
#elif defined(_MSC_VER)
#  define OSGK_INLINE	__inline
#else
#  define OSGK_INLINE
#endif

/* OffscreenGecko/baseobj.h */

struct OSGK_BaseObject_s
{
  int reserved;
};
typedef struct OSGK_BaseObject_s OSGK_BaseObject;

#define OSGK_DERIVEDTYPE(T)           \
  typedef struct T ## _s {            \
    OSGK_BaseObject baseobj;          \
  } T

static int (*osgk_addref) (OSGK_BaseObject* obj);
static int (*osgk_release) (OSGK_BaseObject* obj);

/*static OSGK_INLINE int osgk_addref_real (OSGK_BaseObject* obj)
{
  return osgk_addref (obj);
}*/

static OSGK_INLINE int osgk_release_real (OSGK_BaseObject* obj)
{
  return osgk_release (obj);
}

#define osgk_addref(obj)    osgk_addref_real (&((obj)->baseobj))
#define osgk_release(obj)   osgk_release_real (&((obj)->baseobj))

/* OffscreenGecko/embedding.h */

OSGK_DERIVEDTYPE(OSGK_EmbeddingOptions);

static OSGK_EmbeddingOptions* (*osgk_embedding_options_create) (void);
static void (*osgk_embedding_options_add_search_path) (
  OSGK_EmbeddingOptions* options, const char* path);
/*static void (*osgk_embedding_options_add_components_path) (
  OSGK_EmbeddingOptions* options, const char* path);*/
static void (*osgk_embedding_options_set_profile_dir) (
  OSGK_EmbeddingOptions* options, const char* profileDir,
  const char* localProfileDir);

OSGK_DERIVEDTYPE(OSGK_Embedding);

#define OSGK_API_VERSION    1

static OSGK_Embedding* (*osgk_embedding_create2) (
  unsigned int apiVer, OSGK_EmbeddingOptions* options, 
  OSGK_GeckoResult* geckoResult);

/*static OSGK_INLINE OSGK_Embedding* osgk_embedding_create (
  OSGK_GeckoResult* geckoResult)
{
  return osgk_embedding_create2 (OSGK_API_VERSION, 0, geckoResult);
}*/

static OSGK_INLINE OSGK_Embedding* osgk_embedding_create_with_options (
  OSGK_EmbeddingOptions* options, OSGK_GeckoResult* geckoResult)
{
  return osgk_embedding_create2 (OSGK_API_VERSION, options, geckoResult);
}

/*static OSGK_GeckoMem* (*osgk_embedding_get_gecko_mem) (
  OSGK_Embedding* embedding);*/

/*static OSGK_ComponentMgr* (*osgk_embedding_get_component_mgr) (
  OSGK_Embedding* embedding);*/

/*OSGK_CLASSTYPE_DEF nsIComponentManager;
OSGK_CLASSTYPE_DEF nsIComponentRegistrar;
OSGK_CLASSTYPE_DEF nsIServiceManager;*/

/*static OSGK_CLASSTYPE_REF nsIComponentManager* 
(*osgk_embedding_get_gecko_component_manager) (OSGK_Embedding* embedding);*/
/*static OSGK_CLASSTYPE_REF nsIComponentRegistrar* 
(*osgk_embedding_get_gecko_component_registrar) (OSGK_Embedding* embedding);*/
/*static OSGK_CLASSTYPE_REF nsIServiceManager* 
(*osgk_embedding_get_gecko_service_manager) (OSGK_Embedding* embedding);*/

enum
{
  jsgPrivileged = 1
};
/*static int (*osgk_embedding_register_js_global) (
  OSGK_Embedding* embedding, const char* name, const char* contractID,
  unsigned int flags, OSGK_String** previousContract,
  OSGK_GeckoResult* geckoResult);*/

/*static void (*osgk_embedding_clear_focus*) (OSGK_Embedding* embedding);*/
/*void (*osgk_embedding_set_auto_focus) (OSGK_Embedding* embedding, int autoFocus);*/
/*static int (*osgk_embedding_get_auto_focus) (OSGK_Embedding* embedding);*/

/* OffscreenGecko/browser.h */
OSGK_DERIVEDTYPE(OSGK_Browser);

static OSGK_Browser* (*osgk_browser_create) (
  OSGK_Embedding* embedding, int width, int height);
static void (*osgk_browser_navigate) (OSGK_Browser* browser,
  const char* uri);

static int (*osgk_browser_query_dirty) (OSGK_Browser* browser);
static const unsigned char* (*osgk_browser_lock_data) (
  OSGK_Browser* browser, int* isDirty);
static void (*osgk_browser_unlock_data) (OSGK_Browser* browser,
  const unsigned char* data);

typedef enum OSGK_MouseButton
{
  mbLeft, 
  mbRight, 
  mbMiddle
} OSGK_MouseButton;

typedef enum OSGK_MouseButtonEventType
{
  meDown,
  meUp,
  meDoubleClick
} OSGK_MouseButtonEventType;

static void (*osgk_browser_event_mouse_move) (
  OSGK_Browser* browser, int x, int y);
static void (*osgk_browser_event_mouse_button) (
  OSGK_Browser* browser, OSGK_MouseButton button, 
  OSGK_MouseButtonEventType eventType);

typedef enum OSGK_WheelAxis
{
  waVertical,
  waHorizontal
} OSGK_WheelAxis;

typedef enum OSGK_WheelDirection
{
  wdPositive,
  wdNegative,
  wdPositivePage,
  wdNegativePage
} OSGK_WheelDirection;

static void (*osgk_browser_event_mouse_wheel) (
  OSGK_Browser* browser, OSGK_WheelAxis axis, 
  OSGK_WheelDirection direction);

typedef enum OSGK_KeyboardEventType
{
  keDown,
  keUp,
  kePress
} OSGK_KeyboardEventType;

enum
{
  OSGKKey_First = 0x110000,

  OSGKKey_Backspace = OSGKKey_First,
  OSGKKey_Tab,
  OSGKKey_Return,
  OSGKKey_Shift,
  OSGKKey_Control,
  OSGKKey_Alt,
  OSGKKey_CapsLock,
  OSGKKey_Escape,
  OSGKKey_Space,
  OSGKKey_PageUp,
  OSGKKey_PageDown,
  OSGKKey_End,
  OSGKKey_Home,
  OSGKKey_Left,
  OSGKKey_Up,
  OSGKKey_Right,
  OSGKKey_Down,
  OSGKKey_Insert,
  OSGKKey_Delete,
  OSGKKey_F1,
  OSGKKey_F2,
  OSGKKey_F3,
  OSGKKey_F4,
  OSGKKey_F5,
  OSGKKey_F6,
  OSGKKey_F7,
  OSGKKey_F8,
  OSGKKey_F9,
  OSGKKey_F10,
  OSGKKey_F11,
  OSGKKey_F12,
  OSGKKey_NumLock,
  OSGKKey_ScrollLock,
  OSGKKey_Meta
};

static int (*osgk_browser_event_key) (
  OSGK_Browser* browser, unsigned int key,
  OSGK_KeyboardEventType eventType);

typedef enum OSGK_AntiAliasType
{
  aaNone,
  aaGray,
  aaSubpixel
} OSGK_AntiAliasType;

/*static void (*osgk_browser_set_antialias) (
  OSGK_Browser* browser, OSGK_AntiAliasType aaType);*/
/*static OSGK_AntiAliasType (*osgk_browser_get_antialias) (OSGK_Browser* browser);*/

/*static void (*osgk_browser_focus) (OSGK_Browser* browser);*/

static void (*osgk_browser_resize) (OSGK_Browser* browser,
  int width, int height);

static int (*osgk_browser_set_user_data) (OSGK_Browser* browser,
  unsigned int key, void* data, int overrideData);
static int (*osgk_browser_get_user_data) (OSGK_Browser* browser,
  unsigned int key, void** data);

/* OffscreenGecko/string.h */
OSGK_DERIVEDTYPE(OSGK_String);

static const char* (*osgk_string_get) (OSGK_String* str);

static OSGK_String* (*osgk_string_create) (const char* str);

/* OffscreenGecko/scriptvariant.h */

OSGK_CLASSTYPE_DEF nsISupports;

OSGK_DERIVEDTYPE(OSGK_ScriptVariant);

typedef enum OSGK_ScriptVariantType
{
  svtEmpty,
  svtInt,
  svtUint,
  svtFloat,
  svtDouble,
  svtBool,
  svtChar,
  svtString,
  svtISupports,
  svtScriptObject,
  svtArray
} OSGK_ScriptVariantType;

/*static OSGK_ScriptVariantType (*osgk_variant_get_type) (
  OSGK_ScriptVariant* variant);*/
static OSGK_ScriptVariant* (*osgk_variant_convert) (
  OSGK_ScriptVariant* variant, OSGK_ScriptVariantType newType);

/*static int (*osgk_variant_get_int) (OSGK_ScriptVariant* variant,
  int* val);*/
/*static int (*osgk_variant_get_uint) (OSGK_ScriptVariant* variant,
  unsigned int* val);*/
/*static int (*osgk_variant_get_float) (OSGK_ScriptVariant* variant,
  float* val);*/
/*static int (*osgk_variant_get_double) (OSGK_ScriptVariant* variant,
  double* val);*/
/*static int (*osgk_variant_get_bool) (OSGK_ScriptVariant* variant,
  int* val);*/
/*static int (*osgk_variant_get_char) (OSGK_ScriptVariant* variant,
  unsigned int* val);*/
// Does not increase ref count
static int (*osgk_variant_get_string) (OSGK_ScriptVariant* variant,
  OSGK_String** val);
/*// Does not increase ref count
static int (*osgk_variant_get_isupports) (OSGK_ScriptVariant* variant,
  OSGK_CLASSTYPE_REF nsISupports** val);*/
/*static int (*osgk_variant_get_script_object) (OSGK_ScriptVariant* variant,
  void** tag);*/
/*static int (*osgk_variant_get_array_size) (OSGK_ScriptVariant* variant,
  size_t* size);*/
/*static int (*osgk_variant_get_array_item) (OSGK_ScriptVariant* variant,
  OSGK_ScriptVariant** val);*/

/*static OSGK_ScriptVariant* (*osgk_variant_create_empty) (
  OSGK_Embedding* embedding);*/
/*static OSGK_ScriptVariant* (*osgk_variant_create_int) (
  OSGK_Embedding* embedding, int val);*/
/*static OSGK_ScriptVariant* (*osgk_variant_create_uint) (
  OSGK_Embedding* embedding, unsigned int val);*/
/*static OSGK_ScriptVariant* (*osgk_variant_create_float) (
  OSGK_Embedding* embedding, float val);*/
/*static OSGK_ScriptVariant* (*osgk_variant_create_double) (
  OSGK_Embedding* embedding, double val);*/
/*OSGK_ScriptVariant* (*osgk_variant_create_bool) (
  OSGK_Embedding* embedding, int val);*/
/*static OSGK_ScriptVariant* (*osgk_variant_create_char) (
  OSGK_Embedding* embedding, unsigned int val);*/
static OSGK_ScriptVariant* (*osgk_variant_create_string) (
  OSGK_Embedding* embedding, OSGK_String* val);
/*static OSGK_ScriptVariant* (*osgk_variant_create_isupports) (
  OSGK_Embedding* embedding, OSGK_CLASSTYPE_REF nsISupports* val);*/
/*static OSGK_ScriptVariant* (*osgk_variant_create_script_object) (
  OSGK_Embedding* embedding, void* tag);*/
/*static OSGK_ScriptVariant* (*osgk_variant_create_array) (
  OSGK_Embedding* embedding, size_t numItems, OSGK_ScriptVariant** items);*/

/* OffscreenGecko/scriptobjecttemplate.h */

OSGK_DERIVEDTYPE(OSGK_ScriptObjectTemplate);

typedef enum OSGK_ScriptResult
{
  srSuccess = 0,
  srFailed = (int)0x80004005L /* actually NS_ERROR_FAILURE */
} OSGK_ScriptResult;

typedef struct OSGK_ScriptObjectCreateParams_s
{
  void* createParam;
  OSGK_Browser* browser;
} OSGK_ScriptObjectCreateParams;

typedef OSGK_ScriptResult (*OSGK_CreateObjFunc) (
  OSGK_ScriptObjectCreateParams* params, void** objTag);
typedef void (*OSGK_DestroyObjFunc) (void* objTag);

static OSGK_ScriptObjectTemplate* (*osgk_sot_create) (
  OSGK_Embedding* embedding,
  OSGK_CreateObjFunc createFunc, OSGK_DestroyObjFunc destroyFunc,
  void* createParam);

typedef OSGK_ScriptVariant* (*OSGK_GetPropertyFunc) (void* objTag,
  void* propTag);
typedef OSGK_ScriptResult (*OSGK_SetPropertyFunc) (void* objTag,
  void* propTag, OSGK_ScriptVariant* val);
  
/*static int (*osgk_sot_add_property) (
  OSGK_ScriptObjectTemplate* templ, const char* propName, void* propTag,
  OSGK_GetPropertyFunc getter, OSGK_SetPropertyFunc setter);*/

typedef OSGK_ScriptResult (*OSGK_FunctionCallFunc) (void* objTag,
  void* methTag, size_t numParams, OSGK_ScriptVariant** params,
  OSGK_ScriptVariant** returnVal);

static int (*osgk_sot_add_function) (
  OSGK_ScriptObjectTemplate* templ, const char* funcName, void* funcTag,
  OSGK_FunctionCallFunc callFunc);

/*static OSGK_ScriptVariant* (*osgk_sot_instantiate) (
  OSGK_ScriptObjectTemplate* templ, void** objTag);    */

static int (*osgk_sot_register) (
  OSGK_ScriptObjectTemplate* templ, OSGK_Embedding* embedding, 
  const char* name, unsigned int flags);

/* --- >8 --- >8 --- End OffscreenGecko headers --- 8< --- 8< --- */

#include "quakedef.h"
#include "cl_dyntexture.h"
#include "cl_gecko.h"
#include "timing.h"

#define DEFAULT_GECKO_SIZE	  512

static rtexturepool_t *cl_geckotexturepool;
static OSGK_Embedding *cl_geckoembedding;

struct clgecko_s {
	qboolean active;
	char name[ MAX_QPATH + 32 ];
	int ownerProg;

	OSGK_Browser *browser;
	int width, height;
	int texWidth, texHeight;
	
	rtexture_t *texture;
};

#define USERDATAKEY_CL_GECKO_T	      0

static dllhandle_t osgk_dll = NULL;

static clgecko_t cl_geckoinstances[ MAX_GECKO_INSTANCES ];

static clgecko_t * cl_gecko_findunusedinstance( void ) {
	int i;
	for( i = 0 ; i < MAX_GECKO_INSTANCES ; i++ ) {
		clgecko_t *instance = &cl_geckoinstances[ i ];
		if( !instance->active ) {
			return instance;
		}
	}
	Con_DPrintf( "cl_gecko_findunusedinstance: out of geckos\n" );
	return NULL;
}

clgecko_t * CL_Gecko_FindBrowser( const char *name ) {
	int i;

	if( !name || !*name || strncmp( name, CLGECKOPREFIX, sizeof( CLGECKOPREFIX ) - 1 ) != 0 ) {
		Con_DPrintf( "CL_Gecko_FindBrowser: Bad gecko texture name '%s'!\n", name );
		return NULL;
	}

	for( i = 0 ; i < MAX_GECKO_INSTANCES ; i++ ) {
		clgecko_t *instance = &cl_geckoinstances[ i ];
		if( instance->active && strcmp( instance->name, name ) == 0 ) {
			return instance;
		}
	}

	Con_DPrintf( "CL_Gecko_FindBrowser: No browser named '%s'!\n", name );

	return NULL;
}

static void cl_gecko_updatecallback( rtexture_t *texture, void* callbackData ) {
	clgecko_t *instance = (clgecko_t *) callbackData;
	const unsigned char *data;
	if( instance->browser ) {
		// TODO: OSGK only supports BGRA right now
		TIMING_TIMESTATEMENT(data = osgk_browser_lock_data( instance->browser, NULL ));
		R_UpdateTexture( texture, data, 0, 0, 0, instance->width, instance->height, 1 );
		osgk_browser_unlock_data( instance->browser, data );
	}
}

static void cl_gecko_linktexture( clgecko_t *instance ) {
	// TODO: assert that instance->texture == NULL
	instance->texture = R_LoadTexture2D( cl_geckotexturepool, instance->name, 
		instance->texWidth, instance->texHeight, NULL, TEXTYPE_BGRA, TEXF_ALPHA | TEXF_PERSISTENT, -1, NULL );
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

void CL_Gecko_Resize( clgecko_t *instance, int width, int height ) {
	int newWidth, newHeight;

	// early out if bad parameters are passed (no resize to a texture size bigger than the original texture size)
	if( !instance || !instance->browser) {
		return;
	}

	newWidth = CeilPowerOf2( width );
	newHeight = CeilPowerOf2( height );
	if ((newWidth != instance->texWidth) || (newHeight != instance->texHeight))
	{
		cl_gecko_unlinktexture( instance );
		instance->texWidth = newWidth;
		instance->texHeight = newHeight;
		cl_gecko_linktexture( instance );
	}
	else
	{
		/* The gecko area will only cover a part of the texture; to avoid
		'old' pixels bleeding in at the border clear the texture. */
		R_ClearTexture( instance->texture );
	}

	osgk_browser_resize( instance->browser, width, height);
	instance->width = width;
	instance->height = height;
}

void CL_Gecko_GetTextureExtent( clgecko_t *instance, float* pwidth, float* pheight )
{
	if( !instance || !instance->browser ) {
		return;
	}

	*pwidth = (float)instance->width / instance->texWidth;
	*pheight = (float)instance->height / instance->texHeight;
}

static OSGK_ScriptResult dpGlobal_create (OSGK_ScriptObjectCreateParams* params, 
					  void** objTag)
{
  if (!osgk_browser_get_user_data (params->browser, USERDATAKEY_CL_GECKO_T, objTag))
    return srFailed;
  return srSuccess;
}

static OSGK_ScriptResult dpGlobal_query (void* objTag, void* methTag, 
					 size_t numParams, 
					 OSGK_ScriptVariant** params,
					 OSGK_ScriptVariant** returnVal)
{
  clgecko_t *instance = (clgecko_t *) objTag;
  OSGK_ScriptVariant* strVal;
  OSGK_ScriptResult result = srFailed;
  prvm_prog_t * saveProg;

  /* Can happen when created from console */
  if (instance->ownerProg < 0) return srFailed;

  /* Require exactly one param, for now */
  if (numParams != 1) return srFailed;

  strVal = osgk_variant_convert (params[0], svtString);
  if (strVal == 0) return srFailed;

  saveProg = prog;
  PRVM_SetProg(instance->ownerProg);

  if (PRVM_clientfunction(Gecko_Query))
  {
    OSGK_String* paramStr, *resultStr;

    if (!osgk_variant_get_string (strVal, &paramStr)) return srFailed;
	 PRVM_G_INT(OFS_PARM0) = PRVM_SetTempString (instance->name);
	 PRVM_G_INT(OFS_PARM1) = PRVM_SetTempString (osgk_string_get (paramStr));
    PRVM_ExecuteProgram(PRVM_clientfunction(Gecko_Query),"Gecko_Query() required");
    resultStr = osgk_string_create (PRVM_G_STRING (OFS_RETURN));
    *returnVal = osgk_variant_create_string (cl_geckoembedding, resultStr);
    osgk_release (resultStr);
    
    result = srSuccess;
  }

  prog = saveProg;
  
  return result;
}

#if defined(_WIN64)
# define XULRUNNER_DIR_SUFFIX	"win64"
#elif defined(WIN32)
# define XULRUNNER_DIR_SUFFIX	"win32"
#elif defined(DP_OS_STR) && defined(DP_ARCH_STR)
# define XULRUNNER_DIR_SUFFIX	DP_OS_STR "-" DP_ARCH_STR
#endif

static qboolean CL_Gecko_Embedding_Init (void)
{
	char profile_path [MAX_OSPATH];
	OSGK_GeckoResult grc;
	OSGK_EmbeddingOptions *options;
	OSGK_ScriptObjectTemplate* dpGlobalTemplate;

	if (!osgk_dll) return false;

	if( cl_geckoembedding != NULL ) return true;

	Con_DPrintf( "CL_Gecko_Embedding_Init: setting up gecko embedding\n" );

	options = osgk_embedding_options_create();
#ifdef XULRUNNER_DIR_SUFFIX
	osgk_embedding_options_add_search_path( options, "./xulrunner-" XULRUNNER_DIR_SUFFIX "/" );
#endif
	osgk_embedding_options_add_search_path( options, "./xulrunner/" );
	dpsnprintf (profile_path, sizeof (profile_path), "%s/xulrunner_profile/", fs_gamedir);
	osgk_embedding_options_set_profile_dir( options, profile_path, 0 );
	cl_geckoembedding = osgk_embedding_create_with_options( options, &grc );
	osgk_release( options );
        	
	if( cl_geckoembedding == NULL ) {
		Con_Printf( "CL_Gecko_Embedding_Init: Couldn't retrieve gecko embedding object (%.8x)!\n", grc );
		return false;
	} 
	
	Con_DPrintf( "CL_Gecko_Embedding_Init: Embedding set up correctly\n" );

	dpGlobalTemplate = osgk_sot_create( cl_geckoembedding, dpGlobal_create, NULL, NULL );

	osgk_sot_add_function (dpGlobalTemplate, "query", 0, dpGlobal_query);

	osgk_sot_register (dpGlobalTemplate, cl_geckoembedding, "Darkplaces", 0);
	osgk_release( dpGlobalTemplate );

	return true;
}

clgecko_t * CL_Gecko_CreateBrowser( const char *name, int ownerProg ) {
	clgecko_t *instance;

	if (!CL_Gecko_Embedding_Init ()) return NULL;

	// TODO: verify that we dont use a name twice
	instance = cl_gecko_findunusedinstance();
	// TODO: assert != NULL

	instance->active = true;
	strlcpy( instance->name, name, sizeof( instance->name ) );
	instance->browser = osgk_browser_create( cl_geckoembedding, DEFAULT_GECKO_SIZE, DEFAULT_GECKO_SIZE );
	if( instance->browser == NULL ) {
		Con_Printf( "CL_Gecko_CreateBrowser: Browser object creation failed!\n" );
	}
	// TODO: assert != NULL
	osgk_browser_set_user_data (instance->browser, USERDATAKEY_CL_GECKO_T,
	  instance, 0);
	instance->ownerProg = ownerProg;

	instance->width = instance->texWidth = DEFAULT_GECKO_SIZE;
	instance->height = instance->texHeight = DEFAULT_GECKO_SIZE;
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
	// FIXME: track cl_numgeckoinstances to avoid scanning the entire array?
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

	if (osgk_dll != NULL)
	{
	    Sys_UnloadLibrary (&osgk_dll);
	}
}

static void cl_gecko_create_f( void ) {
	char name[MAX_QPATH];

	if (Cmd_Argc() != 2)
	{
		Con_Print("usage: gecko_create <name>\npcreates a browser (full texture path " CLGECKOPREFIX "<name>)\n");
		return;
	}

	dpsnprintf(name, sizeof(name), CLGECKOPREFIX "%s", Cmd_Argv(1));
	CL_Gecko_CreateBrowser( name, -1 );
}

static void cl_gecko_destroy_f( void ) {
	char name[MAX_QPATH];

	if (Cmd_Argc() != 2)
	{
		Con_Print("usage: gecko_destroy <name>\ndestroys a browser\n");
		return;
	}

	dpsnprintf(name, sizeof(name), CLGECKOPREFIX "%s", Cmd_Argv(1));
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

	dpsnprintf(name, sizeof(name), CLGECKOPREFIX "%s", Cmd_Argv(1));
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

	dpsnprintf(name, sizeof(name), CLGECKOPREFIX "%s", Cmd_Argv(1));
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

		CL_Gecko_Event_Key( instance, (keynum_t) key, CLG_BET_PRESS );
	}
}

static void gl_gecko_movecursor_f( void ) {
	char name[MAX_QPATH];
	float x, y;

	if (Cmd_Argc() != 4)
	{
		Con_Print("usage: gecko_movecursor <name> <x> <y>\nmove the cursor to a certain position\n");
		return;
	}

	dpsnprintf(name, sizeof(name), CLGECKOPREFIX "%s", Cmd_Argv(1));
	x = atof( Cmd_Argv( 2 ) );
	y = atof( Cmd_Argv( 3 ) );

	CL_Gecko_Event_CursorMove( CL_Gecko_FindBrowser( name ), x, y );
}

#undef osgk_addref
#undef osgk_release

static const dllfunction_t osgkFuncs[] =
{
	{"osgk_addref",				    (void **) &osgk_addref},
	{"osgk_release",			    (void **) &osgk_release},
	{"osgk_embedding_create2",		    (void **) &osgk_embedding_create2},
	{"osgk_embedding_options_create",	    (void **) &osgk_embedding_options_create},
	{"osgk_embedding_options_add_search_path",  (void **) &osgk_embedding_options_add_search_path},
	{"osgk_embedding_options_set_profile_dir",  (void **) &osgk_embedding_options_set_profile_dir},
	{"osgk_browser_create",			    (void **) &osgk_browser_create},
	{"osgk_browser_query_dirty",		    (void **) &osgk_browser_query_dirty},
	{"osgk_browser_navigate",		    (void **) &osgk_browser_navigate},
	{"osgk_browser_lock_data",		    (void **) &osgk_browser_lock_data},
	{"osgk_browser_unlock_data",		    (void **) &osgk_browser_unlock_data},
	{"osgk_browser_resize",			    (void **) &osgk_browser_resize},
	{"osgk_browser_event_mouse_move",	    (void **) &osgk_browser_event_mouse_move},
	{"osgk_browser_event_mouse_button",	    (void **) &osgk_browser_event_mouse_button},
	{"osgk_browser_event_mouse_wheel",	    (void **) &osgk_browser_event_mouse_wheel},
	{"osgk_browser_event_key",		    (void **) &osgk_browser_event_key},
	{"osgk_browser_set_user_data",		    (void **) &osgk_browser_set_user_data},
	{"osgk_browser_get_user_data",		    (void **) &osgk_browser_get_user_data},
	{"osgk_sot_create",			    (void **) &osgk_sot_create},
	{"osgk_sot_register",			    (void **) &osgk_sot_register},
	{"osgk_sot_add_function",		    (void **) &osgk_sot_add_function},
	{"osgk_string_get",			    (void **) &osgk_string_get},
	{"osgk_string_create",			    (void **) &osgk_string_create},
	{"osgk_variant_convert",		    (void **) &osgk_variant_convert},
	{"osgk_variant_get_string",		    (void **) &osgk_variant_get_string},
	{"osgk_variant_create_string",		    (void **) &osgk_variant_create_string},
	{NULL, NULL}
};

qboolean CL_Gecko_OpenLibrary (void)
{
	const char* dllnames_gecko [] =
	{
#if defined(WIN32)
		"OffscreenGecko.dll",
#elif defined(MACOSX)
		"OffscreenGecko.dylib",
#else
		"libOffscreenGecko.so",
#endif
		NULL
	};

	// Already loaded?
	if (osgk_dll)
		return true;

// COMMANDLINEOPTION: Sound: -nogecko disables gecko support (web browser support for menu and computer terminals)
	if (COM_CheckParm("-nogecko"))
		return false;

	return Sys_LoadLibrary (dllnames_gecko, &osgk_dll, osgkFuncs);
}

void CL_Gecko_Init( void )
{
	CL_Gecko_OpenLibrary();

	Cmd_AddCommand( "gecko_create", cl_gecko_create_f, "Create a gecko browser instance" );
	Cmd_AddCommand( "gecko_destroy", cl_gecko_destroy_f, "Destroy a gecko browser instance" );
	Cmd_AddCommand( "gecko_navigate", cl_gecko_navigate_f, "Navigate a gecko browser to a URI" );
	Cmd_AddCommand( "gecko_injecttext", cl_gecko_injecttext_f, "Injects text into a browser" );
	Cmd_AddCommand( "gecko_movecursor", gl_gecko_movecursor_f, "Move the cursor to a certain position" );

	R_RegisterModule( "CL_Gecko", cl_gecko_start, cl_gecko_shutdown, cl_gecko_newmap, NULL, NULL );
}

void CL_Gecko_NavigateToURI( clgecko_t *instance, const char *URI ) {
	if( !instance || !instance->browser ) {
		return;
	}

	if( instance->active ) {
		osgk_browser_navigate( instance->browser, URI );
	}
}

void CL_Gecko_Event_CursorMove( clgecko_t *instance, float x, float y ) {
	// TODO: assert x, y \in [0.0, 1.0]
	int mappedx, mappedy;

	if( !instance || !instance->browser ) {
		return;
	}

	mappedx = (int) (x * instance->width);
	mappedy = (int) (y * instance->height);
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

qboolean CL_Gecko_Event_Key( clgecko_t *instance, keynum_t key, clgecko_buttoneventtype_t eventtype ) {
	if( !instance || !instance->browser ) {
		return false;
	}

	// determine whether its a keyboard event
	if( key < K_OTHERDEVICESBEGIN ) {

		OSGK_KeyboardEventType mappedtype = kePress;
		unsigned int mappedkey = key;
		
		unsigned int i;
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
		OSGK_MouseButtonEventType mappedtype = meDoubleClick;
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
	} else if( K_MWHEELUP <= key && key <= K_MWHEELDOWN ) {
		if( eventtype == CLG_BET_DOWN )
			osgk_browser_event_mouse_wheel( instance->browser, 
				waVertical, (key == K_MWHEELUP) ? wdNegative : wdPositive );
		return true;
	}
	// TODO: error?
	return false;
}
