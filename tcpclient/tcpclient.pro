QT -= gui
QT += network core

CONFIG += c++17 console
CONFIG -= app_bundle

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp \
        tcpclient.cpp
        
HEADERS += \
    tcpclient.h

CONFIG(release, debug|release) {
    DESTDIR = $$PWD/../build
}
