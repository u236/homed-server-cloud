#ifndef CLIENT_H
#define CLIENT_H

#define AUTHORIZATION_TIMEOUT   10000

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <capability.h>
#include "crypto.h"

class EndpointObject;
typedef QSharedPointer <EndpointObject> Endpoint;

class DeviceObject;
typedef QSharedPointer <DeviceObject> Device;

class EndpointObject
{

public:

    EndpointObject(const quint8 &id, const Device &device, bool numeric) : m_id(id), m_device(device), m_numeric(numeric) {}

    inline quint8 id(void) { return m_id; }
    inline Device device(void) { return m_device; }
    inline bool numeric(void) { return m_numeric; }

    inline QString type(void) { return m_type; }
    inline void setType(const QString &value) { if (m_type.isEmpty()) m_type = value; }

    inline QList <QString> &exposes(void) { return m_exposes; }
    inline QMap <QString, QVariant> &options(void) { return m_options; }

    inline QList <Capability> &capabilities(void) { return m_capabilities; }
    inline QMap <QString, Property> &properties(void) { return m_properties; }

private:

    quint8 m_id;
    QWeakPointer <DeviceObject> m_device;
    bool m_numeric;

    QString m_type;

    QList <QString> m_exposes;
    QMap <QString, QVariant> m_options;

    QList <Capability> m_capabilities;
    QMap <QString, Property> m_properties;

};

class DeviceObject
{

public:

    DeviceObject(const QString &key, const QString &topic, const QString &name, const QString &description) :
        m_key(key), m_topic(topic), m_name(name), m_description(description), m_available(false) {}

    inline QString key(void) { return m_key; }

    inline QString topic(void) { return m_topic; }
    inline void setTopic(const QString &value) { m_topic = value; }

    inline QString name(void) { return m_name; }
    inline void setName(const QString &value) { m_name = value; }

    inline QString description(void) { return m_description; }
    inline void setDescription(const QString &value) { m_description = value; }

    inline bool available(void) { return m_available; }
    inline void setAvailable(bool value) { m_available = value; }

    inline QMap <quint8, Endpoint> &endpoints(void) { return m_endpoints; }

private:

    QString m_key, m_topic, m_name, m_description;
    bool m_available;

    QMap <quint8, Endpoint> m_endpoints;

};

struct handshakeRequest
{
    quint32 prime;
    quint32 generator;
    quint32 sharedKey;
};

class Client : public QObject
{
    Q_OBJECT

public:

    Client(QTcpSocket *socket);
    ~Client(void);

    inline QString uniqueId(void) { return m_uniqueId; }
    inline QMap <QString, Device> &devices(void) { return m_devices; }

    void publish(const Endpoint &endpoint, const QJsonObject &json);
    void close(void);

private:

    enum class Status
    {
        Handshake,
        Authorization,
        Ready
    };

    QTcpSocket *m_socket;
    QTimer *m_timer;
    AES128 *m_aes;

    QByteArray m_buffer;
    Status m_status;
    QString m_uniqueId;

    QList <QString> m_types;
    QMap <QString, Device> m_devices;

    Device findDevice(const QString &search);

    void parseExposes(const Endpoint &endpoint);
    void sendRequest(const QString &action, const QString &topic, const QJsonObject &message = QJsonObject());
    void parseData(QByteArray &buffer);

private slots:

    void readyRead(void);
    void timeout(void);

signals:

    void disconnected(void);
    void tokenReceived(const QByteArray &token);
    void devicesUpdated(void);
    void dataUpdated(const Device &device);

};

#endif
