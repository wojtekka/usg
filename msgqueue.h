#ifndef __MSGQUEUE_H
#define __MSGQUEUE_H

#include "usg.h"

typedef struct {
	int sender;
	int time;
	int seq;
	int msgclass;
	char *text;
	int length;
} msgqueue_t;

int enqueue_message(int recipient, int sender, int seq, int msgclass, char *text, int length);
int unqueue_message(int uin, msgqueue_t *m);
	

#endif
