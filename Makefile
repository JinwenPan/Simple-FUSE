CXX ?= g++
CXXFLAGS ?= -g -Wall -O2 -std=c++17

all:
	$(CXX) $(CXXFLAGS) -o memfs memfs.cpp `pkg-config fuse --cflags --libs`