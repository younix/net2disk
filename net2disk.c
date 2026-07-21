#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void
usage(void)
{
	fputs("net2disk [-h]\n", stderr);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int ch;

	while ((ch = getopt(argc, argv, "h")) != -1) {
		switch (ch) {
		case 'h':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	return 0;
}
