cmake_minimum_required(VERSION 3.5.0)
project(utool C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_VERBOSE_MAKEFILE ON)

set(UTOOL_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set(UTOOL_BIN_DIR ${PROJECT_SOURCE_DIR}/bin)
set(UTOOL_LIB_DIR ${PROJECT_SOURCE_DIR}/lib)

set(CJSON_PATH third-party/cJSON)
set(LIBARGPARSE_PATH third-party/argparse)
set(LIBCURL_PATH third-party/curl)
#set(LIBUCW_PATH third-party/libucw)

include_directories(
        ${UTOOL_INCLUDE_DIR}
        ${CJSON_PATH}
        ${LIBARGPARSE_PATH}
        ${LIBCURL_PATH}/include
        #        ${LIBUCW_PATH}/run/include
)

link_directories(
        ${CJSON_PATH}/build
        ${LIBARGPARSE_PATH}
        ${LIBCURL_PATH}/lib/.libs
        #        ${LIBUCW_PATH}/run/lib
)


link_libraries(
        argparse
        cjson
        cjson_utils
        curl
        #        ucw-6.5
)


# build & install utool lib
aux_source_directory(src SOURCE_FILES)
aux_source_directory(src/commands SOURCE_FILES)
list(REMOVE_ITEM SOURCE_FILES "src/main.c")
list(REMOVE_ITEM SOURCE_FILES "src/main-debug.c")
message(STATUS "Source files is ${SOURCE_FILES}")

add_library(utool SHARED ${SOURCE_FILES})
set_target_properties(utool PROPERTIES LINKER_LANGUAGE C)
install(TARGETS utool DESTINATION ${UTOOL_LIB_DIR})

# build & install executable utool bin
add_executable(utool-bin src/main.c ${SOURCE_FILES})
install(TARGETS utool-bin DESTINATION ${UTOOL_BIN_DIR})

# TODO
# debug purpose, should remove before release
add_executable(utool-debug src/main-debug.c ${SOURCE_FILES})
install(TARGETS utool-debug DESTINATION ${UTOOL_BIN_DIR})

#add_executable(opt_test src/opt_test.c ${SOURCE_FILES})
#install(TARGETS opt_test DESTINATION ${UTOOL_BIN_DIR})