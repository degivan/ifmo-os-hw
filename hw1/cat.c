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

bool check_errors(ssize_t size) {
    if (size == -1) {
        if (errno == EINTR) {
            return false;
        } else {
            printf("Error: %s", strerror(errno));
            return true;
        }
    }
    return false;
}

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
        if (check_errors(bytes_read)) {
            return 1;
        }
        ssize_t bytes_write  = 0;
        do {
            ssize_t iteration_write = write(fd, buffer + bytes_write, (size_t) (bytes_read - bytes_write));
            if (check_errors(iteration_write)) {
                return 1;
            }
            bytes_write += iteration_write;
        } while (bytes_write < bytes_read);
    }

    if (fd != STDIN_FILENO) {
        close(fd);
    }

    return 0;
}

