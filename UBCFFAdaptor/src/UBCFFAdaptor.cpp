#include "UBCFFAdaptor.h"

#include <QtCore>
#include <QtXml>
#include <QTransform>
#include <QGraphicsItem>
#include <QPainter>

#include "UBGlobals.h"
#include "UBCFFConstants.h"

THIRD_PARTY_WARNINGS_DISABLE
#include "quazip.h"
#include "quazipfile.h"
#include "quazipfileinfo.h"
THIRD_PARTY_WARNINGS_ENABLE

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

    if (!compressZip(tmpDestination, to))
        qDebug() << "error in compression";

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
            if (!compressDir(curFile.absoluteFilePath(), parentDir + curFile.fileName() + "/", outZip)) {
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
    mDocumentToWrite = new QDomDocument; 
    mDocumentToWrite->setContent(QString("<doc></doc>"));

    mIWBContentWriter = new QXmlStreamWriter;
    mIWBContentWriter->setAutoFormatting(true);

    iwbSVGItemsAttributes.insert(tIWBImage, iwbSVGImageAttributes);
    iwbSVGItemsAttributes.insert(tIWBVideo, iwbSVGVideoAttributes);
    iwbSVGItemsAttributes.insert(tIWBText, iwbSVGTextAttributes);
    iwbSVGItemsAttributes.insert(tIWBTextArea, iwbSVGTextAreaAttributes);
    iwbSVGItemsAttributes.insert(tIWBPolyLine, iwbSVGPolyLineAttributes);
    iwbSVGItemsAttributes.insert(tIWBPolygon, iwbSVGPolygonAttributes);
    iwbSVGItemsAttributes.insert(tIWBRect, iwbSVGRectAttributes);
    iwbSVGItemsAttributes.insert(tIWBLine, iwbSVGLineAttributes);
    iwbSVGItemsAttributes.insert(tIWBTspan, iwbSVGTspanAttributes);
}

bool UBCFFAdaptor::UBToCFFConverter::parse()
{
    if(!isValid()) {
        qDebug() << "document metadata is not valid. Can't parse";
        return false;
    }

    qDebug() << "begin parsing ubz";

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

    mIWBContentWriter->writeAttribute(aIWBVersion, avIWBVersionNo);

    if (!parseMetadata()) {
        if (errorStr == noErrorMsg)
            errorStr = "MetadataParsingError";

        outFile.close();
        return false;
    }

    if (!parseContent()) {
        if (errorStr == noErrorMsg)
            errorStr = "ContentParsingError";
        outFile.close();
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

    QDomElement nextInElement = mDataModel->documentElement();

    nextInElement = nextInElement.firstChildElement(tDescription);
    if (!nextInElement.isNull()) {

        mIWBContentWriter->writeStartElement(iwbNS, tIWBMeta);
        mIWBContentWriter->writeAttribute(aIWBName, aAbout);
        mIWBContentWriter->writeAttribute(aIWBContent, nextInElement.attribute(aAbout));
        mIWBContentWriter->writeEndElement();

        nextInElement = nextInElement.firstChildElement();
        while (!nextInElement.isNull()) {

            QString textContent = nextInElement.text();
            if (!textContent.trimmed().isEmpty()) {
                if (nextInElement.tagName() == tUBZSize) { //taking main viewbox rect since for CFF specificaton we have static viewbox
                    QSize tmpSize = getSVGDimentions(nextInElement.text());
                    if (!tmpSize.isNull()) {
                        mSVGSize = tmpSize;
                        mViewbox.setRect(0,0, tmpSize.width(), tmpSize.height());
                    } else {
                        qDebug() << "can't interpret svg section size";
                        errorStr = "InterpretSvgSizeError";
                        return false;
                    }
                } else {
                    mIWBContentWriter->writeStartElement(iwbNS, tIWBMeta);
                    mIWBContentWriter->writeAttribute(aIWBName, nextInElement.tagName());
                    mIWBContentWriter->writeAttribute(aIWBContent, textContent);
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

    QDir sourceDir(sourcePath);
    QStringList fileFilters;
    fileFilters << QString(pageAlias + "???." + pageFileExtentionUBZ);
    QStringList pageList = sourceDir.entryList(fileFilters, QDir::Files, QDir::Name | QDir::IgnoreCase);

    mIWBContentWriter->writeStartElement(svgIWBNS, tSvg);

    if (!mSVGSize.isNull()) { //If viewbox has been set
        mIWBContentWriter->writeAttribute(aWidth, QString("%1").arg(mSVGSize.width()));
        mIWBContentWriter->writeAttribute(aHeight, QString("%1").arg(mSVGSize.height()));        
    }
    if (!mViewbox.isNull()) { //If viewbox has been set
        mIWBContentWriter->writeAttribute(aIWBViewBox, rectToIWBAttr(mViewbox));
    }

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

    mIWBContentWriter->writeEndElement();//iwb svg section

    if (!writeExtendedIwbSection()) {
        if (errorStr == noErrorMsg)
            errorStr = "writeExtendedIwbSectionError";
        return false;
    }

    return true;
}

bool UBCFFAdaptor::UBToCFFConverter::parsePage(const QString &pageFileName)
{
    qDebug() << "begin parsing page" + pageFileName;
    mSvgElements.clear(); //clean Svg elements map before parsing new page

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

    if (!writeSVGIwbPageSection()) {
        if (errorStr == noErrorMsg)
            errorStr = "writeSVGIwbSectionError";
        return false;
    }
    
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
        if      (tagName == tUBZG)             parseSVGGGroup(nextElement);
        else if (tagName == tUBZImage)         parseUBZImage(nextElement);
        else if (tagName == tUBZVideo)         parseUBZVideo(nextElement);
        else if (tagName == tUBZAudio)         parseUBZAudio(nextElement);
        else if (tagName == tUBZForeignObject) parseForeignObject(nextElement);

        nextElement = nextElement.nextSiblingElement();
    }

    return true;
}

void UBCFFAdaptor::UBToCFFConverter::writeQDomElementToXML(const QDomNode &node)
{
    if (!node.isNull())
    if (node.isText())
    {     
        mIWBContentWriter->writeCharacters(node.nodeValue());
    }   
    else
    {
        mIWBContentWriter->writeStartElement(node.namespaceURI(), node.toElement().tagName());

        for (int i = 0; i < node.toElement().attributes().count(); i++)
        {
            QDomAttr attr =  node.toElement().attributes().item(i).toAttr();
            mIWBContentWriter->writeAttribute(attr.name(), attr.value());
        }
        QDomNode child = node.firstChild();
        while(!child.isNull())
        {
            writeQDomElementToXML(child);
            child = child.nextSibling();
        }

        mIWBContentWriter->writeEndElement();
    }       
}

bool UBCFFAdaptor::UBToCFFConverter::writeSVGIwbPageSection()
{
    if (!mSvgElements.count()) {
        qDebug() << "svg content list is empty";
        errorStr = "EmptySvgSectionContentError";
        return false;
    }

    QMapIterator<int, QDomElement> nextSVGElement(mSvgElements);
    while (nextSVGElement.hasNext()) 
    {
        writeQDomElementToXML(nextSVGElement.next().value());
        //TODO write svg element to mIWBContentWriter
    }

    return true;
}

bool UBCFFAdaptor::UBToCFFConverter::writeExtendedIwbSection()
{
    if (!mExtendedElements.count()) {
        qDebug() << "extended iwb content list is empty";
        errorStr = "EmptyExtendedIwbSectionContentError";
        return false;
    }
    QListIterator<QDomElement> nextExtendedIwbElement(mExtendedElements);
    while (nextExtendedIwbElement.hasNext()) {
        writeQDomElementToXML(nextExtendedIwbElement.next());
        //TODO write iwb extended element to mIWBContentWriter
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
            if (avUBZText == element.attribute(aUBZType))
                sRet = tIWBTextArea;
            else
                sRet = element.attribute(aUBZType);
        }
        else
        {      
            if (element.hasAttribute(aSrc))
                sPath = element.attribute(aSrc);
            else 
            if (element.hasAttribute(aIWBHref))
                sPath = element.attribute(aIWBHref);

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

int UBCFFAdaptor::UBToCFFConverter::getElementLayer(const QDomElement &element)
{
    int zLayer = 0;
    if (element.hasAttribute(aZLayer))
        zLayer = (int)element.attribute(aZLayer).toDouble();

    if (element.hasAttribute(aLayer))
        return element.attribute(aLayer).toInt()+zLayer;
    else 
        return DEFAULT_LAYER+zLayer;
}

bool UBCFFAdaptor::UBToCFFConverter::itIsSupportedFormat(const QString &format) const
{
    bool bRet;

    QStringList tsl = format.split(".", QString::SkipEmptyParts);
    if (0 < tsl.count())
        bRet = cffSupportedFileFormats.contains(tsl.at(tsl.count()-1));       
    else
        bRet = false;

    return bRet;
}

bool UBCFFAdaptor::UBToCFFConverter::itIsSVGElementAttribute(const QString ItemType, const QString &AttrName)
{
    QString allowedElementAttributes = iwbSVGItemsAttributes[ItemType];
  
    allowedElementAttributes.remove("/t");
    allowedElementAttributes.remove(" ");
    foreach(QString attr, allowedElementAttributes.split(","))
    {
        if (AttrName == attr.trimmed())
            return true;
    }
    return false;
}


bool UBCFFAdaptor::UBToCFFConverter::itIsIWBAttribute(const QString &attribute) const
{
    foreach (QString attr, iwbElementAttributes.split(","))
    {
       if (attribute == attr.trimmed())
           return true;
    }
    return false;
}

bool UBCFFAdaptor::UBToCFFConverter::itIsUBZAttributeToConvert(const QString &attribute) const
{
    foreach (QString attr, ubzElementAttributesToConvert.split(","))
    {
        if (attribute == attr.trimmed())
            return true;
    }
    return false;
}

bool UBCFFAdaptor::UBToCFFConverter::ibwAddLine(int x1, int y1, int x2, int y2, QString color, int width, bool isBackground)
{
    bool bRet = true;

    QDomDocument doc;

    QDomElement svgBackgroundCrossPart = doc.createElementNS(svgIWBNS,svgIWBNSPrefix + ":line");
    QDomElement iwbBackgroundCrossPart = doc.createElementNS(iwbNS,iwbNsPrefix + ":" + tElement);

    QString sUUID = QUuid::createUuid().toString().remove("{").remove("}");

    svgBackgroundCrossPart.setTagName(tIWBLine);

    svgBackgroundCrossPart.setAttribute(aX+"1", x1);
    svgBackgroundCrossPart.setAttribute(aY+"1", y1);  
    svgBackgroundCrossPart.setAttribute(aX+"2", x2);
    svgBackgroundCrossPart.setAttribute(aY+"2", y2);

    svgBackgroundCrossPart.setAttribute(aStroke, color);
    svgBackgroundCrossPart.setAttribute(aStrokeWidth, width);

    svgBackgroundCrossPart.setAttribute(aID, sUUID);

    if (isBackground)     
    {
        iwbBackgroundCrossPart.setAttribute(aRef, sUUID);
        iwbBackgroundCrossPart.setAttribute(aLocked, avTrue);

        bRet &= addIWBElementToResultModel(iwbBackgroundCrossPart);
    }

    bRet &= addSVGElementToResultModel(svgBackgroundCrossPart, DEFAULT_BACKGROUND_CROSS_LAYER);

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

qreal UBCFFAdaptor::UBToCFFConverter::getAngleFromTransform(QTransform &tr)
{
    qreal angle = -(atan(tr.m21()/tr.m11())*180/PI);
    if (tr.m21() > 0 && tr.m11() < 0) 
        angle += 180;
    else 
        if (tr.m21() < 0 && tr.m11() < 0) 
            angle += 180;
    return angle;
}

void UBCFFAdaptor::UBToCFFConverter::setGeometryFromUBZ(const QDomElement &ubzElement, QDomElement &iwbElement)
{
    setCoordinatesFromUBZ(ubzElement,iwbElement);

  
 
}

void UBCFFAdaptor::UBToCFFConverter::setCoordinatesFromUBZ(const QDomElement &ubzElement, QDomElement &iwbElement)
{
    QTransform tr;

    if (QString() != ubzElement.attribute(aTransform))
        tr = getTransformFromUBZ(ubzElement);

    qreal x = ubzElement.attribute(aX).toDouble();
    qreal y = ubzElement.attribute(aY).toDouble();
    qreal height = ubzElement.attribute(aHeight).toDouble();
    qreal width = ubzElement.attribute(aWidth).toDouble();

    qreal alpha = getAngleFromTransform(tr);
 
    QRectF itemRect;
    QGraphicsRectItem item;

    item.setRect(0,0, width, height);
    item.setTransform(tr);

    item.setTransformOriginPoint(item.boundingRect().center());
    item.setRotation(-alpha);
    QMatrix sceneMatrix = item.sceneMatrix();
 
    iwbElement.setAttribute(aX, x);
    iwbElement.setAttribute(aY, y);
    iwbElement.setAttribute(aHeight, height*sceneMatrix.m22());
    iwbElement.setAttribute(aWidth, width*sceneMatrix.m11());
    iwbElement.setAttribute(aTransform, QString("rotate(%1) translate(%2,%3)").arg(alpha)
                                                                              .arg(sceneMatrix.dx())
                                                                              .arg(sceneMatrix.dy()));
}

bool UBCFFAdaptor::UBToCFFConverter::setContentFromUBZ(const QDomElement &ubzElement, QDomElement &svgElement)
{
    bool bRet = true;
   
    QString srcPath;
    if (tUBZForeignObject != ubzElement.tagName())
        srcPath = ubzElement.attribute(aUBZHref);
    else 
        srcPath = ubzElement.attribute(aSrc);

    QString sFilename = getFileNameFromPath(srcPath);
    
    QString sSrcContentFolder = getSrcContentFolderName(srcPath);
    QString sSrcFileName(sourcePath + "/" + sSrcContentFolder + "/" + sFilename);

    if (itIsSupportedFormat(sSrcFileName))
    { 

        QString sDstContentFolder = getDstContentFolderName(ubzElement.tagName());
        QStringList tl = sSrcFileName.split(".");
        QString fileExtantion = tl.at(tl.count()-1);
        QString sDstFileName(QUuid::createUuid().toString()+"."+fileExtantion);
        sDstFileName = sDstFileName.remove("{").remove("}");
  
        QFile srcFile;
        srcFile.setFileName(sSrcFileName);

        QDir dstDocFolder(destinationPath);
        
        if (!dstDocFolder.exists(sDstContentFolder))
            bRet &= dstDocFolder.mkdir(sDstContentFolder);

        if (bRet)
        {
            QString dstFilePath = destinationPath+"/"+sDstContentFolder+"/"+sDstFileName;
            bRet &= srcFile.copy(dstFilePath);
        }


        if (bRet)
            svgElement.setAttribute(aSVGHref, sDstContentFolder+"/"+sDstFileName);
    }
    else 
    {
        qDebug() << "format is not supported by CFF";
        bRet = false;
    }

    return bRet;
}

void UBCFFAdaptor::UBToCFFConverter::setCFFTextFromHTMLTextNode(const QDomElement htmlTextNode,  QDomElement &iwbElement)
{

    QDomDocument textDoc;
               
    QDomElement textParentElement = iwbElement;

    QString textString;
    QDomNode htmlPNode =  htmlTextNode.firstChild();
    bool bTbreak = false;

    // reads HTML text strings - each string placed in separate <p> section 
    while(!htmlPNode.isNull())
    {  
        // add <tbreak> for split strings
        if (bTbreak)
        {
            bTbreak = false;
            
            QDomElement tbreakNode = textDoc.createElementNS(svgIWBNS, svgIWBNSPrefix+":"+tIWBTbreak);
            textParentElement.appendChild(tbreakNode.cloneNode(true));
        }

        QDomNode spanNode = htmlPNode.firstChild();

       
        while (!spanNode.isNull())
        {
            if (spanNode.isText())
            {
                QDomText nodeText = textDoc.createTextNode(spanNode.nodeValue());
                textParentElement.appendChild(nodeText.cloneNode(true));
            }
            else
            if (spanNode.isElement())
            {      
                QDomElement spanElement = textDoc.createElementNS(svgIWBNS,svgIWBNSPrefix + ":" + tIWBTspan);
                if (spanNode.hasAttributes())
                {       
                    int attrCount = spanNode.attributes().count();
                    if (0 < attrCount)
                    {
                        for (int i = 0; i < attrCount; i++)
                        {
                            // html attributes like: style="font-size:40pt; color:"red";".
                            QStringList cffAttributes = spanNode.attributes().item(i).nodeValue().split(";", QString::SkipEmptyParts);
                            {
                                for (int i = 0; i < cffAttributes.count(); i++)
                                {                       
                                    QString attr = cffAttributes.at(i).trimmed();
                                    QStringList AttrVal = attr.split(":", QString::SkipEmptyParts);
                                    if(1 < AttrVal.count())
                                    {    
                                        QString sAttr = ubzAttrNameToCFFAttrName(AttrVal.at(0));
                                        if (itIsSVGElementAttribute(spanElement.tagName(), sAttr))
                                            spanElement.setAttribute(sAttr, ubzAttrValueToCFFAttrName(AttrVal.at(1)));
                                    }
                                }
                            }
                        }
                    }
                }
                QDomText nodeText = textDoc.createTextNode(spanNode.firstChild().nodeValue());                    
                spanElement.appendChild(nodeText);
                textParentElement.appendChild(spanElement.cloneNode(true));
            }
            spanNode = spanNode.nextSibling();
        }

    bTbreak = true;
    htmlPNode = htmlPNode.nextSibling();
    }
}

QString UBCFFAdaptor::UBToCFFConverter::ubzAttrNameToCFFAttrName(QString cffAttrName)
{
    QString sRet = cffAttrName;
    if (QString("color") == cffAttrName)
        sRet = QString("fill");

    return sRet;
}
QString UBCFFAdaptor::UBToCFFConverter::ubzAttrValueToCFFAttrName(QString cffValue)
{
    QString sRet = cffValue;
    if (QString("text") == cffValue)
        sRet = QString("normal");

    return sRet;
}

bool UBCFFAdaptor::UBToCFFConverter::setCFFAttribute(const QString &attributeName, const QString &attributeValue, const QDomElement &ubzElement, QDomElement &iwbElement,  QDomElement &svgElement)
{  
    bool bRet = true;
    bool bNeedsIWBSection = false;
    


    if (itIsIWBAttribute(attributeName))
    {
        iwbElement.setAttribute(attributeName, attributeValue);
        bNeedsIWBSection = true;
    }
    else
    if (itIsUBZAttributeToConvert(attributeName))
    {
        if (aTransform == attributeName)
        {
            setGeometryFromUBZ(ubzElement, svgElement);
        }
        else 
            if (attributeName.contains(aIWBHref)||attributeName.contains(aSrc))
            {
                bRet &= setContentFromUBZ(ubzElement, svgElement);
                bNeedsIWBSection = bRet||bNeedsIWBSection;
            }
    }
    else
    if (itIsSVGElementAttribute(svgElement.tagName(),attributeName))
    {
            svgElement.setAttribute(attributeName,  attributeValue);
    }

    if (bNeedsIWBSection)  
    {
        if (0 < iwbElement.attributes().count())
        {

            QStringList tl = ubzElement.attribute(aSVGHref).split("/");
            QString id = tl.at(tl.count()-1);
            // if element already have an ID, we use it. Else we create new id for element.
            if (QString() == id)
                id = QUuid::createUuid().toString().remove("{").remove("}");

            svgElement.setAttribute(aID, id);  
            iwbElement.setAttribute(aRef, id);
        }
    }

    return bRet;
}

bool UBCFFAdaptor::UBToCFFConverter::setCommonAttributesFromUBZ(const QDomElement &ubzElement, QDomElement &iwbElement,  QDomElement &svgElement)
{    
    bool bRet = true;

    for (int i = 0; i < ubzElement.attributes().count(); i++)
    {
        QDomNode attribute = ubzElement.attributes().item(i);
        QString attributeName = ubzAttrNameToCFFAttrName(attribute.nodeName().remove("ub:"));

        bRet &= setCFFAttribute(attributeName, ubzAttrValueToCFFAttrName(attribute.nodeValue()), ubzElement, iwbElement, svgElement);
        if (!bRet) break;
    }
    return bRet;
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

    QDomDocument doc;

    QDomElement svgBackgroundElementPart = doc.createElementNS(svgIWBNS,svgIWBNSPrefix + ":" + tUBZImage);
    QDomElement iwbBackgroundElementPart = doc.createElementNS(iwbNS,iwbNsPrefix + ":" + tElement);


    QRect bckRect(mViewbox);

    if (0 <= mViewbox.topLeft().x())
        bckRect.topLeft().setX(0);

    if (0 <= mViewbox.topLeft().y())
        bckRect.topLeft().setY(0);

    QString backgroundImagePath = createBackgroundImage(element, QSize(bckRect.width(), bckRect.height()));
    bRet &= (QString() != backgroundImagePath);
    if (bRet)
    {     
        QString sElementID = QUuid::createUuid().toString().remove("{").remove("}");

        svgBackgroundElementPart.setAttribute(aID, sElementID);
        svgBackgroundElementPart.setAttribute(aX, bckRect.x());
        svgBackgroundElementPart.setAttribute(aY, bckRect.y());
        svgBackgroundElementPart.setAttribute(aHeight, bckRect.height());
        svgBackgroundElementPart.setAttribute(aWidth, bckRect.width());
        svgBackgroundElementPart.setAttribute(aSVGHref, backgroundImagePath);

        iwbBackgroundElementPart.setAttribute(aRef, sElementID);
        iwbBackgroundElementPart.setAttribute(aBackground, avTrue);
        iwbBackgroundElementPart.setAttribute(aLocked, avTrue);


        bRet &= addSVGElementToResultModel(svgBackgroundElementPart, DEFAULT_BACKGROUND_LAYER);
        bRet &= addIWBElementToResultModel(iwbBackgroundElementPart);
    }

    if (!bRet)
    {
        qDebug() << "|error at creating element background";
        errorStr = "CreatingElementBackgroundParsingError.";
    }

    return bRet;
}

QString UBCFFAdaptor::UBToCFFConverter::createBackgroundImage(const QDomElement &element, QSize size)
{
    QString sRet;

    QRect rect(0,0, size.width(), size.height());

    QImage *bckImage = new QImage(size, QImage::Format_RGB888);

    QPainter *painter = new QPainter(bckImage);   

    bool darkBackground = (avTrue == element.attribute(aDarkBackground));
 
    QColor bCrossColor;

    bCrossColor = darkBackground?QColor(Qt::white):QColor(Qt::blue);
    int penAlpha = (int)(255/2); // default Sankore value for transform.m11 < 1
    bCrossColor.setAlpha(penAlpha);
    painter->setPen(bCrossColor);
    painter->setBrush(darkBackground?QColor(Qt::black):QColor(Qt::white));

    painter->drawRect(rect);

    if (avTrue == element.attribute(aCrossedBackground))
    {    
        qreal firstY = ((int) (rect.y () / iCrossSize)) * iCrossSize;

        for (qreal yPos = firstY; yPos <= rect.y () + rect.height (); yPos += iCrossSize)
        {
            painter->drawLine (rect.x (), yPos, rect.x () + rect.width (), yPos);
        }

        qreal firstX = ((int) (rect.x () / iCrossSize)) * iCrossSize;

        for (qreal xPos = firstX; xPos <= rect.x () + rect.width (); xPos += iCrossSize)
        {
            painter->drawLine (xPos, rect.y (), xPos, rect.y () + rect.height ());
        }
    }
    
    painter->end();
    painter->save();
    

    QString sDstContentFolder = "images"; 
    QString sDstFileName("background.png");

    bool bDirExists = true;
    QDir dstDocFolder(destinationPath);
    if (!dstDocFolder.exists(sDstContentFolder))
        bDirExists &= dstDocFolder.mkdir(sDstContentFolder);

    QString dstFilePath;
    if (bDirExists)
        dstFilePath = destinationPath+"/"+sDstContentFolder+"/"+sDstFileName;

    if (QString() != dstFilePath)
       if (bckImage->save(dstFilePath))
           sRet = sDstContentFolder+"/"+sDstFileName;

    delete bckImage;
    delete painter;

    return sRet;
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

    while (!nextElement.isNull()) {
        QString tagName = nextElement.tagName();
        if      (tagName == tUBZLine     && !parseUBZLine(nextElement))     return false;
        else if (tagName == tUBZPolygon  && !parseUBZPolygon(nextElement))  return false;
        else if (tagName == tUBZPolyline && !parseUBZPolyline(nextElement)) return false;

        nextElement = nextElement.nextSiblingElement();
    }
    
    return true;
}
bool UBCFFAdaptor::UBToCFFConverter::parseUBZImage(const QDomElement &element)
{
    qDebug() << "|parsing image";

    bool bRes = true;

    QDomDocument doc;

    QDomElement svgElementPart = doc.createElementNS(svgIWBNS,svgIWBNSPrefix + ":" + getElementTypeFromUBZ(element));
    QDomElement iwbElementPart = doc.createElementNS(iwbNS,iwbNsPrefix + ":" + tElement);

    bRes &= setCommonAttributesFromUBZ(element, iwbElementPart, svgElementPart);
    if (bRes)
    {
        bRes &= addSVGElementToResultModel(svgElementPart, getElementLayer(element));   
        if (0 < iwbElementPart.attributes().count())
            bRes &= addIWBElementToResultModel(iwbElementPart);
    }  
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
    QDomElement iwbElementPart = doc.createElementNS(iwbNS,iwbNsPrefix + ":" + tElement);

    bRes &= setCommonAttributesFromUBZ(element, iwbElementPart, svgElementPart);   
    if (bRes)
    {
        bRes &= addSVGElementToResultModel(svgElementPart, getElementLayer(element));    
        if (0 < iwbElementPart.attributes().count())
            bRes &= addIWBElementToResultModel(iwbElementPart);  
    }

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
    QDomElement iwbElementPart = doc.createElementNS(iwbNS,iwbNsPrefix + ":" + tElement);

    bRes &= setCommonAttributesFromUBZ(element, iwbElementPart, svgElementPart);

    if (bRes)
    {
        bRes &= addSVGElementToResultModel(svgElementPart, getElementLayer(element));    
        if (0 < iwbElementPart.attributes().count())
            bRes &= addIWBElementToResultModel(iwbElementPart);
    }
    
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
    QDomElement iwbElementPart = doc.createElementNS(iwbNS,iwbNsPrefix + ":" + tElement);

    bRes &= setCommonAttributesFromUBZ(element, iwbElementPart, svgElementPart);

    if (bRes)
    {
        bRes &= addSVGElementToResultModel(svgElementPart, getElementLayer(element)); 
        if (0 < iwbElementPart.attributes().count())
            bRes &= addIWBElementToResultModel(iwbElementPart);
    }

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
    QDomElement iwbElementPart = doc.createElementNS(iwbNS,iwbNsPrefix + ":" + tElement);
    
    if (element.hasChildNodes())
    {
        QDomDocument htmlDoc;
        htmlDoc.setContent(findTextNode(element).nodeValue());
        QDomNode bodyNode = findNodeByTagName(htmlDoc.firstChildElement(), "body");

        setCFFTextFromHTMLTextNode(bodyNode.toElement(), svgElementPart);
        bRes &= setCommonAttributesFromUBZ(element, iwbElementPart, svgElementPart);

        QString commonParams;
        for (int i = 0; i < bodyNode.attributes().count(); i++)
        {
            commonParams += " " + bodyNode.attributes().item(i).nodeValue();
        }
        commonParams.remove(" ");
        commonParams.remove("'");

        QStringList commonAttributes = commonParams.split(";", QString::SkipEmptyParts);
        for (int i = 0; i < commonAttributes.count(); i++)
        {
            QStringList AttrVal = commonAttributes.at(i).split(":", QString::SkipEmptyParts);
            if (1 < AttrVal.count())
            {                
                QString sAttr = ubzAttrNameToCFFAttrName(AttrVal.at(0));
                QString sVal  = ubzAttrValueToCFFAttrName(AttrVal.at(1));

                setCFFAttribute(sAttr, sVal, element, iwbElementPart, svgElementPart);
            }
        }        
    }

    if (bRes)
    {
        bRes &= addSVGElementToResultModel(svgElementPart, getElementLayer(element));    
        if (0 < iwbElementPart.attributes().count())
            bRes &= addIWBElementToResultModel(iwbElementPart);
    }

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
    QDomElement iwbElementPart = doc.createElementNS(iwbNS,iwbNsPrefix + ":" + tElement);

    bRes &= setCommonAttributesFromUBZ(element, iwbElementPart, svgElementPart);
    if (0 < iwbElementPart.attributes().count())
    {   
        QString id = QUuid::createUuid().toString().remove("{").remove("}");
        svgElementPart.setAttribute(aID, id);
        iwbElementPart.setAttribute(aRef, id);
    }

    if (bRes)
    {
        bRes &= addSVGElementToResultModel(svgElementPart, getElementLayer(element));    
        if (0 < iwbElementPart.attributes().count())
            bRes &= addIWBElementToResultModel(iwbElementPart);
    }

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
    QDomElement iwbElementPart = doc.createElementNS(iwbNS,iwbNsPrefix + ":" + tElement);

    bRes &= setCommonAttributesFromUBZ(element, iwbElementPart, svgElementPart);

    if (0 < iwbElementPart.attributes().count())
    {
        QString id = QUuid::createUuid().toString().remove("{").remove("}");
        svgElementPart.setAttribute(aID, id);
        iwbElementPart.setAttribute(aRef, id);
    }

    if (bRes)
    {
        bRes &= addSVGElementToResultModel(svgElementPart, getElementLayer(element));  
        if (0 < iwbElementPart.attributes().count())
            bRes &= addIWBElementToResultModel(iwbElementPart);
    }

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
    QDomElement iwbElementPart = doc.createElementNS(iwbNS,iwbNsPrefix + ":" + tElement);

    bRes &= setCommonAttributesFromUBZ(element, iwbElementPart, svgElementPart);

    if (0 < iwbElementPart.attributes().count())
    {
        QString id = QUuid::createUuid().toString().remove("{").remove("}");
        svgElementPart.setAttribute(aID, id);
        iwbElementPart.setAttribute(aRef, id);
    }

    if (bRes)
    {
        bRes &= addSVGElementToResultModel(svgElementPart, getElementLayer(element));    
        if (0 < iwbElementPart.attributes().count())
            bRes &= addIWBElementToResultModel(iwbElementPart);
    }

    if (!bRes) 
    {
        qDebug() << "||error at parsing polygon";
        errorStr = "LineParsingError";
        return false;
    }
    return bRes;
}

bool UBCFFAdaptor::UBToCFFConverter::addSVGElementToResultModel(QDomElement &element, int layer)
{
    int elementLayer = (DEFAULT_LAYER == layer) ? DEFAULT_LAYER : layer;
    mSvgElements.setInsertInOrder(true);
    QDomElement rootElement = element.cloneNode(true).toElement();
    mDocumentToWrite->firstChildElement().appendChild(rootElement);
    mSvgElements.insert(elementLayer, rootElement);

    return true;
}

bool UBCFFAdaptor::UBToCFFConverter::addIWBElementToResultModel(QDomElement &element)
{
    QDomElement rootElement = element.cloneNode(true).toElement();
    mDocumentToWrite->firstChildElement().appendChild(rootElement);
    mExtendedElements.append(rootElement);
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
    //mIWBContentWriter->writeNamespace(ubNS, ubNSPrefix);
    //mIWBContentWriter->writeNamespace(dcNS, dcNSPrefix);
    mIWBContentWriter->writeNamespace(iwbNS, iwbNsPrefix);
    mIWBContentWriter->writeNamespace(svgIWBNS, svgIWBNSPrefix);
    mIWBContentWriter->writeNamespace(xlinkNS, xlinkNSPrefix);
    //mIWBContentWriter->writeNamespace(xsiNS, xsiPrefix);
    //mIWBContentWriter->writeAttribute(xsiPrefix+":"+xsiSchemaLocationPrefix, xsiShemaLocation);
}

QString UBCFFAdaptor::UBToCFFConverter::digitFileFormat(int digit) const
{
    return QString("%1").arg(digit, 3, 10, QLatin1Char('0'));
}
QString UBCFFAdaptor::UBToCFFConverter::contentIWBFileName() const
{
    return destinationPath + "/" + fIWBContent;
}

//setting SVG dimenitons
QSize UBCFFAdaptor::UBToCFFConverter::getSVGDimentions(const QString &element)
{

    QStringList dimList;

    dimList = element.split(dimensionsDelimiter1, QString::KeepEmptyParts);
    if (dimList.count() != 2) // row unlike 0x0
        return QSize();

    bool ok;

    int width = dimList.takeFirst().toInt(&ok);
    if (!ok || !width)
        return QSize();

    int height = dimList.takeFirst().toInt(&ok);
    if (!ok || !height)
        return QSize();

    return QSize(width, height);
}

//Setting viewbox rectangle
QRect UBCFFAdaptor::UBToCFFConverter::getViewboxRect(const QString &element) const
{
    QStringList dimList;

    dimList = element.split(dimensionsDelimiter2, QString::KeepEmptyParts);
    if (dimList.count() != 4) // row unlike 0 0 0 0
        return QRect();
    
    bool ok = false;

    int x = dimList.takeFirst().toInt(&ok);
    if (!ok || !x)
        return QRect();

    int y = dimList.takeFirst().toInt(&ok);
    if (!ok || !y)
        return QRect();

    int width = dimList.takeFirst().toInt(&ok);
    if (!ok || !width)
        return QRect();

    int height = dimList.takeFirst().toInt(&ok);
    if (!ok || !height)
        return QRect();

    return QRect(x, y, width, height);
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
