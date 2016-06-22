#! /usr/bin/python

# Postprocessor for brd2svg application.
# Rearranges elements in SVG file to correct stacking order.
# Pass input filename, output filename

import sys
from xml.dom.minidom import parse

if len(sys.argv) < 3 :
	print "syntax: " + sys.argv[0] + " infile outfile"
	exit(1)

# SVG file format from brd2xml is typically like this:
# <svg>
#  <desc>
#  <g id=breadboard>
#   <g id=icon>
#     ... graphics ...

# Identify 'icon' node in DOM tree:
dom        = parse(sys.argv[1])
svg        = dom.childNodes[1]
breadboard = svg.childNodes[3]
icon       = breadboard.childNodes[1]

# If there's a Featherwing in use, it typically appears at
# the end of the SVG due to the way brd2svg has to handle
# the Featherwing as a component.  It needs to be moved to
# the first element in the <icon> section in order to appear
# as the bottom-most element.
# It's probably the last breadboard child node, but for
# posterity, scan through node list to locate it:
for node in breadboard.childNodes:
	if((node.nodeType == node.ELEMENT_NODE) and
	   (node.tagName.lower() == "g") and
           (node.getAttribute("id").lower() == "featherwing")):
		icon.insertBefore(node, icon.firstChild)

f = open(sys.argv[2], "wb") # outfile
f.write(dom.toxml(encoding="utf-8"))
f.close()
