#ifndef CAPABILITY_H
#define CAPABILITY_H

#include <QJsonArray>
#include <QJsonObject>
#include <QSharedPointer>
#include <QVariant>

class CapabilityObject;
typedef QSharedPointer <CapabilityObject> Capability;

class PropertyObject;
typedef QSharedPointer <PropertyObject> Property;

class CapabilityObject
{

public:

    CapabilityObject(const QString &type) : m_type(type), m_updated(false) {}
    virtual ~CapabilityObject(void) {}

    inline QString type(void) { return m_type; }
    inline QMap <QString, QVariant> &parameters(void) { return m_parameters; }
    inline QMap <QString, QVariant> &data(void) { return m_data; }

    inline bool updated(void) { return m_updated; }
    inline void setUpdated(bool value) { m_updated = value; }

    virtual QJsonObject state(void) = 0;
    virtual QJsonObject action(const QJsonObject &) = 0;

protected:

    QString m_type;
    QMap <QString, QVariant> m_parameters, m_data;
    bool m_updated;

};

class PropertyObject
{

public:

    PropertyObject(const QString &type, const QString &instance, const QString &unit = QString(), double divider = 0);
    virtual ~PropertyObject(void) {}

    inline QString type(void) { return m_type; }
    inline QString instance(void) { return m_instance; }

    inline QMap <QString, QVariant> &parameters(void) { return m_parameters; }
    inline QMap <QString, QVariant> &events(void) { return m_events; }

    inline QVariant value(void) {return m_value; }
    inline void setValue(const QVariant &value) { m_value = value; }

    inline bool updated(void) { return m_updated; }
    inline void setUpdated(bool value) { m_updated = value; }

    QJsonObject state(void);

protected:

    QString m_type, m_instance;
    double m_divider;

    QMap <QString, QVariant> m_parameters, m_events;
    QVariant m_value;
    bool m_updated;

    void addEvents(void);

};

namespace Capabilities
{
    class Switch : public CapabilityObject
    {

    public:

        Switch(void);
        QJsonObject state(void) override;
        QJsonObject action(const QJsonObject &json) override;

    };

    class Brightness : public CapabilityObject
    {

    public:

        Brightness(void);
        QJsonObject state(void) override;
        QJsonObject action(const QJsonObject &json) override;

    };

    class Color : public CapabilityObject
    {

    public:

        Color(const QMap <QString, QVariant> &options);
        QJsonObject state(void) override;
        QJsonObject action(const QJsonObject &json) override;

    private:

        struct RGB { int r, g, b; };

        QMap <int, int> m_colors;
        bool m_colorMode;

        RGB parse(int value);
        int distance(RGB a, RGB b);

    };

    class Curtain : public CapabilityObject
    {

    public:

        Curtain(void);
        QJsonObject state(void) override;
        QJsonObject action(const QJsonObject &json) override;

    };

    class Open : public CapabilityObject
    {

    public:

        Open(void);
        QJsonObject state(void) override;
        QJsonObject action(const QJsonObject &json) override;

    };

    class ThermostatPower : public CapabilityObject
    {

    public:

        ThermostatPower(const QVariant &onValue);
        QJsonObject state(void) override;
        QJsonObject action(const QJsonObject &json) override;

        inline void setOnValue(const QVariant &value) { m_onValue = value; }

    private:

        QVariant m_onValue;

    };

    class ThermostatMode : public CapabilityObject
    {

    public:

        ThermostatMode(const QList <QVariant> &list, ThermostatPower *power);
        QJsonObject state(void) override;
        QJsonObject action(const QJsonObject &json) override;

    private:

        ThermostatPower *m_power;
        QVariant m_value;

    };

    class Temperature : public CapabilityObject
    {

    public:

        Temperature(const QMap <QString, QVariant> &options);
        QJsonObject state(void) override;
        QJsonObject action(const QJsonObject &json) override;

    };

    class FanMode : public CapabilityObject
    {

    public:

        FanMode(const QList <QVariant> &list);
        QJsonObject state(void) override;
        QJsonObject action(const QJsonObject &json) override;

    };

    class SwingMode : public CapabilityObject
    {

    public:

        SwingMode(const QList <QVariant> &list);
        QJsonObject state(void) override;
        QJsonObject action(const QJsonObject &json) override;

    };
};

namespace Properties
{
    class Button : public PropertyObject
    {

    public:

        Button(const QList <QVariant> &actions);

    };

    class Binary : public PropertyObject
    {

    public:

        Binary(const QString &instance, const QString &on, const QString &off);

    };

    class Vibration : public PropertyObject
    {

    public:

        Vibration(void);

    };

    class Temperature : public PropertyObject
    {

    public:

        Temperature(void) : PropertyObject("devices.properties.float", "temperature", "unit.temperature.celsius") {}

    };

    class Pressure : public PropertyObject
    {

    public:

        Pressure(void) : PropertyObject("devices.properties.float", "pressure", "unit.pressure.mmhg", 0.1333) {}

    };

    class Humidity : public PropertyObject
    {

    public:

        Humidity(void) : PropertyObject("devices.properties.float", "humidity", "unit.percent") {}

    };

    class CO2 : public PropertyObject
    {

    public:

        CO2(void) : PropertyObject("devices.properties.float", "co2_level", "unit.ppm") {}

    };

    class PM1 : public PropertyObject
    {

    public:

        PM1(void) : PropertyObject("devices.properties.float", "pm1_density", "unit.density.mcg_m3") {}

    };

    class PM10 : public PropertyObject
    {

    public:

        PM10(void) : PropertyObject("devices.properties.float", "pm10_density", "unit.density.mcg_m3") {}

    };

    class PM25 : public PropertyObject
    {

    public:

        PM25(void) : PropertyObject("devices.properties.float", "pm2.5_density", "unit.density.mcg_m3") {}

    };

    class VOC : public PropertyObject
    {

    public:

        VOC(void) : PropertyObject("devices.properties.float", "tvoc", "unit.density.mcg_m3") {}

    };

    class Illuminance : public PropertyObject
    {

    public:

        Illuminance(void) : PropertyObject("devices.properties.float", "illumination", "unit.illumination.lux") {}

    };

    class Volume : public PropertyObject
    {

    public:

        Volume(void) : PropertyObject("devices.properties.float", "water_meter", "unit.cubic_meter", 1000) {}

    };

    class Energy : public PropertyObject
    {

    public:

        Energy(void) : PropertyObject("devices.properties.float", "electricity_meter", "unit.kilowatt_hour") {}

    };

    class Voltage : public PropertyObject
    {

    public:

        Voltage(void) : PropertyObject("devices.properties.float", "voltage", "unit.volt") {}

    };

    class Current : public PropertyObject
    {

    public:

        Current(void) : PropertyObject("devices.properties.float", "amperage", "unit.ampere") {}

    };

    class Power : public PropertyObject
    {

    public:

        Power(void) : PropertyObject("devices.properties.float", "power", "unit.watt") {}

    };

    class Battery : public PropertyObject
    {

    public:

        Battery(void) : PropertyObject("devices.properties.float", "battery_level", "unit.percent") {}

    };
};

#endif
