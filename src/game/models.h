#ifndef MODELS_H
#define MODELS_H

#define MAX_PLAYERS 20
#define MAX_ROUNDS_PER_GAME 10
#define MAX_GAMES 100
#define MAX_TOTAL_ROUNDS 500
#define MAX_NAME_LEN 128
#define MAX_UUID_STR 37

typedef enum {
    GAME_STATUS_LOBBY,
    GAME_STATUS_ROUNDS,
    GAME_STATUS_ENDED
} GameStatus;

typedef enum {
    ROUND_STATUS_WAITING_ON_LEADER,
    ROUND_STATUS_VOTING,
    ROUND_STATUS_WAITING_ON_GROUP,
    ROUND_STATUS_ENDED
} RoundStatus;

typedef enum {
    ROUND_RESULT_NONE,
    ROUND_RESULT_CITIZENS,
    ROUND_RESULT_ENEMIES
} RoundResult;

typedef enum {
    ROUND_PHASE_VOTE1,
    ROUND_PHASE_VOTE2,
    ROUND_PHASE_VOTE3
} RoundPhase;

typedef struct {
    char id[MAX_UUID_STR];
    char name[MAX_NAME_LEN];
    char owner[64];
    char password[128];
    int  has_password;
    GameStatus status;
    char created_at[32];
    char updated_at[32];
    char players[MAX_PLAYERS][64];
    int  player_count;
    char enemies[MAX_PLAYERS][64];
    int  enemy_count;
    char current_round[MAX_UUID_STR];
} Game;

typedef struct {
    char id[MAX_UUID_STR];
    char game_id[MAX_UUID_STR];
    char leader[64];
    RoundStatus status;
    RoundResult result;
    RoundPhase phase;
    char group[MAX_PLAYERS][64];
    int  group_size;
    int  votes[MAX_PLAYERS];
    int  vote_count;
    int  actions[MAX_PLAYERS];
    int  action_count;
    char already_vote[MAX_PLAYERS][64];
    int  already_vote_count;
} Round;

#endif