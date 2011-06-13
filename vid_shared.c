
#include "quakedef.h"
#include "cdaudio.h"

#ifdef SUPPORTD3D
#include <d3d9.h>
#ifdef _MSC_VER
#pragma comment(lib, "d3d9.lib")
#endif

LPDIRECT3DDEVICE9 vid_d3d9dev;
#endif

#ifdef WIN32
//#include <XInput.h>
#define XINPUT_GAMEPAD_DPAD_UP          0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN        0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT        0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT       0x0008
#define XINPUT_GAMEPAD_START            0x0010
#define XINPUT_GAMEPAD_BACK             0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB       0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB      0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER    0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER   0x0200
#define XINPUT_GAMEPAD_A                0x1000
#define XINPUT_GAMEPAD_B                0x2000
#define XINPUT_GAMEPAD_X                0x4000
#define XINPUT_GAMEPAD_Y                0x8000
#define XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE  7849
#define XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE 8689
#define XINPUT_GAMEPAD_TRIGGER_THRESHOLD    30
#define XUSER_INDEX_ANY                 0x000000FF

typedef struct xinput_gamepad_s
{
	WORD wButtons;
	BYTE bLeftTrigger;
	BYTE bRightTrigger;
	SHORT sThumbLX;
	SHORT sThumbLY;
	SHORT sThumbRX;
	SHORT sThumbRY;
}
xinput_gamepad_t;

typedef struct xinput_state_s
{
	DWORD dwPacketNumber;
	xinput_gamepad_t Gamepad;
}
xinput_state_t;

typedef struct xinput_keystroke_s
{
    WORD    VirtualKey;
    WCHAR   Unicode;
    WORD    Flags;
    BYTE    UserIndex;
    BYTE    HidCode;
}
xinput_keystroke_t;

DWORD (WINAPI *qXInputGetState)(DWORD index, xinput_state_t *state);
DWORD (WINAPI *qXInputGetKeystroke)(DWORD index, DWORD reserved, xinput_keystroke_t *keystroke);

qboolean vid_xinputinitialized = false;
int vid_xinputindex = -1;
#endif

// global video state
viddef_t vid;

// LordHavoc: these are only set in wgl
qboolean isG200 = false; // LordHavoc: the Matrox G200 can't do per pixel alpha, and it uses a D3D driver for GL... ugh...
qboolean isRagePro = false; // LordHavoc: the ATI Rage Pro has limitations with per pixel alpha (the color scaler does not apply to per pixel alpha images...), although not as bad as a G200.

// AK FIXME -> input_dest
qboolean in_client_mouse = true;

// AK where should it be placed ?
float in_mouse_x, in_mouse_y;
float in_windowmouse_x, in_windowmouse_y;

// LordHavoc: if window is hidden, don't update screen
qboolean vid_hidden = true;
// LordHavoc: if window is not the active window, don't hog as much CPU time,
// let go of the mouse, turn off sound, and restore system gamma ramps...
qboolean vid_activewindow = true;

vid_joystate_t vid_joystate;

#ifdef WIN32
cvar_t joy_xinputavailable = {CVAR_READONLY, "joy_xinputavailable", "0", "indicates which devices are being reported by the Windows XInput API (first controller = 1, second = 2, third = 4, fourth = 8, added together)"};
#endif
cvar_t joy_active = {CVAR_READONLY, "joy_active", "0", "indicates that a joystick is active (detected and enabled)"};
cvar_t joy_detected = {CVAR_READONLY, "joy_detected", "0", "number of joysticks detected by engine"};
cvar_t joy_enable = {CVAR_SAVE, "joy_enable", "0", "enables joystick support"};
cvar_t joy_index = {0, "joy_index", "0", "selects which joystick to use if you have multiple (0 uses the first controller, 1 uses the second, ...)"};
cvar_t joy_axisforward = {0, "joy_axisforward", "1", "which joystick axis to query for forward/backward movement"};
cvar_t joy_axisside = {0, "joy_axisside", "0", "which joystick axis to query for right/left movement"};
cvar_t joy_axisup = {0, "joy_axisup", "-1", "which joystick axis to query for up/down movement"};
cvar_t joy_axispitch = {0, "joy_axispitch", "3", "which joystick axis to query for looking up/down"};
cvar_t joy_axisyaw = {0, "joy_axisyaw", "2", "which joystick axis to query for looking right/left"};
cvar_t joy_axisroll = {0, "joy_axisroll", "-1", "which joystick axis to query for tilting head right/left"};
cvar_t joy_deadzoneforward = {0, "joy_deadzoneforward", "0", "deadzone tolerance, suggested values are in the range 0 to 0.01"};
cvar_t joy_deadzoneside = {0, "joy_deadzoneside", "0", "deadzone tolerance, suggested values are in the range 0 to 0.01"};
cvar_t joy_deadzoneup = {0, "joy_deadzoneup", "0", "deadzone tolerance, suggested values are in the range 0 to 0.01"};
cvar_t joy_deadzonepitch = {0, "joy_deadzonepitch", "0", "deadzone tolerance, suggested values are in the range 0 to 0.01"};
cvar_t joy_deadzoneyaw = {0, "joy_deadzoneyaw", "0", "deadzone tolerance, suggested values are in the range 0 to 0.01"};
cvar_t joy_deadzoneroll = {0, "joy_deadzoneroll", "0", "deadzone tolerance, suggested values are in the range 0 to 0.01"};
cvar_t joy_sensitivityforward = {0, "joy_sensitivityforward", "-1", "movement multiplier"};
cvar_t joy_sensitivityside = {0, "joy_sensitivityside", "1", "movement multiplier"};
cvar_t joy_sensitivityup = {0, "joy_sensitivityup", "1", "movement multiplier"};
cvar_t joy_sensitivitypitch = {0, "joy_sensitivitypitch", "1", "movement multiplier"};
cvar_t joy_sensitivityyaw = {0, "joy_sensitivityyaw", "-1", "movement multiplier"};
cvar_t joy_sensitivityroll = {0, "joy_sensitivityroll", "1", "movement multiplier"};
cvar_t joy_axiskeyevents = {CVAR_SAVE, "joy_axiskeyevents", "0", "generate uparrow/leftarrow etc. keyevents for joystick axes, use if your joystick driver is not generating them"};
cvar_t joy_axiskeyevents_deadzone = {CVAR_SAVE, "joy_axiskeyevents_deadzone", "0.5", "deadzone value for axes"};
cvar_t joy_x360_axisforward = {0, "joy_x360_axisforward", "1", "which joystick axis to query for forward/backward movement"};
cvar_t joy_x360_axisside = {0, "joy_x360_axisside", "0", "which joystick axis to query for right/left movement"};
cvar_t joy_x360_axisup = {0, "joy_x360_axisup", "-1", "which joystick axis to query for up/down movement"};
cvar_t joy_x360_axispitch = {0, "joy_x360_axispitch", "3", "which joystick axis to query for looking up/down"};
cvar_t joy_x360_axisyaw = {0, "joy_x360_axisyaw", "2", "which joystick axis to query for looking right/left"};
cvar_t joy_x360_axisroll = {0, "joy_x360_axisroll", "-1", "which joystick axis to query for tilting head right/left"};
cvar_t joy_x360_deadzoneforward = {0, "joy_x360_deadzoneforward", "0.266", "deadzone tolerance, suggested values are in the range 0 to 0.01"};
cvar_t joy_x360_deadzoneside = {0, "joy_x360_deadzoneside", "0.266", "deadzone tolerance, suggested values are in the range 0 to 0.01"};
cvar_t joy_x360_deadzoneup = {0, "joy_x360_deadzoneup", "0.266", "deadzone tolerance, suggested values are in the range 0 to 0.01"};
cvar_t joy_x360_deadzonepitch = {0, "joy_x360_deadzonepitch", "0.266", "deadzone tolerance, suggested values are in the range 0 to 0.01"};
cvar_t joy_x360_deadzoneyaw = {0, "joy_x360_deadzoneyaw", "0.266", "deadzone tolerance, suggested values are in the range 0 to 0.01"};
cvar_t joy_x360_deadzoneroll = {0, "joy_x360_deadzoneroll", "0.266", "deadzone tolerance, suggested values are in the range 0 to 0.01"};
cvar_t joy_x360_sensitivityforward = {0, "joy_x360_sensitivityforward", "1", "movement multiplier"};
cvar_t joy_x360_sensitivityside = {0, "joy_x360_sensitivityside", "1", "movement multiplier"};
cvar_t joy_x360_sensitivityup = {0, "joy_x360_sensitivityup", "1", "movement multiplier"};
cvar_t joy_x360_sensitivitypitch = {0, "joy_x360_sensitivitypitch", "-1", "movement multiplier"};
cvar_t joy_x360_sensitivityyaw = {0, "joy_x360_sensitivityyaw", "-1", "movement multiplier"};
cvar_t joy_x360_sensitivityroll = {0, "joy_x360_sensitivityroll", "1", "movement multiplier"};

// cvars for DPSOFTRAST
cvar_t vid_soft = {CVAR_SAVE, "vid_soft", "0", "enables use of the DarkPlaces Software Rasterizer rather than OpenGL or Direct3D"};
cvar_t vid_soft_threads = {CVAR_SAVE, "vid_soft_threads", "2", "the number of threads the DarkPlaces Software Rasterizer should use"}; 
cvar_t vid_soft_interlace = {CVAR_SAVE, "vid_soft_interlace", "1", "whether the DarkPlaces Software Rasterizer should interlace the screen bands occupied by each thread"};

// we don't know until we try it!
cvar_t vid_hardwaregammasupported = {CVAR_READONLY,"vid_hardwaregammasupported","1", "indicates whether hardware gamma is supported (updated by attempts to set hardware gamma ramps)"};

// VorteX: more info cvars, mostly set in VID_CheckExtensions
cvar_t gl_info_vendor = {CVAR_READONLY, "gl_info_vendor", "", "indicates brand of graphics chip"};
cvar_t gl_info_renderer = {CVAR_READONLY, "gl_info_renderer", "", "indicates graphics chip model and other information"};
cvar_t gl_info_version = {CVAR_READONLY, "gl_info_version", "", "indicates version of current renderer. begins with 1.0.0, 1.1.0, 1.2.0, 1.3.1 etc."};
cvar_t gl_info_extensions = {CVAR_READONLY, "gl_info_extensions", "", "indicates extension list found by engine, space separated."};
cvar_t gl_info_platform = {CVAR_READONLY, "gl_info_platform", "", "indicates GL platform: WGL, GLX, or AGL."};
cvar_t gl_info_driver = {CVAR_READONLY, "gl_info_driver", "", "name of driver library (opengl32.dll, libGL.so.1, or whatever)."};

// whether hardware gamma ramps are currently in effect
qboolean vid_usinghwgamma = false;

int vid_gammarampsize = 0;
unsigned short *vid_gammaramps = NULL;
unsigned short *vid_systemgammaramps = NULL;

cvar_t vid_fullscreen = {CVAR_SAVE, "vid_fullscreen", "1", "use fullscreen (1) or windowed (0)"};
cvar_t vid_width = {CVAR_SAVE, "vid_width", "640", "resolution"};
cvar_t vid_height = {CVAR_SAVE, "vid_height", "480", "resolution"};
cvar_t vid_bitsperpixel = {CVAR_SAVE, "vid_bitsperpixel", "32", "how many bits per pixel to render at (32 or 16, 32 is recommended)"};
cvar_t vid_samples = {CVAR_SAVE, "vid_samples", "1", "how many anti-aliasing samples per pixel to request from the graphics driver (4 is recommended, 1 is faster)"};
cvar_t vid_refreshrate = {CVAR_SAVE, "vid_refreshrate", "60", "refresh rate to use, in hz (higher values flicker less, if supported by your monitor)"};
cvar_t vid_userefreshrate = {CVAR_SAVE, "vid_userefreshrate", "0", "set this to 1 to make vid_refreshrate used, or to 0 to let the engine choose a sane default"};
cvar_t vid_stereobuffer = {CVAR_SAVE, "vid_stereobuffer", "0", "enables 'quad-buffered' stereo rendering for stereo shutterglasses, HMD (head mounted display) devices, or polarized stereo LCDs, if supported by your drivers"};

cvar_t vid_vsync = {CVAR_SAVE, "vid_vsync", "0", "sync to vertical blank, prevents 'tearing' (seeing part of one frame and part of another on the screen at the same time), automatically disabled when doing timedemo benchmarks"};
cvar_t vid_mouse = {CVAR_SAVE, "vid_mouse", "1", "whether to use the mouse in windowed mode (fullscreen always does)"};
cvar_t vid_grabkeyboard = {CVAR_SAVE, "vid_grabkeyboard", "0", "whether to grab the keyboard when mouse is active (prevents use of volume control keys, music player keys, etc on some keyboards)"};
cvar_t vid_minwidth = {0, "vid_minwidth", "0", "minimum vid_width that is acceptable (to be set in default.cfg in mods)"};
cvar_t vid_minheight = {0, "vid_minheight", "0", "minimum vid_height that is acceptable (to be set in default.cfg in mods)"};
cvar_t vid_gl13 = {0, "vid_gl13", "1", "enables faster rendering using OpenGL 1.3 features (such as GL_ARB_texture_env_combine extension)"};
cvar_t vid_gl20 = {0, "vid_gl20", "1", "enables faster rendering using OpenGL 2.0 features (such as GL_ARB_fragment_shader extension)"};
cvar_t gl_finish = {0, "gl_finish", "0", "make the cpu wait for the graphics processor at the end of each rendered frame (can help with strange input or video lag problems on some machines)"};
cvar_t vid_sRGB = {CVAR_SAVE, "vid_sRGB", "0", "if hardware is capable, modify rendering to be gamma corrected for the sRGB color standard (computer monitors, TVs), recommended"};

cvar_t vid_touchscreen = {0, "vid_touchscreen", "0", "Use touchscreen-style input (no mouse grab, track mouse motion only while button is down, screen areas for mimicing joystick axes and buttons"};
cvar_t vid_stick_mouse = {CVAR_SAVE, "vid_stick_mouse", "0", "have the mouse stuck in the center of the screen" };
cvar_t vid_resizable = {CVAR_SAVE, "vid_resizable", "0", "0: window not resizable, 1: resizable, 2: window can be resized but the framebuffer isn't adjusted" };

cvar_t v_gamma = {CVAR_SAVE, "v_gamma", "1", "inverse gamma correction value, a brightness effect that does not affect white or black, and tends to make the image grey and dull"};
cvar_t v_contrast = {CVAR_SAVE, "v_contrast", "1", "brightness of white (values above 1 give a brighter image with increased color saturation, unlike v_gamma)"};
cvar_t v_brightness = {CVAR_SAVE, "v_brightness", "0", "brightness of black, useful for monitors that are too dark"};
cvar_t v_contrastboost = {CVAR_SAVE, "v_contrastboost", "1", "by how much to multiply the contrast in dark areas (1 is no change)"};
cvar_t v_color_enable = {CVAR_SAVE, "v_color_enable", "0", "enables black-grey-white color correction curve controls"};
cvar_t v_color_black_r = {CVAR_SAVE, "v_color_black_r", "0", "desired color of black"};
cvar_t v_color_black_g = {CVAR_SAVE, "v_color_black_g", "0", "desired color of black"};
cvar_t v_color_black_b = {CVAR_SAVE, "v_color_black_b", "0", "desired color of black"};
cvar_t v_color_grey_r = {CVAR_SAVE, "v_color_grey_r", "0.5", "desired color of grey"};
cvar_t v_color_grey_g = {CVAR_SAVE, "v_color_grey_g", "0.5", "desired color of grey"};
cvar_t v_color_grey_b = {CVAR_SAVE, "v_color_grey_b", "0.5", "desired color of grey"};
cvar_t v_color_white_r = {CVAR_SAVE, "v_color_white_r", "1", "desired color of white"};
cvar_t v_color_white_g = {CVAR_SAVE, "v_color_white_g", "1", "desired color of white"};
cvar_t v_color_white_b = {CVAR_SAVE, "v_color_white_b", "1", "desired color of white"};
cvar_t v_hwgamma = {CVAR_SAVE, "v_hwgamma", "0", "enables use of hardware gamma correction ramps if available (note: does not work very well on Windows2000 and above), values are 0 = off, 1 = attempt to use hardware gamma, 2 = use hardware gamma whether it works or not"};
cvar_t v_glslgamma = {CVAR_SAVE, "v_glslgamma", "1", "enables use of GLSL to apply gamma correction ramps if available (note: overrides v_hwgamma)"};
cvar_t v_psycho = {0, "v_psycho", "0", "easter egg"};

// brand of graphics chip
const char *gl_vendor;
// graphics chip model and other information
const char *gl_renderer;
// begins with 1.0.0, 1.1.0, 1.2.0, 1.2.1, 1.3.0, 1.3.1, or 1.4.0
const char *gl_version;
// extensions list, space separated
const char *gl_extensions;
// WGL, GLX, or AGL
const char *gl_platform;
// another extensions list, containing platform-specific extensions that are
// not in the main list
const char *gl_platformextensions;
// name of driver library (opengl32.dll, libGL.so.1, or whatever)
char gl_driver[256];

// GL_ARB_multitexture
void (GLAPIENTRY *qglMultiTexCoord1f) (GLenum, GLfloat);
void (GLAPIENTRY *qglMultiTexCoord2f) (GLenum, GLfloat, GLfloat);
void (GLAPIENTRY *qglMultiTexCoord3f) (GLenum, GLfloat, GLfloat, GLfloat);
void (GLAPIENTRY *qglMultiTexCoord4f) (GLenum, GLfloat, GLfloat, GLfloat, GLfloat);
void (GLAPIENTRY *qglActiveTexture) (GLenum);
void (GLAPIENTRY *qglClientActiveTexture) (GLenum);

// general GL functions

void (GLAPIENTRY *qglClearColor)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);

void (GLAPIENTRY *qglClear)(GLbitfield mask);

void (GLAPIENTRY *qglAlphaFunc)(GLenum func, GLclampf ref);
void (GLAPIENTRY *qglBlendFunc)(GLenum sfactor, GLenum dfactor);
void (GLAPIENTRY *qglCullFace)(GLenum mode);

void (GLAPIENTRY *qglDrawBuffer)(GLenum mode);
void (GLAPIENTRY *qglReadBuffer)(GLenum mode);
void (GLAPIENTRY *qglEnable)(GLenum cap);
void (GLAPIENTRY *qglDisable)(GLenum cap);
GLboolean (GLAPIENTRY *qglIsEnabled)(GLenum cap);

void (GLAPIENTRY *qglEnableClientState)(GLenum cap);
void (GLAPIENTRY *qglDisableClientState)(GLenum cap);

void (GLAPIENTRY *qglGetBooleanv)(GLenum pname, GLboolean *params);
void (GLAPIENTRY *qglGetDoublev)(GLenum pname, GLdouble *params);
void (GLAPIENTRY *qglGetFloatv)(GLenum pname, GLfloat *params);
void (GLAPIENTRY *qglGetIntegerv)(GLenum pname, GLint *params);

GLenum (GLAPIENTRY *qglGetError)(void);
const GLubyte* (GLAPIENTRY *qglGetString)(GLenum name);
void (GLAPIENTRY *qglFinish)(void);
void (GLAPIENTRY *qglFlush)(void);

void (GLAPIENTRY *qglClearDepth)(GLclampd depth);
void (GLAPIENTRY *qglDepthFunc)(GLenum func);
void (GLAPIENTRY *qglDepthMask)(GLboolean flag);
void (GLAPIENTRY *qglDepthRange)(GLclampd near_val, GLclampd far_val);
void (GLAPIENTRY *qglColorMask)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);

void (GLAPIENTRY *qglDrawRangeElements)(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices);
void (GLAPIENTRY *qglDrawElements)(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
void (GLAPIENTRY *qglDrawArrays)(GLenum mode, GLint first, GLsizei count);
void (GLAPIENTRY *qglVertexPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
void (GLAPIENTRY *qglNormalPointer)(GLenum type, GLsizei stride, const GLvoid *ptr);
void (GLAPIENTRY *qglColorPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
void (GLAPIENTRY *qglTexCoordPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
void (GLAPIENTRY *qglArrayElement)(GLint i);

void (GLAPIENTRY *qglColor4ub)(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha);
void (GLAPIENTRY *qglColor4f)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void (GLAPIENTRY *qglTexCoord1f)(GLfloat s);
void (GLAPIENTRY *qglTexCoord2f)(GLfloat s, GLfloat t);
void (GLAPIENTRY *qglTexCoord3f)(GLfloat s, GLfloat t, GLfloat r);
void (GLAPIENTRY *qglTexCoord4f)(GLfloat s, GLfloat t, GLfloat r, GLfloat q);
void (GLAPIENTRY *qglVertex2f)(GLfloat x, GLfloat y);
void (GLAPIENTRY *qglVertex3f)(GLfloat x, GLfloat y, GLfloat z);
void (GLAPIENTRY *qglVertex4f)(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void (GLAPIENTRY *qglBegin)(GLenum mode);
void (GLAPIENTRY *qglEnd)(void);

void (GLAPIENTRY *qglMatrixMode)(GLenum mode);
//void (GLAPIENTRY *qglOrtho)(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
//void (GLAPIENTRY *qglFrustum)(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
void (GLAPIENTRY *qglViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
//void (GLAPIENTRY *qglPushMatrix)(void);
//void (GLAPIENTRY *qglPopMatrix)(void);
void (GLAPIENTRY *qglLoadIdentity)(void);
//void (GLAPIENTRY *qglLoadMatrixd)(const GLdouble *m);
void (GLAPIENTRY *qglLoadMatrixf)(const GLfloat *m);
//void (GLAPIENTRY *qglMultMatrixd)(const GLdouble *m);
//void (GLAPIENTRY *qglMultMatrixf)(const GLfloat *m);
//void (GLAPIENTRY *qglRotated)(GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
//void (GLAPIENTRY *qglRotatef)(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
//void (GLAPIENTRY *qglScaled)(GLdouble x, GLdouble y, GLdouble z);
//void (GLAPIENTRY *qglScalef)(GLfloat x, GLfloat y, GLfloat z);
//void (GLAPIENTRY *qglTranslated)(GLdouble x, GLdouble y, GLdouble z);
//void (GLAPIENTRY *qglTranslatef)(GLfloat x, GLfloat y, GLfloat z);

void (GLAPIENTRY *qglReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);

void (GLAPIENTRY *qglStencilFunc)(GLenum func, GLint ref, GLuint mask);
void (GLAPIENTRY *qglStencilMask)(GLuint mask);
void (GLAPIENTRY *qglStencilOp)(GLenum fail, GLenum zfail, GLenum zpass);
void (GLAPIENTRY *qglClearStencil)(GLint s);

void (GLAPIENTRY *qglTexEnvf)(GLenum target, GLenum pname, GLfloat param);
void (GLAPIENTRY *qglTexEnvfv)(GLenum target, GLenum pname, const GLfloat *params);
void (GLAPIENTRY *qglTexEnvi)(GLenum target, GLenum pname, GLint param);
void (GLAPIENTRY *qglTexParameterf)(GLenum target, GLenum pname, GLfloat param);
void (GLAPIENTRY *qglTexParameterfv)(GLenum target, GLenum pname, GLfloat *params);
void (GLAPIENTRY *qglTexParameteri)(GLenum target, GLenum pname, GLint param);
void (GLAPIENTRY *qglGetTexParameterfv)(GLenum target, GLenum pname, GLfloat *params);
void (GLAPIENTRY *qglGetTexParameteriv)(GLenum target, GLenum pname, GLint *params);
void (GLAPIENTRY *qglGetTexLevelParameterfv)(GLenum target, GLint level, GLenum pname, GLfloat *params);
void (GLAPIENTRY *qglGetTexLevelParameteriv)(GLenum target, GLint level, GLenum pname, GLint *params);
void (GLAPIENTRY *qglGetTexImage)(GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels);
void (GLAPIENTRY *qglHint)(GLenum target, GLenum mode);

void (GLAPIENTRY *qglGenTextures)(GLsizei n, GLuint *textures);
void (GLAPIENTRY *qglDeleteTextures)(GLsizei n, const GLuint *textures);
void (GLAPIENTRY *qglBindTexture)(GLenum target, GLuint texture);
//void (GLAPIENTRY *qglPrioritizeTextures)(GLsizei n, const GLuint *textures, const GLclampf *priorities);
//GLboolean (GLAPIENTRY *qglAreTexturesResident)(GLsizei n, const GLuint *textures, GLboolean *residences);
//GLboolean (GLAPIENTRY *qglIsTexture)(GLuint texture);
//void (GLAPIENTRY *qglPixelStoref)(GLenum pname, GLfloat param);
void (GLAPIENTRY *qglPixelStorei)(GLenum pname, GLint param);

//void (GLAPIENTRY *qglTexImage1D)(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void (GLAPIENTRY *qglTexImage2D)(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
//void (GLAPIENTRY *qglTexSubImage1D)(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels);
void (GLAPIENTRY *qglTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
//void (GLAPIENTRY *qglCopyTexImage1D)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border);
void (GLAPIENTRY *qglCopyTexImage2D)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
//void (GLAPIENTRY *qglCopyTexSubImage1D)(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
void (GLAPIENTRY *qglCopyTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);


void (GLAPIENTRY *qglDrawRangeElementsEXT)(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices);

//void (GLAPIENTRY *qglColorTableEXT)(int, int, int, int, int, const void *);

void (GLAPIENTRY *qglTexImage3D)(GLenum target, GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void (GLAPIENTRY *qglTexSubImage3D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels);
void (GLAPIENTRY *qglCopyTexSubImage3D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);

void (GLAPIENTRY *qglScissor)(GLint x, GLint y, GLsizei width, GLsizei height);

void (GLAPIENTRY *qglPolygonOffset)(GLfloat factor, GLfloat units);
void (GLAPIENTRY *qglPolygonMode)(GLenum face, GLenum mode);
void (GLAPIENTRY *qglPolygonStipple)(const GLubyte *mask);

//void (GLAPIENTRY *qglClipPlane)(GLenum plane, const GLdouble *equation);
//void (GLAPIENTRY *qglGetClipPlane)(GLenum plane, GLdouble *equation);

//[515]: added on 29.07.2005
void (GLAPIENTRY *qglLineWidth)(GLfloat width);
void (GLAPIENTRY *qglPointSize)(GLfloat size);

void (GLAPIENTRY *qglBlendEquationEXT)(GLenum);

void (GLAPIENTRY *qglStencilOpSeparate)(GLenum, GLenum, GLenum, GLenum);
void (GLAPIENTRY *qglStencilFuncSeparate)(GLenum, GLenum, GLint, GLuint);
void (GLAPIENTRY *qglActiveStencilFaceEXT)(GLenum);

void (GLAPIENTRY *qglDeleteShader)(GLuint obj);
void (GLAPIENTRY *qglDeleteProgram)(GLuint obj);
//GLuint (GLAPIENTRY *qglGetHandle)(GLenum pname);
void (GLAPIENTRY *qglDetachShader)(GLuint containerObj, GLuint attachedObj);
GLuint (GLAPIENTRY *qglCreateShader)(GLenum shaderType);
void (GLAPIENTRY *qglShaderSource)(GLuint shaderObj, GLsizei count, const GLchar **string, const GLint *length);
void (GLAPIENTRY *qglCompileShader)(GLuint shaderObj);
GLuint (GLAPIENTRY *qglCreateProgram)(void);
void (GLAPIENTRY *qglAttachShader)(GLuint containerObj, GLuint obj);
void (GLAPIENTRY *qglLinkProgram)(GLuint programObj);
void (GLAPIENTRY *qglUseProgram)(GLuint programObj);
void (GLAPIENTRY *qglValidateProgram)(GLuint programObj);
void (GLAPIENTRY *qglUniform1f)(GLint location, GLfloat v0);
void (GLAPIENTRY *qglUniform2f)(GLint location, GLfloat v0, GLfloat v1);
void (GLAPIENTRY *qglUniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
void (GLAPIENTRY *qglUniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
void (GLAPIENTRY *qglUniform1i)(GLint location, GLint v0);
void (GLAPIENTRY *qglUniform2i)(GLint location, GLint v0, GLint v1);
void (GLAPIENTRY *qglUniform3i)(GLint location, GLint v0, GLint v1, GLint v2);
void (GLAPIENTRY *qglUniform4i)(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
void (GLAPIENTRY *qglUniform1fv)(GLint location, GLsizei count, const GLfloat *value);
void (GLAPIENTRY *qglUniform2fv)(GLint location, GLsizei count, const GLfloat *value);
void (GLAPIENTRY *qglUniform3fv)(GLint location, GLsizei count, const GLfloat *value);
void (GLAPIENTRY *qglUniform4fv)(GLint location, GLsizei count, const GLfloat *value);
void (GLAPIENTRY *qglUniform1iv)(GLint location, GLsizei count, const GLint *value);
void (GLAPIENTRY *qglUniform2iv)(GLint location, GLsizei count, const GLint *value);
void (GLAPIENTRY *qglUniform3iv)(GLint location, GLsizei count, const GLint *value);
void (GLAPIENTRY *qglUniform4iv)(GLint location, GLsizei count, const GLint *value);
void (GLAPIENTRY *qglUniformMatrix2fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void (GLAPIENTRY *qglUniformMatrix3fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void (GLAPIENTRY *qglUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void (GLAPIENTRY *qglGetShaderiv)(GLuint obj, GLenum pname, GLint *params);
void (GLAPIENTRY *qglGetProgramiv)(GLuint obj, GLenum pname, GLint *params);
void (GLAPIENTRY *qglGetShaderInfoLog)(GLuint obj, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
void (GLAPIENTRY *qglGetProgramInfoLog)(GLuint obj, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
void (GLAPIENTRY *qglGetAttachedShaders)(GLuint containerObj, GLsizei maxCount, GLsizei *count, GLuint *obj);
GLint (GLAPIENTRY *qglGetUniformLocation)(GLuint programObj, const GLchar *name);
void (GLAPIENTRY *qglGetActiveUniform)(GLuint programObj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
void (GLAPIENTRY *qglGetUniformfv)(GLuint programObj, GLint location, GLfloat *params);
void (GLAPIENTRY *qglGetUniformiv)(GLuint programObj, GLint location, GLint *params);
void (GLAPIENTRY *qglGetShaderSource)(GLuint obj, GLsizei maxLength, GLsizei *length, GLchar *source);

void (GLAPIENTRY *qglVertexAttrib1f)(GLuint index, GLfloat v0);
void (GLAPIENTRY *qglVertexAttrib1s)(GLuint index, GLshort v0);
void (GLAPIENTRY *qglVertexAttrib1d)(GLuint index, GLdouble v0);
void (GLAPIENTRY *qglVertexAttrib2f)(GLuint index, GLfloat v0, GLfloat v1);
void (GLAPIENTRY *qglVertexAttrib2s)(GLuint index, GLshort v0, GLshort v1);
void (GLAPIENTRY *qglVertexAttrib2d)(GLuint index, GLdouble v0, GLdouble v1);
void (GLAPIENTRY *qglVertexAttrib3f)(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2);
void (GLAPIENTRY *qglVertexAttrib3s)(GLuint index, GLshort v0, GLshort v1, GLshort v2);
void (GLAPIENTRY *qglVertexAttrib3d)(GLuint index, GLdouble v0, GLdouble v1, GLdouble v2);
void (GLAPIENTRY *qglVertexAttrib4f)(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
void (GLAPIENTRY *qglVertexAttrib4s)(GLuint index, GLshort v0, GLshort v1, GLshort v2, GLshort v3);
void (GLAPIENTRY *qglVertexAttrib4d)(GLuint index, GLdouble v0, GLdouble v1, GLdouble v2, GLdouble v3);
void (GLAPIENTRY *qglVertexAttrib4Nub)(GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
void (GLAPIENTRY *qglVertexAttrib1fv)(GLuint index, const GLfloat *v);
void (GLAPIENTRY *qglVertexAttrib1sv)(GLuint index, const GLshort *v);
void (GLAPIENTRY *qglVertexAttrib1dv)(GLuint index, const GLdouble *v);
void (GLAPIENTRY *qglVertexAttrib2fv)(GLuint index, const GLfloat *v);
void (GLAPIENTRY *qglVertexAttrib2sv)(GLuint index, const GLshort *v);
void (GLAPIENTRY *qglVertexAttrib2dv)(GLuint index, const GLdouble *v);
void (GLAPIENTRY *qglVertexAttrib3fv)(GLuint index, const GLfloat *v);
void (GLAPIENTRY *qglVertexAttrib3sv)(GLuint index, const GLshort *v);
void (GLAPIENTRY *qglVertexAttrib3dv)(GLuint index, const GLdouble *v);
void (GLAPIENTRY *qglVertexAttrib4fv)(GLuint index, const GLfloat *v);
void (GLAPIENTRY *qglVertexAttrib4sv)(GLuint index, const GLshort *v);
void (GLAPIENTRY *qglVertexAttrib4dv)(GLuint index, const GLdouble *v);
void (GLAPIENTRY *qglVertexAttrib4iv)(GLuint index, const GLint *v);
void (GLAPIENTRY *qglVertexAttrib4bv)(GLuint index, const GLbyte *v);
void (GLAPIENTRY *qglVertexAttrib4ubv)(GLuint index, const GLubyte *v);
void (GLAPIENTRY *qglVertexAttrib4usv)(GLuint index, const GLushort *v);
void (GLAPIENTRY *qglVertexAttrib4uiv)(GLuint index, const GLuint *v);
void (GLAPIENTRY *qglVertexAttrib4Nbv)(GLuint index, const GLbyte *v);
void (GLAPIENTRY *qglVertexAttrib4Nsv)(GLuint index, const GLshort *v);
void (GLAPIENTRY *qglVertexAttrib4Niv)(GLuint index, const GLint *v);
void (GLAPIENTRY *qglVertexAttrib4Nubv)(GLuint index, const GLubyte *v);
void (GLAPIENTRY *qglVertexAttrib4Nusv)(GLuint index, const GLushort *v);
void (GLAPIENTRY *qglVertexAttrib4Nuiv)(GLuint index, const GLuint *v);
void (GLAPIENTRY *qglVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);
void (GLAPIENTRY *qglEnableVertexAttribArray)(GLuint index);
void (GLAPIENTRY *qglDisableVertexAttribArray)(GLuint index);
void (GLAPIENTRY *qglBindAttribLocation)(GLuint programObj, GLuint index, const GLchar *name);
void (GLAPIENTRY *qglBindFragDataLocation)(GLuint programObj, GLuint index, const GLchar *name);
void (GLAPIENTRY *qglGetActiveAttrib)(GLuint programObj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
GLint (GLAPIENTRY *qglGetAttribLocation)(GLuint programObj, const GLchar *name);
void (GLAPIENTRY *qglGetVertexAttribdv)(GLuint index, GLenum pname, GLdouble *params);
void (GLAPIENTRY *qglGetVertexAttribfv)(GLuint index, GLenum pname, GLfloat *params);
void (GLAPIENTRY *qglGetVertexAttribiv)(GLuint index, GLenum pname, GLint *params);
void (GLAPIENTRY *qglGetVertexAttribPointerv)(GLuint index, GLenum pname, GLvoid **pointer);

//GL_ARB_vertex_buffer_object
void (GLAPIENTRY *qglBindBufferARB) (GLenum target, GLuint buffer);
void (GLAPIENTRY *qglDeleteBuffersARB) (GLsizei n, const GLuint *buffers);
void (GLAPIENTRY *qglGenBuffersARB) (GLsizei n, GLuint *buffers);
GLboolean (GLAPIENTRY *qglIsBufferARB) (GLuint buffer);
GLvoid* (GLAPIENTRY *qglMapBufferARB) (GLenum target, GLenum access);
GLboolean (GLAPIENTRY *qglUnmapBufferARB) (GLenum target);
void (GLAPIENTRY *qglBufferDataARB) (GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage);
void (GLAPIENTRY *qglBufferSubDataARB) (GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid *data);

//GL_EXT_framebuffer_object
GLboolean (GLAPIENTRY *qglIsRenderbufferEXT)(GLuint renderbuffer);
void (GLAPIENTRY *qglBindRenderbufferEXT)(GLenum target, GLuint renderbuffer);
void (GLAPIENTRY *qglDeleteRenderbuffersEXT)(GLsizei n, const GLuint *renderbuffers);
void (GLAPIENTRY *qglGenRenderbuffersEXT)(GLsizei n, GLuint *renderbuffers);
void (GLAPIENTRY *qglRenderbufferStorageEXT)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
void (GLAPIENTRY *qglGetRenderbufferParameterivEXT)(GLenum target, GLenum pname, GLint *params);
GLboolean (GLAPIENTRY *qglIsFramebufferEXT)(GLuint framebuffer);
void (GLAPIENTRY *qglBindFramebufferEXT)(GLenum target, GLuint framebuffer);
void (GLAPIENTRY *qglDeleteFramebuffersEXT)(GLsizei n, const GLuint *framebuffers);
void (GLAPIENTRY *qglGenFramebuffersEXT)(GLsizei n, GLuint *framebuffers);
GLenum (GLAPIENTRY *qglCheckFramebufferStatusEXT)(GLenum target);
//void (GLAPIENTRY *qglFramebufferTexture1DEXT)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
void (GLAPIENTRY *qglFramebufferTexture2DEXT)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
void (GLAPIENTRY *qglFramebufferTexture3DEXT)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset);
void (GLAPIENTRY *qglFramebufferRenderbufferEXT)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
void (GLAPIENTRY *qglGetFramebufferAttachmentParameterivEXT)(GLenum target, GLenum attachment, GLenum pname, GLint *params);
void (GLAPIENTRY *qglGenerateMipmapEXT)(GLenum target);

void (GLAPIENTRY *qglDrawBuffersARB)(GLsizei n, const GLenum *bufs);

void (GLAPIENTRY *qglCompressedTexImage3DARB)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *data);
void (GLAPIENTRY *qglCompressedTexImage2DARB)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border,  GLsizei imageSize, const void *data);
//void (GLAPIENTRY *qglCompressedTexImage1DARB)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void *data);
void (GLAPIENTRY *qglCompressedTexSubImage3DARB)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data);
void (GLAPIENTRY *qglCompressedTexSubImage2DARB)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data);
//void (GLAPIENTRY *qglCompressedTexSubImage1DARB)(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *data);
void (GLAPIENTRY *qglGetCompressedTexImageARB)(GLenum target, GLint lod, void *img);

void (GLAPIENTRY *qglGenQueriesARB)(GLsizei n, GLuint *ids);
void (GLAPIENTRY *qglDeleteQueriesARB)(GLsizei n, const GLuint *ids);
GLboolean (GLAPIENTRY *qglIsQueryARB)(GLuint qid);
void (GLAPIENTRY *qglBeginQueryARB)(GLenum target, GLuint qid);
void (GLAPIENTRY *qglEndQueryARB)(GLenum target);
void (GLAPIENTRY *qglGetQueryivARB)(GLenum target, GLenum pname, GLint *params);
void (GLAPIENTRY *qglGetQueryObjectivARB)(GLuint qid, GLenum pname, GLint *params);
void (GLAPIENTRY *qglGetQueryObjectuivARB)(GLuint qid, GLenum pname, GLuint *params);

void (GLAPIENTRY *qglSampleCoverageARB)(GLclampf value, GLboolean invert);

#if _MSC_VER >= 1400
#define sscanf sscanf_s
#endif

qboolean GL_CheckExtension(const char *minglver_or_ext, const dllfunction_t *funcs, const char *disableparm, int silent)
{
	int failed = false;
	const dllfunction_t *func;
	struct { int major, minor; } min_version, curr_version;
	char extstr[MAX_INPUTLINE];
	int ext;

	if(sscanf(minglver_or_ext, "%d.%d", &min_version.major, &min_version.minor) == 2)
		ext = 0; // opengl version
	else if(minglver_or_ext[0] != toupper(minglver_or_ext[0]))
		ext = -1; // pseudo name
	else
		ext = 1; // extension name

	if (ext)
		Con_DPrintf("checking for %s...  ", minglver_or_ext);
	else
		Con_DPrintf("checking for OpenGL %s core features...  ", minglver_or_ext);

	for (func = funcs;func && func->name;func++)
		*func->funcvariable = NULL;

	if (disableparm && (COM_CheckParm(disableparm) || COM_CheckParm("-safe")))
	{
		Con_DPrint("disabled by commandline\n");
		return false;
	}

	if (ext == 1) // opengl extension
	{
		if (!strstr(gl_extensions ? gl_extensions : "", minglver_or_ext) && !strstr(gl_platformextensions ? gl_platformextensions : "", minglver_or_ext))
		{
			Con_DPrint("not detected\n");
			return false;
		}
	}

	if(ext == 0) // opengl version
	{
		if (sscanf(gl_version, "%d.%d", &curr_version.major, &curr_version.minor) < 2)
			curr_version.major = curr_version.minor = 1;

		if (curr_version.major < min_version.major || (curr_version.major == min_version.major && curr_version.minor < min_version.minor))
		{
			Con_DPrintf("not detected (OpenGL %d.%d loaded)\n", curr_version.major, curr_version.minor);
			return false;
		}
	}

	for (func = funcs;func && func->name != NULL;func++)
	{
		// Con_DPrintf("\n    %s...  ", func->name);

		// functions are cleared before all the extensions are evaluated
		if (!(*func->funcvariable = (void *) GL_GetProcAddress(func->name)))
		{
			if (ext && !silent)
				Con_DPrintf("%s is missing function \"%s\" - broken driver!\n", minglver_or_ext, func->name);
			if (!ext)
				Con_Printf("OpenGL %s core features are missing function \"%s\" - broken driver!\n", minglver_or_ext, func->name);
			failed = true;
		}
	}
	// delay the return so it prints all missing functions
	if (failed)
		return false;
	// VorteX: add to found extension list
	dpsnprintf(extstr, sizeof(extstr), "%s %s ", gl_info_extensions.string, minglver_or_ext);
	Cvar_SetQuick(&gl_info_extensions, extstr);

	Con_DPrint("enabled\n");
	return true;
}

static dllfunction_t opengl110funcs[] =
{
	{"glClearColor", (void **) &qglClearColor},
	{"glClear", (void **) &qglClear},
	{"glAlphaFunc", (void **) &qglAlphaFunc},
	{"glBlendFunc", (void **) &qglBlendFunc},
	{"glCullFace", (void **) &qglCullFace},
	{"glDrawBuffer", (void **) &qglDrawBuffer},
	{"glReadBuffer", (void **) &qglReadBuffer},
	{"glEnable", (void **) &qglEnable},
	{"glDisable", (void **) &qglDisable},
	{"glIsEnabled", (void **) &qglIsEnabled},
	{"glEnableClientState", (void **) &qglEnableClientState},
	{"glDisableClientState", (void **) &qglDisableClientState},
	{"glGetBooleanv", (void **) &qglGetBooleanv},
	{"glGetDoublev", (void **) &qglGetDoublev},
	{"glGetFloatv", (void **) &qglGetFloatv},
	{"glGetIntegerv", (void **) &qglGetIntegerv},
	{"glGetError", (void **) &qglGetError},
	{"glGetString", (void **) &qglGetString},
	{"glFinish", (void **) &qglFinish},
	{"glFlush", (void **) &qglFlush},
	{"glClearDepth", (void **) &qglClearDepth},
	{"glDepthFunc", (void **) &qglDepthFunc},
	{"glDepthMask", (void **) &qglDepthMask},
	{"glDepthRange", (void **) &qglDepthRange},
	{"glDrawElements", (void **) &qglDrawElements},
	{"glDrawArrays", (void **) &qglDrawArrays},
	{"glColorMask", (void **) &qglColorMask},
	{"glVertexPointer", (void **) &qglVertexPointer},
	{"glNormalPointer", (void **) &qglNormalPointer},
	{"glColorPointer", (void **) &qglColorPointer},
	{"glTexCoordPointer", (void **) &qglTexCoordPointer},
	{"glArrayElement", (void **) &qglArrayElement},
	{"glColor4ub", (void **) &qglColor4ub},
	{"glColor4f", (void **) &qglColor4f},
	{"glTexCoord1f", (void **) &qglTexCoord1f},
	{"glTexCoord2f", (void **) &qglTexCoord2f},
	{"glTexCoord3f", (void **) &qglTexCoord3f},
	{"glTexCoord4f", (void **) &qglTexCoord4f},
	{"glVertex2f", (void **) &qglVertex2f},
	{"glVertex3f", (void **) &qglVertex3f},
	{"glVertex4f", (void **) &qglVertex4f},
	{"glBegin", (void **) &qglBegin},
	{"glEnd", (void **) &qglEnd},
//[515]: added on 29.07.2005
	{"glLineWidth", (void**) &qglLineWidth},
	{"glPointSize", (void**) &qglPointSize},
//
	{"glMatrixMode", (void **) &qglMatrixMode},
//	{"glOrtho", (void **) &qglOrtho},
//	{"glFrustum", (void **) &qglFrustum},
	{"glViewport", (void **) &qglViewport},
//	{"glPushMatrix", (void **) &qglPushMatrix},
//	{"glPopMatrix", (void **) &qglPopMatrix},
	{"glLoadIdentity", (void **) &qglLoadIdentity},
//	{"glLoadMatrixd", (void **) &qglLoadMatrixd},
	{"glLoadMatrixf", (void **) &qglLoadMatrixf},
//	{"glMultMatrixd", (void **) &qglMultMatrixd},
//	{"glMultMatrixf", (void **) &qglMultMatrixf},
//	{"glRotated", (void **) &qglRotated},
//	{"glRotatef", (void **) &qglRotatef},
//	{"glScaled", (void **) &qglScaled},
//	{"glScalef", (void **) &qglScalef},
//	{"glTranslated", (void **) &qglTranslated},
//	{"glTranslatef", (void **) &qglTranslatef},
	{"glReadPixels", (void **) &qglReadPixels},
	{"glStencilFunc", (void **) &qglStencilFunc},
	{"glStencilMask", (void **) &qglStencilMask},
	{"glStencilOp", (void **) &qglStencilOp},
	{"glClearStencil", (void **) &qglClearStencil},
	{"glTexEnvf", (void **) &qglTexEnvf},
	{"glTexEnvfv", (void **) &qglTexEnvfv},
	{"glTexEnvi", (void **) &qglTexEnvi},
	{"glTexParameterf", (void **) &qglTexParameterf},
	{"glTexParameterfv", (void **) &qglTexParameterfv},
	{"glTexParameteri", (void **) &qglTexParameteri},
	{"glGetTexImage", (void **) &qglGetTexImage},
	{"glGetTexParameterfv", (void **) &qglGetTexParameterfv},
	{"glGetTexParameteriv", (void **) &qglGetTexParameteriv},
	{"glGetTexLevelParameterfv", (void **) &qglGetTexLevelParameterfv},
	{"glGetTexLevelParameteriv", (void **) &qglGetTexLevelParameteriv},
	{"glHint", (void **) &qglHint},
//	{"glPixelStoref", (void **) &qglPixelStoref},
	{"glPixelStorei", (void **) &qglPixelStorei},
	{"glGenTextures", (void **) &qglGenTextures},
	{"glDeleteTextures", (void **) &qglDeleteTextures},
	{"glBindTexture", (void **) &qglBindTexture},
//	{"glPrioritizeTextures", (void **) &qglPrioritizeTextures},
//	{"glAreTexturesResident", (void **) &qglAreTexturesResident},
//	{"glIsTexture", (void **) &qglIsTexture},
//	{"glTexImage1D", (void **) &qglTexImage1D},
	{"glTexImage2D", (void **) &qglTexImage2D},
//	{"glTexSubImage1D", (void **) &qglTexSubImage1D},
	{"glTexSubImage2D", (void **) &qglTexSubImage2D},
//	{"glCopyTexImage1D", (void **) &qglCopyTexImage1D},
	{"glCopyTexImage2D", (void **) &qglCopyTexImage2D},
//	{"glCopyTexSubImage1D", (void **) &qglCopyTexSubImage1D},
	{"glCopyTexSubImage2D", (void **) &qglCopyTexSubImage2D},
	{"glScissor", (void **) &qglScissor},
	{"glPolygonOffset", (void **) &qglPolygonOffset},
	{"glPolygonMode", (void **) &qglPolygonMode},
	{"glPolygonStipple", (void **) &qglPolygonStipple},
//	{"glClipPlane", (void **) &qglClipPlane},
//	{"glGetClipPlane", (void **) &qglGetClipPlane},
	{NULL, NULL}
};

static dllfunction_t drawrangeelementsfuncs[] =
{
	{"glDrawRangeElements", (void **) &qglDrawRangeElements},
	{NULL, NULL}
};

static dllfunction_t drawrangeelementsextfuncs[] =
{
	{"glDrawRangeElementsEXT", (void **) &qglDrawRangeElementsEXT},
	{NULL, NULL}
};

static dllfunction_t multitexturefuncs[] =
{
	{"glMultiTexCoord1fARB", (void **) &qglMultiTexCoord1f},
	{"glMultiTexCoord2fARB", (void **) &qglMultiTexCoord2f},
	{"glMultiTexCoord3fARB", (void **) &qglMultiTexCoord3f},
	{"glMultiTexCoord4fARB", (void **) &qglMultiTexCoord4f},
	{"glActiveTextureARB", (void **) &qglActiveTexture},
	{"glClientActiveTextureARB", (void **) &qglClientActiveTexture},
	{NULL, NULL}
};

static dllfunction_t texture3dextfuncs[] =
{
	{"glTexImage3DEXT", (void **) &qglTexImage3D},
	{"glTexSubImage3DEXT", (void **) &qglTexSubImage3D},
	{"glCopyTexSubImage3DEXT", (void **) &qglCopyTexSubImage3D},
	{NULL, NULL}
};

static dllfunction_t atiseparatestencilfuncs[] =
{
	{"glStencilOpSeparateATI", (void **) &qglStencilOpSeparate},
	{"glStencilFuncSeparateATI", (void **) &qglStencilFuncSeparate},
	{NULL, NULL}
};

static dllfunction_t gl2separatestencilfuncs[] =
{
	{"glStencilOpSeparate", (void **) &qglStencilOpSeparate},
	{"glStencilFuncSeparate", (void **) &qglStencilFuncSeparate},
	{NULL, NULL}
};

static dllfunction_t stenciltwosidefuncs[] =
{
	{"glActiveStencilFaceEXT", (void **) &qglActiveStencilFaceEXT},
	{NULL, NULL}
};

static dllfunction_t blendequationfuncs[] =
{
	{"glBlendEquationEXT", (void **) &qglBlendEquationEXT},
	{NULL, NULL}
};

static dllfunction_t gl20shaderfuncs[] =
{
	{"glDeleteShader", (void **) &qglDeleteShader},
	{"glDeleteProgram", (void **) &qglDeleteProgram},
//	{"glGetHandle", (void **) &qglGetHandle},
	{"glDetachShader", (void **) &qglDetachShader},
	{"glCreateShader", (void **) &qglCreateShader},
	{"glShaderSource", (void **) &qglShaderSource},
	{"glCompileShader", (void **) &qglCompileShader},
	{"glCreateProgram", (void **) &qglCreateProgram},
	{"glAttachShader", (void **) &qglAttachShader},
	{"glLinkProgram", (void **) &qglLinkProgram},
	{"glUseProgram", (void **) &qglUseProgram},
	{"glValidateProgram", (void **) &qglValidateProgram},
	{"glUniform1f", (void **) &qglUniform1f},
	{"glUniform2f", (void **) &qglUniform2f},
	{"glUniform3f", (void **) &qglUniform3f},
	{"glUniform4f", (void **) &qglUniform4f},
	{"glUniform1i", (void **) &qglUniform1i},
	{"glUniform2i", (void **) &qglUniform2i},
	{"glUniform3i", (void **) &qglUniform3i},
	{"glUniform4i", (void **) &qglUniform4i},
	{"glUniform1fv", (void **) &qglUniform1fv},
	{"glUniform2fv", (void **) &qglUniform2fv},
	{"glUniform3fv", (void **) &qglUniform3fv},
	{"glUniform4fv", (void **) &qglUniform4fv},
	{"glUniform1iv", (void **) &qglUniform1iv},
	{"glUniform2iv", (void **) &qglUniform2iv},
	{"glUniform3iv", (void **) &qglUniform3iv},
	{"glUniform4iv", (void **) &qglUniform4iv},
	{"glUniformMatrix2fv", (void **) &qglUniformMatrix2fv},
	{"glUniformMatrix3fv", (void **) &qglUniformMatrix3fv},
	{"glUniformMatrix4fv", (void **) &qglUniformMatrix4fv},
	{"glGetShaderiv", (void **) &qglGetShaderiv},
	{"glGetProgramiv", (void **) &qglGetProgramiv},
	{"glGetShaderInfoLog", (void **) &qglGetShaderInfoLog},
	{"glGetProgramInfoLog", (void **) &qglGetProgramInfoLog},
	{"glGetAttachedShaders", (void **) &qglGetAttachedShaders},
	{"glGetUniformLocation", (void **) &qglGetUniformLocation},
	{"glGetActiveUniform", (void **) &qglGetActiveUniform},
	{"glGetUniformfv", (void **) &qglGetUniformfv},
	{"glGetUniformiv", (void **) &qglGetUniformiv},
	{"glGetShaderSource", (void **) &qglGetShaderSource},
	{"glVertexAttrib1f", (void **) &qglVertexAttrib1f},
	{"glVertexAttrib1s", (void **) &qglVertexAttrib1s},
	{"glVertexAttrib1d", (void **) &qglVertexAttrib1d},
	{"glVertexAttrib2f", (void **) &qglVertexAttrib2f},
	{"glVertexAttrib2s", (void **) &qglVertexAttrib2s},
	{"glVertexAttrib2d", (void **) &qglVertexAttrib2d},
	{"glVertexAttrib3f", (void **) &qglVertexAttrib3f},
	{"glVertexAttrib3s", (void **) &qglVertexAttrib3s},
	{"glVertexAttrib3d", (void **) &qglVertexAttrib3d},
	{"glVertexAttrib4f", (void **) &qglVertexAttrib4f},
	{"glVertexAttrib4s", (void **) &qglVertexAttrib4s},
	{"glVertexAttrib4d", (void **) &qglVertexAttrib4d},
	{"glVertexAttrib4Nub", (void **) &qglVertexAttrib4Nub},
	{"glVertexAttrib1fv", (void **) &qglVertexAttrib1fv},
	{"glVertexAttrib1sv", (void **) &qglVertexAttrib1sv},
	{"glVertexAttrib1dv", (void **) &qglVertexAttrib1dv},
	{"glVertexAttrib2fv", (void **) &qglVertexAttrib1fv},
	{"glVertexAttrib2sv", (void **) &qglVertexAttrib1sv},
	{"glVertexAttrib2dv", (void **) &qglVertexAttrib1dv},
	{"glVertexAttrib3fv", (void **) &qglVertexAttrib1fv},
	{"glVertexAttrib3sv", (void **) &qglVertexAttrib1sv},
	{"glVertexAttrib3dv", (void **) &qglVertexAttrib1dv},
	{"glVertexAttrib4fv", (void **) &qglVertexAttrib1fv},
	{"glVertexAttrib4sv", (void **) &qglVertexAttrib1sv},
	{"glVertexAttrib4dv", (void **) &qglVertexAttrib1dv},
//	{"glVertexAttrib4iv", (void **) &qglVertexAttrib1iv},
//	{"glVertexAttrib4bv", (void **) &qglVertexAttrib1bv},
//	{"glVertexAttrib4ubv", (void **) &qglVertexAttrib1ubv},
//	{"glVertexAttrib4usv", (void **) &qglVertexAttrib1usv},
//	{"glVertexAttrib4uiv", (void **) &qglVertexAttrib1uiv},
//	{"glVertexAttrib4Nbv", (void **) &qglVertexAttrib1Nbv},
//	{"glVertexAttrib4Nsv", (void **) &qglVertexAttrib1Nsv},
//	{"glVertexAttrib4Niv", (void **) &qglVertexAttrib1Niv},
//	{"glVertexAttrib4Nubv", (void **) &qglVertexAttrib1Nubv},
//	{"glVertexAttrib4Nusv", (void **) &qglVertexAttrib1Nusv},
//	{"glVertexAttrib4Nuiv", (void **) &qglVertexAttrib1Nuiv},
	{"glVertexAttribPointer", (void **) &qglVertexAttribPointer},
	{"glEnableVertexAttribArray", (void **) &qglEnableVertexAttribArray},
	{"glDisableVertexAttribArray", (void **) &qglDisableVertexAttribArray},
	{"glBindAttribLocation", (void **) &qglBindAttribLocation},
	{"glGetActiveAttrib", (void **) &qglGetActiveAttrib},
	{"glGetAttribLocation", (void **) &qglGetAttribLocation},
	{"glGetVertexAttribdv", (void **) &qglGetVertexAttribdv},
	{"glGetVertexAttribfv", (void **) &qglGetVertexAttribfv},
	{"glGetVertexAttribiv", (void **) &qglGetVertexAttribiv},
	{"glGetVertexAttribPointerv", (void **) &qglGetVertexAttribPointerv},
	{NULL, NULL}
};

static dllfunction_t glsl130funcs[] =
{
	{"glBindFragDataLocation", (void **) &qglBindFragDataLocation},
	{NULL, NULL}
};

static dllfunction_t vbofuncs[] =
{
	{"glBindBufferARB"    , (void **) &qglBindBufferARB},
	{"glDeleteBuffersARB" , (void **) &qglDeleteBuffersARB},
	{"glGenBuffersARB"    , (void **) &qglGenBuffersARB},
	{"glIsBufferARB"      , (void **) &qglIsBufferARB},
	{"glMapBufferARB"     , (void **) &qglMapBufferARB},
	{"glUnmapBufferARB"   , (void **) &qglUnmapBufferARB},
	{"glBufferDataARB"    , (void **) &qglBufferDataARB},
	{"glBufferSubDataARB" , (void **) &qglBufferSubDataARB},
	{NULL, NULL}
};

static dllfunction_t fbofuncs[] =
{
	{"glIsRenderbufferEXT"                      , (void **) &qglIsRenderbufferEXT},
	{"glBindRenderbufferEXT"                    , (void **) &qglBindRenderbufferEXT},
	{"glDeleteRenderbuffersEXT"                 , (void **) &qglDeleteRenderbuffersEXT},
	{"glGenRenderbuffersEXT"                    , (void **) &qglGenRenderbuffersEXT},
	{"glRenderbufferStorageEXT"                 , (void **) &qglRenderbufferStorageEXT},
	{"glGetRenderbufferParameterivEXT"          , (void **) &qglGetRenderbufferParameterivEXT},
	{"glIsFramebufferEXT"                       , (void **) &qglIsFramebufferEXT},
	{"glBindFramebufferEXT"                     , (void **) &qglBindFramebufferEXT},
	{"glDeleteFramebuffersEXT"                  , (void **) &qglDeleteFramebuffersEXT},
	{"glGenFramebuffersEXT"                     , (void **) &qglGenFramebuffersEXT},
	{"glCheckFramebufferStatusEXT"              , (void **) &qglCheckFramebufferStatusEXT},
//	{"glFramebufferTexture1DEXT"                , (void **) &qglFramebufferTexture1DEXT},
	{"glFramebufferTexture2DEXT"                , (void **) &qglFramebufferTexture2DEXT},
	{"glFramebufferTexture3DEXT"                , (void **) &qglFramebufferTexture3DEXT},
	{"glFramebufferRenderbufferEXT"             , (void **) &qglFramebufferRenderbufferEXT},
	{"glGetFramebufferAttachmentParameterivEXT" , (void **) &qglGetFramebufferAttachmentParameterivEXT},
	{"glGenerateMipmapEXT"                      , (void **) &qglGenerateMipmapEXT},
	{NULL, NULL}
};

static dllfunction_t texturecompressionfuncs[] =
{
	{"glCompressedTexImage3DARB",    (void **) &qglCompressedTexImage3DARB},
	{"glCompressedTexImage2DARB",    (void **) &qglCompressedTexImage2DARB},
//	{"glCompressedTexImage1DARB",    (void **) &qglCompressedTexImage1DARB},
	{"glCompressedTexSubImage3DARB", (void **) &qglCompressedTexSubImage3DARB},
	{"glCompressedTexSubImage2DARB", (void **) &qglCompressedTexSubImage2DARB},
//	{"glCompressedTexSubImage1DARB", (void **) &qglCompressedTexSubImage1DARB},
	{"glGetCompressedTexImageARB",   (void **) &qglGetCompressedTexImageARB},
	{NULL, NULL}
};

static dllfunction_t occlusionqueryfuncs[] =
{
	{"glGenQueriesARB",              (void **) &qglGenQueriesARB},
	{"glDeleteQueriesARB",           (void **) &qglDeleteQueriesARB},
	{"glIsQueryARB",                 (void **) &qglIsQueryARB},
	{"glBeginQueryARB",              (void **) &qglBeginQueryARB},
	{"glEndQueryARB",                (void **) &qglEndQueryARB},
	{"glGetQueryivARB",              (void **) &qglGetQueryivARB},
	{"glGetQueryObjectivARB",        (void **) &qglGetQueryObjectivARB},
	{"glGetQueryObjectuivARB",       (void **) &qglGetQueryObjectuivARB},
	{NULL, NULL}
};

static dllfunction_t drawbuffersfuncs[] =
{
	{"glDrawBuffersARB",             (void **) &qglDrawBuffersARB},
	{NULL, NULL}
};

static dllfunction_t multisamplefuncs[] =
{
	{"glSampleCoverageARB",          (void **) &qglSampleCoverageARB},
	{NULL, NULL}
};

void VID_ClearExtensions(void)
{
	// VorteX: reset extensions info cvar, it got filled by GL_CheckExtension
	Cvar_SetQuick(&gl_info_extensions, "");

	// clear the extension flags
	memset(&vid.support, 0, sizeof(vid.support));
	vid.renderpath = RENDERPATH_GL11;
	vid.sRGBcapable2D = false;
	vid.sRGBcapable3D = false;
	vid.useinterleavedarrays = false;
	vid.forcevbo = false;
	vid.maxtexturesize_2d = 0;
	vid.maxtexturesize_3d = 0;
	vid.maxtexturesize_cubemap = 0;
	vid.texunits = 1;
	vid.teximageunits = 1;
	vid.texarrayunits = 1;
	vid.max_anisotropy = 1;
	vid.maxdrawbuffers = 1;

	// this is a complete list of all functions that are directly checked in the renderer
	qglDrawRangeElements = NULL;
	qglDrawBuffer = NULL;
	qglPolygonStipple = NULL;
	qglFlush = NULL;
	qglActiveTexture = NULL;
	qglGetCompressedTexImageARB = NULL;
	qglFramebufferTexture2DEXT = NULL;
	qglDrawBuffersARB = NULL;
}

void VID_CheckExtensions(void)
{
	if (!GL_CheckExtension("glbase", opengl110funcs, NULL, false))
		Sys_Error("OpenGL 1.1.0 functions not found");
	vid.support.gl20shaders = GL_CheckExtension("2.0", gl20shaderfuncs, "-noshaders", true);

	CHECKGLERROR

	Con_DPrint("Checking OpenGL extensions...\n");

	if (vid.support.gl20shaders)
	{
		// this one is purely optional, needed for GLSL 1.3 support (#version 130), so we don't even check the return value of GL_CheckExtension
		vid.support.gl20shaders130 = GL_CheckExtension("glshaders130", glsl130funcs, "-noglsl130", true);
		if(vid.support.gl20shaders130)
		{
			char *s = (char *) qglGetString(GL_SHADING_LANGUAGE_VERSION);
			if(!s || atof(s) < 1.30 - 0.00001)
				vid.support.gl20shaders130 = 0;
		}
		if(vid.support.gl20shaders130)
			Con_DPrintf("Using GLSL 1.30\n");
		else
			Con_DPrintf("Using GLSL 1.00\n");
	}

	// GL drivers generally prefer GL_BGRA
	vid.forcetextype = GL_BGRA;

	vid.support.amd_texture_texture4 = GL_CheckExtension("GL_AMD_texture_texture4", NULL, "-notexture4", false);
	vid.support.arb_depth_texture = GL_CheckExtension("GL_ARB_depth_texture", NULL, "-nodepthtexture", false);
	vid.support.arb_draw_buffers = GL_CheckExtension("GL_ARB_draw_buffers", drawbuffersfuncs, "-nodrawbuffers", false);
	vid.support.arb_multitexture = GL_CheckExtension("GL_ARB_multitexture", multitexturefuncs, "-nomtex", false);
	vid.support.arb_occlusion_query = GL_CheckExtension("GL_ARB_occlusion_query", occlusionqueryfuncs, "-noocclusionquery", false);
	vid.support.arb_shadow = GL_CheckExtension("GL_ARB_shadow", NULL, "-noshadow", false);
	vid.support.arb_texture_compression = GL_CheckExtension("GL_ARB_texture_compression", texturecompressionfuncs, "-notexturecompression", false);
	vid.support.arb_texture_cube_map = GL_CheckExtension("GL_ARB_texture_cube_map", NULL, "-nocubemap", false);
	vid.support.arb_texture_env_combine = GL_CheckExtension("GL_ARB_texture_env_combine", NULL, "-nocombine", false) || GL_CheckExtension("GL_EXT_texture_env_combine", NULL, "-nocombine", false);
	vid.support.arb_texture_gather = GL_CheckExtension("GL_ARB_texture_gather", NULL, "-notexturegather", false);
#ifndef __APPLE__
	// LordHavoc: too many bugs on OSX!
	vid.support.arb_texture_non_power_of_two = GL_CheckExtension("GL_ARB_texture_non_power_of_two", NULL, "-notexturenonpoweroftwo", false);
#endif
	vid.support.arb_vertex_buffer_object = GL_CheckExtension("GL_ARB_vertex_buffer_object", vbofuncs, "-novbo", false);
	vid.support.ati_separate_stencil = GL_CheckExtension("separatestencil", gl2separatestencilfuncs, "-noseparatestencil", true) || GL_CheckExtension("GL_ATI_separate_stencil", atiseparatestencilfuncs, "-noseparatestencil", false);
	vid.support.ext_blend_minmax = GL_CheckExtension("GL_EXT_blend_minmax", blendequationfuncs, "-noblendminmax", false);
	vid.support.ext_blend_subtract = GL_CheckExtension("GL_EXT_blend_subtract", blendequationfuncs, "-noblendsubtract", false);
	vid.support.ext_draw_range_elements = GL_CheckExtension("drawrangeelements", drawrangeelementsfuncs, "-nodrawrangeelements", true) || GL_CheckExtension("GL_EXT_draw_range_elements", drawrangeelementsextfuncs, "-nodrawrangeelements", false);
	vid.support.ext_framebuffer_object = GL_CheckExtension("GL_EXT_framebuffer_object", fbofuncs, "-nofbo", false);
	vid.support.ext_stencil_two_side = GL_CheckExtension("GL_EXT_stencil_two_side", stenciltwosidefuncs, "-nostenciltwoside", false);
	vid.support.ext_texture_3d = GL_CheckExtension("GL_EXT_texture3D", texture3dextfuncs, "-notexture3d", false);
	vid.support.ext_texture_compression_s3tc = GL_CheckExtension("GL_EXT_texture_compression_s3tc", NULL, "-nos3tc", false);
	vid.support.ext_texture_edge_clamp = GL_CheckExtension("GL_EXT_texture_edge_clamp", NULL, "-noedgeclamp", false) || GL_CheckExtension("GL_SGIS_texture_edge_clamp", NULL, "-noedgeclamp", false);
	vid.support.ext_texture_filter_anisotropic = GL_CheckExtension("GL_EXT_texture_filter_anisotropic", NULL, "-noanisotropy", false);
	vid.support.ext_texture_srgb = GL_CheckExtension("GL_EXT_texture_sRGB", NULL, "-nosrgb", false);
	vid.support.arb_multisample = GL_CheckExtension("GL_ARB_multisample", multisamplefuncs, "-nomultisample", false);
	vid.allowalphatocoverage = false;

// COMMANDLINEOPTION: GL: -noshaders disables use of OpenGL 2.0 shaders (which allow pixel shader effects, can improve per pixel lighting performance and capabilities)
// COMMANDLINEOPTION: GL: -noanisotropy disables GL_EXT_texture_filter_anisotropic (allows higher quality texturing)
// COMMANDLINEOPTION: GL: -noblendminmax disables GL_EXT_blend_minmax
// COMMANDLINEOPTION: GL: -noblendsubtract disables GL_EXT_blend_subtract
// COMMANDLINEOPTION: GL: -nocombine disables GL_ARB_texture_env_combine or GL_EXT_texture_env_combine (required for bumpmapping and faster map rendering)
// COMMANDLINEOPTION: GL: -nocubemap disables GL_ARB_texture_cube_map (required for bumpmapping)
// COMMANDLINEOPTION: GL: -nodepthtexture disables use of GL_ARB_depth_texture (required for shadowmapping)
// COMMANDLINEOPTION: GL: -nodrawbuffers disables use of GL_ARB_draw_buffers (required for r_shadow_deferredprepass)
// COMMANDLINEOPTION: GL: -nodrawrangeelements disables GL_EXT_draw_range_elements (renders faster)
// COMMANDLINEOPTION: GL: -noedgeclamp disables GL_EXT_texture_edge_clamp or GL_SGIS_texture_edge_clamp (recommended, some cards do not support the other texture clamp method)
// COMMANDLINEOPTION: GL: -nofbo disables GL_EXT_framebuffer_object (which accelerates rendering), only used if GL_ARB_fragment_shader is also available
// COMMANDLINEOPTION: GL: -nomtex disables GL_ARB_multitexture (required for faster map rendering)
// COMMANDLINEOPTION: GL: -noocclusionquery disables GL_ARB_occlusion_query (which allows coronas to fade according to visibility, and potentially used for rendering optimizations)
// COMMANDLINEOPTION: GL: -nos3tc disables GL_EXT_texture_compression_s3tc (which allows use of .dds texture caching)
// COMMANDLINEOPTION: GL: -noseparatestencil disables use of OpenGL2.0 glStencilOpSeparate and GL_ATI_separate_stencil extensions (which accelerate shadow rendering)
// COMMANDLINEOPTION: GL: -noshadow disables use of GL_ARB_shadow (required for hardware shadowmap filtering)
// COMMANDLINEOPTION: GL: -nostenciltwoside disables GL_EXT_stencil_two_side (which accelerate shadow rendering)
// COMMANDLINEOPTION: GL: -notexture3d disables GL_EXT_texture3D (required for spherical lights, otherwise they render as a column)
// COMMANDLINEOPTION: GL: -notexture4 disables GL_AMD_texture_texture4 (which provides fetch4 sampling)
// COMMANDLINEOPTION: GL: -notexturecompression disables GL_ARB_texture_compression (which saves video memory if it is supported, but can also degrade image quality, see gl_texturecompression cvar documentation for more information)
// COMMANDLINEOPTION: GL: -notexturegather disables GL_ARB_texture_gather (which provides fetch4 sampling)
// COMMANDLINEOPTION: GL: -notexturenonpoweroftwo disables GL_ARB_texture_non_power_of_two (which saves video memory if it is supported, but crashes on some buggy drivers)
// COMMANDLINEOPTION: GL: -novbo disables GL_ARB_vertex_buffer_object (which accelerates rendering)
// COMMANDLINEOPTION: GL: -nosrgb disables GL_EXT_texture_sRGB (which is used for higher quality non-linear texture gamma)
// COMMANDLINEOPTION: GL: -nomultisample disables GL_ARB_multisample

	if (vid.support.arb_draw_buffers)
		qglGetIntegerv(GL_MAX_DRAW_BUFFERS_ARB, (GLint*)&vid.maxdrawbuffers);

	// disable non-power-of-two textures on Radeon X1600 and other cards that do not accelerate it with some filtering modes / repeat modes that we use
	// we detect these cards by checking if the hardware supports vertex texture fetch (Geforce6 does, Radeon X1600 does not, all GL3-class hardware does)
	if(vid.support.arb_texture_non_power_of_two && vid.support.gl20shaders)
	{
		int val = 0;
		qglGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &val);CHECKGLERROR
		if (val < 1)
			vid.support.arb_texture_non_power_of_two = false;
	}

	// we don't care if it's an extension or not, they are identical functions, so keep it simple in the rendering code
	if (qglDrawRangeElements == NULL)
		qglDrawRangeElements = qglDrawRangeElementsEXT;

	qglGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint*)&vid.maxtexturesize_2d);
	if (vid.support.ext_texture_filter_anisotropic)
		qglGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, (GLint*)&vid.max_anisotropy);
	if (vid.support.arb_texture_cube_map)
		qglGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB, (GLint*)&vid.maxtexturesize_cubemap);
	if (vid.support.ext_texture_3d)
		qglGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, (GLint*)&vid.maxtexturesize_3d);

	// verify that 3d textures are really supported
	if (vid.support.ext_texture_3d && vid.maxtexturesize_3d < 32)
	{
		vid.support.ext_texture_3d = false;
		Con_Printf("GL_EXT_texture3D reported bogus GL_MAX_3D_TEXTURE_SIZE, disabled\n");
	}

	vid.texunits = vid.teximageunits = vid.texarrayunits = 1;
	if (vid.support.arb_multitexture)
		qglGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, (GLint*)&vid.texunits);
	if (vid_gl20.integer && vid.support.gl20shaders)
	{
		qglGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, (GLint*)&vid.texunits);
		qglGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, (int *)&vid.teximageunits);CHECKGLERROR
		qglGetIntegerv(GL_MAX_TEXTURE_COORDS, (int *)&vid.texarrayunits);CHECKGLERROR
		vid.texunits = bound(4, vid.texunits, MAX_TEXTUREUNITS);
		vid.teximageunits = bound(16, vid.teximageunits, MAX_TEXTUREUNITS);
		vid.texarrayunits = bound(8, vid.texarrayunits, MAX_TEXTUREUNITS);
		Con_DPrintf("Using GL2.0 rendering path - %i texture matrix, %i texture images, %i texcoords%s\n", vid.texunits, vid.teximageunits, vid.texarrayunits, vid.support.ext_framebuffer_object ? ", shadowmapping supported" : "");
		vid.renderpath = RENDERPATH_GL20;
		vid.sRGBcapable2D = false;
		vid.sRGBcapable3D = true;
		vid.useinterleavedarrays = false;
		Con_Printf("vid.support.arb_multisample %i\n", vid.support.arb_multisample);
		Con_Printf("vid.mode.samples %i\n", vid.mode.samples);
		Con_Printf("vid.support.gl20shaders %i\n", vid.support.gl20shaders);
		vid.allowalphatocoverage = true; // but see below, it may get turned to false again if GL_SAMPLES_ARB is <= 1
	}
	else if (vid.support.arb_texture_env_combine && vid.texunits >= 2 && vid_gl13.integer)
	{
		qglGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, (GLint*)&vid.texunits);
		vid.texunits = bound(1, vid.texunits, MAX_TEXTUREUNITS);
		vid.teximageunits = vid.texunits;
		vid.texarrayunits = vid.texunits;
		Con_DPrintf("Using GL1.3 rendering path - %i texture units, single pass rendering\n", vid.texunits);
		vid.renderpath = RENDERPATH_GL13;
		vid.sRGBcapable2D = false;
		vid.sRGBcapable3D = false;
		vid.useinterleavedarrays = false;
	}
	else
	{
		vid.texunits = bound(1, vid.texunits, MAX_TEXTUREUNITS);
		vid.teximageunits = vid.texunits;
		vid.texarrayunits = vid.texunits;
		Con_DPrintf("Using GL1.1 rendering path - %i texture units, two pass rendering\n", vid.texunits);
		vid.renderpath = RENDERPATH_GL11;
		vid.sRGBcapable2D = false;
		vid.sRGBcapable3D = false;
		vid.useinterleavedarrays = false;
	}

	// enable multisample antialiasing if possible
	if(vid.support.arb_multisample)
	{
		int samples = 0;
		qglGetIntegerv(GL_SAMPLES_ARB, &samples);
		if (samples > 1)
			qglEnable(GL_MULTISAMPLE_ARB);
		else
			vid.allowalphatocoverage = false;
	}
	else
		vid.allowalphatocoverage = false;

	// VorteX: set other info (maybe place them in VID_InitMode?)
	Cvar_SetQuick(&gl_info_vendor, gl_vendor);
	Cvar_SetQuick(&gl_info_renderer, gl_renderer);
	Cvar_SetQuick(&gl_info_version, gl_version);
	Cvar_SetQuick(&gl_info_platform, gl_platform ? gl_platform : "");
	Cvar_SetQuick(&gl_info_driver, gl_driver);
}

float VID_JoyState_GetAxis(const vid_joystate_t *joystate, int axis, float sensitivity, float deadzone)
{
	float value;
	value = (axis >= 0 && axis < MAXJOYAXIS) ? joystate->axis[axis] : 0.0f;
	value = value > deadzone ? (value - deadzone) : (value < -deadzone ? (value + deadzone) : 0.0f);
	value *= deadzone > 0 ? (1.0f / (1.0f - deadzone)) : 1.0f;
	value = bound(-1, value, 1);
	return value * sensitivity;
}

qboolean VID_JoyBlockEmulatedKeys(int keycode)
{
	int j;
	vid_joystate_t joystate;

	if (!joy_axiskeyevents.integer)
		return false;
	if (vid_joystate.is360)
		return false;
	if (keycode != K_UPARROW && keycode != K_DOWNARROW && keycode != K_RIGHTARROW && keycode != K_LEFTARROW)
		return false;

	// block system-generated key events for arrow keys if we're emulating the arrow keys ourselves
	VID_BuildJoyState(&joystate);
	for (j = 32;j < 36;j++)
		if (vid_joystate.button[j] || joystate.button[j])
			return true;

	return false;
}

void VID_Shared_BuildJoyState_Begin(vid_joystate_t *joystate)
{
#ifdef WIN32
	xinput_state_t xinputstate;
#endif
	memset(joystate, 0, sizeof(*joystate));
#ifdef WIN32
	if (vid_xinputindex >= 0 && qXInputGetState && qXInputGetState(vid_xinputindex, &xinputstate) == S_OK)
	{
		joystate->is360 = true;
		joystate->button[ 0] = (xinputstate.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
		joystate->button[ 1] = (xinputstate.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
		joystate->button[ 2] = (xinputstate.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
		joystate->button[ 3] = (xinputstate.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
		joystate->button[ 4] = (xinputstate.Gamepad.wButtons & XINPUT_GAMEPAD_START) != 0;
		joystate->button[ 5] = (xinputstate.Gamepad.wButtons & XINPUT_GAMEPAD_BACK) != 0;
		joystate->button[ 6] = (xinputstate.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0;
		joystate->button[ 7] = (xinputstate.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;
		joystate->button[ 8] = (xinputstate.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
		joystate->button[ 9] = (xinputstate.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
		joystate->button[10] = (xinputstate.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;
		joystate->button[11] = (xinputstate.Gamepad.wButtons & XINPUT_GAMEPAD_B) != 0;
		joystate->button[12] = (xinputstate.Gamepad.wButtons & XINPUT_GAMEPAD_X) != 0;
		joystate->button[13] = (xinputstate.Gamepad.wButtons & XINPUT_GAMEPAD_Y) != 0;
		joystate->button[14] = xinputstate.Gamepad.bLeftTrigger >= XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
		joystate->button[15] = xinputstate.Gamepad.bRightTrigger >= XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
		joystate->button[16] = xinputstate.Gamepad.sThumbLY < -16384;
		joystate->button[17] = xinputstate.Gamepad.sThumbLY >  16384;
		joystate->button[18] = xinputstate.Gamepad.sThumbLX < -16384;
		joystate->button[19] = xinputstate.Gamepad.sThumbLX >  16384;
		joystate->button[20] = xinputstate.Gamepad.sThumbRY < -16384;
		joystate->button[21] = xinputstate.Gamepad.sThumbRY >  16384;
		joystate->button[22] = xinputstate.Gamepad.sThumbRX < -16384;
		joystate->button[23] = xinputstate.Gamepad.sThumbRX >  16384;
		joystate->axis[ 4] = xinputstate.Gamepad.bLeftTrigger * (1.0f / 255.0f);
		joystate->axis[ 5] = xinputstate.Gamepad.bRightTrigger * (1.0f / 255.0f);
		joystate->axis[ 0] = xinputstate.Gamepad.sThumbLX * (1.0f / 32767.0f);
		joystate->axis[ 1] = xinputstate.Gamepad.sThumbLY * (1.0f / 32767.0f);
		joystate->axis[ 2] = xinputstate.Gamepad.sThumbRX * (1.0f / 32767.0f);
		joystate->axis[ 3] = xinputstate.Gamepad.sThumbRY * (1.0f / 32767.0f);
	}
#endif
}

void VID_Shared_BuildJoyState_Finish(vid_joystate_t *joystate)
{
	float f, r;
	if (joystate->is360)
		return;
	// emulate key events for thumbstick
	f = VID_JoyState_GetAxis(joystate, joy_axisforward.integer, 1, joy_axiskeyevents_deadzone.value) * joy_sensitivityforward.value;
	r = VID_JoyState_GetAxis(joystate, joy_axisside.integer   , 1, joy_axiskeyevents_deadzone.value) * joy_sensitivityside.value;
#if MAXJOYBUTTON != 36
#error this code must be updated if MAXJOYBUTTON changes!
#endif
	joystate->button[32] = f > 0.0f;
	joystate->button[33] = f < 0.0f;
	joystate->button[34] = r > 0.0f;
	joystate->button[35] = r < 0.0f;
}

void VID_KeyEventForButton(qboolean oldbutton, qboolean newbutton, int key, double *timer)
{
	if (oldbutton)
	{
		if (newbutton)
		{
			if (realtime >= *timer)
			{
				Key_Event(key, 0, true);
				*timer = realtime + 0.1;
			}
		}
		else
		{
			Key_Event(key, 0, false);
			*timer = 0;
		}
	}
	else
	{
		if (newbutton)
		{
			Key_Event(key, 0, true);
			*timer = realtime + 0.5;
		}
	}
}

#if MAXJOYBUTTON != 36
#error this code must be updated if MAXJOYBUTTON changes!
#endif
static int joybuttonkey[MAXJOYBUTTON][2] =
{
	{K_JOY1, K_ENTER}, {K_JOY2, K_ESCAPE}, {K_JOY3, 0}, {K_JOY4, 0}, {K_JOY5, 0}, {K_JOY6, 0}, {K_JOY7, 0}, {K_JOY8, 0}, {K_JOY9, 0}, {K_JOY10, 0}, {K_JOY11, 0}, {K_JOY12, 0}, {K_JOY13, 0}, {K_JOY14, 0}, {K_JOY15, 0}, {K_JOY16, 0},
	{K_AUX1, 0}, {K_AUX2, 0}, {K_AUX3, 0}, {K_AUX4, 0}, {K_AUX5, 0}, {K_AUX6, 0}, {K_AUX7, 0}, {K_AUX8, 0}, {K_AUX9, 0}, {K_AUX10, 0}, {K_AUX11, 0}, {K_AUX12, 0}, {K_AUX13, 0}, {K_AUX14, 0}, {K_AUX15, 0}, {K_AUX16, 0},
	{K_JOY_UP, K_UPARROW}, {K_JOY_DOWN, K_DOWNARROW}, {K_JOY_RIGHT, K_RIGHTARROW}, {K_JOY_LEFT, K_LEFTARROW},
};

static int joybuttonkey360[][2] =
{
	{K_X360_DPAD_UP, K_UPARROW},
	{K_X360_DPAD_DOWN, K_DOWNARROW},
	{K_X360_DPAD_LEFT, K_LEFTARROW},
	{K_X360_DPAD_RIGHT, K_RIGHTARROW},
	{K_X360_START, K_ESCAPE},
	{K_X360_BACK, K_ESCAPE},
	{K_X360_LEFT_THUMB, 0},
	{K_X360_RIGHT_THUMB, 0},
	{K_X360_LEFT_SHOULDER, 0},
	{K_X360_RIGHT_SHOULDER, 0},
	{K_X360_A, K_ENTER},
	{K_X360_B, K_ESCAPE},
	{K_X360_X, 0},
	{K_X360_Y, 0},
	{K_X360_LEFT_TRIGGER, 0},
	{K_X360_RIGHT_TRIGGER, 0},
	{K_X360_LEFT_THUMB_DOWN, K_DOWNARROW},
	{K_X360_LEFT_THUMB_UP, K_UPARROW},
	{K_X360_LEFT_THUMB_LEFT, K_LEFTARROW},
	{K_X360_LEFT_THUMB_RIGHT, K_RIGHTARROW},
	{K_X360_RIGHT_THUMB_DOWN, 0},
	{K_X360_RIGHT_THUMB_UP, 0},
	{K_X360_RIGHT_THUMB_LEFT, 0},
	{K_X360_RIGHT_THUMB_RIGHT, 0},
};

double vid_joybuttontimer[MAXJOYBUTTON];
void VID_ApplyJoyState(vid_joystate_t *joystate)
{
	int j;
	int c = joy_axiskeyevents.integer != 0;
	if (joystate->is360)
	{
#if 0
		// keystrokes (chatpad)
		// DOES NOT WORK - no driver support in xinput1_3.dll :(
		xinput_keystroke_t keystroke;
		while (qXInputGetKeystroke && qXInputGetKeystroke(XUSER_INDEX_ANY, 0, &keystroke) == S_OK)
			Con_Printf("XInput KeyStroke: VirtualKey %i, Unicode %i, Flags %x, UserIndex %i, HidCode %i\n", keystroke.VirtualKey, keystroke.Unicode, keystroke.Flags, keystroke.UserIndex, keystroke.HidCode);
#endif

		// emit key events for buttons
		for (j = 0;j < (int)(sizeof(joybuttonkey360)/sizeof(joybuttonkey360[0]));j++)
			VID_KeyEventForButton(vid_joystate.button[j] != 0, joystate->button[j] != 0, joybuttonkey360[j][c], &vid_joybuttontimer[j]);

		// axes
		cl.cmd.forwardmove += VID_JoyState_GetAxis(joystate, joy_x360_axisforward.integer, joy_x360_sensitivityforward.value, joy_x360_deadzoneforward.value) * cl_forwardspeed.value;
		cl.cmd.sidemove    += VID_JoyState_GetAxis(joystate, joy_x360_axisside.integer, joy_x360_sensitivityside.value, joy_x360_deadzoneside.value) * cl_sidespeed.value;
		cl.cmd.upmove      += VID_JoyState_GetAxis(joystate, joy_x360_axisup.integer, joy_x360_sensitivityup.value, joy_x360_deadzoneup.value) * cl_upspeed.value;
		cl.viewangles[0]   += VID_JoyState_GetAxis(joystate, joy_x360_axispitch.integer, joy_x360_sensitivitypitch.value, joy_x360_deadzonepitch.value) * cl.realframetime * cl_pitchspeed.value;
		cl.viewangles[1]   += VID_JoyState_GetAxis(joystate, joy_x360_axisyaw.integer, joy_x360_sensitivityyaw.value, joy_x360_deadzoneyaw.value) * cl.realframetime * cl_yawspeed.value;
		//cl.viewangles[2]   += VID_JoyState_GetAxis(joystate, joy_x360_axisroll.integer, joy_x360_sensitivityroll.value, joy_x360_deadzoneroll.value) * cl.realframetime * cl_rollspeed.value;
	}
	else
	{
		// emit key events for buttons
		for (j = 0;j < MAXJOYBUTTON;j++)
			VID_KeyEventForButton(vid_joystate.button[j] != 0, joystate->button[j] != 0, joybuttonkey[j][c], &vid_joybuttontimer[j]);

		// axes
		cl.cmd.forwardmove += VID_JoyState_GetAxis(joystate, joy_axisforward.integer, joy_sensitivityforward.value, joy_deadzoneforward.value) * cl_forwardspeed.value;
		cl.cmd.sidemove    += VID_JoyState_GetAxis(joystate, joy_axisside.integer, joy_sensitivityside.value, joy_deadzoneside.value) * cl_sidespeed.value;
		cl.cmd.upmove      += VID_JoyState_GetAxis(joystate, joy_axisup.integer, joy_sensitivityup.value, joy_deadzoneup.value) * cl_upspeed.value;
		cl.viewangles[0]   += VID_JoyState_GetAxis(joystate, joy_axispitch.integer, joy_sensitivitypitch.value, joy_deadzonepitch.value) * cl.realframetime * cl_pitchspeed.value;
		cl.viewangles[1]   += VID_JoyState_GetAxis(joystate, joy_axisyaw.integer, joy_sensitivityyaw.value, joy_deadzoneyaw.value) * cl.realframetime * cl_yawspeed.value;
		//cl.viewangles[2]   += VID_JoyState_GetAxis(joystate, joy_axisroll.integer, joy_sensitivityroll.value, joy_deadzoneroll.value) * cl.realframetime * cl_rollspeed.value;
	}

	vid_joystate = *joystate;
}

int VID_Shared_SetJoystick(int index)
{
#ifdef WIN32
	int i;
	int xinputcount = 0;
	int xinputindex = -1;
	int xinputavailable = 0;
	xinput_state_t state;
	// detect available XInput controllers
	for (i = 0;i < 4;i++)
	{
		if (qXInputGetState && qXInputGetState(i, &state) == S_OK)
		{
			xinputavailable |= 1<<i;
			if (index == xinputcount)
				xinputindex = i;
			xinputcount++;
		}
	}
	if (joy_xinputavailable.integer != xinputavailable)
		Cvar_SetValueQuick(&joy_xinputavailable, xinputavailable);
	if (vid_xinputindex != xinputindex)
	{
		vid_xinputindex = xinputindex;
		if (xinputindex >= 0)
			Con_Printf("Joystick %i opened (XInput Device %i)\n", index, xinputindex);
	}
	return xinputcount;
#else
	return 0;
#endif
}


void Force_CenterView_f (void)
{
	cl.viewangles[PITCH] = 0;
}

static int gamma_forcenextframe = false;
static float cachegamma, cachebrightness, cachecontrast, cacheblack[3], cachegrey[3], cachewhite[3], cachecontrastboost;
static int cachecolorenable, cachehwgamma;

unsigned int vid_gammatables_serial = 0; // so other subsystems can poll if gamma parameters have changed
qboolean vid_gammatables_trivial = true;
void VID_BuildGammaTables(unsigned short *ramps, int rampsize)
{
	float srgbmul = (vid.sRGB2D || vid.sRGB3D) ? 2.2f : 1.0f;
	if (cachecolorenable)
	{
		BuildGammaTable16(1.0f, invpow(0.5, 1 - cachegrey[0]) * srgbmul, cachewhite[0], cacheblack[0], cachecontrastboost, ramps, rampsize);
		BuildGammaTable16(1.0f, invpow(0.5, 1 - cachegrey[1]) * srgbmul, cachewhite[1], cacheblack[1], cachecontrastboost, ramps + rampsize, rampsize);
		BuildGammaTable16(1.0f, invpow(0.5, 1 - cachegrey[2]) * srgbmul, cachewhite[2], cacheblack[2], cachecontrastboost, ramps + rampsize*2, rampsize);
	}
	else
	{
		BuildGammaTable16(1.0f, cachegamma * srgbmul, cachecontrast, cachebrightness, cachecontrastboost, ramps, rampsize);
		BuildGammaTable16(1.0f, cachegamma * srgbmul, cachecontrast, cachebrightness, cachecontrastboost, ramps + rampsize, rampsize);
		BuildGammaTable16(1.0f, cachegamma * srgbmul, cachecontrast, cachebrightness, cachecontrastboost, ramps + rampsize*2, rampsize);
	}

	// LordHavoc: this code came from Ben Winslow and Zinx Verituse, I have
	// immensely butchered it to work with variable framerates and fit in with
	// the rest of darkplaces.
	if (v_psycho.integer)
	{
		int x, y;
		float t;
		static float n[3], nd[3], nt[3];
		static int init = true;
		unsigned short *ramp;
		gamma_forcenextframe = true;
		if (init)
		{
			init = false;
			for (x = 0;x < 3;x++)
			{
				n[x] = lhrandom(0, 1);
				nd[x] = (rand()&1)?-0.25:0.25;
				nt[x] = lhrandom(1, 8.2);
			}
		}

		for (x = 0;x < 3;x++)
		{
			nt[x] -= cl.realframetime;
			if (nt[x] < 0)
			{
				nd[x] = -nd[x];
				nt[x] += lhrandom(1, 8.2);
			}
			n[x] += nd[x] * cl.realframetime;
			n[x] -= floor(n[x]);
		}

		for (x = 0, ramp = ramps;x < 3;x++)
			for (y = 0, t = n[x] - 0.75f;y < rampsize;y++, t += 0.75f * (2.0f / rampsize))
				*ramp++ = (unsigned short)(cos(t*(M_PI*2.0)) * 32767.0f + 32767.0f);
	}
}

void VID_UpdateGamma(qboolean force, int rampsize)
{
	cvar_t *c;
	float f;
	int wantgamma;
	qboolean gamma_changed = false;

	// LordHavoc: don't mess with gamma tables if running dedicated
	if (cls.state == ca_dedicated)
		return;

	wantgamma = v_hwgamma.integer;
	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
	case RENDERPATH_SOFT:
	case RENDERPATH_GLES2:
		if (v_glslgamma.integer)
			wantgamma = 0;
		break;
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
		break;
	}
	if(!vid_activewindow)
		wantgamma = 0;
#define BOUNDCVAR(cvar, m1, m2) c = &(cvar);f = bound(m1, c->value, m2);if (c->value != f) Cvar_SetValueQuick(c, f);
	BOUNDCVAR(v_gamma, 0.1, 5);
	BOUNDCVAR(v_contrast, 0.2, 5);
	BOUNDCVAR(v_brightness, -v_contrast.value * 0.8, 0.8);
	//BOUNDCVAR(v_contrastboost, 0.0625, 16);
	BOUNDCVAR(v_color_black_r, 0, 0.8);
	BOUNDCVAR(v_color_black_g, 0, 0.8);
	BOUNDCVAR(v_color_black_b, 0, 0.8);
	BOUNDCVAR(v_color_grey_r, 0, 0.95);
	BOUNDCVAR(v_color_grey_g, 0, 0.95);
	BOUNDCVAR(v_color_grey_b, 0, 0.95);
	BOUNDCVAR(v_color_white_r, 1, 5);
	BOUNDCVAR(v_color_white_g, 1, 5);
	BOUNDCVAR(v_color_white_b, 1, 5);
#undef BOUNDCVAR

	// set vid_gammatables_trivial to true if the current settings would generate the identity gamma table
	vid_gammatables_trivial = false;
	if(v_psycho.integer == 0)
	if(v_contrastboost.value == 1)
	if(!vid.sRGB2D)
	if(!vid.sRGB3D)
	{
		if(v_color_enable.integer)
		{
			if(v_color_black_r.value == 0)
			if(v_color_black_g.value == 0)
			if(v_color_black_b.value == 0)
			if(fabs(v_color_grey_r.value - 0.5) < 1e-6)
			if(fabs(v_color_grey_g.value - 0.5) < 1e-6)
			if(fabs(v_color_grey_b.value - 0.5) < 1e-6)
			if(v_color_white_r.value == 1)
			if(v_color_white_g.value == 1)
			if(v_color_white_b.value == 1)
				vid_gammatables_trivial = true;
		}
		else
		{
			if(v_gamma.value == 1)
			if(v_contrast.value == 1)
			if(v_brightness.value == 0)
				vid_gammatables_trivial = true;
		}
	}

#define GAMMACHECK(cache, value) if (cache != (value)) gamma_changed = true;cache = (value)
	if(v_psycho.integer)
		gamma_changed = true;
	GAMMACHECK(cachegamma      , v_gamma.value);
	GAMMACHECK(cachecontrast   , v_contrast.value);
	GAMMACHECK(cachebrightness , v_brightness.value);
	GAMMACHECK(cachecontrastboost, v_contrastboost.value);
	GAMMACHECK(cachecolorenable, v_color_enable.integer);
	GAMMACHECK(cacheblack[0]   , v_color_black_r.value);
	GAMMACHECK(cacheblack[1]   , v_color_black_g.value);
	GAMMACHECK(cacheblack[2]   , v_color_black_b.value);
	GAMMACHECK(cachegrey[0]    , v_color_grey_r.value);
	GAMMACHECK(cachegrey[1]    , v_color_grey_g.value);
	GAMMACHECK(cachegrey[2]    , v_color_grey_b.value);
	GAMMACHECK(cachewhite[0]   , v_color_white_r.value);
	GAMMACHECK(cachewhite[1]   , v_color_white_g.value);
	GAMMACHECK(cachewhite[2]   , v_color_white_b.value);

	if(gamma_changed)
		++vid_gammatables_serial;

	GAMMACHECK(cachehwgamma    , wantgamma);
#undef GAMMACHECK

	if (!force && !gamma_forcenextframe && !gamma_changed)
		return;

	gamma_forcenextframe = false;

	if (cachehwgamma)
	{
		if (!vid_usinghwgamma)
		{
			vid_usinghwgamma = true;
			if (vid_gammarampsize != rampsize || !vid_gammaramps)
			{
				vid_gammarampsize = rampsize;
				if (vid_gammaramps)
					Z_Free(vid_gammaramps);
				vid_gammaramps = (unsigned short *)Z_Malloc(6 * vid_gammarampsize * sizeof(unsigned short));
				vid_systemgammaramps = vid_gammaramps + 3 * vid_gammarampsize;
			}
			VID_GetGamma(vid_systemgammaramps, vid_gammarampsize);
		}

		VID_BuildGammaTables(vid_gammaramps, vid_gammarampsize);

		// set vid_hardwaregammasupported to true if VID_SetGamma succeeds, OR if vid_hwgamma is >= 2 (forced gamma - ignores driver return value)
		Cvar_SetValueQuick(&vid_hardwaregammasupported, VID_SetGamma(vid_gammaramps, vid_gammarampsize) || cachehwgamma >= 2);
		// if custom gamma ramps failed (Windows stupidity), restore to system gamma
		if(!vid_hardwaregammasupported.integer)
		{
			if (vid_usinghwgamma)
			{
				vid_usinghwgamma = false;
				VID_SetGamma(vid_systemgammaramps, vid_gammarampsize);
			}
		}
	}
	else
	{
		if (vid_usinghwgamma)
		{
			vid_usinghwgamma = false;
			VID_SetGamma(vid_systemgammaramps, vid_gammarampsize);
		}
	}
}

void VID_RestoreSystemGamma(void)
{
	if (vid_usinghwgamma)
	{
		vid_usinghwgamma = false;
		Cvar_SetValueQuick(&vid_hardwaregammasupported, VID_SetGamma(vid_systemgammaramps, vid_gammarampsize));
		// force gamma situation to be reexamined next frame
		gamma_forcenextframe = true;
	}
}

#ifdef WIN32
static dllfunction_t xinputdllfuncs[] =
{
	{"XInputGetState", (void **) &qXInputGetState},
	{"XInputGetKeystroke", (void **) &qXInputGetKeystroke},
	{NULL, NULL}
};
static const char* xinputdllnames [] =
{
	"xinput1_3.dll",
	"xinput1_2.dll",
	"xinput1_1.dll",
	NULL
};
static dllhandle_t xinputdll_dll = NULL;
#endif

void VID_Shared_Init(void)
{
#ifdef SSE_POSSIBLE
	if (Sys_HaveSSE2())
	{
		Con_Printf("DPSOFTRAST available (SSE2 instructions detected)\n");
		Cvar_RegisterVariable(&vid_soft);
		Cvar_RegisterVariable(&vid_soft_threads);
		Cvar_RegisterVariable(&vid_soft_interlace);
	}
	else
		Con_Printf("DPSOFTRAST not available (SSE2 disabled or not detected)\n");
#else
	Con_Printf("DPSOFTRAST not available (SSE2 not compiled in)\n");
#endif

	Cvar_RegisterVariable(&vid_hardwaregammasupported);
	Cvar_RegisterVariable(&gl_info_vendor);
	Cvar_RegisterVariable(&gl_info_renderer);
	Cvar_RegisterVariable(&gl_info_version);
	Cvar_RegisterVariable(&gl_info_extensions);
	Cvar_RegisterVariable(&gl_info_platform);
	Cvar_RegisterVariable(&gl_info_driver);
	Cvar_RegisterVariable(&v_gamma);
	Cvar_RegisterVariable(&v_brightness);
	Cvar_RegisterVariable(&v_contrastboost);
	Cvar_RegisterVariable(&v_contrast);

	Cvar_RegisterVariable(&v_color_enable);
	Cvar_RegisterVariable(&v_color_black_r);
	Cvar_RegisterVariable(&v_color_black_g);
	Cvar_RegisterVariable(&v_color_black_b);
	Cvar_RegisterVariable(&v_color_grey_r);
	Cvar_RegisterVariable(&v_color_grey_g);
	Cvar_RegisterVariable(&v_color_grey_b);
	Cvar_RegisterVariable(&v_color_white_r);
	Cvar_RegisterVariable(&v_color_white_g);
	Cvar_RegisterVariable(&v_color_white_b);

	Cvar_RegisterVariable(&v_hwgamma);
	Cvar_RegisterVariable(&v_glslgamma);

	Cvar_RegisterVariable(&v_psycho);

	Cvar_RegisterVariable(&vid_fullscreen);
	Cvar_RegisterVariable(&vid_width);
	Cvar_RegisterVariable(&vid_height);
	Cvar_RegisterVariable(&vid_bitsperpixel);
	Cvar_RegisterVariable(&vid_samples);
	Cvar_RegisterVariable(&vid_refreshrate);
	Cvar_RegisterVariable(&vid_userefreshrate);
	Cvar_RegisterVariable(&vid_stereobuffer);
	Cvar_RegisterVariable(&vid_vsync);
	Cvar_RegisterVariable(&vid_mouse);
	Cvar_RegisterVariable(&vid_grabkeyboard);
	Cvar_RegisterVariable(&vid_touchscreen);
	Cvar_RegisterVariable(&vid_stick_mouse);
	Cvar_RegisterVariable(&vid_resizable);
	Cvar_RegisterVariable(&vid_minwidth);
	Cvar_RegisterVariable(&vid_minheight);
	Cvar_RegisterVariable(&vid_gl13);
	Cvar_RegisterVariable(&vid_gl20);
	Cvar_RegisterVariable(&gl_finish);
	Cvar_RegisterVariable(&vid_sRGB);

	Cvar_RegisterVariable(&joy_active);
#ifdef WIN32
	Cvar_RegisterVariable(&joy_xinputavailable);
#endif
	Cvar_RegisterVariable(&joy_detected);
	Cvar_RegisterVariable(&joy_enable);
	Cvar_RegisterVariable(&joy_index);
	Cvar_RegisterVariable(&joy_axisforward);
	Cvar_RegisterVariable(&joy_axisside);
	Cvar_RegisterVariable(&joy_axisup);
	Cvar_RegisterVariable(&joy_axispitch);
	Cvar_RegisterVariable(&joy_axisyaw);
	//Cvar_RegisterVariable(&joy_axisroll);
	Cvar_RegisterVariable(&joy_deadzoneforward);
	Cvar_RegisterVariable(&joy_deadzoneside);
	Cvar_RegisterVariable(&joy_deadzoneup);
	Cvar_RegisterVariable(&joy_deadzonepitch);
	Cvar_RegisterVariable(&joy_deadzoneyaw);
	//Cvar_RegisterVariable(&joy_deadzoneroll);
	Cvar_RegisterVariable(&joy_sensitivityforward);
	Cvar_RegisterVariable(&joy_sensitivityside);
	Cvar_RegisterVariable(&joy_sensitivityup);
	Cvar_RegisterVariable(&joy_sensitivitypitch);
	Cvar_RegisterVariable(&joy_sensitivityyaw);
	//Cvar_RegisterVariable(&joy_sensitivityroll);
	Cvar_RegisterVariable(&joy_axiskeyevents);
	Cvar_RegisterVariable(&joy_axiskeyevents_deadzone);
	Cvar_RegisterVariable(&joy_x360_axisforward);
	Cvar_RegisterVariable(&joy_x360_axisside);
	Cvar_RegisterVariable(&joy_x360_axisup);
	Cvar_RegisterVariable(&joy_x360_axispitch);
	Cvar_RegisterVariable(&joy_x360_axisyaw);
	//Cvar_RegisterVariable(&joy_x360_axisroll);
	Cvar_RegisterVariable(&joy_x360_deadzoneforward);
	Cvar_RegisterVariable(&joy_x360_deadzoneside);
	Cvar_RegisterVariable(&joy_x360_deadzoneup);
	Cvar_RegisterVariable(&joy_x360_deadzonepitch);
	Cvar_RegisterVariable(&joy_x360_deadzoneyaw);
	//Cvar_RegisterVariable(&joy_x360_deadzoneroll);
	Cvar_RegisterVariable(&joy_x360_sensitivityforward);
	Cvar_RegisterVariable(&joy_x360_sensitivityside);
	Cvar_RegisterVariable(&joy_x360_sensitivityup);
	Cvar_RegisterVariable(&joy_x360_sensitivitypitch);
	Cvar_RegisterVariable(&joy_x360_sensitivityyaw);
	//Cvar_RegisterVariable(&joy_x360_sensitivityroll);

#ifdef WIN32
	Sys_LoadLibrary(xinputdllnames, &xinputdll_dll, xinputdllfuncs);
#endif

	Cmd_AddCommand("force_centerview", Force_CenterView_f, "recenters view (stops looking up/down)");
	Cmd_AddCommand("vid_restart", VID_Restart_f, "restarts video system (closes and reopens the window, restarts renderer)");
}

int VID_Mode(int fullscreen, int width, int height, int bpp, float refreshrate, int stereobuffer, int samples)
{
	viddef_mode_t mode;

	memset(&mode, 0, sizeof(mode));
	mode.fullscreen = fullscreen != 0;
	mode.width = width;
	mode.height = height;
	mode.bitsperpixel = bpp;
	mode.refreshrate = vid_userefreshrate.integer ? max(1, refreshrate) : 0;
	mode.userefreshrate = vid_userefreshrate.integer != 0;
	mode.stereobuffer = stereobuffer != 0;
	mode.samples = samples;
	cl_ignoremousemoves = 2;
	VID_ClearExtensions();
	if (VID_InitMode(&mode))
	{
		// accept the (possibly modified) mode
		vid.mode = mode;
		vid.fullscreen     = vid.mode.fullscreen;
		vid.width          = vid.mode.width;
		vid.height         = vid.mode.height;
		vid.bitsperpixel   = vid.mode.bitsperpixel;
		vid.refreshrate    = vid.mode.refreshrate;
		vid.userefreshrate = vid.mode.userefreshrate;
		vid.stereobuffer   = vid.mode.stereobuffer;
		vid.samples        = vid.mode.samples;
		vid.stencil        = vid.mode.bitsperpixel > 16;
		vid.sRGB2D         = vid_sRGB.integer >= 1 && vid.sRGBcapable2D;
		vid.sRGB3D         = vid_sRGB.integer >= 1 && vid.sRGBcapable3D;

		Con_Printf("Video Mode: %s %dx%dx%dx%.2fhz%s%s\n", mode.fullscreen ? "fullscreen" : "window", mode.width, mode.height, mode.bitsperpixel, mode.refreshrate, mode.stereobuffer ? " stereo" : "", mode.samples > 1 ? va(" (%ix AA)", mode.samples) : "");

		Cvar_SetValueQuick(&vid_fullscreen, vid.mode.fullscreen);
		Cvar_SetValueQuick(&vid_width, vid.mode.width);
		Cvar_SetValueQuick(&vid_height, vid.mode.height);
		Cvar_SetValueQuick(&vid_bitsperpixel, vid.mode.bitsperpixel);
		Cvar_SetValueQuick(&vid_samples, vid.mode.samples);
		if(vid_userefreshrate.integer)
			Cvar_SetValueQuick(&vid_refreshrate, vid.mode.refreshrate);
		Cvar_SetValueQuick(&vid_stereobuffer, vid.mode.stereobuffer);

		return true;
	}
	else
		return false;
}

static void VID_OpenSystems(void)
{
	R_Modules_Start();
	S_Startup();
}

static void VID_CloseSystems(void)
{
	S_Shutdown();
	R_Modules_Shutdown();
}

qboolean vid_commandlinecheck = true;
extern qboolean vid_opened;

void VID_Restart_f(void)
{
	// don't crash if video hasn't started yet
	if (vid_commandlinecheck)
		return;

	if (!vid_opened)
	{
		SCR_BeginLoadingPlaque();
		return;
	}

	Con_Printf("VID_Restart: changing from %s %dx%dx%dbpp%s%s, to %s %dx%dx%dbpp%s%s.\n",
		vid.mode.fullscreen ? "fullscreen" : "window", vid.mode.width, vid.mode.height, vid.mode.bitsperpixel, vid.mode.fullscreen && vid.mode.userefreshrate ? va("x%.2fhz", vid.mode.refreshrate) : "", vid.mode.samples > 1 ? va(" (%ix AA)", vid.mode.samples) : "",
		vid_fullscreen.integer ? "fullscreen" : "window", vid_width.integer, vid_height.integer, vid_bitsperpixel.integer, vid_fullscreen.integer && vid_userefreshrate.integer ? va("x%.2fhz", vid_refreshrate.value) : "", vid_samples.integer > 1 ? va(" (%ix AA)", vid_samples.integer) : "");
	VID_CloseSystems();
	VID_Shutdown();
	if (!VID_Mode(vid_fullscreen.integer, vid_width.integer, vid_height.integer, vid_bitsperpixel.integer, vid_refreshrate.value, vid_stereobuffer.integer, vid_samples.integer))
	{
		Con_Print("Video mode change failed\n");
		if (!VID_Mode(vid.mode.fullscreen, vid.mode.width, vid.mode.height, vid.mode.bitsperpixel, vid.mode.refreshrate, vid.mode.stereobuffer, vid.mode.samples))
			Sys_Error("Unable to restore to last working video mode");
	}
	VID_OpenSystems();
}

const char *vidfallbacks[][2] =
{
	{"vid_stereobuffer", "0"},
	{"vid_samples", "1"},
	{"vid_userefreshrate", "0"},
	{"vid_width", "640"},
	{"vid_height", "480"},
	{"vid_bitsperpixel", "16"},
	{NULL, NULL}
};

// this is only called once by Host_StartVideo and again on each FS_GameDir_f
void VID_Start(void)
{
	int i, width, height, success;
	if (vid_commandlinecheck)
	{
		// interpret command-line parameters
		vid_commandlinecheck = false;
// COMMANDLINEOPTION: Video: -window performs +vid_fullscreen 0
		if (COM_CheckParm("-window") || COM_CheckParm("-safe"))
			Cvar_SetValueQuick(&vid_fullscreen, false);
// COMMANDLINEOPTION: Video: -fullscreen performs +vid_fullscreen 1
		if (COM_CheckParm("-fullscreen"))
			Cvar_SetValueQuick(&vid_fullscreen, true);
		width = 0;
		height = 0;
// COMMANDLINEOPTION: Video: -width <pixels> performs +vid_width <pixels> and also +vid_height <pixels*3/4> if only -width is specified (example: -width 1024 sets 1024x768 mode)
		if ((i = COM_CheckParm("-width")) != 0)
			width = atoi(com_argv[i+1]);
// COMMANDLINEOPTION: Video: -height <pixels> performs +vid_height <pixels> and also +vid_width <pixels*4/3> if only -height is specified (example: -height 768 sets 1024x768 mode)
		if ((i = COM_CheckParm("-height")) != 0)
			height = atoi(com_argv[i+1]);
		if (width == 0)
			width = height * 4 / 3;
		if (height == 0)
			height = width * 3 / 4;
		if (width)
			Cvar_SetValueQuick(&vid_width, width);
		if (height)
			Cvar_SetValueQuick(&vid_height, height);
// COMMANDLINEOPTION: Video: -bpp <bits> performs +vid_bitsperpixel <bits> (example -bpp 32 or -bpp 16)
		if ((i = COM_CheckParm("-bpp")) != 0)
			Cvar_SetQuick(&vid_bitsperpixel, com_argv[i+1]);
	}

	success = VID_Mode(vid_fullscreen.integer, vid_width.integer, vid_height.integer, vid_bitsperpixel.integer, vid_refreshrate.value, vid_stereobuffer.integer, vid_samples.integer);
	if (!success)
	{
		Con_Print("Desired video mode fail, trying fallbacks...\n");
		for (i = 0;!success && vidfallbacks[i][0] != NULL;i++)
		{
			Cvar_Set(vidfallbacks[i][0], vidfallbacks[i][1]);
			success = VID_Mode(vid_fullscreen.integer, vid_width.integer, vid_height.integer, vid_bitsperpixel.integer, vid_refreshrate.value, vid_stereobuffer.integer, vid_samples.integer);
		}
		if (!success)
			Sys_Error("Video modes failed");
	}
	VID_OpenSystems();
}

void VID_Stop(void)
{
	VID_CloseSystems();
	VID_Shutdown();
}

int VID_SortModes_Compare(const void *a_, const void *b_)
{
	vid_mode_t *a = (vid_mode_t *) a_;
	vid_mode_t *b = (vid_mode_t *) b_;
	if(a->width > b->width)
		return +1;
	if(a->width < b->width)
		return -1;
	if(a->height > b->height)
		return +1;
	if(a->height < b->height)
		return -1;
	if(a->refreshrate > b->refreshrate)
		return +1;
	if(a->refreshrate < b->refreshrate)
		return -1;
	if(a->bpp > b->bpp)
		return +1;
	if(a->bpp < b->bpp)
		return -1;
	if(a->pixelheight_num * b->pixelheight_denom > a->pixelheight_denom * b->pixelheight_num)
		return +1;
	if(a->pixelheight_num * b->pixelheight_denom < a->pixelheight_denom * b->pixelheight_num)
		return -1;
	return 0;
}
size_t VID_SortModes(vid_mode_t *modes, size_t count, qboolean usebpp, qboolean userefreshrate, qboolean useaspect)
{
	size_t i;
	if(count == 0)
		return 0;
	// 1. sort them
	qsort(modes, count, sizeof(*modes), VID_SortModes_Compare);
	// 2. remove duplicates
	for(i = 0; i < count; ++i)
	{
		if(modes[i].width && modes[i].height)
		{
			if(i == 0)
				continue;
			if(modes[i].width != modes[i-1].width)
				continue;
			if(modes[i].height != modes[i-1].height)
				continue;
			if(userefreshrate)
				if(modes[i].refreshrate != modes[i-1].refreshrate)
					continue;
			if(usebpp)
				if(modes[i].bpp != modes[i-1].bpp)
					continue;
			if(useaspect)
				if(modes[i].pixelheight_num * modes[i-1].pixelheight_denom != modes[i].pixelheight_denom * modes[i-1].pixelheight_num)
					continue;
		}
		// a dupe, or a bogus mode!
		if(i < count-1)
			memmove(&modes[i], &modes[i+1], sizeof(*modes) * (count-1 - i));
		--i; // check this index again, as mode i+1 is now here
		--count;
	}
	return count;
}

void VID_Soft_SharedSetup(void)
{
	gl_platform = "DPSOFTRAST";
	gl_platformextensions = "";

	gl_renderer = "DarkPlaces-Soft";
	gl_vendor = "Forest Hale";
	gl_version = "0.0";
	gl_extensions = "";

	// clear the extension flags
	memset(&vid.support, 0, sizeof(vid.support));
	Cvar_SetQuick(&gl_info_extensions, "");

	// DPSOFTRAST requires BGRA
	vid.forcetextype = TEXTYPE_BGRA;

	vid.forcevbo = false;
	vid.support.arb_depth_texture = true;
	vid.support.arb_draw_buffers = true;
	vid.support.arb_occlusion_query = true;
	vid.support.arb_shadow = true;
	//vid.support.arb_texture_compression = true;
	vid.support.arb_texture_cube_map = true;
	vid.support.arb_texture_non_power_of_two = false;
	vid.support.arb_vertex_buffer_object = true;
	vid.support.ext_blend_subtract = true;
	vid.support.ext_draw_range_elements = true;
	vid.support.ext_framebuffer_object = true;
	vid.support.ext_texture_3d = true;
	//vid.support.ext_texture_compression_s3tc = true;
	vid.support.ext_texture_filter_anisotropic = true;
	vid.support.ati_separate_stencil = true;
	vid.support.ext_texture_srgb = false;

	vid.maxtexturesize_2d = 16384;
	vid.maxtexturesize_3d = 512;
	vid.maxtexturesize_cubemap = 16384;
	vid.texunits = 4;
	vid.teximageunits = 32;
	vid.texarrayunits = 8;
	vid.max_anisotropy = 1;
	vid.maxdrawbuffers = 4;

	vid.texunits = bound(4, vid.texunits, MAX_TEXTUREUNITS);
	vid.teximageunits = bound(16, vid.teximageunits, MAX_TEXTUREUNITS);
	vid.texarrayunits = bound(8, vid.texarrayunits, MAX_TEXTUREUNITS);
	Con_DPrintf("Using DarkPlaces Software Rasterizer rendering path\n");
	vid.renderpath = RENDERPATH_SOFT;
	vid.sRGBcapable2D = false;
	vid.sRGBcapable3D = false;
	vid.useinterleavedarrays = false;

	Cvar_SetQuick(&gl_info_vendor, gl_vendor);
	Cvar_SetQuick(&gl_info_renderer, gl_renderer);
	Cvar_SetQuick(&gl_info_version, gl_version);
	Cvar_SetQuick(&gl_info_platform, gl_platform ? gl_platform : "");
	Cvar_SetQuick(&gl_info_driver, gl_driver);

	// LordHavoc: report supported extensions
	Con_DPrintf("\nQuakeC extensions for server and client: %s\nQuakeC extensions for menu: %s\n", vm_sv_extensions, vm_m_extensions );

	// clear to black (loading plaque will be seen over this)
	GL_Clear(GL_COLOR_BUFFER_BIT, NULL, 1.0f, 128);
}
