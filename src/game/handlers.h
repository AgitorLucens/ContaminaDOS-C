#ifndef HANDLERS_H
#define HANDLERS_H

#include <libwebsockets.h>

int handler_get_hello(struct lws *wsi, const char *method, const char *path, void *in, size_t len);
int handler_create_game(struct lws *wsi, const char *method, const char *path, void *in, size_t len);
int handler_search_games(struct lws *wsi, const char *method, const char *path, void *in, size_t len);
int handler_api_games(struct lws *wsi, const char *method, const char *path, void *in, size_t len);

#endif