#apt  update -y && apt install -y libgflags-dev libgoogle-glog-dev libboost-filesystem-dev

pushd ../
./bootstrap.sh
./bootstrap_apt.sh
./bootstrap_mimalloc.sh
./bootstrap_googletest.sh

popd
mkdir -p build
cd build
cmake ..
make -j`grep '^processor' /proc/cpuinfo | wc -l`
