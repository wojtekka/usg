#ifndef __USG_H
#define __USG_H

#include "dynstuff.h"

enum client_state_t {
	STATE_NONE = 0,
	STATE_LISTENING,
	STATE_LOGIN,
};

typedef struct {
	int fd;		/* socket klienta */
	int uin;	/* numerek */
	int state;	/* stan po��czenia */
	int seed;	/* seed */
	int status;	/* stan klienta */
	char *status_descr;	/* opis stanu */
	char *ibuf;	/* bufor wej�ciowy */
	int ibuf_len;
	char *obuf;	/* bufor wyj�ciowy */
	int obuf_len;
	unsigned long ip;	/* adres */
	unsigned short port;	/* port */
	int version;	/* wersja protoko�u */
	int timeout;	/* timeout */
	list_t friends;	/* lista znajomych */
	int last_ping;	/* czas ostatniego pinga */
	int remove;	/* usun�� po wys�aniu danych? */
} client_t;

#define FLAG_BLOCKED 4

typedef struct {
	int uin;
	int flags;
} friend_t;

list_t clients;

#endif