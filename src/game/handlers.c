#include "handlers.h"
#include "response.h"
#include "gamepool.h"
#include "models.h"
#include <stdio.h>
#include <string.h>

static void game_to_json(Game *g, char *out, size_t size) {
    char name_esc[MAX_NAME_LEN * 2];
    char owner_esc[128];
    char created_esc[64];
    char updated_esc[64];
    json_escape(name_esc, sizeof(name_esc), g->name);
    json_escape(owner_esc, sizeof(owner_esc), g->owner);
    json_escape(created_esc, sizeof(created_esc), g->created_at);
    json_escape(updated_esc, sizeof(updated_esc), g->updated_at);

    const char *status_str = "lobby";
    if (g->status == GAME_STATUS_ROUNDS) status_str = "rounds";
    else if (g->status == GAME_STATUS_ENDED) status_str = "ended";

    char players_json[2048] = "";
    int pos = 0;
    pos += snprintf(players_json + pos, sizeof(players_json) - pos, "[");
    for (int i = 0; i < g->player_count; i++) {
        if (i > 0) pos += snprintf(players_json + pos, sizeof(players_json) - pos, ",");
        pos += snprintf(players_json + pos, sizeof(players_json) - pos, "\"%s\"", g->players[i]);
    }
    snprintf(players_json + pos, sizeof(players_json) - pos, "]");

    char enemies_json[2048] = "";
    pos = 0;
    pos += snprintf(enemies_json + pos, sizeof(enemies_json) - pos, "[");
    for (int i = 0; i < g->enemy_count; i++) {
        if (i > 0) pos += snprintf(enemies_json + pos, sizeof(enemies_json) - pos, ",");
        pos += snprintf(enemies_json + pos, sizeof(enemies_json) - pos, "\"%s\"", g->enemies[i]);
    }
    snprintf(enemies_json + pos, sizeof(enemies_json) - pos, "]");

    snprintf(out, size,
        "{"
        "\"name\":\"%s\","
        "\"owner\":\"%s\","
        "\"status\":\"%s\","
        "\"id\":\"%s\","
        "\"createdAt\":\"%s\","
        "\"updatedAt\":\"%s\","
        "\"players\":%s,"
        "\"enemies\":%s,"
        "\"currentRound\":\"%s\""
        "}",
        name_esc, owner_esc, status_str, g->id,
        created_esc, updated_esc,
        players_json, enemies_json, g->current_round);
}

static void round_to_json(Round *r, char *out, size_t size) {
    const char *status_str = "waiting-on-leader";
    if (r->status == ROUND_STATUS_VOTING) status_str = "voting";
    else if (r->status == ROUND_STATUS_WAITING_ON_GROUP) status_str = "waiting-on-group";
    else if (r->status == ROUND_STATUS_ENDED) status_str = "ended";

    const char *result_str = "none";
    if (r->result == ROUND_RESULT_CITIZENS) result_str = "citizens";
    else if (r->result == ROUND_RESULT_ENEMIES) result_str = "enemies";

    const char *phase_str = "vote1";
    if (r->phase == ROUND_PHASE_VOTE2) phase_str = "vote2";
    else if (r->phase == ROUND_PHASE_VOTE3) phase_str = "vote3";

    char group_json[2048] = "";
    int pos = 0;
    pos += snprintf(group_json + pos, sizeof(group_json) - pos, "[");
    for (int i = 0; i < r->group_size; i++) {
        if (i > 0) pos += snprintf(group_json + pos, sizeof(group_json) - pos, ",");
        pos += snprintf(group_json + pos, sizeof(group_json) - pos, "\"%s\"", r->group[i]);
    }
    snprintf(group_json + pos, sizeof(group_json) - pos, "]");

    char votes_json[1024] = "";
    pos = 0;
    pos += snprintf(votes_json + pos, sizeof(votes_json) - pos, "[");
    for (int i = 0; i < r->vote_count; i++) {
        if (i > 0) pos += snprintf(votes_json + pos, sizeof(votes_json) - pos, ",");
        pos += snprintf(votes_json + pos, sizeof(votes_json) - pos, "%s", r->votes[i] ? "true" : "false");
    }
    snprintf(votes_json + pos, sizeof(votes_json) - pos, "]");

    snprintf(out, size,
        "{"
        "\"id\":\"%s\","
        "\"leader\":\"%s\","
        "\"status\":\"%s\","
        "\"result\":\"%s\","
        "\"phase\":\"%s\","
        "\"group\":%s,"
        "\"votes\":%s"
        "}",
        r->id, r->leader, status_str, result_str, phase_str,
        group_json, votes_json);
}

static void extract_game_id(const char *path, char *out) {
    const char *p = strstr(path, "/api/games/");
    if (!p) { out[0] = '\0'; return; }
    p += 11;
    const char *slash = strchr(p, '/');
    if (slash) {
        size_t len = slash - p;
        strncpy(out, p, len);
        out[len] = '\0';
    } else {
        strcpy(out, p);
    }
}

static void extract_round_id(const char *path, char *out) {
    const char *p = strstr(path, "/rounds/");
    if (!p) { out[0] = '\0'; return; }
    p += 8;

    const char *qmark = strchr(p, '?');
    if (qmark) {
        size_t len = qmark - p;
        strncpy(out, p, len);
        out[len] = '\0';
    } else {
        strcpy(out, p);
    }
}

static int path_has_suffix(const char *path, const char *suffix) {
    size_t plen = strlen(path);
    size_t slen = strlen(suffix);
    if (slen > plen) return 0;
    return strcmp(path + plen - slen, suffix) == 0;
}

static void extract_query_param(const char *path, const char *param, char *out, size_t out_size) {
    const char *q = strchr(path, '?');
    if (!q) { out[0] = '\0'; return; }
    q++;

    char search[64];
    snprintf(search, sizeof(search), "%s=", param);
    const char *start = strstr(q, search);
    if (!start) { out[0] = '\0'; return; }
    start += strlen(search);

    const char *amp = strchr(start, '&');
    if (amp) {
        size_t len = amp - start;
        if (len >= out_size) len = out_size - 1;
        strncpy(out, start, len);
        out[len] = '\0';
    } else {
        strncpy(out, start, out_size - 1);
        out[out_size - 1] = '\0';
    }
}

int handler_get_hello(struct lws *wsi, const char *method, const char *path, void *in, size_t len) {
	(void)method; (void)path; (void)in; (void)len;
	return send_standard_response(wsi, 200, 200, "Hello, world!", NULL);
}

int handler_create_game(struct lws *wsi, const char *method, const char *path, void *in, size_t len) {
	(void)method; (void)path;
    if (!in || len == 0)
        return send_error_response(wsi, 400, "Missing request body");

    char name_buf[256] = "";
    char owner_buf[256] = "";
    char pwd_buf[256] = "";

    json_get_string((const char *)in, "name", name_buf, sizeof(name_buf));
    json_get_string((const char *)in, "owner", owner_buf, sizeof(owner_buf));
    json_get_string((const char *)in, "password", pwd_buf, sizeof(pwd_buf));

    if (!*name_buf || !*owner_buf)
        return send_error_response(wsi, 400, "Missing name or owner");

    Game game;
    if (gamepool_create_game(name_buf, owner_buf, *pwd_buf ? pwd_buf : NULL, &game) != 0) {
        int err_http = (gamepool_get_error() == ERR_GAME_EXISTS) ? 409 : 400;
        return send_error_response(wsi, err_http, gamepool_error_string(gamepool_get_error()));
    }

    char game_json[4096];
    game_to_json(&game, game_json, sizeof(game_json));
    return send_standard_response(wsi, 200, 200, "Game created successfully", game_json);
}

int handler_search_games(struct lws *wsi, const char *method, const char *path, void *in, size_t len) {
	(void)method; (void)in; (void)len;
    char name_buf[256] = "";
    char status_buf[64] = "";
    char page_str[16] = "0";
    char limit_str[16] = "50";

    const char *q = strchr(path, '?');
    if (q) {
        q++;
        const char *name_start = strstr(q, "name=");
        if (name_start) {
            name_start += 5;
            const char *amp = strchr(name_start, '&');
            if (amp) { strncpy(name_buf, name_start, amp - name_start); }
            else { strcpy(name_buf, name_start); }
        }
        const char *status_start = strstr(q, "status=");
        if (status_start) {
            status_start += 7;
            const char *amp = strchr(status_start, '&');
            if (amp) { strncpy(status_buf, status_start, amp - status_start); }
            else { strcpy(status_buf, status_start); }
        }
        const char *page_start = strstr(q, "page=");
        if (page_start) {
            page_start += 5;
            const char *amp = strchr(page_start, '&');
            if (amp) { strncpy(page_str, page_start, amp - page_start); }
            else { strcpy(page_str, page_start); }
        }
        const char *limit_start = strstr(q, "limit=");
        if (limit_start) {
            limit_start += 6;
            const char *amp = strchr(limit_start, '&');
            if (amp) { strncpy(limit_str, limit_start, amp - limit_start); }
            else { strcpy(limit_str, limit_start); }
        }
    }

    int page = atoi(page_str);
    int limit = atoi(limit_str);

    static Game results[MAX_GAMES];
    int count = 0;
    if (gamepool_search_games(name_buf, status_buf, page, limit, results, &count) != 0)
        return send_error_response(wsi, 400, gamepool_error_string(gamepool_get_error()));

    if (count == 0)
        return send_standard_response(wsi, 200, 200, "No games found", "[]");

    static char games_json[16384];
    games_json[0] = '\0';
    int pos = 0;
    pos += snprintf(games_json + pos, sizeof(games_json) - pos, "[");
    for (int i = 0; i < count; i++) {
        if (i > 0) pos += snprintf(games_json + pos, sizeof(games_json) - pos, ",");
        char game_json[4096];
        game_to_json(&results[i], game_json, sizeof(game_json));
        pos += snprintf(games_json + pos, sizeof(games_json) - pos, "%s", game_json);
        if (pos >= (int)sizeof(games_json) - 10) break;
    }
    snprintf(games_json + pos, sizeof(games_json) - pos, "]");

    return send_standard_response(wsi, 200, 200, "Games found", games_json);
}

int handler_api_games(struct lws *wsi, const char *method, const char *path, void *in, size_t len) {
	char game_id[37] = "";
	char round_id[37] = "";

    extract_game_id(path, game_id);
    if (!*game_id)
        return send_error_response(wsi, 404, "Game not found");

    int is_rounds = path_has_suffix(path, "/rounds") || strstr(path, "/rounds/") != NULL;
    int is_start = path_has_suffix(path, "/start") || strstr(path, "/start?") != NULL;
    int is_round_detail = (strstr(path, "/rounds/") != NULL);

    if (is_round_detail)
        extract_round_id(path, round_id);

    if (is_start) {
        char player_buf[256] = "";
        char pwd_buf[256] = "";
        extract_query_param(path, "player", player_buf, sizeof(player_buf));
        extract_query_param(path, "password", pwd_buf, sizeof(pwd_buf));

        Game game;
        int ret = gamepool_start_game(game_id, player_buf, *pwd_buf ? pwd_buf : NULL, &game);
        if (ret != 0)
            return send_error_response(wsi, 409, gamepool_error_string(gamepool_get_error()));

		unsigned char buf[LWS_PRE + 256];
		unsigned char *p = buf + LWS_PRE;
		unsigned char *end = buf + sizeof(buf);

		if (lws_add_http_header_status(wsi, HTTP_STATUS_OK, &p, end))
			return 1;
		if (lws_add_http_header_by_name(wsi, (unsigned char *)"x-msg:", (unsigned char *)"Started successfuly", 19, &p, end))
			return 1;
		if (lws_add_http_header_by_name(wsi, (unsigned char *)"access-control-allow-origin:", (unsigned char *)"*", 1, &p, end))
			return 1;
		if (lws_finalize_http_header(wsi, &p, end))
			return 1;
		if (lws_write(wsi, buf + LWS_PRE, (int)(p - (buf + LWS_PRE)), LWS_WRITE_HTTP_HEADERS) < 0)
			return 1;
		if (lws_http_transaction_completed(wsi))
			return 1;
		return 0;
    }

	if (strcmp(method, "GET") == 0) {
        if (is_round_detail) {
            Round round;
            if (gamepool_show_round(game_id, round_id, &round) != 0)
                return send_error_response(wsi, 404, gamepool_error_string(gamepool_get_error()));
            char round_json[4096];
            round_to_json(&round, round_json, sizeof(round_json));
            return send_standard_response(wsi, 200, 200, "Results found", round_json);
        }

        if (is_rounds) {
            Round rounds[MAX_ROUNDS_PER_GAME];
            int count = 0;
            if (gamepool_get_rounds(game_id, rounds, &count) != 0)
                return send_error_response(wsi, 404, gamepool_error_string(gamepool_get_error()));

            char rounds_json[16384] = "";
            int pos = 0;
            pos += snprintf(rounds_json + pos, sizeof(rounds_json) - pos, "[");
            for (int i = 0; i < count; i++) {
                if (i > 0) pos += snprintf(rounds_json + pos, sizeof(rounds_json) - pos, ",");
                char rj[4096];
                round_to_json(&rounds[i], rj, sizeof(rj));
                pos += snprintf(rounds_json + pos, sizeof(rounds_json) - pos, "%s", rj);
            }
            snprintf(rounds_json + pos, sizeof(rounds_json) - pos, "]");
            return send_standard_response(wsi, 200, 200, "Results found", rounds_json);
        }

        Game game;
        if (gamepool_get_game(game_id, &game) != 0)
            return send_standard_response(wsi, 200, 200, "Game Not Found", NULL);

        char game_json[4096];
        game_to_json(&game, game_json, sizeof(game_json));
        return send_standard_response(wsi, 200, 200, "Game Found", game_json);
    }

	if (strcmp(method, "PUT") == 0) {
        if (is_round_detail) {
            if (!in || len == 0)
                return send_error_response(wsi, 400, "Missing request body");

            int action = 0;
            if (json_get_bool((const char *)in, "action", &action) != 0)
                return send_error_response(wsi, 400, "Invalid or missing action");

            char player_buf[256] = "";
            json_get_string((const char *)in, "player", player_buf, sizeof(player_buf));

            if (!*player_buf)
                return send_error_response(wsi, 400, "Missing player");

            Round round;
            int ret = gamepool_submit_action(game_id, round_id, player_buf, action, &round);
            if (ret != 0) {
                int err = gamepool_get_error();
                int http_st = (err == ERR_NOT_IN_GROUP) ? 403 : (err == ERR_NOT_ACTION_PHASE) ? 428 : 400;
                return send_error_response(wsi, http_st, gamepool_error_string(err));
            }

            char round_json[4096];
            round_to_json(&round, round_json, sizeof(round_json));
            return send_standard_response(wsi, 200, 200, "Action registered", round_json);
        }

        if (!in || len == 0)
            return send_error_response(wsi, 400, "Missing request body");

        char player_buf[256] = "";
        char pwd_buf[256] = "";
        json_get_string((const char *)in, "player", player_buf, sizeof(player_buf));
        json_get_string((const char *)in, "password", pwd_buf, sizeof(pwd_buf));

        if (!*player_buf)
            return send_error_response(wsi, 400, "Missing player");

        Game game;
        int ret = gamepool_join_game(game_id, player_buf, *pwd_buf ? pwd_buf : NULL, &game);
        if (ret != 0) {
            int err = gamepool_get_error();
            int http_st = (err == ERR_WRONG_PASSWORD || err == ERR_GAME_NOT_FOUND || err == ERR_PLAYER_EXISTS) ? 201 : 400;
            return send_standard_response(wsi, http_st, 400, gamepool_error_string(err), NULL);
        }

        char game_json[4096];
        game_to_json(&game, game_json, sizeof(game_json));
        return send_standard_response(wsi, 201, 201, "Game joined successfully", game_json);
    }

	if (strcmp(method, "PATCH") == 0) {
        if (!in || len == 0)
            return send_error_response(wsi, 400, "Missing request body");

        if (!*round_id)
            return send_error_response(wsi, 404, "Round not found");

        char player_buf[256] = "";
        json_get_string((const char *)in, "player", player_buf, sizeof(player_buf));

        if (!*player_buf)
            return send_error_response(wsi, 400, "Missing player");

        char *group_array[MAX_PLAYERS] = {0};
        char group_bufs[MAX_PLAYERS][64] = {{0}};
        for (int i = 0; i < MAX_PLAYERS; i++)
            group_array[i] = group_bufs[i];

        int group_size = json_get_string_array((const char *)in, "group", group_array, MAX_PLAYERS);
        if (group_size <= 0)
            return send_error_response(wsi, 400, "Invalid or missing group");

        const char *group_ptr[MAX_PLAYERS];
        for (int i = 0; i < group_size; i++)
            group_ptr[i] = group_array[i];

        Round round;
        int ret = gamepool_propose_group(game_id, round_id, player_buf, group_ptr, group_size, &round);
        if (ret != 0) {
            int err = gamepool_get_error();
            int http_st = (err == ERR_NOT_LEADER || err == ERR_ROUND_NOT_FOUND || err == ERR_GAME_NOT_FOUND) ? 404 :
                          (err == ERR_NOT_VOTING_PHASE) ? 428 : 400;
            return send_error_response(wsi, http_st, gamepool_error_string(err));
        }

        char round_json[4096];
        round_to_json(&round, round_json, sizeof(round_json));
        return send_standard_response(wsi, 200, 200, "Group Created", round_json);
    }

	if (strcmp(method, "POST") == 0) {
        if (!in || len == 0)
            return send_error_response(wsi, 400, "Missing request body");

        if (!*round_id)
            return send_error_response(wsi, 404, "Round not found");

        char player_buf[256] = "";
        json_get_string((const char *)in, "player", player_buf, sizeof(player_buf));

        int vote = 0;
        if (json_get_bool((const char *)in, "vote", &vote) != 0)
            return send_error_response(wsi, 400, "Invalid or missing vote");

        Round round;
        int ret = gamepool_vote_group(game_id, round_id, player_buf, vote, &round);
        if (ret != 0) {
            int err = gamepool_get_error();
            int http_st = (err == ERR_ALREADY_VOTED) ? 409 :
                          (err == ERR_NOT_VOTING_PHASE) ? 428 :
                          (err == ERR_PLAYER_NOT_FOUND) ? 400 : 404;
            return send_error_response(wsi, http_st, gamepool_error_string(err));
        }

        char round_json[4096];
        round_to_json(&round, round_json, sizeof(round_json));
        return send_standard_response(wsi, 200, 200, "Vote registered", round_json);
    }

    return send_error_response(wsi, 405, "Method not allowed");
}