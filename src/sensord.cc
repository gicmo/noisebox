#include <cinttypes>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <algorithm>

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef __linux
#include <linux/i2c-dev.h>
#else
#include "i2c-fake.h"
#endif

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "zmq.hpp"
#include <json/json.h>

namespace i2c {

class i2c_error : std::runtime_error {
public:
    i2c_error(const char *what_arg) :
            runtime_error(what_arg) {
    }
};

class device {
protected:
    explicit device(int fd) : fd(fd) { }

public:

    device(const device &other) = delete;

    device(device &&other)
        : fd(other.fd) {
        other.fd = -1;
    }

    static device open(int i2c_bus, int address) {
        char path[255];
        snprintf(path, sizeof(path), "/dev/i2c-%d", i2c_bus);

        int file = ::open(path, O_RDWR);
        if (file < 0) {
            throw i2c_error("Could not open i2c bus");
        }

        int res = ioctl(file, I2C_SLAVE, address);

        if (res < 0) {
            throw i2c_error("Failed to acquire bus access or talk to slave");
        }

        return device(file);
    }

    uint16_t read16(uint8_t cmd) {
        ssize_t n;

        if ((n = ::write(fd, &cmd, 1)) != 1) {
            throw i2c_error("Could not write to device");
        }

        uint8_t data[] = {0, 0};
        if ((n = read(fd, data, sizeof(data))) != sizeof(data)) {
            throw i2c_error("Could not read from device");
        }

        uint16_t value = data[0] << 8 | data[1];
        return value;
    }

    ~device() {
        if (fd > -1) {
            ::close(fd);
        }
    }

    int file_descriptor() {
        return fd;
    }

protected:
    int fd;
};


class mcp9808 : public device {
private:
    mcp9808(int fd) : device(fd) {
    }

public:
    mcp9808(device &&other) : device(std::move(other)) {
    }

    static mcp9808 open(int i2c_bus, int address) {
        return device::open(i2c_bus, address);
    }

    float read_temperature() {

        uint16_t data = read16(0x05);

        int value = data & 0x0FFF;
        float temp = value / 16.0f;

        //TODO: handle sign bit

        return temp;
    }

};

} //namespace i2c

namespace util {
    static zmq::message_t message_from_json(const Json::Value &js) {
        Json::StyledWriter writer;
        std::string out = writer.write(js);
        zmq::message_t msg(out.size());

        char *msg_data = static_cast<char *>(msg.data());
        std::copy(out.cbegin(), out.cend(), msg_data);

        return msg;
    }
}

static void
mcp_loop(zmq::context_t &zmq_ctx, i2c::mcp9808 &mcp)
{
    zmq::socket_t publisher(zmq_ctx, ZMQ_PUB);
    publisher.connect("inproc://sensors");

    while (1) {

        std::chrono::milliseconds duration(2000);
        std::this_thread::sleep_for(duration);

        float temp = mcp.read_temperature();
        std::cout << "Temperature: " << temp << std::endl;

        Json::Value js;
        js["temperature"] = temp;

        zmq::message_t msg = util::message_from_json(js);

        publisher.send(msg);
    }

}

int main(int argc, char **argv)
{
    printf("Temperature from MCP9808\n");
    i2c::mcp9808 dev = i2c::mcp9808::open(1, 0x18);

    zmq::context_t context(1);

    zmq::socket_t collector(context, ZMQ_XSUB);
    collector.bind("inproc://sensors");

    zmq::socket_t emitter(context, ZMQ_XPUB);
    emitter.bind("tcp://*:5556");

    std::thread mcp_thread(mcp_loop, std::ref(context), std::ref(dev));

    zmq::proxy(emitter, collector, nullptr);

    //TODO: need a way to stop the thread, shared bool var or so
    mcp_thread.join();

    return 0;
}
