#apt  update -y && apt install -y libgflags-dev libgoogle-glog-dev libboost-filesystem-dev

cd ../
./bootstrap.sh
./bootstrap_apt.sh
./bootstrap_mimalloc.sh
./bootstrap_googletest.sh

cd silo_ext
mkdir -p build
cd build
cmake ..
make -j
