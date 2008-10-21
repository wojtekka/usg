/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *			    Dawid Jarosz <dawjar@poczta.onet.pl>
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
#include <errno.h>
#ifndef _AIX
#  include <string.h>
#endif
#include "dynstuff.h"
#include "xmalloc.h"

void *list_add(struct list **list, void *data) {
	list_t new;

	if (!list) {
		errno = EFAULT;
		return NULL;
	}

	new = xmalloc(sizeof(struct list));
	new->next = *list;
	*list	  = new;

	new->data = data;

	return new->data;
}

/*
 * list_remove()
 *
 * usuwa z listy wpis z podanym elementem.
 *
 *  - list - wska¼nik do listy,
 *  - data - element,
 *  - free_data - zwolniæ pamiêæ po elemencie.
 */
int list_remove(struct list **list, void *data, int free_data)
{
	struct list *tmp, *last = NULL;

	if (!list) {
		errno = EFAULT;
		return -1;
	}

	tmp = *list;
	if (tmp->data == data) {
		*list = tmp->next;
	} else {
		for (; tmp && tmp->data != data; tmp = tmp->next)
			last = tmp;
		if (!tmp) {
			errno = ENOENT;
			return -1;
		}
		last->next = tmp->next;
	}

	if (free_data)
		free(tmp->data);
	free(tmp);

	return 0;
}

/*
 * list_count()
 *
 * zwraca ilo¶æ elementów w danej li¶cie.
 *
 *  - list - lista.
 */
int list_count(struct list *list)
{
	int count = 0;

	for (; list; list = list->next)
		count++;

	return count;
}

/*
 * list_destroy()
 *
 * niszczy wszystkie elementy listy.
 *
 *  - list - lista,
 *  - free_data - czy zwalniaæ bufor danych?
 */
int list_destroy(struct list *list, int free_data)
{
	struct list *tmp;
	
	while (list) {
		if (free_data)
			free(list->data);

		tmp = list->next;

		free(list);

		list = tmp;
	}

	return 0;
}

/*
 * string_realloc()
 *
 * upewnia siê, ¿e w stringu bêdzie wystarczaj±co du¿o miejsca.
 *
 *  - s - ci±g znaków,
 *  - count - wymagana ilo¶æ znaków (bez koñcowego '\0').
 */
static void string_realloc(string_t s, int count)
{
	char *tmp;
	
	if (s->str && (count + 1) <= s->size)
		return;
	
	tmp = xrealloc(s->str, count + 81);
	if (!s->str)
		*tmp = 0;
	tmp[count + 80] = 0;
	s->size = count + 81;
	s->str = tmp;
}

string_t string_init(const char *value) {
	string_t tmp = xmalloc(sizeof(struct string));
	size_t valuelen;

	if (!value)
		value = "";

	valuelen = strlen(value);

	tmp->str = xstrdup(value);
	tmp->len = valuelen;
	tmp->size = valuelen + 1;

	return tmp;
}

int string_append_raw(string_t s, const char *str, int count)
{
	if (!s || !str || count < 0) {
		errno = EFAULT;
		return -1;
	}

	string_realloc(s, s->len + count);

	s->str[s->len + count] = 0;
	memcpy(s->str + s->len, str, count);

	s->len += count;

	return 0;
}

void string_remove(string_t s, int count)
{
	if (!s || count <= 0)
		return;
	
	if (count >= s->len) {
		/* string_clear() */
		s->str[0]	= '\0';
		s->len		= 0;

	} else {
		memmove(s->str, s->str + count, s->len - count);
		s->len -= count;
		s->str[s->len] = '\0';
	}
}

void string_free(string_t s)
{
	if (s) {
		free(s->str);
		free(s);
	}
}

