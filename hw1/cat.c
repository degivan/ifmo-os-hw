//
// Created by vanyadeg on 05.03.18.
//

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>

const size_t BUF_CAP = 1000;

int main(size_t argc, char** argv) {
    int fd;
    char buffer[BUF_CAP];
    ssize_t bytes_read;

    //Открываем файл/stdin
    if (argc == 2) {
        fd = open(argv[1], O_RDONLY);
    } else {
        fd = STDIN_FILENO;
    }

    while ((bytes_read = read(fd, buffer, BUF_CAP)) != 0) {
        if (bytes_read == -1) {
            if (errno == EINTR) {
                continue;
            } else {
                fprintf(stderr, "error : %s\n",  strerror(errno));
                return 1;
            }
        }
        ssize_t bytes_write  = 0;
        do {
            ssize_t iteration_write = write(fd, buffer + bytes_write, (size_t) (bytes_read - bytes_write));
            if (iteration_write == -1) {
                if (errno == EINTR) {
                    continue;
                } else {
                    fprintf(stderr, "error : %s\n",  strerror(errno));
                    return 1;
                }
            }
            bytes_write += iteration_write;
        } while (bytes_write < bytes_read);
    }

    if (fd != STDIN_FILENO) {
        close(fd);
    }

    return 0;
}

