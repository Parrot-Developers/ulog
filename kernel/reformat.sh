#!/bin/bash

set -eux

allfiles=$(find . -name "*.cpp"\
              -or -name "*.cc"\
              -or -name "*.c"\
              -or -name "*.h"\
              -or -name "*.hpp"\
              -or -name "*.hh")

if [ ! -z "$(which clang-format)" ]; then
  cf_cmd="clang-format"
else
  # find the most recent installed version of clang-format
  cf_cmd=$(dpkg -l |grep -w clang-format | awk '{print $2}' |sort -V | tail -n1)
  if [ "${cf_cmd}" == "" ]; then
    echo "ERROR: clang-format not found"
    exit 1
  fi
fi

${cf_cmd} -i ${allfiles}
