#ifndef ROUTER_H
#define ROUTER_H

#include <libwebsockets.h>

typedef int (*route_handler)(struct lws *wsi, const char *method, const char *path, void *in, size_t len);

typedef struct {
    const char *method;
    const char *path;
    const char *prefix;
    route_handler handler;
} Route;

int router_dispatch(struct lws *wsi, const char *method, const char *path, void *in, size_t len);
int router_match(const char *method, const char *path);
const Route *router_get_route(int index);
int router_get_count(void);

#endif