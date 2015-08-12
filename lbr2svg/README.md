**lbr2svg**  is a command line tool that takes EAGLE part libraries (.lbr) as an input and converts them into Fritzing parts (.fzp/.svg).

Usage:

	lbr2svg 
		-w <path> 				: path to folder containing lbr and metadata file
		-p <path>				: path to Fritzing parts folder (output path)
		-c <core | user | contrib>		: which Fritzing library to export to

_Warning_: This tool is very powerful, but currently set up in a way very specific to a certain use case.

In order to properly convert all parts in the lbr, this tool requires a separate metadata database as input. It expects a 'new lbr parts.dif' table in the same folder of the lbr, that lists all parts to be converted, and defines various properties. 
This dif can be exported from [this google spreadsheet](https://docs.google.com/spreadsheets/d/1AgSe6SNsIVqOvrBOt7VH6HD0k3J2shrZemLKJ1UEUK0/edit?usp=sharing). It lists all SparkFun Eagle lbrs from some time ago. This example is overly complicated though, and we need to provide a much simpler one.

The columns defined in this spreadsheet are as follows, and are explained under the spreadsheet's "legend" tab. Most of them have been auto-generated (andré: how i don't remember right now..), and many are optional:

* _new fzp_
* _fzp disp_
* _old fzp_
* _new bread_
* _bread disp_
* _old bread_
* _use subpart_
* _new schem_
* _schem disp_
* _old schem_ 
* _new pcb_
* _pcb disp_
* _old pcb_
* _nr_
* _description_
* _family_
* _props_
* _tags_
* _title_
