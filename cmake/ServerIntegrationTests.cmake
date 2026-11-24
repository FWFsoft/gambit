# Requires an externally managed server on localhost:1234. Label selection
# does not enable disabled tests: opt in at configure time instead.
option(GAMBIT_ENABLE_SERVER_INTEGRATION_TESTS
       "Register tests requiring a running server on localhost:1234" OFF)

if(GAMBIT_ENABLE_SERVER_INTEGRATION_TESTS)
    add_test(NAME HeadlessMovement COMMAND test_headless_movement)
    set_tests_properties(HeadlessMovement PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        LABELS "integration;manual"
    )
endif()
