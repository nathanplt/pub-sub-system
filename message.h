#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>
#include <iostream>

class Message {
    private:
        std::string topic, content;
    
    public:
        Message() {}

        Message(std::string topic, std::string content)
            : topic(topic), content(content) {}

        std::string getTopic() const { return topic;  }
        std::string getContent() const { return content;  }

        void setTopic(std::string topic) { this->topic = topic;  }
        void setContent(std::string content) { this->content = content;  }

        std::string toString() const {
            return topic + ":" + content;
        }

        static Message fromString(const std::string& s) {
            Message msg;

            int pos = s.find(':');
            if (pos >= 0) {
                msg.setTopic(s.substr(0, pos));
                msg.setContent(s.substr(pos + 1));
            } else {
                msg.setTopic("");
                msg.setContent(s);
            }

            return msg;
        }
};

#endif
