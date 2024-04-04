/**
 * @file virgl.64.c
 * @brief virgl implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <graphics/virgl.h>
#include <memory.h>
#include <utils.h>
#include <logging.h>

MODULE("turnstone.kenrel.graphics.virgl");

void video_text_print(const char_t* text);

struct virgl_renderer_t {
    memory_heap_t* heap;
    uint32_t       next_resource_id;
    virgl_cmd_t*   cmd;
};

struct virgl_cmd_t {
    uint32_t         context_id;
    uint16_t         queue_no;
    lock_t**         lock;
    uint64_t*        fence_id;
    virgl_send_cmd_f send_cmd;
    uint32_t         cmd_dw_count;
    uint32_t         cmd_dws[VIRGL_CMD_MAX_DWORDS];
};

virgl_renderer_t* virgl_renderer_create(memory_heap_t* heap, uint32_t context_id, uint16_t queue_no, lock_t** lock, uint64_t* fence_id, virgl_send_cmd_f send_cmd) {
    if(!heap || !lock || !fence_id || !send_cmd) {
        return NULL;
    }

    virgl_renderer_t* renderer = memory_malloc_ext(heap, sizeof(virgl_renderer_t), 0);
    if(!renderer) {
        return NULL;
    }

    renderer->heap = heap;
    renderer->next_resource_id = 1;
    renderer->cmd = memory_malloc_ext(heap, sizeof(virgl_cmd_t), 0);

    if(!renderer->cmd) {
        memory_free_ext(heap, renderer);
        return NULL;
    }

    renderer->cmd->context_id = context_id;
    renderer->cmd->queue_no = queue_no;
    renderer->cmd->lock = lock;
    renderer->cmd->fence_id = fence_id;
    renderer->cmd->send_cmd = send_cmd;

    return renderer;
}

uint32_t virgl_renderer_get_next_resource_id(virgl_renderer_t* renderer) {
    if(!renderer) {
        return 1;
    }

    return renderer->next_resource_id++;
}

memory_heap_t* virgl_renderer_get_heap(virgl_renderer_t* renderer) {
    if(!renderer) {
        return NULL;
    }

    return renderer->heap;
}

virgl_cmd_t* virgl_renderer_get_cmd(virgl_renderer_t* renderer) {
    if(!renderer) {
        return NULL;
    }

    return renderer->cmd;
}

uint32_t virgl_cmd_get_context_id(virgl_cmd_t * cmd) {
    if(!cmd) {
        return 1;
    }

    return cmd->context_id;
}

uint32_t virgl_cmd_get_size(virgl_cmd_t * cmd) {
    if(!cmd) {
        return 0;
    }

    return cmd->cmd_dw_count * sizeof(uint32_t);
}

void virgl_cmd_write_commands(virgl_cmd_t * cmd, void* buffer) {
    if(!cmd || !buffer) {
        return;
    }

    memory_memcopy(cmd->cmd_dws, buffer, virgl_cmd_get_size(cmd));
    cmd->cmd_dw_count = 0;
}

int8_t virgl_cmd_flush_commands(virgl_cmd_t * cmd) {
    if(!cmd || !cmd->send_cmd) {
        return 0;
    }

    if(cmd->cmd_dw_count == 0) {
        return 0;
    }

    uint32_t fence_id = (*cmd->fence_id)++;
    return cmd->send_cmd(cmd->queue_no, cmd->lock, fence_id, cmd);
}


static int8_t virgl_encode_write_dword(virgl_cmd_t* cmd, uint32_t dword) {
    if(cmd->cmd_dw_count + 1 > VIRGL_CMD_MAX_DWORDS) {
        return -1;
    }

    cmd->cmd_dws[cmd->cmd_dw_count] = dword;
    cmd->cmd_dw_count++;

    return 0;
}

static int8_t virgl_encode_write_cmd_header(virgl_cmd_t* cmd, uint16_t cmd_type, uint16_t object_type, uint16_t size) {
    if(cmd->cmd_dw_count + size + 1 > VIRGL_CMD_MAX_DWORDS) {
        int8_t ret = virgl_cmd_flush_commands(cmd);

        if(ret) {
            video_text_print("virgl_encode_write_cmd_header: failed to flush commands\n");
            return -1;
        }
    }

    return virgl_encode_write_dword(cmd, VIRGL_CMD(cmd_type, object_type, size));
}

static int8_t virgl_encode_write_float64(virgl_cmd_t* cmd, float64_t qword) {
    if(cmd->cmd_dw_count + 2 > VIRGL_CMD_MAX_DWORDS) {
        return -1;
    }

    memory_memcopy(&qword, &cmd->cmd_dws[cmd->cmd_dw_count], sizeof(float64_t));

    cmd->cmd_dw_count += 2;

    return 0;
}

int8_t virgl_encode_clear(virgl_cmd_t* cmd, virgl_cmd_clear_t * clear) {
    if(virgl_encode_write_cmd_header(cmd, VIRGL_CCMD_CLEAR, 0, VIRGL_CMD_CLEAR_CCMD_PAYLOAD_SIZE)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, clear->clear_flags)) {
        return -1;
    }

    for(uint32_t i = 0; i < 4; i++) {
        if(virgl_encode_write_dword(cmd, clear->clear_color.ui32[i])) {
            return -1;
        }
    }

    if(virgl_encode_write_float64(cmd, clear->clear_depth)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, clear->clear_stencil)) {
        return -1;
    }

    return 0;
}

int8_t virgl_encode_clear_texture(virgl_cmd_t * cmd, virgl_cmd_clear_texture_t * clear_texture) {
    if(virgl_encode_write_cmd_header(cmd, VIRGL_CCMD_CLEAR_TEXTURE, 0, VIRGL_CMD_CLEAR_TEXTURE_CCMD_PAYLOAD_SIZE)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, clear_texture->texture_id)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, clear_texture->level)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, clear_texture->box.x)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, clear_texture->box.y)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, clear_texture->box.z)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, clear_texture->box.w)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, clear_texture->box.h)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, clear_texture->box.d)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, clear_texture->clear_color.ui32[0])) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, clear_texture->clear_color.ui32[1])) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, clear_texture->clear_color.ui32[2])) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, clear_texture->clear_color.ui32[3])) {
        return -1;
    }

    return 0;
}

int8_t virgl_encode_surface(virgl_cmd_t* cmd, virgl_obj_surface_t * surface, boolean_t is_texture) {
    if(virgl_encode_write_cmd_header(cmd, VIRGL_CCMD_CREATE_OBJECT, VIRGL_OBJECT_SURFACE, VIRGL_OBJ_SURFACE_CCMD_PAYLOAD_SIZE)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, surface->surface_id)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, surface->resource_id)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, surface->format)) {
        return -1;
    }

    if(is_texture) {
        if(virgl_encode_write_dword(cmd, surface->texture.level)) {
            return -1;
        }

        if(virgl_encode_write_dword(cmd, surface->texture.layers)) {
            return -1;
        }
    } else {
        if(virgl_encode_write_dword(cmd, surface->buffer.first_element)) {
            return -1;
        }

        if(virgl_encode_write_dword(cmd, surface->buffer.last_element)) {
            return -1;
        }
    }

    return 0;
}

int8_t virgl_encode_framebuffer_state(virgl_cmd_t* cmd, virgl_obj_framebuffer_state_t * fb_state) {
    if(virgl_encode_write_cmd_header(cmd, VIRGL_CCMD_SET_FRAMEBUFFER_STATE, 0,
                                     VIRGL_OBJ_FRAMEBUFFER_STATE_CCMD_PAYLOAD_SIZE(fb_state->nr_cbufs))) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, fb_state->nr_cbufs)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, fb_state->zsurf_id)) {
        return -1;
    }

    for(uint32_t i = 0; i < fb_state->nr_cbufs; i++) {
        if(virgl_encode_write_dword(cmd, fb_state->cbuf_ids[i])) {
            return -1;
        }
    }

    return 0;
}

int8_t virgl_encode_copy_region(virgl_cmd_t* cmd, virgl_copy_region_t * copy_region) {
    if(virgl_encode_write_cmd_header(cmd, VIRGL_CCMD_RESOURCE_COPY_REGION, 0, VIRGL_COPY_REGION_CCMD_PAYLOAD_SIZE)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, copy_region->dst_resource_id)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, copy_region->dst_level)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, copy_region->dst_x)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, copy_region->dst_y)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, copy_region->dst_z)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, copy_region->src_resource_id)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, copy_region->src_level)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, copy_region->src_box.x)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, copy_region->src_box.y)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, copy_region->src_box.z)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, copy_region->src_box.w)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, copy_region->src_box.h)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, copy_region->src_box.d)) {
        return -1;
    }

    return 0;
}


static void virgl_encode_emit_shader_header(virgl_cmd_t* cmd,
                                            uint32_t handle, uint32_t len,
                                            uint32_t type, uint32_t offlen,
                                            uint32_t num_tokens) {
    virgl_encode_write_cmd_header(cmd, VIRGL_CCMD_CREATE_OBJECT, VIRGL_OBJECT_SHADER, len);
    virgl_encode_write_dword(cmd, handle);
    virgl_encode_write_dword(cmd, type);
    virgl_encode_write_dword(cmd, offlen);
    virgl_encode_write_dword(cmd, num_tokens);
}

static void virgl_encode_emit_shader_streamout(virgl_cmd_t*    cmd,
                                               virgl_shader_t* shader) {
    int32_t num_outputs = 0;
    uint32_t tmp;

    if (shader)
        num_outputs = shader->num_outputs;

    virgl_encode_write_dword(cmd, num_outputs);

    if (num_outputs) {
        for (int32_t i = 0; i < 4; i++) {
            virgl_encode_write_dword(cmd, shader->stride[i]);
        }

        for (uint32_t i = 0; i < shader->num_outputs; i++) {
            tmp = VIRGL_ENCODE_SO_DECLARATION(shader->output[i]);
            virgl_encode_write_dword(cmd, tmp);
            virgl_encode_write_dword(cmd, 0);
        }
    }
}

static void virgl_encode_write_block(virgl_cmd_t* cmd, const void* data, uint32_t length) {
    memory_memcopy(data, &cmd->cmd_dws[cmd->cmd_dw_count], length);

    uint32_t rem = length % 4;

    if (rem) {
        memory_memclean(&cmd->cmd_dws[cmd->cmd_dw_count + length / 4], 0);
    }

    cmd->cmd_dw_count += (length + 3) / 4;
}

int8_t virgl_encode_shader(virgl_cmd_t* cmd, virgl_shader_t* shader) {
    uint32_t left_bytes = shader->data_size;

    uint32_t base_hdr_size = 5;
    uint32_t strm_hdr_size = shader->num_outputs ? shader->num_outputs * 2 + 4 : 0;
    boolean_t first_pass = true;
    const char_t* sptr = shader->data;
    uint32_t num_tokens = 300; // XXX: why 300?

    while(left_bytes) {
        uint32_t length, offlen;
        int32_t hdr_len = base_hdr_size + (first_pass ? strm_hdr_size : 0);

        if (cmd->cmd_dw_count + hdr_len + 1 > VIRGL_CMD_MAX_DWORDS) {
            if (virgl_cmd_flush_commands(cmd)) {
                return -1;
            }
        }

        uint32_t thispass = (VIRGL_CMD_MAX_DWORDS - cmd->cmd_dw_count - hdr_len - 1) * 4;

        length = MIN(thispass, left_bytes);
        uint32_t len = ((length + 3) / 4) + hdr_len;

        if (first_pass) {
            offlen = VIRGL_OBJECT_SHADER_OFFSET_VAL(shader->data_size);
        } else {
            offlen = VIRGL_OBJECT_SHADER_OFFSET_VAL((uint64_t)sptr - (uint64_t)shader->data) | VIRGL_OBJECT_SHADER_OFFSET_CONT;
        }

        virgl_encode_emit_shader_header(cmd, shader->handle, len, shader->type, offlen, num_tokens);

        virgl_encode_emit_shader_streamout(cmd, first_pass ? shader : NULL);

        virgl_encode_write_block(cmd, sptr, length);

        sptr += length;
        first_pass = false;
        left_bytes -= length;
    }

    return 0;
}

int8_t virgl_encode_bind_shader(virgl_cmd_t* cmd, uint32_t handle, uint32_t type) {
    if(virgl_encode_write_cmd_header(cmd, VIRGL_CCMD_BIND_SHADER, 0, 2)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, handle)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, type)) {
        return -1;
    }

    return 0;
}

int8_t virgl_encode_link_shader(virgl_cmd_t* cmd, virgl_link_shader_t* link_shader) {
    if(virgl_encode_write_cmd_header(cmd, VIRGL_CCMD_LINK_SHADER, 0, VIRGL_LINK_SHADER_CCMD_PAYLOAD_SIZE)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, link_shader->vertex_shader_id)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, link_shader->fragment_shader_id)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, link_shader->geometry_shader_id)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, link_shader->tess_ctrl_shader_id)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, link_shader->tess_eval_shader_id)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, link_shader->compute_shader_id)) {
        return -1;
    }

    return 0;
}

int8_t virgl_encode_set_uniform_buffer(virgl_cmd_t* cmd,
                                       uint32_t     shader,
                                       uint32_t     index,
                                       uint32_t     offset,
                                       uint32_t     length,
                                       uint32_t     res) {
    virgl_encode_write_cmd_header(cmd, VIRGL_CCMD_SET_UNIFORM_BUFFER, 0, VIRGL_SET_UNIFORM_BUFFER_CCMD_PAYLOAD_SIZE);
    virgl_encode_write_dword(cmd, shader);
    virgl_encode_write_dword(cmd, index);
    virgl_encode_write_dword(cmd, offset);
    virgl_encode_write_dword(cmd, length);
    virgl_encode_write_dword( cmd, res);
    return 0;
}

int8_t virgl_encode_set_shader_buffers(virgl_cmd_t* cmd, virgl_shader_buffer_t* shader_buffer) {
    if(shader_buffer->num_elements > VIRGL_SHADER_BUFFER_MAX_ELEMENTS) {
        return -1;
    }

    if(virgl_encode_write_cmd_header(cmd, VIRGL_CCMD_SET_SHADER_BUFFERS, 0, VIRGL_SET_SHADER_BUFFERS_CCMD_PAYLOAD_SIZE(shader_buffer->num_elements))) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, shader_buffer->shader_type)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, shader_buffer->index)) {
        return -1;
    }

    for(uint32_t i = 0; i < shader_buffer->num_elements; i++) {
        if(virgl_encode_write_dword(cmd, shader_buffer->elements[i].offset)) {
            return -1;
        }

        if(virgl_encode_write_dword(cmd, shader_buffer->elements[i].length)) {
            return -1;
        }

        if(virgl_encode_write_dword(cmd, shader_buffer->elements[i].res)) {
            return -1;
        }
    }

    return 0;
}

int8_t virgl_encode_set_shader_images(virgl_cmd_t* cmd, virgl_shader_images_t* shader_images) {
    if(shader_images->num_images > VIRGL_SHADER_IMAGE_MAX_IMAGES) {
        return -1;
    }

    if(virgl_encode_write_cmd_header(cmd, VIRGL_CCMD_SET_SHADER_IMAGES, 0, VIRGL_SET_SHADER_IMAGES_CCMD_PAYLOAD_SIZE(shader_images->num_images))) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, shader_images->shader_type)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, shader_images->index)) {
        return -1;
    }

    for(uint32_t i = 0; i < shader_images->num_images; i++) {
        if(virgl_encode_write_dword(cmd, shader_images->images[i].format)) {
            return -1;
        }

        if(virgl_encode_write_dword(cmd, shader_images->images[i].access)) {
            return -1;
        }

        if(virgl_encode_write_dword(cmd, shader_images->images[i].offset)) {
            return -1;
        }

        if(virgl_encode_write_dword(cmd, shader_images->images[i].size)) {
            return -1;
        }

        if(virgl_encode_write_dword(cmd, shader_images->images[i].res)) {
            return -1;
        }
    }

    return 0;
}

int8_t virgl_encode_sampler_view(virgl_cmd_t* cmd, virgl_sampler_view_t* sampler_view, boolean_t is_texture) {
    if(virgl_encode_write_cmd_header(cmd, VIRGL_CCMD_CREATE_OBJECT, VIRGL_OBJECT_SAMPLER_VIEW, VIRGL_OBJ_SAMPLER_VIEW_CCMD_PAYLOAD_SIZE)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, sampler_view->view_id)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, sampler_view->texture_id)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, sampler_view->format)) {
        return -1;
    }

    if(is_texture) {
        if(virgl_encode_write_dword(cmd, sampler_view->texture.level)) {
            return -1;
        }

        if(virgl_encode_write_dword(cmd, sampler_view->texture.layers)) {
            return -1;
        }
    } else {
        if(virgl_encode_write_dword(cmd, sampler_view->buffer.first_element)) {
            return -1;
        }

        if(virgl_encode_write_dword(cmd, sampler_view->buffer.last_element)) {
            return -1;
        }
    }

    uint32_t swizzle = VIRGL_SAMPLER_VIEW_OBJECT_ENCODE_SWIZZLE(sampler_view->swizzle_r, sampler_view->swizzle_g, sampler_view->swizzle_b, sampler_view->swizzle_a);

    if(virgl_encode_write_dword(cmd, swizzle)) {
        return -1;
    }

    return 0;
}

int8_t virgl_encode_sampler_views(virgl_cmd_t* cmd, virgl_sampler_views_t* sampler_views) {
    if(sampler_views->num_views > VIRGL_SAMPLER_VIEWS_MAX_VIEWS) {
        return -1;
    }

    if(virgl_encode_write_cmd_header(cmd, VIRGL_CCMD_SET_SAMPLER_VIEWS, 0, VIRGL_SET_SAMPLER_VIEWS_CCMD_PAYLOAD_SIZE(sampler_views->num_views))) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, sampler_views->shader_type)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, sampler_views->index)) {
        return -1;
    }

    for(uint32_t i = 0; i < sampler_views->num_views; i++) {
        if(virgl_encode_write_dword(cmd, sampler_views->res_ids[i])) {
            return -1;
        }
    }

    return 0;
}

int8_t virgl_encode_draw_vbo(virgl_cmd_t* cmd, virgl_draw_info_t* draw_info) {
    if(virgl_encode_write_cmd_header(cmd, VIRGL_CCMD_DRAW_VBO, 0, VIRGL_DRAW_CCMD_PAYLOAD_SIZE)) {
        return -1;
    }

    virgl_encode_write_dword(cmd, draw_info->start);
    virgl_encode_write_dword(cmd, draw_info->count);
    virgl_encode_write_dword(cmd, draw_info->mode);
    virgl_encode_write_dword(cmd, draw_info->indexed);
    virgl_encode_write_dword(cmd, draw_info->instance_count);
    virgl_encode_write_dword(cmd, draw_info->index_bias);
    virgl_encode_write_dword(cmd, draw_info->start_instance);
    virgl_encode_write_dword(cmd, draw_info->primitive_restart);
    virgl_encode_write_dword(cmd, draw_info->restart_index);
    virgl_encode_write_dword(cmd, draw_info->min_index);
    virgl_encode_write_dword(cmd, draw_info->max_index);

    if (draw_info->has_indirect_handle != 0) {
        virgl_encode_write_dword(cmd, draw_info->indirect_handle);
        virgl_encode_write_dword(cmd, draw_info->indirect_offset);
        virgl_encode_write_dword(cmd, draw_info->indirect_stride);
        virgl_encode_write_dword(cmd, draw_info->indirect_draw_count);
        virgl_encode_write_dword(cmd, draw_info->indirect_draw_count_offset);
        virgl_encode_write_dword(cmd, draw_info->indirect_draw_count_handle);
    }

    if (draw_info->has_count_from_stream_output) {
        virgl_encode_write_dword(cmd, draw_info->has_count_from_stream_output);
    } else {
        virgl_encode_write_dword(cmd, 0);
    }

    return 0;
}

static void virgl_encode_transfer3d_common(virgl_cmd_t* cmd,
                                           uint32_t res,
                                           unsigned level, unsigned usage,
                                           virgl_box_t* box,
                                           unsigned stride, unsigned layer_stride) {
    virgl_encode_write_dword(cmd, res);
    virgl_encode_write_dword(cmd, level);
    virgl_encode_write_dword(cmd, usage);
    virgl_encode_write_dword(cmd, stride);
    virgl_encode_write_dword(cmd, layer_stride);
    virgl_encode_write_dword(cmd, box->x);
    virgl_encode_write_dword(cmd, box->y);
    virgl_encode_write_dword(cmd, box->z);
    virgl_encode_write_dword(cmd, box->w);
    virgl_encode_write_dword(cmd, box->h);
    virgl_encode_write_dword(cmd, box->d);
}

static void virgl_encoder_inline_send_box(virgl_cmd_t* cmd,
                                          uint32_t res,
                                          unsigned level, unsigned usage,
                                          virgl_box_t* box,
                                          const void* data, unsigned stride,
                                          unsigned layer_stride, int length) {
    virgl_encode_write_cmd_header(cmd, VIRGL_CCMD_RESOURCE_INLINE_WRITE, 0, ((length + 3) / 4) + 11);
    virgl_encode_transfer3d_common(cmd, res, level, usage, box, stride, layer_stride);
    virgl_encode_write_block(cmd, data, length);
}

int8_t virgl_encode_inline_write(virgl_cmd_t* cmd, virgl_res_iw_t* res_iw, const void* data) {
    uint32_t length, thispass, left_bytes;
    virgl_box_t mybox = res_iw->box;
    uint32_t elsize, size;
    uint32_t layer_size;
    uint32_t stride_internal = res_iw->stride;
    uint32_t layer_stride_internal = res_iw->layer_stride;
    uint32_t layer, row;
    elsize = res_iw->element_size;

    /* total size of data to transfer */
    if (!res_iw->stride) {
        stride_internal = res_iw->box.w * elsize;
    }

    layer_size = res_iw->box.h * stride_internal;

    if (res_iw->layer_stride && res_iw->layer_stride < layer_size) {
        return -1;
    }

    if (!res_iw->layer_stride) {
        layer_stride_internal = layer_size;
    }

    size = layer_stride_internal * res_iw->box.d;

    length = 11 + (size + 3) / 4;

    /* can we send it all in one cmdbuf? */
    if (length < VIRGL_CMD_MAX_DWORDS) {
        /* is there space in this cmdbuf? if not flush and use another one */
        if ((cmd->cmd_dw_count + length + 1) > VIRGL_CMD_MAX_DWORDS) {
            if (virgl_cmd_flush_commands(cmd)) {
                return -1;
            }
        }
        /* send it all in one go. */
        virgl_encoder_inline_send_box(cmd, res_iw->res_id, res_iw->level, res_iw->usage, &mybox, data, res_iw->stride, res_iw->layer_stride, size);
        return 0;
    }

    /* break things down into chunks we can send */
    /* send layers in separate chunks */
    for (layer = 0; layer < res_iw->box.d; layer++) {
        const uint8_t * layer_data = data;
        mybox.z = layer;
        mybox.d = 1;

        /* send one line in separate chunks */
        for (row = 0; row < res_iw->box.h; row++) {
            const uint8_t * row_data = layer_data;
            mybox.y = row;
            mybox.h = 1;
            mybox.x = 0;

            left_bytes = res_iw->box.w * elsize;
            while (left_bytes) {
                if (cmd->cmd_dw_count + 12 > VIRGL_CMD_MAX_DWORDS) {
                    if (virgl_cmd_flush_commands(cmd)) {
                        return -1;
                    }
                }

                thispass = (VIRGL_CMD_MAX_DWORDS - cmd->cmd_dw_count - 12) * 4;

                length = MIN(thispass, left_bytes);

                mybox.w = length / elsize;

                virgl_encoder_inline_send_box(cmd, res_iw->res_id, res_iw->level, res_iw->usage, &mybox, row_data, res_iw->stride, res_iw->layer_stride, length);

                left_bytes -= length;
                mybox.x += length / elsize;
                row_data += length;
            }

            layer_data += stride_internal;
        }

        data = (uint8_t *)data + layer_stride_internal;
    }

    return 0;
}
