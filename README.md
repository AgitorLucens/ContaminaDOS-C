# Gameserver C

A C server for the Contaminados social-deduction game, built with
[libwebsockets](https://github.com/warmcat/libwebsockets). It reimplements the
HTTP API of the original Go backend: the full game flow (create, search, join,
start, propose group, vote, submit action, win-condition check) with JSON
request/response, CORS support for calling it from the React frontend, and a
WebSocket endpoint for the chat protocol.

## Requirements

- **CMake** 3.18 or newer
- **MinGW-w64** (GCC 13.1.0 was used) with `gcc`, `g++` and `mingw32-make` on the PATH
- **Git** — the configure step downloads libwebsockets v4.3.5 and cmocka
  automatically via CMake FetchContent (no package manager required)

> OpenSSL is **not** required: TLS is disabled in `CMakeLists.txt`
> (`LWS_WITH_SSL=OFF`, `LWS_WITH_MBEDTLS=OFF`).

## Build

From the project root:

```sh
cmake -S . -B build -G "MinGW Makefiles" -DBUILD_TESTS=ON
cmake --build build -j
```

The first configure downloads the dependencies into `build/_deps` (needs an
internet connection). To share them between build folders, add
`-DFETCHCONTENT_BASE_DIR="C:/absolute/path/to/deps"`.

## Run

1. Copy `.env.example` to `.env` in the project root. The server reads it from
   its working directory:

   | Variable          | Meaning                       | Default |
   |-------------------|-------------------------------|---------|
   | `PORT`            | HTTP/WebSocket listening port | `3001`  |
   | `MAX_MSG_PAYLOAD` | Max message size in bytes     | `1024`  |

2. Start the server from the project root (so it finds `.env`):

   ```sh
   ./build/App.exe
   ```

## Tests

With `-DBUILD_TESTS=ON`, 32 cmocka unit tests are built in 3 suites:

```sh
cd build
ctest --output-on-failure
```

If your IDE blocks the test executables, run `ctest` from an elevated terminal.

## API

All JSON responses include `Access-Control-Allow-Origin: *`, and cross-origin
preflight (`OPTIONS`) requests are answered automatically, so the React
frontend can call the API directly (e.g. `http://localhost:5173` →
`http://localhost:3001`).

| Method | Path                                   | Description                          |
|--------|----------------------------------------|--------------------------------------|
| GET    | `/api/health`                          | Health check                         |
| GET    | `/hello`                               | Demo endpoint                        |
| GET    | `/api/users`                           | Example users route                  |
| POST   | `/api/users`                           | Example users route                  |
| POST   | `/api/games`                           | Create a game                        |
| GET    | `/api/games`                           | Search games                         |
| GET    | `/api/games/{gameId}`                  | Get a game                           |
| PUT    | `/api/games/{gameId}`                  | Join a game (`player`, `password`)   |
| HEAD   | `/api/games/{gameId}/start`            | Start a game (`?player=&password=`)  |
| GET    | `/api/games/{gameId}/rounds`           | List rounds of a game                |
| GET    | `/api/games/{gameId}/rounds/{roundId}` | Show a round                         |
| PATCH  | `/api/games/{gameId}/rounds/{roundId}` | Propose a group (`player`, `group`)  |
| POST   | `/api/games/{gameId}/rounds/{roundId}` | Vote for a group (`player`, `vote`)  |
| PUT    | `/api/games/{gameId}/rounds/{roundId}` | Submit action (`player`, `action`)   |

## Project Structure

```
src/
├── core/
│   ├── http.c/h       # HTTP callback with per-session body buffering + preflight
│   ├── ws.c/h         # WebSocket broadcast
│   └── router.c/h     # Route table with exact + prefix matching
├── game/
│   ├── models.h       # Game/Round struct and enum definitions
│   ├── gamepool.c/h   # Singleton game pool with full business logic
│   └── handlers.c/h   # HTTP handlers for the game endpoints
├── util/
│   ├── dotenv.c/h     # .env file loader
│   ├── uuid.c/h       # UUID v4 generator (rand-based)
│   └── response.c/h   # JSON response builder, request parser, CORS headers
├── routes/
│   ├── users.c/h      # Example user route handlers
│   └── health.c/h     # Health check endpoint
└── main.c/h           # Entry point, loads .env, creates lws context

tests/
├── test_router.c      # 12 tests: route matching + dispatch
├── test_dotenv.c      # 4 tests: .env parsing
└── test_gamepool.c    # 16 tests: game logic (create/search/join/start/propose/vote/action/win)

CMakeLists.txt
```

## Features

- **Router** — exact path and prefix-based matching, Open/Closed Principle (add routes without modifying dispatch)
- **Game logic** — full social-deduction game flow: create, search, join, start, propose group, vote, submit action, win-condition check
- **API** — game endpoints matching the Go backend spec, JSON request/response
- **CORS** — `Access-Control-Allow-Origin: *` on all responses, automatic `OPTIONS` preflight handling
- **Configuration** — `.env` file support via self-contained parser
- **Testing** — TDD with cmocka, 32 unit tests across 3 suites, `-Wl,--wrap` for mocking lws calls
- **Dependencies** — libwebsockets and cmocka fetched via CMake FetchContent (no package manager required)

## Building in VS Code (optional)

Install the CMake Tools extension, select the MinGW toolchain, and add to
`settings.json` if you want the dependencies in a shared folder instead of
`build/_deps`:

```json
"cmake.configureArgs": [
    "-DFETCHCONTENT_BASE_DIR=${workspaceFolder}/deps",
    "-DBUILD_TESTS=ON"
]
```
