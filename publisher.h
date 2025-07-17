#ifndef PUBLISHER_H
#define PUBLISHER_H

#include <zmq.hpp>
#include <string>
#include <mutex>
#include "message.h"

class Publisher {
    private:
        zmq::context_t zmqContext;
        zmq::socket_t socket;
        std::string address;
        std::mutex sendMutex;
    
    public:
        Publisher(const std::string& address);

        void send(const Message& message);
};

#endif
