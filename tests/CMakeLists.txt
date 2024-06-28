# Request the minimum version of CMake, in case of lower version throws error.
# See #https://cmake.org/cmake/help/latest/command/cmake_minimum_required.html

cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

# Include headers project
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

# Collect tests
file(GLOB TESTS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_server.c)
file(GLOB SRC_FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/server/server.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/server/server_mocks.c
) 

# Link with Unity header
target_link_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/external/Unity/src)

# Coverage (Optional)
if(RUN_COVERAGE)
    message("Coverage enabled")

    find_program(LCOV_PATH lcov)
    find_program(GENHTML_PATH genhtml)
    if(NOT LCOV_PATH)
        message(FATAL_ERROR "lcov not found!")
    endif()

    find_program(GCOV_PATH gcov)
    if(NOT GCOV_PATH)
        message(FATAL_ERROR "gcov not found!")
    endif() 

    find_program(GENHTML_PATH genhtml)
    if(NOT GENHTML_PATH)
        message(FATAL_ERROR "genhtml not found!")
    endif() 

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lgcov --coverage")
endif()

# Create test executable
add_executable(test_${PROJECT_NAME} ${TESTS_FILES} ${SRC_FILES})

# Link with Unity
target_link_libraries(test_${PROJECT_NAME} unity socketSetup cJSON suppliesDataModule alertInfectionModule emergencyNotification)


# Add test
# See https://cmake.org/cmake/help/latest/command/add_test.html
add_test(NAME test_${PROJECT_NAME} COMMAND test_${PROJECT_NAME})
