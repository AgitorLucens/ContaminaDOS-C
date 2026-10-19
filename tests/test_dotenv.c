#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmocka.h>
#include "dotenv.h"

#ifdef _WIN32
#include <windows.h>
static const char *env_get(const char *key) {
    static char buf[4096];
    DWORD ret = GetEnvironmentVariableA(key, buf, sizeof(buf));
    return (ret > 0 && ret < sizeof(buf)) ? buf : NULL;
}
#else
static const char *env_get(const char *key) {
    return getenv(key);
}
#endif

static void test_dotenv_load_success(void **state) {
    (void)state;
    FILE *f = fopen("test.env", "w");
    assert_non_null(f);
    fprintf(f, "TEST_KEY=test_value\n");
    fprintf(f, "ANOTHER_KEY=12345\n");
    fclose(f);

    int ret = dotenv_load("test.env");
    assert_int_equal(ret, 0);

    const char *val = env_get("TEST_KEY");
    assert_non_null(val);
    assert_string_equal(val, "test_value");

    val = env_get("ANOTHER_KEY");
    assert_non_null(val);
    assert_string_equal(val, "12345");

    remove("test.env");
}

static void test_dotenv_load_comments_and_blanks(void **state) {
    (void)state;
    FILE *f = fopen("test.env", "w");
    assert_non_null(f);
    fprintf(f, "# this is a comment\n");
    fprintf(f, "\n");
    fprintf(f, "KEY_WITH_COMMENT=value\n");
    fclose(f);

    int ret = dotenv_load("test.env");
    assert_int_equal(ret, 0);

    const char *val = env_get("KEY_WITH_COMMENT");
    assert_non_null(val);
    assert_string_equal(val, "value");

    remove("test.env");
}

static void test_dotenv_load_file_not_found(void **state) {
    (void)state;
    int ret = dotenv_load("nonexistent.env");
    assert_int_equal(ret, -1);
}

static void test_dotenv_load_ignores_malformed(void **state) {
    (void)state;
    FILE *f = fopen("test.env", "w");
    assert_non_null(f);
    fprintf(f, "NO_EQUALS_SIGN\n");
    fprintf(f, "=just_value\n");
    fprintf(f, "VALID_KEY=valid_value\n");
    fclose(f);

    int ret = dotenv_load("test.env");
    assert_int_equal(ret, 0);

    const char *val = env_get("VALID_KEY");
    assert_non_null(val);
    assert_string_equal(val, "valid_value");

    remove("test.env");
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_dotenv_load_success),
        cmocka_unit_test(test_dotenv_load_comments_and_blanks),
        cmocka_unit_test(test_dotenv_load_file_not_found),
        cmocka_unit_test(test_dotenv_load_ignores_malformed),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}