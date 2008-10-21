#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>

#include "msgqueue.h"
#include "usg.h"
#include "xmalloc.h"

int enqueue_message(int recipient, int sender, int seq, int msgclass, char *text, int length)
{
	char buf[100];
	int fd;

	mkdir(path_uin("queue", recipient), 0700);
	snprintf(buf, sizeof(buf), "queue/%d/%.10ld-%d-%d-%d", recipient, time(NULL), sender, seq, msgclass);
	if ((fd = open(buf, O_WRONLY | O_CREAT, 0600)) == -1)
		return -1;
	if (write(fd, text, length) < length) {
		close(fd);
		unlink(buf);
		return -1;
	}
	close(fd);

	return 0;
}

int unqueue_message(int uin, msgqueue_t *m)
{
	DIR *d;
	struct dirent *de;
	int fd;
	
	if (!(d = opendir(path_uin("queue", uin))))
		return -1;

	while ((de = readdir(d))) {
		char buf[100];
		struct stat st;

		snprintf(buf, sizeof(buf), "queue/%d/%s", uin, de->d_name);

		if (stat(buf, &st) == -1 || !S_ISREG(st.st_mode))
			continue;

		m->length = st.st_size;

		if (sscanf(de->d_name, "%d-%d-%d-%d", &m->time, &m->sender, &m->seq, &m->msgclass) != 4)
			continue;

		if ((fd = open(buf, O_RDONLY)) == -1) {
			closedir(d);
			return -1;
		}
						
		m->text = xmalloc(st.st_size);
		if (read(fd, m->text, st.st_size) < st.st_size) {
			free(m->text);
			closedir(d);
			return -1;
		}
		close(fd);

		unlink(buf);

		return 0;
		closedir(d);
	}

	closedir(d);
	return -1;
}
