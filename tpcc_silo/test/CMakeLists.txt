file(GLOB SILO_SOURCES
        "${PROJECT_SOURCE_DIR}/../common/util.cc"
        "${PROJECT_SOURCE_DIR}/epoch.cpp"
        "${PROJECT_SOURCE_DIR}/garbage_collection.cpp"
        "${PROJECT_SOURCE_DIR}/interface/interface_delete.cpp"
        "${PROJECT_SOURCE_DIR}/interface/interface_helper.cpp"
        "${PROJECT_SOURCE_DIR}/interface/interface_scan.cpp"
        "${PROJECT_SOURCE_DIR}/interface/interface_search.cpp"
        "${PROJECT_SOURCE_DIR}/interface/interface_termination.cpp"
        "${PROJECT_SOURCE_DIR}/interface/interface_update_insert.cpp"
        "${PROJECT_SOURCE_DIR}/log.cpp"
        "${PROJECT_SOURCE_DIR}/scheme.cpp"
        "${PROJECT_SOURCE_DIR}/session_info.cpp"
        "${PROJECT_SOURCE_DIR}/session_info_table.cpp"
        "${PROJECT_SOURCE_DIR}/tid.cpp"
        "${PROJECT_SOURCE_DIR}/index/masstree_beta/masstree_beta_wrapper.cpp"
        )

file(GLOB TEST_SOURCES
        "aligned_allocator_test.cpp"
        "scheme_global_test.cpp"
        "tpcc_initializer_test.cpp"
        "tpcc_tables_test.cpp"
        "tpcc_util_test.cpp"
        "unit_test.cpp"
        )

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    add_definitions(-DCCBENCH_LINUX)
endif ()

cmake_host_system_information(RESULT cores QUERY NUMBER_OF_LOGICAL_CORES)
add_definitions(-DKVS_EPOCH_TIME=40)
add_definitions(-DKVS_NUMBER_OF_LOGICAL_CORES=${cores})
add_definitions(-DKVS_MAX_PARALLEL_THREADS=500)
add_definitions(-DKVS_MAX_KEY_LENGTH=1000)
add_definitions(-DKVS_LOG_GC_THRESHOLD=1)
add_definitions(-DPROJECT_ROOT=${PROJECT_SOURCE_DIR})

foreach (src IN LISTS TEST_SOURCES)
    get_filename_component(fname "${src}" NAME_WE)
    if (fname MATCHES "test$")
        set(test_name "${fname}.exe")

        if ((fname MATCHES "unit_test$") OR
        (fname MATCHES "tpcc_initializer_test"))
            add_executable(${test_name} ${src} ${SILO_SOURCES})

            target_include_directories(${test_name}
                    PRIVATE ${PROJECT_SOURCE_DIR}
                    PRIVATE ${PROJECT_SOURCE_DIR}/../third_party
                    PRIVATE ${PROJECT_SOURCE_DIR}/../third_party/googletest/googletest/include
                    PRIVATE ${PROJECT_SOURCE_DIR}/include
                    PRIVATE ${PROJECT_SOURCE_DIR}/index/masstree_beta/include
                    PRIVATE ${PROJECT_SOURCE_DIR}/interface
                    PRIVATE ${PROJECT_SOURCE_DIR}/tpcc
                    )

            target_link_libraries(${test_name}
                    Boost::filesystem
                    gflags::gflags
                    ${PROJECT_SOURCE_DIR}/../third_party/mimalloc/out/release/libmimalloc.a
                    ${PROJECT_SOURCE_DIR}/../third_party/masstree/libkohler_masstree_json.a
                    ${PROJECT_SOURCE_DIR}/../third_party/googletest/build/lib/libgtest.a
                    ${PROJECT_SOURCE_DIR}/../third_party/googletest/build/lib/libgtest_main.a
                    Threads::Threads
                    )
        else ()
            add_executable(${test_name} ${src})

            target_include_directories(${test_name}
                    PRIVATE ${PROJECT_SOURCE_DIR}
                    PRIVATE ${PROJECT_SOURCE_DIR}/../third_party/googletest/googletest/include
                    PRIVATE ${PROJECT_SOURCE_DIR}/include
                    )

            target_link_libraries(${test_name}
                    gflags::gflags
                    Threads::Threads
                    ${PROJECT_SOURCE_DIR}/../third_party/googletest/build/lib/libgtest.a
                    ${PROJECT_SOURCE_DIR}/../third_party/googletest/build/lib/libgtest_main.a
                    )

        endif ()
        set_compile_options(${test_name})
        add_test(
                NAME ${test_name}
                COMMAND ${test_name} --gtest_output=xml:${test_name}_gtest_result.xml)
    endif ()
endforeach ()
