cd ../
./bootstrap.sh
./bootstrap_mimalloc.sh
./bootstrap_googletest.sh

mkdir -p build
cd build
cmake ..
make -j
