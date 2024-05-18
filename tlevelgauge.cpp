//QT
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <QXmlStreamWriter>
#include <QDateTime>

//My
#include "Common/common.h"
#include "tconfig.h"

#include "tlevelgauge.h"

using namespace CGILevelGauge;
using namespace Common;

static const float FLOAT_EPSILON = 0.0000001;
static const QString CURRENT_PROTOCOL_VERSION = "0.1";

CGILevelGauge::TLevelGauge::TLevelGauge()
{
}

TLevelGauge::~TLevelGauge()
{
}

void TLevelGauge::connectToDB()
{
    const auto cnf = TConfig::config();
    Q_CHECK_PTR(cnf);

    //настраиваем подключениек БД
    if (!Common::connectToDB(_db, cnf->db_ConnectionInfo(), "MainDB"))
    {
        QString msg = connectDBErrorString(_db);
        qCritical() << QString("%1 %2").arg(QTime::currentTime().toString(SIMPLY_TIME_FORMAT)).arg(msg);
        Common::writeLogFile("ERR>", msg);

        exit(EXIT_CODE::SQL_NOT_CONNECT);
    }
}


int TLevelGauge::run(const QString& XMLText)
{
    writeDebugLogFile("REQUEST>", XMLText);

    QTextStream errStream(stderr);

    //парсим XML
    QXmlStreamReader XMLReader(XMLText);
    QString AZSCode;;
    QString clientVersion;
    QString protocolVersion;
    TanksMeasuments tanksMeasuments;

    try
    {
        while ((!XMLReader.atEnd()) && (!XMLReader.hasError()))
        {
            QXmlStreamReader::TokenType token = XMLReader.readNext();
            if (token == QXmlStreamReader::StartDocument)
            {
                continue;
            }
            else if (token == QXmlStreamReader::EndDocument)
            {
                break;
            }
            else if (token == QXmlStreamReader::StartElement)
            {
                //qDebug() << XMLReader.name().toString();
                if (XMLReader.name().toString()  == "Root")
                {
                    while ((XMLReader.readNext() != QXmlStreamReader::EndElement) && !XMLReader.atEnd() && !XMLReader.hasError())
                    {
                        if (XMLReader.name().toString().isEmpty())
                        {
                            continue;
                        }
                        if (XMLReader.name().toString()  == "AZSCode")
                        {
                            AZSCode = XMLReader.readElementText();
                        }
                        else if (XMLReader.name().toString()  == "ClientVersion")
                        {
                            clientVersion = XMLReader.readElementText();
                        }
                        else if (XMLReader.name().toString()  == "ProtocolVersion")
                        {
                            protocolVersion = XMLReader.readElementText();

                        }
                        else if (XMLReader.name().toString()  == "LevelGaugeMeasument")
                        {
                            tanksMeasuments.emplaceBack(parseMeasuments(XMLReader));
                        }
                        else
                        {
                            throw std::runtime_error(QString("Undefine tag in XML (Root/%1)")
                                            .arg(XMLReader.name().toString()).toStdString());
                        }
                    }
                }
                else
                {
                    throw std::runtime_error(QString("Undefine tag in XML (%1)")
                                    .arg(XMLReader.name().toString()).toStdString());
                }
            }
        }

        if (XMLReader.hasError())
        { //неудалось распарсить пришедшую XML
            throw std::runtime_error(QString("Cannot parse XML query. Message: %1")
                                        .arg(XMLReader.errorString()).toStdString());
        }
        if (AZSCode.isEmpty())
        {
            throw std::runtime_error(QString("Value tag Root/AZSCode cannot be empty").toStdString());
        }
        if (protocolVersion.isEmpty() || protocolVersion != CURRENT_PROTOCOL_VERSION)
        {
            throw std::runtime_error(QString("Value tag Root/ProtocolVersion cannot be empty or protocol version is not support. Value: %1")
                                    .arg(protocolVersion).toStdString());
        }
    }
    catch(std::runtime_error &err)
    {
        _errorString = QString("Error parse XML: %1").arg(err.what());
        errStream << _errorString;

        return EXIT_CODE::XML_PARSE_ERR;
    }

    //Обрабатываем результаты
    connectToDB();
    Q_ASSERT(_db.isOpen());
    _db.transaction();

    saveMeasumentsToDB(tanksMeasuments, AZSCode);

    DBCommit(_db);

    //отправляем ответ
    QTextStream answerTextStream(stdout);
    answerTextStream << "OK";

    writeDebugLogFile("ANSWER>", "OK");

    return EXIT_CODE::OK;
}

TLevelGauge::TankMeasument TLevelGauge::parseMeasuments(QXmlStreamReader &XMLReader)
{
    TankMeasument tankMeasument;

    while ((XMLReader.readNext() != QXmlStreamReader::EndElement) && !XMLReader.atEnd() && !XMLReader.hasError())
    {

        if (XMLReader.name().toString().isEmpty())
        {
            continue;
        }
        else if (XMLReader.name().toString()  == "DateTime")
        {
            const auto tmpStr = XMLReader.readElementText();
            tankMeasument.dateTime = QDateTime::fromString(tmpStr, DATETIME_FORMAT);
            const auto checkDate = QDateTime::currentDateTime().addYears(-1);
            if (tankMeasument.dateTime < checkDate)
            {
                throw std::runtime_error(QString("Incorrect value tag (Root/LevelGaugeMeasument/%1). Value: %2. Value must date/time less %3")
                       .arg(XMLReader.name().toString())
                       .arg(tmpStr)
                       .arg(checkDate.toString(DATETIME_FORMAT)).toStdString());
            }
        }
        else if (XMLReader.name().toString()  == "TankNumber")
        {
            bool ok = false;
            const auto tmpStr = XMLReader.readElementText();
            tankMeasument.tankNumber = tmpStr.toUInt(&ok);
            if (!ok || tankMeasument.tankNumber <= 0 || tankMeasument.tankNumber > 100)
            {
                throw std::runtime_error(QString("Incorrect value tag (Root/LevelGaugeMeasument/%1). Value: %2. Value must be unsigneg number from 1 to 100")
                       .arg(XMLReader.name().toString())
                       .arg(tmpStr).toStdString());
            }
        }
        else if (XMLReader.name().toString()  == "Volume")
        {
            bool ok = false;
            const auto tmpStr = XMLReader.readElementText();
            tankMeasument.volume = tmpStr.toFloat(&ok);
            if (!ok || tankMeasument.volume <= -FLOAT_EPSILON)
            {
                throw std::runtime_error(QString("Incorrect value tag (Root/LevelGaugeMeasument/%1). Value: %2. Value must be float number more or equil 0")
                       .arg(XMLReader.name().toString())
                       .arg(tmpStr).toStdString());
            }
        }
        else if (XMLReader.name().toString()  == "Mass")
        {
            bool ok = false;
            const auto tmpStr = XMLReader.readElementText();
            tankMeasument.mass = tmpStr.toFloat(&ok);
            if (!ok || tankMeasument.mass <= -FLOAT_EPSILON)
            {
                throw std::runtime_error(QString("Incorrect value tag (Root/LevelGaugeMeasument/%1). Value: %2. Value must be float number more or equil 0")
                       .arg(XMLReader.name().toString())
                       .arg(tmpStr).toStdString());
            }
        }
        else if (XMLReader.name().toString()  == "Density")
        {
            bool ok = false;
            const auto tmpStr = XMLReader.readElementText();
            tankMeasument.density = tmpStr.toFloat(&ok);
            if (!ok || tankMeasument.density <= 400.0 || tankMeasument.density >= 1200.0)
            {
                throw std::runtime_error(QString("Incorrect value tag (Root/LevelGaugeMeasument/%1). Value: %2. Value must be float number from 400 to 1200")
                       .arg(XMLReader.name().toString())
                       .arg(tmpStr).toStdString());
            }
        }
        else if (XMLReader.name().toString()  == "Height")
        {
            bool ok = false;
            const auto tmpStr = XMLReader.readElementText();
            tankMeasument.height = tmpStr.toFloat(&ok);
            if (!ok || tankMeasument.height <= -FLOAT_EPSILON)
            {
                throw std::runtime_error(QString("Incorrect value tag (Root/LevelGaugeMeasument/%1). Value: %2. Value must be float number more or equil 0")
                       .arg(XMLReader.name().toString())
                       .arg(tmpStr).toStdString());
            }
        }
        else if (XMLReader.name().toString()  == "Water")
        {
            bool ok = false;
            const auto tmpStr = XMLReader.readElementText();
            tankMeasument.water = tmpStr.toFloat(&ok);
            if (!ok || tankMeasument.water <= -FLOAT_EPSILON)
            {
                throw std::runtime_error(QString("Incorrect value tag (Root/LevelGaugeMeasument/%1). Value: %2. Value must be float number more or equil 0")
                       .arg(XMLReader.name().toString())
                       .arg(tmpStr).toStdString());
            }
        }
        else if (XMLReader.name().toString()  == "Temp")
        {
            bool ok = false;
            const auto tmpStr = XMLReader.readElementText();
            tankMeasument.temp = tmpStr.toFloat(&ok);
            if (!ok || tankMeasument.temp <= -60.0 || tankMeasument.temp >= 100.0)
            {
                throw std::runtime_error(QString("Incorrect value tag (Root/LevelGaugeMeasument/%1). Value: %2. Value must be float number from -60 to +100")
                       .arg(XMLReader.name().toString())
                       .arg(tmpStr).toStdString());
            }
        }
        else
        {
            throw std::runtime_error(QString("Undefine tag in XML (Root/LevelGaugeMeasument/%1)")
                                        .arg(XMLReader.name().toString()).toStdString());
        }
    }

    return tankMeasument;
}


void TLevelGauge::saveMeasumentsToDB(const TLevelGauge::TanksMeasuments& tanksMeasuments, const QString& AZSCode)
{
    for (const auto& tankMeasument: tanksMeasuments)
    {
        QSqlQuery query(_db);

        const auto queryText =
            QString("INSERT INTO [TanksMeasument] (DateTime, AZSCode, TankNumber, Volume, Mass, Density, Height, Water, Temp) "
                    "VALUES (CAST('%1' AS DATETIME2), '%2', %3, %4, %5, %6, %7, %8, %9)")
                        .arg(tankMeasument.dateTime.toString(DATETIME_FORMAT))
                        .arg(AZSCode)
                        .arg(tankMeasument.tankNumber)
                        .arg(tankMeasument.volume)
                        .arg(tankMeasument.mass)
                        .arg(tankMeasument.density)
                        .arg(tankMeasument.height)
                        .arg(tankMeasument.water)
                        .arg(tankMeasument.temp);

        if (!query.exec(queryText))
        {
            errorDBQuery(_db, query);
        }
    }
}


