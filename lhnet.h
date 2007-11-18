
// Written by Forest Hale 2003-06-15 and placed into public domain.

#ifndef LHNET_H
#define LHNET_H

#define LHNETADDRESSTYPE_NONE 0
#define LHNETADDRESSTYPE_LOOP 1
#define LHNETADDRESSTYPE_INET4 2
#define LHNETADDRESSTYPE_INET6 3

typedef struct lhnetaddress_loop_s
{
	unsigned short port;
}
lhnetaddress_loop_t;

#define LHNETADDRESSTYPE_INET4_FAMILY 2
#ifdef WIN32
#define LHNETADDRESSTYPE_INET6_FAMILY 23
#else
#define LHNETADDRESSTYPE_INET6_FAMILY 10
#endif

// compatible with sockaddr_in
typedef struct lhnetaddress_inet4_s
{
	unsigned short family; // 2
	unsigned short port;
	unsigned char address[4];
	unsigned char padding[8]; // to match sockaddr_in
}
lhnetaddress_inet4_t;

// compatible with sockaddr_in6
typedef struct lhnetaddress_inet6_s
{
	unsigned short family; // 10
	unsigned short port;
	unsigned int flowinfo;
	unsigned short address[8];
	unsigned int scope_id;
}
lhnetaddress_inet6_t;

typedef struct lhnetaddress_s
{
	int addresstype;
	union
	{
		lhnetaddress_loop_t loop;
		lhnetaddress_inet4_t inet4;
		lhnetaddress_inet6_t inet6;
	}
	addressdata;
}
lhnetaddress_t;

int LHNETADDRESS_FromPort(lhnetaddress_t *address, int addresstype, int port);
int LHNETADDRESS_FromString(lhnetaddress_t *address, const char *string, int defaultport);
int LHNETADDRESS_ToString(const lhnetaddress_t *address, char *string, int stringbuffersize, int includeport);
int LHNETADDRESS_GetAddressType(const lhnetaddress_t *address);
int LHNETADDRESS_GetPort(const lhnetaddress_t *address);
int LHNETADDRESS_SetPort(lhnetaddress_t *address, int port);
int LHNETADDRESS_Compare(const lhnetaddress_t *address1, const lhnetaddress_t *address2);

typedef struct lhnetsocket_s
{
	lhnetaddress_t address;
	unsigned int inetsocket;
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

