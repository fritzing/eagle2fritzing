#include "lbrapplication.h"
#include "miscutils.h"
#include "utils/textutils.h"
#include "utils/schematicrectconstants.h"
#include "svg/svgfilesplitter.h"

#include <QtDebug>
#include <qmath.h>
#include <QFontDatabase>
#include <QFont>
#include <QFontMetrics>
#include <QHash>
#include <QDate>
#include <QBitArray>
#include <limits>
#include <QGuiApplication>

///////////////////////////////////////////////////////////

// todo:
//
//	smd + tht: must remove copper0 for the smds in fzp view
//  if a symbol is a rectangle, add a part title and pin numbers. Rectangle symbols have 4 wires, two horizontal and two vertical
//  non-zero gate?
//	connector male or female
//	text bounds (esp. rotated)
//	test-point breadboard
//	a few missing pcbs (unusual layers)

///////////////////////////////////////////////////////////

static const QString SchematicLayer("94");
static const QString SMDLayer("1");
static const QString PadLayer("17");
static const QString TPlaceLayer("21");
static const QString TDocuLayer("51");
static const QString MeasuresLayer("47");

static const QString CopperColor("#F7BD13");
static const QString SilkColor("#f0f0f0");
static const QString BreadboardCopperColor("#9A916C");
static const QString BreadboardColor("#1F7A34");
static const QString BreadboardSilkColor("#f8f8f8");

static const QString PropSeparator("___");
static const double TextSizeMultiplier = 1.3;
static int MixIndex = 0;

static QHash<QString, QString> PackageConnectors;
static QHash<QString, QString> PackageConnectorTypes;
static QHash<QString, QString> SymbolConnectors;
static QHash<QString, int> PackageConnectorIndexes;
static QHash<QString, int> SymbolConnectorIndexes;
static QList<QString> AllSMDs;
static QHash<QString, QString> OldBreadboardFiles;

static QHash<QString, QString> Breakouts;

static QSet<QString> SchematicIcons;

static QRegExp dipper("di[lp][\\s\\-_]{0,1}\\d");

// temporary for debugging
static QStringList SquareNames;


///////////////////////////////////////////////////////////

QString cleanChars(const QString & from) {
	QString to;
	foreach (QChar c, from) {
		if (c.isLetterOrNumber()) to.append(c);
		else if (c == '.') to.append(c);
		else if (c == '_') to.append(c);
		else if (c == '-') to.append(c);
		else to.append('_');
	}

	return to;
}

QString cleanChars2(const QString & from) {
	QString to;
	foreach (QChar c, from) {
		if (c.isLetterOrNumber()) to.append(c);
		else if (c == '>') to.append('_');
        else if (c == '\'') to.append('_');
		else if (c == '<') to.append('_');
		else if (c == '&') to.append('_');
		else if (c == '\'') to.append('_');
		else if (c == '/') to.append('_');
		else if (c == '"') to.append('_');
		else to.append(c);
	}

	return to;
}

qreal getRot(const QDomElement & element) {
	QString rot = element.attribute("rot", "");
	if (rot.isEmpty()) return 0;

	rot.remove(0, 1);
	bool ok;
	qreal angle = rot.toDouble(&ok);
	if (!ok) return 0;
	
	return angle;
}

void swap(qreal & v1, qreal & v2) {
	qreal temp = v1;
	v1 = v2;
	v2 = temp;
}

bool xy(const QDomElement & element, qreal & x, qreal & y)
{
	bool ok = true;
	x = element.attribute("x", "").toDouble(&ok);
	if (!ok) return false;

	y = element.attribute("y", "").toDouble(&ok);
	return ok;
}

bool dxdy(const QDomElement & element, qreal & x, qreal & y)
{
	bool ok = true;
	x = element.attribute("dx", "").toDouble(&ok);
	if (!ok) return false;

	y = element.attribute("dy", "").toDouble(&ok);
	return ok;
}

bool drillDiameter(const QDomElement & element, qreal & drill, qreal & diameter)
{
	bool ok = true;
	drill = element.attribute("drill", "").toDouble(&ok);
	if (!ok) return false;

	diameter = element.attribute("diameter", "").toDouble(&ok);

	if (!ok) {
		ok = true;
		diameter = drill + (.01 * 25.4 * 2);   // seems to be the default?
	}
	return ok;
}

void setBounds(const QDomElement & element, qreal & x1, qreal & y1, qreal & x2, qreal & y2, bool poly, QRectF & bounds) 
{
	bool ok;
	qreal width = element.attribute("width").toDouble(&ok);
	if (!ok) width = 0;

	if (x2 < x1) {
		swap(x2, x1);
	}

	if (y2 < y1) {
		swap(y2, y1);
	}

	int divisor = (poly) ? 1 : 2;
	bounds.setCoords(x1 - (width / divisor), y1 - (width / divisor), x2 + (width / divisor), y2 + (width / divisor));
}

void getPairBounds(const QDomElement & element, bool poly, QRectF & bounds)
{
	qreal x1, y1, x2, y2;
	if (!MiscUtils::x1y1x2y2(element, x1, y1, x2, y2)) return;

	setBounds(element, x1, y1, x2, y2, poly, bounds);
}

bool cxcyrw(const QDomElement & element, qreal & cx, qreal & cy, qreal & radius, qreal & width)
{
	bool ok = true;
	radius = element.attribute("radius", "").toDouble(&ok);
	if (!ok) return false;

	width = element.attribute("width", "").toDouble(&ok);
	if (!ok) return false;

	return xy(element, cx, cy);

}

bool parsePin(const QDomElement & element, qreal & x1, qreal & y1, qreal & x2, qreal & y2) {
	if (!xy(element, x1, y1)) return false;

	qreal length;
	QString lstr = element.attribute("length");
	if (lstr.compare("short") == 0) {
		length = 2.54;
	}
	else if (lstr.compare("middle") == 0) {
		length = 5.08;
	}
	else if (lstr.compare("long") == 0 || lstr.isEmpty()) {
		length = 7.62;
	}
	else if (lstr.compare("point") == 0) {
		x2 = x1;
		y2 = y1;
		return true;
	}
	else {
		qDebug() << "bad pin length" << lstr;
		return false;
	}

	qreal angle = getRot(element);
	if (angle == 0) {
		x2 = x1 + length;
		y2 = y1;
		return true;
	}

	if (angle == 90) {
		x2 = x1;
		y2 = y1 + length;
		return true;
	}

	if (angle == 180) {
		x2 = x1 - length;
		y2 = y1;
		return true;
	}

	if (angle == 270) {
		x2 = x1;
		y2 = y1 - length;
		return true;
	}

	qDebug() << "bad pin rotation" << angle;
	return false;
}

QRegExp FindDigits("[^\\d]*(\\d+)");

bool byAttribute(QDomElement & e1, QDomElement & e2, const QString & attr)
{
    QString p1 = e1.attribute(attr);
    int p1ix = FindDigits.indexIn(p1);
    int pn1 = -1;
    if (p1ix >= 0) {
        pn1 = FindDigits.cap(1).toInt();
    }

    QString p2 = e2.attribute(attr);
    int p2ix = FindDigits.indexIn(p2);
    int pn2 = -1;
    if (p2ix >= 0) {
        pn2 = FindDigits.cap(1).toInt();
    }

    if (p1ix >= 0 && p2ix >= 0) {
        return pn1 < pn2;
    }

    if (p1ix >= 0) return true;
    if (p2ix >= 0) return false;

    return p1 < p2;
}

bool byName(QDomElement & e1, QDomElement & e2)
{
    return byAttribute(e1, e2, "name");
}

bool byConnectorID(QDomElement & e1, QDomElement & e2)
{
    return byAttribute(e1, e2, "connectorid");
}

bool byPad(QDomElement & e1, QDomElement & e2)
{
    return byAttribute(e1, e2, "pad");
}


QString getConnectorName(const QDomElement & element) {

	//QString string;
	//QTextStream stream(&string);
	//element.save(stream, 0);
	//qDebug() << string;
	return element.attribute("name");

}

QString getConnectorIndex(const QDomElement & element) {
    QString id = element.attribute("connectorid");
    id.replace("pad", "pin");
    return id;
}

QString makeJoinedName(const QStringList & names) {
	return QString("%1_mix_%2").arg(names.at(0)).arg(MixIndex++);
}

///////////////////////////////////////////////////////////

void FileDescr::init(const QString & prefix, const QHash<QString, int> indexes, const QStringList & values) {
    newName = values.at(indexes.value("new " + prefix));
    disp = values.at(indexes.value(prefix + " disp"));
    oldName = values.at(indexes.value("old " + prefix));
}

PartDescr::PartDescr(const QHash<QString, int> indexes, const QStringList & values) {
    matched = false;
    number = values.at(indexes.value("nr")).toInt();
    description = values.at(indexes.value("description"));
    title = values.at(indexes.value("title"));
    tags = values.at(indexes.value("tags"));
    props = values.at(indexes.value("props"));
    family = values.at(indexes.value("family"));
    useSubpart = values.at(indexes.value("use subpart"));
    if (useSubpart.endsWith(".svg")) {
        useSubpart.chop(4);
    }
    fzp.init("fzp", indexes, values);
    bread.init("bread", indexes, values);
    schem.init("schem", indexes, values);
    pcb.init("pdb", indexes, values);
}

///////////////////////////////////////////////////////////


LbrApplication::LbrApplication(int &argc, char **argv[]) : QGuiApplication(argc, *argv)
{   
//    int result = QFontDatabase::addApplicationFont(":/resources/fonts/DroidSans.ttf");
 //   result = QFontDatabase::addApplicationFont(":/resources/fonts/DroidSans-Bold.ttf");
 //   result = QFontDatabase::addApplicationFont(":/resources/fonts/DroidSansMono.ttf");
 //   result = QFontDatabase::addApplicationFont(":/resources/fonts/OCRA.ttf");
    m_core = "core";
}

LbrApplication::~LbrApplication()
{
}

void LbrApplication::start() 
{
    if (!initArguments()) {
        usage();
        return;
    }

	QDir workingFolder(m_workingPath);
	QDir fzpFolder, breadboardFolder, schematicFolder, pcbFolder, iconFolder;
	if (!MiscUtils::makePartsDirectories(workingFolder, m_core, fzpFolder, breadboardFolder, schematicFolder, pcbFolder, iconFolder)) return;

    QFile::remove(workingFolder.absoluteFilePath("newlbrs.csv"));
	QFile lbr(workingFolder.absoluteFilePath("lbr.csv"));
	lbr.open(QFile::WriteOnly);
	QTextStream lbrStream(&lbr);
	lbrStream.setCodec("UTF-8");
	lbrStream << "new FZP,FZP Disp,nr,part admin,old FZP,new Bread,Bread Disp,old Bread,new Schem,Schem Disp,old Schem,new PCB,PCB Disp,old PCB,title,description,family,props,tags\n";
	lbrStream << "\n";
	lbrStream << ",";
	lbrStream << "\"discard (new)\nreplace (old)\nvariant\nmerge\ngenerate\nnew\",";
	lbrStream << ",,,,";
	lbrStream << "\"DIP\ngeneratedbreakout\nexisting\ngenerated\nillustrate\nprogrammatic\",,";
	lbrStream << ",";
	lbrStream << "\"existing\nillustrate\ngenerated\",,";
	lbrStream << ",";
	lbrStream << "\"existing\nillustrate\ngenerated\",,";
	lbrStream << ",,,,\n";
	lbrStream << "\n";

	QDir binsFolder(workingFolder);
	binsFolder.mkdir("bins");
	binsFolder.cd("bins");
	if (!binsFolder.exists()) {
		qDebug() << QString("unable to make bins folder:%1").arg(binsFolder.absolutePath());
		return;	
	}

	QDir lbrFolder(workingFolder);
	lbrFolder.cd("lbrs");
	if (!lbrFolder.exists()) {
		qDebug() << QString("unable to find lbrs folder:%1").arg(lbrFolder.absolutePath());
		return;	
	}

    loadPartsDescrs(workingFolder, "new lbr parts.dif");

    QDir subpartsFolder(m_fritzingPartsPath);
	subpartsFolder.cd("subparts");

	QStringList nameFilters;
	nameFilters << "*.lbr";
	QStringList fileList = lbrFolder.entryList(nameFilters, QDir::Files | QDir::NoDotAndDotDot);
    QStringList moduleIDs;
	foreach (QString filename, fileList) {
		SchematicIcons.clear();
		PackageConnectors.clear();
		PackageConnectorTypes.clear();
		SymbolConnectors.clear();
        PackageConnectorIndexes.clear();
        SymbolConnectorIndexes.clear();
        AllSMDs.clear();
        OldBreadboardFiles.clear();

		QFile file(lbrFolder.absoluteFilePath(filename));
		QString errorStr;
		int errorLine;
		int errorColumn;
		QDomDocument doc;
		if (!doc.setContent(&file, true, &errorStr, &errorLine, &errorColumn)) {		
			qDebug() << "unable to parse file" << lbrFolder.absoluteFilePath(filename);
			continue;
		}

		QFileInfo info(file);
		QString libraryName = info.completeBaseName();

		qDebug() << "\nparsing" << filename;

		QDomElement root = doc.documentElement();
        QDomNodeList nodeList = root.elementsByTagName("text");
        QList<QDomElement> toDelete;
        for (int i = 0; i < nodeList.count(); i++) {
            QDomElement element = nodeList.at(i).toElement();
            QString text = element.text();
            if (text.compare(">name", Qt::CaseInsensitive) == 0 || text.compare(">value", Qt::CaseInsensitive) == 0) {
                toDelete.append(element);
            }
        }
        foreach (QDomElement element, toDelete) {
            element.parentNode().removeChild(element);
        }

        prepPCBs(root);
        
		moduleIDs.append(makeFZPs(workingFolder, fzpFolder, breadboardFolder, schematicFolder, pcbFolder, iconFolder, binsFolder, subpartsFolder, libraryName, doc, lbrStream));
        makeSchematics(schematicFolder, libraryName, root);
		makePCBs(workingFolder, pcbFolder, breadboardFolder, subpartsFolder, libraryName, root);

		foreach (QString symbolBaseName, SchematicIcons) {
			QFile file(schematicFolder.absoluteFilePath(symbolBaseName + "_schematic.svg"));
			if (file.open(QFile::ReadOnly)) {
				QString svg = file.readAll();
				file.close();
				int ix = svg.indexOf("<g id='schematic'>");
				if (ix < 0) continue;

				svg.insert(ix, "<g id='icon'>");
				ix = svg.lastIndexOf("</g>");
				if (ix < 0) continue;

				svg.insert(ix, "</g>");
				if (file.open(QFile::WriteOnly)) {
					QTextStream out(&file);
					out.setCodec("UTF-8");
					out << svg;
					file.close();
				}
			}
		}

	}
 
    makeBin(moduleIDs, "all", binsFolder);
    
    QList<QString> removed;

    foreach (PartDescr * partDescr, m_partDescrs.values()) {
        if (!partDescr->matched && !partDescr->fzp.newName.isEmpty()) {
            //qDebug() << QString("Part '%1' no longer exists in the lbr file").arg(partDescr->fzp.newName);
        }

        QList<FileDescr *> fileDescrs;
        fileDescrs << &partDescr->bread << &partDescr->schem << &partDescr->pcb;
        QList<QDir> dirs;
        dirs << breadboardFolder << schematicFolder << pcbFolder;
        foreach (FileDescr * fileDescr, fileDescrs) {
            QDir dir = dirs.takeFirst();
            if (fileDescr->disp.contains("illustrate", Qt::CaseInsensitive) || 
                fileDescr->disp.contains("exist", Qt::CaseInsensitive) || 
                fileDescr->disp.contains("program", Qt::CaseInsensitive)) 
            {
                QFile::remove(dir.absoluteFilePath(fileDescr->newName));
                if (!fileDescr->disp.contains("illustrate")) {
                    removed.append(dir.absoluteFilePath(fileDescr->newName));
                }
                else {
                    //qDebug() << "illustrate" << fileDescr->newName;
                }
                //qDebug() << "removing" << fileDescr->newName;
            }
        }

        if (partDescr->fzp.disp.contains("discard")) {
            //qDebug() << "removing part" << partDescr->fzp.newName;
            //qDebug() << "removing part" << partDescr->bread.newName;
            //qDebug() << "removing part" << partDescr->schem.newName;
            //qDebug() << "removing part" << partDescr->pcb.newName;
            QFile::remove(breadboardFolder.absoluteFilePath(partDescr->bread.newName));
            QFile::remove(schematicFolder.absoluteFilePath(partDescr->schem.newName));
            QFile::remove(pcbFolder.absoluteFilePath(partDescr->pcb.newName));
            QFile::remove(fzpFolder.absoluteFilePath(partDescr->fzp.newName));
            removed.append(breadboardFolder.absoluteFilePath(partDescr->bread.newName));
            removed.append(schematicFolder.absoluteFilePath(partDescr->schem.newName));
            removed.append(pcbFolder.absoluteFilePath(partDescr->pcb.newName));
            removed.append(fzpFolder.absoluteFilePath(partDescr->fzp.newName));

        }
    }

	lbr.close();

    qSort(removed);
    QFile rem(workingFolder.absoluteFilePath("removed.txt"));
	rem.open(QFile::WriteOnly);
	QTextStream remStream(&rem);
    foreach(QString r, removed) {
        remStream << r << "\n";
    }
    rem.close();
}

QStringList LbrApplication::makeFZPs(const QDir & workingFolder, const QDir & fzpFolder, const QDir & breadFolder, const QDir & schematicFolder, const QDir & pcbFolder, const QDir & iconFolder, const QDir & binsFolder,  const QDir & subpartsFolder, const QString & libraryName, QDomDocument & doc, QTextStream & lbrStream)
{
    Q_UNUSED(pcbFolder);
    Q_UNUSED(breadFolder);

	QDomElement root = doc.documentElement();

	QDomElement drawing = root.firstChildElement("drawing");
	if (drawing.isNull()) return QStringList();

	QDomElement library = drawing.firstChildElement("library");
	if (library.isNull()) return QStringList();

	QDomElement deviceSets = library.firstChildElement("devicesets");
	if (deviceSets.isNull()) return QStringList();

	QMultiHash<QString, QString> families;

	QDomElement deviceSet = deviceSets.firstChildElement("deviceset");
	while (!deviceSet.isNull()) {
		QString description;
		TextUtils::findText(deviceSet.firstChildElement("description"), description);

		QStringList symbolNames;
		QDomElement gates = deviceSet.firstChildElement("gates");
		QDomElement gate = gates.firstChildElement("gate");
		bool nonZero = false;
		qreal gx, gy;
		while (!gate.isNull()) {
			QString symbol = gate.attribute("symbol");
			xy(gate, gx, gy);
			if (gx != 0 || gy != 0) {
				nonZero = true;
			}

			symbolNames.append(symbol);
			gate = gate.nextSiblingElement("gate");
		}

		if (symbolNames.count() == 1) {
			if (nonZero) {
				//qDebug() << "non-zero gate" << symbolNames.at(0) << gx << gy;
			}
			QDomElement devices = deviceSet.firstChildElement("devices");
			QDomElement device = devices.firstChildElement("device");
			while (!device.isNull()) {
                QHash<QString, QString> pair = processDevice(workingFolder, fzpFolder, iconFolder, subpartsFolder, device, symbolNames.at(0), description, libraryName, deviceSet.attribute("name"), deviceSet.attribute("prefix"), false, lbrStream);
				if (pair.count() == 1) {
                    QString moduleID = pair.keys().at(0);
                    families.insert(pair.value(moduleID), moduleID);
                }
				device = device.nextSiblingElement("device");
			}
		}
		else if (symbolNames.count() == 0) {
			qDebug() << deviceSet.attribute("name") << "no device set symbols";
		}
		else if (symbolNames.count() > 1) {
			QDomElement tempSymbol = doc.createElement("symbol");
			root.appendChild(tempSymbol);
			QString tempSymbolName = makeJoinedName(symbolNames);
			tempSymbol.setAttribute("name", tempSymbolName);
			//qDebug() << "multiple symbols" << tempSymbolName;

			QDomElement symbols = library.firstChildElement("symbols");

			QDomElement gate = gates.firstChildElement("gate");
			while (!gate.isNull()) {
				QString symbolName = gate.attribute("symbol");
				QString gateName = gate.attribute("name");
				qreal x, y;
				xy(gate, x, y);

				QDomElement symbol = TextUtils::findElementWithAttribute(symbols, "name", symbolName);
				if (symbol.isNull()) {
					qDebug() << "symbol" << symbolName << "not found";
				}

				QDomElement child = symbol.firstChildElement();
				while (!child.isNull()) {
					QDomElement copy = child.cloneNode(false).toElement();

					tempSymbol.appendChild(copy);
					qreal x1, y1, x2, y2;
					if (xy(copy, x1, y1)) {
						copy.setAttribute("x", x1 + x);
						copy.setAttribute("y", y1 + y);
					}
					else if (MiscUtils::x1y1x2y2(copy, x1, y1, x2, y2)) {
						copy.setAttribute("x1", x1 + x);
						copy.setAttribute("y1", y1 + y);
						copy.setAttribute("x2", x2 + x);
						copy.setAttribute("y2", y2 + y);
					}
					else {
						qDebug() << "bad coords in" << symbolName;
					}
					if (copy.tagName().compare("pin") == 0) {
						copy.setAttribute("name", gateName + " " + copy.attribute("name"));
					}

					child = child.nextSiblingElement();
				}

				gate = gate.nextSiblingElement("gate");
			}

			QDomElement devices = deviceSet.firstChildElement("devices");
			QDomElement device = devices.firstChildElement("device");
			while (!device.isNull()) {
				QHash<QString, QString> pair = processDevice(workingFolder, fzpFolder, iconFolder, subpartsFolder, device, tempSymbolName, description, libraryName, deviceSet.attribute("name"), deviceSet.attribute("prefix"), true, lbrStream);
				if (pair.count() == 1) {
                    QString moduleID = pair.keys().at(0);
                    families.insert(pair.value(moduleID), moduleID);
                }
				device = device.nextSiblingElement("device");
			}
			processSymbol(schematicFolder, tempSymbol, libraryName);
		}

		deviceSet = deviceSet.nextSiblingElement("deviceset");
	}

    QStringList moduleIDs;
	foreach (QString family, families.uniqueKeys()) {
		if (family.isEmpty()) continue;

        //qDebug() << family;
        //foreach (QString moduleid, families.values(family)) {
        //    qDebug() << "\t" + moduleid;
        //}
        //qDebug() << "";

        QString moduleID = families.value(family);
        if (moduleID.isEmpty()) continue;

        moduleIDs.append(moduleID);
	}

    makeBin(moduleIDs, libraryName, binsFolder);

    return families.values();

}

void LbrApplication::makePCBs(const QDir & workingFolder, const QDir & pcbFolder, const QDir & breadboardFolder,  const QDir & subpartsFolder, const QString & libraryName, const QDomElement & root)
{
	QDomElement drawing = root.firstChildElement("drawing");
	if (drawing.isNull()) return;

	QDomElement library = drawing.firstChildElement("library");
	if (library.isNull()) return;

	QDomElement packages = library.firstChildElement("packages");
	if (packages.isNull()) return;

	QDomElement package = packages.firstChildElement("package");
	while (!package.isNull()) {
		processPackage(workingFolder, subpartsFolder, pcbFolder, breadboardFolder, package, libraryName);
		package = package.nextSiblingElement("package");
	}

    foreach (QString name, SquareNames) {
        qDebug() << "square" << (libraryName.toLower() + "_" + name.toLower() + "_pcb.svg");
    }

    SquareNames.clear();
}

void LbrApplication::prepPCBs(const QDomElement & root)
{
	QDomElement drawing = root.firstChildElement("drawing");
	if (drawing.isNull()) return;

	QDomElement library = drawing.firstChildElement("library");
	if (library.isNull()) return;

	QDomElement packages = library.firstChildElement("packages");
	if (packages.isNull()) return;

	QDomElement package = packages.firstChildElement("package");
	while (!package.isNull()) {
		prepPackage(package);
		package = package.nextSiblingElement("package");
	}
}


void LbrApplication::makeSchematics(const QDir & schematicFolder, const QString & libraryName, const QDomElement & root)
{
	QDomElement drawing = root.firstChildElement("drawing");
	if (drawing.isNull()) return;

	QDomElement library = drawing.firstChildElement("library");
	if (library.isNull()) return;

	QDomElement symbols = library.firstChildElement("symbols");
	if (symbols.isNull()) return;

	QDomElement symbol = symbols.firstChildElement("symbol");
	while (!symbol.isNull()) {
		processSymbol(schematicFolder, symbol, libraryName);
		symbol = symbol.nextSiblingElement("symbol");
	}
}

QHash<QString, QString> LbrApplication::processDevice(const QDir & workingFolder, const QDir & fzpFolder,  const QDir & iconFolder, const QDir & subpartsFolder, const QDomElement & device, const QString & symbol, QString description, const QString & libraryName, const QString & deviceSetName, const QString & deviceSetPrefix, bool useGate, QTextStream & lbrStream)
{
	//qDebug() << "processing device" << device.attribute("name");

	bool schematicOnly = device.attribute("package", "").isEmpty();
	
	QString deviceName = cleanChars2(device.attribute("name"));
	QString package = device.attribute("package");
	QString ccPackage = cleanChars(package);
	QString ccDeviceSetName = cleanChars2(deviceSetName);

    QString packageBaseName = libraryName.toLower() + "_" + ccPackage.toLower();
	QString symbolBaseName = libraryName.toLower() + "_" + cleanChars(symbol).toLower();
	QString breadboardBaseName = libraryName.toLower() + "_" + ccPackage.toLower() + "_breadboard.svg";

    QString schematicBaseName = QString("%1_schematic.svg").arg(symbolBaseName);
    QString pcbBaseName = QString("%1_pcb.svg").arg(packageBaseName);
 
    QString fzp = "<?xml version='1.0' encoding='UTF-8'?>\n";

	QString moduleID = QString("%1-%2-%3").arg(libraryName).arg(ccDeviceSetName).arg(deviceName);
	moduleID = cleanChars2(moduleID);
	QString fzpName = cleanChars(moduleID).toLower() + ".fzp";

    PartDescr * partDescr = m_partDescrs.value(fzpName, NULL);
    if (partDescr == NULL) {
        qDebug() << QString("Part descriptor for '%1' not found in google doc").arg(fzpName);
    }
    else {
        partDescr->matched = true;
    }

	fzp += QString("<module fritzingVersion='%2' moduleId='%1'>\n").arg(moduleID).arg("0.7.2b");
	fzp += QString("<!-- generated from Sparkfun Eagle Library '%1' %2 %3 -->\n").arg(libraryName).arg(ccDeviceSetName).arg(deviceName);
	fzp += QString("<version>4</version>\n");
    fzp += QString("<date>%1</date>\n").arg(QDate::currentDate().toString(Qt::ISODate));
	fzp += QString("<label>%1</label>\n").arg((deviceSetPrefix.isEmpty()) ? "U" : deviceSetPrefix);

	QString author = "www.fritzing.org";
	QString title = ccDeviceSetName;

	int ix1 = description.indexOf("<b>");
	int ix2 = description.indexOf("</b>");
	if (ix1 >= 0 && ix2 > ix1) {
		title = cleanChars2(description.mid(ix1 + 3, ix2 - ix1 - 3));
	}

	QSet<QString> tags;
	QHash<QString, QString> properties;

	if (!deviceName.isEmpty()) {
		properties.insert("variant", deviceName.toLower());
		tags << deviceName;
	}

	tags << ccDeviceSetName;
	tags << cleanChars2(package);

    QString familyName = ccDeviceSetName.toLower();
	properties.insert("family", familyName);
	properties.insert("package", cleanChars2(package).toLower());

    bool breakout = false;
    bool throughHole = !AllSMDs.contains(ccPackage);

    bool usePinheader = false;

    bool discarded = false;
    if (partDescr == NULL) {
        if (!throughHole) {
            Breakouts.insert(package, "_____");
            breakout = true;
        }
    }
    else {
        discarded = partDescr->fzp.disp.contains("discard", Qt::CaseInsensitive);
        if (partDescr->bread.disp.contains("exist", Qt::CaseInsensitive)) {
            if (!partDescr->bread.oldName.isEmpty()) {
                QFileInfo info(partDescr->bread.oldName);
                breadboardBaseName = info.fileName();
            }
            else {
                qDebug() << "existing: missing name" << partDescr->bread.newName;
            }
        }
        else if (partDescr->bread.disp.contains("breakout", Qt::CaseInsensitive)) {
            Breakouts.insert(package, "_____");
            breakout = true;
        }
        else if (partDescr->bread.disp.contains("program", Qt::CaseInsensitive)) {
            if (!partDescr->bread.oldName.isEmpty()) {
                QFileInfo info(partDescr->bread.oldName);
                breadboardBaseName = info.fileName();
            }
            else {
                 //qDebug() << "programmatic: missing name" << partDescr->bread.newName;
           }
        }

        if (!partDescr->title.isEmpty() && partDescr->title.compare(ccDeviceSetName) != 0) {
            title = partDescr->title;
        }

        if (!partDescr->description.isEmpty()) {
            description = partDescr->description;
        }

        if (!partDescr->family.isEmpty()) {
	        properties.insert("family", partDescr->family);
        }

        if (!partDescr->tags.isEmpty()) {
	        QStringList ts = partDescr->tags.split("\n", QString::SkipEmptyParts);
            foreach (QString t, ts) { 
                tags << t.trimmed();
                //qDebug() << t.trimmed();
            }
        }
        if (!partDescr->props.isEmpty()) {
	        QStringList ps = partDescr->props.split("\n", QString::SkipEmptyParts);
            foreach (QString p, ps) {
                QStringList qs = p.split(":", QString::SkipEmptyParts);
                if (qs.count() == 2) {
                    properties.insert(qs.at(0).trimmed(), qs.at(1).trimmed());
                    //qDebug() << qs.at(0).trimmed() << qs.at(1).trimmed();
                }
            }
        }

        QRegExp onex("1x(\\d\\d)");
        QRegExp molex("molex-1x(\\d{1,2})");
        QString name = package.toLower();
        if (onex.indexIn(name) == 0) {
            //qDebug() << "got 1x" << package;
            int pins = onex.cap(1).toInt();
            if (name.length() == 4) {
                pcbBaseName = QString("nsjumper_%1_100mil_pcb.svg").arg(pins);
                breadboardBaseName = QString("generic_female_pin_header_%1_100mil_bread.svg").arg(pins);
                //qDebug() << "using 1x" << pcbBaseName;
                usePinheader = true;
            }
            if (name.length() == 9 && name.endsWith("lock")) {
                pcbBaseName = QString("nsjumper_alternating_%1_100mil_pcb.svg").arg(pins);
                breadboardBaseName = QString("generic_female_pin_header_%1_100mil_bread.svg").arg(pins);
                //qDebug() << "using 1x" << pcbBaseName;
                usePinheader = true;
            }
            if (name.contains("longpad")) {
                QString type = "longpad";
                if (name.contains("lock")) type += "_alternating";
                pcbBaseName = QString("%1_%2_100mil_pcb.svg").arg(type).arg(pins);
                breadboardBaseName = QString("generic_female_pin_header_%1_100mil_bread.svg").arg(pins);
                //qDebug() << "using 1x" << pcbBaseName;
                usePinheader = true;
            }
        }
        else if (molex.indexIn(name) == 0) {
            //qDebug() << "got molex" << package;
            int pins = molex.cap(1).toInt();            
            if (molex.cap(0).length() == name.length()) {
                pcbBaseName = QString("molex_%1_100mil_pcb.svg").arg(pins);
                breadboardBaseName = QString("generic_female_pin_header_%1_100mil_bread.svg").arg(pins);
                //qDebug() << "using molex" << pcbBaseName;
                usePinheader = true;
            }
        }

        if (usePinheader) {
            partDescr->bread.disp = "programmatic";
            partDescr->pcb.disp = "programmatic";
        }
    }

	QString url;
	
	fzp += QString("<author>%1</author>\n").arg(TextUtils::escapeAnd(author));
	fzp += QString("<description>%1</description>\n").arg(TextUtils::escapeAnd(description));
	fzp += QString("<title>%1</title>\n").arg(TextUtils::escapeAnd(title));
	if (!url.isEmpty()) {
		fzp += QString("<url>%1</url>\n").arg(TextUtils::escapeAnd(url));
	}
	fzp += QString("<tags>\n");    
	foreach (QString tag, tags) {
		fzp += QString("<tag>%1</tag>\n").arg(TextUtils::escapeAnd(tag));
	}
	fzp += QString("</tags>\n");
	fzp += QString("<properties>\n");
	foreach (QString prop, properties.keys()) {
		fzp += QString("<property name='%1'>%2</property>\n")
			.arg(TextUtils::escapeAnd(prop))
			.arg(TextUtils::escapeAnd(properties.value(prop)));
	}
	fzp += QString("</properties>\n");

	fzp += QString("<views>\n");

    fzp += QString("<breadboardView>\n");
	if (schematicOnly) {
		SchematicIcons.insert(symbolBaseName);
		fzp += QString("<layers image='schematic/%1'>\n").arg(schematicBaseName);
		fzp += QString("<layer layerId='schematic'/>\n");
	}
	else {
		fzp += QString("<layers image='breadboard/%1'>\n").arg(breadboardBaseName);
		fzp += QString("<layer layerId='breadboard'/>\n");
	}
    fzp += QString("</layers>\n");
    fzp += QString("</breadboardView>\n");

    fzp += QString("<schematicView>\n");
    fzp += QString("<layers image='schematic/%1'>\n").arg(schematicBaseName);
    fzp += QString("<layer layerId='schematic'/>\n");
    fzp += QString("</layers>\n");
    fzp += QString("</schematicView>\n");

    fzp += QString("<pcbView>\n");
	if (schematicOnly) {
		fzp += QString("<layers image='schematic/%1'>\n").arg(schematicBaseName);
		fzp += QString("<layer layerId='schematic'/>\n");
	}
	else {
		fzp += QString("<layers image='pcb/%1'>\n").arg(pcbBaseName);
		fzp += QString("<layer layerId='copper1'/>\n");
	}
    fzp += QString("<layer layerId='silkscreen'/>\n");


	if (throughHole) {
		fzp += QString("<layer layerId='copper0'/>\n");
	}
    fzp += QString("</layers>\n");
    fzp += QString("</pcbView>\n");

    fzp += QString("<iconView>\n");
	if (schematicOnly) {
		fzp += QString("<layers image='schematic/%1'>\n").arg(schematicBaseName);
	}
	else {
        QString subpartName = package;
        if (partDescr != NULL && !partDescr->useSubpart.isEmpty()) {
            subpartName = partDescr->useSubpart;
            //qDebug() << "using subpart" << subpartName;
        }
        QString path = subpartsFolder.absoluteFilePath("breadboard/" + subpartName + ".svg");
        QFileInfo info(path); 
        if (info.exists()) {
            Breakouts.insert(package, subpartName);
		    fzp += QString("<layers image='icon/%1.svg'>\n").arg(subpartName);
            QFile icon(path);
            icon.copy(iconFolder.absoluteFilePath(subpartName + ".svg"));
        }
        else {
		    fzp += QString("<layers image='breadboard/%1'>\n").arg(breadboardBaseName);
        }
	}
	fzp += QString("<layer layerId='icon'/>\n");
    fzp += QString("</layers>\n");
    fzp += QString("</iconView>\n");

    fzp += QString("</views>\n");
    fzp += QString("<connectors>\n");

    //int symbolConnectorIndex = 0;

	QDomElement connects = device.firstChildElement("connects");
    QList<QDomElement> connectElements;
    QDomElement connect = connects.firstChildElement("connect");
    while (!connect.isNull()) {
        connectElements.append(connect);
        connect = connect.nextSiblingElement("connect");
    }

    QMultiHash<QString, QString> buses;

	foreach (QDomElement connect, connectElements) {
		QString padProp = package + PropSeparator + connect.attribute("pad");
		QString gate;
		if (useGate) {
			gate = connect.attribute("gate") + " ";
		}
		QString padValue = PackageConnectors.value(padProp, "");
        if (padValue.isEmpty()) {
            qDebug() << "no padValue for" << padProp;
        }
		QString connectorID(padValue);

		QString symbolProp = symbol + PropSeparator + gate + connect.attribute("pin");
		QString symbolValue = SymbolConnectors.value(symbolProp, "");
        if (symbolValue.isEmpty()) {
            symbolValue = padValue; // QString("connector%1pin").arg(symbolConnectorIndex++);
            symbolValue.replace("pad", "pin");
            SymbolConnectors.insert(symbolProp, symbolValue);
        }

		int p = symbolProp.lastIndexOf(PropSeparator);
        bool onBus = false;
        QString preName = symbolProp.mid(p + PropSeparator.length());
        int ix = preName.indexOf('@');
        if (ix >= 0) {
            onBus = true;
            preName.truncate(ix);
        }
		QString connectorName = cleanChars2(preName); 


		connectorID.chop(3);
        buses.insert(preName, connectorID);
		QString connectorType = "male";
		QString terminalID(symbolValue);
		terminalID.chop(3);
		fzp += QString("<connector id='%1' type='%2' name='%3'>\n").arg(connectorID).arg(connectorType).arg(connectorName);
		fzp += QString("<description>%1</description>\n").arg(connectorName);
		fzp += QString("<views>\n");
		fzp += QString("<breadboardView>\n");
		if (schematicOnly) {
			fzp += QString("<p layer='schematic' svgId='%1' terminalId='%2terminal'/>\n").arg(symbolValue).arg(terminalID);
		}
		else {
			QString pv = padValue;
            pv.replace("pad", "pin");
			fzp += QString("<p layer='breadboard' svgId='%1'/>\n").arg(pv);
		}
		fzp += QString("</breadboardView>\n");
		fzp += QString("<schematicView>\n");
		fzp += QString("<p layer='schematic' svgId='%1' terminalId='%2terminal'/>\n").arg(symbolValue).arg(terminalID);
		fzp += QString("</schematicView>\n");
		fzp += QString("<pcbView>\n");
		if (schematicOnly) {
			fzp += QString("<p layer='schematic' svgId='%1' terminalId='%2terminal'/>\n").arg(symbolValue).arg(terminalID);
		}
		else {
            QString pv = padValue;
            if (usePinheader) pv.replace("pad", "pin");

			if (PackageConnectorTypes.value(padProp).compare("pad", Qt::CaseInsensitive) == 0) {
				fzp += QString("<p layer='copper0' svgId='%1'/>\n").arg(pv);
			}
			fzp += QString("<p layer='copper1' svgId='%1'/>\n").arg(pv);
		}
		fzp += QString("</pcbView>\n");
		fzp += QString("</views>\n");
		fzp += QString("</connector>\n");
	}

    fzp += QString("</connectors>\n");
    if (buses.count() > 0) {
        fzp += QString("<buses>\n");
        foreach (QString busName, buses.uniqueKeys()) {
            QStringList members = buses.values(busName);
            if (members.count() > 1) {
                fzp += QString("<bus id='%1'>\n").arg(cleanChars2(busName));
                foreach (QString busMember, members) {
                    fzp += QString("<nodeMember connectorId='%1' />\n").arg(busMember);
                }
                fzp += QString("</bus>\n");
            }
            
        }
        fzp += QString("</buses>\n");
    }
	fzp += QString("</module>\n");

	QFile file(fzpFolder.absoluteFilePath(fzpName));
	if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		QTextStream out(&file);
		out.setCodec("UTF-8");
		out << fzp;
		file.close();
	}

	QString defaultBehavior = "new";
	if (ccPackage.toLower().startsWith("1x") || ccPackage.toLower().startsWith("2x")) {
		defaultBehavior = "programmatic";
	}
	QString dqDescription(description);
	dqDescription.replace("\"", "\"\"");
    QStringList lbrStrings;
	lbrStrings << fzpName << "," << defaultBehavior << ",,,,";
	lbrStrings << packageBaseName << "_breadboard.svg,";
	
	if (false) {
		lbrStrings << "existing," << breadboardBaseName;
	}
	else {
		lbrStrings << defaultBehavior;
		lbrStrings << ",";
	}
	lbrStrings << ",";
	lbrStrings << schematicBaseName << ",";
	lbrStrings << defaultBehavior << ",,";
	lbrStrings << pcbBaseName << ",";
	lbrStrings << defaultBehavior << ",,";
	lbrStrings << "\"" << title << "\",";
	lbrStrings << "\"" << dqDescription << "\",";
	lbrStrings << ccDeviceSetName.toLower() << ",";

	QString props;
	lbrStrings << "\"";
	foreach (QString prop, properties.keys()) {
		if (prop.compare("family") == 0) continue;

		props.append(prop);
		props.append(":");
		props.append(properties.value(prop));
		props.append("\n");
	}
	props.chop(1);
	lbrStrings << props << "\"" << ",";
    QStringList ts = tags.toList();
	lbrStrings << "\"" << ts.join("\n") << "\"" << ",";
	lbrStrings << "\n";

    foreach (QString string, lbrStrings) {
        lbrStream << string;
    }

    if (partDescr == NULL) {
	    QFile newfile(workingFolder.absoluteFilePath("newlbrs.csv"));
	    newfile.open(QFile::Append);
	    QTextStream newStream(&newfile);
	    newStream.setCodec("UTF-8");
        foreach (QString string, lbrStrings) {
            newStream << string;
        }
    }

    QHash<QString, QString> pair;
	if (discarded) return pair;
        
    pair.insert(moduleID, familyName);
    return pair;
}	

void LbrApplication::processPackage(const QDir & workingFolder, const QDir & subpartsFolder, const QDir & pcbFolder, const QDir & breadboardFolder, const QDomElement & package, const QString & libraryName)
{
	QString packageName = package.attribute("name");
	//qDebug() << "processing package" << packageName;
	QString ccPackageName = cleanChars(packageName);
	
	int nonconnectorIndex = 0;

	QDomElement child = package.firstChildElement();
	bool gotTPlace = false;
	while (!child.isNull()) {
		if (child.attribute("layer", "").compare(TPlaceLayer) == 0) {
			gotTPlace = true;
			break;
		}
		child = child.nextSiblingElement();
	}

	// if there is no silkscreen, try another layer
	if (!gotTPlace) {
		child = package.firstChildElement();
		while (!child.isNull()) {
			if (child.attribute("layer", "").compare(TDocuLayer) == 0) {
                gotTPlace = true;
				child.setAttribute("layer", TPlaceLayer);	
			}
			child = child.nextSiblingElement();
		}	
	}

	// if there is no silkscreen, try another layer
	if (!gotTPlace) {
		child = package.firstChildElement();
		while (!child.isNull()) {
			if (child.attribute("layer", "").compare(MeasuresLayer) == 0) {
                gotTPlace = true;
				child.setAttribute("layer", TPlaceLayer);	
			}
			child = child.nextSiblingElement();
		}	
	}

	bool gotPad = false;
	bool gotSMD = false;
	QDomElement pad = package.firstChildElement("pad");
	QList<QDomElement> packagePads;
    while (!pad.isNull()) {
		pad.setAttribute("layer", PadLayer);
        packagePads.append(pad);
        QString connectorID = prepConnector(pad, package.attribute("name"), true);
        pad.setAttribute("connectorid", connectorID);
		gotPad = true;
		pad = pad.nextSiblingElement("pad");
	}
	QDomElement hole = package.firstChildElement("hole");
	while (!hole.isNull()) {
		hole.setAttribute("layer", PadLayer);
		hole = hole.nextSiblingElement("hole");
	}
	QList<QDomElement> packageSMDs;
	QDomElement smd = package.firstChildElement("smd");
	while (!smd.isNull()) {
		packageSMDs.append(smd);
        QString connectorID = prepConnector(smd, package.attribute("name"), true);
        smd.setAttribute("connectorid", connectorID);
		gotSMD = true;
		smd.setAttribute("layer", SMDLayer);
		smd = smd.nextSiblingElement("smd");
	}

	if (gotPad && gotSMD) {
		//qDebug() << "package" << package.attribute("name") << "has both smd and tht";
	}

    if (!gotPad && !gotSMD) {
		//qDebug() << "package" << package.attribute("name") << "missing both smd and tht";
    }

	QStringList layers;
	layers << PadLayer << SMDLayer << TPlaceLayer;
	QRectF dimensions = getDimensions(package, layers);

	QString header = TextUtils::makeSVGHeader(25.4, 25.4, dimensions.width(), dimensions.height());
	QString svg = header;

    QHash<QString, QString> colors;
    colors.insert("all", CopperColor);

	svg += "<g id='copper1'>\n";	
	layers.clear();
	layers << SMDLayer << PadLayer;
	QString copper1String;
	toSvg(package, layers, dimensions, colors, nonconnectorIndex, true, copper1String);
	svg += copper1String;
	svg += "<g id='copper0'>\n";
	layers.clear();
	layers << PadLayer;
	toSvg(package, layers, dimensions, colors, nonconnectorIndex, true, svg);
	svg += "</g>\n";
	svg += "</g>\n";

    colors.insert("all", SilkColor);
	layers.clear();
	layers << TPlaceLayer;
	svg += "<g id='silkscreen'>\n";
	toSvg(package, layers, dimensions, colors, nonconnectorIndex, true, svg);
	svg += "</g>\n";
	svg += "</svg>";

	QFile file(pcbFolder.absoluteFilePath(libraryName.toLower() + "_" + ccPackageName.toLower() + "_pcb.svg"));
	if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		QTextStream out(&file);
		out.setCodec("UTF-8");
		out << svg;
		file.close();
	}

    QString subpartName = Breakouts.value(packageName);
	if (!subpartName.isEmpty()) {
        bool noText = false;
        QString boardName = ccPackageName;
		// remove libraryName so package can be used by all libraries     
        QString path = subpartsFolder.absoluteFilePath("breadboard/" + subpartName + ".svg");
        QFileInfo info(path); 

        QSizeF innerChipSize, outerChipSize;
        SvgFileSplitter splitter;
        if (!info.exists()) {
            QString c1 = copper1String;
            c1.replace(CopperColor, "#8c8c8c");
		    QString svg = header + c1 + "</svg>";
		    splitter.load(svg);
            QStringList layers;
		    layers << TPlaceLayer;
		    QRectF innerDimensions = getDimensions(package, layers);
            innerChipSize = QSizeF(innerDimensions.width() * 1000 / 25.4, innerDimensions.height() * 1000 / 25.4);
            outerChipSize = QSizeF(dimensions.width() * 1000 / 25.4, dimensions.height() * 1000 / 25.4);
            double factor;
            splitter.normalize(1000, "", false, factor);
            QString svg2 = splitter.toString();
        }
        else {
            QFile file(path);
            bool success = splitter.load(&file);
            if (!success) {
                qDebug() << "unable to load subpart" << path;
                return;
            }

            double factor;
            splitter.normalize(1000, "", false, factor);
            outerChipSize = TextUtils::parseForWidthAndHeight(splitter.toString()) * 1000;
            noText = true;
        }

        QList<QDomElement> elements(packagePads);
        elements.append(packageSMDs);
        if (elements.count() % 2 == 1) {
            // qDebug() << "odd pins in package" << packageName;
            // deal with SMDs having an odd number of pins
            QDomElement empty = package.ownerDocument().createElement("empty");
            empty.setAttribute("empty", "empty");
            elements.append(empty);
        }

        qSort(elements.begin(), elements.end(), byConnectorID);
        svg = MiscUtils::makeGeneric(workingFolder, "#1F7A34", elements, splitter.toString(), boardName, outerChipSize, innerChipSize, getConnectorName, getConnectorIndex, noText);
	}
	else {
		svg = TextUtils::makeSVGHeader(25.4, 25.4, dimensions.width(), dimensions.height());

		svg += "<g id='breadboard'>\n";	

		int clockwise = 0;
		qreal dr = 0;
		QString path = QString("<path fill='%1' stroke='%2' stroke-width='%3' d='M%4,%5l%6,0 0,%7 -%6,0 0,-%7z\n")
					.arg(BreadboardColor)
					.arg("none")
					.arg(0)
					.arg(0 + dr)
					.arg(0 + dr)
					.arg(dimensions.width() - dr - dr)
					.arg(dimensions.height() - dr - dr);

		pad = package.firstChildElement("pad");
		while (!pad.isNull()) {
			qreal x, y;
			if (xy(pad, x, y)) {
				qreal drill, diameter;
				if (drillDiameter(pad, drill, diameter)) {
					path += genHole(x, y, drill / 2, clockwise, dimensions);
				}
			}
			pad = pad.nextSiblingElement("pad");
		}

		path += "'/>\n";
		svg += path;

        colors.insert("all", BreadboardCopperColor);
		layers.clear();
		layers << SMDLayer << PadLayer;
		toSvg(package, layers, dimensions, colors, nonconnectorIndex, true, svg);

        colors.insert("all", BreadboardSilkColor);
		layers.clear();
		layers << TPlaceLayer;
		toSvg(package, layers, dimensions, colors, nonconnectorIndex, true, svg);

		svg += "</g>\n";
		svg += "</svg>";

        svg.replace(QRegExp("connector(\\d+)pad"), "connector\\1pin");
	}

	QString fn = libraryName.toLower() + "_" + ccPackageName.toLower() + "_breadboard.svg";
	QFile file2(breadboardFolder.absoluteFilePath(fn));
	if (file2.open(QIODevice::WriteOnly | QIODevice::Text)) {
		QTextStream out(&file2);
		out.setCodec("UTF-8");
		out << svg;
		file2.close();
	}
}

void LbrApplication::processSymbol(const QDir & schematicFolder, QDomElement & symbol, const QString & libraryName)
{
	QString symbolName = symbol.attribute("name");

	//qDebug() << "processing symbol" << symbolName;
	symbolName = cleanChars(symbolName);

	int nonconnectorIndex = 0;

    QHash<QString, QDomElement> buses;
    QDomElement empty;
	QDomElement pin = symbol.firstChildElement("pin");
	while (!pin.isNull()) {
		pin.setAttribute("layer", SchematicLayer);
		pin.setAttribute("width", SchematicRectConstants::PinWidth);
        QString name = pin.attribute("name");
        int ix = name.indexOf('@');
        if (ix >= 0) {
            QString busName = name;
            busName.truncate(ix);
            QDomElement previous = buses.value(busName, empty);
            if (previous.isNull()) {
                pin.setAttribute("bus-name", busName);
                pin.setAttribute("bus-master", "true");
                buses.insert(busName, pin);
            }
            else {
                QDomNamedNodeMap nnm = previous.attributes();
                for (uint i = 0; i < nnm.length(); i++) {
                    QDomNode node = nnm.item(i);
			        pin.setAttribute(node.nodeName(), node.nodeValue());
                    // qDebug() << "copy" << node.nodeName() << node.nodeValue();
                }
                pin.setAttribute("name", name);
                pin.removeAttribute("bus-master");
                pin.setAttribute("width", 0);
            }
        }   
		pin = pin.nextSiblingElement("pin");
	}

	QStringList layers;
	layers << SchematicLayer;
	QRectF dimensions = getDimensions(symbol, layers);

    QHash<QString, QString> colors;
    colors.insert("all", SchematicRectConstants::RectStrokeColor);
    colors.insert("pin", SchematicRectConstants::PinColor);
    colors.insert("pintext", SchematicRectConstants::PinTextColor);
    colors.insert("text", "#505050");  // for other random texts             
    colors.insert("wire", SchematicRectConstants::RectStrokeColor);
    colors.insert("rectfill", SchematicRectConstants::RectFillColor);
    colors.insert("title", SchematicRectConstants::TitleColor);

	QString svg = TextUtils::makeSVGHeader(25.4, 25.4, dimensions.width(), dimensions.height());
	svg += "<g id='schematic'>\n";
	toSvg(symbol, layers, dimensions, colors, nonconnectorIndex, false, svg);
	svg += "</g>\n";
	svg += "</svg>";

	QFile file(schematicFolder.absoluteFilePath(libraryName.toLower() + "_" + symbolName.toLower() + "_schematic.svg"));
	if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		QTextStream out(&file);
		out.setCodec("UTF-8");
		out << svg;
		file.close();
	}
}

void LbrApplication::toSvg(const QDomElement & root, const QStringList & layers, const QRectF & bounds, const QHash<QString, QString> & colors, int & nonconnectorIndex, bool package, QString & svg) 
{
    bool isRectangular = false;
    if (!package) {
        isRectangular = checkRectangular(root, bounds, colors, svg);
    }

	QDomElement element = root.firstChildElement();
    QString rootName = root.attribute("name");
    QList<QDomElement> wires;
	while (!element.isNull()) {
		QString tagName = element.tagName();
		if (tagName.compare("pin") == 0) ;
		else if (tagName.compare("pad") == 0) ;
		else if (tagName.compare("smd") == 0) ;
		else if (tagName.compare("rectangle") == 0) ;
		else if (tagName.compare("circle") == 0) ;
		else if (tagName.compare("text") == 0) ;
		else if (tagName.compare("wire") == 0) ;
		else if (tagName.compare("polygon") == 0) ;
		else if (tagName.compare("hole") == 0) ;
		else if (tagName.compare("description") == 0) ;
		else {
			qDebug() << "unknown tag" << tagName;
		}
		QString elementLayer = element.attribute("layer");
		if (layers.contains(elementLayer)) {
			toSvg(element, bounds, colors, nonconnectorIndex, rootName, package, isRectangular, svg);
		}

		element = element.nextSiblingElement();
	}

}

void LbrApplication::toSvg(QDomElement & element, const QRectF & bounds, const QHash<QString, QString> & colors, int & nonconnectorIndex, const QString & name, bool package, bool isRectangular, QString & svg) 
{
	QString tagName = element.tagName();

    QString color = colors.value("all");

	if (tagName.compare("circle") == 0) {
		qreal cx, cy, radius, width;
		if (!cxcyrw(element, cx, cy, radius, width)) {
			qDebug() << "circle missing attributes";
			return;
		}

		svg += QString("<circle class='other' cx='%1' cy='%2' r='%3' stroke='%4' stroke-width='%5' fill='none' />\n")
					.arg(cx - bounds.left())
					.arg(bounds.bottom() - cy)
					.arg(radius)
					.arg(color)
					.arg(width)
					;
		return;
	}

	if (tagName.compare("hole") == 0) {
		qreal cx, cy;
		if (!xy(element, cx, cy)) return;

		bool ok;
		qreal drill = element.attribute("drill", "").toDouble(&ok);
		if (!ok) return;

		svg += QString("<circle cx='%1' cy='%2' r='%3' stroke='black' stroke-width='0' fill='black' id='nonconn%4' />\n")
					.arg(cx - bounds.left())
					.arg(bounds.bottom() - cy)
					.arg(drill / 2)
					.arg(nonconnectorIndex++);
					;
		return;
	}

	if (tagName.compare("pad") == 0) {
        makePad(element, bounds, color, name, package, svg);
		return;
	}

	if (tagName.compare("text") == 0) {
        QString textColor = colors.value("text", color);
		genText(element, bounds, textColor, svg);
		return;
	}

	if (tagName.compare("pin") == 0) {
        QString pinColor = colors.value("pin", color);
        QString pinTextColor = colors.value("pintext", color);
        makePin(element, bounds, pinColor, pinTextColor, name, package, isRectangular, svg);
		return;
	}

	if (tagName.compare("wire") == 0) {
        QString wireColor = colors.value("wire", color);
        makeWire(element, bounds, wireColor, svg);
		return;
	}

	if (tagName.compare("rectangle") == 0) {
		qreal x1, y1, x2, y2;
		if (!MiscUtils::x1y1x2y2(element, x1, y1, x2, y2)) {
			qDebug() << "rectangle missing attributes";
			return;
		}

		svg += QString("<rect class='other' x='%1' y='%2' width='%3' height='%4' stroke='%5' stroke-width='%6' fill='%7' stroke-linecap='round'/>\n")
					.arg(x1 - bounds.left())
					.arg(bounds.bottom() - y2)
					.arg(x2 - x1)
					.arg(y2 - y1)
					.arg("none")
					.arg(0)
					.arg(color)
					;
		return;
	}

	if (tagName.compare("smd") == 0) {
        makeSmd(element, bounds, color, name, package, svg);
		return;
	}

	if (tagName.compare("polygon") == 0 ) {
        makePolygon(element, bounds, color, svg);
		return;
	}

	qDebug() << "toSvg missed tag" << tagName;
}

bool LbrApplication::checkRectangular(const QDomElement & root, const QRectF & bounds, const QHash<QString, QString> & colors, QString & svg) {
    QDomElement element = root.firstChildElement();

    QList<QDomElement> wires;
	while (!element.isNull()) {
		QString tagName = element.tagName();
		if (tagName.compare("wire") == 0) {
            qreal x1, y1, x2, y2;
            MiscUtils::x1y1x2y2(element, x1, y1, x2, y2);
            if (x1 == x2 || y1 == y2) {
                QString wireSvg;
                int nco = 0;
                toSvg(element, bounds, colors, nco, "", false, false, wireSvg);
                if (wireSvg.contains("<line")) {
                    wires.append(element);
                }
            }
        }
        element = element.nextSiblingElement();
    }

    if (wires.count() < 4) return false;

    QList<WireTree *> wireTrees;
    bool isClosed = MiscUtils::makeWireTrees(wires, wireTrees);
    bool isRectangular = false;
    if (isClosed && wireTrees.count() == 4) {
        isRectangular = true;
    }
    else {
        int succeededCount = 0;
        foreach (WireTree * wireTree, wireTrees) {
            if (wireTree->failed) continue;

            succeededCount++;
        }

        // assume this is a rectangle, though there could be further checking...
        isRectangular = (succeededCount == 4); 
    }

    if (isRectangular) {
        qreal minx = std::numeric_limits<double>::max();
        qreal miny = std::numeric_limits<double>::max();
        qreal maxx = std::numeric_limits<double>::min();
        qreal maxy = std::numeric_limits<double>::min();

        foreach (WireTree * wireTree, wireTrees) {
            wireTree->element.setAttribute("width", SchematicRectConstants::RectStrokeWidth);
            if (wireTree->x1 < minx) minx = wireTree->x1;
            if (wireTree->x1 > maxx) maxx = wireTree->x1;
            if (wireTree->y1 < miny) miny = wireTree->y1;
            if (wireTree->y1 > maxy) maxy = wireTree->y1;
            if (wireTree->x2 < minx) minx = wireTree->x2;
            if (wireTree->x2 > maxx) maxx = wireTree->x2;
            if (wireTree->y2 < miny) miny = wireTree->y2;
            if (wireTree->y2 > maxy) maxy = wireTree->y2;
        }

         
        QString color = colors.value("all");

        QString r = QString("<rect class='interior rect' x='%1' y='%2' width='%3' height='%4' stroke='none' stroke-width='0' fill='%5' />\n")
                    .arg(minx - bounds.left())
                    .arg(bounds.bottom() - maxy)
                    .arg(maxx - minx)
                    .arg(maxy - miny)
                    .arg(SchematicRectConstants::RectFillColor);

        QStringList names;
        names << root.attribute("name");
        TextUtils::resplit(names, " ");
        TextUtils::resplit(names, "_");
        TextUtils::resplit(names, "-");
        TextUtils::resplit(names, ".");

        double y = bounds.bottom() - ((maxy + miny) / 2);
        y -= names.count() * (SchematicRectConstants::LabelTextHeight + SchematicRectConstants::LabelTextSpace) / 2;
        foreach (QString name, names) {
            r += QString("<text class='text' id='label' font-family=\"'Droid Sans'\" stroke='none' stroke-width='%4' fill='%5' font-size='%1' x='%2' y='%3' text-anchor='middle'>%8</text>\n")
				    .arg(SchematicRectConstants::LabelTextHeight)
				    .arg(((maxx + minx) / 2) - bounds.left())
				    .arg(y)
				    .arg(0)  // SW(width)
				    .arg(SchematicRectConstants::TitleColor) 
                    .arg(name)
                    ; 
            y += (SchematicRectConstants::LabelTextHeight + SchematicRectConstants::LabelTextSpace);
            svg.append(r);
        }
    }


    foreach (WireTree * wireTree, wireTrees) delete wireTree;
    return isRectangular;
}




void LbrApplication::makePolygon(QDomElement & element, const QRectF & bounds, const QString & color, QString & svg)
{
	QDomElement vertex = element.firstChildElement("vertex");
	QString d;
	bool first = true;
	bool didCurve = false;
	while (!vertex.isNull()) {
		qreal x, y;
		if (!xy(vertex, x, y)) break;

		if (first) {
			d += QString("M%1,%2").arg(x - bounds.left()).arg(bounds.bottom() - y);
		}
		bool cok;
		qreal curve = vertex.attribute("curve").toDouble(&cok);
		if (cok && curve != 0) {
			// qDebug() << "polygon curve" << name;

			if (!(first || didCurve)) {
				d += QString("L%2,%3").arg(x - bounds.left()).arg(bounds.bottom() - y);
			}

			QDomElement next = vertex.nextSiblingElement("vertex");
			if (next.isNull()) next = element.firstChildElement("vertex");
			qreal x2, y2;
			xy(next, x2, y2);

			d += genArcString(x, y, x2, y2, curve, bounds);
			didCurve = true;
		}
		else {
			if (!(first || didCurve)) {
				d += QString("L%2,%3").arg(x - bounds.left()).arg(bounds.bottom() - y);
			}
			didCurve = false;
		}
		first = false;
		//qDebug() << "path" << d;
		vertex = vertex.nextSiblingElement("vertex");
	}

	svg += QString("<path class='other' stroke='%1' fill='%2' stroke-width='%3' d='%4z' stroke-linecap='round'/>\n")
		.arg("none")
		.arg(color)
		.arg(0)
		.arg(d);
}

void LbrApplication::makeWire(QDomElement & element, const QRectF & bounds, const QString & color, QString & svg)
{
	qreal x1, y1, x2, y2;
	if (!MiscUtils::x1y1x2y2(element, x1, y1, x2, y2)) {
		qDebug() << "wire missing attributes";
		return;
	}

	bool ok;		
	qreal width = element.attribute("width").toDouble(&ok);
	if (!ok) {
		qDebug() << "wire missing width";
		return;
	}

	double curve = element.attribute("curve", "").toDouble(&ok);
	if ((!ok) || (curve == 0) || (curve == 360)) {
		svg += QString("<line class='other' x1='%1' y1='%2' x2='%3' y2='%4' stroke='%5' stroke-width='%6' stroke-linecap='round'/>\n")
				.arg(x1 - bounds.left())
				.arg(bounds.bottom() - y1)
				.arg(x2 - bounds.left())
				.arg(bounds.bottom() - y2)
				.arg(color)
				.arg(width)
				;
		return;
	}

	QString d = genArcString(x1, y1, x2, y2, curve, bounds);

	svg += QString("<path class='other' fill='none' d='M%1,%2 %3' stroke-width='%4' stroke='%5' />\n")
			.arg(x1 - bounds.left())
			.arg(bounds.bottom() - y1)
			.arg(d)
			.arg(width)
			.arg(color)
			;
}

void LbrApplication::makeSmd(QDomElement & element, const QRectF & bounds, const QString & color, const QString & name, bool package, QString & svg)
{
	qreal x, y;
	if (!xy(element, x, y)) {
		qDebug() << "smd missing xy";
		return;
	}

	qreal dx, dy;
	if (!dxdy(element, dx, dy)) {
		qDebug() << "smd missing dxdy";
		return;
	}

	qreal angle = getRot(element);
	if (angle == 90 || angle == 270) {
		swap(dx, dy);
	}
	else if (angle == 0 || angle == 180) {
	}
	else {
		qDebug() << "bad smd angle" << angle;
	}

	QString connectorID = prepConnector(element, name, package);

	svg += QString("<rect id='%8' connectorname='%9' x='%1' y='%2' width='%3' height='%4' stroke='%5' stroke-width='%6' fill='%7' stroke-linecap='round'/>\n")
				.arg(x - (dx / 2) - bounds.left())
				.arg(bounds.bottom() - (y + (dy / 2)))
				.arg(dx)
				.arg(dy)
				.arg("none")
				.arg(0)
				.arg(color)
				.arg(connectorID)
				.arg(TextUtils::escapeAnd(element.attribute("name")))
				;
}

void LbrApplication::makePin(QDomElement & element, const QRectF & bounds, const QString & color, const QString & textColor, 
                                const QString & name, bool package, bool isRectangular, QString & svg)
{
	qreal x1, y1, x2, y2;
	if (!parsePin(element, x1, y1, x2, y2)) {
		qDebug() << "pin missing attributes";
		return;
	}

	bool ok;
	qreal width = element.attribute("width").toDouble(&ok);
	if (!ok) {
		qDebug() << "pin missing width";
		return;
	}

	QString connectorID = prepConnector(element, name, package);
	QString terminalID(connectorID);
	terminalID.chop(3);
	terminalID += "terminal";

    bool bus = false;
    QString useName = element.attribute("name");
    if (!element.attribute("bus-name", "").isEmpty()) {
        useName = element.attribute("bus-name");
        bus = true;
        //qDebug() << "got bus" << element.attribute("name") << useName;
    }
	svg += QString("<line class='pin' id='%7' connectorname='%8' x1='%1' y1='%2' x2='%3' y2='%4' stroke='%5' stroke-width='%6' stroke-linecap='round'/>\n")
				.arg(x1 - bounds.left())
				.arg(bounds.bottom() - y1)
				.arg(x2 - bounds.left())
				.arg(bounds.bottom() - y2)
				.arg(color)
				.arg(width)
				.arg(connectorID)
				.arg(TextUtils::escapeAnd(useName))
				;

	svg += QString("<rect class='terminal' id='%3' x='%1' y='%2' width='0.0001' height='0.0001' stroke='none' stroke-width='0' fill='none'/>\n")
				.arg(x1 - bounds.left())
				.arg(bounds.bottom() - y1)
				.arg(terminalID)
				;


    if (useName.isEmpty()) return;
    if (bus) {
        if (element.attribute("bus-master", "").isEmpty()) return;
    }

    static QRegExp indexFinder("connector(\\d*)");
    int id = 0;
    if (indexFinder.indexIn(connectorID) >= 0) {
        id = indexFinder.cap(1).toInt() + 1;
    }

    if (y1 == y2) {
        svg += QString("<text class='text' font-family=%8 stroke='none' stroke-width='%6' fill='%7' font-size='%1' x='%2' y='%3' text-anchor='%4'>%5</text>\n")
					.arg(SchematicRectConstants::PinSmallTextHeight)
					.arg(((x2 + x1) / 2) - bounds.left())
					.arg(bounds.bottom() - y2 - width + SchematicRectConstants::PinSmallTextVert)
					.arg("middle")
					.arg(id)
					.arg(0)  // SW(width)
					.arg(SchematicRectConstants::PinTextColor) 
                    .arg(SchematicRectConstants::FontFamily)
                    ; 
    }
    else if (x1 == x2) {
		svg += QString("<g transform='translate(%1,%2)'><g transform='rotate(%3)'>\n")
			.arg(x2 - width - bounds.left() + SchematicRectConstants::PinSmallTextVert)
			.arg(bounds.bottom() - ((y2 + y1) / 2))
			.arg(270);

	    svg += QString("<text class='text' font-family=%8 stroke='none' stroke-width='%6' fill='%7' font-size='%1' x='%2' y='%3' text-anchor='%4'>%5</text>\n")
						.arg(SchematicRectConstants::PinSmallTextHeight)
						.arg(0)
						.arg(0)
						.arg("middle")
						.arg(id)
						.arg(0)  // SW(width)
						.arg(textColor) 
                        .arg(SchematicRectConstants::FontFamily)
                        ;  

		svg += "</g></g>\n";
    }

    if (isRectangular) {
        bool anchorAtStart = true;
        bool rotate = false;
        double xOffset = 0, yOffset = 0;
        if (x1 == x2) {
            rotate = true;
            anchorAtStart = (y1 < y2);
            yOffset = (anchorAtStart ? -SchematicRectConstants::PinTextIndent : SchematicRectConstants::PinTextIndent);
            xOffset = SchematicRectConstants::PinTextVert;
        }
        else if (y1 == y2) {
            // horizontal pin
            anchorAtStart = (x1 < x2);
            xOffset = (anchorAtStart ? SchematicRectConstants::PinTextIndent : -SchematicRectConstants::PinTextIndent);
            yOffset = SchematicRectConstants::PinTextVert;
        }
        else {
            return;
        }

        if (rotate) {
		    svg += QString("<g transform='translate(%1,%2)'><g transform='rotate(%3)'>\n")
			    .arg(x2 - bounds.left() + xOffset)
			    .arg(bounds.bottom() - y2 + yOffset)
			    .arg(270);
		    x2 = bounds.left();
		    y2 = bounds.bottom();
            xOffset = yOffset = 0;
	    }

	    svg += QString("<text class='text' font-family=%8 stroke='none' stroke-width='%6' fill='%7' font-size='%1' x='%2' y='%3' text-anchor='%4'>%5</text>\n")
						    .arg(SchematicRectConstants::PinBigTextHeight)
						    .arg(x2 - bounds.left() + xOffset)
						    .arg(bounds.bottom() - y2 + yOffset)
						    .arg(anchorAtStart ? "start" : "end")
						    .arg(TextUtils::escapeAnd(useName))
						    .arg(0)  // SW(width)
						    .arg(SchematicRectConstants::PinTextColor) 
                            .arg(SchematicRectConstants::FontFamily)
                            ;  

        if (rotate) {
		    svg += "</g></g>\n";
	    }
    }
}

void LbrApplication::makePad(QDomElement & element, const QRectF & bounds, const QString & color, const QString & name, bool package, QString & svg)
{
    qreal x, y;
	if (!xy(element, x, y)) {
		qDebug() << "pad missing xy";
		return;
	}

	qreal drill, diameter;
	bool dd = drillDiameter(element, drill, diameter);
	QString shape = element.attribute("shape");

	if (shape.isEmpty() || shape.compare("octagon") == 0) {
		if (!dd) {
			qDebug() << "pad missing diameter";
			return;
		}
	}

	qreal sw = (diameter - drill) / 2;

	QString connectorID = prepConnector(element, name, package);

	svg += QString("<circle id='%6' connectorname='%7' cx='%1' cy='%2' r='%3' stroke='%4' stroke-width='%5' fill='none' />\n")
				.arg(x - bounds.left())
				.arg(bounds.bottom() - y)
				.arg((diameter / 2) - (sw / 2))
				.arg(color)
				.arg(sw)
				.arg(connectorID)
				.arg(TextUtils::escapeAnd(element.attribute("name")))
				;

	if (shape.compare("long") == 0) {
		qreal angle = getRot(element);
		qreal dx = diameter;
		qreal dy = diameter / 2;
		if (angle == 90 || angle == 270) {
			swap(dx, dy);
		}
		qreal rx = qMin(dx, dy);
		qreal ry = rx;
		QString hole = genHole(x, y, drill / 2, 0, bounds);

		qreal vert = (dy - ry) * 2;
		qreal horiz = (dx - rx) * 2;

		svg += QString("<path stroke='none' stroke-width='0' d='m%1,%2a%5,%6 0 0 1 %5,%6l0,%4a%5,%6 0 0 1 -%5,%6l-%3,0a%5,%6 0 0 1 -%5,-%6l0,-%4a%5,%6 0 0 1 %5,-%6l%3,0z%7' fill='%8' />\n")
					.arg(x + dx - rx - bounds.left())
					.arg(bounds.bottom() - (y + dy))			// note "+ dy" rather than "- dy" because we flip all the y values
					.arg(horiz)
					.arg(vert)
					.arg(rx)
					.arg(ry)
					.arg(hole)
					.arg(color)
					;
	}
	else if (shape.compare("offset") == 0) {
		qreal angle = getRot(element);
		qreal dx = diameter;
		qreal dy = diameter / 2;
		if (angle == 90 || angle == 270) {
			swap(dx, dy);
		}
		qreal rx = qMin(dx, dy);
		qreal ry = rx;
		QString hole = genHole(x, y, drill / 2, 0, bounds);

		qreal vert = (dy - ry) * 2;
		qreal horiz = (dx - rx) * 2;

		qreal stx, sty;
		if (angle == 0) {
			stx = x + diameter;
			sty = y + (diameter / 2);
		}
		else if (angle == 180) {
			stx = x;
			sty = y + (diameter / 2);
		}
		else if (angle == 90) {
			stx = x;
			sty = y + (diameter / 2) + diameter;
		}
		else if (angle == 270) {
			stx = x;
			sty = y + (diameter / 2);
		}
		else {
			qDebug() << "bad angle in offset" << angle;
		}


		svg += QString("<path stroke='none' stroke-width='0' d='m%1,%2a%5,%6 0 0 1 %5,%6l0,%4a%5,%6 0 0 1 -%5,%6l-%3,0a%5,%6 0 0 1 -%5,-%6l0,-%4a%5,%6 0 0 1 %5,-%6l%3,0z%7' fill='%8' />\n")
					.arg(stx - bounds.left())
					.arg(bounds.bottom() - sty)			// note "+ dy" rather than "- dy" because we flip all the y values
					.arg(horiz)
					.arg(vert)
					.arg(rx)
					.arg(ry)
					.arg(hole)
					.arg(color)
					;
	}
	else if (shape.isEmpty()) {
	}
	else if (shape.compare("octagon") == 0) {
	}
	else if (shape.compare("square") == 0) {
        if (!SquareNames.contains(name)) {
            SquareNames.append(name);
        }

		QString hole = genHole(x, y, drill / 2, 0, bounds);
		svg += QString("<path stroke='none' stroke-width='0' d='m%1,%2 %3,0 0,%3 -%3,0 0,-%3z%4' fill='%5' />\n")
					.arg(x - (diameter / 2) - bounds.left())
					.arg(bounds.bottom() - y - (diameter / 2))			
					.arg(diameter)
					.arg(hole)
					.arg(color)
					;
	}
	else {
		qDebug() << "unknown pad shape" << shape;
	}
}


QString LbrApplication::genArcString(qreal x1, qreal y1, qreal x2, qreal y2, qreal curve, const QRectF & bounds) 
{
	double dsqd = ((x2 - x1) * (x2 - x1)) + ((y2 - y1) * (y2 - y1));
	double d = qSqrt(dsqd);
	double halfd = d / 2;
	double otherAngle = (90 - (qAbs(curve) / 2)) * M_PI/ 180;
	double height = tan(otherAngle) * halfd;
	double r = qSqrt((height * height) + (halfd * halfd));

	return QString("A%1,%1 0 %2 %3 %4,%5")
				.arg(r)
				.arg((qAbs(curve) < 180.0) ? 0 : 1)
				.arg(curve < 0 ? 1 : 0)
				.arg(x2 - bounds.left())
				.arg(bounds.bottom() - y2)
				;
}

QRectF LbrApplication::getDimensions(const QDomElement & root, const QStringList & layers) 
{
	qreal left = std::numeric_limits<int>::max();
	qreal right = std::numeric_limits<int>::min();
	qreal top = std::numeric_limits<int>::max();
	qreal bottom = std::numeric_limits<int>::min();

	QDomElement element = root.firstChildElement();
	while (!element.isNull()) {
		QString elementLayer = element.attribute("layer");
		if (layers.contains(elementLayer)) {
			QRectF r = getBounds(element);
			if (!r.isNull()) {
				if (r.left() < left) {
					left = r.left();
				}
				if (r.right() > right) {
					right = r.right();
				}
				if (r.top() < top) {
					top = r.top();
				}
				if (r.bottom() > bottom) {
					bottom = r.bottom();
				}
			}		
		}

		element = element.nextSiblingElement();
	}

	return QRectF(left, top, right - left, bottom - top);
}

QRectF LbrApplication::getBounds(const QDomElement & element) 
{
	QString tagName = element.tagName();

	QRectF bounds;
	if (tagName.compare("circle") == 0) {
		qreal cx, cy, radius, width;
		if (!cxcyrw(element, cx, cy, radius, width)) return bounds;

		bounds.setCoords(cx - radius - (width / 2), cy - radius - (width / 2), cx + radius + (width / 2), cy + radius + (width / 2));
		return bounds;
	}

	if (tagName.compare("hole") == 0) {
		qreal cx, cy;
		if (!xy(element, cx, cy)) return bounds;

		bool ok;
		qreal drill = element.attribute("drill", "").toDouble(&ok);
		if (!ok) return bounds;

		bounds.setCoords(cx - (drill / 2), cy - (drill / 2), cx + (drill / 2), cy + (drill / 2));
		return bounds;
	}

	if (tagName.compare("pin") == 0) {
		qreal x1, x2, y1, y2;
		if (!parsePin(element, x1, y1, x2, y2)) return bounds;

		setBounds(element, x1, y1, x2, y2, false, bounds);
		return bounds;
	}

	if (tagName.compare("smd") == 0) {
		qreal x, y;
		if (!xy(element, x, y)) return bounds;

		qreal dx, dy;
		if (!dxdy(element, dx, dy)) return bounds;

		qreal angle = getRot(element);
		if (angle == 90 || angle == 270) {
			swap(dx, dy);
		}

		bounds.setCoords(x - (dx / 2), y - (dy / 2), x + (dx / 2), y + (dy / 2));

		return bounds;
	}

	if (tagName.compare("pad") == 0) {
		qreal x, y;
		if (!xy(element, x, y)) return bounds;

		qreal drill, diameter;
		bool dd = drillDiameter(element, drill, diameter);

		QString shape = element.attribute("shape");	
		if (shape.compare("long") == 0) {
			qreal angle = getRot(element);
			qreal rx = diameter;
			qreal ry = diameter / 2;
			if (angle == 90 || angle == 270) {
				swap(rx, ry);
			}
			else if (angle == 0 || angle == 180) {
			}
			else {
				qDebug() << "bad pad angle" << angle;
			}

			bounds.setCoords(x - rx, y - ry, x + rx, y + ry);
			return bounds;
		}

		if (shape.compare("offset") == 0) {
			qreal angle = getRot(element);
			qreal dx = diameter;
			qreal dy = diameter / 2;
			if (angle == 90 || angle == 270) {
				swap(dx, dy);
			}

			if (angle == 0) {
				x += diameter / 2;
			}
			else if (angle == 180) {
				x -= diameter / 2;
			}
			else if (angle == 90) {
				y += (diameter / 2);
			}
			else if (angle == 270) {
				y -= (diameter / 2);
			}
			else {
				return bounds;
			}

			bounds.setCoords(x - dx, y - dy, x + dx, y + dy);
			return bounds;

		}

		if (!dd) return bounds;
			
		bounds.setCoords(x - diameter / 2, y - diameter / 2, x + diameter / 2, y + diameter / 2);

		return bounds;
	}

	if (tagName.compare("text") == 0) {
		qreal x, y;
		if (!xy(element, x, y)) {
			return bounds;
		}

		QString text;
		TextUtils::findText(element, text);
		if (text.isEmpty()) return bounds;

		qreal angle = getRot(element);

		bool ok;
		qreal size = element.attribute("size", "").toDouble(&ok);
		if (!ok) {
			return bounds;
		}

		size *= TextSizeMultiplier;			// this is a hack, but it seems to help
	
		bool anchorAtStart;
		MiscUtils::calcTextAngle(angle, 0, 0, size, x, y, anchorAtStart);

		QFont font("OCRA");
		font.setPointSizeF(size * 72 / 25.4);
		QFontMetricsF fontMetrics(font);
		QRectF boundingRect = fontMetrics.boundingRect(text);
		QTransform transform;
		transform.rotate(angle);
		QRectF rotatedRect = transform.mapRect(boundingRect);
		rotatedRect.moveTo(x, y);
		bounds = rotatedRect;
		return bounds;
	}

	if (tagName.compare("wire") == 0) {
		bool ok;
		double curve = element.attribute("curve", "").toDouble(&ok);
		if (!ok || (curve == 0) || (curve == 360)) {
			getPairBounds(element, !element.attribute("polygon").isEmpty(), bounds);
			return bounds;
		}

		qreal x1, y1, x2, y2;
		if (!MiscUtils::x1y1x2y2(element, x1, y1, x2, y2)) return bounds;

		double mx = (x1 + x2) / 2;
		double my = (y1 + y2) / 2;

		double dsqd = ((x2 - x1) * (x2 - x1)) + ((y2 - y1) * (y2 - y1));
		double d = qSqrt(dsqd);
		double halfd = d / 2;
		double otherAngle = (90 - (qAbs(curve) / 2)) * M_PI/ 180;
		double height = tan(otherAngle) * halfd;
		double r = qSqrt((height * height) + (halfd * halfd));

		//qDebug() << "curve" << curve;
		//qDebug() << "points" << x1 << y1 << x2 << y2;
		//qDebug() << "m:" << mx << my << "d:" << dsqd << d << halfd << "oa" << (90 - (qAbs(curve) / 2)) << "h:" << height << "r:" << r;

		double cx1 = mx + (height * (y1-y2) / d);
		double cy1 = my + (height * (x2-x1) / d);  

		double cx2 = mx - (height * (y1-y2) / d);
		double cy2 = my - (height * (x2-x1) / d);  

		//double rtest1 = qSqrt(((cx1 - x1) * (cx1 - x1)) + ((cy1 - y1) * (cy1 - y1)));
		//double rtest2 = qSqrt(((cx2 - x2) * (cx2 - x2)) + ((cy2 - y2) * (cy2 - y2)));

		double anglest1 = atan2(y1 - cy1, x1 - cx1) * 180 / M_PI;
		double anglend1 = atan2(y2 - cy1, x2 - cx1) * 180 / M_PI;

		double anglest2 = atan2(y1 - cy2, x1 - cx2) * 180 / M_PI;
		double anglend2 = atan2(y2 - cy2, x2 - cx2) * 180 / M_PI;

		//qDebug() << "rtest" << rtest1 << rtest2;
		//qDebug() << "angles" << anglest1 << anglend1 << anglest2 << anglend2;
		//qDebug() << "centers" << cx1 << cy1 << cx2 << cy2;

		double anglest, anglend, cx, cy;
		bool got1 = false;
		bool got2 = false;
		if (qAbs(anglend1 - anglest1 - curve) < .001) {
			got1 = true;
		}
		else if (qAbs(anglend2 - anglest2 - curve) < .001) {
			got2 = true;
		}
		else if (qAbs(anglend1 - (anglest1 + 360) - curve) < .001) {
			got1 = true;
		}
		else if (qAbs(anglend2 - (anglest2 + 360) - curve) < .001) {
			got2 = true;
		}
		else if (qAbs(anglend1 + 360 - anglest1 - curve) < .001) {
			got1 = true;
		}
		else if (qAbs(anglend2 + 360 - anglest2 - curve) < .001) {
			got2 = true;
		}

		if (got1) {
			cx = cx1;
			cy = cy1;
			anglest = anglest1;
			anglend = anglend1;
		}
		else if (got2) {
			cx = cx2;
			cy = cy2;
			anglest = anglest2;
			anglend = anglend2;
		}
		else {
			qDebug() << "arc angle not handled";
			return bounds;
		}

		bool sweepFlag = (curve < 0);

		if (sweepFlag) {
			swap(anglest, anglend);
			curve = qAbs(curve);
		}

		while (anglend < anglest) {
			anglend += 360;
		}

		//qDebug() << "c & a" << cx << cy << anglest << anglend;

		if (anglest <= 0 && anglend >= 0) {
			x2 = cx + r;
		}
		if (anglest <= 90 && anglend >= 90) {
			y2 = cy + r;
		}
		if (anglest <= 180 && anglend >= 180) {
			x1 = cx - r;
		}
		if (anglest <= 270 && anglend >= 270) {
			y1 = cy - r;
		}
		if (anglest <= 360 && anglend >= 360) {
			x2 = cx + r;
		}
		if (anglest <= 450 && anglend >= 450) {
			y2 = cy + r;
		}
		if (anglest <= 540 && anglend >= 540) {
			x1 = cx - r;
		}
		if (anglest <= 630 && anglend >= 630) {
			y1 = cy - r;
		}

		//qDebug() << "points2" << x1 << y1 << x2 << y2;

		setBounds(element, x1, y1, x2, y2, false, bounds);

		return bounds;
	}

	if (tagName.compare("rectangle") == 0) {
		qreal x1, y1, x2, y2;
		if (!MiscUtils::x1y1x2y2(element, x1, y1, x2, y2)) return bounds;

		if (x2 < x1) {
			swap(x2, x1);
		}

		if (y2 < y1) {
			swap(y2, y1);
		}

		bounds.setCoords(x1, y1, x2, y2);
		return bounds;

	}

	if (tagName.compare("polygon") == 0 ) {
		qreal left = std::numeric_limits<int>::max();
		qreal right = std::numeric_limits<int>::min();
		qreal top = std::numeric_limits<int>::max();
		qreal bottom = std::numeric_limits<int>::min();

		QDomElement vertex = element.firstChildElement("vertex");
		while (!vertex.isNull()) {
			QDomElement next = vertex.nextSiblingElement("vertex");
			if (next.isNull()) next = element.firstChildElement("vertex");

			QDomElement wire = vertex.cloneNode(false).toElement();
			wire.setTagName("wire");
			wire.setAttribute("polygon", 1);
			wire.setAttribute("x1", vertex.attribute("x"));
			wire.setAttribute("y1", vertex.attribute("y"));
			wire.setAttribute("curve", vertex.attribute("curve"));
			wire.setAttribute("x2", next.attribute("x"));
			wire.setAttribute("y2", next.attribute("y"));
			wire.setAttribute("width", element.attribute("width"));
			QRectF r = getBounds(wire);
			left = qMin(r.right(), qMin(left, r.left()));
			right = qMax(r.left(), qMax(right, r.right()));
			top = qMin(r.bottom(), qMin(top, r.top()));
			bottom = qMax(r.top(), qMax(bottom, r.bottom()));

			vertex = vertex.nextSiblingElement("vertex");
		}

		return QRectF(left, top, right - left, bottom - top);
	}

	qDebug() << "getBounds missed tag" << tagName;
	return bounds;
}

bool LbrApplication::initArguments() {
	m_workingPath = "";
    QStringList args = QCoreApplication::arguments();
    for (int i = 0; i < args.length(); i++) {
        if ((args[i].compare("-h", Qt::CaseInsensitive) == 0) ||
            (args[i].compare("-help", Qt::CaseInsensitive) == 0) ||
            (args[i].compare("--help", Qt::CaseInsensitive) == 0))
        {
            return false;
        }

		if (i + 1 < args.length()) {
			if ((args[i].compare("-w", Qt::CaseInsensitive) == 0) ||
				(args[i].compare("-working", Qt::CaseInsensitive) == 0)||
				(args[i].compare("--working", Qt::CaseInsensitive) == 0))
			{
				m_workingPath = args[++i];
			}
			else if ((args[i].compare("-p", Qt::CaseInsensitive) == 0) ||
				(args[i].compare("-parts", Qt::CaseInsensitive) == 0)||
				(args[i].compare("--parts", Qt::CaseInsensitive) == 0))
			{
				m_fritzingPartsPath = args[++i];
			}
			else if ((args[i].compare("-c", Qt::CaseInsensitive) == 0) ||
				(args[i].compare("-core", Qt::CaseInsensitive) == 0)||
				(args[i].compare("--core", Qt::CaseInsensitive) == 0))
			{
				m_core = args[++i];
			}
		}
    }

    if (m_workingPath.isEmpty()) {
        message("-b <path to lbr (parent) folder> parameter missing");
        return false;
    }

    QDir directory(m_workingPath);
    if (!directory.exists()) {
        message(QString("working folder '%1' not found").arg(m_workingPath));
        return false;
    }

    return true;
}


void LbrApplication::usage() {
    message("usage: lbr2svg -w <path to folder containing lbr files> -p <path to Fritzing parts folder> -c <core | user | contrib>");
}

void LbrApplication::message(const QString & msg) {
   // QTextStream cout(stdout);
  //  cout << msg;
   // cout.flush();

	qDebug() << msg;
 }

void LbrApplication::genText(const QDomElement & element, const QRectF & bounds, const QString & color, QString & svg) 
{
	qreal x, y;
	if (!xy(element, x, y)) {
		qDebug() << "text missing xy";
		return;
	}

	QString text;
	TextUtils::findText(element, text);
	if (text.isEmpty()) return;


	qreal angle = getRot(element);

	bool ok;
	qreal size = element.attribute("size", "").toDouble(&ok);
	if (!ok) {
		qDebug() << "text missing size";
		return;
	}

	size *= TextSizeMultiplier;			// this is a hack, but it seems to help
	
	bool anchorAtStart;
	MiscUtils::calcTextAngle(angle, 0, 0, size, x, y, anchorAtStart);

	if (angle != 0) {
		svg += QString("<g transform='translate(%1,%2)'><g transform='rotate(%3)'>\n")
			.arg(x - bounds.left())
			.arg(bounds.bottom() - y)
			.arg(angle);
		x = bounds.left();
		y = bounds.bottom();
	}
	svg += QString("<text class='text' font-family='OCRA' stroke='none' stroke-width='%6' fill='%7' font-size='%1' x='%2' y='%3' text-anchor='%4'>%5</text>\n")
						.arg(size)
						.arg(x - bounds.left())
						.arg(bounds.bottom() - y)
						.arg(anchorAtStart ? "start" : "end")
						.arg(TextUtils::escapeAnd(text))
						.arg(0)  // SW(width)
						.arg(color)
					;
	if (angle != 0) {
		svg += "</g></g>\n";
	}
}

QString LbrApplication::genHole(qreal cx, qreal cy, qreal r, int sweepFlag, const QRectF & bounds)
{
	return QString("M%1,%2a%3,%3 0 1 %5 %4,0 %3,%3 0 1 %5 -%4,0z\n")
				.arg(cx - bounds.left() - r)
				.arg(bounds.bottom() - cy)
				.arg(r)
				.arg(2 * r)
				.arg(sweepFlag);
}

QString LbrApplication::prepConnector(QDomElement & element, const QString & name, bool package) 
{
	QString prop = (name + PropSeparator + element.attribute("name"));
    QString connectorID;

	if (package) {
        connectorID = PackageConnectors.value(prop, "");
        if (connectorID.isEmpty()) {
            // means no fzp used this connector, but fill it in anyway
            int index = PackageConnectorIndexes.value(name);
            connectorID = QString("connector%1pad").arg(index++);
            PackageConnectors.insert(prop, connectorID);
            PackageConnectorTypes.insert(prop, element.tagName());
            PackageConnectorIndexes.insert(name, index);
        }
	}
	else {
		connectorID = SymbolConnectors.value(prop, "");
        if (connectorID.isEmpty()) {
            // means no fzp used this connector; probably this symbol is used in combination with another
            int index = SymbolConnectorIndexes.value(name, 0);
            connectorID = QString("connector%1pin").arg(index++);
            SymbolConnectors.insert(prop, connectorID);
            SymbolConnectorIndexes.insert(name, index);
        }
	}

	return connectorID;
}

QString LbrApplication::findExistingBreadboardFile(const QString & packageName) {
	if (m_fritzingPartsPath.isEmpty()) return "";

	if (m_bbFiles.isEmpty()) {
		QDir dir(m_fritzingPartsPath);
		dir.cd("svg");
		dir.cd("core");
		dir.cd("breadboard");
		QStringList nameFilters;
		nameFilters << "*_breadboard.svg";
		m_bbFiles = dir.entryList(nameFilters, QDir::Files | QDir::NoDotAndDotDot);
		
		for (int i = 0; i < m_bbFiles.count(); i++) {
			m_bbFiles[i] = m_bbFiles.at(i).toLower();
		}
	}

	QString lp = packageName.toLower();
	foreach (QString fn, m_bbFiles) {
		if (fn.contains(lp)) {
			qDebug() << "matched existing package" << fn;
			return fn;
		}
	}

	// TODO: remove non-alphanumeric and match
	return "";
}

bool LbrApplication::loadPartsDescrs(const QDir & workingFolder, const QString & filename) {
	QFile difFile(workingFolder.absoluteFilePath(filename));
	if (!difFile.exists()) {
		qDebug() << QString("no '%1' found").arg(filename);
		return false;
	}

	if (!difFile.open(QIODevice::ReadOnly)) {
		qDebug() << QString("unable to open '%1'").arg(filename);
		return false;
	}

	QTextStream in(&difFile);
	while (true) {
		QString line = in.readLine();
		if (line.isEmpty()) {
			qDebug() << QString("unexpected format (1) in '%1'").arg(filename);
			return false;
		}

		if (line.startsWith("DATA", Qt::CaseInsensitive)) break;
	}
	in.readLine();
	in.readLine();
	in.readLine();
	QString line = in.readLine();
	if (!line.startsWith("BOT", Qt::CaseInsensitive)) {
		qDebug() << QString("unexpected format (2) in '%1'").arg(filename);
		return false;
	}

	QStringList fields;
	while (true) {
		QString pair = in.readLine();
		QString value = in.readLine();
		if (pair.startsWith("1")) {
            while (!value.endsWith('"')) {
                value += in.readLine();
            }

			value.replace("\"", "");
			fields.append(value.toLower());
		}
		else if (pair.startsWith("0")) {
			QStringList s = pair.split(",");
			if (s.count() != 2) {
				qDebug() << QString("unexpected format (3) in '%1'").arg(filename);
				return false;
			}
			fields.append(s.at(1));
		}
		else {
			if (value.startsWith("BOT", Qt::CaseInsensitive)) {
				break;
			}

			qDebug() << QString("unexpected format (4) in '%1'").arg(filename);
			return false;
		}
	}

    QHash<QString, int> indexes;
    QList<QString> names;
    names << "new fzp" << "fzp disp" << "old fzp" 
        << "new bread" << "bread disp" << "old bread" << "use subpart"
        << "new schem" << "schem disp" << "old schem" 
        << "new pcb" << "pcb disp" << "old pcb" 
        << "nr" << "description" << "family" << "props" << "tags" << "title";

    foreach(QString name, names) {
        int index = fields.indexOf(name);
	    if (index < 0) {
		    qDebug() << QString("'%1' column not found in '%2'").arg(name).arg(filename);
		    return false;
	    }
        indexes.insert(name, index);
    }

	bool done = false;
	while (!done) {
		QStringList values;
		while (true) {
			QString pair = in.readLine();
			QString value = in.readLine();
            if (value.startsWith('"') && !value.endsWith('"')) {
                while (!value.endsWith('"')) {
                    value += "\n";
                    value += in.readLine();
                }
            }
            //qDebug() << pair << value;

			if (pair.startsWith("1")) {
				value.replace("\"", "");
				values.append(value);
			}
			else if (pair.startsWith("0")) {
				QStringList s = pair.split(",");
				if (s.count() != 2) {
					qDebug() << QString("unexpected format (5) in '%1'").arg(filename);
					return false;
				}
				values.append(s.at(1));
			}
			else {
				if (value.startsWith("BOT", Qt::CaseInsensitive)) {
					break;
				}
				if (value.startsWith("EOD", Qt::CaseInsensitive)) {
					done = true;
					break;
				}

				qDebug() << QString("unexpected format (6) in '%1'").arg(filename);
				return false;
			}
		}

        PartDescr * partDescr = new PartDescr(indexes, values);
        //qDebug() << partDescr->fzp.newName;
        m_partDescrs.insert(partDescr->fzp.newName, partDescr);
    }

    return true;

}


void LbrApplication::prepPackage(const QDomElement & package)
{
	QString packageName = package.attribute("name");
	//qDebug() << "processing package" << packageName;
	packageName = cleanChars(packageName);
	
    QList<QDomElement> pads;
    QList<QDomElement> smds;

	QDomElement pad = package.firstChildElement("pad");
    while (!pad.isNull()) {
        pads.append(pad);
		pad = pad.nextSiblingElement("pad");
	}
	QDomElement smd = package.firstChildElement("smd");
	while (!smd.isNull()) {
		smds.append(smd);
		smd = smd.nextSiblingElement("smd");
	}

	if (smds.count() > 0 && pads.count() > 0) {
		//qDebug() << "package" << package.attribute("name") << "has both smd and tht";
	}
    else if (pads.count() > 0) {
    }
    else if (smds.count() > 0) {
        AllSMDs.append(packageName);
    }
    else {
		qDebug() << "package" << package.attribute("name") << "missing both smd and tht";
    }

    if (pads.count() > 0) {
        qSort(pads.begin(), pads.end(), byName);
        foreach (QDomElement element, pads) {
            prepConnector(element, package.attribute("name"), true);
        }
    }
    if (smds.count() > 0) {
        qSort(smds.begin(), smds.end(), byName);
        foreach (QDomElement element, smds) {
            prepConnector(element, package.attribute("name"), true);
        }
    }

    /*

	QString breadboardFile = findExistingBreadboardFile(packageName);
	if (!breadboardFile.isEmpty()) {
		OldBreadboardFiles.insert(packageName, breadboardFile);
		return;
	}

	if (!gotSMD && packageName.toLower().indexOf(dipper) == 0) {
		int tenths = (int) (dimensions.height() / 2.54);
        if (packageName.toLower().contains("dil")) {
            if (tenths > 1) tenths--;
        }
		QString name = QString("generic_ic_dip_%1_%2mil_bread.svg").arg(all.count()).arg(tenths * 100);
		OldBreadboardFiles.insert(packageName, name);
		return;
	}

    */

}

void LbrApplication::makeBin(const QStringList & moduleIDs, const QString & libraryName, const QDir & binsFolder) {
	QString bin;
	bin += "<?xml version='1.0' encoding='UTF-8'?>\n";
	bin += QString("<module fritzingVersion='0.7.2b' icon='%1'>\n").arg(libraryName.toLower() + ".png");
	bin += QString("<title>%1</title>\n").arg(libraryName);
	bin += "<instances>\n";
	int ix = 0;
	foreach (QString moduleID, moduleIDs) {
        if (moduleID.isEmpty()) continue;

		bin += QString("<instance moduleIdRef='%1' modelIndex='%2'>\n").arg(moduleID).arg(ix++);
		bin += "<views>\n<iconView layer='icon'>\n<geometry z='-1' x='-1' y='-1'></geometry>\n</iconView>\n</views>\n";
		bin += "</instance>\n";
	}

	bin += "</instances>\n";
	bin += "</module>\n";

	QFile file(binsFolder.absoluteFilePath(libraryName.toLower() + ".fzb"));
	if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		QTextStream out(&file);
		out.setCodec("UTF-8");
		out << bin;
		file.close();
	}
}

bool LbrApplication::registerFonts() {

    int ix = QFontDatabase::addApplicationFont(":/resources/fonts/DroidSans.ttf");
    if (ix < 0) return false;

    ix = QFontDatabase::addApplicationFont(":/resources/fonts/DroidSans-Bold.ttf");
    if (ix < 0) return false;

    ix = QFontDatabase::addApplicationFont(":/resources/fonts/DroidSansMono.ttf");
    if (ix < 0) return false;

    ix = QFontDatabase::addApplicationFont(":/resources/fonts/OCRA.ttf");
    return ix >= 0;
}
