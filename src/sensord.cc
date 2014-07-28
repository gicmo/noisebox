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


namespace util {

static zmq::message_t message_from_json(const Json::Value &js) {
    Json::StyledWriter writer;
    std::string out = writer.write(js);
    zmq::message_t msg(out.size());

    char *msg_data = static_cast<char *>(msg.data());
    std::copy(out.cbegin(), out.cend(), msg_data);

    return msg;
}

static inline void forward_msg(zmq::message_t &msg,
                               zmq::socket_t  &source,
                               zmq::socket_t  &sink) {
    while (1) {
        source.recv(&msg, 0);
        const bool more = msg.more();
        sink.send(msg, more ? ZMQ_SNDMORE : 0);
        msg.rebuild(); // reset the message

        if (!more) {
            break;
        }
    }
}

template<int N>
static inline bool poll(zmq_pollitem_t (&items)[N], long timeout = -1)
{
    int ret = zmq_poll(items, N, timeout);

    if (ret < 0) {
        if (errno != EINTR) {
            throw std::runtime_error("zmq_poll error!");
        }
    }

    return ret > 0;
}

} //namespace util

static void
mcp_loop(zmq::context_t &zmq_ctx, i2c::mcp9808 &mcp)
{
    zmq::socket_t publisher(zmq_ctx, ZMQ_PUB);
    publisher.connect("inproc://sensors");

    zmq::socket_t hangman(zmq_ctx, ZMQ_SUB);
    hangman.connect("inproc://hangman");
    hangman.setsockopt(ZMQ_SUBSCRIBE, "", 0);

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
htu_loop(zmq::context_t &zmq_ctx, i2c::htu21d &sensor)
{
    zmq::socket_t publisher(zmq_ctx, ZMQ_PUB);
    publisher.connect("inproc://sensors");

    zmq::socket_t hangman(zmq_ctx, ZMQ_SUB);
    hangman.connect("inproc://hangman");
    hangman.setsockopt(ZMQ_SUBSCRIBE, "", 0);

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

static void
datastore_loop(zmq::context_t &zmq_ctx)
{
    zmq::socket_t sensor_updates(zmq_ctx, ZMQ_SUB);
    sensor_updates.connect("tcp://localhost:5556");
    sensor_updates.setsockopt(ZMQ_SUBSCRIBE, "", 0);

    zmq::socket_t hangman(zmq_ctx, ZMQ_SUB);
    hangman.connect("inproc://hangman");
    hangman.setsockopt(ZMQ_SUBSCRIBE, "", 0);

    zmq::socket_t service(zmq_ctx, ZMQ_REP);
    service.bind("tcp://*:5557");

    zmq_pollitem_t items[] = {
            {static_cast<void *>(hangman), 0, ZMQ_POLLIN, 0 },
            {static_cast<void *>(sensor_updates), 0, ZMQ_POLLIN, 0 },
            {static_cast<void *>(service), 0, ZMQ_POLLIN, 0 }
    };

    float temp = 0.0;
    float humidity = 0.0;

    while (1) {

        bool ready = util::poll(items);
        assert(ready);

        if (items[0].revents & ZMQ_POLLIN) {

            assert(items[0].revents & ZMQ_POLLIN);
            zmq::message_t msg;
            hangman.recv(&msg);

            //currently the only message we can get
            //is to stop
            return;
        }

        if (items[1].revents & ZMQ_POLLIN) {
            // Sensor update
            auto update = wire::sensor_update::receive(sensor_updates);

            if (update.type == 0x01) {
                temp = update.value;
            } else if (update.type == 0x02) {
                humidity = update.value;
            } else {
                std::cerr << "[W] got update of unkown type" << std::endl;
            }

        }

        //TODO UPDATE
        if (items[2].revents & ZMQ_POLLIN) {
            zmq::message_t request;
            service.recv (&request);

            Json::Value js;
            js["temperature"] = temp;
            js["humidity"] = humidity;
            zmq::message_t msg = util::message_from_json(js);
            service.send(msg);

        }

    }
}

// signal handling
static std::atomic<bool> run_the_loop;

static void stop_signal_handler (int signal_value)
{
    std::cout << "Got the stop signal. Stopping." << std::endl;
    run_the_loop = false;
}

static void init_signals()
{
    struct sigaction sig_action;

    sig_action.sa_handler = stop_signal_handler;
    sig_action.sa_flags = 0;
    sigemptyset (&sig_action.sa_mask);

    sigaction (SIGINT, &sig_action, NULL);
    sigaction (SIGTERM, &sig_action, NULL);
}

int main(int argc, char **argv)
{
    printf("Temperature from MCP9808, Humidity from HTU21D\n");
    i2c::mcp9808 mcp_dev = i2c::mcp9808::open(1, 0x18);
    i2c::htu21d  htu_dev = i2c::htu21d::open(1, 0x40);

    zmq::context_t context(1);

    zmq::socket_t hangman(context, ZMQ_PUB);
    hangman.bind("inproc://hangman");

    zmq::socket_t collector(context, ZMQ_XSUB);
    collector.bind("inproc://sensors");

    zmq::socket_t emitter(context, ZMQ_XPUB);
    emitter.bind("tcp://*:5556");

    run_the_loop = true;
    init_signals();

    std::thread mcp_thread(mcp_loop, std::ref(context), std::ref(mcp_dev));
    std::thread htu_thread(htu_loop, std::ref(context), std::ref(htu_dev));
    std::thread dst_thread(datastore_loop, std::ref(context));

    //now the main runloop
    zmq_pollitem_t items[] = {
            {static_cast<void *>(collector), 0, ZMQ_POLLIN, 0 },
            {static_cast<void *>(emitter), 0, ZMQ_POLLIN, 0 }
    };


    while (run_the_loop) {
        zmq::message_t msg;

        util::poll(items);

        if (items[0].revents & ZMQ_POLLIN) {
            util::forward_msg(msg, collector, emitter);
        }

        if (items[1].revents & ZMQ_POLLIN) {
            util::forward_msg(msg, emitter, collector);
        }
    }

    zmq::message_t stop_msg;
    hangman.send(stop_msg);

    mcp_thread.join();
    htu_thread.join();
    dst_thread.join();

    return 0;
}
