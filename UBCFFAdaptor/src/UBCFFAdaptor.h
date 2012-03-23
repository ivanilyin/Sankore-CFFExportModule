#ifndef UBCFFADAPTOR_H
#define UBCFFADAPTOR_H

#include "UBCFFAdaptor_global.h"

#include <QtCore>

class QTransform;
class QDomDocument;
class QDomElement;
class QDomNode;
class QuaZipFile;

class UBCFFADAPTORSHARED_EXPORT UBCFFAdaptor {
    class UBToCFFConverter;

public:
    UBCFFAdaptor();
    ~UBCFFAdaptor();

    bool convertUBZToIWB(const QString &from, const QString &to);
    bool deleteDir(const QString& pDirPath) const;

private:
    QString uncompressZip(const QString &zipFile);
    bool compressZip(const QString &source, const QString &destination);
    bool compressDir(const QString &dirName, const QString &parentDir, QuaZipFile *outZip);
    bool compressFile(const QString &fileName, const QString &parentDir, QuaZipFile *outZip);

    QString createNewTmpDir();
    bool freeDir(const QString &dir);
    void freeTmpDirs();

private:
    QStringList tmpDirs;

private:

    class UBToCFFConverter {

       static const int DEFAULT_LAYER = -100000;

    public:
        UBToCFFConverter(const QString &source, const QString &destination);
        ~UBToCFFConverter();
        bool isValid() const;
        QString lastErrStr() const {return errorStr;}
        bool parse();

    private:
        void fillNamespaces();

        bool parseMetadata();
        bool parseContent();
        bool parsePageset(const QStringList &pageFileNames);
        bool parsePage(const QString &pageFileName);
        bool parseSvgPageSection(const QDomElement &element);
        void writeQDomElementToXML(const QDomNode &node);
        bool writeSVGIwbPageSection();
        bool writeExtendedIwbSection();
        bool parseGroupPageSection(const QDomElement &element);

        bool createBackground(const QDomElement &element);
        QString createBackgroundImage(const QDomElement &element, QSize size);

        bool parseSVGGGroup(const QDomElement &element);
        bool parseUBZImage(const QDomElement &element);
        bool parseUBZVideo(const QDomElement &element);
        bool parseUBZAudio(const QDomElement &element);
        bool parseForeignObject(const QDomElement &element);
        bool parseUBZText(const QDomElement &element);

        bool parseUBZPolygon(const QDomElement &element);
        bool parseUBZPolyline(const QDomElement &element);
        bool parseUBZLine(const QDomElement &element);       
        bool addSVGElementToResultModel(QDomElement &element, int layer = DEFAULT_LAYER);
        bool addIWBElementToResultModel(QDomElement &element);

        qreal getAngleFromTransform(QTransform &tr);
        QString getDstContentFolderName(QString elementType);
        QString getSrcContentFolderName(QString href);
        QString getFileNameFromPath(QString sPath);
        QString getElementTypeFromUBZ(const QDomElement &element);

        int getElementLayer(const QDomElement &element);

        bool itIsSupportedFormat(const QString &format) const;
        bool itIsSVGElementAttribute(const QString ItemType, const QString &AttrName);
        bool itIsIWBAttribute(const QString &attribute) const;
        bool itIsUBZAttributeToConvert(const QString &attribute) const;

        bool ibwAddLine(int x1, int y1, int x2, int y2, QString color=QString(), int width=1, bool isBackground=false);

        QTransform getTransformFromUBZ(const QDomElement &ubzElement);
        void setGeometryFromUBZ(const QDomElement &ubzElement, QDomElement &iwbElement);
        void setCoordinatesFromUBZ(const QDomElement &ubzElement, QDomElement &iwbElement);
        bool setContentFromUBZ(const QDomElement &ubzElement, QDomElement &svgElement);
        void setCFFTextFromUBZ(const QDomElement &ubzElement, QDomElement &iwbElement, QDomElement &svgElement);
        void setCFFTextFromHTMLTextNode(const QDomElement htmlTextNode, QDomElement &iwbElement);
        QString ubzAttrNameToCFFAttrName(QString cffAttrName);
        QString ubzAttrValueToCFFAttrName(QString cffAttrValue);

        bool setCFFAttribute(const QString &attributeName, const QString &attributeValue, const QDomElement &ubzElement, QDomElement &iwbElement,  QDomElement &svgElement);
        bool setCommonAttributesFromUBZ(const QDomElement &ubzElement, QDomElement &iwbElement,  QDomElement &svgElement);

        QDomNode findTextNode(const QDomNode &node);
        QDomNode findNodeByTagName(const QDomNode &node, QString tagName);

        QSize getSVGDimentions(const QString &element);

        inline QRect getViewboxRect(const QString &element) const;
        inline QString rectToIWBAttr(const QRect &rect) const;
        inline QString digitFileFormat(int num) const;
        inline bool strToBool(const QString &in) const {return in == "true";}
        QString contentIWBFileName() const;

    private:
        QMap<QString, QString> iwbSVGItemsAttributes;
        QDomDocument *mDataModel; //model for reading indata
        QXmlStreamWriter *mIWBContentWriter; //stream to write outdata
        QSize mSVGSize; //svg page size
        QRect mViewbox; //Main viewbox parameter for CFF
        QString sourcePath; // dir with unpacked source data (ubz)
        QString destinationPath; //dir with unpacked destination data (iwb)
        QDomDocument *mDocumentToWrite; //document for saved QDomElements from mSvgElements and mExtendedElements
        QMultiMap<int, QDomElement> mSvgElements; //Saving svg elements to have a sorted by z order list of elements to write;
        QList<QDomElement> mExtendedElements; //Saving extended options of elements to be able to add them to the end of result iwb document;
        mutable QString errorStr; // last error string message

    public:
        operator bool() const {return isValid();}
    };

    class UBToUBZConverter {
    public:
        UBToUBZConverter();
    };


};

#endif // UBCFFADAPTOR_H
