#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <QString>

#include "AchievementsTypes.hpp"
#include "rc_client.h"

class AchievementsNetwork;

// Owns and drives the RetroAchievements rc_client_t runtime for the Qt
// front-end.
//
// Threading model: rc_client_t is not documented as internally thread-safe,
// so every single call into it (do_frame/idle, login, load game, hardcore
// toggle, HTTP response delivery) is made from exactly one thread -- the
// emulation thread. Snes9xController::mainLoop() calls doFrame() every
// frame; EmuApplication marshals every user-triggered action (login, load
// game, hardcore toggle) onto the emulation thread via
// EmuThread::runOnThread(), exactly like it already does for save states
// and cheats. The only genuinely cross-thread piece is HTTP: rc_client_t's
// server_call callback fires from the emulation thread, hops to
// AchievementsNetwork on the GUI thread (QNetworkAccessManager needs a
// thread pumping a Qt event loop), and the reply is handed back through a
// small mutex-guarded queue that doFrame()/idle() drain before touching
// rc_client_t again -- so rc_client_t itself never sees a concurrent call.
//
// MVP scope: softcore only. Hardcore mode requires gating rewind/cheats/
// save-states/netplay, which is deliberately out of scope for this pass;
// hardcore is left permanently disabled (see the constructor).
class AchievementsClient
{
  public:
    AchievementsClient();
    ~AchievementsClient();

    AchievementsClient(const AchievementsClient &) = delete;
    AchievementsClient &operator=(const AchievementsClient &) = delete;

    // Called once per emulated frame from the emulation thread.
    void doFrame();
    // Called instead of doFrame() while no game is running/emulation is idle.
    void idle();

    void beginLoginWithPassword(const std::string &username, const std::string &password);
    void beginLoginWithToken(const std::string &username, const std::string &token);
    void logout();
    bool isLoginPending() const;
    bool isLoggedIn() const;
    Achievements::UserInfo userInfo() const;

    // Spectator/Encore/unofficial-achievements are plain rc_client_t knobs;
    // hardcore stays permanently off for now (see the class comment).
    void setSpectatorModeEnabled(bool enabled);
    bool isSpectatorModeEnabled() const;
    void setEncoreModeEnabled(bool enabled);
    bool isEncoreModeEnabled() const;
    void setUnofficialEnabled(bool enabled);
    bool isUnofficialEnabled() const;

    // Gates the S9xSetInfoString unlock/completion messages in eventHandler().
    void setNotificationsEnabled(bool enabled);
    // Gates leaderboard started/failed/submitted messages.
    void setLeaderboardNotificationsEnabled(bool enabled);
    // Gates leaderboard tracker (live attempt value) messages.
    void setLeaderboardTrackersEnabled(bool enabled);
    // Gates measured-progress ("4/10") messages for achievements that track one.
    void setProgressIndicatorsEnabled(bool enabled);
    // Gates the "challenge started" message for achievements being attempted right now.
    void setChallengeIndicatorsEnabled(bool enabled);
    // How long (in seconds) each of the above messages stays on screen.
    void setNotificationDurationSeconds(int seconds);

    // rom_data only needs to stay valid for the duration of this call.
    void beginLoadGame(const uint8_t *rom_data, size_t rom_size);
    void unloadGame();
    bool isGameLoaded() const;
    Achievements::GameSummary gameSummary() const;
    std::vector<Achievements::AchievementEntry> achievementList() const;

    // Last asynchronous login/load-game failure, for the GUI to display.
    std::string lastError() const;

    // Called by AchievementsNetwork (GUI thread) once an HTTP reply
    // completes. request_handle is the opaque token AchievementsClient
    // itself handed to AchievementsNetwork::dispatch -- thread-safe.
    void enqueueCompletedResponse(int http_status, const std::string &body, const std::string &error,
                                   void *request_handle);

    // Called by AchievementsNetwork (GUI thread) once a badge image
    // download+decode completes. Queued the same way as
    // enqueueCompletedResponse() -- rgba is applied to the core's
    // GFX.InfoImage from the emulation thread the next time doFrame()/idle()
    // drains the queue, never written to directly from the GUI thread.
    void enqueueBadgeImage(std::string url, std::vector<uint8_t> rgba, int width, int height);

  private:
    struct PendingCall;
    struct CompletedResponse
    {
        int http_status;
        std::string body;
        std::string error;
        PendingCall *pending;
    };
    struct PendingBadgeImage
    {
        std::string url;
        std::vector<uint8_t> rgba;
        int width = 0;
        int height = 0;
    };

    void drainCompletedResponses();
    QString userAgent() const;

    // Displays an achievement-related message via S9xSetInfoString, honoring
    // notification_duration_seconds_ (temporarily overrides the core's global
    // Settings.InitialInfoStringTimeout, then restores it -- same save/restore
    // idiom already used by win32/CSaveLoadWithPreviewDlg.cpp). duration_seconds
    // == 0 means "use notification_duration_seconds_".
    void showNotification(const std::string &message, int duration_seconds = 0) const;

    // Shorter variant for frequent/quick-to-read toasts (achievement unlocks,
    // the game-start summary) so they don't linger as long as the default.
    int shortNotificationDurationSeconds() const;

    // Plays the "ACHIEVEMENT_SOUND" WAVE resource embedded in the exe (see
    // snes9x_win32.rc) via the Windows PlaySound API. No-op on non-Windows builds.
    void playAchievementSound() const;

    // Kicks off an async fetch of a badge/game icon's artwork, using the
    // cache (badge_cache_) if we've already downloaded this URL, and falling
    // back to rc_api_init_fetch_image_request() -- same as DuckStation's
    // GetAchievementBadgeURL()/GetGameImageURL() -- if badge_url wasn't
    // populated (older rc_client states, RAIntegration). Fire and forget:
    // enqueueBadgeImage() applies whatever arrives last, matching the single
    // notification slot (see the ponytail note in eventHandler()).
    void fetchBadgeImage(const char *badge_name, const char *badge_url, uint32_t image_type) const;

    static uint32_t readMemory(uint32_t address, uint8_t *buffer, uint32_t num_bytes, rc_client_t *client);
    static void serverCall(const rc_api_request_t *request, rc_client_server_callback_t callback,
                           void *callback_data, rc_client_t *client);
    static void eventHandler(const rc_client_event_t *event, rc_client_t *client);
    static void logMessage(const char *message, const rc_client_t *client);
    static void loginCallback(int result, const char *error_message, rc_client_t *client, void *userdata);
    static void loadGameCallback(int result, const char *error_message, rc_client_t *client, void *userdata);

    rc_client_t *client_ = nullptr;
    std::unique_ptr<AchievementsNetwork> network_;
    std::string last_error_;
    bool login_pending_ = false;
    bool notifications_enabled_ = true;
    bool leaderboard_notifications_enabled_ = true;
    bool leaderboard_trackers_enabled_ = true;
    bool progress_indicators_enabled_ = true;
    bool challenge_indicators_enabled_ = true;
    int notification_duration_seconds_ = 5;

    std::mutex pending_mutex_;
    std::vector<CompletedResponse> pending_responses_;

    // Single-slot: only the most recently fetched badge matters, since only
    // one notification is ever shown at a time (see eventHandler()).
    std::mutex badge_mutex_;
    bool badge_pending_ = false;
    PendingBadgeImage pending_badge_;

    // Decoded badges keyed by URL, emu-thread-only (populated by
    // drainCompletedResponses(), read by fetchAchievementBadge()) so
    // repeat-unlocking the same achievement (rewind, state reload, resets
    // during testing) doesn't redownload every time.
    std::unordered_map<std::string, PendingBadgeImage> badge_cache_;
};
