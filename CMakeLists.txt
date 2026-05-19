cmake_minimum_required(VERSION 4.0)

include(vcpkg/BootstrapVcpkg.cmake)

project(linker_set LANGUAGES CXX VERSION 1.0.0)

if(NOT DEFINED BUILD_TESTING)
  set(BUILD_TESTING ${PROJECT_IS_TOP_LEVEL})
endif()

option(LINKER_SET_BUILD_TESTS
  "Build linker set tests. Default: ${BUILD_TESTING}"
  ${BUILD_TESTING}
)

add_library(linker_set INTERFACE)
target_sources(linker_set
  INTERFACE
    FILE_SET HEADERS
    BASE_DIRS src
    FILES
      src/linker_set.hpp
)

add_library(linker_set_writeable INTERFACE)
target_link_libraries(linker_set_writeable
  INTERFACE
    linker_set
)
target_compile_definitions(linker_set_writeable
  INTERFACE
    LS_LINKER_SET_WRITABLE=1
)

if(LINKER_SET_BUILD_TESTS)
  find_package(Catch2 REQUIRED CONFIG)

  add_executable(linker_set_tests)
  target_sources(linker_set_tests
    PRIVATE
      test/tests.cpp
      test/tu_a.cpp
      test/tu_b.cpp

    PRIVATE
      FILE_SET HEADERS
      BASE_DIRS test
  )
  target_link_libraries(linker_set_tests
    PRIVATE
      linker_set
      Catch2::Catch2WithMain
  )
  target_compile_features(linker_set_tests
    PRIVATE
      cxx_std_23
  )

  enable_testing()
  include(Catch)
  catch_discover_tests(linker_set_tests)
endif()

include(GNUInstallDirs)

install(
  TARGETS linker_set linker_set_writeable
  EXPORT linker_set-targets
  FILE_SET HEADERS
)
install(
  EXPORT linker_set-targets
  NAMESPACE linker_set::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/linker_set
  FILE linker_set-config.cmake
)
