#include "dotenv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define MAX_LINE 4096

int dotenv_load(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp)
        return -1;

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[--len] = '\0';
        if (len == 0 || line[0] == '#')
            continue;

        char *eq = strchr(line, '=');
        if (!eq)
            continue;

        *eq = '\0';
        char *key = line;
        char *value = eq + 1;

        if (strlen(key) == 0)
            continue;

#ifdef _WIN32
        SetEnvironmentVariableA(key, value);
#else
        setenv(key, value, 1);
#endif
    }

    fclose(fp);
    return 0;
}