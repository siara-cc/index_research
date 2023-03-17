if [ ! -d "build" ]; then
    mkdir build
fi
export CPP_STD=-std=c++11
export COMPILE_OPTS="-O3 -I./hdr -I./src -I../bloom/src"
g++ $CPP_STD $COMPILE_OPTS -c -o build/imain.o ./src/imain.cpp
g++ $CPP_STD $COMPILE_OPTS -c -o build/art.o ./src/art.cpp
g++ $CPP_STD $COMPILE_OPTS -c -o build/rb_tree.o ./src/rb_tree.cpp
g++ $CPP_STD $COMPILE_OPTS -c -o build/univix_util.o ./src/univix_util.cpp
gcc $COMPILE_OPTS -c -o build/bloom.o ../bloom/src/bloom.c
cd build
g++ $COMPILE_OPTS -o imain rb_tree.o imain.o art.o univix_util.o bloom.o
