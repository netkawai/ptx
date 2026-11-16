#include "sdl_private.h"

#include <SDL3/SDL_stdinc.h>

void ptx_sdl_fill_rounded_rect(SDL_Renderer *renderer,
                               SDL_FRect rect, float radius) {
    if (rect.w <= 0.0f || rect.h <= 0.0f) return;

    float half = (rect.w < rect.h ? rect.w : rect.h) / 2.0f;
    if (radius > half) radius = half;
    if (radius <= 0.5f) {
        SDL_RenderFillRect(renderer, &rect);
        return;
    }

    /* Middle body, then two half-disc caps scanned row by row. */
    SDL_FRect body = { rect.x, rect.y + radius, rect.w, rect.h - radius * 2.0f };
    if (body.h > 0.0f) SDL_RenderFillRect(renderer, &body);

    int rows = (int)(radius + 0.5f);
    for (int i = 0; i < rows; i++) {
        float dy = radius - (float)i - 0.5f;
        float sq = radius * radius - dy * dy;
        float inset = radius - (sq > 0.0f ? SDL_sqrtf(sq) : 0.0f);
        SDL_FRect top = { rect.x + inset, rect.y + (float)i, rect.w - inset * 2.0f, 1.0f };
        SDL_FRect bottom = { top.x, rect.y + rect.h - (float)i - 1.0f, top.w, 1.0f };
        if (top.w > 0.0f) SDL_RenderFillRect(renderer, &top);
        if (bottom.w > 0.0f) SDL_RenderFillRect(renderer, &bottom);
    }
}
