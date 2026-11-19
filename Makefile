CXX      ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Ithird_party

all: send recv

send: src/mold_send.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

recv: src/mold_recv.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ -pthread

clean:
	rm -f send recv
