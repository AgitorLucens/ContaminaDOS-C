#ifndef USERS_H
#define USERS_H

#include <libwebsockets.h>

int users_get(struct lws *wsi, const char *method, const char *path, void *in, size_t len);
int users_post(struct lws *wsi, const char *method, const char *path, void *in, size_t len);

#endif
