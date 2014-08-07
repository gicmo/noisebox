#include <cinttypes>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <algorithm>
#include <atomic>
#include <cassert>
#include <string>

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include <zmq.hpp>
#include <json/json.h>

//libnoisebox
#include <wire.h>
#include <iic.h>
#include <util.h>
#include <daemon.h>

static void
mcp_loop(unix::daemon::ctx &ctx, i2c::mcp9808 &mcp)
{
    zmq::context_t &zmq_ctx = ctx.zmq();
    zmq::socket_t publisher(zmq_ctx, ZMQ_PUB);
    publisher.connect("inproc://sensors");

    zmq::socket_t hangman = ctx.sradio().subscribe();

    zmq_pollitem_t items[] = {
        {static_cast<void *>(hangman), 0, ZMQ_POLLIN, 0 }
    };

    long timeout = 2000; //in ms
    while (1) {

        float temp = mcp.read_temperature();
        std::cout << "T: " << temp << std::endl;

        wire::sensor_update update = {0x01, 0x00, 0x18, temp};
        zmq::message_t msg = update.make_msg();
        publisher.send(msg);

        bool ready = util::poll(items, timeout);

        if (ready) {
            assert(items[0].revents & ZMQ_POLLIN);
            zmq::message_t hg_msg;
            hangman.recv(&hg_msg);

            //currently the only message we can get
            //is to stop
            return;
        }
    }
}

static void
htu_loop(unix::daemon::ctx &ctx, i2c::htu21d &sensor)
{
    zmq::context_t &zmq_ctx = ctx.zmq();

    zmq::socket_t publisher(zmq_ctx, ZMQ_PUB);
    publisher.connect("inproc://sensors");

    zmq::socket_t hangman = ctx.sradio().subscribe();

    zmq_pollitem_t items[] = {
            {static_cast<void *>(hangman), 0, ZMQ_POLLIN, 0 }
    };

    long timeout = 2000; //in ms
    while (1) {

        float humidity = sensor.read_humidity();
        std::cout << "H: " << humidity << std::endl;

        wire::sensor_update update = {0x02, 0x00, 0x40, humidity};
        zmq::message_t msg = update.make_msg();
        publisher.send(msg);

        bool ready = util::poll(items, timeout);

        if (ready) {
            assert(items[0].revents & ZMQ_POLLIN);
            zmq::message_t hg_msg;
            hangman.recv(&hg_msg);

            //currently the only message we can get
            //is to stop
            return;
        }
    }
}

int main(int argc, char **argv)
{
    unix::daemon daemon{argv[0]};
    unix::daemon::ctx &ctx = daemon.daemonize();

    daemon.info("noisebox sensor daemon running");

    i2c::mcp9808 mcp_dev = i2c::mcp9808::open(1, 0x18);
    i2c::htu21d  htu_dev = i2c::htu21d::open(1, 0x40);

    zmq::context_t &context = ctx.zmq();

    zmq::socket_t hangman(context, ZMQ_PUB);
    hangman.bind("inproc://hangman");

    zmq::socket_t collector(context, ZMQ_XSUB);
    collector.bind("inproc://sensors");

    zmq::socket_t emitter(context, ZMQ_XPUB);
    emitter.bind("tcp://*:5556");

    std::thread mcp_thread(mcp_loop, std::ref(ctx), std::ref(mcp_dev));
    std::thread htu_thread(htu_loop, std::ref(ctx), std::ref(htu_dev));

    //now the main runloop
    zmq_pollitem_t items[] = {
            {static_cast<void *>(hangman), 0, ZMQ_POLLIN, 0 },
            {static_cast<void *>(collector), 0, ZMQ_POLLIN, 0 },
            {static_cast<void *>(emitter), 0, ZMQ_POLLIN, 0 }
    };


    while (true) {
        zmq::message_t msg;

        util::poll(items);

        if (items[0].revents & ZMQ_POLLIN) {
            break;
        }

        if (items[1].revents & ZMQ_POLLIN) {
            util::forward_msg(msg, collector, emitter);
        }

        if (items[2].revents & ZMQ_POLLIN) {
            util::forward_msg(msg, emitter, collector);
        }
    }

    zmq::message_t stop_msg;
    hangman.send(stop_msg);

    mcp_thread.join();
    htu_thread.join();

    return 0;
}
