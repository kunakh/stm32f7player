#include "config.h"
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include "scale.h"

#define SUPPORT_SIMD
#define clamped(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#define ASSERT(a)
#define NS_ASSERTION(a, b)  ASSERT((a))

/*This contains all of the parameters that are needed to convert a row.
  Passing them in a struct instead of as individual parameters saves the need
   to continually push onto the stack the ones that are fixed for every row.*/
typedef struct {
  uint16_t *rgb_row;
  const uint8_t *y_row;
  const uint8_t *u_row;
  const uint8_t *v_row;
  int y_yweight;
  int y_pitch;
  int width;
  int source_x0_q16;
  int source_dx_q16;
  /*Not used for 4:4:4, except with chroma-nearest.*/
  int source_uv_xoffs_q16;
  /*Not used for 4:4:4 or chroma-nearest.*/
  int uv_pitch;
  /*Not used for 4:2:2, 4:4:4, or chroma-nearest.*/
  int uv_yweight;
} yuv2rgb565_row_scale_bilinear_ctx;



/*This contains all of the parameters that are needed to convert a row.
  Passing them in a struct instead of as individual parameters saves the need
   to continually push onto the stack the ones that are fixed for every row.*/
typedef struct {
  uint16_t *rgb_row;
  const uint8_t *y_row;
  const uint8_t *u_row;
  const uint8_t *v_row;
  int width;
  int source_x0_q16;
  int source_dx_q16;
  /*Not used for 4:4:4.*/
  int source_uv_xoffs_q16;
} yuv2rgb565_row_scale_nearest_ctx;


typedef void (*yuv2rgb565_row_scale_bilinear_func)(
 const yuv2rgb565_row_scale_bilinear_ctx *ctx, int dither);

typedef void (*yuv2rgb565_row_scale_nearest_func)(
 const yuv2rgb565_row_scale_nearest_ctx *ctx, int dither);



//TODO: fix NEON asm for iOS
# if defined(MOZILLA_MAY_SUPPORT_NEON) && !defined(__APPLE__)

extern "C" void ScaleYCbCr42xToRGB565_BilinearY_Row_NEON(
 const yuv2rgb565_row_scale_bilinear_ctx *ctx, int dither);

void __attribute((noinline)) yuv42x_to_rgb565_row_neon(uint16_t *dst,
                                                       const uint8_t *y,
                                                       const uint8_t *u,
                                                       const uint8_t *v,
                                                       int n,
                                                       int oddflag);

#endif



/*Bilinear interpolation of a single value.
  This uses the exact same formulas as the asm, even though it adds some extra
   shifts that do nothing but reduce accuracy.*/
static int bislerp(const uint8_t *row,
                   int pitch,
                   int source_x,
                   int xweight,
                   int yweight) {
  int a;
  int b;
  int c;
  int d;
  a = row[source_x];
  b = row[source_x+1];
  c = row[source_x+pitch];
  d = row[source_x+pitch+1];
  a = ((a<<8)+(c-a)*yweight+128)>>8;
  b = ((b<<8)+(d-b)*yweight+128)>>8;
  return ((a<<8)+(b-a)*xweight+128)>>8;
}

const int DITHER_BIAS[4][3]={
    {-14240,    8704,    -17696},
    {-14240+128,8704+64, -17696+128},
    {-14240+256,8704+128,-17696+256},
    {-14240+384,8704+192,-17696+384}
};

/*Convert a single pixel from Y'CbCr to RGB565.
  This uses the exact same formulas as the asm, even though we could make the
   constants a lot more accurate with 32-bit wide registers.*/
static uint16_t yu2rgb565(int y, int u, int v, int dither) {
  /*This combines the constant offset that needs to be added during the Y'CbCr
     conversion with a rounding offset that depends on the dither parameter.*/
#if 0
  static const int DITHER_BIAS[4][3]={
    {-14240,    8704,    -17696},
    {-14240+128,8704+64, -17696+128},
    {-14240+256,8704+128,-17696+256},
    {-14240+384,8704+192,-17696+384}
  };
#endif
  int r;
  int g;
  int b;
  r = clamped((74*y+102*v+DITHER_BIAS[dither][0])>>9, 0, 31);
  g = clamped((74*y-25*u-52*v+DITHER_BIAS[dither][1])>>8, 0, 63);
  b = clamped((74*y+129*u+DITHER_BIAS[dither][2])>>9, 0, 31);
  return (uint16_t)(r<<11 | g<<5 | b);
}

static inline void ScaleYCbCr420ToRGB565_Bilinear_Row_C(
 const yuv2rgb565_row_scale_bilinear_ctx *ctx, int dither){
  int x;
  int source_x_q16;
  source_x_q16 = ctx->source_x0_q16;
  for (x = 0; x < ctx->width; x++) {
    int source_x;
    int xweight;
    int y;
    int u;
    int v;
    xweight = ((source_x_q16&0xFFFF)+128)>>8;
    source_x = source_x_q16>>16;
    y = bislerp(ctx->y_row, ctx->y_pitch, source_x, xweight, ctx->y_yweight);
    xweight = (((source_x_q16+ctx->source_uv_xoffs_q16)&0x1FFFF)+256)>>9;
    source_x = (source_x_q16+ctx->source_uv_xoffs_q16)>>17;
    source_x_q16 += ctx->source_dx_q16;
    u = bislerp(ctx->u_row, ctx->uv_pitch, source_x, xweight, ctx->uv_yweight);
    v = bislerp(ctx->v_row, ctx->uv_pitch, source_x, xweight, ctx->uv_yweight);
    ctx->rgb_row[x] = yu2rgb565(y, u, v, dither);
    dither ^= 3;
  }
}

static inline void ScaleYCbCr422ToRGB565_Bilinear_Row_C(
 const yuv2rgb565_row_scale_bilinear_ctx *ctx, int dither){
  int x;
  int source_x_q16;
  source_x_q16 = ctx->source_x0_q16;
  for (x = 0; x < ctx->width; x++) {
    int source_x;
    int xweight;
    int y;
    int u;
    int v;
    xweight = ((source_x_q16&0xFFFF)+128)>>8;
    source_x = source_x_q16>>16;
    y = bislerp(ctx->y_row, ctx->y_pitch, source_x, xweight, ctx->y_yweight);
    xweight = (((source_x_q16+ctx->source_uv_xoffs_q16)&0x1FFFF)+256)>>9;
    source_x = (source_x_q16+ctx->source_uv_xoffs_q16)>>17;
    source_x_q16 += ctx->source_dx_q16;
    u = bislerp(ctx->u_row, ctx->uv_pitch, source_x, xweight, ctx->y_yweight);
    v = bislerp(ctx->v_row, ctx->uv_pitch, source_x, xweight, ctx->y_yweight);
    ctx->rgb_row[x] = yu2rgb565(y, u, v, dither);
    dither ^= 3;
  }
}

static inline void ScaleYCbCr444ToRGB565_Bilinear_Row_C(
 const yuv2rgb565_row_scale_bilinear_ctx *ctx, int dither){
  int x;
  int source_x_q16;
  source_x_q16 = ctx->source_x0_q16;
  for (x = 0; x < ctx->width; x++) {
    int source_x;
    int xweight;
    int y;
    int u;
    int v;
    xweight = ((source_x_q16&0xFFFF)+128)>>8;
    source_x = source_x_q16>>16;
    source_x_q16 += ctx->source_dx_q16;
    y = bislerp(ctx->y_row, ctx->y_pitch, source_x, xweight, ctx->y_yweight);
    u = bislerp(ctx->u_row, ctx->y_pitch, source_x, xweight, ctx->y_yweight);
    v = bislerp(ctx->v_row, ctx->y_pitch, source_x, xweight, ctx->y_yweight);
    ctx->rgb_row[x] = yu2rgb565(y, u, v, dither);
    dither ^= 3;
  }
}

static inline void ScaleYCbCr42xToRGB565_BilinearY_Row_C(
 const yuv2rgb565_row_scale_bilinear_ctx *ctx, int dither){
  int x;
  int source_x_q16;
  source_x_q16 = ctx->source_x0_q16;
  for (x = 0; x < ctx->width; x++) {
    int source_x;
    int xweight;
    int y;
    int u;
    int v;
    xweight = ((source_x_q16&0xFFFF)+128)>>8;
    source_x = source_x_q16>>16;
    y = bislerp(ctx->y_row, ctx->y_pitch, source_x, xweight, ctx->y_yweight);
    source_x = (source_x_q16+ctx->source_uv_xoffs_q16)>>17;
    source_x_q16 += ctx->source_dx_q16;
    u = ctx->u_row[source_x];
    v = ctx->v_row[source_x];
    ctx->rgb_row[x] = yu2rgb565(y, u, v, dither);
    dither ^= 3;
  }
}

static inline void ScaleYCbCr444ToRGB565_BilinearY_Row_C(
 const yuv2rgb565_row_scale_bilinear_ctx *ctx, int dither){
  int x;
  int source_x_q16;
  source_x_q16 = ctx->source_x0_q16;
  for (x = 0; x < ctx->width; x++) {
    int source_x;
    int xweight;
    int y;
    int u;
    int v;
    xweight = ((source_x_q16&0xFFFF)+128)>>8;
    source_x = source_x_q16>>16;
    y = bislerp(ctx->y_row, ctx->y_pitch, source_x, xweight, ctx->y_yweight);
    source_x = (source_x_q16+ctx->source_uv_xoffs_q16)>>16;
    source_x_q16 += ctx->source_dx_q16;
    u = ctx->u_row[source_x];
    v = ctx->v_row[source_x];
    ctx->rgb_row[x] = yu2rgb565(y, u, v, dither);
    dither ^= 3;
  }
}


static inline void ScaleYCbCr42xToRGB565_Nearest_Row_C(
 const yuv2rgb565_row_scale_nearest_ctx *ctx, int dither){
  int y;
  int u;
  int v;
  int x;
  int source_x_q16;
  int source_x;
  source_x_q16 = ctx->source_x0_q16;
  for (x = 0; x < ctx->width; x++) {
    source_x = source_x_q16>>16;
    y = ctx->y_row[source_x];
    source_x = (source_x_q16+ctx->source_uv_xoffs_q16)>>17;
    source_x_q16 += ctx->source_dx_q16;
    u = ctx->u_row[source_x];
    v = ctx->v_row[source_x];
    ctx->rgb_row[x] = yu2rgb565(y, u, v, dither);
    dither ^= 3;
  }
}

static inline void ScaleYCbCr42xToRGB565_Nearest_Row_SIMD(
    uint16_t *rgb_row, const uint8_t *y_row, const uint8_t *u_row, const uint8_t *v_row,
    int width, int source_x0_q16, int source_dx_q16, int source_uv_xoffs_q16) {

__asm__ volatile (
"MOV     r11, #74                                                                                 \n"
"MOV     r12, #-25                                                                                \n"
"Nearest_Row_SIMD:                                                                                \n"
"ADD     r7, %[x1], %[x5], LSR #16                                                                \n"
"LDRB    r8, [r7]                @ y = ctx->y_row[source_x] : even pixel                          \n"
"ADD     r7, %[x5], %[x7]        @ source_x_q16+ctx->source_uv_xoffs_q16                          \n"
"LSR     r7, r7, #17             @ source_x = (source_x_q16+ctx->source_uv_xoffs_q16)>>17         \n"
"LDRB    r9, [%[x2], r7]         @ u = ctx->u_row[source_x]                                       \n"
"LDRB    r10, [%[x3], r7]        @ v = ctx->v_row[source_x]                                       \n"
"ADD     %[x5], %[x6]            @ source_x_q16 += ctx->source_dx_q16                             \n"
"LDR     r7, =%[x10]             @ DITHER_BIAS[0][2]                                              \n"
"MOVT    r11, #129               @ 0:129:0:74                                                     \n"
"PKHBT   r9, r8, r9, LSL #16     @ 0:u:0:y                                                        \n"
"PKHBT   r8, r8, r10, LSL #16    @ 0:v:0:y                                                        \n"
"SMLAD   r7, r11, r9, r7         @ 74*y+129*u+DITHER_BIAS[0][2]                                   \n"
"USAT    r10, #5, r7, ASR #9     @ b = clamped((74*y+129*u+DITHER_BIAS[0][2])>>9, 0, 31)          \n"
"LDR     r7, =%[x8]              @ DITHER_BIAS[0][0]                                              \n"
"MOVT    r11, #102               @ 0:102:0:74                                                     \n"
"SMLAD   r7, r11, r8, r7         @ 74*y+102*v+DITHER_BIAS[0][0]                                   \n"
"USAT    r7, #5, r7, ASR #9      @ r =clamped((74*y+102*v+DITHER_BIAS[0][0])>>9, 0, 31)           \n"
"ORR     r10, r10, r7, LSL #11   @ (r<<11 | b)                                                    \n"
"LDR     r7, =%[x9]              @ DITHER_BIAS[0][1]                                              \n"
"MOVT    r11, 0xFFCC             @ 0:-52:0:74                                                     \n"
"SMLAD   r7, r11, r8, r7         @ 74*y-52*v+DITHER_BIAS[0][1]                                    \n"
"SMLABT  r7, r12, r9, r7         @ 74*y-25*u-52*v+DITHER_BIAS[0][1]                               \n"
"USAT    r7, #6, r7, ASR #8      @ g = clamped((74*y-25*u-52*v+DITHER_BIAS[0][1])>>8, 0, 63)      \n"
"ORR     r10, r10, r7, LSL #5    @ (r<<11 | g<<5 | b)                                             \n"
"STRH.W  r10, [%[x0]], #2        @ ctx->rgb_row[x] = yu2rgb565(y, u, v, 0)                        \n"
"ADD     r7, %[x1], %[x5], LSR #16                                                                \n"
"LDRB    r8, [r7]                @ y = ctx->y_row[source_x] : even pixel                          \n"
"ADD     r7, %[x5], %[x7]        @ source_x_q16+ctx->source_uv_xoffs_q16                          \n"
"LSR     r7, r7, #17             @ source_x = (source_x_q16+ctx->source_uv_xoffs_q16)>>17         \n"
"LDRB    r9, [%[x2], r7]         @ u = ctx->u_row[source_x]                                       \n"
"LDRB    r10, [%[x3], r7]        @ v = ctx->v_row[source_x]                                       \n"
"ADD     %[x5], %[x6]            @ source_x_q16 += ctx->source_dx_q16                             \n"
"LDR     r7, =%[x13]             @ DITHER_BIAS[3][2]                                              \n"
"MOVT    r11, #129               @ 0:129:0:74                                                     \n"
"PKHBT   r9, r8, r9, LSL #16     @ 0:u:0:y                                                        \n"
"PKHBT   r8, r8, r10, LSL #16    @ 0:v:0:y                                                        \n"
"SMLAD   r7, r11, r9, r7         @ 74*y+129*u+DITHER_BIAS[0][2]                                   \n"
"USAT    r10, #5, r7, ASR #9     @ b = clamped((74*y+129*u+DITHER_BIAS[0][2])>>9, 0, 31)          \n"
"LDR     r7, =%[x11]             @ DITHER_BIAS[3][0]                                              \n"
"MOVT    r11, #102               @ 0:102:0:74                                                     \n"
"SMLAD   r7, r11, r8, r7         @ 74*y+102*v+DITHER_BIAS[0][0]                                   \n"
"USAT    r7, #5, r7, ASR #9      @ r =clamped((74*y+102*v+DITHER_BIAS[0][0])>>9, 0, 31)           \n"
"ORR     r10, r10, r7, LSL #11   @ (r<<11 | b)                                                    \n"
"LDR     r7, =%[x12]             @ DITHER_BIAS[3][1]                                              \n"
"MOVT    r11, 0xFFCC             @ 0:-52:0:74                                                     \n"
"SMLAD   r7, r11, r8, r7         @ 74*y-52*v+DITHER_BIAS[0][1]                                    \n"
"SMLABT  r7, r12, r9, r7         @ 74*y-25*u-52*v+DITHER_BIAS[0][1]                               \n"
"USAT    r7, #6, r7, ASR #8      @ g = clamped((74*y-25*u-52*v+DITHER_BIAS[0][1])>>8, 0, 63)      \n"
"ORR     r10, r10, r7, LSL #5    @ (r<<11 | g<<5 | b)                                             \n"
"STRH.W  r10, [%[x0]], #2        @ ctx->rgb_row[x] = yu2rgb565(y, u, v, 0)                        \n"
"SUBS    %[x4], %[x4], #2        @ width-=2                                                       \n"
"BNE     Nearest_Row_SIMD                                                                         \n"
:
    [x0]"+r"(rgb_row),
    [x1]"+r"(y_row),
    [x2]"+r"(u_row),
    [x3]"+r"(v_row),
    [x5]"+r"(source_x0_q16)
:
    [x4]"r"(width),
    [x6]"r"(source_dx_q16),
    [x7]"r"(source_uv_xoffs_q16),
    [x8] "i"(DITHER_BIAS[0][0]),
    [x9] "i"(DITHER_BIAS[0][1]),
    [x10]"i"(DITHER_BIAS[0][2]),
    [x11]"i"(DITHER_BIAS[3][0]),
    [x12]"i"(DITHER_BIAS[3][1]),
    [x13]"i"(DITHER_BIAS[3][2])
:   "cc", "memory", "r7", "r8", "r9", "r10", "r11", "r12");
}

static inline void ScaleYCbCr444ToRGB565_Nearest_Row_C(
    uint16_t *rgb_row, const uint8_t *y_row, const uint8_t *u_row, const uint8_t *v_row,
    int width, int source_x0_q16, int source_dx_q16, int dither) {
  int y;
  int u;
  int v;
  int x;
  int source_x_q16;
  int source_x;
  source_x_q16 = source_x0_q16;
  for (x = 0; x < width; x++) {
    source_x = source_x_q16>>16;
    source_x_q16 += source_dx_q16;
    y = y_row[source_x];
    u = u_row[source_x];
    v = v_row[source_x];
    rgb_row[x] = yu2rgb565(y, u, v, dither);
    dither ^= 3;
  }
}

static inline void ScaleYCbCr42xToRGB565_BilinearY_Row_SIMD(
    uint16_t *rgb_row, const uint8_t *y_row, const uint8_t *u_row, const uint8_t *v_row,
    int source_x0_q16, int y_pitch, int yweight, int source_uv_xoffs_q16, int source_dx_q16, int width)
{

__asm__ volatile (
"ScaleYCbCr42xToRGB565_BilinearY_Row:                                                             \n"
"scale_loop:                                                                                      \n"
"ADD     r7, %[x7], %[x1], LSR #16  @ &ctx->y_row[source_x] : even pixel                          \n"
"LDRH    r8, [r7]                @ 0:0:b:a                                                        \n"
"LDRH    r9, [r7, %[x2]]         @ 0:0:d:c                                                        \n"
"MOV     r11, #128                                                                                \n"
"USUB8   r9, r9, r8              @ 0:0:d-b:c-a                                                    \n"
"UXTAH   r10, r11, r8, ROR #24   @ (a<<8)+128                                                     \n"
"AND     r8, r8, #0xFF00         @ 0:0:b:0                                                        \n"
"ADD     r8, r8, r11             @ (b<<8)+128                                                     \n"
"SXTB    r7, r9                  @ 0:0:0:c-a                                                      \n"
"SXTB    r9, r9, ROR #8          @ 0:0:0:d-b                                                      \n"
"SMLABB  r10, r7, %[x3], r10     @ (a<<8)+(c-a)*yweight+128                                       \n"
"SMLABB  r8, r9, %[x3], r8       @ (b<<8)+(d-b)*yweight+128                                       \n"
"SUB     r8, r8, r10                                                                              \n"
"SXTH    r8, r8, ROR #8          @ (b-a)                                                          \n"
"ADD     r9, %[x1], r11          @ source_x_q16 + 128                                             \n"
"UBFX    r9, r9, #8, #8          @ xweight = ((source_x_q16&0xFFFF)+128)>>8                       \n"
"SMLABB  r8, r8, r9, r10                                                                          \n"
"UXTB    r8, r8, ROR #8          @ y=((a<<8)+(b-a)*xweight+128)>>8                                \n"
"LDR     r7, %[x4]               @ source_uv_xoffs_q16                                            \n"
"ADD     r7, %[x1], r7                                                                            \n"
"LSR     r7, r7, #17             @ source_x = (source_x_q16+ctx->source_uv_xoffs_q16)>>17         \n"
"ADD     %[x1], %[x5]            @ source_x_q16 += ctx->source_dx_q16                             \n"
"LDRB    r9, [%[x8], r7]         @ u = ctx->u_row[source_x]                                       \n"
"LDRB    r10, [%[x9], r7]        @ v = ctx->v_row[source_x]                                       \n"
"LDR     r7, =%[x12]             @ DITHER_BIAS[0][2]                                              \n"
"MOV     r11, #74                                                                                 \n"
"MOVT    r11, #129               @ 0:129:0:74                                                     \n"
"PKHBT   r9, r8, r9, LSL #16     @ 0:u:0:y                                                        \n"
"PKHBT   r8, r8, r10, LSL #16    @ 0:v:0:y                                                        \n"
"SMLAD   r7, r11, r9, r7         @ 74*y+129*u+DITHER_BIAS[0][2]                                   \n"
"USAT    r10, #5, r7, ASR #9     @ b = clamped((74*y+129*u+DITHER_BIAS[0][2])>>9, 0, 31)          \n"
"LDR     r7, =%[x10]             @ DITHER_BIAS[0][0]                                              \n"
"MOVT    r11, #102               @ 0:102:0:74                                                     \n"
"SMLAD   r7, r11, r8, r7         @ 74*y+102*v+DITHER_BIAS[0][0]                                   \n"
"USAT    r7, #5, r7, ASR #9      @ r =clamped((74*y+102*v+DITHER_BIAS[0][0])>>9, 0, 31)           \n"
"ORR     r10, r10, r7, LSL #11   @ (r<<11 | b)                                                    \n"
"LDR     r7, =%[x11]             @ DITHER_BIAS[0][1]                                              \n"
"MOVT    r11, 0xFFCC             @ 0:-52:0:74                                                     \n"
"SMLAD   r7, r11, r8, r7         @ 74*y-52*v+DITHER_BIAS[0][1]                                    \n"
"MOV     r11, #-25                                                                                \n"
"SMLABT  r7, r11, r9, r7         @ 74*y-25*u-52*v+DITHER_BIAS[0][1]                               \n"
"USAT    r7, #6, r7, ASR #8      @ g = clamped((74*y-25*u-52*v+DITHER_BIAS[0][1])>>8, 0, 63)      \n"
"ORR     r10, r10, r7, LSL #5    @ (r<<11 | g<<5 | b)                                             \n"
"STRH.W  r10, [%[x0]], #2        @ ctx->rgb_row[x] = yu2rgb565(y, u, v, 0)                        \n"
"ADD     r7, %[x7], %[x1], LSR #16  @ &ctx->y_row[source_x] : odd pixel                           \n"
"LDRH    r8, [r7]                @ 0:0:b:a                                                        \n"
"LDRH    r9, [r7, %[x2]]         @ 0:0:d:c                                                        \n"
"MOV     r11, #128                                                                                \n"
"USUB8   r9, r9, r8              @ 0:0:d-b:c-a                                                    \n"
"UXTAH   r10, r11, r8, ROR #24   @ (a<<8)+128                                                     \n"
"AND     r8, r8, #0xFF00         @ 0:0:b:0                                                        \n"
"ADD     r8, r8, r11             @ (b<<8)+128                                                     \n"
"SXTB    r7, r9                  @ 0:0:0:c-a                                                      \n"
"SXTB    r9, r9, ROR #8          @ 0:0:0:d-b                                                      \n"
"SMLABB  r10, r7, %[x3], r10     @ (a<<8)+(c-a)*yweight+128                                       \n"
"SMLABB  r8, r9, %[x3], r8       @ (b<<8)+(d-b)*yweight+128                                       \n"
"SUB     r8, r8, r10                                                                              \n"
"SXTH    r8, r8, ROR #8          @ (b-a)                                                          \n"
"ADD     r9, %[x1], r11          @ source_x_q16 + 128                                             \n"
"UBFX    r9, r9, #8, #8          @ xweight = ((source_x_q16&0xFFFF)+128)>>8                       \n"
"SMLABB  r8, r8, r9, r10                                                                          \n"
"UXTB    r8, r8, ROR #8          @ y=((a<<8)+(b-a)*xweight+128)>>8                                \n"
"LDR     r7, %[x4]               @ source_uv_xoffs_q16                                            \n"
"ADD     r7, %[x1], r7                                                                            \n"
"LSR     r7, r7, #17             @ source_x = (source_x_q16+ctx->source_uv_xoffs_q16)>>17         \n"
"ADD     %[x1], %[x5]            @ source_x_q16 += ctx->source_dx_q16                             \n"
"LDRB    r9, [%[x8], r7]         @ u = ctx->u_row[source_x]                                       \n"
"LDRB    r10, [%[x9], r7]        @ v = ctx->v_row[source_x]                                       \n"
"LDR     r7, =%[x15]             @ DITHER_BIAS[3][2]                                              \n"
"MOV     r11, #74                                                                                 \n"
"MOVT    r11, #129               @ 0:129:0:74                                                     \n"
"PKHBT   r9, r8, r9, LSL #16     @ 0:u:0:y                                                        \n"
"PKHBT   r8, r8, r10, LSL #16    @ 0:v:0:y                                                        \n"
"SMLAD   r7, r11, r9, r7         @ 74*y+129*u+DITHER_BIAS[3][2]                                   \n"
"USAT    r10, #5, r7, ASR #9     @ b = clamped((74*y+129*u+DITHER_BIAS[3][2])>>9, 0, 31)          \n"
"LDR     r7, =%[x13]             @ DITHER_BIAS[3][0]                                              \n"
"MOVT    r11, #102               @ 0:102:0:74                                                     \n"
"SMLAD   r7, r11, r8, r7         @ 74*y+102*v+DITHER_BIAS[3][0]                                   \n"
"USAT    r7, #5, r7, ASR #9      @ r =clamped((74*y+102*v+DITHER_BIAS[3][0])>>9, 0, 31)           \n"
"ORR     r10, r10, r7, LSL #11   @ (r<<11 | b)                                                    \n"
"LDR     r7, =%[x14]             @ DITHER_BIAS[3][1]                                              \n"
"MOVT    r11, 0xFFCC             @ 0:-52:0:74                                                     \n"
"SMLAD   r7, r11, r8, r7         @ 74*y-52*v+DITHER_BIAS[3][1]                                    \n"
"MOV     r11, #-25                                                                                \n"
"SMLABT  r7, r11, r9, r7         @ 74*y-25*u-52*v+DITHER_BIAS[3][1]                               \n"
"USAT    r7, #6, r7, ASR #8      @ g = clamped((74*y-25*u-52*v+DITHER_BIAS[3][1])>>8, 0, 63)      \n"
"ORR     r10, r10, r7, LSL #5    @ (r<<11 | g<<5 | b)                                             \n"
"STRH.W  r10, [%[x0]], #2        @ ctx->rgb_row[x] = yu2rgb565(y, u, v, 0)                        \n"
"SUBS    %[x6], %[x6], #2        @ width-=2                                                       \n"
"BNE     scale_loop                                                                               \n"
:
    [x0]"+r"(rgb_row),
    [x7]"+r"(y_row),
    [x8]"+r"(u_row),
    [x9]"+r"(v_row)
:
    [x1]"r"(source_x0_q16),
    [x2]"r"(y_pitch),
    [x3]"r"(yweight),
    [x4]"m"(source_uv_xoffs_q16),
    [x5]"r"(source_dx_q16),
    [x6]"r"(width),
    [x10]"i"(DITHER_BIAS[0][0]),
    [x11]"i"(DITHER_BIAS[0][1]),
    [x12]"i"(DITHER_BIAS[0][2]),
    [x13]"i"(DITHER_BIAS[3][0]),
    [x14]"i"(DITHER_BIAS[3][1]),
    [x15]"i"(DITHER_BIAS[3][2])
:   "cc", "memory", "r7", "r8", "r9", "r10", "r11");
}

void ScaleYUV2RGB565(const uint8_t *y_buf,
                     const uint8_t *u_buf,
                     const uint8_t *v_buf,
                     uint8_t *rgb_buf,
                     int source_width,
                     int source_height,
                     int width,
                     int height,
                     int y_pitch,
                     int uv_pitch,
                     int rgb_pitch,
                     YUVType yuv_type) {
  int source_x0_q16;
  int source_y0_q16;
  int source_dx_q16;
  int source_dy_q16;
  int source_uv_xoffs_q16;
  int source_uv_yoffs_q16;
  int x_shift;
  int y_shift;
  int ymax;
  int uvmax;

  const uint8_t *y_row, *u_row, *v_row;

  source_dx_q16 = (source_width<<16) / width;
  source_x0_q16 = (source_dx_q16>>1);
  source_dy_q16 = (source_height<<16) / height;
  source_y0_q16 = (source_dy_q16>>1);
  x_shift = (yuv_type != YV24);
  y_shift = (yuv_type == YV12);

  source_uv_xoffs_q16 = -(x_shift<<15);
  source_uv_yoffs_q16 = -(y_shift<<15);
  ymax = source_height-1;
  uvmax = ((ymax+1+y_shift)>>y_shift)-1;

  /*Nearest-neighbor scaling.*/
  
  /*Add rounding offsets once, in advance.*/
  source_uv_xoffs_q16 += (x_shift<<15);
  source_uv_yoffs_q16 += (y_shift<<15);

  for(int y=0; y<height; y++) {
    int source_y;
    uint16_t *rgb_row = (uint16_t *)(rgb_buf + y*rgb_pitch);
    source_y = source_y0_q16>>16;
    source_y = clamped(source_y, 0, ymax);
    y_row = y_buf + source_y*y_pitch;
    source_y = (source_y0_q16+source_uv_yoffs_q16)>>(16+y_shift);
    source_y = clamped(source_y, 0, uvmax);
    source_y0_q16 += source_dy_q16;
    u_row = u_buf + source_y*uv_pitch;
    v_row = v_buf + source_y*uv_pitch;

    ScaleYCbCr42xToRGB565_Nearest_Row_SIMD(
        rgb_row, y_row, u_row, v_row, width, source_x0_q16, source_dx_q16, source_uv_xoffs_q16);
  }
}
  
void ScaleYCbCrToRGB565(const uint8_t *y_buf,
                        const uint8_t *u_buf,
                        const uint8_t *v_buf,
                        uint8_t *rgb_buf,
                        int source_width,
                        int source_height,
                        int width,
                        int height,
                        int y_pitch,
                        int uv_pitch,
                        int rgb_pitch,
                        YUVType yuv_type,
                        ScaleFilter filter) {
  int source_x0_q16;
  int source_y0_q16;
  int source_dx_q16;
  int source_dy_q16;
  int source_uv_xoffs_q16;
  int source_uv_yoffs_q16;
  int x_shift;
  int y_shift;
  int ymin;
  int ymax;
  int uvmin;
  int uvmax;
#ifndef SUPPORT_SIMD
  int dither;
#endif
  const uint8_t *y_row, *u_row, *v_row;
  
  /*We don't support negative destination rectangles (just flip the source
     instead), and for empty ones there's nothing to do.*/
//  if (width <= 0 || height <= 0)
//    return;
  /*These bounds are required to avoid 16.16 fixed-point overflow.*/
  NS_ASSERTION(source_x0 > (INT_MIN>>16) && source_x0 < (INT_MAX>>16),
    "ScaleYCbCrToRGB565 source X offset out of bounds.");
  NS_ASSERTION(source_x0+source_width > (INT_MIN>>16)
            && source_x0+source_width < (INT_MAX>>16),
    "ScaleYCbCrToRGB565 source width out of bounds.");
  NS_ASSERTION(source_y0 > (INT_MIN>>16) && source_y0 < (INT_MAX>>16),
    "ScaleYCbCrToRGB565 source Y offset out of bounds.");
  NS_ASSERTION(source_y0+source_height > (INT_MIN>>16)
            && source_y0+source_height < (INT_MAX>>16),
    "ScaleYCbCrToRGB565 source height out of bounds.");
  /*We require the same stride for Y' and Cb and Cr for 4:4:4 content.*/
  NS_ASSERTION(yuv_type != YV24 || y_pitch == uv_pitch,
    "ScaleYCbCrToRGB565 luma stride differs from chroma for 4:4:4 content.");
  /*We assume we can read outside the bounds of the input, because it makes
     the code much simpler (and in practice is true: both Theora and VP8 return
     padded reference frames).
    In practice, we do not even _have_ the actual bounds of the source, as
     we are passed a crop rectangle from it, and not the dimensions of the full
     image.
    This assertion will not guarantee our out-of-bounds reads are safe, but it
     should at least catch the simple case of passing in an unpadded buffer.*/
/*
  NS_ASSERTION(abs(y_pitch) >= abs(source_width)+16,
    "ScaleYCbCrToRGB565 source image unpadded?");
*/
  /*The NEON code requires the pointers to be aligned to a 16-byte boundary at
     the start of each row.
    This should be true for all of our sources.
    We could try to fix this up if it's not true by adjusting source_x0, but
     that would require the mis-alignment to be the same for the U and V
     planes.*/
/*
  NS_ASSERTION((y_pitch&15) == 0 && (uv_pitch&15) == 0 &&
   (((uint32_t)y_buf)&15) == 0 &&
   (((uint32_t)u_buf)&15) == 0 &&
   (((uint32_t)v_buf)&15) == 0,
   "ScaleYCbCrToRGB565 source image unaligned");
*/
  /*We take an area-based approach to pixel coverage to avoid shifting by small
     amounts (or not so small, when up-scaling or down-scaling by a large
     factor).

    An illustrative example: scaling 4:2:0 up by 2, using JPEG chroma cositing^.

    + = RGB destination locations
    * = Y' source locations
    - = Cb, Cr source locations

    +   +   +   +  +   +   +   +
      *       *      *       *
    +   +   +   +  +   +   +   +
          -              -
    +   +   +   +  +   +   +   +
      *       *      *       *
    +   +   +   +  +   +   +   +

    +   +   +   +  +   +   +   +
      *       *      *       *
    +   +   +   +  +   +   +   +
          -              -
    +   +   +   +  +   +   +   +
      *       *      *       *
    +   +   +   +  +   +   +   +

    So, the coordinates of the upper-left + (first destination site) should
     be (-0.25,-0.25) in the source Y' coordinate system.
    Similarly, the coordinates should be (-0.375,-0.375) in the source Cb, Cr
     coordinate system.
    Note that the origin and scale of these two coordinate systems is not the
     same!

    ^JPEG cositing is required for Theora; VP8 doesn't specify cositing rules,
     but nearly all software converters in existence (at least those that are
     open source, and many that are not) use JPEG cositing instead of MPEG.*/
  source_dx_q16 = (source_width<<16) / width;
  source_x0_q16 = (source_dx_q16>>1)-0x8000;
  source_dy_q16 = (source_height<<16) / height;
  source_y0_q16 = (source_dy_q16>>1)-0x8000;
  x_shift = (yuv_type != YV24);
  y_shift = (yuv_type == YV12);
  /*These two variables hold the difference between the origins of the Y' and
     the Cb, Cr coordinate systems, using the scale of the Y' coordinate
     system.*/
  source_uv_xoffs_q16 = -(x_shift<<15);
  source_uv_yoffs_q16 = -(y_shift<<15);
  /*Compute the range of source rows we'll actually use.
    This doesn't guarantee we won't read outside this range.*/
  ymin = 0;//source_height >= 0 ? 0 : source_height-1;
  ymax = source_height-1;//source_height >= 0 ? source_height-1 : 0;
  uvmin = 0;//ymin>>y_shift;
  uvmax = ((ymax+1+y_shift)>>y_shift)-1;
  /*Pick a dithering pattern.
    The "&3" at the end is just in case RAND_MAX is lying.*/
#ifndef SUPPORT_SIMD
  dither = 0;//(rand()/(RAND_MAX>>2))&3;
#endif
  /*Nearest-neighbor scaling.*/
  if (filter == FILTER_NONE) {
    int y;
    /*Add rounding offsets once, in advance.*/
    source_x0_q16 += 0x8000;
    source_y0_q16 += 0x8000;
    source_uv_xoffs_q16 += (x_shift<<15);
    source_uv_yoffs_q16 += (y_shift<<15);
//    if (yuv_type == YV24)
//        return;

    for (y=0; y<height; y++) {
      int source_y;
      uint16_t *rgb_row = (uint16_t *)(rgb_buf + y*rgb_pitch);
      source_y = source_y0_q16>>16;
      source_y = clamped(source_y, 0, ymax);
      y_row = y_buf + source_y*y_pitch;
      source_y = (source_y0_q16+source_uv_yoffs_q16)>>(16+y_shift);
      source_y = clamped(source_y, uvmin, uvmax);
      source_y0_q16 += source_dy_q16;
      u_row = u_buf + source_y*uv_pitch;
      v_row = v_buf + source_y*uv_pitch;

//      ScaleYCbCr42xToRGB565_Nearest_Row_SIMD(
//          rgb_row, y_row, u_row, v_row, width, source_x0_q16, source_dx_q16, source_uv_xoffs_q16);
    }
  }
  /*Bilinear scaling.*/
  else {
    yuv2rgb565_row_scale_bilinear_ctx ctx;
#ifndef SUPPORT_SIMD
    yuv2rgb565_row_scale_bilinear_func scale_row;
#endif
    int uvxscale_min;
    int uvxscale_max;
    int uvyscale_min;
    int uvyscale_max;
    int y;
    /*Check how close the chroma scaling is to unity.
      If it's close enough, we can get away with nearest-neighbor chroma
       sub-sampling, and only doing bilinear on luma.
      If a given axis is subsampled, we use bounds on the luma step of
       [0.67...2], which is equivalent to scaling chroma by [1...3].
      If it's not subsampled, we use bounds of [0.5...1.33], which is
       equivalent to scaling chroma by [0.75...2].
      The lower bound is chosen as a trade-off between speed and how terrible
       nearest neighbor looks when upscaling.*/
# define CHROMA_NEAREST_SUBSAMP_STEP_MIN  0xAAAA
# define CHROMA_NEAREST_NORMAL_STEP_MIN   0x8000
# define CHROMA_NEAREST_SUBSAMP_STEP_MAX 0x20000
# define CHROMA_NEAREST_NORMAL_STEP_MAX  0x15555
    uvxscale_min = yuv_type != YV24 ?
     CHROMA_NEAREST_SUBSAMP_STEP_MIN : CHROMA_NEAREST_NORMAL_STEP_MIN;
    uvxscale_max = yuv_type != YV24 ?
     CHROMA_NEAREST_SUBSAMP_STEP_MAX : CHROMA_NEAREST_NORMAL_STEP_MAX;
    uvyscale_min = yuv_type == YV12 ?
     CHROMA_NEAREST_SUBSAMP_STEP_MIN : CHROMA_NEAREST_NORMAL_STEP_MIN;
    uvyscale_max = yuv_type == YV12 ?
     CHROMA_NEAREST_SUBSAMP_STEP_MAX : CHROMA_NEAREST_NORMAL_STEP_MAX;
    if (uvxscale_min <= abs(source_dx_q16)
     && abs(source_dx_q16) <= uvxscale_max
     && uvyscale_min <= abs(source_dy_q16)
     && abs(source_dy_q16) <= uvyscale_max) {
      /*Add the rounding offsets now.*/
      source_uv_xoffs_q16 += 1<<(15+x_shift);
      source_uv_yoffs_q16 += 1<<(15+y_shift);
#ifndef SUPPORT_SIMD
      if (yuv_type != YV24) {
        scale_row = ScaleYCbCr42xToRGB565_BilinearY_Row_C;
      }
      else
        scale_row = ScaleYCbCr444ToRGB565_BilinearY_Row_C;
#endif
    }
#ifndef SUPPORT_SIMD
    else {
      if (yuv_type == YV12)
        scale_row = ScaleYCbCr420ToRGB565_Bilinear_Row_C;
      else if (yuv_type == YV16)
        scale_row = ScaleYCbCr422ToRGB565_Bilinear_Row_C;
      else
        scale_row = ScaleYCbCr444ToRGB565_Bilinear_Row_C;
    }
#endif
    ctx.width = width;
    ctx.y_pitch = y_pitch;
    ctx.source_x0_q16 = source_x0_q16;
    ctx.source_dx_q16 = source_dx_q16;
    ctx.source_uv_xoffs_q16 = source_uv_xoffs_q16;
    ctx.uv_pitch = uv_pitch;
    
    for (y=0; y<height; y++) {
      int source_y;
      int yweight;
      int uvweight;
      ctx.rgb_row = (uint16_t *)(rgb_buf + y*rgb_pitch);
      source_y = (source_y0_q16+128)>>16;
      yweight = ((source_y0_q16+128)>>8)&0xFF;
      if (source_y < ymin) {
        source_y = ymin;
        yweight = 0;
      }
      if (source_y > ymax) {
        source_y = ymax;
        yweight = 0;
      }
      ctx.y_row = y_buf + source_y*y_pitch;
      source_y = source_y0_q16+source_uv_yoffs_q16+(128<<y_shift);
      source_y0_q16 += source_dy_q16;
      uvweight = source_y>>(8+y_shift)&0xFF;
      source_y >>= 16+y_shift;
      if (source_y < uvmin) {
        source_y = uvmin;
        uvweight = 0;
      }
      if (source_y > uvmax) {
        source_y = uvmax;
        uvweight = 0;
      }
      ctx.u_row = u_buf + source_y*uv_pitch;
      ctx.v_row = v_buf + source_y*uv_pitch;
      ctx.y_yweight = yweight;
      ctx.uv_yweight = uvweight;
#ifdef SUPPORT_SIMD
      ScaleYCbCr42xToRGB565_BilinearY_Row_SIMD(ctx.rgb_row, ctx.y_row, ctx.u_row, ctx.v_row,
                                               ctx.source_x0_q16, y_pitch, yweight,
                                               source_uv_xoffs_q16, source_dx_q16, ctx.width);
#else
      (*scale_row)(&ctx, dither);
      dither ^= 2;
#endif
    }
  }
}

#if 0
__asm__ volatile (
"ScaleYCbCr42xToRGB565_BilinearY_Row:                                                             \n"
"LDR     r5, %[x7]               @ y_row                                                          \n"
"LDR     r6, %[x8]               @ u_row                                                          \n"
"LDR     r7, %[x9]               @ v_row                                                          \n"
"LDR     r8, %[x10]              @ yuv                                                            \n"
"PLD     [r5]                                                                                     \n"
"scale_loop0:                                                                                     \n"
"LDR     r3, %[x2]               @ ctx.y_pitch                                                    \n"
"ADD     r0, r5, %[x1], LSR #16  @ &ctx->y_row[source_x_q16>>16]                                  \n"
"LDRH    r1, [r0]                @ 0:0:b:a                                                        \n"
"LDRH    r2, [r0, r3]            @ 0:0:d:c                                                        \n"
"USUB8   r2, r2, r1              @ 0:0:d-b:c-a                                                    \n"
"SXTH    r0, r2, ROR #24         @ 0:0:c-a:0                                                      \n"
"SSUB8   r2, r2, r0              @ 0:0:(d-b)-(c-a):c-a                                            \n"
"SXTB    r2, r2, ROR #8          @ (d-b)-(c-a)                                                    \n"
"UXTB    r4, r1                  @ a                                                              \n"
"RSB     r1, r4, r1, LSR #8      @ b-a                                                            \n"
"UXTH    r3, %[x1]               @                                                                \n"
"ADD     r3, r3, #128            @ source_x_q16&0xFFFF)+128                                       \n"
"MUL     r1, r1, r3              @ (xweight<<8)*(b-a)                                             \n"
"LSR     r3, r3, #8              @ xweight = ((source_x_q16&0xFFFF)+128)>>8                       \n"
"MLA     r2, r2, r3, r0          @ (c-a)<<8 + x*(d-b-(c-a))                                       \n"
"MLA     r2, r2, %[x3], r1       @ yweight*((c-a)<<8 + x*(d-b-(c-a)))+(xweight<<8)*(b-a)          \n"
"ADD     r0, r4, r2, ASR #16     @ y=a+(yweight*((c-a)<<8 +x*(d-b-(c-a)))+(xweight<<8)*(b-a))>>16 \n"
"ADD     r3, %[x1], %[x4]                                                                         \n"
"LSR     r3, r3, #17             @ source_x = (source_x_q16+ctx->source_uv_xoffs_q16)>>17         \n"
"LDRB    r1, [r6, r3]            @ u=ctx->u_row[source_x]                                         \n"
"LDRB    r2, [r7, r3]            @ v=ctx->v_row[source_x]                                         \n"
"ADD     %[x1], %[x5]            @ source_x_q16 += ctx->source_dx_q16                             \n"
"PKHBT   r1, r0, r1, LSL #16     @ 0:u:0:y                                                        \n"
"PKHBT   r0, r0, r2, LSL #16     @ 0:v:0:y                                                        \n"
"STRD    r1, r0, [r8], #8        @ *(yuv) = 0:u:0:y : 0:v:0:y, yuv+=8                             \n"
"SUBS    %[x6], %[x6], #1        @ width--                                                        \n"
"BNE     scale_loop0                                                                              \n"
::
    [x1] "r"(ctx.source_x0_q16),
    [x2] "m"(ctx.y_pitch),
    [x3] "r"(ctx.y_yweight),
    [x4] "r"(ctx.source_uv_xoffs_q16),
    [x5] "r"(ctx.source_dx_q16),
    [x6] "r"(ctx.width),
    [x7] "m"(ctx.y_row),
    [x8] "m"(ctx.u_row),
    [x9] "m"(ctx.v_row),
    [x10]"m"(ctx.yuv)
:   "cc", "memory", "r0", "r1", "r2", "r3", "r4", "r5","r6", "r7", "r8");

    ctx.width = width;
/* yuv2rgb*/
__asm__ volatile (
"LDR     r6,  =%[x3]           @ DITHER_BIAS[dither][0]                                          \n"
"LDR     r7,  =%[x4]           @ DITHER_BIAS[dither][1]                                          \n"
"LDR     r8,  =%[x5]           @ DITHER_BIAS[dither][2]                                          \n"
"LDR     r9,  =%[x6]           @ DITHER_BIAS[dither][0]                                          \n"
"LDR     r10, =%[x7]           @ DITHER_BIAS[dither][1]                                          \n"
"LDR     r11, =%[x8]           @ DITHER_BIAS[dither][2]                                          \n"
"LDR     r12, %[x1]            @ ctx.yuv                                                         \n"
"LDR     r2, %[x0]             @ ctx.rgb                                                         \n"
"MOV     r4, #74                                                                                 \n"
"scale_loop1:                                                                                    \n"
"LDRD    r0, r1, [r12], #8                                                                       \n"
"MOVT    r4, #129                                                                                \n"
"SMLAD   r5, r0, r4, r8         @ 74*y+129*u+DITHER_BIAS[dither][2]                              \n"
"USAT    r14, #5, r5, ASR #9    @ b = clamped((74*y+129*u+DITHER_BIAS[dither][2])>>9, 0, 31)     \n"
"MOVT    r4, #102                                                                                \n"
"SMLAD   r5, r1, r4, r6         @ 74*y+102*v+DITHER_BIAS[dither][0]                              \n"
"USAT    r5, #5, r5, ASR #9     @ r =clamped((74*y+102*v+DITHER_BIAS[dither][0])>>9, 0, 31)      \n"
"ORR     r14, r14, r5, LSL #11  @ (r<<11 | b)                                                    \n"
"MOVT    r4, 0xFFCC                                                                              \n"
"SMLAD   r5, r1, r4, r7         @ 74*y-52*v+DITHER_BIAS[dither][1]                               \n"
"MOV     r1, #-25                                                                                \n"
"SMLABT  r5, r1, r0, r5         @ 74*y-25*u-52*v+DITHER_BIAS[dither][1]                          \n"
"USAT    r5, #6, r5, ASR #8     @ g = clamped((74*y-25*u-52*v+DITHER_BIAS[dither][1])>>8, 0, 63) \n"
"ORR     r14, r14, r5, LSL #5   @ (r<<11 | g<<5 | b)                                             \n"
"STRH.W  r14, [r2], #2          @ ctx->rgb_row[x] = yu2rgb565(y, u, v, dither)                   \n"
"LDRD    r0, r1, [r12], #8                                                                       \n"
"MOVT    r4, #129                                                                                \n"
"SMLAD   r5, r0, r4, r11        @ 74*y+129*u+DITHER_BIAS[dither][2]                              \n"
"USAT    r14, #5, r5, ASR #9    @ b = clamped((74*y+129*u+DITHER_BIAS[dither][2])>>9, 0, 31)     \n"
"MOVT    r4, #102                                                                                \n"
"SMLAD   r5, r1, r4, r9         @ 74*y+102*v+DITHER_BIAS[dither][0]                              \n"
"USAT    r5, #5, r5, ASR #9     @ r =clamped((74*y+102*v+DITHER_BIAS[dither][0])>>9, 0, 31)      \n"
"ORR     r14, r14, r5, LSL #11  @ (r<<11 | b)                                                    \n"
"MOVT    r4, 0xFFCC                                                                              \n"
"SMLAD   r5, r1, r4, r10        @ 74*y-52*v+DITHER_BIAS[dither][1]                               \n"
"MOV     r1, #-25                                                                                \n"
"SMLABT  r5, r1, r0, r5         @ 74*y-25*u-52*v+DITHER_BIAS[dither][1]                          \n"
"USAT    r5, #6, r5, ASR #8     @ g = clamped((74*y-25*u-52*v+DITHER_BIAS[dither][1])>>8, 0, 63) \n"
"ORR     r14, r14, r5, LSL #5   @ (r<<11 | g<<5 | b)                                             \n"
"STRH.W  r14, [r2], #2          @ ctx->rgb_row[x] = yu2rgb565(y, u, v, dither)                   \n"
"SUBS    %[x2], %[x2], #2       @ width-=2                                                       \n"
"BNE     scale_loop1                                                                             \n"
::
    [x0]"m"(ctx.rgb_row),
    [x1]"m"(ctx.yuv),
    [x2]"r"(ctx.width),
    [x3]"i"(DITHER_BIAS[0][0]),
    [x4]"i"(DITHER_BIAS[0][1]),
    [x5]"i"(DITHER_BIAS[0][2]),
    [x6]"i"(DITHER_BIAS[3][0]),
    [x7]"i"(DITHER_BIAS[3][1]),
    [x8]"i"(DITHER_BIAS[3][2])
:   "cc", "memory", "r0", "r1", "r2", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12", "r14");
#endif //0
