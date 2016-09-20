CFLAGS := -Wall -O2 -I../include
LDFLAGS := -L../ -lulog -lpthread

all: ulogtest ulogtestcpp

ulogtest: ulogtest.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

ulogtestcpp: ulogtest.cpp
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $<
