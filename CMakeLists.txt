cmake_minimum_required(VERSION 3.5.0)
set(LIB_NAME utool)

project(${LIB_NAME} C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_VERBOSE_MAKEFILE ON)

set(PROJECT_VERSION_MAJOR 1)
set(PROJECT_VERSION_MINOR 0)
set(PROJECT_VERSION_PATCH 0)
set(UTOOL_SO_VERSION 1)
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

set(UTOOL_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set(UTOOL_BIN_DIR ${PROJECT_SOURCE_DIR}/bin)
set(UTOOL_LIB_DIR ${PROJECT_SOURCE_DIR}/lib)

set(CJSON_PATH third-party/cJSON)
set(LIBARGPARSE_PATH third-party/argparse)
set(LIBCURL_PATH third-party/curl)
set(LIBZFLOG_PATH third-party/zf_log)
#set(LIBUCW_PATH third-party/libucw)

include_directories(
        ${UTOOL_INCLUDE_DIR}
        ${CJSON_PATH}
        ${LIBARGPARSE_PATH}
        ${LIBCURL_PATH}/include
        ${LIBZFLOG_PATH}/zf_log
        #        ${LIBUCW_PATH}/run/include
)

link_directories(
        ${CJSON_PATH}/build
        ${LIBARGPARSE_PATH}
        ${LIBCURL_PATH}/lib/.libs
        ${LIBZFLOG_PATH}/build/zf_log
        #        ${LIBUCW_PATH}/run/lib
)


link_libraries(
        argparse
        cjson
        cjson_utils
        curl
        zf_log
        #        ucw-6.5
)


aux_source_directory(src SOURCE_FILES)
aux_source_directory(src/commands SOURCE_FILES)
list(REMOVE_ITEM SOURCE_FILES "src/main.c")
list(REMOVE_ITEM SOURCE_FILES "src/main-debug.c")
message(STATUS "Source files is ${SOURCE_FILES}")

message(STATUS "Install dir prefix is: ${CMAKE_INSTALL_PREFIX}")

# header files
#aux_source_directory(include INCLUDE_FILES)
install(FILES include/utool.h DESTINATION "${CMAKE_INSTALL_PREFIX}/include/${LIB_NAME}")

# shared library
add_library(${LIB_NAME} SHARED ${SOURCE_FILES})
set_target_properties(${LIB_NAME} PROPERTIES OUTPUT_NAME ${LIB_NAME})
set_target_properties(${LIB_NAME} PROPERTIES LINKER_LANGUAGE C)
set_target_properties("${LIB_NAME}"
        PROPERTIES
        SOVERSION "${UTOOL_SO_VERSION}"
        VERSION "${PROJECT_VERSION}")
install(TARGETS ${LIB_NAME} DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")


# static library
add_library(${LIB_NAME}-static STATIC ${SOURCE_FILES})
set_target_properties(${LIB_NAME}-static PROPERTIES OUTPUT_NAME ${LIB_NAME})
set_target_properties(${LIB_NAME}-static PROPERTIES LINKER_LANGUAGE C)
install(TARGETS ${LIB_NAME}-static DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")

# build & install executable utool bin
add_executable(utool-bin src/main.c ${SOURCE_FILES})
install(TARGETS utool-bin DESTINATION ${UTOOL_BIN_DIR})

# TODO
# debug purpose, should remove before release
add_executable(utool-debug src/main-debug.c ${SOURCE_FILES})
install(TARGETS utool-debug DESTINATION ${UTOOL_BIN_DIR})

#add_executable(opt_test src/opt_test.c ${SOURCE_FILES})
#install(TARGETS opt_test DESTINATION ${UTOOL_BIN_DIR})