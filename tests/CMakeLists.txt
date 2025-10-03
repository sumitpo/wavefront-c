# tests/CMakeLists.txt Check for CMocka
FIND_PACKAGE(cmocka CONFIG QUIET)

IF(cmocka_FOUND)
  ENABLE_TESTING()

  # Create test executable
  ADD_EXECUTABLE(wavefront-test test_wavefront.c)

  # Link libraries
  TARGET_LINK_LIBRARIES(wavefront-test PRIVATE cmocka::cmocka wavefront-parser)

  # Include directories
  TARGET_INCLUDE_DIRECTORIES(wavefront-test
                             PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

  IF(ENABLE_ASAN)
    TARGET_COMPILE_OPTIONS(wavefront-test PRIVATE -fsanitize=address
                                                  -fno-omit-frame-pointer -g)
    TARGET_LINK_OPTIONS(wavefront-test PRIVATE -fsanitize=address)
  ENDIF()

  # Copy test data
  FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test_data/
       DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/test_data)

  # Add test
  ADD_TEST(NAME wavefront-test COMMAND wavefront-test)

  # ASan support for tests
  IF(ENABLE_ASAN)
    TARGET_COMPILE_OPTIONS(wavefront-test PRIVATE -fsanitize=address
                                                  -fno-omit-frame-pointer -g)
    TARGET_LINK_OPTIONS(wavefront-test PRIVATE -fsanitize=address)
  ENDIF()

ELSE()
  MESSAGE(WARNING "CMocka not found, skipping tests")
ENDIF()
