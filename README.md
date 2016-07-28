# eagle2fritzing

Converters from EAGLE to Fritzing file format. This is a fork of the original tools (github.com/fritzing/eagle2fritzing) to help facilitate some Adafruit- and microbuilder-specific parts.

To build, you will need three repositories: this one plus two others that can be retrieved from github.com/fritzing.
* eagle2fritzing (this repository)
* fritzing-app (from Fritzing GitHub acct)
* fritzing-parts (from Fritzing GitHub acct)

The resulting folders should be all be kept in the same parent directory, a la:

    some_folder
    |-- eagle2fritzing
    |-- fritzing-app
    |-- fritzing-parts

EAGLE must also be installed on the system.

Then:

```
brew install qt
cd eagle2fritzing/brd2svg
qmake -spec macx-g++ brd2svg.pro
make
```

To use, create a working folder (let's call it `FOO`), then create a subfolder inside called `brds`. Copy as many EAGLE `.brd` files to this as you wish to convert.

    FOO
    |-- brds
        |-- board1.brd
        |-- board2.brd
        |-- board3.brd

We won't be using the normal brd2svg program invocation, but if we did it would look like:

```
./brd2svg -w /Absolute/path/to/FOO -e /Applications/EAGLE-7.6.0/EAGLE.app/Contents/MacOS/EAGLE -c contrib -s ../subparts -a ./and
```

Instead, use the shell script `run.sh`. This does some preprocessing on the `.brd` file(s) (via `sed`) to avoid some rendering artifacts. **`run.sh` modifies the `.brd` files. Always keep a backup of the originals in a different directory!**

* Any `<text>` elements that are NOT in layer 21 (`tPlace`) are deleted. They throw off the bounding rect calculation for some parts.
* Any `<attribute>` elements with `name="NAME"` or `name="VALUE"` are deleted, for similar reasons... bounding rect calcs.

Adjacent to the `brds` folder, two new directories will be created: `params` and `xml`. For each `.brd` file in `brds`, a corresponding `.params` and `.xml` file will be created in those directories.

The board color can be changed. It's early in this file, look for the `"breadboard-color"` attribute, provide a hex RGB value.

After editing `.params` to your liking, running `run.sh` a second time then converts the `.params` and `.xml` files into `.svg`.

brd2svg has been tweaked to modify the output of some components in the Microbuilder library: 0805 resistors and capacitors normally use a generic footprint, but the code distinguishes between the two so they appear different in the resulting `.svg`. Similarly, 0805 LEDs were generic, some decisions are made in the code to substitute specific colors.

The `.svg` files will usually require a little cleanup in Illustrator or similar.

Note about `<nudge>` elements in the `.params` file: I'm trying to avoid some/most of these by strategically sizing the artboard of the component `.svg`s -- it's typical to have the artboard exactly match the component bounds, but these dimensions may vary from EAGLE's concept of the same component. brd2svg centers component SVGs within their EAGLE bounds (but does not crop) -- so, for example, if all USB host ports need to scoot 0.3mm toward the opening, add 0.6mm to the opposite bounding edge.

Subparts Art Attribution
============================================
MicroSD Card holder, ESP-12E module, and Bluefruit LE (MDBT40) art is from PigHixx's lovely diagrams!
http://www.pighixxx.com/
