/**
 * @file VlcPlayer.cpp
 * @brief Implementation of self-contained dynamic libvlc wrapper for Qt 6.
 */

#include "VlcPlayer.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QStandardPaths>
#include <QProcessEnvironment>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

VlcPlayer::VlcPlayer(QObject* parent)
    : QObject(parent)
{
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(100); // 100 ms polling interval for smooth position updates
    connect(m_pollTimer, &QTimer::timeout, this, &VlcPlayer::onPollTimer);
}

VlcPlayer::~VlcPlayer()
{
    stop();
    releaseCurrentMedia();

    if (m_equalizer && sym_libvlc_audio_equalizer_release) {
        sym_libvlc_audio_equalizer_release(m_equalizer);
        m_equalizer = nullptr;
    }

    if (m_vlcInstance && sym_libvlc_release) {
        sym_libvlc_release(m_vlcInstance);
        m_vlcInstance = nullptr;
    }
}

QString VlcPlayer::findLibVlcPath(const QString& customPath)
{
    if (!customPath.isEmpty() && QFileInfo::exists(customPath)) {
        if (QFileInfo(customPath).isDir()) {
            QString dllInDir = QDir(customPath).filePath("libvlc.dll");
            if (QFileInfo::exists(dllInDir)) {
                return QDir::toNativeSeparators(dllInDir);
            }
        }
        return QDir::toNativeSeparators(customPath);
    }

    // 1. Check current application directory
    QString appDir = QCoreApplication::applicationDirPath();
    QString localDll = QDir(appDir).filePath("libvlc.dll");
    if (QFileInfo::exists(localDll)) {
        return QDir::toNativeSeparators(localDll);
    }

    // 2. Check local project directory (Utils/VLC-Qt/bin in repo)
    QString repoDll = QDir::current().filePath("Utils/VLC-Qt/bin/libvlc.dll");
    if (QFileInfo::exists(repoDll)) {
        return QDir::toNativeSeparators(repoDll);
    }

    // 3. Check default 64-bit Windows installation directory (recommended for Qt 6 x64)
    QString win64Dll = "C:/Program Files/VideoLAN/VLC/libvlc.dll";
    if (QFileInfo::exists(win64Dll)) {
        return QDir::toNativeSeparators(win64Dll);
    }

    // 4. Check 32-bit Windows installation directory (will be diagnosed if loaded on x64)
    QString win32Dll = "C:/Program Files (x86)/VideoLAN/VLC/libvlc.dll";
    if (QFileInfo::exists(win32Dll)) {
        return QDir::toNativeSeparators(win32Dll);
    }

    // Default to plain filename to allow standard PATH lookup
    return "libvlc.dll";
}

bool VlcPlayer::init(const QString& customVlcPath)
{
    if (m_initialized) {
        return true;
    }

    QString dllPath = findLibVlcPath(customVlcPath);
    m_vlcDllPath = dllPath;

    // Configure Windows DLL search directory so libvlc can locate libvlccore.dll and plugins
    QFileInfo dllInfo(dllPath);
    if (dllInfo.exists()) {
        QString dllDir = QDir::toNativeSeparators(dllInfo.absolutePath());
#ifdef Q_OS_WIN
        ::SetDllDirectoryW((LPCWSTR)dllDir.utf16());
#endif
        // Ensure VLC finds its plugins directory in the same install tree
        QString pluginDir = QDir(dllInfo.absolutePath()).filePath("plugins");
        if (QFileInfo::exists(pluginDir)) {
            qputenv("VLC_PLUGIN_PATH", QDir::toNativeSeparators(pluginDir).toUtf8());
        }
    }

    m_vlcLib.setFileName(dllPath);
    if (!m_vlcLib.load()) {
        QString err = m_vlcLib.errorString();
        // Check for common Windows architecture mismatch (Win32 Error 193)
        if (err.contains("193") || err.contains("not a valid Win32 application", Qt::CaseInsensitive)
            || err.contains("not a valid win32 application", Qt::CaseInsensitive)) {
            m_lastError = QString("VLC DLL architecture mismatch (Error 193): Qt 6 is 64-bit (x86_64), "
                                  "but the VLC DLL at '%1' is 32-bit. Please install 64-bit VLC.")
                              .arg(dllPath);
        } else {
            m_lastError = QString("Failed to load VLC library '%1': %2").arg(dllPath, err);
        }
        qWarning() << "VlcPlayer::init -" << m_lastError;
        emit errorOccurred(m_lastError);
        return false;
    }

    if (!resolveSymbols(dllPath)) {
        return false;
    }

    // CRITICAL: Pass arguments to libvlc_new() that prevent GUI/interface plugins from loading
    // Qt 5 DLLs into our Qt 6 host application (preventing symbol clashes and DLL hell).
    const char* vlcArgs[] = {
        "--intf=dummy",              // Do NOT load Qt/GUI interface plugins (prevents Qt5/Qt6 conflict!)
        "--no-video",                // Audio-only mode; do not initialize video output modules
        "--no-qt-privacy-ask",       // Disable Qt privacy dialogs
        "--no-interact",             // Suppress interactive prompts
        "--ignore-config",           // Ignore user's VLC config file
        "--quiet"                    // Suppress verbose console logging
    };
    int argc = sizeof(vlcArgs) / sizeof(vlcArgs[0]);

    m_vlcInstance = sym_libvlc_new(argc, vlcArgs);
    if (!m_vlcInstance) {
        if (sym_libvlc_errmsg) {
            const char* msg = sym_libvlc_errmsg();
            m_lastError = msg ? QString::fromUtf8(msg) : "libvlc_new() returned null instance";
        } else {
            m_lastError = "libvlc_new() failed to create instance";
        }
        qWarning() << "VlcPlayer::init -" << m_lastError;
        emit errorOccurred(m_lastError);
        return false;
    }

    m_initialized = true;
    m_pollTimer->start();
    qDebug() << "VlcPlayer successfully initialized with VLC version:" << vlcVersion() << "from:" << dllPath;
    return true;
}

bool VlcPlayer::resolveSymbols(const QString& dllPath)
{
    sym_libvlc_get_version = (fn_libvlc_get_version)m_vlcLib.resolve("libvlc_get_version");
    sym_libvlc_errmsg = (fn_libvlc_errmsg)m_vlcLib.resolve("libvlc_errmsg");
    sym_libvlc_new = (fn_libvlc_new)m_vlcLib.resolve("libvlc_new");
    sym_libvlc_release = (fn_libvlc_release)m_vlcLib.resolve("libvlc_release");

    sym_libvlc_media_new_path = (fn_libvlc_media_new_path)m_vlcLib.resolve("libvlc_media_new_path");
    sym_libvlc_media_new_location = (fn_libvlc_media_new_location)m_vlcLib.resolve("libvlc_media_new_location");
    sym_libvlc_media_release = (fn_libvlc_media_release)m_vlcLib.resolve("libvlc_media_release");
    sym_libvlc_media_get_meta = (fn_libvlc_media_get_meta)m_vlcLib.resolve("libvlc_media_get_meta");
    sym_libvlc_media_parse_with_options = (fn_libvlc_media_parse_with_options)m_vlcLib.resolve("libvlc_media_parse_with_options");
    sym_libvlc_media_parse = (fn_libvlc_media_parse)m_vlcLib.resolve("libvlc_media_parse");
    sym_libvlc_media_get_duration = (fn_libvlc_media_get_duration)m_vlcLib.resolve("libvlc_media_get_duration");

    sym_libvlc_media_player_new = (fn_libvlc_media_player_new)m_vlcLib.resolve("libvlc_media_player_new");
    sym_libvlc_media_player_new_from_media = (fn_libvlc_media_player_new_from_media)m_vlcLib.resolve("libvlc_media_player_new_from_media");
    sym_libvlc_media_player_release = (fn_libvlc_media_player_release)m_vlcLib.resolve("libvlc_media_player_release");
    sym_libvlc_media_player_set_media = (fn_libvlc_media_player_set_media)m_vlcLib.resolve("libvlc_media_player_set_media");
    sym_libvlc_media_player_play = (fn_libvlc_media_player_play)m_vlcLib.resolve("libvlc_media_player_play");
    sym_libvlc_media_player_pause = (fn_libvlc_media_player_pause)m_vlcLib.resolve("libvlc_media_player_pause");
    sym_libvlc_media_player_set_pause = (fn_libvlc_media_player_set_pause)m_vlcLib.resolve("libvlc_media_player_set_pause");
    sym_libvlc_media_player_stop = (fn_libvlc_media_player_stop)m_vlcLib.resolve("libvlc_media_player_stop");
    sym_libvlc_media_player_stop_async = (fn_libvlc_media_player_stop_async)m_vlcLib.resolve("libvlc_media_player_stop_async");
    sym_libvlc_media_player_is_playing = (fn_libvlc_media_player_is_playing)m_vlcLib.resolve("libvlc_media_player_is_playing");
    sym_libvlc_media_player_get_time = (fn_libvlc_media_player_get_time)m_vlcLib.resolve("libvlc_media_player_get_time");
    sym_libvlc_media_player_set_time = (fn_libvlc_media_player_set_time)m_vlcLib.resolve("libvlc_media_player_set_time");
    sym_libvlc_media_player_get_length = (fn_libvlc_media_player_get_length)m_vlcLib.resolve("libvlc_media_player_get_length");
    sym_libvlc_media_player_get_state = (fn_libvlc_media_player_get_state)m_vlcLib.resolve("libvlc_media_player_get_state");

    sym_libvlc_audio_get_volume = (fn_libvlc_audio_get_volume)m_vlcLib.resolve("libvlc_audio_get_volume");
    sym_libvlc_audio_set_volume = (fn_libvlc_audio_set_volume)m_vlcLib.resolve("libvlc_audio_set_volume");
    sym_libvlc_audio_get_mute = (fn_libvlc_audio_get_mute)m_vlcLib.resolve("libvlc_audio_get_mute");
    sym_libvlc_audio_set_mute = (fn_libvlc_audio_set_mute)m_vlcLib.resolve("libvlc_audio_set_mute");

    sym_libvlc_audio_equalizer_new = (fn_libvlc_audio_equalizer_new)m_vlcLib.resolve("libvlc_audio_equalizer_new");
    sym_libvlc_audio_equalizer_release = (fn_libvlc_audio_equalizer_release)m_vlcLib.resolve("libvlc_audio_equalizer_release");
    sym_libvlc_audio_equalizer_set_amp_at_index = (fn_libvlc_audio_equalizer_set_amp_at_index)m_vlcLib.resolve("libvlc_audio_equalizer_set_amp_at_index");
    sym_libvlc_audio_equalizer_set_preamp = (fn_libvlc_audio_equalizer_set_preamp)m_vlcLib.resolve("libvlc_audio_equalizer_set_preamp");
    sym_libvlc_media_player_set_equalizer = (fn_libvlc_media_player_set_equalizer)m_vlcLib.resolve("libvlc_media_player_set_equalizer");
    sym_libvlc_free = (fn_libvlc_free)m_vlcLib.resolve("libvlc_free");

    // Ensure all critical symbols are loaded
    if (!sym_libvlc_new || !sym_libvlc_release || !sym_libvlc_media_player_new_from_media ||
        !sym_libvlc_media_player_play || !sym_libvlc_media_player_get_time) {
        m_lastError = QString("Missing critical symbols in VLC DLL at '%1'.").arg(dllPath);
        qWarning() << "VlcPlayer::resolveSymbols -" << m_lastError;
        emit errorOccurred(m_lastError);
        return false;
    }

    return true;
}

bool VlcPlayer::isInitialized() const
{
    return m_initialized && (m_vlcInstance != nullptr);
}

QString VlcPlayer::lastError() const
{
    return m_lastError;
}

QString VlcPlayer::vlcVersion() const
{
    if (sym_libvlc_get_version) {
        const char* v = sym_libvlc_get_version();
        return v ? QString::fromUtf8(v) : "Unknown";
    }
    return "Unknown";
}

QString VlcPlayer::vlcDllPath() const
{
    return m_vlcDllPath;
}

void VlcPlayer::releaseCurrentMedia()
{
    if (m_mediaPlayer && sym_libvlc_media_player_release) {
        sym_libvlc_media_player_release(m_mediaPlayer);
        m_mediaPlayer = nullptr;
    }
    if (m_media && sym_libvlc_media_release) {
        sym_libvlc_media_release(m_media);
        m_media = nullptr;
    }
    m_metadata.clear();
}

void VlcPlayer::setSource(const QUrl& url)
{
    if (!init()) {
        return;
    }

    stop();
    releaseCurrentMedia();

    m_currentUrl = url;
    if (url.isLocalFile()) {
        m_currentFilePath = url.toLocalFile();
        QString nativePath = QDir::toNativeSeparators(m_currentFilePath);
        m_media = sym_libvlc_media_new_path(m_vlcInstance, nativePath.toUtf8().constData());
    } else {
        m_currentFilePath = url.toString();
        m_media = sym_libvlc_media_new_location(m_vlcInstance, url.toString().toUtf8().constData());
    }

    if (!m_media) {
        m_lastError = QString("Failed to create libvlc media for '%1'").arg(m_currentFilePath);
        qWarning() << "VlcPlayer::setSource -" << m_lastError;
        m_status = Error;
        emit mediaStatusChanged(m_status);
        emit errorOccurred(m_lastError);
        return;
    }

    m_mediaPlayer = sym_libvlc_media_player_new_from_media(m_media);
    if (!m_mediaPlayer) {
        m_lastError = "Failed to create media player from media";
        qWarning() << "VlcPlayer::setSource -" << m_lastError;
        m_status = Error;
        emit mediaStatusChanged(m_status);
        emit errorOccurred(m_lastError);
        return;
    }

    // Restore volume, mute, and equalizer on the newly created media player
    if (sym_libvlc_audio_set_volume) {
        sym_libvlc_audio_set_volume(m_mediaPlayer, m_volume);
    }
    if (sym_libvlc_audio_set_mute) {
        sym_libvlc_audio_set_mute(m_mediaPlayer, m_muted ? 1 : 0);
    }
    if (m_equalizerEnabled && !m_currentEqBands.empty()) {
        setEqualizer(m_currentEqBands, m_currentPreamp);
    }

    m_status = Loaded;
    emit mediaStatusChanged(m_status);
    updateMetadata();
}

void VlcPlayer::setSource(const QString& filePath)
{
    if (QFileInfo(filePath).exists()) {
        setSource(QUrl::fromLocalFile(filePath));
    } else {
        setSource(QUrl(filePath));
    }
}

QUrl VlcPlayer::source() const
{
    return m_currentUrl;
}

QString VlcPlayer::currentFilePath() const
{
    return m_currentFilePath;
}

void VlcPlayer::play()
{
    if (!m_mediaPlayer || !sym_libvlc_media_player_play) {
        return;
    }

    int ret = sym_libvlc_media_player_play(m_mediaPlayer);
    if (ret != 0) {
        m_lastError = "libvlc_media_player_play() failed";
        m_status = Error;
        emit mediaStatusChanged(m_status);
        emit errorOccurred(m_lastError);
    } else {
        m_status = Playing;
        emit mediaStatusChanged(m_status);
    }
}

void VlcPlayer::pause()
{
    if (!m_mediaPlayer || !sym_libvlc_media_player_pause) {
        return;
    }
    sym_libvlc_media_player_pause(m_mediaPlayer);
}

void VlcPlayer::stop()
{
    if (!m_mediaPlayer) {
        return;
    }
    if (sym_libvlc_media_player_stop_async) {
        sym_libvlc_media_player_stop_async(m_mediaPlayer);
    } else if (sym_libvlc_media_player_stop) {
        sym_libvlc_media_player_stop(m_mediaPlayer);
    }
    m_status = Stopped;
    emit mediaStatusChanged(m_status);
}

void VlcPlayer::setPosition(qint64 positionMs)
{
    if (m_mediaPlayer && sym_libvlc_media_player_set_time) {
        sym_libvlc_media_player_set_time(m_mediaPlayer, positionMs);
    }
}

qint64 VlcPlayer::position() const
{
    if (m_mediaPlayer && sym_libvlc_media_player_get_time) {
        return (qint64)sym_libvlc_media_player_get_time(m_mediaPlayer);
    }
    return 0;
}

qint64 VlcPlayer::duration() const
{
    if (m_mediaPlayer && sym_libvlc_media_player_get_length) {
        qint64 len = (qint64)sym_libvlc_media_player_get_length(m_mediaPlayer);
        if (len > 0) return len;
    }
    if (m_media && sym_libvlc_media_get_duration) {
        qint64 len = (qint64)sym_libvlc_media_get_duration(m_media);
        if (len > 0) return len;
    }
    return 0;
}

bool VlcPlayer::isPlaying() const
{
    if (m_mediaPlayer && sym_libvlc_media_player_is_playing) {
        return sym_libvlc_media_player_is_playing(m_mediaPlayer) != 0;
    }
    return false;
}

bool VlcPlayer::isPaused() const
{
    if (m_mediaPlayer && sym_libvlc_media_player_get_state) {
        int state = sym_libvlc_media_player_get_state(m_mediaPlayer);
        return state == 4; // libvlc_Paused
    }
    return false;
}

VlcPlayer::MediaStatus VlcPlayer::mediaStatus() const
{
    return m_status;
}

void VlcPlayer::setVolume(int volume)
{
    m_volume = qBound(0, volume, 200);
    if (m_mediaPlayer && sym_libvlc_audio_set_volume) {
        sym_libvlc_audio_set_volume(m_mediaPlayer, m_volume);
    }
    emit volumeChanged(m_volume);
}

void VlcPlayer::setVolumeLinear(qreal volumeLinear)
{
    int intVol = qRound(volumeLinear * 100.0);
    setVolume(intVol);
}

int VlcPlayer::volume() const
{
    if (m_mediaPlayer && sym_libvlc_audio_get_volume) {
        return sym_libvlc_audio_get_volume(m_mediaPlayer);
    }
    return m_volume;
}

void VlcPlayer::setMuted(bool muted)
{
    m_muted = muted;
    if (m_mediaPlayer && sym_libvlc_audio_set_mute) {
        sym_libvlc_audio_set_mute(m_mediaPlayer, muted ? 1 : 0);
    }
    emit mutedChanged(muted);
}

bool VlcPlayer::isMuted() const
{
    if (m_mediaPlayer && sym_libvlc_audio_get_mute) {
        return sym_libvlc_audio_get_mute(m_mediaPlayer) != 0;
    }
    return m_muted;
}

bool VlcPlayer::setEqualizer(const std::vector<int>& bandGains, float preamp)
{
    m_currentEqBands = bandGains;
    m_currentPreamp = preamp;

    if (!sym_libvlc_audio_equalizer_new || !sym_libvlc_media_player_set_equalizer) {
        return false;
    }

    if (!m_equalizer) {
        m_equalizer = sym_libvlc_audio_equalizer_new();
        if (!m_equalizer) {
            return false;
        }
    }

    if (sym_libvlc_audio_equalizer_set_preamp) {
        sym_libvlc_audio_equalizer_set_preamp(m_equalizer, preamp);
    }

    if (sym_libvlc_audio_equalizer_set_amp_at_index) {
        for (size_t idx = 0; idx < bandGains.size(); ++idx) {
            sym_libvlc_audio_equalizer_set_amp_at_index(m_equalizer, (float)bandGains[idx], (unsigned)idx);
        }
    }

    if (m_mediaPlayer) {
        sym_libvlc_media_player_set_equalizer(m_mediaPlayer, m_equalizer);
    }
    m_equalizerEnabled = true;
    return true;
}

void VlcPlayer::setEqualizerEnabled(bool enabled)
{
    m_equalizerEnabled = enabled;
    if (!m_mediaPlayer || !sym_libvlc_media_player_set_equalizer) {
        return;
    }
    if (enabled && m_equalizer) {
        sym_libvlc_media_player_set_equalizer(m_mediaPlayer, m_equalizer);
    } else {
        sym_libvlc_media_player_set_equalizer(m_mediaPlayer, nullptr);
    }
}

QString VlcPlayer::title() const
{
    return m_metadata.value("Title").toString();
}

QString VlcPlayer::artist() const
{
    return m_metadata.value("Artist").toString();
}

QString VlcPlayer::album() const
{
    return m_metadata.value("Album").toString();
}

QMap<QString, QVariant> VlcPlayer::metaData() const
{
    return m_metadata;
}

void VlcPlayer::updateMetadata()
{
    if (!m_media || !sym_libvlc_media_get_meta) {
        return;
    }

    auto fetchMeta = [this](int eMeta, const QString& key) {
        char* value = sym_libvlc_media_get_meta(m_media, eMeta);
        if (value) {
            QString s = QString::fromUtf8(value);
            m_metadata[key] = s;
            if (sym_libvlc_free) {
                sym_libvlc_free(value);
            } else {
                free(value);
            }
        }
    };

    fetchMeta(0, "Title");  // libvlc_meta_Title
    fetchMeta(1, "Artist"); // libvlc_meta_Artist
    fetchMeta(4, "Album");  // libvlc_meta_Album

    emit metaDataChanged();
}

void VlcPlayer::onPollTimer()
{
    if (!m_mediaPlayer || !sym_libvlc_media_player_get_state) {
        return;
    }

    int state = sym_libvlc_media_player_get_state(m_mediaPlayer);
    MediaStatus newStatus = m_status;
    switch (state) {
        case 0: // libvlc_NothingSpecial
            newStatus = Loaded;
            break;
        case 1: // libvlc_Opening
        case 2: // libvlc_Buffering
            newStatus = Loading;
            break;
        case 3: // libvlc_Playing
            newStatus = Playing;
            break;
        case 4: // libvlc_Paused
            newStatus = Paused;
            break;
        case 5: // libvlc_Stopped
            newStatus = Stopped;
            break;
        case 6: // libvlc_Ended
            newStatus = EndOfMedia;
            break;
        case 7: // libvlc_Error
            newStatus = Error;
            break;
    }

    if (newStatus != m_status) {
        m_status = newStatus;
        emit mediaStatusChanged(m_status);
        if (newStatus == Loaded || newStatus == Playing || newStatus == EndOfMedia) {
            updateMetadata();
        }
    }

    qint64 pos = position();
    if (pos != m_lastPosition && (m_status == Playing || m_status == Paused)) {
        m_lastPosition = pos;
        emit positionChanged(pos);
    }

    qint64 dur = duration();
    if (dur != m_lastDuration && dur > 0) {
        m_lastDuration = dur;
        emit durationChanged(dur);
    }
}
