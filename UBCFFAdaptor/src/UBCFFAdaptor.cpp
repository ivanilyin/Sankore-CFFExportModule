#include "UBCFFAdaptor.h"

#include <QtCore>
#include <QtXml>

#include "UBGlobals.h"

THIRD_PARTY_WARNINGS_DISABLE
#include "quazip.h"
#include "quazipfile.h"
#include "quazipfileinfo.h"
THIRD_PARTY_WARNINGS_ENABLE


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
const QString tIWBRect = "rect";
const QString tIWBLine = "line";

// Attributes names
const QString aAbout  = "about";
const QString aIWBViewBox = "viewbox";
const QString aUBZViewBox = "viewBox";
const QString aDarkBackground = "dark-background";
const QString aBackground = "background";
const QString aCrossedBackground = "crossed-background";
const QString aUBZType = "type";
const QString aFill = "fill"; // IWB attribute contans color to fill

const QString aID = "id";   // ID of any svg element can be placed in to iwb section
const QString aRef = "ref"; // as reference for applying additional attributes

const QString aX = "x";
const QString aY = "y";
const QString aWidth = "width";
const QString aHeight = "height";
const QString aStroke = "stroke";
const QString aStrokeWidth = "stroke-width";
const QString aPoints = "points";
const QString aZLayer = "z-value";
const QString aTransform = "transform";

// Attribute values
const QString avUBZText = "text";
const QString avFalse = "false";
const QString avTrue = "true";

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

const int iCrossSize = 32;
const int iCrossWidth = 5;


struct UBItemLayerType
{
    enum Enum
    {
        FixedBackground = -2000, Object = -1000, Graphic = 0, Tool = 1000, Control = 2000
    };
};


UBCFFAdaptor::UBCFFAdaptor()
{}

bool UBCFFAdaptor::convertUBZToIWB(const QString &from, const QString &to)
{
    qDebug() << "starting converion from" << from << "to" << to;

    QString source = QString();
    if (QFileInfo(from).isDir() && QFile::exists(from)) {
        qDebug() << "File specified is dir, continuing convertion";
        source = from;
    } else {
        source = uncompressZip(from);
        if (!source.isNull()) qDebug() << "File specified is zip file. Uncompressed to tmp dir, continuing convertion";
    }
    if (source.isNull()) {
        qDebug() << "File specified is not a dir or a zip file, stopping covretion";
        return false;
    }

    QString tmpDestination = createNewTmpDir();
    if (tmpDestination.isNull()) {
        qDebug() << "can't create temp destination folder. Stopping parsing...";
        return false;
    }

    UBToCFFConverter tmpConvertrer(source, tmpDestination);
    if (!tmpConvertrer) {
        qDebug() << "The convertrer class is invalid, stopping conversion. Error message" << tmpConvertrer.lastErrStr();
        return false;
    }
    if (!tmpConvertrer.parse()) {
        return false;
    }

    compressZip(tmpDestination, to);

    //Cleanning tmp souces in filesystem
    if (!freeDir(source))
        qDebug() << "can't delete tmp directory" << QDir(source).absolutePath() << "try to delete them manually";

    if (!freeDir(tmpDestination))
        qDebug() << "can't delete tmp directory" << QDir(tmpDestination).absolutePath() << "try to delete them manually";

    return true;
}

QString UBCFFAdaptor::uncompressZip(const QString &zipFile)
{
    QuaZip zip(zipFile);

    if(!zip.open(QuaZip::mdUnzip)) {
        qWarning() << "Import failed. Cause zip.open(): " << zip.getZipError();
        return QString();
    }

    zip.setFileNameCodec("UTF-8");
    QuaZipFileInfo info;
    QuaZipFile file(&zip);

    //create unique cff document root fodler
    QString documentRootFolder = createNewTmpDir();

    if (documentRootFolder.isNull()) {
        qDebug() << "can't create tmp directory for zip file" << zipFile;
        return QString();
    }

    QDir rootDir(documentRootFolder);
    QFile out;
    char c;
    bool allOk = true;
    for(bool more = zip.goToFirstFile(); more; more=zip.goToNextFile()) {
        if(!zip.getCurrentFileInfo(&info)) {
            qWarning() << "Import failed. Cause: getCurrentFileInfo(): " << zip.getZipError();
            allOk = false;
            break;
        }
        if(!file.open(QIODevice::ReadOnly)) {
            allOk = false;
            break;
        }
        if(file.getZipError()!= UNZ_OK) {
            qWarning() << "Import failed. Cause: file.getFileName(): " << zip.getZipError();
            allOk = false;
            break;
        }

        QString newFileName = documentRootFolder + "/" + file.getActualFileName();

        QFileInfo newFileInfo(newFileName);
        rootDir.mkpath(newFileInfo.absolutePath());

        out.setFileName(newFileName);
        out.open(QIODevice::WriteOnly);

        while(file.getChar(&c))
            out.putChar(c);

        out.close();

        if(file.getZipError()!=UNZ_OK) {
            qWarning() << "Import failed. Cause: " << zip.getZipError();
            allOk = false;
            break;
        }
        if(!file.atEnd()) {
            qWarning() << "Import failed. Cause: read all but not EOF";
            allOk = false;
            break;
        }
        file.close();

        if(file.getZipError()!=UNZ_OK) {
            qWarning() << "Import failed. Cause: file.close(): " <<  file.getZipError();
            allOk = false;
            break;
        }
    }

    if (!allOk) {
        out.close();
        file.close();
        zip.close();
        return QString();
    }

    if(zip.getZipError()!=UNZ_OK) {
        qWarning() << "Import failed. Cause: zip.close(): " << zip.getZipError();
        return QString();
    }

    return documentRootFolder;
}

bool UBCFFAdaptor::compressZip(const QString &source, const QString &destination)
{
    QDir toDir = QFileInfo(destination).dir();
    if (!toDir.exists())
        if (!QDir().mkpath(toDir.absolutePath())) {
            qDebug() << "can't create destination folder to uncompress file";
            return false;
        }

    QuaZip zip(destination);
    zip.setFileNameCodec("UTF-8");
    if(!zip.open(QuaZip::mdCreate)) {
        qDebug("Export failed. Cause: zip.open(): %d", zip.getZipError());
        return false;
    }

    QuaZipFile outZip(&zip);

    QFileInfo sourceInfo(source);
    if (sourceInfo.isDir()) {
        if (!compressDir(QFileInfo(source).absoluteFilePath(), "", &outZip))
            return false;
    } else if (sourceInfo.isFile()) {
        if (!compressFile(QFileInfo(source).absoluteFilePath(), "", &outZip))
            return false;
    }

    return true;
}

bool UBCFFAdaptor::compressDir(const QString &dirName, const QString &parentDir, QuaZipFile *outZip)
{
    QFileInfoList dirFiles = QDir(dirName).entryInfoList(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    QListIterator<QFileInfo> iter(dirFiles);
    while (iter.hasNext()) {
        QFileInfo curFile = iter.next();

        if (curFile.isDir()) {
            if (!compressDir(curFile.absoluteFilePath(), parentDir + curFile.dir().dirName() + "/", outZip)) {
                qDebug() << "error at compressing dir" << curFile.absoluteFilePath();
                return false;
            }
        } else if (curFile.isFile()) {
            if (!compressFile(curFile.absoluteFilePath(), parentDir, outZip)) {
               return false;
            }
        }
    }

    return true;
}

bool UBCFFAdaptor::compressFile(const QString &fileName, const QString &parentDir, QuaZipFile *outZip)
{
    QFile sourceFile(fileName);

    if(!sourceFile.open(QIODevice::ReadOnly)) {
        qDebug() << "Compression of file" << sourceFile.fileName() << " failed. Cause: inFile.open(): " << sourceFile.errorString();
        return false;
    }

    if(!outZip->open(QIODevice::WriteOnly, QuaZipNewInfo(parentDir + QFileInfo(fileName).fileName(), sourceFile.fileName()))) {
        qDebug() << "Compression of file" << sourceFile.fileName() << " failed. Cause: outFile.open(): " << outZip->getZipError();
        sourceFile.close();
        return false;
    }

    outZip->write(sourceFile.readAll());
    if(outZip->getZipError() != UNZ_OK) {
        qDebug() << "Compression of file" << sourceFile.fileName() << " failed. Cause: outFile.write(): " << outZip->getZipError();

        sourceFile.close();
        outZip->close();
        return false;
    }

    if(outZip->getZipError() != UNZ_OK)
    {
        qWarning() << "Compression of file" << sourceFile.fileName() << " failed. Cause: outFile.close(): " << outZip->getZipError();

        sourceFile.close();
        outZip->close();
        return false;
    }

    outZip->close();
    sourceFile.close();

    return true;
}

QString UBCFFAdaptor::createNewTmpDir()
{
    int tmpNumber = 0;
    QDir systemTmp = QDir::temp();

    while (true) {
        QString dirName = QString("CFF_adaptor_filedata_store%1.%2")
                .arg(QDateTime::currentDateTime().toString("dd_MM_yyyy_HH-mm"))
                .arg(tmpNumber++);
        if (!systemTmp.exists(dirName)) {
            if (systemTmp.mkdir(dirName)) {
                QString result = systemTmp.absolutePath() + "/" + dirName;
                tmpDirs.append(result);
                return result;
            } else {
                qDebug() << "Can't create temporary dir maybe due to permissions";
                return QString();
            }
        } else if (tmpNumber == 10) {
            qWarning() << "Import failed. Failed to create temporary file ";
            return QString();
        }
        tmpNumber++;
    }

    return QString();
}
bool UBCFFAdaptor::deleteDir(const QString& pDirPath) const
{
    if (pDirPath == "" || pDirPath == "." || pDirPath == "..")
        return false;

    QDir dir(pDirPath);

    if (dir.exists())
    {
        foreach(QFileInfo dirContent, dir.entryInfoList(QDir::Files | QDir::Dirs
                | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System , QDir::Name))
        {
            if (dirContent.isDir())
            {
                deleteDir(dirContent.absoluteFilePath());
            }
            else
            {
                if (!dirContent.dir().remove(dirContent.fileName()))
                {
                    return false;
                }
            }
        }
    }

    return dir.rmdir(pDirPath);
}
bool UBCFFAdaptor::freeDir(const QString &dir)
{
    bool result = true;
    if (!deleteDir(dir))
        result = false;

    tmpDirs.removeAll(QDir(dir).absolutePath());

    return result;
}
void UBCFFAdaptor::freeTmpDirs()
{
    foreach (QString dir, tmpDirs)
        freeDir(dir);
}

UBCFFAdaptor::~UBCFFAdaptor()
{
    freeTmpDirs();
}

UBCFFAdaptor::UBToCFFConverter::UBToCFFConverter(const QString &source, const QString &destination)
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
        if (!createBackground(element)) return false;
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

bool UBCFFAdaptor::UBToCFFConverter::ibwSetElementAsBackground(QDomElement &element)
{
    QDomElement iwbBackgroundElementPart;

    iwbBackgroundElementPart.setTagName(tElement);
    iwbBackgroundElementPart.setAttribute(aRef, element.attribute(aID));
    iwbBackgroundElementPart.setAttribute(aBackground, avTrue);
    iwbBackgroundElementPart.setAttribute(aZLayer, UBItemLayerType::FixedBackground);
  
    return addElementToResultModel(iwbBackgroundElementPart);
}

bool UBCFFAdaptor::UBToCFFConverter::ibwAddLine(int x1, int y1, int x2, int y2, QString color, int width, bool isBackground)
{
    bool bRet = true;

    QDomElement svgBackgroundCrossPart;
    QDomElement iwbBackgroundCrossPart;

    QString sUUID = QUuid::createUuid().toString();

    svgBackgroundCrossPart.setTagName(tIWBLine);

    svgBackgroundCrossPart.setAttribute(aX+"1", x1);
    svgBackgroundCrossPart.setAttribute(aY+"1", y1);  
    svgBackgroundCrossPart.setAttribute(aX+"2", x2);
    svgBackgroundCrossPart.setAttribute(aY+"2", y2);

    svgBackgroundCrossPart.setAttribute(aStroke, color);
    svgBackgroundCrossPart.setAttribute(aStrokeWidth, width);

    svgBackgroundCrossPart.setAttribute(aID, sUUID);

    if (isBackground)     
        bRet = ibwSetElementAsBackground(svgBackgroundCrossPart);

    bRet = addElementToResultModel(svgBackgroundCrossPart);

    if (!bRet)
    {
        qDebug() << "|error at creating crosses on background";
        errorStr = "CreatingCrossedBackgroundParsingError.";
    }

    return bRet;
}

QString UBCFFAdaptor::UBToCFFConverter::convertTransformFromUBZ(QString ubzTransform)
{
    QString sRes;

    return sRes;
}

bool UBCFFAdaptor::UBToCFFConverter::createBackground(const QDomElement &element)
{
    qDebug() << "|creating element background";

    bool bRet = true;

    QDomElement svgBackgroundElementPart;
    QDomElement iwbBackgroundElementPart;
    bool isDark = (avTrue == element.attribute(aDarkBackground));

    svgBackgroundElementPart.setTagName(tIWBRect);
    svgBackgroundElementPart.setAttribute(aFill, isDark ? "black" : "white");           

    QRect bckRect(mViewbox);

    if (0 <= mViewbox.topLeft().x())
        bckRect.topLeft().setX(0);

    if (0 <= mViewbox.topLeft().y())
        bckRect.topLeft().setY(0);

    QString sElementID = QUuid::createUuid().toString();

    svgBackgroundElementPart.setAttribute(aID, sElementID);
    svgBackgroundElementPart.setAttribute(aX, bckRect.x());
    svgBackgroundElementPart.setAttribute(aY, bckRect.y());
    svgBackgroundElementPart.setAttribute(aHeight, bckRect.height());
    svgBackgroundElementPart.setAttribute(aWidth, bckRect.width());

    bRet = ibwSetElementAsBackground(svgBackgroundElementPart);
    bRet = addElementToResultModel(svgBackgroundElementPart);

    if (!bRet)
    {
        qDebug() << "|error at creating element background";
        errorStr = "CreatingElementBackgroundParsingError.";
    }

    bool isCrossed = (avTrue == element.attribute(aCrossedBackground));
    if (isCrossed)
    {
        QString linesColor = isDark ? "white" : "blue";
        for (int i = mViewbox.x(); i < mViewbox.x()+mViewbox.width(); i+=iCrossSize)
        {
            bRet = ibwAddLine(i, mViewbox.x(), i, mViewbox.x()+mViewbox.height(), linesColor, iCrossWidth, true);
        }

        for (int i = mViewbox.y(); i < mViewbox.y()+mViewbox.height(); i+=iCrossSize)
        {
            bRet = ibwAddLine(mViewbox.x(), i, mViewbox.x()+mViewbox.width(), i, linesColor, iCrossWidth, true);
        }

        if (!bRet)
        {
            qDebug() << "|error at creating crossed background";
            errorStr = "CreatingElementBackgroundParsingError.";
        }
    }

    
    return bRet;
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

//     QDomElement iwbElementPart
    
    QString ubzTransform = element.attribute(aTransform);

    QString transform = convertTransformFromUBZ(, ubzTransform);

    qDebug() << "|parsing image";
    if (false) {
        qDebug() << "|error at image parsing";
        errorStr = "ImageParsingError";
        return false;
    }
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



bool UBCFFAdaptor::UBToCFFConverter::parseUBZPolyline(const QDomElement &element)
{
    bool bRes = true;

    QDomElement svgElementPart = element;
    
        

    bRes = addElementToResultModel(svgElementPart);

    qDebug() << "||parsing polyline";
    if (!bRes) {
        qDebug() << "||error at parsing polygon";
        errorStr = "PolylineParsingError";
     }
    return bRes;
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
    bool result = QFileInfo(sourcePath).exists()
               && QFileInfo(sourcePath).isDir()
               && errorStr == noErrorMsg;

    if (!result) {
        qDebug() << "specified data is not valid";
        errorStr = "ValidateDataError";
    }

    return result;
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
