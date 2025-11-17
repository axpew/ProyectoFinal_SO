TEMPLATE = app
CONFIG += c++17
QT += widgets core gui

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    transportbeltwidget.cpp \
    productioncontroller.cpp \
    ipc_common.cpp \
    station_child.cpp \
    threadmanager.cpp

HEADERS += \
    mainwindow.h \
    station_child.h \
    transportbeltwidget.h \
    productioncontroller.h \
    ipc_common.h \
    threadmanager.h \
    product.h

FORMS += \
    mainwindow.ui

RESOURCES += resources.qrc

unix: LIBS += -pthread
