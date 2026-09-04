#include "AchievementsClient.hpp"
#include "AchievementsNetwork.hpp"

#include <algorithm>
#include <cstring>

#include <QMetaObject>
#include <QSysInfo>

#include "display.h"
#include "memmap.h"
#include "rc_api_runtime.h"
#include "rc_consoles.h"
#include "snes9x.h"

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

// Opaque token handed to AchievementsNetwork::dispatch() and returned
// unchanged in enqueueCompletedResponse() -- see the class comment in
// AchievementsClient.hpp for why this round-trips through the GUI thread.
struct AchievementsClient::PendingCall
{
    rc_client_server_callback_t callback;
    void *callback_data;
};

AchievementsClient::AchievementsClient()
{
    client_ = rc_client_create(&AchievementsClient::readMemory, &AchievementsClient::serverCall);
    rc_client_set_userdata(client_, this);
    rc_client_set_event_handler(client_, &AchievementsClient::eventHandler);
    rc_client_enable_logging(client_, RC_CLIENT_LOG_LEVEL_WARN, &AchievementsClient::logMessage);

    // MVP is softcore-only: hardcore mode requires gating rewind, cheats,
    // save states, and netplay resync, which is out of scope for this pass.
    rc_client_set_hardcore_enabled(client_, 0);

    network_ = std::make_unique<AchievementsNetwork>(this);
}

AchievementsClient::~AchievementsClient()
{
    if (client_)
        rc_client_destroy(client_);
}

void AchievementsClient::doFrame()
{
    drainCompletedResponses();
    if (client_ && rc_client_is_game_loaded(client_))
        rc_client_do_frame(client_);
}

void AchievementsClient::idle()
{
    drainCompletedResponses();
    if (client_)
        rc_client_idle(client_);
}

void AchievementsClient::drainCompletedResponses()
{
    std::vector<CompletedResponse> ready;
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        ready.swap(pending_responses_);
    }

    for (auto &entry : ready)
    {
        rc_api_server_response_t response{};
        if (entry.http_status != 0)
        {
            response.http_status_code = entry.http_status;
            response.body = entry.body.c_str();
            response.body_length = entry.body.size();
        }
        else
        {
            /* Request never reached the server (DNS/connection failure, etc). */
            response.http_status_code = RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR;
            response.body = entry.error.c_str();
            response.body_length = entry.error.size();
        }

        entry.pending->callback(&response, entry.pending->callback_data);
        delete entry.pending;
    }

    PendingBadgeImage badge;
    bool have_badge = false;
    {
        std::lock_guard<std::mutex> lock(badge_mutex_);
        if (badge_pending_)
        {
            badge = std::move(pending_badge_);
            have_badge = true;
            badge_pending_ = false;
        }
    }
    if (have_badge)
    {
        S9xSetInfoImage(badge.rgba.data(), badge.width, badge.height);
        std::string url = badge.url;
        badge_cache_[url] = std::move(badge);
    }
}

void AchievementsClient::enqueueBadgeImage(std::string url, std::vector<uint8_t> rgba, int width, int height)
{
    std::lock_guard<std::mutex> lock(badge_mutex_);
    pending_badge_ = PendingBadgeImage{std::move(url), std::move(rgba), width, height};
    badge_pending_ = true;
}

void AchievementsClient::enqueueCompletedResponse(int http_status, const std::string &body,
                                                   const std::string &error, void *request_handle)
{
    auto *pending = static_cast<PendingCall *>(request_handle);

    std::lock_guard<std::mutex> lock(pending_mutex_);
    pending_responses_.push_back(CompletedResponse{http_status, body, error, pending});
}

void AchievementsClient::beginLoginWithPassword(const std::string &username, const std::string &password)
{
    if (!client_)
        return;
    last_error_.clear();
    login_pending_ = true;
    rc_client_begin_login_with_password(client_, username.c_str(), password.c_str(),
                                         &AchievementsClient::loginCallback, this);
}

void AchievementsClient::beginLoginWithToken(const std::string &username, const std::string &token)
{
    if (!client_)
        return;
    last_error_.clear();
    login_pending_ = true;
    rc_client_begin_login_with_token(client_, username.c_str(), token.c_str(),
                                      &AchievementsClient::loginCallback, this);
}

void AchievementsClient::logout()
{
    if (client_)
        rc_client_logout(client_);
}

void AchievementsClient::setSpectatorModeEnabled(bool enabled)
{
    if (client_)
        rc_client_set_spectator_mode_enabled(client_, enabled);
}

bool AchievementsClient::isSpectatorModeEnabled() const
{
    return client_ && rc_client_get_spectator_mode_enabled(client_);
}

void AchievementsClient::setEncoreModeEnabled(bool enabled)
{
    if (client_)
        rc_client_set_encore_mode_enabled(client_, enabled);
}

bool AchievementsClient::isEncoreModeEnabled() const
{
    return client_ && rc_client_get_encore_mode_enabled(client_);
}

void AchievementsClient::setUnofficialEnabled(bool enabled)
{
    if (client_)
        rc_client_set_unofficial_enabled(client_, enabled);
}

bool AchievementsClient::isUnofficialEnabled() const
{
    return client_ && rc_client_get_unofficial_enabled(client_);
}

void AchievementsClient::setNotificationsEnabled(bool enabled)
{
    notifications_enabled_ = enabled;
}

void AchievementsClient::setLeaderboardNotificationsEnabled(bool enabled)
{
    leaderboard_notifications_enabled_ = enabled;
}

void AchievementsClient::setLeaderboardTrackersEnabled(bool enabled)
{
    leaderboard_trackers_enabled_ = enabled;
}

void AchievementsClient::setProgressIndicatorsEnabled(bool enabled)
{
    progress_indicators_enabled_ = enabled;
}

void AchievementsClient::setChallengeIndicatorsEnabled(bool enabled)
{
    challenge_indicators_enabled_ = enabled;
}

void AchievementsClient::setNotificationDurationSeconds(int seconds)
{
    notification_duration_seconds_ = seconds;
}

void AchievementsClient::showNotification(const std::string &message, int duration_seconds) const
{
    // ponytail: a new notification always replaces any in-flight badge, so a
    // badge fetch that completes after a *different*, newer message has
    // already taken over the single notification slot just gets dropped
    // here instead of attaching to the wrong text. Upgrade: per-notification
    // IDs, if achievements ever need to show two things at once.
    S9xClearInfoImage();

    // Temporarily override the core's global message timeout for this one
    // call, then restore it -- same save/restore idiom already used by
    // win32/CSaveLoadWithPreviewDlg.cpp for the same field.
    uint32_t saved_timeout = Settings.InitialInfoStringTimeout;
    double fps = Memory.ROMFramesPerSecond > 0 ? Memory.ROMFramesPerSecond : 60.0;
    int seconds = duration_seconds > 0 ? duration_seconds : notification_duration_seconds_;
    Settings.InitialInfoStringTimeout = static_cast<uint32_t>(seconds * fps);
    S9xSetInfoString(message.c_str());
    Settings.InitialInfoStringTimeout = saved_timeout;
}

int AchievementsClient::shortNotificationDurationSeconds() const
{
    return std::max(1, notification_duration_seconds_ - 1);
}

void AchievementsClient::playAchievementSound() const
{
#ifdef _WIN32
    // "ACHIEVEMENT_SOUND" is a WAVE resource baked into the exe by
    // snes9x_win32.rc -- no external file to ship/find at runtime.
    PlaySoundW(L"ACHIEVEMENT_SOUND", GetModuleHandleW(nullptr), SND_ASYNC | SND_RESOURCE);
#endif
}

void AchievementsClient::fetchBadgeImage(const char *badge_name, const char *badge_url, uint32_t image_type) const
{
    std::string url;
    if (badge_url && badge_url[0] != '\0')
    {
        url = badge_url;
    }
    else if (badge_name && badge_name[0] != '\0')
    {
        // Matches DuckStation's Achievements::GetAchievementBadgeURL() fallback:
        // some rc_client states (RAIntegration, older servers) leave the URL
        // fields null, but the badge can still be built straight from rc_api.
        rc_api_fetch_image_request_t image_request{};
        image_request.image_name = badge_name;
        image_request.image_type = image_type;

        rc_api_request_t request{};
        if (rc_api_init_fetch_image_request(&request, &image_request) == RC_OK)
            url = request.url;
        rc_api_destroy_request(&request);
    }

    if (url.empty())
        return;

    // Same as DuckStation's HTTPCache: skip the network entirely for an image
    // we've already downloaded (e.g. the same achievement unlocking again
    // after a rewind/state reload, or reloading the same game).
    if (auto it = badge_cache_.find(url); it != badge_cache_.end())
    {
        S9xSetInfoImage(it->second.rgba.data(), it->second.width, it->second.height);
        return;
    }

    QMetaObject::invokeMethod(network_.get(), "fetchImage", Qt::QueuedConnection,
                               Q_ARG(QString, QString::fromStdString(url)),
                               Q_ARG(QString, userAgent()));
}

bool AchievementsClient::isLoginPending() const
{
    return login_pending_;
}

bool AchievementsClient::isLoggedIn() const
{
    return client_ && rc_client_get_user_info(client_) != nullptr;
}

Achievements::UserInfo AchievementsClient::userInfo() const
{
    Achievements::UserInfo info;
    const rc_client_user_t *user = client_ ? rc_client_get_user_info(client_) : nullptr;
    if (!user)
        return info;

    info.logged_in = true;
    info.display_name = user->display_name ? user->display_name : "";
    info.username = user->username ? user->username : "";
    info.token = user->token ? user->token : "";
    info.score = user->score;
    info.score_softcore = user->score_softcore;
    return info;
}

void AchievementsClient::beginLoadGame(const uint8_t *rom_data, size_t rom_size)
{
    if (client_)
        rc_client_begin_identify_and_load_game(client_, RC_CONSOLE_SUPER_NINTENDO, nullptr, rom_data, rom_size,
                                                &AchievementsClient::loadGameCallback, this);
}

void AchievementsClient::unloadGame()
{
    if (client_)
        rc_client_unload_game(client_);
}

bool AchievementsClient::isGameLoaded() const
{
    return client_ && rc_client_is_game_loaded(client_);
}

Achievements::GameSummary AchievementsClient::gameSummary() const
{
    Achievements::GameSummary summary;
    const rc_client_game_t *game = client_ ? rc_client_get_game_info(client_) : nullptr;
    if (!game)
        return summary;

    summary.game_loaded = true;
    summary.title = game->title ? game->title : "";

    rc_client_user_game_summary_t user_summary{};
    rc_client_get_user_game_summary(client_, &user_summary);
    summary.num_core_achievements = user_summary.num_core_achievements;
    summary.num_unlocked_achievements = user_summary.num_unlocked_achievements;
    return summary;
}

std::vector<Achievements::AchievementEntry> AchievementsClient::achievementList() const
{
    std::vector<Achievements::AchievementEntry> result;
    if (!client_ || !rc_client_is_game_loaded(client_))
        return result;

    rc_client_achievement_list_t *list = rc_client_create_achievement_list(
        client_, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
    if (!list)
        return result;

    for (uint32_t b = 0; b < list->num_buckets; ++b)
    {
        const rc_client_achievement_bucket_t &bucket = list->buckets[b];
        for (uint32_t a = 0; a < bucket.num_achievements; ++a)
        {
            const rc_client_achievement_t *achievement = bucket.achievements[a];
            Achievements::AchievementEntry entry;
            entry.id = achievement->id;
            entry.title = achievement->title ? achievement->title : "";
            entry.description = achievement->description ? achievement->description : "";
            entry.points = achievement->points;
            entry.unlocked = achievement->unlocked != RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE;
            result.push_back(std::move(entry));
        }
    }

    rc_client_destroy_achievement_list(list);
    return result;
}

std::string AchievementsClient::lastError() const
{
    return last_error_;
}

QString AchievementsClient::userAgent() const
{
    char clause[128] = {};
    if (client_)
        rc_client_get_user_agent_clause(client_, clause, sizeof(clause));

    return QStringLiteral("Snes9xRD/%1 (%2) %3")
        .arg(QStringLiteral(VERSION), QSysInfo::prettyProductName(), QString::fromUtf8(clause));
}

uint32_t AchievementsClient::readMemory(uint32_t address, uint8_t *buffer, uint32_t num_bytes, rc_client_t *)
{
    /* RetroAchievements' SNES memory map (rcheevos/src/rcheevos/consoleinfo.c):
     * $000000-$01FFFF is System RAM, $020000-$09FFFF is Cartridge RAM.
     * SA-1 I-RAM ($0A0000-$0A07FF) isn't wired up -- rare in practice, and
     * this is the single choke point to add it later if needed. */
    constexpr uint32_t kWramSize = 0x020000;
    constexpr uint32_t kSramBase = 0x020000;

    if (address < kWramSize)
    {
        uint32_t available = std::min<uint32_t>(num_bytes, kWramSize - address);
        memcpy(buffer, Memory.RAM + address, available);
        return available;
    }

    uint32_t sram_size = Memory.SRAMMask ? Memory.SRAMMask + 1 : 0;
    if (sram_size > 0 && address >= kSramBase && address < kSramBase + sram_size)
    {
        uint32_t offset = address - kSramBase;
        uint32_t available = std::min<uint32_t>(num_bytes, sram_size - offset);
        memcpy(buffer, Memory.SRAM + offset, available);
        return available;
    }

    return 0;
}

void AchievementsClient::serverCall(const rc_api_request_t *request, rc_client_server_callback_t callback,
                                    void *callback_data, rc_client_t *client)
{
    auto *self = static_cast<AchievementsClient *>(rc_client_get_userdata(client));
    auto *pending = new PendingCall{callback, callback_data};

    /* rc_client only ever calls this from the emulation thread (see the
     * class comment); hop to the GUI thread where AchievementsNetwork's
     * QNetworkAccessManager lives. */
    QMetaObject::invokeMethod(
        self->network_.get(), "dispatch", Qt::QueuedConnection, Q_ARG(QString, QString::fromUtf8(request->url)),
        Q_ARG(QByteArray, request->post_data ? QByteArray(request->post_data) : QByteArray()),
        Q_ARG(QString, request->content_type ? QString::fromUtf8(request->content_type) : QString()),
        Q_ARG(QString, self->userAgent()), Q_ARG(void *, pending));
}

void AchievementsClient::eventHandler(const rc_client_event_t *event, rc_client_t *client)
{
    auto *self = static_cast<AchievementsClient *>(rc_client_get_userdata(client));

    switch (event->type)
    {
        case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
            if (event->achievement && self->notifications_enabled_)
            {
                self->showNotification(std::string("Achievement Unlocked: ") + event->achievement->title,
                                        self->shortNotificationDurationSeconds());
                self->fetchBadgeImage(event->achievement->badge_name, event->achievement->badge_url, RC_IMAGE_TYPE_ACHIEVEMENT);
                self->playAchievementSound();
            }
            break;

        case RC_CLIENT_EVENT_GAME_COMPLETED:
            if (self->notifications_enabled_)
                self->showNotification("Congratulations! All achievements unlocked.");
            break;

        case RC_CLIENT_EVENT_SERVER_ERROR:
            if (event->server_error)
                self->showNotification(std::string("RetroAchievements error: ") + event->server_error->error_message);
            break;

        case RC_CLIENT_EVENT_LEADERBOARD_STARTED:
            if (event->leaderboard && self->leaderboard_notifications_enabled_)
                self->showNotification(std::string("Leaderboard attempt started: ") + event->leaderboard->title);
            break;

        case RC_CLIENT_EVENT_LEADERBOARD_FAILED:
            if (event->leaderboard && self->leaderboard_notifications_enabled_)
                self->showNotification(std::string("Leaderboard attempt failed: ") + event->leaderboard->title);
            break;

        case RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED:
            if (event->leaderboard && self->leaderboard_notifications_enabled_)
            {
                std::string message = std::string("Leaderboard submitted: ") + event->leaderboard->title;
                if (event->leaderboard->tracker_value)
                    message += std::string(" -- ") + event->leaderboard->tracker_value;
                self->showNotification(message);
            }
            break;

        case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW:
        case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE:
            if (event->leaderboard_tracker && self->leaderboard_trackers_enabled_)
                self->showNotification(event->leaderboard_tracker->display);
            break;

        case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE:
            if (self->leaderboard_trackers_enabled_)
                S9xClearInfoString();
            break;

        case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW:
        case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE:
            if (event->achievement && self->progress_indicators_enabled_)
                self->showNotification(std::string(event->achievement->title) + ": " + event->achievement->measured_progress);
            break;

        case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW:
            if (event->achievement && self->challenge_indicators_enabled_)
                self->showNotification(std::string("Challenge: ") + event->achievement->title);
            break;

        default:
            /* Progress/challenge indicator hides, leaderboard scoreboards,
             * rich presence, hardcore reset, subset completed, disconnect/
             * reconnect, etc. are out of scope for the MVP. */
            break;
    }
}


void AchievementsClient::logMessage(const char *message, const rc_client_t *)
{
    S9xMessage(S9X_INFO, S9X_NO_INFO, message);
}

void AchievementsClient::loginCallback(int result, const char *error_message, rc_client_t *, void *userdata)
{
    auto *self = static_cast<AchievementsClient *>(userdata);
    self->last_error_ = (result == RC_OK) ? std::string() : (error_message ? error_message : rc_error_str(result));
    self->login_pending_ = false;
}

void AchievementsClient::loadGameCallback(int result, const char *error_message, rc_client_t *client, void *userdata)
{
    auto *self = static_cast<AchievementsClient *>(userdata);
    if (result != RC_OK)
    {
        self->last_error_ = error_message ? error_message : rc_error_str(result);
        return;
    }

    self->last_error_.clear();

    // Mirrors DuckStation's game-start summary toast ("You have earned X of
    // Y achievements, and X of Y points. Leaderboards are DISABLED because
    // Hardcore Mode is off.") -- both renderers already handle embedded '\n'
    // (see gfx.cpp's S9xDisplayString / ImGui_DrawTextOverlay's AddText).
    rc_client_user_game_summary_t summary{};
    rc_client_get_user_game_summary(client, &summary);

    const rc_client_game_t *game = rc_client_get_game_info(client);
    std::string message = (game && game->title) ? std::string(game->title) + "\n" : std::string();

    if (summary.num_core_achievements == 0)
    {
        message += "This game has no achievements.";
    }
    else
    {
        message += "You have earned " + std::to_string(summary.num_unlocked_achievements) + " of " +
                   std::to_string(summary.num_core_achievements) + " achievements, and " +
                   std::to_string(summary.points_unlocked) + " of " + std::to_string(summary.points_core) +
                   " points.";

        // Hardcore is permanently disabled in this fork (see the ctor), so
        // leaderboards are always unavailable whenever the game has any.
        if (rc_client_has_leaderboards(client))
            message += "\nLeaderboards are DISABLED because Hardcore Mode is off.";
    }

    // Shorter than other achievement notifications -- this one shows on every
    // game load, so it doesn't need to linger as long as an unlock toast.
    self->showNotification(message, self->shortNotificationDurationSeconds());
    if (game)
        self->fetchBadgeImage(game->badge_name, game->badge_url, RC_IMAGE_TYPE_GAME);
}
