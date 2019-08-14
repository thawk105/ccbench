git submodule init
git submodule update

cd third_party/masstree
./bootstrap.sh
./configure
make -j 224

cd ../mimalloc
mkdir -p out/release
cd out/release
cmake ../..
make -j 224

cd ../
mkdir debug
cd debug
cmake -DCMAKE_BUILD_TYPE=Debug ../..
make -j 224

cd ../
mkdir secure
cd secure
cmake -DMI_SECURE=ON ../..
make -j 224

cd ../../../../
