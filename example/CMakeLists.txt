cmake_minimum_required(VERSION 3.3)
project(example LANGUAGES C CXX)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

message(${PROJECT_NAME} " build type: " ${CMAKE_BUILD_TYPE})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/../bin)

add_compile_definitions(ASSETS_DIR=\"${CMAKE_SOURCE_DIR}/assets/\")

macro(example DIR APP)
  # Allow to multiple examples in one directory but note that one example
  # corresponds to one source file only
  file(GLOB ${APP}_SRC
    "${DIR}/*.h"
    "${DIR}/${APP}.cpp"
  )

  add_executable(${APP} ${${APP}_SRC})

  target_compile_definitions(${APP} PRIVATE GL_GLEXT_PROTOTYPES)
  target_include_directories(${APP} PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_link_libraries(${APP} PRIVATE glfw GL)
endmacro(example)

example(gears glxgears)
example(msaa msaa)
example(texture texture_wrap)
example(tessellation tess)
example(tessellation tess_gs)
example(geometry checkerboard_gs)
example(instanced brickwall)
example(triangle triangle)
example(triangle tri_gs)
example(indirect checkerboard)
# Add new example from here
