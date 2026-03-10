#pragma once

#include "connectionsettings.h"
#include "generated/paramtype.h"
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
        SettingsMenu,
        LoginForm,
    };

    std::string endpoint() const;
    std::string connectionDisplayName() const;
    SavedConnection currentConnection(bool allowFingerprintUpdate) const;
    void clampThingSelection();
    int thingDetailEntryCount() const;
    void clampThingDetailSelection();
    void resetThingDetailSelection();
    bool openSelectedActionDialog();
    void closeActionDialog();
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
    SettingsView m_settingsView = SettingsView::General;
    FocusArea m_focusArea = FocusArea::MainMenu;
    ftxui::ScreenInteractive* m_screen = nullptr;
    std::mutex m_uiTaskMutex;
    std::vector<std::function<void()>> m_uiTasks;

    ftxui::InputOption m_passwordInputOption;
    ftxui::Component m_usernameInput;
    ftxui::Component m_passwordInput;
    ftxui::Component m_loginForm;
};

} // namespace nymea
