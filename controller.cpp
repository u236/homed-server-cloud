#include <QCryptographicHash>
#include <QRandomGenerator>
#include "controller.h"

Controller::Controller(QObject *parent) : QObject(parent), m_db(QSqlDatabase::addDatabase("QSQLITE")), m_settings(new QSettings("/etc/homed/homed-cloud-server.conf", QSettings::IniFormat, this)), m_codeTimer(new QTimer(this)), m_statsTimer(new QTimer(this)), m_server(new QTcpServer(this)), m_http(new HTTP(m_settings, this)), m_aes(new AES128), m_apiCount(0), m_eventCount(0)
{
    QSqlQuery query(m_db);

    if (!m_server->listen(QHostAddress::Any, static_cast <quint16> (m_settings->value("server/port", 8042).toInt())))
    {
        qWarning() << "Cloud server startup error:" << m_server->errorString();
        return;
    }

    m_db.setDatabaseName(m_settings->value("server/database").toString());

    if (!m_db.open())
    {
        qWarning() << "Database open error" << m_db.lastError().text();
        return;
    }

    m_debug = m_settings->value("server/debug", false).toBool();
    m_path = m_settings->value("server/path").toByteArray();
    m_clientId = m_settings->value("client/id").toByteArray();
    m_clientSecret = QByteArray::fromHex(m_settings->value("client/secret").toByteArray());
    m_skillId = m_settings->value("skill/id").toByteArray();
    m_skillToken = m_settings->value("skill/token").toByteArray();
    m_botToken = m_settings->value("bot/token").toByteArray();
    m_rrdPath = m_settings->value("rrd/path").toByteArray();

    m_aes->init(m_clientSecret, QCryptographicHash::hash(m_clientSecret, QCryptographicHash::Md5));
    query.exec("SELECT chat, name, hash, clientToken, accessToken, refreshToken, tokenExpire FROM users");

    while (query.next())
    {
        qint64 id = query.value(0).toLongLong();
        User user(new UserObject);

        user->setName(query.value(1).toByteArray());
        user->setHash(query.value(2).toByteArray());
        user->setClientToken(QByteArray::fromHex(query.value(3).toByteArray()));
        user->setAccessToken(QByteArray::fromHex(query.value(4).toByteArray()));
        user->setRefreshToken(QByteArray::fromHex(query.value(5).toByteArray()));
        user->setTokenExpire(query.value(6).toLongLong());

        m_users.insert(id, user);
    }

    connect(m_codeTimer, &QTimer::timeout, this, &Controller::clearCodes);
    connect(m_statsTimer, &QTimer::timeout, this, &Controller::updateStats);
    connect(m_http, &HTTP::requestReceived, this, &Controller::requestReceived);
    connect(m_server, &QTcpServer::newConnection, this, &Controller::newConnection);

    m_codeTimer->start(1000);

    if (!m_rrdPath.isEmpty())
        m_statsTimer->start(10000);

    qDebug() << "Cloud server started";
}

Controller::~Controller()
{
    m_db.close();
    delete m_aes;
}

QByteArray Controller::randomData(int length)
{
    QByteArray data;

    for (int i = 0; i < length; i++)
        data.append(static_cast <char> (QRandomGenerator::global()->generate()));

    return data;
}

void Controller::updateTokens(const User &user)
{
    QSqlQuery query(m_db);
    query.exec(QString("UPDATE users SET accessToken = '%1', refreshToken = '%2', tokenExpire = %3, timestamp = %4 WHERE name = '%5'").arg(user->accessToken().toHex(), user->refreshToken().toHex()).arg(user->tokenExpire()).arg(QDateTime::currentSecsSinceEpoch()).arg(user->name().constData()));
}

User Controller::findUser(const QByteArray &name)
{
    for (auto it = m_users.begin(); it != m_users.end(); it++)
        if (it.value()->name() == name)
            return it.value();

    return User();
}

User Controller::findUser(const QString &header)
{
    QList <QString> list = header.split(' ');
    QByteArray accessToken = QByteArray::fromHex(list.value(1).toUtf8());

    if (list.value(0) != "Bearer")
        return User();

    m_aes->cbcDecrypt(accessToken);

    for (auto it = m_users.begin(); it != m_users.end(); it++)
        if (it.value()->accessToken() == accessToken && it.value()->tokenExpire() >= QDateTime::currentSecsSinceEpoch())
            return it.value();

    return User();
}

void Controller::clearCodes(void)
{
    for (auto it = m_codes.begin(); it != m_codes.end(); it++)
    {
        if (it.value()->codeExpire() < QDateTime::currentSecsSinceEpoch())
            m_codes.erase(it++);

        if (it == m_codes.end())
            break;
    }
}

void Controller::updateStats(void)
{
    quint64 clients = 0, time = QDateTime::currentSecsSinceEpoch();

    for (auto it = m_users.begin(); it != m_users.end(); it++)
        clients += it.value()->clients().count();

    system(QString("rrdcreate %1/user.rrd --no-overwrite --step 10 DS:data:GAUGE:3600:U:U RRA:AVERAGE:0.5:1:8640 RRA:AVERAGE:0.5:60:1008 RRA:AVERAGE:0.5:360:744 RRA:AVERAGE:0.5:2160:1460 > /dev/null &").arg(m_rrdPath.constData()).toUtf8());
    system(QString("rrdupdate %1/user.rrd %2:%3 > /dev/null &").arg(m_rrdPath.constData()).arg(time - time % 10).arg(m_users.count()).toUtf8());

    system(QString("rrdcreate %1/client.rrd --no-overwrite --step 10 DS:data:GAUGE:3600:U:U RRA:AVERAGE:0.5:1:8640 RRA:AVERAGE:0.5:60:1008 RRA:AVERAGE:0.5:360:744 RRA:AVERAGE:0.5:2160:1460 > /dev/null &").arg(m_rrdPath.constData()).toUtf8());
    system(QString("rrdupdate %1/client.rrd %2:%3 > /dev/null &").arg(m_rrdPath.constData()).arg(time - time % 10).arg(clients).toUtf8());

    system(QString("rrdcreate %1/api.rrd --no-overwrite --step 10 DS:data:GAUGE:3600:U:U RRA:AVERAGE:0.5:1:8640 RRA:AVERAGE:0.5:60:1008 RRA:AVERAGE:0.5:360:744 RRA:AVERAGE:0.5:2160:1460 > /dev/null &").arg(m_rrdPath.constData()).toUtf8());
    system(QString("rrdupdate %1/api.rrd %2:%3 > /dev/null &").arg(m_rrdPath.constData()).arg(time - time % 10).arg(m_apiCount).toUtf8());

    system(QString("rrdcreate %1/event.rrd --no-overwrite --step 10 DS:data:GAUGE:3600:U:U RRA:AVERAGE:0.5:1:8640 RRA:AVERAGE:0.5:60:1008 RRA:AVERAGE:0.5:360:744 RRA:AVERAGE:0.5:2160:1460 > /dev/null &").arg(m_rrdPath.constData()).toUtf8());
    system(QString("rrdupdate %1/event.rrd %2:%3 > /dev/null &").arg(m_rrdPath.constData()).arg(time - time % 10).arg(m_eventCount).toUtf8());

    m_apiCount = 0;
    m_eventCount = 0;
}

void Controller::requestReceived(Request &request)
{
    if (request.url() == "/telegram")
    {
        QJsonObject json = QJsonDocument::fromJson(request.body().toUtf8()).object().value("message").toObject(), chat = json.value("chat").toObject(), from = json.value("from").toObject();

        if (chat.value("type").toString() == "private" && !from.value("is_bot").toBool())
        {
            QList <QString> list = {"/start", "/renew", "/remove", "/confirm", "/cancel", "/getid"};
            QString command = json.value("text").toString(), message;
            qint64 id = chat.value("id").toVariant().toLongLong();
            bool update = false, remove = false;
            auto it = m_users.find(id);

            switch (list.indexOf(command))
            {
                case 0: // start

                    if (it != m_users.end())
                        break;

                    message = "Credentials created.\n\n";
                    update = true;
                    break;

                case 1: // renew

                    if (it != m_users.end())
                    {
                        message = "Are you really want to get new credentials?\nSend /confirm or /cancel.";
                        it.value()->setBotStatus(BotStatus::Renew);
                        break;
                    }

                    message = "Credentials created.\n\n";
                    update = true;
                    break;

                case 2: // remove

                    if (it != m_users.end())
                    {
                        message = "Are you really want to remove your credentials?\nSend /confirm or /cancel.";
                        it.value()->setBotStatus(BotStatus::Remove);
                        break;
                    }

                    message = "Credentials not found.";
                    break;

                case 3: // confirm

                    if (it == m_users.end())
                        break;

                    switch (it.value()->botStatus())
                    {
                        case BotStatus::Renew:
                            message = "Credentials updated.\n\n";
                            update = true;
                            break;

                        case BotStatus::Remove:
                            message = "Credentials successfully removed.";
                            remove = true;
                            break;

                        default:
                            break;
                    }

                    break;

                case 4: // cancel

                    if (it == m_users.end() || it.value()->botStatus() == BotStatus::Idle)
                        break;

                    message = "Action cancelled.";
                    it.value()->setBotStatus(BotStatus::Idle);
                    break;

                case 5: // getid
                    message = QString("Your chat identifier:\n`%1`").arg(id);
                    break;
            }

            if (update)
            {
                QByteArray salt = randomData(16), password = randomData(8).toHex();
                QSqlQuery query(m_db);

                if (it == m_users.end())
                    it = m_users.insert(id, User(new UserObject));

                it.value()->setName(QByteArray("user_").append(randomData(5).toHex()));
                it.value()->setHash(salt.toHex().append(QCryptographicHash::hash(QByteArray(salt).append(password), QCryptographicHash::Md5).toHex()));
                it.value()->setClientToken(randomData(32));
                it.value()->setAccessToken(QByteArray());
                it.value()->setRefreshToken(QByteArray());
                it.value()->setTokenExpire(0);

                message.append(QString("Username:\n`%1`\n\nPassword:\n`%2`\n\nClient token:\n`%3`").arg(it.value()->name(), password, it.value()->clientToken().toHex()));
                query.exec(QString("INSERT INTO users (chat, name, hash, clientToken, timestamp) VALUES (%1, '%2', '%3', '%4', %5) ON CONFLICT (chat) DO UPDATE SET name = excluded.name, hash = excluded.hash, clientToken = excluded.clientToken, accessToken = NULL, refreshToken = NULL, tokenExpire = NULL, timestamp = excluded.timestamp").arg(id).arg(it.value()->name(), it.value()->hash(), it.value()->clientToken().toHex()).arg(QDateTime::currentSecsSinceEpoch()));
                it.value()->setBotStatus(BotStatus::Idle);
            }
            else if (remove)
            {
                QSqlQuery query(m_db);
                m_users.erase(it);
                query.exec(QString("DELETE FROM users WHERE chat = %1").arg(id));
            }

            if (!message.isEmpty())
            {
                QJsonObject json = {{"chat_id", id}, {"parse_mode", "Markdown"}, {"text", message}};
                system(QString("curl -X POST -H 'Content-Type: application/json' -d '%1' -s https://api.telegram.org/bot%2/sendMessage > /dev/null &").arg(QJsonDocument(json).toJson(QJsonDocument::Compact), m_botToken).toUtf8().constData());
            }
        }

        m_http->sendResponse(request, 200);
        return;
    }
    else if (request.url() == "/logo.png")
    {
        QFile file(QString("%1/logo.png").arg(m_path.constData()));

        if (file.open(QFile::ReadOnly))
        {
            m_http->sendResponse(request, 200, {{"Content-Type", "image/png"}}, file.readAll());
            file.close();
            return;
        }
    }
    else if (request.url() == "/login")
    {
        if (request.method() == "GET")
        {
            QFile file(QString("%1/login.html").arg(m_path.constData()));

            if (file.open(QFile::ReadOnly))
            {
                QString data = QString(file.readAll().constData()).arg(request.data().value("client_id"), request.data().value("redirect_uri"), request.data().value("state"), request.data().value("username"), request.data().value("password"));
                m_http->sendResponse(request, 200, {{"Content-Type", "text/html"}}, data.toUtf8());
                file.close();
                return;
            }
        }
        else if (request.method() == "POST")
        {
            const User &user = findUser(request.data().value("username").toUtf8());
            QByteArray salt, code;

            if (request.data().value("client_id").toUtf8() != m_clientId)
            {
                m_http->sendResponse(request, 403);
                return;
            }

            if (user.isNull())
            {
                m_http->sendResponse(request, 301, {{"Location", QString("/login?%1").arg(request.body())}});
                return;
            }

            salt = QByteArray::fromHex(user->hash().mid(0, 32));

            if (user->hash() != salt.toHex().append(QCryptographicHash::hash(QByteArray(salt).append(request.data().value("password").toUtf8()), QCryptographicHash::Md5).toHex()))
            {
                m_http->sendResponse(request, 301, {{"Location", QString("/login?%1").arg(request.body())}});
                return;
            }

            user->setCodeExpire(QDateTime::currentSecsSinceEpoch() + CODE_EXPIRE_TIMEOUT);
            qDebug() << user->name() << "logged in";

            code = randomData(32);
            m_codes.insert(code, user);
            m_aes->cbcEncrypt(code);

            m_http->sendResponse(request, 301, {{"Location", QString("%1?state=%2&code=%3").arg(request.data().value("redirect_uri"), request.data().value("state"), code.toHex())}});
            return;
        }
        else
        {
            m_http->sendResponse(request, 405);
            return;
        }
    }
    else if (request.url() == "/refresh" || request.url() == "/token")
    {
        QByteArray secret = QByteArray::fromHex(request.data().value("client_secret").toUtf8()), accessToken, refreshToken;
        AES128 aes;
        User user;

        if (request.method() != "POST")
        {
            m_http->sendResponse(request, 405);
            return;
        }

        if (request.data().value("client_id").toUtf8() != m_clientId || request.data().value("grant_type") != (request.url() == "/refresh" ? "refresh_token" : "authorization_code"))
        {
            m_http->sendResponse(request, 403);
            return;
        }

        aes.init(secret, QCryptographicHash::hash(secret, QCryptographicHash::Md5));

        if (request.url() == "/refresh")
        {
            refreshToken = QByteArray::fromHex(request.data().value("refresh_token").toUtf8());
            aes.cbcDecrypt(refreshToken);

            for (auto it = m_users.begin(); it != m_users.end(); it++)
            {
                if (it.value()->refreshToken() == refreshToken)
                {
                    user = it.value();
                    break;
                }
            }
        }
        else
        {
            QByteArray code = QByteArray::fromHex(request.data().value("code").toUtf8());
            aes.cbcDecrypt(code);
            user = m_codes.value(code);
        }

        if (user.isNull())
        {
            m_http->sendResponse(request, 401);
            return;
        }

        qDebug() << user->name() << "token" << (request.url() == "/refresh" ? "refreshed" : "issued");

        user->setAccessToken(randomData(32));
        user->setRefreshToken(randomData(32));
        user->setTokenExpire(QDateTime::currentSecsSinceEpoch() + TOKEN_EXPIRE_TIMEOUT);
        updateTokens(user);

        accessToken = user->accessToken();
        refreshToken = user->refreshToken();

        m_aes->cbcEncrypt(accessToken);
        m_aes->cbcEncrypt(refreshToken);

        m_http->sendResponse(request, 200, {{"Content-Type", "application/json"}}, QJsonDocument(QJsonObject {{"access_token", accessToken.toHex().constData()}, {"refresh_token", refreshToken.toHex().constData()}, {"token_type", "Bearer"}, {"expires_in", TOKEN_EXPIRE_TIMEOUT}}).toJson(QJsonDocument::Compact));
        return;
    }
    else if (request.url() == "/api/v1.0")
    {
        if (request.method() != "HEAD")
        {
            m_http->sendResponse(request, 405);
            return;
        }

        m_http->sendResponse(request, 200);
        return;
    }
    else if (request.url() == "/api/v1.0/user/unlink")
    {
        const User &user = findUser(request.headers().value("Authorization"));

        if (request.method() != "POST")
        {
            m_http->sendResponse(request, 405);
            return;
        }

        if (user.isNull())
        {
            m_http->sendResponse(request, 401);
            return;
        }

        user->setAccessToken(QByteArray());
        user->setRefreshToken(QByteArray());
        user->setTokenExpire(0);

        qDebug() << user->name() << "unlinked";
        updateTokens(user);

        m_http->sendResponse(request, 200, {{"Content-Type", "application/json"}}, QJsonDocument(QJsonObject {{"request_id", request.headers().value("X-Request-Id")}}).toJson(QJsonDocument::Compact));
        return;
    }
    else if (request.url() == "/api/v1.0/user/devices")
    {
        const User &user = findUser(request.headers().value("Authorization"));
        QJsonArray devices;
        QJsonObject json;

        if (request.method() != "GET")
        {
            m_http->sendResponse(request, 405);
            return;
        }

        if (user.isNull())
        {
            m_http->sendResponse(request, 401);
            return;
        }

        for (auto it = user->clients().begin(); it != user->clients().end(); it++)
        {
            Client *client = it.value();

            for (auto it = client->devices().begin(); it != client->devices().end(); it++)
            {
                const Device device = it.value();

                for (auto it = device->endpoints().begin(); it != device->endpoints().end(); it++)
                {
                    const Endpoint endpoint = it.value();
                    QJsonArray capabilities, properties;

                    for (int i = 0; i < endpoint->capabilities().count(); i++)
                    {
                        const Capability capability = endpoint->capabilities().at(i);
                        QJsonObject item = {{"type", capability->type()}, {"retrievable", true}, {"reportable", true}, {"state", capability->state()}};

                        if (!capability->parameters().isEmpty())
                            item.insert("parameters", QJsonObject::fromVariantMap(capability->parameters()));

                        capabilities.append(item);
                    }

                    for (auto it = endpoint->properties().begin(); it != endpoint->properties().end(); it++)
                    {
                        QJsonObject item = {{"type", it.value()->type()}, {"retrievable", true}, {"reportable", true}, {"parameters", QJsonObject::fromVariantMap(it.value()->parameters())}};

                        if (it.value()->value().isValid())
                            item.insert("state", it.value()->state());

                        properties.append(item);
                    }

                    if (!capabilities.isEmpty() || !properties.isEmpty())
                    {
                        QString id = client->uniqueId().append('/').append(device->id()), name = device->name(), model = device->name();
                        QJsonObject json;

                        if (it.value()->id())
                        {
                            id.append(QString("/%1").arg(it.value()->id()));
                            name.append(QString(" %1").arg(it.value()->id()));
                        }

                        if (!device->description().isEmpty())
                            model.append(QString(" (%1)").arg(device->description()));

                        devices.append(QJsonObject {{"id", id}, {"name", name}, {"type", endpoint->type()}, {"capabilities", capabilities}, {"properties", properties}, {"device_info", QJsonObject{{"model", model}}}});
                    }
                }
            }
        }

        json = {{"request_id", request.headers().value("X-Request-Id")}, {"payload", QJsonObject {{"user_id", user->name().constData()}, {"devices", devices}}}};

        if (m_debug)
            qDebug() << user->name() << "devices data" << QJsonDocument(json).toJson(QJsonDocument::Compact).constData();

        m_http->sendResponse(request, 200, {{"Content-Type", "application/json"}}, QJsonDocument(json).toJson(QJsonDocument::Compact));
        m_apiCount++;
        return;
    }
    else if (request.url() == "/api/v1.0/user/devices/query")
    {
        const User &user = findUser(request.headers().value("Authorization"));
        QJsonArray queries = QJsonDocument::fromJson(request.body().toUtf8()).object().value("devices").toArray(), devices;
        QJsonObject json;

        if (request.method() != "POST")
        {
            m_http->sendResponse(request, 405);
            return;
        }

        if (user.isNull())
        {
            m_http->sendResponse(request, 401);
            return;
        }

        for (auto it = queries.begin(); it != queries.end(); it++)
        {
            QJsonObject query = it->toObject();
            QString id = query.value("id").toString();
            QList <QString> list = id.split('/');
            Client *client = user->clients().value(list.value(0));

            if (client)
            {
                const Device &device = client->devices().value(QString("%1/%2").arg(list.value(1), list.value(2)));

                if (device.isNull())
                {
                    devices.append(QJsonObject {{"id", id}, {"error_code", "DEVICE_NOT_FOUND"}});
                    continue;
                }

                if (device->available())
                {
                    const Endpoint &endpoint = device->endpoints().value(static_cast <quint8> (list.value(3).toInt()));
                    QJsonArray capabilities, properties;

                    if (endpoint.isNull())
                    {
                        devices.append(QJsonObject {{"id", id}, {"error_code", "DEVICE_NOT_FOUND"}});
                        continue;
                    }

                    for (int i = 0; i < endpoint->capabilities().count(); i++)
                    {
                        const Capability capability = endpoint->capabilities().at(i);
                        capabilities.append(QJsonObject {{"type", capability->type()}, {"state", capability->state()}});
                    }

                    for (auto it = endpoint->properties().begin(); it != endpoint->properties().end(); it++)
                    {
                        if (!it.value()->value().isValid())
                            continue;

                        properties.append(QJsonObject {{"type", it.value()->type()}, {"state", it.value()->state()}});
                    }

                    devices.append(QJsonObject {{"id", id}, {"capabilities", capabilities}, {"properties", properties}});
                    continue;
                }
            }

            devices.append(QJsonObject {{"id", id}, {"error_code", "DEVICE_UNREACHABLE"}});
        }

        json = {{"request_id", request.headers().value("X-Request-Id")}, {"payload", QJsonObject {{"devices", devices}}}};

        if (m_debug)
        {
            qDebug() << user->name() << "query reqest:" << request.body().toUtf8().constData();
            qDebug() << user->name() << "query reply:" << QJsonDocument(json).toJson(QJsonDocument::Compact).constData();
        }

        m_http->sendResponse(request, 200, {{"Content-Type", "application/json"}}, QJsonDocument(json).toJson(QJsonDocument::Compact));
        m_apiCount++;
        return;
    }
    else if (request.url() == "/api/v1.0/user/devices/action")
    {
        const User &user = findUser(request.headers().value("Authorization"));
        QJsonArray actions = QJsonDocument::fromJson(request.body().toUtf8()).object().value("payload").toObject().value("devices").toArray(), devices;
        QJsonObject json;

        if (request.method() != "POST")
        {
            m_http->sendResponse(request, 405);
            return;
        }

        if (user.isNull())
        {
            m_http->sendResponse(request, 401);
            return;
        }

        for (auto it = actions.begin(); it != actions.end(); it++)
        {
            QJsonObject action = it->toObject();
            QJsonArray capabilities = action.value("capabilities").toArray();
            QString id = action.value("id").toString();
            QList <QString> list = id.split('/');
            Client *client = user->clients().value(list.value(0));
            bool check = false;

            if (client)
            {
                const Device &device = client->devices().value(QString("%1/%2").arg(list.value(1), list.value(2)));

                if (device.isNull())
                {
                    devices.append(QJsonObject {{"id", action.value("id")}, {"action_result", QJsonObject {{"status", "ERROR"}, {"error_code", "DEVICE_NOT_FOUND"}}}});
                    continue;
                }

                if (device->available())
                {
                    const Endpoint &endpoint = device->endpoints().value(static_cast <quint8> (list.value(3).toInt()));

                    if (!endpoint.isNull())
                    {
                        for (auto it = capabilities.begin(); it != capabilities.end(); it++)
                        {
                            QJsonObject json = it->toObject();
                            QString type = json.value("type").toString();

                            for (int i = 0; i < endpoint->capabilities().count(); i++)
                            {
                                const Capability capability = endpoint->capabilities().at(i);

                                if (capability->type() == type)
                                {
                                    client->publish(endpoint, capability->action(json.value("state").toObject()));
                                    check = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (check)
                    {
                        devices.append(QJsonObject {{"id", action.value("id")}, {"action_result", QJsonObject {{"status", "DONE"}}}});
                        continue;
                    }
                }
            }

            devices.append(QJsonObject {{"id", action.value("id")}, {"action_result", QJsonObject {{"status", "ERROR"}, {"error_code", "DEVICE_UNREACHABLE"}}}});
        }

        json = {{"request_id", request.headers().value("X-Request-Id")}, {"payload", QJsonObject {{"devices", devices}}}};

        if (m_debug)
        {
            qDebug() << user->name() << "action reqest:" << request.body().toUtf8().constData();
            qDebug() << user->name() << "action reply:" << QJsonDocument(json).toJson(QJsonDocument::Compact).constData();
        }

        m_http->sendResponse(request, 200, {{"Content-Type", "application/json"}}, QJsonDocument(json).toJson(QJsonDocument::Compact));
        m_apiCount++;
        return;
    }

    m_http->sendResponse(request, 404);
}

void Controller::newConnection(void)
{
    Client *client(new Client(m_server->nextPendingConnection()));

    if (m_debug)
        qDebug() << client << "connected";

    connect(client, &Client::disconnected, this, &Controller::disconnected);
    connect(client, &Client::tokenReceived, this, &Controller::tokenReceived);
    connect(client, &Client::devicesUpdated, this, &Controller::devicesUpdated);
    connect(client, &Client::dataUpdated, this, &Controller::dataUpdated);
}

void Controller::disconnected(void)
{
    Client *client = reinterpret_cast <Client*> (sender());
    UserObject *user = reinterpret_cast <UserObject*> (client->parent());

    if (user)
        qDebug() << "Client" << QString("%1:%2").arg(user->name(), client->uniqueId()) << "disconnected";

    client->deleteLater();
}

void Controller::tokenReceived(const QByteArray &token)
{
    Client *client = reinterpret_cast <Client*> (sender());

    for (auto it = m_users.begin(); it != m_users.end(); it++)
    {
        if (it.value()->clientToken() != token)
            continue;

        qDebug() << "Client" << QString("%1:%2").arg(it.value()->name(), client->uniqueId()) << "authorized";
        client->setParent(it.value().data());
        it.value()->clients().insert(client->uniqueId(), client);
        break;
    }
}

void Controller::devicesUpdated(void)
{
    Client *client = reinterpret_cast <Client*> (sender());
    UserObject *user = reinterpret_cast <UserObject*> (client->parent());

    if (user)
    {
        QJsonObject json = {{"ts", QDateTime::currentSecsSinceEpoch()}, {"payload", QJsonObject {{"user_id", user->name().constData()}}}};
        system(QString("curl -i -s -X POST https://dialogs.yandex.net/api/v1/skills/%1/callback/discovery -H 'Authorization: OAuth %2' -H 'Content-Type: application/json' -d '%3' > /dev/null &").arg(m_skillId, m_skillToken, QJsonDocument(json).toJson(QJsonDocument::Compact).constData()).toUtf8().constData());
        m_eventCount++;
    }
}

void Controller::dataUpdated(const Endpoint &endpoint, const QList <Capability> &capabilitiesList, const QList <Property> &propertiesList)
{
    Client *client = reinterpret_cast <Client*> (sender());
    UserObject *user = reinterpret_cast <UserObject*> (client->parent());

    if (user)
    {
        QString id = client->uniqueId().append('/').append(endpoint->device()->id());
        QJsonObject json = {{"ts", QDateTime::currentSecsSinceEpoch()}};
        QJsonArray capabilities, properties;

        if (endpoint->id())
            id.append(QString("/%1").arg(endpoint->id()));

        for (int i = 0; i < capabilitiesList.count(); i++)
        {
            const Capability &capability = capabilitiesList.at(i);
            capabilities.append(QJsonObject {{"type", capability->type()}, {"state", capability->state()}});
        }

        for (int i = 0; i < propertiesList.count(); i++)
        {
            const Property &property = propertiesList.at(i);
            properties.append(QJsonObject {{"type", property->type()}, {"state", property->state()}});

            if (property->instance() != "button" && property->instance() != "vibration")
                continue;

            property->setValue(QVariant());
        }

        json.insert("payload", QJsonObject {{"user_id", user->name().constData()}, {"devices", QJsonArray {QJsonObject {{"id", id}, {"capabilities", capabilities}, {"properties", properties}}}}});
        system(QString("curl -i -s -X POST https://dialogs.yandex.net/api/v1/skills/%1/callback/state -H 'Authorization: OAuth %2' -H 'Content-Type: application/json' -d '%3' > /dev/null &").arg(m_skillId, m_skillToken, QJsonDocument(json).toJson(QJsonDocument::Compact).constData()).toUtf8().constData());
        m_eventCount++;
    }
}
