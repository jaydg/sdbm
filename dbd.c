/*
 * dbd - dump a dbm data file
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>

#include "sdbm.h"
#include "util.h"

char *progname;

#define empty(page)	(((short *) page)[0] == 0)

static void sdump(int pagf);
static void dispage(char *pag);

int
main(int argc, char **argv)
{
	int n;
	char *p;
	char *name;
	int pagf;

	progname = argv[0];

	if (argc > 1 && (p = argv[1])) {
		name = (char *) malloc((n = strlen(p)) + 5);
		strcpy(name, p);
		strcpy(name + n, ".pag");

		if ((pagf = open(name, O_RDONLY)) < 0)
			oops("cannot open %s.", name);

		sdump(pagf);
	}
	else
		oops("usage: %s dbname", progname);
	return 0;
}

static void
sdump(int pagf)
{
	int r;
	int n = 0;
	int o = 0;
	char pag[PBLKSIZ];

	while ((r = read(pagf, pag, PBLKSIZ)) > 0) {
		if (!okpage(pag))
			fprintf(stderr, "%d: bad page.\n", n);
		else if (empty(pag))
			o++;
		else
			dispage(pag);
		n++;
	}

	if (r == 0)
		fprintf(stderr, "%d pages (%d holes).\n", n, o);
	else
		oops("read failed: block %d", n);
}

static void
dispage(char *pag)
{
	int i, n;
	int off;
	short *ino = (short *) pag;

	off = PBLKSIZ;
	for (i = 1; i < ino[0]; i += 2) {
		for (n = ino[i]; n < off; n++)
			if (pag[n] != 0)
				putchar(pag[n]);
		putchar('\t');
		off = ino[i];
		for (n = ino[i + 1]; n < off; n++)
			if (pag[n] != 0)
				putchar(pag[n]);
		putchar('\n');
		off = ino[i + 1];
	}
}
