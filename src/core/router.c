#include "router.h"
#include "routes/users.h"
#include "routes/health.h"
#include "handlers.h"
#include <string.h>

static const Route routes[] = {
    { "GET",  "/api/users",  NULL, users_get  },
    { "POST", "/api/users",  NULL, users_post },
    { "GET",  "/api/health", NULL, health_get },
    { "GET",  "/hello",      NULL, handler_get_hello },
    { "POST", "/api/games",  NULL, handler_create_game },
    { "POST", "/api/games/", NULL, handler_create_game },
    { "GET",  "/api/games",  NULL, handler_search_games },
    { "GET",  "/api/games/", NULL, handler_search_games },
    { "GET",  NULL,          "/api/games/", handler_api_games },
    { "PUT",  NULL,          "/api/games/", handler_api_games },
    { "HEAD", NULL,          "/api/games/", handler_api_games },
    { "PATCH", NULL,         "/api/games/", handler_api_games },
    { "POST", NULL,          "/api/games/", handler_api_games },
};

static const int route_count = sizeof(routes) / sizeof(routes[0]);

int router_get_count(void) {
    return route_count;
}

const Route *router_get_route(int index) {
    if (index < 0 || index >= route_count)
        return NULL;
    return &routes[index];
}

int router_match(const char *method, const char *path) {
    char clean[256];
    strncpy(clean, path, sizeof(clean) - 1);
    clean[sizeof(clean) - 1] = '\0';
    char *qp = strchr(clean, '?');
    if (qp) *qp = '\0';

    for (int i = 0; i < route_count; i++) {
        if (strcmp(routes[i].method, method) != 0)
            continue;
        if (routes[i].path) {
            if (strcmp(routes[i].path, clean) == 0)
                return i;
        } else if (routes[i].prefix) {
            size_t plen = strlen(routes[i].prefix);
            if (strncmp(routes[i].prefix, clean, plen) == 0)
                return i;
        }
    }
    return -1;
}

int router_dispatch(struct lws *wsi, const char *method, const char *path, void *in, size_t len) {
	int idx = router_match(method, path);
	if (idx >= 0) {
		return routes[idx].handler(wsi, method, path, in, len);
	}
	lws_return_http_status(wsi, HTTP_STATUS_NOT_FOUND, "<h1>Not Found</h1>");
	return 1;
}