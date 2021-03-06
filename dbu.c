#ifdef __linux
# define _XOPEN_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <string.h>
#include <unistd.h>

#ifdef TIME
# include <sys/time.h>
#endif

#include "sdbm.h"
#include "util.h"

char *progname;

static int rflag;
static char *usage = "%s [-R] cat | look |... dbmname";

#define DERROR	0
#define DLOOK	1
#define DINSERT	2
#define DDELETE 3
#define	DCAT	4
#define DBUILD	5
#define DPRESS	6
#define DCREAT	7

#define LINEMAX	8192

typedef struct {
	char *sname;
	int scode;
	int flags;
} cmd;

static cmd cmds[] = {
	{ "fetch", DLOOK, 	O_RDONLY },
	{ "get", DLOOK,		O_RDONLY },
	{ "look", DLOOK,		O_RDONLY },
	{ "add", DINSERT,		O_RDWR },
	{ "insert", DINSERT,	O_RDWR },
	{ "store", DINSERT,	O_RDWR },
	{ "delete", DDELETE,	O_RDWR },
	{ "remove", DDELETE,	O_RDWR },
	{ "dump", DCAT,		O_RDONLY },
	{ "list", DCAT, 		O_RDONLY },
	{ "cat", DCAT,		O_RDONLY },
	{ "creat", DCREAT,	O_RDWR | O_CREAT | O_TRUNC },
	{ "new", DCREAT,		O_RDWR | O_CREAT | O_TRUNC },
	{ "build", DBUILD,	O_RDWR | O_CREAT },
	{ "squash", DPRESS,	O_RDWR },
	{ "compact", DPRESS,	O_RDWR },
	{ "compress", DPRESS,	O_RDWR }
};

#define CTABSIZ (sizeof (cmds)/sizeof (cmd))

static cmd *parse();
static void badk(), doit(), prdatum();

int
main(int argc, char *argv[])
{
	int c;
	cmd *act;

	progname = argv[0];

	while ((c = getopt(argc, argv, "R")) != -1)
		switch (c) {
		case 'R':	       /* raw processing  */
			rflag++;
			break;

		default:
			oops("usage: %s", usage);
			break;
		}

	if ((argc -= optind) < 2)
		oops("usage: %s", usage);

	if ((act = parse(argv[optind])) == NULL)
		badk(argv[optind]);
	optind++;
	doit(act, argv[optind]);
	return 0;
}

static void
doit(cmd *act, char *file)
{
	datum key;
	datum val;
	DBM *db;
	char *op;
	int n;
	char *line;
#ifdef TIME
	struct timeval start, end;
	long diff;
#endif

	if ((db = dbm_open(file, act->flags, 0644)) == NULL)
		oops("cannot open: %s", file);

	if ((line = (char *) malloc(LINEMAX)) == NULL)
		oops("%s: cannot get memory", "line alloc");

	switch (act->scode) {

	case DLOOK:
		while (fgets(line, LINEMAX, stdin) != NULL) {
			n = strlen(line) - 1;
			line[n] = 0;
			key.dptr = line;
			key.dsize = n;
			val = dbm_fetch(db, key);
			if (val.dptr != NULL) {
				prdatum(stdout, val);
				putchar('\n');
				continue;
			}
			prdatum(stderr, key);
			fprintf(stderr, ": not found.\n");
		}
		break;
	case DINSERT:
		break;
	case DDELETE:
		while (fgets(line, LINEMAX, stdin) != NULL) {
			n = strlen(line) - 1;
			line[n] = 0;
			key.dptr = line;
			key.dsize = n;
			if (dbm_delete(db, key) == -1) {
				prdatum(stderr, key);
				fprintf(stderr, ": not found.\n");
			}
		}
		break;
	case DCAT:
		for (key = dbm_firstkey(db); key.dptr != 0;
		     key = dbm_nextkey(db)) {
			prdatum(stdout, key);
			putchar('\t');
			prdatum(stdout, dbm_fetch(db, key));
			putchar('\n');
		}
		break;
	case DBUILD:
#ifdef TIME
		gettimeofday(&start, NULL);
#endif
		while (fgets(line, LINEMAX, stdin) != NULL) {
			n = strlen(line) - 1;
			line[n] = 0;
			key.dptr = line;
			if ((op = strchr(line, '\t')) != 0) {
				key.dsize = op - line;
				*op++ = 0;
				val.dptr = op;
				val.dsize = line + n - op;
			}
			else
				oops("bad input; %s", line);

			if (dbm_store(db, key, val, DBM_REPLACE) < 0) {
				prdatum(stderr, key);
				fprintf(stderr, ": ");
				oops("store: %s", "failed");
			}
		}
#ifdef TIME
		gettimeofday(&end, NULL);
		diff = ( end.tv_usec - start.tv_usec )
			+ ( end.tv_sec - start.tv_sec ) * 1000000L;

		printf("done: %ld µs.\n", diff);
#endif
		break;
	case DPRESS:
		break;
	case DCREAT:
		break;
	}

	dbm_close(db);
}

static void
badk(char *word)
{
	int i;

	if (progname)
		fprintf(stderr, "%s: ", progname);
	fprintf(stderr, "bad keyword %s. use one of\n", word);
	for (i = 0; i < (int)CTABSIZ; i++)
		fprintf(stderr, "%-8s%c", cmds[i].sname,
			((i + 1) % 6 == 0) ? '\n' : ' ');
	fprintf(stderr, "\n");
	exit(EXIT_FAILURE);
	/*NOTREACHED*/
}

static cmd *
parse(char *str)
{
	int i = CTABSIZ;
	cmd *p;

	for (p = cmds; i--; p++)
		if (strcmp(p->sname, str) == 0)
			return p;
	return NULL;
}

static void
prdatum(FILE *stream, datum d)
{
	int c;
	char *p = d.dptr;
	int n = d.dsize;

	while (n--) {
		c = *p++ & 0377;
		if (c & 0200) {
			fprintf(stream, "M-");
			c &= 0177;
		}
		if (c == 0177 || c < ' ')
			fprintf(stream, "^%c", (c == 0177) ? '?' : c + '@');
		else
			putc(c, stream);
	}
}
