PROG=xdg-xmenu

PREFIX = /usr/local

PROG:
	${CC} -Wall -linih xdg-xmenu.c -o ${PROG}

install:
	install -D -m 755 ${PROG} ${DESTDIR}${PREFIX}/bin/${PROG}

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/${PROG}

.PHONY: install uninstall
