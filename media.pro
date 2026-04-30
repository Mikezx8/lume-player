QT += core gui widgets multimedia multimediawidgets concurrent dbus

CONFIG += c++17

TARGET = media
TEMPLATE = app

SOURCES += main.cpp

HEADERS += 

# Force MOC to process main.cpp
moc_headers += main.cpp