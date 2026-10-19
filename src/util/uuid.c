#include "uuid.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int uuid_seeded = 0;

void uuid_generate(char *out) {
    if (!uuid_seeded) {
        srand((unsigned int)(time(NULL) ^ (rand() * 137)));
        uuid_seeded = 1;
    }

    unsigned int r1 = rand();
    unsigned int r2 = rand();
    unsigned int r3 = rand();
    unsigned int r4 = rand();

    snprintf(out, MAX_UUID_STR,
        "%08x-%04x-%04x-%04x-%08x%04x",
        r1,
        (unsigned short)((r2 >> 16) & 0xffff),
        (unsigned short)(((r2 & 0x0fff) | 0x4000) & 0xffff),
        (unsigned short)(((r3 & 0x3fff) | 0x8000) & 0xffff),
        r3, (unsigned short)(r4 & 0xffff));
}