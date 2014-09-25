**lbr2svg**  is a command line tool that takes EAGLE part libraries (.lbr) as an input and converts them into Fritzing parts (.fzp/.svg).

Usage:

	lbr2svg 
		-w <path> 						: path to folder containing lbr files
		-p <path>						: path to Fritzing parts folder (output path)
		-c <core | user | contrib>		: which Fritzing library to use
