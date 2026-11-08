#include <SDL.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>

namespace {
    constexpr int kWindowWidth = 240 * 2;
    constexpr int kWindowHeight = 160 * 2;

    struct SDLSession {
        ~SDLSession() { SDL_Quit(); }
    };
}

auto main(int argc, char **argv) -> int {
    const bool smokeTest = argc == 2 && std::strcmp(argv[1], "--smoke-test") == 0;
    if (argc != 1 && !smokeTest) {
        std::cerr << "Usage: gba_sdl [--smoke-test]\n";
        return 2;
    }

    const SDLSession session{};
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return EXIT_FAILURE;
    }

    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(
        SDL_CreateWindow("GBA-EMU", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         kWindowWidth, kWindowHeight, SDL_WINDOW_SHOWN),
        SDL_DestroyWindow);
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        return EXIT_FAILURE;
    }

    if (smokeTest) {
        SDL_Event quit{};
        quit.type = SDL_QUIT;
        if (SDL_PushEvent(&quit) != 1) {
            std::cerr << "SDL_PushEvent failed: " << SDL_GetError() << '\n';
            return EXIT_FAILURE;
        }
    }

    for (;;) {
        SDL_Event event{};
        if (SDL_WaitEvent(&event) == 0) {
            std::cerr << "SDL_WaitEvent failed: " << SDL_GetError() << '\n';
            return EXIT_FAILURE;
        }
        if (event.type == SDL_QUIT ||
            (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)) {
            return EXIT_SUCCESS;
        }
    }
}
