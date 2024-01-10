#include <math.h>
#include "capability.h"

PropertyObject::PropertyObject(const QString &type, const QString &instance, const QString &unit, double divider) : m_type(type), m_instance(instance), m_divider(divider)
{
    if (!unit.isEmpty())
        m_parameters.insert("unit", unit);

    m_parameters.insert("instance", m_instance);
}

QJsonObject PropertyObject::state()
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

Capabilities::Switch::Switch(void) : CapabilityObject("devices.capabilities.on_off")
{
    m_data.insert("status", QVariant());
}

QJsonObject Capabilities::Switch::state(void)
{
    return {{"instance", "on"}, {"value", m_data.value("status").toString() == "on" ? true : false}};
}

QJsonObject Capabilities::Switch::action(const QJsonObject &json)
{
    return {{"status", json.value("value").toBool() ? "on" : "off"}};
}

Capabilities::Brightness::Brightness(void) : CapabilityObject("devices.capabilities.range")
{
    m_parameters.insert("instance", "brightness");
    m_parameters.insert("range", QMap <QString, QVariant> {{"min", 0}, {"max", 100}});
    m_parameters.insert("unit", "unit.percent");

    m_data.insert("level", QVariant());
}

QJsonObject Capabilities::Brightness::state(void)
{
    return QJsonObject {{"instance", "brightness"}, {"value", round(m_data.value("level").toDouble() / 2.55)}};
}

QJsonObject Capabilities::Brightness::action(const QJsonObject &json)
{
    double value = json.value("value").toDouble() * 2.55;
    return {{"level", round(json.value("relative").toBool() ? m_data.value("level").toDouble() + value : value)}};
}

Capabilities::Color::Color(const QMap <QString, QVariant> &options) : CapabilityObject("devices.capabilities.color_setting"), m_rgb(false)
{
    QList <QVariant> list = options.value("light").toList();

    if (list.contains("color"))
    {
        m_parameters.insert("color_model", "rgb");
        m_data.insert("color", QVariant());
        m_rgb = true;
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

    if (!m_parameters.isEmpty())
        return;

    m_parameters.insert("temperature_k", QMap <QString, QVariant> {{"min", 5600}, {"max", 5600}});
}

QJsonObject Capabilities::Color::state(void)
{
    if (m_rgb)
    {
        QList <QVariant> list = m_data.value("color").toList();
        return QJsonObject {{"instance", "rgb"}, {"value", list.value(0).toInt() << 16 | list.value(1).toInt() << 8 | list.value(2).toInt()}};
    }
    else
    {
        double value = m_data.value("colorTemperature").toDouble();
        return QJsonObject {{"instance", "temperature_k"}, {"value", value ? round(1e6 / value) : 5600}};
    }
}

QJsonObject Capabilities::Color::action(const QJsonObject &json)
{
    m_rgb = json.value("instance").toString() == "rgb" ? true : false;

    if (m_rgb)
    {
        int value = json.value("value").toInt();
        return {{"color", QJsonArray {value >> 16 & 0xFF, value >> 8 & 0xFF, value & 0xFF}}};
    }
    else
    {
        double value = 1e6 / json.value("value").toDouble();
        return {{"colorTemperature", round(json.value("relative").toBool() ? m_data.value("colorTemperature").toDouble() + value : value)}};
    }
}

Capabilities::Curtain::Curtain(void) : CapabilityObject("devices.capabilities.on_off")
{
    m_data.insert("cover", QVariant());
}

QJsonObject Capabilities::Curtain::state(void)
{
    return {{"instance", "on"}, {"value", m_data.value("cover").toString() == "open" ? true : false}};
}

QJsonObject Capabilities::Curtain::action(const QJsonObject &json)
{
    return {{"cover", json.value("value").toBool() ? "open" : "close"}};
}

Capabilities::Open::Open(void) : CapabilityObject("devices.capabilities.range")
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
    return {{"position", json.value("relative").toBool() ? m_data.value("position").toDouble() + value : value}};
}

Capabilities::Thermostat::Thermostat(void) : CapabilityObject("devices.capabilities.on_off")
{
    m_data.insert("systemMode", QVariant());
}

QJsonObject Capabilities::Thermostat::state(void)
{
    return {{"instance", "on"}, {"value", m_data.value("systemMode").toString() != "off" ? true : false}};
}

QJsonObject Capabilities::Thermostat::action(const QJsonObject &json)
{
    return {{"systemMode", json.value("value").toBool() ? "heat" : "off"}};
}

Capabilities::Temperature::Temperature(const QMap <QString, QVariant> &options) : CapabilityObject("devices.capabilities.range")
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
    return {{"targetTemperature", round(json.value("relative").toBool() ? m_data.value("targetTemperature").toDouble() + value : value)}};
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
