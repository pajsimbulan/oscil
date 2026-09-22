cmake_minimum_required(VERSION 3.16)
project(oscil_host_tests C)

include(FetchContent)
FetchContent_Declare(unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG v2.6.0)
FetchContent_MakeAvailable(unity)

set(SHARED ${CMAKE_CURRENT_LIST_DIR}/../shared)
enable_testing()

#one executable per module.  Add a block like this for each new test file.
add_executable(test_afe test_afe.c ${SHARED}/oscil_afe.c)
target_include_directories(test_afe PRIVATE ${SHARED})
target_link_libraries(test_afe PRIVATE unity)
if(NOT MSVC)
    target_link_libraries(test_afe PRIVATE m)
endif()
add_test(NAME afe COMMAND test_afe)