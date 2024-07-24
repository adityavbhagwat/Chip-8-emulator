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
typedef struct {
    uint16_t opcode;
    uint16_t NNN;   // 12 bit address/constant
    uint8_t NN;     // 8 bit constant
    uint8_t N;      // 4 bit constant
    uint8_t X;      // 4 bit register identifier
    uint8_t Y;      // 4 bit register identifier
} instruction_t;

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
    instruction_t inst;  // Current instruction
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
    config->bg_color = 0x000000FF; //black
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
    const uint8_t r = (config.bg_color >> 24) & 0xFF;
    const uint8_t g = (config.bg_color >> 16) & 0xFF;
    const uint8_t b = (config.bg_color >>  8) & 0xFF;
    const uint8_t a = (config.bg_color >>  0) & 0xFF;

    SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
    SDL_RenderClear(sdl.renderer);


}
void update_screen(const sdl_t sdl, const config_t config, chip8_t *chip8) {
    SDL_Rect rect = {.x = 0, .y = 0, .w = config.scale_factor, .h = config.scale_factor};
    const uint8_t fg_r = (config.fg_color >> 24) & 0xFF;
    const uint8_t fg_g = (config.fg_color >> 16) & 0xFF;
    const uint8_t fg_b = (config.fg_color >>  8) & 0xFF;
    const uint8_t fg_a = (config.fg_color >>  0) & 0xFF;
    // Grab bg color values to draw outlines
    const uint8_t bg_r = (config.bg_color >> 24) & 0xFF;
    const uint8_t bg_g = (config.bg_color >> 16) & 0xFF;
    const uint8_t bg_b = (config.bg_color >>  8) & 0xFF;
    const uint8_t bg_a = (config.bg_color >>  0) & 0xFF;

    // Loop through display pixels, draw a rectangle per pixel to the SDL window
    for (uint32_t i = 0; i < sizeof chip8->display; i++) {
        // Translate 1D index i value to 2D X/Y coordinates
        // X = i % window width
        // Y = i / window width
        rect.x = (i % config.window_width) * config.scale_factor;
        rect.y = (i / config.window_width) * config.scale_factor;

        if (chip8->display[i]) {
            // Pixel is on, draw foreground color
           
                // Lerp towards fg_color
                SDL_SetRenderDrawColor(sdl.renderer,fg_r,fg_g,fg_b,fg_a);
                SDL_RenderFillRect(sdl.renderer, &rect);
            

           
        } else {
            // Pixel is off, draw background color
            SDL_SetRenderDrawColor(sdl.renderer,bg_r,bg_g,bg_b,bg_a);
            SDL_RenderFillRect(sdl.renderer, &rect);
        }
    }

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
void emulate_instruction(chip8_t *chip8,const config_t config)
{
    chip8->inst.opcode = (chip8->ram[chip8->PC] << 8) | chip8->ram[chip8->PC+1];
    chip8->PC += 2; // Pre-increment program counter for next opcode

    // Fill out current instruction format
    chip8->inst.NNN = chip8->inst.opcode & 0x0FFF;
    chip8->inst.NN = chip8->inst.opcode & 0x0FF;
    chip8->inst.N = chip8->inst.opcode & 0x0F;
    chip8->inst.X = (chip8->inst.opcode >> 8) & 0x0F;
    chip8->inst.Y = (chip8->inst.opcode >> 4) & 0x0F;
    switch ((chip8->inst.opcode >> 12) & 0x0F) {
        case 0x00:
            if (chip8->inst.NN == 0xE0) {
                // 0x00E0: Clear the screen
                memset(&chip8->display[0], false, sizeof chip8->display);
               

            } else if (chip8->inst.NN == 0xEE) {
                // 0x00EE: Return from subroutine
                // Set program counter to last address on subroutine stack ("pop" it off the stack)
                //   so that next opcode will be gotten from that address.
                chip8->PC = *--chip8->stack_ptr;

            } else {
                // Unimplemented/invalid opcode, may be 0xNNN for calling machine code routine for RCA1802
            }

            break;

        case 0x01:
            // 0x1NNN: Jump to address NNN
            chip8->PC = chip8->inst.NNN;    // Set program counter so that next opcode is from NNN
            break;

        case 0x02:
            // 0x2NNN: Call subroutine at NNN
            // Store current address to return to on subroutine stack ("push" it on the stack)
            //   and set program counter to subroutine address so that the next opcode
            //   is gotten from there.
            *chip8->stack_ptr++ = chip8->PC;  
            chip8->PC = chip8->inst.NNN;
            break;

        case 0x03:
            // 0x3XNN: Check if VX == NN, if so, skip the next instruction
            if (chip8->V[chip8->inst.X] == chip8->inst.NN)
                chip8->PC += 2;       // Skip next opcode/instruction
            break;

        case 0x04:
            // 0x4XNN: Check if VX != NN, if so, skip the next instruction
            if (chip8->V[chip8->inst.X] != chip8->inst.NN)
                chip8->PC += 2;       // Skip next opcode/instruction
            break;

        case 0x05:
            // 0x5XY0: Check if VX == VY, if so, skip the next instruction
            if (chip8->inst.N != 0) break; // Wrong opcode

            if (chip8->V[chip8->inst.X] == chip8->V[chip8->inst.Y])
                chip8->PC += 2;       // Skip next opcode/instruction
            
            break;

        case 0x06:
            // 0x6XNN: Set register VX to NN
            chip8->V[chip8->inst.X] = chip8->inst.NN;
            break;

        case 0x07:
            // 0x7XNN: Set register VX += NN
            chip8->V[chip8->inst.X] += chip8->inst.NN;
            break;

      

        case 0x09:
            // 0x9XY0: Check if VX != VY; Skip next instruction if so
            if (chip8->V[chip8->inst.X] != chip8->V[chip8->inst.Y])
                chip8->PC += 2;
            break;

        case 0x0A:
            // 0xANNN: Set index register I to NNN
            chip8->I = chip8->inst.NNN;
            break;

        case 0x0B:
            // 0xBNNN: Jump to V0 + NNN
            chip8->PC = chip8->V[0] + chip8->inst.NNN;
            break;

        case 0x0C:
            // 0xCXNN: Sets register VX = rand() % 256 & NN (bitwise AND)
            chip8->V[chip8->inst.X] = (rand() % 256) & chip8->inst.NN;
            break;

        case 0x0D: {
            // 0xDXYN: Draw N-height sprite at coords X,Y; Read from memory location I;
            //   Screen pixels are XOR'd with sprite bits, 
            //   VF (Carry flag) is set if any screen pixels are set off; This is useful
            //   for collision detection or other reasons.
            uint8_t X_coord = chip8->V[chip8->inst.X] % config.window_width;
            uint8_t Y_coord = chip8->V[chip8->inst.Y] % config.window_height;
            const uint8_t orig_X = X_coord; // Original X value

            chip8->V[0xF] = 0;  // Initialize carry flag to 0

            // Loop over all N rows of the sprite
            for (uint8_t i = 0; i < chip8->inst.N; i++) {
                // Get next byte/row of sprite data
                const uint8_t sprite_data = chip8->ram[chip8->I + i];
                X_coord = orig_X;   // Reset X for next row to draw

                for (int8_t j = 7; j >= 0; j--) {
                    // If sprite pixel/bit is on and display pixel is on, set carry flag
                    bool *pixel = &chip8->display[Y_coord * config.window_width + X_coord]; 
                    const bool sprite_bit = (sprite_data & (1 << j));

                    if (sprite_bit && *pixel) {
                        chip8->V[0xF] = 1;  
                    }

                    // XOR display pixel with sprite pixel/bit to set it on or off
                    *pixel ^= sprite_bit;

                    // Stop drawing this row if hit right edge of screen
                    if (++X_coord >= config.window_width) break;
                }

                // Stop drawing entire sprite if hit bottom edge of screen
                if (++Y_coord >= config.window_height) break;
            }
            //chip8->draw = true; // Will update screen on next 60hz tick
            break;
        }

        // case 0x0E:
        //     if (chip8->inst.NN == 0x9E) {
        //         // 0xEX9E: Skip next instruction if key in VX is pressed
        //         if (chip8->keypad[chip8->V[chip8->inst.X]])
        //             chip8->PC += 2;

        //     } else if (chip8->inst.NN == 0xA1) {
        //         // 0xEX9E: Skip next instruction if key in VX is not pressed
        //         if (!chip8->keypad[chip8->V[chip8->inst.X]])
        //             chip8->PC += 2;
        //     }
        //     break;

      
            
        default:
            break;  // Unimplemented or invalid opcode
    }
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
        
        //handle user input 
        handle_input(&chip8);
        if(chip8.state == PAUSED)
        continue;
        emulate_instruction(&chip8,config);
        SDL_Delay(16);
        update_screen(sdl,config,&chip8);
    }
   

    //final cleanup
    final_cleanup(&sdl);

    return 0;
}
