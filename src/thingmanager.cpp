#include "thingmanager.h"

#include <set>
#include <utility>

namespace nymea {

namespace {

std::string uuidKey(const QUuid& uuid)
{
    return uuid.toString(QUuid::WithoutBraces).toStdString();
}

} // namespace

void ThingManager::clear()
{
    m_things.clear();
    m_thingClasses.clear();
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

const api::Things& ThingManager::things() const
{
    return m_things;
}

const api::Thing* ThingManager::thingAt(int index) const
{
    if (index < 0 || index >= m_things.size()) {
        return nullptr;
    }

    return &m_things.at(index);
}

const api::ThingClass* ThingManager::thingClassById(const QUuid& thingClassId) const
{
    const auto iterator = m_thingClasses.find(uuidKey(thingClassId));
    if (iterator == m_thingClasses.end()) {
        return nullptr;
    }

    return &iterator->second;
}

const api::ThingClass* ThingManager::thingClassForThing(const api::Thing& thing) const
{
    return thingClassById(thing.thingClassId);
}

const api::ParamType* ThingManager::paramTypeForThing(const api::Thing& thing, const api::Param& param) const
{
    if (!param.paramTypeId.has_value()) {
        return nullptr;
    }

    const api::ThingClass* thingClass = thingClassForThing(thing);
    if (thingClass == nullptr) {
        return nullptr;
    }

    for (const api::ParamType& paramType : thingClass->paramTypes) {
        if (paramType.id == *param.paramTypeId) {
            return &paramType;
        }
    }

    return nullptr;
}

const api::StateType* ThingManager::stateTypeForThing(const api::Thing& thing, const api::State& state) const
{
    const api::ThingClass* thingClass = thingClassForThing(thing);
    if (thingClass == nullptr) {
        return nullptr;
    }

    for (const api::StateType& stateType : thingClass->stateTypes) {
        if (stateType.id == state.stateTypeId) {
            return &stateType;
        }
    }

    return nullptr;
}

const api::ActionType* ThingManager::actionTypeForThing(const api::Thing& thing, int actionIndex) const
{
    const api::ThingClass* thingClass = thingClassForThing(thing);
    if (thingClass == nullptr || actionIndex < 0 || actionIndex >= thingClass->actionTypes.size()) {
        return nullptr;
    }

    return &thingClass->actionTypes.at(actionIndex);
}

const api::ParamType* ThingManager::paramTypeForAction(const api::ActionType& actionType, int paramIndex) const
{
    if (paramIndex < 0 || paramIndex >= actionType.paramTypes.size()) {
        return nullptr;
    }

    return &actionType.paramTypes.at(paramIndex);
}

std::vector<std::string> ThingManager::thingClassIds() const
{
    std::set<std::string> uniqueIds;
    for (const api::Thing& thing : m_things) {
        uniqueIds.insert(uuidKey(thing.thingClassId));
    }

    return {uniqueIds.begin(), uniqueIds.end()};
}

bool ThingManager::updateFromReply(const QJsonObject& reply, std::string& errorMessage)
{
    errorMessage.clear();

    const api::IntegrationsGetThingsResponse response = api::IntegrationsGetThingsResponse::fromJson(reply.value(QStringLiteral("params")).toObject());
    if (response.thingError != api::ThingError::ThingErrorNoError) {
        errorMessage = "Integrations.GetThings returned error: " + api::toString(response.thingError).toStdString();
        return false;
    }

    m_things = response.things.value_or(api::Things{});
    m_thingClasses.clear();
    m_status = "Loaded " + std::to_string(m_things.size()) + " thing(s).";
    return true;
}

bool ThingManager::updateThingClassesFromReply(const QJsonObject& reply, std::string& errorMessage)
{
    errorMessage.clear();

    const api::IntegrationsGetThingClassesResponse response = api::IntegrationsGetThingClassesResponse::fromJson(reply.value(QStringLiteral("params")).toObject());
    if (response.thingError != api::ThingError::ThingErrorNoError) {
        errorMessage = "Integrations.GetThingClasses returned error: " + api::toString(response.thingError).toStdString();
        return false;
    }

    m_thingClasses.clear();
    if (response.thingClasses.has_value()) {
        for (const api::ThingClass& thingClass : *response.thingClasses) {
            m_thingClasses.emplace(uuidKey(thingClass.id), thingClass);
        }
    }

    return true;
}

} // namespace nymea
