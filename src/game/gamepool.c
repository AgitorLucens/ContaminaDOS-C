#include "gamepool.h"
#include "uuid.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static GamePool pool;
static int pool_initialized = 0;
static int last_error = ERR_NONE;

void gamepool_reset(void) {
    memset(&pool, 0, sizeof(pool));
    pool_initialized = 1;
    last_error = ERR_NONE;
}

static GamePool *get_pool(void) {
    if (!pool_initialized) {
        memset(&pool, 0, sizeof(pool));
        pool_initialized = 1;
    }
    return &pool;
}

int gamepool_get_error(void) { return last_error; }

static void set_error(int err) { last_error = err; }

static void timestamp_now(char *buf, size_t size) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_s(&tm, &t);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", &tm);
}

static int game_index_by_name(GamePool *p, const char *name) {
    for (int i = 0; i < p->game_count; i++)
        if (strcmp(p->games[i].name, name) == 0)
            return i;
    return -1;
}

static int game_index_by_id(GamePool *p, const char *id) {
    for (int i = 0; i < p->game_count; i++)
        if (strcmp(p->games[i].id, id) == 0)
            return i;
    return -1;
}

static int round_index_by_id(GamePool *p, const char *id) {
    for (int i = 0; i < p->round_count; i++)
        if (strcmp(p->rounds[i].id, id) == 0)
            return i;
    return -1;
}

static int round_index_by_game_and_id(GamePool *p, const char *game_id, const char *round_id) {
    for (int i = 0; i < p->round_count; i++)
        if (strcmp(p->rounds[i].game_id, game_id) == 0 && strcmp(p->rounds[i].id, round_id) == 0)
            return i;
    return -1;
}

static int player_exists(Game *g, const char *player) {
    for (int i = 0; i < g->player_count; i++)
        if (strcmp(g->players[i], player) == 0)
            return 1;
    return 0;
}

static int get_enemies_count(int player_count) {
    if (player_count >= 10) return 4;
    if (player_count >= 7)  return 3;
    if (player_count >= 5)  return 2;
    return 0;
}

static int get_expected_group_size(int player_count, int round_count) {
    static const int table[6][5] = {
        {2, 3, 2, 3, 3},
        {2, 3, 4, 3, 4},
        {2, 3, 3, 4, 4},
        {3, 4, 4, 5, 5},
        {3, 4, 4, 5, 5},
        {3, 4, 4, 5, 5},
    };
    int idx = player_count - 5;
    if (idx < 0 || idx > 5 || round_count < 1 || round_count > 5)
        return -1;
    return table[idx][round_count - 1];
}

static int get_random_leader(const char **players, int count) {
    return rand() % count;
}

static int count_rounds_result(GamePool *p, const char *game_id, RoundResult result) {
    int count = 0;
    for (int i = 0; i < p->round_count; i++)
        if (strcmp(p->rounds[i].game_id, game_id) == 0 && p->rounds[i].result == result)
            count++;
    return count;
}

static int verify_game_winner(GamePool *p, const char *game_id, Game *g) {
    int citizens = count_rounds_result(p, game_id, ROUND_RESULT_CITIZENS);
    int enemies  = count_rounds_result(p, game_id, ROUND_RESULT_ENEMIES);
    if (citizens >= 3 || enemies >= 3) {
        g->status = GAME_STATUS_ENDED;
        char ts[32];
        timestamp_now(ts, sizeof(ts));
        strcpy(g->updated_at, ts);
        return 1;
    }
    return 0;
}

static Round *create_new_round(GamePool *p, Game *g) {
    if (p->round_count >= MAX_TOTAL_ROUNDS) return NULL;
    Round *r = &p->rounds[p->round_count++];
    memset(r, 0, sizeof(Round));
    uuid_generate(r->id);
    strcpy(r->game_id, g->id);
    r->status = ROUND_STATUS_WAITING_ON_LEADER;
    r->result = ROUND_RESULT_NONE;
    r->phase = ROUND_PHASE_VOTE1;

    const char **player_names = malloc(g->player_count * sizeof(char *));
    for (int i = 0; i < g->player_count; i++)
        player_names[i] = g->players[i];
    int leader_idx = get_random_leader(player_names, g->player_count);
    strcpy(r->leader, player_names[leader_idx]);
    free(player_names);

    strcpy(g->current_round, r->id);
    char ts[32];
    timestamp_now(ts, sizeof(ts));
    strcpy(g->updated_at, ts);
    return r;
}

int gamepool_create_game(const char *name, const char *owner, const char *password, Game *out) {
    GamePool *p = get_pool();
    if (!name || !*name || !owner || !*owner) { set_error(ERR_MISSING_DATA); return -1; }
    if (game_index_by_name(p, name) >= 0) { set_error(ERR_GAME_EXISTS); return -1; }
    if (p->game_count >= MAX_GAMES) { set_error(ERR_MISSING_DATA); return -1; }

    Game *g = &p->games[p->game_count++];
    memset(g, 0, sizeof(Game));
    uuid_generate(g->id);
    strncpy(g->name, name, MAX_NAME_LEN - 1);
    strncpy(g->owner, owner, sizeof(g->owner) - 1);
    if (password && *password) {
        strncpy(g->password, password, sizeof(g->password) - 1);
        g->has_password = 1;
    }
    g->status = GAME_STATUS_LOBBY;
    timestamp_now(g->created_at, sizeof(g->created_at));
    memset(g->current_round, '0', MAX_UUID_STR - 1);
    g->current_round[MAX_UUID_STR - 1] = '\0';
    strncpy(g->players[g->player_count++], owner, sizeof(g->players[0]) - 1);

    if (out) *out = *g;
    set_error(ERR_NONE);
    return 0;
}

int gamepool_search_games(const char *name, const char *status_str, int page, int limit, Game *out, int *out_count) {
    GamePool *p = get_pool();
    if (limit <= 0) { set_error(ERR_MISSING_DATA); return -1; }

    static Game temp[MAX_GAMES];
    int temp_count = 0;

    for (int i = 0; i < p->game_count; i++) {
        Game *g = &p->games[i];
        if (name && *name && strstr(g->name, name) == NULL)
            continue;
        if (status_str && *status_str) {
            if (strcmp(status_str, "lobby") == 0 && g->status != GAME_STATUS_LOBBY) continue;
            if (strcmp(status_str, "rounds") == 0 && g->status != GAME_STATUS_ROUNDS) continue;
            if (strcmp(status_str, "ended") == 0 && g->status != GAME_STATUS_ENDED) continue;
        }
        temp[temp_count++] = *g;
    }

    int start = page * limit;
    int end = start + limit;
    if (end > temp_count) end = temp_count;
    if (start >= temp_count) { *out_count = 0; set_error(ERR_NONE); return 0; }

    int idx = 0;
    for (int i = start; i < end; i++)
        out[idx++] = temp[i];
    *out_count = idx;
    set_error(ERR_NONE);
    return 0;
}

int gamepool_get_game(const char *id, Game *out) {
    GamePool *p = get_pool();
    int idx = game_index_by_id(p, id);
    if (idx < 0) { set_error(ERR_GAME_NOT_FOUND); return -1; }
    if (out) *out = p->games[idx];
    set_error(ERR_NONE);
    return 0;
}

int gamepool_join_game(const char *game_id, const char *player, const char *password, Game *out) {
    GamePool *p = get_pool();
    if (!game_id || !*game_id || !player || !*player) { set_error(ERR_MISSING_DATA); return -1; }

    int idx = game_index_by_id(p, game_id);
    if (idx < 0) { set_error(ERR_GAME_NOT_FOUND); return -1; }

    Game *g = &p->games[idx];
    if (g->status != GAME_STATUS_LOBBY) { set_error(ERR_GAME_NOT_LOBBY); return -1; }
    if (g->has_password) {
        if (!password || strcmp(g->password, password) != 0) { set_error(ERR_WRONG_PASSWORD); return -1; }
    }
    if (player_exists(g, player)) { set_error(ERR_PLAYER_EXISTS); return -1; }
    if (g->player_count >= MAX_PLAYERS) { set_error(ERR_MISSING_DATA); return -1; }

    strncpy(g->players[g->player_count++], player, sizeof(g->players[0]) - 1);
    char ts[32];
    timestamp_now(ts, sizeof(ts));
    strcpy(g->updated_at, ts);

    if (out) *out = *g;
    set_error(ERR_NONE);
    return 0;
}

int gamepool_start_game(const char *game_id, const char *player, const char *password, Game *out) {
    GamePool *p = get_pool();
    if (!game_id || !*game_id) { set_error(ERR_MISSING_DATA); return -1; }

    int idx = game_index_by_id(p, game_id);
    if (idx < 0) { set_error(ERR_GAME_NOT_FOUND); return -1; }

    Game *g = &p->games[idx];
    if (g->status != GAME_STATUS_LOBBY) { set_error(ERR_GAME_NOT_LOBBY); return -1; }
    if (g->has_password && (!password || strcmp(g->password, password) != 0)) { set_error(ERR_WRONG_PASSWORD); return -1; }
    if (strcmp(g->owner, player) != 0) { set_error(ERR_NOT_OWNER); return -1; }
    if (g->player_count < 5) { set_error(ERR_MIN_PLAYERS); return -1; }

    int enemy_count = get_enemies_count(g->player_count);
    char *player_copy[MAX_PLAYERS];
    for (int i = 0; i < g->player_count; i++)
        player_copy[i] = g->players[i];

    for (int i = g->player_count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        char *tmp = player_copy[i];
        player_copy[i] = player_copy[j];
        player_copy[j] = tmp;
    }

    g->enemy_count = enemy_count;
    for (int i = 0; i < enemy_count; i++)
        strncpy(g->enemies[i], player_copy[i], sizeof(g->enemies[0]) - 1);

    g->status = GAME_STATUS_ROUNDS;

    Round *r = create_new_round(p, g);
    if (!r) { set_error(ERR_MISSING_DATA); return -1; }

    if (out) *out = *g;
    set_error(ERR_NONE);
    return 0;
}

int gamepool_get_rounds(const char *game_id, Round *out, int *out_count) {
    GamePool *p = get_pool();
    if (game_index_by_id(p, game_id) < 0) { set_error(ERR_GAME_NOT_FOUND); return -1; }

    int count = 0;
    for (int i = 0; i < p->round_count; i++) {
        if (strcmp(p->rounds[i].game_id, game_id) == 0) {
            if (out) out[count] = p->rounds[i];
            count++;
        }
    }
    if (out_count) *out_count = count;
    set_error(ERR_NONE);
    return 0;
}

int gamepool_show_round(const char *game_id, const char *round_id, Round *out) {
    GamePool *p = get_pool();
    int idx = round_index_by_game_and_id(p, game_id, round_id);
    if (idx < 0) { set_error(ERR_ROUND_NOT_FOUND); return -1; }
    if (out) *out = p->rounds[idx];
    set_error(ERR_NONE);
    return 0;
}

int gamepool_propose_group(const char *game_id, const char *round_id, const char *player, const char **group, int group_size, Round *out) {
    GamePool *p = get_pool();
    int gidx = game_index_by_id(p, game_id);
    if (gidx < 0) { set_error(ERR_GAME_NOT_FOUND); return -1; }

    Game *g = &p->games[gidx];
    int ridx = round_index_by_game_and_id(p, game_id, round_id);
    if (ridx < 0) { set_error(ERR_ROUND_NOT_FOUND); return -1; }

    Round *r = &p->rounds[ridx];
    if (r->status != ROUND_STATUS_WAITING_ON_LEADER) { set_error(ERR_NOT_VOTING_PHASE); return -1; }
    if (strcmp(r->leader, player) != 0) { set_error(ERR_NOT_LEADER); return -1; }
    if (!group || group_size <= 0) { set_error(ERR_INVALID_GROUP); return -1; }

    for (int i = 0; i < group_size; i++) {
        if (!player_exists(g, group[i])) { set_error(ERR_MEMBER_NOT_IN_GAME); return -1; }
    }

    int rounds_sofar = 0;
    for (int i = 0; i < p->round_count; i++)
        if (strcmp(p->rounds[i].game_id, game_id) == 0) rounds_sofar++;

    int expected = get_expected_group_size(g->player_count, rounds_sofar);
    if (expected > 0 && group_size != expected) { set_error(ERR_INVALID_GROUP_SIZE); return -1; }

    r->group_size = group_size;
    for (int i = 0; i < group_size; i++)
        strncpy(r->group[i], group[i], sizeof(r->group[0]) - 1);
    r->status = ROUND_STATUS_VOTING;

    if (out) *out = *r;
    set_error(ERR_NONE);
    return 0;
}

int gamepool_vote_group(const char *game_id, const char *round_id, const char *player, int vote, Round *out) {
    GamePool *p = get_pool();
    int gidx = game_index_by_id(p, game_id);
    if (gidx < 0) { set_error(ERR_GAME_NOT_FOUND); return -1; }

    Game *g = &p->games[gidx];
    if (!player_exists(g, player)) { set_error(ERR_PLAYER_NOT_FOUND); return -1; }

    int ridx = round_index_by_game_and_id(p, game_id, round_id);
    if (ridx < 0) { set_error(ERR_ROUND_NOT_FOUND); return -1; }

    Round *r = &p->rounds[ridx];
    if (r->status != ROUND_STATUS_VOTING) { set_error(ERR_NOT_VOTING_PHASE); return -1; }

    for (int i = 0; i < r->already_vote_count; i++)
        if (strcmp(r->already_vote[i], player) == 0) { set_error(ERR_ALREADY_VOTED); return -1; }

    strncpy(r->already_vote[r->already_vote_count], player, sizeof(r->already_vote[0]) - 1);
    r->already_vote_count++;
    r->votes[r->vote_count++] = vote;

    if (r->vote_count == g->player_count) {
        int true_count = 0, false_count = 0;
        for (int i = 0; i < r->vote_count; i++) {
            if (r->votes[i]) true_count++;
            else false_count++;
        }

        if (true_count > false_count) {
            r->status = ROUND_STATUS_WAITING_ON_GROUP;
        } else if (false_count > true_count) {
            if (r->phase == ROUND_PHASE_VOTE3) {
                r->result = ROUND_RESULT_ENEMIES;
                r->status = ROUND_STATUS_ENDED;
                if (!verify_game_winner(p, game_id, g)) {
                    Round *nr = create_new_round(p, g);
                    (void)nr;
                }
            } else {
                r->group_size = 0;
                r->vote_count = 0;
                r->already_vote_count = 0;
                r->phase++;
                r->status = ROUND_STATUS_WAITING_ON_LEADER;
            }
        }
    }

    if (out) *out = *r;
    set_error(ERR_NONE);
    return 0;
}

int gamepool_submit_action(const char *game_id, const char *round_id, const char *player, int action, Round *out) {
    GamePool *p = get_pool();
    int gidx = game_index_by_id(p, game_id);
    if (gidx < 0) { set_error(ERR_GAME_NOT_FOUND); return -1; }

    Game *g = &p->games[gidx];
    if (!player_exists(g, player)) { set_error(ERR_PLAYER_NOT_FOUND); return -1; }

    int ridx = round_index_by_game_and_id(p, game_id, round_id);
    if (ridx < 0) { set_error(ERR_ROUND_NOT_FOUND); return -1; }

    Round *r = &p->rounds[ridx];
    if (r->status != ROUND_STATUS_WAITING_ON_GROUP) { set_error(ERR_NOT_ACTION_PHASE); return -1; }

    int in_group = 0;
    for (int i = 0; i < r->group_size; i++)
        if (strcmp(r->group[i], player) == 0) { in_group = 1; break; }
    if (!in_group) { set_error(ERR_NOT_IN_GROUP); return -1; }

    r->actions[r->action_count++] = action;

    if (r->action_count == r->group_size) {
        int has_sabotage = 0;
        for (int i = 0; i < r->action_count; i++)
            if (!r->actions[i]) { has_sabotage = 1; break; }

        r->result = has_sabotage ? ROUND_RESULT_ENEMIES : ROUND_RESULT_CITIZENS;
        r->status = ROUND_STATUS_ENDED;

        if (!verify_game_winner(p, game_id, g)) {
            Round *nr = create_new_round(p, g);
            (void)nr;
        }
    }

    if (out) *out = *r;
    set_error(ERR_NONE);
    return 0;
}

const char *gamepool_error_string(int err) {
    switch (err) {
        case ERR_NONE:               return "";
        case ERR_MISSING_DATA:       return "Missing data";
        case ERR_GAME_EXISTS:        return "Game already exists";
        case ERR_GAME_NOT_FOUND:     return "Game not found";
        case ERR_WRONG_PASSWORD:     return "Wrong password";
        case ERR_PLAYER_EXISTS:      return "Player already exists";
        case ERR_PLAYER_NOT_FOUND:   return "Player not found";
        case ERR_GAME_NOT_LOBBY:     return "Game is not in lobby state";
        case ERR_MIN_PLAYERS:        return "Need at least 5 players";
        case ERR_NOT_OWNER:          return "You are not the owner";
        case ERR_ROUND_NOT_FOUND:    return "Round not found";
        case ERR_NOT_LEADER:         return "You are not the leader";
        case ERR_NOT_VOTING_PHASE:   return "It is not the time for this action";
        case ERR_ALREADY_VOTED:      return "You have already voted";
        case ERR_INVALID_GROUP:      return "Invalid or missing group";
        case ERR_MEMBER_NOT_IN_GAME: return "Group member is not part of the game";
        case ERR_INVALID_GROUP_SIZE: return "Invalid group size";
        case ERR_NOT_ACTION_PHASE:   return "It is not the time for actions";
        case ERR_NOT_IN_GROUP:       return "You cannot contribute in this round";
        case ERR_GAME_NOT_ROUNDS:    return "Game is not in rounds state";
        default:                     return "Unknown error";
    }
}