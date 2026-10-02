#include "msuinput.h"
#include <QLoggingCategory>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <limits>

Q_LOGGING_CATEGORY(MSU_LOG, "fy.msu")

using namespace Qt::StringLiterals;

namespace {
    QStringList fileExtensions()
    {
        static const QStringList extensions = {u"pcm"_s};
        return extensions;
    }
} // namespace

namespace Fooyin::MSU {

    MSUDecoder::MSUDecoder() {
        // Configure example format
        m_format.setSampleFormat(SampleFormat::S16);
        m_format.setSampleRate(44100);
        m_format.setChannelCount(2);

        // Initialize default values
        m_loopCount = 0;     // 0 = infinite
        m_loopsDone = 0;
    }

    QStringList MSUDecoder::extensions() const { return fileExtensions(); }
    bool MSUDecoder::isSeekable() const { return m_file && !m_file->isSequential(); }

    std::optional<AudioFormat> MSUDecoder::init(
        const AudioSource& source,
        const Track& track,
        DecoderOptions options)
    {
        m_file = nullptr;
        m_error.clear();
        m_options = options;
        m_loopsDone = 0;
        m_currentFrame = 0;
        m_totalFrames = 0;
        m_loopFrame = 0;
        m_loopTimeMs = 0;
        m_dataOffset = 8;

        auto* device = source.device;
        if (!device || !device->isReadable() || device->isSequential()) {
            fail(QStringLiteral("MSU input requires a readable, seekable device"));
            return {};
        }
        const qint64 dataBytes = device->size() - m_dataOffset;
        if (dataBytes <= 0 || dataBytes % 4 != 0) {
            fail(QStringLiteral("Invalid MSU audio size: expected complete stereo frames"));
            return {};
        }
        if (!device->seek(0)) {
            fail(QStringLiteral("Failed to seek to MSU header"));
            return {};
        }
        char header[8];
        qint64 received = 0;
        while (received < 8) {
            const auto count = device->read(header + received, 8 - received);
            if (count <= 0) {
                fail(QStringLiteral("Failed to read complete MSU header"));
                return {};
            }
            received += count;
        }
        if (QByteArray(header, 4) != "MSU1") {
            fail(QStringLiteral("Invalid MSU1 signature"));
            return {};
        }
        m_totalFrames = static_cast<quint64>(dataBytes / 4);
        m_loopFrame = qFromLittleEndian<quint32>(header + 4);
        if (m_loopFrame >= m_totalFrames) {
            // Preserve the previous fallback for files with an unusable loop point.
            qCWarning(MSU_LOG) << "MSU loop point outside audio; looping from start";
            m_loopFrame = 0;
        }
        m_loopTimeMs = static_cast<qint64>(m_loopFrame * 1000 / 44100);
        m_file = device; // The complete header read leaves the device at the audio data.

        return m_format;
    }

    void MSUDecoder::fail(const QString& message) {
        m_error = message;
        qCWarning(MSU_LOG) << message;
    }

    void MSUDecoder::stop() {
        m_file = nullptr;
        m_currentFrame = 0;
        m_loopsDone = 0;
        m_error.clear();
    }

    void MSUDecoder::seek(uint64_t timeMs) {
        if (!m_file || !m_error.isEmpty()) return;

        // Clamp before multiplication to avoid overflow for very large seek requests.
        const uint64_t seconds = timeMs / 1000;
        const uint64_t targetFrame = seconds > m_totalFrames / 44100
                                         ? m_totalFrames
                                         : std::min<uint64_t>(m_totalFrames, seconds * 44100 + timeMs % 1000 * 44100 / 1000);
        if (!m_file->seek(m_dataOffset + static_cast<qint64>(targetFrame * 4))) {
            fail(QStringLiteral("Failed to seek in MSU audio"));
            return;
        }
        m_currentFrame = targetFrame;
        m_loopsDone = 0;
    }

    AudioDecoder::ReadResult MSUDecoder::readAudio(size_t bytes) {
        auto buffer = readBuffer(bytes);
        if (buffer.isValid() && buffer.byteCount() > 0)
            return ReadResult::data(std::move(buffer));
        if (!m_error.isEmpty())
            return ReadResult::errorResult(m_error);
        return ReadResult::endOfStream();
    }

    AudioBuffer MSUDecoder::readBuffer(size_t bytes) {
        if (!m_file || !m_error.isEmpty() || bytes == 0)
            return {};

        // Audio buffers must contain whole stereo frames and fit the API's int sizes.
        bytes = std::min(bytes, static_cast<size_t>(std::numeric_limits<int>::max()));
        bytes -= bytes % 4;
        if (bytes == 0) return {};
        AudioBuffer buffer{m_format, m_currentFrame / 44100 * 1000 + m_currentFrame % 44100 * 1000 / 44100};
        buffer.resize(bytes);

        qint64 bytesReadTotal = 0;
        char* writePtr = reinterpret_cast<char*>(buffer.data());

        while (bytesReadTotal < static_cast<qint64>(bytes)) {
            quint64 framesRemaining = m_totalFrames - m_currentFrame;

            if (framesRemaining == 0) {
                // --- Loop handling ---
                // Conversion renders explicitly configured finite repeats, even with NoLooping.
                const bool conversion = m_options.testFlag(ForConversion);
                const bool configuredConversionLoops = conversion && m_loopCount > 0;
                const bool noLooping = m_options.testFlag(NoLooping) && !configuredConversionLoops;
                // Positive counts include the initial pass; zero is the infinite setting.
                const bool infinite = m_loopCount == 0 && !conversion
                                      && !m_options.testFlag(NoInfiniteLooping);
                // Keep the bounded infinite fallback at one additional repeat.
                const quint32 repeatLimit = m_loopCount > 0 ? m_loopCount - 1 : 1;
                if (m_enableLoop && !noLooping
                    && (infinite || m_loopsDone < repeatLimit)) {
                    if (!m_file->seek(m_dataOffset + static_cast<qint64>(m_loopFrame * 4))) {
                        fail(QStringLiteral("Failed to seek to MSU loop point"));
                        break;
                    }
                    m_currentFrame = m_loopFrame;
                    ++m_loopsDone;
                } else {
                    m_currentFrame = m_totalFrames; // reached end
                    break; // Return any audio already read before signalling EOF.
                }
                framesRemaining = m_totalFrames - m_currentFrame;
            }

            qint64 bytesToRead = qMin<qint64>(
                bytes - bytesReadTotal,
                static_cast<qint64>(framesRemaining * 4)
            );

            // Short reads may split a frame. Accumulate before updating frame position.
            qint64 received = 0;
            while (received < bytesToRead) {
                const auto count = m_file->read(writePtr + bytesReadTotal + received,
                                                bytesToRead - received);
                if (count <= 0) {
                    fail(count < 0 ? QStringLiteral("MSU audio read failed: %1").arg(m_file->errorString())
                                   : QStringLiteral("Unexpected end of MSU audio"));
                    break;
                }
                received += count;
            }
            const auto completeBytes = received - received % 4;
            bytesReadTotal += completeBytes;
            m_currentFrame += static_cast<quint64>(completeBytes / 4);
            if (!m_error.isEmpty()) break;
        }

        if (bytesReadTotal == 0)
            return {};

        // Adjust buffer size to what was actually read
        if (bytesReadTotal < static_cast<qint64>(bytes)) {
            buffer.resize(static_cast<size_t>(bytesReadTotal));
        }

        // --- Apply gain ---
        if (m_gainDb != 0.0) {
            const float gainFactor = std::pow(10.0f, float(m_gainDb) / 20.0f);
            const int16_t* src = reinterpret_cast<const int16_t*>(buffer.data());
            int16_t* dst = reinterpret_cast<int16_t*>(buffer.data());

            const size_t samples = static_cast<size_t>(bytesReadTotal) / sizeof(int16_t);

            for (size_t i = 0; i < samples; ++i) {
                int val = int(src[i] * gainFactor);
                if (val > 32767) val = 32767;
                if (val < -32768) val = -32768;
                dst[i] = int16_t(val);
            }
        }

        return buffer;
    }

    QStringList MSUReader::extensions() const { return fileExtensions(); }
    bool MSUReader::canReadCover() const { return false; }
    bool MSUReader::canWriteMetaData() const { return false; }

    bool MSUReader::readTrack(const AudioSource& source, Track& track) {
        if(!source.device || source.device->size() == 0) {
            qCWarning(MSU_LOG) << "Invalid MSU file";
            return false;
        }

        // Create a temporary decoder with empty constructor
        auto tmpDecoder = std::make_unique<MSUDecoder>();

        // Initialize the decoder using init()
        auto formatOpt = tmpDecoder->init(source, track, {});
        if (!formatOpt.has_value()) {
            qCWarning(MSU_LOG) << "Failed to initialize MSU decoder";
            return false;
        }

        // Duration in milliseconds
        const auto totalFrames = tmpDecoder->totalFrames();
        const auto sampleRate  = tmpDecoder->format().sampleRate();

        track.setDuration(static_cast<int64_t>(totalFrames) * 1000 / sampleRate);
        track.setSampleRate(44100);
        track.setChannels(2);
        track.setBitDepth(16);
        track.setCodec(u"MSU-1"_s);
        //track.setCodec(u"PCM (MSU-1)"_s);
        track.setEncoding(u"Lossless"_s);

        // Add loop point information to the track's Details tab
        track.setExtraProperty(QStringLiteral("LOOPSTART - ms"), QString::number(tmpDecoder->loopTimeMs()));
        track.setExtraProperty(QStringLiteral("LOOPSTART - Samples"), QString::number(tmpDecoder->loopFrame()));

        // Add loop point information to the track's Metadata tab
        //track.setComment(QStringLiteral("LOOPSTART: %1").arg(tmpDecoder->loopFrame()));

        return true;
    }

} // namespace Fooyin::MSU
