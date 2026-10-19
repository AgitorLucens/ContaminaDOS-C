#ifndef WS_H
#define WS_H

#include <libwebsockets.h>

// Structure to represent a connected client
struct client_data {
    struct lws *wsi;
    struct client_data *next;
};

int ws_callback(struct lws *wsi, enum lws_callback_reasons reason,
                  void *user, void *in, size_t len);

#endif // WS_H

