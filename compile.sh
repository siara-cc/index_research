if [ ! -d "Release" ]; then
    mkdir Release
fi
if [ ! -d "Release/src" ]; then
    mkdir Release/src
fi
export COMPILE_OPTS="-O3 -I./hdr -I./src"
g++ $COMPILE_OPTS -c -o Release/src/huge_db.o ./src/huge_db.cpp
g++ $COMPILE_OPTS -c -o Release/src/art.o ./src/art.cpp
g++ $COMPILE_OPTS -c -o Release/src/rb_tree.o ./src/rb_tree.cpp
g++ $COMPILE_OPTS -c -o Release/src/dfox.o ./src/dfox.cpp
g++ $COMPILE_OPTS -c -o Release/src/dfos.o ./src/dfos.cpp
g++ $COMPILE_OPTS -c -o Release/src/dfqx.o ./src/dfqx.cpp
g++ $COMPILE_OPTS -c -o Release/src/bfos.o ./src/bfos.cpp
g++ $COMPILE_OPTS -c -o Release/src/octp.o ./src/octp.cpp
g++ $COMPILE_OPTS -c -o Release/src/bfqs.o ./src/bfqs.cpp
g++ $COMPILE_OPTS -c -o Release/src/bft.o ./src/bft.cpp
g++ $COMPILE_OPTS -c -o Release/src/dft.o ./src/dft.cpp
g++ $COMPILE_OPTS -c -o Release/src/GenTree.o ./src/GenTree.cpp
g++ $COMPILE_OPTS -c -o Release/src/linex.o ./src/linex.cpp
g++ $COMPILE_OPTS -c -o Release/src/basix.o ./src/basix.cpp
g++ $COMPILE_OPTS -c -o Release/src/univix_util.o ./src/univix_util.cpp
g++ $COMPILE_OPTS -c -o Release/src/lru_cache.o ./src/lru_cache.cpp
cd Release
g++ $COMPILE_OPTS -o ../huge_db src/rb_tree.o src/basix.o src/huge_db.o src/dfox.o src/dfos.o src/dfqx.o src/linex.o src/bfos.o src/octp.o src/bfqs.o src/dft.o src/bft.o src/art.o src/GenTree.o src/univix_util.o src/lru_cache.o
