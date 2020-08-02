PROG=xdg-xmenu

PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

install:
	install -D -m 755 ${PROG} ${DESTDIR}${PREFIX}/bin/${PROG}
	install -D -m 644 ${PROG}.1 ${DESTDIR}${MANPREFIX}/man1/${PROG}.1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/${PROG}
	rm -f ${DESTDIR}${MANPREFIX}/man1/${PROG}.1

.PHONY: install uninstall
