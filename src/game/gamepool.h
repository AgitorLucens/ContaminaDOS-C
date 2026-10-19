#ifndef GAMEPOOL_H
#define GAMEPOOL_H

#include "models.h"

typedef struct {
    Game games[MAX_GAMES];
    int  game_count;
    Round rounds[MAX_TOTAL_ROUNDS];
    int  round_count;
} GamePool;

GamePool *gamepool_get_instance(void);

int  gamepool_create_game(const char *name, const char *owner, const char *password, Game *out);
int  gamepool_search_games(const char *name, const char *status_str, int page, int limit, Game *out, int *out_count);
int  gamepool_get_game(const char *id, Game *out);
int  gamepool_join_game(const char *game_id, const char *player, const char *password, Game *out);
int  gamepool_start_game(const char *game_id, const char *player, const char *password, Game *out);
int  gamepool_get_rounds(const char *game_id, Round *out, int *out_count);
int  gamepool_show_round(const char *game_id, const char *round_id, Round *out);
int  gamepool_propose_group(const char *game_id, const char *round_id, const char *player, const char **group, int group_size, Round *out);
int  gamepool_vote_group(const char *game_id, const char *round_id, const char *player, int vote, Round *out);
int  gamepool_submit_action(const char *game_id, const char *round_id, const char *player, int action, Round *out);

void gamepool_reset(void);
int  gamepool_get_error(void);
const char *gamepool_error_string(int err);

enum {
    ERR_NONE = 0,
    ERR_MISSING_DATA,
    ERR_GAME_EXISTS,
    ERR_GAME_NOT_FOUND,
    ERR_WRONG_PASSWORD,
    ERR_PLAYER_EXISTS,
    ERR_PLAYER_NOT_FOUND,
    ERR_GAME_NOT_LOBBY,
    ERR_MIN_PLAYERS,
    ERR_NOT_OWNER,
    ERR_ROUND_NOT_FOUND,
    ERR_NOT_LEADER,
    ERR_NOT_VOTING_PHASE,
    ERR_ALREADY_VOTED,
    ERR_INVALID_GROUP,
    ERR_MEMBER_NOT_IN_GAME,
    ERR_INVALID_GROUP_SIZE,
    ERR_NOT_ACTION_PHASE,
    ERR_NOT_IN_GROUP,
    ERR_GAME_NOT_ROUNDS,
};

#endif