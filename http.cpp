#include <QUrl>
#include "http.h"

HTTP::HTTP(QSettings *settings, QObject *parent) : QObject(parent), m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &HTTP::newConnection);

    if (!m_server->listen(QHostAddress::Any, static_cast <quint16> (settings->value("http/port", 8084).toInt())))
    {
        qWarning() << "HTTP server startup error:" << m_server->errorString();
        return;
    }

    qDebug() << "HTTP server listening on port" << m_server->serverPort();
}

void HTTP::sendResponse(Request &request, quint16 code, const QMap <QString, QString> &headers, const QByteArray &response)
{
    QByteArray buffer;

    switch (code)
    {
        case 200: buffer = "HTTP/1.1 200 OK"; break;
        case 301: buffer = "HTTP/1.1 301 Moved Permanently"; break;
        case 401: buffer = "HTTP/1.1 401 Unauthorized"; break;
        case 403: buffer = "HTTP/1.1 403 Forbidden"; break;
        case 404: buffer = "HTTP/1.1 404 Not Found"; break;
        case 405: buffer = "HTTP/1.1 405 Method Not Allowed"; break;
    }

    for (auto it = headers.begin(); it != headers.end(); it++)
        buffer.append(QString("\r\n%1: %2").arg(it.key(), it.value()).toUtf8());

    request.socket()->write(buffer.append("\r\n\r\n").append(response));
    request.socket()->close();
}

void HTTP::newConnection(void)
{
    QTcpSocket *socket = m_server->nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, &HTTP::readyRead);
}

void HTTP::readyRead(void)
{
    QTcpSocket *socket = reinterpret_cast <QTcpSocket*> (sender());
    QList <QString> list = QString(socket->readAll()).split("\r\n\r\n"), head = list.value(0).split("\r\n"), target = head.value(0).split(0x20), items;
    QString method = target.value(0), url = target.value(1), body = list.value(1);
    Request request(socket);

    items = QString(method == "GET" && url.contains('?') ? url.mid(url.indexOf('?') + 1) : body).split('&');
    url = url.mid(0, url.indexOf('?'));

    request.setMethod(method);
    request.setUrl(url);
    request.setBody(body);

    for (int i = 1; i < head.length(); i++)
    {
        QList <QString> header = head.at(i).split(':');
        request.headers().insert(header.value(0).trimmed(), header.value(1).trimmed());
    }

    for (int i = 0; i < items.length(); i++)
    {
        QList <QString> item = items.at(i).split('=');
        request.data().insert(item.value(0), QUrl::fromPercentEncoding(item.value(1).toUtf8()));

    }

    emit requestReceived(request);
}
