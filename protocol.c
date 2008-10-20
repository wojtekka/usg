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

/* jeśli klient dopisał kogoś do swojej userlisty, odpowiadamy, jeśli jest */
void notify_reply(client_t *c, int uin) {
	client_t *f = find_client(uin);
	client_t dummy;
	char buf[100];
	struct gg_header h;
	struct gg_notify_reply n;

	if (uin == c->uin)
		return;

	memset(&dummy, 0, sizeof(dummy));
	
	/* jeśli nie jest połączony, zobacz, czy nie zostawił opisu */
	if (!f) {
		FILE *fd;

		f = &dummy;
		f->status = GG_STATUS_NOT_AVAIL;
		f->uin = uin;
		
		snprintf(buf, sizeof(buf), "reasons/%d", uin);
		
		if ((fd = fopen(buf, "r"))) {
			if (fgets(buf, sizeof(buf), fd)) {
				f->status = GG_STATUS_NOT_AVAIL_DESCR;
				f->status_descr = buf;
			}
			fclose(fd);
		}
	}

	if (f->status == GG_STATUS_NOT_AVAIL || f->status == GG_STATUS_INVISIBLE)
		return;
	
	if (f->status == GG_STATUS_NOT_AVAIL_DESCR || f->status == GG_STATUS_INVISIBLE_DESCR) {
		struct gg_status s;

		h.type = GG_STATUS;
		h.length = sizeof(s) + ((f->status_descr) ? strlen(f->status_descr) : 0);
		s.status = f->status;
		if (s.status == GG_STATUS_INVISIBLE_DESCR)
			s.status = GG_STATUS_NOT_AVAIL_DESCR;
		s.uin = f->uin;
		write_client(c, &h, sizeof(h));
		write_client(c, &s, sizeof(s));
		write_client(c, f->status_descr, strlen(f->status_descr));
	} else {
		h.type = GG_NOTIFY_REPLY;
		h.length = sizeof(n) + ((f->status_descr) ? strlen(f->status_descr) : 0);
		n.uin = f->uin;
		n.status = f->status;
		n.remote_ip = f->ip;
		n.remote_port = f->port;
		n.version = f->version;
		n.dunno2 = f->port;
	
		write_client(c, &h, sizeof(h));
		write_client(c, &n, sizeof(n));
		if (f->status_descr)
			write_client(c, f->status_descr, strlen(f->status_descr));
	}
}

static int gg_ping_handler(client_t *c, void *data, uint32_t len) {
	printf("received ping from %d\n", c->uin);

	c->last_ping = time(NULL);
	c->timeout = time(NULL) + 120;	/* + 2*timeout_ping */

	return 0;
}

static void gg_login_ok(client_t *c, uint32_t uin) {
	struct gg_header h;
	msgqueue_t m;

	printf("succeded!\n");

	if (find_client(uin)) {
		printf("duplicate client, removing previous\n");
		/* XXX kopie się */
		remove_client(find_client(uin));
	}

	c->uin = uin;
	c->state = STATE_LOGIN_OK;

	h.type = GG_LOGIN_OK;
	h.length = 0;
	write_client(c, &h, sizeof(h));

	while (!unqueue_message(c->uin, &m)) {
		struct gg_recv_msg r;
		friend_t *f = find_friend(c, m.sender);

		if (f)
			if (f->flags & GG_USER_BLOCKED)
				break;

		printf("sending queued message from %d to %d\n", m.sender, c->uin);
		h.type = GG_RECV_MSG;
		h.length = sizeof(r) + m.length;
		r.sender = m.sender;
		r.seq = m.seq;
		r.time = m.time;
		r.msgclass = m.msgclass;

		write_client(c, &h, sizeof(h));
		write_client(c, &r, sizeof(r));
		write_client(c, m.text, m.length);

		xfree(m.text);
	}
}

static void gg_login_failed(client_t *c) {
	struct gg_header h;

	h.type = GG_LOGIN_FAILED;
	h.length = 0;
	write_client(c, &h, sizeof(h));
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

	c->status = l->status;
	c->version = l->version;
	c->ip = l->local_ip;
	c->port = l->local_port;

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

static int gg_list_empty_handler(client_t *c, void *data, uint32_t len) {

	return 0;
}

static int gg_notify_handler(client_t *c, void *data, uint32_t len) {
	struct gg_notify *n = data;
	int i;

	printf("received notify list from %d\n", c->uin);

	for (i = 0; i < len / sizeof(*n); i++) {
		friend_t f;

		f.uin = n[i].uin;
		f.flags = n[i].dunno1;
		list_add(&c->friends, &f, sizeof(f));
		notify_reply(c, f.uin);
	}
	return 0;
}

static int gg_notify_add_handler(client_t *c, void *data, uint32_t len) {
	struct gg_add_remove *ar = data;
	friend_t f;

	printf("adding notify on %d for %d\n", ar->uin, c->uin);

	f.uin = ar->uin;
	f.flags = ar->dunno1;
	list_add(&c->friends, &f, sizeof(f));
	notify_reply(c, f.uin);

	return 0;
}

static int gg_notify_remove_handler(client_t *c, void *data, uint32_t len) {
	struct gg_add_remove *ar = data;
	list_t l;

	printf("removing notify from %d for %d\n", c->uin, ar->uin);

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
	struct gg_new_status *s = data;
	char buf[100];

	c->status = s->status;
	if (c->status_descr) {
		xfree(c->status_descr);
		c->status_descr = NULL;
	}
	if (len > sizeof(*s)) {
		c->status_descr = xmalloc(len - sizeof(*s) + 1);
		memcpy(c->status_descr, data + sizeof(*s), len - sizeof(*s));
		c->status_descr[len - sizeof(*s)] = 0;
	}

	printf("status change: uin=%d, status=%d, descr=\"%s\"\n", c->uin, c->status, c->status_descr);

	snprintf(buf, sizeof(buf), "reasons/%d", c->uin);

	if (c->status == GG_STATUS_NOT_AVAIL_DESCR) {
		FILE *f = fopen(buf, "w");

		if (f) {
			fputs(c->status_descr, f);
			fclose(f);
		}
	} else
		unlink(buf);

	changed_status(c);

	return 0;
}

static int gg_send_msg_handler(client_t *c, void *data, uint32_t len) {
	struct gg_send_msg *s = (struct gg_send_msg *) data;
	int ack = GG_ACK_QUEUED;
	struct gg_header h;
	struct gg_send_msg_ack a;
	client_t *rcpt;

	if (len < sizeof(struct gg_send_msg))
		return -2;

	if (!(rcpt = find_client(s->recipient))) {
		printf("enqueuing message from %d to %d\n", c->uin, s->recipient);
		enqueue_message(s->recipient, c->uin, s->seq, s->msgclass, data + sizeof(*s), len - sizeof(*s));
	} else {
		struct gg_recv_msg r;
		friend_t *f = find_friend(rcpt, c->uin);

		if (f) {
			if (f->flags & GG_USER_BLOCKED)
				return 0;
		}

		printf("sending message from %d to %d\n", c->uin, s->recipient);

		h.type = GG_RECV_MSG;
		h.length = sizeof(r) + (len - sizeof(*s));
		r.sender = c->uin;
		r.seq = s->seq;
		r.time = time(NULL);
		r.msgclass = s->msgclass;

		write_client(rcpt, &h, sizeof(h));
		write_client(rcpt, &r, sizeof(r));
		write_client(rcpt, data + sizeof(*s), len - sizeof(*s));
		ack = GG_ACK_DELIVERED;
	}

	printf("sending ack back to %d\n", c->uin);
	h.type = GG_SEND_MSG_ACK;
	h.length = sizeof(a);
	a.status = ack;
	a.recipient = s->recipient;
	a.seq = s->seq;
	write_client(c, &h, sizeof(h));
	write_client(c, &a, sizeof(a));

	return 0;
}

struct {
	uint32_t type;
	int (*handler)(client_t *c, void *data, uint32_t len);
	int flags;

} 
static const gg_handlers[] = 
{
	/* login */
	{ GG_LOGIN,	gg_login_handler },
	{ GG_LOGIN_EXT, gg_login_ext_handler },
	{ GG_LOGIN60,	gg_login60_handler },
	{ GG_LOGIN70,	gg_login70_handler },
	{ GG_LOGIN80,	gg_login80_handler },

	/* userlist */
	{ GG_LIST_EMPTY, 	gg_list_empty_handler },
	{ GG_NOTIFY,		gg_notify_handler },

	{ GG_ADD_NOTIFY,	gg_notify_add_handler },
	{ GG_REMOVE_NOTIFY,	gg_notify_remove_handler },

	{ GG_NEW_STATUS,gg_new_status_handler },

	/* wiadomosc */
	{ GG_SEND_MSG,	gg_send_msg_handler },

	/* rozne */
	{ GG_PING,	gg_ping_handler },

	{ 0, NULL }
};

/* obsługuje przychodzące pakiety */
void handle_input_packet(client_t *c) {
	struct gg_header *h = (struct gg_header*) c->ibuf->str;
	void *data = c->ibuf->str + sizeof(struct gg_header);
	int i;

	printf("uin %d, fd %d sent packet %.02x length %d\n", c->uin, c->fd, h->type, h->length);

	for (i = 0; gg_handlers[i].type; i++) {
		if (gg_handlers[i].type == h->type) {
			gg_handlers[i].handler(c, data, h->length);
			return;
		}
	}

	printf("received unknown packet [%.2x] from %d\n", h->type, c->uin);
	return;
}

