#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <fcntl.h>
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
client(struct sockaddr_in *sin, const char *file)
{
	int		s;
	int		fd;

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		err(1, "socket");

	if (connect(s, (struct sockaddr *)sin, sizeof *sin) == -1)
		err(1, "connect");

	logstr(1, sin, "connected");

	if ((fd = open(file, O_RDONLY)) == -1)
		err(1, "open: %s", file);

	for (;;) {
		char buf[BUFSIZ];
		ssize_t size;

		if ((size = read(fd, buf, sizeof buf)) == -1)
			err(1, "read");

		if (size == 0)
			break;

		logstr(2, sin, "read %zd bytes", size);

		if (write(s, buf, size) == -1)
			err(1, "write");
	}

	if (close(fd) == -1)
		err(1, "close");

	if (close(s) == -1)
		err(1, "close");

	logstr(1, sin, "closed");
}

void
server(struct sockaddr_in *sin, const char *file)
{
	socklen_t	slen = sizeof *sin;
	int		s;
	int		c;
	int		fd;

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		err(1, "socket");

	if (bind(s, (struct sockaddr *)sin, sizeof *sin) == -1)
		err(1, "bind");

	if (listen(s, 10) == -1)
		err(1, "listen");

	if ((c = accept(s, (struct sockaddr *)sin, &slen)) == -1)
		err(1, "accept");

	logstr(1, sin, "connected");

	if ((fd = open(file, O_WRONLY)) == -1)
		err(1, "open: %s", file);

	for (;;) {
		char buf[BUFSIZ];
		ssize_t size;

		if ((size = read(c, buf, sizeof buf)) == -1)
			err(1, "read");

		if (size == 0)
			break;

		logstr(2, sin, "read %zd bytes", size);

		if (write(fd, buf, size) == -1)
			err(1, "write");
	}

	if (close(fd) == -1)
		err(1, "close");

	if (close(c) == -1)
		err(1, "close");

	logstr(1, sin, "closed");
}

void
usage(void)
{
	fputs("net2disk [-sh] [host] [port]\n", stderr);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct sockaddr_in	 sin;
	char			*addr = "127.0.0.1";
	char			*port = "12345";
	char			*file = "/dev/zero";
	int			 ch;
	bool			 sflag = false;

	while ((ch = getopt(argc, argv, "svh")) != -1) {
		switch (ch) {
		case 's':
			addr = "0.0.0.0";
			file = "/dev/null";
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
		server(&sin, file);
	else
		client(&sin, file);

	return 0;
}
