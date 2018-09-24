#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#define fail() exit(EXIT_FAILURE)
#define fail_clean() do { status = EXIT_FAILURE; goto finally; } while (0)

int main(int argc, char *argv[]) {
	int status = EXIT_SUCCESS;
	int fd, kq, n, i;
	struct kevent event;

	setbuf(stdout, NULL); // disable buffering even non-interactively

	if (argc < 2) {
		fprintf(stderr, "usage: %s file\n", argv[0]);
		fail();
	}

	// 1. Open file
	if ((fd = open(argv[1], O_RDONLY)) == -1) {
		 perror(argv[1]);
		 fail();
	}

	if ((kq = kqueue()) == -1) {
		perror("kqueue");
		fail_clean();
	}

	struct kevent change = { // event to watch for
		.ident = fd,
		.filter = EVFILT_VNODE,
		.flags = EV_ADD | EV_CLEAR,
		.fflags = NOTE_WRITE | NOTE_RENAME | NOTE_DELETE,
		.data = 0,
		.udata = NULL
	};

	// 2. Register event to watch for
	if (kevent(kq, &change, 1, NULL, 0, NULL) == -1) {
		perror("kevent");
		fail_clean();
	}
	if (change.flags & EV_ERROR) {
		fprintf(stderr, "Event error: %s", strerror(change.data));
		fail_clean();
	}

	while (1) {
		// 3. Wait for event
		if ((n = kevent(kq, NULL, 0, &event, 1, NULL)) != 1) {
			perror("kevent wait");
			fail_clean();
		} else if (n > 0) {
			if (event.fflags & NOTE_WRITE) {
				printf("%s\n", argv[1]);
			}
			if (event.fflags & NOTE_RENAME)
				fprintf(stderr, "Note: file was renamed\n");
			if (event.fflags & NOTE_DELETE) {
				fprintf(stderr, "Error: file was deleted\n");
				fail_clean();
			}
		}
	}

finally:
	if (close(kq) == -1) {
		perror("close");
		fail();
	}
	if (close(fd) == -1) {
		perror("close");
		fail();
	}

	exit(status);
}
