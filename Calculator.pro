QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# 关闭废弃 API 的警告，代码同时兼容 Qt 5.12 / Qt 6.x
DEFINES += QT_DEPRECATED_WARNINGS

TARGET   = Calculator
TEMPLATE = app

# 源文件里的中文注释和字符串按 UTF-8 编译（MSVC 编译器需要）
msvc: QMAKE_CXXFLAGS += /utf-8

SOURCES += \
    main.cpp \
    calculator.cpp

HEADERS += \
    calculator.h

FORMS += \
    calculator.ui

RESOURCES += \
    resources.qrc
