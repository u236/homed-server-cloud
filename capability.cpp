#include <math.h>
#include "capability.h"

PropertyObject::PropertyObject(const QString &type, const QString &instance, const QString &unit, double divider) : m_type(type), m_instance(instance), m_divider(divider), m_updated(false)
{
    if (!unit.isEmpty())
        m_parameters.insert("unit", unit);

    m_parameters.insert("instance", m_instance);
}

QJsonObject PropertyObject::state(void)
{
    return {{"instance", m_instance}, {"value", m_type == "devices.properties.event" ? m_events.value(m_value.toString()).toString() : m_divider ? m_value.toDouble() / m_divider : QJsonValue::fromVariant(m_value)}};
}

void PropertyObject::addEvents(void)
{
    QList <QVariant> events;

    for (auto it = m_events.begin(); it != m_events.end(); it++)
        events.append(QMap <QString, QVariant> {{"value", it.value()}});

    m_parameters.insert("events", events);
}

Capabilities::Switch::Switch(void) : CapabilityObject("devices.capabilities.on_off", "on")
{
    m_data.insert("status", QVariant());
}

QJsonObject Capabilities::Switch::state(void)
{
    return {{"instance", "on"}, {"value", m_data.value("status").toString() == "on"}};
}

QJsonObject Capabilities::Switch::action(const QJsonObject &json)
{
    return {{"status", json.value("value").toBool() ? "on" : "off"}};
}

Capabilities::Brightness::Brightness(void) : CapabilityObject("devices.capabilities.range", "brightness")
{
    m_parameters.insert("instance", "brightness");
    m_parameters.insert("range", QMap <QString, QVariant> {{"min", 1}, {"max", 100}});
    m_parameters.insert("unit", "unit.percent");

    m_data.insert("level", QVariant());
}

QJsonObject Capabilities::Brightness::state(void)
{
    return QJsonObject {{"instance", "brightness"}, {"value", round(m_data.value("level").toInt() / 2.55)}};
}

QJsonObject Capabilities::Brightness::action(const QJsonObject &json)
{
    double value = json.value("value").toDouble() * 2.55;

    if (json.value("relative").toBool())
        value += m_data.value("level").toDouble();

    return {{"level", round(value < 2.55 ? 2.55 : value > 255 ? 255 : value)}};
}

Capabilities::Color::Color(const QMap <QString, QVariant> &options) : CapabilityObject("devices.capabilities.color_setting", {"rgb", "temperature_k"}), m_colorMode(false)
{
    QList <QVariant> list = options.value("light").toList();

    m_colors =
    {
        {16714250, 16711680}, // Red*
        {16729907, 16729907}, // Coral
        {16727040, 16727040}, // Orange
        {16740362, 16740362}, // Yellow
        {13303562, 13303562}, // Lime
        {720711,   65280},    // Green*
        {720813,   720813},   // Emerald
        {720883,   720883},   // Turquoise
        {710399,   65535},    // Cyan*
        {673791,   255},      // Blue*
        {15067647, 15067647}, // Moonlight
        {8719103,  8719103},  // Lavender
        {11340543, 11340543}, // Violet
        {16714471, 16714471}, // Purple
        {16714393, 16714393}, // Orchid
        {16722742, 16722742}, // Mauve
        {16711765, 16711765}  // Raspberry
    };

    if (list.contains("color"))
    {
        m_parameters.insert("color_model", "rgb");
        m_data.insert("color", QVariant());
    }

    if (list.contains("colorTemperature"))
    {
        QMap <QString, QVariant> option = options.value("colorTemperature").toMap(), range;
        QList <quint16> list = {1500, 2700, 3400, 4500, 5600, 6500, 7500, 9000};
        double min = option.contains("max") ? round(1e6 / option.value("max").toDouble()) : 1500, max = option.contains("min") ? round(1e6 / option.value("min").toDouble()) : 9000;

        for (int i = 0; i < list.count() - 1; i++)
        {
            if (list.at(i) <= min && list.at(i + 1) > min)
                range.insert("min", list.at(i));

            if (list.at(i) < max && list.at(i + 1) >= max)
                range.insert("max", list.at(i + 1));
        }

        m_parameters.insert("temperature_k", range);
        m_data.insert("colorTemperature", QVariant());
    }

    if (list.contains("colorMode"))
        m_data.insert("colorMode", QVariant());
}

QJsonObject Capabilities::Color::state(void)
{
    m_colorMode = m_data.value("colorMode", m_colorMode).toBool();

    if (m_colorMode)
    {
        QList <QVariant> list = m_data.value("color").toList();
        int value = list.value(0).toInt() << 16 | list.value(1).toInt() << 8 | list.value(2).toInt();

        for (auto it = m_colors.begin(); it != m_colors.end(); it++)
        {
            if (distance(parse(it.value()), parse(value)) < 20)
            {
                value = it.key();
                break;
            }
        }

        return QJsonObject {{"instance", "rgb"}, {"value", value}};
    }
    else
    {
        double value = m_data.value("colorTemperature").toDouble();
        return QJsonObject {{"instance", "temperature_k"}, {"value", value ? round(1e6 / value) : 5600}};
    }
}

QJsonObject Capabilities::Color::action(const QJsonObject &json)
{
    m_colorMode = json.value("instance").toString() == "rgb";

    if (m_colorMode)
    {
        int value = json.value("value").toInt();
        RGB rgb;

        if (m_colors.contains(value))
            value = m_colors.value(value);

        rgb = parse(value);
        return {{"color", QJsonArray {rgb.r, rgb.g, rgb.b}}};
    }
    else
    {
        double value = 1e6 / json.value("value").toDouble();
        return {{"colorTemperature", round(json.value("relative").toBool() ? m_data.value("colorTemperature").toDouble() + value : value)}};
    }
}

Capabilities::Color::RGB Capabilities::Color::parse(int value)
{
    return {value >> 16 & 0xFF, value >> 8 & 0xFF, value & 0xFF};
}

int Capabilities::Color::distance(RGB a, RGB b)
{
    return abs(sqrt(pow(a.r - b.r, 2) + pow(a.g - b.g, 2) + pow(a.b - b.b, 2)));
}

Capabilities::Curtain::Curtain(void) : CapabilityObject("devices.capabilities.on_off", "on")
{
    m_data.insert("cover", QVariant());
}

QJsonObject Capabilities::Curtain::state(void)
{
    return {{"instance", "on"}, {"value", m_data.value("cover").toString() == "open"}};
}

QJsonObject Capabilities::Curtain::action(const QJsonObject &json)
{
    return {{"cover", json.value("value").toBool() ? "open" : "close"}};
}

Capabilities::Open::Open(void) : CapabilityObject("devices.capabilities.range", "open")
{
    m_parameters.insert("instance", "open");
    m_parameters.insert("range", QMap <QString, QVariant> {{"min", 0}, {"max", 100}});
    m_parameters.insert("unit", "unit.percent");

    m_data.insert("position", QVariant());
}

QJsonObject Capabilities::Open::state(void)
{
    return QJsonObject {{"instance", "open"}, {"value", m_data.value("position").toInt()}};
}

QJsonObject Capabilities::Open::action(const QJsonObject &json)
{
    int value = json.value("value").toInt();

    if (json.value("relative").toBool())
        value += m_data.value("position").toDouble();

    return {{"position", value < 0 ? 0 : value > 100 ? 100 : value}};
}

Capabilities::ThermostatPower::ThermostatPower(const QVariant &onValue) : CapabilityObject("devices.capabilities.on_off", "on"), m_onValue(onValue)
{
    m_data.insert("systemMode", QVariant());
}

QJsonObject Capabilities::ThermostatPower::state(void)
{
    return {{"instance", "on"}, {"value", m_data.value("systemMode").toString() != "off"}};
}

QJsonObject Capabilities::ThermostatPower::action(const QJsonObject &json)
{
    return {{"systemMode", json.value("value").toBool() ? QJsonValue::fromVariant(m_onValue) : "off"}};
}

Capabilities::ThermostatMode::ThermostatMode(const QList <QVariant> &list, ThermostatPower *power) : CapabilityObject("devices.capabilities.mode", "thermostat"), m_power(power), m_value(list.value(0))
{
    QList <QVariant> check = {"auto", "cool", "heat", "dry", "fan"}, modes;

    for (int i = 0; i < list.count(); i++)
    {
        QVariant value = list.at(i);

        if (!check.contains(value))
            continue;

        modes.append(QMap <QString, QVariant> {{"value", value != "fan" ? value : "fan_only"}});
    }

    m_parameters.insert("instance", "thermostat");
    m_parameters.insert("modes", modes);

    m_data.insert("systemMode", m_value);
}

QJsonObject Capabilities::ThermostatMode::state(void)
{
    QString value = m_data.value("systemMode").toString();

    if (value != "off")
        m_value = value;

    if (m_power)
        m_power->setOnValue(m_value);

    return {{"instance", "thermostat"}, {"value", m_value != "fan" ? QJsonValue::fromVariant(m_value) : "fan_only"}};
}

QJsonObject Capabilities::ThermostatMode::action(const QJsonObject &json)
{
    QString value = json.value("value").toString();
    return {{"systemMode", value != "fan_only" ? value : "fan"}};
}

Capabilities::Temperature::Temperature(const QMap <QString, QVariant> &options) : CapabilityObject("devices.capabilities.range", "temperature")
{
    QMap <QString, QVariant> option = options.value("targetTemperature").toMap();

    m_parameters.insert("instance", "temperature");
    m_parameters.insert("range", QMap <QString, QVariant> {{"min", option.value("min").toDouble()}, {"max", option.value("max").toDouble()}, {"precision", option.value("step", 1).toDouble()}});
    m_parameters.insert("unit", "unit.temperature.celsius");

    m_data.insert("targetTemperature", QVariant());
}

QJsonObject Capabilities::Temperature::state(void)
{
    return QJsonObject {{"instance", "temperature"}, {"value", m_data.value("targetTemperature").toDouble()}};
}

QJsonObject Capabilities::Temperature::action(const QJsonObject &json)
{
    double value = json.value("value").toDouble();
    return {{"targetTemperature", json.value("relative").toBool() ? m_data.value("targetTemperature").toDouble() + value : value}};
}

Capabilities::FanMode::FanMode(const QList <QVariant> &list) : CapabilityObject("devices.capabilities.mode", "fan_speed")
{
    QList <QVariant> check = {"min", "low", "medium", "high", "max", "auto"}, modes;

    for (int i = 0; i < list.count(); i++)
    {
        QVariant value = list.at(i);

        if (!check.contains(value))
            continue;

        modes.append(QMap <QString, QVariant> {{"value", value}});
    }

    m_parameters.insert("instance", "fan_speed");
    m_parameters.insert("modes", modes);

    m_data.insert("fanMode", QVariant());
}

QJsonObject Capabilities::FanMode::state(void)
{
    return QJsonObject {{"instance", "fan_speed"}, {"value", m_data.value("fanMode").toString()}};
}

QJsonObject Capabilities::FanMode::action(const QJsonObject &json)
{
    return {{"fanMode", json.value("value").toString()}};
}

Capabilities::HeatMode::HeatMode(const QList <QVariant> &list) : CapabilityObject("devices.capabilities.mode", "heat")
{
    QList <QVariant> check = {"min", "normal", "turbo", "max", "auto"}, modes;

    for (int i = 0; i < list.count(); i++)
    {
        QVariant value = list.at(i);

        if (!check.contains(value))
            continue;

        modes.append(QMap <QString, QVariant> {{"value", value}});
    }

    m_parameters.insert("instance", "heat");
    m_parameters.insert("modes", modes);

    m_data.insert("heatMode", QVariant());
}

QJsonObject Capabilities::HeatMode::state(void)
{
    return QJsonObject {{"instance", "heat"}, {"value", m_data.value("heatMode").toString()}};
}

QJsonObject Capabilities::HeatMode::action(const QJsonObject &json)
{
    return {{"heatMode", json.value("value").toString()}};
}

Capabilities::SwingMode::SwingMode(const QList <QVariant> &list) : CapabilityObject("devices.capabilities.mode", "swing")
{
    QList <QVariant> check = {"stationary", "horizontal", "vertical"}, modes;

    for (int i = 0; i < list.count(); i++)
    {
        QVariant value = list.at(i);

        if (!check.contains(value))
            continue;

        modes.append(QMap <QString, QVariant> {{"value", value}});
    }

    m_parameters.insert("instance", "swing");
    m_parameters.insert("modes", modes);

    m_data.insert("swingMode", QVariant());
}

QJsonObject Capabilities::SwingMode::state(void)
{
    return QJsonObject {{"instance", "swing"}, {"value", m_data.value("swingMode").toString()}};
}

QJsonObject Capabilities::SwingMode::action(const QJsonObject &json)
{
    return {{"swingMode", json.value("value").toString()}};
}

Properties::Button::Button(const QList <QVariant> &actions) : PropertyObject("devices.properties.event", "button")
{
    if (actions.contains("singleClick"))
        m_events.insert("singleClick", "click");

    if (actions.contains("doubleClick"))
        m_events.insert("doubleClick", "double_click");

    if (actions.contains("hold"))
        m_events.insert("hold", "long_press");

    addEvents();
}

Properties::Binary::Binary(const QString &instance, const QString &on, const QString &off) : PropertyObject("devices.properties.event", instance)
{
    m_events = {{"true", on}, {"false", off}};
    addEvents();
}

Properties::Vibration::Vibration(void) : PropertyObject("devices.properties.event", "vibration")
{
    m_events = {{"vibration", "vibration"}, {"tilt", "tilt"}, {"drop", "fall"}};
    addEvents();
}
