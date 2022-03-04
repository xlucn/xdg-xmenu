PROG=xdg-xmenu

PREFIX = /usr/local

install:
	install -D -m 755 ${PROG} ${DESTDIR}${PREFIX}/bin/${PROG}

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/${PROG}

.PHONY: install uninstall
