#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libwebsockets.h>
#include <signal.h>

#include "http.h"
#include "ws.h"
#include "main.h"
#include "dotenv.h"

static volatile int force_exit = 0;

void sighandler(int sig) {
    force_exit = 1;
}

static struct lws_protocols protocols[] = {
	{
		"http-only",
		http_callback,
		sizeof(struct http_session),
		MAX_MSG_PAYLOAD
	},
	{
		"chat-protocol",
		ws_callback,
		0,
		MAX_MSG_PAYLOAD
	},
	{ NULL, NULL, 0, 0 }
};

static struct lws_context *context;

int main(int argc, char **argv) {
    struct lws_context_creation_info info;
    int ret = 0;

    dotenv_load(".env");

    signal(SIGINT, sighandler);

    memset(&info, 0, sizeof(info));
    info.port = get_port();
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;

	lws_set_log_level(LLL_USER | LLL_ERR | LLL_WARN | LLL_INFO | LLL_NOTICE, NULL);

    lwsl_user("Starting WebSocket chat server on port %d...\n", get_port());

    context = lws_create_context(&info);
    if (!context) {
        lwsl_err("lws init failed\n");
        return 1;
    }

    while (ret >= 0 && !force_exit) {
        ret = lws_service(context, 50);
    }

    lwsl_user("Shutting down server...\n");
    lws_context_destroy(context);

    struct client_data *current = client_list;
    struct client_data *next = NULL;
    while (current) {
        next = current->next;
        free(current);
        current = next;
    }
    client_list = NULL;

    lwsl_user("Server shut down cleanly.\n");
    return 0;
}