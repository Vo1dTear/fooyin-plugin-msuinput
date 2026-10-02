#include "msuinput.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QtEndian>

#include <algorithm>
#include <cstdlib>
#include <iostream>

using namespace Fooyin;
using namespace Fooyin::MSU;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

// Unbuffered device allows precise failures and reads split across frame boundaries.
class Device : public QBuffer {
public:
    explicit Device(const QByteArray& data) {
        setData(data);
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    }
    qint64 chunk{1000000};
    qint64 failAt{-1};
    bool returnEof{false};
    bool failSeek{false};
    bool sequential{false};
    bool isSequential() const override { return sequential; }
    bool seek(qint64 offset) override { return !failSeek && QBuffer::seek(offset); }
protected:
    qint64 readData(char* data, qint64 size) override {
        if (failAt >= 0 && pos() >= failAt) {
            setErrorString(QStringLiteral("Injected read failure"));
            return returnEof ? 0 : -1;
        }
        size = std::min(size, chunk);
        if (failAt >= 0) size = std::min(size, failAt - pos());
        return QBuffer::readData(data, size);
    }
};

QByteArray fileData(quint32 loop = 2) {
    QByteArray result("MSU1", 4);
    char encoded[4];
    qToLittleEndian(loop, encoded);
    result.append(encoded, 4);
    for (int i = 0; i < 20; ++i) result.append(char(i));
    return result;
}

bool init(MSUDecoder& decoder, QIODevice& device, AudioDecoder::DecoderOptions options = {}) {
    AudioSource source;
    source.device = &device;
    return decoder.init(source, Track{}, options).has_value();
}

QByteArray collect(MSUDecoder& decoder, size_t chunk = 12) {
    QByteArray result;
    for (int i = 0; i < 100; ++i) {
        auto read = decoder.readAudio(chunk);
        if (read.status == AudioDecoder::ReadStatus::EndOfStream) return result;
        require(read.status == AudioDecoder::ReadStatus::DecodedAudio, "Unexpected decode error");
        result.append(reinterpret_cast<const char*>(read.buffer.data()), read.buffer.byteCount());
    }
    require(false, "Decoder did not terminate");
    return {};
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const auto valid = fileData();
    for (const auto& invalid : {QByteArray{}, valid.left(7), valid.left(8),
                               valid.left(9), valid.left(27), QByteArray("BAD!") + valid.mid(4)}) {
        Device device(invalid);
        MSUDecoder decoder;
        require(!init(decoder, device), "Invalid file accepted");
        require(decoder.readAudio(12).status == AudioDecoder::ReadStatus::Error, "Missing init error");
    }
    for (int fault = 0; fault < 4; ++fault) {
        Device device(valid);
        device.failSeek = fault == 0;
        device.sequential = fault == 1;
        if (fault == 2) device.close();
        if (fault == 3) device.failAt = 6;
        MSUDecoder decoder;
        require(!init(decoder, device), "Invalid device accepted");
    }
    for (auto chunk : {4, 12, 16, 4096, 15}) {
        Device device(valid);
        device.chunk = 3;
        MSUDecoder decoder;
        decoder.setEnableLoop(false);
        require(init(decoder, device), "Short header reads rejected");
        require(collect(decoder, chunk) == valid.mid(8), "PCM lost across short reads or final buffer");
    }
    for (int mode = 0; mode < 4; ++mode) {
        Device device(valid);
        MSUDecoder decoder;
        decoder.setLoopCount(mode == 0 ? 2 : 0);
        const auto options = mode == 1 ? AudioDecoder::DecoderOptions(AudioDecoder::NoLooping)
                           : mode == 2 ? AudioDecoder::DecoderOptions(AudioDecoder::NoInfiniteLooping)
                           : mode == 3 ? AudioDecoder::DecoderOptions(AudioDecoder::NoLooping | AudioDecoder::NoInfiniteLooping)
                                       : AudioDecoder::DecoderOptions{};
        const int repeats = mode == 0 ? 1 : mode == 2 ? 1 : 0;
        auto expected = valid.mid(8);
        for (int i = 0; i < repeats; ++i) expected += valid.mid(16);
        for (int pass = 0; pass < 2; ++pass) {
            require(init(decoder, device, options), "Valid init failed");
            require(collect(decoder) == expected, "Loop PCM or reinitialization mismatch");
        }
    }
    for (quint32 count = 1; count <= 16; ++count) {
        Device device(valid);
        MSUDecoder decoder;
        decoder.setLoopCount(count);
        require(init(decoder, device), "Playback count init failed");
        auto expected = valid.mid(8);
        for (quint32 pass = 1; pass < count; ++pass) expected += valid.mid(16);
        require(collect(decoder) == expected, "Playback total-pass count mismatch");
    }
    // Conversion must preserve finite user repeats despite generic loop restrictions.
    for (bool enabled : {false, true}) {
        for (quint32 count = 0; count <= 16; ++count) {
            for (bool restricted : {false, true}) {
                Device device(valid);
                MSUDecoder decoder;
                decoder.setEnableLoop(enabled);
                decoder.setLoopCount(count);
                auto options = AudioDecoder::DecoderOptions(AudioDecoder::ForConversion);
                if (restricted) options |= AudioDecoder::NoLooping | AudioDecoder::NoInfiniteLooping;
                require(init(decoder, device, options), "Conversion init failed");
                const auto repeats = !enabled ? 0 : count > 0 ? count - 1 : restricted ? 0 : 1;
                auto expected = valid.mid(8);
                for (quint32 i = 0; i < repeats; ++i) expected += valid.mid(16);
                require(collect(decoder) == expected, "Conversion repeat duration/content mismatch");
            }
        }
    }
    for (auto loop : {quint32(0), quint32(5), quint32(0xffffffff)}) {
        Device device(fileData(loop));
        MSUDecoder decoder;
        decoder.setLoopCount(2);
        require(init(decoder, device), "Loop fallback rejected");
        require(collect(decoder) == valid.mid(8) + valid.mid(8), "Loop fallback mismatch");
    }
    for (bool eof : {false, true}) {
        Device device(valid);
        MSUDecoder decoder;
        require(init(decoder, device), "Read error setup failed");
        device.failAt = 15; // One full frame plus an incomplete frame.
        device.returnEof = eof;
        auto first = decoder.readAudio(12);
        require(first.status == AudioDecoder::ReadStatus::DecodedAudio && first.buffer.byteCount() == 4,
                "Complete frames before error lost");
        auto second = decoder.readAudio(12);
        require(second.status == AudioDecoder::ReadStatus::Error && !second.error.isEmpty(),
                "Read failure treated as normal EOF");
        require(decoder.readAudio(12).status == AudioDecoder::ReadStatus::Error, "Error not latched");
    }
    for (bool loopSeek : {false, true}) {
        Device device(valid);
        MSUDecoder decoder;
        require(init(decoder, device), "Seek test init failed");
        device.failSeek = true;
        if (loopSeek) {
            auto first = decoder.readAudio(24);
            require(first.status == AudioDecoder::ReadStatus::DecodedAudio && first.buffer.byteCount() == 20,
                    "Audio before failed loop seek lost");
        } else decoder.seek(0);
        require(decoder.readAudio(12).status == AudioDecoder::ReadStatus::Error, "Seek failure ignored");
    }
    Device device(valid);
    MSUDecoder decoder;
    require(init(decoder, device), "Infinite loop init failed");
    auto read = decoder.readAudio(4096);
    require(read.status == AudioDecoder::ReadStatus::DecodedAudio && read.buffer.byteCount() == 4096,
            "Infinite loop stopped");
    Device bad(QByteArray("invalid"));
    require(!init(decoder, bad), "Invalid reinit accepted");
    require(decoder.readAudio(12).status == AudioDecoder::ReadStatus::Error, "Stale decoder state reused");
    std::cout << "Decoder validation and I/O regression tests passed\n";
}
