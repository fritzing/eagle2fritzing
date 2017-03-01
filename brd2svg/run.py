#!/usr/bin/python

import argparse
import glob
import os
import shutil
from sys import platform as _platform

# Location of EAGLE executable
#EXEC = "/Applications/EAGLE-6.6.0/EAGLE.app/Contents/MacOS/EAGLE"
#EXEC = "C:\\Program Files (x86)\\EAGLE-6.4.0\\bin\\eagle.exe"
EXEC = "C:\\Program Files (x86)\\EAGLE-6.6.0\\bin\\eagle.exe"
# Default brd2svg working path (override by passing argument to this script)
WORKPATH = "FritzingTest"
# Other paths used by brd2svg:
# PARTPATH and ANDPATH are relative to brd2svg application.
# BACKUPPATH is relative to 'brds' directory in WORKPATH
PARTPATH = "../subparts"
ANDPATH = "./and"
BACKUPPATH = "bak"
# Set to False to skip pre/post processing steps
PREPROCESS = True
POSTPROCESS = False
# POSTPROCESSING IS NOT CURRENTLY NEEDED -- fixed FeatherWing issue in code


if __name__ == '__main__':
    # Parse the input arguments
    argparser = argparse.ArgumentParser(description="Does the joyous fritzingification")
    argparser.add_argument("-v", "--verbose", dest="verbose", action="store_true", default=False,
                           help="verbose mode")
    argparser.add_argument("-w", "--workpath", dest="workpath",
                           help="If argument is passed to script, this overrides WORKPATH")
    args = argparser.parse_args()

    if (args.workpath):
        WORKPATH = args.workpath
    
    if (PREPROCESS):
        BRDPATH = WORKPATH + "/brds"
        #print BRDPATH+"/*.brd"
        files = glob.glob(BRDPATH+"/*.brd")
        print files
        print _platform
        for FILENAME in files:
            os.system("python preprocess.py \""+FILENAME+"\" \""+FILENAME+".tmp\"")
            OLDSIZE = os.stat(FILENAME)[6]
            NEWSIZE = os.stat(FILENAME+".tmp")[6]
            if (NEWSIZE != OLDSIZE):
                print("File modified: "+str(OLDSIZE)+" vs "+str(NEWSIZE))
                if (not os.path.isdir(BRDPATH+"/"+BACKUPPATH)):
                    os.mkdir(BRDPATH+"/"+BACKUPPATH)
                shutil.copy(FILENAME, BRDPATH+"/"+BACKUPPATH)
                shutil.move(FILENAME+".tmp", FILENAME)
            else:
                os.remove(FILENAME+".tmp")

            if _platform == "darwin":
               # MAC OS X
               cmd = './brd2svg -c contrib -w "'+WORKPATH+'" -e "'+EXEC+'" -s "'+PARTPATH+'" -a "'+ANDPATH+'"'
            else:
               # Windows
               cmd = 'brd2svg -c contrib -w "'+WORKPATH+'" -e "'+EXEC+'" -s "'+PARTPATH+'" -a "'+ANDPATH+'"'

            print cmd
            # run twice!
            os.system(cmd)
            os.system(cmd)
                
"""

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
"""
