cmake_minimum_required(VERSION 3.5.0)
project(utool C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_VERBOSE_MAKEFILE ON)

set(UTOOL_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set(UTOOL_BIN_DIR ${PROJECT_SOURCE_DIR}/bin)
set(UTOOL_LIB_DIR ${PROJECT_SOURCE_DIR}/lib)

set(CJSON_PATH third-party/cJSON)
set(LIBCURL_PATH third-party/curl)
set(LIBUCW_PATH third-party/libucw)

include_directories(
        ${UTOOL_INCLUDE_DIR}
        ${CJSON_PATH}
        ${LIBCURL_PATH}/include
        ${LIBUCW_PATH}/run/include
)

link_directories(
        ${CJSON_PATH}
        ${LIBCURL_PATH}/lib/.libs
        ${LIBUCW_PATH}/run/lib
)


#link_libraries(
#        cjson
#        curl
#        libUCW
#)


# build & install utool lib
aux_source_directory(src/handlers SOURCE_FILES)
message(STATUS "Source files is ${SOURCE_FILES}")

add_library(utool-lib SHARED ${SOURCE_FILES})
set_target_properties(utool-lib PROPERTIES LINKER_LANGUAGE C)
install(TARGETS utool-lib DESTINATION ${UTOOL_LIB_DIR})

# build & install executable utool bin
add_executable(utool src/main.c ${HANDLER_SRCS})
install(TARGETS utool DESTINATION ${UTOOL_BIN_DIR})