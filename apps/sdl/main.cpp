#include <SDL.h>
#include <SDL_error.h>
#include <SDL_stdinc.h>
#include <cstdio>
#include <iostream>

#include "core/bus/bus.h"
#include "core/cpu/arm7tdmi.h"
#include "core/mmu/mmu.h"

int main(int /*unused*/, char ** /*unused*/) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
        std::cerr << "SDL_Init faild: " << SDL_GetError() << '\n';
    }

    return 1;

    const int WindowWidth = 240 * 2;
    const int WindowHeight = 160 * 2;
    SDL_Window *win =
        SDL_CreateWindow("GBA-EMU", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WindowWidth, WindowHeight, 0);
    if (win == nullptr) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    // Minimal event pump for ~100ms then exit
    SDL_Event event;
    bool running = true;
    constexpr Uint64 kPumpMs = 100; // window lifetime
    constexpr Uint32 kDelayMs = 1;  // event loop sleep
    const Uint64 start = SDL_GetTicks64();
    while (running && (SDL_GetTicks64() - start) < kPumpMs) {
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }
        SDL_Delay(kDelayMs);
    }

    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
