#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include "application.hpp"


void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

float lastX = (float)SCR_WIDTH / 2.0;
float lastY = (float)SCR_HEIGHT / 2.0;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

const std::string vertex_source = R"(
#version 400 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aCol;

out vec4 col;

void main()
{
    gl_Position = vec4(aPos, 1.0);
    col = vec4(aCol, 1.0);
}
)";

const std::string tcs = R"(
#version 400 core

layout (vertices = ${OUTPUT_PATCH_VERTICES}) out;

uniform float outer_level;
uniform float inner_level;

in vec4 col[];
out vec4 Color[];

void main() {
    // pass attributes through
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    Color[gl_InvocationID] = col[gl_InvocationID];

    // invocation 0 controls tessellation levels for the entire patch
    //
    // As for different abstract primitives, different subsets of tessellation
    // level parameters are involved.
    //
    // 'triangles':
    //   gl_TessLevelInner[0]
    //   gl_TessLevelOuter[0]
    //   gl_TessLevelOuter[1]
    //   gl_TessLevelOuter[2]
    //
    // 'quads'
    //   gl_TessLevelInner[0]
    //   gl_TessLevelInner[0]
    //   gl_TessLevelOuter[0]
    //   gl_TessLevelOuter[1]
    //   gl_TessLevelOuter[2]
    //   gl_TessLevelOuter[3]
    //
    // 'isolines'
    //   gl_TessLevelOuter[0]
    //   gl_TessLevelOuter[1]

    if (gl_InvocationID == 0) {
        gl_TessLevelOuter[0] = outer_level;
        gl_TessLevelOuter[1] = outer_level;
        gl_TessLevelOuter[2] = outer_level;
        gl_TessLevelOuter[3] = outer_level;

        gl_TessLevelInner[0] = inner_level;
        gl_TessLevelInner[1] = inner_level;
    }
}
)";

const std::string tes = R"(
#version 400 core

layout (triangles, fractional_odd_spacing, ccw) in;

// received from TCS
in vec4 Color[];
out vec4 vColor;

void main() {
    // get patch coordinate
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;
    float w = gl_TessCoord.z;

    vColor = u * Color[0] + v * Color[1] + w * Color[2];
    // retrieve control point position
    vec4 p00 = gl_in[0].gl_Position;
    vec4 p01 = gl_in[1].gl_Position;
    vec4 p10 = gl_in[2].gl_Position;

    gl_Position = u * p00 + v * p01 + w * p10;
}
)";

/// As glPolygonMode() is not supported in GLES
/// So we use a geometry shader to draw a wireframe
const std::string geometry_source = R"(
#version 400 core

layout (triangles) in;
layout (line_strip, max_vertices = 4) out;

in vec4 vColor[];
out vec4 fColor;

void main() {
    gl_Position = gl_in[0].gl_Position;
    fColor = vColor[0];
    EmitVertex();

    gl_Position = gl_in[1].gl_Position;
    fColor = vColor[1];
    EmitVertex();

    gl_Position = gl_in[2].gl_Position;
    fColor = vColor[2];
    EmitVertex();

    gl_Position = gl_in[0].gl_Position;
    fColor = vColor[0];
    EmitVertex();

    EndPrimitive();
}
)";

const std::string fragment_source_wireframe = R"(
#version 400 core

in vec4 fColor;
out vec4 FragColor;

void main()
{
    FragColor = fColor;
}
)";

const std::string fragment_source_normal = R"(
#version 400 core

in vec4 vColor;
out vec4 FragColor;

void main()
{
    FragColor = vColor;
}
)";


int main(int argc, char **argv)
{
    trif::Option outer_level = {"-o,--outer-level", "Set all outer tessellation levels of the current patch"};
    trif::Option inner_level = {"-i,--inner-level", "Set all inner tessellation levels of the current patch"};
    trif::Option output_patch_vertices = { "-v, --patch-vertices", "Set output patch vertices count ([1, 32])"};
    trif::Option wireframe = {"-w,--wireframe",
                              "Set the wireframe implementation [standard,geometry,none] (default standard)"};

    trif::Application app("tess_es", argc, argv,
                          {&outer_level, &inner_level, &output_patch_vertices, &wireframe});
    trif::Config config = app.getConfig();

    const uint32_t win_w = config.window_size.first;
    const uint32_t win_h = config.window_size.second;
    uint32_t frames = config.n_frames;

    /// User-defined command-line options
    float ol = app.get_option_value<float>(&outer_level, 2.0f);
    float il = app.get_option_value<float>(&inner_level, 3.0f);
    std::string patch_vertices = app.get_option_value<std::string>(&output_patch_vertices, "3");
    std::string draw_wireframe = app.get_option_value<std::string>(&wireframe, "standard");

    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(win_w, win_h, "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    /// Preprocess TCS
    trif::ShaderSourceTemplate::ParamsType tcs_params;
    tcs_params["OUTPUT_PATCH_VERTICES"] = patch_vertices;

    trif::Program<
        trif::Shaders<GL_VERTEX_SHADER>,
        trif::Shaders<GL_TESS_CONTROL_SHADER>,
        trif::Shaders<GL_TESS_EVALUATION_SHADER>,
        trif::Shaders<GL_GEOMETRY_SHADER>,
        trif::Shaders<GL_FRAGMENT_SHADER>
    > program_wireframe(
        vertex_source,
        trif::ShaderSourceTemplate(tcs).specialize(tcs_params),
        tes,
        geometry_source,
        fragment_source_wireframe
    );

    trif::Program<
            trif::Shaders<GL_VERTEX_SHADER>,
            trif::Shaders<GL_TESS_CONTROL_SHADER>,
            trif::Shaders<GL_TESS_EVALUATION_SHADER>,
            trif::Shaders<GL_FRAGMENT_SHADER>
    > program_normal(
            vertex_source,
            trif::ShaderSourceTemplate(tcs).specialize(tcs_params),
            tes,
            fragment_source_normal
    );

    // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------
    float triVertAttributes[] = {
        // positions            // color
         0.0f,  0.5f, 0.0f,    1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, 0.0f,    0.0f, 1.0f, 0.0f,
         0.5f, -0.5f, 0.0f,    0.0f, 0.0f, 1.0f,
    };

    // setup cube VAO
    unsigned int cubeVAO, cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(triVertAttributes), &triVertAttributes, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

    // Set rasterization mode to LINES
    if (draw_wireframe == "standard") {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window) && frames)
    {
        // per-frame time logic
        // --------------------
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        // -----
        processInput(window);

        // render
        // ------
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // In some OpenGL implementation, standard glPolygonMode() is not supported
        // we try to draw in line mode with a geometry shader which transforms a triangle
        // to three lines.
        if (draw_wireframe == "geometry") {
            program_wireframe.use();
            program_wireframe.uniform("outer_level", ol);
            program_wireframe.uniform("inner_level", il);
        } else {
            program_normal.use();
            program_normal.uniform("outer_level", ol);
            program_normal.uniform("inner_level", il);
        }

        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_PATCHES, 0, 3);
        glBindVertexArray(0);

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();

        if (!config.forever)
            frames--;
    }

    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

//    camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
//    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}
