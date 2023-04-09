#include "leptjson.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char buffer[1024];

int main() {
    lept_value v;
    char *s = NULL;
    lept_init(&v);
    FILE *fp = fopen("../.vscode/c_cpp_properties.json", "rb");
    if (fp == NULL) {
        return 1;
    }
    fread(buffer, sizeof(buffer), 1, fp);
    if (lept_parse(&v, buffer) != LEPT_PARSE_OK) {
        return 1;
    }
    // lept_parse(&v, "\"\\u0000\"");
    s = lept_stringify(&v, NULL);
    printf("%s", s);
    lept_free(&v);
    free(s);
    return 0;
}