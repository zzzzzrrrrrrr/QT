#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QHash>
#include <QMetaType>
#include <QObject>
#include <QVariant>

/**
 * @brief ConfigManager stores simple key-value settings and persists them.
 *
 * JSON is used for typed configuration data. XML support is intentionally
 * simple and maps each key to one <entry> element for easy extension.
 */
class ConfigManager : public QObject
{
    Q_OBJECT

public:
    enum class FileType {
        Json,
        Xml
    };
    Q_ENUM(FileType)

    explicit ConfigManager(QObject *parent = nullptr);

    bool load(const QString &filePath, FileType fileType);
    bool save(const QString &filePath, FileType fileType) const;

    bool loadJson(const QString &filePath);
    bool saveJson(const QString &filePath) const;
    bool loadXml(const QString &filePath);
    bool saveXml(const QString &filePath) const;

    QVariant getValue(const QString &key, const QVariant &defaultValue = {}) const;
    QVariant getValue(const QString &key,
                      QMetaType::Type valueType,
                      const QVariant &defaultValue = {}) const;
    void setValue(const QString &key, const QVariant &value);

    bool contains(const QString &key) const;
    void clear();

private:
    static QString variantTypeName(const QVariant &value);
    static QString variantToString(const QVariant &value);
    static QVariant stringToVariant(const QString &text, const QString &typeName);

    QHash<QString, QVariant> m_values;
};

#endif // CONFIGMANAGER_H
