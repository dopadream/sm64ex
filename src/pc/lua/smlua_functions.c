#include "smlua.h"
#include "smlua_cobject.h"

#include "game/level_update.h"
#include "game/area.h"
#include "game/mario.h"
#include "game/mario_step.h"
#include "game/mario_actions_stationary.h"
#include "audio/external.h"
#include "object_fields.h"
#include "engine/math_util.h"
#include "pc/djui/djui_hud_utils.h"
#include "include/level_misc_macros.h"
#include "include/macro_presets.h"

bool smlua_functions_valid_param_count(lua_State* L, int expected) {
    int top = lua_gettop(L);
    if (top != expected) {
        LOG_LUA_LINE("Improper param count: Expected %u, Received %u", expected, top);
        return false;
    }
    return true;
}

bool smlua_functions_valid_param_range(lua_State* L, int min, int max) {
    int top = lua_gettop(L);
    if (top < min || top > max) {
        LOG_LUA_LINE("Improper param count: Expected (%u - %u), Received %u", min, max, top);
        return false;
    }
    return true;
}

  //////////
 // misc //
//////////

int smlua_func_sins(lua_State* L) {
    if (!smlua_functions_valid_param_count(L, 1)) { return 0; }

    f32 x = smlua_to_number(L, 1);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 1"); return 0; }

    lua_pushnumber(L, sins(x));
    return 1;
}

int smlua_func_coss(lua_State* L) {
    if (!smlua_functions_valid_param_count(L, 1)) { return 0; }

    f32 x = smlua_to_number(L, 1);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 1"); return 0; }

    lua_pushnumber(L, coss(x));
    return 1;
}

int smlua_func_atan2s(lua_State* L) {
    if (!smlua_functions_valid_param_count(L, 2)) { return 0; }

    f32 y = smlua_to_number(L, 1);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 1"); return 0; }
    f32 x = smlua_to_number(L, 2);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 2"); return 0; }

    lua_pushinteger(L, atan2s(y, x));
    return 1;
}

int smlua_func_init_mario_after_warp(lua_State* L) {
    if (network_player_connected_count() >= 2) {
        LOG_LUA_LINE("This function can only be used in single-player");
        return 0;
    }

    if(!smlua_functions_valid_param_count(L, 0)) { return 0; }

    extern void init_mario_after_warp(void);
    init_mario_after_warp();

    return 1;
}

int smlua_func_initiate_warp(lua_State* L) {
    if(!smlua_functions_valid_param_count(L, 4)) { return 0; }

    s16 destLevel = smlua_to_number(L, 1);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 1"); return 0; }
    s16 destArea = smlua_to_number(L, 2);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 2"); return 0; }
    s16 destWarpNode = smlua_to_number(L, 3);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 3"); return 0; }
    s32 arg3 = smlua_to_number(L, 4);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 4"); return 0; }

    extern void initiate_warp(s16 destLevel, s16 destArea, s16 destWarpNode, s32 arg3);
    initiate_warp(destLevel, destArea, destWarpNode, arg3);

    return 1;
}

int smlua_func_reset_level(lua_State* L) {
    if (network_player_connected_count() >= 2) {
        LOG_LUA_LINE("This function can only be used in single-player");
        return 0;
    }

    if(!smlua_functions_valid_param_count(L, 0)) { return 0; }

    gChangeLevel = gCurrLevelNum;

    return 1;
}

int smlua_func_network_init_object(lua_State* L) {
    if (!smlua_functions_valid_param_count(L, 3)) { return 0; }

    struct Object* obj = smlua_to_cobject(L, 1, LOT_OBJECT);
    if (!gSmLuaConvertSuccess || obj == NULL) { LOG_LUA("Failed to convert parameter 1"); return 0; }

    bool standardSync = smlua_to_boolean(L, 2);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 2"); return 0; }

    if (lua_type(L, 3) != LUA_TNIL && lua_type(L, 3) != LUA_TTABLE) {
        LOG_LUA_LINE("network_init_object() called with an invalid type for param 3: %u", lua_type(L, 3));
        return 0;
    }

    struct SyncObject* so = sync_object_init(obj, standardSync ? 4000.0f : SYNC_DISTANCE_ONLY_EVENTS);
    if (so == NULL) {
        LOG_LUA_LINE("Failed to allocate sync object.");
        return 0;
    }

    if (lua_type(L, 3) == LUA_TTABLE) {
        lua_pushnil(L);  // first key

        while (lua_next(L, 3) != 0) {
            // uses 'key' (at index -2) and 'value' (at index -1)
            if (lua_type(L, -1) != LUA_TSTRING) {
                LOG_LUA_LINE("Invalid type passed to network_init_object(): %u", lua_type(L, -1));
                lua_pop(L, 1); // pop value
                continue;
            }
            const char* fieldIdentifier = smlua_to_string(L, -1);
            if (!gSmLuaConvertSuccess) {
                LOG_LUA_LINE("Invalid field passed to network_init_object()");
                lua_pop(L, 1); // pop value
                continue;
            }

            struct LuaObjectField* data = smlua_get_object_field(LOT_OBJECT, fieldIdentifier);
            if (data == NULL) {
                data = smlua_get_custom_field(L, LOT_OBJECT, lua_gettop(L));
            }

            u8 lvtSize = 0;
            if ((data->valueType == LVT_U32) || (data->valueType == LVT_S32) || (data->valueType == LVT_F32)) { lvtSize = 32; }
            if ((data->valueType == LVT_U16) || (data->valueType == LVT_S16)) { lvtSize = 16; }
            if ((data->valueType == LVT_U8) || (data->valueType == LVT_S8)) { lvtSize = 8; }

            if (data == NULL || lvtSize == 0) {
                LOG_LUA_LINE("Invalid field passed to network_init_object(): %s", fieldIdentifier);
                lua_pop(L, 1); // pop value
                continue;
            }

            u8* field = ((u8*)(intptr_t)obj) + data->valueOffset;
            sync_object_init_field_with_size(obj, field, lvtSize);

            lua_pop(L, 1); // pop value
        }
        lua_pop(L, 1); // pop key
    }

    return 1;
}

int smlua_func_network_send_object(lua_State* L) {
    if (!smlua_functions_valid_param_count(L, 2)) { return 0; }

    struct Object* obj = smlua_to_cobject(L, 1, LOT_OBJECT);
    if (!gSmLuaConvertSuccess || obj == NULL) { LOG_LUA("Failed to convert parameter 1"); return 0; }

    bool reliable = smlua_to_boolean(L, 2);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 2"); return 0; }

    struct SyncObject* so = sync_object_get(obj->oSyncID);
    if (!so || so->o != obj) {
        LOG_LUA_LINE("Failed to retrieve sync object.");
        return 0;
    }

    network_send_object_reliability(obj, reliable);

    return 1;
}

int smlua_func_network_send(lua_State* L) {
    if (!smlua_functions_valid_param_count(L, 2)) { return 0; }
    network_send_lua_custom(true);
    return 1;
}

int smlua_func_network_send_to(lua_State* L) {
    if (!smlua_functions_valid_param_count(L, 3)) { return 0; }
    network_send_lua_custom(false);
    return 1;
}

  //////////////
 // Textures //
//////////////

int smlua_func_get_texture_info(lua_State* L) {
    if (!smlua_functions_valid_param_count(L, 1)) { return 0; }

    if (lua_type(L, -1) != LUA_TSTRING) {
        LOG_LUA_LINE("Invalid type passed to get_texture_info(): %u", lua_type(L, -1));
        lua_pop(L, 1); // pop value
        return 0;
    }

    struct TextureInfo texInfo = { 0 };
    const char* textureName = smlua_to_string(L, -1);
    if (!dynos_texture_get(textureName, &texInfo)) {
        LOG_LUA_LINE("Could not find texture info for '%s'", textureName);
        return 0;
    }

    lua_newtable(L);

    lua_pushstring(L, "texture");
    smlua_push_pointer(L, LVT_U8_P, texInfo.texture);
    lua_settable(L, -3);

    lua_pushstring(L, "bitSize");
    lua_pushinteger(L, texInfo.bitSize);
    lua_settable(L, -3);

    lua_pushstring(L, "width");
    lua_pushinteger(L, texInfo.width);
    lua_settable(L, -3);

    lua_pushstring(L, "height");
    lua_pushinteger(L, texInfo.height);
    lua_settable(L, -3);

    return 1;
}

int smlua_func_djui_hud_render_texture(lua_State* L) {
    if(!smlua_functions_valid_param_count(L, 5)) { return 0; }

    struct TextureInfo tmpTexInfo = { 0 };
    struct TextureInfo* texInfo = &tmpTexInfo;

    if (smlua_is_cobject(L, 1, LOT_TEXTUREINFO)) {
        texInfo = (struct TextureInfo*)smlua_to_cobject(L, 1, LOT_TEXTUREINFO);
        if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 1"); return 0; }
    } else {
        int top = lua_gettop(L);
        lua_pushvalue(L, 1);

        lua_pushstring(L, "texture");
        lua_gettable(L, top+1);
        tmpTexInfo.texture = smlua_to_cpointer(L, lua_gettop(L), LVT_U8_P);
        lua_pop(L, 1);
        if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 1's 'texture' field"); return 0; }

        tmpTexInfo.bitSize = smlua_get_integer_field(top+1, "bitSize");
        if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 1's 'bitSize' field"); return 0; }

        tmpTexInfo.width   = smlua_get_integer_field(top+1, "width");
        if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 1's 'width' field"); return 0; }

        tmpTexInfo.height  = smlua_get_integer_field(top+1, "height");
        if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 1's 'height' field"); return 0; }

        lua_settop(L, top);
    }

    f32 x = smlua_to_number(L, 2);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 2"); return 0; }
    f32 y = smlua_to_number(L, 3);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 3"); return 0; }
    f32 scaleW = smlua_to_number(L, 4);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 4"); return 0; }
    f32 scaleH = smlua_to_number(L, 5);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 5"); return 0; }

    djui_hud_render_texture_raw(texInfo->texture, texInfo->bitSize, texInfo->width, texInfo->height, x, y, scaleW, scaleH);

    return 1;
}

int smlua_func_djui_hud_render_texture_tile(lua_State* L) {
    if(!smlua_functions_valid_param_count(L, 9)) { return 0; }

    struct TextureInfo tmpTexInfo = { 0 };
    struct TextureInfo* texInfo = &tmpTexInfo;

    if (smlua_is_cobject(L, 1, LOT_TEXTUREINFO)) {
        texInfo = (struct TextureInfo*)smlua_to_cobject(L, 1, LOT_TEXTUREINFO);
        if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 1"); return 0; }
    } else {
        int top = lua_gettop(L);
        lua_pushvalue(L, 1);

        lua_pushstring(L, "texture");
        lua_gettable(L, top+1);
        tmpTexInfo.texture = smlua_to_cpointer(L, lua_gettop(L), LVT_U8_P);
        lua_pop(L, 1);
        if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 1's 'texture' field"); return 0; }

        tmpTexInfo.bitSize = smlua_get_integer_field(top+1, "bitSize");
        if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 1's 'bitSize' field"); return 0; }

        tmpTexInfo.width   = smlua_get_integer_field(top+1, "width");
        if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 1's 'width' field"); return 0; }

        tmpTexInfo.height  = smlua_get_integer_field(top+1, "height");
        if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 1's 'height' field"); return 0; }

        lua_settop(L, top);
    }

    f32 x = smlua_to_number(L, 2);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 2"); return 0; }
    f32 y = smlua_to_number(L, 3);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 3"); return 0; }
    f32 scaleW = smlua_to_number(L, 4);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 4"); return 0; }
    f32 scaleH = smlua_to_number(L, 5);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 5"); return 0; }
    f32 tileX = smlua_to_number(L, 6);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 6"); return 0; }
    f32 tileY = smlua_to_number(L, 7);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 7"); return 0; }
    f32 tileW = smlua_to_number(L, 8);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 8"); return 0; }
    f32 tileH = smlua_to_number(L, 9);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 9"); return 0; }

    djui_hud_render_texture_tile_raw(texInfo->texture, texInfo->bitSize, texInfo->width, texInfo->height, x, y, scaleW, scaleH, tileX, tileY, tileW, tileH);

    return 1;
}

int smlua_func_djui_hud_render_texture_interpolated(lua_State* L) {
    if(!smlua_functions_valid_param_count(L, 9)) { return 0; }

    struct TextureInfo tmpTexInfo = { 0 };
    struct TextureInfo* texInfo = &tmpTexInfo;

    if (smlua_is_cobject(L, 1, LOT_TEXTUREINFO)) {
        texInfo = (struct TextureInfo*)smlua_to_cobject(L, 1, LOT_TEXTUREINFO);
        if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 1"); return 0; }
    } else {
        int top = lua_gettop(L);
        lua_pushvalue(L, 1);

        lua_pushstring(L, "texture");
        lua_gettable(L, top+1);
        tmpTexInfo.texture = smlua_to_cpointer(L, lua_gettop(L), LVT_U8_P);
        lua_pop(L, 1);
        if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 1's 'texture' field"); return 0; }

        tmpTexInfo.bitSize = smlua_get_integer_field(top+1, "bitSize");
        if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 1's 'bitSize' field"); return 0; }

        tmpTexInfo.width   = smlua_get_integer_field(top+1, "width");
        if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 1's 'width' field"); return 0; }

        tmpTexInfo.height  = smlua_get_integer_field(top+1, "height");
        if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 1's 'height' field"); return 0; }

        lua_settop(L, top);
    }

    f32 prevX = smlua_to_number(L, 2);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 2"); return 0; }
    f32 prevY = smlua_to_number(L, 3);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 3"); return 0; }
    f32 prevScaleW = smlua_to_number(L, 4);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 4"); return 0; }
    f32 prevScaleH = smlua_to_number(L, 5);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 5"); return 0; }
    f32 x = smlua_to_number(L, 6);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 6"); return 0; }
    f32 y = smlua_to_number(L, 7);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 7"); return 0; }
    f32 scaleW = smlua_to_number(L, 8);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 8"); return 0; }
    f32 scaleH = smlua_to_number(L, 9);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 9"); return 0; }

    djui_hud_render_texture_interpolated(texInfo, prevX, prevY, prevScaleW, prevScaleH, x, y, scaleW, scaleH);

    return 1;
}

int smlua_func_djui_hud_render_texture_tile_interpolated(lua_State* L) {
    if(!smlua_functions_valid_param_count(L, 13)) { return 0; }

    struct TextureInfo tmpTexInfo = { 0 };
    struct TextureInfo* texInfo = &tmpTexInfo;

    if (smlua_is_cobject(L, 1, LOT_TEXTUREINFO)) {
        texInfo = (struct TextureInfo*)smlua_to_cobject(L, 1, LOT_TEXTUREINFO);
        if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 1"); return 0; }
    } else {
        int top = lua_gettop(L);
        lua_pushvalue(L, 1);

        lua_pushstring(L, "texture");
        lua_gettable(L, top+1);
        tmpTexInfo.texture = smlua_to_cpointer(L, lua_gettop(L), LVT_U8_P);
        lua_pop(L, 1);
        if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 1's 'texture' field"); return 0; }

        tmpTexInfo.bitSize = smlua_get_integer_field(top+1, "bitSize");
        if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 1's 'bitSize' field"); return 0; }

        tmpTexInfo.width   = smlua_get_integer_field(top+1, "width");
        if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 1's 'width' field"); return 0; }

        tmpTexInfo.height  = smlua_get_integer_field(top+1, "height");
        if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 1's 'height' field"); return 0; }

        lua_settop(L, top);
    }

    f32 prevX = smlua_to_number(L, 2);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 2"); return 0; }
    f32 prevY = smlua_to_number(L, 3);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 3"); return 0; }
    f32 prevScaleW = smlua_to_number(L, 4);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 4"); return 0; }
    f32 prevScaleH = smlua_to_number(L, 5);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 5"); return 0; }
    f32 x = smlua_to_number(L, 6);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 6"); return 0; }
    f32 y = smlua_to_number(L, 7);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 7"); return 0; }
    f32 scaleW = smlua_to_number(L, 8);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 8"); return 0; }
    f32 scaleH = smlua_to_number(L, 9);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 9"); return 0; }
    f32 tileX = smlua_to_number(L, 10);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 10"); return 0; }
    f32 tileY = smlua_to_number(L, 11);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 11"); return 0; }
    f32 tileW = smlua_to_number(L, 12);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 12"); return 0; }
    f32 tileH = smlua_to_number(L, 13);
    if (!gSmLuaConvertSuccess) { LOG_LUA("Failed to convert parameter 13"); return 0; }

    djui_hud_render_texture_tile_interpolated(texInfo, prevX, prevY, prevScaleW, prevScaleH, x, y, scaleW, scaleH, tileX, tileY, tileW, tileH);

    return 1;
}

  ////////////////////////////////
 // level script preprocessing //
////////////////////////////////

struct LuaLevelScriptParse {
    int reference;
    struct Mod* mod;
};

struct LuaLevelScriptParse sLevelScriptParse = { 0 };

s32 smlua_func_level_script_parse_callback(u8 type, void *cmd) {
    if (type != 0x24 && type != 0x39 && type != 0x1F) {
        return 0;
    }
    lua_State* L = gLuaState;
    if (L == NULL) { return 0; }
    struct LuaLevelScriptParse* preprocess = &sLevelScriptParse;

    lua_rawgeti(L, LUA_REGISTRYINDEX, preprocess->reference);

    if (type == 0x1F) {
        u8 area = (u8) dynos_level_cmd_get(cmd, 2);
        lua_pushinteger(L, area);
    } else {
        lua_pushnil(L);
    }

    if (type == 0x24) {
        const BehaviorScript *bhv = (const BehaviorScript *) dynos_level_cmd_get(cmd, 20);
        u32 behaviorArg = (u32) dynos_level_cmd_get(cmd, 16);

        lua_newtable(L);

        lua_pushstring(L, "behavior");
        lua_pushinteger(L, get_id_from_behavior(bhv));
        lua_settable(L, -3);

        lua_pushstring(L, "behaviorArg");
        lua_pushinteger(L, behaviorArg);
        lua_settable(L, -3);
    } else {
        lua_pushnil(L);
    }

    if (type == 0x39) {
        MacroObject *data = (MacroObject *) dynos_level_cmd_get(cmd, 4);
        int i = 0;
        s32 len = 0;

        lua_newtable(L);
        int t = lua_gettop(gLuaState);

        lua_newtable(L);
        int args = lua_gettop(gLuaState);
        while (data[len++] != MACRO_OBJECT_END()) {
            s32 presetId = (s32) ((data[len - 1] & 0x1FF) - 0x1F);
            const BehaviorScript *bhv = (const BehaviorScript *) MacroObjectPresets[presetId].behavior;
            s32 presetParams = MacroObjectPresets[presetId].param;
            s32 objParams = (data[4] & 0xFF00) + (presetParams & 0x00FF);
            u32 behaviorArg = ((objParams & 0x00FF) << 16) + (objParams & 0xFF00);

            lua_pushinteger(L, i);
            lua_pushinteger(L, get_id_from_behavior(bhv));
            lua_settable(L, t);

            lua_pushinteger(L, i);
            lua_pushinteger(L, behaviorArg);
            lua_settable(L, args);

            i++;
            len += 4;
        }
    } else {
        lua_pushnil(L);
        lua_pushnil(L);
    }

    // call the callback
    if (0 != smlua_call_hook(L, 4, 0, 0, preprocess->mod)) {
        LOG_LUA("Failed to call the callback behaviors: %u", type);
        return 0;
    }
}

void smlua_func_level_script_parse(lua_State* L) {
    if (!smlua_functions_valid_param_count(L, 2)) { return; }

    lua_Integer levelNum = smlua_to_integer(L, 1);
    if (!gSmLuaConvertSuccess) {
        LOG_LUA_LINE("Invalid level script name");
        return;
    }

    struct LuaLevelScriptParse* preprocess = &sLevelScriptParse;
    preprocess->reference = LUA_NOREF;

    lua_pushvalue(L, 2);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    if (ref == -1) {
        LOG_LUA_LINE("Level Script Parse: %lld tried to parse using undefined function", levelNum);
        return;
    }

    preprocess->reference = ref;
    preprocess->mod = gLuaActiveMod;

    void *script = dynos_level_get_script(levelNum);
    if (script == NULL) {
        LOG_LUA("Failed to find script: %lld", levelNum);
        return;
    }
    dynos_level_parse_script(script, smlua_func_level_script_parse_callback);
}

  //////////
 // bind //
//////////

void smlua_bind_functions(void) {
    lua_State* L = gLuaState;

    // misc
    smlua_bind_function(L, "sins", smlua_func_sins);
    smlua_bind_function(L, "coss", smlua_func_coss);
    smlua_bind_function(L, "atan2s", smlua_func_atan2s);
    smlua_bind_function(L, "init_mario_after_warp", smlua_func_init_mario_after_warp);
    smlua_bind_function(L, "initiate_warp", smlua_func_initiate_warp);
    smlua_bind_function(L, "network_init_object", smlua_func_network_init_object);
    smlua_bind_function(L, "network_send_object", smlua_func_network_send_object);
    smlua_bind_function(L, "reset_level", smlua_func_reset_level);
    smlua_bind_function(L, "network_send", smlua_func_network_send);
    smlua_bind_function(L, "network_send_to", smlua_func_network_send_to);
    smlua_bind_function(L, "get_texture_info", smlua_func_get_texture_info);
    smlua_bind_function(L, "djui_hud_render_texture", smlua_func_djui_hud_render_texture);
    smlua_bind_function(L, "djui_hud_render_texture_tile", smlua_func_djui_hud_render_texture_tile);
    smlua_bind_function(L, "djui_hud_render_texture_interpolated", smlua_func_djui_hud_render_texture_interpolated);
    smlua_bind_function(L, "djui_hud_render_texture_tile_interpolated", smlua_func_djui_hud_render_texture_tile_interpolated);
    smlua_bind_function(L, "level_script_parse", smlua_func_level_script_parse);
}
