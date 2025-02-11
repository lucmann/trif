//
// Created by luc on 2023/1/16.
//

#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "CLI11.hpp"

#include "shader.hpp"

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
    std::string title{"Demo"};
    int frames{-1};
    std::pair<int, int> window_size{800, 600};
    // TODO: add other common config as default
};

class Application : public CLI::App {
public:
    // Allow client to customize other options
    Application(const char *title = "") : CLI::App(title) {
        add_option("-n,--frames", config.frames, "Draw the given number of frames then exit");
        add_option("-g,--geometry", config.window_size, "Specify the size of window like -g NNNxMMM (default 800x600)")
                ->delimiter('x');

        config.title = title;
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

        std::cout << "Window size: " << config.window_size.first << "x"
                                     << config.window_size.second << std::endl;
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            std::exit(2);
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        window = glfwCreateWindow(config.window_size.first, config.window_size.second,
                                  config.title.c_str(), NULL, NULL);
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
        glViewport(0, 0, config.window_size.first, config.window_size.second);

        while (!glfwWindowShouldClose(window) &&
                (config.frames < 0 || config.frames--)) {
            processInput(window);

            render();

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

    // No SwapBuffers version in case that an application may render to non-default fbo
    // std::function<> doesn't support default parameter, we have to provide
    // two versions of main_loop
    void main_loop(std::function<void(bool)> render) {
        glViewport(0, 0, config.window_size.first, config.window_size.second);

        while (!glfwWindowShouldClose(window) &&
                (config.frames < 0 || config.frames--)) {
            processInput(window);

            // Just make compiler happy
            render(true);

            glfwPollEvents();
        }
    }

    int getWindowWidth() const {
        return config.window_size.first;
    }

    int getWindowHeight() const {
        return config.window_size.second;
    }

    GLFWwindow* getWindow() const {
        return window;
    }

private:
    // Parsed from default options. Application is resposible for providing variables to bind to
    // and to use on their own.
    Config config;
    GLFWwindow* window;
};
}
