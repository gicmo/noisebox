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

std::string receive_string(zmq::socket_t &sock)
{
    zmq::message_t msg;
    sock.recv(&msg, 0);
    std::string result(static_cast<char *>(msg.data()), msg.size());
    return result;
}

inline bool receive_json(zmq::socket_t &sock, Json::Value &js)
{
    std::string data = receive_string(sock);
    Json::Reader reader;
    return reader.parse(data, js);
}

inline bool receive_empty(zmq::socket_t &sock, bool do_assert)
{
    std::string msg = receive_string(sock);
    bool res = msg.size() == 0;

    if (do_assert) {
        assert(res);
    }

    return res;
}

inline void send_empty(zmq::socket_t &sock, bool more = false)
{
    int flags = more ? ZMQ_SNDMORE : 0;
    sock.send(nullptr, 0, flags);
}

inline void send_string(zmq::socket_t &sock, const std::string &data, bool more = false)
{
    int flags = more ? ZMQ_SNDMORE : 0;
    sock.send(static_cast<const void *>(data.c_str()), data.size(), flags);
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