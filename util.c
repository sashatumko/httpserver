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