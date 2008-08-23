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

/*
 * list_add_sorted()
 *
 * dodaje do listy dany element. przy okazji mo¿e te¿ skopiowaæ zawarto¶æ.
 * je¶li poda siê jako ostatni parametr funkcjê porównuj±c± zawarto¶æ
 * elementów, mo¿e posortowaæ od razu.
 *
 *  - list - wska¼nik do listy,
 *  - data - wska¼nik do elementu,
 *  - alloc_size - rozmiar elementu, je¶li chcemy go skopiowaæ.
 *
 * zwraca wska¼nik zaalokowanego elementu lub NULL w przpadku b³êdu.
 */
void *list_add_sorted(struct list **list, void *data, int alloc_size, int (*comparision)(void *, void *))
{
	struct list *new, *tmp;

	if (!list) {
		errno = EFAULT;
		return NULL;
	}

	new = xmalloc(sizeof(struct list));

	new->data = data;
	new->next = NULL;

	if (alloc_size) {
		new->data = xmalloc(alloc_size);
		memcpy(new->data, data, alloc_size);
	}

	if (!(tmp = *list)) {
		*list = new;
	} else {
		if (!comparision) {
			while (tmp->next)
				tmp = tmp->next;
			tmp->next = new;
		} else {
			struct list *prev = NULL;
			
			while (comparision(new->data, tmp->data) > 0) {
				prev = tmp;
				tmp = tmp->next;
				if (!tmp)
					break;
			}
			
			if (!prev) {
				tmp = *list;
				*list = new;
				new->next = tmp;
			} else {
				prev->next = new;
				new->next = tmp;
			}
		}
	}

	return new->data;
}

/*
 * list_add()
 *
 * wrapper do list_add_sorted(), który zachowuje poprzedni± sk³adniê.
 */
void *list_add(struct list **list, void *data, int alloc_size)
{
	return list_add_sorted(list, data, alloc_size, NULL);
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

int string_append_c(string_t s, char c)
{
	if (!s) {
		errno = EFAULT;
		return -1;
	}
	
	string_realloc(s, s->len + 1);

	s->str[s->len + 1] = 0;
	s->str[s->len++] = c;

	return 0;
}

int string_append_n(string_t s, const char *str, int count)
{
	if (!s || !str || count < 0) {
		errno = EFAULT;
		return -1;
	}

	string_realloc(s, s->len + count);

	s->str[s->len + count] = 0;
	strncpy(s->str + s->len, str, count);

	s->len += count;

	return 0;
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

int string_append(string_t s, const char *str)
{
	if (!s || !str) {
		errno = EFAULT;
		return -1;
	}

	return string_append_n(s, str, strlen(str));
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

char *string_free(string_t s, int free_string)
{
	char *tmp = NULL;

	if (!s)
		return NULL;

	if (free_string)
		xfree(s->str);
	else
		tmp = s->str;

	xfree(s);

	return tmp;
}

/*
 * itoa()
 *
 * prosta funkcja, która zwraca tekstow± reprezentacjê liczby. w obrêbie
 * danego wywo³ania jakiej¶ funkcji lub wyra¿enia mo¿e byæ wywo³ania 10
 * razy, poniewa¿ tyle mamy statycznych buforów. lepsze to ni¿ ci±g³e
 * tworzenie tymczasowych buforów na stosie i sprintf()owanie.
 *
 *  - i - liczba do zamiany.
 *
 * zwraca adres do bufora, którego _NIE_NALE¯Y_ zwalniaæ.
 */
const char *itoa(long int i)
{
	static char bufs[10][16];
	static int index = 0;
	char *tmp = bufs[index++];

	if (index > 9)
		index = 0;
	
	snprintf(tmp, 16, "%ld", i);

	return tmp;
}

/*
 * array_make()
 *
 * tworzy tablicê tekstów z jednego, rozdzielonego podanymi znakami.
 *
 * zaalokowan± tablicê z zaalokowanymi ci±gami znaków lub NULL w przypadku
 * b³êdu.
 */
char **array_make(const char *string, const char *sep, int max, int trim, int quotes)
{
	const char *p, *q;
	char **result = NULL;
	int items, last = 0;

	for (p = string, items = 0; ; ) {
		int len = 0;
		char *token = NULL;

		if (max && items >= max - 1)
			last = 1;
		
		if (trim) {
			while (*p && strchr(sep, *p))
				p++;
			if (!*p)
				break;
		}

		if (!last && quotes && (*p == '\'' || *p == '\"')) {
			char sep = *p;

			for (q = p + 1, len = 0; *q; q++, len++) {
				if (*q == '\\') {
					q++;
					if (!*q)
						break;
				} else if (*q == sep)
					break;
			}

			if ((token = xcalloc(1, len + 1))) {
				char *r = token;
			
				for (q = p + 1; *q; q++, r++) {
					if (*q == '\\') {
						q++;
						
						if (!*q)
							break;
						
						switch (*q) {
							case 'n':
								*r = '\n';
								break;
							case 'r':
								*r = '\r';
								break;
							case 't':
								*r = '\t';
								break;
							default:
								*r = *q;
						}
					} else if (*q == sep) {
						break;
					} else 
						*r = *q;
				}
				
				*r = 0;
			}
			
			p = q + 1;
			
		} else {
			for (q = p, len = 0; *q && (last || !strchr(sep, *q)); q++, len++);
			token = xcalloc(1, len + 1);
			strncpy(token, p, len);
			token[len] = 0;
			p = q;
		}
		
		result = xrealloc(result, (items + 2) * sizeof(char*));
		result[items] = token;
		result[++items] = NULL;

		if (!*p)
			break;

		p++;
	}

	if (!items)
		result = xcalloc(1, sizeof(char*));

	return result;
}

/*
 * array_free()
 *
 * zwalnia pamieæ zajmowan± przez tablicê.
 */
void array_free(char **array)
{
	char **tmp;

	if (!array)
		return;

	for (tmp = array; *tmp; tmp++)
		free(*tmp);

	free(array);
}

