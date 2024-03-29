#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
//#include <glm/gtc/type_ptr.hpp>

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

void main() {
    gl_Position = vec4(aPos, 1.0);
}
)";

const std::string tcs = R"(
#version 400 core

layout (vertices = ${OUTPUT_PATCH_VERTICES}) out;

uniform float outer_level;
uniform float inner_level;

void main() {
    // pass attributes through
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;

    // invocation 0 controls tessellation levels for the entire patch
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

layout (isolines, fractional_odd_spacing, ccw) in;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    // get patch coordinate
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    // retrieve control point position
    vec4 p00 = gl_in[0].gl_Position;
    vec4 p01 = gl_in[1].gl_Position;
    vec4 p10 = gl_in[2].gl_Position;
    vec4 p11 = gl_in[3].gl_Position;

    // bi-linearly interpolate position across patches
    vec4 p0 = (p01 - p00) * u + p00;
    vec4 p1 = (p11 - p10) * u + p10;

    vec4 p = (p1 - p0) * v + p0;

    gl_Position = projection * view * model * p;
}
)";

const std::string fragment_source = R"(
#version 400 core
out vec4 FragColor;

void main()
{
    FragColor = vec4(0.0, 1.0, 0.0, 1.0);
}
)";


int main(int argc, char **argv)
{
    trif::Option outer_level = {"-o,--outer-level", "Set all outer tessellation levels of the current patch"};
    trif::Option inner_level = {"-i,--inner-level", "Set all inner tessellation levels of the current patch"};
    trif::Option output_patch_vertices = { "-v, --patch-vertices", "Set output patch vertices count ([1, 32])"};

    trif::Application app("tess", argc, argv, {&outer_level, &inner_level, &output_patch_vertices});
    trif::Config config = app.getConfig();

    const uint32_t win_w = config.window_size.first;
    const uint32_t win_h = config.window_size.second;

    uint32_t frames = config.n_frames;
    float ol = app.get_option_value<float>(&outer_level, 8.0f);
    float il = app.get_option_value<float>(&inner_level, 8.0f);
    std::string patch_vertices = app.get_option_value<std::string>(&output_patch_vertices, "4");

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

    // configure global opengl state
    // -----------------------------
    // Some implementation does not support PolygonMode setting
//    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // build and compile shaders
    // -------------------------
//    Shader shader("11.1.anti_aliasing.vs", "11.1.anti_aliasing.fs");

    /// Preprocess TCS
    trif::ShaderSourceTemplate::ParamsType tcs_params;
    tcs_params["OUTPUT_PATCH_VERTICES"] = patch_vertices;

    trif::Program<
        trif::Shaders<GL_VERTEX_SHADER>,
        trif::Shaders<GL_TESS_CONTROL_SHADER>,
        trif::Shaders<GL_TESS_EVALUATION_SHADER>,
        trif::Shaders<GL_FRAGMENT_SHADER>
    > program(
        vertex_source,
        trif::ShaderSourceTemplate(tcs).specialize(tcs_params),
        tes,
        fragment_source
    );

    // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------
    float cubeVertices[] = {
            // positions
            -0.5f, -0.5f, -0.5f,
            0.5f, -0.5f, -0.5f,
            0.5f,  0.5f, -0.5f,
            0.5f,  0.5f, -0.5f,
            -0.5f,  0.5f, -0.5f,
            -0.5f, -0.5f, -0.5f,

            -0.5f, -0.5f,  0.5f,
            0.5f, -0.5f,  0.5f,
            0.5f,  0.5f,  0.5f,
            0.5f,  0.5f,  0.5f,
            -0.5f,  0.5f,  0.5f,
            -0.5f, -0.5f,  0.5f,

            -0.5f,  0.5f,  0.5f,
            -0.5f,  0.5f, -0.5f,
            -0.5f, -0.5f, -0.5f,
            -0.5f, -0.5f, -0.5f,
            -0.5f, -0.5f,  0.5f,
            -0.5f,  0.5f,  0.5f,

            0.5f,  0.5f,  0.5f,
            0.5f,  0.5f, -0.5f,
            0.5f, -0.5f, -0.5f,
            0.5f, -0.5f, -0.5f,
            0.5f, -0.5f,  0.5f,
            0.5f,  0.5f,  0.5f,

            -0.5f, -0.5f, -0.5f,
            0.5f, -0.5f, -0.5f,
            0.5f, -0.5f,  0.5f,
            0.5f, -0.5f,  0.5f,
            -0.5f, -0.5f,  0.5f,
            -0.5f, -0.5f, -0.5f,

            -0.5f,  0.5f, -0.5f,
            0.5f,  0.5f, -0.5f,
            0.5f,  0.5f,  0.5f,
            0.5f,  0.5f,  0.5f,
            -0.5f,  0.5f,  0.5f,
            -0.5f,  0.5f, -0.5f
    };
    // setup cube VAO
    unsigned int cubeVAO, cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), &cubeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    // camera
    //Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
    glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    float cameraYaw = -100.0f;
    float cameraPitch = -10.0f;
    glm::vec3 cameraPosition = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 front;
    front.x = cos(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
    front.y = sin(glm::radians(cameraPitch));
    front.z = sin(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
    glm::vec3 cameraFront = glm::normalize(front);

    glm::vec3 cameraRight = glm::normalize(glm::cross(cameraFront, worldUp));
    glm::vec3 cameraUp = glm::normalize(glm::cross(cameraRight, cameraFront));

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
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // set transformation matrices
        glm::mat4 projection = glm::perspective(glm::radians(45.0f),
                                                (float)win_w / (float)win_h,
                                                0.1f,
                                                1000.0f);
        glm::mat4 view = glm::lookAt(cameraPosition,
                                     cameraPosition + cameraFront,
                                     cameraUp);

        program.use();
        program.uniform("outer_level", ol);
        program.uniform("inner_level", il);
        program.uniform("projection", projection);
        program.uniform("view", view);
        program.uniform("model", glm::mat4(1.0f));

        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_PATCHES, 0, 36);
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
