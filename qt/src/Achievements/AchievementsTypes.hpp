#pragma once

// Plain data shared between AchievementsClient and its UI (dialogs talk to
// EmuApplication's thin wrappers, never to rc_client_t directly -- these
// structs are the entire contract between the two sides).

#include <cstdint>
#include <string>
#include <vector>

namespace Achievements
{

struct UserInfo
{
    bool logged_in = false;
    std::string display_name;
    std::string username;
    // Persisted by EmuConfig for auto re-login; never the password.
    std::string token;
    uint32_t score = 0;
    uint32_t score_softcore = 0;
};

struct GameSummary
{
    bool game_loaded = false;
    std::string title;
    uint32_t num_core_achievements = 0;
    uint32_t num_unlocked_achievements = 0;
};

struct AchievementEntry
{
    uint32_t id = 0;
    std::string title;
    std::string description;
    uint32_t points = 0;
    bool unlocked = false;
};

} // namespace Achievements
