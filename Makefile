C = gcc
CXXFLAGS = -pthread -march=native
CXX = g++
CXXFLAGS = -pthread -std=c++11 -march=native -DMARISA_USE_POPCNT -mpopcnt -DMARISA_USE_BMI2 -DMARISA_WORD_SIZE=64 -mbmi2 -msse4
OBJS = build/rb_tree.o build/imain.o build/art.o build/univix_util.o build/bloom.o
INCLUDES = -I./hdr -I./src -I../bloom/src
L_FLAGS = -lsnappy -llz4 -lbrotlienc -lbrotlidec -lz -lmarisa -lsqlite3 -DMARISA_USE_POPCNT -mpopcnt -DMARISA_USE_BMI2 -DMARISA_WORD_SIZE=64 -mbmi2 -msse4

opt: CXXFLAGS += -g -O3 -funroll-loops -DNDEBUG
opt: imain

debug: CXXFLAGS += -g -O0 -fno-inline
debug: imain

imain: $(OBJS) src/imain.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OBJS) -o build/imain $(L_FLAGS)

clean:
	rm -rf build/*

build/imain.o: src/imain.cpp src/*.h ../madras-trie/src/*.hpp ../leopard-trie/src/*.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c src/imain.cpp -o build/imain.o

build/art.o: src/art.cpp src/art.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c src/art.cpp -o build/art.o

build/rb_tree.o: src/rb_tree.cpp src/rb_tree.h src/bplus_tree_handler.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c src/rb_tree.cpp -o build/rb_tree.o

build/univix_util.o: src/univix_util.cpp src/univix_util.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c src/univix_util.cpp -o build/univix_util.o

build/bloom.o: ../bloom/src/bloom.c ../bloom/src/bloom.h
	$(C) $(CFLAGS) $(INCLUDES) -c ../bloom/src/bloom.c -o build/bloom.o
