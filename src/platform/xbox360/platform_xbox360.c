#include "platform_xbox360.h"
#include "font.h"
#include "xui_ui.h"
#include <xtl.h>
#include <xonline.h>
#include <d3d9.h>
#include <d3dx9.h>
#include "image.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define XBOX360_SCREEN_WIDTH 1280
#define XBOX360_SCREEN_HEIGHT 720
#define CURSOR_SIZE 32
#define CURSOR_HOT_X 2
#define CURSOR_HOT_Y 2
#define JOYSTICK_SPEED 600.0f
#define JOYSTICK_ACCEL 2.0f

static bool g_xbox_initialized = false;
static XINPUT_STATE g_controller_state[4];
static WORD g_prev_buttons[4] = {0, 0, 0, 0};

static Direct3D* g_d3d = NULL;
static D3DDevice* g_device = NULL;
static D3DTexture* g_fb_texture = NULL;
static D3DTexture* g_cursor_texture = NULL;
static int g_fb_tex_w = 0;
static int g_fb_tex_h = 0;
static bool g_d3d_ready = false;
static D3DVertexDeclaration* g_vd = NULL;
static D3DVertexShader* g_vs = NULL;
static D3DPixelShader* g_ps = NULL;

static float g_cursor_x = 640.0f;
static float g_cursor_y = 360.0f;
static float g_cursor_vel_x = 0.0f;
static float g_cursor_vel_y = 0.0f;
static uint64_t g_last_tick = 0;
static D3DPRESENT_PARAMETERS g_d3dpp;

typedef struct {
    float x, y;
    DWORD color;
    float u, v;
} D3DVertex;

static const char g_vs_source[] =
    "struct VS_IN { float2 Pos : POSITION; float4 Color : COLOR0; float2 Tex : TEXCOORD0; };\n"
    "struct VS_OUT { float4 Pos : POSITION; float4 Color : COLOR0; float2 Tex : TEXCOORD0; };\n"
    "VS_OUT main(VS_IN input) {\n"
    "  VS_OUT o;\n"
    "  o.Pos = float4(input.Pos.x - 0.5, input.Pos.y - 0.5, 0.0, 1.0);\n"
    "  o.Color = input.Color;\n"
    "  o.Tex = input.Tex;\n"
    "  return o;\n"
    "}\n";

static const char g_ps_source[] =
    "sampler TexSampler : register(s0);\n"
    "struct VS_OUT { float4 Pos : POSITION; float4 Color : COLOR0; float2 Tex : TEXCOORD0; };\n"
    "float4 main(VS_OUT input) : COLOR0 {\n"
    "  return tex2D(TexSampler, input.Tex) * input.Color;\n"
    "}\n";

static const BYTE CURSOR_BITMAP[CURSOR_SIZE * CURSOR_SIZE * 4] = {
/* Row 0 */  0,0,0,0, 0,0,0,0, 0,0,0,0, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 1 */  0,0,0,0, 0,0,0,0, 0,0,0,0, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 2 */  0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 3 */  0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 4 */  0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 5 */  0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 6 */  0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 7 */  0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 8 */  0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 9 */  0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 10 */ 0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 11 */ 0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 12 */ 0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 13 */ 0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 14 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 15 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 16 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 17 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 18 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 19 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 20 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 21 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 22 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 23 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 24 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 25 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 26 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 27 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 28 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 29 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 30 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* Row 31 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
};

static uint64_t xbox360_get_ticks(void);
static uint64_t xbox360_get_freq(void);

static bool compile_shaders(void) {
    HRESULT hr;
    ID3DXBuffer* vs_buf = NULL;
    ID3DXBuffer* ps_buf = NULL;
    ID3DXBuffer* errors = NULL;

    hr = D3DXCompileShader(g_vs_source, sizeof(g_vs_source) - 1,
        NULL, NULL, "main", "vs_2_0", 0, &vs_buf, &errors, NULL);
    if (FAILED(hr)) {
        if (errors) { OutputDebugString((char*)errors->GetBufferPointer()); errors->Release(); }
        return false;
    }

    hr = g_device->CreateVertexShader((DWORD*)vs_buf->GetBufferPointer(), &g_vs);
    vs_buf->Release();
    if (FAILED(hr)) return false;

    hr = D3DXCompileShader(g_ps_source, sizeof(g_ps_source) - 1,
        NULL, NULL, "main", "ps_2_0", 0, &ps_buf, &errors, NULL);
    if (FAILED(hr)) {
        if (errors) { OutputDebugString((char*)errors->GetBufferPointer()); errors->Release(); }
        if (g_vs) { g_vs->Release(); g_vs = NULL; }
        return false;
    }

    hr = g_device->CreatePixelShader((DWORD*)ps_buf->GetBufferPointer(), &g_ps);
    ps_buf->Release();
    if (FAILED(hr)) { if (g_vs) { g_vs->Release(); g_vs = NULL; } return false; }

    return true;
}

static bool cursor_texture_from_png(void) {
    if (!g_device) return false;

    const char* path = "game:\\media\\cursor.png";
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return false; }

    uint8_t* data = (uint8_t*)malloc((size_t)sz);
    if (!data) { fclose(f); return false; }

    size_t rd = fread(data, 1, (size_t)sz, f);
    fclose(f);
    if (rd != (size_t)sz) { free(data); return false; }

    ImageData* img = image_load_from_memory(data, (int)sz);
    free(data);
    if (!img) return false;

    bool ok = false;
    HRESULT hr = g_device->CreateTexture(
        img->width, img->height, 1, 0,
        D3DFMT_LIN_A8R8G8B8, D3DPOOL_DEFAULT, &g_cursor_texture, NULL);
    if (SUCCEEDED(hr)) {
        D3DLOCKED_RECT lr;
        hr = g_cursor_texture->LockRect(0, &lr, NULL, 0);
        if (SUCCEEDED(hr)) {
            for (int py = 0; py < img->height; py++) {
                DWORD* dst = (DWORD*)((BYTE*)lr.pBits + py * lr.Pitch);
                const uint8_t* src = (const uint8_t*)&img->pixels[py * img->width];
                for (int px = 0; px < img->width; px++) {
                    BYTE r = src[px * 4 + 0];
                    BYTE g = src[px * 4 + 1];
                    BYTE b = src[px * 4 + 2];
                    BYTE a = src[px * 4 + 3];
                    dst[px] = ((DWORD)a << 24) | ((DWORD)r << 16) | ((DWORD)g << 8) | (DWORD)b;
                }
            }
            g_cursor_texture->UnlockRect(0);
            ok = true;
        }
    }

    image_free(img);
    return ok;
}

static void create_cursor_texture(void) {
    if (!g_device || g_cursor_texture) return;

    if (cursor_texture_from_png()) return;

    HRESULT hr = g_device->CreateTexture(
        CURSOR_SIZE, CURSOR_SIZE, 1, 0,
        D3DFMT_LIN_A8R8G8B8, D3DPOOL_DEFAULT, &g_cursor_texture, NULL);
    if (FAILED(hr)) return;

    D3DLOCKED_RECT lr;
    hr = g_cursor_texture->LockRect(0, &lr, NULL, 0);
    if (FAILED(hr)) return;

    for (int py = 0; py < CURSOR_SIZE; py++) {
        DWORD* dst = (DWORD*)((BYTE*)lr.pBits + py * lr.Pitch);
        const BYTE* src = CURSOR_BITMAP + py * CURSOR_SIZE * 4;
        for (int px = 0; px < CURSOR_SIZE; px++) {
            BYTE b = src[px * 4 + 0];
            BYTE g = src[px * 4 + 1];
            BYTE r = src[px * 4 + 2];
            BYTE a = src[px * 4 + 3];
            dst[px] = ((DWORD)a << 24) | ((DWORD)r << 16) | ((DWORD)g << 8) | (DWORD)b;
        }
    }
    g_cursor_texture->UnlockRect(0);
}

static void ensure_fb_texture(int w, int h) {
    if (!g_device) return;
    if (g_fb_texture && g_fb_tex_w == w && g_fb_tex_h == h) return;

    if (g_fb_texture) {
        g_fb_texture->Release();
        g_fb_texture = NULL;
    }

    HRESULT hr = g_device->CreateTexture(
        w, h, 1, 0,
        D3DFMT_LIN_A8R8G8B8, D3DPOOL_DEFAULT, &g_fb_texture, NULL);
    if (FAILED(hr)) return;

    g_fb_tex_w = w;
    g_fb_tex_h = h;
}

static void setup_2d_render(void) {
    if (!g_device) return;

    g_device->SetVertexShader(g_vs);
    g_device->SetPixelShader(g_ps);
    g_device->SetVertexDeclaration(g_vd);

    g_device->SetRenderState(D3DRS_VIEWPORTENABLE, FALSE);
    g_device->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    g_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    g_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    g_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    g_device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    g_device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    g_device->SetRenderState(D3DRS_STENCILENABLE, FALSE);

    g_device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    g_device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    g_device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    g_device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
}

static bool init_d3d(void) {
    if (g_d3d_ready) return true;
    if (!g_device) return false;

    font_init_default();

    HRESULT hr;

    D3DVERTEXELEMENT9 decl[] = {
        {0,  0, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0,  8, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
        {0, 12, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
        D3DDECL_END()
    };
    hr = g_device->CreateVertexDeclaration(decl, &g_vd);
    if (FAILED(hr)) return false;

    if (!compile_shaders()) return false;

    setup_2d_render();
    create_cursor_texture();
    g_cursor_x = (float)(XBOX360_SCREEN_WIDTH / 2);
    g_cursor_y = (float)(XBOX360_SCREEN_HEIGHT / 2);
    g_last_tick = xbox360_get_ticks();
    g_d3d_ready = true;
    xui_ui_init(g_device, &g_d3dpp);
    return true;
}

static void shutdown_d3d(void) {
    xui_ui_shutdown();
    if (g_vs) { g_vs->Release(); g_vs = NULL; }
    if (g_ps) { g_ps->Release(); g_ps = NULL; }
    if (g_vd) { g_vd->Release(); g_vd = NULL; }
    if (g_cursor_texture) { g_cursor_texture->Release(); g_cursor_texture = NULL; }
    if (g_fb_texture) { g_fb_texture->Release(); g_fb_texture = NULL; }
    if (g_device) { g_device->Release(); g_device = NULL; }
    g_d3d_ready = false;
}

static bool xbox360_init(void) {
    if (g_xbox_initialized) return true;
    XNetStartup(NULL);
    XOnlineStartup();
    g_xbox_initialized = true;
    return true;
}

static void xbox360_shutdown(void) {
    shutdown_d3d();
    if (g_xbox_initialized) {
        XOnlineCleanup();
        XNetCleanup();
        g_xbox_initialized = false;
    }
}

static void xbox360_poll_events(PlatformState* state) {
    uint64_t now = xbox360_get_ticks();
    uint64_t freq = xbox360_get_freq();
    float dt = (freq > 0) ? (float)(now - g_last_tick) / (float)freq : 0.016f;
    if (dt > 0.1f) dt = 0.1f;
    g_last_tick = now;

    float target_vx = 0.0f;
    float target_vy = 0.0f;

    for (DWORD i = 0; i < 4; i++) {
        if (XInputGetState(i, &g_controller_state[i]) != ERROR_SUCCESS)
            continue;
        {
            WORD buttons = g_controller_state[i].Gamepad.wButtons;
            SHORT lx = g_controller_state[i].Gamepad.sThumbLX;
            SHORT ly = g_controller_state[i].Gamepad.sThumbLY;
            SHORT rx = g_controller_state[i].Gamepad.sThumbRX;
            SHORT ry = g_controller_state[i].Gamepad.sThumbRY;

            float deadzone = 7849.0f;
            float mag_l = (float)((int)lx * lx + (int)ly * ly);
            float mag_r = (float)((int)rx * rx + (int)ry * ry);
            if (mag_l < deadzone * deadzone) { lx = 0; ly = 0; }
            if (mag_r < deadzone * deadzone) { rx = 0; ry = 0; }

            state->controller_connected = 1;
            state->thumb_lx = lx;
            state->thumb_ly = ly;
            state->thumb_rx = rx;
            state->thumb_ry = ry;
            state->left_trigger = g_controller_state[i].Gamepad.bLeftTrigger / 255.0f;
            state->right_trigger = g_controller_state[i].Gamepad.bRightTrigger / 255.0f;

            state->buttons = buttons;
            WORD buttons_pressed = buttons & ~g_prev_buttons[i];
            WORD buttons_released = ~buttons & g_prev_buttons[i];
            g_prev_buttons[i] = buttons;
            state->buttons_pressed = buttons_pressed;
            state->buttons_released = buttons_released;

            target_vx = lx / 32768.0f * JOYSTICK_SPEED;
            target_vy = -ly / 32768.0f * JOYSTICK_SPEED;

            g_cursor_vel_x += (target_vx - g_cursor_vel_x) * JOYSTICK_ACCEL * dt;
            g_cursor_vel_y += (target_vy - g_cursor_vel_y) * JOYSTICK_ACCEL * dt;

            g_cursor_x += g_cursor_vel_x * dt;
            g_cursor_y += g_cursor_vel_y * dt;

            if (g_cursor_x < 0) g_cursor_x = 0;
            if (g_cursor_y < 0) g_cursor_y = 0;
            if (g_cursor_x >= (float)state->window_width) g_cursor_x = (float)(state->window_width - 1);
            if (g_cursor_y >= (float)state->window_height) g_cursor_y = (float)(state->window_height - 1);

            state->mouse.x = (int)g_cursor_x;
            state->mouse.y = (int)g_cursor_y;
            state->mouse.left = (buttons & XINPUT_GAMEPAD_A) != 0;
            state->mouse.right = false;
            state->mouse.middle = (buttons & XINPUT_GAMEPAD_X) != 0;

            memset(state->keyboard.keys, 0, sizeof(state->keyboard.keys));
            if (buttons & XINPUT_GAMEPAD_DPAD_UP) state->keyboard.keys[PLATFORM_KEY_UP] = 1;
            if (buttons & XINPUT_GAMEPAD_DPAD_DOWN) state->keyboard.keys[PLATFORM_KEY_DOWN] = 1;
            if (buttons & XINPUT_GAMEPAD_DPAD_LEFT) state->keyboard.keys[PLATFORM_KEY_LEFT] = 1;
            if (buttons & XINPUT_GAMEPAD_DPAD_RIGHT) state->keyboard.keys[PLATFORM_KEY_RIGHT] = 1;
            if (buttons & XINPUT_GAMEPAD_START) state->keyboard.keys[PLATFORM_KEY_ENTER] = 1;
        }
        break; /* use only the first connected controller */
    }
}

static PlatformWindow xbox360_window_create(const PlatformWindowDesc* desc) {
    (void)desc;

    if (g_device) return (PlatformWindow)1;

    g_d3d = Direct3DCreate9(D3D_SDK_VERSION);

    D3DPRESENT_PARAMETERS d3dpp;
    memset(&d3dpp, 0, sizeof(d3dpp));
    d3dpp.BackBufferWidth = XBOX360_SCREEN_WIDTH;
    d3dpp.BackBufferHeight = XBOX360_SCREEN_HEIGHT;
    d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
    d3dpp.BackBufferCount = 1;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.Windowed = FALSE;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    g_d3dpp = d3dpp;

    HRESULT hr = g_d3d->CreateDevice(
        D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL,
        0, &d3dpp, &g_device);

    if (FAILED(hr)) {
        g_d3d = NULL;
        return NULL;
    }

    g_d3d->Release();
    g_d3d = NULL;

    g_device->BeginScene();
    init_d3d();
    return (PlatformWindow)1;
}

static void xbox360_window_destroy(PlatformWindow window) {
    (void)window;
    if (g_device) g_device->EndScene();
    shutdown_d3d();
}

static void xbox360_window_set_title(PlatformWindow window, const char* title) {
    (void)window; (void)title;
}

static void xbox360_window_set_size(PlatformWindow window, int width, int height) {
    (void)window; (void)width; (void)height;
}

static void xbox360_window_get_size(PlatformWindow window, int* width, int* height) {
    (void)window;
    if (width) *width = XBOX360_SCREEN_WIDTH;
    if (height) *height = XBOX360_SCREEN_HEIGHT;
}

static void xbox360_window_show(PlatformWindow window) { (void)window; }
static void xbox360_window_hide(PlatformWindow window) { (void)window; }
static void xbox360_window_focus(PlatformWindow window) { (void)window; }

static PlatformGLContext xbox360_gl_create_context(PlatformWindow window) {
    (void)window;
    return (PlatformGLContext)1;
}

static void xbox360_gl_destroy_context(PlatformGLContext ctx) { (void)ctx; }
static void xbox360_gl_make_current(PlatformGLContext ctx) { (void)ctx; }
static void xbox360_gl_swap_buffers(PlatformWindow window) { (void)window; }

static void* xbox360_file_open(const char* path, PlatformFileMode mode) {
    DWORD access = 0;
    DWORD creation = 0;
    switch (mode) {
    case PLATFORM_FILE_READ: access = GENERIC_READ; creation = OPEN_EXISTING; break;
    case PLATFORM_FILE_WRITE: access = GENERIC_WRITE; creation = CREATE_ALWAYS; break;
    case PLATFORM_FILE_APPEND: access = FILE_APPEND_DATA; creation = OPEN_ALWAYS; break;
    }
    HANDLE h = CreateFile(path, access, 0, NULL, creation, FILE_ATTRIBUTE_NORMAL, NULL);
    return (h == INVALID_HANDLE_VALUE) ? NULL : (void*)h;
}

static void xbox360_file_close(void* handle) {
    if (handle) CloseHandle((HANDLE)handle);
}

static int64_t xbox360_file_read(void* handle, void* buffer, int64_t size) {
    DWORD bytesRead = 0;
    if (ReadFile((HANDLE)handle, buffer, (DWORD)size, &bytesRead, NULL))
        return (int64_t)bytesRead;
    return -1;
}

static int64_t xbox360_file_write(void* handle, const void* buffer, int64_t size) {
    DWORD bytesWritten = 0;
    if (WriteFile((HANDLE)handle, buffer, (DWORD)size, &bytesWritten, NULL))
        return (int64_t)bytesWritten;
    return -1;
}

static int64_t xbox360_file_size(const char* path) {
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesEx(path, GetFileExInfoStandard, &fad)) return -1;
    LARGE_INTEGER size;
    size.HighPart = fad.nFileSizeHigh;
    size.LowPart = fad.nFileSizeLow;
    return size.QuadPart;
}

static bool xbox360_file_exists(const char* path) {
    DWORD attr = GetFileAttributesA(path);
    return attr != ((DWORD)-1);
}

static bool xbox360_file_delete(const char* path) {
    return DeleteFile(path) != 0;
}

static char* xbox360_file_read_all(const char* path, int64_t* out_size) {
    int64_t size = xbox360_file_size(path);
    if (size <= 0) return NULL;
    void* handle = xbox360_file_open(path, PLATFORM_FILE_READ);
    if (!handle) return NULL;
    char* buffer = (char*)malloc((size_t)size + 1);
    if (!buffer) { xbox360_file_close(handle); return NULL; }
    int64_t read = xbox360_file_read(handle, buffer, size);
    xbox360_file_close(handle);
    if (read != size) { free(buffer); return NULL; }
    buffer[size] = '\0';
    if (out_size) *out_size = size;
    return buffer;
}

typedef struct {
    void (*func)(void*);
    void* arg;
} ThreadData;

static DWORD WINAPI xbox360_thread_wrapper(LPVOID lpParam) {
    ThreadData* td = (ThreadData*)lpParam;
    td->func(td->arg);
    free(td);
    return 0;
}

static PlatformThread xbox360_thread_create(void (*func)(void*), void* arg) {
    ThreadData* td = (ThreadData*)malloc(sizeof(ThreadData));
    if (!td) return NULL;
    td->func = func;
    td->arg = arg;
    HANDLE h = CreateThread(NULL, 0, xbox360_thread_wrapper, td, 0, NULL);
    return (PlatformThread)h;
}

static void xbox360_thread_join(PlatformThread thread) {
    if (thread) {
        WaitForSingleObject((HANDLE)thread, INFINITE);
        CloseHandle((HANDLE)thread);
    }
}

static PlatformMutex xbox360_mutex_create(void) {
    CRITICAL_SECTION* cs = (CRITICAL_SECTION*)malloc(sizeof(CRITICAL_SECTION));
    if (!cs) return NULL;
    InitializeCriticalSection(cs);
    return (PlatformMutex)cs;
}

static void xbox360_mutex_lock(PlatformMutex mutex) {
    if (mutex) EnterCriticalSection((CRITICAL_SECTION*)mutex);
}

static void xbox360_mutex_unlock(PlatformMutex mutex) {
    if (mutex) LeaveCriticalSection((CRITICAL_SECTION*)mutex);
}

static void xbox360_mutex_destroy(PlatformMutex mutex) {
    if (mutex) {
        DeleteCriticalSection((CRITICAL_SECTION*)mutex);
        free(mutex);
    }
}

static void xbox360_sleep_ms(uint32_t ms) {
    Sleep(ms);
}

static PlatformSocket xbox360_socket_create(PlatformSocketType type) {
    int sockType = (type == PLATFORM_SOCKET_TCP) ? SOCK_STREAM : SOCK_DGRAM;
    int proto = (type == PLATFORM_SOCKET_TCP) ? IPPROTO_TCP : IPPROTO_UDP;
    SOCKET s = socket(AF_INET, sockType, proto);
    return (PlatformSocket)(s == INVALID_SOCKET ? NULL : (void*)s);
}

static void xbox360_socket_destroy(PlatformSocket sock) {
    if (sock) closesocket((SOCKET)sock);
}

static bool xbox360_resolve_host(const char* host, IN_ADDR* out_addr) {
    if (!host || !out_addr) return false;

    unsigned long ip = inet_addr(host);
    if (ip != INADDR_NONE) {
        out_addr->s_addr = ip;
        return true;
    }

    XNDNS* pxndns = NULL;
    WSAEVENT ev = WSACreateEvent();
    if (ev == WSA_INVALID_EVENT) return false;

    INT lr = XNetDnsLookup(host, ev, &pxndns);
    if (lr == 0 || lr == WSAEINPROGRESS) {
        WaitForSingleObject(ev, 10000);
        WSAResetEvent(ev);
    }

    if (!pxndns || pxndns->iStatus != 0 || pxndns->cina == 0) {
        if (pxndns) XNetDnsRelease(pxndns);
        WSACloseEvent(ev);
        return false;
    }

    *out_addr = pxndns->aina[0];
    XNetDnsRelease(pxndns);
    WSACloseEvent(ev);
    return true;
}

static bool xbox360_socket_connect(PlatformSocket sock, const char* host, uint16_t port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (!xbox360_resolve_host(host, &addr.sin_addr)) return false;
    bool connected = (connect((SOCKET)sock, (struct sockaddr*)&addr, sizeof(addr)) == 0);
    return connected;
}

static int xbox360_socket_send(PlatformSocket sock, const void* data, int len) {
    return send((SOCKET)sock, (const char*)data, len, 0);
}

static int xbox360_socket_recv(PlatformSocket sock, void* buffer, int len) {
    return recv((SOCKET)sock, (char*)buffer, len, 0);
}

static bool xbox360_socket_set_nonblocking(PlatformSocket sock, bool nonblocking) {
    u_long mode = nonblocking ? 1 : 0;
    return ioctlsocket((SOCKET)sock, FIONBIO, &mode) == 0;
}

static uint64_t xbox360_get_ticks(void) {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

static uint64_t xbox360_get_freq(void) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return freq.QuadPart;
}

static void xbox360_log(PlatformLogLevel level, const char* fmt, ...) {
    const char* levelStr[] = {"[INFO]", "[WARN]", "[ERROR]", "[DEBUG]"};
    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    _vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    OutputDebugString(levelStr[level]);
    OutputDebugString(" NetXbox: ");
    OutputDebugString(buffer);
    OutputDebugString("\n");
#if defined(_DEBUG)
    DbgPrint("NetXbox %s %s\n", levelStr[level], buffer);
#endif
}

static void xbox360_clipboard_set(const char* text) { (void)text; }
static char* xbox360_clipboard_get(void) { return NULL; }

static void xbox360_surface_blit(PlatformWindow window, const uint32_t* pixels, int width, int height) {
    (void)window;
    if (!g_device || !pixels || width <= 0 || height <= 0) return;

    setup_2d_render();

    g_device->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFF2D2D30, 1.0f, 0);

    ensure_fb_texture(width, height);
    if (!g_fb_texture) return;

    D3DLOCKED_RECT lr;
    HRESULT hr = g_fb_texture->LockRect(0, &lr, NULL, 0);
    if (FAILED(hr)) return;

    const BYTE* src = (const BYTE*)pixels;
    int row_bytes = width * 4;
    for (int y = 0; y < height; y++) {
        memcpy((BYTE*)lr.pBits + y * lr.Pitch, src + y * row_bytes, row_bytes);
    }
    g_fb_texture->UnlockRect(0);

    g_device->SetTexture(0, g_fb_texture);

    float fw = (float)width;
    float fh = (float)height;
    D3DVertex verts[6];
    verts[0].x = 0;    verts[0].y = 0;    verts[0].color = 0xFFFFFFFF; verts[0].u = 0; verts[0].v = 0;
    verts[1].x = fw;   verts[1].y = 0;    verts[1].color = 0xFFFFFFFF; verts[1].u = 1; verts[1].v = 0;
    verts[2].x = 0;    verts[2].y = fh;   verts[2].color = 0xFFFFFFFF; verts[2].u = 0; verts[2].v = 1;
    verts[3].x = fw;   verts[3].y = 0;    verts[3].color = 0xFFFFFFFF; verts[3].u = 1; verts[3].v = 0;
    verts[4].x = fw;   verts[4].y = fh;   verts[4].color = 0xFFFFFFFF; verts[4].u = 1; verts[4].v = 1;
    verts[5].x = 0;    verts[5].y = fh;   verts[5].color = 0xFFFFFFFF; verts[5].u = 0; verts[5].v = 1;

    g_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, verts, sizeof(D3DVertex));

    setup_2d_render();

    if (g_cursor_texture) {
        g_device->SetTexture(0, g_cursor_texture);
        float cx = g_cursor_x;
        float cy = g_cursor_y;
        float cs = (float)CURSOR_SIZE;
        D3DVertex cverts[6];
        cverts[0].x = cx;        cverts[0].y = cy;        cverts[0].color = 0xFFFFFFFF; cverts[0].u = 0; cverts[0].v = 0;
        cverts[1].x = cx + cs;   cverts[1].y = cy;        cverts[1].color = 0xFFFFFFFF; cverts[1].u = 1; cverts[1].v = 0;
        cverts[2].x = cx;        cverts[2].y = cy + cs;   cverts[2].color = 0xFFFFFFFF; cverts[2].u = 0; cverts[2].v = 1;
        cverts[3].x = cx + cs;   cverts[3].y = cy;        cverts[3].color = 0xFFFFFFFF; cverts[3].u = 1; cverts[3].v = 0;
        cverts[4].x = cx + cs;   cverts[4].y = cy + cs;   cverts[4].color = 0xFFFFFFFF; cverts[4].u = 1; cverts[4].v = 1;
        cverts[5].x = cx;        cverts[5].y = cy + cs;   cverts[5].color = 0xFFFFFFFF; cverts[5].u = 0; cverts[5].v = 1;
        g_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, cverts, sizeof(D3DVertex));
    }
}

static void xbox360_surface_present(PlatformWindow window) {
    (void)window;
    if (g_device) {
        g_device->EndScene();
        g_device->Present(NULL, NULL, NULL, NULL);
        g_device->BeginScene();
    }
}

static void* xbox360_get_hwnd(PlatformWindow window) {
    (void)window;
    return NULL;
}

static void xbox360_draw_char(int x, int y, char c, int font_size, DWORD color) {
    (void)x; (void)y; (void)c; (void)font_size; (void)color;
}

static void xbox360_text_draw(PlatformWindow window, int x, int y, const char* text, int font_size, uint32_t color, bool bold) {
    (void)window; (void)x; (void)y; (void)text; (void)font_size; (void)color; (void)bold;
}

static void xbox360_text_draw_clipped(PlatformWindow window, int x, int y, int max_w, const char* text, int font_size, uint32_t color, bool bold) {
    (void)window; (void)x; (void)y; (void)max_w; (void)text; (void)font_size; (void)color; (void)bold;
}

static int xbox360_text_measure(const char* text, int font_size, bool bold) {
    if (!text) return 0;
    const Font* font = font_get_default();
    float scale = (float)font_size / (float)font->glyph_height;
    if (bold) return font_measure_string_bold(text, scale);
    return font_measure_string(font, text, scale);
}

static int xbox360_text_measure_clipped(int max_w, const char* text, int font_size, bool bold) {
    if (!text) return 0;
    int full_w = xbox360_text_measure(text, font_size, bold);
    if (full_w <= max_w) return full_w;
    if (max_w <= 0) return 0;
    const Font* font = font_get_default();
    float scale = (float)font_size / (float)font->glyph_height;
    int cx = 0;
    const char* t = text;
    while (*t) {
        int cw = font_char_advance(font, *t, scale);
        if (cx + cw > max_w) break;
        cx += cw;
        t++;
    }
    if (cx <= 0) {
        int cw = font_char_advance(font, text[0], scale);
        cx = (cw > 0) ? cw : 8;
        if (cx > max_w) cx = max_w;
    }
    return cx;
}

static int xbox360_text_height(int font_size) {
    return font_size;
}

static char* xbox360_file_save_dialog(const char* default_name, const char* filter) {
    (void)default_name; (void)filter;
    return NULL;
}

static bool xbox360_file_write_bytes(const char* path, const void* data, int len) {
    if (!path || !data || len <= 0) return false;
    HANDLE h = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    BOOL ok = WriteFile(h, data, (DWORD)len, &written, NULL);
    CloseHandle(h);
    return ok && (int)written == len;
}

static bool xbox360_show_keyboard(const char* title, const char* default_text, char* out, int out_size) {
    if (!out || out_size <= 0) return false;
    out[0] = '\0';

    if (!xui_ui_keyboard_begin(title, default_text))
        return false;

    uint64_t freq = xbox360_get_freq();
    uint64_t last = xbox360_get_ticks();

    while (g_device) {
        uint64_t now = xbox360_get_ticks();
        float dt = (freq > 0) ? (float)(now - last) / (float)freq : 0.016f;
        if (dt > 0.1f) dt = 0.1f;
        last = now;

        xui_ui_feed_input();

        char result_utf8[2048];
        int poll = xui_ui_keyboard_poll(result_utf8, (int)sizeof(result_utf8));
        if (poll != 0) {
            g_device->BeginScene();
            xui_ui_render(XBOX360_SCREEN_WIDTH, XBOX360_SCREEN_HEIGHT, dt);
            g_device->EndScene();
            g_device->Present(NULL, NULL, NULL, NULL);
            if (poll == 1) {
                strncpy(out, result_utf8, (size_t)out_size - 1);
                out[out_size - 1] = '\0';
                return out[0] != '\0';
            }
            return false;
        }

        g_device->BeginScene();
        xui_ui_render(XBOX360_SCREEN_WIDTH, XBOX360_SCREEN_HEIGHT, dt);
        g_device->EndScene();
        g_device->Present(NULL, NULL, NULL, NULL);
    }

    return false;
}

static int xbox360_run_chrome(int mode) {
    if (mode < XUI_CHROME_TOOLBAR || mode > XUI_CHROME_HOME)
        return -1;

    XuiChromeMode cm = (XuiChromeMode)mode;
    if (!xui_ui_chrome_begin(cm))
        return -1;

    uint64_t freq = xbox360_get_freq();
    uint64_t last = xbox360_get_ticks();

    while (g_device) {
        uint64_t now = xbox360_get_ticks();
        float dt = (freq > 0) ? (float)(now - last) / (float)freq : 0.016f;
        if (dt > 0.1f) dt = 0.1f;
        last = now;

        xui_ui_feed_input();

        if (xui_ui_chrome_poll() != 0)
            break;

        g_device->BeginScene();
        xui_ui_render(XBOX360_SCREEN_WIDTH, XBOX360_SCREEN_HEIGHT, dt);
        g_device->EndScene();
        g_device->Present(NULL, NULL, NULL, NULL);
    }

    return xui_ui_chrome_take_action();
}

static const PlatformAPI g_xbox360_api = {
    xbox360_init,
    xbox360_shutdown,
    xbox360_poll_events,

    xbox360_window_create,
    xbox360_window_destroy,
    xbox360_window_set_title,
    xbox360_window_set_size,
    xbox360_window_get_size,
    xbox360_window_show,
    xbox360_window_hide,
    xbox360_window_focus,

    xbox360_gl_create_context,
    xbox360_gl_destroy_context,
    xbox360_gl_make_current,
    xbox360_gl_swap_buffers,

    xbox360_file_open,
    xbox360_file_close,
    xbox360_file_read,
    xbox360_file_write,
    xbox360_file_size,
    xbox360_file_exists,
    xbox360_file_delete,
    xbox360_file_read_all,

    xbox360_thread_create,
    xbox360_thread_join,
    xbox360_mutex_create,
    xbox360_mutex_lock,
    xbox360_mutex_unlock,
    xbox360_mutex_destroy,
    xbox360_sleep_ms,

    xbox360_socket_create,
    xbox360_socket_destroy,
    xbox360_socket_connect,
    xbox360_socket_send,
    xbox360_socket_recv,
    xbox360_socket_set_nonblocking,

    xbox360_get_ticks,
    xbox360_get_freq,

    xbox360_log,

    xbox360_clipboard_set,
    xbox360_clipboard_get,

    xbox360_surface_blit,
    xbox360_surface_present,
    xbox360_get_hwnd,

    xbox360_text_draw,
    xbox360_text_draw_clipped,
    xbox360_text_measure,
    xbox360_text_measure_clipped,
    xbox360_text_height,
    xbox360_file_save_dialog,
    xbox360_file_write_bytes,
    xbox360_show_keyboard,
    xbox360_run_chrome,
};

const PlatformAPI* xbox360_get_api(void) {
    return &g_xbox360_api;
}

const char* xbox360_get_name(void) {
    return "Xbox360";
}

bool xbox360_is_running_on_xbox360(void) {
    return true;
}
