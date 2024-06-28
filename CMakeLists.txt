# Request the minimum version of CMake, in case of lower version throws error.
# See #https://cmake.org/cmake/help/latest/command/cmake_minimum_required.html

cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project(
    "server"
    VERSION 1.0.0
    DESCRIPTION "CMake C Project Template for Operating Systems II"
    LANGUAGES C
)

# Enable testing and add the tests
# CTest is the framework used by CMake to run tests
include(CTest)

# Define the C standard, we are going to use std17
# See https://cmake.org/cmake/help/latest/variable/CMAKE_CXX_STANDARD.html
set(CMAKE_C_STANDARD 17)

# Include the 'include' directory, where the header files are located.
# See https://cmake.org/cmake/help/latest/command/include_directories.html
include_directories(include)

# To automatic download external dependencies, where the headers are located.
# See https://cmake.org/cmake/help/latest/command/fetch_content.html
include(FetchContent)

# In this example we are going to use Unity to download and to test our program.
set(UNITY_GET_URL "https://github.com/ThrowTheSwitch/Unity.git")

# Enable debug
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
    Unity
    GIT_REPOSITORY ${UNITY_GET_URL}
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/Unity
    UPDATE_COMMAND "" # Disable the update command
)

# Asures the named dependencies have been populated, either by and earlier call or by populating them itself.
FetchContent_MakeAvailable(Unity)

# Add the 'src' directory, where the source files are located.
# See https://cmake.org/cmake/help/latest/command/file.html#glob
file(GLOB_RECURSE SOURCES "src/server/server.c" "src/server/main.c")

# Add the compilation flags
# See https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_FLAGS.html#variable:CMAKE_%3CLANG%3E_FLAGS
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -pedantic -Wextra -Werror -Wconversion -std=gnu11")

# Add the executable
#See https://cmake.org/cmake/help/latest/command/add_executable.html
add_executable(${PROJECT_NAME} ${SOURCES})
add_executable(tcp_client "src/clients/tcp_client.c")
add_executable(udp_client "src/clients/udp_client.c")

add_subdirectory(lib/socketSetup)
add_subdirectory(lib/cJSON)
add_subdirectory(lib/suppliesData)
add_subdirectory(lib/alertInfection)
add_subdirectory(lib/emergNotif)

target_include_directories(${PROJECT_NAME}  PUBLIC lib/socketSetup/include)
target_include_directories(${PROJECT_NAME}  PUBLIC lib/cJSON/include)
target_include_directories(${PROJECT_NAME}  PUBLIC lib/suppliesData/include)
target_include_directories(${PROJECT_NAME}  PUBLIC lib/alertInfection/include)
target_include_directories(${PROJECT_NAME}  PUBLIC lib/emergNotif/include)
target_include_directories(tcp_client PUBLIC lib/cJSON/include)
target_include_directories(udp_client PUBLIC lib/cJSON/include)

target_link_libraries(${PROJECT_NAME} socketSetup cJSON suppliesDataModule alertInfectionModule emergencyNotification)
target_link_libraries(tcp_client socketSetup cJSON)
target_link_libraries(udp_client socketSetup cJSON)

# Add subdirectory of tests
if(RUN_TESTS EQUAL 1 OR RUN_COVERAGE EQUAL 1)
# See https://cmake.org/cmake/help/book/mastering-cmake/chapter/Testing%20With%20CMake%20and%20CTest.html
    add_subdirectory(tests)
endif()
