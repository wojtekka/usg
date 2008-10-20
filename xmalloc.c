/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
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
#include <signal.h>
#ifndef _AIX
#  include <string.h>
#endif

extern int ibuf_len, obuf_len;

static void oom_handler(char *a, int b)
{
	fprintf(stderr, "Out of memory in %s:%d\nDumping core, so you could backtrace it with gdb\n\n", a, b);

	raise(SIGSEGV);
}

void *xcalloc_(int nmemb, int size, char *a, int b)
{
	void *tmp = calloc(nmemb, size);

	if (!tmp)
		oom_handler(a, b);

	return tmp;
}

void *xmalloc_(int size, char *a, int b)
{
	void *tmp = malloc(size);

	if (!tmp)
		oom_handler(a, b);
	
	return tmp;
}

void xfree_(void *ptr)
{
	if (ptr)
		free(ptr);
}

void *xrealloc_(void *ptr, int size, char *a, int b)
{
	void *tmp = realloc(ptr, size);

	if (!tmp)
		oom_handler(a, b);

	return tmp;
}

char *xstrdup_(const char *s, char *a, int b)
{
	char *tmp;

	if (!s)
		return NULL;

	if (!(tmp = strdup(s)))
		oom_handler(a, b);

	return tmp;
}


