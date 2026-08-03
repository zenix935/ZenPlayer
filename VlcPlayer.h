/**
 * @file VlcPlayer.h
 * @brief Self-Contained Dynamic Wrapper for libvlc.dll (VLC Media Player) in Qt 6.
 *
 * ============================================================================
 * QT & VLC VERSION COMPATIBILITY AND CONFLICT PREVENTION NOTES
 * ============================================================================
 * 1. 64-BIT ARCHITECTURE ALIGNMENT:
 *    - This application is built with Qt 6 (64-bit x86_64 on Windows).
 *    - The loaded VLC runtime (`libvlc.dll` and `libvlccore.dll`) MUST be 64-bit
 *      (e.g., VLC 3.0.x x64 or VLC 4.x x64).
 *    - Attempting to load a 32-bit `libvlc.dll` into a 64-bit Qt process will
 *      fail with Win32 error 193 (%1 is not a valid Win32 application).
 *    - `VlcPlayer::init()` automatically checks multiple 64-bit search paths
 *      and reports diagnostic architecture mismatch errors.
 *
 * 2. PREVENTING QT5 vs QT6 GUI PLUGIN CONFLICTS (DLL HELL):
 *    - VLC distributions typically bundle a Qt GUI interface plugin compiled
 *      against Qt 5 (`plugins/gui/libqt_plugin.dll`).
 *    - If `libvlc_new()` initializes GUI/interface plugins inside a Qt 6 host
 *      application, Windows will attempt to load Qt 5 DLLs into the same address
 *      space, causing severe symbol collisions and immediate application aborts.
 *    - This wrapper passes explicit command-line arguments to `libvlc_new()`:
 *        `--intf=dummy`        -> Disable interface plugins (no Qt GUI plugin)
 *        `--no-video`          -> Audio-only mode; do not load video output modules
 *        `--no-qt-privacy-ask` -> Prevent privacy popups
 *        `--no-interact`       -> Suppress interactive dialogs
 *        `--ignore-config`     -> Ignore user vlcrc config
 *        `--quiet`             -> Suppress verbose console logging
 *
 * 3. THREAD-SAFE SIGNAL DISPATCH:
 *    - VLC callbacks execute on internal libvlc worker threads. Touching Qt GUI
 *      objects or emitting direct signals across threads can cause mutex deadlocks
 *      or undefined behavior.
 *    - This wrapper uses a GUI-thread QTimer for smooth polling of position,
 *      duration, and media status, guaranteeing 100% thread safety with zero
 *      deadlocks and identical signal behavior to QMediaPlayer.
 * ============================================================================
 */

#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QTimer>
#include <QMap>
#include <QVariant>
#include <QLibrary>
#include <QDir>
#include <vector>
#include <cstdint>

// Forward declarations for opaque libvlc C types
struct libvlc_instance_t;
struct libvlc_media_t;
struct libvlc_media_player_t;
struct libvlc_audio_equalizer_t;

class VlcPlayer : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Media status enum compatible with QMediaPlayer usage.
     */
    enum MediaStatus {
        NoMedia = 0,
        Loading,
        Loaded,
        Playing,
        Paused,
        Stopped,
        EndOfMedia,
        Error
    };
    Q_ENUM(MediaStatus)

    explicit VlcPlayer(QObject* parent = nullptr);
    ~VlcPlayer() override;

    /**
     * @brief Dynamically loads libvlc.dll and initializes the VLC instance.
     * @param customVlcPath Optional absolute path to libvlc.dll or VLC install dir.
     * @return true if libvlc.dll was successfully loaded and initialized.
     */
    bool init(const QString& customVlcPath = QString());

    /**
     * @brief Checks whether libvlc has been successfully loaded and initialized.
     */
    bool isInitialized() const;

    /**
     * @brief Returns the last error message from initialization or playback.
     */
    QString lastError() const;

    /**
     * @brief Returns the loaded VLC version string (e.g. "3.0.21 Vetinari").
     */
    QString vlcVersion() const;

    /**
     * @brief Returns the absolute file path of the loaded libvlc.dll.
     */
    QString vlcDllPath() const;

    // --- Media Source API (QMediaPlayer compatible) ---
    void setSource(const QUrl& url);
    void setSource(const QString& filePath);
    QUrl source() const;
    QString currentFilePath() const;

    // --- Playback Control API ---
    void play();
    void pause();
    void stop();
    void setPosition(qint64 positionMs);
    qint64 position() const;
    qint64 duration() const;

    bool isPlaying() const;
    bool isPaused() const;
    MediaStatus mediaStatus() const;

    // --- Volume & Audio Control API (replaces QAudioOutput) ---
    /**
     * @brief Set volume as 0 to 100 integer (or higher up to 200).
     */
    void setVolume(int volume);

    /**
     * @brief Set volume as linear float (0.0 to 1.0 -> 0 to 100).
     */
    void setVolumeLinear(qreal volumeLinear);

    int volume() const;
    void setMuted(bool muted);
    bool isMuted() const;

    // --- Equalizer API ---
    /**
     * @brief Set 9-band or 10-band equalizer gains and preamp.
     * @param bandGains Vector of gains in dB (-20.0 to +20.0) for bands.
     * @param preamp Pre-amplification in dB (-20.0 to +20.0).
     * @return true if equalizer was applied successfully.
     */
    bool setEqualizer(const std::vector<int>& bandGains, float preamp = 0.0f);
    void setEqualizerEnabled(bool enabled);

    // --- Metadata API ---
    QString title() const;
    QString artist() const;
    QString album() const;
    QMap<QString, QVariant> metaData() const;

signals:
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void mediaStatusChanged(VlcPlayer::MediaStatus status);
    void metaDataChanged();
    void volumeChanged(int volume);
    void mutedChanged(bool muted);
    void errorOccurred(const QString& message);

private slots:
    void onPollTimer();

private:
    void releaseCurrentMedia();
    bool resolveSymbols(const QString& dllPath);
    QString findLibVlcPath(const QString& customPath);
    void updateMetadata();

    // Internal state
    bool m_initialized = false;
    QString m_lastError;
    QString m_vlcDllPath;
    QUrl m_currentUrl;
    QString m_currentFilePath;
    MediaStatus m_status = NoMedia;

    qint64 m_lastPosition = 0;
    qint64 m_lastDuration = 0;
    MediaStatus m_lastStatus = NoMedia;

    int m_volume = 50;
    bool m_muted = false;
    bool m_equalizerEnabled = false;
    std::vector<int> m_currentEqBands;
    float m_currentPreamp = 0.0f;

    QMap<QString, QVariant> m_metadata;

    QLibrary m_vlcLib;
    QTimer* m_pollTimer = nullptr;

    // VLC C handle pointers
    libvlc_instance_t* m_vlcInstance = nullptr;
    libvlc_media_player_t* m_mediaPlayer = nullptr;
    libvlc_media_t* m_media = nullptr;
    libvlc_audio_equalizer_t* m_equalizer = nullptr;

    // =========================================================================
    // Self-Contained Function Pointer Types for Dynamically Resolved API
    // =========================================================================
    typedef const char* (*fn_libvlc_get_version)();
    typedef const char* (*fn_libvlc_errmsg)();
    typedef libvlc_instance_t* (*fn_libvlc_new)(int argc, const char* const* argv);
    typedef void (*fn_libvlc_release)(libvlc_instance_t* p_instance);

    typedef libvlc_media_t* (*fn_libvlc_media_new_path)(libvlc_instance_t* p_instance, const char* path);
    typedef libvlc_media_t* (*fn_libvlc_media_new_location)(libvlc_instance_t* p_instance, const char* psz_mrl);
    typedef void (*fn_libvlc_media_release)(libvlc_media_t* p_md);
    typedef char* (*fn_libvlc_media_get_meta)(libvlc_media_t* p_md, int e_meta);
    typedef void (*fn_libvlc_media_parse_with_options)(libvlc_media_t* p_md, int parse_flag, int timeout);
    typedef void (*fn_libvlc_media_parse)(libvlc_media_t* p_md);
    typedef int64_t (*fn_libvlc_media_get_duration)(libvlc_media_t* p_md);

    typedef libvlc_media_player_t* (*fn_libvlc_media_player_new)(libvlc_instance_t* p_instance);
    typedef libvlc_media_player_t* (*fn_libvlc_media_player_new_from_media)(libvlc_media_t* p_md);
    typedef void (*fn_libvlc_media_player_release)(libvlc_media_player_t* p_mi);
    typedef void (*fn_libvlc_media_player_set_media)(libvlc_media_player_t* p_mi, libvlc_media_t* p_md);
    typedef int (*fn_libvlc_media_player_play)(libvlc_media_player_t* p_mi);
    typedef void (*fn_libvlc_media_player_pause)(libvlc_media_player_t* p_mi);
    typedef void (*fn_libvlc_media_player_set_pause)(libvlc_media_player_t* p_mi, int do_pause);
    typedef void (*fn_libvlc_media_player_stop)(libvlc_media_player_t* p_mi);
    typedef int (*fn_libvlc_media_player_stop_async)(libvlc_media_player_t* p_mi);
    typedef int (*fn_libvlc_media_player_is_playing)(libvlc_media_player_t* p_mi);
    typedef int64_t (*fn_libvlc_media_player_get_time)(libvlc_media_player_t* p_mi);
    typedef void (*fn_libvlc_media_player_set_time)(libvlc_media_player_t* p_mi, int64_t i_time);
    typedef int64_t (*fn_libvlc_media_player_get_length)(libvlc_media_player_t* p_mi);
    typedef int (*fn_libvlc_media_player_get_state)(libvlc_media_player_t* p_mi);

    typedef int (*fn_libvlc_audio_get_volume)(libvlc_media_player_t* p_mi);
    typedef int (*fn_libvlc_audio_set_volume)(libvlc_media_player_t* p_mi, int i_volume);
    typedef int (*fn_libvlc_audio_get_mute)(libvlc_media_player_t* p_mi);
    typedef void (*fn_libvlc_audio_set_mute)(libvlc_media_player_t* p_mi, int status);

    typedef libvlc_audio_equalizer_t* (*fn_libvlc_audio_equalizer_new)();
    typedef void (*fn_libvlc_audio_equalizer_release)(libvlc_audio_equalizer_t* p_equalizer);
    typedef int (*fn_libvlc_audio_equalizer_set_amp_at_index)(libvlc_audio_equalizer_t* p_equalizer, float f_amp, unsigned u_band);
    typedef int (*fn_libvlc_audio_equalizer_set_preamp)(libvlc_audio_equalizer_t* p_equalizer, float f_preamp);
    typedef int (*fn_libvlc_media_player_set_equalizer)(libvlc_media_player_t* p_mi, libvlc_audio_equalizer_t* p_equalizer);
    typedef void (*fn_libvlc_free)(void* ptr);

    // Resolved function pointer instances
    fn_libvlc_get_version sym_libvlc_get_version = nullptr;
    fn_libvlc_errmsg sym_libvlc_errmsg = nullptr;
    fn_libvlc_new sym_libvlc_new = nullptr;
    fn_libvlc_release sym_libvlc_release = nullptr;

    fn_libvlc_media_new_path sym_libvlc_media_new_path = nullptr;
    fn_libvlc_media_new_location sym_libvlc_media_new_location = nullptr;
    fn_libvlc_media_release sym_libvlc_media_release = nullptr;
    fn_libvlc_media_get_meta sym_libvlc_media_get_meta = nullptr;
    fn_libvlc_media_parse_with_options sym_libvlc_media_parse_with_options = nullptr;
    fn_libvlc_media_parse sym_libvlc_media_parse = nullptr;
    fn_libvlc_media_get_duration sym_libvlc_media_get_duration = nullptr;

    fn_libvlc_media_player_new sym_libvlc_media_player_new = nullptr;
    fn_libvlc_media_player_new_from_media sym_libvlc_media_player_new_from_media = nullptr;
    fn_libvlc_media_player_release sym_libvlc_media_player_release = nullptr;
    fn_libvlc_media_player_set_media sym_libvlc_media_player_set_media = nullptr;
    fn_libvlc_media_player_play sym_libvlc_media_player_play = nullptr;
    fn_libvlc_media_player_pause sym_libvlc_media_player_pause = nullptr;
    fn_libvlc_media_player_set_pause sym_libvlc_media_player_set_pause = nullptr;
    fn_libvlc_media_player_stop sym_libvlc_media_player_stop = nullptr;
    fn_libvlc_media_player_stop_async sym_libvlc_media_player_stop_async = nullptr;
    fn_libvlc_media_player_is_playing sym_libvlc_media_player_is_playing = nullptr;
    fn_libvlc_media_player_get_time sym_libvlc_media_player_get_time = nullptr;
    fn_libvlc_media_player_set_time sym_libvlc_media_player_set_time = nullptr;
    fn_libvlc_media_player_get_length sym_libvlc_media_player_get_length = nullptr;
    fn_libvlc_media_player_get_state sym_libvlc_media_player_get_state = nullptr;

    fn_libvlc_audio_get_volume sym_libvlc_audio_get_volume = nullptr;
    fn_libvlc_audio_set_volume sym_libvlc_audio_set_volume = nullptr;
    fn_libvlc_audio_get_mute sym_libvlc_audio_get_mute = nullptr;
    fn_libvlc_audio_set_mute sym_libvlc_audio_set_mute = nullptr;

    fn_libvlc_audio_equalizer_new sym_libvlc_audio_equalizer_new = nullptr;
    fn_libvlc_audio_equalizer_release sym_libvlc_audio_equalizer_release = nullptr;
    fn_libvlc_audio_equalizer_set_amp_at_index sym_libvlc_audio_equalizer_set_amp_at_index = nullptr;
    fn_libvlc_audio_equalizer_set_preamp sym_libvlc_audio_equalizer_set_preamp = nullptr;
    fn_libvlc_media_player_set_equalizer sym_libvlc_media_player_set_equalizer = nullptr;
    fn_libvlc_free sym_libvlc_free = nullptr;
};
