// SPDX-License-Identifier: GPL-3.0-or-later

#include "engine.h"
#include "engineutils.h"

#include "generated/actiontype.h"

#include <algorithm>
#include <ftxui/component/mouse.hpp>

namespace nymea {

void Engine::clampThingSelection(const QUuid& preferredThingId)
{
    const std::vector<const api::Thing*> things = filteredThings();
    if (things.empty()) {
        m_selectedThingIndex = 0;
        m_showThingDetailInspector = false;
        if (m_focusArea == FocusArea::ThingDetails) {
            m_focusArea = FocusArea::ThingList;
        }
        return;
    }

    if (!preferredThingId.isNull()) {
        for (int index = 0; index < static_cast<int>(things.size()); ++index) {
            if (things.at(index)->id == preferredThingId) {
                m_selectedThingIndex = index;
                return;
            }
        }
    }

    if (m_selectedThingIndex < 0) {
        m_selectedThingIndex = 0;
    } else if (m_selectedThingIndex >= static_cast<int>(things.size())) {
        m_selectedThingIndex = static_cast<int>(things.size()) - 1;
    }
}

void Engine::clampConfigureThingClassSelection()
{
    const std::vector<api::ThingClass> thingClasses = filteredConfigThingClasses();
    if (thingClasses.empty()) {
        m_selectedConfigureThingClassIndex = 0;
        if (m_focusArea == FocusArea::ConfigureThingClassList) {
            m_focusArea = FocusArea::ConfigureThingClassSearch;
        }
        return;
    }

    if (m_selectedConfigureThingClassIndex < 0) {
        m_selectedConfigureThingClassIndex = 0;
    } else if (m_selectedConfigureThingClassIndex >= static_cast<int>(thingClasses.size())) {
        m_selectedConfigureThingClassIndex = static_cast<int>(thingClasses.size()) - 1;
    }
}

void Engine::clampConfigureThingSelection()
{
    if (m_thingManager.things().empty()) {
        m_selectedConfigureThingIndex = 0;
        if (m_focusArea == FocusArea::ConfigureThingSelection) {
            m_focusArea = FocusArea::ConfigureMenu;
        }
        return;
    }

    if (m_selectedConfigureThingIndex < 0) {
        m_selectedConfigureThingIndex = 0;
    } else if (m_selectedConfigureThingIndex >= static_cast<int>(m_thingManager.things().size())) {
        m_selectedConfigureThingIndex = static_cast<int>(m_thingManager.things().size()) - 1;
    }
}

int Engine::thingDetailEntryCount() const
{
    const api::Thing* thing = selectedThing();
    const api::ThingClass* thingClass = thing != nullptr ? m_thingManager.thingClassForThing(*thing) : nullptr;
    return static_cast<int>(buildThingDetailEntries(thing, thingClass).size());
}

void Engine::clampThingDetailSelection()
{
    const int entryCount = thingDetailEntryCount();
    if (entryCount <= 0) {
        m_selectedThingDetailIndex = 0;
        m_showThingDetailInspector = false;
        return;
    }

    if (m_selectedThingDetailIndex < 0) {
        m_selectedThingDetailIndex = 0;
    } else if (m_selectedThingDetailIndex >= entryCount) {
        m_selectedThingDetailIndex = entryCount - 1;
    }
}

void Engine::resetThingDetailSelection()
{
    m_selectedThingDetailIndex = 0;
    m_showThingDetailInspector = false;
    closeActionDialog();
    clampThingDetailSelection();
}

bool Engine::openSelectedActionDialog()
{
    const api::Thing* thing = selectedThing();
    if (thing == nullptr) {
        return false;
    }

    const api::ThingClass* thingClass = m_thingManager.thingClassForThing(*thing);
    const std::vector<ThingDetailEntry> detailEntries = buildThingDetailEntries(thing, thingClass);
    if (m_selectedThingDetailIndex < 0 || m_selectedThingDetailIndex >= static_cast<int>(detailEntries.size())) {
        return false;
    }

    const ThingDetailEntry& selectedEntry = detailEntries.at(m_selectedThingDetailIndex);
    if (selectedEntry.type != ThingDetailEntry::Type::Action) {
        return false;
    }

    const api::ActionType* actionType = m_thingManager.actionTypeForThing(*thing, selectedEntry.index);
    if (actionType == nullptr) {
        return false;
    }

    m_showThingDetailInspector = false;
    m_showActionDialog = true;
    m_focusArea = FocusArea::ActionDialog;
    m_actionDialogActionIndex = selectedEntry.index;
    m_actionDialogThingId = thing->id;
    m_actionDialogSelectedParamIndex = 0;
    m_actionDialogRangeEditIndex.reset();
    m_actionDialogActionName = firstNonEmpty({actionType->displayName.toStdString(), actionType->name.toStdString(), "Action"});
    m_actionDialogParamTypes.assign(actionType->paramTypes.begin(), actionType->paramTypes.end());
    m_actionDialogParamValues.clear();
    m_actionDialogParamValues.reserve(m_actionDialogParamTypes.size());
    for (const api::ParamType& paramType : m_actionDialogParamTypes) {
        m_actionDialogParamValues.push_back(normalizedActionDialogValue(paramType, {}));
    }
    m_actionExecutionPending = false;
    m_pendingActionRequestId = -1;
    m_pendingActionInvocation.clear();
    m_actionDialogStatus = "Edit action params and press Enter to execute.";
    m_lastActionExecutionStatus.clear();
    m_lastActionExecutionStatusWarning = false;
    return true;
}

void Engine::closeActionDialog()
{
    m_showActionDialog = false;
    m_actionDialogStatus.clear();
    m_actionDialogActionName.clear();
    m_actionDialogActionIndex = -1;
    m_actionDialogThingId = QUuid();
    m_actionDialogSelectedParamIndex = 0;
    m_actionDialogRangeEditIndex.reset();
    m_actionDialogParamTypes.clear();
    m_actionDialogParamValues.clear();
    m_actionExecutionPending = false;
    m_pendingActionRequestId = -1;
    m_pendingActionInvocation.clear();
    m_lastActionExecutionStatus.clear();
    m_lastActionExecutionStatusWarning = false;
    if (m_focusArea == FocusArea::ActionDialog) {
        m_focusArea = FocusArea::ThingDetails;
    }
}

std::vector<const api::Thing*> Engine::filteredThings() const
{
    const QString search = QString::fromStdString(m_thingSearch).trimmed();
    std::vector<const api::Thing*> things;
    things.reserve(m_thingManager.things().size());

    for (const api::Thing& thing : m_thingManager.things()) {
        const api::ThingClass* thingClass = m_thingManager.thingClassForThing(thing);
        const ThingCategory category = thingCategory(thing);
        if (m_thingCategoryFilter != ThingCategory::All && category != m_thingCategoryFilter) {
            continue;
        }

        if (!search.isEmpty()) {
            const bool matchesThing = thing.name.has_value() && caseInsensitiveContains(*thing.name, search);
            const bool matchesClass = thingClass != nullptr
                                      && (caseInsensitiveContains(thingClass->displayName, search) || caseInsensitiveContains(thingClass->name, search)
                                          || caseInsensitiveContains(thingClass->interfaces.join(QStringLiteral(" ")), search));
            if (!matchesThing && !matchesClass) {
                continue;
            }
        }

        things.push_back(&thing);
    }

    if (m_thingSortMode == ThingSortMode::Alphabetical) {
        std::stable_sort(things.begin(), things.end(), [](const api::Thing* left, const api::Thing* right) {
            return QString::compare(QString::fromStdString(thingLabel(left)), QString::fromStdString(thingLabel(right)), Qt::CaseInsensitive) < 0;
        });
    } else if (m_thingSortMode == ThingSortMode::Grouped) {
        std::stable_sort(things.begin(), things.end(), [this](const api::Thing* left, const api::Thing* right) {
            const ThingCategory leftCategory = thingCategory(*left);
            const ThingCategory rightCategory = thingCategory(*right);
            if (leftCategory != rightCategory) {
                return static_cast<int>(leftCategory) < static_cast<int>(rightCategory);
            }
            return QString::compare(QString::fromStdString(thingLabel(left)), QString::fromStdString(thingLabel(right)), Qt::CaseInsensitive) < 0;
        });
    }

    return things;
}

const api::Thing* Engine::selectedThing() const
{
    const std::vector<const api::Thing*> things = filteredThings();
    if (m_selectedThingIndex < 0 || m_selectedThingIndex >= static_cast<int>(things.size())) {
        return nullptr;
    }
    return things.at(m_selectedThingIndex);
}

QUuid Engine::selectedThingId() const
{
    const api::Thing* thing = selectedThing();
    return thing != nullptr ? thing->id : QUuid();
}

Engine::ThingCategory Engine::thingCategory(const api::Thing& thing) const
{
    const api::ThingClass* thingClass = m_thingManager.thingClassForThing(thing);
    if (thingClass == nullptr) {
        return ThingCategory::Other;
    }

    const QString interfaces = thingClass->interfaces.join(QStringLiteral(" "));
    if (caseInsensitiveContainsAny(interfaces, {"light", "lamp", "dimmer"})) {
        return ThingCategory::Lights;
    }
    if (caseInsensitiveContainsAny(interfaces, {"blind", "shading", "cover", "shutter"})) {
        return ThingCategory::Blinds;
    }
    if (caseInsensitiveContainsAny(interfaces, {"media", "multimedia", "audio", "video", "player"})) {
        return ThingCategory::Multimedia;
    }
    if (caseInsensitiveContainsAny(interfaces, {"powersocket", "power socket", "socket", "outlet", "plug"})) {
        return ThingCategory::PowerSockets;
    }
    if (caseInsensitiveContainsAny(interfaces, {"thermostat", "heating", "climate"})) {
        return ThingCategory::Thermostats;
    }
    if (caseInsensitiveContainsAny(interfaces, {"sensor"})) {
        return ThingCategory::Sensors;
    }
    if (caseInsensitiveContainsAny(interfaces, {"smartmeter", "smart meter", "energymeter", "energy meter", "meter"})) {
        return ThingCategory::SmartMeters;
    }
    if (caseInsensitiveContainsAny(interfaces, {"switch", "button"})) {
        return ThingCategory::Switches;
    }
    return ThingCategory::Other;
}

std::string Engine::thingCategoryLabel(ThingCategory category) const
{
    switch (category) {
    case ThingCategory::All:
        return "All";
    case ThingCategory::Lights:
        return "Lights";
    case ThingCategory::Blinds:
        return "Blinds";
    case ThingCategory::Multimedia:
        return "Multimedia";
    case ThingCategory::PowerSockets:
        return "Power sockets";
    case ThingCategory::Thermostats:
        return "Thermostats";
    case ThingCategory::Sensors:
        return "Sensors";
    case ThingCategory::SmartMeters:
        return "Smart meters";
    case ThingCategory::Switches:
        return "Switches";
    case ThingCategory::Other:
        return "Other";
    }
    return "Other";
}

std::string Engine::thingSortModeLabel() const
{
    switch (m_thingSortMode) {
    case ThingSortMode::Default:
        return "Default";
    case ThingSortMode::Alphabetical:
        return "A-Z";
    case ThingSortMode::Grouped:
        return "Grouped";
    }
    return "Default";
}

void Engine::cycleThingSortMode()
{
    const QUuid selectedId = selectedThingId();
    switch (m_thingSortMode) {
    case ThingSortMode::Default:
        m_thingSortMode = ThingSortMode::Alphabetical;
        break;
    case ThingSortMode::Alphabetical:
        m_thingSortMode = ThingSortMode::Grouped;
        break;
    case ThingSortMode::Grouped:
        m_thingSortMode = ThingSortMode::Default;
        break;
    }
    clampThingSelection(selectedId);
    resetThingDetailSelection();
}

void Engine::cycleThingCategoryFilter()
{
    const QUuid selectedId = selectedThingId();
    if (m_thingCategoryFilter == ThingCategory::Other) {
        m_thingCategoryFilter = ThingCategory::All;
    } else {
        m_thingCategoryFilter = static_cast<ThingCategory>(static_cast<int>(m_thingCategoryFilter) + 1);
    }
    clampThingSelection(selectedId);
    resetThingDetailSelection();
}

std::vector<api::ThingClass> Engine::filteredConfigThingClasses() const
{
    const QString search = QString::fromStdString(m_configureThingSearch).trimmed();
    std::vector<api::ThingClass> filtered;
    for (const api::ThingClass& thingClass : m_thingManager.thingClasses()) {
        if (!isThingClassAddable(thingClass)) {
            continue;
        }
        if (!search.isEmpty() && !caseInsensitiveContains(thingClass.displayName, search) && !caseInsensitiveContains(thingClass.name, search)) {
            continue;
        }
        filtered.push_back(thingClass);
    }
    return filtered;
}

const api::ThingClass* Engine::selectedConfigThingClass() const
{
    const std::vector<api::ThingClass> thingClasses = filteredConfigThingClasses();
    if (m_selectedConfigureThingClassIndex < 0 || m_selectedConfigureThingClassIndex >= static_cast<int>(thingClasses.size())) {
        return nullptr;
    }

    return m_thingManager.thingClassById(thingClasses.at(m_selectedConfigureThingClassIndex).id);
}

const api::Thing* Engine::selectedConfigureThing() const
{
    return m_thingManager.thingAt(m_selectedConfigureThingIndex);
}

void Engine::ensureApiBrowserLoaded()
{
    if (m_apiBrowserPending || m_apiBrowserLoaded) {
        return;
    }

    if (!m_client.isConnected()) {
        m_apiBrowserStatus = "Connect to browse the API.";
        return;
    }

    if (m_isAuthenticationRequired && !m_isAuthenticated) {
        m_apiBrowserStatus = "Authentication required to browse the API.";
        return;
    }

    m_apiBrowserPending = true;
    m_apiBrowserStatus = "Loading JSONRPC.Introspect...";

    observeReply(m_client.sendRequest(api::JSONRPCIntrospectMethod::methodName(), QJsonObject{}),
                 [this](const QJsonObject& message, const QString& transportError) { handleApiBrowserIntrospectionReply(message, transportError); });
}

namespace {

enum class ApiBrowserKind {
    Method,
    Notification,
    Type,
    Enum,
};

struct ApiBrowserItem
{
    QString section;
    QString name;
    ApiBrowserKind kind = ApiBrowserKind::Type;
    QJsonValue value;
    QString searchText;
    std::vector<std::pair<QString, QString>> references;
};

QString apiBrowserKindLabel(ApiBrowserKind kind)
{
    switch (kind) {
    case ApiBrowserKind::Method:
        return QStringLiteral("Method");
    case ApiBrowserKind::Notification:
        return QStringLiteral("Notification");
    case ApiBrowserKind::Type:
        return QStringLiteral("Type");
    case ApiBrowserKind::Enum:
        return QStringLiteral("Enum");
    }
    return QStringLiteral("Type");
}

int apiBrowserKindOrder(ApiBrowserKind kind)
{
    switch (kind) {
    case ApiBrowserKind::Method:
        return 0;
    case ApiBrowserKind::Notification:
        return 1;
    case ApiBrowserKind::Type:
        return 2;
    case ApiBrowserKind::Enum:
        return 3;
    }
    return 4;
}

bool apiBrowserArrayLooksLikeEnum(const QJsonValue& value)
{
    if (!value.isArray()) {
        return false;
    }

    const QJsonArray array = value.toArray();
    if (array.isEmpty()) {
        return false;
    }

    for (const QJsonValue& entry : array) {
        if (!entry.isString()) {
            return false;
        }
        const QString text = entry.toString();
        if (text.startsWith(QStringLiteral("$ref:"))) {
            return false;
        }
    }
    return true;
}

void collectApiBrowserReferences(const QJsonValue& value, const QString& path, std::vector<std::pair<QString, QString>>& references)
{
    if (value.isString()) {
        const QString text = value.toString();
        if (text.startsWith(QStringLiteral("$ref:"))) {
            references.emplace_back(path.isEmpty() ? QStringLiteral("<root>") : path, text.mid(5));
        }
        return;
    }

    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            const QString childPath = path.isEmpty() ? it.key() : path + QStringLiteral(".") + it.key();
            collectApiBrowserReferences(it.value(), childPath, references);
        }
        return;
    }

    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        for (int index = 0; index < array.size(); ++index) {
            const QString childPath = path + QStringLiteral("[") + QString::number(index) + QStringLiteral("]");
            collectApiBrowserReferences(array.at(index), childPath, references);
        }
    }
}

QString apiBrowserPrettyJson(const QJsonValue& value)
{
    return QString::fromStdString(prettyJsonValueToString(value));
}

std::vector<ApiBrowserItem> buildApiBrowserItems(const QJsonObject& introspection)
{
    std::vector<ApiBrowserItem> items;
    const bool hasExplicitEnums = introspection.contains(QStringLiteral("enums")) && introspection.value(QStringLiteral("enums")).isObject();

    auto appendItems = [&](const QString& section, ApiBrowserKind kind, bool allowEnumDetection) {
        const QJsonObject object = introspection.value(section).toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            ApiBrowserItem item;
            item.section = section;
            item.name = it.key();
            item.kind = kind;
            item.value = it.value();

            if (allowEnumDetection && !hasExplicitEnums && apiBrowserArrayLooksLikeEnum(item.value)) {
                item.kind = ApiBrowserKind::Enum;
                item.section = QStringLiteral("enums");
            }

            item.searchText = (apiBrowserKindLabel(item.kind) + QStringLiteral(" ") + item.name + QStringLiteral(" ") + apiBrowserPrettyJson(item.value)).toLower();
            collectApiBrowserReferences(item.value, QString(), item.references);
            std::sort(item.references.begin(), item.references.end(), [](const auto& left, const auto& right) {
                if (left.second != right.second) {
                    return left.second.toLower() < right.second.toLower();
                }
                return left.first.toLower() < right.first.toLower();
            });
            item.references.erase(std::unique(item.references.begin(), item.references.end()), item.references.end());
            items.push_back(std::move(item));
        }
    };

    appendItems(QStringLiteral("methods"), ApiBrowserKind::Method, false);
    appendItems(QStringLiteral("notifications"), ApiBrowserKind::Notification, false);
    appendItems(QStringLiteral("types"), ApiBrowserKind::Type, true);
    if (introspection.contains(QStringLiteral("enums"))) {
        appendItems(QStringLiteral("enums"), ApiBrowserKind::Enum, false);
    }

    std::sort(items.begin(), items.end(), [](const ApiBrowserItem& left, const ApiBrowserItem& right) {
        if (apiBrowserKindOrder(left.kind) != apiBrowserKindOrder(right.kind)) {
            return apiBrowserKindOrder(left.kind) < apiBrowserKindOrder(right.kind);
        }
        const int sectionCompare = QString::compare(left.section, right.section, Qt::CaseInsensitive);
        if (sectionCompare != 0) {
            return sectionCompare < 0;
        }
        return QString::compare(left.name, right.name, Qt::CaseInsensitive) < 0;
    });

    return items;
}

std::vector<ApiBrowserItem> filterApiBrowserItems(const std::vector<ApiBrowserItem>& items, const QString& search)
{
    if (search.trimmed().isEmpty()) {
        return items;
    }

    const QString normalizedSearch = search.trimmed().toLower();
    std::vector<ApiBrowserItem> filtered;
    filtered.reserve(items.size());
    for (const ApiBrowserItem& item : items) {
        if (item.searchText.contains(normalizedSearch)) {
            filtered.push_back(item);
        }
    }
    return filtered;
}

int apiBrowserItemIndex(const std::vector<ApiBrowserItem>& items, const QString& section, const QString& name)
{
    for (int index = 0; index < static_cast<int>(items.size()); ++index) {
        const ApiBrowserItem& item = items.at(index);
        if (item.section == section && item.name == name) {
            return index;
        }
    }
    return -1;
}

QString apiBrowserItemDisplayLabel(const ApiBrowserItem& item)
{
    return apiBrowserKindLabel(item.kind) + QStringLiteral(": ") + item.name;
}

} // namespace

void Engine::handleApiBrowserIntrospectionReply(const QJsonObject& message, const QString& transportError)
{
    m_apiBrowserPending = false;

    if (!transportError.isEmpty()) {
        m_apiBrowserStatus = "Failed to load JSONRPC.Introspect: " + transportError.toStdString();
        return;
    }

    const QString status = message.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("unauthorized")) {
        clearStoredToken();
        m_client.clearAuthToken();
        m_isAuthenticationRequired = true;
        m_isAuthenticated = false;
        m_notificationsEnabled = false;
        m_showLoginForm = true;
        m_loginSelectedInputIndex = 0;
        m_focusArea = FocusArea::LoginForm;
        m_apiBrowserLoaded = false;
        m_apiBrowserIntrospection = QJsonObject();
        m_apiBrowserHistory.clear();
        m_apiBrowserSelectedSection.clear();
        m_apiBrowserSelectedName.clear();
        m_apiBrowserSearch.clear();
        m_apiBrowserSelectedReferenceIndex = 0;
        m_apiBrowserStatus = "Stored token was rejected. Please login.";
        return;
    }

    if (status == QStringLiteral("error")) {
        m_apiBrowserStatus = "JSONRPC.Introspect returned an error.";
        return;
    }

    const QJsonObject params = message.value(QStringLiteral("params")).toObject();
    m_apiBrowserIntrospection = params;
    m_apiBrowserLoaded = true;
    m_apiBrowserStatus = "Loaded JSONRPC.Introspect.";

    const std::vector<ApiBrowserItem> items = buildApiBrowserItems(m_apiBrowserIntrospection);
    if (m_apiBrowserSelectedSection.isEmpty() || m_apiBrowserSelectedName.isEmpty() || apiBrowserItemIndex(items, m_apiBrowserSelectedSection, m_apiBrowserSelectedName) < 0) {
        if (!items.empty()) {
            m_apiBrowserSelectedSection = items.front().section;
            m_apiBrowserSelectedName = items.front().name;
        }
        m_apiBrowserSelectedReferenceIndex = 0;
        m_apiBrowserJsonLineIndex = 0;
    }
}

void Engine::selectApiBrowserItem(const QString& section, const QString& name, bool addToHistory)
{
    const std::vector<ApiBrowserItem> items = buildApiBrowserItems(m_apiBrowserIntrospection);
    const int itemIndex = apiBrowserItemIndex(items, section, name);
    if (itemIndex < 0) {
        m_apiBrowserStatus = "API browser item not found: " + section.toStdString() + "/" + name.toStdString();
        return;
    }

    if (addToHistory && !m_apiBrowserSelectedSection.isEmpty() && !m_apiBrowserSelectedName.isEmpty()
        && (m_apiBrowserSelectedSection != section || m_apiBrowserSelectedName != name)) {
        m_apiBrowserHistory.emplace_back(m_apiBrowserSelectedSection, m_apiBrowserSelectedName);
    }

    m_apiBrowserSelectedSection = section;
    m_apiBrowserSelectedName = name;
    m_apiBrowserSelectedReferenceIndex = 0;
    m_apiBrowserJsonLineIndex = 0;
    m_focusArea = FocusArea::ApiBrowserList;
}

void Engine::apiBrowserGoBack()
{
    while (!m_apiBrowserHistory.empty()) {
        const std::pair<QString, QString> selection = m_apiBrowserHistory.back();
        m_apiBrowserHistory.pop_back();
        const std::vector<ApiBrowserItem> items = buildApiBrowserItems(m_apiBrowserIntrospection);
        if (apiBrowserItemIndex(items, selection.first, selection.second) >= 0) {
            m_apiBrowserSelectedSection = selection.first;
            m_apiBrowserSelectedName = selection.second;
            m_apiBrowserSelectedReferenceIndex = 0;
            m_apiBrowserJsonLineIndex = 0;
            m_focusArea = FocusArea::ApiBrowserList;
            m_apiBrowserStatus = "Went back in API browser history.";
            return;
        }
    }
}

void Engine::clampApiBrowserSelection()
{
    const std::vector<ApiBrowserItem> items = buildApiBrowserItems(m_apiBrowserIntrospection);
    if (items.empty()) {
        m_apiBrowserSelectedSection.clear();
        m_apiBrowserSelectedName.clear();
        m_apiBrowserSelectedReferenceIndex = 0;
        m_apiBrowserJsonLineIndex = 0;
        return;
    }

    if (m_apiBrowserSelectedSection.isEmpty() || m_apiBrowserSelectedName.isEmpty() || apiBrowserItemIndex(items, m_apiBrowserSelectedSection, m_apiBrowserSelectedName) < 0) {
        m_apiBrowserSelectedSection = items.front().section;
        m_apiBrowserSelectedName = items.front().name;
        m_apiBrowserSelectedReferenceIndex = 0;
        m_apiBrowserJsonLineIndex = 0;
    }
}

void Engine::clampApiBrowserReferenceSelection()
{
    const std::vector<ApiBrowserItem> items = buildApiBrowserItems(m_apiBrowserIntrospection);
    const int itemIndex = apiBrowserItemIndex(items, m_apiBrowserSelectedSection, m_apiBrowserSelectedName);
    if (itemIndex < 0) {
        m_apiBrowserSelectedReferenceIndex = 0;
        return;
    }

    const ApiBrowserItem& item = items.at(itemIndex);
    if (item.references.empty()) {
        m_apiBrowserSelectedReferenceIndex = 0;
        return;
    }

    if (m_apiBrowserSelectedReferenceIndex < 0) {
        m_apiBrowserSelectedReferenceIndex = 0;
    } else if (m_apiBrowserSelectedReferenceIndex >= static_cast<int>(item.references.size())) {
        m_apiBrowserSelectedReferenceIndex = static_cast<int>(item.references.size()) - 1;
    }
}

int Engine::apiBrowserJsonLineCount() const
{
    const std::vector<ApiBrowserItem> items = buildApiBrowserItems(m_apiBrowserIntrospection);
    const int itemIndex = apiBrowserItemIndex(items, m_apiBrowserSelectedSection, m_apiBrowserSelectedName);
    if (itemIndex < 0) {
        return 0;
    }

    const QString json = apiBrowserPrettyJson(items.at(itemIndex).value);
    return json.split('\n').size();
}

void Engine::clampApiBrowserJsonSelection()
{
    const int lineCount = apiBrowserJsonLineCount();
    if (lineCount <= 0) {
        m_apiBrowserJsonLineIndex = 0;
        return;
    }

    if (m_apiBrowserJsonLineIndex < 0) {
        m_apiBrowserJsonLineIndex = 0;
    } else if (m_apiBrowserJsonLineIndex >= lineCount) {
        m_apiBrowserJsonLineIndex = lineCount - 1;
    }
}

enum class HelpRowKind {
    Heading,
    Text,
    Separator,
};

struct HelpRow
{
    HelpRowKind kind = HelpRowKind::Text;
    QString text;
};

std::vector<HelpRow> buildHelpRows()
{
    return {
        {HelpRowKind::Heading, QStringLiteral("Keyboard help")},
        {HelpRowKind::Text, QStringLiteral("Esc closes this screen and restores the previous view.")},
        {HelpRowKind::Text, QStringLiteral("q quits the app.")},
        {HelpRowKind::Text, QStringLiteral("Mouse wheel scrolls any focused list or window.")},
        {HelpRowKind::Text, QStringLiteral("h or ? opens this help screen.")},
        {HelpRowKind::Separator, {}},
        {HelpRowKind::Heading, QStringLiteral("Global")},
        {HelpRowKind::Text, QStringLiteral("Up / Down: move the selection in the focused list.")},
        {HelpRowKind::Text, QStringLiteral("Left / Right: switch panels or move between subviews.")},
        {HelpRowKind::Text, QStringLiteral("Enter: activate the selected item.")},
        {HelpRowKind::Text, QStringLiteral("Space: toggle the thing inspector.")},
        {HelpRowKind::Text, QStringLiteral("c: reconnect to the current server.")},
        {HelpRowKind::Text, QStringLiteral("t: refresh things from the server.")},
        {HelpRowKind::Text, QStringLiteral("s: cycle thing sort mode.")},
        {HelpRowKind::Text, QStringLiteral("f: cycle thing category filter.")},
        {HelpRowKind::Separator, {}},
        {HelpRowKind::Heading, QStringLiteral("Main menu")},
        {HelpRowKind::Text, QStringLiteral("Up / Down keeps focus on the main menu and updates the selected section.")},
        {HelpRowKind::Text, QStringLiteral("Right enters the selected section; in API browser it enters the filter field.")},
        {HelpRowKind::Text, QStringLiteral("About is a separate main-menu entry at the end.")},
        {HelpRowKind::Separator, {}},
        {HelpRowKind::Heading, QStringLiteral("Things view")},
        {HelpRowKind::Text, QStringLiteral("Left / Right: switch between search, list, and details.")},
        {HelpRowKind::Text, QStringLiteral("Enter on an action: open the action execution dialog.")},
        {HelpRowKind::Separator, {}},
        {HelpRowKind::Heading, QStringLiteral("Configure things")},
        {HelpRowKind::Text, QStringLiteral("Up / Down: move between menu entries or list rows.")},
        {HelpRowKind::Text, QStringLiteral("Enter: open the selected setup / rename / remove flow.")},
        {HelpRowKind::Text, QStringLiteral("Type in search fields to filter thing classes.")},
        {HelpRowKind::Separator, {}},
        {HelpRowKind::Heading, QStringLiteral("Settings")},
        {HelpRowKind::Text, QStringLiteral("Up / Down: select Server info, Timezone, Update, Shutdown, Restart, or Reboot.")},
        {HelpRowKind::Text, QStringLiteral("Right: open the settings details panel.")},
        {HelpRowKind::Text, QStringLiteral("Enter: apply the selected time zone, start an update, or open a power confirmation. Left or Esc returns to the settings menu.")},
        {HelpRowKind::Separator, {}},
        {HelpRowKind::Heading, QStringLiteral("API browser")},
        {HelpRowKind::Text, QStringLiteral("Type to filter methods, notifications, types, and enums.")},
        {HelpRowKind::Text, QStringLiteral("Backspace removes the last filter character.")},
        {HelpRowKind::Text, QStringLiteral("Left: go back in API browser history.")},
        {HelpRowKind::Text, QStringLiteral("Right: move between search, list, and references panes.")},
        {HelpRowKind::Text, QStringLiteral("Enter on a reference: follow that referenced type.")},
        {HelpRowKind::Separator, {}},
        {HelpRowKind::Heading, QStringLiteral("Dialogs")},
        {HelpRowKind::Text, QStringLiteral("Action dialogs: Up / Down choose a parameter, Enter executes, Esc closes.")},
        {HelpRowKind::Text, QStringLiteral("Configure dialogs: Enter confirms, Esc closes, and selector fields use Left / Right or Space.")},
        {HelpRowKind::Text, QStringLiteral("Login: Enter submits credentials, Esc quits.")},
    };
}

int Engine::helpLineCount() const
{
    return static_cast<int>(buildHelpRows().size());
}

void Engine::clampHelpSelection()
{
    const int lineCount = helpLineCount();
    if (lineCount <= 0) {
        m_helpLineIndex = 0;
        return;
    }

    if (m_helpLineIndex < 0) {
        m_helpLineIndex = 0;
    } else if (m_helpLineIndex >= lineCount) {
        m_helpLineIndex = lineCount - 1;
    }
}

bool Engine::handleMouseWheel(const ftxui::Event& event)
{
    if (!event.is_mouse()) {
        return false;
    }

    ftxui::Event mouseEvent = event;
    const ftxui::Mouse::Button button = mouseEvent.mouse().button;
    if (button != ftxui::Mouse::WheelUp && button != ftxui::Mouse::WheelDown) {
        return false;
    }

    const int delta = button == ftxui::Mouse::WheelUp ? -1 : 1;
    const auto inside = [&](const ftxui::Box& box) { return box.x_min <= box.x_max && box.y_min <= box.y_max && box.Contain(mouseEvent.mouse().x, mouseEvent.mouse().y); };

    auto moveMainMenu = [this, delta]() {
        switch (m_selectedMainMenuEntry) {
        case MainMenuEntry::Things:
            m_selectedMainMenuEntry = delta < 0 ? MainMenuEntry::About : MainMenuEntry::ConfigureThings;
            break;
        case MainMenuEntry::ConfigureThings:
            m_selectedMainMenuEntry = delta < 0 ? MainMenuEntry::Things : MainMenuEntry::ApiBrowser;
            break;
        case MainMenuEntry::ApiBrowser:
            m_selectedMainMenuEntry = delta < 0 ? MainMenuEntry::ConfigureThings : MainMenuEntry::Settings;
            break;
        case MainMenuEntry::Settings:
            m_selectedMainMenuEntry = delta < 0 ? MainMenuEntry::ApiBrowser : MainMenuEntry::Logout;
            break;
        case MainMenuEntry::Logout:
            m_selectedMainMenuEntry = delta < 0 ? MainMenuEntry::Settings : MainMenuEntry::About;
            break;
        case MainMenuEntry::About:
            m_selectedMainMenuEntry = delta < 0 ? MainMenuEntry::Things : MainMenuEntry::Logout;
            break;
        }
    };
    auto moveThingList = [this, delta]() {
        const std::vector<const api::Thing*> things = filteredThings();
        if (things.empty()) {
            return;
        }
        m_selectedThingIndex = (m_selectedThingIndex + static_cast<int>(things.size()) + delta) % static_cast<int>(things.size());
        resetThingDetailSelection();
    };
    auto moveThingDetails = [this, delta]() {
        const int count = thingDetailEntryCount();
        if (count <= 0) {
            return;
        }
        m_selectedThingDetailIndex = (m_selectedThingDetailIndex + count + delta) % count;
    };
    auto moveConfigureMenu = [this, delta]() {
        if (delta < 0) {
            if (m_configureThingsView == ConfigureThingsView::AddThing) {
                m_configureThingsView = ConfigureThingsView::RenameThing;
            } else {
                m_configureThingsView = static_cast<ConfigureThingsView>(static_cast<int>(m_configureThingsView) - 1);
            }
        } else {
            if (m_configureThingsView == ConfigureThingsView::RenameThing) {
                m_configureThingsView = ConfigureThingsView::AddThing;
            } else {
                m_configureThingsView = static_cast<ConfigureThingsView>(static_cast<int>(m_configureThingsView) + 1);
            }
        }
        if (m_configureThingsView == ConfigureThingsView::AddThing && !m_haveAllThingClasses && !m_fetchAllThingClassesPending) {
            fetchAllThingClasses();
        }
    };
    auto moveConfigureDetails = [this, delta]() {
        if (m_configureThingsView == ConfigureThingsView::AddThing) {
            const std::vector<api::ThingClass> thingClasses = filteredConfigThingClasses();
            if (thingClasses.empty()) {
                return;
            }
            m_selectedConfigureThingClassIndex = (m_selectedConfigureThingClassIndex + static_cast<int>(thingClasses.size()) + delta) % static_cast<int>(thingClasses.size());
            return;
        }
        if (m_thingManager.things().empty()) {
            return;
        }
        m_selectedConfigureThingIndex = (m_selectedConfigureThingIndex + static_cast<int>(m_thingManager.things().size()) + delta)
                                        % static_cast<int>(m_thingManager.things().size());
    };
    auto moveSettingsMenu = [this, delta]() {
        const int count = 6;
        int next = static_cast<int>(m_settingsView) + delta;
        if (next < 0) {
            next = count - 1;
        } else if (next >= count) {
            next = 0;
        }
        m_settingsView = static_cast<SettingsView>(next);
        m_settingsDetailsLineIndex = 0;
        ensureSystemCapabilitiesLoaded();
        ensureSystemTimeLoaded();
        ensureSystemUpdateStatusLoaded();
        if (m_settingsView == SettingsView::Timezone) {
            ensureSystemTimeZonesLoaded();
        } else if (m_settingsView == SettingsView::Update) {
            ensureSystemPackagesLoaded();
        }
    };
    auto moveSettingsDetails = [this, delta]() {
        const int count = settingsDetailsLineCount();
        if (count <= 0) {
            return;
        }
        m_settingsDetailsLineIndex = (m_settingsDetailsLineIndex + count + delta) % count;
    };
    auto moveApiBrowserList = [this, delta]() {
        const std::vector<ApiBrowserItem> items = buildApiBrowserItems(m_apiBrowserIntrospection);
        const std::vector<ApiBrowserItem> filteredItems = filterApiBrowserItems(items, QString::fromStdString(m_apiBrowserSearch));
        if (filteredItems.empty()) {
            return;
        }
        const int visibleIndex = apiBrowserItemIndex(filteredItems, m_apiBrowserSelectedSection, m_apiBrowserSelectedName);
        const int nextIndex = visibleIndex < 0 ? (delta < 0 ? static_cast<int>(filteredItems.size()) - 1 : 0)
                                               : (visibleIndex + static_cast<int>(filteredItems.size()) + delta) % static_cast<int>(filteredItems.size());
        selectApiBrowserItem(filteredItems.at(nextIndex).section, filteredItems.at(nextIndex).name, false);
        clampApiBrowserReferenceSelection();
        clampApiBrowserJsonSelection();
    };
    auto moveApiBrowserJson = [this, delta]() {
        const int lineCount = apiBrowserJsonLineCount();
        if (lineCount <= 0) {
            return;
        }
        m_apiBrowserJsonLineIndex = (m_apiBrowserJsonLineIndex + lineCount + delta) % lineCount;
    };
    auto moveApiBrowserReferences = [this, delta]() {
        const std::vector<ApiBrowserItem> items = buildApiBrowserItems(m_apiBrowserIntrospection);
        const int itemIndex = apiBrowserItemIndex(items, m_apiBrowserSelectedSection, m_apiBrowserSelectedName);
        if (itemIndex < 0) {
            return;
        }
        const ApiBrowserItem& item = items.at(itemIndex);
        if (item.references.empty()) {
            return;
        }
        m_apiBrowserSelectedReferenceIndex = (m_apiBrowserSelectedReferenceIndex + static_cast<int>(item.references.size()) + delta) % static_cast<int>(item.references.size());
    };

    if (m_showLoginForm || m_showLogoutConfirm) {
        return true;
    }

    if (m_showActionDialog) {
        if (m_actionExecutionPending || m_actionDialogParamTypes.empty()) {
            return true;
        }
        m_actionDialogSelectedParamIndex = (m_actionDialogSelectedParamIndex + static_cast<int>(m_actionDialogParamTypes.size()) + delta)
                                           % static_cast<int>(m_actionDialogParamTypes.size());
        m_actionDialogRangeEditIndex.reset();
        return true;
    }

    if (m_showConfigureDialog) {
        if (m_configureRequestPending) {
            return true;
        }

        switch (m_configureDialogMode) {
        case ConfigureDialogMode::AddChooseCreateMethod:
            if (!m_configureCreateMethodOptions.empty()) {
                m_configureCreateMethodIndex = (m_configureCreateMethodIndex + static_cast<int>(m_configureCreateMethodOptions.size()) + delta)
                                               % static_cast<int>(m_configureCreateMethodOptions.size());
            }
            return true;
        case ConfigureDialogMode::AddManualParams: {
            const int rowCount = 1 + static_cast<int>(m_configureParamTypes.size());
            if (rowCount <= 0) {
                return true;
            }
            m_configureParamSelectionIndex = (m_configureParamSelectionIndex + rowCount + delta) % rowCount;
            m_configureRangeEditIndex.reset();
            return true;
        }
        case ConfigureDialogMode::AddDiscoveryParams:
            if (!m_configureParamTypes.empty()) {
                m_configureParamSelectionIndex = (m_configureParamSelectionIndex + static_cast<int>(m_configureParamTypes.size()) + delta)
                                                 % static_cast<int>(m_configureParamTypes.size());
                m_configureRangeEditIndex.reset();
            }
            return true;
        case ConfigureDialogMode::AddDiscoveryResults:
            if (!m_configureThingDescriptors.empty()) {
                m_configureThingDescriptorIndex = (m_configureThingDescriptorIndex + static_cast<int>(m_configureThingDescriptors.size()) + delta)
                                                  % static_cast<int>(m_configureThingDescriptors.size());
            }
            return true;
        case ConfigureDialogMode::AddPairingConfirmation: {
            int fieldCount = 0;
            const bool needsUsername = m_configureSetupMethod == api::SetupMethod::SetupMethodUserAndPassword;
            const bool needsSecret = m_configureSetupMethod == api::SetupMethod::SetupMethodDisplayPin || m_configureSetupMethod == api::SetupMethod::SetupMethodEnterPin
                                     || m_configureSetupMethod == api::SetupMethod::SetupMethodUserAndPassword || m_configureSetupMethod == api::SetupMethod::SetupMethodOAuth;
            if (needsUsername) {
                ++fieldCount;
            }
            if (needsSecret) {
                ++fieldCount;
            }
            if (fieldCount > 0) {
                m_configureParamSelectionIndex = (m_configureParamSelectionIndex + fieldCount + delta) % fieldCount;
            }
            return true;
        }
        case ConfigureDialogMode::RenameThing:
        case ConfigureDialogMode::RemoveThingConfirm:
        case ConfigureDialogMode::ReconfigureThingInfo:
        case ConfigureDialogMode::None:
            return true;
        }
    }

    if (m_mainView == MainView::Help && inside(m_helpBox)) {
        const int lineCount = helpLineCount();
        if (lineCount > 0) {
            m_helpLineIndex = (m_helpLineIndex + lineCount + delta) % lineCount;
        }
        return true;
    }

    if (inside(m_mainMenuBox)) {
        moveMainMenu();
        return true;
    }

    if (m_mainView == MainView::Things) {
        if (inside(m_thingListBox)) {
            m_focusArea = FocusArea::ThingList;
            moveThingList();
            return true;
        }
        if (inside(m_thingDetailsBox)) {
            m_focusArea = FocusArea::ThingDetails;
            moveThingDetails();
            return true;
        }
    } else if (m_mainView == MainView::ConfigureThings) {
        if (inside(m_configureMenuBox)) {
            m_focusArea = FocusArea::ConfigureMenu;
            moveConfigureMenu();
            return true;
        }
        if (inside(m_configureDetailsBox)) {
            m_focusArea = m_configureThingsView == ConfigureThingsView::AddThing ? FocusArea::ConfigureThingClassList : FocusArea::ConfigureThingSelection;
            moveConfigureDetails();
            return true;
        }
    } else if (m_mainView == MainView::Settings) {
        if (inside(m_settingsMenuBox)) {
            m_focusArea = FocusArea::SettingsMenu;
            moveSettingsMenu();
            return true;
        }
        if (inside(m_settingsDetailsBox)) {
            m_focusArea = FocusArea::SettingsDetails;
            moveSettingsDetails();
            return true;
        }
    } else if (m_mainView == MainView::ApiBrowser) {
        if (inside(m_apiBrowserListBox)) {
            m_focusArea = FocusArea::ApiBrowserList;
            moveApiBrowserList();
            return true;
        }
        if (inside(m_apiBrowserJsonBox)) {
            moveApiBrowserJson();
            return true;
        }
        if (inside(m_apiBrowserReferencesBox)) {
            m_focusArea = FocusArea::ApiBrowserReferences;
            moveApiBrowserReferences();
            return true;
        }
    }

    return false;
}

ftxui::Element Engine::renderApiBrowser() const
{
    m_apiBrowserJsonBox = {};
    m_apiBrowserReferencesBox = {};
    m_apiBrowserDetailsBox = {};

    const std::vector<ApiBrowserItem> items = buildApiBrowserItems(m_apiBrowserIntrospection);
    const std::vector<ApiBrowserItem> filteredItems = filterApiBrowserItems(items, QString::fromStdString(m_apiBrowserSearch));
    const int selectedIndex = apiBrowserItemIndex(filteredItems, m_apiBrowserSelectedSection, m_apiBrowserSelectedName);
    const int fullIndex = apiBrowserItemIndex(items, m_apiBrowserSelectedSection, m_apiBrowserSelectedName);
    const ApiBrowserItem* selectedItem = fullIndex >= 0 ? &items.at(fullIndex) : nullptr;
    const bool selectionVisible = selectedIndex >= 0;

    ftxui::Elements leftPaneLines;
    auto searchRow = ftxui::text("Filter: " + (m_apiBrowserSearch.empty() ? std::string("<type to filter>") : m_apiBrowserSearch));
    if (m_focusArea == FocusArea::ApiBrowserSearch) {
        searchRow = renderActiveField(std::move(searchRow) | ftxui::inverted | ftxui::bold | ftxui::color(ftxui::Color::CyanLight), true, 28);
    }
    leftPaneLines.push_back(searchRow);
    leftPaneLines.push_back(ftxui::text(m_apiBrowserStatus));
    leftPaneLines.push_back(ftxui::separator());

    if (!m_apiBrowserLoaded) {
        leftPaneLines.push_back(ftxui::text(m_apiBrowserPending ? "Loading API browser..." : "Open the browser to load API metadata."));
    } else if (filteredItems.empty()) {
        leftPaneLines.push_back(ftxui::text("No API entries match the current filter."));
    } else {
        for (int index = 0; index < static_cast<int>(filteredItems.size()); ++index) {
            const ApiBrowserItem& item = filteredItems.at(index);
            auto row = ftxui::text(" " + apiBrowserItemDisplayLabel(item).toStdString() + " ");
            const bool selected = index == selectedIndex;
            if (selected) {
                row = row | ftxui::bold | ftxui::inverted;
            }
            if (m_focusArea == FocusArea::ApiBrowserList && selected) {
                row = row | ftxui::color(ftxui::Color::CyanLight);
            }
            if (selected) {
                row = row | ftxui::focus;
            }
            leftPaneLines.push_back(row);
        }
    }

    if (selectedItem != nullptr && !selectionVisible) {
        leftPaneLines.push_back(ftxui::separator());
        leftPaneLines.push_back(ftxui::text("Selected item is hidden by the current filter.") | ftxui::dim);
    }

    ftxui::Element leftPane = renderFocusedWindow(ftxui::text("API browser"),
                                                  ftxui::vbox(std::move(leftPaneLines)) | ftxui::vscroll_indicator | ftxui::frame,
                                                  m_focusArea == FocusArea::ApiBrowserSearch || m_focusArea == FocusArea::ApiBrowserList)
                              | ftxui::reflect(m_apiBrowserListBox) | ftxui::flex;

    ftxui::Element detailsPanel;
    if (!m_apiBrowserLoaded || selectedItem == nullptr) {
        ftxui::Elements lines;
        lines.push_back(ftxui::text("No API item selected."));
        lines.push_back(ftxui::separator());
        lines.push_back(ftxui::text("Press Enter on a reference to browse deeper.") | ftxui::dim);
        detailsPanel = renderFocusedWindow(ftxui::text("JSON"),
                                           ftxui::vbox(std::move(lines)) | ftxui::vscroll_indicator | ftxui::frame,
                                           m_focusArea == FocusArea::ApiBrowserReferences)
                       | ftxui::reflect(m_apiBrowserDetailsBox) | ftxui::flex;
    } else {
        ftxui::Elements jsonLines;
        const QString json = apiBrowserPrettyJson(selectedItem->value);
        const QStringList lines = json.split('\n');
        for (int index = 0; index < lines.size(); ++index) {
            ftxui::Element line = ftxui::paragraph(lines.at(index).toStdString());
            if (index == m_apiBrowserJsonLineIndex) {
                line = line | ftxui::focus;
            }
            jsonLines.push_back(std::move(line));
        }

        ftxui::Element jsonWindow = ftxui::window(ftxui::text(apiBrowserItemDisplayLabel(*selectedItem).toStdString()),
                                                  ftxui::vbox(std::move(jsonLines)) | ftxui::vscroll_indicator | ftxui::frame)
                                    | ftxui::reflect(m_apiBrowserJsonBox) | ftxui::flex;

        ftxui::Elements referenceLines;
        if (selectedItem->references.empty()) {
            referenceLines.push_back(ftxui::text("No outgoing type references."));
        } else {
            for (int index = 0; index < static_cast<int>(selectedItem->references.size()); ++index) {
                const std::pair<QString, QString>& reference = selectedItem->references.at(index);
                auto row = ftxui::text(" " + reference.first.toStdString() + " -> " + reference.second.toStdString() + " ");
                const bool selected = index == m_apiBrowserSelectedReferenceIndex;
                if (m_focusArea == FocusArea::ApiBrowserReferences && selected) {
                    row = row | ftxui::bold | ftxui::inverted | ftxui::color(ftxui::Color::CyanLight);
                } else if (selected) {
                    row = row | ftxui::bold | ftxui::inverted;
                }
                if (selected) {
                    row = row | ftxui::focus;
                }
                referenceLines.push_back(row);
            }
        }

        ftxui::Element referenceWindow = renderFocusedWindow(ftxui::text("References"),
                                                             ftxui::vbox(std::move(referenceLines)) | ftxui::vscroll_indicator | ftxui::frame,
                                                             m_focusArea == FocusArea::ApiBrowserReferences)
                                         | ftxui::reflect(m_apiBrowserReferencesBox) | ftxui::size(ftxui::HEIGHT, ftxui::GREATER_THAN, 7);

        detailsPanel = ftxui::vbox({
                           jsonWindow,
                           referenceWindow,
                       })
                       | ftxui::reflect(m_apiBrowserDetailsBox) | ftxui::flex;
    }

    return ftxui::hbox({
               leftPane | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 44),
               detailsPanel | ftxui::flex,
           })
           | ftxui::flex;
}

ftxui::Element Engine::renderHelp() const
{
    m_helpBox = {};

    const std::vector<HelpRow> rows = buildHelpRows();
    ftxui::Elements lines;
    for (int index = 0; index < static_cast<int>(rows.size()); ++index) {
        const HelpRow& row = rows.at(index);
        ftxui::Element element;
        switch (row.kind) {
        case HelpRowKind::Heading:
            element = ftxui::text(row.text.toStdString()) | ftxui::bold | ftxui::color(ftxui::Color::CyanLight);
            break;
        case HelpRowKind::Text:
            element = ftxui::paragraph(row.text.toStdString());
            break;
        case HelpRowKind::Separator:
            element = ftxui::separator();
            break;
        }

        if (index == m_helpLineIndex) {
            element = element | ftxui::inverted | ftxui::focus;
        }
        lines.push_back(std::move(element));
    }

    lines.push_back(ftxui::separator());
    lines.push_back(ftxui::text("Esc closes help and returns to the previous view.") | ftxui::dim);

    return ftxui::window(ftxui::text("Help"), ftxui::vbox(std::move(lines)) | ftxui::vscroll_indicator | ftxui::frame) | ftxui::reflect(m_helpBox);
}

} // namespace nymea
