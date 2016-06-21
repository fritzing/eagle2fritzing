#!/bin/bash

# Location of EAGLE executable
EXEC=/Applications/EAGLE-7.6.0/EAGLE.app/Contents/MacOS/EAGLE
# Default brd2svg working path (override by passing argument to this script)
WORKPATH=/Users/pburgess/Desktop/FritzingTest
# Other paths used by brd2svg:
# PARTPATH and ANDPATH are relative to brd2svg application.
# BACKUPPATH is relative to 'brds' directory in WORKPATH
PARTPATH=../subparts
ANDPATH=./and
BACKUPPATH=bak

# If argument is passed to script, this overrides WORKPATH
if [ -n "$1" ]
then
  WORKPATH=$1
fi

# Preprocess each .brd file
BRDPATH=$WORKPATH/brds
for FILENAME in $BRDPATH/*.brd; do
  python preprocess.py "$FILENAME" "$FILENAME.tmp"
  OLDSIZE=$(wc -c <"$FILENAME")
  NEWSIZE=$(wc -c <"$FILENAME.tmp")
  if [ $NEWSIZE -lt $OLDSIZE ]; then
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

./brd2svg -c contrib -w $WORKPATH -e $EXEC -s $PARTPATH -a $ANDPATH

# TO DO: postprocess .param and .xml files if needed
