#ifdef __linux
# define _XOPEN_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <regex.h>
#include <ctype.h>
#include <unistd.h>

#include "sdbm.h"
#include "util.h"


void
print_datum(datum db)
{
	int i;

	putchar('"');
	for (i = 0; i < db.dsize; i++) {
		if (isprint(db.dptr[i]))
			putchar(db.dptr[i]);
		else {
			putchar('\\');
			putchar('0' + ((db.dptr[i] >> 6) & 0x07));
			putchar('0' + ((db.dptr[i] >> 3) & 0x07));
			putchar('0' + (db.dptr[i] & 0x07));
		}
	}
	putchar('"');
}


datum
read_datum(char *s)
{
	datum db;
	char *p;
	int i;

	db.dsize = 0;
	db.dptr = (char *) malloc(strlen(s) * sizeof(char));
	for (p = db.dptr; *s != '\0'; p++, db.dsize++, s++) {
		if (*s == '\\') {
			if (*++s == 'n')
				*p = '\n';
			else if (*s == 'r')
				*p = '\r';
			else if (*s == 'f')
				*p = '\f';
			else if (*s == 't')
				*p = '\t';
			else if (isdigit(*s) && isdigit(*(s + 1)) && isdigit(*(s + 2))) {
				i = (*s++ - '0') << 6;
				i |= (*s++ - '0') << 3;
				i |= *s - '0';
				*p = i;
			}
			else if (*s == '0')
				*p = '\0';
			else
				*p = *s;
		}
		else
			*p = *s;
	}

	return db;
}


char *
key2s(datum db)
{
	char *buf;
	char *p1, *p2;

	buf = (char *) malloc((db.dsize + 1) * sizeof(char));
	for (p1 = buf, p2 = db.dptr; *p2 != '\0'; *p1++ = *p2++);
	*p1 = '\0';
	return buf;
}


int
main(int argc, char **argv)
{
	typedef enum {
		YOW, FETCH, STORE, DELETE, SCAN, REGEXP
	} commands;
	char opt;
	int flags = O_RDWR;
	int giveusage = 0;
	int verbose = 0;
	commands what = YOW;
	int st_flag = DBM_INSERT;
	DBM *db;
	datum key;
	datum content;
	regex_t *re = NULL;

	while ((opt = getopt(argc, argv, "acdfFm:rstvx")) != -1) {
		switch (opt) {
		case 'a':
			what = SCAN;
			break;
		case 'c':
			flags |= O_CREAT;
			break;
		case 'd':
			what = DELETE;
			break;
		case 'f':
			what = FETCH;
			break;
		case 'F':
			what = REGEXP;
			break;
		case 'm':
			flags &= ~(000007);
			if (strcmp(optarg, "r") == 0)
				flags |= O_RDONLY;
			else if (strcmp(optarg, "w") == 0)
				flags |= O_WRONLY;
			else if (strcmp(optarg, "rw") == 0)
				flags |= O_RDWR;
			else {
				fprintf(stderr, "Invalid mode: \"%s\"\n", optarg);
				giveusage = 1;
			}
			break;
		case 'r':
			st_flag = DBM_REPLACE;
			break;
		case 's':
			what = STORE;
			break;
		case 't':
			flags |= O_TRUNC;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'x':
			flags |= O_EXCL;
			break;
		default:
			giveusage = 1;
			break;
		}
	}

	if (giveusage | (what == YOW) | (argc < 1)) {
		fprintf(stderr, "Usage: %s [-m r|w|rw] [-crtx] -a|-d|-f|-F|-s database [key [content]]\n", argv[0]);
		return EXIT_FAILURE;
	}

	argc -= optind;
	argv += optind;

	if ((db = dbm_open(argv[0], flags, 0777)) == NULL) {
		fprintf(stderr, "Error opening database \"%s\"\n", argv[0]);
		return EXIT_FAILURE;
	}

	if (argc > 1)
		key = read_datum(argv[1]);
	if (argc > 2)
		content = read_datum(argv[2]);

	switch (what) {

	case YOW:
		/* make compiler happy */
		break;

	case SCAN:
		key = dbm_firstkey(db);
		if (dbm_error(db)) {
			fprintf(stderr, "Error when fetching first key\n");
			goto db_exit;
		}
		while (key.dptr != NULL) {
			content = dbm_fetch(db, key);
			if (dbm_error(db)) {
				fprintf(stderr, "Error when fetching ");
				print_datum(key);
				printf("\n");
				goto db_exit;
			}
			print_datum(key);
			printf(": ");
			print_datum(content);
			printf("\n");
			if (dbm_error(db)) {
				fprintf(stderr, "Error when fetching next key\n");
				goto db_exit;
			}
			key = dbm_nextkey(db);
		}
		break;

	case REGEXP:
		if (argc < 2) {
			fprintf(stderr, "Missing regular expression.\n");
			goto db_exit;
		}
		if (!regcomp(re, argv[1], REG_NOSUB)) {
			fprintf(stderr, "Invalid regular expression\n");
			goto db_exit;
		}
		key = dbm_firstkey(db);
		if (dbm_error(db)) {
			fprintf(stderr, "Error when fetching first key\n");
			goto db_exit;
		}
		while (key.dptr != NULL) {
			if (regexec(re, key2s(key), 0, NULL, 0)) {
				content = dbm_fetch(db, key);
				if (dbm_error(db)) {
					fprintf(stderr, "Error when fetching ");
					print_datum(key);
					printf("\n");
					goto db_exit;
				}
				print_datum(key);
				printf(": ");
				print_datum(content);
				printf("\n");
				if (dbm_error(db)) {
					fprintf(stderr, "Error when fetching next key\n");
					goto db_exit;
				}
			}
			key = dbm_nextkey(db);
		}
		regfree(re);
		break;

	case FETCH:
		if (argc < 2) {
			fprintf(stderr, "Missing fetch key.\n");
			goto db_exit;
		}
		content = dbm_fetch(db, key);
		if (dbm_error(db)) {
			fprintf(stderr, "Error when fetching ");
			print_datum(key);
			printf("\n");
			goto db_exit;
		}
		if (content.dptr == NULL) {
			fprintf(stderr, "Cannot find ");
			print_datum(key);
			printf("\n");
			goto db_exit;
		}
		print_datum(key);
		printf(": ");
		print_datum(content);
		printf("\n");
		break;

	case DELETE:
		if (argc < 2) {
			fprintf(stderr, "Missing delete key.\n");
			goto db_exit;
		}
		if (dbm_delete(db, key) || dbm_error(db)) {
			fprintf(stderr, "Error when deleting ");
			print_datum(key);
			printf("\n");
			goto db_exit;
		}
		if (verbose) {
			print_datum(key);
			printf(": DELETED\n");
		}
		break;

	case STORE:
		if (argc < 3) {
			fprintf(stderr, "Missing key and/or content.\n");
			goto db_exit;
		}
		if (dbm_store(db, key, content, st_flag) || dbm_error(db)) {
			fprintf(stderr, "Error when storing ");
			print_datum(key);
			printf("\n");
			goto db_exit;
		}
		if (verbose) {
			print_datum(key);
			printf(": ");
			print_datum(content);
			printf(" STORED\n");
		}
		break;
	}

db_exit:
	dbm_clearerr(db);
	dbm_close(db);
	if (dbm_error(db)) {
		fprintf(stderr, "Error closing database \"%s\"\n", argv[0]);
		return EXIT_FAILURE;
	}

    return EXIT_SUCCESS;
}
