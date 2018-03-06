//
// Created by vanyadeg on 05.03.18.
//

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <zconf.h>

int caught = 0;
int is_alarm = 0;
pid_t cpid;

void handler(int sig, siginfo_t *siginfo, void *_) {
    if (sig == SIGALRM) {
        is_alarm = 1;
        return;
    }
    if (caught == 0) {
        caught = sig;
        cpid = siginfo->si_pid;
    }
}

int main() {
    struct sigaction act;
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = &handler;

    if (sigemptyset(&act.sa_mask)) {
        fprintf(stderr, "Error during sigemptyset(): %s\n", strerror(errno));
    }
    if (sigaddset(&act.sa_mask, SIGALRM) ||
        sigaddset(&act.sa_mask, SIGUSR1) ||
        sigaddset(&act.sa_mask, SIGUSR2)) {
        fprintf(stderr, "Error during sigaddset(): %s\n", strerror(errno));
    }
    if (sigaction(SIGALRM, &act, NULL) ||
        sigaction(SIGUSR1, &act, NULL) ||
        sigaction(SIGUSR2, &act, NULL)) {
        fprintf(stderr, "Error during sigaction(): %s\n", strerror(errno));
    }

    alarm(10);
    while (is_alarm == 0) {
        if (caught != 0) {
            printf("%s from %d\n", caught == SIGUSR1 ? "SIGUSR1" : "SIGUSR2", cpid);
            return 0;
        }
        pause();
    }

    printf("No signals were caught\n");
    return 0;
}
