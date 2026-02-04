/**
 * @file sgfx.h
 * @brief Software Graphics Library
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___SOFTGFX_H
#define ___SOFTGFX_H

#include <math_f32.h>
#include <graphics/color.h>
#include <utils.h>

// Enums mimicking OpenGL
#define SGFX_PROJECTION 0
#define SGFX_MODELVIEW 1

#define SGFX_LINES 0
#define SGFX_TRIANGLES 1
#define SGFX_QUADS 2 // For rectangles

#define SGFX_MAX_VERTICES 1200 // it should multiples of 2 (for lines), 3 (for triangles) and 4 (for quads)
#define SGFX_MAX_TEXTURES 16
#define SGFX_MAX_SUB_CONTEXT_DEPTH 32

typedef uint32_t sgfx_texture_t;

// Vector and matrix types
typedef struct sgfx_vec4_i32_t {
    int32_t x, y, z, w;
} sgfx_vec4_i32_t;

typedef struct sgfx_vec2_f32_t {
    float32_t x, y;
} sgfx_vec2_f32_t;

typedef struct sgfx_vec3_f32_t {
    float32_t x, y, z;
} sgfx_vec3_f32_t;

typedef struct sgfx_vec4_f32_t {
    float32_t x, y, z, w;
} sgfx_vec4_f32_t;

typedef union sgfx_mat4_f32_t {
    float32_t       m[16];
    sgfx_vec4_f32_t cols[4];
} sgfx_mat4_f32_t;

// Context
typedef struct sgfx_context_t sgfx_context_t;

typedef enum sgfx_cap_t {
    SGFX_CAP_TEXTURE_2D = BIT(0),
    SGFX_CAP_BLEND      = BIT(1),
    SGFX_CAP_DEPTH_TEST = BIT(2),
    SGFX_CAP_SCISSOR_TEST = BIT(3),
} sgfx_cap_t;

typedef enum {
    SGFX_TEXTURE_COLOR, // color_t (uint32_t BGRA)
    SGFX_TEXTURE_SDF // float32_t SDF atlas
} sgfx_texture_format_t;

// API
sgfx_context_t* sgfx_create_context(int32_t width, int32_t height, color_t* framebuffer);
void            sgfx_destroy_context(sgfx_context_t* ctx);

void sgfx_clear(sgfx_context_t* ctx, float32_t r, float32_t g, float32_t b, float32_t a);
void sgfx_clear_color(sgfx_context_t* ctx, color_t color);
void sgfx_swap_buffers(sgfx_context_t* ctx);

void sgfx_matrix_mode(sgfx_context_t* ctx, int32_t mode);
void sgfx_load_identity(sgfx_context_t* ctx);
void sgfx_translate_f32(sgfx_context_t* ctx, float32_t x, float32_t y, float32_t z);
void sgfx_rotate_f32(sgfx_context_t* ctx, float32_t angle, float32_t x, float32_t y, float32_t z);
void sgfx_scale_f32(sgfx_context_t* ctx, float32_t x, float32_t y, float32_t z);
void sgfx_ortho_f32(sgfx_context_t* ctx, float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t near, float32_t far);
void sgfx_perspective_f32(sgfx_context_t* ctx, float32_t fovy, float32_t aspect, float32_t near, float32_t far);

void      sgfx_enable(sgfx_context_t* ctx, sgfx_cap_t cap);
void      sgfx_disable(sgfx_context_t* ctx, sgfx_cap_t cap);
boolean_t sgfx_is_enabled(sgfx_context_t* ctx, sgfx_cap_t cap);

void sgfx_scissor(sgfx_context_t* ctx, int32_t x, int32_t y, int32_t w, int32_t h);

void sgfx_begin(sgfx_context_t* ctx, int32_t mode);
void sgfx_vertex3_f32(sgfx_context_t* ctx, float32_t x, float32_t y, float32_t z);
void sgfx_vertex2_f32(sgfx_context_t* ctx, float32_t x, float32_t y);
void sgfx_texcoord2_f32(sgfx_context_t* ctx, float32_t u, float32_t v);
void sgfx_color4_f32(sgfx_context_t* ctx, float32_t r, float32_t g, float32_t b, float32_t a);
void sgfx_end(sgfx_context_t* ctx);

sgfx_texture_t sgfx_gen_texture(sgfx_context_t* ctx);
void           sgfx_bind_texture(sgfx_context_t* ctx, sgfx_texture_t tex);
void           sgfx_tex_with_format(sgfx_context_t* ctx, int32_t width, int32_t height, const void* data, sgfx_texture_format_t format);
#define sgfx_tex_image2d(ctx, w, h, data) sgfx_tex_with_format(ctx, w, h, data, SGFX_TEXTURE_COLOR)
#define sgfx_tex_sdf(ctx, w, h, data)   sgfx_tex_with_format(ctx, w, h, data, SGFX_TEXTURE_SDF)


void sgfx_blit_glyph_color(sgfx_context_t* ctx, sgfx_texture_t tex,
                           float32_t src_x, float32_t src_y, float32_t src_w, float32_t src_h,
                           float32_t dst_x, float32_t dst_y, float32_t dst_w, float32_t dst_h,
                           color_t fg) __attribute__((deprecated("use texture binding")));

void sgfx_create_sub_context(sgfx_context_t* ctx,
                             int32_t x, int32_t y, int32_t width, int32_t height);
void sgfx_destroy_sub_context(sgfx_context_t* ctx);

#endif // ___SOFTGFX_H
