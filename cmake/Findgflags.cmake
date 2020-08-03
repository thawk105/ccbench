if(TARGET gflags::gflags)
    return()
endif()

find_library(gflags_LIBRARY_FILE NAMES gflags)
find_path(gflags_INCLUDE_DIR NAMES gflags/gflags.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(gflags DEFAULT_MSG
    gflags_LIBRARY_FILE
    gflags_INCLUDE_DIR)

if(gflags_LIBRARY_FILE AND gflags_INCLUDE_DIR)
    set(gflags_FOUND ON)
    add_library(gflags::gflags SHARED IMPORTED)
    set_target_properties(gflags::gflags PROPERTIES
        IMPORTED_LOCATION "${gflags_LIBRARY_FILE}"
        INTERFACE_INCLUDE_DIRECTORIES "${gflags_INCLUDE_DIR}")
else()
    set(gflags_FOUND OFF)
endif()

unset(gflags_LIBRARY_FILE CACHE)
unset(gflags_INCLUDE_DIR CACHE)
