#
#  This file is part of OpenAstro Guide Insight, a fork of PHD2 Log Viewer
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

project(guideinsight LANGUAGES C CXX)

if(WIN32)
  enable_language(RC)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release MinSizeRel RelWithDebInfo)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#################################################################################
# Compiler warnings, applied to this project's targets only

option(GUIDEINSIGHT_WERROR "Treat compiler warnings as errors" OFF)

function(guideinsight_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
    if(GUIDEINSIGHT_WERROR)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra)
    if(GUIDEINSIGHT_WERROR)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

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
# its own minor version, so the 3.2 minimum is checked separately.
find_package(wxWidgets CONFIG QUIET COMPONENTS html core base)
if(wxWidgets_FOUND)
  if(wxWidgets_VERSION VERSION_LESS 3.2)
    message(FATAL_ERROR "wxWidgets 3.2 or newer is required, found ${wxWidgets_VERSION}")
  endif()
  set(GUIDEINSIGHT_WX_LIBS wx::html wx::core wx::base)
  message(STATUS "Found wxWidgets ${wxWidgets_VERSION} (CMake package)")
else()
  find_package(wxWidgets 3.2 REQUIRED COMPONENTS html core base)
  include(${wxWidgets_USE_FILE})
  set(GUIDEINSIGHT_WX_LIBS ${wxWidgets_LIBRARIES})
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

# Parser and analysis code, independent of the GUI so it can be unit tested.
add_library(guideinsight_core STATIC
  logparser.cpp
  logparser.h
  analysis.cpp
  analysis.h
  graphmath.cpp
  graphmath.h
  ticks.cpp
  ticks.h
  exclusion.cpp
  exclusion.h
  settingsmigration.cpp
  settingsmigration.h
)
target_include_directories(guideinsight_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(guideinsight_core PUBLIC _USE_MATH_DEFINES HAVE_TYPE_TRAITS)
target_link_libraries(guideinsight_core PUBLIC ${GUIDEINSIGHT_WX_LIBS} GSL::gsl GSL::gslcblas)
guideinsight_warnings(guideinsight_core)
if(MSVC)
  target_compile_definitions(guideinsight_core PUBLIC _CRT_SECURE_NO_WARNINGS)
  target_compile_options(guideinsight_core PUBLIC /utf-8)
endif()

set(SRC
  AnalysisWin.cpp
  AnalysisWin.h
  LogViewApp.cpp
  LogViewApp.h
  LogViewFrame.cpp
  LogViewFrame.h
)

if(WIN32)
  list(APPEND SRC openastro-guide-insight.rc)
endif()

add_executable(guideinsight WIN32 MACOSX_BUNDLE ${SRC} ${FBSRC})

target_include_directories(guideinsight PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(guideinsight PRIVATE guideinsight_core)
guideinsight_warnings(guideinsight)

if(WIN32)
  set_target_properties(guideinsight PROPERTIES OUTPUT_NAME "OpenAstroGuideInsight")
elseif(UNIX AND NOT APPLE)
  set_target_properties(guideinsight PROPERTIES OUTPUT_NAME "openastro-guide-insight")
endif()

if(APPLE)
  set_target_properties(guideinsight PROPERTIES
    OUTPUT_NAME                        "OpenAstro Guide Insight"
    MACOSX_BUNDLE_BUNDLE_NAME          "OpenAstro Guide Insight"
    MACOSX_BUNDLE_GUI_IDENTIFIER       "org.openastro.guideinsight"
    MACOSX_BUNDLE_ICON_FILE            openastro-guide-insight.ico
    MACOSX_BUNDLE_INFO_STRING          "OpenAstro Guide Insight, a fork of PHD2 Log Viewer"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.6.5"
    MACOSX_BUNDLE_LONG_VERSION_STRING  "0.6.5"
    MACOSX_BUNDLE_BUNDLE_VERSION       "0.6.5"
    MACOSX_BUNDLE_COPYRIGHT            "(C) 2016-2020 Andy Galasso, (C) 2026 OpenAstro contributors"
  )
endif()

add_custom_command(TARGET guideinsight POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/README.md
    $<TARGET_FILE_DIR:guideinsight>
)

#################################################################################
# Tests

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

#################################################################################
# Install

include(GNUInstallDirs)

if(APPLE)
  install(TARGETS guideinsight BUNDLE DESTINATION .)
elseif(WIN32)
  install(TARGETS guideinsight RUNTIME DESTINATION .)
  install(FILES LICENSE.txt README.md DESTINATION .)
else()
  install(TARGETS guideinsight RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(FILES openastro-guide-insight.png DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps)
  install(FILES openastro-guide-insight.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  install(FILES LICENSE.txt README.md DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()
