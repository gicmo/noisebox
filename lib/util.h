#ifndef _util_h_
#define _util_h_

#include <string>
#include <stdexcept>
#include <algorithm>

#include <zmq.hpp>
#include <json/json.h>

namespace util {

zmq::message_t message_from_json(const Json::Value &js) {
    Json::StyledWriter writer;
    std::string out = writer.write(js);
    zmq::message_t msg(out.size());

    char *msg_data = static_cast<char *>(msg.data());
    std::copy(out.cbegin(), out.cend(), msg_data);

    return msg;
}

inline void forward_msg(zmq::message_t &msg,
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
inline bool poll(zmq_pollitem_t (&items)[N], long timeout = -1)
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

#endif