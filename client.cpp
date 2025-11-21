#include <QtEndian>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include "client.h"

Client::Client(QTcpSocket *socket) : QObject(nullptr), m_socket(socket), m_timer(new QTimer(this)), m_aes(new AES128), m_status(Status::Handshake)
{
    m_types = {"zigbee", "modbus", "custom"};

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
    QString topic = QString("td/").append(endpoint->device()->topic());

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

Device Client::findDevice(const QString &search)
{
    for (auto it = m_devices.begin(); it != m_devices.end(); it++)
        if (search.startsWith(it.value()->key()) || search.startsWith(it.value()->topic()))
            return it.value();

    return Device();
}

void Client::parseExposes(const Endpoint &endpoint)
{
    // basic

    if (endpoint->exposes().contains("switch"))
    {
        endpoint->setType(endpoint->options().value("switch").toString() == "outlet" ? "devices.types.socket" : "devices.types.switch");
        endpoint->capabilities().append(Capability(new Capabilities::Switch));
    }

    if (endpoint->exposes().contains("lock"))
    {
        endpoint->setType(endpoint->options().value("lock").toString() == "valve" ? "devices.types.openable.valve" : "devices.types.openable.door_lock");
        endpoint->capabilities().append(Capability(new Capabilities::Switch));
    }

    if (endpoint->exposes().contains("light"))
    {
        QList <QVariant> list = endpoint->options().value("light").toList();

        endpoint->setType("devices.types.light");
        endpoint->capabilities().append(Capability(new Capabilities::Switch));

        if (list.contains("level"))
            endpoint->capabilities().append(Capability(new Capabilities::Brightness));

        if (list.contains("color") || list.contains("colorTemperature"))
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
        Capabilities::ThermostatPower *power = nullptr;

        endpoint->setType("devices.types.thermostat");

        if (list.contains("off"))
        {
            list.removeAll("off");
            power = new Capabilities::ThermostatPower(list.value(0));
            endpoint->capabilities().append(Capability(power));
        }

        if (!list.isEmpty())
            endpoint->capabilities().append(Capability(new Capabilities::ThermostatMode(list, power)));

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

    // water meter

    if (endpoint->exposes().contains("volume"))
    {
        endpoint->setType("devices.types.smart_meter");
        endpoint->properties().insert("volume", Property(new Properties::Volume));
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

    // other

    if (endpoint->type().isEmpty())
        return;

    if (endpoint->exposes().contains("fanMode"))
        endpoint->capabilities().append(Capability(new Capabilities::FanMode(endpoint->options().value("fanMode").toMap().value("enum").toList())));

    if (endpoint->exposes().contains("heatMode"))
        endpoint->capabilities().append(Capability(new Capabilities::HeatMode(endpoint->options().value("heatMode").toMap().value("enum").toList())));

    if (endpoint->exposes().contains("swingMode"))
        endpoint->capabilities().append(Capability(new Capabilities::SwingMode(endpoint->options().value("swingMode").toMap().value("enum").toList())));

    if (endpoint->exposes().contains("battery"))
        endpoint->properties().insert("battery", Property(new Properties::Battery));

    if (endpoint->exposes().contains("batteryLow"))
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

        sendRequest("subscribe", "status/#");
        m_status = Status::Ready;
        m_timer->stop();
    }
    else
    {
        QJsonObject message = json.value("message").toObject();
        QString topic = json.value("topic").toString();

        if (topic.startsWith("status/"))
        {
            QMap <QString, Device> map;
            QString type = topic.split('/').value(1), service = topic.mid(topic.indexOf('/') + 1);
            QJsonArray devices = message.value("devices").toArray();
            bool names = message.value("names").toBool(), check = false;

            if (!m_types.contains(type))
                return;

            for (auto it = devices.begin(); it != devices.end(); it++)
            {
                QJsonObject item = it->toObject();
                QString name = item.value("name").toString(), id, key;

                if (name.isEmpty() || item.value("removed").toBool() || !item.value("cloud").toBool(true) || name == "HOMEd Coordinator")
                    continue;

                switch (m_types.indexOf(type))
                {
                    case 0: id = item.value("ieeeAddress").toString(); break;                                                  // zigbee
                    case 1: id = QString("%1.%2").arg(item.value("portId").toInt()).arg(item.value("slaveId").toInt()); break; // modbus
                    case 2: id = item.value("id").toString(); break;                                                           // custom
                }

                key = QString("%1/%2").arg(type, id);
                map.insert(key, Device(new DeviceObject(key, QString("%1/%2").arg(service, names ? name : id), name, item.value("description").toString())));
            }

            for (auto it = map.begin(); it != map.end(); it++)
            {
                const Device &device = m_devices.value(it.key());

                if (device.isNull())
                {
                    m_devices.insert(it.key(), it.value());
                    sendRequest("subscribe", QString("expose/").append(it.value()->topic()));
                    sendRequest("subscribe", QString("device/").append(it.value()->topic()));
                    check = true;
                }
                else
                {
                    device->setTopic(it.value()->topic());
                    device->setName(it.value()->name());
                    device->setDescription(it.value()->description());
                }
            }

            for (auto it = m_devices.begin(); it != m_devices.end(); NULL)
            {
                if (it.value()->topic().startsWith(service) && !map.contains(it.key()))
                {
                    it = m_devices.erase(it);
                    check = true;
                    continue;
                }

                it++;
            }

            if (!check)
                return;

            emit devicesUpdated();
        }
        else if (topic.startsWith("expose/"))
        {
            const Device &device = findDevice(topic.mid(topic.indexOf('/') + 1));
            QList <QString> subscriptions;

            if (device.isNull() || !device->endpoints().isEmpty())
                return;

            for (auto it = message.begin(); it != message.end(); it++)
            {
                QJsonObject json = it.value().toObject(), options = json.value("options").toObject();
                QJsonArray items = json.value("items").toArray();

                for (int i = 0; i < items.count(); i++)
                {
                    QString item = items.at(i).toString(), subscription = QString("fd/").append(device->topic()), expose;
                    QList <QString> itemList = item.split('_');
                    quint8 id = static_cast <quint8> (itemList.count() > 1 ? itemList.value(1).toInt() : it.key().toInt());
                    Endpoint endpoint = device->endpoints().value(id);

                    expose = itemList.value(0);

                    if (endpoint.isNull())
                    {
                        endpoint = Endpoint(new EndpointObject(id, device, itemList.count() > 1));

                        for (auto it = options.begin(); it != options.end(); it++)
                        {
                            QList <QString> optionList = it.key().split('_');

                            if (!optionList.value(1).isEmpty() && optionList.value(1).toInt() != id)
                                continue;

                            endpoint->options().insert(optionList.value(0), it.value().toVariant());
                        }

                        device->endpoints().insert(id, endpoint);
                    }

                    endpoint->setType(options.value("yandexType").toString());

                    if (!endpoint->exposes().contains(expose))
                        endpoint->exposes().append(expose);

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

            sendRequest("publish", QString("command/").append(device->topic().mid(0, device->topic().lastIndexOf('/'))), {{"action", "getProperties"}, {"device", device->topic().split('/').last()}, {"service", "cloud"}});
        }
        else if (topic.startsWith("device/"))
        {
            const Device &device = findDevice(topic.mid(topic.indexOf('/') + 1));

            if (device.isNull())
                return;

            device->setAvailable(message.value("status").toString() == "online");
        }
        else if (topic.startsWith("fd/"))
        {
            const Device &device = findDevice(topic.mid(topic.indexOf('/') + 1));
            QMap <QString, QVariant> data = message.toVariantMap();

            if (device.isNull())
                return;

            for (auto it = data.begin(); it != data.end(); it++)
            {
                QList <QString> itemList = it.key().split('_');
                Endpoint endpoint = device->endpoints().value(static_cast <quint8> (itemList.count() > 1 ? itemList.value(1).toInt() : topic.split('/').last().toInt()));

                if (!endpoint.isNull())
                {
                    QString name = itemList.value(0);
                    const Property &property = endpoint->properties().value(name);

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

        while ((length = m_buffer.indexOf(0x43)) > 0)
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
