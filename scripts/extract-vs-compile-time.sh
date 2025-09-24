#!/usr/bin/env sh

# From a cmd prompt on a Visual Studio dev box, cd to root of pep/core repo and
#       mkdir build & cd build
#       ..\scripts\cmake-vs.bat .. ..\..\ops\keys ..\config\local ..\config\projects\gum Debug "" "-DVERIFY_HEADERS_STANDALONE=ON -DSHOW_COMPILE_TIME=ON -DCOMPILE_SEQUENTIALLY=ON"
#       msbuild pep.sln > msbuild.log
# Then from a WSL prompt, cd to the same (pep/core root) directory and
#       ./scripts/extract-vs-compile-time.sh build/msbuild.log
# The script also accepts Visual Studio (IDE) output, but VS builds projects (targets) in parallel by default, which may affect build timings.
# See the setting under Tools -> Options -> Projects and Solutions -> Build And Run.

set -o errexit
set -o nounset

processline() {
  line="$1"
  
  part=$(echo "$line" | awk -F")=" '{print $1}')
  tool="${part##*/}"
  printf "$tool\t"
  line=$(echo "$line" | awk -F")=" '{print $2}')
  
  time=$(echo "$line" | awk -Fs '{print $1}' | sed 's/\./,/g') # sed call replaces dot by comma so that Excel recognizes the number as such (on my machine)
  printf "$time\t"
  line=$(echo "$line" | awk -F[ '{print $2}')
  
  src=$(echo "$line" | awk -F] '{print $1}')
  echo "$src"
}

buildlog="$1"
cat $buildlog | grep "time[(]" \
  | sed 's/\\/\//g' \
  | while read LINE; do processline "$LINE"; done
