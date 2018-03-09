#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <termios.h>
#include <fstream>
#include <signal.h>
#include <memory.h>
#include <memory>
#include <iostream>
#include <vector>
#include <wait.h>

using namespace std;

#define EVENTS_SIZE 20
#define BUFFER_SIZE 1000
#define SOCK_QUEUE_SIZE 100

struct raii_fd {
    raii_fd(int fd) : fd(fd) {}

    ~raii_fd() {
        cout << "file descriptor closed" << endl;
        close(fd);
    }

    int fd;
};

enum class fd_type {
    listener, socket, terminal
};

struct fd_container {
    fd_container(int fd, fd_type type) :
            fd(fd),
            type(type),
            write_blocked(false),
            read_blocked(false),
            write_set(false),
            read_set(false) {}

    fd_container *other;
    raii_fd fd;
    bool write_blocked;
    bool write_set;
    bool read_set;
    bool read_blocked;
    fd_type type;
    string write_queue;

    int read_data() {
        char buffer[BUFFER_SIZE];
        int bytes_read = read(fd.fd, buffer, BUFFER_SIZE);

        if (bytes_read == 0) {
            return -1; //socket is closed
        } else if (bytes_read == -1) {
            if (errno != EAGAIN) {
                return -1;
            } else {
                return 0;
            }
        } else {
            other->write_queue.append(buffer, bytes_read);
            int write_res = other->write_data();
            if (write_res == 0) {
                read_blocked = true;
                read_set = true;
                other->write_blocked = false;
                other->write_set = true;
            }
        }

        return bytes_read;
    }

    int write_data() {
        while (!write_queue.empty()) {
            int bytes_write = write(fd.fd, write_queue.c_str(), write_queue.size());
            if (bytes_write == -1) {
                if (errno == EWOULDBLOCK) {
                    return 0;
                } else {
                    return -1;
                }
            } else {
                write_queue = write_queue.substr(bytes_write);
                if (write_queue.empty()) {
                    write_set = true;
                    write_blocked = true;
                    other->read_blocked = false;
                    other->read_set = true;
                }
            }
        }
        return 0;
    }

};

int create_listening_socket(uint16_t port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        cout << "failed to create socket" << endl;
        return -1;
    }
    sockaddr_in s_addr;
    memset(&s_addr, 0, sizeof(sockaddr_in));
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(port);
    s_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (const struct sockaddr *) &s_addr, sizeof(sockaddr_in)) < 0) {
        cout << "failed to bind" << endl;
        return -1;
    }
    listen(sock, SOCK_QUEUE_SIZE);
    return sock;
}

int accept_socket(int listening_socket) {
    sockaddr_in accept_data;
    memset(&accept_data, 0, sizeof(sockaddr_in));
    socklen_t len = sizeof(sockaddr_in);
    int client_sock = accept(listening_socket, (sockaddr *) &accept_data, &len);
    return client_sock;
}

void enable_nonblocking(int fd) {
    int status = fcntl(fd, F_GETFD);
    if (fcntl(fd, F_SETFL, status | O_NONBLOCK) == -1) {
        cout << "failed to set non blocking" << endl;
        exit(errno);
    }

}

int create_epoll(fd_container *listener) {
    int epoll_fd = epoll_create(EVENTS_SIZE);
    if (epoll_fd == -1) {
        cout << "failed to create epoll" << endl;
        exit(errno);
    }

    epoll_event event;
    event.events = EPOLLIN;
    event.data.ptr = (void *) listener;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listener->fd.fd, &event) == -1) {
        cout << "failed to set listener event to epoll" << endl;
        close(epoll_fd);
        exit(errno);
    }
    return epoll_fd;
}

void add_to_epoll(int epoll_fd, fd_container *client) {
    epoll_event event;
    event.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLET;
    event.data.ptr = (void *) client;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client->fd.fd, &event) == -1) {
        cout << "failed to add client to epoll" << endl;
        exit(errno);
    }
}


void modify_epoll(int epoll_fd, fd_container *client) {
    epoll_event event;
    event.events = 0;
    if (!client->write_blocked) {
        event.events = event.events | EPOLLIN | EPOLLET;
    }
    if (!client->read_blocked) {
        event.events = event.events | EPOLLOUT | EPOLLET;
    }
    event.data.ptr = (void *) client;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client->fd.fd, &event) == -1) {
        cout << "failed to modify client in epoll" << endl;
        exit(errno);
    }
}

int create_master_terminal() {
    int fdm = posix_openpt(O_RDWR);
    if (fdm < 0) {
        cout << "failed to open terminal" << endl;
        exit(errno);
    }
    if (grantpt(fdm) || unlockpt(fdm)) {
        cout << "failed to unlock terminal" << endl;
        exit(errno);
    }
    return fdm;
}


string const daemon_file = "/tmp/rshd.pid";
string const daemon_err_log = "/tmp/rshd.err.log";

void demonize() {
    ifstream inf(daemon_file);
    if (inf) {
        int pid;
        inf >> pid;
        if (!kill(pid, 0)) {
            cerr << "Daemon is already running with PID " << pid << endl;
            exit(pid);
        }
    }
    inf.close();
    auto res = fork();
    if (res == -1) {
        perror("Fork FAIL");
        exit(errno);
    }
    if (res != 0) {
        exit(EXIT_SUCCESS);
    }
    setsid();

    int daemon_pid = fork();
    if (daemon_pid) {
        ofstream ouf(daemon_file, ofstream::trunc);
        ouf << daemon_pid;
        ouf.close();
        exit(EXIT_SUCCESS);
    }
    int slave = open("/dev/null", O_RDWR);
    int err = open(daemon_err_log.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
    dup2(slave, STDIN_FILENO);
    dup2(slave, STDOUT_FILENO);
    dup2(err, STDERR_FILENO);
    close(slave);
    close(err);
}

void handler(int sig, siginfo_t *siginfo, void *_) {
    if (sig == SIGCHLD) {
        waitpid(siginfo->si_pid, NULL, 0);
    }
}

vector<shared_ptr<fd_container> > clients;
vector<shared_ptr<fd_container> > terminals;

int main(int argc, char **argv) {
    if (argc < 2) {
        cout << "Need port to work." << endl;
        exit(errno);
    }

    struct sigaction act;
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = &handler;
    if (sigaction(SIGTERM, &act, NULL) || sigaction(SIGINT, &act, NULL)
        || sigaction(SIGCHLD, &act, NULL)) {
        cout << "Cannot set handlers." << endl;
        exit(errno);
    }

    demonize();

    uint16_t port = atoi(argv[1]);
    auto listener = make_shared<fd_container>(create_listening_socket(port), fd_type::listener);
    int epoll_fd = create_epoll(&(*listener));
    raii_fd epoll_container(epoll_fd);
    while (true) {
        epoll_event events[EVENTS_SIZE];
        int events_num = epoll_wait(epoll_fd, events, EVENTS_SIZE, -1);
        for (int i = 0; i < events_num; i++) {
            auto *cont = (fd_container *) events[i].data.ptr;
            if (cont->type == fd_type::listener) {
                cout << "New client connected." << endl;
                clients.push_back(make_shared<fd_container>(accept_socket(listener->fd.fd), fd_type::socket));
                terminals.push_back(make_shared<fd_container>(create_master_terminal(), fd_type::terminal));
                clients.back()->other = &(*terminals.back());
                terminals.back()->other = &(*clients.back());
                add_to_epoll(epoll_fd, &(*clients.back()));
                add_to_epoll(epoll_fd, &(*terminals.back()));
                enable_nonblocking(clients.back()->fd.fd);
                enable_nonblocking(terminals.back()->fd.fd);

                int slave = open(ptsname(terminals.back()->fd.fd), O_RDWR);
                auto proc = fork();
                if (!proc) {
                    clients.clear();
                    terminals.clear();
                    listener.reset();
                    close(epoll_fd);

                    struct termios slave_orig_term_settings; // Saved terminal settings
                    struct termios new_term_settings; // Current terminal settings
                    tcgetattr(slave, &slave_orig_term_settings);
                    new_term_settings = slave_orig_term_settings;
                    new_term_settings.c_lflag &= ~(ECHO | ECHONL | ICANON);

                    tcsetattr(slave, TCSANOW, &new_term_settings);

                    dup2(slave, STDIN_FILENO);
                    dup2(slave, STDOUT_FILENO);
                    dup2(slave, STDERR_FILENO);
                    close(slave);

                    setsid();

                    ioctl(0, TIOCSCTTY, 1);

                    execlp("/bin/sh", "sh", NULL);
                } else {
                    close(slave);
                }

            } else {
                int res = -1;
                    cout << "Working with client." << endl;
                if ((events[i].events & EPOLLIN) != 0) {
                    cout << "Read event.\n";
                    res = cont->read_data();
                    if (res == -1) {
                        cout << "res -1 after read" << endl;
                    }
                } else if ((events[i].events & EPOLLOUT) != 0) {
                    cout << "Write event.\n";
                    res = cont->write_data();
                    cout << "Write finished." << endl;
                    if (res == -1) {
                        cout << "res -1 after write" << endl;
                    }
                } else if ((events[i].events & EPOLLERR) != 0) {
                    cout << "Error event." << endl;
                }

                if (cont->write_set || cont->read_set) {
                    cont->write_set = false;
                    cont->read_set = false;
                    modify_epoll(epoll_fd, cont);
                }
                if (cont->other->write_set || cont->other->read_set) {
                    cont->other->write_set = false;
                    cont->other->read_set = false;
                    modify_epoll(epoll_fd, cont->other);
                }

                if (res == -1) {
                    fd_container *term = cont->other;
                    if (term->type == fd_type::socket) {
                        swap(term, cont);
                    }
                    for (auto it = clients.begin(); it != clients.end(); ++it) {
                        if (it->get() == cont) {
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, (*it)->fd.fd, events + i);
                            clients.erase(it);
                            break;
                        }
                    }
                    for (auto it = terminals.begin(); it != terminals.end(); ++it) {
                        if (it->get() == term) {
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, (*it)->fd.fd, events + i);
                            terminals.erase(it);
                            break;
                        }
                    }
                    cerr << "Client disconnected" << endl;
                }
            }
        }
    }
}