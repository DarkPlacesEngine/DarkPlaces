
#ifndef PALLETE_H
#define PALLETE_H

extern cvar_t v_gamma;
extern cvar_t v_contrast;
extern cvar_t v_brightness;
extern cvar_t v_overbrightbits;
extern cvar_t v_hwgamma;

extern unsigned int palette_complete[256];
extern unsigned int palette_nofullbrights[256];
extern unsigned int palette_onlyfullbrights[256];
extern unsigned int palette_nocolormapnofullbrights[256];
extern unsigned int palette_nocolormap[256];
extern unsigned int palette_pantsaswhite[256];
extern unsigned int palette_shirtaswhite[256];
extern unsigned int palette_alpha[256];
extern unsigned int palette_font[256];

extern qboolean hardwaregammasupported;

void VID_UpdateGamma(qboolean force);

// used by hardware gamma functions in vid_* files
void BuildGammaTable8(float prescale, float gamma, float scale, float base, qbyte *out);
void BuildGammaTable16(float prescale, float gamma, float scale, float base, unsigned short *out);

void Gamma_Init(void);
void Palette_Init(void);

#endif

