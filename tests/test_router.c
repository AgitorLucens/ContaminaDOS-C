#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include "router.h"

int __wrap_lws_return_http_status(struct lws *wsi, unsigned int code,
                                   const char *html_body) {
    check_expected(wsi);
    check_expected(code);
    check_expected_ptr(html_body);
    return (int)mock();
}

static void test_router_get_count(void **state) {
    (void)state;
    assert_int_equal(router_get_count(), 13);
}

static void test_router_get_route_users_get(void **state) {
    (void)state;
    const Route *r = router_get_route(0);
    assert_non_null(r);
    assert_string_equal(r->method, "GET");
    assert_string_equal(r->path, "/api/users");
    assert_null(r->prefix);
    assert_non_null(r->handler);
}

static void test_router_get_route_hello(void **state) {
    (void)state;
    const Route *r = router_get_route(3);
    assert_non_null(r);
    assert_string_equal(r->method, "GET");
    assert_string_equal(r->path, "/hello");
    assert_non_null(r->handler);
}

static void test_router_get_route_create_game(void **state) {
    (void)state;
    const Route *r = router_get_route(4);
    assert_non_null(r);
    assert_string_equal(r->method, "POST");
    assert_string_equal(r->path, "/api/games");
    assert_non_null(r->handler);
}

static void test_router_get_route_search(void **state) {
    (void)state;
    const Route *r = router_get_route(6);
    assert_non_null(r);
    assert_string_equal(r->method, "GET");
    assert_string_equal(r->path, "/api/games");
    assert_non_null(r->handler);
}

static void test_router_get_route_prefix_match(void **state) {
    (void)state;
    const Route *r = router_get_route(8);
    assert_non_null(r);
    assert_string_equal(r->method, "GET");
    assert_null(r->path);
    assert_non_null(r->prefix);
    assert_string_equal(r->prefix, "/api/games/");
    assert_non_null(r->handler);
}

static void test_router_get_route_out_of_bounds(void **state) {
    (void)state;
    assert_null(router_get_route(-1));
    assert_null(router_get_route(13));
}

static void test_router_match_exact(void **state) {
    (void)state;
    assert_int_equal(router_match("GET", "/api/users"), 0);
    assert_int_equal(router_match("GET", "/hello"), 3);
    assert_int_equal(router_match("POST", "/api/games"), 4);
}

static void test_router_match_prefix(void **state) {
    (void)state;
    assert_int_equal(router_match("GET", "/api/games/some-game-id"), 8);
    assert_int_equal(router_match("GET", "/api/games/id/rounds"), 8);
    assert_int_equal(router_match("GET", "/api/games/id/rounds/rid"), 8);
    assert_int_equal(router_match("HEAD", "/api/games/id/start"), 10);
    assert_int_equal(router_match("PATCH", "/api/games/id/rounds/rid"), 11);
}

static void test_router_match_not_found(void **state) {
    (void)state;
    assert_int_equal(router_match("GET", "/api/unknown"), -1);
    assert_int_equal(router_match("DELETE", "/api/users"), -1);
}

static void test_router_match_prefers_exact_over_prefix(void **state) {
    (void)state;
    assert_int_equal(router_match("GET", "/api/games"), 6);
    assert_int_equal(router_match("GET", "/api/games/"), 7);
}

static void test_router_dispatch_not_found(void **state) {
    (void)state;
    struct lws *fake_wsi = (struct lws *)0x1234;

    expect_value(__wrap_lws_return_http_status, wsi, fake_wsi);
    expect_value(__wrap_lws_return_http_status, code, 404);
    expect_string(__wrap_lws_return_http_status, html_body, "<h1>Not Found</h1>");
    will_return(__wrap_lws_return_http_status, 0);

	int result = router_dispatch(fake_wsi, "GET", "/api/nonexistent", NULL, 0);
	assert_int_equal(result, 1);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_router_get_count),
        cmocka_unit_test(test_router_get_route_users_get),
        cmocka_unit_test(test_router_get_route_hello),
        cmocka_unit_test(test_router_get_route_create_game),
        cmocka_unit_test(test_router_get_route_search),
        cmocka_unit_test(test_router_get_route_prefix_match),
        cmocka_unit_test(test_router_get_route_out_of_bounds),
        cmocka_unit_test(test_router_match_exact),
        cmocka_unit_test(test_router_match_prefix),
        cmocka_unit_test(test_router_match_not_found),
        cmocka_unit_test(test_router_match_prefers_exact_over_prefix),
        cmocka_unit_test(test_router_dispatch_not_found),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}