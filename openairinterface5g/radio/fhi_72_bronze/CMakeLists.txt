# use env var PKG_CONFIG_PATH to override paths to libdpdk.pc
pkg_check_modules(dpdk REQUIRED libdpdk)
pkg_check_modules(numa REQUIRED numa)

find_package(xran 2.1.1 EXACT REQUIRED) # B release -> 2

# Ignore xran-specific warning: we don't care/can't change the following warning, so suppress
# alignment 1 of 'struct XYZ' is less than 2
add_compile_options(-Wno-packed-not-aligned)

add_library(oran_fhlib_5g_b MODULE
	    sample-app-bronze.c
	    common_bronze.c
	    config_bronze.c
            oran_isolate.c
            oaioran.c
)

set_target_properties(oran_fhlib_5g_b PROPERTIES COMPILE_FLAGS "-fvisibility=hidden -march=native -DORAN_BRONZE")
target_link_libraries(oran_fhlib_5g_b PRIVATE xran::xran)
#target_link_libraries(oran_fhlib_5g_b PRIVATE ${dpdk_STATIC_LDFLAGS})
target_link_libraries(oran_fhlib_5g_b PRIVATE ${dpdk_LINK_LIBRARIES})
target_include_directories(oran_fhlib_5g_b PRIVATE ${dpdk_INCLUDE_DIRS})
target_link_libraries(oran_fhlib_5g_b PRIVATE pthread dl rt m numa)
target_link_libraries(oran_fhlib_5g_b PRIVATE ${T_LIB})

# TODO: can be removed?
target_include_directories(oran_fhlib_5g_b PRIVATE ${xran_INCLUDE_DIRS}/../src/)

set_target_properties(oran_fhlib_5g_b PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_custom_command(TARGET oran_fhlib_5g_b POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E create_symlink liboran_fhlib_5g_b.so liboai_transpro.so
                   WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
