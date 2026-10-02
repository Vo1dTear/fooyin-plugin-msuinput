#pragma once

#include <core/engine/audioinput.h>
#include <QFile>

namespace Fooyin::MSU {

    class MSUDecoder : public AudioDecoder
    {
    public:
        MSUDecoder();
        qint64 loopTimeMs() const { return m_loopTimeMs; }
        quint64 loopFrame() const { return m_loopFrame; }

        void setGain(double db) { m_gainDb = db; }
        void setLoopCount(quint32 count) { m_loopCount = count; }
        void setEnableLoop(bool enabled) { m_enableLoop = enabled; }

        [[nodiscard]] QStringList extensions() const override;
        [[nodiscard]] bool isSeekable() const override;
        [[nodiscard]] uint64_t totalFrames() const { return m_totalFrames; }
        [[nodiscard]] const AudioFormat& format() const { return m_format; }

        std::optional<AudioFormat> init(const AudioSource& source, const Track& track, DecoderOptions options) override;
        void stop() override;
        void seek(uint64_t pos) override;
        AudioBuffer readBuffer(size_t bytes) override;

    private:
        double m_gainDb{0.0}; // value in dB

        QIODevice* m_file{nullptr};
        AudioFormat m_format;
        uint64_t m_currentFrame{0};
        quint64 m_totalFrames{0};
        qint64 m_dataOffset{0};
        quint64 m_loopFrame{0};

        // --- loop point ---
        qint64 m_loopTimeMs{0};  // loop point in milliseconds

        // --- loop count ---
        quint32 m_loopCount{0}; // 0 = infinite
        quint32 m_loopsDone{0};
        bool m_enableLoop{true};
        DecoderOptions m_options;
    };

    class MSUReader : public AudioReader
    {
    public:
        [[nodiscard]] QStringList extensions() const override;
        [[nodiscard]] bool canReadCover() const override;
        [[nodiscard]] bool canWriteMetaData() const override;

        bool readTrack(const AudioSource& source, Track& track) override;
    };

} // namespace Fooyin::MSU
