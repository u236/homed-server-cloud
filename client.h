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

    EndpointObject(const quint8 &id, const Device &device) : m_id(id), m_device(device) {}

    inline quint8 id(void) { return m_id; }
    inline Device device(void) { return m_device; }

    inline QString type(void) { return m_type; }
    inline void setType(const QString &value) { if (m_type.isEmpty()) m_type = value; }

    inline QList <Capability> &capabilities(void) { return m_capabilities; }
    inline QMap <QString, Property> &properties(void) { return m_properties; }

private:

    quint8 m_id;
    QWeakPointer <DeviceObject> m_device;

    QString m_type;
    QList <Capability> m_capabilities;
    QMap <QString, Property> m_properties;

};

class DeviceObject
{

public:

    DeviceObject(const QString &id, const QString &name) : m_id(id), m_name(name) {}

    inline QString id(void) { return m_id; }
    inline QString name(void) { return m_name; }

    inline bool available(void) { return m_availabale; }
    inline void setAvailable(bool value) { m_availabale = value; }

    inline QMap <quint8, Endpoint> &endpoints(void) { return m_endpoints; }

private:

    QString m_id, m_name;
    bool m_availabale;

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

    QList <QString> m_services, m_subscriptions;
    QMap <QString, Device> m_devices;

    void parseExposes(const Endpoint &endpoint, const QList<QVariant> &exposes, const QMap <QString, QVariant> &options);

    void sendRequest(const QString &action, const QString &topic, const QJsonObject &message = QJsonObject());
    void parseData(QByteArray &buffer);

private slots:

    void readyRead(void);
    void timeout(void);

signals:

    void disconnected(void);
    void tokenReceived(const QByteArray &token);
    void devicesUpdated(void);
    void dataUpdated(const Endpoint &endpoint, const QList <Capability> &capabilitiesList, const QList <Property> &propertiesList);

};

#endif
