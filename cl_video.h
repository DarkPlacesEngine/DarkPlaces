
#ifndef CL_VIDEO_H
#define CL_VIDEO_H

extern int cl_videoplaying;
void CL_VideoFrame(void);
void CL_DrawVideo(void);
void CL_VideoStart(char *filename);
void CL_VideoStop(void);
void CL_Video_Init(void);

#endif
