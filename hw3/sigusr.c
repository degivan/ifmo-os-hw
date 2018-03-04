//
// Created by vanyadeg on 05.03.18.
//

#include <stdio.h>
#include <signal.h>
#include <string.h>

void handler(int sig, siginfo_t *siginfo,  void* _) {
    //TODO:
}

int main() {
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = &handler;

    //TODO:

    return 0;
}
