#include <iostream>
#include <thread>
#include <chrono>
#include "pubsub/message.h"
#include "pubsub/publisher.h"
#include "pubsub/subscriber.h"

using namespace std;

void printMessage(const Message& msg) {
    cout << "Topic: " << msg.getTopic() << endl;
    cout << "Content: " << msg.getContent() << endl;
    cout << endl;
}

int main() {
    const int MESSAGE_COUNT = 50;
    try {
        cout << "Setting up publisher" << endl;
        Publisher pub("tcp://*:5555");

        cout << "Setting up subscriber" << endl;
        Subscriber sub("tcp://localhost:5555");

        cout << "Starting subscriber" << endl;
        cout << endl;

        sub.start(printMessage);
        this_thread::sleep_for(chrono::seconds(1));

        cout << "Sending messages" << endl;
        for (int i=0; i<MESSAGE_COUNT; i++) {
            Message msg;
            msg.setTopic("Message #" + to_string(i));
            msg.setContent("I am message " + to_string(i));

            pub.send(msg);
        } 

        this_thread::sleep_for(chrono::seconds(1));
        sub.stop();
    } catch (exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
    return 0;
}
