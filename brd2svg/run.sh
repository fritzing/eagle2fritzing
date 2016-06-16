#!/bin/bash

# Location of EAGLE executable
EXEC=/Applications/EAGLE-7.6.0/EAGLE.app/Contents/MacOS/EAGLE
# Default brd2svg working path (override by passing argument to this script)
WORKPATH=/Users/pburgess/Desktop/FritzingTest
# Other paths used by brd2svg
PARTPATH=../subparts
ANDPATH=./and

# If argument is passed to script, this overrides WORKPATH
if [ -n "$1" ]
then
  WORKPATH=$1
fi

# Preprocess each .brd file
BRDPATH=$WORKPATH/brds
for FILENAME in $BRDPATH/*.brd; do
  # Delete all <text> lines where layer!=21 (tplace)
  # Ugly brute force hack because weird OSX regex?
  sed -i .orig -E '/^\s*<text\ .*layer="([0-9]|2[02-9]|[13-9][0-9]|[0-9][0-9][0-9])"/d' "$FILENAME"
  # Delete 'NAME' and 'VALUE' attributes for similar reasons
  sed -i .orig -E '/^\s*<attribute name="(NAME|VALUE)"/d' "$FILENAME"
  # These throw off the part boundary calculation in EAGLE...
  # trying to do the same in the .ulp script is too late.
done

#exit 0

./brd2svg -c contrib -w $WORKPATH -e $EXEC -s $PARTPATH -a $ANDPATH

# TO DO: postprocess .param and .xml files if needed
