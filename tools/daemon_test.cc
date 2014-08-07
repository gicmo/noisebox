

#include <daemon.h>


static void worker(unix::daemon::ctx &ctx)
{
    zmq::socket_t radio = ctx.sradio().subscribe();

    zmq_pollitem_t items[] = {
            {static_cast<void *>(radio), 0, ZMQ_POLLIN, 0},
    };

    while (true) {
        zmq::message_t msg;
        int res = zmq_poll (items, 1, -1);
        if (items[0].revents & ZMQ_POLLIN) {
            break;
        }
    }
}

int main(int argc, char **argv)
{
    unix::daemon daemon{argv[0]};

    unix::daemon::ctx &ctx = daemon.daemonize();
    //from now on we are the daemon


    try {

        zmq::socket_t radio = ctx.sradio().subscribe();
        std::thread w1(worker, std::ref(ctx));

        zmq_pollitem_t items[] = {
                {static_cast<void *>(radio), 0, ZMQ_POLLIN, 0},
        };

        while (true) {
            zmq::message_t msg;

            util::poll(items);

            if (items[0].revents & ZMQ_POLLIN) {
                daemon.info("Got signal!");
                break;
            }
        }

        w1.join();

    } catch(std::exception &e) {
        daemon.err("Got exception: %s", e.what());
    }  catch(...) {
        daemon.err("Got unknown exception");
    }
}