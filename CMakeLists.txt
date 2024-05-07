
cmake_minimum_required(VERSION 2.8.12)

# Project
set(CMAKE_PROJECT_NAME "main" CACHE STRING "Default main project name")
project("${CMAKE_PROJECT_NAME}")


# Add cmake-mx modules option
set(CMAKE_MX_MODULES "" CACHE STRING "Path to user cmake modules")

if(NOT CMAKE_MX_MODULES)
    set(CMAKE_MX_MODULES "$ENV{CMAKE_MX_MODULES}")
endif()

if(NOT CMAKE_MX_MODULES)
    set(CMAKE_MX_MODULES_DEFAULT "/usr/share/cmake-mx/cmake")
    if(EXISTS "${CMAKE_MX_MODULES_DEFAULT}")
        set(CMAKE_MX_MODULES "${CMAKE_MX_MODULES_DEFAULT}")
    endif()
endif()

if(NOT CMAKE_MX_MODULES)
    message(FATAL_ERROR "cmake-mx modules not found")
endif()

# Set custom modules path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_MX_MODULES}")

include(cmake_utils)



# ------------------------------------------------------------------------------

if(CMAKE_BUILD_VARIANT MATCHES "release")
    set(CMAKE_BUILD_TYPE  Release)
else()
    set(CMAKE_BUILD_TYPE  Debug)
endif()

include_conditional(CMakeVariant)

