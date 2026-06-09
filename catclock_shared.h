/******************************************************************************
 * File Name:    catclock_shared.h
 * Project:      catclock-sdl3 (Modernized Kit-Cat Clock Desktop Widget)
 *
 * Authorship & Collaboration:
 *   - Developed in collaborative partnership between the User and Google Gemini AI.
 *   - Core engine optimization, refactoring architecture, and porting logic
 *     engineered jointly to achieve production-grade performance.
 *
 * Attribution & Legacy:
 *   - Inspired by the classic X11/Motif 'catclock' original program.
 *   - XBM Graphic Assets derived from the historical open-source X11 layout.
 *
 * Engineering Milestones & Refactoring Pass (2026):
 *   - Ported natively to the SDL3 framework with desktop compositing support.
 *   - Designed a low-CPU runtime architecture utilizing pre-baked texture atlases.
 *   - Implemented 1-bit binary threshold rendering to optimize alpha blending.
 *   - Restored polygon scanline rasterization engines to patch geometry gaps.
 *   - Synchronized monotonic ticks with the wall clock to erase frame jitter.
 *   - Enabled adaptive pacing, focus-aware kernel sleeps, and zero-VSync spinlocks.
 *   - Added borderless transparent windows, OS-level hit-tests, and sharp nearest-pixel art integer scaling.
 *
 * License: Open Source / Educational - Preserve attribution upon redistribution.
 *****************************************************************************/

#ifndef CATCLOCK_SHARED_H
#define CATCLOCK_SHARED_H

#include <SDL3/SDL.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* GIMP Extraction Coordinates */
#define CLOCK_CENTER_X  74.0f
#define CLOCK_CENTER_Y  150.0f
#define EYES_MASK_DX    49.0f
#define EYES_MASK_DY    30.0f
#define TAIL_PIVOT_X    74.0f
#define TAIL_PIVOT_Y    210.5f
#define LEFT_EYE_AXIS_X   60.0f
#define RIGHT_EYE_AXIS_X  91.0f
#define EYE_AXIS_Y        41.0f
#define CLOCK_BOX_SIZE 64

typedef struct {
    float x, y;
} OriginalPoint;

/* Shared State Variable Flags */
extern float current_tail_angle;
extern float pupil_translation_x;
extern float eye_perspective_scale;
extern bool show_second_hand;
extern bool show_outline_border;
extern bool use_window_decorations;
extern int target_fps_limit;
extern float window_scale_factor;

/* Unified Atlas Interfaces */
int CompareFloats(const void *a, const void *b);
void InitPreflippedTextureAtlases(SDL_Renderer *renderer);
void FreePreflippedTextureAtlases(void);
void DrawStaticAssetLayer(SDL_Renderer *renderer, int layer_id);
void DrawPrebakedOutlineLayer(SDL_Renderer *renderer);

/* Hand Baking Architecture Interfaces */
void BakeClockHandsAtlases(SDL_Renderer *renderer);
void FreeClockHandsAtlases(void);
void DrawBakedClockHand(SDL_Renderer *renderer, int hand_type, int position_index);

void RenderOriginalThickSwayingTail(SDL_Renderer *renderer, float cx, float cy, float angle_deg, SDL_Color color, bool inflate_mode);
void RenderAuthenticOriginalEyes(SDL_Renderer *renderer, float swing_phase, SDL_Color color);

SDL_HitTestResult WidgetWindowHitTestCallback(SDL_Window *win, const SDL_Point *area, void *data);

#endif
