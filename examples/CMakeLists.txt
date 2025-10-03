# examples/CMakeLists.txt
ADD_EXECUTABLE(wavefront-example loader.c)
TARGET_LINK_LIBRARIES(wavefront-example PRIVATE wavefront-parser)

# ASan for example (if enabled in parent)
IF(ENABLE_ASAN)
  TARGET_COMPILE_OPTIONS(wavefront-example PRIVATE -fsanitize=address
                                                   -fno-omit-frame-pointer -g)
  TARGET_LINK_OPTIONS(wavefront-example PRIVATE -fsanitize=address)
ENDIF()
