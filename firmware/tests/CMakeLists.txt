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

add_executable(test_split test_split.c)
target_include_directories(test_split PRIVATE ${SHARED})
target_link_libraries(test_split PRIVATE unity)
add_test(NAME split COMMAND test_split)

add_executable(test_trigger test_trigger.c ${SHARED}/oscil_trigger.c)
target_include_directories(test_trigger PRIVATE ${SHARED})
target_link_libraries(test_trigger PRIVATE unity)
if(NOT MSVC)
    target_link_libraries(test_trigger PRIVATE m)
endif()
add_test(NAME trigger COMMAND test_trigger)

add_executable(test_decimate test_decimate.c ${SHARED}/oscil_decimate.c)
target_include_directories(test_decimate PRIVATE ${SHARED})
target_link_libraries(test_decimate PRIVATE unity)
add_test(NAME decimate COMMAND test_decimate)

add_executable(test_link test_link.c ${SHARED}/oscil_link.c)
target_include_directories(test_link PRIVATE ${SHARED})
target_link_libraries(test_link PRIVATE unity)
add_test(NAME link COMMAND test_link)