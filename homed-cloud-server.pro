QT = core network sql

CONFIG += c++17 console

SOURCES += \
        capability.cpp \
        client.cpp \
        controller.cpp \
        crypto.cpp \
        http.cpp \
        main.cpp

HEADERS += \
    capability.h \
    client.h \
    controller.h \
    crypto.h \
    http.h

target.path = /home/u236
INSTALLS += target
