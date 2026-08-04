#pragma once

#include <QTimer>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QLibrary>
#include <QByteArray>
#include <QAudioSink>
#include <QAudioFormat>
#include <QAudioDevice>
#include <QMediaDevices>

//  Forward-declare opaque libVLC types so we never need
//  the VLC SDK headers at compile time.
struct libvlc_instance_t;
struct libvlc_media_t;
struct libvlc_media_player_t;
struct libvlc_equalizer_t;
struct libvlc_event_t;
struct libvlc_event_manager_t;

//  Function-pointer typedefs for every libVLC API we use.
//  Resolved at runtime via QLibrary.

// Core
using pfn_libvlc_new                             = libvlc_instance_t*     (*)(int, const char* const*);
using pfn_libvlc_release                         = void                   (*)(libvlc_instance_t*);

// Media
using pfn_libvlc_media_new_path                  = libvlc_media_t*        (*)(libvlc_instance_t*, const char*);
using pfn_libvlc_media_release                   = void                   (*)(libvlc_media_t*);

// Media Player
using pfn_libvlc_media_player_new                = libvlc_media_player_t* (*)(libvlc_instance_t*);
using pfn_libvlc_media_player_release            = void                   (*)(libvlc_media_player_t*);
using pfn_libvlc_media_player_set_media          = void                   (*)(libvlc_media_player_t*, libvlc_media_t*);
using pfn_libvlc_media_player_play               = int                    (*)(libvlc_media_player_t*);
using pfn_libvlc_media_player_pause              = void                   (*)(libvlc_media_player_t*);
using pfn_libvlc_media_player_stop               = void                   (*)(libvlc_media_player_t*);
using pfn_libvlc_media_player_get_time           = int64_t                (*)(libvlc_media_player_t*);
using pfn_libvlc_media_player_set_time           = void                   (*)(libvlc_media_player_t*, int64_t);
using pfn_libvlc_media_player_get_length         = int64_t                (*)(libvlc_media_player_t*);
using pfn_libvlc_media_player_get_position       = float                  (*)(libvlc_media_player_t*);
using pfn_libvlc_media_player_is_playing         = int                    (*)(libvlc_media_player_t*);
using pfn_libvlc_media_player_get_state          = int                    (*)(libvlc_media_player_t*);
using pfn_libvlc_audio_set_volume                = int                    (*)(libvlc_media_player_t*, int);
using pfn_libvlc_audio_get_volume                = int                    (*)(libvlc_media_player_t*);
using pfn_libvlc_audio_set_mute                  = void                   (*)(libvlc_media_player_t*, int);

// Events
using pfn_libvlc_media_player_event_manager      = libvlc_event_manager_t* (*)(libvlc_media_player_t*);
using pfn_libvlc_event_attach                    = int                     (*)(libvlc_event_manager_t*, int, void(*)(const libvlc_event_t*, void*), void*);

// Equalizer
using pfn_libvlc_audio_equalizer_new                = libvlc_equalizer_t* (*)();
using pfn_libvlc_audio_equalizer_new_from_preset    = libvlc_equalizer_t* (*)(unsigned);
using pfn_libvlc_audio_equalizer_release            = void                (*)(libvlc_equalizer_t*);
using pfn_libvlc_audio_equalizer_set_preamp         = int                 (*)(libvlc_equalizer_t*, float);
using pfn_libvlc_audio_equalizer_get_preamp         = float               (*)(libvlc_equalizer_t*);
using pfn_libvlc_audio_equalizer_set_amp_at_index   = int                 (*)(libvlc_equalizer_t*, float, unsigned);
using pfn_libvlc_audio_equalizer_get_amp_at_index   = float               (*)(libvlc_equalizer_t*, unsigned);
using pfn_libvlc_audio_equalizer_get_band_count     = unsigned            (*)();
using pfn_libvlc_audio_equalizer_get_preset_count   = unsigned            (*)();
using pfn_libvlc_audio_equalizer_get_preset_name    = const char*         (*)(unsigned);
using pfn_libvlc_audio_equalizer_get_band_frequency = float               (*)(unsigned);
using pfn_libvlc_media_player_set_equalizer         = int                 (*)(libvlc_media_player_t*, libvlc_equalizer_t*);

// Audio Callbacks
using pfn_libvlc_audio_set_callbacks             = void               (*)(libvlc_media_player_t*,
                                                                          void(*)(void*, const void*, unsigned, int64_t),
                                                                          void(*)(void*, int64_t),
                                                                          void(*)(void*, int64_t),
                                                                          void(*)(void*, int64_t),
                                                                          void(*)(void*),
                                                                          void*);
using pfn_libvlc_audio_set_format                = void               (*)(libvlc_media_player_t*, const char*, unsigned, unsigned);

//  VlcEngine — lightweight Qt wrapper around libVLC
//  for audio playback & 10-band equalizer.

class VlcEngine : public QObject
{
    Q_OBJECT

public:
    enum State
    {
        Stopped,
        Playing,
        Paused,
        Ended,    // media reached end-of-file
        Error
    };
    Q_ENUM(State)

    explicit VlcEngine(QObject* parent = nullptr);
    ~VlcEngine() override;

    // Initialise the engine.  |vlcLibPath| is the folder that
    // contains libvlc.dll (and its plugins/ subfolder).
    // Returns true on success.
    bool init(const QString& vlcLibPath);
    bool isInitialized() const { return m_vlcInstance != nullptr; }

    // ── Playback ──────────────────────────────────────
    void play(const QString& filePath);     // open + play
    void resume();                          // un-pause
    void pause();
    void stop();
    void setPosition(qint64 positionMs);    // seek

    qint64 position()  const;               // current pos (ms)
    qint64 duration()  const;               // total length (ms)
    bool   isPlaying() const;
    State  state()     const;

    // ── Volume ────────────────────────────────────────
    void setVolume(int percent);            // 0-100
    int  volume() const;
    void setMuted(bool muted);

    void  setSoftwareVolume(float vol);     // 0.0f to 1.0f (crossfade)
    float softwareVolume() const;
    float effectiveVolume() const;

    // ── Software Mixing PCM Buffer ────────────────────
    int   pcmBufferFrames() const;
    void  mixPcmFrames(int16_t* mixOut, int framesToProcess);
    void  clearPcmBuffer();

    // ── Equalizer ─────────────────────────────────────
    void applyEqualizer(float preampDb, const float* bandDb, int bandCount);
    void resetEqualizer();                  // flat / disable

    static unsigned equalizerBandCount();
    static float    equalizerBandFrequency(unsigned index);

signals:
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    void stateChanged(VlcEngine::State newState);

private:
    // Timer that polls VLC for position/duration because
    // libVLC callbacks fire on VLC threads.
    QTimer m_pollTimer;
    void onPollTimer();

    // Static callback forwarded from libVLC event manager
    static void vlcEventCallback(const libvlc_event_t* event, void* userData);
    void handleVlcEventType(int eventType);

    // Audio Callbacks from libVLC
    static void vlcAudioPlayCallback(void *data, const void *samples, unsigned count, int64_t pts);
    static void vlcAudioPauseCallback(void *data, int64_t pts);
    static void vlcAudioResumeCallback(void *data, int64_t pts);
    static void vlcAudioFlushCallback(void *data, int64_t pts);
    static void vlcAudioDrainCallback(void *data);

    // ── libVLC handles ────────────────────────────────
    libvlc_instance_t*      m_vlcInstance  = nullptr;
    libvlc_media_player_t*  m_vlcPlayer    = nullptr;
    libvlc_equalizer_t*     m_vlcEqualizer = nullptr;

    // Cached state so we can detect transitions
    State   m_state          = Stopped;
    qint64  m_lastPos        = -1;
    qint64  m_lastDur        = -1;
    int     m_volume         = 50;
    float   m_softwareVolume = 1.0f;
    bool    m_muted          = false;

    mutable QMutex m_pcmMutex;
    QByteArray     m_pcmBuffer;

    // ── Dynamically-resolved function pointers ────────
    QLibrary m_lib;

    pfn_libvlc_new                             fn_new                    = nullptr;
    pfn_libvlc_release                         fn_release                = nullptr;

    pfn_libvlc_media_new_path                  fn_media_new_path         = nullptr;
    pfn_libvlc_media_release                   fn_media_release          = nullptr;

    pfn_libvlc_media_player_new                fn_player_new             = nullptr;
    pfn_libvlc_media_player_release            fn_player_release         = nullptr;
    pfn_libvlc_media_player_set_media          fn_player_set_media       = nullptr;
    pfn_libvlc_media_player_play               fn_player_play            = nullptr;
    pfn_libvlc_media_player_pause              fn_player_pause           = nullptr;
    pfn_libvlc_media_player_stop               fn_player_stop            = nullptr;
    pfn_libvlc_media_player_get_time           fn_player_get_time        = nullptr;
    pfn_libvlc_media_player_set_time           fn_player_set_time        = nullptr;
    pfn_libvlc_media_player_get_length         fn_player_get_length      = nullptr;
    pfn_libvlc_media_player_get_position       fn_player_get_position    = nullptr;
    pfn_libvlc_media_player_is_playing         fn_player_is_playing      = nullptr;
    pfn_libvlc_media_player_get_state          fn_player_get_state       = nullptr;
    pfn_libvlc_audio_set_volume                fn_audio_set_volume       = nullptr;
    pfn_libvlc_audio_get_volume                fn_audio_get_volume       = nullptr;
    pfn_libvlc_audio_set_mute                  fn_audio_set_mute         = nullptr;

    pfn_libvlc_media_player_event_manager      fn_event_manager          = nullptr;
    pfn_libvlc_event_attach                    fn_event_attach           = nullptr;

    pfn_libvlc_audio_equalizer_new             fn_eq_new                 = nullptr;
    pfn_libvlc_audio_equalizer_new_from_preset fn_eq_new_from_preset     = nullptr;
    pfn_libvlc_audio_equalizer_release         fn_eq_release             = nullptr;
    pfn_libvlc_audio_equalizer_set_preamp      fn_eq_set_preamp          = nullptr;
    pfn_libvlc_audio_equalizer_get_preamp      fn_eq_get_preamp          = nullptr;
    pfn_libvlc_audio_equalizer_set_amp_at_index fn_eq_set_amp            = nullptr;
    pfn_libvlc_audio_equalizer_get_amp_at_index fn_eq_get_amp            = nullptr;
    pfn_libvlc_audio_equalizer_get_band_count  fn_eq_band_count          = nullptr;
    pfn_libvlc_audio_equalizer_get_preset_count fn_eq_preset_count       = nullptr;
    pfn_libvlc_audio_equalizer_get_preset_name fn_eq_preset_name         = nullptr;
    pfn_libvlc_audio_equalizer_get_band_frequency fn_eq_band_freq        = nullptr;
    pfn_libvlc_media_player_set_equalizer      fn_player_set_equalizer   = nullptr;

    pfn_libvlc_audio_set_callbacks             fn_audio_set_callbacks    = nullptr;
    pfn_libvlc_audio_set_format                fn_audio_set_format       = nullptr;

    bool resolveFunctions();

    // Helpers
    void setState(State s);
};