if(TARGET glog::glog)
    return()
endif()

find_library(glog_LIBRARY_FILE NAMES glog)
find_path(glog_INCLUDE_DIR NAMES glog/logging.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(glog DEFAULT_MSG
    glog_LIBRARY_FILE
    glog_INCLUDE_DIR)

if(glog_LIBRARY_FILE AND glog_INCLUDE_DIR)
    set(glog_FOUND ON)
    add_library(glog::glog SHARED IMPORTED)
    set_target_properties(glog::glog PROPERTIES
        IMPORTED_LOCATION "${glog_LIBRARY_FILE}"
        INTERFACE_INCLUDE_DIRECTORIES "${glog_INCLUDE_DIR}")
else()
    set(glog_FOUND OFF)
endif()

unset(glog_LIBRARY_FILE CACHE)
unset(glog_INCLUDE_DIR CACHE)
