C = gcc
CXXFLAGS = -pthread -march=native
CXX = g++
CXXFLAGS = -pthread -std=c++11 -march=native
OBJS = build/rb_tree.o build/basix.o build/imain.o build/dfox.o build/dfos.o build/dfqx.o build/linex.o build/bfos.o build/octp.o build/bfqs.o build/dft.o build/bft.o build/art.o build/GenTree.o build/univix_util.o build/lru_cache.o build/bloom.o
INCLUDES = -I./hdr -I./src -I../bloom/src

opt: CXXFLAGS += -O3 -funroll-loops -DNDEBUG
opt: imain

debug: CXXFLAGS += -g -O0 -fno-inline
debug: imain

imain: $(OBJS) src/imain.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OBJS) -o build/imain

clean:
	rm -rf build/*

build/imain.o: src/imain.cpp src/*.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c src/imain.cpp -o build/imain.o

build/art.o: src/art.cpp src/art.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c src/art.cpp -o build/art.o

build/rb_tree.o: src/rb_tree.cpp src/rb_tree.h src/bplus_tree_handler.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c src/rb_tree.cpp -o build/rb_tree.o

build/dfox.o: src/dfox.cpp src/dfox.h src/bplus_tree_handler.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c src/dfox.cpp -o build/dfox.o

build/dfos.o: src/dfos.cpp src/dfos.h src/bplus_tree_handler.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c src/dfos.cpp -o build/dfos.o

build/dfqx.o: src/dfqx.cpp src/dfqx.h src/bplus_tree_handler.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c src/dfqx.cpp -o build/dfqx.o

build/bfos.o: src/bfos.cpp src/bfos.h src/bplus_tree_handler.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c src/bfos.cpp -o build/bfos.o

build/octp.o: src/octp.cpp src/octp.h src/bplus_tree_handler.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c src/octp.cpp -o build/octp.o

build/bfqs.o: src/bfqs.cpp src/bfqs.h src/bplus_tree_handler.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c src/bfqs.cpp -o build/bfqs.o

build/bft.o: src/bft.cpp src/bft.h src/bplus_tree_handler.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c src/bft.cpp -o build/bft.o

build/dft.o: src/dft.cpp src/dft.h src/bplus_tree_handler.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c src/dft.cpp -o build/dft.o

build/GenTree.o: src/GenTree.cpp src/GenTree.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c src/GenTree.cpp -o build/GenTree.o

build/linex.o: src/linex.cpp src/linex.h src/bplus_tree_handler.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c src/linex.cpp -o build/linex.o

build/basix.o: src/basix.cpp src/basix.h src/bplus_tree_handler.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c src/basix.cpp -o build/basix.o

build/bas_blk.o: src/bas_blk.cpp src/bas_blk.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c src/bas_blk.cpp -o build/bas_blk.o

build/basix3.o: src/basix3.cpp src/basix3.h src/bplus_tree_handler.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c src/basix3.cpp -o build/basix3.o

build/univix_util.o: src/univix_util.cpp src/univix_util.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c src/univix_util.cpp -o build/univix_util.o

build/lru_cache.o: src/lru_cache.cpp src/lru_cache.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c src/lru_cache.cpp -o build/lru_cache.o

build/bloom.o: ../bloom/src/bloom.c ../bloom/src/bloom.h
	$(C) $(CFLAGS) $(INCLUDES) -c ../bloom/src/bloom.c -o build/bloom.o
