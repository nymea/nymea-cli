#include "thingmanager.h"

#include "generated/integrationsgetthingclassesresponse.h"
#include "generated/integrationsgetthingsresponse.h"

#include <set>
#include <utility>

namespace nymea {

namespace {

std::string uuidKey(const QUuid& uuid)
{
    return uuid.toString(QUuid::WithoutBraces).toStdString();
}

bool sameParamTypeId(const std::optional<QUuid>& paramTypeId, const QUuid& expected)
{
    return paramTypeId.has_value() && *paramTypeId == expected;
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

api::Thing* ThingManager::findThingById(const QUuid& thingId)
{
    for (api::Thing& thing : m_things) {
        if (thing.id == thingId) {
            return &thing;
        }
    }

    return nullptr;
}

const api::Thing* ThingManager::thingById(const QUuid& thingId) const
{
    for (const api::Thing& thing : m_things) {
        if (thing.id == thingId) {
            return &thing;
        }
    }

    return nullptr;
}

bool ThingManager::hasThingClass(const QUuid& thingClassId) const
{
    return thingClassById(thingClassId) != nullptr;
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

bool ThingManager::upsertThing(const api::Thing& thing)
{
    if (api::Thing* existingThing = findThingById(thing.id); existingThing != nullptr) {
        *existingThing = thing;
        return true;
    }

    m_things.append(thing);
    return true;
}

bool ThingManager::removeThing(const QUuid& thingId)
{
    for (int index = 0; index < m_things.size(); ++index) {
        if (m_things.at(index).id == thingId) {
            m_things.removeAt(index);
            return true;
        }
    }

    return false;
}

bool ThingManager::updateThingState(
    const QUuid& thingId, const QUuid& stateTypeId, const QJsonValue& value, const QJsonValue& minValue, const QJsonValue& maxValue, const QList<QJsonValue>& possibleValues)
{
    api::Thing* thing = findThingById(thingId);
    if (thing == nullptr) {
        return false;
    }

    for (api::State& state : thing->states) {
        if (state.stateTypeId == stateTypeId) {
            state.value = value;
            state.minValue = minValue.isUndefined() ? std::nullopt : std::optional<QJsonValue>(minValue);
            state.maxValue = maxValue.isUndefined() ? std::nullopt : std::optional<QJsonValue>(maxValue);
            state.possibleValues = possibleValues.isEmpty() ? std::nullopt : std::optional<QList<QJsonValue>>(possibleValues);
            return true;
        }
    }

    api::State state;
    state.filter = api::StateValueFilter::StateValueFilterNone;
    state.stateTypeId = stateTypeId;
    state.value = value;
    state.minValue = minValue.isUndefined() ? std::nullopt : std::optional<QJsonValue>(minValue);
    state.maxValue = maxValue.isUndefined() ? std::nullopt : std::optional<QJsonValue>(maxValue);
    state.possibleValues = possibleValues.isEmpty() ? std::nullopt : std::optional<QList<QJsonValue>>(possibleValues);
    thing->states.append(state);
    return true;
}

bool ThingManager::updateThingSetting(const QUuid& thingId, const QUuid& paramTypeId, const QJsonValue& value)
{
    api::Thing* thing = findThingById(thingId);
    if (thing == nullptr) {
        return false;
    }

    if (!thing->settings.has_value()) {
        thing->settings = api::ParamList{};
    }

    for (api::Param& param : *thing->settings) {
        if (sameParamTypeId(param.paramTypeId, paramTypeId)) {
            param.value = value;
            return true;
        }
    }

    api::Param param;
    param.paramTypeId = paramTypeId;
    param.value = value;
    thing->settings->append(param);
    return true;
}

} // namespace nymea
