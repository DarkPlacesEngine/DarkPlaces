
extern cvar_t vid_gamma;
extern cvar_t vid_brightness;
extern cvar_t vid_contrast;

extern unsigned int d_8to24table[256];
//extern byte d_15to8table[32768];

extern qboolean hardwaregammasupported;

void VID_UpdateGamma(qboolean force);

// used by hardware gamma functions in vid_* files
void BuildGammaTable8(float prescale, float gamma, float scale, float base, byte *out);
void BuildGammaTable16(float prescale, float gamma, float scale, float base, unsigned short *out);

void Gamma_Init(void);
void Palette_Init(void);
