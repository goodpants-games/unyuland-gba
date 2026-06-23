#include <stdbool.h>

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_video.h>

#include <tonc_video.h>
#include <glad/gl.h>

#define WINDOW_SCALE 3

void platform_app_init(void);
void platform_app_frame(void);

static SDL_Window *s_window = NULL;
static SDL_GLContext s_gl = NULL;

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

    const int scr_w = SCREEN_WIDTH * WINDOW_SCALE;
    const int scr_h = SCREEN_HEIGHT * WINDOW_SCALE;

    s_window = SDL_CreateWindow("Unyuland", scr_w, scr_h, SDL_WINDOW_OPENGL);
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

    if (!gladLoadGL(SDL_GL_GetProcAddress))
    {
        SDL_Log("OpenGL loader failed!");
        return SDL_APP_FAILURE;
    }

    SDL_GL_MakeCurrent(s_window, s_gl);

    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    
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
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    SDL_GL_SwapWindow(s_window);

    platform_app_frame();
    // const double now = ((double)SDL_GetTicks()) / 1000.0;  /* convert from milliseconds to seconds. */
    // /* choose the color for the frame we will draw. The sine wave trick makes it fade between colors smoothly. */
    // const float red = (float) (0.5 + 0.5 * SDL_sin(now));
    // const float green = (float) (0.5 + 0.5 * SDL_sin(now + SDL_PI_D * 2 / 3));
    // const float blue = (float) (0.5 + 0.5 * SDL_sin(now + SDL_PI_D * 4 / 3));
    // SDL_SetRenderDrawColorFloat(renderer, red, green, blue, SDL_ALPHA_OPAQUE_FLOAT);  /* new color, full alpha. */

    // /* clear the window to the draw color. */
    // SDL_RenderClear(renderer);

    // /* put the newly-cleared rendering on the screen. */
    // SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    /* SDL will clean up the window/renderer for us. */
}