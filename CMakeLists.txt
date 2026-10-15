#
#  This file is part of phdlogview
#
#  Copyright (C) 2016 Andy Galasso
#  Copyright (C) 2026 Open Astro contributors
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License along
#  with this program; if not, visit the http://fsf.org website.

cmake_minimum_required(VERSION 3.24)

# macOS: native Apple Silicon only. Must be set before project().
if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "macOS target architectures")
  set(CMAKE_OSX_DEPLOYMENT_TARGET "26.0" CACHE STRING "Minimum macOS version")
endif()

# Windows + vcpkg static triplets: use the static CRT so no runtime DLLs ship.
if(VCPKG_TARGET_TRIPLET MATCHES "-static$")
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

project(phdlogview LANGUAGES C CXX)

if(WIN32)
  enable_language(RC)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release MinSizeRel RelWithDebInfo)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#################################################################################
# Dependencies
#
# Windows and macOS release builds get wxWidgets and GSL from the vcpkg
# manifest (vcpkg.json). Linux and local development builds can use system
# packages. Either way the same find_package calls apply.

# Prefer wxWidgets' own CMake package (vcpkg installs it). Its targets carry
# the full static dependency list, such as PCRE2 for wxRegEx. Fall back to
# CMake's FindwxWidgets module, which uses wx-config, for system packages.
# No version in the CONFIG call: wxWidgets' package only accepts requests for
# its own minor version, so the minimum is checked separately.
find_package(wxWidgets CONFIG QUIET COMPONENTS html core base)
if(wxWidgets_FOUND)
  if(wxWidgets_VERSION VERSION_LESS 3.0)
    message(FATAL_ERROR "wxWidgets 3.0 or newer is required, found ${wxWidgets_VERSION}")
  endif()
  set(PHDLOGVIEW_WX_LIBS wx::html wx::core wx::base)
  message(STATUS "Found wxWidgets ${wxWidgets_VERSION} (CMake package)")
else()
  find_package(wxWidgets 3.0 REQUIRED COMPONENTS html core base)
  include(${wxWidgets_USE_FILE})
  set(PHDLOGVIEW_WX_LIBS ${wxWidgets_LIBRARIES})
endif()

find_package(GSL REQUIRED)

#################################################################################
# wxFormBuilder generated UI code

option(HAVE_WXFB "Regenerate the UI code from phdlogview.fbp with wxFormBuilder" OFF)

set(FBSRC
  ${CMAKE_CURRENT_BINARY_DIR}/LogViewFrameBase.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/LogViewFrameBase.h
)

if(HAVE_WXFB)
  find_program(WXFB NAMES wxformbuilder wxFormBuilder
    PATHS "C:/Program Files/wxFormBuilder" "C:/Program Files (x86)/wxFormBuilder"
          "/Applications/wxFormBuilder.app/Contents/MacOS"
    REQUIRED)
  add_custom_command(
    OUTPUT ${FBSRC}
    COMMAND ${WXFB} -g ${CMAKE_CURRENT_SOURCE_DIR}/phdlogview.fbp
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/phdlogview.fbp
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )
else()
  configure_file(generated/LogViewFrameBase.cpp ${CMAKE_CURRENT_BINARY_DIR}/LogViewFrameBase.cpp COPYONLY)
  configure_file(generated/LogViewFrameBase.h ${CMAKE_CURRENT_BINARY_DIR}/LogViewFrameBase.h COPYONLY)
endif()

#################################################################################
# Application

set(SRC
  AnalysisWin.cpp
  AnalysisWin.h
  LogViewApp.cpp
  LogViewApp.h
  LogViewFrame.cpp
  LogViewFrame.h
  logparser.cpp
  logparser.h
)

if(WIN32)
  list(APPEND SRC phdlogview.rc)
endif()

add_executable(phdlogview WIN32 MACOSX_BUNDLE ${SRC} ${FBSRC})

target_include_directories(phdlogview PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(phdlogview PRIVATE _USE_MATH_DEFINES HAVE_TYPE_TRAITS)
target_link_libraries(phdlogview PRIVATE ${PHDLOGVIEW_WX_LIBS} GSL::gsl GSL::gslcblas)

if(MSVC)
  target_compile_definitions(phdlogview PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_options(phdlogview PRIVATE /utf-8)
endif()

if(APPLE)
  set_target_properties(phdlogview PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME          phdlogview
    MACOSX_BUNDLE_GUI_IDENTIFIER       "com.adgsoftware.phdlogview"
    MACOSX_BUNDLE_ICON_FILE            phdlogview.ico
    MACOSX_BUNDLE_INFO_STRING          "PHD2 Log Viewer, Copyright 2016-2025 Andy Galasso"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.6.5"
    MACOSX_BUNDLE_LONG_VERSION_STRING  "0.6.5"
    MACOSX_BUNDLE_BUNDLE_VERSION       "0.6.5"
    MACOSX_BUNDLE_COPYRIGHT            "(C) 2016-2025 Andy Galasso"
  )
endif()

add_custom_command(TARGET phdlogview POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/README.txt
    $<TARGET_FILE_DIR:phdlogview>
)

#################################################################################
# Install

include(GNUInstallDirs)

if(APPLE)
  install(TARGETS phdlogview BUNDLE DESTINATION .)
elseif(WIN32)
  install(TARGETS phdlogview RUNTIME DESTINATION .)
  install(FILES LICENSE.txt README.txt DESTINATION .)
else()
  install(TARGETS phdlogview RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(FILES phdlogview.png DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps)
  install(FILES phdlogview.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  install(FILES LICENSE.txt README.txt DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()
