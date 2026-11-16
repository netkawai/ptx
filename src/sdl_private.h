#ifndef PTX_SDL_PRIVATE_H
#define PTX_SDL_PRIVATE_H

/*
 * ptx private: SDL drawing helpers shared by the mini-toolkit widgets.
 *
 * Not part of the public (GTK4-mirroring) API; widgets such as
 * PtxScrollbar use these so they all draw in the same Adwaita style.
 */

#include <SDL3/SDL.h>

/* ptx extension: fill a rectangle with rounded corners. SDL3 has no rounded
 * rectangle primitive; Adwaita tiles are drawn with this. `radius` is
 * clamped to half the shorter side. */
void ptx_sdl_fill_rounded_rect(SDL_Renderer *renderer,
                               SDL_FRect rect, float radius);

#endif /* PTX_SDL_PRIVATE_H */
