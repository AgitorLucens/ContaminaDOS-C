#ifndef HTTP_H
#define HTTP_H
#include <libwebsockets.h>

struct http_session {
    char method[16];
    char path[256];
    char body[8192];
    int body_len;
};

int http_callback(struct lws *wsi, enum lws_callback_reasons reason,
                      void *user, void *in, size_t len);

#endif