#include <stddef.h>
#include <string.h>
#include <libwebsockets.h>
#include "router.h"
#include "response.h"
#include <stdio.h>

struct http_session {
    char method[16];
    char path[256];
    char body[8192];
    int body_len;
};

static const char *method_to_string(int method) {
    switch (method) {
        case LWSHUMETH_GET:    return "GET";
        case LWSHUMETH_POST:   return "POST";
        case LWSHUMETH_PUT:    return "PUT";
        case LWSHUMETH_DELETE: return "DELETE";
        case LWSHUMETH_PATCH:  return "PATCH";
        case LWSHUMETH_HEAD:   return "HEAD";
        case LWSHUMETH_OPTIONS: return "OPTIONS";
        default:               return "UNKNOWN";
    }
}

static int needs_body(const char *method) {
    return strcmp(method, "POST") == 0 ||
           strcmp(method, "PUT") == 0 ||
           strcmp(method, "PATCH") == 0;
}

int http_callback(struct lws *wsi, enum lws_callback_reasons reason,
                      void *user, void *in, size_t len)
{
    struct http_session *sess = (struct http_session *)user;

    switch (reason) {
	case LWS_CALLBACK_HTTP: {
		if (!sess) return 0;

		char *uri_ptr = NULL;
		int uri_len = 0;
		int method_code = lws_http_get_uri_and_method(wsi, &uri_ptr, &uri_len);
		if (method_code < 0) {
			lws_return_http_status(wsi, HTTP_STATUS_METHOD_NOT_ALLOWED, NULL);
			return 1;
		}

		const char *method_str = method_to_string(method_code);
		strncpy(sess->method, method_str, sizeof(sess->method) - 1);
		sess->method[sizeof(sess->method) - 1] = '\0';

		const char *uri = (const char *)in;
		if (!uri) {
			lws_return_http_status(wsi, HTTP_STATUS_BAD_REQUEST, NULL);
			return 1;
		}
		strncpy(sess->path, uri, sizeof(sess->path) - 1);
		sess->path[sizeof(sess->path) - 1] = '\0';

		lwsl_user("HTTP Request: Method=%s, Path=%s\n", sess->method, sess->path);

		if (strcmp(sess->method, "OPTIONS") == 0)
			return send_cors_preflight(wsi);

		if (!needs_body(sess->method)) {
			return router_dispatch(wsi, sess->method, sess->path, NULL, 0);
		}

		sess->body_len = 0;
		sess->body[0] = '\0';
		return 0;
	}

    case LWS_CALLBACK_HTTP_BODY: {
        if (!sess || !in || len == 0) break;
        int remaining = (int)sizeof(sess->body) - sess->body_len - 1;
        int copy = (int)len < remaining ? (int)len : remaining;
        if (copy > 0) {
            memcpy(sess->body + sess->body_len, in, copy);
            sess->body_len += copy;
        }
        break;
    }

	case LWS_CALLBACK_HTTP_BODY_COMPLETION: {
		if (!sess) break;
		sess->body[sess->body_len] = '\0';
		lwsl_user("HTTP BODY received: method=%s path=%s body_len=%d\n",
			sess->method, sess->path, sess->body_len);
		return router_dispatch(wsi, sess->method, sess->path, sess->body, sess->body_len);
	}

    default:
        break;
    }

    return 0;
}