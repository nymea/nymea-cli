#include "thingmanager.h"

#include <QJsonArray>

#include <utility>

namespace nymea {

void ThingManager::clear()
{
    m_things.clear();
    m_status = "No thing list loaded.";
}

void ThingManager::setStatus(const std::string& status)
{
    m_status = status;
}

const std::string& ThingManager::status() const
{
    return m_status;
}

const std::vector<Thing>& ThingManager::things() const
{
    return m_things;
}

bool ThingManager::updateFromReply(const QJsonObject& reply, std::string& errorMessage)
{
    errorMessage.clear();

    const QJsonObject params = reply.value(QStringLiteral("params")).toObject();
    const QString thingError = params.value(QStringLiteral("thingError")).toString();
    if (!thingError.isEmpty() && thingError != QStringLiteral("ThingErrorNoError")) {
        errorMessage = "Integrations.GetThings returned error: " + thingError.toStdString();
        return false;
    }

    m_things.clear();
    const QJsonArray thingsArray = params.value(QStringLiteral("things")).toArray();
    for (const QJsonValue& thingValue : thingsArray) {
        const QJsonObject thingObject = thingValue.toObject();
        QString name = thingObject.value(QStringLiteral("name")).toString();
        if (name.isEmpty()) {
            name = QStringLiteral("<unnamed>");
        }

        Thing thing;
        thing.name = name.toStdString();
        thing.id = thingObject.value(QStringLiteral("id")).toString().toStdString();
        m_things.push_back(std::move(thing));
    }

    m_status = "Loaded " + std::to_string(m_things.size()) + " thing(s).";
    return true;
}

} // namespace nymea
