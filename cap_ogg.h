#include "snd_main.h"
void SCR_CaptureVideo_Ogg_Init();
qboolean SCR_CaptureVideo_Ogg_Available();
void SCR_CaptureVideo_Ogg_Begin();
void SCR_CaptureVideo_Ogg_EndVideo();
void SCR_CaptureVideo_Ogg_VideoFrame();
void SCR_CaptureVideo_Ogg_SoundFrame(const portable_sampleframe_t *paintbuffer, size_t length);
void SCR_CaptureVideo_Ogg_CloseDLL();
