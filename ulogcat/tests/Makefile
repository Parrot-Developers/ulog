ULOG	:= ../../libulog
CFLAGS	:= -Wall -O2 -I../include -I$(ULOG)/include
LDFLAGS := -L. -L$(ULOG)/tests -lulogcat -lulog -lpthread
DYNFLAGS:= -fPIC -shared -Wl,-soname -Wl,libulogcat.so -Wl,--no-undefined \
	-pthread -lrt

SOURCES	:= \
	../libulogcat_compat.c \
	../libulogcat_core.c \
	../libulogcat_klog.c \
	../libulogcat_text.c \
	../libulogcat_ulog.c

HEADERS	:= \
	../libulogcat_list.h \
	../libulogcat_private.h \
	../../libulog/include/ulog.h \
	../../libulog/include/ulog.hpp \
	../../libulog/include/ulogbin.h \
	../../libulog/include/ulogger.h \
	../../libulog/include/ulogprint.h \
	../../libulog/include/ulograw.h \
	../include/libulogcat.h

all: libulogcat.so ulogcat ulogger libulogcat_test

libulogcat.so: $(SOURCES) $(HEADERS) $(ULOG)/tests/libulog.so
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(ULOG)/tests/libulog.so $(DYNFLAGS)

ulogcat: ../ulogcat.c libulogcat.so
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

libulogcat_test: libulogcat_test.c libulogcat.so
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

ulogger: ../../ulogger/ulogger.c $(ULOG)/tests/libulog.so
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	-rm -f libulogcat.so ulogcat ulogger libulogcat_test
