#! /usr/bin/python

# Preprocessor for brd2svg application.  Removes some potentially
# troublesome elements from EAGLE .brd files before conversion.
# Pass input filename, output filename

import sys
from xml.dom.minidom import parse

# Recursively walk through the whole DOM tree and adjusts certain
# nodes that throw off the brd2svg program:
# - Deletes all <text> lines where layer!=21 (tplace) --
#   these throw off the part boundary calculation in EAGLE...
#   this MUST be preprocessed, trying to do the same in the .ulp
#   script is too late.
# - Deletes <attribute> lines whose name is "NAME" or "VALUE"
#   for similar reasons.
def adjustNodes(root):
	for node in root.childNodes:
		if node.nodeType == node.ELEMENT_NODE:
			adjustNodes(node)
			tag = node.tagName.lower()
			if tag == "text":
				if node.getAttribute("layer") != "21":
					root.removeChild(node)
			elif tag == "attribute":
				name = node.getAttribute("name").lower()
				if (name == "name") or (name == "value"):
					root.removeChild(node)

# --------------------------------------------------------------------------

if len(sys.argv) < 3 :
	print "syntax: " + sys.argv[0] + " infile outfile"
	exit(1)

dom = parse(sys.argv[1])    # infile
adjustNodes(dom)

f = open(sys.argv[2], "wb") # outfile
f.write(dom.toxml(encoding="utf-8"))
f.close()
