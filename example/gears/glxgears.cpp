#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "application.hpp"

static const unsigned STRIPS_PER_TOOTH = 7;
static const unsigned VERTICES_PER_TOOTH = 34;
static const unsigned GEAR_VERTEX_STRIDE = 6;

/// Struct describing the vertices in triangle strip
struct vertex_strip {
    /// The first vertex in the strip
    GLint first;
    /// The number of consecutive vertices in the strip after the first
    GLint count;
};

/// Each vertex consist of GEAR_VERTEX_STRIDE GLfloat attributes
typedef GLfloat GearVertex[GEAR_VERTEX_STRIDE];

/// Struct representing a gear.
struct gear {
    /// The array of vertices comprising the gear
    GearVertex *vertices;
    /// The number of vertices comprising the gear
    int nvertices;
    /// The array of triangle strips comprising the gear
    struct vertex_strip *strips;
    /// The number of triangle strips comprising the gear
    int nstrips;
    /// The Vertex Buffer Object holding the vertices in the graphics card
    GLuint vbo;
    /// for Mesa's llvmpipe. if no, glDrawArrays(no VAO bound)
    GLuint vao;
};

/// The view rotation
static GLfloat view_rotx = 20.0, view_roty = 30.0, view_rotz = 0.0;
/// The current gear rotation angle
static GLfloat angle = 0.0;
/// The projection matrix
static glm::mat4 ProjectionMatrix;
/// The direction of the directional light for the scene
static const glm::vec4 LightSourcePosition = {5.0, 5.0, 10.0, 1.0};


enum GearMask {
    GEAR_NONE  = 0,
    GEAR_RED   = 1 << 0,
    GEAR_GREEN = 1 << 1,
    GEAR_BLUE  = 1 << 2,
    GEAR_ALL   = GEAR_RED | GEAR_GREEN | GEAR_BLUE
};

inline GearMask operator |(const GearMask& lhs, const GearMask& rhs)
{
    return static_cast<GearMask>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

inline GearMask operator &(const GearMask& lhs, const GearMask& rhs)
{
    return static_cast<GearMask>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

inline GearMask& operator |=(GearMask& lhs, const GearMask& rhs) {
    return lhs = lhs | rhs;
}

static GearMask gears_filter = GEAR_NONE;


static GLboolean animate = GL_TRUE;     // Animation
static bool fat_draw = false;         // true if we put too many vertices in one draw call
static bool use_fbo = false;          // true if we are rendering off-screen using fbo

const std::string vertex_source = R"(
#version 330 core

precision mediump float;
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;

uniform mat4 ModelView;
uniform mat4 Projection;
uniform mat4 NormalMatrix;
uniform vec4 LightSourcePosition;
uniform vec4 MaterialColor;

out vec4 Color;

void main(void)
{
    // Transform the normal to eye coordinates
    vec3 N = normalize(vec3(NormalMatrix * vec4(normal, 1.0)));

    // The LightSourcePosition is actually its direction for directional light
    vec3 L = normalize(LightSourcePosition.xyz);

    // Multiply the diffuse value by the vertex color (which is fixed in this case)
    // to get the actual color that we will use to draw this vertex with
    float diffuse = max(dot(N, L), 0.0);
    Color = diffuse * MaterialColor;

    // Transform the position to clip coordinates
    gl_Position = Projection * ModelView * vec4(position, 1.0);
}
)";

const std::string fragment_source = R"(
#version 330 core
precision mediump float;
in vec4 Color;
layout (location = 0) out vec4 fg_FragColor;

void main(void)
{
    fg_FragColor = Color;
}
)";

/// gears only uses vertex and fragment shaders
using ProgramType = trif::Program<trif::Shaders<GL_VERTEX_SHADER>, trif::Shaders<GL_FRAGMENT_SHADER>>;

/// The gears
static struct gear *gear1, *gear2, *gear3;

///
/// Fills a gear vertex.
///
/// @param v the vertex to fill
/// @param x the x coordinate
/// @param y the y coordinate
/// @param z the z coortinate
/// @param n pointer to the normal table
///
/// @return the operation error code

static GearVertex *vert(GearVertex *v, GLfloat x, GLfloat y, GLfloat z,
                        GLfloat n[3]) {
    v[0][0] = x;
    v[0][1] = y;
    v[0][2] = z;
    v[0][3] = n[0];
    v[0][4] = n[1];
    v[0][5] = n[2];

    return v + 1;
}

///
/// Create a gear wheel.
///
/// @param inner_radius radius of hole at center
/// @param outer_radius radius at center of teeth
/// @param width width of gear
/// @param teeth number of teeth
/// @param tooth_depth depth of tooth
///
/// @return pointer to the constructed struct gear

static struct gear *create_gear(GLfloat inner_radius, GLfloat outer_radius,
                                GLfloat width, GLint teeth,
                                GLfloat tooth_depth) {
    GLfloat r0, r1, r2;
    GLfloat da;
    GearVertex *v;
    struct gear *gear;
    double s[5], c[5];
    GLfloat normal[3];
    int cur_strip = 0;
    int i;

    /// Allocate memory for the gear
    gear = (struct gear *)malloc(sizeof *gear);
    if (gear == NULL)
        return NULL;

    /// Calculate the radii used in the gear
    r0 = inner_radius;
    r1 = outer_radius - tooth_depth / 2.0;
    r2 = outer_radius + tooth_depth / 2.0;

    da = 2.0 * M_PI / teeth / 4.0;

    /// Allocate memory for the triangle strip information
    gear->nstrips = STRIPS_PER_TOOTH * teeth;
    gear->strips =
            (struct vertex_strip *)calloc(gear->nstrips, sizeof(*gear->strips));

    /// Allocate memory for the vertices
    gear->vertices = (GearVertex *)calloc(VERTICES_PER_TOOTH * teeth,
                                          sizeof(*gear->vertices));
    v = gear->vertices;

    for (i = 0; i < teeth; i++) {
        /// Calculate needed sin/cos for varius angles
        sincos(i * 2.0 * M_PI / teeth, &s[0], &c[0]);
        sincos(i * 2.0 * M_PI / teeth + da, &s[1], &c[1]);
        sincos(i * 2.0 * M_PI / teeth + da * 2, &s[2], &c[2]);
        sincos(i * 2.0 * M_PI / teeth + da * 3, &s[3], &c[3]);
        sincos(i * 2.0 * M_PI / teeth + da * 4, &s[4], &c[4]);

        /// A set of macros for making the creation of the gears easier
#define GEAR_POINT(r, da)                                                      \
    { (r) * c[(da)], (r)*s[(da)] }
#define SET_NORMAL(x, y, z)                                                    \
    do {                                                                       \
        normal[0] = (x);                                                       \
        normal[1] = (y);                                                       \
        normal[2] = (z);                                                       \
    } while (0)

#define GEAR_VERT(v, point, sign)                                              \
    vert((v), p[(point)].x, p[(point)].y, (sign)*width * 0.5, normal)

#define START_STRIP                                                            \
    do {                                                                       \
        gear->strips[cur_strip].first = v - gear->vertices;                    \
    } while (0);

#define END_STRIP                                                              \
    do {                                                                       \
        int _tmp = (v - gear->vertices);                                       \
        gear->strips[cur_strip].count = _tmp - gear->strips[cur_strip].first;  \
        cur_strip++;                                                           \
    } while (0)

#define QUAD_WITH_NORMAL(p1, p2)                                               \
    do {                                                                       \
        SET_NORMAL((p[(p1)].y - p[(p2)].y), -(p[(p1)].x - p[(p2)].x), 0);      \
        v = GEAR_VERT(v, (p1), -1);                                            \
        v = GEAR_VERT(v, (p1), 1);                                             \
        v = GEAR_VERT(v, (p2), -1);                                            \
        v = GEAR_VERT(v, (p2), 1);                                             \
    } while (0)

        struct point {
            GLdouble x;
            GLdouble y;
        };

        /// Create the 7 points (only x,y coords) used to draw a tooth
        struct point p[7] = {
                GEAR_POINT(r2, 1), // 0
                GEAR_POINT(r2, 2), // 1
                GEAR_POINT(r1, 0), // 2
                GEAR_POINT(r1, 3), // 3
                GEAR_POINT(r0, 0), // 4
                GEAR_POINT(r1, 4), // 5
                GEAR_POINT(r0, 4), // 6
        };

        /// Front face
        START_STRIP;
        SET_NORMAL(0, 0, 1.0);
        v = GEAR_VERT(v, 0, +1);
        v = GEAR_VERT(v, 1, +1);
        v = GEAR_VERT(v, 2, +1);
        v = GEAR_VERT(v, 3, +1);
        v = GEAR_VERT(v, 4, +1);
        v = GEAR_VERT(v, 5, +1);
        v = GEAR_VERT(v, 6, +1);
        END_STRIP;

        /// Inner face
        START_STRIP;
        QUAD_WITH_NORMAL(4, 6);
        END_STRIP;

        /// Back face
        START_STRIP;
        SET_NORMAL(0, 0, -1.0);
        v = GEAR_VERT(v, 6, -1);
        v = GEAR_VERT(v, 5, -1);
        v = GEAR_VERT(v, 4, -1);
        v = GEAR_VERT(v, 3, -1);
        v = GEAR_VERT(v, 2, -1);
        v = GEAR_VERT(v, 1, -1);
        v = GEAR_VERT(v, 0, -1);
        END_STRIP;

        /// Outer face
        START_STRIP;
        QUAD_WITH_NORMAL(0, 2);
        END_STRIP;

        START_STRIP;
        QUAD_WITH_NORMAL(1, 0);
        END_STRIP;

        START_STRIP;
        QUAD_WITH_NORMAL(3, 1);
        END_STRIP;

        START_STRIP;
        QUAD_WITH_NORMAL(5, 3);
        END_STRIP;
    }

    gear->nvertices = (v - gear->vertices);

    /// Store the vertices in a vertex buffer object (VBO)
    glGenBuffers(1, &gear->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, gear->vbo);
    glBufferData(GL_ARRAY_BUFFER, gear->nvertices * sizeof(GearVertex),
                 gear->vertices, GL_STATIC_DRAW);

    glGenVertexArrays(1, &gear->vao);
    glBindVertexArray(gear->vao);

    return gear;
}

/// construct gears

static void model_gears() {
    gear1 = create_gear(1.0, 4.0, 1.0, 20, 0.7);
    gear2 = create_gear(0.5, 2.0, 2.0, 10, 0.7);
    gear3 = create_gear(1.3, 2.0, 0.5, 10, 0.7);
}

///
/// Draws a gear.
///
/// @param gear the gear to draw
/// @param transform the current transformation matrix
/// @param x the x position to draw the gear at
/// @param y the y position to draw the gear at
/// @param angle the rotation angle of the gear
/// @param color the color of the gear

static void draw_gear(ProgramType &program, struct gear *gear,
                      const glm::mat4 &transform,
                      const glm::vec3 &position,
                      const GLfloat angle,
                      const glm::vec4 &color) {
    glm::mat4 model_view;
    glm::mat4 normal_matrix;

    /// Translate and rotate the gear
    model_view = glm::translate(transform, position);
    model_view = glm::rotate(model_view, glm::radians(angle), glm::vec3(0.0, 0.0, 1.0));

    /// Set ModelViewProjection matrix
    program.uniform("ModelView", model_view);
    program.uniform("Projection", ProjectionMatrix);

    /// Create and set the NormalMatrix. It's the inverse transpose of the ModelView matrix.
    normal_matrix = glm::inverseTranspose(model_view);
    program.uniform("NormalMatrix", normal_matrix);

    /// Set light source position
    program.uniform("LightSourcePosition", LightSourcePosition);

    /// Set the gear color
    program.uniform("MaterialColor", color);

    /// Set the vertex buffer object to use
    glBindBuffer(GL_ARRAY_BUFFER, gear->vbo);

    /// Set up the position of the attributes in the vertex buffer object
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), NULL);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLfloat *)0 + 3);

    /// Enable the attributes
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    /// Draw the triangle strips that comprise the gear

    int n;
    unsigned vertex_count = 0;

    if (fat_draw) {
        /// curious what if put so many vertices in one draw call
        for (n = 0; n < gear->nstrips; n++)
            vertex_count += gear->strips[n].count;

        glDrawArrays(GL_TRIANGLE_STRIP, gear->strips[0].first, vertex_count);
    } else {
        for (n = 0; n < gear->nstrips; n++)
            glDrawArrays(GL_TRIANGLE_STRIP, gear->strips[n].first, gear->strips[n].count);
    }

    /// Disable the attributes
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
}

/// Draws the gears.

static void draw_gears(ProgramType &program) {
    const glm::vec4 red = {0.8, 0.1, 0.0, 1.0};
    const glm::vec4 green = {0.0, 0.8, 0.2, 1.0};
    const glm::vec4 blue = {0.2, 0.2, 1.0, 1.0};

    glClearColor(0.1, 0.1, 0.1, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /// Translate and rotate the view
    glm::mat4 transform = glm::translate(glm::mat4(1.0), glm::vec3(0.0, 0.0, -20.0));

    transform = glm::rotate(transform, glm::radians(view_rotx), glm::vec3(1.0, 0.0, 0.0));
    transform = glm::rotate(transform, glm::radians(view_roty), glm::vec3(0.0, 1.0, 0.0));
    transform = glm::rotate(transform, glm::radians(view_rotz), glm::vec3(0.0, 0.0, 1.0));

    /// Draw the gears
    if (GEAR_RED & gears_filter)
        draw_gear(program, gear1, transform, glm::vec3(-3.0, -2.0, 0.0), angle, red);
    if (GEAR_GREEN & gears_filter)
        draw_gear(program, gear2, transform, glm::vec3(3.1, -2.0, 0.0), -2 * angle - 9.0, green);
    if (GEAR_BLUE & gears_filter)
        draw_gear(program, gear3, transform, glm::vec3(-3.1, 4.2, 0.0), -2 * angle - 25.0, blue);
}

/// Draw single frame, do SwapBuffers, compute FPS
static void draw_frame(GLFWwindow *window, ProgramType &program) {
    static int frames = 0;
    static double tRot0 = -1.0, tRate0 = -1.0;
    double dt, t = glfwGetTime();

    if (tRot0 < 0.0)
        tRot0 = t;

    dt = t - tRot0;
    tRot0 = t;

    if (animate) {
        /// advance rotation for next frame
        /// 70 degrees per second
        angle += 70.0 * dt;
        if (angle > 3600.0)
            angle -= 3600.0;
    }

    draw_gears(program);

    if (use_fbo)
        glFinish();
    else
        glfwSwapBuffers(window);

    frames++;

    if (tRate0 < 0.0)
        tRate0 = t;
    if (t - tRate0 >= 5.0) {
        GLfloat seconds = t - tRate0;
        GLfloat fps = frames / seconds;
        std::cout << frames << " frames in " << seconds << " seconds = " << fps << " FPS\n";
        fflush(stdout);
        tRate0 = t;
        frames = 0;
    }
}

int main(int argc, const char **argv)
{
    trif::Application app("glxgears");

    // app.add_option("-m, --gears-mask", gears_filter, "Mask gears which need to be drawn (default 'all')");
    // app.add_flag("--fat-draw, !--no-fat-draw", fat_draw, "If put too many vertices in one draw call (default false)");
    // app.add_flag("--fbo, !--no-fbo", use_fbo, "Rendering off-screen using fbo");

    app.init(argc, argv);

    const uint32_t win_w = app.getWindowWidth();
    const uint32_t win_h = app.getWindowHeight();

    /// Set gears_filter as user demands
    if (gears_filter == GEAR_NONE)
        gears_filter = GEAR_ALL;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    GLuint color_renderbuffer = 0;
    GLuint depth_renderbuffer = 0;
    GLuint fbo = 0;

    if (use_fbo) {
        glGenRenderbuffers(1, &color_renderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, color_renderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, win_w, win_h);

        glGenRenderbuffers(1, &depth_renderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, depth_renderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, win_w, win_h);

        // set up fbo
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color_renderbuffer);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_renderbuffer);
    }

    model_gears();

    ProgramType program(vertex_source, fragment_source);
    program.use();

    ProjectionMatrix = glm::perspective(glm::radians(60.0f), (float)win_w / (float)win_h, 1.0f, 1024.0f);

    // render loop
    // -----------
    app.main_loop([&](bool) {
        draw_frame(app.getWindow(), program);
    });

    if (fbo)
        glDeleteFramebuffers(1, &fbo);

    if (color_renderbuffer)
        glDeleteRenderbuffers(1, &color_renderbuffer);

    if (depth_renderbuffer)
        glDeleteRenderbuffers(1, &depth_renderbuffer);

    return 0;
}
