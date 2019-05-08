#!/usr/bin/env bash

export ProjectRoot=$(pwd)

thirdparty::curl() {
  echo "Build curl..."
  echo "install openssl lib..."
  sudo apt install libssl-dev
  cd ${ProjectRoot}
  cd third-party/curl
  ./buildconf
  ./configure
  make
}

thirdparty::cJSON() {
  echo "Build cJSON..."
  cd ${ProjectRoot}
  cd third-party/cJSON
  rm -rf build && mkdir -p build && cd build
  cmake -DENABLE_CJSON_UTILS=On .. && make
}

thirdparty::argparse() {
  echo "Build argparse..."
  cd ${ProjectRoot}
  cd third-party/argparse
  make
}

#thirdparty::libucw() {
#  echo "Build libUCW..."
#  cd ${ProjectRoot}
#  cd third-party/libucw
##  ./configure CONFIG_LOCAL && make
#  ./configure -CONFIG_UCW_PERL -CONFIG_XML -CONFIG_JSON CONFIG_DEBUG CONFIG_LOCAL \
#	                -CONFIG_SHARED -CONFIG_DOC -CONFIG_CHARSET && \
#    make runtree libs api
#}

thirdparty::git::update() {
  echo "Update submodule..."
  cd ${ProjectRoot}
  git submodule update --init
}

main() {
   # TODO add toolchain ENV setting up
  thirdparty::git::update
  thirdparty::curl
#  thirdparty::libucw
  thirdparty::cJSON
  thirdparty::argparse
}

(( $_s_ )) || main
