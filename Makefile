SRC=xdg-xmenu.c
BIN=xdg-xmenu
TESTS=$(wildcard tests/test_*)

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

test: ${TESTS}

tests/test_*:
	@printf "Testing %-70s" $@
	@# modify XDG_DATA_* variables to search only the test directory
	@XDG_DATA_DIRS= XDG_DATA_HOME=$@ ./xdg-xmenu -d > $@/output
	@diff $@/output $@/menu && echo "\033[32mOK\033[0m" || echo "\033[31mFailed\033[0m"
	@rm $@/output

.PHONY: install uninstall clean ${TESTS}
