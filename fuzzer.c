#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <limits.h>


#define FUZZ_MAX_INPUT  256
#define MAX_CORPUS 1000
#define INPUT_FILE  "/tmp/fuzz_input"
#define ITERATIONS 1000

typedef struct  {
    unsigned char bytes[FUZZ_MAX_INPUT];
    size_t len;
} corpus_entry;

int corpus_count = 0;
corpus_entry corpus_array[MAX_CORPUS];

void mutate_bitflip(unsigned char *buf, size_t len) {
    int bit_position = rand() % (len * 8);
    int byte_index = bit_position / 8;  //which byte is modified based on this bit position
    int bit_index = bit_position % 8;  //which bit within that byte needs to be modified
    buf[byte_index] ^= (1 << bit_index);
}

void mutate_byte_random(unsigned char *buf, size_t len) {
    int byte_position = rand() % len;
    buf[byte_position] = rand() % 256;
}

void mutate_interesting(unsigned char *buf, size_t len) {
    //try edge cases
    int interesting[] = {0, 255, 127, 128, 1};
    int byte_position = rand() % len;
    int interesting_val = interesting[rand() % (sizeof(interesting) / sizeof(interesting[0]))];
    buf[byte_position] = interesting_val;
}

void (*mutators[])(unsigned char *, size_t) = {
    mutate_bitflip,
    mutate_byte_random,
};

int num_mutators = sizeof(mutators) / sizeof(mutators[0]);

void mutate(unsigned char *buf, size_t len) {
    int r = rand() % num_mutators;
    mutators[r](buf, len);
}


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

int is_crash(int status) {
    return WIFSIGNALED(status);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <target binary>\n", argv[0]);
        return 1;
    }

    
    srand(time(NULL));

    //load corpus files into memory
    DIR *corpus_dir = opendir("corpus");
    if (corpus_dir == NULL) {
        perror("opendir");
        return 1;
    }

    struct dirent *file;
    while ((file = readdir(corpus_dir)) != NULL) {
        if (strcmp(file->d_name, ".") == 0 || strcmp(file->d_name, "..") == 0) {
            continue;
        }
        if (corpus_count >= MAX_CORPUS) {
            break;
        }

        char filepath[PATH_MAX];
        snprintf(filepath, sizeof(filepath), "corpus/%s", file->d_name);

        corpus_entry *entry = &corpus_array[corpus_count]; //pointer to next open slot
        FILE *corpus_file = fopen(filepath, "rb");
        entry->len = fread(entry->bytes, 1, FUZZ_MAX_INPUT, corpus_file);
        fclose(corpus_file);
        corpus_count++;
    } 
    
    closedir(corpus_dir);

    //DEBUG
    printf("Loaded %d corpus entries\n", corpus_count);
    for (int i = 0; i < corpus_count; i++) {
        printf("  [%d] len=%zu first_byte=0x%02X\n", i, corpus_array[i].len, corpus_array[i].bytes[0]);
    }

    unsigned char buf[FUZZ_MAX_INPUT];
    int crashes = 0;

    mkdir("crashes", 0755);

    for (int i = 0; i < ITERATIONS; i++) {
        //size_t len = (rand() % FUZZ_MAX_INPUT) + 1;

        //load random corpus file into working buffer
        int index = rand() % corpus_count; 
        corpus_entry *chosen = &corpus_array[index];
        size_t len = chosen->len;
        memcpy(buf, chosen->bytes, len);

        //pick random mutation function, apply it to buffer
        mutate(buf, len);     


        //run program >:)
        int status = run_program(argv[1], buf, len);

        if (is_crash(status)) {
            crashes++;

            printf("[Crash %d] - Crash on iteration %d (signal %d, len=%zu)\n", crashes, i, WTERMSIG(status), len);

            //write crash to file
            char crash_file[64];
            snprintf(crash_file, sizeof(crash_file), "crashes/crash_%d.bin", crashes);

            FILE *file = fopen(crash_file, "wb");
            fwrite(buf, 1, len, file);
            fclose(file);
        }
    }

    printf("Done. %d crashes in %d iterations\n", crashes, ITERATIONS);
    return 0;

    
}