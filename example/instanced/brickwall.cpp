#include <GLFW/glfw3.h>
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

static const std::string checkerboard_vs_source = R"(
    #version 330 core                                                               
                                                                                    
    layout (location = 0) in vec2 aPos;                                         
    layout (location = 1) in vec3 aColor;                                         
    layout (location = 2) in vec2 aOffset;

    out vec3 fColor; 

    void main(void)                                                                 
    {                                                                               
        gl_Position = vec4(aPos + aOffset, 0.0, 1.0);
        fColor = aColor;
    }                                                                               
)";

static const std::string checkerboard_fs_source = R"(
    #version 330 core                                                                
    
    in vec3 fColor;
    out vec4 Color;                                                                  
                                                                                     
    void main(void)                                                                  
    {                                                                                
       Color = vec4(fColor, 1.0);
    }                                                                                
)";

#define BRICK_SIZE 16

int main(int argc, char **argv)
{
    trif::Application app("brickwall", argc, argv);
    trif::Config config = app.getConfig();

    const uint32_t win_w = config.window_size.first;
    const uint32_t win_h = config.window_size.second;

    const int cols = win_w / BRICK_SIZE;
    const int rows = win_h / BRICK_SIZE;

    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
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

    trif::Program<
        trif::Shaders<GL_VERTEX_SHADER>,
        trif::Shaders<GL_FRAGMENT_SHADER>
    > program(
        checkerboard_vs_source,
        checkerboard_fs_source
    );

    glm::vec2* translations = new glm::vec2[cols * rows];

    glm::vec2 stride = glm::vec2(2.0/cols, 2.0/rows);
    int index = 0;
    for (int y = 0; y < rows; ++y)
        for (int x = 0; x < cols; ++x) {
            glm::vec2 translation;
            translation.x = x * stride.x;
            translation.y = y * stride.y;

            translations[index++] = translation;
        }

    GLuint instanceVBO;

    glGenBuffers(1, &instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2) * cols * rows, translations, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    static GLfloat quadVertices[] =
    {
        // right-bottom
        -1.0f, -1.0f, 1.0f, 0.0f, 0.0f,
         0.0f,  0.0f, 0.0f, 1.0f, 0.0f,
         0.0f,  0.0f, 0.0f, 0.0f, 1.0f,

        // left-top
         0.0f,  0.0f, 0.0f, 0.0f, 1.0f,
         0.0f,  0.0f, 0.0f, 1.0f, 0.0f,
        -1.0f, -1.0f, 1.0f, 0.0f, 0.0f,
    };

    GLfloat _x = -1.0 + 2.0 / cols;
    GLfloat _y = -1.0 + 2.0 / rows;

    quadVertices[5]  = _x;      quadVertices[6]  = -1.0f;
    quadVertices[10] = _x;      quadVertices[11] = _y;
    quadVertices[15] = _x;      quadVertices[16] = _y;
    quadVertices[20] = -1.0f;   quadVertices[21]  = _y;

    // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------
    GLuint quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);

    // Color attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(2 * sizeof(float)));

    // per instance attribute
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Tell OpenGL this is an instanced vertex attribute
    glVertexAttribDivisor(2, 1);


    trif::Config conf = app.getConfig();
    uint32_t frames = conf.n_frames;

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    program.use();

    glBindVertexArray(quadVAO);

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
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, cols * rows * 2);

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();

        if (!conf.forever)
            frames--;
    }

    glBindVertexArray(0);

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
