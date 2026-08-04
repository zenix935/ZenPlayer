#include "VlcEngine.h"
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QCoreApplication>
#include <QtMath>
#include <QMutexLocker>
#include <algorithm>

//  SoftwareAudioMixer — mixes raw PCM stereo streams
//  from all VlcEngine instances into a single QAudioSink
//  stream sent to the sound card.
class SoftwareAudioMixer : public QObject
{
    Q_OBJECT
public:
    static SoftwareAudioMixer& instance()
    {
        static SoftwareAudioMixer mixer;
        return mixer;
    }

    void registerEngine(VlcEngine* engine)
    {
        QMutexLocker locker(&m_mutex);
        if (!m_engines.contains(engine))
            m_engines.append(engine);
        ensureAudioSink();
    }

    void unregisterEngine(VlcEngine* engine)
    {
        QMutexLocker locker(&m_mutex);
        m_engines.removeAll(engine);
    }

    void mixAndWrite()
    {
        QMutexLocker locker(&m_mutex);
        if (!m_audioSink || !m_audioDevice)
        {
            ensureAudioSink();
            if (!m_audioSink || !m_audioDevice)
                return;
        }

        qint64 bytesFree=m_audioSink->bytesFree();
        if (bytesFree <= 0)
            return;

        // Cap bytesFree per pass for low latency
        bytesFree=qMin(bytesFree, qint64(16384));
        int framesToMix=static_cast<int>(bytesFree/4);
        if (framesToMix <= 0)
            return;

        int maxAvailFrames=0;
        for (VlcEngine* engine : m_engines)
        {
            int engineAvail=engine->pcmBufferFrames();
            if (engineAvail > maxAvailFrames)
                maxAvailFrames=engineAvail;
        }

        if (maxAvailFrames <= 0)
            return;

        int framesToProcess=qMin(framesToMix, maxAvailFrames);
        int bytesToProcess=framesToProcess*4;

        if (m_mixBuffer.size() < bytesToProcess)
            m_mixBuffer.resize(bytesToProcess);

        int16_t* mixOut=reinterpret_cast<int16_t*>(m_mixBuffer.data());
        std::fill(mixOut, mixOut+(framesToProcess*2), static_cast<int16_t>(0));

        for (VlcEngine* engine : m_engines)
            engine->mixPcmFrames(mixOut, framesToProcess);

        m_audioDevice->write(m_mixBuffer.constData(), bytesToProcess);
    }

private:
    SoftwareAudioMixer() : QObject(nullptr) {}
    ~SoftwareAudioMixer() override
    {
        if (m_timer)
        {
            m_timer->stop();
            delete m_timer;
            m_timer=nullptr;
        }
        if (m_audioSink)
        {
            m_audioSink->stop();
            delete m_audioSink;
            m_audioSink=nullptr;
        }
    }

    void ensureAudioSink()
    {
        if (m_audioSink)
            return;

        QAudioFormat format;
        format.setSampleRate(44100);
        format.setChannelCount(2);
        format.setSampleFormat(QAudioFormat::Int16);

        QAudioDevice device = QMediaDevices::defaultAudioOutput();
        if (!device.isFormatSupported(format))
            format = device.preferredFormat();

        m_audioSink=new QAudioSink(device, format, this);
        m_audioSink->setBufferSize(44100*4/4); // ~250ms buffer
        m_audioDevice=m_audioSink->start();

        m_timer=new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &SoftwareAudioMixer::mixAndWrite);
        m_timer->start(15); // ~66 Hz polling loop
    }

    QMutex m_mutex;
    QList<VlcEngine*> m_engines;
    QAudioSink* m_audioSink=nullptr;
    QIODevice* m_audioDevice=nullptr;
    QByteArray m_mixBuffer;
    QTimer* m_timer=nullptr;
};

//  libVLC event type constants we subscribe to.
static constexpr int VLC_EVENT_MediaPlayerPlaying         =0x100+4;  // 260
static constexpr int VLC_EVENT_MediaPlayerPaused          =0x100+5;  // 261
static constexpr int VLC_EVENT_MediaPlayerStopped         =0x100+6;  // 262
static constexpr int VLC_EVENT_MediaPlayerEndReached      =0x100+9;  // 265
static constexpr int VLC_EVENT_MediaPlayerEncounteredError=0x100+10; // 266
static constexpr int VLC_EVENT_MediaPlayerTimeChanged     =0x100+13; // 269
static constexpr int VLC_EVENT_MediaPlayerLengthChanged   =0x100+15; // 271

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
    SoftwareAudioMixer::instance().unregisterEngine(this);
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

//  init()
bool VlcEngine::init(const QString& vlcLibPath)
{
    if (m_vlcInstance)
        return true;  // already initialised

    QStringList candidates;
    if (!vlcLibPath.isEmpty())
        candidates<<vlcLibPath;
    candidates<<QCoreApplication::applicationDirPath();
#ifdef _WIN32
    candidates<<"C:/Program Files/VideoLAN/VLC";
    candidates<<"C:/Program Files (x86)/VideoLAN/VLC";
    static const QStringList libNames={"libvlc.dll"};
#elif defined(__APPLE__)
    candidates<<"/Applications/VLC.app/Contents/MacOS/lib";
    candidates<<"/usr/local/lib";
    candidates<<"/opt/homebrew/lib";
    static const QStringList libNames={"libvlc.dylib", "libvlc.5.dylib"};
#else
    candidates<<"/usr/lib/x86_64-linux-gnu"<<"/usr/lib"<<"/usr/local/lib"<<"/usr/lib64"<<"/usr/lib/aarch64-linux-gnu";
    static const QStringList libNames={"libvlc.so.5", "libvlc.so.12", "libvlc.so"};
#endif

    QString dllDir;
    QString targetLibName;
    for (const QString& dir : candidates)
    {
        for (const QString& libName : libNames)
        {
            if (QFile::exists(QDir(dir).absoluteFilePath(libName)))
            {
                dllDir=dir;
                targetLibName=libName;
                break;
            }
        }
        if (!dllDir.isEmpty()) 
            break;
    }

    // On Linux/Unix, if not found in candidate paths, try system default load
    if (dllDir.isEmpty())
    {
        #ifndef _WIN32
        m_lib.setFileName("vlc");
        if (!m_lib.load())
            m_lib.setFileName("libvlc.so.5");
        #endif
        if (!m_lib.isLoaded() && !m_lib.load())
        {
            qWarning()<<"[VlcEngine] Could not find libvlc in candidate paths or system library path.";
            return false;
        }
    }
    else
    {
        QString dllPath=QDir(dllDir).absoluteFilePath(targetLibName);
        m_lib.setFileName(dllPath);
        if (!m_lib.load())
        {
            qWarning()<<"[VlcEngine] Failed to load"<<dllPath<<": "<<m_lib.errorString();
            return false;
        }
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

    // Configure custom PCM audio callbacks
    if (fn_audio_set_format && fn_audio_set_callbacks)
    {
        fn_audio_set_format(m_vlcPlayer, "S16N", 44100, 2);
        fn_audio_set_callbacks(m_vlcPlayer, &VlcEngine::vlcAudioPlayCallback, &VlcEngine::vlcAudioPauseCallback, &VlcEngine::vlcAudioResumeCallback,
                               &VlcEngine::vlcAudioFlushCallback, &VlcEngine::vlcAudioDrainCallback, this);
    }

    SoftwareAudioMixer::instance().registerEngine(this);

    // Subscribe to events
    if (fn_event_manager && fn_event_attach)
    {
        auto em=fn_event_manager(m_vlcPlayer);
        fn_event_attach(em, VLC_EVENT_MediaPlayerPlaying, &VlcEngine::vlcEventCallback, this);
        fn_event_attach(em, VLC_EVENT_MediaPlayerPaused, &VlcEngine::vlcEventCallback, this);
        fn_event_attach(em, VLC_EVENT_MediaPlayerStopped, &VlcEngine::vlcEventCallback, this);
        fn_event_attach(em, VLC_EVENT_MediaPlayerEndReached, &VlcEngine::vlcEventCallback, this);
        fn_event_attach(em, VLC_EVENT_MediaPlayerEncounteredError, &VlcEngine::vlcEventCallback, this);
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
    RESOLVE(fn_audio_set_callbacks,  pfn_libvlc_audio_set_callbacks,                "libvlc_audio_set_callbacks");
    RESOLVE(fn_audio_set_format,     pfn_libvlc_audio_set_format,                   "libvlc_audio_set_format");

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

    clearPcmBuffer();

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

    if (fn_audio_set_volume)
        fn_audio_set_volume(m_vlcPlayer, 100);

    // Re-apply equalizer if one is active
    if (m_vlcEqualizer)
        fn_player_set_equalizer(m_vlcPlayer, m_vlcEqualizer);

    fn_player_play(m_vlcPlayer);
}

void VlcEngine::resume()
{
    if (!m_vlcPlayer)
        return;
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
    clearPcmBuffer();
    setState(Stopped);
}

void VlcEngine::setPosition(qint64 positionMs)
{
    if (!m_vlcPlayer)
        return;
    clearPcmBuffer();
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
void VlcEngine::setVolume(int percent) { m_volume=qBound(0, percent, 100); }

int VlcEngine::volume() const { return m_volume; }

void VlcEngine::setMuted(bool muted) { m_muted=muted; }

void VlcEngine::setSoftwareVolume(float vol) { m_softwareVolume = qBound(0.0f, vol, 1.0f); }

float VlcEngine::softwareVolume() const { return m_softwareVolume; }

float VlcEngine::effectiveVolume() const
{
    if (m_muted) return 0.0f;
    return m_softwareVolume*(m_volume/100.0f);
}

//  PCM Software Mixing
int VlcEngine::pcmBufferFrames() const
{
    QMutexLocker locker(&m_pcmMutex);
    return m_pcmBuffer.size()/4;
}

void VlcEngine::clearPcmBuffer()
{
    QMutexLocker locker(&m_pcmMutex);
    m_pcmBuffer.clear();
}

void VlcEngine::mixPcmFrames(int16_t* mixOut, int framesToProcess)
{
    QMutexLocker locker(&m_pcmMutex);
    int availFrames=m_pcmBuffer.size()/4;
    int framesToTake=qMin(framesToProcess, availFrames);
    if (framesToTake <= 0)
        return;

    const int16_t* inSamples=reinterpret_cast<const int16_t*>(m_pcmBuffer.constData());
    float vol=effectiveVolume();

    for (int i=0; i<framesToTake*2; ++i)
    {
        int32_t val=mixOut[i]+static_cast<int32_t>(inSamples[i]*vol);
        if (val > 32767) 
            val=32767;
        else if (val < -32768) 
            val=-32768;
        mixOut[i]=static_cast<int16_t>(val);
    }

    m_pcmBuffer.remove(0, framesToTake*4);
}

//  libVLC Audio Callbacks
void VlcEngine::vlcAudioPlayCallback(void *data, const void *samples, unsigned count, int64_t pts)
{
    Q_UNUSED(pts);
    VlcEngine *self=static_cast<VlcEngine*>(data);
    if (!self) 
        return;

    int bytes=count*4;
    {
        QMutexLocker locker(&self->m_pcmMutex);
        if (self->m_pcmBuffer.size() < 44100*4*2) // max 2s buffer cap
            self->m_pcmBuffer.append(reinterpret_cast<const char*>(samples), bytes);
    }

    SoftwareAudioMixer::instance().mixAndWrite();
}

void VlcEngine::vlcAudioPauseCallback(void *data, int64_t pts)
{
    Q_UNUSED(data);
    Q_UNUSED(pts);
}

void VlcEngine::vlcAudioResumeCallback(void *data, int64_t pts)
{
    Q_UNUSED(data);
    Q_UNUSED(pts);
}

void VlcEngine::vlcAudioFlushCallback(void *data, int64_t pts)
{
    Q_UNUSED(pts);
    VlcEngine *self=static_cast<VlcEngine*>(data);
    if (!self) return;
    self->clearPcmBuffer();
}

void VlcEngine::vlcAudioDrainCallback(void *data) { Q_UNUSED(data); }

//  Equalizer
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
    if (m_vlcPlayer && fn_player_set_equalizer)
        fn_player_set_equalizer(m_vlcPlayer, nullptr);
}

unsigned VlcEngine::equalizerBandCount() { return 10; }

float VlcEngine::equalizerBandFrequency(unsigned index)
{
    static const float freqs[]={60, 170, 310, 600, 1000, 3000, 6000, 12000, 14000, 16000};
    if (index<10)
        return freqs[index];
    return 0;
}

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

void VlcEngine::vlcEventCallback(const libvlc_event_t* event, void* userData)
{
    VlcEngine* self=static_cast<VlcEngine*>(userData);
    auto header=reinterpret_cast<const VlcEventHeader*>(event);
    int eventType=header->type;
    QMetaObject::invokeMethod(self, [self, eventType]() {self->handleVlcEventType(eventType);}, Qt::QueuedConnection);
}

void VlcEngine::handleVlcEventType(int type)
{
    switch (type)
    {
    case VLC_EVENT_MediaPlayerPlaying:
        setState(Playing);
        if (fn_audio_set_volume)
            fn_audio_set_volume(m_vlcPlayer, 100);
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

#include "VlcEngine.moc"