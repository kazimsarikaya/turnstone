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

static int8_t virgl_encode_write_dword(virgl_cmd_t* cmd, uint32_t dword) {
    if(cmd->cmd_dw_count + 1 > VIRGL_CMD_MAX_DWORDS) {
        return -1;
    }

    cmd->cmd_dws[cmd->cmd_dw_count] = dword;
    cmd->cmd_dw_count++;

    return 0;
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
    if(virgl_encode_write_dword(cmd, VIRGL_CMD(VIRGL_CCMD_CLEAR, 0, VIRGL_CMD_CLEAR_CCMD_PAYLOAD_SIZE))) {
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

int8_t virgl_encode_surface(virgl_cmd_t* cmd, virgl_obj_surface_t * surface, boolean_t is_texture) {
    if(virgl_encode_write_dword(cmd, VIRGL_CMD(VIRGL_CCMD_CREATE_OBJECT, VIRGL_OBJECT_SURFACE, VIRGL_OBJ_SURFACE_CCMD_PAYLOAD_SIZE))) {
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
    if(virgl_encode_write_dword(cmd, VIRGL_CMD(VIRGL_CCMD_SET_FRAMEBUFFER_STATE, 0,
                                               VIRGL_OBJ_FRAMEBUFFER_STATE_CCMD_PAYLOAD_SIZE(fb_state->nr_cbufs)))) {
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
    if(virgl_encode_write_dword(cmd, VIRGL_CMD(VIRGL_CCMD_RESOURCE_COPY_REGION, 0, VIRGL_COPY_REGION_CCMD_PAYLOAD_SIZE))) {
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

    if(virgl_encode_write_dword(cmd, copy_region->src_x)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, copy_region->src_y)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, copy_region->src_z)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, copy_region->width)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, copy_region->height)) {
        return -1;
    }

    if(virgl_encode_write_dword(cmd, copy_region->depth)) {
        return -1;
    }

    return 0;
}
