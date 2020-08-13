#include "../include/random.hh"
#include "tpcc_initializer.hpp"

int main(){
    Xoroshiro128Plus rnd;

    TPCC::Initializer::load(1);
}
