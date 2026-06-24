#include <stdbool.h>
#include <stdio.h>

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <tonc_video.h>
#include <glad/glad.h>

#define WINDOW_SCALE 3

void platform_app_init(void);
void platform_app_frame(void);

static SDL_Window *s_window = NULL;
static SDL_GLContext s_gl = NULL;

struct gfx_state
{
    GLuint screen_tex;
    GLuint display_prog;
    GLuint display_quad;
    GLuint display_quad_idx;

    u32 screen_pixels[SCREEN_WIDTH * SCREEN_HEIGHT];
}
static s_gfx_state;

static const char *VS_SOURCE =
    "#version 130\n"
    "in vec2 a_position;\n"
    "in vec2 a_texcoord;\n"
    "out vec2 v_texcoord;\n"
    "uniform mat4 u_matrix;\n"
    "void main() {\n"
    "   v_texcoord = a_texcoord;\n"
    "   gl_Position = u_matrix * vec4(a_position, 0.0, 1.0);\n"
    "}\n";

static const char *FS_SOURCE =
    "#version 130\n"
    "in vec2 v_texcoord;\n"
    "out vec4 o_color;\n"
    "uniform sampler2D u_texture;\n"
    "void main() {\n"
    "   o_color = texture2D(u_texture, v_texcoord);\n"
    "}\n";

static void gl_debug_output(GLenum source, GLenum type, GLuint id,
                            GLenum severity, GLsizei length,
                            const GLchar *msg, const void *userdata)
{
    // ignore non-significant error/warning codes
    if(id == 131169 || id == 131185 || id == 131218 || id == 131204) return;

    fprintf(stderr, "---------------\n");
    fprintf(stderr, "Debug message (%i): %s\n", id, msg);

    switch (source)
    {
        case GL_DEBUG_SOURCE_API:             fprintf(stderr, "Source: API"); break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   fprintf(stderr, "Source: Window System"); break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: fprintf(stderr, "Source: Shader Compiler"); break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:     fprintf(stderr, "Source: Third Party"); break;
        case GL_DEBUG_SOURCE_APPLICATION:     fprintf(stderr, "Source: Application"); break;
        case GL_DEBUG_SOURCE_OTHER:           fprintf(stderr, "Source: Other"); break;
    } fprintf(stderr, "\n");

    switch (type)
    {
        case GL_DEBUG_TYPE_ERROR:               fprintf(stderr, "Type: Error"); break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: fprintf(stderr, "Type: Deprecated Behaviour"); break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  fprintf(stderr, "Type: Undefined Behaviour"); break; 
        case GL_DEBUG_TYPE_PORTABILITY:         fprintf(stderr, "Type: Portability"); break;
        case GL_DEBUG_TYPE_PERFORMANCE:         fprintf(stderr, "Type: Performance"); break;
        case GL_DEBUG_TYPE_MARKER:              fprintf(stderr, "Type: Marker"); break;
        case GL_DEBUG_TYPE_PUSH_GROUP:          fprintf(stderr, "Type: Push Group"); break;
        case GL_DEBUG_TYPE_POP_GROUP:           fprintf(stderr, "Type: Pop Group"); break;
        case GL_DEBUG_TYPE_OTHER:               fprintf(stderr, "Type: Other"); break;
    } fprintf(stderr, "\n");
    
    switch (severity)
    {
        case GL_DEBUG_SEVERITY_HIGH:         fprintf(stderr, "Severity: high"); break;
        case GL_DEBUG_SEVERITY_MEDIUM:       fprintf(stderr, "Severity: medium"); break;
        case GL_DEBUG_SEVERITY_LOW:          fprintf(stderr, "Severity: low"); break;
        case GL_DEBUG_SEVERITY_NOTIFICATION: fprintf(stderr, "Severity: notification"); break;
    } fprintf(stderr, "\n\n");
}

static bool gfx_state_init(void)
{
    s_gfx_state = (struct gfx_state){0};

    // create screen texture (will be cpu-rendered into)
    glGenTextures(1, &s_gfx_state.screen_tex);
    glBindTexture(GL_TEXTURE_2D, s_gfx_state.screen_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCREEN_WIDTH, SCREEN_HEIGHT, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // create display quad geometry buffer
    static const float display_quad_data[] = {
        -1.0f, 1.0f,    0.0f, 0.0f,
        -1.0f,-1.0f,    0.0f, 1.0f,
         1.0f,-1.0f,    1.0f, 1.0f,
         1.0f, 1.0f,    1.0f, 0.0f,
    };

    glGenBuffers(1, &s_gfx_state.display_quad);
    glBindBuffer(GL_ARRAY_BUFFER, s_gfx_state.display_quad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(display_quad_data), display_quad_data,
                 GL_STATIC_DRAW);
    
    // create display quad element buffer
    static const u16 display_quad_ebo_data[] = {
        0, 1, 2,
        2, 3, 0
    };

    glGenBuffers(1, &s_gfx_state.display_quad_idx);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_gfx_state.display_quad_idx);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(display_quad_ebo_data),
                 display_quad_ebo_data, GL_STATIC_DRAW);

    // create shader
    GLint s;
    GLsizei log_len = 0;
    char errlog[1024];

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &VS_SOURCE, NULL);
    glCompileShader(vs);
    glGetShaderiv(vs, GL_COMPILE_STATUS, &s);
    if (!s)
    {
        glGetShaderInfoLog(vs, sizeof(errlog), &log_len, errlog);
        fprintf(stderr, "error compiling vertex shader: %s\n", errlog);

        glDeleteShader(vs);
        return false;
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &FS_SOURCE, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &s);
    if (!s)
    {
        glGetShaderInfoLog(fs, sizeof(errlog), &log_len, errlog);
        fprintf(stderr, "error compiling fragment shader: %s\n", errlog);

        glDeleteShader(fs);
        glDeleteShader(vs);
        return false;
    }

    GLuint prog = glCreateProgram();
    glBindAttribLocation(prog, 0, "a_position");
    glBindAttribLocation(prog, 1, "a_texcoord");
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    glDeleteShader(vs);
    glDeleteShader(fs);

    glGetProgramiv(prog, GL_LINK_STATUS, &s);
    if (!s)
    {
        glGetProgramInfoLog(prog, sizeof(errlog), &log_len, errlog);
        fprintf(stderr, "error linking shader program: %s\n", errlog);
        glDeleteProgram(prog);
        return false;
    }

    return true;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);

    const int scr_w = SCREEN_WIDTH * WINDOW_SCALE;
    const int scr_h = SCREEN_HEIGHT * WINDOW_SCALE;
    const SDL_WindowFlags win_flags =
        SDL_WINDOW_OPENGL| SDL_WINDOW_HIGH_PIXEL_DENSITY;
    
    s_window = SDL_CreateWindow("Unyuland", scr_w, scr_h, win_flags);
    if (!s_window)
    {
        SDL_Log("Couldn't create window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    s_gl = SDL_GL_CreateContext(s_window);
    if (!s_gl)
    {
        SDL_Log("Couldn't create OpenGL context: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!gladLoadGLLoader((void *)SDL_GL_GetProcAddress))
    {
        SDL_Log("OpenGL loader failed!");
        return SDL_APP_FAILURE;
    }

    SDL_GL_MakeCurrent(s_window, s_gl);
    SDL_GL_SetSwapInterval(1);
    
    GLint gl_flags;
    glGetIntegerv(GL_CONTEXT_FLAGS, &gl_flags);
    if (gl_flags & GL_CONTEXT_FLAG_DEBUG_BIT)
    {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(gl_debug_output, NULL);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL,
                              GL_TRUE);
    }

    if (!gfx_state_init())
        return SDL_APP_FAILURE;

    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i)
        s_gfx_state.screen_pixels[i] = 0x00FF00FF;
    
    platform_app_init();

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    platform_app_frame();

    int win_sx, win_sy;
    SDL_GetWindowSizeInPixels(s_window, &win_sx, &win_sy);

    glViewport(0, 0, win_sx, win_sy);

    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // update texture
    glBindTexture(GL_TEXTURE_2D, s_gfx_state.screen_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                    GL_RGBA, GL_UNSIGNED_BYTE, s_gfx_state.screen_pixels);
    
    // activate shader
    glUseProgram(s_gfx_state.display_prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_gfx_state.screen_tex);

    // bind geometry and attributes
    typedef struct
    {
        float position[2];
        float texcoord[2];
    } vertex_s;

    glBindBuffer(GL_ARRAY_BUFFER, s_gfx_state.display_quad);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_gfx_state.display_quad_idx);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex_s),
                          (void*)offsetof(vertex_s, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex_s),
                          (void*)offsetof(vertex_s, texcoord));
    glEnableVertexAttribArray(1);

    // draw the fucking quad
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

    SDL_GL_SwapWindow(s_window);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    glDeleteTextures(1, &s_gfx_state.screen_tex);
    glDeleteBuffers(1, &s_gfx_state.display_quad);
    glDeleteBuffers(1, &s_gfx_state.display_quad);
    glDeleteProgram(s_gfx_state.display_prog);
}