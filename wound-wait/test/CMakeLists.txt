file(GLOB SS2PL_SOURCES
        "${PROJECT_SOURCE_DIR}/../common/result.cc"
        "${PROJECT_SOURCE_DIR}/../common/util.cc"
        "${PROJECT_SOURCE_DIR}/result.cc"
        "${PROJECT_SOURCE_DIR}/transaction.cc"
        "${PROJECT_SOURCE_DIR}/util.cc"
        )

file (GLOB TEST_SOURCES
"make_db_test.cpp"
)

if (DEFINED ADD_ANALYSIS)
    add_definitions(-DADD_ANALYSIS=${ADD_ANALYSIS})
else ()
    add_definitions(-DADD_ANALYSIS=0)
endif ()

if (DEFINED BACK_OFF)
    add_definitions(-DBACK_OFF=${BACK_OFF})
else ()
    add_definitions(-DBACK_OFF=0)
endif ()

add_definitions(-DDLR1)

if (DEFINED KEY_SIZE)
    add_definitions(-DKEY_SIZE=${KEY_SIZE})
else ()
    add_definitions(-DKEY_SIZE=8)
endif ()

if (DEFINED KEY_SORT)
    add_definitions(-DKEY_SORT=${KEY_SORT})
else ()
    add_definitions(-DKEY_SORT=0)
endif ()

if (DEFINED MASSTREE_USE)
    add_definitions(-DMASSTREE_USE=${MASSTREE_USE})
else ()
    add_definitions(-DMASSTREE_USE=1)
endif ()

if (DEFINED VAL_SIZE)
    add_definitions(-DVAL_SIZE=${VAL_SIZE})
else ()
    add_definitions(-DVAL_SIZE=4)
endif ()

foreach(src IN LISTS TEST_SOURCES)
    get_filename_component(fname "${src}" NAME_WE)
    set(test_name "${fname}")

    add_executable(${test_name} ${src} ${SS2PL_SOURCES})

    target_include_directories(${test_name}
            PRIVATE ${PROJECT_SOURCE_DIR}/../third_party/googletest/googletest/include
    )

    target_link_libraries(${test_name}
            Boost::filesystem
            gflags::gflags
            glog::glog
            ${PROJECT_SOURCE_DIR}/../third_party/mimalloc/out/release/libmimalloc.a
            ${PROJECT_SOURCE_DIR}/../third_party/masstree/libkohler_masstree_json.a
            Threads::Threads
            ${PROJECT_SOURCE_DIR}/../third_party/googletest/build/lib/libgtest.a
            ${PROJECT_SOURCE_DIR}/../third_party/googletest/build/lib/libgtest_main.a
            )
    set_compile_options(${test_name})
    add_test(
        NAME ${test_name}
        COMMAND ${test_name} --gtest_output=xml:${test_name}_gtest_result.xml
    )
endforeach()