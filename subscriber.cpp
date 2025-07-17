#include "subscriber.h"
#include <chrono>
#include <iostream>

Subscriber::Subscriber(const std::string& address)
    : context(1), socket(context, zmq::socket_type::sub), address(address), pool(2) {
    socket.connect(address);
    socket.set(zmq::sockopt::subscribe, "");
}

Subscriber::~Subscriber() {
    if (running) {
        stop();
    }
}

void Subscriber::start(MessageCallback cb) {
    if (running) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(callbackMutex); 
        callback = cb;
    }

    running = true;

    thread = std::thread([this]() {
        while (running) {
            zmq::message_t zmqMessage;

            bool hasMessage = false;
            std::string data;

            {
                std::lock_guard<std::mutex> lock(socketMutex);
                auto result = socket.recv(zmqMessage, zmq::recv_flags::dontwait);
                if (result.has_value()) {
                    hasMessage = true;
                    data = std::string(static_cast<char*>(zmqMessage.data()), zmqMessage.size());
                }
            }

            if (hasMessage) {
                Message message = Message::fromString(data);

                MessageCallback cb_copy;
                {
                    std::lock_guard<std::mutex> lock(callbackMutex);
                    cb_copy = callback;
                }

                boost::asio::post(pool, [cb_copy, message]() {
                    cb_copy(message);
                });
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
    });
}

void Subscriber::stop() {
    if (!running) {
        return;
    }

    running = false;

    if (thread.joinable()) {
        thread.join();
    }
}
