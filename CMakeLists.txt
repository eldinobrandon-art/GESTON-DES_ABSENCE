cmake_minimum_required(VERSION 3.16)

project(gestion_absences VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Recherche des composants Qt 6 nécessaires (Widgets, Gui, Sql)
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Sql)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
)

qt_add_executable(gestion_absences
    ${PROJECT_SOURCES}
)

# Liaison des bibliothèques (Interface graphique, Widgets et Base de données)
target_link_libraries(gestion_absences PRIVATE
    Qt6::Widgets
    Qt6::Gui
    Qt6::Sql
)

# Finalisation pour Qt 6
if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(gestion_absences)
endif()