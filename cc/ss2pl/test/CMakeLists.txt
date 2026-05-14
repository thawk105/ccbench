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

# Build-time flags. The universal `-D` set comes from cmake/Options.cmake so
# the test target uses the same flags as the ss2pl protocol body; DLR1 and
# KEY_SORT are ss2pl-specific (mirrors cc/ss2pl/CMakeLists.txt).
ccbench_universal_definitions(SS2PL_TEST_DEFINITIONS)

foreach(src IN LISTS TEST_SOURCES)
    get_filename_component(fname "${src}" NAME_WE)
    set(test_name "${fname}")

    add_executable(${test_name} ${src} ${SS2PL_SOURCES})

    target_compile_definitions(${test_name}
            PRIVATE
            ${SS2PL_TEST_DEFINITIONS}
            DLR1
            KEY_SORT=${CCBENCH_KEY_SORT}
    )

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