#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "sdbm.h"

void
oops(char *s1, char *s2)
{
	extern char *progname;

	if (progname)
		fprintf(stderr, "%s: ", progname);
	fprintf(stderr, s1, s2);
	if (errno > 0)
		fprintf(stderr, " (%s)", strerror(errno));
	fprintf(stderr, "\n");
	exit(1);
}

int
okpage(char *pag)
{
	unsigned n;
	int off;
	short *ino = (short *) pag;

	if ((n = ino[0]) > PBLKSIZ / sizeof(short))
		return 0;

	if (!n)
		return 1;

	off = PBLKSIZ;
	for (ino++; n; ino += 2) {
		if (ino[0] > off || ino[1] > off ||
		    ino[1] > ino[0])
			return 0;
		off = ino[1];
		n -= 2;
	}

	return 1;
}
