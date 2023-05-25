# SPDX-FileCopyrightText: 2023 Carl Schwan <carlschwan@kde.org>
# SPDX-License-Identifier: BSD-2-Clause

cmake_minimum_required(VERSION 3.16)

project(epubviewer VERSION 0.1)

set(KF_MIN_VERSION "5.80.0")
set(QT_MIN_VERSION "5.15.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)

set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FeatureSummary)
include(ECMSetupVersion)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} NO_MODULE COMPONENTS Widgets Svg Xml Gui Core)
find_package(KF${QT_MAJOR_VERSION} ${KF_MIN_VERSION} COMPONENTS Archive)

add_executable(epubviewer)

target_sources(epubviewer PUBLIC
    epubcontainer.h
    epubdocument.h
    widget.h
    epubcontainer.cpp
    epubdocument.cpp
    main.cpp
    widget.cpp
)

target_link_libraries(epubviewer PUBLIC
    KF${QT_MAJOR_VERSION}::Archive
    Qt::Widgets
    Qt::Core
    Qt::Gui
    Qt::Svg
    Qt::Xml
)

install(TARGETS epubviewer ${KF${QT_MAJOR_VERSION}_INSTALL_TARGETS_DEFAULT_ARGS})