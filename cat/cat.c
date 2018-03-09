#include <stdlib.h>
#include <fcntl.h>
#include <zconf.h>

const size_t BUF_CAP = 1000;

void process(int fd, char *buffer) {
    ssize_t rd; //rd - buffer_read
    while ((rd = read(fd, buffer, BUF_CAP)) > 0) {
        ssize_t tw = rd, bw; //tw - to_write, bw - buffer_wrote
        while (tw > 0 && (bw = write(1, buffer + (rd - tw), rd))) {
            if (bw < 0) return;
            tw = rd - bw;
        }
    }
}

int main(int argc, char **argv) {
    char *buffer = (char *) malloc(BUF_CAP);
    if (argc == 1) {
        process(0, buffer);
    } else {
        size_t it;
        for (it = 1; it < argc; it++) {
            int fd = open(argv[it], O_RDONLY);
            if (fd == -1)
                continue;
            process(fd, buffer);
            close(fd);
        }
    }
    free(buffer);
    return 0;
}