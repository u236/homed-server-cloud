#include <QtEndian>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include "client.h"

Client::Client(QTcpSocket *socket) : QObject(nullptr), m_socket(socket), m_timer(new QTimer(this)), m_aes(new AES128), m_status(Status::Handshake)
{
    connect(m_socket, &QTcpSocket::readyRead, this, &Client::readyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &Client::disconnected);
    connect(m_timer, &QTimer::timeout, this, &Client::timeout);

    m_timer->setSingleShot(true);
    m_timer->start(AUTHORIZATION_TIMEOUT);
}

Client::~Client(void)
{
    delete m_aes;
}

void Client::publish(const Endpoint &endpoint, const QJsonObject &json)
{
    QString topic = QString("td/").append(endpoint->device()->id());

    if (endpoint->id())
        topic.append(QString("/%1").arg(endpoint->id()));

    sendRequest("publish", topic, json);
}

void Client::parseExposes(const Endpoint &endpoint, const QList <QVariant> &exposes, const QMap <QString, QVariant> &options)
{
    // basic

    if (exposes.contains("switch"))
    {
        endpoint->setType(options.value("switch").toString() == "outlet" ? "devices.types.socket" : "devices.types.switch");
        endpoint->capabilities().append(Capability(new Capabilities::Switch));
    }

    if (exposes.contains("light"))
    {
        endpoint->setType("devices.types.light");
        endpoint->capabilities().append(Capability(new Capabilities::Switch));

        if (options.value("light").toList().contains("level"))
            endpoint->capabilities().append(Capability(new Capabilities::Brightness));

        endpoint->capabilities().append(Capability(new Capabilities::Color(options)));
    }

    if (exposes.contains("cover"))
    {
        endpoint->setType("devices.types.openable.curtain");
        endpoint->capabilities().append(Capability(new Capabilities::Curtain));
        endpoint->capabilities().append(Capability(new Capabilities::Open));
    }

    if (exposes.contains("thermostat"))
    {
        QList <QVariant> list = options.value("systemMode").toMap().value("enum").toList();

        endpoint->setType("devices.types.thermostat");

        if (list.contains("off") && list.contains("heat"))
            endpoint->capabilities().append(Capability(new Capabilities::Thermostat));

        endpoint->capabilities().append(Capability(new Capabilities::Temperature(options)));
        endpoint->properties().insert("temperature", Property(new Properties::Temperature));
    }

    // event

    if (exposes.contains("action"))
    {
        QList <QVariant> list = options.value("action").toMap().value("trigger").toList();

        if (list.contains("singleClick") || list.contains("doubleClick") || list.contains("hold"))
        {
            endpoint->setType("devices.types.sensor.button");
            endpoint->properties().insert("action", Property(new Properties::Button(list)));
        }
    }

    if (exposes.contains("contact"))
    {
        endpoint->setType("devices.types.sensor.open");
        endpoint->properties().insert("contact", Property(new Properties::Binary("open", "opened", "closed")));
    }

    if (exposes.contains("gas"))
    {
        endpoint->setType("devices.types.sensor.gas");
        endpoint->properties().insert("gas", Property(new Properties::Binary("gas", "detected", "not_detected")));
    }

    if (exposes.contains("occupancy"))
    {
        endpoint->setType("devices.types.sensor.motion");
        endpoint->properties().insert("occupancy", Property(new Properties::Binary("motion", "detected", "not_detected")));
    }

    if (exposes.contains("smoke"))
    {
        endpoint->setType("devices.types.sensor.smoke");
        endpoint->properties().insert("smoke", Property(new Properties::Binary("smoke", "detected", "not_detected")));
    }

    if (exposes.contains("waterLeak"))
    {
        endpoint->setType("devices.types.sensor.water_leak");
        endpoint->properties().insert("waterLeak", Property(new Properties::Binary("water_leak", "leak", "dry")));
    }

    if (exposes.contains("vibration"))
    {
        endpoint->setType("devices.types.sensor.vibration");
        endpoint->properties().insert("event", Property(new Properties::Vibration));
    }

    // climate

    if (exposes.contains("temperature") && !options.value("temperature").toMap().value("diagnostic").toBool())
    {
        endpoint->setType("devices.types.sensor.climate");
        endpoint->properties().insert("temperature", Property(new Properties::Temperature));
    }

    if (exposes.contains("pressure"))
    {
        endpoint->setType("devices.types.sensor.climate");
        endpoint->properties().insert("pressure", Property(new Properties::Pressure));
    }

    if (exposes.contains("humidity"))
    {
        endpoint->setType("devices.types.sensor.climate");
        endpoint->properties().insert("humidity", Property(new Properties::Humidity));
    }

    if (exposes.contains("co2"))
    {
        endpoint->setType("devices.types.sensor.climate");
        endpoint->properties().insert("co2", Property(new Properties::CO2));
    }

    if (exposes.contains("pm1"))
    {
        endpoint->setType("devices.types.sensor.climate");
        endpoint->properties().insert("pm1", Property(new Properties::PM1));
    }

    if (exposes.contains("pm10"))
    {
        endpoint->setType("devices.types.sensor.climate");
        endpoint->properties().insert("pm10", Property(new Properties::PM10));
    }

    if (exposes.contains("pm25"))
    {
        endpoint->setType("devices.types.sensor.climate");
        endpoint->properties().insert("pm25", Property(new Properties::PM25));
    }

    if (exposes.contains("voc"))
    {
        endpoint->setType("devices.types.sensor.climate");
        endpoint->properties().insert("voc", Property(new Properties::VOC));
    }

    // illumination

    if (exposes.contains("illuminance"))
    {
        endpoint->setType("devices.types.sensor.illumination");
        endpoint->properties().insert("illuminance", Property(new Properties::Illuminance));
    }

    // electricity

    if (exposes.contains("energy"))
    {
        endpoint->setType("devices.types.smart_meter.electricity");
        endpoint->properties().insert("energy", Property(new Properties::Energy));
    }

    if (exposes.contains("voltage"))
    {
        endpoint->setType("devices.types.smart_meter.electricity");
        endpoint->properties().insert("voltage", Property(new Properties::Voltage));
    }

    if (exposes.contains("current"))
    {
        endpoint->setType("devices.types.smart_meter.electricity");
        endpoint->properties().insert("current", Property(new Properties::Current));
    }

    if (exposes.contains("power"))
    {
        endpoint->setType("devices.types.smart_meter.electricity");
        endpoint->properties().insert("power", Property(new Properties::Power));
    }

    // battery

    if (endpoint->type().isEmpty())
        return;

    if (exposes.contains("battery"))
        endpoint->properties().insert("battery", Property(new Properties::Battery));
    else if (exposes.contains("batteryLow"))
        endpoint->properties().insert("batteryLow", Property(new Properties::Binary("battery_level", "low", "normal")));
}

void Client::sendRequest(const QString &action, const QString &topic, const QJsonObject &message)
{
    QJsonObject json = {{"action", action}, {"topic", topic}};
    QByteArray buffer, packet = QByteArray(1, 0x42);

    if (action == "publish" && !message.isEmpty())
        json.insert("message", message);

    buffer = QJsonDocument(json).toJson(QJsonDocument::Compact);

    if (buffer.length() % 16)
        buffer.append(16 - buffer.length() % 16, 0);

    m_aes->cbcEncrypt(buffer);

    for (int i = 0; i < buffer.length(); i++)
    {
        switch (buffer.at(i))
        {
            case 0x42: packet.append(0x44).append(0x62); break;
            case 0x43: packet.append(0x44).append(0x63); break;
            case 0x44: packet.append(0x44).append(0x64); break;
            default:   packet.append(buffer.at(i)); break;
        }
    }

    m_socket->write(packet.append(0x43));
}

void Client::parseData(void)
{
    QJsonObject json;

    m_aes->cbcDecrypt(m_buffer);
    json = QJsonDocument::fromJson(m_buffer.constData()).object();

    if (m_status == Status::Authorization)
    {
        m_uniqueId = json.value("uniqueId").toString();
        emit tokenReceived(QByteArray::fromHex(json.value("token").toString().toUtf8()));

        if (!parent())
        {
            m_socket->close();
            return;
        }

        m_timer->stop();
        m_status = Status::Ready;

        // TODO: subscribe others here
        sendRequest("subscribe", "status/zigbee");
    }
    else
    {
        QJsonObject message = json.value("message").toObject();
        QString topic = json.value("topic").toString();

        if (topic == "status/zigbee")
        {
            QMap <QString, Device> map;
            QJsonArray devices = message.value("devices").toArray();
            bool names = message.value("names").toBool(), check = false;

            // TODO: refactor this
            for (auto it = devices.begin(); it != devices.end(); it++)
            {
                QJsonObject device = it->toObject();
                QString name = device.value("name").toString(), id = QString("zigbee/").append(names ? name : device.value("ieeeAddress").toString());

                if (name.isEmpty() || !device.value("logicalType").toInt())
                    continue;
                
                map.insert(id, Device(new DeviceObject(id, name)));
            }

            for (auto it = map.begin(); it != map.end(); it++)
            {
                if (!m_devices.contains(it.key()))
                {
                    QList <QString> subscriptions = {QString("device/").append(it.key()), QString("expose/").append(it.key())};

                    m_devices.insert(it.key(), it.value());

                    for (int i = 0; i < subscriptions.count(); i++)
                    {
                        sendRequest("subscribe", subscriptions.at(i));
                        m_subscriptions.append(subscriptions.at(i));
                    }

                    check = true;
                }
            }

            for (auto it = m_devices.begin(); it != m_devices.end(); it++)
            {
                if (!map.contains(it.key()))
                {
                    m_devices.erase(it++);
                    check = true;
                }

                if (it == m_devices.end())
                    break;
            }

            if (!check)
                return;

            emit devicesUpdated();
        }
        else if (topic.startsWith("device/zigbee/"))
        {
            QList list = topic.split('/');
            Device device = m_devices.value(QString("%1/%2").arg(list.value(1), list.value(2)));

            if (device.isNull())
                return;

            device->setAvailable(message.value("status").toString() == "online" ? true : false);
        }
        else if (topic.startsWith("expose/zigbee/"))
        {
            QList list = topic.split('/');
            Device device = m_devices.value(QString("%1/%2").arg(list.value(1), list.value(2)));

            if (device.isNull() || !device->endpoints().isEmpty())
                return;

            for (auto it = message.begin(); it != message.end(); it++)
            {
                Endpoint endpoint(new EndpointObject(static_cast <quint8> (it.key().toInt()), device));
                QJsonObject json = it.value().toObject();
                parseExposes(endpoint, json.value("items").toArray().toVariantList(), json.value("options").toObject().toVariantMap());
                device->endpoints().insert(endpoint->id(), endpoint);
            }

            for (auto it = device->endpoints().begin(); it != device->endpoints().end(); it++)
            {
                QString subscription = QString("fd/").append(device->id());

                if (it.value()->id())
                    subscription.append(QString("/%1").arg(it.value()->id()));

                sendRequest("subscribe", subscription);
                m_subscriptions.append(subscription);
            }

            sendRequest("publish", "command/zigbee", {{"action", "getProperties"}, {"device", device->name()}});
        }
        else if (topic.startsWith("fd/"))
        {
            QMap <QString, QVariant>  data = message.toVariantMap();
            QList <QString> list = topic.split('/');
            QList <Property> propertiesList;
            QList <Capability> capabilitiesList;
            Device device = m_devices.value(QString("%1/%2").arg(list.value(1), list.value(2)));
            Endpoint endpoint;

            if (device.isNull())
                return;

            endpoint = device->endpoints().value(static_cast <quint8> (list.value(3).toInt()));

            if (endpoint.isNull())
                return;

            for (auto it = data.begin(); it != data.end(); it++)
            {
                const Property property = endpoint->properties().value(it.key());

                for (int i = 0; i < endpoint->capabilities().count(); i++)
                {
                    const Capability &capability = endpoint->capabilities().at(i);

                    if (!capability->data().contains(it.key()) || capability->data().value(it.key()) == it.value())
                        continue;

                    capability->data().insert(it.key(), it.value());
                    capabilitiesList.append(capability);
                }

                if (!property.isNull() && property->value() != it.value() && (property->type() != "devices.properties.event" || property->events().contains(it.value().toString())))
                {
                    property->setValue(it.value());
                    propertiesList.append(property);
                }
            }

            if (capabilitiesList.isEmpty() && propertiesList.isEmpty())
                return;

            emit dataUpdated(endpoint, capabilitiesList, propertiesList);
        }
    }
}

void Client::readyRead(void)
{
    QByteArray buffer = m_socket->readAll();

    if (m_status == Status::Handshake)
    {
        handshakeRequest data;
        QByteArray hash;
        quint32 value, key;
        DH dh;

        memcpy(&data, buffer.constData(), sizeof(data));

        dh.setPrime(qFromBigEndian(data.prime));
        dh.setGenerator(qFromBigEndian(data.generator));

        value = qToBigEndian(dh.sharedKey());
        m_socket->write(QByteArray(reinterpret_cast <char*> (&value), sizeof(value)));

        key = qToBigEndian(dh.privateKey(qFromBigEndian(data.sharedKey)));
        hash = QCryptographicHash::hash(QByteArray(reinterpret_cast <char*> (&key), sizeof(key)), QCryptographicHash::Md5);

        m_aes->init(hash, QCryptographicHash::hash(hash, QCryptographicHash::Md5));
        m_status = Status::Authorization;
    }
    else
    {
        for (int i = 0; i < buffer.length(); i++)
        {
            switch (buffer.at(i))
            {
                case 0x42: m_buffer.clear(); break;
                case 0x43: parseData(); break;

                case 0x44:

                    switch (buffer.at(++i))
                    {
                        case 0x62: m_buffer.append(0x42); break;
                        case 0x63: m_buffer.append(0x43); break;
                        case 0x64: m_buffer.append(0x44); break;
                    }

                    break;

                default: m_buffer.append(buffer.at(i)); break;
            }
        }
    }
}

void Client::timeout(void)
{
    m_socket->close();
}
