/**
 * @file sgfx.64.c
 * @brief Software Graphics Library Implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <graphics/softgfx.h>
#include <memory.h>
#include <strings.h>

MODULE("turnstone.kernel.graphics.sgfx");

void video_text_print(const char_t* text);

// Internal texture struct
typedef struct sgfx_texture_internal_t {
    sgfx_texture_format_t format;
    int32_t               width, height;
    union {
        float32_t* sdf_data; // for SDF format
        color_t*   color_data; // for color format
    };
} sgfx_texture_internal_t;

typedef enum sgfx_buffer_type_t {
    SGFX_BUFFER_FRONT = 0,
    SGFX_BUFFER_BACK,
    SGFX_BUFFER_FRAME,
    SGFX_BUFFER_COUNT
} sgfx_buffer_type_t;

typedef struct sgfx_vec4_i32_t {
    int32_t x, y, z, w;
} sgfx_vec4_i32_t;

typedef struct sgfx_context_info_t {
    int32_t         x, y, width, height;
    int32_t         ortho_left, ortho_right, ortho_bottom, ortho_top;
    sgfx_mat4_f32_t modelview;
    sgfx_mat4_f32_t projection;
    sgfx_vec4_i32_t scissor; // x, y, w, h
} sgfx_context_info_t;

// Context definition
struct sgfx_context_t {
    color_t* buffers[SGFX_BUFFER_COUNT];

    sgfx_context_info_t  contexts[SGFX_MAX_SUB_CONTEXT_DEPTH];
    int32_t              current_context_idx;
    sgfx_context_info_t* current_context;
    sgfx_context_info_t* base_context;

    sgfx_mat4_f32_t* current_matrix;
    int32_t          matrix_mode;

    sgfx_vec4_f32_t current_color;
    sgfx_vec2_f32_t current_texcoord;

    sgfx_texture_t bound_texture;

    sgfx_cap_t enabled_caps;

    // Immediate mode buffers
    sgfx_vec4_f32_t vertices[SGFX_MAX_VERTICES];
    sgfx_vec4_f32_t screen_vertices[SGFX_MAX_VERTICES];
    sgfx_vec2_f32_t texcoords[SGFX_MAX_VERTICES];
    sgfx_vec4_f32_t colors[SGFX_MAX_VERTICES];
    int32_t         vertex_count;
    int32_t         draw_mode;

    // Textures
    sgfx_texture_internal_t textures[SGFX_MAX_TEXTURES];
    uint32_t                texture_count;
};

static void sgfx_perspective_divide_by_count(sgfx_context_t* ctx, sgfx_vec4_f32_t* clip, int32_t count);
static void sgfx_viewport_by_count(sgfx_context_t* ctx, sgfx_vec4_f32_t* clip, int32_t count);

// Matrix helpers
static void sgfx_mat4_identity(sgfx_mat4_f32_t* mat) {
    memory_memset(mat->m, 0, sizeof(mat->m));
    mat->m[0] = mat->m[5] = mat->m[10] = mat->m[15] = 1.0f;
}

static void sgfx_mat4_mul(sgfx_mat4_f32_t* out, const sgfx_mat4_f32_t* a, const sgfx_mat4_f32_t* b) {
    sgfx_mat4_f32_t tmp;
    for (int i = 0; i < 4; ++i) { // row of a
        for (int j = 0; j < 4; ++j) { // column of b
            tmp.m[i * 4 + j] =
                a->m[i * 4 + 0] * b->m[0 * 4 + j] +
                a->m[i * 4 + 1] * b->m[1 * 4 + j] +
                a->m[i * 4 + 2] * b->m[2 * 4 + j] +
                a->m[i * 4 + 3] * b->m[3 * 4 + j];
        }
    }
    *out = tmp;
}

static void sgfx_mat4_vec4_mul(sgfx_vec4_f32_t* result, const sgfx_mat4_f32_t* mat, const sgfx_vec4_f32_t* vec) {
    sgfx_vec4_f32_t tmp;
    tmp.x = mat->m[0] * vec->x + mat->m[1] * vec->y + mat->m[2] * vec->z + mat->m[3] * vec->w;
    tmp.y = mat->m[4] * vec->x + mat->m[5] * vec->y + mat->m[6] * vec->z + mat->m[7] * vec->w;
    tmp.z = mat->m[8] * vec->x + mat->m[9] * vec->y + mat->m[10] * vec->z + mat->m[11] * vec->w;
    tmp.w = mat->m[12] * vec->x + mat->m[13] * vec->y + mat->m[14] * vec->z + mat->m[15] * vec->w;
    *result = tmp;
}

static void sgfx_mat4_translate(sgfx_mat4_f32_t* mat, float32_t x, float32_t y, float32_t z) {
    sgfx_mat4_f32_t trans;
    sgfx_mat4_identity(&trans);
    trans.m[3]  = x;
    trans.m[7]  = y;
    trans.m[11] = z;
    sgfx_mat4_mul(mat, mat, &trans);
}

static void sgfx_mat4_scale(sgfx_mat4_f32_t* mat, float32_t x, float32_t y, float32_t z) {
    sgfx_mat4_f32_t scale;
    sgfx_mat4_identity(&scale);
    scale.m[0] = x;
    scale.m[5] = y;
    scale.m[10] = z;
    sgfx_mat4_mul(mat, mat, &scale);
}

static void sgfx_mat4_rotate(sgfx_mat4_f32_t* mat, float32_t angle, float32_t x, float32_t y, float32_t z) {
    float32_t c = math_cos_f32(angle * math_pi_f32() / 180.0f);
    float32_t s = math_sin_f32(angle * math_pi_f32() / 180.0f);
    float32_t len = math_sqrt_f32(x * x + y * y + z * z);

    if (len != 0.0f) {
        x /= len; y /= len; z /= len;
    }

    sgfx_mat4_f32_t rot;
    sgfx_mat4_identity(&rot);

    rot.m[0] = x * x * (1 - c) + c;     rot.m[1] = x * y * (1 - c) - z * s; rot.m[2] = x * z * (1 - c) + y * s;
    rot.m[4] = y * x * (1 - c) + z * s; rot.m[5] = y * y * (1 - c) + c;     rot.m[6] = y * z * (1 - c) - x * s;
    rot.m[8] = z * x * (1 - c) - y * s; rot.m[9] = z * y * (1 - c) + x * s; rot.m[10] = z * z * (1 - c) + c;

    sgfx_mat4_mul(mat, mat, &rot);
}

static void sgfx_mat4_ortho(sgfx_mat4_f32_t* mat, float32_t l, float32_t r, float32_t b, float32_t t, float32_t n, float32_t f) {
    sgfx_mat4_identity(mat);
    mat->m[0] = 2.0f / (r - l); // scale X
    mat->m[5] = 2.0f / (t - b); // scale Y
    mat->m[10] = -2.0f / (f - n); // scale Z

    mat->m[3]  = -(r + l) / (r - l); // translate X
    mat->m[7]  = -(t + b) / (t - b); // translate Y
    mat->m[11] = -(f + n) / (f - n); // translate Z
}

static void sgfx_mat4_perspective(sgfx_mat4_f32_t* mat, float32_t fovy, float32_t aspect, float32_t n, float32_t f) {
    float32_t tan_half_fovy = math_tan_f32(fovy / 2.0f * math_pi_f32() / 180.0f);
    sgfx_mat4_identity(mat);
    mat->m[0] = 1.0f / (aspect * tan_half_fovy);
    mat->m[5] = 1.0f / tan_half_fovy;
    mat->m[10] = -(f + n) / (f - n);
    mat->m[11] = -1.0f;
    mat->m[14] = -2.0f * f * n / (f - n);
    mat->m[15] = 0.0f;
}

static sgfx_vec4_f32_t sgfx_mat4_mul_vec4(const sgfx_mat4_f32_t* mat, sgfx_vec4_f32_t v) {
    sgfx_vec4_f32_t out;
    out.x = mat->m[0] * v.x + mat->m[1] * v.y + mat->m[2] * v.z + mat->m[3] * v.w;
    out.y = mat->m[4] * v.x + mat->m[5] * v.y + mat->m[6] * v.z + mat->m[7] * v.w;
    out.z = mat->m[8] * v.x + mat->m[9] * v.y + mat->m[10] * v.z + mat->m[11] * v.w;
    out.w = mat->m[12] * v.x + mat->m[13] * v.y + mat->m[14] * v.z + mat->m[15] * v.w;
    return out;
}

// Modulate texture color with vertex color
static color_t sgfx_modulate_color(color_t tex, sgfx_vec4_f32_t vertex_color) {
    uint8_t vr = (uint8_t)(vertex_color.x * 255.0f);
    uint8_t vg = (uint8_t)(vertex_color.y * 255.0f);
    uint8_t vb = (uint8_t)(vertex_color.z * 255.0f);
    uint8_t va = (uint8_t)(vertex_color.w * 255.0f);

    uint8_t r = (uint8_t)((tex.red * vr) / 255);
    uint8_t g = (uint8_t)((tex.green * vg) / 255);
    uint8_t b = (uint8_t)((tex.blue * vb) / 255);
    uint8_t a = (uint8_t)((tex.alpha * va) / 255);

    return (color_t){.red = r, .green = g, .blue = b, .alpha = a};
}

// Alpha blend src with dst (used in plot_pixel)
static color_t sgfx_alpha_blend(color_t src, color_t dst) {
    if (src.alpha == 0) return dst;  // Skip fully transparent
    float32_t a = (float32_t)src.alpha / 255.0f;
    uint8_t r = (uint8_t)(src.red * a + dst.red * (1.0f - a));
    uint8_t g = (uint8_t)(src.green * a + dst.green * (1.0f - a));
    uint8_t b = (uint8_t)(src.blue * a + dst.blue * (1.0f - a));
    uint8_t aa = (uint8_t)(src.alpha * a + dst.alpha * (1.0f - a)); // Or max(src.alpha, dst.alpha) if preferred
    return (color_t){.red = r, .green = g, .blue = b, .alpha = aa};
}

// Pixel plotting with clip
static void sgfx_plot_pixel(sgfx_context_t* ctx, int32_t x, int32_t y, color_t pix) {
    if (x < 0 ||
        x >= ctx->base_context->width ||
        y < 0 ||
        y >= ctx->base_context->height) {
        return;
    }

    if (sgfx_is_enabled(ctx, SGFX_CAP_SCISSOR_TEST)) {
        if (x < ctx->current_context->scissor.x ||
            x >= ctx->current_context->scissor.x + ctx->current_context->scissor.z ||
            y < ctx->current_context->scissor.y ||
            y >= ctx->current_context->scissor.y + ctx->current_context->scissor.w) {
            return;
        }
    }

    int32_t idx = y * ctx->base_context->width + x;
    color_t dst = ctx->buffers[SGFX_BUFFER_FRONT][idx];
    ctx->buffers[SGFX_BUFFER_FRONT][idx] = sgfx_alpha_blend(pix, dst);
}

// Texture sample nearest
static color_t sgfx_sample_texture(const sgfx_texture_internal_t* tex, float32_t u, float32_t v) {
    if (!tex || tex->width <= 0 || tex->height <= 0) {
        return (color_t){{0, 0, 0, 0}};
    }

    // Map u,v [0,1] to pixel coordinates
    int32_t tx = (int32_t)(u * (float32_t)tex->width);
    int32_t ty = (int32_t)(v * (float32_t)tex->height);

    // Clamp to edge
    if (tx < 0) tx = 0;
    else if (tx >= tex->width) tx = tex->width - 1;

    if (ty < 0) ty = 0;
    else if (ty >= tex->height) ty = tex->height - 1;

    if (tex->format == SGFX_TEXTURE_COLOR) {
        return tex->color_data[ty * tex->width + tx];
    } else if (tex->format == SGFX_TEXTURE_SDF) {
        float32_t alpha = tex->sdf_data[ty * tex->width + tx] * 255.0f;
        if (alpha < 0.0f) alpha = 0.0f;
        else if (alpha > 255.0f) alpha = 255.0f;

        // Return white with alpha from SDF
        return (color_t){.red = 255, .green = 255, .blue = 255, .alpha = (uint8_t)alpha};
    }

    return (color_t){{0, 0, 0, 0}};
}

static boolean_t sgfx_point_in_triangle(float32_t px, float32_t py,
                                        const sgfx_vec4_f32_t* a,
                                        const sgfx_vec4_f32_t* b,
                                        const sgfx_vec4_f32_t* c) {
    // Barycentric technique
    float32_t v0x = b->x - a->x;
    float32_t v0y = b->y - a->y;
    float32_t v1x = c->x - a->x;
    float32_t v1y = c->y - a->y;
    float32_t v2x = px - a->x;
    float32_t v2y = py - a->y;

    float32_t dot00 = v0x * v0x + v0y * v0y;
    float32_t dot01 = v0x * v1x + v0y * v1y;
    float32_t dot02 = v0x * v2x + v0y * v2y;
    float32_t dot11 = v1x * v1x + v1y * v1y;
    float32_t dot12 = v1x * v2x + v1y * v2y;

    float32_t invDenom = 1.0f / (dot00 * dot11 - dot01 * dot01);
    float32_t u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    float32_t v = (dot00 * dot12 - dot01 * dot02) * invDenom;

    return (u >= 0.0f) && (v >= 0.0f) && (u + v <= 1.0f);
}

static boolean_t sgfx_point_in_quad(float32_t x, float32_t y, const sgfx_vec4_f32_t* c) {
    // Divide quad into two triangles: (0,1,2) and (2,1,3)
    return sgfx_point_in_triangle(x, y, &c[0], &c[1], &c[2]) ||
           sgfx_point_in_triangle(x, y, &c[2], &c[1], &c[3]);
}

// Rasterizers
static void sgfx_draw_line(sgfx_context_t* ctx,
                           sgfx_vec2_f32_t p0, sgfx_vec2_f32_t p1,
                           sgfx_vec4_f32_t c0, sgfx_vec4_f32_t c1,
                           sgfx_vec2_f32_t t0, sgfx_vec2_f32_t t1) {
    // Bresenham
    int32_t x0 = (int32_t)p0.x, y0 = (int32_t)p0.y, x1 = (int32_t)p1.x, y1 = (int32_t)p1.y;
    int32_t dx = math_fabs_f32((float32_t)(x1 - x0)), sx = x0 < x1 ? 1 : -1;
    int32_t dy = -math_fabs_f32((float32_t)(y1 - y0)), sy = y0 < y1 ? 1 : -1;
    int32_t err = dx + dy, e2;
    while (1) {
        float32_t lerp_t = (float32_t)math_fabs_f32((float32_t)(x0 - (int32_t)p0.x)) / (float32_t)dx; // Approx
        sgfx_vec4_f32_t color = { math_lerp_f32(c0.x, c1.x, lerp_t), math_lerp_f32(c0.y, c1.y, lerp_t), math_lerp_f32(c0.z, c1.z, lerp_t), math_lerp_f32(c0.w, c1.w, lerp_t) };
        sgfx_vec2_f32_t tex = { math_lerp_f32(t0.x, t1.x, lerp_t), math_lerp_f32(t0.y, t1.y, lerp_t) };

        color_t pix = {
            .red = (uint32_t)(color.x * 255.0f),
            .green = (uint32_t)(color.y * 255.0f),
            .blue = (uint32_t)(color.z * 255.0f),
            .alpha = (uint32_t)(color.w * 255.0f)
        };

        if (ctx->bound_texture) {
            color_t tex_pix = sgfx_sample_texture(&ctx->textures[ctx->bound_texture - 1], tex.x, tex.y);
            pix = sgfx_modulate_color(tex_pix, color); // Modulate texture with vertex color

            if (pix.alpha == 0) {
                continue;
            }
        } else {
            if (pix.alpha == 0) {
                continue;
            }
        }

        sgfx_plot_pixel(ctx, x0, y0, pix);

        if (x0 == x1 && y0 == y1) {
            break;
        }

        e2 = 2 * err;

        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }

        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void sgfx_draw_triangle(sgfx_context_t* ctx,
                               sgfx_vec2_f32_t p0, sgfx_vec2_f32_t p1, sgfx_vec2_f32_t p2,
                               sgfx_vec4_f32_t c0, sgfx_vec4_f32_t c1, sgfx_vec4_f32_t c2,
                               sgfx_vec2_f32_t t0, sgfx_vec2_f32_t t1, sgfx_vec2_f32_t t2) {
    // Barycentric
    int32_t min_x = (int32_t)math_min_f32(math_min_f32(p0.x, p1.x), p2.x);
    int32_t min_y = (int32_t)math_min_f32(math_min_f32(p0.y, p1.y), p2.y);
    int32_t max_x = (int32_t)math_max_f32(math_max_f32(p0.x, p1.x), p2.x) + 1;
    int32_t max_y = (int32_t)math_max_f32(math_max_f32(p0.y, p1.y), p2.y) + 1;

    for (int32_t y = min_y; y < max_y; ++y) {
        for (int32_t x = min_x; x < max_x; ++x) {
            float32_t bx = (float32_t)x + 0.5f, by = (float32_t)y + 0.5f;
            float32_t w0 = ((p1.y - p2.y) * (bx - p2.x) + (p2.x - p1.x) * (by - p2.y)) / ((p1.y - p2.y) * (p0.x - p2.x) + (p2.x - p1.x) * (p0.y - p2.y));
            float32_t w1 = ((p2.y - p0.y) * (bx - p2.x) + (p0.x - p2.x) * (by - p2.y)) / ((p1.y - p2.y) * (p0.x - p2.x) + (p2.x - p1.x) * (p0.y - p2.y));
            float32_t w2 = 1.0f - w0 - w1;

            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                sgfx_vec4_f32_t color = {
                    w0 * c0.x + w1 * c1.x + w2 * c2.x,
                    w0 * c0.y + w1 * c1.y + w2 * c2.y,
                    w0 * c0.z + w1 * c1.z + w2 * c2.z,
                    w0 * c0.w + w1 * c1.w + w2 * c2.w
                };

                sgfx_vec2_f32_t tex = {
                    w0 * t0.x + w1 * t1.x + w2 * t2.x,
                    w0 * t0.y + w1 * t1.y + w2 * t2.y
                };

                color_t pix = {
                    .red = (uint32_t)(color.x * 255.0f),
                    .green = (uint32_t)(color.y * 255.0f),
                    .blue = (uint32_t)(color.z * 255.0f),
                    .alpha = (uint32_t)(color.w * 255.0f)
                };

                if (ctx->bound_texture) {
                    color_t tex_pix = sgfx_sample_texture(&ctx->textures[ctx->bound_texture - 1], tex.x, tex.y);
                    pix = sgfx_modulate_color(tex_pix, color); // Modulate texture with vertex color

                    if (pix.alpha == 0) {
                        continue;
                    }
                } else {
                    if (pix.alpha == 0) {
                        continue;
                    }
                }

                sgfx_plot_pixel(ctx, x, y, pix);
            }
        }
    }
}

// Context creation
sgfx_context_t* sgfx_create_context(int32_t width, int32_t height, color_t* framebuffer) {
    sgfx_context_t* ctx = (sgfx_context_t*)memory_malloc(sizeof(sgfx_context_t));

    if (!ctx) {
        return NULL;
    }

    ctx->buffers[SGFX_BUFFER_FRONT] = memory_malloc_ext(NULL, width * height * sizeof(uint32_t), 0x1000); // 4K aligned for SIMD
    ctx->buffers[SGFX_BUFFER_BACK] = memory_malloc_ext(NULL, width * height * sizeof(uint32_t), 0x1000); // 4K aligned for SIMD

    if (!ctx->buffers[SGFX_BUFFER_FRONT] || !ctx->buffers[SGFX_BUFFER_BACK]) {
        if (ctx->buffers[SGFX_BUFFER_FRONT]) memory_free(ctx->buffers[SGFX_BUFFER_FRONT]);
        if (ctx->buffers[SGFX_BUFFER_BACK]) memory_free(ctx->buffers[SGFX_BUFFER_BACK]);
        memory_free(ctx);
        return NULL;
    }

    ctx->buffers[SGFX_BUFFER_FRAME] = framebuffer;

    ctx->current_context = &ctx->contexts[0];
    ctx->base_context = &ctx->contexts[0];
    ctx->current_context_idx = 0;

    // boundaries
    ctx->current_context->x = 0;
    ctx->current_context->y = 0;
    ctx->current_context->width = width;
    ctx->current_context->height = height;

    // Orthographic projection boundaries
    sgfx_mat4_identity(&ctx->current_context->projection);
    sgfx_mat4_ortho(&ctx->current_context->projection, 0.0f, (float32_t)width, (float32_t)height, 0.0f, -1.0f, 1.0f);
    ctx->current_matrix = &ctx->current_context->modelview;
    sgfx_mat4_identity(&ctx->current_context->modelview);
    ctx->matrix_mode = SGFX_MODELVIEW;

    // default ortho boundaries
    ctx->current_context->ortho_left = 0;
    ctx->current_context->ortho_right = width;
    ctx->current_context->ortho_top = 0;
    ctx->current_context->ortho_bottom = height;

    return ctx;
}

void sgfx_destroy_context(sgfx_context_t* ctx) {
    for (uint32_t i = 0; i < ctx->texture_count; ++i) {
        if (ctx->textures[i].format == SGFX_TEXTURE_COLOR) {
            memory_free(ctx->textures[i].color_data);
        } else if (ctx->textures[i].format == SGFX_TEXTURE_SDF) {
            memory_free(ctx->textures[i].sdf_data);
        }
    }

    if (ctx->buffers[0]) memory_free(ctx->buffers[0]);
    if (ctx->buffers[1]) memory_free(ctx->buffers[1]);

    memory_free(ctx);
}

void sgfx_create_sub_context(sgfx_context_t* ctx,
                             int32_t x, int32_t y, int32_t width, int32_t height) {

    if (ctx->current_context_idx + 1 >= SGFX_MAX_SUB_CONTEXT_DEPTH) {
        return; // Max depth reached
    }

    ctx->current_context_idx++;

    ctx->current_context = &ctx->contexts[ctx->current_context_idx];

    // boundaries
    ctx->current_context->x = x;
    ctx->current_context->y = y;
    ctx->current_context->width = width;
    ctx->current_context->height = height;

    // Orthographic projection boundaries
    sgfx_mat4_identity(&ctx->current_context->projection);
    sgfx_mat4_ortho(&ctx->current_context->projection, 0.0f, (float32_t)width, (float32_t)height, 0.0f, -1.0f, 1.0f);
    sgfx_mat4_identity(&ctx->current_context->modelview);
    ctx->current_matrix = &ctx->current_context->modelview;
    ctx->matrix_mode = SGFX_MODELVIEW;

    // default ortho boundaries
    ctx->current_context->ortho_left = 0;
    ctx->current_context->ortho_right = width;
    ctx->current_context->ortho_top = 0;
    ctx->current_context->ortho_bottom = height;
}

void sgfx_destroy_sub_context(sgfx_context_t* ctx) {
    if (ctx->current_context_idx <= 0) {
        return; // No subcontext to destroy
    }

    ctx->current_context_idx--;
    ctx->current_context = &ctx->contexts[ctx->current_context_idx];
    ctx->current_matrix = &ctx->current_context->modelview;
    ctx->matrix_mode = SGFX_MODELVIEW;
}

void sgfx_clear(sgfx_context_t* ctx, float32_t r, float32_t g, float32_t b, float32_t a) {
    color_t color = {
        .red = (uint32_t)(r * 255.0f),
        .green = (uint32_t)(g * 255.0f),
        .blue = (uint32_t)(b * 255.0f),
        .alpha = (uint32_t)(a * 255.0f)
    };

    color_t* buf = ctx->buffers[SGFX_BUFFER_FRONT];

    if (ctx->current_context_idx > 0) {
        // Define subcontext rectangle corners in projection(?) space (same as vertices)
        sgfx_vec4_f32_t corners[4] = {
            { ctx->current_context->ortho_left, ctx->current_context->ortho_top, 0.0f, 1.0f }, // Top-left
            { ctx->current_context->ortho_right, ctx->current_context->ortho_top, 0.0f, 1.0f }, // Top-right
            { ctx->current_context->ortho_left, ctx->current_context->ortho_bottom, 0.0f, 1.0f }, // Bottom-left
            { ctx->current_context->ortho_right, ctx->current_context->ortho_bottom, 0.0f, 1.0f } // Bottom-right
        };

        // Compute MVP matrix (row-major)
        sgfx_mat4_f32_t mvp;
        sgfx_mat4_mul(&mvp, &ctx->current_context->projection, &ctx->current_context->modelview);

        // Transform corners to clip space
        sgfx_vec4_f32_t clip_corners[ARRAY_SIZE(corners)];
        for (int i = 0; i < 4; ++i) {
            sgfx_mat4_vec4_mul(&clip_corners[i], &mvp, &corners[i]);
        }

        // Apply perspective division
        sgfx_perspective_divide_by_count(ctx, clip_corners, ARRAY_SIZE(corners));

        // Map to viewport (pixel) coordinates
        sgfx_viewport_by_count(ctx, clip_corners, ARRAY_SIZE(corners));

        // Find bounding box in pixel space
        int32_t min_x = ctx->base_context->width, max_x = 0, min_y = ctx->base_context->height, max_y = 0;
        for (int32_t i = 0; i < 4; ++i) {
            int32_t x = (int32_t)clip_corners[i].x;
            int32_t y = (int32_t)clip_corners[i].y;
            min_x = x < min_x ? x : min_x;
            max_x = x > max_x ? x : max_x;
            min_y = y < min_y ? y : min_y;
            max_y = y > max_y ? y : max_y;
        }

        // Clamp to buffer bounds
        min_x = min_x < 0 ? 0 : min_x;
        max_x = max_x >= ctx->base_context->width ? ctx->base_context->width - 1 : max_x;
        min_y = min_y < 0 ? 0 : min_y;
        max_y = max_y >= ctx->base_context->height ? ctx->base_context->height - 1 : max_y;

        // Clear the bounding box
        for (int32_t y = min_y; y <= max_y; ++y) {
            for (int32_t x = min_x; x <= max_x; ++x) {
                if (sgfx_point_in_quad((float32_t)x + 0.5f, (float32_t)y + 0.5f, clip_corners)) {
                    buf[y * ctx->base_context->width + x] = color;
                }
            }
        }

        return;
    }

    for (int i = 0; i < ctx->base_context->width * ctx->base_context->height; ++i) {
        buf[i] = color;
    }
}

#ifdef __AVX512F__
// AVX-512 version
static inline void sgfx_blit_changed_pixels(
    const color_t * front,
    color_t *       back,
    color_t *       frame,
    size_t          size
    ) {
    // Ensure size is multiple of 16
    for (size_t idx = 0; idx < size; idx += 16) {
        size_t offset = idx * sizeof(color_t);

        asm volatile (
            // Load 16 pixels from curr and prev
            "vmovdqu32   (%0,%3), %%zmm0\n\t"
            "vmovdqu32   (%1,%3), %%zmm1\n\t"
            // Compare for not equal, result in k1
            "vpcmpd      $4, %%zmm1, %%zmm0, %%k1\n\t"
            // Test if mask is zero (all equal)
            "kortestw    %%k1, %%k1\n\t"
            "jz .L%=\n\t" // jump forward to unique local label
            // Masked store for differing pixels
            "vmovdqu32   %%zmm0, (%1,%3)%{%%k1}\n\t" // update back buffer
            "vmovdqu32   %%zmm0, (%2,%3)%{%%k1}\n\t" // update frame buffer
            ".L%=:\n\t" // local label for skipping store
            :
            : "r" (front), "r" (back), "r" (frame), "r" ((long)offset)
            : "memory", "zmm0", "zmm1", "k1"
            );
    }
}
#else
// AVX2 version
static inline void sgfx_blit_changed_pixels(
    const color_t * front,
    color_t *       back,
    color_t *       frame,
    size_t          size
    ) {
    size_t i = 0;
    size_t stride = 8; // 8 pixels per __m256 (8 * 32-bit = 256-bit)

    for (; i + stride <= size; i += stride) {
        long offset = i * sizeof(color_t);

        asm volatile (
            // Load 8 pixels from front and back
            "vmovdqu   (%0,%3), %%ymm0\n\t"
            "vmovdqu   (%1,%3), %%ymm1\n\t"

            // Compare each 32-bit lane
            "vpcmpeqd  %%ymm1, %%ymm0, %%ymm2\n\t"
            "vpcmpeqd  %%ymm0, %%ymm0, %%ymm3\n\t" // all ones mask
            "vpxor     %%ymm2, %%ymm3, %%ymm2\n\t" // invert mask -> 0xFFFFFFFF if different

            // Blend manually
            "vblendvps %%ymm1, %%ymm0, %%ymm2, %%ymm0\n\t"

            // Store to back and frame
            "vmovdqu   %%ymm0, (%1,%3)\n\t"
            "vmovdqu   %%ymm0, (%2,%3)\n\t"
            :
            : "r" (front), "r" (back), "r" (frame), "r" (offset)
            : "memory", "ymm0", "ymm1", "ymm2", "ymm3"
            );
    }
}
#endif

void sgfx_swap_buffers(sgfx_context_t* ctx) {
    int32_t size = ctx->base_context->width * ctx->base_context->height;

    sgfx_blit_changed_pixels(
        ctx->buffers[SGFX_BUFFER_FRONT],
        ctx->buffers[SGFX_BUFFER_BACK],
        ctx->buffers[SGFX_BUFFER_FRAME],
        size
        );
}

// Matrix functions
void sgfx_matrix_mode(sgfx_context_t* ctx, int32_t mode) {
    ctx->matrix_mode = mode;
    ctx->current_matrix = (mode == SGFX_PROJECTION) ?
                          &ctx->current_context->projection :
                          &ctx->current_context->modelview;
}

void sgfx_load_identity(sgfx_context_t* ctx) {
    sgfx_mat4_identity(ctx->current_matrix);
}

void sgfx_translate_f32(sgfx_context_t* ctx, float32_t x, float32_t y, float32_t z) {
    sgfx_mat4_translate(ctx->current_matrix, x, y, z);
}

void sgfx_rotate_f32(sgfx_context_t* ctx, float32_t angle, float32_t x, float32_t y, float32_t z) {
    sgfx_mat4_rotate(ctx->current_matrix, angle, x, y, z);
}

void sgfx_scale_f32(sgfx_context_t* ctx, float32_t x, float32_t y, float32_t z) {
    sgfx_mat4_scale(ctx->current_matrix, x, y, z);
}

void sgfx_ortho_f32(sgfx_context_t* ctx, float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t near, float32_t far) {
    sgfx_mat4_ortho(ctx->current_matrix, left, right, bottom, top, near, far);
    ctx->current_context->ortho_left = left;
    ctx->current_context->ortho_right = right;
    ctx->current_context->ortho_bottom = bottom;
    ctx->current_context->ortho_top = top;
}

void sgfx_perspective_f32(sgfx_context_t* ctx, float32_t fovy, float32_t aspect, float32_t near, float32_t far) {
    sgfx_mat4_perspective(ctx->current_matrix, fovy, aspect, near, far);
}

// Capabilities enable/disable
void sgfx_enable(sgfx_context_t* ctx, sgfx_cap_t cap) {
    ctx->enabled_caps |= cap;
}

void sgfx_disable(sgfx_context_t* ctx, sgfx_cap_t cap) {
    ctx->enabled_caps &= ~cap;
}

boolean_t sgfx_is_enabled(sgfx_context_t* ctx, sgfx_cap_t cap) {
    return (ctx->enabled_caps & cap) != 0;
}

// Scissor
void sgfx_scissor(sgfx_context_t* ctx, int32_t x, int32_t y, int32_t w, int32_t h) {
    ctx->current_context->scissor.x = x;
    ctx->current_context->scissor.y = y;
    ctx->current_context->scissor.z = w;
    ctx->current_context->scissor.w = h;
}

// Drawing
void sgfx_begin(sgfx_context_t* ctx, int32_t mode) {
    ctx->draw_mode = mode;
    ctx->vertex_count = 0;
}

void sgfx_vertex3_f32(sgfx_context_t* ctx, float32_t x, float32_t y, float32_t z) {
    ctx->vertices[ctx->vertex_count] = (sgfx_vec4_f32_t){x, y, z, 1.0f};
    ctx->texcoords[ctx->vertex_count] = ctx->current_texcoord;
    ctx->colors[ctx->vertex_count] = ctx->current_color;
    ctx->vertex_count++;

    if (ctx->vertex_count >= SGFX_MAX_VERTICES) {
        // Flush if max vertices reached
        // use same draw mode
        sgfx_end(ctx);
        ctx->vertex_count = 0;
    }
}

void sgfx_texcoord2_f32(sgfx_context_t* ctx, float32_t u, float32_t v) {
    ctx->current_texcoord = (sgfx_vec2_f32_t){u, v};
}

void sgfx_color4_f32(sgfx_context_t* ctx, float32_t r, float32_t g, float32_t b, float32_t a) {
    ctx->current_color = (sgfx_vec4_f32_t){r, g, b, a};
}

static void sgfx_vertex_shader(sgfx_context_t* ctx, sgfx_mat4_f32_t* mvp, sgfx_vec4_f32_t* out) {
    for(int32_t i = 0; i < ctx->vertex_count; ++i) {
        out[i] = sgfx_mat4_mul_vec4(mvp, ctx->vertices[i]);
    }
}

static void sgfx_perspective_divide_by_count(sgfx_context_t* ctx, sgfx_vec4_f32_t* clip, int32_t count) {
    UNUSED(ctx);
    for(int32_t i = 0; i < count; ++i) {
        if(clip[i].w > math_epsilon_f32()) {
            clip[i].x /= clip[i].w;
            clip[i].y /= clip[i].w;
            clip[i].z /= clip[i].w;
        } else {
            clip[i].x = clip[i].y = clip[i].z = 0.0f;
        }
    }
}

static void sgfx_perspective_divide(sgfx_context_t* ctx, sgfx_vec4_f32_t* clip) {
    sgfx_perspective_divide_by_count(ctx, clip, ctx->vertex_count);
}

static void sgfx_viewport_by_count(sgfx_context_t* ctx, sgfx_vec4_f32_t* clip, int32_t count) {
    for(int32_t i = 0; i < count; ++i) {
        clip[i].x = (clip[i].x * 0.5f + 0.5f) * (float32_t)ctx->current_context->width + (float32_t)ctx->current_context->x;
        clip[i].y = (1.0f - (clip[i].y * 0.5f + 0.5f)) * (float32_t)ctx->current_context->height + (float32_t)ctx->current_context->y;
    }
}

static void sgfx_viewport(sgfx_context_t* ctx, sgfx_vec4_f32_t* clip) {
    sgfx_viewport_by_count(ctx, clip, ctx->vertex_count);
}

void sgfx_end(sgfx_context_t* ctx) {
    if (ctx->vertex_count == 0) {
        return;
    }

    sgfx_mat4_f32_t mvp;
    sgfx_mat4_mul(&mvp, &ctx->current_context->projection, &ctx->current_context->modelview);

    sgfx_vec4_f32_t* screen_verts = ctx->screen_vertices;

    sgfx_vertex_shader(ctx, &mvp, screen_verts);
    sgfx_perspective_divide(ctx, screen_verts);
    sgfx_viewport(ctx, screen_verts);

    if (ctx->draw_mode == SGFX_LINES) {
        for (int32_t i = 0; i < ctx->vertex_count; i += 2) {
            sgfx_vec2_f32_t p0 = {screen_verts[i].x, screen_verts[i].y};
            sgfx_vec2_f32_t p1 = {screen_verts[i + 1].x, screen_verts[i + 1].y};



            sgfx_draw_line(ctx,
                           p0, p1,
                           ctx->colors[i], ctx->colors[i + 1],
                           ctx->texcoords[i], ctx->texcoords[i + 1]
                           );
        }
    } else if (ctx->draw_mode == SGFX_TRIANGLES) {
        for (int32_t i = 0; i < ctx->vertex_count; i += 3) {
            sgfx_vec2_f32_t p0 = {screen_verts[i].x, screen_verts[i].y};
            sgfx_vec2_f32_t p1 = {screen_verts[i + 1].x, screen_verts[i + 1].y};
            sgfx_vec2_f32_t p2 = {screen_verts[i + 2].x, screen_verts[i + 2].y};

            sgfx_draw_triangle(ctx,
                               p0, p1, p2,
                               ctx->colors[i], ctx->colors[i + 1], ctx->colors[i + 2],
                               ctx->texcoords[i], ctx->texcoords[i + 1], ctx->texcoords[i + 2]
                               );
        }
    } else if (ctx->draw_mode == SGFX_QUADS) {
        for (int32_t i = 0; i < ctx->vertex_count; i += 4) {
            sgfx_vec2_f32_t p0 = {screen_verts[i].x, screen_verts[i].y};
            sgfx_vec2_f32_t p1 = {screen_verts[i + 1].x, screen_verts[i + 1].y};
            sgfx_vec2_f32_t p2 = {screen_verts[i + 2].x, screen_verts[i + 2].y};
            sgfx_vec2_f32_t p3 = {screen_verts[i + 3].x, screen_verts[i + 3].y};

            sgfx_draw_triangle(ctx, p0, p1, p2,
                               ctx->colors[i], ctx->colors[i + 1], ctx->colors[i + 2],
                               ctx->texcoords[i], ctx->texcoords[i + 1], ctx->texcoords[i + 2]
                               );
            sgfx_draw_triangle(ctx, p0, p2, p3,
                               ctx->colors[i], ctx->colors[i + 2], ctx->colors[i + 3],
                               ctx->texcoords[i], ctx->texcoords[i + 2], ctx->texcoords[i + 3]
                               );
        }
    }
}

// Textures
sgfx_texture_t sgfx_gen_texture(sgfx_context_t* ctx) {
    if (ctx->texture_count >= 16) {return 0;}

    return ++ctx->texture_count;
}

void sgfx_bind_texture(sgfx_context_t* ctx, sgfx_texture_t tex) {
    if (tex > 0 && tex <= ctx->texture_count) {
        ctx->bound_texture = tex;
    } else {
        ctx->bound_texture = 0;
    }
}

void sgfx_tex_with_format(sgfx_context_t* ctx, int32_t width, int32_t height, const void* data, sgfx_texture_format_t format) {
    if (!ctx->bound_texture) {
        return;
    }

    if (width <= 0 || height <= 0 || data == NULL) {
        return;
    }

    if (format != SGFX_TEXTURE_COLOR && format != SGFX_TEXTURE_SDF) {
        return;
    }

    sgfx_texture_internal_t* tex = &ctx->textures[ctx->bound_texture - 1];
    tex->format = format;
    tex->width = width;
    tex->height = height;
    size_t data_size = (format == SGFX_TEXTURE_COLOR) ? (width * height * sizeof(color_t)) : (width * height * sizeof(float32_t));

    if (format == SGFX_TEXTURE_COLOR) {
        tex->color_data = memory_malloc(data_size);
        memory_memcopy(data, tex->color_data, data_size);
    } else if (format == SGFX_TEXTURE_SDF) {
        tex->sdf_data = memory_malloc(data_size);
        memory_memcopy(data, tex->sdf_data, data_size);
    }
}

// Blit glyph with foreground and background colors
void sgfx_blit_glyph_color(sgfx_context_t* ctx, sgfx_texture_t tex,
                           float32_t src_x, float32_t src_y, float32_t src_w, float32_t src_h,
                           float32_t dst_x, float32_t dst_y, float32_t dst_w, float32_t dst_h,
                           color_t fg) {
    if (tex == 0 || tex > ctx->texture_count) {
        return;
    }

    sgfx_texture_internal_t* t = &ctx->textures[tex - 1];

    for (int32_t dy = 0; dy < (int32_t)dst_h; ++dy) {
        int32_t py = (int32_t)dst_y + dy;
        float32_t v = src_y + (float32_t)dy / dst_h * src_h;

        for (int32_t dx = 0; dx < (int32_t)dst_w; ++dx) {
            int32_t px = (int32_t)dst_x + dx;
            float32_t u = src_x + (float32_t)dx / dst_w * src_w;

            color_t mask = sgfx_sample_texture(t, u / t->width, v / t->height);

            color_t src_color = fg;

            // If you want alpha blending with framebuffer:
            color_t dst_color = ctx->buffers[SGFX_BUFFER_FRONT][py * ctx->current_context->width + px];
            color_t out = sgfx_modulate_color(src_color, (sgfx_vec4_f32_t){1.0f, 1.0f, 1.0f, (float32_t)mask.alpha / 255.0f});
            out = sgfx_alpha_blend(out, dst_color);

            sgfx_plot_pixel(ctx, px, py, out);
        }
    }
}
