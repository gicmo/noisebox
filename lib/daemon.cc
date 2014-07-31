
#include "daemon.h"

#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/unistd.h>
#include <sys/select.h>
#include <stdlib.h>

namespace unix {

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

bool daemon::daemonize()
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

        zmq_ctx_ptr = std::unique_ptr<zmq::context_t>(new zmq::context_t(1));
        zmq_signal_ptr = std::unique_ptr<zmq::socket_t>(new zmq::socket_t(zmq_ctx(), ZMQ_PUB));
        zmq_signal().bind("inproc://signal");

        return true;
    }
}

void daemon::handle_signal()
{
    int sig = daemon_signal_next();

    if (signal <= 0) {
        daemon_log(LOG_ERR, "daemon_signal_next() failed: %s", strerror(errno));
        exit();
    }

    /* Dispatch signal */
    switch (sig) {

        case SIGINT:
        case SIGQUIT:
        case SIGTERM:
            quit = 1;
            zmq_signal().send(&sig, sizeof(sig), 0);
            break;

        default:
            //we ignore the rest
            break;
    }
}

zmq::socket_t daemon::signal_client()
{
    zmq::socket_t sock(zmq_ctx(), ZMQ_SUB);
    sock.connect("inproc://signal");
    sock.setsockopt(ZMQ_SUBSCRIBE, "", 0);

    return sock;
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