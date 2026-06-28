#include <stdbool.h>
#include <stdio.h>
#include <math.h>

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#ifndef PLATFORM_WEB
#define USE_GLAD
#endif

#ifdef USE_GLAD
#include "glad/glad.h"
// #define GL_DEBUG
#else
#include <SDL3/SDL_opengles2.h>
#endif

#include <tonc_video.h>
#include <tonc_math.h>
#include <modplay.h>
#include <psg_ctl.h>
#include "display.h"
#include "audioutil.h"

#define WINDOW_SCALE 3
#define SAMPLE_RATE 48000//32768

// these functions are defined by src/main/main.c
void platform_app_init(void);
void platform_app_frame(void);

static SDL_Window *s_window = NULL;
static SDL_AudioStream *s_astream = NULL;
static SDL_GLContext s_gl = NULL;
static double s_asamp_accum[2] = { 0.0, 0.0 };

static uint s_key_input = 0x3FF;

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
    "precision mediump float;\n"
    "attribute vec2 a_position;\n"
    "attribute vec2 a_texcoord;\n"
    "varying vec2 v_texcoord;\n"
    "uniform mat4 u_matrix;\n"
    "void main() {\n"
    "   v_texcoord = a_texcoord;\n"
    "   gl_Position = u_matrix * vec4(a_position, 0.0, 1.0);\n"
    "}\n";

static const char *FS_SOURCE =
    "precision mediump float;\n"
    "varying vec2 v_texcoord;\n"
    "uniform sampler2D u_texture;\n"
    "void main() {\n"
    "   gl_FragColor = texture2D(u_texture, v_texcoord);\n"
    "}\n";

#ifdef GL_DEBUG
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
#endif

static bool gfx_state_init(void)
{
    s_gfx_state = (struct gfx_state){0};

    // create screen texture (will be software-rendered into)
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

    s_gfx_state.display_prog = prog;

    return true;
}

static uint get_key_input_flag(SDL_Keycode key)
{
    switch (key)
    {
    case SDLK_Z:      return 0x001; // A
    case SDLK_X:      return 0x002; // B
    case SDLK_C:      return 0x200; // L
    case SDLK_V:      return 0x100; // R
    case SDLK_ESCAPE: return 0x008; // Start 
    case SDLK_TAB:    return 0x004; // Select
    case SDLK_RIGHT:  return 0x010; // Right
    case SDLK_LEFT:   return 0x020; // Left
    case SDLK_UP:     return 0x040; // Up
    case SDLK_DOWN:   return 0x080; // Down
    default:          return 0x000;
    }
}

static void audio_update(void)
{
    #define FRAME_COUNT 64
    #define NCHANNELS 2
    const int min_audio = (SAMPLE_RATE * sizeof(float)) * 0.1;
    while (SDL_GetAudioStreamQueued(s_astream) < min_audio)
    {
        static s16 samples[FRAME_COUNT * NCHANNELS];
        static s16 mplay_samples[FRAME_COUNT * NCHANNELS];
        static s16 psg_samples[FRAME_COUNT * NCHANNELS];

        mplay_render(mplay_samples, FRAME_COUNT);
        psg_render(psg_samples, FRAME_COUNT);

        const double sample_len = 1.0 / SAMPLE_RATE;

        // these registers should be read at the start of every sound engine
        // audio tick, not at the start of the audio frame. but I'm lazy and the
        // game literally only modifies these registers once, at boot.
        uint psg_mix_i  = REG_SNDDSCNT & 3;
        uint dsa_mix_i = REG_SNDDSCNT & SDS_A100;
        uint dsb_mix_i = REG_SNDDSCNT & SDS_B100;

        // a psg mix value of 3 is prohibited, but for convenience I 
        // interpret it as 125% so that I can use simple arithmetic to calculate
        // the mix percetange.
        // also, i think the program can choose which channels DSA or DSB emit
        // to. I'm just going to assume A is on channe L and B is on channel R.
        // Dunno if this is what maxmod does actually does, but sure.
        double psg_mix = (psg_mix_i / 3.0) + 0.25;
        double dsa_mix = (double) dsa_mix_i * 0.5 + 0.5;
        double dsb_mix = (double) dsb_mix_i * 0.5 + 0.5;

        for (size_t i = 0; i < FRAME_COUNT * NCHANNELS; i += 2)
        {
            samples[i+0] = mplay_samples[i+0] * dsa_mix +
                           psg_samples[i+0] * psg_mix;
            samples[i+1] = mplay_samples[i+1] * dsb_mix +
                           psg_samples[i+1] * psg_mix; 

            // post-process: "convert" to 9-bit audio
            samples[i+0] = (samples[i+0] - 64) / 128;
            samples[i+0] = CLAMP(samples[i+0], INT8_MIN, INT8_MAX+1) * 128;
            samples[i+1] = (samples[i+1] - 64) / 128;
            samples[i+1] = CLAMP(samples[i+1], INT8_MIN, INT8_MAX+1) * 128;

            // dc offset normlization. this subtracts the voltage level by
            // a leaky integration of it
            double sf0 = smpconv_s16_f64(samples[i+0]);
            double sf1 = smpconv_s16_f64(samples[i+1]);

            s_asamp_accum[0] += (-0.99 * s_asamp_accum[0] + sf0) * sample_len * 20.0;
            s_asamp_accum[1] += (-0.99 * s_asamp_accum[1] + sf1) * sample_len * 20.0;

            samples[i+0] -= smpconv_f64_s16(s_asamp_accum[0]);
            samples[i+1] -= smpconv_f64_s16(s_asamp_accum[1]);
        }

        SDL_PutAudioStreamData(s_astream, samples, sizeof(samples));
    }

    #undef FRAME_COUNT
    #undef NCHANNELS
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
#ifdef PLATFORM_WEB
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_CANVAS_SELECTOR, "#gameCanvas");
#elif defined(_WIN32)
    SDL_SetHint(SDL_HINT_VIDEO_FORCE_EGL, "1");
#endif

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
    {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#ifdef GL_DEBUG
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif

    const int scr_w = SCREEN_WIDTH * WINDOW_SCALE;
    const int scr_h = SCREEN_HEIGHT * WINDOW_SCALE;
    const SDL_WindowFlags win_flags =
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    
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

#ifdef USE_GLAD
    if (!gladLoadGLES2Loader((void *)SDL_GL_GetProcAddress))
    {
        SDL_Log("OpenGL loader failed!");
        return SDL_APP_FAILURE;
    }
#endif

    // set up audio
    SDL_AudioSpec spec = (SDL_AudioSpec)
    {
        .channels = 2,
        .format = SDL_AUDIO_S16,
        .freq = SAMPLE_RATE
    };
    s_astream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                          &spec, NULL, NULL);
    if (!s_astream)
    {
        SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_ResumeAudioStreamDevice(s_astream);

    mplay_set_sample_rate(SAMPLE_RATE);
    psg_set_sample_rate(SAMPLE_RATE);

    SDL_GL_MakeCurrent(s_window, s_gl);
    SDL_GL_SetSwapInterval(1);
    
#ifdef GL_DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(gl_debug_output, NULL);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL,
                          GL_TRUE);
#endif

    if (!gfx_state_init())
        return SDL_APP_FAILURE;

    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i)
        s_gfx_state.screen_pixels[i] = 0xFF000000;
    
    platform_app_init();

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    switch (event->type)
    {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */

    case SDL_EVENT_KEY_DOWN:
    {
        uint k = get_key_input_flag(event->key.key);
        if (k)
            s_key_input &= ~k;

        break;
    }
        
    case SDL_EVENT_KEY_UP:
    {
        uint k = get_key_input_flag(event->key.key);
        if (k)
            s_key_input |= k;

        break;
    }
    }
    
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    u64 frame_start_us = SDL_GetTicksNS() / 1000;

    REG_KEYINPUT = s_key_input;
    platform_app_frame();

    audio_update();

    g_display_buffer = s_gfx_state.screen_pixels;
    display_update();

    glBindTexture(GL_TEXTURE_2D, s_gfx_state.screen_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                    GL_RGBA, GL_UNSIGNED_BYTE, s_gfx_state.screen_pixels);

    int win_sx, win_sy;
    SDL_GetWindowSizeInPixels(s_window, &win_sx, &win_sy);

    glViewport(0, 0, win_sx, win_sy);

    // not really any need to clear, so why not.
    // glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    // glClear(GL_COLOR_BUFFER_BIT);
    
    // activate shader
    glUseProgram(s_gfx_state.display_prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_gfx_state.screen_tex);

    {
        GLint loc = glGetUniformLocation(s_gfx_state.display_prog, "u_matrix");
        float mat[16] = {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        };

        glUniformMatrix4fv(loc, 1, GL_FALSE, mat);
    }

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

    u64 frame_end_us = SDL_GetTicksNS() / 1000;
    u64 frame_len_us = frame_end_us - frame_start_us;
    // fprintf(stderr, "frame time: %.3f ms\n", (double)frame_len_us / 1000);

    SDL_GL_SwapWindow(s_window);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    glDeleteTextures(1, &s_gfx_state.screen_tex);
    glDeleteBuffers(1, &s_gfx_state.display_quad);
    glDeleteBuffers(1, &s_gfx_state.display_quad);
    glDeleteProgram(s_gfx_state.display_prog);

    mplay_deinit();
}