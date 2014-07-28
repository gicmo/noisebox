#ifndef _nb_wire_h_
#define _nb_wire_h_

#include <type_traits>
#include <cinttypes>
#include <cstring>

#include <zmq.hpp>

namespace wire {

struct sensor_update {
    //64-bit header
    uint16_t type;
    uint16_t flags;
    uint32_t sender;

    //payload
    float value;

public:
    static void zmq_free(void *self, void *hint) {
        sensor_update *su = static_cast<sensor_update *>(self);
        delete su;
    }

    zmq::message_t make_msg() const {
        zmq::message_t msg(sizeof(*this));
        const void *data = reinterpret_cast<const void *>(this);
        std::memcpy(msg.data(), data, sizeof(*this));
        return msg;
    }

    static sensor_update from_msg(const zmq::message_t &msg) {
        //TODO:: check for size & validity
        sensor_update su;
        void *data = reinterpret_cast<void *>(&su);
        std::memcpy(data, msg.data(), msg.size());
        return su;
    }

    static inline const sensor_update* cast_from_msg(const zmq::message_t &msg) {
        const void *msg_data = msg.data();
        const sensor_update *data = static_cast<const sensor_update *>(msg_data);
        //TODO: validate
        return data;
    }
};

template<typename T>
inline bool zmq_data_is_aligned_for(const void *data)
{
    const size_t address = reinterpret_cast<size_t>(data);
    const size_t alignment = std::alignment_of<T>::value;
    return address % alignment == 0;
}

}
#endif

