/******************************************************************************
 * File Name:    catclock_atlas.h
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
 * License: Open Source / Educational - Preserve attribution upon redistribution.
 *****************************************************************************/

#ifndef CATCLOCK_ATLAS_H
#define CATCLOCK_ATLAS_H

#include "catclock_shared.h"
#include <SDL3/SDL.h>

void CatClock_OnWindowResize(SDL_WindowEvent* resize_event, CatClock_AppContext* ctx,
							 SDL_Renderer* renderer);
void CatClock_SynchronizePipelineAtlases(SDL_Renderer** renderer_ptr, CatClock_AppContext* ctx,
										 float sway_deg, int hour_phase, int minute_phase,
										 int second_phase);
void CatClock_DestroyComputeAtlas(CatClock_ComputeAtlas* atlas);
void CatClock_RebakeComputeAtlas(SDL_Renderer* renderer, CatClock_ComputeAtlas* atlas,
								 int cell_base_w, int cell_base_h, int total_frames, int cols,
								 CatClock_ShaderCallback shader, void* userdata);

/* Asset Layer Typings */
#define HAND_TYPE_HOUR 0
#define HAND_TYPE_MINUTE 1
#define HAND_TYPE_SECOND 2

SDL_Texture* CatClock_CompileEyesAtlasRGBA4444(SDL_Renderer* renderer,
											   const unsigned char* xbm_mask_bytes,
											   const unsigned char* pupil_alpha_src, int mask_w,
											   int mask_h, int total_frames,
											   SDL_Color eyes_mask_color, SDL_Color pupil_color);

/* Generalized Scale-Aware Pre-Baked Texture Blueprint */
typedef struct {
	SDL_Texture* texture;
	int total_frames;
	int base_cell_w;
	int base_cell_h;
} AtlasPipelineLayer;

/* Native Framework Architecture Pipeline Hooks */
int CompareFloats(const void* a, const void* b);
void DrawStaticAssetLayer(SDL_Renderer* renderer, int layer_id);
void DrawPrebakedOutlineLayer(SDL_Renderer* renderer);
SDL_HitTestResult WidgetWindowHitTestCallback(SDL_Window* win, const SDL_Point* area, void* data);

/* Shared Abstract Texture Management Pipeline Interfaces */
void ResetAtlasPipelineScales(void);
void InitHandTextureAtlases(SDL_Renderer* renderer);
void DestroyHandTextureAtlases(void);
void RenderBakedClockHandToPipeline(SDL_Renderer* renderer, int hand_type, int position_index);

/* Main Loop Linker Compatibility Remappings */
void BakeClockHandsAtlases(SDL_Renderer* renderer);
void FreeClockHandsAtlases(void);

void CatClock_DiagnosticShotDump(SDL_Renderer* renderer, CatClock_ComputeAtlas* atlas);

/* Command-line subsystem processing links */
void PrintHelpDocumentation(const char* program_name);
bool HelperParseHexColor(const char* hex_str, SDL_Color* out_color);
void ParseCommandLineArguments(int argc, char* argv[], CatClock_AppContext* ctx);

#endif /* CATCLOCK_ATLAS_H */
