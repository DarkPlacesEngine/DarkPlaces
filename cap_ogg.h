#include "qtypes.h"

#ifdef CAP_OGG_OPUS
#ifdef _MSC_VER
typedef __int64 ogg_int64_t;
#else
typedef long long ogg_int64_t;
#endif
#endif

void SCR_CaptureVideo_Ogg_Init(void);
qbool SCR_CaptureVideo_Ogg_Available(void);
void SCR_CaptureVideo_Ogg_BeginVideo(void);
void SCR_CaptureVideo_Ogg_CloseDLL(void);
