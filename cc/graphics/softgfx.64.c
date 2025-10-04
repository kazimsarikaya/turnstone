/**
 * @file sgfx.64.c
 * @brief Software Graphics Library Implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <graphics/softgfx.h>
#include <memory.h>

MODULE("turnstone.kernel.graphics.sgfx");

// Internal texture struct
typedef struct sgfx_texture_internal_t {
    int32_t  width, height;
    color_t* data;
} sgfx_texture_internal_t;

typedef enum sgfx_buffer_type_t {
    SGFX_BUFFER_FRONT = 0,
    SGFX_BUFFER_BACK,
    SGFX_BUFFER_FRAME,
    SGFX_BUFFER_COUNT
} sgfx_buffer_type_t;

// Context definition
struct sgfx_context_t {
    int32_t  width, height;
    color_t* buffers[SGFX_BUFFER_COUNT];

    sgfx_mat4_f32_t modelview;
    sgfx_mat4_f32_t projection;
    sgfx_mat4_f32_t modelview_backup;
    sgfx_mat4_f32_t projection_backup;

    boolean_t sub_context_enabled;

    struct {
        int32_t         x, y, w, h;
        sgfx_mat4_f32_t modelview;
        sgfx_mat4_f32_t projection;
    } sub_context;

    sgfx_mat4_f32_t* current_matrix;
    int32_t          matrix_mode;

    sgfx_vec4_f32_t current_color;
    sgfx_vec2_f32_t current_texcoord;

    sgfx_texture_t bound_texture;

    sgfx_cap_t enabled_caps;

    struct {
        int32_t x, y, w, h;
    } scissor;

    // Immediate mode buffers
    sgfx_vec4_f32_t vertices[SGFX_MAX_VERTICES];
    sgfx_vec2_f32_t texcoords[SGFX_MAX_VERTICES];
    sgfx_vec4_f32_t colors[SGFX_MAX_VERTICES];
    int32_t         vertex_count;
    int32_t         draw_mode;

    // Textures (simple array, max 16)
    sgfx_texture_internal_t textures[16];
    uint32_t                texture_count;
};

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

static void sgfx_mat4_translate(sgfx_mat4_f32_t* mat, float32_t x, float32_t y, float32_t z) {
    sgfx_mat4_f32_t trans;
    sgfx_mat4_identity(&trans);
    trans.m[12] = x;
    trans.m[13] = y;
    trans.m[14] = z;
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
    float32_t c = math_cos_f32(angle * math_pi_f32() / 180.0f); // Degrees? OpenGL uses degrees
    float32_t s = math_sin_f32(angle * math_pi_f32() / 180.0f);
    float32_t len = math_sqrt_f32(x * x + y * y + z * z);

    if (len != 0.0f) {
        x /= len; y /= len; z /= len;
    }

    sgfx_mat4_f32_t rot;
    sgfx_mat4_identity(&rot);

    rot.m[0] = x * x * (1 - c) + c;   rot.m[1] = x * y * (1 - c) - z * s; rot.m[2] = x * z * (1 - c) + y * s;
    rot.m[4] = y * x * (1 - c) + z * s; rot.m[5] = y * y * (1 - c) + c;   rot.m[6] = y * z * (1 - c) - x * s;
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
    if (x < 0 || x >= ctx->width || y < 0 || y >= ctx->height) {
        return;
    }

    if (sgfx_is_enabled(ctx, SGFX_CAP_SCISSOR_TEST)) {
        if (x < ctx->scissor.x || x >= ctx->scissor.x + ctx->scissor.w ||
            y < ctx->scissor.y || y >= ctx->scissor.y + ctx->scissor.h) {
            return;
        }
    }

    int32_t idx = y * ctx->width + x;
    color_t dst = ctx->buffers[SGFX_BUFFER_FRONT][idx];
    ctx->buffers[SGFX_BUFFER_FRONT][idx] = sgfx_alpha_blend(pix, dst);
}

// Texture sample nearest
static color_t sgfx_sample_texture(const sgfx_texture_internal_t* tex, float32_t u, float32_t v) {
    int32_t tx = (int32_t)(u * (float32_t)tex->width) % tex->width;
    int32_t ty = (int32_t)(v * (float32_t)tex->height) % tex->height;
    if (tx < 0) tx += tex->width;
    if (ty < 0) ty += tex->height;
    return tex->data[ty * tex->width + tx];
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

    ctx->width = width;
    ctx->height = height;
    ctx->buffers[SGFX_BUFFER_FRAME] = framebuffer;
    sgfx_mat4_identity(&ctx->modelview);
    sgfx_mat4_identity(&ctx->projection);
    ctx->current_matrix = &ctx->modelview;
    ctx->matrix_mode = SGFX_MODELVIEW;
    ctx->current_color = (sgfx_vec4_f32_t){1.0f, 1.0f, 1.0f, 1.0f};
    ctx->texture_count = 0;
    ctx->bound_texture = 0;
    ctx->enabled_caps = 0;
    return ctx;
}

void sgfx_destroy_context(sgfx_context_t* ctx) {
    for (uint32_t i = 0; i < ctx->texture_count; ++i) {
        memory_free(ctx->textures[i].data);
    }

    if (ctx->buffers[0]) memory_free(ctx->buffers[0]);
    if (ctx->buffers[1]) memory_free(ctx->buffers[1]);

    memory_free(ctx);
}

void sgfx_clear(sgfx_context_t* ctx, float32_t r, float32_t g, float32_t b, float32_t a) {
    color_t color = {
        .red = (uint32_t)(r * 255.0f),
        .green = (uint32_t)(g * 255.0f),
        .blue = (uint32_t)(b * 255.0f),
        .alpha = (uint32_t)(a * 255.0f)
    };

    color_t* buf = ctx->buffers[SGFX_BUFFER_FRONT];

    if (ctx->sub_context_enabled) {
        for (int32_t y = ctx->sub_context.y; y < ctx->sub_context.y + ctx->sub_context.h; ++y) {
            for (int32_t x = ctx->sub_context.x; x < ctx->sub_context.x + ctx->sub_context.w; ++x) {
                if (x < 0 || x >= ctx->width || y < 0 || y >= ctx->height) {
                    continue;
                }

                buf[y * ctx->width + x] = color;
            }
        }

        return;
    }

    for (int i = 0; i < ctx->width * ctx->height; ++i) {
        buf[i] = color;
    }
}

static inline void sgfx_blit_changed_pixels(
    const color_t * front,
    const color_t * back,
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

void sgfx_swap_buffers(sgfx_context_t* ctx) {
    int32_t size = ctx->width * ctx->height;

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
    ctx->current_matrix = (mode == SGFX_PROJECTION) ? &ctx->projection : &ctx->modelview;
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
}

void sgfx_perspective_f32(sgfx_context_t* ctx, float32_t fovy, float32_t aspect, float32_t near, float32_t far) {
    sgfx_mat4_perspective(ctx->current_matrix, fovy, aspect, near, far);
}

// Enable/scissor
void sgfx_enable(sgfx_context_t* ctx, sgfx_cap_t cap) {
    ctx->enabled_caps |= cap;
}

void sgfx_disable(sgfx_context_t* ctx, sgfx_cap_t cap) {
    ctx->enabled_caps &= ~cap;
}

boolean_t sgfx_is_enabled(sgfx_context_t* ctx, sgfx_cap_t cap) {
    return (ctx->enabled_caps & cap) != 0;
}

void sgfx_scissor(sgfx_context_t* ctx, int32_t x, int32_t y, int32_t w, int32_t h) {
    ctx->scissor.x = x;
    ctx->scissor.y = y;
    ctx->scissor.w = w;
    ctx->scissor.h = h;
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

static void sgfx_perspective_divide(sgfx_context_t* ctx, sgfx_vec4_f32_t* clip) {
    for(int32_t i = 0; i < ctx->vertex_count; ++i) {
        if(clip[i].w > math_epsilon_f32()) {
            clip[i].x /= clip[i].w;
            clip[i].y /= clip[i].w;
            clip[i].z /= clip[i].w;
        } else {
            clip[i].x = clip[i].y = clip[i].z = 0.0f;
        }
    }
}

static void sgfx_viewport(sgfx_context_t* ctx, sgfx_vec4_f32_t* clip) {
    if(ctx->sub_context_enabled) {
        for(int32_t i = 0; i < ctx->vertex_count; ++i) {
            clip[i].x = (clip[i].x * 0.5f + 0.5f) * (float32_t)ctx->sub_context.w + (float32_t)ctx->sub_context.x;
            clip[i].y = (1.0f - (clip[i].y * 0.5f + 0.5f)) * (float32_t)ctx->sub_context.h + (float32_t)ctx->sub_context.y;

        }
    } else {
        for(int32_t i = 0; i < ctx->vertex_count; ++i) {
            clip[i].x = (clip[i].x * 0.5f + 0.5f) * (float32_t)ctx->width;
            clip[i].y = (1.0f - (clip[i].y * 0.5f + 0.5f)) * (float32_t)ctx->height;
        }
    }
}

void sgfx_end(sgfx_context_t* ctx) {
    if (ctx->vertex_count == 0) {
        return;
    }

    sgfx_mat4_f32_t mvp;
    sgfx_mat4_mul(&mvp, &ctx->projection, &ctx->modelview);

    sgfx_vec4_f32_t screen_verts[SGFX_MAX_VERTICES];

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

void sgfx_tex_image2d(sgfx_context_t* ctx, int32_t width, int32_t height, const color_t* data) {
    if (!ctx->bound_texture) {
        return;
    }

    sgfx_texture_internal_t* tex = &ctx->textures[ctx->bound_texture - 1];
    tex->width = width;
    tex->height = height;
    tex->data = memory_malloc(width * height * sizeof(color_t));
    memory_memcopy(data, tex->data, width * height * sizeof(color_t));
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
            color_t dst_color = ctx->buffers[SGFX_BUFFER_FRONT][py * ctx->width + px];
            color_t out = sgfx_modulate_color(src_color, (sgfx_vec4_f32_t){1.0f, 1.0f, 1.0f, (float32_t)mask.alpha / 255.0f});
            out = sgfx_alpha_blend(out, dst_color);

            sgfx_plot_pixel(ctx, px, py, out);
        }
    }
}

void sgfx_create_sub_context(sgfx_context_t* ctx,
                             int32_t x, int32_t y, int32_t width, int32_t height) {

    if (ctx->sub_context_enabled) {
        return; // Already enabled
    }

    ctx->modelview_backup = ctx->modelview;
    ctx->projection_backup = ctx->projection;

    ctx->sub_context.x = x;
    ctx->sub_context.y = y;
    ctx->sub_context.w = width;
    ctx->sub_context.h = height;

    ctx->sub_context_enabled = true;
}

void sgfx_destroy_sub_context(sgfx_context_t* ctx) {
    if (!ctx->sub_context_enabled) {
        return; // Not enabled
    }

    ctx->modelview = ctx->modelview_backup;
    ctx->projection = ctx->projection_backup;
    ctx->sub_context_enabled = false;
}
