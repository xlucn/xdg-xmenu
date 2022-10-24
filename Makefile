SRC=xdg-xmenu.c
BIN=xdg-xmenu

PREFIX=/usr/local

all: ${BIN}

${BIN}: ${SRC}
	${CC} -o ${BIN} ${SRC} -linih

install:
	install -D -m 755 ${BIN} ${DESTDIR}${PREFIX}/bin/${BIN}
	install -D -m 644 ${BIN}.1 ${DESTDIR}${PREFIX}/share/man/man1/${BIN}.1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/${BIN}
	rm -f ${DESTDIR}${PREFIX}/share/man/man1/${BIN}.1

clean:
	rm -f ${BIN}

.PHONY: install uninstall clean
