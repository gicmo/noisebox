
#include <wire.h>

#include <iostream>
#include <iostream>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <thread>
#include <deque>
#include <cmath>
#include <atomic>

#include <signal.h>

#include <db_cxx.h>
#include <zmq.h>

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
            : db_env(0), db(&db_env, 0) {

        uint32_t open_flags = DB_CREATE | DB_THREAD;
        uint32_t env_flags = DB_CREATE | DB_THREAD | DB_INIT_MPOOL | DB_INIT_LOCK | DB_INIT_LOG;

        try {
            db_env.open(path.c_str(), env_flags, 0);

            db.set_bt_compare(store::compare_epoch);
            db.set_flags(DB_RECNUM);
            db.open(nullptr, "data.db", nullptr, DB_BTREE, open_flags, 0);
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
    DbEnv db_env;
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

namespace worker {

static void db_writer(zmq::context_t &zmq_ctx, store &db)
{
    zmq::socket_t hangman(zmq_ctx, ZMQ_SUB);
    hangman.connect("inproc://hangman");
    hangman.setsockopt(ZMQ_SUBSCRIBE, "", 0);

    zmq::socket_t cache(zmq_ctx, ZMQ_REQ);
    cache.connect("inproc://data/current");

    zmq::socket_t service(zmq_ctx, ZMQ_REP);
    service.bind("inproc://data/control");

    zmq_pollitem_t items[] = {
            {static_cast<void *>(service), 0, ZMQ_POLLIN, 0},
            {static_cast<void *>(hangman), 0, ZMQ_POLLIN, 0}
    };

    time_t last_write = 0;
    //bool should_record = true;
    const time_t write_interval = 60;

    while (1) {
        long timeout = calc_timeout(last_write, write_interval);
        bool ready = util::poll(items, timeout);

        if (items[0].revents & ZMQ_POLLIN) {
            //implement control functionality here
            std::cerr << "Warning, got unhandled control message" << std::endl;
        }

        if (items[1].revents & ZMQ_POLLIN) {
            zmq::message_t msg;
            hangman.recv(&msg);
            //currently the only message we can get is to stop
            return;
        }

        time_t now = std::time(nullptr);
        if (now - last_write > write_interval) {

            //get the current temp data
            util::send_empty(cache);
            Json::Value js;

            bool res = util::receive_json(cache, js);
            assert(res);

            float temp = js["temperature"].asFloat();
            float humidity = js["humidity"].asFloat();

            db.put_data(static_cast<uint32_t>(now), temp, humidity);

            last_write = now;
        }

    }

}

static void data_cache(zmq::context_t &zmq_ctx) {
    zmq::socket_t service(zmq_ctx, ZMQ_REP);
    service.bind("inproc://data/current");

    zmq::socket_t update_radio(zmq_ctx, ZMQ_SUB);
    update_radio.connect("tcp://localhost:5556");
    update_radio.setsockopt(ZMQ_SUBSCRIBE, "", 0);

    zmq::socket_t hangman(zmq_ctx, ZMQ_SUB);
    hangman.connect("inproc://hangman");
    hangman.setsockopt(ZMQ_SUBSCRIBE, "", 0);

    float temp = std::numeric_limits<float>::infinity();
    float humidity = std::numeric_limits<float>::infinity();;

    zmq_pollitem_t items[] = {
            {static_cast<void *>(service), 0, ZMQ_POLLIN, 0},
            {static_cast<void *>(update_radio), 0, ZMQ_POLLIN, 0},
            {static_cast<void *>(hangman), 0, ZMQ_POLLIN, 0}
    };

    while (1) {
        bool ready = util::poll(items, -1);

        if (items[0].revents & ZMQ_POLLIN) {
            //TODO: switch to binary proto for better speed
            zmq::message_t request;
            service.recv(&request);

            Json::Value js;
            if (std::isfinite(temp) && std::isfinite(humidity)) {
                js["temperature"] = temp;
                js["humidity"] = humidity;
            }
            zmq::message_t msg = util::message_from_json(js);
            service.send(msg);
        }

        if (items[1].revents & ZMQ_POLLIN) {
            auto update = wire::sensor_update::receive(update_radio);
            if (update.type == 0x01) {
                temp = update.value;
            } else if (update.type == 0x02) {
                humidity = update.value;
            } else {
                std::cerr << "[W] got update of unkown type" << std::endl;
            }
        }

        if (items[2].revents & ZMQ_POLLIN) {
            zmq::message_t msg;
            hangman.recv(&msg);

            //currently the only message we can get
            //is to stop
            return;
        }
    }
}

static void slave(zmq::context_t &zmq_ctx, store &db)
{
    zmq::socket_t hangman(zmq_ctx, ZMQ_SUB);
    hangman.connect("inproc://hangman");
    hangman.setsockopt(ZMQ_SUBSCRIBE, "", 0);

    zmq::socket_t service(zmq_ctx, ZMQ_REQ);
    service.connect("inproc://data/store");

    zmq::socket_t cache(zmq_ctx, ZMQ_REQ);
    cache.connect("inproc://data/current");

    zmq_pollitem_t items[] = {
            {static_cast<void *>(hangman), 0, ZMQ_POLLIN, 0},
            {static_cast<void *>(service), 0, ZMQ_POLLIN, 0}
    };

    //message broker that we are ready to work
    util::send_string(service, "HELLO");

    while (1) {

        bool ready = util::poll(items);
        assert(ready);

        if (items[0].revents & ZMQ_POLLIN) {
            zmq::message_t msg;
            hangman.recv(&msg);
            //currently the only message we can get is to stop
            return;
        }

        if (items[1].revents & ZMQ_POLLIN) {

            std::string c_address = util::receive_string(service);
            util::receive_empty(service, true);
            zmq::message_t msg;
            service.recv(&msg);

            //lets hope msg.data() is properly aligned
            uint32_t *req_type = static_cast<uint32_t *>(msg.data());
            if (*req_type == 1) {
                //get the current temp data
                util::send_empty(cache);
                zmq::message_t data_in;
                cache.recv(&data_in);

                //send it to the client now
                util::send_string(service, c_address, true);
                util::send_empty(service, true);

                zmq::message_t data_out;
                data_out.copy(&data_in);
                service.send(data_out);

            } else if (*req_type == 2) {

                uint32_t t_start = *++req_type;
                uint32_t t_end   = *++req_type;
                std::cout << "S: " << t_start << " " << "E: " << t_end << std::endl;
                const auto records = db.find_data(t_start, t_end);
                std::cerr << "Got " << records.size() << "records" << std::endl;
                Json::Value array;
                for (const auto &record : records) {
                    Json::Value data_point;
                    data_point["timestamp"] = record.timestamp;
                    data_point["humidity"] = record.humidity();
                    data_point["temp"] = record.temperature();
                    array.append(data_point);
                }

                Json::Value js;
                js["data"] = array;

                zmq::message_t reply = util::message_from_json(js);

                //send it to the client now
                util::send_string(service, c_address, true);
                util::send_empty(service, true);
                service.send(reply);

            } else {
                std::cerr << "[W] unkown request" << std::endl;
            }
        }

    }

}

} //namespace worker


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
    std::cout << "noisebox store daemon" << std::endl;
    store db("data.db");
    zmq::context_t zmq_ctx(1);

    zmq::socket_t frontend(zmq_ctx, ZMQ_ROUTER);
    frontend.bind("tcp://*:5557");

    zmq::socket_t backend(zmq_ctx, ZMQ_ROUTER);
    backend.bind("inproc://data/store");

    zmq::socket_t hangman(zmq_ctx, ZMQ_PUB);
    hangman.bind("inproc://hangman");

    std::deque<std::string> workers;

    std::thread cache_thread(worker::data_cache, std::ref(zmq_ctx));

    zmq::socket_t cache(zmq_ctx, ZMQ_REQ);
    bool cache_is_ready = false;
    do {
        int res = zmq_connect (static_cast<void *>(cache), "inproc://data/current");
        cache_is_ready = !res;
    } while(!cache_is_ready);

    cache_is_ready = false;
    do {
        //get the current temp data
        util::send_empty(cache);
        Json::Value js;

        bool res = util::receive_json(cache, js);
        assert(res);

        cache_is_ready = js.isMember("temperature") && js.isMember("humidity");

        //lets wait for a second
        std::chrono::milliseconds dura(1000);
        std::this_thread::sleep_for(dura);

    } while (!cache_is_ready);
    std::cout << "cache is ready" << std::endl;

    std::thread db_thread(worker::db_writer, std::ref(zmq_ctx), std::ref(db));
    std::vector<std::thread> worker_threads;
    for (int i = 0; i < 5; i++) {
        worker_threads.emplace_back(worker::slave, std::ref(zmq_ctx), std::ref(db));
    }

    init_signals();

    zmq_pollitem_t items[] = {
            {static_cast<void *>(backend), 0, ZMQ_POLLIN, 0},
            {static_cast<void *>(frontend), 0, ZMQ_POLLIN, 0}
    };

    run_the_loop = true;

    while (run_the_loop) {

        size_t n_workers = workers.size();
        int n_items = 2;
        if (n_workers <  1) {
            n_items = 1;
            //manually clear revents in this case
            items[1].revents = 0;
        }
        int res = zmq_poll (items, n_items, -1);
        if (res < 1) {
            break;
        }

        if (items[0].revents & ZMQ_POLLIN) {
            std::string w_address = util::receive_string(backend);
            util::receive_empty(backend, true);
            std::string c_address = util::receive_string(backend);

            workers.push_back(w_address);

            if (c_address != "HELLO") {

                util::receive_empty(backend, true);
                zmq::message_t data_in;
                backend.recv(&data_in);

                util::send_string(frontend, c_address, true);
                util::send_empty(frontend, true);

                zmq::message_t data_out;
                data_out.copy(&data_in);
                frontend.send(data_out);

            }
        }

        if (items[1].revents & ZMQ_POLLIN) {
            std::string c_address = util::receive_string(frontend);
            util::receive_empty(frontend, true);
            zmq::message_t data_in;
            frontend.recv(&data_in);

            std::string w_address = workers.front();
            workers.pop_front();

            util::send_string(backend, w_address, true);
            util::send_empty(backend, true);
            util::send_string(backend, c_address, true);
            util::send_empty(backend, true);

            zmq::message_t data_out;
            data_out.copy(&data_in);
            backend.send(data_out);
        }

    }

    std::cerr << "Shutting down" << std::endl;
    zmq::message_t stop_msg;
    hangman.send(stop_msg);

    cache_thread.join();
    db_thread.join();

    for(auto &wth : worker_threads) {
        wth.join();
    }

    return 0;
}