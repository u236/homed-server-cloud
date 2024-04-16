#include <QtEndian>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include "client.h"

Client::Client(QTcpSocket *socket) : QObject(nullptr), m_socket(socket), m_timer(new QTimer(this)), m_aes(new AES128), m_status(Status::Handshake)
{
    m_services = {"zigbee", "modbus", "custom"};

    connect(m_socket, &QTcpSocket::readyRead, this, &Client::readyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &Client::disconnected);
    connect(m_timer, &QTimer::timeout, this, &Client::timeout);

    m_timer->setSingleShot(true);
    m_timer->start(AUTHORIZATION_TIMEOUT);
}

Client::~Client(void)
{
    delete m_aes;
    close();
}

void Client::publish(const Endpoint &endpoint, const QJsonObject &json)
{
    QString topic = QString("td/").append(endpoint->device()->id());

    if (endpoint->numeric())
    {
        QJsonObject data;

        for (auto it = json.begin(); it != json.end(); it++)
            data.insert(QString("%1_%2").arg(it.key()).arg(endpoint->id()), it.value());

        sendRequest("publish", topic, data);
    }
    else
    {
        if (endpoint->id())
            topic.append(QString("/%1").arg(endpoint->id()));

        sendRequest("publish", topic, json);
    }
}

void Client::close(void)
{
    m_socket->close();
}

void Client::parseExposes(const Endpoint &endpoint)
{
    // basic

    if (endpoint->exposes().contains("switch"))
    {
        endpoint->setType(endpoint->options().value("switch").toString() == "outlet" ? "devices.types.socket" : "devices.types.switch");
        endpoint->capabilities().append(Capability(new Capabilities::Switch));
    }

    if (endpoint->exposes().contains("light"))
    {
        endpoint->setType("devices.types.light");
        endpoint->capabilities().append(Capability(new Capabilities::Switch));

        if (endpoint->options().value("light").toList().contains("level"))
            endpoint->capabilities().append(Capability(new Capabilities::Brightness));

        endpoint->capabilities().append(Capability(new Capabilities::Color(endpoint->options())));
    }

    if (endpoint->exposes().contains("cover"))
    {
        endpoint->setType("devices.types.openable.curtain");
        endpoint->capabilities().append(Capability(new Capabilities::Curtain));
        endpoint->capabilities().append(Capability(new Capabilities::Open));
    }

    if (endpoint->exposes().contains("thermostat"))
    {
        QList <QVariant> list = endpoint->options().value("systemMode").toMap().value("enum").toList();

        endpoint->setType("devices.types.thermostat");

        if (list.contains("off") && list.contains("heat"))
            endpoint->capabilities().append(Capability(new Capabilities::Thermostat));

        endpoint->capabilities().append(Capability(new Capabilities::Temperature(endpoint->options())));
        endpoint->properties().insert("temperature", Property(new Properties::Temperature));
    }

    // event

    if (endpoint->exposes().contains("action"))
    {
        QList <QVariant> list = endpoint->options().value("action").toMap().value("enum").toList();

        if (list.contains("singleClick") || list.contains("doubleClick") || list.contains("hold"))
        {
            endpoint->setType("devices.types.sensor.button");
            endpoint->properties().insert("action", Property(new Properties::Button(list)));
        }
    }

    if (endpoint->exposes().contains("contact"))
    {
        endpoint->setType("devices.types.sensor.open");
        endpoint->properties().insert("contact", Property(new Properties::Binary("open", "opened", "closed")));
    }

    if (endpoint->exposes().contains("gas"))
    {
        endpoint->setType("devices.types.sensor.gas");
        endpoint->properties().insert("gas", Property(new Properties::Binary("gas", "detected", "not_detected")));
    }

    if (endpoint->exposes().contains("occupancy"))
    {
        endpoint->setType("devices.types.sensor.motion");
        endpoint->properties().insert("occupancy", Property(new Properties::Binary("motion", "detected", "not_detected")));
    }

    if (endpoint->exposes().contains("smoke"))
    {
        endpoint->setType("devices.types.sensor.smoke");
        endpoint->properties().insert("smoke", Property(new Properties::Binary("smoke", "detected", "not_detected")));
    }

    if (endpoint->exposes().contains("waterLeak"))
    {
        endpoint->setType("devices.types.sensor.water_leak");
        endpoint->properties().insert("waterLeak", Property(new Properties::Binary("water_leak", "leak", "dry")));
    }

    if (endpoint->exposes().contains("vibration"))
    {
        endpoint->setType("devices.types.sensor.vibration");
        endpoint->properties().insert("event", Property(new Properties::Vibration));
    }

    // climate

    if (endpoint->exposes().contains("temperature") && !endpoint->options().value("temperature").toMap().value("diagnostic").toBool())
    {
        endpoint->setType("devices.types.sensor.climate");
        endpoint->properties().insert("temperature", Property(new Properties::Temperature));
    }

    if (endpoint->exposes().contains("pressure"))
    {
        endpoint->setType("devices.types.sensor.climate");
        endpoint->properties().insert("pressure", Property(new Properties::Pressure));
    }

    if (endpoint->exposes().contains("humidity"))
    {
        endpoint->setType("devices.types.sensor.climate");
        endpoint->properties().insert("humidity", Property(new Properties::Humidity));
    }

    if (endpoint->exposes().contains("co2"))
    {
        endpoint->setType("devices.types.sensor.climate");
        endpoint->properties().insert("co2", Property(new Properties::CO2));
    }

    if (endpoint->exposes().contains("pm1"))
    {
        endpoint->setType("devices.types.sensor.climate");
        endpoint->properties().insert("pm1", Property(new Properties::PM1));
    }

    if (endpoint->exposes().contains("pm10"))
    {
        endpoint->setType("devices.types.sensor.climate");
        endpoint->properties().insert("pm10", Property(new Properties::PM10));
    }

    if (endpoint->exposes().contains("pm25"))
    {
        endpoint->setType("devices.types.sensor.climate");
        endpoint->properties().insert("pm25", Property(new Properties::PM25));
    }

    if (endpoint->exposes().contains("voc"))
    {
        endpoint->setType("devices.types.sensor.climate");
        endpoint->properties().insert("voc", Property(new Properties::VOC));
    }

    // illumination

    if (endpoint->exposes().contains("illuminance"))
    {
        endpoint->setType("devices.types.sensor.illumination");
        endpoint->properties().insert("illuminance", Property(new Properties::Illuminance));
    }

    // electricity

    if (endpoint->exposes().contains("energy"))
    {
        endpoint->setType("devices.types.smart_meter.electricity");
        endpoint->properties().insert("energy", Property(new Properties::Energy));
    }

    if (endpoint->exposes().contains("voltage"))
    {
        endpoint->setType("devices.types.smart_meter.electricity");
        endpoint->properties().insert("voltage", Property(new Properties::Voltage));
    }

    if (endpoint->exposes().contains("current"))
    {
        endpoint->setType("devices.types.smart_meter.electricity");
        endpoint->properties().insert("current", Property(new Properties::Current));
    }

    if (endpoint->exposes().contains("power"))
    {
        endpoint->setType("devices.types.smart_meter.electricity");
        endpoint->properties().insert("power", Property(new Properties::Power));
    }

    // battery

    if (endpoint->type().isEmpty())
        return;

    if (endpoint->exposes().contains("battery"))
        endpoint->properties().insert("battery", Property(new Properties::Battery));
    else if (endpoint->exposes().contains("batteryLow"))
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

void Client::parseData(QByteArray &buffer)
{
    QJsonObject json;

    m_aes->cbcDecrypt(buffer);
    json = QJsonDocument::fromJson(buffer.constData()).object();

    if (m_status == Status::Authorization)
    {
        m_uniqueId = json.value("uniqueId").toString();
        emit tokenReceived(QByteArray::fromHex(json.value("token").toString().toUtf8()));

        if (!parent())
        {
            m_socket->close();
            return;
        }

        for (int i = 0; i < m_services.count(); i++)
            sendRequest("subscribe", QString("status/").append(m_services.at(i)));

        m_status = Status::Ready;
        m_timer->stop();
    }
    else
    {
        QJsonObject message = json.value("message").toObject();
        QString topic = json.value("topic").toString();

        if (topic.startsWith("status/"))
        {
            QList <QString> list = topic.split('/');
            QMap <QString, Device> map;
            QJsonArray devices = message.value("devices").toArray();
            QString service = list.value(1);
            bool check = false;

            for (auto it = devices.begin(); it != devices.end(); it++)
            {
                QJsonObject device = it->toObject();
                QString name = device.value("name").toString(), id;

                if (name.isEmpty() || !device.value("cloud").toBool(true))
                    continue;

                switch (m_services.indexOf(service))
                {
                    case 0: // zigbee
                        id = QString("zigbee/%1").arg(message.value("names").toBool() ? name : device.value("ieeeAddress").toString());
                        break;

                    case 1: // modbus
                        // id = QString("modbus/%1.%2").arg(device.value("portId").toInt()).arg(device.value("slaveId").toInt());
                        continue;

                    case 2: // custom
                        id = QString("custom/%1").arg(message.value("names").toBool() ? name : device.value("id").toString());
                        break;
                }

                map.insert(id, Device(new DeviceObject(id, name, device.value("description").toString())));
            }

            for (auto it = map.begin(); it != map.end(); it++)
            {
                if (!m_devices.contains(it.key()))
                {
                    QList <QString> subscriptions = {QString("device/").append(it.key()), QString("expose/").append(it.key())};

                    m_devices.insert(it.key(), it.value());

                    for (int i = 0; i < subscriptions.count(); i++)
                        sendRequest("subscribe", subscriptions.at(i));

                    check = true;
                }
            }

            for (auto it = m_devices.begin(); it != m_devices.end(); it++)
            {
                if (it.key().split('/').value(0) != service)
                    continue;

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
        else if (topic.startsWith("device/"))
        {
            QList <QString> list = topic.split('/');
            Device device = m_devices.value(QString("%1/%2").arg(list.value(1), list.value(2)));

            if (device.isNull())
                return;

            device->setAvailable(message.value("status").toString() == "online" ? true : false);
        }
        else if (topic.startsWith("expose/"))
        {
            QList <QString> topicList = topic.split('/'), subscriptions;
            Device device = m_devices.value(QString("%1/%2").arg(topicList.value(1), topicList.value(2)));

            if (device.isNull() || !device->endpoints().isEmpty())
                return;

            for (auto it = message.begin(); it != message.end(); it++)
            {
                QJsonObject json = it.value().toObject(), options = json.value("options").toObject();
                QJsonArray items = json.value("items").toArray();

                for (int i = 0; i < items.count(); i++)
                {
                    QString item = items.at(i).toString(), subscription = QString("fd/").append(device->id()), expose;
                    QList <QString> itemList = item.split('_');
                    quint8 id = static_cast <quint8> (itemList.count() > 1 ? itemList.value(1).toInt() : it.key().toInt());
                    Endpoint endpoint = device->endpoints().value(id);

                    if (endpoint.isNull())
                    {
                        endpoint = Endpoint(new EndpointObject(id, device, itemList.count() > 1));
                        device->endpoints().insert(id, endpoint);
                    }

                    expose = itemList.value(0);

                    if (!endpoint->exposes().contains(expose))
                        endpoint->exposes().append(expose);

                    if (options.contains(item))
                        endpoint->options().insert(expose, options.value(item));

                    if (endpoint->id() && !endpoint->numeric())
                        subscription.append(QString("/%1").arg(id));

                    if (subscriptions.contains(subscription))
                        continue;

                    subscriptions.append(subscription);
                }
            }

            for (auto it = device->endpoints().begin(); it != device->endpoints().end(); it++)
                parseExposes(it.value());

            for (int i = 0; i < subscriptions.count(); i++)
                sendRequest("subscribe", subscriptions.at(i));

            sendRequest("publish", QString("command/").append(topicList.value(1)), {{"action", "getProperties"}, {"device", device->name()}});
        }
        else if (topic.startsWith("fd/"))
        {
            QMap <QString, QVariant> data = message.toVariantMap();
            QList <QString> topicList = topic.split('/');
            Device device = m_devices.value(QString("%1/%2").arg(topicList.value(1), topicList.value(2)));

            if (device.isNull())
                return;

            for (auto it = data.begin(); it != data.end(); it++)
            {
                QList <QString> itemList = it.key().split('_');
                QString name = itemList.value(0);
                Endpoint endpoint = device->endpoints().value(static_cast <quint8> (itemList.count() > 1 ? itemList.value(1).toInt() : topicList.value(3).toInt()));

                if (!endpoint.isNull())
                {
                    const Property property = endpoint->properties().value(name);

                    for (int i = 0; i < endpoint->capabilities().count(); i++)
                    {
                        const Capability &capability = endpoint->capabilities().at(i);

                        if (!capability->data().contains(name) || capability->data().value(name) == it.value())
                            continue;

                        capability->data().insert(name, it.value());
                        capability->setUpdated(true);
                    }

                    if (!property.isNull() && property->value() != it.value() && (property->type() != "devices.properties.event" || property->events().contains(it.value().toString())))
                    {
                        property->setValue(it.value());
                        property->setUpdated(true);
                    }
                }
            }

            emit dataUpdated(device);
        }
    }
}

void Client::readyRead(void)
{
    QByteArray data = m_socket->readAll();

    if (m_status == Status::Handshake)
    {
        handshakeRequest hanshake;
        QByteArray hash;
        quint32 value, key;
        DH dh;

        memcpy(&hanshake, data.constData(), sizeof(hanshake));

        dh.setPrime(qFromBigEndian(hanshake.prime));
        dh.setGenerator(qFromBigEndian(hanshake.generator));

        value = qToBigEndian(dh.sharedKey());
        m_socket->write(QByteArray(reinterpret_cast <char*> (&value), sizeof(value)));

        key = qToBigEndian(dh.privateKey(qFromBigEndian(hanshake.sharedKey)));
        hash = QCryptographicHash::hash(QByteArray(reinterpret_cast <char*> (&key), sizeof(key)), QCryptographicHash::Md5);

        m_aes->init(hash, QCryptographicHash::hash(hash, QCryptographicHash::Md5));
        m_status = Status::Authorization;
    }
    else
    {
        QByteArray buffer;
        int length;

        m_buffer.append(data); // TODO: check for overflow

        while((length = m_buffer.indexOf(0x43)) > 0)
        {
            for (int i = 0; i < length; i++)
            {
                switch (m_buffer.at(i))
                {
                    case 0x42: buffer.clear(); break;
                    case 0x44: buffer.append(m_buffer.at(++i) & 0xDF); break;
                    default:   buffer.append(m_buffer.at(i)); break;
                }
            }

            m_buffer.remove(0, length + 1);
            parseData(buffer);
        }
    }
}

void Client::timeout(void)
{
    close();
}
