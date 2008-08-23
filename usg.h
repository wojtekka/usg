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
	int state;	/* stan po³±czenia */
	int seed;	/* seed */
	int status;	/* stan klienta */
	char *status_descr;	/* opis stanu */
	string_t ibuf;	/* bufor wej¶ciowy */
	string_t obuf;	/* bufor wyj¶ciowy */
	unsigned long ip;	/* adres */
	unsigned short port;	/* port */
	int version;	/* wersja protoko³u */
	int timeout;	/* timeout */
	list_t friends;	/* lista znajomych */
	int last_ping;	/* czas ostatniego pinga */
	int remove;	/* usun±æ po wys³aniu danych? */
} client_t;

enum friend_state_t {
	GG_USER_NORMAL = 0x3,
	GG_USER_BLOCKED,
};

typedef struct {
	int uin;
	int flags;
} friend_t;

list_t clients;

#endif
