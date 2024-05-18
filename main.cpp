//QT
#include <QCoreApplication>
#include <QSettings>

//My
#include "Common/common.h"
#include "tlevelgauge.h"
#include "tconfig.h"

using namespace CGILevelGauge;
using namespace Common;

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    qInstallMessageHandler(messageOutput);

    QCoreApplication::setApplicationName("CGILevelGauges");
    QCoreApplication::setOrganizationName("OOO SA");
    QCoreApplication::setApplicationVersion(QString("Version:0.2a Build: %1 %2").arg(__DATE__).arg(__TIME__));

    setlocale(LC_CTYPE, ""); //настраиваем локаль

    QString configFileName = QString("%1/%2.ini").arg(QCoreApplication::applicationDirPath()).arg(QCoreApplication::applicationName());

    TConfig* cfg = TConfig::config(configFileName);

    if (cfg->isError())
    {
        QString errorMsg = "Error load configuration: " + cfg->errorString();
        qCritical() << errorMsg;
        writeLogFile("Error load configuration", errorMsg);

        exit(EXIT_CODE::LOAD_CONFIG_ERR);
    }

    QString buf;
    QTextStream inputStream(stdin);
    while (1)
    {
        QString tmpStr = inputStream.readLine();
        if (tmpStr != "EOF")
        {
            buf += tmpStr + "\n";
        }
        else
        {
            break;
        }
    }

    TLevelGauge levelGauge;

    int res = levelGauge.run(buf); //обрабатываем пришедшие данные

    if (res != 0 )
    {
        writeLogFile("Error parse XML: " + levelGauge.errorString(), buf);
    }
    else
    {
        qDebug() << "OK";
    }

    TConfig::deleteConfig();

    return res;
}
