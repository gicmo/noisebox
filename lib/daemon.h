#ifndef _daemon_h_
#define _daemon_h_

#include <string>
#include <stdexcept>
#include <memory>
#include <thread>

#include <sys/unistd.h>
#include <zmq.hpp>

namespace unix {

class signal_radio {
public:
    signal_radio(zmq::context_t &zmq_ctx, int fd);
    ~signal_radio();

    zmq::socket_t subscribe();

    static constexpr const char* address() {
        return "inproc://signals";
    }

private:
    static constexpr const char* pair_address() {
        return "inproc://signals/pairing";
    }

    void thread_loop(int fd);

private:
    zmq::context_t &zmq_ctx;
    std::thread     bg_thread;
};



class daemon {
public:
    class ctx;

    daemon(char *);

    bool is_running() {
        return pid >= 0;
    }

    ctx& daemonize(bool background = true);

    void exit();

    ~daemon() {
        exit();
    }

    void log(int level, const char *format, ...);

    void err(const char *format, ...);
    void info(const char *format, ...);


public:
    class ctx {

    public:
        zmq::context_t& zmq() {
            return zmq_ctx;
        }

        signal_radio &sradio() {
            return signal_station;
        }

    private:

        ctx(int fd) : zmq_ctx(1), signal_station(zmq_ctx, fd) { }

        friend class daemon;
        zmq::context_t zmq_ctx;
        signal_radio   signal_station;
    };

private:
    ctx &get_context();
    int setup_signals();

private:
    pid_t pid;
    std::unique_ptr<ctx> context;
};

}

#endif