set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Test)

add_executable(test_winman test_winman.cpp)
target_link_libraries(test_winman PRIVATE Qt5::Test winman)

set_target_properties(test_winman PROPERTIES
    DEBUG_POSTFIX d
)

add_test(NAME test_winman COMMAND test_winman
    WORKING_DIRECTORY $<TARGET_FILE_DIR:test_winman>)
