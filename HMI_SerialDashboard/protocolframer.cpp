#include "protocolframer.h"

#include <QtGlobal>

namespace {

int hexDigitValue(QChar character)
{
    if (character >= QLatin1Char('0') && character <= QLatin1Char('9')) {
        return character.unicode() - '0';
    }
    if (character >= QLatin1Char('a') && character <= QLatin1Char('f')) {
        return character.unicode() - 'a' + 10;
    }
    if (character >= QLatin1Char('A') && character <= QLatin1Char('F')) {
        return character.unicode() - 'A' + 10;
    }
    return -1;
}

} // namespace

ProtocolFrameConfig::Mode ProtocolFrameConfig::modeFromString(const QString &text)
{
    const QString normalized = text.trimmed().toLower();
    if (normalized == QStringLiteral("raw")) {
        return Mode::Raw;
    }
    if (normalized == QStringLiteral("delimiter")) {
        return Mode::Delimiter;
    }
    if (normalized == QStringLiteral("fixed") || normalized == QStringLiteral("fixed-length")) {
        return Mode::FixedLength;
    }
    return Mode::Line;
}

QString ProtocolFrameConfig::modeToString(Mode mode)
{
    switch (mode) {
    case Mode::Raw:
        return QStringLiteral("raw");
    case Mode::Delimiter:
        return QStringLiteral("delimiter");
    case Mode::FixedLength:
        return QStringLiteral("fixed");
    case Mode::Line:
    default:
        return QStringLiteral("line");
    }
}

QByteArray ProtocolFrameConfig::parseEscapedBytes(const QString &text, const QByteArray &fallback)
{
    if (text.isEmpty()) {
        return fallback;
    }

    QByteArray bytes;
    bytes.reserve(text.size());

    for (int i = 0; i < text.size(); ++i) {
        const QChar character = text.at(i);
        if (character != QLatin1Char('\\') || i + 1 >= text.size()) {
            bytes.append(character.toLatin1());
            continue;
        }

        const QChar next = text.at(++i);
        if (next == QLatin1Char('n')) {
            bytes.append('\n');
        } else if (next == QLatin1Char('r')) {
            bytes.append('\r');
        } else if (next == QLatin1Char('t')) {
            bytes.append('\t');
        } else if (next == QLatin1Char('0')) {
            bytes.append('\0');
        } else if (next == QLatin1Char('\\')) {
            bytes.append('\\');
        } else if (next == QLatin1Char('x') && i + 2 < text.size()) {
            const int high = hexDigitValue(text.at(i + 1));
            const int low = hexDigitValue(text.at(i + 2));
            if (high >= 0 && low >= 0) {
                bytes.append(static_cast<char>((high << 4) | low));
                i += 2;
            } else {
                bytes.append('x');
            }
        } else {
            bytes.append(next.toLatin1());
        }
    }

    return bytes.isEmpty() ? fallback : bytes;
}

QString ProtocolFrameConfig::escapedBytes(const QByteArray &bytes)
{
    QString text;
    text.reserve(bytes.size() * 2);

    for (char byte : bytes) {
        switch (byte) {
        case '\n':
            text.append(QStringLiteral("\\n"));
            break;
        case '\r':
            text.append(QStringLiteral("\\r"));
            break;
        case '\t':
            text.append(QStringLiteral("\\t"));
            break;
        case '\\':
            text.append(QStringLiteral("\\\\"));
            break;
        default:
            if (static_cast<unsigned char>(byte) < 0x20) {
                text.append(QStringLiteral("\\x%1")
                                .arg(static_cast<unsigned char>(byte), 2, 16, QLatin1Char('0')));
            } else {
                text.append(QLatin1Char(byte));
            }
            break;
        }
    }

    return text;
}

ProtocolFramer::ProtocolFramer() = default;

void ProtocolFramer::setConfig(const ProtocolFrameConfig &config)
{
    m_config = config;
    m_config.maxFrameBytes = qBound(128, m_config.maxFrameBytes, 1024 * 1024);
    m_config.fixedLength = qBound(1, m_config.fixedLength, m_config.maxFrameBytes);
    if (m_config.delimiter.isEmpty()) {
        m_config.delimiter = QByteArray("\n");
    }
    reset();
}

ProtocolFrameConfig ProtocolFramer::config() const
{
    return m_config;
}

QStringList ProtocolFramer::ingest(const QByteArray &bytes)
{
    QStringList frames;
    if (bytes.isEmpty()) {
        return frames;
    }

    if (m_config.mode == ProtocolFrameConfig::Mode::Raw) {
        frames.append(frameToText(bytes));
        return frames;
    }

    m_buffer.append(bytes);

    if (m_config.mode == ProtocolFrameConfig::Mode::FixedLength) {
        while (m_buffer.size() >= m_config.fixedLength) {
            frames.append(frameToText(m_buffer.left(m_config.fixedLength)));
            m_buffer.remove(0, m_config.fixedLength);
        }
        appendOversizedFrames(&frames);
        return frames;
    }

    const QByteArray delimiter = m_config.mode == ProtocolFrameConfig::Mode::Delimiter
                                     ? m_config.delimiter
                                     : QByteArray("\n");

    int delimiterIndex = m_buffer.indexOf(delimiter);
    while (delimiterIndex >= 0) {
        frames.append(frameToText(m_buffer.left(delimiterIndex)));
        m_buffer.remove(0, delimiterIndex + delimiter.size());
        delimiterIndex = m_buffer.indexOf(delimiter);
    }

    appendOversizedFrames(&frames);
    return frames;
}

QStringList ProtocolFramer::flush()
{
    QStringList frames;
    if (!m_buffer.isEmpty()) {
        frames.append(frameToText(m_buffer));
        m_buffer.clear();
    }
    return frames;
}

void ProtocolFramer::reset()
{
    m_buffer.clear();
}

QString ProtocolFramer::frameToText(QByteArray frame) const
{
    if (m_config.trimCarriageReturn && frame.endsWith('\r')) {
        frame.chop(1);
    }
    return QString::fromUtf8(frame);
}

void ProtocolFramer::appendOversizedFrames(QStringList *frames)
{
    while (m_buffer.size() > m_config.maxFrameBytes) {
        frames->append(frameToText(m_buffer.left(m_config.maxFrameBytes)));
        m_buffer.remove(0, m_config.maxFrameBytes);
    }
}
