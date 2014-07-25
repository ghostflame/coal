CC     = /usr/bin/gcc -Wall -Wshadow

MAXV   = 0
MINV   = 0
MICV   = 1

VER    = $(MAXV).$(MINV).$(MICV)

LNM    = libcoal
LIB    = $(LNM).so
RKV    = $(LNM).a
HDIR   = ../include
LDIR   = ./
LIBT   = $(LDIR)/$(LIB)
RKVT   = $(LDIR)/$(RKV)

IHDR   = $(HDIR)/libcoal.h
OBJS   = net.o data.o query.o init.o

LIBDIR = /usr/lib
INCDIR = /usr/include

IFLAGS = -I. -I$(HDIR) -I../../libc3db/include
DFLAGS = -g -pg -ggdb3
CFLAGS = $(IFLAGS) -fPIC -DPIC
LFLAGS = -shared -fPIC -DPIC -Wl,-soname,$(LIB).$(MAXV)

all: $(LIBT) $(RKVT)

debug: CFLAGS += $(DFLAGS)
debug: LFLAGS += $(DFLAGS)
debug: $(LIBT) $(RKVT)

fast: CFLAGS += -O2
fast:  $(LIBT) $(RKVT)

$(LIBT): $(OBJS)
	$(CC) -o $(LIBT) $(LFLAGS) $(OBJS)
	@ln -s $(LIB) $(LIB).$(MAXV)
	@echo "Built coal binary access lib."

$(RKVT): $(OBJS)
	@mkdir -p $(LDIR)
	@ar crs $(RKVT) $(OBJS)
	@ranlib $(RKVT)
	@echo "Built coal binary access archive."

install:
	@install -m644 $(RKVT) $(LIBDIR)/$(RKV)
	@install -m644 $(LIBT) $(LIBDIR)/$(LIB).$(VER)
	@rm -f $(LIBDIR)/$(LIB).$(MAXV) $(LIBDIR)/$(LIB)
	@ln -s $(LIBDIR)/$(LIB).$(VER) $(LIBDIR)/$(LIB).$(MAXV)
	@ln -s $(LIBDIR)/$(LIB).$(VER) $(LIBDIR)/$(LIB)
	@install -m644 $(IHDR) $(INCDIR)/$(IHDR)
	@ldconfig
	@echo "Installed coal binary access libs."

uninstall:
	@rm -f $(LIBDIR)/$(LIB).$(VER) $(LIBDIR)/$(LIB).$(MAXV) $(LIBDIR)/$(LIB)
	@ldconfig
	@echo "Uninstalled coal binary access libs."

clean:
	@rm -f core* *.core $(OBJS)
	@rm -f $(LIB) $(LIB).$(MAXV) $(RKV)


