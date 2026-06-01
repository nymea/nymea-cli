// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "connectionsettings.h"
#include "generated/package.h"
#include "generated/paramtype.h"
#include "generated/systemgetcapabilitiesresponse.h"
#include "generated/systemgettimeresponse.h"
#include "generated/systemgetupdatestatusresponse.h"
#include "generated/thingclass.h"
#include "generated/thingdescriptor.h"
#include "nymeajsonrpcclient.h"
#include "thingmanager.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QUuid>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nymea {

struct EngineOptions
{
    QString host = QStringLiteral("127.0.0.1");
    int port = 2223;
    bool useSsl = false;
    int timeoutMs = 5000;
    std::string username;
    std::string password;
    std::string appVersion;
};

class Engine
{
public:
    explicit Engine(EngineOptions options);

    int run();

private:
    enum class MainView {
        Things,
        ConfigureThings,
        ApiBrowser,
        Settings,
        Logout,
        About,
        Help,
    };

    enum class SettingsView {
        ServerInfo,
        Timezone,
        Update,
        Shutdown,
        Restart,
        Reboot,
    };

    enum class PowerAction {
        Shutdown,
        Restart,
        Reboot,
    };

    enum class FocusArea {
        MainMenu,
        ThingSearch,
        ThingList,
        ThingDetails,
        ActionDialog,
        ConfigureMenu,
        ConfigureThingClassSearch,
        ConfigureThingClassList,
        ConfigureThingSelection,
        ConfigureDialog,
        ApiBrowserSearch,
        ApiBrowserList,
        ApiBrowserReferences,
        SettingsMenu,
        SettingsDetails,
        LoginForm,
    };

    enum class MainMenuEntry {
        Things,
        ConfigureThings,
        ApiBrowser,
        Settings,
        Logout,
        About,
    };

    enum class ConfigureThingsView {
        AddThing,
        RemoveThing,
        ReconfigureThing,
        RenameThing,
    };

    enum class ConfigureDialogMode {
        None,
        AddChooseCreateMethod,
        AddManualParams,
        AddDiscoveryParams,
        AddDiscoveryResults,
        AddPairingConfirmation,
        RemoveThingConfirm,
        RenameThing,
        ReconfigureThingInfo,
    };

    enum class ThingSortMode {
        Default,
        Alphabetical,
        Grouped,
    };

    enum class ThingCategory {
        All,
        Lights,
        Blinds,
        Multimedia,
        PowerSockets,
        Thermostats,
        Sensors,
        SmartMeters,
        Switches,
        Other,
    };

    std::string endpoint() const;
    std::string connectionDisplayName() const;
    SavedConnection currentConnection(bool allowFingerprintUpdate) const;
    std::optional<QUuid> currentTokenId() const;
    void clampThingSelection(const QUuid& preferredThingId = QUuid());
    void clampConfigureThingClassSelection();
    void clampConfigureThingSelection();
    int thingDetailEntryCount() const;
    void clampThingDetailSelection();
    void resetThingDetailSelection();
    void selectInitialThingDetailSection();
    void ensureSystemCapabilitiesLoaded();
    void ensureSystemTimeLoaded();
    void ensureSystemUpdateStatusLoaded();
    void ensureSystemPackagesLoaded();
    void ensureSystemTimeZonesLoaded();
    QStringList filteredSystemTimeZones() const;
    bool openSelectedActionDialog();
    void closeActionDialog();
    std::vector<const api::Thing*> filteredThings() const;
    const api::Thing* selectedThing() const;
    QUuid selectedThingId() const;
    ThingCategory thingCategory(const api::Thing& thing) const;
    std::string thingCategoryLabel(ThingCategory category) const;
    std::string thingSortModeLabel() const;
    void cycleThingSortMode();
    void cycleThingCategoryFilter();
    std::vector<api::ThingClass> filteredConfigThingClasses() const;
    const api::ThingClass* selectedConfigThingClass() const;
    const api::Thing* selectedConfigureThing() const;
    void closeConfigureDialog();
    void openHelpView();
    void closeHelpView();
    void openAddThingDialog();
    void openRemoveThingDialog();
    void openRenameThingDialog();
    void openReconfigureThingDialog();
    void startAddThingFlow(api::CreateMethod createMethod);
    bool submitConfigureDialog();
    void ensureApiBrowserLoaded();
    void handleApiBrowserIntrospectionReply(const QJsonObject& message, const QString& transportError);
    void selectApiBrowserItem(const QString& section, const QString& name, bool addToHistory = false);
    void apiBrowserGoBack();
    void clampApiBrowserSelection();
    void clampApiBrowserReferenceSelection();
    void clampSettingsDetailsSelection();
    void clampApiBrowserJsonSelection();
    void clampHelpSelection();
    int settingsDetailsLineCount() const;
    int apiBrowserJsonLineCount() const;
    int helpLineCount() const;
    void fetchAllThingClasses();
    void handleFetchAllThingClassesReply(const QJsonObject& message, const QString& transportError);
    void handleDiscoverThingsReply(const QJsonObject& message, const QString& transportError);
    void handleAddThingReply(const QJsonObject& message, const QString& transportError);
    void handlePairThingReply(const QJsonObject& message, const QString& transportError);
    void handleConfirmPairingReply(const QJsonObject& message, const QString& transportError);
    void handleRemoveThingReply(const QJsonObject& message, const QString& transportError);
    void handleRenameThingReply(const QJsonObject& message, const QString& transportError);
    bool executeCurrentAction();
    void observeReply(JsonRpcReply* reply, std::function<void(const QJsonObject&, const QString&)> handler);
    void handleHelloReply(const QJsonObject& message, const QString& transportError);
    void handleAuthenticateReply(const QJsonObject& message, const QString& transportError);
    void handleRequestPushButtonAuthReply(const QJsonObject& message, const QString& transportError);
    void handlePushButtonAuthFinished(const QJsonObject& message);
    void handleEnableNotificationsReply(const QJsonObject& message, const QString& transportError, bool fetchThingsAfterReply);
    void handleFetchThingsReply(const QJsonObject& message, const QString& transportError);
    void handleFetchThingClassesReply(const QJsonObject& message, const QString& transportError);
    void handleFetchSystemCapabilitiesReply(const QJsonObject& message, const QString& transportError);
    void handleFetchSystemTimeReply(const QJsonObject& message, const QString& transportError);
    void handleFetchSystemUpdateStatusReply(const QJsonObject& message, const QString& transportError);
    void handleFetchSystemPackagesReply(const QJsonObject& message, const QString& transportError);
    void handleFetchSystemTimeZonesReply(const QJsonObject& message, const QString& transportError);
    void handleCheckForUpdatesReply(const QJsonObject& message, const QString& transportError);
    void handleSetTimeZoneReply(const QJsonObject& message, const QString& transportError);
    void handleUpdatePackagesReply(const QJsonObject& message, const QString& transportError);
    void handlePowerActionReply(const QJsonObject& message, const QString& transportError, PowerAction action);
    void handleActionExecutionReply(const QJsonObject& message, const QString& transportError);
    void handleNotification(const QJsonObject& message);
    void enqueueUiTask(std::function<void()> task);
    void drainUiTasks();
    void handleClientStateChanged(bool connected, bool encrypted, const QString& peerCertificateFingerprint, const QString& authToken, const QString& lastError);

    bool connectToServer(bool shouldLoadSavedConnection = true);
    void sendHello();
    void authenticate(const std::string& username, const std::string& password);
    void requestPushButtonAuth();
    void enterAuthenticationRequiredState(const std::string& statusMessage, bool requestPushButtonAuth = false);
    void completeAuthentication(const QString& token, const std::optional<QString>& username, const std::string& statusMessage);
    void logout();
    void revokeCurrentTokenAndLogout();
    void finalizeLogout();
    void enableNotifications(bool fetchThingsAfterReply = true);
    void fetchThingClasses();
    void fetchThings();
    void loadSavedConnection();
    void updateCertificateWarning();
    bool clearStoredToken();
    void saveCurrentConnection(bool allowFingerprintUpdate);
    void runHandshakeAndLoadThings();
    void openPowerActionConfirmDialog(PowerAction action);
    void closePowerActionConfirmDialog();
    void executePowerAction();

    ftxui::Element renderMainMenu() const;
    ftxui::Element renderThingList() const;
    ftxui::Element renderThingDetails() const;
    ftxui::Element renderConfigureMenu() const;
    ftxui::Element renderConfigureDetails() const;
    ftxui::Element renderSettingsMenu() const;
    ftxui::Element renderSettingsDetails() const;
    ftxui::Element renderLogout() const;
    ftxui::Element renderAbout() const;
    ftxui::Element renderConnectionLost() const;
    ftxui::Element renderApiBrowser() const;
    ftxui::Element renderHelp() const;
    ftxui::Element renderThings() const;
    ftxui::Element renderUi();
    bool handleMouseWheel(const ftxui::Event& event);
    bool handleEvent(const ftxui::Event& event, ftxui::ScreenInteractive& screen);

    EngineOptions m_options;
    ConnectionSettings m_connectionSettings;
    NymeaJsonRpcClient m_client;
    ThingManager m_thingManager;

    std::string m_connectionStatus = "Disconnected";
    std::string m_serverVersion = "n/a";
    std::string m_serverApiVersion = "n/a";
    QUuid m_serverUuid;
    QString m_serverName;

    bool m_isAuthenticationRequired = false;
    bool m_isAuthenticated = false;
    bool m_pushButtonAuthAvailable = false;
    bool m_pushButtonAuthPending = false;
    qint64 m_pushButtonAuthTransactionId = -1;
    bool m_showLoginForm = false;
    int m_loginSelectedInputIndex = 0;
    bool m_connectionLost = false;
    bool m_skipNextAutoAuthenticate = false;
    bool m_ignoreStoredToken = false;
    bool m_showLogoutConfirm = false;
    bool m_logoutRequestPending = false;
    bool m_showSystemActionConfirm = false;
    bool m_systemActionRequestPending = false;
    std::string m_authStatus = "Not authenticated.";
    std::string m_logoutStatus;
    std::string m_systemActionStatus;
    std::string m_securityWarning;
    std::string m_settingsWarning;

    std::string m_username;
    std::string m_password;
    std::optional<SavedConnection> m_savedConnection;
    MainMenuEntry m_selectedMainMenuEntry = MainMenuEntry::Things;
    QString m_apiBrowserSelectedSection;
    QString m_apiBrowserSelectedName;
    std::string m_apiBrowserSearch;
    std::vector<std::pair<QString, QString>> m_apiBrowserHistory;
    bool m_apiBrowserLoaded = false;
    bool m_apiBrowserPending = false;
    int m_apiBrowserSelectedReferenceIndex = 0;
    int m_apiBrowserJsonLineIndex = 0;
    std::string m_apiBrowserStatus;
    QJsonObject m_apiBrowserIntrospection;
    int m_selectedThingIndex = 0;
    std::string m_thingSearch;
    ThingSortMode m_thingSortMode = ThingSortMode::Alphabetical;
    ThingCategory m_thingCategoryFilter = ThingCategory::All;
    QUuid m_preferredThingSelectionId;
    int m_selectedThingDetailIndex = 0;
    bool m_showThingDetailInspector = false;
    bool m_showActionDialog = false;
    std::string m_actionDialogStatus;
    std::string m_actionDialogActionName;
    int m_actionDialogActionIndex = -1;
    QUuid m_actionDialogThingId;
    int m_actionDialogSelectedParamIndex = 0;
    std::optional<int> m_actionDialogRangeEditIndex;
    std::vector<api::ParamType> m_actionDialogParamTypes;
    std::vector<std::string> m_actionDialogParamValues;
    bool m_helloPending = false;
    bool m_authenticationPending = false;
    bool m_notificationSetupPending = false;
    bool m_fetchThingsPending = false;
    bool m_fetchThingClassesPending = false;
    bool m_actionExecutionPending = false;
    int m_pendingActionRequestId = -1;
    std::string m_pendingActionInvocation;
    std::chrono::steady_clock::time_point m_pendingActionStartedAt{};
    std::string m_lastActionExecutionStatus;
    bool m_lastActionExecutionStatusWarning = false;
    bool m_notificationsEnabled = false;
    MainView m_mainView = MainView::Things;
    MainView m_previousMainView = MainView::Things;
    ConfigureThingsView m_configureThingsView = ConfigureThingsView::AddThing;
    ConfigureDialogMode m_configureDialogMode = ConfigureDialogMode::None;
    SettingsView m_settingsView = SettingsView::ServerInfo;
    FocusArea m_focusArea = FocusArea::MainMenu;
    FocusArea m_previousFocusArea = FocusArea::MainMenu;
    int m_settingsDetailsLineIndex = 0;
    int m_helpLineIndex = 0;
    PowerAction m_systemAction = PowerAction::Shutdown;
    std::string m_configureThingSearch;
    int m_selectedConfigureThingClassIndex = 0;
    int m_selectedConfigureThingIndex = 0;
    bool m_fetchAllThingClassesPending = false;
    bool m_haveAllThingClasses = false;
    bool m_showConfigureDialog = false;
    bool m_configureRequestPending = false;
    bool m_configureFlowComplete = false;
    int m_configurePendingRequestId = -1;
    std::chrono::steady_clock::time_point m_configurePendingStartedAt{};
    std::string m_pendingConfigureInvocation;
    std::string m_lastConfigureExecutionStatus;
    bool m_lastConfigureExecutionStatusWarning = false;
    std::string m_configureDialogTitle;
    std::string m_configureDialogStatus;
    QUuid m_configureThingClassId;
    QUuid m_configureTargetThingId;
    std::vector<api::CreateMethod> m_configureCreateMethodOptions;
    int m_configureCreateMethodIndex = 0;
    std::optional<api::CreateMethod> m_configureCreateMethod;
    std::string m_configureThingName;
    std::vector<api::ParamType> m_configureParamTypes;
    std::vector<std::string> m_configureParamValues;
    int m_configureParamSelectionIndex = 0;
    std::optional<int> m_configureRangeEditIndex;
    std::vector<api::ThingDescriptor> m_configureThingDescriptors;
    int m_configureThingDescriptorIndex = 0;
    std::optional<api::SetupMethod> m_configureSetupMethod;
    QUuid m_configurePairingTransactionId;
    std::string m_configurePairingDisplayMessage;
    std::string m_configurePairingOauthUrl;
    std::string m_configurePairingPin;
    std::string m_configurePairingUsername;
    std::string m_configurePairingSecret;
    bool m_systemCapabilitiesLoaded = false;
    bool m_systemCapabilitiesPending = false;
    api::SystemGetCapabilitiesResponse m_systemCapabilities;
    bool m_systemTimeLoaded = false;
    bool m_systemTimePending = false;
    api::SystemGetTimeResponse m_systemTime;
    bool m_systemUpdateStatusLoaded = false;
    bool m_systemUpdateStatusPending = false;
    api::SystemGetUpdateStatusResponse m_systemUpdateStatus;
    bool m_systemPackagesLoaded = false;
    bool m_systemPackagesPending = false;
    std::vector<api::Package> m_systemPackages;
    bool m_systemTimeZonesLoaded = false;
    bool m_systemTimeZonesPending = false;
    QStringList m_systemTimeZones;
    std::string m_systemTimeZoneSearch;
    mutable ftxui::Box m_mainMenuBox;
    mutable ftxui::Box m_thingListBox;
    mutable ftxui::Box m_thingDetailsBox;
    mutable ftxui::Box m_configureMenuBox;
    mutable ftxui::Box m_configureDetailsBox;
    mutable ftxui::Box m_settingsMenuBox;
    mutable ftxui::Box m_settingsDetailsBox;
    mutable ftxui::Box m_apiBrowserListBox;
    mutable ftxui::Box m_apiBrowserDetailsBox;
    mutable ftxui::Box m_apiBrowserJsonBox;
    mutable ftxui::Box m_apiBrowserReferencesBox;
    mutable ftxui::Box m_helpBox;
    ftxui::ScreenInteractive* m_screen = nullptr;
    std::mutex m_uiTaskMutex;
    std::vector<std::function<void()>> m_uiTasks;

    ftxui::InputOption m_passwordInputOption;
    ftxui::Component m_usernameInput;
    ftxui::Component m_passwordInput;
    ftxui::Component m_loginForm;
};

} // namespace nymea
