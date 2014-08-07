
#include "daemon.h"

#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/unistd.h>
#include <sys/select.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <poll.h>
#include <unistd.h>

namespace unix {

signal_radio::signal_radio(zmq::context_t &zmq_ctx, int fd)
        : zmq_ctx(zmq_ctx),
          bg_thread(&signal_radio::thread_loop, this, fd)
{
    //we need to make sure that the PUB socket
    //has been bound already, so we handshake
    zmq::socket_t pairing(zmq_ctx, ZMQ_REP);
    pairing.bind(pair_address());
    util::receive_empty(pairing, true);
    util::send_empty(pairing, false);
}

zmq::socket_t signal_radio::subscribe()
{
    zmq::socket_t sock(zmq_ctx, ZMQ_SUB);
    sock.connect(address());
    sock.setsockopt(ZMQ_SUBSCRIBE, "", 0);
    return sock;
}

signal_radio::~signal_radio()
{
    if (bg_thread.joinable()) {
        bg_thread.join();
    }
}


void signal_radio::thread_loop(int fd)
{
    zmq::socket_t station(zmq_ctx, ZMQ_PUB);
    station.bind(address());

    zmq::socket_t pairing(zmq_ctx, ZMQ_REQ);
    pairing.connect(pair_address());
    util::send_empty(pairing, false);
    util::receive_empty(pairing, true);

    struct pollfd pfd = {fd, POLLIN, 0};

    bool do_loop = true;
    while (do_loop) {

        int r = poll(&pfd, 1, -1);
        if (r < 1) {
            if (errno == EINTR) {
                continue;
            }
            //maybe send this as well?
            std::cerr << "signal_radio: Error during poll()";
            std::cerr << std::endl;
        }

        int sig;
        ssize_t s = read(fd, &sig, sizeof(sig));

        if (s != sizeof(sig)) {
            std::cerr << "signal_radio: error during read()";
            std::cerr << std::endl;
            break;
        }

        switch (sig) {

            case SIGINT:
            case SIGQUIT:
            case SIGTERM:
                do_loop = false;
                station.send(&sig, sizeof(sig));
                break;

            default:
                //we ignore the rest
                break;
        }
    }
}

daemon::daemon(char *argv0)
{
        if (daemon_reset_sigs(-1) < 0) {
            daemon_log(LOG_ERR, "Failed to reset all signal handlers: %s", strerror(errno));
            ::exit(-1);
        }

        if (daemon_unblock_sigs(-1) < 0) {
            daemon_log(LOG_ERR, "Failed to unblock all signals: %s", strerror(errno));
            ::exit(-2);
        }

        daemon_pid_file_ident = daemon_log_ident = daemon_ident_from_argv0(argv0);

        pid = daemon_pid_file_is_running();

}

daemon::ctx& daemon::daemonize()
{
    if (daemon_retval_init()) {
        std::string err_msg = "Failed to create pipe";
        daemon_log(LOG_ERR, err_msg.c_str());
        ::exit(-3);
    }

    pid = daemon_fork();

    if (pid < 0) {
        //error
        daemon_retval_done();
        ::exit(-4);

    } else if (pid) {
        //parent

        int ret = daemon_retval_wait(20); //seconds

        if (ret < 0) {
            std::string err_msg = "Timeout waiting for daemon";
            daemon_log(LOG_ERR, err_msg.c_str());
            ::exit(-4);
        } else if (ret > 0) {
            std::string err_msg = "Error starting daemon";
            daemon_log(LOG_ERR, err_msg.c_str());
            ::exit(ret);
        }

        ::exit(ret);

    } else {
        //daemon
        if (daemon_close_all(-1) < 0) {
            daemon_log(LOG_ERR, "Failed to close all file descriptors: %s", strerror(errno));
            daemon_retval_send(1);
            exit();
        }

        if (daemon_pid_file_create() < 0) {
            daemon_log(LOG_ERR, "Could not create PID file (%s).", strerror(errno));
            daemon_retval_send(2);
            exit();
        }

        if (daemon_signal_init(SIGINT, SIGTERM, SIGQUIT, SIGHUP, 0) < 0) {
            daemon_log(LOG_ERR, "Could not register signal handlers (%s).", strerror(errno));
            daemon_retval_send(3);
            exit();
        }

        /* Send OK to parent process */
        daemon_retval_send(0);

        context = std::unique_ptr<ctx>(new ctx(daemon_signal_fd()));
        return *context;
    }
}

void daemon::log(int level, const char *format, ...)
{
    va_list va;
    va_start(va, format);
    daemon_logv(level, format, va);
    va_end(va);
}

void daemon::err(const char *format, ...)
{
    va_list va;
    va_start(va, format);
    daemon_logv(LOG_ERR, format, va);
    va_end(va);
}
void daemon::info(const char *format, ...)
{
    va_list va;
    va_start(va, format);
    daemon_logv(LOG_INFO, format, va);
    va_end(va);
}

void daemon::exit()
{
    daemon_log(LOG_INFO, "shutting down.");
    daemon_retval_send(255);
    daemon_signal_done();
    daemon_pid_file_remove();
    ::exit(255);
}

}