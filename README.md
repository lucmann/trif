trif [Working In Progress]
==========================
An OpenGL simple triangle demos framework

# Why is trif simple

- [Simple#0] Each demo written under **trif** framework corresponds to an individual cpp source file in general.
- [Simple#1] Efforts are taken to make **trif** be a header-only cpp framework
- [Simple#2] Third-party libraries used by **trif** are also header-only as much as possible

# How to use trif

You may write `main` function as following

```c++
...
#include <GLFW/glfw3.h>
#include "application.hpp"
...

int main(int argc, char **argv) {
    // customize command-line options or flags. Note that it is optional
    trif::Option generate_mipmap = {
        "--mipmap,!--no-mipmap",
        "Whether to generate MIPMAP or not",
        trif::OptionType::FlagOnly
    };

    trif::Application app("msaa", argc, argv, {&generate_mipmap});

    trif::Config conf = app.getConfig();

    bool has_mipmap = app.get_option_value<bool>(&generate_mipmap);
    uint32_t frames = conf.n_frames;    // default value is 1

    ...

    trif::Program<
        trif::Shaders<GL_VERTEX_SHADER>,
        trif::Shaders<GL_FRAGMENT_SHADER>
    > program(
        vertex_source, fragment_source
    );

    ...

    if (has_mipmap) {
        ...
    }

    while (!glfwWindowShouldClose(window) && frames) {

        ...

        if (!conf.forever)
            frames--;
    }

    ...

    return 0;
}
```

# How to add a demo

As an individual executable for each demo, you are encouraged to create a separate
directory for your new demo(s) under the directory `example`. Then

- write your demo code in a new `DEMO_NAME.cpp` source file
- insert the following line to `example/CMakeLists.txt`

```cmake
example(DEMO_NAME DIR_NAME)
```

NOTE: Since `file(GLOB)` is used in the CMakeLists.txt, `cmake -B build` must be
invoked after the addition of new demo.

# References

- [LearnOpenGL](https://github.com/JoeyDeVries/LearnOpenGL)
- [stb](https://github.com/nothings/stb)
- [Vulkan-Samples](https://github.com/KhronosGroup/Vulkan-Samples)
