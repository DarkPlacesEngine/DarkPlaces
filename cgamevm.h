
#ifndef CGAMEVM_H
#define CGAMEVM_H

void CL_CGVM_Init(void);
void CL_CGVM_Clear(void);
void CL_CGVM_Frame(void);
void CL_CGVM_Start(void);
void CL_CGVM_ParseNetwork(byte *netbuffer, int length);

#endif

