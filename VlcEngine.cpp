#include "VlcEngine.h"
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QCoreApplication>

// ─────────────────────────────────────────────────────
//  libVLC event type constants we subscribe to.
//  (from vlc/libvlc_events.h — stable ABI)
// ─────────────────────────────────────────────────────
static constexpr int VLC_EVENT_MediaPlayerPlaying         =0x100+4;  // 260
static constexpr int VLC_EVENT_MediaPlayerPaused          =0x100+5;  // 261
static constexpr int VLC_EVENT_MediaPlayerStopped         =0x100+6;  // 262
static constexpr int VLC_EVENT_MediaPlayerEndReached      =0x100+9;  // 265
static constexpr int VLC_EVENT_MediaPlayerEncounteredError=0x100+10; // 266
static constexpr int VLC_EVENT_MediaPlayerTimeChanged     =0x100+13; // 269
static constexpr int VLC_EVENT_MediaPlayerLengthChanged   =0x100+15; // 271

// ─────────────────────────────────────────────────────
//  libvlc_event_t layout (simplified — we only read the
//  fields we need).  The real struct has a union; we
//  replicate just enough to extract time/length values.
// ─────────────────────────────────────────────────────
//  Unfortunately we can't include <vlc/libvlc_events.h>
//  because the user doesn't have the SDK installed.
//  We'll access event->type (int at offset 0) and for
//  time-changed the int64 at the start of the union.
//  The union begins right after {int type; void* p_obj;}.
//
//  On 64-bit Windows:
//    offset 0:  int     type
//    offset 8:  void*   p_obj  (8 bytes on x64)
//    offset 16: union   u      — first member is a struct
//               containing an int64_t (new_time or new_length)
//
//  We'll use reinterpret_cast to read these safely.

struct VlcEventHeader 
{
    int type;
    int _pad;         // alignment
    void* p_obj;
    int64_t value;    // first int64 in the union (time or length)
};

// ─────────────────────────────────────────────────────
VlcEngine::VlcEngine(QObject* parent) : QObject(parent)
{
    m_pollTimer.setInterval(100);  // 10 Hz polling
    connect(&m_pollTimer, &QTimer::timeout, this, &VlcEngine::onPollTimer);
}

VlcEngine::~VlcEngine()
{
    m_pollTimer.stop();

    if (m_vlcEqualizer && fn_eq_release)
        fn_eq_release(m_vlcEqualizer);

    if (m_vlcPlayer)
    {
        if (fn_player_stop)
            fn_player_stop(m_vlcPlayer);
        if (fn_player_release)
            fn_player_release(m_vlcPlayer);
    }

    if (m_vlcInstance && fn_release)
        fn_release(m_vlcInstance);
}

// ─────────────────────────────────────────────────────
//  init()
// ─────────────────────────────────────────────────────
bool VlcEngine::init(const QString& vlcLibPath)
{
    if (m_vlcInstance)
        return true;  // already initialised

    QStringList candidates;
    if (!vlcLibPath.isEmpty())
        candidates<<vlcLibPath;
    candidates<<QCoreApplication::applicationDirPath();
    candidates<<"C:/Program Files/VideoLAN/VLC";
    candidates<<"C:/Program Files (x86)/VideoLAN/VLC";

    QString dllDir;
    for (const QString& dir : candidates)
    {
        if (QFile::exists(QDir(dir).absoluteFilePath("libvlc.dll")))
        {
            dllDir=dir;
            break;
        }
    }

    if (dllDir.isEmpty())
    {
        qWarning()<<"[VlcEngine] Could not find libvlc.dll in candidate paths.";
        return false;
    }

    // Load libvlc.dll
    QString dllPath=QDir(dllDir).absoluteFilePath("libvlc.dll");
    m_lib.setFileName(dllPath);
    if (!m_lib.load())
    {
        qWarning()<<"[VlcEngine] Failed to load"<<dllPath<<": "<<m_lib.errorString();
        return false;
    }

    if (!resolveFunctions())
    {
        qWarning()<<"[VlcEngine] Failed to resolve one or more libVLC symbols.";
        m_lib.unload();
        return false;
    }

    // Tell VLC where its plugins folder is
    QString pluginPath=QDir(dllDir).absoluteFilePath("plugins");
    QByteArray pluginPathUtf8=QString("--plugin-path="+pluginPath).toUtf8();
    QByteArray noVideo="--no-video";    // audio-only player
    QByteArray noXlib="--no-xlib";
    QByteArray quiet="--quiet";

    const char* args[]={pluginPathUtf8.constData(), noVideo.constData(), noXlib.constData(), quiet.constData(),};

    m_vlcInstance=fn_new(4, args);
    if (!m_vlcInstance)
    {
        qWarning()<<"[VlcEngine] libvlc_new() returned null.";
        return false;
    }

    m_vlcPlayer=fn_player_new(m_vlcInstance);
    if (!m_vlcPlayer)
    {
        qWarning()<<"[VlcEngine] libvlc_media_player_new() returned null.";
        fn_release(m_vlcInstance);
        m_vlcInstance=nullptr;
        return false;
    }

    // Subscribe to events
    if (fn_event_manager && fn_event_attach)
    {
        auto em=fn_event_manager(m_vlcPlayer);
        fn_event_attach(em, VLC_EVENT_MediaPlayerPlaying, &VlcEngine::vlcEventCallback, this);
        fn_event_attach(em, VLC_EVENT_MediaPlayerPaused, &VlcEngine::vlcEventCallback, this);
        fn_event_attach(em, VLC_EVENT_MediaPlayerStopped, &VlcEngine::vlcEventCallback, this);
        fn_event_attach(em, VLC_EVENT_MediaPlayerEndReached, &VlcEngine::vlcEventCallback, this);
        fn_event_attach(em, VLC_EVENT_MediaPlayerEncounteredError, &VlcEngine::vlcEventCallback, this);
        // We intentionally do NOT subscribe to TimeChanged / LengthChanged here
        // because those callbacks fire on VLC threads. We poll instead.
    }

    m_pollTimer.start();
    qDebug()<<"[VlcEngine] Initialised successfully.";
    return true;
}

// ─────────────────────────────────────────────────────
//  Dynamic symbol resolution
// ─────────────────────────────────────────────────────
bool VlcEngine::resolveFunctions()
{
    #define RESOLVE(name, type, sym) \
        name=reinterpret_cast<type>(m_lib.resolve(sym)); \
        if (!name) { qWarning()<<"[VlcEngine] Missing symbol:"<<sym; return false; }

    RESOLVE(fn_new,                  pfn_libvlc_new,                                "libvlc_new");
    RESOLVE(fn_release,              pfn_libvlc_release,                            "libvlc_release");
    
    RESOLVE(fn_media_new_path,       pfn_libvlc_media_new_path,                     "libvlc_media_new_path");
    RESOLVE(fn_media_release,        pfn_libvlc_media_release,                      "libvlc_media_release");
    
    RESOLVE(fn_player_new,           pfn_libvlc_media_player_new,                   "libvlc_media_player_new");
    RESOLVE(fn_player_release,       pfn_libvlc_media_player_release,               "libvlc_media_player_release");
    RESOLVE(fn_player_set_media,     pfn_libvlc_media_player_set_media,             "libvlc_media_player_set_media");
    RESOLVE(fn_player_play,          pfn_libvlc_media_player_play,                  "libvlc_media_player_play");
    RESOLVE(fn_player_pause,         pfn_libvlc_media_player_pause,                 "libvlc_media_player_pause");
    RESOLVE(fn_player_stop,          pfn_libvlc_media_player_stop,                  "libvlc_media_player_stop");
    RESOLVE(fn_player_get_time,      pfn_libvlc_media_player_get_time,              "libvlc_media_player_get_time");
    RESOLVE(fn_player_set_time,      pfn_libvlc_media_player_set_time,              "libvlc_media_player_set_time");
    RESOLVE(fn_player_get_length,    pfn_libvlc_media_player_get_length,            "libvlc_media_player_get_length");
    RESOLVE(fn_player_get_position,  pfn_libvlc_media_player_get_position,          "libvlc_media_player_get_position");
    RESOLVE(fn_player_is_playing,    pfn_libvlc_media_player_is_playing,            "libvlc_media_player_is_playing");
    RESOLVE(fn_player_get_state,     pfn_libvlc_media_player_get_state,             "libvlc_media_player_get_state");
    RESOLVE(fn_audio_set_volume,     pfn_libvlc_audio_set_volume,                   "libvlc_audio_set_volume");
    RESOLVE(fn_audio_get_volume,     pfn_libvlc_audio_get_volume,                   "libvlc_audio_get_volume");
    RESOLVE(fn_audio_set_mute,       pfn_libvlc_audio_set_mute,                     "libvlc_audio_set_mute");
    
    RESOLVE(fn_event_manager,        pfn_libvlc_media_player_event_manager,         "libvlc_media_player_event_manager");
    RESOLVE(fn_event_attach,         pfn_libvlc_event_attach,                       "libvlc_event_attach");

    RESOLVE(fn_eq_new,               pfn_libvlc_audio_equalizer_new,                "libvlc_audio_equalizer_new");
    RESOLVE(fn_eq_new_from_preset,   pfn_libvlc_audio_equalizer_new_from_preset,    "libvlc_audio_equalizer_new_from_preset");
    RESOLVE(fn_eq_release,           pfn_libvlc_audio_equalizer_release,            "libvlc_audio_equalizer_release");
    RESOLVE(fn_eq_set_preamp,        pfn_libvlc_audio_equalizer_set_preamp,         "libvlc_audio_equalizer_set_preamp");
    RESOLVE(fn_eq_get_preamp,        pfn_libvlc_audio_equalizer_get_preamp,         "libvlc_audio_equalizer_get_preamp");
    RESOLVE(fn_eq_set_amp,           pfn_libvlc_audio_equalizer_set_amp_at_index,   "libvlc_audio_equalizer_set_amp_at_index");
    RESOLVE(fn_eq_get_amp,           pfn_libvlc_audio_equalizer_get_amp_at_index,   "libvlc_audio_equalizer_get_amp_at_index");
    RESOLVE(fn_eq_band_count,        pfn_libvlc_audio_equalizer_get_band_count,     "libvlc_audio_equalizer_get_band_count");
    RESOLVE(fn_eq_preset_count,      pfn_libvlc_audio_equalizer_get_preset_count,   "libvlc_audio_equalizer_get_preset_count");
    RESOLVE(fn_eq_preset_name,       pfn_libvlc_audio_equalizer_get_preset_name,    "libvlc_audio_equalizer_get_preset_name");
    RESOLVE(fn_eq_band_freq,         pfn_libvlc_audio_equalizer_get_band_frequency, "libvlc_audio_equalizer_get_band_frequency");
    RESOLVE(fn_player_set_equalizer, pfn_libvlc_media_player_set_equalizer,         "libvlc_media_player_set_equalizer");

    #undef RESOLVE
    return true;
}

// ─────────────────────────────────────────────────────
//  Playback
// ─────────────────────────────────────────────────────
void VlcEngine::play(const QString& filePath)
{
    if (!m_vlcInstance || !m_vlcPlayer)
        return;

    // Convert to native separators for libVLC
    QByteArray pathUtf8=QDir::toNativeSeparators(filePath).toUtf8();

    libvlc_media_t* media=fn_media_new_path(m_vlcInstance, pathUtf8.constData());
    if (!media)
    {
        qWarning()<<"[VlcEngine] Failed to create media for:"<<filePath;
        return;
    }

    fn_player_set_media(m_vlcPlayer, media);
    fn_media_release(media);  // player holds its own reference

    // Restore volume before playing
    fn_audio_set_volume(m_vlcPlayer, m_muted ? 0 : m_volume);

    // Re-apply equalizer if one is active
    if (m_vlcEqualizer)
        fn_player_set_equalizer(m_vlcPlayer, m_vlcEqualizer);

    fn_player_play(m_vlcPlayer);
}

void VlcEngine::resume()
{
    if (!m_vlcPlayer)
        return;

    // libvlc_media_player_play() resumes if paused
    fn_player_play(m_vlcPlayer);
}

void VlcEngine::pause()
{
    if (!m_vlcPlayer)
        return;
    fn_player_pause(m_vlcPlayer);
}

void VlcEngine::stop()
{
    if (!m_vlcPlayer)
        return;
    fn_player_stop(m_vlcPlayer);
    setState(Stopped);
}

void VlcEngine::setPosition(qint64 positionMs)
{
    if (!m_vlcPlayer)
        return;
    fn_player_set_time(m_vlcPlayer, positionMs);
}

qint64 VlcEngine::position() const
{
    if (!m_vlcPlayer || !fn_player_get_time)
        return 0;
    int64_t t=fn_player_get_time(const_cast<libvlc_media_player_t*>(m_vlcPlayer));
    return (t < 0) ? 0 : t;
}

qint64 VlcEngine::duration() const
{
    if (!m_vlcPlayer || !fn_player_get_length)
        return 0;
    int64_t d = fn_player_get_length(const_cast<libvlc_media_player_t*>(m_vlcPlayer));
    return (d<0) ? 0 : d;
}

bool VlcEngine::isPlaying() const
{
    if (!m_vlcPlayer || !fn_player_is_playing)
        return false;
    return fn_player_is_playing(const_cast<libvlc_media_player_t*>(m_vlcPlayer));
}

VlcEngine::State VlcEngine::state() const { return m_state; }

// ─────────────────────────────────────────────────────
//  Volume
// ─────────────────────────────────────────────────────
void VlcEngine::setVolume(int percent)
{
    int temp=percent;
    percent=static_cast<int>(temp*(6.6-qLn(temp)));
    m_volume=qBound(0, percent, 200);
    if (m_vlcPlayer && !m_muted)
        fn_audio_set_volume(m_vlcPlayer, m_volume);
}

int VlcEngine::volume() const { return m_volume; }

void VlcEngine::setMuted(bool muted)
{
    m_muted=muted;
    if (m_vlcPlayer)
        fn_audio_set_mute(m_vlcPlayer, muted ? 1 : 0);
}

// ─────────────────────────────────────────────────────
//  Equalizer
// ─────────────────────────────────────────────────────
void VlcEngine::applyEqualizer(float preampDb, const float* bandDb, int bandCount)
{
    if (!fn_eq_new || !fn_eq_set_preamp || !fn_eq_set_amp || !fn_player_set_equalizer)
        return;

    // Release old equalizer if any
    if (m_vlcEqualizer && fn_eq_release)
        fn_eq_release(m_vlcEqualizer);

    m_vlcEqualizer=fn_eq_new();
    if (!m_vlcEqualizer)
        return;

    fn_eq_set_preamp(m_vlcEqualizer, preampDb);

    unsigned maxBands=fn_eq_band_count ? fn_eq_band_count() : 10;
    int count=qMin(static_cast<unsigned>(bandCount), maxBands);
    for (int i=0; i<count; ++i)
        fn_eq_set_amp(m_vlcEqualizer, bandDb[i], static_cast<unsigned>(i));

    if (m_vlcPlayer)
        fn_player_set_equalizer(m_vlcPlayer, m_vlcEqualizer);
}

void VlcEngine::resetEqualizer()
{
    if (m_vlcEqualizer && fn_eq_release)
    {
        fn_eq_release(m_vlcEqualizer);
        m_vlcEqualizer=nullptr;
    }
    // Pass nullptr to disable the equalizer
    if (m_vlcPlayer && fn_player_set_equalizer)
        fn_player_set_equalizer(m_vlcPlayer, nullptr);
}

unsigned VlcEngine::equalizerBandCount()
{
    // This is a static query; can't call through instance.
    // Return VLC's standard 10 bands.
    return 10;
}

float VlcEngine::equalizerBandFrequency(unsigned index)
{
    // Standard VLC 10-band frequencies
    static const float freqs[]={60, 170, 310, 600, 1000, 3000, 6000, 12000, 14000, 16000};
    if (index<10)
        return freqs[index];
    return 0;
}

// ─────────────────────────────────────────────────────
//  Polling timer — runs on the Qt event loop at ~10 Hz.
//  Emits positionChanged and durationChanged when values
//  actually change, keeping UI updates efficient.
// ─────────────────────────────────────────────────────
void VlcEngine::onPollTimer()
{
    if (!m_vlcPlayer)
        return;

    qint64 pos=position();
    qint64 dur=duration();

    if (dur != m_lastDur)
    {
        m_lastDur=dur;
        emit durationChanged(dur);
    }
    if (pos != m_lastPos)
    {
        m_lastPos=pos;
        emit positionChanged(pos);
    }
}

// ─────────────────────────────────────────────────────
//  VLC event callback (called on VLC internal thread!)
//  We only do a state transition here and use
//  QMetaObject::invokeMethod to bounce to the Qt thread.
// ─────────────────────────────────────────────────────
void VlcEngine::vlcEventCallback(const libvlc_event_t* event, void* userData)
{
    VlcEngine* self=static_cast<VlcEngine*>(userData);
    // Capture the event type by value — the event pointer becomes
    // invalid as soon as this callback returns.
    auto header=reinterpret_cast<const VlcEventHeader*>(event);
    int eventType=header->type;
    // Bounce to the Qt main thread
    QMetaObject::invokeMethod(self, [self, eventType]() {self->handleVlcEventType(eventType);}, Qt::QueuedConnection);
}

void VlcEngine::handleVlcEventType(int type)
{
    switch (type)
    {
    case VLC_EVENT_MediaPlayerPlaying:
        setState(Playing);
        // Apply volume on start (VLC resets it sometimes)
        if (fn_audio_set_volume)
            fn_audio_set_volume(m_vlcPlayer, m_muted ? 0 : m_volume);
        break;
    case VLC_EVENT_MediaPlayerPaused:
        setState(Paused);
        break;
    case VLC_EVENT_MediaPlayerStopped:
        setState(Stopped);
        break;
    case VLC_EVENT_MediaPlayerEndReached:
        setState(Ended);
        break;
    case VLC_EVENT_MediaPlayerEncounteredError:
        setState(Error);
        break;
    default:
        break;
    }
}

void VlcEngine::setState(State s)
{
    if (m_state != s)
    {
        m_state=s;
        emit stateChanged(s);
    }
}
