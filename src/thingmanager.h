#pragma once

#include "generated/actiontype.h"
#include "generated/param.h"
#include "generated/paramtype.h"
#include "generated/state.h"
#include "generated/statetype.h"
#include "generated/thing.h"
#include "generated/thingclass.h"

#include <QJsonObject>

#include <string>
#include <unordered_map>
#include <vector>

namespace nymea {

class ThingManager
{
public:
    void clear();
    void setStatus(const std::string& status);
    const std::string& status() const;
    const api::Things& things() const;
    const api::Thing* thingAt(int index) const;
    const api::ThingClass* thingClassForThing(const api::Thing& thing) const;
    const api::ParamType* paramTypeForThing(const api::Thing& thing, const api::Param& param) const;
    const api::StateType* stateTypeForThing(const api::Thing& thing, const api::State& state) const;
    const api::ActionType* actionTypeForThing(const api::Thing& thing, int actionIndex) const;
    const api::ParamType* paramTypeForAction(const api::ActionType& actionType, int paramIndex) const;
    std::vector<std::string> thingClassIds() const;

    bool updateFromReply(const QJsonObject& reply, std::string& errorMessage);
    bool updateThingClassesFromReply(const QJsonObject& reply, std::string& errorMessage);

private:
    const api::ThingClass* thingClassById(const QUuid& thingClassId) const;

    api::Things m_things;
    std::unordered_map<std::string, api::ThingClass> m_thingClasses;
    std::string m_status = "No thing list loaded.";
};

} // namespace nymea
