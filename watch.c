#include <stdio.h>
#include <stdlib.h>
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

int main(int argc, char *argv[]) {
	setbuf(stdout, NULL); // disable buffering even non-interactively

	if (argc < 2) {
		fprintf(stderr, "usage: %s file\n", argv[0]);
		fail();
	}

	int fd;
	try (fd = open(argv[1], O_RDONLY)) else fail_with_error(argv[1]);

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

	if (change.flags & EV_ERROR) {
		fprintf(stderr, "event error: %s", strerror(change.data));
		fail();
	}

	int n, i;
	struct kevent event;
	while (1) {
		// Wait for event
		try (n = kevent(kq, NULL, 0, &event, 1, NULL)) else fail_with_error("kevent wait");
		if (n > 0) {
			if (event.fflags & NOTE_WRITE)
				printf("%s\n", argv[1]);
			if (event.fflags & NOTE_RENAME)
				fprintf(stderr, "note: file was renamed\n");
			if (event.fflags & NOTE_DELETE) {
				fprintf(stderr, "error: file was deleted\n");
				fail();
			}
		}
	}

	exit(EXIT_SUCCESS);
	// assume that the kernel closes the file descriptors
}
