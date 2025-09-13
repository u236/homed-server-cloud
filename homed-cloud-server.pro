QT = core network sql

CONFIG += c++17 console debug

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

target.path = /home/u236/staging
INSTALLS += target
