SRC=xdg-xmenu.c
BIN=xdg-xmenu

PREFIX=/usr/local

all: ${BIN}

${BIN}: ${SRC}
	${CC} -Wall -linih ${SRC} -o ${BIN} -g

install:
	install -D -m 755 ${BIN} ${DESTDIR}${PREFIX}/bin/${PROG}

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/${BIN}

clean:
	rm -f ${BIN}

.PHONY: install uninstall clean
