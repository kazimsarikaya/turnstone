/**
 * @file virgl.h
 * @brief Virgl header file.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___VIRGL_H
#define ___VIRGL_H 0

#include <types.h>


typedef enum virgl_object_type_t {
    VIRGL_OBJECT_NULL,
    VIRGL_OBJECT_BLEND,
    VIRGL_OBJECT_RASTERIZER,
    VIRGL_OBJECT_DSA,
    VIRGL_OBJECT_SHADER,
    VIRGL_OBJECT_VERTEX_ELEMENTS,
    VIRGL_OBJECT_SAMPLER_VIEW,
    VIRGL_OBJECT_SAMPLER_STATE,
    VIRGL_OBJECT_SURFACE,
    VIRGL_OBJECT_QUERY,
    VIRGL_OBJECT_STREAMOUT_TARGET,
    VIRGL_OBJECT_MSAA_SURFACE,
    VIRGL_MAX_OBJECTS,
} virgl_object_type_t;

/* context cmds to be encoded in the command stream */
typedef enum virgl_context_cmd_t {
    VIRGL_CCMD_NOP = 0,
    VIRGL_CCMD_CREATE_OBJECT = 1,
    VIRGL_CCMD_BIND_OBJECT,
    VIRGL_CCMD_DESTROY_OBJECT,
    VIRGL_CCMD_SET_VIEWPORT_STATE,
    VIRGL_CCMD_SET_FRAMEBUFFER_STATE,
    VIRGL_CCMD_SET_VERTEX_BUFFERS,
    VIRGL_CCMD_CLEAR,
    VIRGL_CCMD_DRAW_VBO,
    VIRGL_CCMD_RESOURCE_INLINE_WRITE,
    VIRGL_CCMD_SET_SAMPLER_VIEWS,
    VIRGL_CCMD_SET_INDEX_BUFFER,
    VIRGL_CCMD_SET_CONSTANT_BUFFER,
    VIRGL_CCMD_SET_STENCIL_REF,
    VIRGL_CCMD_SET_BLEND_COLOR,
    VIRGL_CCMD_SET_SCISSOR_STATE,
    VIRGL_CCMD_BLIT,
    VIRGL_CCMD_RESOURCE_COPY_REGION,
    VIRGL_CCMD_BIND_SAMPLER_STATES,
    VIRGL_CCMD_BEGIN_QUERY,
    VIRGL_CCMD_END_QUERY,
    VIRGL_CCMD_GET_QUERY_RESULT,
    VIRGL_CCMD_SET_POLYGON_STIPPLE,
    VIRGL_CCMD_SET_CLIP_STATE,
    VIRGL_CCMD_SET_SAMPLE_MASK,
    VIRGL_CCMD_SET_STREAMOUT_TARGETS,
    VIRGL_CCMD_SET_RENDER_CONDITION,
    VIRGL_CCMD_SET_UNIFORM_BUFFER,

    VIRGL_CCMD_SET_SUB_CTX,
    VIRGL_CCMD_CREATE_SUB_CTX,
    VIRGL_CCMD_DESTROY_SUB_CTX,
    VIRGL_CCMD_BIND_SHADER,
    VIRGL_CCMD_SET_TESS_STATE,
    VIRGL_CCMD_SET_MIN_SAMPLES,
    VIRGL_CCMD_SET_SHADER_BUFFERS,
    VIRGL_CCMD_SET_SHADER_IMAGES,
    VIRGL_CCMD_MEMORY_BARRIER,
    VIRGL_CCMD_LAUNCH_GRID,
    VIRGL_CCMD_SET_FRAMEBUFFER_STATE_NO_ATTACH,
    VIRGL_CCMD_TEXTURE_BARRIER,
    VIRGL_CCMD_SET_ATOMIC_BUFFERS,
    VIRGL_CCMD_SET_DEBUG_FLAGS,
    VIRGL_CCMD_GET_QUERY_RESULT_QBO,
    VIRGL_CCMD_TRANSFER3D,
    VIRGL_CCMD_END_TRANSFERS,
    VIRGL_CCMD_COPY_TRANSFER3D,
    VIRGL_CCMD_SET_TWEAKS,
    VIRGL_CCMD_CLEAR_TEXTURE,
    VIRGL_CCMD_PIPE_RESOURCE_CREATE,
    VIRGL_CCMD_PIPE_RESOURCE_SET_TYPE,
    VIRGL_CCMD_GET_MEMORY_INFO,
    VIRGL_CCMD_SEND_STRING_MARKER,
    VIRGL_CCMD_LINK_SHADER,

    /* video codec */
    VIRGL_CCMD_CREATE_VIDEO_CODEC,
    VIRGL_CCMD_DESTROY_VIDEO_CODEC,
    VIRGL_CCMD_CREATE_VIDEO_BUFFER,
    VIRGL_CCMD_DESTROY_VIDEO_BUFFER,
    VIRGL_CCMD_BEGIN_FRAME,
    VIRGL_CCMD_DECODE_MACROBLOCK,
    VIRGL_CCMD_DECODE_BITSTREAM,
    VIRGL_CCMD_ENCODE_BITSTREAM,
    VIRGL_CCMD_END_FRAME,

    VIRGL_CCMD_CLEAR_SURFACE,

    VIRGL_MAX_COMMANDS
} virgl_context_cmd_t;

typedef enum virgl_texture_target_t {
    VIRGL_TEXTURE_TARGET_BUFFER = 0,
    VIRGL_TEXTURE_TARGET_1D,
    VIRGL_TEXTURE_TARGET_2D,
    VIRGL_TEXTURE_TARGET_3D,
    VIRGL_TEXTURE_TARGET_CUBE,
    VIRGL_TEXTURE_TARGET_RECT,
    VIRGL_TEXTURE_TARGET_1D_ARRAY,
    VIRGL_TEXTURE_TARGET_2D_ARRAY,
    VIRGL_TEXTURE_TARGET_CUBE_ARRAY,
    VIRGL_TEXTURE_TARGET_2D_MSAA,
    VIRGL_TEXTURE_TARGET_2D_MSAA_ARRAY,
    VIRGL_TEXTURE_TARGET_3D_MSAA,
    VIRGL_TEXTURE_TARGET_MAX,
} virgl_texture_target_t;

typedef enum virgl_bind_t {
    VIRGL_BIND_DEPTH_STENCIL       = (1 << 0), /* create_surface */
    VIRGL_BIND_RENDER_TARGET       = (1 << 1), /* create_surface */
    VIRGL_BIND_BLENDABLE           = (1 << 2), /* create_surface */
    VIRGL_BIND_SAMPLER_VIEW        = (1 << 3), /* create_sampler_view */
    VIRGL_BIND_VERTEX_BUFFER       = (1 << 4), /* set_vertex_buffers */
    VIRGL_BIND_INDEX_BUFFER        = (1 << 5), /* draw_elements */
    VIRGL_BIND_CONSTANT_BUFFER     = (1 << 6), /* set_constant_buffer */
    VIRGL_BIND_DISPLAY_TARGET      = (1 << 8), /* flush_front_buffer */
    VIRGL_BIND_TRANSFER_WRITE      = (1 << 9), /* transfer_map */
    VIRGL_BIND_TRANSFER_READ       = (1 << 10), /* transfer_map */
    VIRGL_BIND_STREAM_OUTPUT       = (1 << 11), /* set_stream_output_buffers */
    VIRGL_BIND_SCANOUT             = (1 << 14), /* ??? */
    VIRGL_BIND_SHARED              = (1 << 15), /* get_texture_handle ??? */
    VIRGL_BIND_CURSOR              = (1 << 16), /* mouse cursor */
    VIRGL_BIND_CUSTOM              = (1 << 17), /* state-tracker/winsys usages */
    VIRGL_BIND_GLOBAL              = (1 << 18), /* set_global_binding */
    VIRGL_BIND_SHADER_RESOURCE     = (1 << 19), /* set_shader_resources */
    VIRGL_BIND_COMPUTE_RESOURCE    = (1 << 20), /* set_compute_resources */
    VIRGL_BIND_COMMAND_ARGS_BUFFER = (1 << 21), /* pipe_draw_info.indirect */
    VIRGL_BIND_LINEAR              = (1 << 21), /* ??? */
    VIRGL_BIND_QUERY_BUFFER        = (1 << 22), /* get_query_result_resource */
} virgl_bind_t;

#define VIRGL_CMD(cmd, obj, len) ((cmd) | ((obj) << 8) | ((len) << 16))
#define VIRGL_CMD_MAX_DWORDS ((4096 - 4) / 4)

typedef struct virgl_cmd_t {
    uint32_t context_id;
    uint32_t cmd_dw_count;
    uint32_t cmd_dws[VIRGL_CMD_MAX_DWORDS];
} virgl_cmd_t;

typedef union virgl_color_t {
    int32_t   i32[4];
    uint32_t  ui32[4];
    float32_t f32[4];
} virgl_color_t;

_Static_assert(sizeof(virgl_color_t) == 16, "virgl_color_t size is not 16 bytes");

typedef enum virgl_clear_flags_t {
    VIRGL_CLEAR_FLAG_CLEAR_DEPTH      =  (1 << 0),
    VIRGL_CLEAR_FLAG_CLEAR_STENCIL    =  (1 << 1),
    VIRGL_CLEAR_FLAG_CLEAR_COLOR0     =  (1 << 2),
    VIRGL_CLEAR_FLAG_CLEAR_COLOR1     =  (1 << 3),
    VIRGL_CLEAR_FLAG_CLEAR_COLOR2     =  (1 << 4),
    VIRGL_CLEAR_FLAG_CLEAR_COLOR3     =  (1 << 5),
    VIRGL_CLEAR_FLAG_CLEAR_COLOR4     =  (1 << 6),
    VIRGL_CLEAR_FLAG_CLEAR_COLOR5     =  (1 << 7),
    VIRGL_CLEAR_FLAG_CLEAR_COLOR6     =  (1 << 8),
    VIRGL_CLEAR_FLAG_CLEAR_COLOR7     =  (1 << 9),
    VIRGL_CLEAR_FLAG_CLEAR_ALL_COLORS =  (VIRGL_CLEAR_FLAG_CLEAR_COLOR0 |
                                          VIRGL_CLEAR_FLAG_CLEAR_COLOR1 |
                                          VIRGL_CLEAR_FLAG_CLEAR_COLOR2 |
                                          VIRGL_CLEAR_FLAG_CLEAR_COLOR3 |
                                          VIRGL_CLEAR_FLAG_CLEAR_COLOR4 |
                                          VIRGL_CLEAR_FLAG_CLEAR_COLOR5 |
                                          VIRGL_CLEAR_FLAG_CLEAR_COLOR6 |
                                          VIRGL_CLEAR_FLAG_CLEAR_COLOR7),
    VIRGL_CLEAR_FLAG_CLEAR_DEPTH_STENCIL = (VIRGL_CLEAR_FLAG_CLEAR_DEPTH | VIRGL_CLEAR_FLAG_CLEAR_STENCIL),
} virgl_clear_flags_t;

typedef struct virgl_cmd_clear_t {
    uint32_t      clear_flags;
    virgl_color_t clear_color;
    float64_t     clear_depth;
    uint32_t      clear_stencil;
}__attribute__((packed)) virgl_cmd_clear_t;

_Static_assert(sizeof(virgl_cmd_clear_t) == 32, "virgl_cmd_clear_t size is not 24 bytes");

#define VIRGL_CMD_CLEAR_CCMD_PAYLOAD_SIZE (sizeof(virgl_cmd_clear_t) / sizeof(uint32_t))

#define VIRGL_OBJ_SURFACE_TEXTURE_LAYERS(fl, ll) (((ll) << 16) | (fl))

typedef struct virgl_obj_surface_t {
    uint32_t surface_id;
    uint32_t resource_id;
    uint32_t format;
    union {
        struct {
            uint32_t first_element;
            uint32_t last_element;
        } buffer;
        struct {
            uint32_t level;
            uint32_t layers;
        } texture;
    };
} virgl_obj_surface_t;

_Static_assert(sizeof(virgl_obj_surface_t) == 5 * sizeof(uint32_t), "virgl_obj_surface_t size is not 20 bytes");

#define VIRGL_OBJ_SURFACE_CCMD_PAYLOAD_SIZE (sizeof(virgl_obj_surface_t) / sizeof(uint32_t))

#define VIRGL_OBJ_FRAMEBUFFER_MAX_CBUFS 8

typedef struct virgl_obj_framebuffer_state_t {
    uint32_t nr_cbufs;
    uint32_t zsurf_id;
    uint32_t cbuf_ids[VIRGL_OBJ_FRAMEBUFFER_MAX_CBUFS];
} virgl_obj_framebuffer_state_t;

#define VIRGL_OBJ_FRAMEBUFFER_STATE_CCMD_PAYLOAD_SIZE(nr_cbufs) (2 + (nr_cbufs))

typedef struct virgl_copy_region_t {
    uint32_t src_resource_id;
    uint32_t src_level;
    uint32_t src_x;
    uint32_t src_y;
    uint32_t src_z;
    uint32_t dst_resource_id;
    uint32_t dst_level;
    uint32_t dst_x;
    uint32_t dst_y;
    uint32_t dst_z;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
} virgl_copy_region_t;

_Static_assert(sizeof(virgl_copy_region_t) == 13 * sizeof(uint32_t), "virgl_copy_region_t size is not 52 bytes");

#define VIRGL_COPY_REGION_CCMD_PAYLOAD_SIZE (sizeof(virgl_copy_region_t) / sizeof(uint32_t))

int8_t virgl_encode_clear(virgl_cmd_t* cmd, virgl_cmd_clear_t * clear);
int8_t virgl_encode_surface(virgl_cmd_t* cmd, virgl_obj_surface_t * surface, boolean_t is_texture);
int8_t virgl_encode_framebuffer_state(virgl_cmd_t* cmd, virgl_obj_framebuffer_state_t * fb_state);
int8_t virgl_encode_copy_region(virgl_cmd_t* cmd, virgl_copy_region_t * copy_region);

#endif // ___VIRGL_H
