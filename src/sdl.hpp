
#define SDL_DISABLE_IMMINTRIN_H
#define SDL_DISABLE_XMMINTRIN_H
#define SDL_DISABLE_EMMINTRIN_H

#ifdef WIN32
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif
