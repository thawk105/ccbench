git submodule init
git submodule update

cd third_party/masstree
./bootstrap.sh
./configure
make -j224

cd ../mimalloc
mkdir -p out/release
cd out/release
cmake ../..
make -j 224

