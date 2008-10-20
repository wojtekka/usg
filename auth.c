/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>,
 *                          Robert J. Wo¼ny <speedy@ziew.org>,
 *                          Arkadiusz Mi¶kiewicz <misiek@pld.org.pl>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include <openssl/sha.h>

#include "usg.h"

unsigned int gg_login_hash(const char *pass, unsigned int seed)
{
	const unsigned char *password = (const unsigned char *) pass;

	unsigned int x, y, z;

	y = seed;

	for (x = 0; *password; password++) {
		x = (x & 0xffffff00) | *password;
		y ^= x;
		y += x;
		x <<= 8;
		y ^= x;
		x <<= 8;
		y -= x;
		x <<= 8;
		y ^= x;

		z = y & 0x1F;
		y = (y << z) | (y >> (32 - z));
	}

	return y;
}

void gg_login_hash_sha1(const char *pass, unsigned int seed, unsigned char *result)
{
	SHA_CTX ctx;
	
	SHA1_Init(&ctx);
	SHA1_Update(&ctx, (const unsigned char*) pass, strlen(pass));
	SHA1_Update(&ctx, (unsigned char *) &seed, 4);
	
	SHA1_Final(result, &ctx);
}

const char *get_password(int uin) {
	FILE *f;
	struct passwd *p;

	if (!(f = fopen("passwd", "r"))) {
		printf("auth attempt: file passwd doesn't exists!\n");
		return NULL;
	}

	while ((p = fgetpwent(f))) {
		if (p->pw_uid == uin)
			return p->pw_passwd;
	}
	return NULL;
}

int authorize(int uin, unsigned int seed, unsigned long response)
{
	const char *password;
	unsigned long hash;

	if (!(password = get_password(uin))) {
		printf("auth attempt: uin %d not found in passwd repo, or empty password\n", uin);
		return 0;
	}

	hash = gg_login_hash(password, seed);

	printf("auth attempt: seed=%d, pass=\"%s\", myhash=%ld, hishash=%ld\n", seed, password, hash, response);

	return (hash == response);
}

int authorize70(int uin, unsigned int seed, unsigned int type, unsigned char *response)
{
	unsigned char hash[20];

	char myhash[41];
	char hishash[41];
	int i;

	const char *password;

	if (!(password = get_password(uin))) {
		printf("auth attempt: uin %d not found in passwd repo, or empty password\n", uin);
		return 0;
	}

	/* XXX, type */

	gg_login_hash_sha1(password, seed, hash);

	for (i = 0; i < 40; i += 2) {
		snprintf(hishash + i, sizeof(hishash) - i, "%02x", response[i / 2]);
		snprintf(myhash + i, sizeof(myhash) - i, "%02x", hash[i / 2]);
	}

	printf("auth attempt: seed=%d, pass=\"%s\", myhash=%s, hishash=%s\n", seed, password, myhash, hishash);

	for (i = 0; i < 20; i++) {
		if (response[i] != hash[i])
			return 0;
	}
	return 1;
}

