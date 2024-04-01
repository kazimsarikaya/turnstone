/**
 * @file virgl.64.c
 * @brief virgl implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <graphics/virgl.h>
#include <memory.h>

MODULE("turnstone.kenrel.graphics.virgl");

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
        int8_t ret = 0;
        if(cmd->send_cmd) {
            uint32_t fence_id = (*cmd->fence_id)++;
            ret = cmd->send_cmd(cmd->queue_no, cmd->lock, fence_id, cmd);
        } else {
            ret = -1;
        }

        if(ret) {
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
