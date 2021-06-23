cmake_minimum_required(VERSION 3.5.0)
set(LIB_NAME utool)
project(${LIB_NAME} C)

set(CMAKE_SH "CMAKE_SH-NOTFOUND")
set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
            STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
            "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
    set(CMAKE_BUILD_TYPE "Release")
endif()

message("CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")

#execute_process(COMMAND lsb_release -is
#        OUTPUT_VARIABLE RELEASE_CODENAME
#        OUTPUT_STRIP_TRAILING_WHITESPACE
#        )

set(ALLOW_DUPLICATE_CUSTOM_TARGETS TRUE)
set(CMAKE_C_STANDARD 99)
set(CMAKE_VERBOSE_MAKEFILE ON)

IF(NOT CMAKE_BUILD_TYPE MATCHES Debug)
    if(CMAKE_C_COMPILER_VERSION VERSION_GREATER 4.9)
        set(PROTECTOR strong)
    else()
        set(PROTECTOR all)
    endif()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -fstack-protector-${PROTECTOR} -Wl,-z,relro -Wl,-z,now ")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-z,noexecstack -s -D_FORTIFY_SOURCE=2 -O2")
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DZF_LOG_BUF_SZ=1024 -DZF_LOG_MEM_WIDTH=64")

set(PROJECT_VERSION_MAJOR 1)
set(PROJECT_VERSION_MINOR 0)
set(PROJECT_VERSION_PATCH 0)
set(UTOOL_SO_VERSION 1)
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")


set(MINGW_64_PATH "d:/tools/msys2/mingw64")
#set(MINGW_64_PATH "/d/tools/msys2/mingw64")
set(CYGWIN64_BIN_PATH "/cygdrive/c/cygwin64/bin")
set(UTOOL_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/src/include)
set(UTOOL_BIN_DIR ${PROJECT_SOURCE_DIR}/bin)
set(UTOOL_LIB_DIR ${PROJECT_SOURCE_DIR}/lib)

IF (WIN32)
    message("Compile env is window, will use mingw64 to compile utool.")
    message("MinGW 64 path is ${MINGW_64_PATH}.")
endif()

set(CJSON_PATH third-party/cJSON)
set(LIBARGPARSE_PATH third-party/argparse)
set(LIBZFLOG_PATH third-party/zf_log/zf_log)
set(LIBBCF_PATH third-party/bounds_checking_function)
set(LIBCURL_PATH third-party/curl)
IF (WIN32)
    set(IPMITOOL_BIN_PATH third-party/ipmi/ipmitool.exe)
else()
    set(IPMITOOL_BIN_PATH third-party/ipmi/ipmitool)
endif()

## use, i.e. don't skip the full RPATH for the build tree
SET(CMAKE_SKIP_BUILD_RPATH  FALSE)
# when building, don't use the install RPATH already
# (but later on when installing)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
# the RPATH to be used when installing
SET(CMAKE_INSTALL_RPATH "\$ORIGIN/libs")
# don't add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)


#find_package(CURL REQUIRED)
#include_directories(${CURL_INCLUDE_DIR})

# use relatived link path
#cmake_policy(SET CMP0015 OLD)
#cmake_policy(SET CMP0081 OLD)

message(STATUS "Copying libcurl shared libraries.")
set(ThirdPartyLibPath "${CMAKE_CURRENT_BINARY_DIR}/libs")
file(COPY "${LIBCURL_PATH}/lib/.libs/" DESTINATION "${ThirdPartyLibPath}")
#configure_file("${LIBCURL_PATH}/lib/.libs/libcurl.so.4.5.0" "${ThirdPartyLibPath}/libcurl.so.4.5.0" COPYONLY)


include_directories(
        ${UTOOL_INCLUDE_DIR}
        ${LIBBCF_PATH}/include
        ${LIBARGPARSE_PATH}
        ${CJSON_PATH}
        ${LIBZFLOG_PATH}
        ${LIBCURL_PATH}/include
)

#link_directories(
#        "${LIBCURL_PATH}/lib/.libs"
#)

link_libraries(
        curl
)

file(GLOB THIRD_PARTY_SOURCE
        ${LIBBCF_PATH}/src/*.c
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
configure_file(${IPMITOOL_BIN_PATH} "${CMAKE_BINARY_DIR}/ipmitool" COPYONLY)

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
set_target_properties(${LIB_NAME} PROPERTIES LINK_FLAGS "-Wl,-rpath,'$ORIGIN/libs' -L./libs")
#install(TARGETS ${LIB_NAME} DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")
install(TARGETS ${LIB_NAME} DESTINATION "${UTOOL_LIB_DIR}")
install(TARGETS ${LIB_NAME} DESTINATION "${UTOOL_BIN_DIR}")

# static library
add_library(${LIB_NAME}-static STATIC ${SOURCE_FILES})
set_target_properties(${LIB_NAME}-static PROPERTIES OUTPUT_NAME ${LIB_NAME})
set_target_properties(${LIB_NAME}-static PROPERTIES LINKER_LANGUAGE C)
set_target_properties("${LIB_NAME}-static"
        PROPERTIES
        SOVERSION "${UTOOL_SO_VERSION}"
        VERSION "${PROJECT_VERSION}")
install(TARGETS ${LIB_NAME}-static DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")
install(TARGETS ${LIB_NAME}-static DESTINATION "${UTOOL_LIB_DIR}")

macro(install_symlink filepath sympath)
    install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${filepath} ${sympath})")
    install(CODE "message(\"-- Created symlink: ${sympath} -> ${filepath}\")")
endmacro(install_symlink)

# build & install executable utool bin
add_executable(${LIB_NAME}-bin src/main.c ${SOURCE_FILES})
set_target_properties(${LIB_NAME}-bin PROPERTIES OUTPUT_NAME ${LIB_NAME})
set_target_properties(${LIB_NAME}-bin PROPERTIES LINK_FLAGS "-Wl,-rpath,'$ORIGIN/libs' -L./libs")
target_compile_options(${LIB_NAME}-bin PRIVATE -fPIE -pie)
install(TARGETS ${LIB_NAME}-bin DESTINATION ${UTOOL_BIN_DIR})

# debug purpose, should not present in release
IF(CMAKE_BUILD_TYPE MATCHES Debug)
    add_executable(${LIB_NAME}-debug src/main-debug.c ${SOURCE_FILES})
    set_target_properties(${LIB_NAME}-debug PROPERTIES LINK_FLAGS "-Wl,-rpath,'$ORIGIN/libs' -L./libs")
    #install(TARGETS utool-debug DESTINATION ${UTOOL_BIN_DIR})
ENDIF()

#install(TARGETS utool-bin DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(FILES "${IPMITOOL_BIN_PATH}" DESTINATION "${UTOOL_BIN_DIR}")

# install libraries
IF (WIN32)
    # libcurl
    install(FILES "${LIBCURL_PATH}/lib/.libs/libcurl-4.dll" DESTINATION "${UTOOL_BIN_DIR}")
    #install(FILES "${LIBCURL_PATH}/lib/.libs/libcurl.dll.a" DESTINATION "${UTOOL_BIN_DIR}")
    #install(FILES "${LIBCURL_PATH}/lib/.libs/libcurl.a" DESTINATION "${UTOOL_BIN_DIR}")

    #libwinpthread
    #install(FILES "${MINGW_64_PATH}/x86_64-w64-mingw32/lib/libwinpthread.a" DESTINATION "${UTOOL_BIN_DIR}")
    #install(FILES "${MINGW_64_PATH}/x86_64-w64-mingw32/lib/libwinpthread.dll.a" DESTINATION "${UTOOL_BIN_DIR}")
    install(FILES "${MINGW_64_PATH}/bin/libwinpthread-1.dll" DESTINATION "${UTOOL_BIN_DIR}")

    # zlib1
    install(FILES "${MINGW_64_PATH}/bin/zlib1.dll" DESTINATION "${UTOOL_BIN_DIR}")
else()
    install(FILES "${LIBCURL_PATH}/lib/.libs/libcurl.so.4.6.0" DESTINATION "${UTOOL_BIN_DIR}/libs/")
    install_symlink("./libcurl.so.4.6.0" "../bin/libs/libcurl.so.4")
    install_symlink("./libcurl.so.4.6.0" "../bin/libs/libcurl.so")
ENDIF()