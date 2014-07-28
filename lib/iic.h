#ifndef _hw_h_
#define _hw_h_

#include <cinttypes>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <atomic>
#include <cassert>
#include <string>
#include <chrono>
#include <thread>

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef __linux
#include <linux/i2c-dev.h>
#else
#include <i2c-fake.h>
#endif

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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

#endif