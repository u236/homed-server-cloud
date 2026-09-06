![HOMEd Cloud Server](.github/logo.png)

# HOMEd Cloud Server

Сервер для интеграции устройств HOMEd с Умным Домом Яндекса

Параметры сервера:

http/port (8084): Порт для взаимодействия с yandex. 
С учётом необходимости https ссылок имеет смысл пропускать через nginx или любой другой прокси с поддержкой https и указывать в навыке порт, где https соединение или изменить порт в настройках cloud server, а в прокси указать 8084. 
В настройках навыка указать:
Backend: Endpoint URL https://домен:8084/api
URL авторизации: https://домен:8084/login
URL для получения токена: https://домен:8084/token
URL для обновления токена: https://домен:8084/refresh


server/port (8042): Порт для обмена с homed сервисом, совпадает с указанным в клиентском homed-cloud.conf

client/id и client/secret: Из навыка https://dialogs.yandex.ru/developer/smart-home - раздел "Связка аккаунтов" Client Identifier и Client Password

skill/id: можно получить из адреса навыка в диалогах https://dialogs.yandex.ru/developer/skills/<skill-id>/authorization

skill/token: https://yandex.ru/dev/dialogs/smart-home/doc/ru/reference-alerts/resources-alerts#oauth - получить токен

bot/token и bot/secret: параметры Телеграм бота, получить можно через @BotFather

user/login и user/password: Имя и пароль для входа пользователя при связывании аккаунтов, для тестирования или если не подключен бот Телеграм

rrd/path: путь к папке для формирования статистики, папка должна существовать.

