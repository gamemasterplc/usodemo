#!/bin/bash

#Exit when failure
set -e

#Generate USO rules for building
./gen_uso_rules.sh > uso_rules.mak
#Pass arguments to common.mak
make -f common.mak "$@"