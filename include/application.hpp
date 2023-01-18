//
// Created by luc on 2023/1/16.
//

#pragma once

#include <utility>
#include "parser.hpp"

namespace trif
{
Option n_frames = {"-n,--num-frames",   "Draw the given number of frames then exit"};
Option geometry = {"-g,--geometry",     "Specify the size of window (default 800x600)", OptionType::Pair};

struct Config {
    uint32_t n_frames;
    bool forever;
    std::pair<uint32_t, uint32_t> window_size;
    // TODO: add some config if needed
};

class Application {
public:
    // Allow client to customize other options
    Application(const std::string &name, int argc, char **argv, const std::vector<Option *> &custom_options = {}) {
        // use argv pointer array to initialize arguments vector
        arguments = {argv, argv + argc };
        // append user-defined options to the default
        options.insert(options.end(), custom_options.begin(), custom_options.end());

        parser = std::make_unique<CLI11Parser>(
                name,
                "\nDemo based on tri framework to demonstrate the 3D graphics app best practice.\n",
                arguments);

        if (!parser->parse(options)) {
            // TODO:
        } else {
            if (parser->contains(&n_frames)) {
                default_config.n_frames = parser->as<uint32_t>(&n_frames);
                default_config.forever = false;
            } else if (parser->contains(&geometry)) {
                default_config.window_size = parser->as<std::pair<uint32_t, uint32_t>>(&geometry);
            }
        }
    }

    template <typename Type>
    auto get_option_value(Option *option) const {
        return parser->as<Type>(option);
    }

    auto &getConfig() const {
        return default_config;
    }

private:
    std::vector<std::string>        arguments;
    std::unique_ptr<CLI11Parser>    parser;
    std::vector<Option *>           options = {&n_frames, &geometry};
    Config                          default_config{1, true, std::make_pair(800, 600)};
};
}
