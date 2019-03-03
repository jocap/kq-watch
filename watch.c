#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <err.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/resource.h>

#define LIMIT 256

bool initial = false; // print one initial line at startup

int main(int argc, char *argv[]) {
	char **filenames;
	int first_file_index, kq;
	size_t file_count;
	struct kevent changes[LIMIT];
	struct kevent events[LIMIT];
	struct rlimit rlp;

	file_count = argc - 1;
	first_file_index = 1; // index of first file argument

	setbuf(stdout, NULL); // disable buffering even non-interactively

	if (argc < 2) goto usage; // quit if no arguments
	if (argv[1][0] == '-') {
		if (argc < 3) goto usage; // quit if no filenames

		/* adjust indices */
		file_count = argc - 2;
		first_file_index = 2;

		if (strcmp(argv[1], "-i") == 0)
			initial = true;
		else
			goto usage;
	}

	if (file_count > LIMIT)
		errx(1, "more than %d files", LIMIT);

	if ((kq = kqueue()) == -1) err(1, "kqueue");

	/* save filenames by file descriptor */
	if (getrlimit(RLIMIT_NOFILE, &rlp) == -1)
		err(1, "getrlimit");
	if ((filenames = reallocarray(NULL, rlp.rlim_max, sizeof(char **))) == NULL)
		err(1, "reallocarray");

	/* add each file to change list */
	for (int i = first_file_index; i < argc; i++) {
		int fd;
		if ((fd = open(argv[i], O_RDONLY)) == -1)
			err(1, "%s", argv[i]);

		filenames[fd] = argv[i];

		if (initial)
			printf("%s\n", argv[i]);

		struct kevent change = { // event to watch for
			.ident = fd,
			.filter = EVFILT_VNODE,
			.flags = EV_ADD | EV_CLEAR,
			.fflags = NOTE_WRITE | NOTE_DELETE,
			.data = 0,
			.udata = NULL
		};

		changes[i - first_file_index] = change;
	}

	if (pledge("stdio", NULL) == -1) err(1, "pledge");

	for (;;) {
		int n;
		/* register changes and wait for events */
		if ((n = kevent(kq, changes, file_count, events, file_count, NULL)) == -1)
			err(1, "kevent wait");
		if (n > 0) {
			for (int i = 0; i < n; i++) {
				if (events[i].flags & EV_ERROR)
					errx(1, "event error: %s", strerror(events[i].data));
				if (events[i].fflags & NOTE_WRITE)
					printf("%s\n", filenames[events[i].ident]);
				if (events[i].fflags & NOTE_DELETE)
					errx(1, "%s was deleted", filenames[events[i].ident]);
			}
		}
	}

	return 0; // assume that the kernel closes the file descriptors

usage:
	fprintf(stderr, "usage: %s [-i] file\n", argv[0]);
}
