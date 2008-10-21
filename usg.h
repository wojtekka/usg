#ifndef __USG_H
#define __USG_H

#include "dynstuff.h"
#include "msgqueue.h"

enum client_state_t {
	STATE_NONE = 0,
	STATE_LISTENING,
	STATE_LOGIN,
	STATE_LOGIN_OK
};

typedef struct client {
	int fd;			/* socket klienta */
	int uin;		/* numerek */
	int state;		/* stan po³±czenia */
	int seed;		/* seed */

	string_t ibuf;		/* bufor wej¶ciowy */
	string_t obuf;		/* bufor wyj¶ciowy */

	int status;		/* stan klienta */
	int status_private;	/* czy stan tylko dla znajomych? */		/* XXX */
	char *status_descr;	/* opis stanu */

	unsigned long ip;		/* adres */
	unsigned short port;		/* port */
	unsigned char image_size;	/* maksymalny rozmiar grafiki w KiB */
	int version;			/* wersja protoko³u */
	int timeout;			/* timeout */
	list_t friends;			/* lista znajomych */
	int last_ping;			/* czas ostatniego pinga */
	int remove;			/* usun±æ po wys³aniu danych? */

	void (*status_write)(struct client *, struct client *);		/* odpowiednie GG_STATUS* */		/* XXX */
	void (*notify_reply)(struct client *, int);			/* odpowiednie GG_NOTIFY_REPLY* */	/* XXX */
	void (*msg_send)(struct client *, msgqueue_t *);
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

client_t *find_client(int uin);
void write_client(client_t *c, void *buf, int len);
void write_full_packet(client_t *c, int type, void *buf, int len);

#endif
