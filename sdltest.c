#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdbool.h>
typedef struct 
{
    SDL_Window *window; // holds pointer refernec to window 
    SDL_Renderer *renderer; //holds ref ptr to renderer 
}sdl_t;
typedef struct 
{
    uint32_t window_width;
    uint32_t window_height;
    uint32_t fg_color;
    uint32_t bg_color;
}config_t;

//initialise sdl subsystems 
bool init_sdl(sdl_t *sdl,const config_t config)
{
    if((SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER) != 0))
    {
        SDL_Log("COULD NOT INITIALISE SDL SUBSYSTEMS, TRY AGAIN! %s \n",SDL_GetError());
        return false;
    }
    sdl->window = SDL_CreateWindow("Chip-8_Emulator",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,config.window_width,config.window_height,0);

    if(!sdl->window)
    {
        SDL_Log('Could not make a window %s \n',SDL_GetError());
        return false;
    }
    sdl->renderer = SDL_CreateRenderer(sdl->window,-1,SDL_RENDERER_ACCELERATED);
    if(!sdl->renderer)
    {
        SDL_Log('Could not make a renderer %s \n',SDL_GetError());
        return false;
    }
    return true; //Success
}
void final_cleanup(sdl_t *sdl)
{
    SDL_DestroyRenderer(sdl->renderer);
    SDL_DestroyWindow(sdl->window);
    SDL_Quit(); //SHUT DOWN SDL SUB SYSTEMS 

}
bool set_config_from_args(config_t *config,const int argc,const char** argv)
{
    config->window_height = 32;
    config->window_width = 64;
    config->fg_color=0xFFFF00FF; //yellow
    config->bg_color = 0x00000000; //black
    for (int i = 1; i < argc; i++)
    {
        (void)argv[i];
    }
    
    return true
}
int main(int argc, char **argv)
{
    //initilialise config details
    config_t config = {0};
    if(!set_config_from_args(&config,argc,argv))
    exit(EXIT_FAILURE);
    //initialise SDL 

    sdl_t sdl = {0};
    if(!init_sdl(&sdl,config))
    {
        exit(EXIT_FAILURE);
    }
    //clear the screen
    SDL_RenderClear(sdl.renderer);

    //final cleanup
    final_cleanup(&sdl);

    return 0;
}
