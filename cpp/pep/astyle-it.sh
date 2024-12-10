#!/usr/bin/env sh
find -E . \
    -path ./ext -prune -o \
    -regex '.*\.(java|h|hpp|c|cpp|cxx)' -exec astyle --options=pep.astylerc {} ';'
