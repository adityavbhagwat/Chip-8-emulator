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
    uint8_t scale_factor; //amoutn to which we want to scale our window
}config_t;
typedef enum{
    QUIT,
    RUNNING,
    PAUSED,
}emulator_state_t;
typedef struct
{
    emulator_state_t state;
    uint8_t ram[4096];
    bool display[64*32];    // Emulate original CHIP8 resolution pixels
    uint32_t pixel_color[64*32];    // CHIP8 pixel colors to draw 
    uint16_t stack[12];     // Subroutine stack
    uint16_t *stack_ptr;
    uint8_t V[16];          // Data registers V0-VF
    uint16_t I;             // Index register
    uint16_t PC;            // Program Counter
    uint8_t delay_timer;    // Decrements at 60hz when >0
    uint8_t sound_timer;    // Decrements at 60hz and plays tone when >0 
    bool keypad[16];        // Hexadecimal keypad 0x0-0xF
    const char *rom_name;   // Currently running ROM
}chip8_t;
//initialise sdl subsystems 
bool init_sdl(sdl_t *sdl,const config_t config)
{
    if((SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER) != 0))
    {
        SDL_Log("COULD NOT INITIALISE SDL SUBSYSTEMS, TRY AGAIN! %s \n",SDL_GetError());
        return false;
    }
    sdl->window = SDL_CreateWindow("Chip-8_Emulator",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,config.window_width*config.scale_factor,config.window_height*config.scale_factor,0);

    if(!sdl->window)
    {
        SDL_Log("Could not make a window %s \n",SDL_GetError());
        return false;
    }
    sdl->renderer = SDL_CreateRenderer(sdl->window,-1,SDL_RENDERER_ACCELERATED);
    if(!sdl->renderer)
    {
        SDL_Log("Could not make a renderer %s \n",SDL_GetError());
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
    config->fg_color=0xFFFFFFFF; //yellow
    config->bg_color = 0xFFFF00FF; //black
    config->scale_factor = 20;
    for (int i = 1; i < argc; i++)
    {
        (void)argv[i];
    }
    
    return true;
}
// clear screen to sdl background 
void clear_screen(const config_t config,sdl_t sdl)
{
    const uint8_t r = (config.bg_color >> 24)&(0xFF);
    const uint8_t g = (config.bg_color >> 16)&(0xFF);
    const uint8_t b = (config.bg_color >> 8)&(0xFF);
    const uint8_t a = (config.bg_color >> 0)&(0xFF);
    SDL_SetRenderDrawColor(sdl.renderer,r,g,b,a);
    SDL_RenderClear(sdl.renderer);


}
void update_screen(sdl_t sdl)
{
    SDL_RenderPresent(sdl.renderer);
}
void handle_input(chip8_t *chip8)
{
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        switch (event.type)
        {
             case SDL_QUIT:
                chip8->state = QUIT;
                return;
            
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym)
                {
                    case SDLK_ESCAPE:
                        chip8->state = QUIT;
                        return;
                    case SDLK_SPACE:
                        // Space bar
                        if (chip8->state == RUNNING) {
                            chip8->state = PAUSED;  // Pause
                            puts("==== PAUSED ====");
                        } else {
                            chip8->state = RUNNING; // Resume
                             puts("==== RESUME ====");
                        }
                        break;
                    default:
                        break;
                }
                break;

           

            default:
                break;
        }
    }
}
// Initialize CHIP8 machine
bool init_chip8(chip8_t *chip8, const config_t config, const char rom_name[]) {
    const uint32_t entry_point = 0x200; // CHIP8 Roms will be loaded to 0x200
    const uint8_t font[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0,   // 0   
        0x20, 0x60, 0x20, 0x20, 0x70,   // 1  
        0xF0, 0x10, 0xF0, 0x80, 0xF0,   // 2 
        0xF0, 0x10, 0xF0, 0x10, 0xF0,   // 3
        0x90, 0x90, 0xF0, 0x10, 0x10,   // 4    
        0xF0, 0x80, 0xF0, 0x10, 0xF0,   // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0,   // 6
        0xF0, 0x10, 0x20, 0x40, 0x40,   // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0,   // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0,   // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90,   // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0,   // B
        0xF0, 0x80, 0x80, 0x80, 0xF0,   // C
        0xE0, 0x90, 0x90, 0x90, 0xE0,   // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0,   // E
        0xF0, 0x80, 0xF0, 0x80, 0x80,   // F
    };

    // Initialize entire CHIP8 machine
    memset(chip8, 0, sizeof(chip8_t));

    // Load font 
    memcpy(&chip8->ram[0], font, sizeof(font));
   
    // Open ROM file
    FILE *rom = fopen(rom_name, "rb");
    if (!rom) {
        SDL_Log("Rom file %s is invalid or does not exist\n", rom_name);
        return false;
    }

    // Get/check rom size
    fseek(rom, 0, SEEK_END);
    const size_t rom_size = ftell(rom);
    const size_t max_size = sizeof chip8->ram - entry_point;
    rewind(rom);

    if (rom_size > max_size) {
        SDL_Log("Rom file %s is too big! Rom size: %llu, Max size allowed: %llu\n", 
                rom_name, (long long unsigned)rom_size, (long long unsigned)max_size);
        return false;
    }

    // Load ROM
    if (fread(&chip8->ram[entry_point], rom_size, 1, rom) != 1) {
        SDL_Log("Could not read Rom file %s into CHIP8 memory\n", 
                rom_name);
        return false;
    }
    fclose(rom);

    // Set chip8 machine defaults
    chip8->state = RUNNING;     // Default machine state to on/running
    chip8->PC = entry_point;    // Start program counter at ROM entry point
    chip8->rom_name = rom_name;
    chip8->stack_ptr = &chip8->stack[0];
    memset(&chip8->pixel_color[0], config.bg_color, sizeof chip8->pixel_color); // Init pixels to bg color

    return true;    // Success
}

int main(int argc,const char **argv)
{
      if (argc < 2) {
       fprintf(stderr, "Usage: %s <rom_name>\n", argv[0]);
       exit(EXIT_FAILURE);
    }
    //initilialise config details
    config_t config = {0};
    chip8_t chip8 = {0};
    const char *rom_name = argv[1];
    if (!init_chip8(&chip8, config, rom_name)) exit(EXIT_FAILURE);
    if(!set_config_from_args(&config,argc,argv))
    exit(EXIT_FAILURE);
    //initialise SDL 

    sdl_t sdl = {0};
    if(! init_sdl(&sdl,config))
    {
        exit(EXIT_FAILURE);
    }
    
    //clear the screen
    clear_screen(config,sdl);
    //main loop
    while(chip8.state != QUIT)
    {
        //delay the screen 
        SDL_Delay(16);
        //handle user input 
        handle_input(&chip8);
        if(chip8.state == PAUSED)
        continue;
        update_screen(sdl);
    }
   

    //final cleanup
    final_cleanup(&sdl);

    return 0;
}
