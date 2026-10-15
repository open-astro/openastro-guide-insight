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

add_executable(phdlogview_tests
  main.cpp
  test_logparser.cpp
  test_analysis.cpp
)
target_link_libraries(phdlogview_tests PRIVATE phdlogview_core Catch2::Catch2)
phdlogview_warnings(phdlogview_tests)
target_compile_definitions(phdlogview_tests PRIVATE
  TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

if(TARGET Catch2::Catch2 AND NOT COMMAND catch_discover_tests)
  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()
include(Catch)
catch_discover_tests(phdlogview_tests DISCOVERY_MODE PRE_TEST)
