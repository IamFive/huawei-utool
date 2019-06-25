cmake_minimum_required(VERSION 3.5.0)
set(LIB_NAME utool)

project(${LIB_NAME} C)

set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
            STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
            "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

set(CMAKE_BUILD_TYPEc "Release")
message("CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")

#execute_process(COMMAND lsb_release -is
#        OUTPUT_VARIABLE RELEASE_CODENAME
#        OUTPUT_STRIP_TRAILING_WHITESPACE
#        )

set(ALLOW_DUPLICATE_CUSTOM_TARGETS TRUE)
set(CMAKE_C_STANDARD 99)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DZF_LOG_BUF_SZ=1024 -DZF_LOG_MEM_WIDTH=64")

set(PROJECT_VERSION_MAJOR 1)
set(PROJECT_VERSION_MINOR 0)
set(PROJECT_VERSION_PATCH 0)
set(UTOOL_SO_VERSION 1)
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

set(UTOOL_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/src/include)
set(UTOOL_BIN_DIR ${PROJECT_SOURCE_DIR}/bin)
set(UTOOL_LIB_DIR ${PROJECT_SOURCE_DIR}/lib)

set(CJSON_PATH third-party/cJSON)
set(LIBARGPARSE_PATH third-party/argparse)
set(LIBZFLOG_PATH third-party/zf_log/zf_log)
set(LIBCURL_PATH third-party/curl)
set(IPMITOOL_BIN_PATH third-party/ipmi/ipmitool)

#find_package(CURL REQUIRED)
#include_directories(${CURL_INCLUDE_DIR})

include_directories(
        ${UTOOL_INCLUDE_DIR}
        ${LIBARGPARSE_PATH}
        ${CJSON_PATH}
        ${LIBZFLOG_PATH}
        ${LIBCURL_PATH}/include
)

link_directories(
        ${LIBCURL_PATH}/lib/.libs
)

link_libraries(
        curl
)

file(GLOB THIRD_PARTY_SOURCE
        ${LIBARGPARSE_PATH}/argparse.c
        ${LIBZFLOG_PATH}/zf_log.c
        ${CJSON_PATH}/cJSON.c
        ${CJSON_PATH}/cJSON_Utils.c)

message(STATUS "third-party source files is ${THIRD_PARTY_SOURCE}")
aux_source_directory(src PRO_SOURCE_FILES)
aux_source_directory(src/commands PRO_SOURCE_FILES)
list(REMOVE_ITEM PRO_SOURCE_FILES "src/main.c")
list(REMOVE_ITEM PRO_SOURCE_FILES "src/main-debug.c")
list(APPEND SOURCE_FILES ${PRO_SOURCE_FILES} ${THIRD_PARTY_SOURCE})

message(STATUS "CMAKE binary dir is ${CMAKE_BINARY_DIR}")
message(STATUS "Source files is ${SOURCE_FILES}")
message(STATUS "Install dir prefix is: ${CMAKE_INSTALL_PREFIX}")

# copy ipmitool
configure_file(${IPMITOOL_BIN_PATH} ${CMAKE_BINARY_DIR} COPYONLY)

# header files
#aux_source_directory(include INCLUDE_FILES)
install(FILES src/include/utool.h DESTINATION "${CMAKE_INSTALL_PREFIX}/include/")

# shared library
add_library(${LIB_NAME} SHARED ${SOURCE_FILES})
#target_link_libraries(${LIB_NAME} curl)
set_target_properties(${LIB_NAME} PROPERTIES OUTPUT_NAME ${LIB_NAME})
set_target_properties(${LIB_NAME} PROPERTIES LINKER_LANGUAGE C)
set_target_properties("${LIB_NAME}"
        PROPERTIES
        SOVERSION "${UTOOL_SO_VERSION}"
        VERSION "${PROJECT_VERSION}")
install(TARGETS ${LIB_NAME} DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")
install(TARGETS ${LIB_NAME} DESTINATION "${UTOOL_LIB_DIR}")

# static library
add_library(${LIB_NAME}-static STATIC ${SOURCE_FILES})
set_target_properties(${LIB_NAME}-static PROPERTIES OUTPUT_NAME ${LIB_NAME})
set_target_properties(${LIB_NAME}-static PROPERTIES LINKER_LANGUAGE C)
install(TARGETS ${LIB_NAME}-static DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")
install(TARGETS ${LIB_NAME}-static DESTINATION "${UTOOL_LIB_DIR}")

# build & install executable utool bin
add_executable(${LIB_NAME}-bin src/main.c ${SOURCE_FILES})
set_target_properties(${LIB_NAME}-bin PROPERTIES OUTPUT_NAME ${LIB_NAME})
install(TARGETS utool-bin DESTINATION ${UTOOL_BIN_DIR})
install(TARGETS utool-bin DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)


IF(CMAKE_BUILD_TYPE MATCHES Debug)
    # debug purpose, should remove before release
    add_executable(utool-debug src/main-debug.c ${SOURCE_FILES})
    #install(TARGETS utool-debug DESTINATION ${UTOOL_BIN_DIR})
ENDIF()
