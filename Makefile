SRC = src/main.cpp src/pubsub/publisher.cpp src/pubsub/subscriber.cpp
CC = g++
CFLAGS = -std=c++17 -pthread -I/opt/homebrew/include
LDFLAGS = -L/opt/homebrew/lib -lzmq -lboost_system

messaging_system: $(SRC)
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -o $@

clean:
	rm -f messaging_system
