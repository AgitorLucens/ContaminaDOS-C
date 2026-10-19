#include "response.h"
#include <stdio.h>
#include <string.h>

static int add_cors_headers(struct lws *wsi, unsigned char **p, unsigned char *end) {
	if (lws_add_http_header_by_name(wsi, (unsigned char *)"access-control-allow-origin:",
					(unsigned char *)"*", 1, p, end) != 0)
		return 1;
	if (lws_add_http_header_by_name(wsi, (unsigned char *)"access-control-expose-headers:",
					(unsigned char *)"Content-Length", 14, p, end) != 0)
		return 1;
	return 0;
}

int send_json(struct lws *wsi, int http_status, const char *body) {
	unsigned char buffer[LWS_PRE + 8192];
	unsigned char *p = buffer + LWS_PRE;
	unsigned char *end = buffer + sizeof(buffer);
	int len = strlen(body);
	if (lws_add_http_common_headers(wsi, http_status, "application/json", len, &p, end) != 0)
		return 1;
	if (add_cors_headers(wsi, &p, end) != 0)
		return 1;
	if (lws_finalize_http_header(wsi, &p, end) != 0)
		return 1;
	if (p + len >= end) return 1;
	memcpy(p, body, len);
	p += len;
	if (lws_write(wsi, buffer + LWS_PRE, (int)(p - (buffer + LWS_PRE)), LWS_WRITE_HTTP_FINAL) < 0)
		return 1;
	if (lws_http_transaction_completed(wsi))
		return 1;
	return 0;
}

int send_cors_preflight(struct lws *wsi) {
	unsigned char buffer[LWS_PRE + 1024];
	unsigned char *p = buffer + LWS_PRE;
	unsigned char *end = buffer + sizeof(buffer);
	if (lws_add_http_header_status(wsi, HTTP_STATUS_NO_CONTENT, &p, end) != 0)
		return 1;
	if (add_cors_headers(wsi, &p, end) != 0)
		return 1;
	if (lws_add_http_header_by_name(wsi, (unsigned char *)"access-control-allow-methods:",
					(unsigned char *)"GET, POST, PUT, PATCH, DELETE, HEAD, OPTIONS", 43, &p, end) != 0)
		return 1;
	if (lws_add_http_header_by_name(wsi, (unsigned char *)"access-control-allow-headers:",
					(unsigned char *)"Authorization, Content-Type, X-CSRF-Token, X-Max, password, player, owner, name, Origin, Accept",
					(int)strlen("Authorization, Content-Type, X-CSRF-Token, X-Max, password, player, owner, name, Origin, Accept"), &p, end) != 0)
		return 1;
	if (lws_add_http_header_by_name(wsi, (unsigned char *)"access-control-max-age:",
					(unsigned char *)"86400", 5, &p, end) != 0)
		return 1;
	if (lws_finalize_http_header(wsi, &p, end) != 0)
		return 1;
	if (lws_write(wsi, buffer + LWS_PRE, (int)(p - (buffer + LWS_PRE)), LWS_WRITE_HTTP_FINAL) < 0)
		return 1;
	if (lws_http_transaction_completed(wsi))
		return 1;
	return 0;
}

int send_standard_response(struct lws *wsi, int http_status, int app_status, const char *msg, const char *data_json) {
    char body[8192];
    if (data_json)
        snprintf(body, sizeof(body), "{\"status\":%d,\"msg\":\"%s\",\"data\":%s,\"others\":[]}", app_status, msg, data_json);
    else
        snprintf(body, sizeof(body), "{\"status\":%d,\"msg\":\"%s\",\"data\":null,\"others\":[]}", app_status, msg);
    return send_json(wsi, http_status, body);
}

int send_error_response(struct lws *wsi, int http_status, const char *msg) {
    return send_standard_response(wsi, http_status, http_status, msg, NULL);
}

void json_escape(char *out, size_t out_size, const char *in) {
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 6 < out_size; i++) {
        if (in[i] == '"' || in[i] == '\\') {
            out[j++] = '\\';
            out[j++] = in[i];
        } else if (in[i] < 32) {
            j += (size_t)snprintf(out + j, out_size - j, "\\u%04x", (unsigned char)in[i]);
        } else {
            out[j++] = in[i];
        }
    }
    out[j] = '\0';
}

static char *skip_ws(char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

static char *find_key(char *json, const char *key) {
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    char *p = strstr(json, pattern);
    if (!p) return NULL;
    p += strlen(pattern);
    p = skip_ws(p);
    if (*p != ':') return NULL;
    p++;
    p = skip_ws(p);
    return p;
}

char *json_get_string(const char *json, const char *key, char *out, size_t out_size) {
    char *p = find_key((char *)json, key);
    if (!p || *p != '"') return NULL;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < out_size) {
        if (*p == '\\' && *(p + 1)) { p++; }
        out[i++] = *p++;
    }
    out[i] = '\0';
    return out;
}

int json_get_bool(const char *json, const char *key, int *out) {
    char *p = find_key((char *)json, key);
    if (!p) return -1;
    if (strncmp(p, "true", 4) == 0) { *out = 1; return 0; }
    if (strncmp(p, "false", 5) == 0) { *out = 0; return 0; }
    return -1;
}

int json_get_string_array(const char *json, const char *key, char **out, int max_items) {
    char *p = find_key((char *)json, key);
    if (!p || *p != '[') return -1;
    p++;
    int count = 0;
    while (*p && *p != ']' && count < max_items) {
        p = skip_ws(p);
        if (*p == '"') {
            p++;
            char buf[64];
            size_t i = 0;
            while (*p && *p != '"' && i + 1 < sizeof(buf)) {
                buf[i++] = *p++;
            }
            buf[i] = '\0';
            if (*p == '"') p++;
            if (out[count]) strncpy(out[count], buf, 63);
            count++;
        }
        p = skip_ws(p);
        if (*p == ',') p++;
    }
    return count;
}