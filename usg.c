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
int obuf_len = 0, ibuf_len = 0, ufds_len = 0;

/* wysy³a dane do danego klienta */
void write_client(client_t *c, void *buf, int len)
{
	c->obuf = xrealloc(c->obuf, c->obuf_len + len);
	memcpy(c->obuf + c->obuf_len, buf, len);
	c->obuf_len += len;
	obuf_len += len;
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

			if (f->uin == c->uin && !(f->flags & FLAG_BLOCKED)) {
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

/* je¶li klient dopisa³ kogo¶ do swojej userlisty, odpowiadamy, je¶li jest */
void notify_reply(client_t *c, int uin)
{
	client_t *f = find_client(uin);
	client_t dummy;
	char buf[100];
	struct gg_header h;
	struct gg_notify_reply n;

	if (uin == c->uin)
		return;

	memset(&dummy, 0, sizeof(dummy));
	
	/* je¶li nie jest po³±czony, zobacz, czy nie zostawi³ opisu */
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

/* usuwa klienta, zamyka po³±czneie itd. */
void remove_client(client_t *c)
{
	if (c->status != GG_STATUS_NOT_AVAIL && c->status != GG_STATUS_NOT_AVAIL_DESCR && c->status != GG_STATUS_INVISIBLE && c->status != GG_STATUS_INVISIBLE_DESCR) {
		struct gg_header h;
		struct gg_status s;
		h.type = GG_STATUS;
		h.length = sizeof(s);
		s.uin = c->uin;
		s.status = GG_STATUS_NOT_AVAIL;
		write_client_friends(c, &h, sizeof(h));
		write_client_friends(c, &s, sizeof(s));
	}

	shutdown(c->fd, 2);
	close(c->fd);
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

/* obs³uguje przychodz±ce pakiety */
void handle_input_packet(client_t *c)
{
	struct gg_header *h = (struct gg_header*) c->ibuf;
	void *data = c->ibuf + sizeof(struct gg_header);

	printf("uin %d, fd %d sent packet %.02x length %d\n", c->uin, c->fd, h->type, h->length);

	switch (h->type) {
		case GG_LOGIN:
		case GG_LOGIN_FWD:
		{
			struct gg_login *l = data;
			struct gg_header h;
			msgqueue_t m;

			printf("login attempt from %d\n", l->uin);

			if (!authorize(l->uin, c->seed, l->hash)) {
				h.type = GG_LOGIN_FAILED;
				h.length = 0;
				write_client(c, &h, sizeof(h));
				printf("failed, sending reject\n");
				c->remove = 1;
				break;
			}

			printf("succeded!\n");

			if (find_client(l->uin)) {
				printf("duplicate client, removing previous\n");
				/* XXX kopie siê */
				remove_client(find_client(l->uin));
			}

			c->uin = l->uin;
			c->status = l->status;
			c->version = l->version;
			c->ip = l->local_ip;
			c->port = l->local_port;

			changed_status(c);

			h.type = GG_LOGIN_OK;
			h.length = 0;
			write_client(c, &h, sizeof(h));

			while (!unqueue_message(c->uin, &m)) {
				struct gg_recv_msg r;
				
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
			
			break;
		}
		
		case GG_NEW_STATUS:
		{
			struct gg_new_status *s = data;
			char buf[100];

			c->status = s->status;
			if (c->status_descr)
				xfree(c->status_descr);
			if (h->length > sizeof(*s)) {
				c->status_descr = xmalloc(h->length - sizeof(*s) + 1);
				memcpy(c->status_descr, data + sizeof(*s), h->length - sizeof(*s));
				c->status_descr[h->length - sizeof(*s)] = 0;
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
			
			break;
		}

		case GG_NOTIFY:
		{
			struct gg_notify *n = data;
			int i;

			printf("received notify list from %d\n", c->uin);

			for (i = 0; i < h->length / sizeof(*n); i++) {
				friend_t f;

				f.uin = n[i].uin;
				f.flags = n[i].dunno1;
				list_add(&c->friends, &f, sizeof(f));
				notify_reply(c, f.uin);
			}
			
			break;
		}

		case GG_ADD_NOTIFY:
		{
			struct gg_add_remove *ar = data;
			friend_t f;

			printf("adding notify on %d for %d\n", ar->uin, c->uin);
			
			f.uin = ar->uin;
			f.flags = ar->dunno1;
			list_add(&c->friends, &f, sizeof(f));
			notify_reply(c, f.uin);
			
			break;
		}

		case GG_REMOVE_NOTIFY:
		{
			struct gg_add_remove *ar = data;
			list_t l;

			printf("removing notify from %d for %d\n", c->uin, ar->uin);

			for (l = c->friends; l; l = l->next) {
				friend_t *f = l->data;

				if (f->uin == ar->uin) {
					list_remove(&c->friends, f, 1);
					break;
				}
			}

			break;
		}
		
		case GG_SEND_MSG:
		{
			struct gg_send_msg *s = data;
			int ack = GG_ACK_QUEUED;
			struct gg_header h2;
			struct gg_send_msg_ack a;
			client_t *rcpt;

			if (!(rcpt = find_client(s->recipient))) {
				printf("enqueuing message from %d to %d\n", c->uin, s->recipient);
				enqueue_message(s->recipient, c->uin, s->seq, s->msgclass, data + sizeof(*s), h->length - sizeof(*s));
			} else {
				struct gg_recv_msg r;

				printf("sending message from %d to %d\n", c->uin, s->recipient);
				
				h2.type = GG_RECV_MSG;
				h2.length = sizeof(r) + (h->length - sizeof(*s));
				r.sender = c->uin;
				r.seq = s->seq;
				r.time = time(NULL);
				r.msgclass = s->msgclass;

				write_client(rcpt, &h2, sizeof(h2));
				write_client(rcpt, &r, sizeof(r));
				write_client(rcpt, data + sizeof(*s), h->length - sizeof(*s));
				ack = GG_ACK_DELIVERED;
			}

			printf("sending ack back to %d\n", c->uin);
			h2.type = GG_SEND_MSG_ACK;
			h2.length = sizeof(a);
			a.status = ack;
			a.recipient = s->recipient;
			a.seq = s->seq;
			write_client(c, &h2, sizeof(h2));
			write_client(c, &a, sizeof(a));

			break;
		}

		case GG_PING:
		{
			printf("received ping from %d\n", c->uin);
			c->last_ping = time(NULL);

			break;
		}
	}
}

/* obs³uguje przychodz±ce po³±cznia */
int handle_connection(client_t *c)
{
	client_t n;
	struct sockaddr_in sin;
	int fd, sin_len = sizeof(sin), nb = 1;
	struct gg_header h;
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

	h.type = GG_WELCOME;
	h.length = sizeof(w);
	n.seed = w.key = random();

	write_client(&n, &h, sizeof(h));
	write_client(&n, &w, sizeof(w));
	
	list_add(&clients, &n, sizeof(n));

	return 0;
}

/* gdy przychodz± dane, dopisz do bufora i wywo³aj obs³ugê pakietów, je¶li
 * jaki¶ uda³o siê ju¿ posk³adaæ */
int handle_input(client_t *c)
{
	char buf[1024];
	int res, first = 1;

	while (1) {
		res = read(c->fd, buf, sizeof(buf));
		printf("read: fd=%d, res=%d\n", c->fd, res);

		if (res > 0) {
			c->ibuf = xrealloc(c->ibuf, c->ibuf_len + res);
			memcpy(c->ibuf + c->ibuf_len, buf, res);
			c->ibuf_len += res;
			ibuf_len += res;
		}

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

	printf("      ibuf_len = %d\n", c->ibuf_len);

	while (c->ibuf_len >= 8) {
		struct gg_header *h = (struct gg_header*) c->ibuf;
		
		if (h->length < 0 || h->length > 2500) {
			remove_client(c);
			return 1;
		}
		
		if (c->ibuf_len - 8 >= h->length) {
			handle_input_packet(c);

			if (c->ibuf_len - 8 == h->length) {
				xfree(c->ibuf);
				c->ibuf_len = 0;
				c->ibuf = NULL;
				ibuf_len -= c->ibuf_len;
			} else {
				memmove(c->ibuf, c->ibuf + 8 + h->length, c->ibuf_len - 8 - h->length);
				c->ibuf_len -= (8 + h->length);
				ibuf_len -= (8 + h->length);
			}
		}
	}

	return 0;
}

/* je¶li mo¿emy s³aæ to ¶lij */
int handle_output(client_t *c)
{
	int res;
	
	if (!c->obuf_len)
		return 0;

	res = write(c->fd, c->obuf, c->obuf_len);

	if (res < 1) {
		remove_client(c);
		return 1;
	}
	
	if (res == c->obuf_len) {
		xfree(c->obuf);
		c->obuf = NULL;
		c->obuf_len = 0;
	} else {
		memmove(c->obuf, c->obuf + res, c->obuf_len - res);
		c->obuf = xrealloc(c->obuf, c->obuf_len - res);
		c->obuf_len -= res;
	}
	obuf_len -= res;

	if (!c->obuf_len && c->remove) {
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

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
		perror("setsockopt");
		return 1;
	}
	
	sin.sin_port = htons(8074);
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_family = AF_INET;

	if (bind(sock, (struct sockaddr*) &sin, sizeof(sin))) {
		perror("bind");
		return 1;
	}

	if (listen(sock, 100)) {
		perror("listen");
		return 1;
	}

	memset(&cl, 0, sizeof(cl));
	cl.fd = sock;
	cl.state = STATE_LISTENING;
	cl.timeout = -1;
	list_add(&clients, &cl, sizeof(cl));

	while (1) {
		list_t l, n;
		int nfds = 0, i, ret;
		struct pollfd *ufds;

		for (nfds = 0, l = clients; l; l = l->next)
			nfds++;

		printf("poll: nfds = %d\n", nfds);

		ufds = xmalloc(sizeof(struct pollfd) * nfds);

		for (l = clients, i = 0; l; l = l->next, i++) {
			client_t *c = l->data;

			ufds[i].fd = c->fd;
			ufds[i].events = POLLIN | POLLERR | POLLHUP;
			if (c->obuf_len)
				ufds[i].events |= POLLOUT;
			printf("poll: ufds[%d].fd = %d, ufds[%d].events = 0x%.2x\n", i, c->fd, i, ufds[i].events);
		}

		ret = poll(ufds, nfds, -1);

		if (ret == -1 && errno != EINTR) {
			perror("poll");
			return 1;
		}

		for (l = clients; l; l = n) {
			client_t *c = l->data;

			/* na wypadek usuniêcia aktualnego elementu */
			n = l->next;

			/* XXX zaimplementowaæ timeouty */

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
