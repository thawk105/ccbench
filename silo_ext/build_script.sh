cd ../
./bootstrap.sh
./bootstrap_mimalloc.sh
./bootstrap_googletest.sh

cd silo_ext
mkdir -p build
cd build
cmake ..
make -j
