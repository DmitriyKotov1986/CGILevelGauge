#pragma once

//QT
#include <QObject>
#include <QtSql/QSqlDatabase>
#include <QXmlStreamReader>
#include <QString>
#include <QDateTime>

namespace CGILevelGauge
{

class TLevelGauge
{
public:
    TLevelGauge();
    ~TLevelGauge();

    int run(const QString& XMLText);
    QString errorString() const { return _errorString; }

private:
    struct TankMeasument {
        uint tankNumber = 0;
        QDateTime dateTime; //время получения данных
        float volume = -1.0; //текущий объем
        float mass = -1.0; //текущая масса
        float density = -1.0; //теккущая плотность
        float height = -1.0; //текущий уровеньb
        float water = -1.0; //текущий уровень воды
        float temp = -273.0; //текущая температура
    };

    using TanksMeasuments = QList<TankMeasument>;

private:
    void connectToDB();

    TankMeasument parseMeasuments(QXmlStreamReader& XMLReader);
    void saveMeasumentsToDB(const TanksMeasuments& tanksMeasuments, const QString& AZSCode);

private:
    QSqlDatabase _db;

    QString _errorString;

};

}

