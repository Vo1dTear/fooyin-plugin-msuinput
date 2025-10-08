cmake_minimum_required(VERSION 3.16)
project(msuinput LANGUAGES CXX)

# Enable automatic Qt meta-object code generation
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC OFF)

# Find Fooyin and its modules
find_package(Fooyin REQUIRED)

# Create the MSU-1 plugin
create_fooyin_plugin(
    msuinput
    JSON_IN msuinput.json.in
    DEPENDS
        Fooyin::Core
        Fooyin::Gui
    SOURCES
        msuplugin.cpp
        msuplugin.h
        msuinput.cpp
        msuinput.h
        msusettingswidget.cpp
        msusettingswidget.h
)

# Install the plugin into the standard user or system plugin directory
include(GNUInstallDirs)
install(
    TARGETS msuinput
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/fooyin/plugins"
)
