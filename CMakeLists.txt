cmake_minimum_required(VERSION 3.22)
set(SC_MODULE web)
project(simply-cpp-${SC_MODULE} HOMEPAGE_URL "https://github.com/roelofrossouw/simply-cpp-web" LANGUAGES CXX)
include(cmake/sc_bootstrap.cmake)

option(SC_WEB_BUILD_TESTS "Build simply-cpp-web tests" ${PROJECT_IS_TOP_LEVEL})

get_sc_version()
message(STATUS "SimplyCPP ${SC_MODULE} Version: ${SC_VERSION}")

# The demo's sc::console and sc::params.
find_or_install_package(sc-core simply-cpp-dev simply-cpp 1.12.0)
# libxml2: CMake's own FindLibXml2 finds Ubuntu's (libxml2-dev) or the one in the macOS SDK, so
# nothing comes from Homebrew. It stays out of the installed headers.
find_or_install_package(LibXml2 libxml2-dev libxml2)

set(SOURCE_OBJECTS)
set(SOURCE_LIBRARIES)
set(SOURCE_LINK_LIBRARIES)

# CSS declarations, as in a style attribute; plain C++.
add_sc_object(style)
add_sc_object(xml LINK_LIBRARIES LibXml2::LibXml2 PUBLIC_LINK_LIBRARIES LibXml2::LibXml2)

add_sc_libraries(DESCRIPTION "simply-cpp-web")

install_sc_module(PATH_VARS CMAKE_INSTALL_INCLUDEDIR)
install(FILES cmake/SimplyCppFunctions.cmake cmake/sc_bootstrap.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sc-web
        COMPONENT development)

if (SC_WEB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests sc-web-test)
endif ()

add_sc_example(sc-web-xml LINK_LIBRARIES sc::sc-core-shared sc::sc-web-shared)

# libxml2's runtime package (libxml2, or libxml2-16 on resolute) comes from dpkg-shlibdeps.
package_sc_module(DEV_DEPENDS libxml2-dev)
