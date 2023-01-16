cmake_minimum_required(VERSION 3.3)
project(example LANGUAGES C CXX)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/../bin)

add_compile_definitions(ASSETS_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/assets/\")

macro(example APP DIR)
  # Allow to multiple examples in one directory but note that the corresponding
  # source files must prefix with APP so that CMake can match them exclusively
  file(GLOB ${APP}_SRC
    "${DIR}/*.h"
    "${DIR}/${APP}*.cpp"
  )

  add_executable(${APP} ${${APP}_SRC})

  target_compile_definitions(${APP} PRIVATE GL_GLEXT_PROTOTYPES)
  target_include_directories(${APP} PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_link_libraries(${APP} PRIVATE glfw GL)
endmacro(example)

example(msaa msaa)
example(texture_wrap texture)
# Add new example from here
