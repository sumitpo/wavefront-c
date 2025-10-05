# tests/CMakeLists.txt Check for CMocka
FIND_PACKAGE(cmocka CONFIG QUIET)

SET(TARGET_TEST wavefront-test)
IF(NOT cmocka_FOUND)
  MESSAGE(WARNING "CMocka not found, skipping tests")
  RETURN()
ENDIF()

ENABLE_TESTING()

# Create test executable
ADD_EXECUTABLE(${TARGET_TEST} test_wavefront.c)

# Link libraries
TARGET_LINK_LIBRARIES(${TARGET_TEST} PRIVATE cmocka::cmocka wavefront-parser
                                             log4c::log4c)

# Include directories
TARGET_INCLUDE_DIRECTORIES(${TARGET_TEST}
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

IF(ENABLE_ASAN)
  TARGET_COMPILE_OPTIONS(${TARGET_TEST} PRIVATE -fsanitize=address
                                                -fno-omit-frame-pointer -g)
  TARGET_LINK_OPTIONS(${TARGET_TEST} PRIVATE -fsanitize=address)
ENDIF()

# Copy test data
FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test_data/
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/test_data)

# Add test
ADD_TEST(NAME ${TARGET_TEST} COMMAND ${TARGET_TEST})

# ASan support for tests
IF(ENABLE_ASAN)
  TARGET_COMPILE_OPTIONS(${TARGET_TEST} PRIVATE -fsanitize=address
                                                -fno-omit-frame-pointer -g)
  TARGET_LINK_OPTIONS(${TARGET_TEST} PRIVATE -fsanitize=address)
ENDIF()

INSTALL(
  TARGETS ${TARGET_TEST}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
