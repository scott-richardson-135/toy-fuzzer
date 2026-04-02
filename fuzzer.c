#include <stdio.h>
#include <stdlib.h>
#include <time.h>


#define MAX_INPUT  256


void generate_random_input(unsigned char *buf, size_t len) {
    for (int i = 0; i < len; i++) {
        buf[i] = rand() % 256;
    }
}
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <target binary>\n", argv[0]);
        return 1;
    }

    unsigned char buf[MAX_INPUT];

    srand(time(NULL));

    size_t len = (rand() % MAX_INPUT) + 1;

    generate_random_input(buf, len);
    buf[len] = '\0';
    printf("%s\n", buf);
}