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

static const std::string square_vs_source = R"(
    #version 410 core                                                               
                                                                                    
    layout (location = 0) in vec4 position;                                         
    layout (location = 1) in vec4 instance_color;                                   
    layout (location = 2) in vec4 instance_position;                                

    uniform vec2 divisors;
                                                                                    
    out Fragment                                                                    
    {                                                                               
        vec4 color;                                                                 
    } fragment;                                                                     
                                                                                    
    void main(void)                                                                 
    {                                                                               
        gl_Position = (position + instance_position) * vec4(1.0/divisors.x, 1.0/divisors.y, 1.0, 1.0);
        fragment.color = instance_color;                                            
    }                                                                               
)";

static const std::string square_fs_source = R"(
    #version 410 core                                                                
    precision highp float;                                                           
                                                                                     
    in Fragment                                                                      
    {                                                                                
        vec4 color;                                                                  
    } fragment;                                                                      
                                                                                     
    out vec4 color;                                                                  
                                                                                     
    void main(void)                                                                  
    {                                                                                
       color = fragment.color;                                                      
    }                                                                                
)";

struct DrawElementsIndirectCommand {
    GLuint count;
    GLuint primCount;
    GLuint firstIndex;
    GLint  baseVertex;
    GLuint baseInstance;
};

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

    std::pair<uint32_t, uint32_t> board_sz = app.get_option_value<std::pair<uint32_t, uint32_t>>(&board_size, std::make_pair(8, 8));

    const uint32_t BOARD_WIDTH = board_sz.first;
    const uint32_t BOARD_HEIGHT = board_sz.second;
    const uint32_t NUM_DRAWS = BOARD_WIDTH * BOARD_HEIGHT;

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
        square_vs_source, square_fs_source
    );

    // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------

    GLuint indirect_draw_buffer;
    GLuint draw_index_buffer;
    GLuint square_buffer;
    GLuint square_vao;

    static const GLfloat square_vertices[] =
    {
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 0.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f
    };

    static const GLushort square_indices[] =
    {
        0, 1, 2, 3
    };

    const glm::vec4 WHITE = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    const glm::vec4 BLACK = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    std::vector<glm::vec4> instance_colors;

    for (int i = 0; i < BOARD_HEIGHT; ++i) {
        for (int j = 0; j < BOARD_WIDTH; ++j) {
            if ((i % 2) == 0)
                if ((j % 2) == 0) 
                    instance_colors.emplace_back(WHITE);
                else
                    instance_colors.emplace_back(BLACK);
            else
                if ((j % 2) == 0) 
                    instance_colors.emplace_back(BLACK);
                else
                    instance_colors.emplace_back(WHITE);
        }
    }
    
    std::vector<glm::vec4> instance_positions;
    const int half_w = BOARD_WIDTH / 2;
    const int half_h = BOARD_HEIGHT / 2;

    for (int i = -half_h; i < half_h; ++i) {
        for (int j = -half_w; j < half_w; ++j) {
            instance_positions.emplace_back(
                glm::vec4(
                  j * 2.0f + 1.0f,
                  i * 2.0f + 1.0f,
                  0.0f,
                  0.0f
                )
            );
        }
    }

    int i;
    GLuint offset = 0;

    // Indirect params
    glGenBuffers(1, &indirect_draw_buffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect_draw_buffer);
    glBufferData(GL_DRAW_INDIRECT_BUFFER,
             NUM_DRAWS * sizeof(DrawElementsIndirectCommand),
             NULL,
             GL_STATIC_DRAW);

    DrawElementsIndirectCommand * cmd = (DrawElementsIndirectCommand *)
    glMapBufferRange(GL_DRAW_INDIRECT_BUFFER,
                     0,
                     NUM_DRAWS * sizeof(DrawElementsIndirectCommand),
                     GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

    for (i = 0; i < NUM_DRAWS; ++i) {
        cmd[i].count = 4;
        cmd[i].primCount = 1;
        cmd[i].firstIndex = 0;
        cmd[i].baseVertex = 0;
        cmd[i].baseInstance = i;
    }

    glUnmapBuffer(GL_DRAW_INDIRECT_BUFFER);

    // Indices
    glGenBuffers(1, &draw_index_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, draw_index_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(square_indices), square_indices, GL_STATIC_DRAW);

    glGenVertexArrays(1, &square_vao);
    glGenBuffers(1, &square_buffer);
    glBindVertexArray(square_vao);
    glBindBuffer(GL_ARRAY_BUFFER, square_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(square_vertices) + sizeof(glm::vec4) * instance_colors.size() + sizeof(glm::vec4) * instance_positions
        .size(), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, offset, sizeof(square_vertices), square_vertices);
    offset += sizeof(square_vertices);
    glBufferSubData(GL_ARRAY_BUFFER, offset, sizeof(glm::vec4) * instance_colors.size(), instance_colors.data());
    offset += sizeof(glm::vec4) * instance_colors.size();
    glBufferSubData(GL_ARRAY_BUFFER, offset, sizeof(glm::vec4) * instance_positions.size(), instance_positions.data());
    offset += sizeof(glm::vec4) * instance_positions.size();

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, (GLvoid *)sizeof(square_vertices));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 0, (GLvoid *)(sizeof(square_vertices) + sizeof(glm::vec4) * instance_colors.size()));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    glVertexAttribDivisor(1, 1);
    glVertexAttribDivisor(2, 1);

    trif::Config conf = app.getConfig();
    uint32_t frames = conf.n_frames;

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    program.use();
    program.uniform("divisors", glm::vec2(BOARD_WIDTH, BOARD_HEIGHT));

    glBindVertexArray(square_vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, draw_index_buffer);

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
        for (int i = 0; i < NUM_DRAWS; ++i)
            glDrawElementsIndirect(GL_TRIANGLE_FAN, GL_UNSIGNED_SHORT,
                    (void *)(i * sizeof(DrawElementsIndirectCommand)));

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
