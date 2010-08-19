
// Written by Forest Hale 2003-06-15 and placed into public domain.

#ifndef LHNET_H
#define LHNET_H

typedef enum lhnetaddresstype_e
{
	LHNETADDRESSTYPE_NONE,
	LHNETADDRESSTYPE_LOOP,
	LHNETADDRESSTYPE_INET4,
	LHNETADDRESSTYPE_INET6
}
lhnetaddresstype_t;

typedef struct lhnetaddress_s
{
	lhnetaddresstype_t addresstype;
	int port; // used by LHNETADDRESSTYPE_LOOP
	unsigned char storage[256]; // sockaddr_in or sockaddr_in6
}
lhnetaddress_t;

int LHNETADDRESS_FromPort(lhnetaddress_t *address, lhnetaddresstype_t addresstype, int port);
int LHNETADDRESS_FromString(lhnetaddress_t *address, const char *string, int defaultport);
int LHNETADDRESS_ToString(const lhnetaddress_t *address, char *string, int stringbuffersize, int includeport);
int LHNETADDRESS_GetAddressType(const lhnetaddress_t *address);
const char *LHNETADDRESS_GetInterfaceName(const lhnetaddress_t *address);
int LHNETADDRESS_GetPort(const lhnetaddress_t *address);
int LHNETADDRESS_SetPort(lhnetaddress_t *address, int port);
int LHNETADDRESS_Compare(const lhnetaddress_t *address1, const lhnetaddress_t *address2);

typedef struct lhnetsocket_s
{
	lhnetaddress_t address;
	int inetsocket;
	struct lhnetsocket_s *next, *prev;
}
lhnetsocket_t;

void LHNET_Init(void);
void LHNET_Shutdown(void);
void LHNET_SleepUntilPacket_Microseconds(int microseconds);
lhnetsocket_t *LHNET_OpenSocket_Connectionless(lhnetaddress_t *address);
void LHNET_CloseSocket(lhnetsocket_t *lhnetsocket);
lhnetaddress_t *LHNET_AddressFromSocket(lhnetsocket_t *sock);
int LHNET_Read(lhnetsocket_t *lhnetsocket, void *content, int maxcontentlength, lhnetaddress_t *address);
int LHNET_Write(lhnetsocket_t *lhnetsocket, const void *content, int contentlength, const lhnetaddress_t *address);

#endif

