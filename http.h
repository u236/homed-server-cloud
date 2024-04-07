#ifndef HTTP_H
#define HTTP_H

#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>

class Request
{

public:

    Request(QTcpSocket *socket) : m_socket(socket) {}

    inline QTcpSocket *socket(void) { return m_socket; }

    inline QString method(void) { return m_method; }
    inline void setMethod(const QString &value) { m_method = value; }

    inline QString url(void) { return m_url; }
    inline void setUrl(const QString &value) { m_url = value; }

    inline QString body(void) { return m_body; }
    inline void setBody(const QString &value) { m_body = value; }

    inline QMap <QString, QString> &headers(void) { return m_headers; }
    inline QMap <QString, QString> &data(void) { return m_data; }

private:

    QTcpSocket *m_socket;
    QString m_method, m_url, m_body;
    QMap <QString, QString> m_headers, m_data;

};

class HTTP : public QObject
{
    Q_OBJECT

public:

    HTTP(QSettings *settings, QObject *parent = nullptr);
    void sendResponse(Request &request, quint16 code, const QMap <QString, QString> &headers = QMap <QString, QString> (), const QByteArray &response = QByteArray());

private:

    QTcpServer *m_server;

private slots:

    void newConnection(void);
    void readyRead(void);

signals:

    void requestReceived(Request &request);

};

#endif
