#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <cmocka.h>
#include "gamepool.h"
#include "models.h"

static int setup(void **state) {
    (void)state;
    gamepool_reset();
    return 0;
}

static int add_players_to_game(const char *game_id, int count) {
    for (int i = 0; i < count; i++) {
        char pname[16];
        snprintf(pname, sizeof(pname), "Player%d", i + 1);
        if (gamepool_join_game(game_id, pname, NULL, NULL) != 0)
            return -1;
    }
    return 0;
}

static void test_create_game(void **state) {
    (void)state;
    Game g;
    int ret = gamepool_create_game("TestGame", "Alice", "secret", &g);
    assert_int_equal(ret, 0);
    assert_string_equal(g.name, "TestGame");
    assert_string_equal(g.owner, "Alice");
    assert_int_equal(g.status, GAME_STATUS_LOBBY);
    assert_int_equal(g.player_count, 1);
    assert_string_equal(g.players[0], "Alice");
    assert_int_equal(g.has_password, 1);
    assert_int_equal(strlen(g.id), 36);
}

static void test_create_game_duplicate_name(void **state) {
    (void)state;
    Game g;
    gamepool_create_game("DupTest", "Alice", NULL, &g);
    int ret = gamepool_create_game("DupTest", "Bob", NULL, &g);
    assert_int_not_equal(ret, 0);
    assert_int_equal(gamepool_get_error(), ERR_GAME_EXISTS);
}

static void test_create_game_missing_data(void **state) {
    (void)state;
    Game g;
    assert_int_not_equal(gamepool_create_game("", "Bob", NULL, &g), 0);
    assert_int_equal(gamepool_get_error(), ERR_MISSING_DATA);
    assert_int_not_equal(gamepool_create_game("Game", "", NULL, &g), 0);
}

static void test_create_game_no_password(void **state) {
    (void)state;
    Game g;
    int ret = gamepool_create_game("NoPassGame", "Charlie", NULL, &g);
    assert_int_equal(ret, 0);
    assert_int_equal(g.has_password, 0);
}

static void test_join_game(void **state) {
    (void)state;
    Game g;
    gamepool_create_game("JoinTest", "Alice", "pwd", &g);

    Game updated;
    int ret = gamepool_join_game(g.id, "Bob", "pwd", &updated);
    assert_int_equal(ret, 0);
    assert_int_equal(updated.player_count, 2);
    assert_string_equal(updated.players[1], "Bob");
}

static void test_join_game_wrong_password(void **state) {
    (void)state;
    Game g;
    gamepool_create_game("JoinFail", "Alice", "pwd", &g);

    Game updated;
    int ret = gamepool_join_game(g.id, "Bob", "wrong", &updated);
    assert_int_not_equal(ret, 0);
    assert_int_equal(gamepool_get_error(), ERR_WRONG_PASSWORD);
}

static void test_join_game_duplicate_player(void **state) {
    (void)state;
    Game g;
    gamepool_create_game("JoinDup", "Alice", NULL, &g);

    Game updated;
    int ret = gamepool_join_game(g.id, "Alice", NULL, &updated);
    assert_int_not_equal(ret, 0);
    assert_int_equal(gamepool_get_error(), ERR_PLAYER_EXISTS);
}

static void test_join_game_not_found(void **state) {
    (void)state;
    Game updated;
    int ret = gamepool_join_game("nonexistent-id", "Bob", NULL, &updated);
    assert_int_not_equal(ret, 0);
    assert_int_equal(gamepool_get_error(), ERR_GAME_NOT_FOUND);
}

static void test_search_games(void **state) {
    (void)state;
    gamepool_create_game("SearchA", "Alice", NULL, NULL);
    gamepool_create_game("SearchB", "Bob", NULL, NULL);
    gamepool_create_game("Other", "Charlie", NULL, NULL);

    Game results[MAX_GAMES];
    int count = 0;

    int ret = gamepool_search_games("Search", NULL, 0, 50, results, &count);
    assert_int_equal(ret, 0);
    assert_int_equal(count, 2);
}

static void test_search_games_all(void **state) {
    (void)state;
    gamepool_create_game("Alpha", "Alice", NULL, NULL);
    gamepool_create_game("Beta", "Bob", NULL, NULL);
    gamepool_create_game("Gamma", "Charlie", NULL, NULL);

    Game results[MAX_GAMES];
    int count = 0;

    int ret = gamepool_search_games(NULL, NULL, 0, 50, results, &count);
    assert_int_equal(ret, 0);
    assert_int_equal(count, 3);
}

static void test_get_game(void **state) {
    (void)state;
    Game created;
    gamepool_create_game("GetTest", "Alice", NULL, &created);

    Game found;
    int ret = gamepool_get_game(created.id, &found);
    assert_int_equal(ret, 0);
    assert_string_equal(found.name, "GetTest");
}

static void test_get_game_not_found(void **state) {
    (void)state;
    Game found;
    int ret = gamepool_get_game("nonexistent", &found);
    assert_int_not_equal(ret, 0);
}

static void test_start_game_too_few_players(void **state) {
    (void)state;
    Game g;
    gamepool_create_game("SmallGame", "Alice", NULL, &g);

    Game started;
    int ret = gamepool_start_game(g.id, "Alice", NULL, &started);
    assert_int_not_equal(ret, 0);
    assert_int_equal(gamepool_get_error(), ERR_MIN_PLAYERS);
}

static void test_start_game_not_owner(void **state) {
    (void)state;
    Game g;
    gamepool_create_game("OwnerTest", "Alice", NULL, &g);
    gamepool_join_game(g.id, "Bob", NULL, NULL);
    gamepool_join_game(g.id, "Charlie", NULL, NULL);
    gamepool_join_game(g.id, "Dave", NULL, NULL);
    gamepool_join_game(g.id, "Eve", NULL, NULL);

    Game started;
    int ret = gamepool_start_game(g.id, "Bob", NULL, &started);
    assert_int_not_equal(ret, 0);
    assert_int_equal(gamepool_get_error(), ERR_NOT_OWNER);
}

static void test_start_game_success(void **state) {
    (void)state;
    Game g;
    gamepool_create_game("FullGame", "Alice", NULL, &g);
    assert_int_equal(add_players_to_game(g.id, 5), 0);

    Game started;
    int ret = gamepool_start_game(g.id, "Alice", NULL, &started);
    assert_int_equal(ret, 0);
    assert_int_equal(started.status, GAME_STATUS_ROUNDS);
    assert_int_equal(started.enemy_count, 2);
    assert_int_equal(strlen(started.current_round), 36);
}

static void test_get_rounds_after_start(void **state) {
    (void)state;
    Game g;
    gamepool_create_game("RoundsTest", "Alice", NULL, &g);
    add_players_to_game(g.id, 5);
    gamepool_start_game(g.id, "Alice", NULL, NULL);

    Round rounds[MAX_ROUNDS_PER_GAME];
    int count = 0;
    int ret = gamepool_get_rounds(g.id, rounds, &count);
    assert_int_equal(ret, 0);
    assert_int_equal(count, 1);
    assert_int_equal(rounds[0].status, ROUND_STATUS_WAITING_ON_LEADER);
    assert_true(strlen(rounds[0].leader) > 0);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_create_game, setup),
        cmocka_unit_test_setup(test_create_game_duplicate_name, setup),
        cmocka_unit_test_setup(test_create_game_missing_data, setup),
        cmocka_unit_test_setup(test_create_game_no_password, setup),
        cmocka_unit_test_setup(test_join_game, setup),
        cmocka_unit_test_setup(test_join_game_wrong_password, setup),
        cmocka_unit_test_setup(test_join_game_duplicate_player, setup),
        cmocka_unit_test_setup(test_join_game_not_found, setup),
        cmocka_unit_test_setup(test_search_games, setup),
        cmocka_unit_test_setup(test_search_games_all, setup),
        cmocka_unit_test_setup(test_get_game, setup),
        cmocka_unit_test_setup(test_get_game_not_found, setup),
        cmocka_unit_test_setup(test_start_game_too_few_players, setup),
        cmocka_unit_test_setup(test_start_game_not_owner, setup),
        cmocka_unit_test_setup(test_start_game_success, setup),
        cmocka_unit_test_setup(test_get_rounds_after_start, setup),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}