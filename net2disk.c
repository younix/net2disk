#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void
usage(void)
{
	fputs("net2disk [-h] [host] [port]\n", stderr);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct sockaddr_in	 sin;
	char			*addr = "127.0.0.1";
	char			*port = "12345";
	int			 ch;

	while ((ch = getopt(argc, argv, "hs")) != -1) {
		switch (ch) {
		case 's':
			addr = "0.0.0.0";
		case 'h':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc >= 1)
		addr = argv[0];
	if (argc == 2)
		port = argv[1];

	memset(&sin, 0, sizeof sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(atoi(port));
	if (sin.sin_port == 0)
		err(1, "invalid port %s", port);

	sin.sin_addr.s_addr = inet_addr(addr);
	if (sin.sin_addr.s_addr == 0 && strcmp(addr, "0.0.0.0") != 0)
		err(1, "invalid host %s", addr);

	return 0;
}
