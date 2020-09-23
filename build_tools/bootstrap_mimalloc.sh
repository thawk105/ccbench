# prepare for mimalloc
cd third_party/mimalloc
mkdir -p out/release
cd out/release
cmake -DCMAKE_BUILD_TYPE=Release ../..
make clean all -j

cd ../
mkdir debug
cd debug
cmake -DCMAKE_BUILD_TYPE=Debug ../..
make clean all -j

cd ../
mkdir secure
cd secure
cmake -DMI_SECURE=ON ../..
make clean all -j

# prepare for tbb
cd ../../../tbb
make clean all -j

cd ../../
