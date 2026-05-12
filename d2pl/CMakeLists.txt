include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CompileOptions)

file(GLOB SBOMB_D2PL_SOURCES
        "result.cc"
        "sbomb_d2pl.cc"
        "transaction.cc"
        "util.cc"
        )

file(GLOB DBOMB_D2PL_SOURCES
        "result.cc"
        "dbomb_d2pl.cc"
        "transaction.cc"
        "util.cc"
        )

add_executable(sbomb_d2pl.exe ${SBOMB_D2PL_SOURCES})
add_executable(bomb_d2pl.exe ${DBOMB_D2PL_SOURCES})


foreach(t sbomb_d2pl.exe bomb_d2pl.exe)
    target_link_libraries(${t} PRIVATE
            ccbench_common
            ccbench::masstree
            ccbench::mimalloc
            )
endforeach()
if (DEFINED ADD_ANALYSIS)
    add_definitions(-DADD_ANALYSIS=${ADD_ANALYSIS})
else ()
    add_definitions(-DADD_ANALYSIS=0)
endif ()

if (DEFINED BACK_OFF)
    add_definitions(-DBACK_OFF=${BACK_OFF})
else ()
    add_definitions(-DBACK_OFF=1)
endif ()

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
