CONFIG += release
TARGET = treemaker
QT = core gui widgets opengl
SOURCES += editor/main.cpp editor/window.cpp editor/editor.cpp editor/camera.cpp editor/file_exporter.cpp editor/scene.cpp editor/render_system.cpp editor/primitives.cpp editor/property_box.cpp editor/curve_editor.cpp editor/curve_button.cpp
HEADERS += editor/window.h editor/editor.h editor/camera.h editor/file_exporter.h editor/scene.h editor/objects.h editor/render_system.h editor/primitives.h editor/property_box.h editor/curve_editor.h editor/curve_button.h
FORMS += editor/qt/window.ui
MOC_DIR = editor/qt
RCC_DIR = editor/qt
UI_DIR = editor/qt
OBJECTS_DIR = build
LIBS += -L ./lib -ltreemaker
INCLUDEPATH = ./lib/include
