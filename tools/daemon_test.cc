

#include <daemon.h>
#include <util.h>
#include <zmq.h>


int main(int argc, char **argv)
{
    unix::daemon daemon{argv[0]};

    daemon.daemonize();
    //from now on we are the daemon

    zmq_pollitem_t items[] = {
            {nullptr, daemon.signal_fd(), ZMQ_POLLIN, 0 }
    };

    while (daemon.run()) {
        util::poll(items);

        if (items[0].revents & ZMQ_POLLIN) {
            daemon.handle_signal();
        }
        
    }
}