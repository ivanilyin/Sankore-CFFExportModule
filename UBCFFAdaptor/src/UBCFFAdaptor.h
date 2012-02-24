#ifndef UBCFFADAPTOR_H
#define UBCFFADAPTOR_H

#include "UBCFFAdaptor_global.h"

#include <QtCore>

class QDomDocument;
class QDomElement;

class UBCFFADAPTORSHARED_EXPORT UBCFFAdaptor {
    class UBToCFFConverter;

public:
    UBCFFAdaptor();

    bool convertUBZToIWB(const QString &from, const QString &to);

private:
    QString uncompressZip() {return QString();}

private:
    UBToCFFConverter *converter;



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

        bool createDarkBackground(const QDomElement &element);
        bool createCrossedBackground(const QDomElement &element);

        bool parseSVGGGroup(const QDomElement &element);
        bool parseUBZImage(const QDomElement &element);
        bool parseUBZVideo(const QDomElement &element);
        bool parseUBZAudio(const QDomElement &element);
        bool parseForeignObject(const QDomElement &element);
        bool parseUBZText(const QDomElement &element);

        bool parseUBZPolygon(const QDomElement &element);
        bool parseUBZPolyline(const QDomElement &element);
        bool parseUBZLine(const QDomElement &element);


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
        QString errorStr; // last error string message

    public:
        operator bool() const {return isValid();}
    };

    class UBToUBZConverter {
    public:
        UBToUBZConverter();
    };


};

#endif // UBCFFADAPTOR_H
