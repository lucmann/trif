//
// Created by luc on 2023/1/17.
//

#pragma once

#include <cassert>
#include <unordered_map>
#include "CLI11.hpp"


namespace trif
{

enum class OptionType {
    FlagOnly,
    OneValue,
    Pair,
};

class Option {
public:
    Option(const std::string &name, const std::string &help_line, OptionType type = OptionType::OneValue)
        : _name(name)
        , _help_line(help_line)
        , _type(type) {

    }

    ~Option() = default;


    const std::string &get_name() const {
        assert(!_name.empty() && "Option name unset");
        return _name;
    }

    void set_name(const std::string &name) {
        _name = name;
    }

    const std::string &get_help_line() const {
        assert(!_help_line.empty() && "Option help line unset");
        return _help_line;
    }

    void set_help_line(const std::string &help_line) {
        _help_line = help_line;
    }

    OptionType get_option_type() const {
        return _type;
    }

private:
    std::string _name;
    std::string _help_line;
    OptionType  _type;
};


class CLI11Parser {
public:
    CLI11Parser(const std::string &name, const std::string &desc, const std::vector<std::string> &args)
        :_cli11{std::make_unique<CLI::App>(desc, name)}
        ,_formatter{std::make_shared<CLI::Formatter>()} {
        _cli11->formatter(_formatter);

        _args.resize(args.size());
        std::transform(args.begin(), args.end(), _args.begin(),
                       [](const std::string &s) -> char * { return const_cast<char *>(s.c_str()); });
    }
    ~CLI11Parser() = default;


    // Retrieve the option value, called by the client e.g. Application
    template <typename Type>
    Type as(Option *option) const {
        auto values = get_option_values(option);

        Type type{};
        bool implemented_type_conversion = convert_type(values, &type);
        assert(implemented_type_conversion && "Failed to retrieve value. Type unsupported");
        return type;
    }

    // Argument parser, called by the client e.g. Application
    bool parse(const std::vector<Option *> &options) {
        CLI::Option *co;
        for (auto *option : options) {
            switch (option->get_option_type()) {
                case OptionType::FlagOnly:
                    co = _cli11->add_flag(option->get_name(), option->get_help_line());
                    break;
                case OptionType::OneValue:
                    co = _cli11->add_option(option->get_name(), option->get_help_line());
                    break;
                case OptionType::Pair:
                    co = _cli11->add_option(option->get_name(), option->get_help_line())
                            ->delimiter('x')
                            ->expected(2);
                    break;
            }

            _options.emplace(option, co);
        }

        return cli11_parse(_cli11.get());
    }

    // Return true if the given option is passed, otherwise false
    bool contains(Option *option) const {
        auto it = _options.find(option);

        if (it != _options.end()) {
            return it->second->count() > 0;
        }

        return false;
    }

private:
    std::vector<std::string> get_option_values(Option *option) const {
        auto iter = _options.find(option);

        if (iter == _options.end()) {
            return {};
        }

        return iter->second->results();
    }

    template <typename Type>
    inline bool convert_type(const std::vector<std::string> &values, Type *out) const {
        return false;
    }

    bool cli11_parse(CLI::App *app) {
        // The try/catch block ensures that '-h,--help' or a parse error will exit with
        // the correct return code
        try {
            app->parse(static_cast<int>(_args.size()), _args.data());
        } catch (CLI::CallForHelp e) {
            return app->exit(e);
        } catch (CLI::ParseError e) {
            bool success = e.get_exit_code() == static_cast<int>(CLI::ExitCodes::Success);

            if (!success) {
                return app->exit(e);
            }
        }

        return true;
    }

private:
    std::vector<const char *>                           _args;
    std::unique_ptr<CLI::App>                           _cli11;
    std::unordered_map<trif::Option *, CLI::Option *>   _options;
    std::shared_ptr<CLI::Formatter>                     _formatter;
};

// specialization
template <>
inline bool CLI11Parser::convert_type(const std::vector<std::string> &values, float *out) const {
    if (values.size() != 1) {
        *out = 0.0f;
    } else {
        *out = std::stof(values[0]);
    }

    return true;
}

template <>
inline bool CLI11Parser::convert_type(const std::vector<std::string> &values, int *out) const {
    if (values.size() != 1) {
        *out = 0;
    } else {
        *out = std::stoi(values[0]);
    }

    return true;
}

template <>
inline bool CLI11Parser::convert_type(const std::vector<std::string> &values, uint32_t *out) const {
    if (values.size() != 1) {
        *out = 0;
    } else {
        auto val = std::stoi(values[0]);
        *out = static_cast<uint32_t>(val);
    }

    return true;
}

// Used to convert flag options which result will be true (string) or contain a value of 1. For the latter
// client must add flag with additional default values. e.g.
// app.add_flag("--flag{1},!--no-flag{0}", result, "help for flag");
template <>
inline bool CLI11Parser::convert_type(const std::vector<std::string> &values, bool *out) const {
    if (values.size() != 1)
        *out = false;    // if not specified on the command line, keep it disabled anyway
    else
        *out = values[0].compare("true") == 0 ? true : false;

    return true;
}

// set window size
// for instance --geometry 300x300
template <>
inline bool CLI11Parser::convert_type(const std::vector<std::string> &values, std::pair<uint32_t, uint32_t> *out) const {
    if (values.size() != 2) {
        out->first = 800;
        out->second = 600;
    } else {
        out->first = static_cast<uint32_t>(std::stoi(values[0]));
        out->second = static_cast<uint32_t>(std::stoi(values[1]));
    }

    return true;
}

template <>
inline bool CLI11Parser::convert_type(const std::vector<std::string> &values, std::string *out) const {
    if (values.size() > 0)
        *out = values[0];
    else
        *out = "";

    return true;
}

// TODO: add other types as you'd like
}
