#include "health.h"
#include "response.h"
#include <string.h>

int health_get(struct lws *wsi, const char *method, const char *path, void *in, size_t len) {
	(void)method; (void)path; (void)in; (void)len;
	return send_standard_response(wsi, 200, 200, "healthy", NULL);
}
