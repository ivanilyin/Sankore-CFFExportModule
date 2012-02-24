#include "UBCFFAdaptor.h"

#include <QtCore>
#include <QtXml>

// Constant names. Use only them instead const char* in each function

// Constant fileNames;
const QString fMetadata = "metadata.rdf";
const QString fIWBContent = "content.xml";

// Constant messages;
const QString noErrorMsg = "NoError";

// Tag names
const QString tDescription = "Description";
const QString tIWBRoot = "iwb";
const QString tIWBMeta = "meta";
const QString tUBZSize = "size";
const QString tSvg = "svg";
const QString tIWBPage = "page";
const QString tIWBPageSet = "pageset";
const QString tId = "id";
const QString tElement = "element";
const QString tUBZGroup = "group";
const QString tUBZG = "g";
const QString tUBZPolygon = "polygon";
const QString tUBZPolyline = "polyline";
const QString tUBZLine = "line";
const QString tUBZAudio = "audio";
const QString tUBZVideo = "video";
const QString tUBZImage = "image";
const QString tUBZForeignObject = "foreignObject";

const QString tIWBImage = "image";
const QString tIWBVideo = "video";
const QString tIWBAudio = "audio";
const QString tIWBText = "text";
const QString tIWBPolyline = "polyline";
const QString tIWBPolygon = "polygon";
const QString tIWBFlash = "video";

// Attributes names
const QString aAbout  = "about";
const QString aIWBViewBox = "viewbox";
const QString aUBZViewBox = "viewBox";
const QString aDarkBackground = "dark-background";
const QString aCrossedBackground = "crossed-background";
const QString aUBZType = "type";

// Attribute values
const QString avUBZText = "text";

// Namespaces and prefixes
const QString dcNS = "http://purl.org/dc/elements/1.1/";
const QString ubNS = "http://uniboard.mnemis.com/document";
const QString svgUBZNS = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
const QString svgIWBNS = "http://www.w3.org/2000/svg";
const QString xlinkNS = "http://www.w3.org/1999/xlink";
const QString iwbNS = "http://www.becta.org.uk/iwb";
const QString dcNSPrefix = "dc";
const QString ubNSPrefix = "ub";
const QString svgIWBNSPrefix = "svg";
const QString xlinkNSPrefix = "xlink";
const QString iwbNsPrefix = "iwb";

//constant symbols and words etc
const QString dimensionsDelimiter1 = "x";
const QString dimensionsDelimiter2 = " ";
const QString pageAlias = "page";
const QString pageFileExtentionUBZ = "svg";


UBCFFAdaptor::UBCFFAdaptor() :
    converter(0)
{

}
bool UBCFFAdaptor::convertUBZToIWB(const QString &from, const QString &to)
{
    qDebug() << "starting converion from" << from << "to" << to;

    QString source = QString();
    if (QFileInfo(from).isDir() && QFile::exists(from)) {
        qDebug() << "File specified is dir, continuing convertion";
        source = from;
    } else {
        source = uncompressZip();
        if (!source.isNull()) qDebug() << "File specified is zip file. Uncompressed to tmp dir, continuing convertion";
    }
    if (source.isNull()) {
        qDebug() << "File specified is not a dir or a zip file, stopping covretion";
        return false;
    }

    UBToCFFConverter tmpConvertrer(from, to);
    if (!tmpConvertrer) {
        qDebug() << "The convertrer class is invalid, stopping conversion";
        return false;
    }
    tmpConvertrer.parse();

    return true;
}

UBCFFAdaptor::UBToCFFConverter::UBToCFFConverter(const QString &source, const QString &destination)
    : mDataModel(0)
{
    sourcePath = source;
    destinationPath = destination;

    errorStr = noErrorMsg;
    mDataModel = new QDomDocument;
    mIWBContentWriter = new QXmlStreamWriter;
    mIWBContentWriter->setAutoFormatting(true);
}

bool UBCFFAdaptor::UBToCFFConverter::parse()
{
    if(!isValid()) {
        qDebug() << "document metadata is not valid. Can't parse";
        return false;
    }

    qDebug() << "begin parsing ubz";
    QFile outFile(destinationPath + "/" + fIWBContent);
    if (!outFile.open(QIODevice::WriteOnly| QIODevice::Text)) {
        qDebug() << "can't open output file for writing";
    }
    mIWBContentWriter->setDevice(&outFile);

    mIWBContentWriter->writeStartDocument();
    mIWBContentWriter->writeStartElement(tIWBRoot);

    fillNamespaces();

    if (!parseMetadata()) {
        if (errorStr == noErrorMsg)
            errorStr = "MetadataParsingError";
        return false;
    }
    else if (!parseContent()) {
        if (errorStr == noErrorMsg)
            errorStr = "ContentParsingError";
        return false;
    }
    mIWBContentWriter->writeEndElement();
    mIWBContentWriter->writeEndDocument();

    outFile.close();

    qDebug() << "finished with success";

    return true;
}
bool UBCFFAdaptor::UBToCFFConverter::parseMetadata()
{
    int errorLine, errorColumn;
    QFile metaDataFile(sourcePath + "/" + fMetadata);

    if (!metaDataFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorStr = "can't open" + QFileInfo(sourcePath + "/" + fMetadata).absoluteFilePath();
        qDebug() << errorStr;
        return false;

    } else if (!mDataModel->setContent(metaDataFile.readAll(), true, &errorStr, &errorLine, &errorColumn)) {
        qWarning() << "Error:Parseerroratline" << errorLine << ","
                   << "column" << errorColumn << ":" << errorStr;
        return false;
    }

    QDomElement nextElement = mDataModel->documentElement();

    nextElement = nextElement.firstChildElement(tDescription);
    if (!nextElement.isNull()) {

        mIWBContentWriter->writeStartElement(iwbNS, tIWBMeta);
        mIWBContentWriter->writeAttribute(aAbout, nextElement.attribute(aAbout));
        mIWBContentWriter->writeEndElement();

        nextElement = nextElement.firstChildElement();
        while (!nextElement.isNull()) {

            QString textContent = nextElement.text();
            if (!textContent.trimmed().isEmpty()) {
                if (nextElement.tagName() == tUBZSize) { //getting main viewbox rect since for CFF specificaton we have static viewbox
                    QRect tmpRect = getViewboxRect(nextElement.text());
                    if (!tmpRect.isNull()) {
                        mViewbox = tmpRect;
                    } else {
                        qDebug() << "can't interpret viewbox rectangle";
                        errorStr = "InterpretViewboxRectangleError";
                        return false;
                    }
                } else {
                    mIWBContentWriter->writeStartElement(iwbNS, tIWBMeta);
                    mIWBContentWriter->writeAttribute(nextElement.namespaceURI(), nextElement.tagName(), textContent);
                    mIWBContentWriter->writeEndElement();
                }

            }
            nextElement = nextElement.nextSiblingElement();
        }
    }

    metaDataFile.close();
    return true;
}
bool UBCFFAdaptor::UBToCFFConverter::parseContent() {

    mIWBContentWriter->writeStartElement(svgIWBNS, tSvg);

    if (!mViewbox.isNull()) { //If viewbox has been set
        mIWBContentWriter->writeAttribute(aIWBViewBox, rectToIWBAttr(mViewbox));
    }

    QDir sourceDir(sourcePath);
    QStringList fileFilters;
    fileFilters << QString(pageAlias + "???." + pageFileExtentionUBZ);
    QStringList pageList = sourceDir.entryList(fileFilters, QDir::Files, QDir::Name | QDir::IgnoreCase);

    if (!pageList.count()) {
        qDebug() << "can't find any content file";
        errorStr = "ErrorContentFile";
        return false;
    } else if (pageList.count() == 1) {
        if (!parsePage(pageList.first()))
            return false;

    } else if (!parsePageset(pageList)) {
        return false;
    }

    mIWBContentWriter->writeEndElement();
    return true;
}

bool UBCFFAdaptor::UBToCFFConverter::parsePage(const QString &pageFileName)
{
    qDebug() << "begin parsing page" + pageFileName;

    int errorLine, errorColumn;

    QFile pageFile(sourcePath + "/" + pageFileName);
    if (!pageFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "can't open file" << pageFileName << "for reading";
        return false;
    } else if (!mDataModel->setContent(pageFile.readAll(), true, &errorStr, &errorLine, &errorColumn)) {
        qWarning() << "Error:Parseerroratline" << errorLine << ","
                   << "column" << errorColumn << ":" << errorStr;
        pageFile.close();
        return false;
    }

    QDomElement nextTopElement = mDataModel->firstChildElement();
    if (!nextTopElement.isNull()) {
        QString tagname = nextTopElement.tagName();
        if (tagname == tSvg) {
            if (!parseSvgPageSection(nextTopElement)) {
                pageFile.close();
                return false;
            }
        } else if (tagname == tUBZGroup) {
            if (!parseGroupPageSection(nextTopElement)) {
                pageFile.close();
                return false;
            }
        }

        nextTopElement = nextTopElement.nextSiblingElement();
    }

    pageFile.close();
    return true;
}

bool UBCFFAdaptor::UBToCFFConverter::parsePageset(const QStringList &pageFileNames)
{
    mIWBContentWriter->writeStartElement(svgIWBNS, tIWBPageSet);

    QStringListIterator curPage(pageFileNames);

    while (curPage.hasNext()) {

        QString curPageFile = curPage.next();
        mIWBContentWriter->writeStartElement(svgIWBNS, tIWBPage);
        mIWBContentWriter->writeAttribute(tId, curPageFile);

        if (!parsePage(curPageFile))
            return false;
        mIWBContentWriter->writeEndElement();
    }

    mIWBContentWriter->writeEndElement();
    return true;
}
bool UBCFFAdaptor::UBToCFFConverter::parseSvgPageSection(const QDomElement &element)
{
    //Parsing top level tag attributes
    QRect currViewBoxRect;

    //getting current page viewbox to be able to convert coordinates to global viewbox parameter
    if (element.hasAttribute(aUBZViewBox)) {
        currViewBoxRect = getViewboxRect(element.attribute(aUBZViewBox));
    }

    if (element.hasAttribute(aDarkBackground)) {
        if (!createDarkBackground(element)) return false;
    }

    if (element.hasAttribute(aCrossedBackground)) {
        if (!createCrossedBackground(element)) return false;
    }

    //Parsing svg children attributes
    QDomElement nextElement = element.firstChildElement();
    while (!nextElement.isNull()) {
        QString tagName = nextElement.tagName();
        if      (tagName == tUBZG             && !parseSVGGGroup(nextElement))      return false;
        else if (tagName == tUBZImage         && !parseUBZImage(nextElement))       return false;
        else if (tagName == tUBZVideo         && !parseUBZVideo(nextElement))       return false;
        else if (tagName == tUBZForeignObject && !parseForeignObject(nextElement))  return false;

        nextElement = nextElement.nextSiblingElement();
    }

    return true;
}
// extended element options
// editable, background, locked are supported for now

bool UBCFFAdaptor::UBToCFFConverter::parseGroupPageSection(const QDomElement &element)
{
//    First sankore side implementation needed. TODO in Sankore 1.5
    Q_UNUSED(element)
    qDebug() << "parsing ubz group section";
    return true;
}

bool UBCFFAdaptor::UBToCFFConverter::createDarkBackground(const QDomElement &element)
{
    Q_UNUSED(element)
    qDebug() << "|creating element background";
    if (false) {
        qDebug() << "|error at creating element background";
        errorStr = "CreatingElementBackgroundParsingError";
        return false;
    }
    return true;
}

bool UBCFFAdaptor::UBToCFFConverter::createCrossedBackground(const QDomElement &element)
{
    Q_UNUSED(element)
    qDebug() << "|creating background grid";
    if (false) {
        qDebug() << "|error at creating background grid";
        errorStr = "CreatingBackgroundGridImageParsingError";
        return false;
    }
    return true;
}

bool UBCFFAdaptor::UBToCFFConverter::parseSVGGGroup(const QDomElement &element)
{
    qDebug() << "|parsing g section";
    QDomElement nextElement = element.firstChildElement();
    if (nextElement.isNull()) {
        qDebug() << "Empty g element";
        errorStr = "EmptyGSection";
        return false;
    }

    mIWBContentWriter->writeStartElement(svgIWBNS, tIWBPolyline);
    while (!nextElement.isNull()) {
        QString tagName = nextElement.tagName();
        if      (tagName == tUBZLine     && !parseUBZLine(nextElement))     return false;
        else if (tagName == tUBZPolygon  && !parseUBZPolygon(nextElement))  return false;
        else if (tagName == tUBZPolyline && !parseUBZPolyline(nextElement)) return false;

        nextElement = nextElement.nextSiblingElement();
    }
    mIWBContentWriter->writeEndElement();

    return true;
}
bool UBCFFAdaptor::UBToCFFConverter::parseUBZImage(const QDomElement &element)
{
    Q_UNUSED(element)
    mIWBContentWriter->writeStartElement(svgIWBNS, tIWBImage);

    qDebug() << "|parsing image";
    if (false) {
        qDebug() << "|error at image parsing";
        errorStr = "ImageParsingError";
        return false;
    }
    mIWBContentWriter->writeEndElement();

    return true;
}
bool UBCFFAdaptor::UBToCFFConverter::parseUBZVideo(const QDomElement &element)
{
    Q_UNUSED(element)
    qDebug() << "|parsing video";

    mIWBContentWriter->writeStartElement(svgIWBNS, tIWBVideo);
    if (false) {
        qDebug() << "|error at video parsing";
        errorStr = "VideoParsingError";
        return false;
    }
    mIWBContentWriter->writeEndElement();

    return true;
}
bool UBCFFAdaptor::UBToCFFConverter::parseUBZAudio(const QDomElement &element)
{
    Q_UNUSED(element)
    qDebug() << "|parsing audio";

    mIWBContentWriter->writeStartElement(svgIWBNS, tIWBAudio);
    if (false) {
        qDebug() << "|error at audio parsing";
        errorStr = "AudioParsingError";
        return false;
    }
    mIWBContentWriter->writeEndElement();

    return true;
}
bool UBCFFAdaptor::UBToCFFConverter::parseForeignObject(const QDomElement &element)
{
    if (element.attribute(aUBZType) == avUBZText) {
       if (!parseUBZText(element)) {
           return false;
       } else {
           return true;
       }
    }

    mIWBContentWriter->writeStartElement(svgIWBNS, tIWBImage);

    qDebug() << "|parsing foreign object";
    if (false) {
        qDebug() << "|error at parsing foreign object";
        errorStr = "ForeignObjectParsingError";
        return false;
    }
    mIWBContentWriter->writeEndElement();

    return true;
}

bool UBCFFAdaptor::UBToCFFConverter::parseUBZText(const QDomElement &element)
{
    Q_UNUSED(element)
    qDebug() << "|parsing text";

    mIWBContentWriter->writeStartElement(svgIWBNS, tIWBText);
    if (false) {
        qDebug() << "|error at text parsing";
        errorStr = "TextParsingError";
        return false;
    }
    mIWBContentWriter->writeEndElement();

    return true;
}

bool UBCFFAdaptor::UBToCFFConverter::parseUBZPolygon(const QDomElement &element)
{
    Q_UNUSED(element)
    qDebug() << "||parsing polygon";
    if (false) {
        qDebug() << "||error at parsing polygon";
        errorStr = "PolygonParsingError";
        return false;
    }
    return true;
}
bool UBCFFAdaptor::UBToCFFConverter::parseUBZPolyline(const QDomElement &element){
    Q_UNUSED(element)
    qDebug() << "||parsing polyline";
    if (false) {
        qDebug() << "||error at parsing polygon";
        errorStr = "PolylineParsingError";
        return false;
    }
    return true;
}

bool UBCFFAdaptor::UBToCFFConverter::parseUBZLine(const QDomElement &element){
    Q_UNUSED(element)
    qDebug() << "||parsing line";
    if (false) {
        qDebug() << "||error at parsing polygon";
        errorStr = "LineParsingError";
        return false;
    }
    return true;
}


UBCFFAdaptor::UBToCFFConverter::~UBToCFFConverter()
{
    if (mDataModel)
        delete mDataModel;
    if (mIWBContentWriter)
        delete mIWBContentWriter;
}
bool UBCFFAdaptor::UBToCFFConverter::isValid() const
{
    return QFileInfo(sourcePath).exists()
            && QFileInfo(sourcePath).isDir()
            && errorStr == noErrorMsg;
}

void UBCFFAdaptor::UBToCFFConverter::fillNamespaces()
{
    mIWBContentWriter->writeDefaultNamespace(svgUBZNS);
    mIWBContentWriter->writeNamespace(ubNS, ubNSPrefix);
    mIWBContentWriter->writeNamespace(dcNS, dcNSPrefix);
    mIWBContentWriter->writeNamespace(iwbNS, iwbNsPrefix);
    mIWBContentWriter->writeNamespace(svgIWBNS, svgIWBNSPrefix);
    mIWBContentWriter->writeNamespace(xlinkNS, xlinkNSPrefix);
}

QString UBCFFAdaptor::UBToCFFConverter::digitFileFormat(int digit) const
{
    return QString("%1").arg(digit, 3, 10, QLatin1Char('0'));
}

//Setting viewbox rectangle
QRect UBCFFAdaptor::UBToCFFConverter::getViewboxRect(const QString &element) const
{
    bool twoDimentional = false;


    QStringList dimList;
    dimList = element.split(dimensionsDelimiter1, QString::KeepEmptyParts);
    if (dimList.count() == 2) // row like 0x0
        twoDimentional = true;
    else {
        dimList = element.split(dimensionsDelimiter2, QString::KeepEmptyParts);
        if (dimList.count() != 4) // row unlike 0 0 0 0
            return QRect();
    }

    bool ok = false;
    int x = 0, y = 0;

    if (!twoDimentional) {
        x = dimList.takeFirst().toInt(&ok);
        if (!ok || !x)
            return QRect();

        y = dimList.takeFirst().toInt(&ok);
        if (!ok || !y)
            return QRect();
    }

    int width = dimList.takeFirst().toInt(&ok);
    if (!ok || !width)
        return QRect();

    int height = dimList.takeFirst().toInt(&ok);
    if (!ok || !height)
        return QRect();

    return twoDimentional ? QRect(0, 0, width, height) : QRect(x, y, width, height);
}

QString UBCFFAdaptor::UBToCFFConverter::rectToIWBAttr(const QRect &rect) const
{
    if (rect.isNull()) return QString();

    return QString("%1 %2 %3 %4").arg(rect.topLeft().x())
                                 .arg(rect.topLeft().y())
                                 .arg(rect.width())
                                 .arg(rect.height());
}

UBCFFAdaptor::UBToUBZConverter::UBToUBZConverter()
{

}
