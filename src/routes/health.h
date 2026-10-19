#ifndef HEALTH_H
#define HEALTH_H

#include <libwebsockets.h>

int health_get(struct lws *wsi, const char *method, const char *path, void *in, size_t len);

#endif
