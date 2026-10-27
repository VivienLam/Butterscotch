#!/bin/bash
# Regenerates src/n3ds/n3ds_aot_gen.inc: regen.sh <data.win> <AM2R project folder (AM2R.project.gmx, scripts/, objects/)>
# The list is aot_list.txt (scripts by name, object events as object:eventtype:subtype). After a change, the
# bytecode hashes in n3ds_am2r_native_hashes.h pin each translation to the 1.5.5 code it was checked against.
here=$(dirname "$0")
python3 "$here/gml2c.py" "$1" "$2" "$here/../../../src/n3ds/n3ds_aot_gen.inc" $(cat "$here/aot_list.txt")
