#ifndef APPLICATION_H
#define APPLICATION_H

#include <QCoreApplication>
#include <QDomDocument>
#include <QRectF>
#include <QDir>
#include <QHash>


struct FileDescr {
    QString newName;
    QString disp;
    QString oldName;

    void init(const QString & prefix, const QHash<QString, int> indexes, const QStringList & values);
};

struct PartDescr {
    FileDescr fzp;
    FileDescr bread;
    FileDescr schem;
    FileDescr pcb;
    int number;
    QString title;
    QString description;
    QString family;
    QString props;
    QString tags;
    bool matched;
    QString useSubpart;

    PartDescr(const QHash<QString, int> indexes, const QStringList & values);
};

class LbrApplication : public QCoreApplication
{
public:
	LbrApplication(int argc, char *argv[]);
	~LbrApplication();

    void start();

protected:
	bool initArguments();
	void usage();
	void message(const QString & msg);
	void makeSchematics(const QDir & svgFolder, const QString & libraryName, const QDomElement & root);
	void makePCBs(const QDir & workingFolder, const QDir & pcbFolder, const QDir & breadboardFolder,  const QDir & subpartsFolder, const QString & libraryName, const QDomElement & root);
	QStringList makeFZPs(const QDir & workingFolder, const QDir & fzpFolder, const QDir & breadFolder, const QDir & schematicFolder, const QDir & pcbFolder, const QDir & iconFolder, const QDir & binsFolder, const QDir & subpartsFolder, const QString & libraryName, QDomDocument & doc, QTextStream & lbrStream);
	void processSymbol(const QDir & schematicFolder, QDomElement & symbol, const QString & libraryName);
	void processPackage(const QDir & workingFolder, const QDir & subpartsFolder, const QDir & pcbFolder, const QDir & breadboardFolder, const QDomElement & package, const QString & libraryName);
	QHash<QString, QString> processDevice(const QDir & workingFolder, const QDir & fzpFolder,  const QDir & iconFolder, const QDir & subpartsFolder, const QDomElement & device, const QString & symbol, QString description, const QString & libraryName, const QString & deviceSetName, const QString & deviceSetPrefix, bool useGate, QTextStream & lbrStream);
	QRectF getDimensions(const QDomElement & root, const QStringList & layers);
	QRectF getBounds(const QDomElement & element);
	void toSvg(const QDomElement & root, const QStringList & layers, const QRectF & bounds, const QHash<QString, QString> & colors, int & nonconnectorIndex, bool package, QString & svg);
	void toSvg(QDomElement & element, const QRectF & bounds, const QHash<QString, QString> & colors, int & nonconnectorIndex, const QString & name, bool package, bool isRectangular, QString & svg);
	void genText(const QDomElement & element, const QRectF & bounds, const QString & color, QString & svg);
	QString genHole(qreal cx, qreal cy, qreal r, int sweepFlag, const QRectF & bounds);
	QString prepConnector(QDomElement & element, const QString & name, bool package);
	QString genArcString(qreal x1, qreal y1, qreal x2, qreal y2, qreal curve, const QRectF & bounds);
	QString findExistingBreadboardFile(const QString & packageName);
    void makePad(QDomElement & element, const QRectF & bounds, const QString & color, const QString & name, bool package, QString & svg);
    void makePin(QDomElement & element, const QRectF & bounds, const QString & color, const QString & textColor, const QString & name, bool package, bool isRectangular, QString & svg);
    void makeSmd(QDomElement & element, const QRectF & bounds, const QString & color, const QString & name, bool package, QString & svg);
    void makeWire(QDomElement & element, const QRectF & bounds, const QString & color, QString & svg);
    void makePolygon(QDomElement & element, const QRectF & bounds, const QString & color, QString & svg);
    bool loadPartsDescrs(const QDir & folder, const QString & filename);
    void prepPCBs(const QDomElement & root);
    void prepPackage(const QDomElement & package);
    bool checkRectangular(const QDomElement & root, const QRectF & bounds, const QHash<QString, QString> & colors, QString & svg);
    void makeBin(const QStringList & moduleIDs, const QString & libraryName, const QDir & binsFolder);

protected:
    QString m_workingPath;
    QString m_fritzingPartsPath;
	QString m_core;
	QStringList m_bbFiles;
    QHash<QString, PartDescr *> m_partDescrs;
};

#endif