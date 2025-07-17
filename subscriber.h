#ifndef SUBSCRIBER_H
#define SUBSCRIBER_H

#include <zmq.hpp>
#include <boost/asio.hpp>
#include <string>
#include <atomic>
#include <thread>
#include <functional>
#include <mutex>
#include "message.h"

using MessageCallback = std::function<void(const Message&)>;

class Subscriber {
    private:
        zmq::context_t context;
        zmq::socket_t socket;
        std::string address;

        std::atomic<bool> running{false};

        std::thread thread;
        boost::asio::thread_pool pool;

        MessageCallback callback;
        std::mutex callbackMutex;
        std::mutex socketMutex;

    public:
        Subscriber(const std::string& address);
        ~Subscriber();

        void start(MessageCallback callback);

        void stop();

        bool isRunning();
};

#endif
