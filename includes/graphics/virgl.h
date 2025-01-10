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
#include <memory.h>
#include <cpu/sync.h>

#define VIRGL_RSTR(...) #__VA_ARGS__

typedef enum virgl_format_t {
    VIRGL_FORMAT_B8G8R8A8_UNORM = 1,
    VIRGL_FORMAT_B8G8R8X8_UNORM = 2,
    VIRGL_FORMAT_A8R8G8B8_UNORM = 3,
    VIRGL_FORMAT_X8R8G8B8_UNORM = 4,
    VIRGL_FORMAT_R32G32B32A32_FLOAT      = 31,
    VIRGL_FORMAT_R32G32B32A32_UNORM      = 35,
    VIRGL_FORMAT_R8_UNORM                = 64,
    VIRGL_FORMAT_R8G8B8A8_UNORM = 67,
    VIRGL_FORMAT_X8B8G8R8_UNORM = 68,
    VIRGL_FORMAT_A8B8G8R8_UNORM = 121,
    VIRGL_FORMAT_R8G8B8X8_UNORM = 134,
} virgl_format_t;

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
#define VIRGL_CMD_MAX_DWORDS ((64 * 1024 - 24 - 24) / 4)

typedef struct virgl_cmd_t      virgl_cmd_t;
typedef struct virgl_renderer_t virgl_renderer_t;

typedef int8_t (*virgl_send_cmd_f)(uint32_t queue_no, lock_t** lock, uint32_t fence_id, virgl_cmd_t * cmd);

typedef union virgl_color_t {
    int32_t   i32[4];
    uint32_t  ui32[4];
    float32_t f32[4];
} virgl_color_t;

typedef struct virgl_box_t {
    uint32_t x;
    uint32_t y;
    uint32_t z;
    uint32_t w;
    uint32_t h;
    uint32_t d;
} virgl_box_t;

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

typedef struct virgl_cmd_clear_texture_t {
    uint32_t      texture_id;
    uint32_t      level;
    virgl_box_t   box;
    virgl_color_t clear_color;
} virgl_cmd_clear_texture_t;

_Static_assert(sizeof(virgl_cmd_clear_texture_t) == 12 * sizeof(uint32_t), "virgl_cmd_clear_texture_t size is not 48 bytes");

#define VIRGL_CMD_CLEAR_TEXTURE_CCMD_PAYLOAD_SIZE (12)

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
    uint32_t    dst_resource_id;
    uint32_t    dst_level;
    uint32_t    dst_x;
    uint32_t    dst_y;
    uint32_t    dst_z;
    uint32_t    src_resource_id;
    uint32_t    src_level;
    virgl_box_t src_box;
} virgl_copy_region_t;

_Static_assert(sizeof(virgl_copy_region_t) == 13 * sizeof(uint32_t), "virgl_copy_region_t size is not 52 bytes");

#define VIRGL_COPY_REGION_CCMD_PAYLOAD_SIZE (sizeof(virgl_copy_region_t) / sizeof(uint32_t))


typedef enum virgl_shader_type_t {
    VIRGL_SHADER_TYPE_VERTEX,
    VIRGL_SHADER_TYPE_FRAGMENT,
    VIRGL_SHADER_TYPE_GEOMETRY,
    VIRGL_SHADER_TYPE_TESS_CTRL,
    VIRGL_SHADER_TYPE_TESS_EVAL,
    VIRGL_SHADER_TYPE_COMPUTE,
    VIRGL_SHADER_TYPE_TYPES,
    VIRGL_SHADER_TYPE_INVALID,
} virgl_shader_type_t;

#define VIRGL_SHADER_MAX_SO_OUTPUT_BUFFERS 4
#define VIRGL_SHADER_MAX_SO_OUTPUTS 64

typedef struct virgl_stream_output_t {
    uint32_t register_index;
    uint32_t start_component;
    uint32_t num_components;
    uint32_t output_buffer;
    uint32_t dst_offset;
    uint32_t stream;
} virgl_stream_output_t;

#define VIRGL_ENCODE_SO_DECLARATION_RAW(register_index, start_component, num_components, output_buffer, dst_offset, stream) \
        (((register_index) << 0) | ((start_component) << 8) | ((num_components) << 16) | ((output_buffer) << 24) | ((dst_offset) << 28) | ((stream) << 31))
#define VIRGL_ENCODE_SO_DECLARATION(so) \
        VIRGL_ENCODE_SO_DECLARATION_RAW(so.register_index, so.start_component, so.num_components, so.output_buffer, so.dst_offset, so.stream)

typedef struct virgl_shader_t {
    uint32_t              type;
    uint32_t              handle;
    const char_t*         data;
    uint32_t              data_size;
    uint32_t              num_tokens;
    uint32_t              num_outputs;
    uint32_t              stride[VIRGL_SHADER_MAX_SO_OUTPUT_BUFFERS];
    virgl_stream_output_t output[VIRGL_SHADER_MAX_SO_OUTPUTS];
} virgl_shader_t;

#define VIRGL_OBJECT_SHADER_OFFSET_VAL(x) (((x) & 0x7fffffff) << 0)
#define VIRGL_OBJECT_SHADER_OFFSET_CONT (0x1u << 31)

typedef struct virgl_link_shader_t {
    uint32_t vertex_shader_id;
    uint32_t fragment_shader_id;
    uint32_t geometry_shader_id;
    uint32_t tess_ctrl_shader_id;
    uint32_t tess_eval_shader_id;
    uint32_t compute_shader_id;
} virgl_link_shader_t;

#define VIRGL_LINK_SHADER_CCMD_PAYLOAD_SIZE (6)

#define VIRGL_SET_UNIFORM_BUFFER_CCMD_PAYLOAD_SIZE (5)

typedef struct virgl_shader_buffer_element_t {
    uint32_t offset;
    uint32_t length;
    uint32_t res;
} virgl_shader_buffer_element_t;

#define VIRGL_SHADER_BUFFER_MAX_ELEMENTS 16

typedef struct virgl_shader_buffer_t {
    uint32_t                      shader_type;
    uint32_t                      index;
    uint32_t                      num_elements;
    virgl_shader_buffer_element_t elements[VIRGL_SHADER_BUFFER_MAX_ELEMENTS];
} virgl_shader_buffer_t;

#define VIRGL_SET_SHADER_BUFFERS_CCMD_PAYLOAD_SIZE(num_elements) (2 + (num_elements * 3))

typedef enum virgl_access_type_t {
    VIRGL_ACCESS_TYPE_READ = 1,
    VIRGL_ACCESS_TYPE_WRITE = 2,
    VIRGL_ACCESS_TYPE_READ_WRITE = 3,
} virgl_access_type_t;

typedef struct virgl_shader_image_t {
    uint32_t format;
    uint32_t access;
    uint32_t offset;
    uint32_t size;
    uint32_t res;
} virgl_shader_image_t;

#define VIRGL_SHADER_IMAGE_MAX_IMAGES 16

typedef struct virgl_shader_images_t {
    uint32_t             shader_type;
    uint32_t             index;
    uint32_t             num_images;
    virgl_shader_image_t images[VIRGL_SHADER_IMAGE_MAX_IMAGES];
} virgl_shader_images_t;

#define VIRGL_SET_SHADER_IMAGES_CCMD_PAYLOAD_SIZE(num_elements) (2 + (num_elements * 5))

typedef struct virgl_sampler_view_t {
    uint32_t view_id;
    uint32_t texture_id;
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
    uint32_t swizzle_r;
    uint32_t swizzle_g;
    uint32_t swizzle_b;
    uint32_t swizzle_a;
} virgl_sampler_view_t;

#define VIRGL_SAMPLER_VIEW_OBJECT_ENCODE_SWIZZLE(r, g, b, a) (((r) << 0) | ((g) << 3) | ((b) << 6) | ((a) << 9))

#define VIRGL_OBJ_SAMPLER_VIEW_CCMD_PAYLOAD_SIZE 6


#define VIRGL_SAMPLER_VIEWS_MAX_VIEWS 16

typedef struct virgl_sampler_views_t {
    uint32_t shader_type;
    uint32_t index;
    uint32_t num_views;
    uint32_t res_ids[VIRGL_SAMPLER_VIEWS_MAX_VIEWS];
} virgl_sampler_views_t;

#define VIRGL_SET_SAMPLER_VIEWS_CCMD_PAYLOAD_SIZE(num_views) (2 + (num_views))

typedef enum virgl_prim_type_t {
    VIRGL_PRIM_POINTS,
    VIRGL_PRIM_LINES,
    VIRGL_PRIM_LINE_LOOP,
    VIRGL_PRIM_LINE_STRIP,
    VIRGL_PRIM_TRIANGLES,
    VIRGL_PRIM_TRIANGLE_STRIP,
    VIRGL_PRIM_TRIANGLE_FAN,
    VIRGL_PRIM_QUADS,
    VIRGL_PRIM_QUAD_STRIP,
    VIRGL_PRIM_POLYGON,
    VIRGL_PRIM_LINES_ADJACENCY,
    VIRGL_PRIM_LINE_STRIP_ADJACENCY,
    VIRGL_PRIM_TRIANGLES_ADJACENCY,
    VIRGL_PRIM_TRIANGLE_STRIP_ADJACENCY,
    VIRGL_PRIM_PATCHES,
} virgl_prim_type_t;

typedef struct virgl_draw_info_t {
    uint32_t start;
    uint32_t count;
    uint32_t mode;
    uint32_t indexed;
    uint32_t instance_count;
    uint32_t index_bias;
    uint32_t start_instance;
    uint32_t primitive_restart;
    uint32_t restart_index;
    uint32_t min_index;
    uint32_t max_index;

    boolean_t has_indirect_handle;
    uint32_t  indirect_handle;
    uint32_t  indirect_offset;
    uint32_t  indirect_stride;
    uint32_t  indirect_draw_count;
    uint32_t  indirect_draw_count_offset;
    uint32_t  indirect_draw_count_handle;

    boolean_t has_count_from_stream_output;
    uint32_t  stream_output_buffer_size;
} virgl_draw_info_t;

#define VIRGL_DRAW_CCMD_PAYLOAD_SIZE (12)

typedef enum virgl_resource_usage_t {
    VIRGL_RESOURCE_USAGE_DEFAULT, /* fast GPU access */
    VIRGL_RESOURCE_USAGE_IMMUTABLE, /* fast GPU access, immutable */
    VIRGL_RESOURCE_USAGE_DYNAMIC, /* uploaded data is used multiple times */
    VIRGL_RESOURCE_USAGE_STREAM, /* uploaded data is used once */
    VIRGL_RESOURCE_USAGE_STAGING, /* fast CPU access */
} virgl_resource_usage_t;

typedef struct virgl_res_iw_t {
    uint32_t    res_id;
    uint32_t    format;
    uint32_t    element_size;
    uint32_t    level;
    uint32_t    usage;
    uint32_t    stride;
    uint32_t    layer_stride;
    virgl_box_t box;
    uint32_t    data_start;
} virgl_res_iw_t;

typedef struct virgl_vertex_element_t {
    uint32_t src_offset;
    uint32_t instance_divisor;
    uint32_t buffer_index;
    uint32_t src_format;
} virgl_vertex_element_t;

#define VIRGL_CREATE_VERTEX_ELEMENTS_CCMD_PAYLOAD_SIZE(num_elements) (1 + (num_elements * 4))

typedef struct virgl_vertex_t {
    float32_t     x;
    float32_t     y;
    float32_t     z;
    float32_t     d;
    virgl_color_t color;
} virgl_vertex_t;

_Static_assert(sizeof(virgl_vertex_t) == 8 * sizeof(float32_t), "virgl_vertex_t size is not 32 bytes");

typedef enum virgl_logicop_t {
    VIRGL_LOGICOP_CLEAR,
    VIRGL_LOGICOP_NOR,
    VIRGL_LOGICOP_AND_INVERTED,
    VIRGL_LOGICOP_COPY_INVERTED,
    VIRGL_LOGICOP_AND_REVERSE,
    VIRGL_LOGICOP_INVERT,
    VIRGL_LOGICOP_XOR,
    VIRGL_LOGICOP_NAND,
    VIRGL_LOGICOP_AND,
    VIRGL_LOGICOP_EQUIV,
    VIRGL_LOGICOP_NOOP,
    VIRGL_LOGICOP_OR_INVERTED,
    VIRGL_LOGICOP_COPY,
    VIRGL_LOGICOP_OR_REVERSE,
    VIRGL_LOGICOP_OR,
    VIRGL_LOGICOP_SET,
} virgl_logicop_t;

typedef enum virgl_mask_t {
    VIRGL_MASK_R =  0x1,
    VIRGL_MASK_G =  0x2,
    VIRGL_MASK_B =  0x4,
    VIRGL_MASK_A =  0x8,
    VIRGL_MASK_RGBA = 0xf,
    VIRGL_MASK_Z = 0x10,
    VIRGL_MASK_S = 0x20,
    VIRGL_MASK_ZS = 0x30,
    VIRGL_MASK_RGBAZS = (VIRGL_MASK_RGBA | VIRGL_MASK_ZS),
} virgl_mask_t;

typedef enum virgl_compare_func_t {
    VIRGL_FUNC_NEVER,
    VIRGL_FUNC_LESS,
    VIRGL_FUNC_EQUAL,
    VIRGL_FUNC_LEQUAL,
    VIRGL_FUNC_GREATER,
    VIRGL_FUNC_NOTEQUAL,
    VIRGL_FUNC_GEQUAL,
    VIRGL_FUNC_ALWAYS,
} virgl_compare_func_t;

typedef enum virgl_stencil_op_t {
    VIRGL_STENCIL_OP_KEEP,
    VIRGL_STENCIL_OP_ZERO,
    VIRGL_STENCIL_OP_REPLACE,
    VIRGL_STENCIL_OP_INCR,
    VIRGL_STENCIL_OP_DECR,
    VIRGL_STENCIL_OP_INCR_WRAP,
    VIRGL_STENCIL_OP_DECR_WRAP,
    VIRGL_STENCIL_OP_INVERT,
} virgl_stencil_op_t;

typedef struct virgl_depth_state_t {
    uint32_t enabled  :1; /**< depth test enabled? */
    uint32_t writemask:1; /**< allow depth buffer writes? */
    uint32_t func     :3; /**< depth test func (VIRGL_FUNC_x) */
} virgl_depth_state_t;


typedef struct virgl_stencil_state_t {
    uint32_t enabled  :1; /**< stencil[0]: stencil enabled, stencil[1]: two-side enabled */
    uint32_t func     :3; /**< VIRGL_FUNC_x */
    uint32_t fail_op  :3; /**< VIRGL_STENCIL_OP_x */
    uint32_t zpass_op :3; /**< VIRGL_STENCIL_OP_x */
    uint32_t zfail_op :3; /**< VIRGL_STENCIL_OP_x */
    uint32_t valuemask:8;
    uint32_t writemask:8;
} virgl_stencil_state_t;

typedef struct virgl_alpha_state_t {
    uint32_t enabled:1;
    uint32_t func   :3; /**< VIRGL_FUNC_x */
    float    ref_value; /**< reference value */
} virgl_alpha_state_t;

typedef struct virgl_depth_stencil_alpha_state_t {
    virgl_depth_state_t   depth;
    virgl_stencil_state_t stencil[2]; /**< [0] = front, [1] = back */
    virgl_alpha_state_t   alpha;
} virgl_depth_stencil_alpha_state_t;

#define VIRGL_OBJ_DSA_SIZE 5
#define VIRGL_OBJ_DSA_S0_DEPTH_ENABLE(x) (((x) & 0x1) << 0)
#define VIRGL_OBJ_DSA_S0_DEPTH_WRITEMASK(x) (((x) & 0x1) << 1)
#define VIRGL_OBJ_DSA_S0_DEPTH_FUNC(x) (((x) & 0x7) << 2)
#define VIRGL_OBJ_DSA_S0_ALPHA_ENABLED(x) (((x) & 0x1) << 8)
#define VIRGL_OBJ_DSA_S0_ALPHA_FUNC(x) (((x) & 0x7) << 9)
#define VIRGL_OBJ_DSA_S1_STENCIL_ENABLED(x) (((x) & 0x1) << 0)
#define VIRGL_OBJ_DSA_S1_STENCIL_FUNC(x) (((x) & 0x7) << 1)
#define VIRGL_OBJ_DSA_S1_STENCIL_FAIL_OP(x) (((x) & 0x7) << 4)
#define VIRGL_OBJ_DSA_S1_STENCIL_ZPASS_OP(x) (((x) & 0x7) << 7)
#define VIRGL_OBJ_DSA_S1_STENCIL_ZFAIL_OP(x) (((x) & 0x7) << 10)
#define VIRGL_OBJ_DSA_S1_STENCIL_VALUEMASK(x) (((x) & 0xff) << 13)
#define VIRGL_OBJ_DSA_S1_STENCIL_WRITEMASK(x) (((x) & 0xff) << 21)

typedef enum virgl_blend_factor_t {
    VIRGL_BLEND_FACTOR_ONE = 1,
    VIRGL_BLEND_FACTOR_SRC_COLOR,
    VIRGL_BLEND_FACTOR_SRC_ALPHA,
    VIRGL_BLEND_FACTOR_DST_ALPHA,
    VIRGL_BLEND_FACTOR_DST_COLOR,
    VIRGL_BLEND_FACTOR_SRC_ALPHA_SATURATE,
    VIRGL_BLEND_FACTOR_CONST_COLOR,
    VIRGL_BLEND_FACTOR_CONST_ALPHA,
    VIRGL_BLEND_FACTOR_SRC1_COLOR,
    VIRGL_BLEND_FACTOR_SRC1_ALPHA,

    VIRGL_BLEND_FACTOR_ZERO = 0x11,
    VIRGL_BLEND_FACTOR_INV_SRC_COLOR,
    VIRGL_BLEND_FACTOR_INV_SRC_ALPHA,
    VIRGL_BLEND_FACTOR_INV_DST_ALPHA,
    VIRGL_BLEND_FACTOR_INV_DST_COLOR,

    VIRGL_BLEND_FACTOR_INV_CONST_COLOR = 0x17,
    VIRGL_BLEND_FACTOR_INV_CONST_ALPHA,
    VIRGL_BLEND_FACTOR_INV_SRC1_COLOR,
    VIRGL_BLEND_FACTOR_INV_SRC1_ALPHA,
} virgl_blend_factor_t;

typedef enum virgl_blend_func_t {
    VIRGL_BLEND_ADD,
    VIRGL_BLEND_SUBTRACT,
    VIRGL_BLEND_REVERSE_SUBTRACT,
    VIRGL_BLEND_MIN,
    VIRGL_BLEND_MAX,
} virgl_blend_func_t;

typedef struct virgl_rt_blend_state_t {
    uint32_t blend_enable:1;

    uint32_t rgb_func      :3; /**< VIRGL_BLEND_x */
    uint32_t rgb_src_factor:5; /**< VIRGL_BLENDFACTOR_x */
    uint32_t rgb_dst_factor:5; /**< VIRGL_BLENDFACTOR_x */

    uint32_t alpha_func      :3; /**< VIRGL_BLEND_x */
    uint32_t alpha_src_factor:5; /**< VIRGL_BLENDFACTOR_x */
    uint32_t alpha_dst_factor:5; /**< VIRGL_BLENDFACTOR_x */

    uint32_t colormask:4; /**< bitmask of VIRGL_MASK_R/G/B/A */
} virgl_rt_blend_state_t;

#define VIRGL_BLEND_STATE_MAX_RT 8

typedef struct virgl_blend_state_t {
    uint32_t               independent_blend_enable:1;
    uint32_t               logicop_enable          :1;
    uint32_t               logicop_func            :4; /**< VIRGL_LOGICOP_x */
    uint32_t               dither                  :1;
    uint32_t               alpha_to_coverage       :1;
    uint32_t               alpha_to_one            :1;
    virgl_rt_blend_state_t rt[VIRGL_BLEND_STATE_MAX_RT];
} virgl_blend_state_t;

#define VIRGL_OBJ_BLEND_SIZE (VIRGL_BLEND_STATE_MAX_RT + 3)
#define VIRGL_OBJ_BLEND_S0_INDEPENDENT_BLEND_ENABLE(x) ((x) & 0x1 << 0)
#define VIRGL_OBJ_BLEND_S0_LOGICOP_ENABLE(x) (((x) & 0x1) << 1)
#define VIRGL_OBJ_BLEND_S0_DITHER(x) (((x) & 0x1) << 2)
#define VIRGL_OBJ_BLEND_S0_ALPHA_TO_COVERAGE(x) (((x) & 0x1) << 3)
#define VIRGL_OBJ_BLEND_S0_ALPHA_TO_ONE(x) (((x) & 0x1) << 4)
#define VIRGL_OBJ_BLEND_S1_LOGICOP_FUNC(x) (((x) & 0xf) << 0)
#define VIRGL_OBJ_BLEND_S2(cbuf) (4 + (cbuf))
#define VIRGL_OBJ_BLEND_S2_RT_BLEND_ENABLE(x) (((x) & 0x1) << 0)
#define VIRGL_OBJ_BLEND_S2_RT_RGB_FUNC(x) (((x) & 0x7) << 1)
#define VIRGL_OBJ_BLEND_S2_RT_RGB_SRC_FACTOR(x) (((x) & 0x1f) << 4)
#define VIRGL_OBJ_BLEND_S2_RT_RGB_DST_FACTOR(x) (((x) & 0x1f) << 9)
#define VIRGL_OBJ_BLEND_S2_RT_ALPHA_FUNC(x) (((x) & 0x7) << 14)
#define VIRGL_OBJ_BLEND_S2_RT_ALPHA_SRC_FACTOR(x) (((x) & 0x1f) << 17)
#define VIRGL_OBJ_BLEND_S2_RT_ALPHA_DST_FACTOR(x) (((x) & 0x1f) << 22)
#define VIRGL_OBJ_BLEND_S2_RT_COLORMASK(x) (((x) & 0xf) << 27)

typedef enum virgl_polygon_mode_t {
    VIRGL_POLYGON_MODE_FILL,
    VIRGL_POLYGON_MODE_LINE,
    VIRGL_POLYGON_MODE_POINT,
} virgl_polygon_mode_t;

typedef enum virgl_face_t {
    VIRGL_FACE_NONE           = 0,
    VIRGL_FACE_FRONT          = 1,
    VIRGL_FACE_BACK           = 2,
    VIRGL_FACE_FRONT_AND_BACK = (VIRGL_FACE_FRONT | VIRGL_FACE_BACK),
} virgl_face_t;

#define VIRGL_MAX_CLIP_PLANES 8

typedef struct virgl_rasterizer_state_t {
    unsigned flatshade               :1;
    unsigned light_twoside           :1;
    unsigned clamp_vertex_color      :1;
    unsigned clamp_fragment_color    :1;
    unsigned front_ccw               :1;
    unsigned cull_face               :2; /**< VIRGL_FACE_x */
    unsigned fill_front              :2; /**< VIRGL_POLYGON_MODE_x */
    unsigned fill_back               :2; /**< VIRGL_POLYGON_MODE_x */
    unsigned offset_point            :1;
    unsigned offset_line             :1;
    unsigned offset_tri              :1;
    unsigned scissor                 :1;
    unsigned poly_smooth             :1;
    unsigned poly_stipple_enable     :1;
    unsigned point_smooth            :1;
    unsigned sprite_coord_mode       :1; /**< VIRGL_SPRITE_COORD_ */
    unsigned point_quad_rasterization:1; /** points rasterized as quads or points */
    unsigned point_tri_clip          :1; /** large points clipped as tris or points */
    unsigned point_size_per_vertex   :1; /**< size computed in vertex shader */
    unsigned multisample             :1; /* XXX maybe more ms state in future */
    unsigned force_persample_interp  :1;
    unsigned line_smooth             :1;
    unsigned line_stipple_enable     :1;
    unsigned line_last_pixel         :1;
    unsigned flatshade_first         :1;
    unsigned half_pixel_center       :1;
    unsigned bottom_edge_rule        :1;
    unsigned rasterizer_discard      :1;
    unsigned depth_clip              :1;
    unsigned clip_halfz              :1;
    unsigned clip_plane_enable       :VIRGL_MAX_CLIP_PLANES;
    unsigned line_stipple_factor     :8; /**< [1..256] actually */
    unsigned line_stipple_pattern    :16;
    uint32_t sprite_coord_enable; /* referring to 32 TEXCOORD/GENERIC inputs */
    float    line_width;
    float    point_size; /**< used when no per-vertex size */
    float    offset_units;
    float    offset_scale;
    float    offset_clamp;
} virgl_rasterizer_state_t;

#define VIRGL_OBJ_RS_SIZE 9
#define VIRGL_OBJ_RS_S0_FLATSHADE(x) (((x) & 0x1) << 0)
#define VIRGL_OBJ_RS_S0_DEPTH_CLIP(x) (((x) & 0x1) << 1)
#define VIRGL_OBJ_RS_S0_CLIP_HALFZ(x) (((x) & 0x1) << 2)
#define VIRGL_OBJ_RS_S0_RASTERIZER_DISCARD(x) (((x) & 0x1) << 3)
#define VIRGL_OBJ_RS_S0_FLATSHADE_FIRST(x) (((x) & 0x1) << 4)
#define VIRGL_OBJ_RS_S0_LIGHT_TWOSIZE(x) (((x) & 0x1) << 5)
#define VIRGL_OBJ_RS_S0_SPRITE_COORD_MODE(x) (((x) & 0x1) << 6)
#define VIRGL_OBJ_RS_S0_POINT_QUAD_RASTERIZATION(x) (((x) & 0x1) << 7)
#define VIRGL_OBJ_RS_S0_CULL_FACE(x) (((x) & 0x3) << 8)
#define VIRGL_OBJ_RS_S0_FILL_FRONT(x) (((x) & 0x3) << 10)
#define VIRGL_OBJ_RS_S0_FILL_BACK(x) (((x) & 0x3) << 12)
#define VIRGL_OBJ_RS_S0_SCISSOR(x) (((x) & 0x1) << 14)
#define VIRGL_OBJ_RS_S0_FRONT_CCW(x) (((x) & 0x1) << 15)
#define VIRGL_OBJ_RS_S0_CLAMP_VERTEX_COLOR(x) (((x) & 0x1) << 16)
#define VIRGL_OBJ_RS_S0_CLAMP_FRAGMENT_COLOR(x) (((x) & 0x1) << 17)
#define VIRGL_OBJ_RS_S0_OFFSET_LINE(x) (((x) & 0x1) << 18)
#define VIRGL_OBJ_RS_S0_OFFSET_POINT(x) (((x) & 0x1) << 19)
#define VIRGL_OBJ_RS_S0_OFFSET_TRI(x) (((x) & 0x1) << 20)
#define VIRGL_OBJ_RS_S0_POLY_SMOOTH(x) (((x) & 0x1) << 21)
#define VIRGL_OBJ_RS_S0_POLY_STIPPLE_ENABLE(x) (((x) & 0x1) << 22)
#define VIRGL_OBJ_RS_S0_POINT_SMOOTH(x) (((x) & 0x1) << 23)
#define VIRGL_OBJ_RS_S0_POINT_SIZE_PER_VERTEX(x) (((x) & 0x1) << 24)
#define VIRGL_OBJ_RS_S0_MULTISAMPLE(x) (((x) & 0x1) << 25)
#define VIRGL_OBJ_RS_S0_LINE_SMOOTH(x) (((x) & 0x1) << 26)
#define VIRGL_OBJ_RS_S0_LINE_STIPPLE_ENABLE(x) (((x) & 0x1) << 27)
#define VIRGL_OBJ_RS_S0_LINE_LAST_PIXEL(x) (((x) & 0x1) << 28)
#define VIRGL_OBJ_RS_S0_HALF_PIXEL_CENTER(x) (((x) & 0x1) << 29)
#define VIRGL_OBJ_RS_S0_BOTTOM_EDGE_RULE(x) (((x) & 0x1) << 30)
#define VIRGL_OBJ_RS_S0_FORCE_PERSAMPLE_INTERP(x) (((x) & 0x1) << 31)
#define VIRGL_OBJ_RS_S3_LINE_STIPPLE_PATTERN(x) (((x) & 0xffff) << 0)
#define VIRGL_OBJ_RS_S3_LINE_STIPPLE_FACTOR(x) (((x) & 0xff) << 16)
#define VIRGL_OBJ_RS_S3_CLIP_PLANE_ENABLE(x) (((x) & 0xff) << 24)

typedef struct virgl_viewport_state_t {
    float32_t scale[3];
    float32_t translate[3];
} virgl_viewport_state_t;

#define VIRGL_SET_VIEWPORT_STATE_CCMD_PAYLOAD_SIZE(num_viewports) ((6 * num_viewports) + 1)

#define VIRGL_MAX_VERTEX_BUFFERS 16

typedef struct virgl_vertex_buffer_t {
    uint32_t num_buffers;
    struct {
        uint32_t resource_id;
        uint32_t stride;
        uint32_t offset;
    } buffers[VIRGL_MAX_VERTEX_BUFFERS];
} virgl_vertex_buffer_t;

#define VIRGL_SET_VERTEX_BUFFERS_CCMD_PAYLOAD_SIZE(num_buffers) ((num_buffers * 3))

virgl_renderer_t* virgl_renderer_create(memory_heap_t* heap, uint32_t context_id, uint16_t queue_no, lock_t** lock, uint64_t* fence_id, virgl_send_cmd_f send_cmd);

uint32_t       virgl_renderer_get_next_resource_id(virgl_renderer_t* renderer);
memory_heap_t* virgl_renderer_get_heap(virgl_renderer_t* renderer);
virgl_cmd_t*   virgl_renderer_get_cmd(virgl_renderer_t* renderer);

uint32_t virgl_cmd_get_context_id(virgl_cmd_t * cmd);
uint32_t virgl_cmd_get_size(virgl_cmd_t * cmd);
void     virgl_cmd_write_commands(virgl_cmd_t * cmd, void* buffer);
int8_t   virgl_cmd_flush_commands(virgl_cmd_t * cmd);

int8_t virgl_encode_clear(virgl_cmd_t * cmd, virgl_cmd_clear_t * clear);
int8_t virgl_encode_clear_texture(virgl_cmd_t * cmd, virgl_cmd_clear_texture_t * clear_texture);
int8_t virgl_encode_surface(virgl_cmd_t * cmd, virgl_obj_surface_t * surface, boolean_t is_texture);
int8_t virgl_encode_framebuffer_state(virgl_cmd_t * cmd, virgl_obj_framebuffer_state_t * fb_state);
int8_t virgl_encode_copy_region(virgl_cmd_t * cmd, virgl_copy_region_t * copy_region);
int8_t virgl_encode_shader(virgl_cmd_t * cmd, virgl_shader_t * shader);
int8_t virgl_encode_bind_shader(virgl_cmd_t* cmd, uint32_t handle, uint32_t type);
int8_t virgl_encode_link_shader(virgl_cmd_t* cmd, virgl_link_shader_t* link_shader);
int8_t virgl_encode_set_uniform_buffer(virgl_cmd_t* cmd, uint32_t shader, uint32_t index, uint32_t offset, uint32_t length, uint32_t res);
int8_t virgl_encode_set_shader_buffers(virgl_cmd_t* cmd, virgl_shader_buffer_t* shader_buffer);
int8_t virgl_encode_set_shader_images(virgl_cmd_t* cmd, virgl_shader_images_t* shader_images);
int8_t virgl_encode_sampler_view(virgl_cmd_t* cmd, virgl_sampler_view_t* sampler_view, boolean_t is_texture);
int8_t virgl_encode_sampler_views(virgl_cmd_t* cmd, virgl_sampler_views_t* sample_views);
int8_t virgl_encode_draw_vbo(virgl_cmd_t* cmd, virgl_draw_info_t* draw_info);
int8_t virgl_encode_inline_write(virgl_cmd_t* cmd, virgl_res_iw_t* res_iw, const void* data);
int8_t virgl_encode_bind_object(virgl_cmd_t* cmd, virgl_object_type_t object_type, uint32_t object_id);
int8_t virgl_encode_create_vertex_elements(virgl_cmd_t* cmd, uint32_t handle, uint32_t num_elements, const virgl_vertex_element_t* element);
int8_t virgl_encode_blend_state(virgl_cmd_t* cmd, uint32_t handle, virgl_blend_state_t* blend_state);
int8_t virgl_encode_dsa_state(virgl_cmd_t* cmd, uint32_t handle, virgl_depth_stencil_alpha_state_t* dsa_state);
int8_t virgl_encode_rasterizer_state(virgl_cmd_t* cmd, uint32_t handle, virgl_rasterizer_state_t* rasterizer_state);
int8_t virgl_encode_set_viewport_states(virgl_cmd_t* cmd, int32_t start_slot, int num_viewports, const virgl_viewport_state_t * states);
int8_t virgl_encode_set_vertex_buffers(virgl_cmd_t* cmd, virgl_vertex_buffer_t* buffers);

#endif // ___VIRGL_H
