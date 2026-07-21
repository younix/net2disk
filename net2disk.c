#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int verbosity = 0;

void
logstr(int level, struct sockaddr_in *sin, const char *fmt, ...)
{
	va_list args;

	if (verbosity < level)
		return;

	printf("%s:%hu ", inet_ntoa(sin->sin_addr), ntohs(sin->sin_port));

	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);

	putchar('\n');
}

void
usage(void)
{
	fputs("net2disk [-sh] [host] [port]\n", stderr);
	exit(1);
}

void
server(struct sockaddr_in *sin)
{
	socklen_t	slen = sizeof *sin;
	int		s;
	int		c;

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		err(1, "socket");

	if (bind(s, (struct sockaddr *)sin, sizeof *sin) == -1)
		err(1, "bind");

	if (listen(s, 10) == -1)
		err(1, "listen");

	if ((c = accept(s, (struct sockaddr *)sin, &slen)) == -1)
		err(1, "accept");

	logstr(1, sin, "connected");

	for (;;) {
		char buf[BUFSIZ];
		ssize_t size;

		if ((size = read(c, buf, sizeof buf)) == -1)
			err(1, "read");

		if (size == 0) {
			logstr(1, sin, "closed");
			break;
		}

		logstr(2, sin, "read %zd bytes", size);
	}
}

int
main(int argc, char *argv[])
{
	struct sockaddr_in	 sin;
	char			*addr = "127.0.0.1";
	char			*port = "12345";
	int			 ch;
	bool			 sflag = false;

	while ((ch = getopt(argc, argv, "svh")) != -1) {
		switch (ch) {
		case 's':
			addr = "0.0.0.0";
			sflag = true;
			break;
		case 'v':
			verbosity++;
			break;
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

	if (sflag)
		server(&sin);

	return 0;
}
