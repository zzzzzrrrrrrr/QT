#include "configmanager.h"

#include <QDomDocument>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
{
}

bool ConfigManager::load(const QString &filePath, FileType fileType)
{
    return fileType == FileType::Json ? loadJson(filePath) : loadXml(filePath);
}

bool ConfigManager::save(const QString &filePath, FileType fileType) const
{
    return fileType == FileType::Json ? saveJson(filePath) : saveXml(filePath);
}

bool ConfigManager::loadJson(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return false;
    }

    m_values.clear();

    const QJsonObject object = document.object();
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        m_values.insert(it.key(), it.value().toVariant());
    }

    return true;
}

bool ConfigManager::saveJson(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    QJsonObject object;
    for (auto it = m_values.constBegin(); it != m_values.constEnd(); ++it) {
        object.insert(it.key(), QJsonValue::fromVariant(it.value()));
    }

    const QJsonDocument document(object);
    file.write(document.toJson(QJsonDocument::Indented));
    return true;
}

bool ConfigManager::loadXml(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QDomDocument document;
    const auto parseResult = document.setContent(&file);
    if (!parseResult) {
        return false;
    }

    const QDomElement root = document.documentElement();
    if (root.tagName() != QStringLiteral("config")) {
        return false;
    }

    m_values.clear();

    const QDomNodeList entries = root.elementsByTagName(QStringLiteral("entry"));
    for (int i = 0; i < entries.count(); ++i) {
        const QDomElement entry = entries.at(i).toElement();
        const QString key = entry.attribute(QStringLiteral("key"));
        const QString type = entry.attribute(QStringLiteral("type"));

        if (!key.isEmpty()) {
            m_values.insert(key, stringToVariant(entry.text(), type));
        }
    }

    return true;
}

bool ConfigManager::saveXml(const QString &filePath) const
{
    QDomDocument document(QStringLiteral("config"));
    QDomElement root = document.createElement(QStringLiteral("config"));
    document.appendChild(root);

    for (auto it = m_values.constBegin(); it != m_values.constEnd(); ++it) {
        QDomElement entry = document.createElement(QStringLiteral("entry"));
        entry.setAttribute(QStringLiteral("key"), it.key());
        entry.setAttribute(QStringLiteral("type"), variantTypeName(it.value()));
        entry.appendChild(document.createTextNode(variantToString(it.value())));
        root.appendChild(entry);
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    file.write(document.toByteArray(4));
    return true;
}

QVariant ConfigManager::getValue(const QString &key, const QVariant &defaultValue) const
{
    return m_values.value(key, defaultValue);
}

QVariant ConfigManager::getValue(const QString &key,
                                 QMetaType::Type valueType,
                                 const QVariant &defaultValue) const
{
    if (!m_values.contains(key)) {
        return defaultValue;
    }

    QVariant value = m_values.value(key);
    if (valueType == QMetaType::UnknownType) {
        return value;
    }

    if (!value.convert(QMetaType(valueType))) {
        return defaultValue;
    }

    return value;
}

void ConfigManager::setValue(const QString &key, const QVariant &value)
{
    if (!key.isEmpty()) {
        m_values.insert(key, value);
    }
}

bool ConfigManager::contains(const QString &key) const
{
    return m_values.contains(key);
}

void ConfigManager::clear()
{
    m_values.clear();
}

QString ConfigManager::variantTypeName(const QVariant &value)
{
    switch (value.typeId()) {
    case QMetaType::Bool:
        return QStringLiteral("bool");
    case QMetaType::Int:
        return QStringLiteral("int");
    case QMetaType::LongLong:
        return QStringLiteral("longlong");
    case QMetaType::Double:
        return QStringLiteral("double");
    default:
        return QStringLiteral("string");
    }
}

QString ConfigManager::variantToString(const QVariant &value)
{
    return value.toString();
}

QVariant ConfigManager::stringToVariant(const QString &text, const QString &typeName)
{
    if (typeName == QStringLiteral("bool")) {
        return text.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0
               || text == QStringLiteral("1");
    }

    if (typeName == QStringLiteral("int")) {
        return text.toInt();
    }

    if (typeName == QStringLiteral("longlong")) {
        return text.toLongLong();
    }

    if (typeName == QStringLiteral("double")) {
        return text.toDouble();
    }

    return text;
}
