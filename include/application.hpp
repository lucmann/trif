//
// Created by luc on 2023/1/16.
//

#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "CLI11.hpp"

// #include "parser.hpp"
#include "shader.hpp"

using namespace CLI;

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

namespace trif
{
struct Config {
    int n_frames{-1};
    std::pair<int, int> window_size{800, 600};
    // TODO: add other common config as default
};

class Application : CLI::App {
public:
    // Allow client to customize other options
    Application() : CLI::App("trif") {
        add_option("-n", configs.n_frames, "Draw the given number of frames then exit");
        add_option("-g, --geometry", configs.window_size, "Specify the size of window as WxH (default 800x600)");
    }

    ~Application() {
        glfwTerminate();
    }

    void init(int argc, const char *argv[]) {
        // Parse the command line arguments
        try {
            parse((argc), (argv));
        } catch(const CLI::ParseError &e) {
            exit(e);
            std::exit(1);
        }

        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            std::exit(2);
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        window = glfwCreateWindow(800, 600, "Render to Texture", NULL, NULL);
        if (!window) {
            std::cerr << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            std::exit(2);
        }

        glfwMakeContextCurrent(window);
        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

        glfwMakeContextCurrent(window);
        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK) {
            std::cerr << "Failed to initialize GLEW" << std::endl;
            std::exit(2);
        }
    }

    void main_loop(std::function<void(void)> render) {
        while (!glfwWindowShouldClose(window)) {
            processInput(window);

            render();

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

private:
    // Parsed from default options. Application is resposible for providing variables to bind to
    // and to use on their own.
    Config configs;
    GLFWwindow* window;
};
}
