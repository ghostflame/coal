CC     = /usr/bin/gcc -Wall -Wshadow

MAXV   = 0
MINV   = 0
MICV   = 1

VER    = $(MAXV).$(MINV).$(MICV)

BINDIR = ../bin
LC3DIR = ../../libc3db/lib
HC3DIR = ../../libc3db/include
LCLDIR = ../lib
HCLDIR = ../include

FILES  = utils priv log thread config mem loop node net data query out_bin out_json out_lines relay search sync stats main

OBJS   = $(FILES:%=%.o)
#OBJS   = utils.o log.o thread.o config.o mem.o loop.o node.o net.o data.o query.o out_bin.o out_json.o out_lines.o search.o sync.o stats.o relay.o main.o

IFLAGS = -I. -I$(HC3DIR) -I$(HCLDIR)
DFLAGS = -g -pg -ggdb3 -DDEBUG
TFLAGS = -pthread
CFLAGS = $(TFLAGS) $(IFLAGS)
LFLAGS = $(TFLAGS) $(IFLAGS) -lm

BIN    = coal



all:   $(BIN)

debug: CFLAGS += $(DFLAGS)
debug: LFLAGS += $(DFLAGS)
debug: $(BIN)

fast:  CFLAGS += -O2
fast:  $(BIN)

$(BIN): $(OBJS)
	@mkdir -p $(BINDIR)
	$(CC) -o $(BINDIR)/$(BIN) $(LFLAGS) $(OBJS) $(LC3DIR)/libc3db.a
	@echo "Built coal.  Go mining!"

install:
	@install -m755 $(BINDIR)/$(BIN) $(COAL_BIN_DIR)/$(BIN)
	@echo "Installed coal as $(COAL_BIN_DIR)/$(BIN)"

clean:
	@rm -f core* *.core $(OBJS) $(BINDIR)/$(BIN)


