#pragma once

#include "connectionsettings.h"
#include "nymeajsonrpcclient.h"
#include "thingmanager.h"

#include <QString>
#include <QUuid>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <optional>
#include <string>

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
        SettingsMenu,
        LoginForm,
    };

    std::string endpoint() const;
    std::string connectionDisplayName() const;
    SavedConnection currentConnection(bool allowFingerprintUpdate) const;

    bool connectToServer();
    bool sendHello();
    bool authenticate(const std::string& username, const std::string& password);
    bool fetchThings();
    void loadSavedConnection();
    void updateCertificateWarning();
    void clearStoredToken();
    void saveCurrentConnection(bool allowFingerprintUpdate);
    void runHandshakeAndLoadThings();

    ftxui::Element renderMainMenu() const;
    ftxui::Element renderSettingsMenu() const;
    ftxui::Element renderSettingsDetails() const;
    ftxui::Element renderThings() const;
    ftxui::Element renderUi() const;
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
    MainView m_mainView = MainView::Things;
    SettingsView m_settingsView = SettingsView::General;
    FocusArea m_focusArea = FocusArea::MainMenu;

    ftxui::InputOption m_passwordInputOption;
    ftxui::Component m_usernameInput;
    ftxui::Component m_passwordInput;
    ftxui::Component m_loginForm;
};

} // namespace nymea
