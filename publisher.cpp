#include "publisher.h"
#include <thread>
#include <chrono>
#include <iostream>

Publisher::Publisher(const std::string& address)
    : zmqContext(1), socket(zmqContext, zmq::socket_type::pub), address(address) {
        try {
            socket.bind(address);
        } catch (std::exception& e) {
            std::cerr << "Error binding socket: " << e.what() << std::endl;
            throw;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
}

void Publisher::send(const Message& message) {
    std::lock_guard<std::mutex> lock(sendMutex);
    std::string msg = message.toString();
    zmq::message_t zmqMessage(msg.size());

    memcpy(zmqMessage.data(), msg.c_str(), msg.size());

    auto result = socket.send(zmqMessage, zmq::send_flags::none);
    if (!result.has_value()) {
        std::cerr << "Failed to send message" << std::endl;
    }
}
