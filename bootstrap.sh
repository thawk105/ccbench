git submodule init
git submodule update

# prepare for masstree
cd third_party/masstree
./bootstrap.sh
./configure
make

# prepare for mimalloc
cd ../mimalloc
mkdir -p out/release
cd out/release
cmake ../..
make

cd ../
mkdir debug
cd debug
cmake -DCMAKE_BUILD_TYPE=Debug ../..
make

cd ../
mkdir secure
cd secure
cmake -DMI_SECURE=ON ../..
make

cd ../../../../
