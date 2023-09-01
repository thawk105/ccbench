cmake_minimum_required(VERSION 3.10)

project(ccbench_ermia
        VERSION 0.0.1
        DESCRIPTION "ermia of ccbench rewrited by Takada"
        LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")

option(ENABLE_SANITIZER "enable sanitizer on debug build" ON)
option(ENABLE_UB_SANITIZER "enable undefined behavior sanitizer on debug build" OFF)
option(ENABLE_COVERAGE "enable coverage on debug build" OFF)

#find_package(Doxygen)
find_package(Threads REQUIRED)
find_package(gflags REQUIRED)
find_package(glog REQUIRED)
find_package(Boost COMPONENTS filesystem)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CompileOptions)

#if you want to execute SI+SSN
#file(GLOB ERMIA_SOURCES "frame.cc" "ssn.cc" "si.cc")

#if you want to execute RC+SSN
#file(GLOB ERMIA_SOURCES "frame.cc" "ssn.cc" "rc.cc")

#if you want to execute RCL+SSN
file(GLOB ERMIA_SOURCES "frame.cc" "ssn.cc" "rcl.cc")


add_executable(ermia.exe ${ERMIA_SOURCES})

target_link_libraries(ermia.exe
        Boost::filesystem
        gflags::gflags
        ${PROJECT_SOURCE_DIR}/../third_party/mimalloc/out/release/libmimalloc.a
        Threads::Threads
        )
 
add_definitions(-DTIDFLAG=1)
add_definitions(-DCACHE_LINE_SIZE=64)
add_definitions(-DPAGE_SIZE=4096)
add_definitions(-Dclocks_per_us=2100)   #CPU_MHz. Use this info for measuring time
add_definitions(-Dextime=3)             #Execution time[sec]
add_definitions(-Dmax_ope=20)           #Total number of operations per single transaction
add_definitions(-Dmax_ope_readonly=100)  #Total number of operations per read only transaction
add_definitions(-Dronly_ratio=40)        #read-only transaction rate
add_definitions(-Drratio=40)            #read ratio of single transaction
add_definitions(-Dthread_num=4)        #Total number of records
add_definitions(-Dtuple_num=10000)       #Total number of records
add_definitions(-Dskew=0.99)               #skew(0~0.99)
add_definitions(-DUSE_LOCK=0)           #read-only lock flag "0 = dont use, 1 = use"
