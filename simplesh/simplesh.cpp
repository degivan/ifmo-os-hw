#include <fcntl.h>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <string>
#include <vector>
#include <sstream>

using namespace std;

const size_t BUF_MAX_SIZE = 1000;
const string ENV = "$ ";

vector<pid_t> children;


void write_all(int fd, const char *buf, size_t len);

void write_all(int fd, const string &str);

void check_error(ssize_t ret, string const &msg);

void check_error();

void env();

vector<string> &split(const string &s, char delimeter, vector<string> &res);

void close_pipe(int *pipefd);

bool try_close(int fd);

bool exec_commandpipe(const vector<string> &cpipe);

bool exec_command(const string &command, int *infd, int *outfd);

char **get_args(const string &command);

void try_duplicate(int fd1, int fd2);

void check_sig_intr();

bool sig_intr = false;
bool first_dead = false;
int *firstfd;

void signal_handler(int sig, siginfo_t *info, void *_) {
    if (sig == SIGINT) {
        sig_intr = true;
    }
    if (sig == SIGCHLD && info->si_pid == children[0]) {
        first_dead = true;
    }
}

void write_all(int fd, const char *buf, size_t len) {
    while (len > 0) {
        ssize_t bytes_writen = write(fd, buf, len);
        if (bytes_writen == -1) {
            check_error();
            continue;
        }
        len -= bytes_writen;
        buf += bytes_writen;
    }
}

inline void write_all(int fd, const string &str) {
    write_all(fd, str.c_str(), str.size());
}

void check_error(ssize_t ret, string const &msg) {
    if (ret == -1) {
        if (errno != EINTR) {
            write_all(STDERR_FILENO, "Error during " + msg + " -- " + strerror(errno) + "\n");
            exit(errno);
        }
    }
}

inline void check_error() {
    if (errno != EINTR) {
        exit(errno);
    }
}

bool try_close(int fd) {
    while (close(fd) == -1) {
        if (errno != EINTR) {
            return false;
        }
    }
    return true;
}

void close_pipe(int *pipefd) {
    try_close(pipefd[0]);
    try_close(pipefd[1]);
    delete[] pipefd;
}

inline void env() {
    write_all(STDOUT_FILENO, ENV);
}

void check_sig_intr() {
    if (sig_intr) {
        for (pid_t c : children) {
            kill(c, SIGINT);
        }
    }
    sig_intr = false;
}

vector<string> &split(const string &s, char delimeter, vector<string> &res) {
    stringstream ss(s);
    string item;
    while (getline(ss, item, delimeter)) {
        if (!item.empty()) {
            res.push_back(item);
        }
    }
    return res;
}

char **get_args(const string &command) {
    vector<string> words;
    split(command, ' ', words);

    auto **args = new char *[words.size() + 1];
    for (size_t i = 0; i < words.size(); i++) {
        args[i] = new char[words[i].size()];
        strcpy(args[i], words[i].c_str());
    }
    args[words.size()] = nullptr;
    return args;
}

void try_duplicate(int fd1, int fd2) {
    while (dup2(fd1, fd2) == -1) {
        check_error();
    }
}

bool exec_command(const string &command, int *infd, int *outfd) {
    char **args = get_args(command);
    pid_t cpid = fork();
    if (cpid == -1) {
        return false;
    }
    if (cpid == 0) {
        try_duplicate(infd[0], STDIN_FILENO);
        if (outfd != nullptr) {
            try_duplicate(outfd[1], STDOUT_FILENO);
        }
        if (infd != firstfd) {
            close_pipe(infd);
        }
        if (outfd != nullptr) {
            close_pipe(outfd);
        }
        close_pipe(firstfd);
        (execvp(args[0], const_cast<char* const*>(args)), "execution a command \'" + string{args[0]} + "\'");
    }
    children.push_back(cpid);
    return true;
}

bool exec_commandpipe(const vector<string> &cpipe) {
    auto *infd = new int[2];
    auto *outfd = new int[2];
    firstfd = infd;
    if (pipe(infd) == -1) {
        return false;
    }
    for (int i = 0; i < cpipe.size() - 1; i++) {
        if (pipe(outfd) == -1) {
            if (firstfd != infd) {
                close_pipe(infd);
            }
            close_pipe(firstfd);
            return false;
        }
        if (!exec_command(cpipe[i], infd, outfd)) {
            if (firstfd != infd) {
                close_pipe(infd);
            }
            close_pipe(firstfd);
            close_pipe(outfd);
            return false;
        }
        if (firstfd != infd) {
            close_pipe(infd);
        }
        infd = outfd;
        outfd = new int[2];
    }
    bool success = exec_command(cpipe.back(), infd, nullptr);
    if (firstfd != infd) {
        close_pipe(infd);
    }
    return success;
}

int main() {
    struct sigaction sa;
    sa.sa_sigaction = &signal_handler;
    sa.sa_flags = SA_SIGINFO;
    check_error(sigemptyset(&sa.sa_mask), "sigemptyset");
    check_error(sigaddset(&sa.sa_mask, SIGINT), "sigaddset <-- SIGINT");
    check_error(sigaddset(&sa.sa_mask, SIGCHLD), "sigaddset <-- SIGCHLD");
    check_error(sigaction(SIGINT, &sa, nullptr), "sigaction <-- SIGINT");
    check_error(sigaction(SIGCHLD, &sa, nullptr), "sigaction <-- SIGCHLD");

    char buffer[BUF_MAX_SIZE];
    ssize_t ssize = 0;
    size_t checked_symbols = 0;
    string command{};
    env();
    while ((ssize = read(STDIN_FILENO, buffer, BUF_MAX_SIZE)) != 0 || !command.empty()) {
        if (ssize == -1) {
            check_error();
            continue;
        }
        auto size = (size_t) ssize;
        command += {buffer, size};
        size_t nl_char = command.find('\n', checked_symbols);
        if (nl_char == -1) {
            checked_symbols = command.size();
            if (size != 0) {
                continue;
            }
        } else {
            checked_symbols = nl_char;
        }
        if (checked_symbols == 0) {
            command = command.substr(1);
            env();
            continue;
        }

        string com = command.substr(0, checked_symbols);
        string tail;
        if (checked_symbols < command.size()) {
            tail = command.substr(checked_symbols + 1);
        }
        vector<string> subcommands{};
        split(com, '|', subcommands);
        bool success = exec_commandpipe(subcommands);
        if (success) {
            write_all(firstfd[1], tail);
            while (!(sig_intr || first_dead)) {
                ssize = read(STDIN_FILENO, buffer, BUF_MAX_SIZE);
                if (ssize == -1) {
                    check_error();
                    continue;
                }
                if (ssize == 0) {
                    try_close(firstfd[1]);
                    break;
                }
                write_all(firstfd[1], buffer, (size_t) ssize);
            }
        }
        check_sig_intr();

        for (pid_t c : children) {
            while(true) {
                if (waitpid(c, nullptr, 0) == -1) {
                    check_sig_intr();
                    continue;
                }
                break;
            }
        }

        children.clear();
        checked_symbols = 0;
        command.clear();

        if (success) {
            try_close(firstfd[1]);
            while ((ssize = read(firstfd[0], buffer, BUF_MAX_SIZE)) != 0) {
                if (ssize == -1) {
                    check_error();
                    continue;
                }
                command += {buffer, (size_t) ssize};
            }
            try_close(firstfd[0]);
            delete [] firstfd;
        }
        sig_intr = false;
        first_dead = false;
        env();
    }
}