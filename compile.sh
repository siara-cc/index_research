if [ ! -d "build" ]; then
    mkdir build
fi
export COMPILE_OPTS="-O3 -I./hdr -I./src"
g++ $COMPILE_OPTS -c -o build/huge_db.o ./huge_db.cpp
g++ $COMPILE_OPTS -c -o build/art.o ./art.cpp
g++ $COMPILE_OPTS -c -o build/rb_tree.o ./rb_tree.cpp
g++ $COMPILE_OPTS -c -o build/dfox.o ./dfox.cpp
g++ $COMPILE_OPTS -c -o build/dfos.o ./dfos.cpp
g++ $COMPILE_OPTS -c -o build/dfqx.o ./dfqx.cpp
g++ $COMPILE_OPTS -c -o build/bfos.o ./bfos.cpp
g++ $COMPILE_OPTS -c -o build/octp.o ./octp.cpp
g++ $COMPILE_OPTS -c -o build/bfqs.o ./bfqs.cpp
g++ $COMPILE_OPTS -c -o build/bft.o ./bft.cpp
g++ $COMPILE_OPTS -c -o build/dft.o ./dft.cpp
g++ $COMPILE_OPTS -c -o build/GenTree.o ./GenTree.cpp
g++ $COMPILE_OPTS -c -o build/linex.o ./linex.cpp
g++ $COMPILE_OPTS -c -o build/basix.o ./basix.cpp
g++ $COMPILE_OPTS -c -o build/univix_util.o ./univix_util.cpp
g++ $COMPILE_OPTS -c -o build/lru_cache.o ./lru_cache.cpp
cd build
g++ $COMPILE_OPTS -o ../huge_db rb_tree.o basix.o huge_db.o dfox.o dfos.o dfqx.o linex.o bfos.o octp.o bfqs.o dft.o bft.o art.o GenTree.o univix_util.o lru_cache.o
