#include <QtCore/QCoreApplication>
#include <QtCore>
#include "UBCFFAdaptor.h"


int main(int argc, char *argv[])
{
    Q_UNUSED(argc)
    Q_UNUSED(argv)
    QCoreApplication a(argc, argv);

    UBCFFAdaptor testAdaptor;
    testAdaptor.convertUBZToIWB("../resources/suse.ubz", "../resources/newDir/toDir.iwb");

    qDebug() << "closing an application";
    
    return a.exec();
}
