add_executable(test_correctness test_correctness.cpp)

# Link against GoogleTest
target_link_libraries(test_correctness gtest_main)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(test_correctness)