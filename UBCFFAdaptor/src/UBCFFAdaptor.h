#ifndef UBCFFADAPTOR_H
#define UBCFFADAPTOR_H

#include "UBCFFAdaptor_global.h"

#include <QtCore>

class QDomDocument;
class QDomElement;
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
        bool parseGroupPageSection(const QDomElement &element);

        bool createBackground(const QDomElement &element);

        bool parseSVGGGroup(const QDomElement &element);
        bool parseUBZImage(const QDomElement &element);
        bool parseUBZVideo(const QDomElement &element);
        bool parseUBZAudio(const QDomElement &element);
        bool parseForeignObject(const QDomElement &element);
        bool parseUBZText(const QDomElement &element);

        bool parseUBZPolygon(const QDomElement &element);
        bool parseUBZPolyline(const QDomElement &element);
        bool parseUBZLine(const QDomElement &element);
        bool addElementToResultModel(QDomElement &element){return true;}

        bool ibwSetElementAsBackground(QDomElement &element);

        bool ibwAddLine(int x1, int y1, int x2, int y2, QString color=QString(), int width=1, bool isBackground=false);

        QString convertTransformFromUBZ(QString ubzTransform);

        inline QRect getViewboxRect(const QString &element) const;
        inline QString rectToIWBAttr(const QRect &rect) const;
        inline QString digitFileFormat(int num) const;
        inline bool strToBool(const QString &in) const {return in == "true";}

    private:
        QDomDocument *mDataModel; //model for reading indata
        QXmlStreamWriter *mIWBContentWriter; //stream to write outdata
        QRect mViewbox; //Main viewbox parameter for CFF
        QString sourcePath; // dir with unpacked source data (ubz)
        QString destinationPath; //dir with unpacked destination data (iwb)
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