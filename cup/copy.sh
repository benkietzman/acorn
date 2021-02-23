#!/bin/bash

prefix=/usr/local

if [ "${2}" != '' ]; then
  prefix=$2
fi

install $1 ${prefix}/acorn/cup/
