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
    #version 410 core                                                               
                                                                                    
    layout (location = 0) in vec4 aPos;                                         
    layout (location = 0) in vec4 aColor;                                         

    out vec4 color; 

    void main(void)                                                                 
    {                                                                               
        gl_Position = aPos;
        color = aColor;
    }                                                                               
)";

static const std::string checkerboard_gs_source = R"(
    #version 410 core                                                               
                                                                                    
    layout (points) in;                                         
    layout (triangle_strip, max_vertices = 256) out;                                   

    in vec4 color[];

    out vec4 fColor;

    uniform vec2 size;
    uniform int rows;
    uniform int cols;
    uniform vec4 color1;
    uniform vec4 color2;
                                                                                    
    void main(void)                                                                 
    {                                                                               
        vec2 quadSize = vec2(size.x/rows, size.y/cols);
        vec2 quadPos = gl_in[0].gl_Position.xy;
        fColor = color[0];

        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                fColor = mod(float(row + col), 2.0) == 0.0 ? color1 : color2;

                gl_Position = vec4(quadPos + vec2(quadSize.x * col, quadSize.y * row), 0.0, 1.0);
                EmitVertex();

                gl_Position = vec4(quadPos + vec2(quadSize.x * (col + 1), quadSize.y * row), 0.0, 1.0);
                EmitVertex();

                gl_Position = vec4(quadPos + vec2(quadSize.x * col, quadSize.y * (row + 1)), 0.0, 1.0);
                EmitVertex();

                gl_Position = vec4(quadPos + vec2(quadSize.x * (col + 1), quadSize.y * (row + 1)), 0.0, 1.0);
                EmitVertex();

                EndPrimitive();
            }
        }
    }                                                                               
)";

static const std::string checkerboard_fs_source = R"(
    #version 410 core                                                                
    precision highp float;                                                           
    
    in vec4 fColor;
    out vec4 Color;                                                                  
                                                                                     
    void main(void)                                                                  
    {                                                                                
       Color = fColor;
    }                                                                                
)";


int main(int argc, char **argv)
{
    trif::Option board_size = {
        "-s,--size", "Specify the width and height of checkerboard as WxH (default: 8x8)",
        trif::OptionType::Pair
    };

    trif::Application app("checkerboard", argc, argv, {&board_size});
    trif::Config config = app.getConfig();

    const uint32_t win_w = config.window_size.first;
    const uint32_t win_h = config.window_size.second;

    std::pair<int, int> board_sz = app.get_option_value<std::pair<int, int>>(&board_size, std::make_pair(8, 8));

    const int BOARD_WIDTH = board_sz.first;
    const int BOARD_HEIGHT = board_sz.second;

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
        trif::Shaders<GL_GEOMETRY_SHADER>,
        trif::Shaders<GL_FRAGMENT_SHADER>
    > program(
        checkerboard_vs_source,
        checkerboard_gs_source,
        checkerboard_fs_source
    );

    // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------

    GLuint square_buffer;
    GLuint square_vao;

    static const GLfloat square_vertices[] =
    {
        -1.0f, -1.0f, 0.0f, 1.0f,
    };

    static const GLfloat square_color[] =
    {
         1.0f,  1.0f, 1.0f, 1.0f,
    };

    const glm::vec4 RED = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    const glm::vec4 GREEN = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);


    glGenVertexArrays(1, &square_vao);
    glGenBuffers(1, &square_buffer);
    glBindVertexArray(square_vao);
    glBindBuffer(GL_ARRAY_BUFFER, square_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(square_vertices) + sizeof(square_color), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(square_vertices), square_vertices);
    glBufferSubData(GL_ARRAY_BUFFER, sizeof(square_vertices), sizeof(square_color), square_color);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, 0);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);


    trif::Config conf = app.getConfig();
    uint32_t frames = conf.n_frames;

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    program.use();
    program.uniform("size", glm::vec2(2.0, 2.0));
    program.uniform("rows", BOARD_HEIGHT);
    program.uniform("cols", BOARD_WIDTH);
    program.uniform("color1", RED);
    program.uniform("color2", GREEN);

    glBindVertexArray(square_vao);

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
        glDrawArrays(GL_POINTS, 0, 1);

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
