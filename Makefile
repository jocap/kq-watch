PREFIX = /usr/local

CFLAGS = -Wall -Wno-missing-braces -Wextra -Wpedantic

all: watch

install: watch
	cp watch $(PREFIX)/bin/watch
	cp watch.1 $(PREFIX)/man/man1/watch.1

uninstall:
	rm $(PREFIX)/bin/watch
	rm $(PREFIX)/man/man1/watch.1
