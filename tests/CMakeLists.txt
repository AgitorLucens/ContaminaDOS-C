include(ExternalProject)
include(FetchContent)

ExternalProject_Add(
    cmocka_ext
    GIT_REPOSITORY https://gitlab.com/cmocka/cmocka.git
    GIT_TAG        cmocka-1.1.7
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Debug
        -DBUILD_SHARED_LIBS=OFF
        -DWITH_EXAMPLES=OFF
        -DWITH_CMOCKERY_SUPPORT=OFF
        -DUNIT_TESTING=OFF
        -DPICKY_DEVELOPER=OFF
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/cmocka-install
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
    BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/cmocka-install/lib/libcmocka.a
)

set(CMOCKA_INCLUDE_DIR ${CMAKE_BINARY_DIR}/cmocka-install/include)
set(CMOCKA_LIB ${CMAKE_BINARY_DIR}/cmocka-install/lib/libcmocka.a)

# ─── router_tests ────────────────────────────────────────────
add_executable(router_tests test_router.c)
add_dependencies(router_tests cmocka_ext)
target_include_directories(router_tests PRIVATE ${CMOCKA_INCLUDE_DIR})
target_link_libraries(router_tests PRIVATE ${CMOCKA_LIB} app_lib websockets)
target_link_options(router_tests PRIVATE -Wl,--wrap=lws_return_http_status)
add_test(NAME router_tests COMMAND router_tests)

# ─── dotenv_tests ────────────────────────────────────────────
add_executable(dotenv_tests test_dotenv.c)
add_dependencies(dotenv_tests cmocka_ext)
target_include_directories(dotenv_tests PRIVATE ${CMOCKA_INCLUDE_DIR})
target_link_libraries(dotenv_tests PRIVATE ${CMOCKA_LIB} app_lib websockets)
add_test(NAME dotenv_tests COMMAND dotenv_tests)

# ─── gamepool_tests ──────────────────────────────────────────
add_executable(gamepool_tests test_gamepool.c)
add_dependencies(gamepool_tests cmocka_ext)
target_include_directories(gamepool_tests PRIVATE ${CMOCKA_INCLUDE_DIR})
target_link_libraries(gamepool_tests PRIVATE ${CMOCKA_LIB} app_lib websockets)
add_test(NAME gamepool_tests COMMAND gamepool_tests)