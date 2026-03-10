#pragma once

#include "connectionsettings.h"
#include "generated/paramtype.h"
#include "generated/thingclass.h"
#include "generated/thingdescriptor.h"
#include "nymeajsonrpcclient.h"
#include "thingmanager.h"

#include <QJsonObject>
#include <QString>
#include <QUuid>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
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
        Settings,
    };

    enum class SettingsView {
        General,
        About,
    };

    enum class FocusArea {
        MainMenu,
        ThingList,
        ThingDetails,
        ActionDialog,
        ConfigureMenu,
        ConfigureThingClassSearch,
        ConfigureThingClassList,
        ConfigureThingSelection,
        ConfigureDialog,
        SettingsMenu,
        LoginForm,
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

    std::string endpoint() const;
    std::string connectionDisplayName() const;
    SavedConnection currentConnection(bool allowFingerprintUpdate) const;
    void clampThingSelection();
    void clampConfigureThingClassSelection();
    void clampConfigureThingSelection();
    int thingDetailEntryCount() const;
    void clampThingDetailSelection();
    void resetThingDetailSelection();
    bool openSelectedActionDialog();
    void closeActionDialog();
    std::vector<api::ThingClass> filteredConfigThingClasses() const;
    const api::ThingClass* selectedConfigThingClass() const;
    const api::Thing* selectedConfigureThing() const;
    void closeConfigureDialog();
    void openAddThingDialog();
    void openRemoveThingDialog();
    void openRenameThingDialog();
    void openReconfigureThingDialog();
    void startAddThingFlow(api::CreateMethod createMethod);
    bool submitConfigureDialog();
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
    void handleEnableNotificationsReply(const QJsonObject& message, const QString& transportError, bool fetchThingsAfterReply);
    void handleFetchThingsReply(const QJsonObject& message, const QString& transportError);
    void handleFetchThingClassesReply(const QJsonObject& message, const QString& transportError);
    void handleActionExecutionReply(const QJsonObject& message, const QString& transportError);
    void handleNotification(const QJsonObject& message);
    void enqueueUiTask(std::function<void()> task);
    void drainUiTasks();

    bool connectToServer();
    void sendHello();
    void authenticate(const std::string& username, const std::string& password);
    void enableNotifications(bool fetchThingsAfterReply = true);
    void fetchThingClasses();
    void fetchThings();
    void loadSavedConnection();
    void updateCertificateWarning();
    void clearStoredToken();
    void saveCurrentConnection(bool allowFingerprintUpdate);
    void runHandshakeAndLoadThings();

    ftxui::Element renderMainMenu() const;
    ftxui::Element renderThingList() const;
    ftxui::Element renderThingDetails() const;
    ftxui::Element renderConfigureMenu() const;
    ftxui::Element renderConfigureDetails() const;
    ftxui::Element renderSettingsMenu() const;
    ftxui::Element renderSettingsDetails() const;
    ftxui::Element renderThings() const;
    ftxui::Element renderUi();
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
    bool m_showLoginForm = false;
    std::string m_authStatus = "Not authenticated.";
    std::string m_securityWarning;
    std::string m_settingsWarning;

    std::string m_username;
    std::string m_password;
    std::optional<SavedConnection> m_savedConnection;
    int m_selectedThingIndex = 0;
    int m_selectedThingDetailIndex = 0;
    bool m_showThingDetailInspector = false;
    bool m_showActionDialog = false;
    std::string m_actionDialogStatus;
    std::string m_actionDialogActionName;
    int m_actionDialogActionIndex = -1;
    QUuid m_actionDialogThingId;
    int m_actionDialogSelectedParamIndex = 0;
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
    ConfigureThingsView m_configureThingsView = ConfigureThingsView::AddThing;
    ConfigureDialogMode m_configureDialogMode = ConfigureDialogMode::None;
    SettingsView m_settingsView = SettingsView::General;
    FocusArea m_focusArea = FocusArea::MainMenu;
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
    std::vector<api::ThingDescriptor> m_configureThingDescriptors;
    int m_configureThingDescriptorIndex = 0;
    std::optional<api::SetupMethod> m_configureSetupMethod;
    QUuid m_configurePairingTransactionId;
    std::string m_configurePairingDisplayMessage;
    std::string m_configurePairingOauthUrl;
    std::string m_configurePairingPin;
    std::string m_configurePairingUsername;
    std::string m_configurePairingSecret;
    ftxui::ScreenInteractive* m_screen = nullptr;
    std::mutex m_uiTaskMutex;
    std::vector<std::function<void()>> m_uiTasks;

    ftxui::InputOption m_passwordInputOption;
    ftxui::Component m_usernameInput;
    ftxui::Component m_passwordInput;
    ftxui::Component m_loginForm;
};

} // namespace nymea
