#pragma once

#include <algorithm> // for std::transform
#include <array>
#include <fstream>
#include <vector>

#include <GL/gl.h>
#include <glm/glm.hpp>

namespace trif
{

static inline std::string shader_source_from_string_or_file(const std::string& s)
{
    std::ifstream maybe_file(s);

    if (!maybe_file.is_open()) {
        // Input has seemingly been a shader source string
        return s;
    }

    return std::string((std::istreambuf_iterator<char>(maybe_file)),
                  std::istreambuf_iterator<char>());
}


/// shader source template
///
/// provides another way to parameterize the shader program at runtime
/// besides uniform variables.
class ShaderSourceTemplate {
public:
    using ParamsType = std::map<std::string, std::string>;

public:
    ShaderSourceTemplate() = default;
    ShaderSourceTemplate(const std::string& str) { set_template(str); }
    ~ShaderSourceTemplate() = default;

    /// not allowed
    ShaderSourceTemplate(const ShaderSourceTemplate&) = delete;
    ShaderSourceTemplate& operator=(const ShaderSourceTemplate&) = delete;

    void set_template(const std::string& str) { _tmpl = str; }
    std::string specialize(const ParamsType& params) const;

private:
    std::string _tmpl;
};

std::string ShaderSourceTemplate::specialize(const ParamsType &params) const {
    std::ostringstream res;
    size_t cur_pos = 0;

    for (;;) {
        size_t param_start = _tmpl.find("${", cur_pos);
        /// Contains parameters in the source template
        if (param_start != std::string::npos) {
            res << _tmpl.substr(cur_pos, param_start - cur_pos);

            size_t param_end = _tmpl.find("}", param_start);
            if (param_end == std::string::npos)
                assert(0 && "Missing '}' in the shader source template parameter");

            /// Fill in parameter value
            std::string param_name = _tmpl.substr(param_start + 2, param_end - (2 + param_start));

            auto iter = params.find(param_name);
            if (iter != params.end())
                res << (*iter).second;

            /// Scan forward
            cur_pos = param_end + 1;
        } else {
            if (cur_pos < _tmpl.length())
                res << &_tmpl[cur_pos];

            break;
        }
    }

    /// TODO: add debug flags
    /// std::cout << res.str() << '\n';

    return res.str();
}


template<GLenum ST>
class Shaders {
public:
    Shaders() {}

    template<typename... Ts>
    Shaders(Ts... sources) {
        constexpr std::size_t n_sources = sizeof...(sources);
        std::vector<std::string> dummy = {
                shader_source_from_string_or_file(std::move(sources))...
        };

        std::array<const GLchar*, n_sources> p_sources;

        std::transform(dummy.cbegin(), dummy.cend(), p_sources.begin(),
                       [](const std::string& s) -> const GLchar* { return s.c_str(); });

        _id = glCreateShader(ST);
        glShaderSource(_id, n_sources, p_sources.data(), NULL);
        glCompileShader(_id);
        GLint compiled;
        glGetShaderiv(_id, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint length;
            GLchar *log;
            glGetShaderiv(_id, GL_INFO_LOG_LENGTH, &length);

            log = (GLchar *)malloc(length);
            glGetShaderInfoLog(_id, length, &length, log);
            glDeleteShader(_id);
            std::string err(log);
            free(log);
            throw std::invalid_argument(err);
        }
    };

    ~Shaders() {
        glDeleteShader(_id);
    }

    GLuint id() { return _id; }

private:
    GLuint _id;
};

//
// never instantiated
//
template<typename... T>
class Program {

};

//
// zero argument specialization
//
// the root of the whole hierarchy of inheritance as well as
// the end of recursion of inheritance
//
template<>
class Program<> {
public:
    Program(): _id(glCreateProgram()) {}
    ~Program() { glDeleteProgram(_id); }

    const GLuint id() const { return _id; }

private:
    GLuint _id;
};

//
// one argument and one parameter pack specialization
//
template<typename First, typename... Rest>
class Program<First, Rest...> : public Program<Rest...> {
public:
    Program() {}

    Program(const First& v, const Rest&... vrest)
            : _first(v), Program<Rest...>(vrest...) {
        glAttachShader(this->id(), _first.id());
    }

    template<typename... Types>
    Program(const Program<Types...>& rhs)
            : _first(rhs.first()), Program<Rest...>(rhs.rest()) {}

    template<typename... Types>
    Program& operator=(const Program<Types...>& rhs) {
        _first = rhs.first();
        rest() = rhs.rest();
        return *this;
    }

    //
    // Helpers
    //
    First& first() { return _first; }
    const First& first() const {  return _first; }

    Program<Rest...>& rest() { return *this; }
    const Program<Rest...>& rest() const { return *this; }

    void use() {
        // Deferred linkage
        glLinkProgram(this->id());

        // Activate program
        glUseProgram(this->id());
    }

    unsigned int uniform(const std::string& name) {
        return glGetUniformLocation(this->id(), name.c_str());
    }

    void uniform(const std::string &name, const float value) {
        glUniform1f(uniform(name), value);
    }

    void uniform(const std::string &name, const int value) {
        glUniform1i(uniform(name), value);
    }

    void uniform(const std::string &name, const glm::mat4 &value) {
        glUniformMatrix4fv(uniform(name), 1, GL_FALSE, &value[0][0]);
    }

protected:
    First _first;
};


// full specialization
template<int... T>
class Program<Shaders<T>...> {
};

}
