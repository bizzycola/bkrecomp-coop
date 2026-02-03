use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::time::Instant;

// position value tolerance
const POS_TOLERANCE: i16 = 10;

/**
 * Keeps all the information for a collected note
 * (map, index, position, the cutie who collected it and the timestamp)
 */
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CollectedNote {
    pub map_id: i32,
    pub x: i16,
    pub y: i16,
    pub z: i16,
    pub collected_by: String,
    pub timestamp: i64,
}

/**
 * Keeps all the information for a collected Jiggy
 * (level id, jiggy id, the cutie who collected it and the timestamp)
 */
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CollectedJiggy {
    pub level_id: i32,
    pub jiggy_id: i32,
    pub collected_by: String,
    pub timestamp: i64,
}

/**
 * Keeps all the information for a collected honeycomb
 */
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CollectedHoneycomb {
    pub map_id: i32,
    pub honeycomb_id: i32,
    pub x: i32,
    pub y: i32,
    pub z: i32,
    pub collected_by: String,
    pub timestamp: i64,
}

/**
 * Keeps all the information for a collected Mumbo token
 */
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CollectedMumboToken {
    pub map_id: i32,
    pub token_id: i32,
    pub x: i32,
    pub y: i32,
    pub z: i32,
    pub collected_by: String,
    pub timestamp: i64,
}

/**
 * Keeps information for an unlocked level
 * (world id, jiggy cost, player who spent all our jiggies, timestamp)
 */
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OpenedLevel {
    pub world_id: i32,
    pub jiggy_cost: i32,
    pub opened_by: String,
    pub timestamp: i64,
}

/**
 * Holds savefile flags.
 * (Not listing them, there's too many, but trust me they're useful)
 */
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SaveFlags {
    /**
     * Save flags for unlocked cheat codes
     */
    pub cheat_flags: Vec<u8>,

    /**
     * General game flags
     */
    pub game_flags: Vec<u8>,

    /**
     * Honeycomb flags
     */
    pub honeycomb_flags: Vec<u8>,

    /**
     * Collected jiggy flags
     */
    pub jiggy_flags: Vec<u8>,

    /**
     * Collected mumbo token flags
     */
    pub token_flags: Vec<u8>,

    /**
     * Note totals
     */
    pub note_totals: Vec<u8>,

    /**
     * Completed puzzles
     */
    pub puzzles_completed: Vec<u8>,

    /**
     * Level event flags
     */
    pub level_events: u32,

    /**
     * Full fileProgressFlag blob (BK decomp) used for many progression gates.
     * Expected size: 0x25 bytes.
     */
    pub file_progress_flags: Vec<u8>,

    /**
     * Unlocked moves
     */
    pub moves: u32,

    /**
     * Raw abilityprogress blob (BK decomp). Expected size: 8 bytes.
     */
    pub ability_progress: Vec<u8>,

    /**
     * Raw honeycombscore blob (BK decomp). Expected size: 0x03 bytes.
     */
    pub honeycomb_score: Vec<u8>,

    /**
     * Raw mumboscore blob (BK decomp). Expected size: 0x10 bytes.
     */
    pub mumbo_score: Vec<u8>,

    /**
     * Specific individually collected notes
     */
    pub note_save_data: Vec<Vec<u8>>,
}

impl Default for SaveFlags {
    fn default() -> Self {
        Self {
            cheat_flags: vec![0; 0x19],
            game_flags: vec![0; 0x20],
            honeycomb_flags: vec![0; 0x03],
            jiggy_flags: vec![0; 0x0d],
            token_flags: vec![0; 0x10],
            note_totals: vec![0; 0x0f],
            puzzles_completed: vec![0; 11],
            level_events: 0,
            file_progress_flags: vec![0; 0x25],
            moves: 0,
            ability_progress: vec![0; 8],
            honeycomb_score: vec![0; 0x03],
            mumbo_score: vec![0; 0x10],
            note_save_data: vec![vec![0; 32]; 9],
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Lobby {
    /**
     * Human friendly (stay away, robots!) lobby name
     */
    pub name: String,

    /**
     * Top secret lobby password
     */
    pub password: String,

    /**
     * Date of the birth of this lobby
     */
    pub created_at: i64,

    /**
     * Time of last packet activity on this lobby
     */
    pub last_activity: i64,

    /**
     * Whether the first player has joined and synced their game state
     * to the lobby or not
     */
    pub has_initial_save_data: bool,

    /**
     * Players in the lobby
     */
    #[serde(skip)]
    pub players: HashMap<u32, String>,

    /**
     * Collected notes
     */
    pub collected_notes: Vec<CollectedNote>,

    /**
     * Collected jiggies
     */
    pub collected_jiggies: Vec<CollectedJiggy>,

    /**
     * Opened worlds
     */
    pub opened_levels: Vec<OpenedLevel>,

    /**
     * Collected honeycombs (event-based, for deterministic despawn)
     */
    pub collected_honeycombs: Vec<CollectedHoneycomb>,

    /**
     * Collected Mumbo tokens (event-based, for deterministic despawn)
     */
    pub collected_mumbo_tokens: Vec<CollectedMumboToken>,

    /**
     * General savefile flags
     */
    pub save_flags: SaveFlags,
}

impl Lobby {
    pub fn new(name: String, password: String) -> Self {
        let now = chrono::Utc::now().timestamp();

        Self {
            name,
            password,
            created_at: now,
            last_activity: now,
            has_initial_save_data: false,
            players: HashMap::new(),
            collected_jiggies: Vec::new(),
            collected_notes: Vec::new(),
            opened_levels: Vec::new(),
            collected_honeycombs: Vec::new(),
            collected_mumbo_tokens: Vec::new(),
            save_flags: SaveFlags::default(),
        }
    }

    pub fn is_honeycomb_collected(&self, map_id: i32, honeycomb_id: i32) -> bool {
        self.collected_honeycombs
            .iter()
            .any(|h| h.map_id == map_id && h.honeycomb_id == honeycomb_id)
    }

    pub fn add_collected_honeycomb(
        &mut self,
        map_id: i32,
        honeycomb_id: i32,
        x: i32,
        y: i32,
        z: i32,
        collected_by: String,
    ) -> bool {
        if self.is_honeycomb_collected(map_id, honeycomb_id) {
            return false;
        }

        self.collected_honeycombs.push(CollectedHoneycomb {
            map_id,
            honeycomb_id,
            x,
            y,
            z,
            collected_by,
            timestamp: chrono::Utc::now().timestamp(),
        });

        self.update_activity();
        true
    }

    pub fn is_mumbo_token_collected(&self, map_id: i32, token_id: i32) -> bool {
        self.collected_mumbo_tokens
            .iter()
            .any(|t| t.map_id == map_id && t.token_id == token_id)
    }

    pub fn add_collected_mumbo_token(
        &mut self,
        map_id: i32,
        token_id: i32,
        x: i32,
        y: i32,
        z: i32,
        collected_by: String,
    ) -> bool {
        if self.is_mumbo_token_collected(map_id, token_id) {
            return false;
        }

        self.collected_mumbo_tokens.push(CollectedMumboToken {
            map_id,
            token_id,
            x,
            y,
            z,
            collected_by,
            timestamp: chrono::Utc::now().timestamp(),
        });

        self.update_activity();
        true
    }

    pub fn update_activity(&mut self) {
        self.last_activity = chrono::Utc::now().timestamp();
    }

    pub fn is_idle(&self, timeout_secs: u64) -> bool {
        let now = chrono::Utc::now().timestamp();

        (now - self.last_activity) as u64 >= timeout_secs
    }

    pub fn add_player(&mut self, player_id: u32, username: String) {
        self.players.insert(player_id, username);
        self.update_activity();
    }

    pub fn remove_player(&mut self, player_id: u32) {
        self.players.remove(&player_id);
        self.update_activity();
    }

    pub fn player_count(&self) -> usize {
        self.players.len()
    }

    pub fn is_note_collect(&self, map_id: i32, x: i16, y: i16, z: i16) -> bool {
        self.collected_notes.iter().any(|note| {
            note.map_id == map_id
                && (note.x - x).abs() <= POS_TOLERANCE
                && (note.y - y).abs() <= POS_TOLERANCE
                && (note.z - z).abs() <= POS_TOLERANCE
        })
    }

    pub fn add_collected_note(
        &mut self,
        map_id: i32,
        x: i16,
        y: i16,
        z: i16,
        collected_by: String,
    ) -> bool {
        // Collecting thirty of the same note?
        // not in this economy
        if self.is_note_collect(map_id, x, y, z) {
            return false;
        }

        self.collected_notes.push(CollectedNote {
            map_id,
            x,
            y,
            z,
            collected_by,
            timestamp: chrono::Utc::now().timestamp(),
        });

        self.update_activity();
        true
    }

    pub fn is_jiggy_collected(&self, jiggy_enum_id: i32, collected_value: i32) -> bool {
        self.collected_jiggies
            .iter()
            .any(|j| j.level_id == jiggy_enum_id && j.jiggy_id == collected_value)
    }

    pub fn add_collected_jiggy(
        &mut self,
        jiggy_enum_id: i32,
        collected_value: i32,
        collected_by: String,
    ) -> bool {
        if self.is_jiggy_collected(jiggy_enum_id, collected_value) {
            return false;
        }

        self.collected_jiggies.push(CollectedJiggy {
            level_id: jiggy_enum_id,
            jiggy_id: collected_value,
            collected_by,
            timestamp: chrono::Utc::now().timestamp(),
        });

        self.update_activity();
        true
    }

    pub fn is_level_opened(&self, world_id: i32) -> bool {
        self.opened_levels.iter().any(|l| l.world_id == world_id)
    }

    pub fn add_opened_level(&mut self, world_id: i32, jiggy_cost: i32) -> bool {
        if self.is_level_opened(world_id) {
            return false;
        }

        self.opened_levels.push(OpenedLevel {
            world_id,
            jiggy_cost,
            opened_by: String::from("JiggyWiggy"), // todo: add user to packet
            timestamp: chrono::Utc::now().timestamp(),
        });

        self.update_activity();
        true
    }

    pub fn merge_flags(dest: &mut Vec<u8>, src: &[u8]) -> bool {
        let mut changed = false;
        let len = dest.len().min(src.len());

        for i in 0..len {
            let old = dest[i];
            dest[i] |= src[i];
            if dest[i] != old {
                changed = true;
            }
        }

        changed
    }

    pub fn update_save_flags(&mut self, flag_type: &str, data: &[u8]) -> bool {
        let changed = match flag_type {
            "cheat" => Self::merge_flags(&mut self.save_flags.cheat_flags, data),
            "game" => Self::merge_flags(&mut self.save_flags.game_flags, data),
            "honeycomb" => Self::merge_flags(&mut self.save_flags.honeycomb_flags, data),
            "jiggy" => Self::merge_flags(&mut self.save_flags.jiggy_flags, data),
            "token" => Self::merge_flags(&mut self.save_flags.token_flags, data),
            "note_totals" => {
                let mut changed = false;
                let len = self.save_flags.note_totals.len().min(data.len());

                for i in 0..len {
                    let old = self.save_flags.note_totals[i];

                    self.save_flags.note_totals[i] = self.save_flags.note_totals[i].max(data[i]);

                    if self.save_flags.note_totals[i] != old {
                        changed = true
                    }
                }

                changed
            }
            _ => false,
        };

        if changed {
            self.update_activity();
        }

        changed
    }

    pub fn update_note_save_data(&mut self, level_index: usize, data: &[u8]) -> bool {
        if level_index >= 9 || data.len() != 32 {
            return false;
        }

        let mut changed = false;
        for i in 0..32 {
            let old = self.save_flags.note_save_data[level_index][i];
            self.save_flags.note_save_data[level_index][i] |= data[i];

            if self.save_flags.note_save_data[level_index][i] != old {
                changed = true
            }
        }

        if changed {
            self.update_activity();
        }

        changed
    }

    pub fn update_file_progress_flags(&mut self, data: &[u8]) -> bool {
        let changed = Self::merge_flags(&mut self.save_flags.file_progress_flags, data);

        if changed {
            self.update_activity();
        }

        changed
    }

    pub fn update_ability_progress(&mut self, data: &[u8]) -> bool {
        let changed = Self::merge_flags(&mut self.save_flags.ability_progress, data);

        if changed {
            self.update_activity();
        }

        changed
    }

    pub fn update_honeycomb_score(&mut self, data: &[u8]) -> bool {
        let changed = Self::merge_flags(&mut self.save_flags.honeycomb_score, data);

        if changed {
            self.update_activity();
        }

        changed
    }

    pub fn update_mumbo_score(&mut self, data: &[u8]) -> bool {
        let changed = Self::merge_flags(&mut self.save_flags.mumbo_score, data);

        if changed {
            self.update_activity();
        }

        changed
    }
}
