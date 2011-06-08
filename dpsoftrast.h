
#ifndef DPSOFTRAST_H
#define DPSOFTRAST_H

#include <stdlib.h>

#define DPSOFTRAST_MAXMIPMAPS 16
#define DPSOFTRAST_TEXTURE_MAXSIZE (1<<(DPSOFTRAST_MAXMIPMAPS - 1))
#define DPSOFTRAST_MAXTEXTUREUNITS 16
#define DPSOFTRAST_MAXTEXCOORDARRAYS 8

// type of pixels in texture (some of these are converted to BGRA8 on update)
#define DPSOFTRAST_TEXTURE_FORMAT_BGRA8 0
#define DPSOFTRAST_TEXTURE_FORMAT_DEPTH 1
#define DPSOFTRAST_TEXTURE_FORMAT_RGBA8 2
#define DPSOFTRAST_TEXTURE_FORMAT_ALPHA8 3
#define DPSOFTRAST_TEXTURE_FORMAT_RGBA16F 4
#define DPSOFTRAST_TEXTURE_FORMAT_RGBA32F 5
#define DPSOFTRAST_TEXTURE_FORMAT_COMPAREMASK 0x0F

// modifier flags for texture (can not be changed after creation)
#define DPSOFTRAST_TEXTURE_FLAG_MIPMAP 0x10
#define DPSOFTRAST_TEXTURE_FLAG_CUBEMAP 0x20
#define DPSOFTRAST_TEXTURE_FLAG_USEALPHA 0x40
#define DPSOFTRAST_TEXTURE_FLAG_CLAMPTOEDGE 0x80

typedef enum DPSOFTRAST_TEXTURE_FILTER_e
{
	DPSOFTRAST_TEXTURE_FILTER_NEAREST = 0,
	DPSOFTRAST_TEXTURE_FILTER_LINEAR = 1,
	DPSOFTRAST_TEXTURE_FILTER_NEAREST_MIPMAP_TRIANGLE = 2,
	DPSOFTRAST_TEXTURE_FILTER_LINEAR_MIPMAP_TRIANGLE = 3,
}
DPSOFTRAST_TEXTURE_FILTER;

int DPSOFTRAST_Init(int width, int height, int numthreads, int interlace, unsigned int *colorpixels, unsigned int *depthpixels);
void DPSOFTRAST_Shutdown(void);
void DPSOFTRAST_Flush(void);
void DPSOFTRAST_Finish(void);

int DPSOFTRAST_Texture_New(int flags, int width, int height, int depth);
void DPSOFTRAST_Texture_Free(int index);
void DPSOFTRAST_Texture_UpdatePartial(int index, int mip, const unsigned char *pixels, int blockx, int blocky, int blockwidth, int blockheight);
void DPSOFTRAST_Texture_UpdateFull(int index, const unsigned char *pixels);
int DPSOFTRAST_Texture_GetWidth(int index, int mip);
int DPSOFTRAST_Texture_GetHeight(int index, int mip);
int DPSOFTRAST_Texture_GetDepth(int index, int mip);
unsigned char *DPSOFTRAST_Texture_GetPixelPointer(int index, int mip);
void DPSOFTRAST_Texture_Filter(int index, DPSOFTRAST_TEXTURE_FILTER filter);

void DPSOFTRAST_SetRenderTargets(int width, int height, unsigned int *depthpixels, unsigned int *colorpixels0, unsigned int *colorpixels1, unsigned int *colorpixels2, unsigned int *colorpixels3);
void DPSOFTRAST_Viewport(int x, int y, int width, int height);
void DPSOFTRAST_ClearColor(float r, float g, float b, float a);
void DPSOFTRAST_ClearDepth(float d);
void DPSOFTRAST_ColorMask(int r, int g, int b, int a);
void DPSOFTRAST_DepthTest(int enable);
void DPSOFTRAST_ScissorTest(int enable);
void DPSOFTRAST_Scissor(float x, float y, float width, float height);
void DPSOFTRAST_ClipPlane(float x, float y, float z, float w);

void DPSOFTRAST_BlendFunc(int smodulate, int dmodulate);
void DPSOFTRAST_BlendSubtract(int enable);
void DPSOFTRAST_DepthMask(int enable);
void DPSOFTRAST_DepthFunc(int comparemode);
void DPSOFTRAST_DepthRange(float range0, float range1);
void DPSOFTRAST_PolygonOffset(float alongnormal, float intoview);
void DPSOFTRAST_CullFace(int mode);
void DPSOFTRAST_Color4f(float r, float g, float b, float a);
void DPSOFTRAST_GetPixelsBGRA(int blockx, int blocky, int blockwidth, int blockheight, unsigned char *outpixels);
void DPSOFTRAST_CopyRectangleToTexture(int index, int mip, int tx, int ty, int sx, int sy, int width, int height);
void DPSOFTRAST_SetTexture(int unitnum, int index);

void DPSOFTRAST_SetVertexPointer(const float *vertex3f, size_t stride);
void DPSOFTRAST_SetColorPointer(const float *color4f, size_t stride);
void DPSOFTRAST_SetColorPointer4ub(const unsigned char *color4ub, size_t stride);
void DPSOFTRAST_SetTexCoordPointer(int unitnum, int numcomponents, size_t stride, const float *texcoordf);

typedef enum gl20_texunit_e
{
	// postprocess shaders, and generic shaders:
	GL20TU_FIRST = 0,
	GL20TU_SECOND = 1,
	GL20TU_GAMMARAMPS = 2,
	// standard material properties
	GL20TU_NORMAL = 0,
	GL20TU_COLOR = 1,
	GL20TU_GLOSS = 2,
	GL20TU_GLOW = 3,
	// material properties for a second material
	GL20TU_SECONDARY_NORMAL = 4,
	GL20TU_SECONDARY_COLOR = 5,
	GL20TU_SECONDARY_GLOSS = 6,
	GL20TU_SECONDARY_GLOW = 7,
	// material properties for a colormapped material
	// conflicts with secondary material
	GL20TU_PANTS = 4,
	GL20TU_SHIRT = 7,
	// fog fade in the distance
	GL20TU_FOGMASK = 8,
	// compiled ambient lightmap and deluxemap
	GL20TU_LIGHTMAP = 9,
	GL20TU_DELUXEMAP = 10,
	// refraction, used by water shaders
	GL20TU_REFRACTION = 3,
	// reflection, used by water shaders, also with normal material rendering
	// conflicts with secondary material
	GL20TU_REFLECTION = 7,
	// rtlight attenuation (distance fade) and cubemap filter (projection texturing)
	// conflicts with lightmap/deluxemap
	GL20TU_ATTENUATION = 9,
	GL20TU_CUBE = 10,
	GL20TU_SHADOWMAP2D = 15,
	GL20TU_CUBEPROJECTION = 12,
	// rtlight prepass data (screenspace depth and normalmap)
	GL20TU_SCREENDEPTH = 13,
	GL20TU_SCREENNORMALMAP = 14,
	// lightmap prepass data (screenspace diffuse and specular from lights)
	GL20TU_SCREENDIFFUSE = 11,
	GL20TU_SCREENSPECULAR = 12,
	// fake reflections
	GL20TU_REFLECTMASK = 5,
	GL20TU_REFLECTCUBE = 6,
	GL20TU_FOGHEIGHTTEXTURE = 14
}
gl20_texunit;

typedef enum glsl_attrib_e
{
	GLSLATTRIB_POSITION = 0,
	GLSLATTRIB_COLOR = 1,
	GLSLATTRIB_TEXCOORD0 = 2,
	GLSLATTRIB_TEXCOORD1 = 3,
	GLSLATTRIB_TEXCOORD2 = 4,
	GLSLATTRIB_TEXCOORD3 = 5,
	GLSLATTRIB_TEXCOORD4 = 6,
	GLSLATTRIB_TEXCOORD5 = 7,
	GLSLATTRIB_TEXCOORD6 = 8,
	GLSLATTRIB_TEXCOORD7 = 9,
}
glsl_attrib;

// this enum selects which of the glslshadermodeinfo entries should be used
typedef enum shadermode_e
{
	SHADERMODE_GENERIC, ///< (particles/HUD/etc) vertex color, optionally multiplied by one texture
	SHADERMODE_POSTPROCESS, ///< postprocessing shader (r_glsl_postprocess)
	SHADERMODE_DEPTH_OR_SHADOW, ///< (depthfirst/shadows) vertex shader only
	SHADERMODE_FLATCOLOR, ///< (lightmap) modulate texture by uniform color (q1bsp, q3bsp)
	SHADERMODE_VERTEXCOLOR, ///< (lightmap) modulate texture by vertex colors (q3bsp)
	SHADERMODE_LIGHTMAP, ///< (lightmap) modulate texture by lightmap texture (q1bsp, q3bsp)
	SHADERMODE_FAKELIGHT, ///< (fakelight) modulate texture by "fake" lighting (no lightmaps, no nothing)
	SHADERMODE_LIGHTDIRECTIONMAP_MODELSPACE, ///< (lightmap) use directional pixel shading from texture containing modelspace light directions (q3bsp deluxemap)
	SHADERMODE_LIGHTDIRECTIONMAP_TANGENTSPACE, ///< (lightmap) use directional pixel shading from texture containing tangentspace light directions (q1bsp deluxemap)
	SHADERMODE_LIGHTDIRECTION, ///< (lightmap) use directional pixel shading from fixed light direction (q3bsp)
	SHADERMODE_LIGHTSOURCE, ///< (lightsource) use directional pixel shading from light source (rtlight)
	SHADERMODE_REFRACTION, ///< refract background (the material is rendered normally after this pass)
	SHADERMODE_WATER, ///< refract background and reflection (the material is rendered normally after this pass)
	SHADERMODE_SHOWDEPTH, ///< (debugging) renders depth as color
	SHADERMODE_DEFERREDGEOMETRY, ///< (deferred) render material properties to screenspace geometry buffers
	SHADERMODE_DEFERREDLIGHTSOURCE, ///< (deferred) use directional pixel shading from light source (rtlight) on screenspace geometry buffers
	SHADERMODE_COUNT
}
shadermode_t;

typedef enum shaderpermutation_e
{
	SHADERPERMUTATION_DIFFUSE = 1<<0, ///< (lightsource) whether to use directional shading
	SHADERPERMUTATION_VERTEXTEXTUREBLEND = 1<<1, ///< indicates this is a two-layer material blend based on vertex alpha (q3bsp)
	SHADERPERMUTATION_VIEWTINT = 1<<2, ///< view tint (postprocessing only), use vertex colors (generic only)
	SHADERPERMUTATION_COLORMAPPING = 1<<3, ///< indicates this is a colormapped skin
	SHADERPERMUTATION_SATURATION = 1<<4, ///< saturation (postprocessing only)
	SHADERPERMUTATION_FOGINSIDE = 1<<5, ///< tint the color by fog color or black if using additive blend mode
	SHADERPERMUTATION_FOGOUTSIDE = 1<<6, ///< tint the color by fog color or black if using additive blend mode
	SHADERPERMUTATION_FOGHEIGHTTEXTURE = 1<<7, ///< fog color and density determined by texture mapped on vertical axis
	SHADERPERMUTATION_FOGALPHAHACK = 1<<8, ///< fog color and density determined by texture mapped on vertical axis
	SHADERPERMUTATION_GAMMARAMPS = 1<<9, ///< gamma (postprocessing only)
	SHADERPERMUTATION_CUBEFILTER = 1<<10, ///< (lightsource) use cubemap light filter
	SHADERPERMUTATION_GLOW = 1<<11, ///< (lightmap) blend in an additive glow texture
	SHADERPERMUTATION_BLOOM = 1<<12, ///< bloom (postprocessing only)
	SHADERPERMUTATION_SPECULAR = 1<<13, ///< (lightsource or deluxemapping) render specular effects
	SHADERPERMUTATION_POSTPROCESSING = 1<<14, ///< user defined postprocessing (postprocessing only)
	SHADERPERMUTATION_REFLECTION = 1<<15, ///< normalmap-perturbed reflection of the scene infront of the surface, preformed as an overlay on the surface
	SHADERPERMUTATION_OFFSETMAPPING = 1<<16, ///< adjust texcoords to roughly simulate a displacement mapped surface
	SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING = 1<<17, ///< adjust texcoords to accurately simulate a displacement mapped surface (requires OFFSETMAPPING to also be set!)
	SHADERPERMUTATION_SHADOWMAP2D = 1<<18, ///< (lightsource) use shadowmap texture as light filter
	SHADERPERMUTATION_SHADOWMAPPCF = 1<<19, ///< (lightsource) use percentage closer filtering on shadowmap test results
	SHADERPERMUTATION_SHADOWMAPPCF2 = 1<<20, ///< (lightsource) use higher quality percentage closer filtering on shadowmap test results
	SHADERPERMUTATION_SHADOWSAMPLER = 1<<21, ///< (lightsource) use hardware shadowmap test
	SHADERPERMUTATION_SHADOWMAPVSDCT = 1<<22, ///< (lightsource) use virtual shadow depth cube texture for shadowmap indexing
	SHADERPERMUTATION_SHADOWMAPORTHO = 1<<23, ///< (lightsource) use orthographic shadowmap projection
	SHADERPERMUTATION_DEFERREDLIGHTMAP = 1<<24, ///< (lightmap) read Texture_ScreenDiffuse/Specular textures and add them on top of lightmapping
	SHADERPERMUTATION_ALPHAKILL = 1<<25, ///< (deferredgeometry) discard pixel if diffuse texture alpha below 0.5
	SHADERPERMUTATION_REFLECTCUBE = 1<<26, ///< fake reflections using global cubemap (not HDRI light probe)
	SHADERPERMUTATION_NORMALMAPSCROLLBLEND = 1<<27, ///< (water) counter-direction normalmaps scrolling
	SHADERPERMUTATION_BOUNCEGRID = 1<<28, ///< (lightmap) use Texture_BounceGrid as an additional source of ambient light
	SHADERPERMUTATION_BOUNCEGRIDDIRECTIONAL = 1<<29, ///< (lightmap) use 16-component pixels in bouncegrid texture for directional lighting rather than standard 4-component
	SHADERPERMUTATION_TRIPPY = 1<<30, ///< use trippy vertex shader effect
	SHADERPERMUTATION_LIMIT = 1<<31, ///< size of permutations array
	SHADERPERMUTATION_COUNT = 31 ///< size of shaderpermutationinfo array
}
shaderpermutation_t;

typedef enum DPSOFTRAST_UNIFORM_e
{
	DPSOFTRAST_UNIFORM_Texture_First,
	DPSOFTRAST_UNIFORM_Texture_Second,
	DPSOFTRAST_UNIFORM_Texture_GammaRamps,
	DPSOFTRAST_UNIFORM_Texture_Normal,
	DPSOFTRAST_UNIFORM_Texture_Color,
	DPSOFTRAST_UNIFORM_Texture_Gloss,
	DPSOFTRAST_UNIFORM_Texture_Glow,
	DPSOFTRAST_UNIFORM_Texture_SecondaryNormal,
	DPSOFTRAST_UNIFORM_Texture_SecondaryColor,
	DPSOFTRAST_UNIFORM_Texture_SecondaryGloss,
	DPSOFTRAST_UNIFORM_Texture_SecondaryGlow,
	DPSOFTRAST_UNIFORM_Texture_Pants,
	DPSOFTRAST_UNIFORM_Texture_Shirt,
	DPSOFTRAST_UNIFORM_Texture_FogHeightTexture,
	DPSOFTRAST_UNIFORM_Texture_FogMask,
	DPSOFTRAST_UNIFORM_Texture_Lightmap,
	DPSOFTRAST_UNIFORM_Texture_Deluxemap,
	DPSOFTRAST_UNIFORM_Texture_Attenuation,
	DPSOFTRAST_UNIFORM_Texture_Cube,
	DPSOFTRAST_UNIFORM_Texture_Refraction,
	DPSOFTRAST_UNIFORM_Texture_Reflection,
	DPSOFTRAST_UNIFORM_Texture_ShadowMap2D,
	DPSOFTRAST_UNIFORM_Texture_CubeProjection,
	DPSOFTRAST_UNIFORM_Texture_ScreenDepth,
	DPSOFTRAST_UNIFORM_Texture_ScreenNormalMap,
	DPSOFTRAST_UNIFORM_Texture_ScreenDiffuse,
	DPSOFTRAST_UNIFORM_Texture_ScreenSpecular,
	DPSOFTRAST_UNIFORM_Texture_ReflectMask,
	DPSOFTRAST_UNIFORM_Texture_ReflectCube,
	DPSOFTRAST_UNIFORM_Alpha,
	DPSOFTRAST_UNIFORM_BloomBlur_Parameters,
	DPSOFTRAST_UNIFORM_ClientTime,
	DPSOFTRAST_UNIFORM_Color_Ambient,
	DPSOFTRAST_UNIFORM_Color_Diffuse,
	DPSOFTRAST_UNIFORM_Color_Specular,
	DPSOFTRAST_UNIFORM_Color_Glow,
	DPSOFTRAST_UNIFORM_Color_Pants,
	DPSOFTRAST_UNIFORM_Color_Shirt,
	DPSOFTRAST_UNIFORM_DeferredColor_Ambient,
	DPSOFTRAST_UNIFORM_DeferredColor_Diffuse,
	DPSOFTRAST_UNIFORM_DeferredColor_Specular,
	DPSOFTRAST_UNIFORM_DeferredMod_Diffuse,
	DPSOFTRAST_UNIFORM_DeferredMod_Specular,
	DPSOFTRAST_UNIFORM_DistortScaleRefractReflect,
	DPSOFTRAST_UNIFORM_EyePosition,
	DPSOFTRAST_UNIFORM_FogColor,
	DPSOFTRAST_UNIFORM_FogHeightFade,
	DPSOFTRAST_UNIFORM_FogPlane,
	DPSOFTRAST_UNIFORM_FogPlaneViewDist,
	DPSOFTRAST_UNIFORM_FogRangeRecip,
	DPSOFTRAST_UNIFORM_LightColor,
	DPSOFTRAST_UNIFORM_LightDir,
	DPSOFTRAST_UNIFORM_LightPosition,
	DPSOFTRAST_UNIFORM_OffsetMapping_ScaleSteps,
	DPSOFTRAST_UNIFORM_PixelSize,
	DPSOFTRAST_UNIFORM_ReflectColor,
	DPSOFTRAST_UNIFORM_ReflectFactor,
	DPSOFTRAST_UNIFORM_ReflectOffset,
	DPSOFTRAST_UNIFORM_RefractColor,
	DPSOFTRAST_UNIFORM_Saturation,
	DPSOFTRAST_UNIFORM_ScreenCenterRefractReflect,
	DPSOFTRAST_UNIFORM_ScreenScaleRefractReflect,
	DPSOFTRAST_UNIFORM_ScreenToDepth,
	DPSOFTRAST_UNIFORM_ShadowMap_Parameters,
	DPSOFTRAST_UNIFORM_ShadowMap_TextureScale,
	DPSOFTRAST_UNIFORM_SpecularPower,
	DPSOFTRAST_UNIFORM_UserVec1,
	DPSOFTRAST_UNIFORM_UserVec2,
	DPSOFTRAST_UNIFORM_UserVec3,
	DPSOFTRAST_UNIFORM_UserVec4,
	DPSOFTRAST_UNIFORM_ViewTintColor,
	DPSOFTRAST_UNIFORM_ViewToLightM1,
	DPSOFTRAST_UNIFORM_ViewToLightM2,
	DPSOFTRAST_UNIFORM_ViewToLightM3,
	DPSOFTRAST_UNIFORM_ViewToLightM4,
	DPSOFTRAST_UNIFORM_ModelToLightM1,
	DPSOFTRAST_UNIFORM_ModelToLightM2,
	DPSOFTRAST_UNIFORM_ModelToLightM3,
	DPSOFTRAST_UNIFORM_ModelToLightM4,
	DPSOFTRAST_UNIFORM_TexMatrixM1,
	DPSOFTRAST_UNIFORM_TexMatrixM2,
	DPSOFTRAST_UNIFORM_TexMatrixM3,
	DPSOFTRAST_UNIFORM_TexMatrixM4,
	DPSOFTRAST_UNIFORM_BackgroundTexMatrixM1,
	DPSOFTRAST_UNIFORM_BackgroundTexMatrixM2,
	DPSOFTRAST_UNIFORM_BackgroundTexMatrixM3,
	DPSOFTRAST_UNIFORM_BackgroundTexMatrixM4,
	DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1,
	DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM2,
	DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM3,
	DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM4,
	DPSOFTRAST_UNIFORM_ModelViewMatrixM1,
	DPSOFTRAST_UNIFORM_ModelViewMatrixM2,
	DPSOFTRAST_UNIFORM_ModelViewMatrixM3,
	DPSOFTRAST_UNIFORM_ModelViewMatrixM4,
	DPSOFTRAST_UNIFORM_PixelToScreenTexCoord,
	DPSOFTRAST_UNIFORM_ModelToReflectCubeM1,
	DPSOFTRAST_UNIFORM_ModelToReflectCubeM2,
	DPSOFTRAST_UNIFORM_ModelToReflectCubeM3,
	DPSOFTRAST_UNIFORM_ModelToReflectCubeM4,
	DPSOFTRAST_UNIFORM_ShadowMapMatrixM1,
	DPSOFTRAST_UNIFORM_ShadowMapMatrixM2,
	DPSOFTRAST_UNIFORM_ShadowMapMatrixM3,
	DPSOFTRAST_UNIFORM_ShadowMapMatrixM4,
	DPSOFTRAST_UNIFORM_BloomColorSubtract,
	DPSOFTRAST_UNIFORM_NormalmapScrollBlend,
	DPSOFTRAST_UNIFORM_TOTAL
}
DPSOFTRAST_UNIFORM;

void DPSOFTRAST_SetShader(int mode, int permutation, int exactspecularmath);
#define DPSOFTRAST_Uniform1f(index, v0) DPSOFTRAST_Uniform4f(index, v0, 0, 0, 0)
#define DPSOFTRAST_Uniform2f(index, v0, v1) DPSOFTRAST_Uniform4f(index, v0, v1, 0, 0)
#define DPSOFTRAST_Uniform3f(index, v0, v1, v2) DPSOFTRAST_Uniform4f(index, v0, v1, v2, 0)
void DPSOFTRAST_Uniform4f(DPSOFTRAST_UNIFORM index, float v0, float v1, float v2, float v3);
void DPSOFTRAST_Uniform4fv(DPSOFTRAST_UNIFORM index, const float *v);
void DPSOFTRAST_UniformMatrix4fv(DPSOFTRAST_UNIFORM index, int arraysize, int transpose, const float *v);
void DPSOFTRAST_Uniform1i(DPSOFTRAST_UNIFORM index, int i0);

void DPSOFTRAST_DrawTriangles(int firstvertex, int numvertices, int numtriangles, const int *element3i, const unsigned short *element3s);

#endif // DPSOFTRAST_H
