# eagle2fritzing

Converters from EAGLE to Fritzing file format:

*  [**brd2svg**](https://github.com/fritzing/eagle2fritzing/tree/master/brd2svg) converts EAGLE board files (.brd) to Fritzing parts (.fzp/.svg)

*  [**lbr2svg**](https://github.com/fritzing/eagle2fritzing/tree/master/lbr2svg) converts EAGLE part libraries (.lbr) to Fritzing parts and bins (.fzp/.svg/.fzb)

These are stand-alone command line tools built with [Qt](http://www.qt.io). Look into their individual readmes to get started.

Both tools depend on the [fritzing-app](https://github.com/fritzing/fritzing-app) repository. Make sure you clone this one next to it and keep the original path names:

    some_folder
    |-- eagle2fritzing
    |-- fritzing-app
