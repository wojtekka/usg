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
#include <unistd.h>
#include "usg.h"

unsigned int gg_login_hash(const unsigned char *password, unsigned int seed)
{
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

int authorize(int uin, int seed, unsigned long response)
{
	FILE *f;
	char buf[100];
	unsigned long hash;

	snprintf(buf, sizeof(buf), "passwd/%d", uin);

	if (!(f = fopen(buf, "r"))) {
		printf("auth attempt: uin %d not found in passwd repo\n", uin);
		return 0;
	}

	if (!fgets(buf, sizeof(buf), f)) {
		printf("auth attempt: empty password file for %d\n", uin);
		fclose(f);
		return 0;
	}

	fclose(f);

	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = 0;
	if (buf[strlen(buf) - 1] == '\r')
		buf[strlen(buf) - 1] = 0;

	hash = gg_login_hash(buf, seed);

	printf("auth attempt: seed=%d, pass=\"%s\", myhash=%ld, hishash=%ld\n", seed, buf, hash, response);

	return (hash == response);
}
