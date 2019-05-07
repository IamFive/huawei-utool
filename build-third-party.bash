#!/usr/bin/env bash

export ProjectRoot=$(pwd)

thirdparty::curl() {
  echo "Build curl..."
  cd ${ProjectRoot}
  cd third-party/curl
  ./buildconf
  ./configure --without-ssl
  make
}

thirdparty::cJSON() {
  echo "Build cJSON..."
  cd ${ProjectRoot}
  cd third-party/cJSON
  mkdir -p build && cd build
  cmake .. && make
}

thirdparty::libucw() {
  echo "Build libUCW..."
  cd ${ProjectRoot}
  cd third-party/libucw
#  ./configure CONFIG_LOCAL && make
  ./configure -CONFIG_UCW_PERL -CONFIG_XML -CONFIG_JSON CONFIG_DEBUG CONFIG_LOCAL \
	                -CONFIG_SHARED -CONFIG_DOC -CONFIG_CHARSET && \
    make runtree libs api
}

thirdparty::git::update() {
  echo "Update submodule..."
  cd ${ProjectRoot}
  git submodule update --init
}

main() {
   # TODO add toolchain ENV setting up
  thirdparty::git::update
  thirdparty::curl
  thirdparty::libucw
  thirdparty::cJSON
}

(( $_s_ )) || main
