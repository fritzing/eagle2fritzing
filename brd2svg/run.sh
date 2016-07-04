#!/bin/bash

# Location of EAGLE executable:
EXEC=/Applications/EAGLE-7.3.0/EAGLE.app/Contents/MacOS/EAGLE
# Default brd2svg working path (override by passing argument to this script):
WORKPATH=~/Desktop/FritzingTest
# Other paths used by brd2svg:
# PARTPATH and ANDPATH are relative to brd2svg application.
# BACKUPPATH is relative to 'brds' directory in WORKPATH.
PARTPATH=../subparts
ANDPATH=./and
BACKUPPATH=bak
# Converted parts will be copied here for testing w/Fritzing:
TESTPATH=~/Documents/Fritzing
# Set to 0 to skip pre/post processing steps:
PREPROCESS=1
POSTPROCESS=0
# POSTPROCESSING IS NOT CURRENTLY NEEDED -- fixed FeatherWing issue in code

# If argument is passed to script, this overrides WORKPATH
if [ -n "$1" ]
then
  WORKPATH=$1
fi

# Preprocess .brd files - MUST BE IN EAGLE XML FORMAT
if [ -n "$PREPROCESS" ] && [ "$PREPROCESS" -gt 0 ]; then
  # Preprocess each .brd file
  BRDPATH=$WORKPATH/brds
  for FILENAME in $BRDPATH/*.brd; do
    python preprocess.py "$FILENAME" "$FILENAME.tmp"
    OLDSIZE=$(wc -c <"$FILENAME")
    NEWSIZE=$(wc -c <"$FILENAME.tmp")
    if [ $NEWSIZE -ne $OLDSIZE ]; then
      # File size changed; copy original to BACKUPPATH
      mkdir -p "$BRDPATH/$BACKUPPATH"
      cp "$FILENAME" "$BRDPATH/$BACKUPPATH"
      # Then overwrite original with new (smaller) file.
      mv "$FILENAME.tmp" "$FILENAME"
      # It's done this way because getting the brd2svg application
      # and .ulp script to process the .brd.tmp files isn't working;
      # need to use the original .brd filename.
    else
      # No change in file size; delete temp file.
      rm -f "$FILENAME.tmp"
    fi
  done
fi

# If brd2svg returns an exit code of 42, this indicates that the
# EAGLE ULP script was run and new XML was generated, in which case
# brd2svg can be run a second time to produce more 'finished'
# results.  If it returns 0 (or anything else), one pass is sufficient.
(exit 42) # Force first invocation
while [ $? -eq 42 ]; do
  ./brd2svg -c contrib -w $WORKPATH -e $EXEC -s $PARTPATH -a $ANDPATH
done

# Postprocess each .svg file
if [ -n "$POSTPROCESS" ] && [ "$POSTPROCESS" -gt 0 ]; then
  BRDPATH=$WORKPATH/parts/svg/contrib/breadboard
  for FILENAME in $BRDPATH/*.svg; do
    python postprocess.py "$FILENAME" "$FILENAME.tmp"
    OLDSIZE=$(wc -c <"$FILENAME")
    NEWSIZE=$(wc -c <"$FILENAME.tmp")
    if [ $NEWSIZE -ne $OLDSIZE ]; then
      mv "$FILENAME.tmp" "$FILENAME"
    else
      rm -f "$FILENAME.tmp"
    fi
  done
fi

# Install converted parts into Fritzing (may need to relaunch)
cp -r $WORKPATH/parts $TESTPATH
