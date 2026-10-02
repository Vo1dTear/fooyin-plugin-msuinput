#include <core/engine/audioinput.h> //
#include "msuinput.h"
#include <QLoggingCategory>

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
        m_options = options;
        m_loopsDone = 0;
        m_file = source.device;
        if (!m_file || m_file->size() <= 8) {
            qCWarning(MSU_LOG) << "Invalid MSU file";
            return {};
        }

        m_file->seek(4);

        quint32 loopSamples = 0;
        m_file->read(reinterpret_cast<char*>(&loopSamples), sizeof(loopSamples));

        m_loopFrame = loopSamples;
        m_dataOffset = 8;
        m_totalFrames = (m_file->size() - m_dataOffset) / 4;

        // Convert loop point to milliseconds
        m_loopTimeMs = static_cast<qint64>((m_loopFrame * 1000.0) / m_format.sampleRate());

        /*
         *        // Debug to confirm loop point
         *        qCInfo(MSU_LOG) << "Detected loop point:" << m_loopFrame
         *                                                   << "frames (" << m_loopTimeMs << "ms)";
         */

        m_currentFrame = 0;
        m_file->seek(m_dataOffset);

        return m_format;
    }

    void MSUDecoder::stop() {
        m_currentFrame = 0;
        m_loopsDone = 0;
    }

    void MSUDecoder::seek(uint64_t timeMs) {
        if (!m_file || !m_file->isOpen()) return;

        // Convert time to frames
        uint64_t targetFrame = static_cast<uint64_t>((timeMs * m_format.sampleRate()) / 1000.0);

        if (targetFrame > m_totalFrames) targetFrame = m_totalFrames;

        m_currentFrame = targetFrame;
        m_file->seek(m_dataOffset + m_format.bytesForFrames(static_cast<int>(m_currentFrame)));
        m_loopsDone = 0;
    }

    AudioBuffer MSUDecoder::readBuffer(size_t bytes) {
        if (!m_file || !m_file->isOpen() || bytes == 0)
            return {};

        AudioBuffer buffer{m_format, m_format.durationForFrames(static_cast<int>(m_currentFrame))};
        buffer.resize(bytes);

        qint64 bytesReadTotal = 0;
        char* writePtr = reinterpret_cast<char*>(buffer.data());

        while (bytesReadTotal < static_cast<qint64>(bytes)) {
            quint64 framesRemaining = m_totalFrames - m_currentFrame;

            if (framesRemaining == 0) {
                // --- Loop handling ---
                // Bound the default infinite setting to one repeat when requested by fooyin.
                const quint32 loopLimit = m_loopCount == 0 && m_options.testFlag(NoInfiniteLooping)
                                              ? 1 : m_loopCount;
                if (m_enableLoop && !m_options.testFlag(NoLooping)
                    && (loopLimit == 0 || m_loopsDone < loopLimit)) {
                    if (m_loopFrame > 0 && m_loopFrame < m_totalFrames) {
                        // Loop from loop point
                        m_currentFrame = m_loopFrame;
                    } else {
                        // No loop point → loop from start
                        m_currentFrame = 0;
                    }
                    m_file->seek(m_dataOffset + m_format.bytesForFrames(static_cast<int>(m_currentFrame)));
                    ++m_loopsDone;
                } else {
                    m_currentFrame = m_totalFrames; // reached end
                    break; // Return any audio already read before signalling EOF.
                }
                framesRemaining = m_totalFrames - m_currentFrame;
            }

            qint64 bytesToRead = qMin<qint64>(
                bytes - bytesReadTotal,
                m_format.bytesForFrames(static_cast<int>(framesRemaining))
            );

            qint64 read = m_file->read(writePtr + bytesReadTotal, bytesToRead);
            if (read <= 0) break;

            bytesReadTotal += read;
            m_currentFrame += m_format.framesForBytes(static_cast<int>(read));
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

#include "moc_msuinput.cpp"
