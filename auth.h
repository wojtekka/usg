#ifndef __AUTH_H
#define __AUTH_H

#include "usg.h"

int authorize(int uin, unsigned int seed, unsigned long response);
int authorize70(int uin, unsigned int seed, unsigned int type, unsigned char *response);

#endif
