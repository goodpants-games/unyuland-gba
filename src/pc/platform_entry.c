#include <stdbool.h>
#include <stdio.h>
#include <log.h>
#include <stdlib.h>

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

// if GL_ES or GL_DESKTOP was not already defined by the build system, then
// define it based off the given platform. although, currently, the makefile
// does not support defining these, so the condition will always evaluate to
// true.
#if !(defined(GL_ES) || defined(GL_DESKTOP))
#   ifdef PLATFORM_WEB
#       define GL_ES
#   else
#       define GL_DESKTOP
#   endif
#endif

#if defined(GL_ES)
#   include <SDL3/SDL_opengles2.h>
#elif defined(GL_DESKTOP)
#   include "glad/glad.h"
//# define GL_DEBUG
#endif

#include <tonc_video.h>
#include <tonc_math.h>
#include <modplay.h>
#include <platctl.h>
#include <psg_ctl.h>
#include "display.h"
#include "audioutil.h"

#define DEF_WINDOW_SCALE 3
#define SAMPLE_RATE      32768

#define SECS(x) (s64)((x) * 1000000000)
#define FRAME_LENGTH_NS SECS(1.0 / 60.0)
#define DT_SNAP_THRESH  SECS(0.002)

// these functions are defined by src/main/main.c
void platform_app_init(void);
void platform_app_frame(void);

static SDL_Window *s_window = NULL;
static SDL_AudioStream *s_astream = NULL;
static SDL_GLContext s_gl = NULL;
static bool s_is_fullscreen = false;

// leaky integrator for dc offset removal
static double s_asamp_accum[2] = { 0.0, 0.0 };

static uint s_key_input = 0x3FF;

struct gfx_state
{
    GLuint screen_tex;
    GLuint display_prog;
    GLuint display_quad;
    GLuint display_quad_idx;
#ifdef GL_DESKTOP
    GLuint vao;
#endif

    u32 screen_pixels[SCREEN_WIDTH * SCREEN_HEIGHT];
}
static s_gfx_state;










//------------------------------------------------------------------------------
// graphics
//------------------------------------------------------------------------------
#pragma region graphics

#if defined(GL_ES)
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
#elif defined(GL_DESKTOP)
static const char *VS_SOURCE =
    "#version 330 core\n"
    "in vec2 a_position;\n"
    "in vec2 a_texcoord;\n"
    "out vec2 v_texcoord;\n"
    "uniform mat4 u_matrix;\n"
    "void main() {\n"
    "   v_texcoord = a_texcoord;\n"
    "   gl_Position = u_matrix * vec4(a_position, 0.0, 1.0);\n"
    "}\n";

static const char *FS_SOURCE =
    "#version 330 core\n"
    "in vec2 v_texcoord;\n"
    "out vec4 o_color;\n"
    "uniform sampler2D u_texture;\n"
    "void main() {\n"
    "   o_color = texture(u_texture, v_texcoord);\n"
    "}\n";
#endif

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

#ifdef GL_DESKTOP
    // OpenGL 3.3 Core requires use of a VAO, so just generate and bind a
    // default one.
    glGenVertexArrays(1, &s_gfx_state.vao);
    glBindVertexArray(s_gfx_state.vao);
#endif

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

static void gfx_update(void)
{
    int win_sx_i, win_sy_i;
    float win_sx, win_sy;
    SDL_GetWindowSizeInPixels(s_window, &win_sx_i, &win_sy_i);
    win_sx = (float) win_sx_i;
    win_sy = (float) win_sy_i;

    glViewport(0, 0, win_sx_i, win_sy_i);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // activate shader
    glUseProgram(s_gfx_state.display_prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_gfx_state.screen_tex);

    // projection matrix
    float win_ar = win_sy / win_sx;
    float scr_ar = (float)SCREEN_HEIGHT / SCREEN_WIDTH;

    GLint uloc = glGetUniformLocation(s_gfx_state.display_prog, "u_matrix");
    float mat[16] = {
        win_ar / scr_ar, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    glUniformMatrix4fv(uloc, 1, GL_FALSE, mat);

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
}

#pragma endregion graphics










//------------------------------------------------------------------------------
// audio
//------------------------------------------------------------------------------
#pragma region audio

static unsigned int s_audio_volume = PLATCTL_VOLUME_MAX;

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
        // audio tick, not at the start of the audio frame. but I'm lazy; that
        // would require me to rework the ticking system to occur here rather
        // than in the psg playback "module". and besides, the game literally
        // only modifies these registers once, at boot.
        // also, i think the program can choose which channels DSA or DSB emit
        // to. I'm just going to assume B is on channel L and A is on channel R.
        // Dunno if this is what maxmod does actually does, but sure.
        uint psg_mix = REG_SNDDSCNT & 3;
        uint dsa_mix = REG_SNDDSCNT & SDS_A100;
        uint dsb_mix = REG_SNDDSCNT & SDS_B100;

        // https://jsgroth.dev/blog/posts/gba-audio/
        for (size_t i = 0; i < FRAME_COUNT * NCHANNELS; i += 2)
        {
            // 8-bit samples are converted to clamped 10-bit samples. also,
            // apply REG_SNDDSCNT volume control
            int dsa = mplay_samples[i+1] << (1 + dsa_mix);
            dsa = CLAMP(dsa, -0x200, 0x1FF);
            int dsb = mplay_samples[i+0] << (1 + dsb_mix);
            dsb = CLAMP(dsb, -0x200, 0x1FF);

            s16 psg0 = psg_samples[i+0];
            s16 psg1 = psg_samples[i+1];

            psg0 >>= 2 - (psg_mix % 3);
            psg1 >>= 2 - (psg_mix % 3);

            samples[i+0] = CLAMP(dsb + psg0, -0x200, 0x1FF);
            samples[i+1] = CLAMP(dsa + psg1, -0x200, 0x1FF);

            // convert 10-bit range to ~16-bit range. not actually full-range,
            // but close enough.
            samples[i+0] *= 0x40;
            samples[i+1] *= 0x40;

            // apply speaker volume
            samples[i+0] = (s16)(((s32)samples[i+0] * s_audio_volume) / PLATCTL_VOLUME_MAX);
            samples[i+1] = (s16)(((s32)samples[i+1] * s_audio_volume) / PLATCTL_VOLUME_MAX);

            // dc offset removal. this subtracts the voltage level by
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

#pragma endregion audio










//------------------------------------------------------------------------------
// platctl
//------------------------------------------------------------------------------
#pragma region platctl

static platctl_fulscr_change_watcher_f s_fulscr_change_watcher = NULL;

void platctl_set_volume(unsigned int volume)
{
    s_audio_volume = volume;
}

void platctl_set_fullscreen(bool fulscr)
{
    SDL_SetWindowFullscreen(s_window, fulscr);
}

bool platctl_get_fullscreen(void)
{
    return s_is_fullscreen;
}

void platctl_set_fullscreen_change_watcher(platctl_fulscr_change_watcher_f fun)
{
    s_fulscr_change_watcher = fun;
}

#pragma endregion platctl










//------------------------------------------------------------------------------
// lifecycle
//------------------------------------------------------------------------------
#pragma region lifecycle

// time accumulator for framerate locking
static u64 s_time_accum = 0;
static u64 s_last_frame_time = 0;

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

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
#ifdef PLATFORM_WEB
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_CANVAS_SELECTOR, "#gameCanvas");
#elif defined(_WIN32) && defined(GL_ES)
    SDL_SetHint(SDL_HINT_VIDEO_FORCE_EGL, "1");
#endif

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
    {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

#if defined(GL_ES)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#elif defined(GL_DESKTOP)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);
#   ifdef GL_DEBUG
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#   endif
#endif

    const int scr_w = SCREEN_WIDTH * DEF_WINDOW_SCALE;
    const int scr_h = SCREEN_HEIGHT * DEF_WINDOW_SCALE;
    SDL_WindowFlags win_flags =
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    // for some reason, having a non-resizable window messes with HTML5
    // fullscreen. i.e., apparently, when exiting fullscreen, the canvas's size
    // becomes 0x0 (https://github.com/libsdl-org/SDL/issues/6798)
#ifdef PLATFORM_WEB
    win_flags |= SDL_WINDOW_RESIZABLE;
#endif
    
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

#ifdef GL_DESKTOP
    if (!gladLoadGLLoader((void *)SDL_GL_GetProcAddress))
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

    // glDisable(GL_FRAMEBUFFER_SRGB);
    
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

    s_last_frame_time = SDL_GetTicksNS();

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    u64 cur_time = SDL_GetTicksNS();
    s64 dt_ns = cur_time - s_last_frame_time;
    s_last_frame_time = cur_time;

    // if the measured frame length is already reasonably close to 60 Hz
    // assume this is due to VSync, and don't do anything to prevent potential
    // stuttering
    if (llabs(dt_ns - FRAME_LENGTH_NS) < DT_SNAP_THRESH) {
        dt_ns = FRAME_LENGTH_NS;
    }

    // ...120 Hz
    else if (llabs(dt_ns - FRAME_LENGTH_NS / 2) < DT_SNAP_THRESH) {
        dt_ns = FRAME_LENGTH_NS / 2;
    }

    // ...240 Hz? maybe?

    s_time_accum += dt_ns;

    bool did_update = false;
    for (int iter = 0; iter < 8; ++iter)
    {
        if (s_time_accum < FRAME_LENGTH_NS) break;

        REG_KEYINPUT = s_key_input;
        platform_app_frame();
    
        did_update = true;
        s_time_accum -= FRAME_LENGTH_NS;
    }

    s_time_accum %= FRAME_LENGTH_NS;

    if (did_update)
    {
        g_display_buffer = s_gfx_state.screen_pixels;
        display_update();

        glBindTexture(GL_TEXTURE_2D, s_gfx_state.screen_tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                        GL_RGBA, GL_UNSIGNED_BYTE, s_gfx_state.screen_pixels);    
    }
                    
    audio_update();
    gfx_update();
    SDL_GL_SwapWindow(s_window);

#ifndef PLATFORM_WEB
    // if the frame finished quicker than expected, sleep for the remainder of
    // the frame. sleep for a bit less than the desired time, to account for
    // error in OS timing.
    u64 end_time = SDL_GetTicksNS();
    s64 frame_len = (s64)(end_time - cur_time);
    if (frame_len < FRAME_LENGTH_NS - DT_SNAP_THRESH)
    {
        s64 sleep_time = FRAME_LENGTH_NS - frame_len;
        if (sleep_time > SECS(0.003))
            SDL_DelayNS(sleep_time - SECS(0.003));
    }
#endif

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

    case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
        LOG_DBG("Enter fullscreen");
        s_is_fullscreen = true;
        if (s_fulscr_change_watcher)
            s_fulscr_change_watcher(true);

        break;

    case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
        LOG_DBG("Leave fullscreen");
        s_is_fullscreen = false;
        if (s_fulscr_change_watcher)
            s_fulscr_change_watcher(false);

        break;
    }
    
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    glDeleteTextures(1, &s_gfx_state.screen_tex);
    glDeleteBuffers(1, &s_gfx_state.display_quad);
    glDeleteBuffers(1, &s_gfx_state.display_quad);
    glDeleteProgram(s_gfx_state.display_prog);
#ifdef GL_DESKTOP
    glDeleteVertexArrays(1, &s_gfx_state.vao);
#endif

    mplay_deinit();
}

#pragma endregion lifecycle