cmake_minimum_required(VERSION 3.22)
set(SC_MODULE web)
project(simply-cpp-${SC_MODULE} HOMEPAGE_URL "https://github.com/roelofrossouw/simply-cpp-web" LANGUAGES CXX)
include(cmake/sc_bootstrap.cmake)

option(SC_WEB_BUILD_TESTS "Build simply-cpp-web tests" ${PROJECT_IS_TOP_LEVEL})

get_sc_version()
message(STATUS "SimplyCPP ${SC_MODULE} Version: ${SC_VERSION}")

# The demo's sc::console and sc::params.
find_or_install_package(sc-core simply-cpp-dev simply-cpp 4.0.0)
# libxml2: CMake's own FindLibXml2 finds Ubuntu's (libxml2-dev) or the one in the macOS SDK, so
# nothing comes from Homebrew. It stays out of the installed headers.
find_or_install_package(LibXml2 libxml2-dev libxml2)
# md4c parses Markdown for sc::web::markdown (MIT; libmd4c-dev, Homebrew md4c), behind its header.
# Ubuntu's md4cConfig.cmake names md4c-html's library too, so its package comes along (on noble a
# missing libmd4c-html.so fails the find): both are installed, a list apt gets as two packages.
find_or_install_package(md4c "libmd4c-dev;libmd4c-html0-dev" md4c)

set(SOURCE_OBJECTS)
set(SOURCE_LIBRARIES)
set(SOURCE_LINK_LIBRARIES)

# CSS declarations, as in a style attribute; plain C++.
add_sc_object(style)
# HTML character references, encoded and decoded as Perl's HTML::HTML5::Entities does; plain C++.
add_sc_object(html_entities)
add_sc_object(xml LINK_LIBRARIES LibXml2::LibXml2 PUBLIC_LINK_LIBRARIES LibXml2::LibXml2)
# Markdown as HTML (CommonMark and GitHub's tables, strikethrough, task lists, autolinks).
add_sc_object(markdown LINK_LIBRARIES md4c::md4c PUBLIC_LINK_LIBRARIES md4c::md4c)

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

# libxml2's and md4c's runtime packages (libxml2, or libxml2-16 on resolute; libmd4c0) come from dpkg-shlibdeps. The
# library doesn't use sc-core; the demo loads libsc-core-shared (simply-cpp).
package_sc_module(DEPENDS "simply-cpp (>= 4.0.0~)" DEV_DEPENDS "libxml2-dev, libmd4c-dev, libmd4c-html0-dev")
