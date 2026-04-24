CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

all: mipssim

mipssim: main.o
	$(CXX) $(CXXFLAGS) -o mipssim main.o

main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c main.cpp

clean:
	rm -f mipssim *.o