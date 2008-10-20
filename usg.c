/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Maurycy Paw³owski <maurycy@bofh.szczecin.pl>
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

list_t clients = NULL;

/* wysy³a dane do danego klienta */
void write_client(client_t *c, void *buf, int len)
{
	string_append_raw(c->obuf, (char *) buf, len);
}

void write_full_packet(client_t *c, int type, void *buf, int len)
{
	struct gg_header h;

	h.type = type;
	h.length = len;

	write_client(c, &h, sizeof(h));
	write_client(c, buf, len);
}

/* wysy³a dane do wszystkich, którzy maj± go w userli¶cie */
void write_client_friends(client_t *c, void *buf, int len)
{
	list_t l, k;
	
	for (l = clients; l; l = l->next) {
		client_t *i = l->data;
		
		if (c == i)
			continue;
		
		for (k = i->friends; k; k = k->next) {
			friend_t *f = k->data;

			if (f->uin == c->uin && !(f->flags & GG_USER_BLOCKED)) {
				write_client(i, buf, len);
				break;
			}
		}
	}
}

/* szuka klienta o podanym numerku */
client_t *find_client(int uin)
{
	list_t l;

	for (l = clients; l; l = l->next) {
		client_t *c = l->data;

		if (c->uin == uin)
			return c;
	}

	return NULL;
}

/* szuka u danego klienta przyjaciela o podanym numerku */
friend_t *find_friend(client_t *c, int uin)
{
	list_t l;

	for (l = c->friends; l; l = l->next) {
		friend_t *f = l->data;

		if (f->uin == uin)
			return f;
	}

	return NULL;
}

/* usuwa klienta, zamyka po³±czneie itd. */
void remove_client(client_t *c)
{
	if (c->state == STATE_LOGIN_OK &&
		(c->status != GG_STATUS_NOT_AVAIL && c->status != GG_STATUS_NOT_AVAIL_DESCR && c->status != GG_STATUS_INVISIBLE && c->status != GG_STATUS_INVISIBLE_DESCR))
	{
			c->status = (c->status_descr) ? GG_STATUS_NOT_AVAIL_DESCR : GG_STATUS_NOT_AVAIL;
			changed_status(c);
	}

	shutdown(c->fd, 2);
	close(c->fd);
	string_free(c->ibuf, 1);
	string_free(c->obuf, 1);
	list_remove(&clients, c, 1);
}

/* wysy³a do ludzi, którzy maj± go w userli¶cie informacjê o zmianie stanu */
void changed_status(client_t *c)
{
	struct gg_header h;
	struct gg_status s;

	h.type = GG_STATUS;
	h.length = sizeof(s) + ((c->status_descr) ? strlen(c->status_descr) : 0);
	s.status = c->status;
	s.uin = c->uin;
	
	if (s.status == GG_STATUS_INVISIBLE)
		s.status = GG_STATUS_NOT_AVAIL;

	if (s.status == GG_STATUS_INVISIBLE_DESCR)
		s.status = GG_STATUS_NOT_AVAIL_DESCR;
	
	write_client_friends(c, &h, sizeof(h));
	write_client_friends(c, &s, sizeof(s));
	if (c->status_descr)
		write_client_friends(c, c->status_descr, strlen(c->status_descr));
}


/* obs³uguje przychodz±ce po³±cznia */
int handle_connection(client_t *c)
{
	client_t n;
	struct sockaddr_in sin;
	int fd, sin_len = sizeof(sin), nb = 1;
	struct gg_welcome w;

	if ((fd = accept(c->fd, (struct sockaddr*) &sin, &sin_len)) == -1) {
		perror("accept");
		return 0;
	}

	if (ioctl(fd, FIONBIO, &nb) == -1) {
		perror("ioctl");
		close(fd);
		return 0;
	}
	
	memset(&n, 0, sizeof(n));
	n.fd = fd;
	n.state = STATE_LOGIN;
	n.timeout = time(NULL) + TIMEOUT_CONNECT;

	n.seed = w.key = random();
	n.ibuf = string_init(NULL);
	n.obuf = string_init(NULL);

	write_full_packet(&n, GG_WELCOME, &w, sizeof(w));

	list_add(&clients, &n, sizeof(n));

	return 0;
}

extern void handle_input_packet(client_t *c);		/* protocol.c */

/* gdy przychodz± dane, dopisz do bufora i wywo³aj obs³ugê pakietów, je¶li
 * jaki¶ uda³o siê ju¿ posk³adaæ */
int handle_input(client_t *c)
{
	char buf[1024];
	int res, first = 1;

	while (1) {
		res = read(c->fd, buf, sizeof(buf));
		printf("read: fd=%d, res=%d\n", c->fd, res);

		if (res > 0)
			string_append_raw(c->ibuf, buf, res);

		if (res == -1) {
			if (errno == EAGAIN)
				break;
			if (errno == EINTR)
				continue;
			perror("read");
			remove_client(c);
			return 1;
		}

		if (res == 0 && first) {
			remove_client(c);
			return 1;
		}

		if (res == 0)
			break;

		first = 0;
	}

	printf("      ibuf->len = %d\n", c->ibuf->len);

	while (c->ibuf->len >= 8) {
		struct gg_header *h = (struct gg_header*) c->ibuf->str;
		
		if (h->length < 0 || h->length > 2500) {
			remove_client(c);
			return 1;
		}
		
		if (c->ibuf->len >= 8 + h->length) {
			handle_input_packet(c);

			string_remove(c->ibuf, 8 + h->length);
		} else
			break;	/* czekaj na wiecej danych.. */
	}

	return 0;
}

/* je¶li mo¿emy s³aæ to ¶lij */
int handle_output(client_t *c)
{
	int res;
	
	if (!c->obuf->len)
		return 0;

	res = write(c->fd, c->obuf->str, c->obuf->len);

	if (res < 1) {
		printf("write failed. removing client fd=%d\n", c->fd);
		remove_client(c);
		return 1;
	}

	string_remove(c->obuf, res);
	
	if (!c->obuf->len && c->remove) {
		c->status = GG_STATUS_NOT_AVAIL;
		remove_client(c);
		return 1;
	}

	return 0;
}

/* je¶li siê powiesi³, to sio z nim */
int handle_hangup(client_t *c)
{
	remove_client(c);
	return 1;
}

int main(int argc, char **argv)
{
	struct sockaddr_in sin;
	int sock, opt = 1;
	client_t cl;

	srand(time(NULL));

	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		perror("socket");
		return 1;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
		perror("setsockopt");
	
	sin.sin_port = htons(8074);
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_family = AF_INET;

	if (bind(sock, (struct sockaddr*) &sin, sizeof(sin))) {
		perror("bind");
		close(sock);
		return 1;
	}

	if (listen(sock, 100)) {
		perror("listen");
		close(sock);
		return 1;
	}

	memset(&cl, 0, sizeof(cl));
	cl.fd = sock;
	cl.state = STATE_LISTENING;
	cl.timeout = -1;
	list_add(&clients, &cl, sizeof(cl));

	while (1) {
		int nfds = 0, i, ret;
		struct pollfd *ufds;
		list_t l;

		for (nfds = 0, l = clients; l; l = l->next)
			nfds++;

		printf("poll: nfds = %d\n", nfds);

		ufds = xmalloc(sizeof(struct pollfd) * nfds);

		for (l = clients, i = 0; l; l = l->next, i++) {
			client_t *c = l->data;

			ufds[i].fd = c->fd;
			ufds[i].events = POLLIN | POLLERR | POLLHUP;
			if (c->obuf && c->obuf->len)
				ufds[i].events |= POLLOUT;
			printf("poll: ufds[%d].fd = %d, ufds[%d].events = 0x%.2x\n", i, c->fd, i, ufds[i].events);
		}

		ret = poll(ufds, nfds, -1);

		if (ret == -1 && errno != EINTR) {
			perror("poll");
			return 1;
		}

		for (l = clients; l;) {
			client_t *c = l->data;

			/* na wypadek usuniêcia aktualnego elementu */
			l = l->next;

			if ((c->state == STATE_LOGIN || c->state == STATE_LOGIN_OK) && c->timeout <= time(NULL)) {
				printf("timeout for uin=%d\n", c->uin); /* XXX brzydki komunikat */
				remove_client(c);
			}

			for (i = 0; i < nfds; i++) {
				if (ufds[i].fd == c->fd) {
					int res = 0;

					if (ufds[i].revents & POLLIN) {
						if (c->state == STATE_LISTENING)
							res = handle_connection(c);
						else
							res = handle_input(c);
					}
					if (!res && ufds[i].revents & POLLOUT)
						res = handle_output(c);

					if (!res && ufds[i].revents & (POLLHUP | POLLERR))
						res = handle_hangup(c);

					break;
				}
			}
		}

		xfree(ufds);
	}

	return 1;
}
