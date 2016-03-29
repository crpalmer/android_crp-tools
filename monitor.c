#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

char buffer[100*1024];

int main(int argc, char **argv)
{
    if (argc <= 1) {
        fprintf(stderr, "usage: list of files to monitor\n");
        exit(1);
    }
    while (1) {
        int i;

        printf("%lu", (unsigned long) time(NULL));
        for (i = 1; i < argc; i++) {
            FILE *f = fopen(argv[i], "r");
            if (f) {
                ssize_t read;
                if ((read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
                    buffer[read-1] = '\0';
                    printf(" %s", buffer);
                }
                fclose(f);
            }
        }
        printf("\n");
        sleep(1);
    }
}
