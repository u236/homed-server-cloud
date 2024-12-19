#ifndef CONTROLLER_H
#define CONTROLLER_H

#define CODE_EXPIRE_TIMEOUT     60
#define TOKEN_EXPIRE_TIMEOUT    8640000

#include <QtSql>
#include <QTcpServer>
#include "crypto.h"
#include "http.h"
#include "client.h"

class UserObject;
typedef QSharedPointer <UserObject> User;

enum class BotStatus
{
    Idle,
    Remove,
    Renew
};

class UserObject : public QObject
{
    Q_OBJECT

public:

    UserObject(void) : m_botStatus(BotStatus::Idle) {}

    inline QByteArray name(void) { return m_name; }
    inline void setName(const QByteArray &value) { m_name = value; }

    inline QByteArray hash(void) { return m_hash; }
    inline void setHash(const QByteArray &value) { m_hash = value; }

    inline QByteArray clientToken(void) { return m_clientToken; }
    inline void setClientToken(const QByteArray &value) { m_clientToken = value; }

    inline QByteArray accessToken(void) { return m_accessToken; }
    inline void setAccessToken(const QByteArray &value) { m_accessToken = value; }

    inline QByteArray refreshToken(void) { return m_refreshToken; }
    inline void setRefreshToken(const QByteArray &value) { m_refreshToken = value; }

    inline qint64 tokenExpire(void) { return m_tokenExpire; }
    inline void setTokenExpire(const qint64 &value) { m_tokenExpire = value; }

    inline qint64 codeExpire(void) { return m_codeExpire; }
    inline void setCodeExpire(const qint64 &value) { m_codeExpire = value; }

    inline BotStatus botStatus(void) { return m_botStatus; }
    inline void setBotStatus(BotStatus value) { m_botStatus = value; }

    inline QMap <QString, Client*> &clients(void) { return m_clients; }

private:

    QByteArray m_name, m_hash, m_clientToken, m_accessToken, m_refreshToken;
    qint64 m_tokenExpire, m_codeExpire;
    BotStatus m_botStatus;

    QMap <QString, Client*> m_clients;

};

class Controller : public QObject
{
    Q_OBJECT

public:

    Controller(QObject *parent = nullptr);
    ~Controller(void);

private:

    QSqlDatabase m_db;

    QSettings *m_settings;
    QTimer *m_codeTimer, *m_statsTimer;
    QTcpServer *m_server;
    HTTP *m_http;
    AES128 *m_aes;

    bool m_debug;
    QByteArray m_path, m_clientId, m_clientSecret, m_skillId, m_skillToken, m_botToken, m_rrdPath;
    quint32 m_apiCount, m_eventCount;

    QMap <qint64, User> m_users;
    QMap <QByteArray, User> m_codes;

    QByteArray randomData(int length);
    void storeTokens(const User &user);

    User findUser(const QByteArray &name);
    User findUser(const QString &header);

private slots:

    void clearCodes(void);
    void updateStats(void);

    void requestReceived(Request &request);
    void newConnection(void);

    void disconnected(void);
    void tokenReceived(const QByteArray &token);
    void devicesUpdated(void);
    void dataUpdated(const Device &device);

};

#endif
