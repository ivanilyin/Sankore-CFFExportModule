#include "UBCFFAdaptor.h"

#include <QtCore>
#include <QtXml>
#include <QTransform>

#include "UBGlobals.h"

THIRD_PARTY_WARNINGS_DISABLE
#include "quazip.h"
#include "quazipfile.h"
#include "quazipfileinfo.h"
THIRD_PARTY_WARNINGS_ENABLE


#define PI 3.1415926535

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
const QString tUBZTextContent = "itemTextContent";

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
const QString aUBZUuid = "uuid";
const QString aFill = "fill"; // IWB attribute contans color to fill

const QString aID = "id";   // ID of any svg element can be placed in to iwb section
const QString aRef = "ref"; // as reference for applying additional attributes
const QString aHref = "href"; // reference to file
const QString aSrc = "src";

const QString aX = "x";
const QString aY = "y";
const QString aWidth = "width";
const QString aHeight = "height";
const QString aStroke = "stroke";
const QString aStrokeWidth = "stroke-width";
const QString aPoints = "points";
const QString aZLayer = "z-value";
const QString aTransform = "transform";
const QString aLocked = "locked";


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

// Image formats supported by CFF exclude wgt. Wgt is widget from sankore, who places as it png preview.
const QString iwbElementImage(" \
wgt \
jpeg \
jpg \
bmp \
gif \
wmf \
emf \
png \
tif \
tiff \
");

// Video formats supported by CFF
const QString iwbElementVideo(" \
mpg \
mpeg \
swf \
");

// Audio formats supported by CFF
const QString iwbElementAudio(" \
mp3 \
wav \
");

const QString cffSupportedFileFormats(iwbElementImage + iwbElementVideo + iwbElementAudio);


// 1 to 1 copy to SVG section
const QString iwbElementAttributes(" \
background, \
background-fill, \
background-posture, \
flip, \
freehand, \
highlight, \
highlight-fill, \
list-style-type, \
list-style-type-fill, \
locked, \
replicate, \
revealer, \
stroke-lineshape-start, \
stroke-lineshape-end \
");

// cannot be copied 1 to 1 to SVG section
const QString ubzElementAttributesToConvert(" \
xlink:href \
src \
transform \
");

// additional attributes. Have references in SVG section.
const QString svgElementAttributes(" \
points \
fill \
fill-opacity \
stroke \
stroke-dasharray \
stroke-linecap \
stroke-opacity \
stroke-width \
stroke_linejoin \
requiredExtensions \
viewbox \
x \
y \
height \
width \
font-family \
font-size \
font-style \
font-weight \
font-stretch \
text-align \
");



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
                | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name))
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
    mResultDataModel = new QDomDocument;

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

    if (!createXMLOutputPattern()) {
        if (errorStr == noErrorMsg)
            errorStr = "createXMLOutputPatternError";
        return false;
    }

    QFile QDomModelFile(contentIWBFileName());
    if (!QDomModelFile.open(QIODevice::ReadWrite | QIODevice::Text)) {
        qDebug() << "can't open pattern file for writing ";
        errorStr = "OpenPatternFileError";
        return false;
    }

    int errorLine, errorColumn;
    if (!mResultDataModel->setContent(&QDomModelFile, true, &errorStr, &errorLine, &errorColumn)) {
            qWarning() << "Error:Parseerroratline" << errorLine << ","
                           << "column" << errorColumn << ":" << errorStr;
            return false;
    }

    if (!parseMetadata()) {
        if (errorStr == noErrorMsg)
            errorStr = "MetadataParsingError";
        return false;
    }

    if (!parseContent()) {
        if (errorStr == noErrorMsg)
            errorStr = "ContentParsingError";
        return false;
    }

    qDebug() << "finished with success";

    return true;
}
bool UBCFFAdaptor::UBToCFFConverter::createXMLOutputPattern()
{
    QFile outFile(contentIWBFileName());
    if (!outFile.open(QIODevice::WriteOnly| QIODevice::Text)) {
        qDebug() << "can't open output file for writing";
        errorStr = "createXMLOutputPatternError";
        return false;
    }
    mIWBContentWriter->setDevice(&outFile);

    mIWBContentWriter->writeStartDocument();
    mIWBContentWriter->writeStartElement(tIWBRoot);

    fillNamespaces();

    mIWBContentWriter->writeEndElement();
    mIWBContentWriter->writeEndDocument();

    outFile.close();
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

    QDomElement nextInElement = mDataModel->documentElement();
    QDomElement parentOutElement = mResultDataModel->documentElement().firstChildElement();
    if (parentOutElement.isNull()) {
        qDebug() << "The content.xml result document patern is invalid";
//        return false;
    }

    nextInElement = nextInElement.firstChildElement(tDescription);
    if (!nextInElement.isNull()) {

        mIWBContentWriter->writeStartElement(iwbNS, tIWBMeta);
        mIWBContentWriter->writeAttribute(aAbout, nextInElement.attribute(aAbout));
        mIWBContentWriter->writeEndElement();

        nextInElement = nextInElement.firstChildElement();
        while (!nextInElement.isNull()) {

            QString textContent = nextInElement.text();
            if (!textContent.trimmed().isEmpty()) {
                if (nextInElement.tagName() == tUBZSize) { //getting main viewbox rect since for CFF specificaton we have static viewbox
                    QRect tmpRect = getViewboxRect(nextInElement.text());
                    if (!tmpRect.isNull()) {
                        mViewbox = tmpRect;
                    } else {
                        qDebug() << "can't interpret viewbox rectangle";
                        errorStr = "InterpretViewboxRectangleError";
                        return false;
                    }
                } else {
                    mIWBContentWriter->writeStartElement(iwbNS, tIWBMeta);
                    mIWBContentWriter->writeAttribute(nextInElement.namespaceURI(), nextInElement.tagName(), textContent);
                    mIWBContentWriter->writeEndElement();
                }

            }
            nextInElement = nextInElement.nextSiblingElement();
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

QString UBCFFAdaptor::UBToCFFConverter::getDstContentFolderName(QString elementType)
{
    QString sRet;
    QString sDstContentFolderName;

    // widgets must be saved as .png images.
    if (("image" == elementType) || ("foreignObject" == elementType))
        sDstContentFolderName = "images";
    else
    if ("video" == elementType)
        sDstContentFolderName = "videos";    
    else
    if ("audio" == elementType)
        sDstContentFolderName = "audios";

    sRet = sDstContentFolderName;
 
    return sRet;
}

QString UBCFFAdaptor::UBToCFFConverter::getSrcContentFolderName(QString href)
{
    QString sRet;

    QStringList ls = href.split("/");
    if (0 < ls.count())
        sRet = ls.at(ls.count()-1);
    
    sRet = href.remove(sRet);

    if (sRet.endsWith("/"))
        sRet.remove("/");

    return sRet;   
}

QString UBCFFAdaptor::UBToCFFConverter::getFileNameFromPath(QString sPath)
{
    QString sRet;
    QStringList sl = sPath.split("/",QString::SkipEmptyParts);

    if (0 < sl.count())
    {
        QString name = sl.at(sl.count()-1);

        // if item is widtet - take preview.       
        if(name.endsWith(".wgt"))
        {
            qDebug() << "element is widget";
            name.remove(".wgt");
            name.remove("{");
            name.remove("}");
            name += ".png";
        }
        else    // .svg must be saved as .png because of it is not suppotrted in CFF.
        if(name.endsWith(".svg"))
        {
            qDebug() << "element is svg image";
            name.remove(".svg");
            name.remove("{");
            name.remove("}");
            name += ".png";
        }

        sRet = name;
    }
    return sRet;
}

QString UBCFFAdaptor::UBToCFFConverter::getElementTypeFromUBZ(const QDomElement &element)
{
    QString sRet;
    if (tUBZForeignObject == element.tagName())
    {
        QString sPath;
        if (element.hasAttribute(aUBZType))
        {
            if ("text" == element.attribute(aUBZType))
                sRet = "textarea";
            else
                sRet = element.attribute(aUBZType);
        }
        else
        {      
            if (element.hasAttribute(aSrc))
                sPath = element.attribute(aSrc);
            else 
            if (element.hasAttribute(aHref))
                sPath = element.attribute(aHref);

            QStringList tsl = sPath.split(".", QString::SkipEmptyParts);
            if (0 < tsl.count())
            {
                QString elementType = tsl.at(tsl.count()-1);
                if (iwbElementImage.contains(elementType))
                    sRet = tIWBImage;
                else 
                if (iwbElementAudio.contains(elementType))
                    sRet = tIWBAudio;
                else
                if (iwbElementVideo.contains(elementType))
                    sRet = tIWBVideo;   
            }
        }
    }
    else
        sRet = element.tagName();

    return sRet;
}

bool UBCFFAdaptor::UBToCFFConverter::itIsSupportedFormat(QString format)
{
    bool bRet;

    QStringList tsl = format.split(".", QString::SkipEmptyParts);
    if (0 < tsl.count())
        bRet = cffSupportedFileFormats.contains(tsl.at(tsl.count()-1));       
    else
        bRet = false;

    return bRet;
}

bool UBCFFAdaptor::UBToCFFConverter::itIsSVGAttribute(QString attribute)
{
    return svgElementAttributes.contains(attribute);
}

bool UBCFFAdaptor::UBToCFFConverter::itIsIWBAttribute(QString attribute)
{
    return iwbElementAttributes.contains(attribute);
}

bool UBCFFAdaptor::UBToCFFConverter::itIsUBZAttributeToConvert(QString attribute)
{
    return ubzElementAttributesToConvert.contains(attribute);
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

QTransform UBCFFAdaptor::UBToCFFConverter::getTransformFromUBZ(const QDomElement &ubzElement)
{
    QTransform trRet;
  
    QStringList transformParameters;

    QString ubzTransform = ubzElement.attribute(aTransform);
    ubzTransform.remove("matrix");
    ubzTransform.remove("(");
    ubzTransform.remove(")");

    transformParameters = ubzTransform.split(",", QString::SkipEmptyParts);

    if (6 <= transformParameters.count())
    {
        QTransform *tr = NULL;
        tr = new QTransform(transformParameters.at(0).toDouble(),
            transformParameters.at(1).toDouble(),
            transformParameters.at(2).toDouble(),
            transformParameters.at(3).toDouble(),
            transformParameters.at(4).toDouble(),
            transformParameters.at(5).toDouble());

        trRet = *tr;
        
        delete tr;
    }

    if (6 <= transformParameters.count())
    {
        QTransform *tr = NULL;
        tr = new QTransform(transformParameters.at(0).toDouble(),
            transformParameters.at(1).toDouble(),
            transformParameters.at(2).toDouble(),
            transformParameters.at(3).toDouble(),
            transformParameters.at(4).toDouble(),
            transformParameters.at(5).toDouble());

        trRet = *tr;
        
        delete tr;
    }
    return trRet;
}

void UBCFFAdaptor::UBToCFFConverter::setGeometryFromUBZ(const QDomElement &ubzElement, QDomElement &iwbElement)
{
    setCoordinatesFromUBZ(ubzElement,iwbElement);

    QTransform tr = getTransformFromUBZ(ubzElement);
    double angle = (atan(tr.m21()/tr.m11())*180/PI);
    if (tr.m21() > 0 && tr.m11() < 0) 
        angle += 180;
    else 
        if (tr.m21() < 0 && tr.m11() < 0) 
            angle += 180;

    iwbElement.setAttribute(aTransform, QString("rotate(%1)").arg(angle));
    // "translate" attribute can be added there, but Sankore doesn't use translation in the same sense, so it is not needed to add that attribute now.
}

void UBCFFAdaptor::UBToCFFConverter::setCoordinatesFromUBZ(const QDomElement &ubzElement, QDomElement &iwbElement)
{
    QTransform tr;

    if (QString() != ubzElement.attribute(aTransform))
        tr = getTransformFromUBZ(ubzElement);

    iwbElement.setAttribute(aX,ubzElement.attribute(aX).toDouble()+tr.m31());
    iwbElement.setAttribute(aY,ubzElement.attribute(aY).toDouble()+tr.m32());
    iwbElement.setAttribute(aHeight, ubzElement.attribute(aHeight));
    iwbElement.setAttribute(aWidth,  ubzElement.attribute(aWidth));
}

bool UBCFFAdaptor::UBToCFFConverter::setContentFromUBZ(const QDomElement &ubzElement, QDomElement &iwbElement)
{
    bool bRet = true;
   
    QString srcPath;
    if (tUBZForeignObject != ubzElement.tagName())
        srcPath = ubzElement.attribute(aHref);
    else 
        srcPath = ubzElement.attribute(aSrc);

    QString sFilename = getFileNameFromPath(srcPath);
    
    QString sSrcContentFolder = getSrcContentFolderName(srcPath);
    QString sSrcFileName(sourcePath + "/" + sSrcContentFolder + "/" + sFilename);

    if (itIsSupportedFormat(sSrcFileName))
    { 

        QString sDstContentFolder = getDstContentFolderName(ubzElement.tagName());
        QString sDstFileName(destinationPath+"/"+sDstContentFolder+"/"+sFilename);     
  
        QFile srcFile;
        srcFile.setFileName(sSrcFileName);

        QDir dstDocFolder(destinationPath);
        
        if (!dstDocFolder.exists(sDstContentFolder))
            bRet &= dstDocFolder.mkdir(sDstContentFolder);

        if (bRet)
            bRet &= srcFile.copy(sDstFileName);


        if (bRet)
            iwbElement.setAttribute(aHref, sDstContentFolder+"/"+sFilename);
    }
    else 
    {
        qDebug() << "format is not supported by CFF";
        bRet = false;
    }

    return bRet;
}

QString UBCFFAdaptor::UBToCFFConverter::getCFFTextFromHTMLTextNode(const QDomElement htmlTextNode)
{
    QDomDocument doc;
    

    QDomNode spanNode = htmlTextNode.firstChild().firstChild();
    
    QString textString;
    while (!spanNode.isNull())
    {
        if (htmlTextNode.isText())
            textString += htmlTextNode.nodeValue();
        else
        if (htmlTextNode.hasAttributes())
        {       
            QString newAttr;
            QString newVal;
            int attrCount = spanNode.attributes().count();
            if (0 < attrCount)
            {
                textString += "<svg:tspan";
                for (int i = 0; i < attrCount; i++)
                {
                    QStringList cffAttributes = spanNode.attributes().item(i).nodeValue().split(";", QString::SkipEmptyParts);
                    {
                        for (int i = 0; i < cffAttributes.count(); i++)
                        {                       
                            QString attr = cffAttributes.at(i).trimmed();
                            QStringList AttrVal = attr.split(":", QString::SkipEmptyParts);
                            if(1 < AttrVal.count())
                            {
                                newAttr = ubzAttrNamtToCFFAttrName(AttrVal.at(0));
                                newVal  = "\"" + AttrVal.at(1) + "\"";
                                textString += " " + newAttr + "=" + newVal;
                            }
                        }
                    }


                }
                textString += ">";
             
                QDomElement el = doc.createElementNS(svgIWBNS, svgIWBNSPrefix+"tspan");
//                el.setTagName()
                QDomText node = doc.createTextNode(spanNode.firstChild().nodeValue());
                


                textString += spanNode.firstChild().nodeValue();

                textString += "</svg:tspan>";
            }
        }


        spanNode = spanNode.nextSibling();
    }

    return textString;
}

QString UBCFFAdaptor::UBToCFFConverter::ubzAttrNamtToCFFAttrName(QString cffAttrName)
{
    QString sRet = cffAttrName;
    if (QString("color") == cffAttrName)
        sRet = QString("fill");

    return sRet;
}

void UBCFFAdaptor::UBToCFFConverter::setCFFAttribute(const QString &attributeName, const QString &attributeValue, const QDomElement &ubzElement, QDomElement &iwbElement,  QDomElement &svgElement)
{  
    if (itIsSVGAttribute(attributeName))
    {
        svgElement.setAttribute(attributeName,  attributeValue);
    }
    else
    if (itIsIWBAttribute(attributeName))
    {
        iwbElement.setAttribute(attributeName, attributeValue);
    }
    else
    if (itIsUBZAttributeToConvert(attributeName))
    {
        if (aTransform == attributeName)
        {
            setGeometryFromUBZ(ubzElement, svgElement);
        }
        else 
        if (attributeName.contains(aHref)||attributeName.contains(aSrc))
        {
            setContentFromUBZ(ubzElement, svgElement);
        }
    }

    if (0 < iwbElement.attributes().count())
    {

        QString id = ubzElement.attribute(aUBZUuid);
        // if element already have an ID, we use it. Else we create new id for element.
        if (QString() == id)
            id = QUuid::createUuid().toString();

        svgElement.setAttribute(aID, id);  
        iwbElement.setAttribute(aHref, id);
    }
}

void UBCFFAdaptor::UBToCFFConverter::setCommonAttributesFromUBZ(const QDomElement &ubzElement, QDomElement &iwbElement,  QDomElement &svgElement)
{
    qDebug() << "Parsing Common Attributes";

    for (int i = 0; i < ubzElement.attributes().count(); i++)
    {
        QDomNode attribute = ubzElement.attributes().item(i);
        QString attributeName = attribute.nodeName().remove("ub:");

        setCFFAttribute(attributeName, attribute.nodeValue(), ubzElement, iwbElement, svgElement);
    }
}

QDomNode UBCFFAdaptor::UBToCFFConverter::findTextNode(const QDomNode &node)
{
    QDomNode iterNode = node;

    while (!iterNode.isNull())
    {       
        if (iterNode.isText())
        {   
            if (!iterNode.isNull())              
                return iterNode;
        }
        else 
        {
            if (!iterNode.firstChild().isNull())
            {   
                QDomNode foundNode = findTextNode(iterNode.firstChild());
                if (!foundNode.isNull())
                    if (foundNode.isText())
                        return foundNode;
            }
        }
        if (!iterNode.nextSibling().isNull())
            iterNode = iterNode.nextSibling();
        else 
            break;
    }
    return iterNode;
}

QDomNode UBCFFAdaptor::UBToCFFConverter::findNodeByTagName(const QDomNode &node, QString tagName)
{
    QDomNode iterNode = node;

    while (!iterNode.isNull())
    {       
        QString t = iterNode.toElement().tagName();
        if (tagName == t)
            return iterNode;
        else 
        {
            if (!iterNode.firstChildElement().isNull())
            {
                QDomNode foundNode = findNodeByTagName(iterNode.firstChildElement(), tagName);
                if (!foundNode.isNull())
                    if (foundNode.isElement())
                    {
                        if (tagName == foundNode.toElement().tagName())
                            return foundNode;
                    }
                    else
                        break;
            }
        }
        
        if (!iterNode.nextSibling().isNull())
            iterNode = iterNode.nextSibling();
        else 
            break;
    }
    return QDomNode();

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

    if (avTrue == element.attribute(aCrossedBackground))
    {
        QString linesColor = isDark ? "white" : "blue";
        for (int i = bckRect.x(); i < bckRect.x()+bckRect.width(); i+=iCrossSize)
        {
            bRet = ibwAddLine(i, bckRect.x(), i, bckRect.x()+bckRect.height(), linesColor, iCrossWidth, true);
        }

        for (int i = bckRect.y(); i < bckRect.y()+bckRect.height(); i+=iCrossSize)
        {
            bRet = ibwAddLine(bckRect.x(), i, bckRect.x()+bckRect.width(), i, linesColor, iCrossWidth, true);
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
    qDebug() << "|parsing image";

    bool bRes = true;

    QDomDocument doc;

    QDomElement svgElementPart = doc.createElementNS(svgIWBNS,svgIWBNSPrefix + ":" + getElementTypeFromUBZ(element));
    QDomElement iwbElementPart = doc.createElementNS(svgIWBNS,svgIWBNSPrefix + ":" + tElement);

    setCommonAttributesFromUBZ(element, iwbElementPart, svgElementPart);
    bRes &= addElementToResultModel(svgElementPart);    
    bRes &= addElementToResultModel(iwbElementPart);
      
    if (!bRes) 
    {
        qDebug() << "|error at image parsing";
        errorStr = "ImageParsingError";
    }
    return bRes;
}
bool UBCFFAdaptor::UBToCFFConverter::parseUBZVideo(const QDomElement &element)
{
    qDebug() << "|parsing video";

    bool bRes = true;

    QDomDocument doc;

    QDomElement svgElementPart = doc.createElementNS(svgIWBNS,svgIWBNSPrefix + ":" + getElementTypeFromUBZ(element));
    QDomElement iwbElementPart = doc.createElementNS(svgIWBNS,svgIWBNSPrefix + ":" + tElement);

    setCommonAttributesFromUBZ(element, iwbElementPart, svgElementPart);   
    bRes &= addElementToResultModel(svgElementPart);    
    bRes &= addElementToResultModel(iwbElementPart);  

    if (!bRes) 
    {
        qDebug() << "|error at video parsing";
        errorStr = "VideoParsingError";
    }

    return bRes;
}
bool UBCFFAdaptor::UBToCFFConverter::parseUBZAudio(const QDomElement &element)
{
    qDebug() << "|parsing audio";

    bool bRes = true;

    QDomDocument doc;

    QDomElement svgElementPart = doc.createElementNS(svgIWBNS,svgIWBNSPrefix + ":" + getElementTypeFromUBZ(element));
    QDomElement iwbElementPart = doc.createElementNS(svgIWBNS,svgIWBNSPrefix + ":" + tElement);

    setCommonAttributesFromUBZ(element, iwbElementPart, svgElementPart);
    bRes &= addElementToResultModel(svgElementPart);    
    bRes &= addElementToResultModel(iwbElementPart);
    
    if (!bRes) 
    {
        qDebug() << "|error at audio parsing";
        errorStr = "AudioParsingError";
    }

    return bRes;
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

    qDebug() << "|parsing foreign object";

    bool bRes = true;

    QDomDocument doc;

    QDomElement svgElementPart = doc.createElementNS(svgIWBNS,svgIWBNSPrefix + ":" + getElementTypeFromUBZ(element));
    QDomElement iwbElementPart = doc.createElementNS(svgIWBNS,svgIWBNSPrefix + ":" + tElement);

    setCommonAttributesFromUBZ(element, iwbElementPart, svgElementPart);
    bRes &= addElementToResultModel(svgElementPart);    
    bRes &= addElementToResultModel(iwbElementPart);

    if (!bRes) 
    {
        qDebug() << "|error at parsing foreign object";
        errorStr = "ForeignObjectParsingError";
    }

    return bRes;
}

bool UBCFFAdaptor::UBToCFFConverter::parseUBZText(const QDomElement &element)
{
    qDebug() << "|parsing text";

    bool bRes = true;

    QDomDocument doc;

    QDomElement svgElementPart = doc.createElementNS(svgIWBNS,svgIWBNSPrefix + ":" + getElementTypeFromUBZ(element));
    QDomElement iwbElementPart = doc.createElementNS(svgIWBNS,svgIWBNSPrefix + ":" + tElement);
    
    setCommonAttributesFromUBZ(element, iwbElementPart, svgElementPart);

    if (element.hasChildNodes())
    {
        QDomDocument htmlDoc;
        htmlDoc.setContent(findTextNode(element).nodeValue());

        QDomNode bodyNode = findNodeByTagName(htmlDoc.firstChildElement(), "body");

        QString commonParams;

        for (int i = 0; i < bodyNode.attributes().count(); i++)
        {
            commonParams += " " + bodyNode.attributes().item(i).nodeValue();
        }
        qDebug() << "common parameters: " + commonParams;

        commonParams.remove(" ");
        commonParams.remove("'");

        QStringList commonAttributes = commonParams.split(";", QString::SkipEmptyParts);
        for (int i = 0; i < commonAttributes.count(); i++)
        {
            QStringList AttrVal = commonAttributes.at(i).split(":", QString::SkipEmptyParts);
            if (1 < AttrVal.count())
            {                
                QString sAttr = AttrVal.at(0);
                QString sVal  = AttrVal.at(1);

                setCFFAttribute(sAttr, sVal, element, iwbElementPart, svgElementPart);
            }
 
        }

        QString cffText = getCFFTextFromHTMLTextNode(bodyNode.toElement());

        QDomDocument textDoc;
        bool b = textDoc.setContent("<content>"+ cffText+ "</content>", true);

        qDebug() << textDoc.toString();
        
        QDomNode nextNode = textDoc.firstChild().firstChild();
        
        while (!nextNode.isNull())
        {
            qDebug() << nextNode.toElement().text();
            svgElementPart.appendChild(nextNode.cloneNode(true));        
            nextNode = nextNode.nextSibling();
        }
    }
    
    doc.appendChild(svgElementPart);
    doc.appendChild(iwbElementPart);

    QString s = doc.toString();
    qDebug() << s;

    bRes &= addElementToResultModel(svgElementPart);    
    bRes &= addElementToResultModel(iwbElementPart);

    if (!bRes)
    {
        qDebug() << "|error at text parsing";
        errorStr = "TextParsingError";
    }

    return bRes;
}

bool UBCFFAdaptor::UBToCFFConverter::parseUBZPolygon(const QDomElement &element)
{
    qDebug() << "||parsing polygon";

    bool bRes = true;

    QDomDocument doc;

    QDomElement svgElementPart = doc.createElementNS(svgIWBNS,svgIWBNSPrefix + ":" + getElementTypeFromUBZ(element));
    QDomElement iwbElementPart = doc.createElementNS(svgIWBNS,svgIWBNSPrefix + ":" + tElement);

    setCommonAttributesFromUBZ(element, iwbElementPart, svgElementPart);
    QString id = QUuid::createUuid().toString();
    svgElementPart.setAttribute(aID, id);
    if (0 < iwbElementPart.attributes().count())
        svgElementPart.setAttribute(aHref, id);

    bRes &= addElementToResultModel(svgElementPart);    
    bRes &= addElementToResultModel(iwbElementPart);

    if (!bRes)
    {
        qDebug() << "||error at parsing polygon";
        errorStr = "PolygonParsingError";
    }
    return bRes;
}



bool UBCFFAdaptor::UBToCFFConverter::parseUBZPolyline(const QDomElement &element)
{
    qDebug() << "||parsing polyline";
    bool bRes = true;

    QDomDocument doc;

    QDomElement svgElementPart = doc.createElementNS(svgIWBNS,svgIWBNSPrefix + ":" + getElementTypeFromUBZ(element));
    QDomElement iwbElementPart = doc.createElementNS(svgIWBNS,svgIWBNSPrefix + ":" + tElement);

    setCommonAttributesFromUBZ(element, iwbElementPart, svgElementPart);

    QString id = QUuid::createUuid().toString();
    svgElementPart.setAttribute(aID, id);
    if (0 < iwbElementPart.attributes().count())
        svgElementPart.setAttribute(aHref, id);

    bRes &= addElementToResultModel(svgElementPart);    
    bRes &= addElementToResultModel(iwbElementPart);

    if (!bRes) {
        qDebug() << "||error at parsing polygon";
        errorStr = "PolylineParsingError";
     }
    return bRes;
}

bool UBCFFAdaptor::UBToCFFConverter::parseUBZLine(const QDomElement &element)
{   
    qDebug() << "||parsing line";
    bool bRes = true;

    QDomDocument doc;

    QDomElement svgElementPart = doc.createElementNS(svgIWBNS,svgIWBNSPrefix + ":" + getElementTypeFromUBZ(element));
    QDomElement iwbElementPart = doc.createElementNS(svgIWBNS,svgIWBNSPrefix + ":" + tElement);

    setCommonAttributesFromUBZ(element, iwbElementPart, svgElementPart);

    QString id = QUuid::createUuid().toString();
    svgElementPart.setAttribute(aID, id);
    if (0 < iwbElementPart.attributes().count())
        svgElementPart.setAttribute(aHref, id);

    bRes &= addElementToResultModel(svgElementPart);    
    bRes &= addElementToResultModel(iwbElementPart);

    if (!bRes) 
    {
        qDebug() << "||error at parsing polygon";
        errorStr = "LineParsingError";
        return false;
    }
    return bRes;
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
QString UBCFFAdaptor::UBToCFFConverter::contentIWBFileName() const{
    return destinationPath + "/" + fIWBContent;
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
