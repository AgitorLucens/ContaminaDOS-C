#ifndef RESPONSE_H
#define RESPONSE_H

#include <libwebsockets.h>

int send_json(struct lws *wsi, int http_status, const char *body);
int send_cors_preflight(struct lws *wsi);
int send_standard_response(struct lws *wsi, int http_status, int app_status, const char *msg, const char *data_json);
int send_error_response(struct lws *wsi, int http_status, const char *msg);

void json_escape(char *out, size_t out_size, const char *in);
char *json_get_string(const char *json, const char *key, char *out, size_t out_size);
int json_get_bool(const char *json, const char *key, int *out);
int json_get_string_array(const char *json, const char *key, char **out, int max_items);

#endif