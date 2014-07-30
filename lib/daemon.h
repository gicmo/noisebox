#ifndef _daemon_h_
#define _daemon_h_

#include <string>
#include <stdexcept>
#include <memory>

#include <sys/unistd.h>


#include <libdaemon/daemon.h>
#include <zmq.hpp>

namespace unix {

class daemon {
public:

    daemon(char *);

    bool is_running() {
        return pid >= 0;
    }

    bool daemonize();

    zmq::context_t& zmq_ctx() {
        return *zmq_ctx_ptr;
    }

    zmq::socket_t& zmq_signal() {
        return *zmq_signal_ptr;
    }

    void exit();

    ~daemon() {
        exit();
    }

    int signal_fd() {
        return daemon_signal_fd();
    }

    void handle_signal();

    zmq::socket_t signal_client();

    bool run() {
        return !quit;
    }

private:
    pid_t pid;
    std::unique_ptr<zmq::context_t> zmq_ctx_ptr;
    std::unique_ptr<zmq::socket_t> zmq_signal_ptr;
    bool quit;
};

}

#endif