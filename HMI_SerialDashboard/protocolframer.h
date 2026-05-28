#ifndef PROTOCOLFRAMER_H
#define PROTOCOLFRAMER_H

#include <QByteArray>
#include <QString>
#include <QStringList>

struct ProtocolFrameConfig {
    enum class Mode {
        Raw,
        Line,
        Delimiter,
        FixedLength
    };

    Mode mode = Mode::Line;
    QByteArray delimiter = QByteArray("\n");
    int fixedLength = 64;
    int maxFrameBytes = 4096;
    bool trimCarriageReturn = true;

    static Mode modeFromString(const QString &text);
    static QString modeToString(Mode mode);
    static QByteArray parseEscapedBytes(const QString &text,
                                        const QByteArray &fallback = QByteArray("\n"));
    static QString escapedBytes(const QByteArray &bytes);
};

class ProtocolFramer
{
public:
    ProtocolFramer();

    void setConfig(const ProtocolFrameConfig &config);
    ProtocolFrameConfig config() const;

    QStringList ingest(const QByteArray &bytes);
    QStringList flush();
    void reset();

private:
    QString frameToText(QByteArray frame) const;
    void appendOversizedFrames(QStringList *frames);

    ProtocolFrameConfig m_config;
    QByteArray m_buffer;
};

#endif // PROTOCOLFRAMER_H
