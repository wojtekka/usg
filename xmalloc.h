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

#ifndef __XMALLOC_H
#define __XMALLOC_H

#define xcalloc(x,y) xcalloc_(x,y,__LINE__,__FILE__)

void *xcalloc_(int nmemb, int size, char *a, char *b);
void *xmalloc_(int size);
void xfree_(void *ptr);
void *xrealloc_(void *ptr, int size);
char *xstrdup_(const char *s);

#endif /* __XMALLOC_H */
