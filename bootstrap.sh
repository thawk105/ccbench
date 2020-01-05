git submodule init
git submodule update

# prepare for masstree
cd third_party/masstree
./bootstrap.sh
./configure
make -j

# prepare for mimalloc
cd ../mimalloc
mkdir -p out/release
cd out/release
cmake ../..
make -j

cd ../
mkdir debug
cd debug
cmake -DCMAKE_BUILD_TYPE=Debug ../..
make -j

cd ../
mkdir secure
cd secure
cmake -DMI_SECURE=ON ../..
make -j

# prepare for tbb
cd ../../../tbb
make -j

cd ../../

# setting path, so this script should be executed by "source" command.
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PWD/third_party/mimalloc/out/release
