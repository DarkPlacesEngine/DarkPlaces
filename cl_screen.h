
#ifndef CL_SCREEN_H
#define CL_SCREEN_H

void SHOWLMP_decodehide(void);
void SHOWLMP_decodeshow(void);
void SHOWLMP_drawall(void);

extern cvar_t vid_conwidth;
extern cvar_t vid_conheight;
extern cvar_t vid_pixelheight;
extern cvar_t scr_screenshot_jpeg;
extern cvar_t scr_screenshot_jpeg_quality;
extern cvar_t scr_screenshot_png;
extern cvar_t scr_screenshot_gammaboost;
extern cvar_t scr_screenshot_name;

void CL_Screen_NewMap(void);
void CL_Screen_Init(void);
void CL_Screen_Shutdown(void);
void CL_UpdateScreen(void);

qboolean R_Stereo_Active(void);
qboolean R_Stereo_ColorMasking(void);

#endif

