# eagle2fritzing

Converters from EAGLE to Fritzing file format. This is a fork of the original tools (github.com/fritzing/eagle2fritzing) to help facilitate some Adafruit- and microbuilder-specific parts.

To build, you will need three repositories: this one plus two others that can be retrieved from github.com/fritzing.
* eagle2fritzing (this repository)
* fritzing-app (from Fritzing Github acct)
* fritzing-parts (from Fritzing Github acct)

The resulting folders should be all be kept in the same parent directory, a la:
    some_folder
    |-- eagle2fritzing
    |-- fritzing-app
    |-- fritzing-parts

EAGLE must also be installed on the system.

Then:

brew install qt
cd eagle2fritzing/brd2svg
qmake -spec macx-g++ brd2svg.pro
make

To use, create a working folder (let's call it "FOO"), then create a subfolder inside called "brds". Copy as many EAGLE .brd files to this as you wish to convert.

    FOO
    |-- brds
        |-- board1.brd
        |-- board2.brd
        |-- board3.brd

MANUAL EDITING STEP: before running the conversion, the .brd file(s) require some hand editing. TODO: try to automate or eliminate this step. WORKING FROM A COPY (save your original .brds), in each .brd file:

* In the <packages> section of the file, in each package, delete the text elements in layers 25 and 27 (typically these will be strings like "NAME" and "VALUE"). Just delete those entire lines. Although the converter program skips these layers, those text elements still throw off the bounding rectangle calculation for some parts. (You can leave the NAME and VALUE attributes in the <elements> section of the file -- these don't seem to affect the bounds calc.)
* Also in the <packages> section, for any packages starting with CHIPLED*, look for two <smd> elements with names "A" and "C" and delete those lines. Similar reasons...these throw off the bounding-rect calculation.

Then run the brd2svg program (pass 1 of 2), passing in the various working directories:

./brd2svg -w /Absolute/path/to/FOO -e /Applications/EAGLE-7.6.0/EAGLE.app/Contents/MacOS/EAGLE -c contrib -s ../subparts -a ./and

Adjacent to the 'brds' folder, two new directories will be created: 'params' and 'xml'. For each .brd file in 'brds', a corresponding .params and .xml file will be created in those directories.

Edit the .params file. There's a section called <unused> containing a bunch of copper pads. MOVE all of these UP into the <right> section. If you convert a board and find most of the copper pads are missing in the final .svg, this is why. TODO: why are these pads going in 'unused'?

The board color can be changed. It's early in this file, look for the "breadboard-color" attribute, provide a hex RGB value.

Also in the .params file: the microbuilder library doesn't distinguish between certain parts footprints. For example, SMD resistors and capacitors will both have a 'package' attribute of "0802-NO". Search the file for "0802-NO" and replace each with either "0802-res" or "0802-cap" depending on the element name (e.g. "R1" or "C1"). Similarly, all SMD LEDs have the same package, but these can be changed (e.g. "0802-led-yellow"). TO DO: can some/all of this be automated?

Edit the .xml file, making similar changes. "0805-NO" can be replaced with "0805-res" or "0805-cap", and LEDs similarly substituted. In this file, you can determine the LED colors by looking at the parent <element> -- there's an attribute called "value" that'll indicate red/blue/etc.

Running brd2svg a second time (exact same arguments) then converts the .params and .xml files into .svg:

./brd2svg -w /Absolute/path/to/FOO -e /Applications/EAGLE-7.6.0/EAGLE.app/Contents/MacOS/EAGLE -c contrib -s ../subparts -a ./and

The .svg files will usually require a little cleanup in Illustrator or similar:

* Any text that wasn't bottom-left-aligned in EAGLE will be positioned wrong (centering and other alignments weren't added until later EAGLE releases). Usually just a few (if any) - easy to move any such text items manually.
* Boards with the 2.5mm Adafruit logo will get an "optimized" vector version placed close to the raster logo, but the position will need some fine-tuning. Then you'll want to hide this element, delete the raster logo (a zillion rectangles) and unhide the vector logo before saving.
