all: s5c s5s

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)
COBJS := $(subst server.o,,$(OBJS))
SOBJS := $(subst client.o,,$(OBJS))
CC := gcc
CFLAGS := -O2
LDFLAGS := -lev

-include $(SRCS:.c=.d)

%.d: %.c
	@set -e; rm -f $@; \
	$(CC) -MM $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

s5c: $(COBJS)
	gcc -o $@ $(CFLAGS) $^ $(LDFLAGS)
	strip $@

s5s: $(SOBJS)
	gcc -o $@ $(CFLAGS) $^ $(LDFLAGS)
	strip $@

clean:
	-rm -f *.d
	-rm -f *.d.*
	-rm -f *.o
	-rm -f s5c s5s

.PHONY: all clean
