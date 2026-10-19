#ifndef MAIN_H
#define MAIN_H

#include <stdlib.h>
#include "ws.h"

#ifndef MAX_MSG_PAYLOAD
#define MAX_MSG_PAYLOAD 1024
#endif

static inline int get_port(void) {
    char *env = getenv("PORT");
    return env ? atoi(env) : 3001;
}

static struct client_data *client_list = NULL;

#endif