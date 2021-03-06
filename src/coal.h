#ifndef COAL_H
#define COAL_H

#define _GNU_SOURCE

// this pulls in a lot of basics
#include <c3db.h>

// here's what we need in addition
#include <poll.h>
#include <netdb.h>
#include <regex.h>
#include <dirent.h>
#include <signal.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/sha.h>

// this pulls in some definitions of the binary interface
#include <libcoal.h>


// crazy-prevention - this goes here
#include "typedefs.h"

// independent
#include "run.h"
#include "utils.h"
#include "log.h"
#include "thread.h"

// dependent - order matters
#include "mem.h"
#include "net.h"
#include "loop.h"
#include "data.h"
#include "node.h"
#include "out.h"
#include "search.h"
#include "query.h"
#include "sync.h"
#include "stats.h"
#include "relay.h"

// depends on almost everything
#include "config.h"


// then our control structure
COAL_CTL *ctl;



// functions from main
void shut_down( int exval );


#endif
