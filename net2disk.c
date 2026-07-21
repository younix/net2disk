/*
 * Copyright (c) 2026 Jan Klemkow <j.klemkow@wemelug.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define nitems(_a)	(sizeof((_a)) / sizeof((_a)[0]))
#define READ	0
#define WRITE	1

int verbosity = 0;
bool loop = true;

void
sighandler(int sig)
{
	if (sig == SIGALRM)
		loop = false;
}

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

ssize_t
client(struct sockaddr_in *sin, const char *file, int jobs, unsigned int sec)
{
	ssize_t		sum = 0;
	ssize_t		val;
	socklen_t	slen = sizeof *sin;
	int		s;
	int		fd;
	int		p[2];
	int		child[jobs];

 next:
	if (pipe(p) == -1)
		err(1, "pipe");

	switch (fork()) {
	case -1:
		err(1, "fork");
	case 0:
		if (close(p[READ]) == -1)
			err(1, "close pipe");

		break;
	default:
		/*
		 * parent
		 */

		if (close(p[WRITE]) == -1)
			err(1, "close pipe");

		child[--jobs] = p[READ];

		if (jobs)
			goto next;


		for (size_t i = 0; i < nitems(child); i++) {
			if (read(child[i], &val, sizeof val) == -1)
				err(1, "read pipe");
			sum += val;

			if (wait(NULL) == -1)
				err(1, "wait");
		}

		return sum;
	}

	/*
	 * child
	 */

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		err(1, "socket");

	if (connect(s, (struct sockaddr *)sin, sizeof *sin) == -1)
		err(1, "connect");

	if (getsockname(s, (struct sockaddr *)sin, &slen) == -1)
		err(1, "getsockname");

	logstr(1, sin, "connected");

	if ((fd = open(file, O_RDONLY)) == -1)
		err(1, "open: %s", file);

	if (signal(SIGALRM, sighandler) == SIG_ERR)
		err(1, "signal");
	logstr(1, sin, "alarm in %u sec", sec);
	if (alarm(sec) != 0)
		err(1, "alarm");

	while (loop) {
		char buf[BUFSIZ];
		ssize_t size;

		if ((size = read(fd, buf, sizeof buf)) == -1)
			err(1, "read");
		logstr(2, sin, "read %llu bytes", size);

		if (size == 0)
			break;

		sum += size;
		logstr(2, sin, "read %zd bytes", size);

		if (write(s, buf, size) == -1)
			err(1, "write");
		logstr(2, sin, "write %llu bytes", size);
	}

	logstr(2, sin, "loop exit");

	if (write(p[1], &sum, sizeof sum) == -1)
		err(1, "write pipe");

	if (close(p[1]) == -1)
		err(1, "close pipe");

	if (close(fd) == -1)
		err(1, "close file");

	if (close(s) == -1)
		err(1, "close socket");

	logstr(1, sin, "closed");

	exit(0);
}

void
server(struct sockaddr_in *sin, const char *file)
{
	char		path[PATH_MAX];
	socklen_t	slen = sizeof *sin;
	int		s;
	int		c;
	int		fd;
	int		flags = O_WRONLY;

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		err(1, "socket");

	if (bind(s, (struct sockaddr *)sin, sizeof *sin) == -1)
		err(1, "bind");

	if (listen(s, 10) == -1)
		err(1, "listen");

 next:
	if ((c = accept(s, (struct sockaddr *)sin, &slen)) == -1)
		err(1, "accept");

	switch (fork()) {
	case -1:
		err(1, "fork");
	case 0:
		break;
	default:
		goto next;
	};

	logstr(1, sin, "connected");

 again:
	if ((fd = open(file, flags, S_IRUSR|S_IWUSR)) == -1) {
		if (errno == EISDIR) {
			snprintf(path, sizeof path, "%s/%s:%hu", file,
			    inet_ntoa(sin->sin_addr), ntohs(sin->sin_port));
			flags |= O_CREAT;
			file = path;
			logstr(1, sin, "create file");

			goto again;
		} else
			err(1, "open: %s", file);
	}

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

	if (flags | O_CREAT) {
		logstr(1, sin, "unlink file");

		if (unlink(file) == -1)
			err(1, "unlink: %s", file);
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
	fputs("net2disk [-bhsv] [-f file] [-j jobs] [-t sec] [host] [port]\n",
	    stderr);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct sockaddr_in	 sin;
	char			*addr = "127.0.0.1";
	char			*port = "12345";
	char			*file = "/dev/zero";
	unsigned int		 sec = 5;
	int			 ch;
	int			 jobs = 1;
	bool			 sflag = false;
	bool			 bflag = false;
	bool			 hflag = false;

	while ((ch = getopt(argc, argv, "bf:hj:st:v")) != -1) {
		switch (ch) {
		case 'b':
			bflag = true;
			break;
		case 'f':
			file = optarg;
			break;
		case 'h':
			hflag = true;
			break;
		case 'j':
			jobs = atoi(optarg);
			if (jobs == 0)
				err(1, "invalid jobs: %s", optarg);
			break;
		case 's':
			addr = "0.0.0.0";
			if (strcmp(file, "/dev/zero") == 0)
				file = "/dev/null";
			sflag = true;
			break;
		case 't':
			sec = atoi(optarg);
			if (sec == 0)
				err(1, "invalid waiting time: %s", optarg);
			break;
		case 'v':
			verbosity++;
			break;
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
	else {
		ssize_t	 factor = 1024;
		ssize_t	 sum = client(&sin, file, jobs, sec);
		char	*unit = "";

		if (bflag) {
			sum *= 8;
			factor = 1000;
		}

		while (hflag && sum > factor*10) {
			sum /= factor;

			if (strcmp(unit, "") == 0)
				unit = bflag ? "K" : "Ki";
			else if (strcmp(unit, bflag ? "K" : "Ki") == 0)
				unit = bflag ? "M" : "Mi";
			else if (strcmp(unit, bflag ? "M" : "Mi") == 0)
				unit = bflag ? "G" : "Gi";
			else if (strcmp(unit, bflag ? "G" : "Gi") == 0)
				unit = bflag ? "T" : "Ti";
			else if (strcmp(unit, bflag ? "T" : "Ti") == 0) {
				unit = bflag ? "P" : "Pi";
				break;
			}
		}

		printf("%zd %s%s/s\n", sum / sec, unit,
		    bflag ? "bits" : "Bytes");
	}

	return 0;
}
