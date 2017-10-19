CFLAGS := -Wall -O2 -I../include
LDFLAGS := -L../ -L. -lulog -lpthread
DYNFLAGS:= -fPIC -shared -Wl,-soname -Wl,libulog.so -Wl,--no-undefined \
	-pthread -lrt

SOURCES	:= \
	../ulog_write.c \
	../ulog_read.c \
	../ulog_write_android.c \
	../ulog_write_bin.c \
	../ulog_write_raw.c
HEADERS	:= \
	../include/ulog.h \
	../include/ulog.hpp \
	../include/ulogbin.h \
	../include/ulogger.h \
	../include/ulogprint.h \
	../include/ulograw.h

all:  libulog.so ulogtest

libulog.so: $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(DYNFLAGS)

ulogtest: ulogtest.c libulog.so
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

ulogtestcpp: ulogtest.cpp
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	-rm -f libulog.so ulogtest
