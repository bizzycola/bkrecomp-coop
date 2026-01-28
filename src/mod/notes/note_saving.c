// #include "patches.h"
// 
#include "misc_funcs.h"
// #include "save_extension.h"
#include "note_saving.h"

#include "functions.h"
#include "prop.h"
#include "core2/timedfunc.h"

#include "modding.h"
// #include "bk_api.h"

// Static variable - only accessible within this file
static PropExtensionId note_saving_prop_extension_id;

// Accessor function for other files to get the extension ID
PropExtensionId get_note_saving_prop_extension_id() {
    return note_saving_prop_extension_id;
}


typedef struct map_info{
    s16 map_id;
    s16 level_id;
    char* name;
}MapInfo;

extern struct {
    s32 unk0;
    s32 unk4;
    u8 unk8[0x25];
} gFileProgressFlags;

MapInfo * func_8030AD00(enum map_e map_id);
enum map_e map_get(void);
enum level_e level_get(void);
void progressDialog_showDialogMaskZero(s32);
void fxSparkle_musicNote(s16 position[3]);
void item_inc(enum item_e item);
void item_adjustByDiffWithoutHud(enum item_e item, s32 diff);
bool func_802FADD4(enum item_e item_id);
s32 item_getCount(enum item_e item);
s32 jiggyscore_leveltotal(s32 lvl);
void itemPrint_reset(void);
s32 bitfield_get_bit(u8 *array, s32 index);

extern s32 D_80385F30[0x2C];
extern s32 D_80385FE8;

typedef struct {
    u8 bytes[32];
} LevelNotes;
typedef struct {
    LevelNotes level_notes[9];
    u8 padding[32]; // Reserved for future use.
} SaveFileExtensionData;

static SaveFileExtensionData loaded_file_extension_data;

extern struct {
    Cube *cubes;
    f32 margin;
    s32 min[3];
    s32 max[3];
    s32 stride[2];
    s32 cubeCnt; 
    s32 unk2C;
    s32 width[3];
    Cube *unk3C; // fallback cube?
    Cube *unk40; // some other fallback cube?
    s32 unk44; // index of some sort
} sCubeList;

// New declarations.
typedef struct {
    u16 static_note_count;
    u16 dynamic_note_count;
    u16 start_note_index;
    u16 level_id;
} MapNoteData;

MapNoteData map_note_data[512]; // One entry per map, with room for extras in case any mods add additional maps.
u16 level_note_counts[256]; // One entry per level, with room for extras in case any mods add additional levels.


// Note saving can only savely be changed while in the lair, so this value is only updated when in the lair.
bool note_saving_enabled_cached = FALSE;
// Override for allowing mods to disable note saving.
bool note_saving_override_disabled = FALSE;

u32 spawned_static_note_count = 0;

// Per-map values containing the number of dynamic notes to despawn for the current session.
// This is calculated when a new level is entered for every map in the level.
u8 map_dynamic_note_despawn_counts[512];

static bool recomp_in_demo_playback_game_mode();

RECOMP_IMPORT("*", s32 bkrecomp_note_saving_active());
RECOMP_IMPORT("*", s32 bkrecomp_note_saving_enabled());

// import bkrecomp_extend_prop_all
RECOMP_IMPORT("*", PropExtensionId bkrecomp_extend_prop_all(size_t size));

void init_note_saving() {
    note_saving_prop_extension_id = bkrecomp_extend_prop_all(sizeof(NoteSavingExtensionData));

    // Collected from map data.
    map_note_data[MAP_2_MM_MUMBOS_MOUNTAIN].static_note_count = 85;
    map_note_data[MAP_2_MM_MUMBOS_MOUNTAIN].dynamic_note_count = 5;
    map_note_data[MAP_5_TTC_BLUBBERS_SHIP].static_note_count = 8;
    map_note_data[MAP_6_TTC_NIPPERS_SHELL].static_note_count = 6;
    map_note_data[MAP_7_TTC_TREASURE_TROVE_COVE].static_note_count = 82;
    map_note_data[MAP_A_TTC_SANDCASTLE].static_note_count = 4;
    map_note_data[MAP_B_CC_CLANKERS_CAVERN].static_note_count = 72;
    map_note_data[MAP_C_MM_TICKERS_TOWER].static_note_count = 6;
    map_note_data[MAP_D_BGS_BUBBLEGLOOP_SWAMP].static_note_count = 83;
    map_note_data[MAP_D_BGS_BUBBLEGLOOP_SWAMP].dynamic_note_count = 5;
    map_note_data[MAP_E_MM_MUMBOS_SKULL].static_note_count = 4;
    map_note_data[MAP_10_BGS_MR_VILE].static_note_count = 6;
    map_note_data[MAP_11_BGS_TIPTUP].static_note_count = 6;
    map_note_data[MAP_12_GV_GOBIS_VALLEY].static_note_count = 70;
    map_note_data[MAP_13_GV_MEMORY_GAME].static_note_count = 4;
    map_note_data[MAP_14_GV_SANDYBUTTS_MAZE].static_note_count = 7;
    map_note_data[MAP_15_GV_WATER_PYRAMID].static_note_count = 4;
    map_note_data[MAP_16_GV_RUBEES_CHAMBER].static_note_count = 8;
    map_note_data[MAP_1A_GV_INSIDE_JINXY].static_note_count = 7;
    map_note_data[MAP_1B_MMM_MAD_MONSTER_MANSION].static_note_count = 47;
    map_note_data[MAP_1C_MMM_CHURCH].static_note_count = 10;
    map_note_data[MAP_1D_MMM_CELLAR].static_note_count = 4;
    map_note_data[MAP_21_CC_WITCH_SWITCH_ROOM].static_note_count = 6;
    map_note_data[MAP_22_CC_INSIDE_CLANKER].static_note_count = 16;
    map_note_data[MAP_23_CC_GOLDFEATHER_ROOM].static_note_count = 6;
    map_note_data[MAP_24_MMM_TUMBLARS_SHED].static_note_count = 4;
    map_note_data[MAP_25_MMM_WELL].static_note_count = 7;
    map_note_data[MAP_26_MMM_NAPPERS_ROOM].static_note_count = 8;
    map_note_data[MAP_27_FP_FREEZEEZY_PEAK].static_note_count = 82;
    map_note_data[MAP_29_MMM_NOTE_ROOM].static_note_count = 9;
    map_note_data[MAP_2D_MMM_BEDROOM].static_note_count = 4;
    map_note_data[MAP_2F_MMM_WATERDRAIN_BARREL].static_note_count = 5;
    map_note_data[MAP_30_MMM_MUMBOS_SKULL].static_note_count = 2;
    map_note_data[MAP_31_RBB_RUSTY_BUCKET_BAY].static_note_count = 43;
    map_note_data[MAP_34_RBB_ENGINE_ROOM].static_note_count = 16;
    map_note_data[MAP_35_RBB_WAREHOUSE].static_note_count = 4;
    map_note_data[MAP_37_RBB_CONTAINER_1].static_note_count = 8;
    map_note_data[MAP_38_RBB_CONTAINER_3].static_note_count = 4;
    map_note_data[MAP_39_RBB_CREW_CABIN].static_note_count = 4;
    map_note_data[MAP_3B_RBB_STORAGE_ROOM].static_note_count = 5;
    map_note_data[MAP_3C_RBB_KITCHEN].static_note_count = 5;
    map_note_data[MAP_3D_RBB_NAVIGATION_ROOM].static_note_count = 4;
    map_note_data[MAP_3F_RBB_CAPTAINS_CABIN].static_note_count = 3;
    map_note_data[MAP_40_CCW_HUB].static_note_count = 4;
    map_note_data[MAP_43_CCW_SPRING].static_note_count = 16;
    map_note_data[MAP_44_CCW_SUMMER].static_note_count = 16;
    map_note_data[MAP_45_CCW_AUTUMN].static_note_count = 37;
    map_note_data[MAP_46_CCW_WINTER].static_note_count = 16;
    map_note_data[MAP_48_FP_MUMBOS_SKULL].static_note_count = 6;
    map_note_data[MAP_4C_CCW_AUTUMN_MUMBOS_SKULL].static_note_count = 4;
    map_note_data[MAP_53_FP_CHRISTMAS_TREE].static_note_count = 12;
    map_note_data[MAP_5C_CCW_AUTUMN_ZUBBA_HIVE].static_note_count = 4;
    map_note_data[MAP_60_CCW_AUTUMN_NABNUTS_HOUSE].static_note_count = 3;
    map_note_data[MAP_8B_RBB_ANCHOR_ROOM].static_note_count = 4;
}

void calculate_map_start_note_indices() {
    for (u32 map_id = 0; map_id < ARRLEN(map_note_data); map_id++) {
        MapNoteData* note_data = &map_note_data[map_id];
        MapInfo* info = func_8030AD00(map_id);

        if (info != NULL) {
            note_data->level_id = info->level_id;
            note_data->start_note_index = level_note_counts[info->level_id];
            level_note_counts[info->level_id] += note_data->static_note_count + note_data->dynamic_note_count;
        }
        else {
            note_data->level_id = 0;
        }
    }
}

s32 level_id_to_level_array_index(enum level_e level_id) {
    switch (level_id) {
        case LEVEL_1_MUMBOS_MOUNTAIN:
            return 0;
        case LEVEL_2_TREASURE_TROVE_COVE:
            return 1;
        case LEVEL_3_CLANKERS_CAVERN:
            return 2;
        case LEVEL_4_BUBBLEGLOOP_SWAMP:
            return 3;
        case LEVEL_5_FREEZEEZY_PEAK:
            return 4;
        case LEVEL_7_GOBIS_VALLEY:
            return 5;
        case LEVEL_A_MAD_MONSTER_MANSION:
            return 6;
        case LEVEL_9_RUSTY_BUCKET_BAY:
            return 7;
        case LEVEL_8_CLICK_CLOCK_WOOD:
            return 8;
        default:
            return -1;
    }
}

bool is_note_collected(enum map_e map_id, enum level_e level_id, u8 note_index) {
    if (map_id >= ARRLEN(map_note_data)) {
        return FALSE;
    }

    s32 level_array_index = level_id_to_level_array_index(level_id);
    if (level_array_index == -1 || level_array_index >= ARRLEN(loaded_file_extension_data.level_notes)) {
        return FALSE;
    }

    MapNoteData *note_data = &map_note_data[map_id];
    note_index += note_data->start_note_index;

    u32 byte_index = note_index / 8;
    u32 bit_index = note_index % 8;

    return (loaded_file_extension_data.level_notes[level_array_index].bytes[byte_index] & (1 << bit_index)) != 0;
}

void set_note_collected(enum map_e map_id, enum level_e level_id, u8 note_index) {
    if (map_id >= ARRLEN(map_note_data)) {
        return;
    }

    s32 level_array_index = level_id_to_level_array_index(level_id);
    if (level_array_index == -1 || level_array_index >= ARRLEN(loaded_file_extension_data.level_notes)) {
        return;
    }

    MapNoteData *note_data = &map_note_data[map_id];
    note_index += note_data->start_note_index;

    u32 byte_index = note_index / 8;
    u32 bit_index = note_index % 8;

    loaded_file_extension_data.level_notes[level_array_index].bytes[byte_index] |= (1 << bit_index);
}

void collect_dynamic_note(enum map_e map_id, enum level_e level_id) {
    if (map_id < ARRLEN(map_note_data)) {
        MapNoteData *map_data = &map_note_data[map_id];
        s32 start_note_index = map_data->static_note_count + map_data->start_note_index;
        s32 map_dynamic_note_count = map_data->dynamic_note_count;

        // Set the first unset dynamic note bit for this map.
        for (s32 i = 0; i < map_dynamic_note_count; i++) {
            s32 note_index = start_note_index + i;
            if (!is_note_collected(map_id, level_id, note_index)) {
                set_note_collected(map_id, level_id, note_index);
                break;
            }
        }
    }
}

s32 dynamic_note_collected_count(enum map_e map_id) {
    s32 ret = 0;
    if (map_id < ARRLEN(map_note_data)) {
        MapNoteData *map_data = &map_note_data[map_id];
        s32 start_note_index = map_data->static_note_count + map_data->start_note_index;
        s32 map_dynamic_note_count = map_data->dynamic_note_count;

        // Check the dynamic note bits for this map.
        for (s32 i = 0; i < map_dynamic_note_count; i++) {
            s32 note_index = start_note_index + i;
            if (is_note_collected(map_id, map_data->level_id, note_index)) {
                ret++;
            }
        }
    }
    return ret;
}

void note_saving_on_map_load() {
    spawned_static_note_count = 0;

    // Prevent the note score passed dialog from running if note saving is enabled.
    if (note_saving_enabled_cached) {
        // This flag controls whether Bottles will tell you when you pass your note score.
        levelSpecificFlags_set(LEVEL_FLAG_34_UNKNOWN, TRUE);
    }
}

void note_saving_update() {
    // When in the lair or file select, update the cached note saving enabled state.
    if (level_get() == LEVEL_6_LAIR || map_get() == MAP_91_FILE_SELECT) {
        if (note_saving_override_disabled) {
            note_saving_enabled_cached = FALSE;
        }
        else {
            note_saving_enabled_cached = bkrecomp_note_saving_enabled();
        }
    }
}

void note_saving_handle_static_note(Cube *c, Prop *p) {

    // If note saving is enabled, check if this note has been collected and remove it if so.
    if (note_saving_enabled_cached) {
        if (is_note_collected(map_get(), level_get(), spawned_static_note_count)) {
            // Clear the note's alive bit.
            p->spriteProp.unk8_4 = FALSE;
        }
    }

    NoteSavingExtensionData* note_data = (NoteSavingExtensionData*)bkrecomp_get_extended_prop_data(c, p, note_saving_prop_extension_id);
    note_data->note_index = spawned_static_note_count;

    spawned_static_note_count++;
}

void note_saving_handle_dynamic_note(Actor *actor, ActorMarker *marker) {
    if (note_saving_enabled_cached) {
        s32 map_id = map_get();
        if (map_id < ARRLEN(map_dynamic_note_despawn_counts)) {
            if (map_dynamic_note_despawn_counts[map_id] > 0) {
                map_dynamic_note_despawn_counts[map_id]--;
                // Clear the note's alive bit so it doesn't draw for good measure.
                marker->propPtr->unk8_4 = FALSE;
                // Set the actor as despawned.
                actor->despawn_flag = TRUE;
            }
        }
    }
}

bool prop_in_cube(Cube *c, Prop *p) {
    s32 prop_index = p - c->prop2Ptr;
    if (prop_index >= 0 && prop_index < c->prop2Cnt) {
        return TRUE;
    }
    return FALSE;
}

Cube *find_cube_for_prop(Prop *p) {
    for (s32 i = 0; i < sCubeList.cubeCnt; i++) {
        Cube *cur_cube = &sCubeList.cubes[i];
        if (prop_in_cube(cur_cube, p)) {
            return cur_cube;
        }
    }
    
    if (prop_in_cube(sCubeList.unk3C, p)) {
        return sCubeList.unk3C;
    }

    if (prop_in_cube(sCubeList.unk40, p)) {
        return sCubeList.unk40;
    }

    return NULL;
}



// RECOMP_HOOK("__code7AF80_initCubeFromFile") void handleCubeInit(Cube *cube, File* file_ptr) {
//     recomp_printf("Handling cube init for note saving\n");
//     recomp_printf(" - Cube has %u props\n", cube->prop2Cnt);
//     for (u32 i = 0; i < cube->prop2Cnt; i++) {
//         Prop *prop = &cube->prop2Ptr[i];
//         recomp_printf("Handling prop %u/%u\n", i, cube->prop2Cnt);
//         if (!prop->is_3d && !prop->is_actor) {
//             recomp_printf(" - Prop is not 3D and not actor\n");

//             enum asset_e sprite_prop_asset_id;
//             func_8032DE78(&prop->spriteProp, &sprite_prop_asset_id);

//             recomp_printf(" - Prop asset ID: 0x%X\n", sprite_prop_asset_id);

//             if (sprite_prop_asset_id == ASSET_6D6_MODEL_MUSIC_NOTE) {
//                 recomp_printf(" - Prop is a static note, handling\n");
//                 note_saving_handle_static_note(cube, prop);
//             }
//         }
//     }
// }

// This stuff is hooked to setup node indexes correctly when entering a level
// We run through it once when the function is called and store the cubes
// and wait for the base patch to init the cubes
// Then we run through the initalised cubes and setup the notes
static Cube *pending_cube = NULL;
static File *pending_file = NULL;

RECOMP_HOOK("__code7AF80_initCubeFromFile") void handleCubeInit_entry(Cube *cube, File *file_ptr) {
    pending_cube = cube;
    pending_file = file_ptr;
}

RECOMP_HOOK_RETURN("__code7AF80_initCubeFromFile") void handleCubeInit_return(void) {
    if (pending_cube == NULL) {
        return;
    }
    
    Cube *cube = pending_cube;
    pending_cube = NULL;
    pending_file = NULL;
    
    for (u32 i = 0; i < cube->prop2Cnt; i++) {
        Prop *prop = &cube->prop2Ptr[i];

        if (!prop->is_3d && !prop->is_actor) {
            enum asset_e sprite_prop_asset_id;
            func_8032DE78(&prop->spriteProp, &sprite_prop_asset_id);

            if (sprite_prop_asset_id == ASSET_6D6_MODEL_MUSIC_NOTE) {
                note_saving_handle_static_note(cube, prop);
            }
        }
    }
}
