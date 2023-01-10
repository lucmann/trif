set(SRC
    msaa.cpp
)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/../bin)

string(REPLACE ".cpp" "" TARGET_NAME ${SRC})
add_executable(${TARGET_NAME} ${SRC})

target_compile_definitions(${TARGET_NAME} PRIVATE GL_GLEXT_PROTOTYPES)
target_link_libraries(${TARGET_NAME} PRIVATE glfw GL)