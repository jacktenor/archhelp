QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

INCLUDEPATH += /home/greg/openssl-3/include
LIBS += -L/home/greg/openssl-3/lib64 -lssl -lcrypto

QMAKE_LFLAGS += -Wl,-rpath,/home/greg/openssl-3/lib64

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    Installwizard.cpp \
    installerworker.cpp \
    systemworker.cpp \
    partitionworker.cpp \
    main.cpp

HEADERS += \
    Installwizard.h \
    installerworker.h \
    systemworker.h \
    partitionworker.h \
    systemworker.h

FORMS += \
    Installwizard.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
