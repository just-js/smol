CC=g++
RELEASE=0.0.1
INSTALL=/usr/local/bin
LIBS=
MODULES=
TARGET=smol
LIB=
EMBEDS=just.cc just.h Makefile main.cc main.js
FLAGS=${CFLAGS}
LFLAG=${LFLAGS}
JUST_HOME=$(shell pwd)

.PHONY: help clean

help:
	@awk 'BEGIN {FS = ":.*?## "} /^[a-zA-Z0-9_\.-]+:.*?## / {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}' $(MAKEFILE_LIST)

deps/v8/libv8_monolith.a: ## download v8 monolithic library for linking
ifeq (,$(wildcard /etc/alpine-release))
	curl -L -o v8lib-$(RELEASE).tar.gz https://raw.githubusercontent.com/just-js/libv8/$(RELEASE)/v8.tar.gz
else
	curl -L -o v8lib-$(RELEASE).tar.gz https://raw.githubusercontent.com/just-js/libv8/$(RELEASE)/v8-alpine.tar.gz
endif
	tar -zxvf v8lib-$(RELEASE).tar.gz
	rm -f v8lib-$(RELEASE).tar.gz

builtins.o: just.cc just.h Makefile main.cc ## compile builtins with build dependencies
	gcc builtins.S -c -o builtins.o

main: builtins.o deps/v8/libv8_monolith.a
	$(CC) -c ${FLAGS} -DJUST_VERSION='"${RELEASE}"' -std=c++17 -DV8_COMPRESS_POINTERS -I. -I./deps/v8/include -g -O3 -march=native -mtune=native -Wpedantic -Wall -Wextra -flto -Wno-unused-parameter just.cc
	$(CC) -c ${FLAGS} -std=c++17 -DV8_COMPRESS_POINTERS -I. -I./deps/v8/include -g -O3 -march=native -mtune=native -Wpedantic -Wall -Wextra -flto -Wno-unused-parameter main.cc
	$(CC) -g -rdynamic -flto -pthread -m64 -Wl,--start-group deps/v8/libv8_monolith.a main.o just.o builtins.o ${MODULES} -Wl,--end-group ${LFLAG} ${LIB} -o ${TARGET} -Wl,-rpath=/usr/local/lib/${TARGET}
	objcopy --only-keep-debug ${TARGET} ${TARGET}.debug
	strip --strip-debug --strip-unneeded ${TARGET}
	objcopy --add-gnu-debuglink=${TARGET}.debug ${TARGET}

clean: ## tidy up
	rm -f *.o
	rm -f ${TARGET}

cleanall: ## remove just and build deps
	rm -fr deps
	rm -f *.gz
	$(MAKE) clean

install: ## install
	mkdir -p ${INSTALL}
	cp -f ${TARGET} ${INSTALL}/${TARGET}

install-debug: ## install debug symbols
	mkdir -p ${INSTALL}/.debug
	cp -f ${TARGET}.debug ${INSTALL}/.debug/${TARGET}.debug

uninstall: ## uninstall
	rm -f ${INSTALL}/${TARGET}
	rm -f ${INSTALL}/${TARGET}/.debug

.DEFAULT_GOAL := help
