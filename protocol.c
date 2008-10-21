/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Maurycy Pawłowski <maurycy@bofh.szczecin.pl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <sys/ioctl.h>
#include "dynstuff.h"
#include "xmalloc.h"
#include "usg.h"
#include "protocol.h"
#include "auth.h"
#include "msgqueue.h"

static char motd_text[] = "Witaj na wolnym serwerze gg! (usg 0.2)";

client_t *get_client(client_t *c, int uin) {
	client_t *f;

	if (uin == c->uin)
		return NULL;

	/* jeśli nie jest połączony, zobacz, czy nie zostawił opisu */
	if (!(f = find_client(uin))) {
		static client_t dummy;
		FILE *fd;

		memset(&dummy, 0, sizeof(dummy));

		f = &dummy;
		f->uin = uin;
		f->status = GG_STATUS_NOT_AVAIL;
		f->status_descr = NULL;
		
		if ((fd = fopen(path_uin("reasons", uin), "r"))) {
			static char descr_buf[300];

			if (fgets(descr_buf, sizeof(descr_buf), fd)) {
				f->status = GG_STATUS_NOT_AVAIL_DESCR;
				f->status_descr = descr_buf;
			}
			fclose(fd);
		}
	}

	if (f->status == GG_STATUS_NOT_AVAIL || f->status == GG_STATUS_INVISIBLE)
		return NULL;
	
	return f;
}

static void gg77_notify_reply_data(client_t *ten, int uid) {
	struct gg_header h;
	struct gg_notify_reply77 n;
	client_t *c;

	int status;
	
	if (!(c = get_client(ten, uid)))
		return;

	if (c->status_private) {
		/* XXX */
	}

	if (c->status == GG_STATUS_INVISIBLE)
		status = GG_STATUS_NOT_AVAIL;
	else if (c->status == GG_STATUS_INVISIBLE_DESCR)
		status = GG_STATUS_NOT_AVAIL_DESCR;
	else
		status = c->status;

	h.type = GG_NOTIFY_REPLY77;
	h.length = sizeof(n) + ((c->status_descr) ? 1+strlen(c->status_descr)+1 : 0);

	n.uin = c->uin;
	n.status = status;
	n.remote_ip = c->ip;
	n.remote_port = c->port;
	n.version = c->version;
	n.image_size = c->image_size;
	n.dunno1 = 0x00;		/* ? */
	n.dunno2 = 0x00;		/* ? */

	write_client(ten, &h, sizeof(h));
	write_client(ten, &n, sizeof(n));
	if (c->status_descr) {
		unsigned char ile = strlen(c->status_descr);

		write_client(ten, &ile, 1);
		write_client(ten, c->status_descr, strlen(c->status_descr)+1);
	}
}

static void gg77_notify_reply(client_t *c, int uid) {
	list_t l;

	if (!uid) {
		/* XXX */
		for (l = c->friends; l; l = l->next) {
			friend_t *f = l->data;

			gg77_notify_reply_data(c, f->uin);
		}
	} else {

		gg77_notify_reply_data(c, uid);
	}
}

static void gg77_status_write(client_t *ten, client_t *c) {
	struct gg_header h;
	struct gg_status77 s;

	int status = c->status;

	if (c->status_private) {
		/* XXX */
	}

	if (status == GG_STATUS_INVISIBLE)
		status = GG_STATUS_NOT_AVAIL;

	if (status == GG_STATUS_INVISIBLE_DESCR)
		status = GG_STATUS_NOT_AVAIL_DESCR;

	h.type = GG_STATUS77;
	h.length = sizeof(s) + ((c->status_descr) ? strlen(c->status_descr) : 0);

	s.uin = c->uin;
	s.status = status;
	s.remote_ip = c->ip;		/* XXX */
	s.remote_port = c->port;	/* XXX */
	s.version = c->version;
	s.image_size = c->image_size;
	s.dunno1 = 0x00;		/* ? */
	s.dunno2 = 0x00;		/* ? */

	write_client(ten, &h, sizeof(h));
	write_client(ten, &s, sizeof(s));
	if (c->status_descr)
		write_client(ten, c->status_descr, strlen(c->status_descr));
}

/* */

static int gg_ping_handler(client_t *c, void *data, uint32_t len) {
	printf("received ping from %d\n", c->uin);

	c->last_ping = time(NULL);
	c->timeout = time(NULL) + TIMEOUT_DEFAULT;

	return 0;
}

static void gg_msg_send(client_t *c, msgqueue_t *m) {
	struct gg_header h;
	struct gg_recv_msg r;

	friend_t *f;

	if ((f = find_friend(c, m->sender))) {
		if (f->flags & GG_USER_BLOCKED)
			return;
	}

	h.type = GG_RECV_MSG;
	h.length = sizeof(r) + m->length;
	r.sender = m->sender;
	r.seq = m->seq;
	r.time = m->time;
	r.msgclass = m->msgclass;

	write_client(c, &h, sizeof(h));
	write_client(c, &r, sizeof(r));
	write_client(c, m->text, m->length);
}

static void gg_msg_send80(client_t *c, msgqueue_t *m) {	// not tested/working
	struct gg_header h;
	struct gg_recv_msg80 r;

	friend_t *f;

	if ((f = find_friend(c, m->sender))) {
		if (f->flags & GG_USER_BLOCKED)
			return;
	}

	h.type = GG_RECV_MSG80;
	h.length = sizeof(r) + m->length;
	r.sender = m->sender;
	r.seq = m->seq;
	r.time = m->time;
	r.msgclass = m->msgclass;
	r.offset_plain = 0;		// XXX
	r.offset_attr = 0;		// XXX

	write_client(c, &h, sizeof(h));
	write_client(c, &r, sizeof(r));
	write_client(c, m->text, m->length);
}

static void gg_login_ok(client_t *c, uint32_t uin) {
	client_t *old_client;
	msgqueue_t m;

	printf("succeded!\n");

	if ((old_client = find_client(uin))) {
		printf("duplicate client, removing previous\n");

		write_full_packet(old_client, GG_DISCONNECTING, NULL, 0);
		old_client->remove = 1;
//		remove_client(old_client);	// XXX?
	}

	c->uin = uin;
	c->state = STATE_LOGIN_OK;
	c->timeout = time(NULL) + TIMEOUT_DEFAULT;

	write_full_packet(c, GG_LOGIN_OK, NULL, 0);

	/* XXX */
/*
	c->status_write = gg_status_write;
	c->status_write = gg60_status_write;
	c->status_write = gg80_status_write;

	c->msg_send	= gg80_msg_send;
*/

	c->status_write = gg77_status_write;
	c->notify_reply = gg77_notify_reply;
	c->msg_send	= gg_msg_send;

	/* motd */
	{
		msgqueue_t motd;

		motd.sender = 0;
		motd.time = 0;
		motd.seq = 0;
		motd.msgclass = GG_CLASS_MSG;
		motd.text = motd_text;
		motd.length = sizeof(motd_text);

		c->msg_send(c, &motd);
	}

	/* XXX, sprawdzic czy w dobrym miejscu */
	while (!unqueue_message(c->uin, &m)) {
		printf("sending queued message from %d to %d\n", m.sender, c->uin);

		c->msg_send(c, &m);

		free(m.text);
	}
}

static void gg_login_failed(client_t *c) {
	write_full_packet(c, GG_LOGIN_FAILED, NULL, 0);

	printf("failed, sending reject\n");
	c->remove = 1;
}

static int gg_login_handler(client_t *c, void *data, uint32_t len) {
	struct gg_login *l = (struct gg_login *) data;

	if (len < sizeof(struct gg_login))
		return -2;

	if (c->state != STATE_LOGIN) {
		printf("gg_login_handler() c->state = %d\n", c->state);
		return -3;
	}

	printf("login attempt from %d\n", l->uin);

	if (!authorize(l->uin, c->seed, l->hash)) {
		gg_login_failed(c);
		return -1;
	}

	gg_login_ok(c, l->uin);

	c->status = l->status;
	c->version = l->version;
	c->ip = l->local_ip;
	c->port = l->local_port;

	changed_status(c);
	return 0;
}

static int gg_login_ext_handler(client_t *c, void *data, uint32_t len) {
	struct gg_login_ext *l = (struct gg_login_ext *) data;

	if (len < sizeof(struct gg_login_ext))
		return -2;

	if (c->state != STATE_LOGIN) {
		printf("gg_login_handler() c->state = %d\n", c->state);
		return -3;
	}

	printf("login_ext attempt from %d\n", l->uin);

	if (!authorize(l->uin, c->seed, l->hash)) {
		gg_login_failed(c);
		return -1;
	}

	gg_login_ok(c, l->uin);

	c->status = l->status;
	c->version = l->version;
	c->ip = l->local_ip;
	c->port = l->local_port;

	changed_status(c);
	return 0;
}

static int gg_login60_handler(client_t *c, void *data, uint32_t len) {
	struct gg_login60 *l = (struct gg_login60 *) data;

	if (len < sizeof(struct gg_login60))
		return -2;

	if (c->state != STATE_LOGIN) {
		printf("gg_login_handler() c->state = %d\n", c->state);
		return -3;
	}

	printf("login60 attempt from %d\n", l->uin);

	if (!authorize(l->uin, c->seed, l->hash)) {
		gg_login_failed(c);
		return -1;
	}

	gg_login_ok(c, l->uin);

	c->status = l->status;
	c->version = l->version;
	c->ip = l->local_ip;
	c->port = l->local_port;

	changed_status(c);
	return 0;
}

static int gg_login70_handler(client_t *c, void *data, uint32_t len) {
	struct gg_login70 *l = (struct gg_login70 *) data;

	if (len < sizeof(struct gg_login70))
		return -2;

	if (c->state != STATE_LOGIN) {
		printf("gg_login_handler() c->state = %d\n", c->state);
		return -3;
	}

	printf("login70 attempt from %d\n", l->uin);

	if (!authorize70(l->uin, c->seed, l->hash_type, l->hash)) {
		gg_login_failed(c);
		return -1;
	}

	gg_login_ok(c, l->uin);

	c->status	= l->status;
	c->version	= l->version;
	c->ip		= l->local_ip;
	c->port		= l->local_port;
	c->image_size	= l->image_size;

	/* XXX, dunno1: 0x00, dunno2: 0xbe, external_ip, external_port */
	/* XXX, status */

	changed_status(c);
	return 0;
}

static int gg_login80_handler(client_t *c, void *data, uint32_t len) {
	struct gg_login70 *l = (struct gg_login70 *) data;

	if (len < sizeof(struct gg_login70))
		return -2;

	if (c->state != STATE_LOGIN) {
		printf("gg_login_handler() c->state = %d\n", c->state);
		return -3;
	}

	printf("login80 attempt from %d\n", l->uin);

	if (!authorize70(l->uin, c->seed, l->hash_type, l->hash)) {
		gg_login_failed(c);
		return -1;
	}

	gg_login_ok(c, l->uin);

	c->status = l->status;
	c->version = l->version;
	c->ip = l->local_ip;
	c->port = l->local_port;

	changed_status(c);
	return 0;
}

static int gg_notify_handler(client_t *c, void *data, uint32_t len) {
	struct gg_notify *n = data;
	int i;

	if (c->state != STATE_LOGIN_OK) {
		printf("gg_notify_handler() c->state = %d\n", c->state);
		return -3;
	}

	printf("received notify list from %d\n", c->uin);

	for (i = 0; i < len / sizeof(*n); i++) {
		friend_t f;

		f.uin = n[i].uin;
		f.flags = n[i].dunno1;
		list_add(&c->friends, &f, sizeof(f));
	}
	return 0;
}

static int gg_notify_end_handler(client_t *c, void *data, uint32_t len) {
	if (gg_notify_handler(c, data, len) == 0) {
		c->notify_reply(c, 0);
		return 0;
	}

	return -1;
}

static int gg_list_empty_handler(client_t *c, void *data, uint32_t len) {
	if (c->state != STATE_LOGIN_OK) {
		printf("gg_list_empty_handler() c->state = %d\n", c->state);
		return -3;
	}

	return 0;
}

static int gg_notify_add_handler(client_t *c, void *data, uint32_t len) {
	struct gg_add_remove *ar = (struct gg_add_remove *) data;
	friend_t f;

	if (len < sizeof(struct gg_add_remove))
		return -1;

	if (c->state != STATE_LOGIN_OK) {
		printf("gg_notify_add_handler() c->state = %d\n", c->state);
		return -3;
	}

	printf("adding notify on %d for %d\n", ar->uin, c->uin);

	if (ar->uin == 0) {
		/* oryginalny serwer nie wiadomo co robi, my po prostu ignorujemy taki pakiet */
		return 1;
	}

	/* XXX, czy moze wystepowac wiecej niz 1 numerek w pakiecie? 
	 * 	libgadu nie umie wysylac, potestowac z oryginalnym serwerem
	 */

	f.uin = ar->uin;
	f.flags = ar->dunno1;

	list_add(&c->friends, &f, sizeof(f));
	c->notify_reply(c, f.uin);

	return 0;
}

static int gg_notify_remove_handler(client_t *c, void *data, uint32_t len) {
	struct gg_add_remove *ar = (struct gg_add_remove *) data;
	list_t l;

	if (len < sizeof(struct gg_add_remove))
		return -1;

	if (c->state != STATE_LOGIN_OK) {
		printf("gg_notify_remove_handler() c->state = %d\n", c->state);
		return -3;
	}

	printf("removing notify from %d for %d\n", c->uin, ar->uin);

	/* XXX, tak samo czy moze wystepowac wiecej niz 1 numerek w pakiecie? */

	for (l = c->friends; l; l = l->next) {
		friend_t *f = l->data;

		if (f->uin == ar->uin) {
			list_remove(&c->friends, f, 1);
			return 0;
		}
	}
	return 1;
}

static int gg_new_status_handler(client_t *c, void *data, uint32_t len) {
	struct gg_new_status *s = (struct gg_new_status *) data;
	int status;

	status = s->status;

	/* XXX, sprawdzac protokol? */

	if (status & GG_STATUS_VOICE_MASK) {
		status &= ~GG_STATUS_VOICE_MASK;
		/* XXX */
	}

	if (status & GG_STATUS_FRIENDS_MASK) {
		status &= ~GG_STATUS_FRIENDS_MASK;
		c->status_private = 1;
	} else
		c->status_private = 0;

	c->status = status;

	if (c->status_descr) {
		free(c->status_descr);
		c->status_descr = NULL;
	}
	if (len > sizeof(*s)) {
		c->status_descr = xmalloc(len - sizeof(*s) + 1);
		memcpy(c->status_descr, data + sizeof(*s), len - sizeof(*s));
		c->status_descr[len - sizeof(*s)] = 0;
	}

	printf("status change: uin=%d, status=%d, descr=\"%s\"\n", c->uin, c->status, c->status_descr);

	if (c->status == GG_STATUS_NOT_AVAIL_DESCR) {
		FILE *f;

		if ((f = fopen(path_uin("reasons", c->uin), "w"))) {
			fputs(c->status_descr, f);
			fclose(f);
		}
	} else
		unlink(path_uin("reasons", c->uin));

	changed_status(c);

	if (c->status == GG_STATUS_NOT_AVAIL_DESCR || c->status == GG_STATUS_NOT_AVAIL) {
		/* rozlaczamy klienta */
		
		write_full_packet(c, 0x0d, NULL, 0);
		c->remove = 1;
	}

	return 0;
}

static int gg_userlist_req_handler(client_t *c, void *data, uint32_t len) {
	return -2;
}

static int gg_pubdir50_req_handler(client_t *c, void *data, uint32_t len) {

	return -2;
}

static int gg_send_msg_handler(client_t *c, void *data, uint32_t len) {
	struct gg_send_msg *s = (struct gg_send_msg *) data;
	struct gg_send_msg_ack a;
	client_t *rcpt;
	int ack;

	if (len < sizeof(struct gg_send_msg))
		return -2;

	if (c->state != STATE_LOGIN_OK) {
		printf("gg_send_msg_handler() c->state = %d\n", c->state);
		return -3;
	}

	if (!(rcpt = find_client(s->recipient))) {
		printf("enqueuing message from %d to %d\n", c->uin, s->recipient);
		enqueue_message(s->recipient, c->uin, s->seq, s->msgclass, data + sizeof(*s), len - sizeof(*s));

		ack= GG_ACK_QUEUED;
	} else {
		msgqueue_t m;

		printf("sending message from %d to %d\n", c->uin, s->recipient);

		m.sender = c->uin;
		m.seq = s->seq;
		m.time = time(NULL);
		m.msgclass = s->msgclass;

		m.text = data + sizeof(*s);
		m.length = (len - sizeof(*s));

		rcpt->msg_send(rcpt, &m);

		ack = GG_ACK_DELIVERED;
	}

	printf("sending ack back to %d\n", c->uin);
	a.status = ack;
	a.recipient = s->recipient;
	a.seq = s->seq;

	write_full_packet(c, GG_SEND_MSG_ACK, &a, sizeof(a));

	return 0;
}

struct {
	uint32_t type;
	int (*handler)(client_t *c, void *data, uint32_t len);
	int flags;

} 
static const gg_handlers[] = 
{
	/* logowanie */
	{ GG_LOGIN,	gg_login_handler },
	{ GG_LOGIN_EXT, gg_login_ext_handler },
	{ GG_LOGIN60,	gg_login60_handler },
	{ GG_LOGIN70,	gg_login70_handler },				/* almost-ok */
	{ GG_LOGIN80,	gg_login80_handler },

	/* userlista */
	{ GG_NOTIFY_FIRST,	gg_notify_handler },
	{ GG_NOTIFY_LAST,	gg_notify_end_handler },
	{ GG_LIST_EMPTY, 	gg_list_empty_handler },
	{ GG_ADD_NOTIFY,	gg_notify_add_handler },		/* ok */
	{ GG_REMOVE_NOTIFY,	gg_notify_remove_handler },		/* ok */

	/* statusy.. */
	{ GG_NEW_STATUS,	gg_new_status_handler },
	/* { GG_NEW_STATUS80,	gg_new_status80_handler }, */

	/* katalog */
	{ GG_USERLIST_REQUEST,	gg_userlist_req_handler },
	{ GG_PUBDIR50_REQUEST,	gg_pubdir50_req_handler },

	/* wiadomosci */
	{ GG_SEND_MSG,		gg_send_msg_handler },
	/* { GG_SEND_MSG80,	gg_send_msg80_handler }, */

	/* rozne */
	{ GG_PING,	gg_ping_handler },

	/* dcc7.x */
/*
	{ GG_DCC7_NEW,		(void *) sniff_gg_dcc7_new, 0}, 
	{ GG_DCC7_ID_REQUEST,	(void *) sniff_gg_dcc7_new_id_request, 0},
	{ GG_DCC7_REJECT,	(void *) sniff_gg_dcc7_reject, 0},
	{ GG_DCC_ACCEPT,	(void *) sniff_gg_dcc7_accept, 0}, 
	{ GG_DCC_2XXX,		(void *) sniff_gg_dcc_2xx_out, 0},
	{ GG_DCC_3XXX,		(void *) sniff_gg_dcc_3xx_out, 0},
	{ GG_DCC_4XXX,		(void *) sniff_gg_dcc_4xx_out, 0},
*/
	{ 0, NULL }
};

/* obsługuje przychodzące pakiety */
void handle_input_packet(client_t *c) {
	struct gg_header *h = (struct gg_header*) c->ibuf->str;
	void *data = c->ibuf->str + sizeof(struct gg_header);
	int i;

	printf("uin %d, fd %d sent packet %.02x length %d\n", c->uin, c->fd, h->type, h->length);

	if (c->remove) {
		printf("(removed?)\n");
		return;
	}

	for (i = 0; gg_handlers[i].type; i++) {
		if (gg_handlers[i].type == h->type) {
			gg_handlers[i].handler(c, data, h->length);
			return;
		}
	}

	printf("received unknown packet [%.2x] from %d\n", h->type, c->uin);
	return;
}

