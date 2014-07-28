
#include <wire.h>

#include <iostream>
#include <iostream>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <db_cxx.h>
#include "util.h"


class store {
public:

    struct record {
        struct data {
            float temperature;
            float humidity;
        };


        uint32_t timestamp;
        data    value;

        //accessors
        data &readings() {
            return value;
        }

        float temperature() const {
            return value.temperature;
        }

        float humidity() const {
            return value.humidity;
        }

    };

    store(std::string path)
            : db(nullptr, 0) {

        uint32_t open_flags = DB_CREATE | DB_THREAD;

        try {
            db.set_bt_compare(store::compare_epoch);
            db.set_flags(DB_RECNUM);
            db.open(nullptr, path.c_str(), nullptr, DB_BTREE, open_flags, 0);
            is_open = true;
            db.set_error_stream(&std::cerr);

        } catch (DbException &e) {
            std::cerr << "Could not open DB" << std::endl;
        }
    }


    void put_data(uint32_t time_point, float temp, float humidity) {
        record::data data = {temp, humidity};
        Dbt db_key(static_cast<void *>(&time_point), sizeof(uint32_t));
        Dbt db_data(static_cast<void *>(&data), sizeof(data));

        int ret = db.put(nullptr, &db_key, &db_data, DB_NOOVERWRITE);

        std::cout << "storing: " << time_point << " " << temp << " " << humidity << std::endl;

        if (ret == DB_KEYEXIST) {
            std::cerr << "Key exists" << std::endl;
        } else if (ret != 0) {
            db.err(ret, "");
        }
    }

    std::vector<record> find_data(uint32_t t_start, uint32_t t_end) {
        Dbc *cursor;

        db.cursor(nullptr, &cursor, 0);
        Dbt db_key(static_cast<void *>(&t_start), sizeof(uint32_t));
        Dbt db_data;

        std::vector<record> result;

        int ret = cursor->get(&db_key, &db_data, DB_SET_RANGE);

        if (ret == DB_NOTFOUND) {
            return result;
        }

        if (ret != 0) {
            db.err(ret, "");
        }

        do {
            uint32_t timestamp;
            memcpy(&timestamp, db_key.get_data(), sizeof(timestamp));

            if (timestamp > t_end) {
                break;
            }

            size_t size = result.size();
            result.resize(size + 1);

            record &cur = result[size];
            record::data &r = cur.readings();
            cur.timestamp = timestamp;

            memcpy(static_cast<void *>(&r), db_data.get_data(), sizeof(r));
        } while ((ret = cursor->get(&db_key, &db_data, DB_NEXT)) == 0);
        //check ret

        if (cursor != nullptr) {
            cursor->close();
        }

        return result;

    }

    ~store() {
        if (is_open) {
            db.close(0);
        }
    }

private:

    static int compare_epoch(Db *db, const Dbt *a, const Dbt *b) {

        uint32_t ta;
        uint32_t tb;

        std::memcpy(static_cast<void *>(&ta), a->get_data(), sizeof(ta));
        std::memcpy(static_cast<void *>(&tb), b->get_data(), sizeof(tb));

        return ta - tb;
    }

private:
    Db db;
    bool is_open;
};

static long calc_timeout(time_t last_update, time_t interval)
{
    time_t now = std::time(nullptr);
    time_t dt = now - last_update;
    time_t secs_to_next_update = interval - std::min(dt, interval);
    return static_cast<long>(secs_to_next_update);
}

static void service_thread(zmq::context_t &zmq_ctx, store &db)
{
    zmq::socket_t hangman(zmq_ctx, ZMQ_SUB);
    hangman.connect("inproc://hangman");
    hangman.setsockopt(ZMQ_SUBSCRIBE, "", 0);

    zmq::socket_t service(zmq_ctx, ZMQ_REP);
    service.bind("tcp://*:5557");

    zmq_pollitem_t items[] = {
            {static_cast<void *>(service), 0, ZMQ_POLLIN, 0 },
            {static_cast<void *>(hangman), 0, ZMQ_POLLIN, 0 }
    };

    float temp = 0.0;
    float humidity = 0.0;

    while (1) {

        bool ready = util::poll(items);
        assert(ready);


        if (items[0].revents & ZMQ_POLLIN) {
            zmq::message_t request;
            service.recv (&request);

            Json::Value js;
            js["temperature"] = temp;
            js["humidity"] = humidity;
            zmq::message_t msg = util::message_from_json(js);
            service.send(msg);

        }

        if (items[1].revents & ZMQ_POLLIN) {

            assert(items[0].revents & ZMQ_POLLIN);
            zmq::message_t msg;
            hangman.recv(&msg);

            //currently the only message we can get
            //is to stop
            return;
        }


    }
}

int main(int argc, char **argv)
{
    store db("data.db");

    zmq::context_t zmq_ctx(1);

    zmq::socket_t sensor_updates(zmq_ctx, ZMQ_SUB);
    sensor_updates.connect("tcp://localhost:5556");
    sensor_updates.setsockopt(ZMQ_SUBSCRIBE, "", 0);

    zmq_pollitem_t items[] = {
            {static_cast<void *>(sensor_updates), 0, ZMQ_POLLIN, 0 }
    };

    float temp = std::numeric_limits<float>::infinity();
    float humidity = std::numeric_limits<float>::infinity();;
    time_t last_update = 0;

    const time_t store_interval = 60;

    while(1) {
        long timeout = calc_timeout(last_update, store_interval);
        bool ready = util::poll(items, timeout);

        if (ready) {
            auto update = wire::sensor_update::receive(sensor_updates);
            if (update.type == 0x01) {
                temp = update.value;
            } else if (update.type == 0x02) {
                humidity = update.value;
            } else {
                std::cerr << "[W] got update of unkown type" << std::endl;
            }
        }

        time_t now = std::time(nullptr);
        if (now - last_update > store_interval) {
            db.put_data(static_cast<uint32_t>(now), temp, humidity);
            last_update = now;
        }
    }


    return 0;
}