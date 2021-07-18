#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// checks if filename is valid (according to class specs)
int valid_filename(char* s) {
    if(s[27] != '\0') return 0;
    for(int i = 0; i < (int)(strlen(s)); i++) {
        if(!((s[i] == '-') || (s[i] == '.')
            || (s[i] == '_')
            || (s[i] >= '0' && s[i] <= '9')
            || (s[i] >= 'A' && s[i] <= 'Z')
            || (s[i] >= 'a' && s[i] <= 'z'))) {
            return 0;
        }
    }
    return 1;
}

// fix
// returns port number or -1
int valid_port(char *p) {
    size_t port_len = strlen(p);
    for (size_t i = 0; i < port_len; i++) {
        if (p[i] < '0' || p[i] > '9') {
            return -1;
        }
    }
    return atoi(p);
}

void warn_exit(char *msg) {
    fprintf(stderr, "%s", msg);
    exit(EXIT_FAILURE);
}



