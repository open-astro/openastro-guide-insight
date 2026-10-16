include(FetchContent)

# Use an installed Catch2 3.x if one is available, otherwise download it.
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.8.1
  GIT_SHALLOW    TRUE
  FIND_PACKAGE_ARGS 3
)
FetchContent_MakeAvailable(Catch2)

add_executable(guideinsight_tests
  main.cpp
  test_logparser.cpp
  test_analysis.cpp
  test_build.cpp
  test_graphmath.cpp
  test_ticks.cpp
  test_viewport.cpp
  test_sessiondetails.cpp
  test_selection.cpp
  test_document.cpp
  test_labels.cpp
  test_appsettings.cpp
  test_timeline.cpp
  test_scatter.cpp
  test_periodogram.cpp
  test_exclusion.cpp
  test_settingsmigration.cpp
)
target_link_libraries(guideinsight_tests PRIVATE guideinsight_core Catch2::Catch2)
guideinsight_warnings(guideinsight_tests)
target_compile_definitions(guideinsight_tests PRIVATE
  TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

set(GUIDEINSIGHT_EXPECTED_WX_VERSION "" CACHE STRING
  "If set, a test checks the build uses exactly this wxWidgets version")
if(GUIDEINSIGHT_EXPECTED_WX_VERSION)
  target_compile_definitions(guideinsight_tests PRIVATE
    GUIDEINSIGHT_EXPECTED_WX_VERSION="${GUIDEINSIGHT_EXPECTED_WX_VERSION}")
endif()

if(TARGET Catch2::Catch2 AND NOT COMMAND catch_discover_tests)
  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()
include(Catch)
catch_discover_tests(guideinsight_tests DISCOVERY_MODE PRE_TEST)

# Build policy checks for the macOS app bundle: native Apple Silicon only,
# and the supported minimum macOS version.
if(APPLE)
  add_test(NAME macos_app_is_arm64_only
    COMMAND sh -c "test \"$(lipo -archs \"$1\")\" = arm64" _ $<TARGET_FILE:guideinsight>)
  add_test(NAME macos_app_minimum_is_26
    COMMAND sh -c "vtool -show-build \"$1\" | grep -q -E 'minos 26\\.0$'" _ $<TARGET_FILE:guideinsight>)
endif()
