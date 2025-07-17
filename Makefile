messaging_system: main.cpp publisher.cpp subscriber.cpp
	g++ -std=c++17 -pthread \
  -I/opt/homebrew/include \
  main.cpp publisher.cpp subscriber.cpp \
  -L/opt/homebrew/lib -lzmq -lboost_system \
  -o messaging_system

clean:
	rm -f messaging_system
