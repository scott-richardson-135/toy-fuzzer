#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>


#define MAX_INPUT  256
#define INPUT_FILE  "/tmp/fuzz_input"


void generate_random_input(unsigned char *buf, size_t len) {
    for (int i = 0; i < len; i++) {
        buf[i] = rand() % 256;
    }
}

int run_program(char *target, unsigned char *buf, size_t len) {
    //write random input (buf) to file
    FILE *file = fopen(INPUT_FILE, "wb");
    if (!file) {
        perror("fopen");
        return -1;
    }

    fwrite(buf, 1, len, file);
    fclose(file);

    //execute program with input file passed in to stdin
    pid_t pid;
    if ((pid = fork()) < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        //child process
        freopen("/dev/null", "w", stderr);
        execl(target, target, INPUT_FILE, NULL);  //execl is weird - 2nd arg becomes argv[0], 3rd is argv[1] etc
        
        //if we get this far then exec screwed something up
        perror("execl");
        exit(1);

    }
    
    //parent
    int status;
    waitpid(pid, &status, 0);
    return status;

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

    int status = run_program(argv[1], buf, len);
    printf("Status: %d\n", status);
}