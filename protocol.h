/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>,
 *                          Robert J. Wo¼ny <speedy@ziew.org>,
 *                          Arkadiusz Mi¶kiewicz <misiek@pld.org.pl>,
 *                          Tomasz Chiliñski <chilek@chilan.com>
 *                          Piotr Wysocki <wysek@linux.bydg.org>
 *                          Dawid Jarosz <dawjar@poczta.onet.pl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License Version
 *  2.1 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <sys/types.h>
#include <stdint.h>

typedef uint32_t uin_t;

struct gg_header {
	uint32_t type;			/* typ pakietu */
	uint32_t length;		/* d³ugo¶æ reszty pakietu */
} __attribute__ ((packed));

#define GG_WELCOME 0x0001

struct gg_welcome {
	uint32_t key;			/* klucz szyfrowania has³a */
} __attribute__ ((packed));
	
#define GG_LOGIN 0x000c

struct gg_login {
	uint32_t uin;			/* mój numerek */
	uint32_t hash;			/* hash has³a */
	uint32_t status;		/* status na dzieñ dobry */
	uint32_t version;		/* moja wersja klienta */
	uint32_t local_ip;		/* mój adres ip */
	uint16_t local_port;		/* port, na którym s³ucham */
} __attribute__ ((packed));

#define GG_LOGIN_EXT 0x0013

struct gg_login_ext {
	uint32_t uin;			/* mój numerek */
	uint32_t hash;			/* hash has³a */
	uint32_t status;		/* status na dzieñ dobry */
	uint32_t version;		/* moja wersja klienta */
	uint32_t local_ip;		/* mój adres ip */
	uint16_t local_port;		/* port, na którym s³ucham */
	uint32_t external_ip;		/* zewnêtrzny adres ip */
	uint16_t external_port;		/* zewnêtrzny port */
} __attribute__ ((packed));

#define GG_LOGIN60 0x0015

struct gg_login60 {
	uint32_t uin;			/* mój numerek */
	uint32_t hash;			/* hash has³a */
	uint32_t status;		/* status na dzieñ dobry */
	uint32_t version;		/* moja wersja klienta */
	uint8_t dunno1;			/* 0x00 */
	uint32_t local_ip;		/* mój adres ip */
	uint16_t local_port;		/* port, na którym s³ucham */
	uint32_t external_ip;		/* zewnêtrzny adres ip */
	uint16_t external_port;		/* zewnêtrzny port */
	uint8_t image_size;		/* maksymalny rozmiar grafiki w KiB */
	uint8_t dunno2;			/* 0xbe */
} __attribute__ ((packed));

#define GG_LOGIN70 0x0019
#define GG_LOGIN80 0x0029

struct gg_login70 {
	uint32_t uin;			/* mój numerek */
	uint8_t hash_type;		/* rodzaj hashowania has³a */
	uint8_t hash[64];		/* hash has³a dope³niony zerami */
	uint32_t status;		/* status na dzieñ dobry */
	uint32_t version;		/* moja wersja klienta */
	uint8_t dunno1;			/* 0x00 */
	uint32_t local_ip;		/* mój adres ip */
	uint16_t local_port;		/* port, na którym s³ucham */
	uint32_t external_ip;		/* zewnêtrzny adres ip (???) */
	uint16_t external_port;		/* zewnêtrzny port (???) */
	uint8_t image_size;		/* maksymalny rozmiar grafiki w KiB */
	uint8_t dunno2;			/* 0xbe */
} __attribute__ ((packed));

#define GG_LOGIN_OK 0x0003

#define GG_LOGIN_FAILED 0x0009

#define GG_NEW_STATUS 0x0002

#define GG_STATUS_NOT_AVAIL 0x0001		/* niedostêpny */
#define GG_STATUS_NOT_AVAIL_DESCR 0x0015	/* niedostêpny z opisem (4.8) */
#define GG_STATUS_AVAIL 0x0002			/* dostêpny */
#define GG_STATUS_AVAIL_DESCR 0x0004		/* dostêpny z opisem (4.9) */
#define GG_STATUS_BUSY 0x0003			/* zajêty */
#define GG_STATUS_BUSY_DESCR 0x0005		/* zajêty z opisem (4.8) */
#define GG_STATUS_INVISIBLE 0x0014		/* niewidoczny (4.6) */
#define GG_STATUS_INVISIBLE_DESCR 0x0016	/* niewidoczny z opisem (4.9) */

#define GG_STATUS_FRIENDS_MASK 0x8000		/* tylko dla znajomych (4.6) */

struct gg_new_status {
	uint32_t status;			/* na jaki zmieniæ? */
} __attribute__ ((packed));

#define GG_LIST_EMPTY 0x0012

#define GG_NOTIFY_FIRST 0x000f
#define GG_NOTIFY_LAST 0x0010
	
struct gg_notify {
	uint32_t uin;				/* numerek danej osoby */
	uint8_t dunno1;			/* == 3 */
} __attribute__ ((packed));

#define GG_NOTIFY_REPLY 0x000c	/* tak, to samo co GG_LOGIN */
	
struct gg_notify_reply {
	uint32_t uin;			/* numerek */
	uint32_t status;		/* status danej osoby */
	uint32_t remote_ip;	/* adres ip delikwenta */
	uint16_t remote_port;	/* port, na którym s³ucha klient */
	uint32_t version;		/* wersja klienta */
	uint16_t dunno2;		/* znowu port? */
} __attribute__ ((packed));

#define GG_NOTIFY_REPLY77 0x0018
#define GG_NOTIFY_REPLY80 0x002b

struct gg_notify_reply77 {
	uint32_t uin;			/* numerek plus flagi w MSB */
	uint8_t status;			/* status danej osoby */
	uint32_t remote_ip;		/* adres ip delikwenta */
	uint16_t remote_port;		/* port, na którym s³ucha klient */
	uint8_t version;		/* wersja klienta */
	uint8_t image_size;		/* maksymalny rozmiar grafiki w KiB */
	uint8_t dunno1;			/* 0x00 */
	uint32_t dunno2;		/* ? */
} __attribute__ ((packed));

#define GG_ADD_NOTIFY 0x000d
#define GG_REMOVE_NOTIFY 0x000e
	
struct gg_add_remove {
	uint32_t uin;			/* numerek */
	uint8_t dunno1;		/* == 3 */
} __attribute__ ((packed));

#define GG_STATUS 0x0002

struct gg_status {
	uint32_t uin;			/* numerek */
	uint32_t status;		/* nowy stan */
} __attribute__ ((packed));

#define GG_STATUS77 0x0017
#define GG_STATUS80 0x002a

struct gg_status77 {
	uint32_t uin;			/* numerek plus flagi w MSB */
	uint8_t status;			/* status danej osoby */
	uint32_t remote_ip;		/* adres ip delikwenta */
	uint16_t remote_port;		/* port, na którym s³ucha klient */
	uint8_t version;		/* wersja klienta */
	uint8_t image_size;		/* maksymalny rozmiar grafiki w KiB */
	uint8_t dunno1;			/* 0x00 */
	uint32_t dunno2;		/* ? */
} __attribute__ ((packed));

#define GG_SEND_MSG 0x000b

#define GG_CLASS_QUEUED 0x0001
#define GG_CLASS_OFFLINE GG_CLASS_QUEUED
#define GG_CLASS_MSG 0x0004
#define GG_CLASS_CHAT 0x0008
#define GG_CLASS_CTCP 0x0010
#define GG_CLASS_EXT_MSG 0x0020 	/* EXT = extended */

#define GG_MSG_MAXSIZE 2000

struct gg_send_msg {
	uint32_t recipient;
	uint32_t seq;
	uint32_t msgclass;
} __attribute__ ((packed));

struct gg_msg_recipients {
	uint8_t flag;
	uint32_t count;
} __attribute__ ((packed));

#define GG_SEND_MSG_ACK 0x0005

#define GG_ACK_DELIVERED 0x0002
#define GG_ACK_QUEUED 0x0003
#define GG_ACK_NOT_DELIVERED 0x0006
	
struct gg_send_msg_ack {
	uint32_t status;
	uint32_t recipient;
	uint32_t seq;
} __attribute__ ((packed));

#define GG_RECV_MSG 0x000a
	
struct gg_recv_msg {
	uint32_t sender;
	uint32_t seq;
	uint32_t time;
	uint32_t msgclass;
} __attribute__ ((packed));

#define GG_PING 0x0008
	
#define GG_PONG 0x0007

#define GG_DISCONNECTING 0x000b

#define GG_PUBDIR50_REQUEST 0x0014

struct gg_pubdir50_request {
	uint8_t type;			/* GG_PUBDIR50_* */
	uint32_t seq;			/* czas wys³ania zapytania */
} __attribute__ ((packed));

#define GG_USERLIST_REQUEST 0x0016

struct gg_userlist_request {
	uint8_t type;
} __attribute__ ((packed));

#define TIMEOUT_CONNECT 15
// #define TIMEOUT_CONNECT 180			/* old: XXX + 3*timeout_ping */
#define TIMEOUT_DEFAULT	(5*60)
//#define TIMEOUT_DEFAULT 120			/* old: 2*timeout_ping */

#endif /* __PROTOCOL_H */

