if [ ! -d "build" ]; then
    mkdir build
fi
export COMPILE_OPTS="-O3 -I./hdr -I./src"
g++ $COMPILE_OPTS -c -o build/imain.o ./src/imain.cpp
g++ $COMPILE_OPTS -c -o build/art.o ./src/art.cpp
g++ $COMPILE_OPTS -c -o build/rb_tree.o ./src/rb_tree.cpp
g++ $COMPILE_OPTS -c -o build/dfox.o ./src/dfox.cpp
g++ $COMPILE_OPTS -c -o build/dfos.o ./src/dfos.cpp
g++ $COMPILE_OPTS -c -o build/dfqx.o ./src/dfqx.cpp
g++ $COMPILE_OPTS -c -o build/bfos.o ./src/bfos.cpp
g++ $COMPILE_OPTS -c -o build/octp.o ./src/octp.cpp
g++ $COMPILE_OPTS -c -o build/bfqs.o ./src/bfqs.cpp
g++ $COMPILE_OPTS -c -o build/bft.o ./src/bft.cpp
g++ $COMPILE_OPTS -c -o build/dft.o ./src/dft.cpp
g++ $COMPILE_OPTS -c -o build/GenTree.o ./src/GenTree.cpp
g++ $COMPILE_OPTS -c -o build/linex.o ./src/linex.cpp
g++ $COMPILE_OPTS -c -o build/basix.o ./src/basix.cpp
g++ $COMPILE_OPTS -c -o build/univix_util.o ./src/univix_util.cpp
g++ $COMPILE_OPTS -c -o build/lru_cache.o ./src/lru_cache.cpp
cd build
g++ $COMPILE_OPTS -o imain rb_tree.o basix.o imain.o dfox.o dfos.o dfqx.o linex.o bfos.o octp.o bfqs.o dft.o bft.o art.o GenTree.o univix_util.o lru_cache.o
