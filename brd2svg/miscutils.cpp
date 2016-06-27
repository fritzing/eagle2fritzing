#include "miscutils.h"
#include <QtDebug>
#include <qmath.h>
#include "utils/textutils.h"
#include "svg/svgfilesplitter.h"

///////////////////////////////////////////////////////

WireTree::WireTree() { }

WireTree::WireTree(QDomElement & w)
{
	MiscUtils::x1y1x2y2(w, x1, y1, x2, y2);
	element = w;
	curve   = w.attribute("curve", "0").toDouble();
	width   = w.attribute("width", "");
	//qDebug() << "wiretree" << this << x1 << y1 << x2 << y2 << curve;


	if (curve != 0) {
		QDomElement piece = w.firstChildElement("piece");
		if (!piece.isNull()) {
			QDomElement arc = piece.firstChildElement("arc");
			if (!arc.isNull()) {
				qreal ax1, ay1, ax2, ay2;
				MiscUtils::x1y1x2y2(arc, ax1, ay1, ax2, ay2);
				qreal width;
				MiscUtils::rwaa(arc, radius, width, angle1, angle2);
				// Ensure 'angle1' refers to (x1,y1)
				// and 'angle2' to (x2,y2)
				if((ax1 != x1) || (ay1 != y1)) {
#if 1
					qreal tmp = angle1;
					angle1 = angle2;
					angle2 = tmp;
//curve = -curve;
#endif
				}
			}
		}
	}
}

void WireTree::turn() {
	qreal     tmp;
	WireTree *tptr;

	tmp  = x1;     x1     = x2;     x2     = tmp;
	tmp  = y1;     y1     = y2;     y2     = tmp;
	tmp  = angle1; angle1 = angle2; angle2 = tmp;
	curve = -curve;
}

////////////////////////////////////////////

bool MiscUtils::makePartsDirectories(const QDir & workingFolder, const QString & core, QDir & fzpFolder, QDir & breadboardFolder, QDir & schematicFolder, QDir & pcbFolder, QDir & iconFolder) {
	workingFolder.mkdir("parts");

	QDir partsFolder(workingFolder.absolutePath());
	partsFolder.cd("parts");
	if (!partsFolder.exists()) {
		qDebug() << QString("unable to create parts folder:%1").arg(partsFolder.absolutePath());
		return false;
	}

	partsFolder.mkdir("svg");
	partsFolder.mkdir(core);

	fzpFolder = partsFolder;
	fzpFolder.cd(core);
	if (!fzpFolder.exists()) {
		qDebug() << QString("unable to create fzp folder:%1").arg(fzpFolder.absolutePath());
		return false;
	}

	if (!partsFolder.cd("svg")) {
		qDebug() << QString("unable to create svg folder:%1").arg(partsFolder.absolutePath());
		return false;
	}

	partsFolder.mkdir(core);
	if (!partsFolder.cd(core)) {
		qDebug() << QString("unable to create svg %1 folder:%2").arg(core).arg(partsFolder.absolutePath());
		return false;
	}
	
	partsFolder.mkdir("breadboard");
	partsFolder.mkdir("schematic");
	partsFolder.mkdir("pcb");
	partsFolder.mkdir("icon");

	breadboardFolder = partsFolder;
	breadboardFolder.cd("breadboard");
	if (!breadboardFolder.exists()) {
		qDebug() << QString("unable to create breadboard folder:%1").arg(breadboardFolder.absolutePath());
		return false;
	}

	schematicFolder = partsFolder;
	schematicFolder.cd("schematic");
	if (!schematicFolder.exists()) {
		qDebug() << QString("unable to create schematic folder:%1").arg(schematicFolder.absolutePath());
		return false;
	}

	pcbFolder = partsFolder;
	pcbFolder.cd("pcb");
	if (!pcbFolder.exists()) {
		qDebug() << QString("unable to create pcb folder:%1").arg(pcbFolder.absolutePath());
		return false;
	}

	iconFolder = partsFolder;
	iconFolder.cd("icon");
	if (!iconFolder.exists()) {
		qDebug() << QString("unable to create icon folder:%1").arg(iconFolder.absolutePath());
		return false;
	}

	return true;
}

// ADAFRUIT 2016-06-23: support for Eagle's eight text alignment modes,
// rotation, plus multi-line text.  Mirroring is not supported because
// brain hurts already..
void MiscUtils::calcTextAngle(qreal & angle, int mirror, int spin, qreal size, qreal & x, qreal & y, int & anchor)
{
	// Normal 2D rotation:
	// x' = x * cos(r) - y * sin(r)
	// y' = y * cos(r) + x * sin(r)
	// BUT, because we're only shifting line height, x=0, thus:
	// x' = -y * sin(r)
	// y' =  y * cos(r)

	double xOffset = -size * sin(M_PI * angle / 180.0),
	       yOffset =  size * cos(M_PI * angle / 180.0);

	if((spin) || (angle <= 90.0) || (angle > 270.0)) {
		// If 'spin' is set, things rotate as expected.
		// e.g. 180 degree text is upside-down and
		// positioned the same relative to its anchor.
		// Same applies for unspun text at certain angles
		// (per checks above).
		if(anchor >= 6) {           // Top anchor:
			x -= xOffset;       // Move text down
			y -= yOffset;       // one line
		} else if(anchor >= 3) {    // Mid anchor:
			x -= xOffset * 0.5; // Move text down
			y -= yOffset * 0.5; // one half file
		} // Else bottom (baseline) anchor
	} else {
		// Spin unset -- text rotates weird in EAGLE
		angle += (angle < 180) ? 180 : -180;
		anchor = 8 - anchor;
		if(anchor >= 6) {           // Top anchor:
			x += xOffset;       // Move text down
			y += yOffset;       // one line
		} else if(anchor >= 3) {    // Mid anchor:
			x += xOffset * 0.5; // Move text down
			y += yOffset * 0.5; // one half file
		} // Else bottom (baseline) anchor
	}

	if(angle) angle = 360 - angle;
}


QString MiscUtils::makeGeneric(const QDir & workingFolder, const QString & boardColor, QList<QDomElement> & powers,
  const QString & copper, const QString & boardName, QSizeF outerChipSize, QSizeF innerChipSize,
  GetConnectorNameFn getConnectorName, GetConnectorNameFn getConnectorIndex, bool noText)
{
	// assumes 1000 dpi

	bool includeChip = innerChipSize.width() > 0 && innerChipSize.height() > 0;

	QString copper_local = copper;
	copper_local.remove(QRegExp("id=.connector[\\d]*p..."));

	int halfPowers = powers.count() / 2;
	qreal width = halfPowers * 100;		// width in mils; assume we always have an even number

	qreal chipDivisor = includeChip ? 2 : 1;
	if (outerChipSize.width() / chipDivisor > width - 200) {
		width = (outerChipSize.width() / chipDivisor) + 200;
	}

	//qreal chipHeight = qSqrt((bounds.width() * bounds.width()) + (bounds.height() * bounds.height()));
	qreal chipHeight = outerChipSize.height() / chipDivisor;
	chipHeight += noText ? 200 : 300;			// add room for pins and pin labels
	chipHeight = 200 * qCeil(chipHeight / 200);		// ensure an even number of rows

	// add textHeight

	// TODO: if the chip width is too wide find another factor to make it fit


	qreal height = qMax(400.0, chipHeight);				

	QString svg = TextUtils::makeSVGHeader(1000, 1000, width, height);
	svg += "<desc>Fritzing breadboard generated by brd2svg</desc>\n";
	svg += "<g id='breadboard'>\n";
	svg += "<g id='icon'>\n";						// make sure we can use this image for icon view


	svg += QString("<path fill='%1' stroke='none' d='M0,0L0,%2A30,30 0 0 1 0,%3L0,%4L%5,%4L%5,0L0,0z' stroke-width='0'/>\n")
				.arg(boardColor)
				.arg((height / 2.0) - 30)
				.arg((height / 2.0) + 30)
				.arg(height)
				.arg(width);



	qreal subx = outerChipSize.width() / 2;
	qreal suby = outerChipSize.height() / 2;
	if (!includeChip) {
		svg += QString("<g transform='translate(%1,%2)'>\n")
		  .arg((width / 2) - subx)
		  .arg((height / 2) - suby);
		svg += TextUtils::removeSVGHeader(copper_local);
		svg += "</g>\n";
	}
	else {
		QMatrix matrix;
		matrix.translate(subx, suby);
		//matrix.rotate(45);
		matrix.scale(1 / chipDivisor, 1 / chipDivisor);
		matrix.translate(-subx, -suby);
		svg += QString("<g transform='translate(%1,%2)'>\n")
		  .arg((width  / 2) - subx)
		  .arg((height / 2) - suby);
		svg += QString("<g transform='%1'>\n").arg(TextUtils::svgMatrix(matrix));

		svg += TextUtils::removeSVGHeader(copper_local);

		qreal icLeft = (outerChipSize.width()  - innerChipSize.width()) / 2;
		qreal icTop  = (outerChipSize.height() - innerChipSize.height()) / 2;
		svg += QString("<rect x='%1' y='%2' fill='#303030' width='%3' height='%4' stroke='none' stroke-width='0' />\n")
		  .arg(icLeft)
		  .arg(icTop)
		  .arg(innerChipSize.width())
		  .arg(innerChipSize.height());
		svg += QString("<polygon fill='#1f1f1f' points='%1,%2 %3,%2 %4,%5 %6,%5' />\n")
		  .arg(icLeft)
		  .arg(icTop)
		  .arg(icLeft + innerChipSize.width())
		  .arg(icLeft + innerChipSize.width() - 10)
		  .arg(icTop  + 10)
		  .arg(icLeft + 10);
		svg += QString("<polygon fill='#1f1f1f' points='%1,%2 %3,%2 %4,%5 %6,%5' />\n")
		  .arg(icLeft)
		  .arg(icTop  + innerChipSize.height())
		  .arg(icLeft + innerChipSize.width())
		  .arg(icLeft + innerChipSize.width() - 10)
		  .arg(icTop  + innerChipSize.height() - 10)
		  .arg(icLeft + 10);
		svg += QString("<polygon fill='#000000' points='%1,%2 %1,%3 %4,%5 %4,%6' />\n")
		  .arg(icLeft)
		  .arg(icTop)
		  .arg(icTop + innerChipSize.height())
		  .arg(icLeft + 10)
		  .arg(icTop + innerChipSize.height() - 10)
		  .arg(icTop +  10);
		svg += QString("<polygon fill='#3d3d3d' points='%1,%2 %1,%3 %4,%5 %4,%6' />\n")
		  .arg(icLeft + innerChipSize.width())
		  .arg(icTop)
		  .arg(icTop + innerChipSize.height())
		  .arg(icLeft + innerChipSize.width() - 10)
		  .arg(icTop + innerChipSize.height() - 10)
		  .arg(icTop +  10);
		svg += QString("<circle fill='#1f1f1f' cx='%1' cy='%2' r='10' stroke='none' stroke-width='0' />\n")
		  .arg(icLeft + 10 + 10 + 10)
		  .arg(icTop + innerChipSize.height() - 10 - 10 - 10);
	}

	svg += "</g>\n"; // chip xform
	svg += "</g>\n"; // chip xform

	if (!noText) {
		qreal fontSize = 65;
		svg += QString("<text id='label' font-family='OCRA' stroke='none' stroke-width='0' fill='white' font-size='%1' x='%2' y='%3' text-anchor='%4'>%5</text>\n")
		  .arg(fontSize)
		  .arg(50)
		  .arg((height / 2.0) + (fontSize / 2) - (fontSize / 8))
		  .arg("start")
		  .arg(boardName);
	}

	bool gotIncludes = false;
	qreal sWidth, sHeight;
	QDir includesFolder(workingFolder);
	includesFolder.cdUp();
	includesFolder.cd("includes");
	if (includesFolder.exists()) {
		QString errorStr;
		int errorLine;
		int errorColumn;
		QDomDocument pinDoc;
		QFile file(includesFolder.absoluteFilePath("bb_pin.svg"));
		if (pinDoc.setContent(&file, true, &errorStr, &errorLine, &errorColumn)) {
			qreal vbWidth, vbHeight;
			TextUtils::getSvgSizes(pinDoc, sWidth, sHeight, vbWidth, vbHeight);
			gotIncludes = true;
		}
	}

	double xOffset = (width - (halfPowers * 100)) / 2;
	if (gotIncludes) {
		qreal fontSize = 45;
		for (int i = 0; i < halfPowers; i++) {
			qreal x = (i * 100) + 50 + (fontSize / 2) - (fontSize / 5);
			qreal y = height - 50 - 8 - (sHeight * 1000 / 2);
	
			QDomElement power = powers.at(i);
			if (power.attribute("empty", "").isEmpty() && !noText) {
				svg += QString("<g transform='translate(%1,%2)'><g transform='rotate(%3)'>\n")
				  .arg(x + xOffset)
				  .arg(y)
				  .arg(-90);
				svg += QString("<text font-family='OCRA' stroke='none' stroke-width='0' fill='white' font-size='%1' x='%2' y='%3' text-anchor='%4'>%5</text>\n")
				  .arg(fontSize)
				  .arg(0)
				  .arg(0)
				  .arg("start")
				  .arg(TextUtils::escapeAnd(getConnectorName(power)));
				svg += "</g></g>\n";
			}

			y = 50 + 8 + (sHeight * 1000 / 2);
			power = powers.at(powers.count() - 1 - i);
			if (power.attribute("empty", "").isEmpty() && !noText) {
				svg += QString("<g transform='translate(%1,%2)'><g transform='rotate(%3)'>\n")
				  .arg(x + xOffset)
				  .arg(y)
				  .arg(-90);
				svg += QString("<text font-family='OCRA' stroke='none' stroke-width='0' fill='white' font-size='%1' x='%2' y='%3' text-anchor='%4'>%5</text>\n")
				  .arg(fontSize)
				  .arg(0)
				  .arg(0)
				  .arg("end")
				  .arg(TextUtils::escapeAnd(getConnectorName(power)));
				svg += "</g></g>\n";
			}
		}
	}

	svg += "</g>\n"; // icon
	svg += "</g>\n"; // breadboard
	svg += "</svg>\n";

	if (!gotIncludes) return svg;

	QDomDocument doc;
	TextUtils::mergeSvg(doc, svg, "breadboard");

	for (int i = 0; i < halfPowers; i++) {
		qreal x = (i * 100) + 50 - (sWidth * 1000 / 2);
		QDomElement power = powers.at(i);
		qreal y = height - 50 - (sHeight * 1000 / 2);
		if (power.attribute("empty", "").isEmpty()) {
			includeSvg2(doc, includesFolder.absoluteFilePath("bb_pin.svg"), getConnectorIndex(power), x + xOffset, y);
		}
	
		power = powers.at(powers.count() - 1 - i);
		if (power.attribute("empty", "").isEmpty()) {
			includeSvg2(doc, includesFolder.absoluteFilePath("bb_pin.svg"), getConnectorIndex(power), x + xOffset, 50 - (sHeight * 1000 / 2));
		}
	}

	svg = TextUtils::mergeSvgFinish(doc);

	return svg;
}

void MiscUtils::includeSvg2(QDomDocument & doc, const QString & path, const QString & name, qreal x, qreal y) {
	QFile file(path);
	if (!file.exists()) {
		qDebug() << "file '" << path << "' not found.";
		return;
	}

	SvgFileSplitter splitter;
	if (!splitter.load(&file)) return;

	QDomDocument subDoc = splitter.domDocument();
	QDomElement root = subDoc.documentElement();
	QDomElement child = TextUtils::findElementWithAttribute(root, "id", "connectorXpin");
	if (!child.isNull()) {
		child.setAttribute("id", name);
	}

	double factor;
	splitter.normalize(1000, "", false, factor);
	QHash<QString, QString> attributes;
	attributes.insert("transform", QString("translate(%1,%2)").arg(x).arg(y));
	splitter.gWrap(attributes);
	TextUtils::mergeSvg(doc, splitter.toString(), "breadboard");
}

bool MiscUtils::makeWirePolygons(QList<QDomElement> & wireList, QList<QList<struct WireTree *> > & polygons)
{
	QList<WireTree *> unsortedWires;

	// Add all wires from wireList to unsortedWires.
	foreach (QDomElement wire, wireList) {
		unsortedWires.append(new WireTree(wire));
	}
	// Then pick them off one by one as they're added to polygons...

	qreal maxPolyWidth=0, maxPolyHeight=0; // For finding perimeter polygon

	while(unsortedWires.count() >= 2) {
		QList<WireTree *> polygon;
		WireTree         *head, *tail;
		int               add;
		qreal             minX, minY, maxX, maxY;

		head = tail = unsortedWires.first(); // Take first wire thing in unsorted list
		polygon.append(tail);                // Add to current polygon
		unsortedWires.removeOne(tail);       // And remove from unsorted list
		minX = maxX = tail->x1;              // Note first point for
		minY = maxY = tail->y1;              // poly bounds calculation
		while(unsortedWires.count()) {
			add = 0;
			foreach(WireTree *w, unsortedWires) { // Compare tail against remaining wires
				if((tail->x2 == w->x1) && (tail->y2 == w->y1)) {
					// End (x2,y2) of tail is start (x1,y1) of wire w
					add = 1; // Append to tail
				} else if((tail->x2 == w->x2) && (tail->y2 == w->y2)) {
					// End (x2,y2) of tail is end (x2,y2) of wire w --
					// flip wire w so x2/y2 of tail is x1/y1 of w
					w->turn();
					add = 1; // Append to tail
				} else if((head->x1 == w->x2) && (head->y1 == w->y2)) {
					// End (x2,y2) of wire w is start (x1,y1) of head
					add = 2; // Prepend to head
				} else if((head->x1 == w->x1) && (head->y1 == w->y1)) {
					// Start (x1,y1) of wire w is start (x1,y1) of head --
					// flip wire w so x2/y2 of w is x1/y1 of head
					w->turn();
					add = 2; // Prepend to head
				}

				if(add) {
					if(w->x1 > maxX) maxX = w->x1; // Poly bounds calc
					if(w->y1 > maxY) maxY = w->y1;
					if(w->x1 < minX) minX = w->x1;
					if(w->y1 < minY) minY = w->y1;
					if(add == 1) {
						polygon.append(w);     // Add wire to current polygon
						tail = w;              // w is new tail of polygon
					} else {
						polygon.prepend(w);    // Prepend wire to current polygon
						head = w;              // w is new head of polygon
					}
					unsortedWires.removeOne(w);    // Remove from unsorted list
					break; // removeOne() throws off foreach(), so restart loop
				}
			}
			if(!add) break; // Couldn't find a wire to link up; is broken poly
		}

		if((polygon.last()->x2 != polygon.first()->x1) ||
		   (polygon.last()->y2 != polygon.first()->y1)) {
			printf("Not a closed polygon -- fudging it\n");
			WireTree *w = new WireTree();
			w->x1 = polygon.last()->x2;
			w->y1 = polygon.last()->y2;
			w->x2 = polygon.first()->x1;
			w->y2 = polygon.first()->y1;
			w->curve = 0;
			w->width = "";
			polygon.append(w);
		}

//printf("Adding polygon with %d points\n", polygon.count());
		qreal polyWidth  = maxX - minX,
		      polyHeight = maxY - minY;
		if((polyWidth  > maxPolyWidth) ||
		   (polyHeight > maxPolyHeight)) {
			maxPolyWidth  = polyWidth;
			maxPolyHeight = polyHeight;
			// Largest poly?  Add at head of list.
//printf("At head of list\n");
			polygons.prepend(polygon);
		} else {
			// Else end of list.
//printf("At tail of list\n");
			polygons.append(polygon);
		}
	}
//printf("%d polygons\n", polygons.count());

	// First item in polygons is known perimeter now, and should be clockwise.
	// All other polys should be counterclockwise (these are presumed holes).

	foreach(QList<WireTree *> polygon, polygons) {
		WireTree *w1 = polygon.last();
		qreal     sum = 0, xA, yA, xB, yB, xC, yC, angle, c, s;
		foreach(WireTree *w2, polygon) {
			xA    = w1->x2 - w1->x1; // Vector A
			yA    = w1->y2 - w1->y1;
			xB    = w2->x2 - w2->x1; // Vector B
			yB    = w2->y2 - w2->y1;
			angle = -atan2(yA, xA);  // Cartesian -angle of vector A
			c     = cos(angle);
			s     = sin(angle);
			xC    = xB * c - yB * s; // Vector C is a rotated vector B
			yC    = yB * c + xB * s;
			sum  += atan2(yC, xC);   // Angle B-A (-Pi to +Pi)
			w1    = w2; // Next wire segment
		}

//if(sum > 0) printf("Counterclockwise\n");
//else        printf("Clockwise\n");
		if((polygon == polygons.first()) == (sum > 0)) {
//printf("Reversing\n");
			int polyIndex = polygons.indexOf(polygon);
			foreach(WireTree *w, polygon) {
				w->turn(); // Exchange x1/y1/x2/y2 etc.
			}
			int last = polygon.size() - 1,
			    mid  = polygon.size() / 2;
			for(int i=0; i<mid; i++) {
				polygon.swap(i, last-i);
			}
			// Reordering polygon's wire list messes with
			// the list entry point...replace element in
			// polygons list with new entry.
			polygons.replace(polyIndex, polygon);
		}

		foreach(WireTree *w, polygon) {
			printf("(%f,%f) (%f,%f)\n", w->x1, w->y1, w->x2, w->y2);
		}
	}

	return (polygons.count() > 0);
}


bool MiscUtils::rwaa(QDomElement & element, qreal & radius, qreal & width, qreal & angle1, qreal & angle2)
{
	bool ok = true;
	radius = strToMil(element.attribute("r", ""), ok);
	if (!ok) return false;

	width = strToMil(element.attribute("width", ""), ok);
	if (!ok) return false;

	angle1 = element.attribute("angle1", "").toDouble(&ok);
	if (!ok) return false;

	angle2 = element.attribute("angle2", "").toDouble(&ok);
	return ok;
}

bool MiscUtils::x1y1x2y2(const QDomElement & element, qreal & x1, qreal & y1, qreal & x2, qreal & y2)
{
	bool ok = true;
	x1 = element.attribute("x1", "").toDouble(&ok);
	if (!ok) {
		x1 = strToMil(element.attribute("x1", ""), ok);
		if (!ok) return false;
	}

	y1 = element.attribute("y1", "").toDouble(&ok);
	if (!ok) {
		y1 = strToMil(element.attribute("y1", ""), ok);
		if (!ok) return false;
	}

	x2 = element.attribute("x2", "").toDouble(&ok);
	if (!ok) {
		x2 = strToMil(element.attribute("x2", ""), ok);
		if (!ok) return false;
	}

	y2 = element.attribute("y2", "").toDouble(&ok);
	if (!ok) {
		y2 = strToMil(element.attribute("y2", ""), ok);
	}
	return ok;
}


qreal MiscUtils::strToMil(const QString & str, bool & ok) {
	ok = false;
	if (!str.endsWith("mil")) return 0;

	QString sub = str.left(str.length() - 3);
	return sub.toDouble(&ok);
}
