#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#define try(x) if ((x) != -1) {}

void fail() {
	exit(EXIT_FAILURE);
}

void fail_with_error(char *s) {
	perror(s);
	exit(EXIT_FAILURE);
}

#define fail_with(...) {          \
	fprintf(stderr, __VA_ARGS__); \
	exit(EXIT_FAILURE);           \
}

bool initial = false; /* print one initial line at startup */

int main(int argc, char *argv[]) {
	size_t file = 1; /* argv index */

	setbuf(stdout, NULL); // disable buffering even non-interactively

	if (argc < 2) goto usage;
	if (argc == 3) {
		if (strncmp(argv[1], "-i", 2) == 0) {
			file = 2;
			initial = true;
		} else {
			goto usage;
		}
	}

	int fd;
	try (fd = open(argv[file], O_RDONLY)) else fail_with_error(argv[file]);

	if (initial)
		printf("%s\n", argv[file]);

	try (pledge("stdio", NULL)) else fail_with_error("pledge");

	int kq;
	try (kq = kqueue()) else fail_with_error("kqueue");

	struct kevent change = { // event to watch for
		.ident = fd,
		.filter = EVFILT_VNODE,
		.flags = EV_ADD | EV_CLEAR,
		.fflags = NOTE_WRITE | NOTE_RENAME | NOTE_DELETE,
		.data = 0,
		.udata = NULL
	};

	// Register event to watch for
	try (kevent(kq, &change, 1, NULL, 0, NULL)) else fail_with_error("kevent");

	if (change.flags & EV_ERROR)
		fail_with("event error: %s", strerror(change.data));

	int n;
	struct kevent event;
	while (1) {
		// Wait for event
		try (n = kevent(kq, NULL, 0, &event, 1, NULL))
			else fail_with_error("kevent wait");
		if (n > 0) {
			if (event.fflags & NOTE_WRITE)
				printf("%s\n", argv[file]);
			if (event.fflags & NOTE_RENAME)
				fprintf(stderr, "note: file was renamed\n");
			if (event.fflags & NOTE_DELETE)
				fail_with("error: file was deleted\n");
		}
	}

	// assume that the kernel closes the file descriptors

usage:
	fail_with("usage: %s [-i] file\n", argv[0]);
}
