#include <iostream>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>

#include "engine.h"

#ifndef APP_VERSION
#define APP_VERSION "0.1.0"
#endif

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("nymea-cli"));
    QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("nymea-cli terminal client for nymead JSON-RPC over TCP."));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption hostOption({QStringLiteral("H"), QStringLiteral("host")}, QStringLiteral("nymead host."), QStringLiteral("host"), QStringLiteral("127.0.0.1"));
    QCommandLineOption sslOption({QStringLiteral("s"), QStringLiteral("ssl")}, QStringLiteral("Use SSL/TLS (QSslSocket). Default port: 2222. Plain default port: 2223."));
    QCommandLineOption portOption({QStringLiteral("p"), QStringLiteral("port")},
                                  QStringLiteral("nymead TCP port (defaults: 2222 for --ssl, 2223 for plain)."),
                                  QStringLiteral("port"));
    QCommandLineOption timeoutOption({QStringLiteral("t"), QStringLiteral("timeout")},
                                     QStringLiteral("Timeout in milliseconds for connect/reply waits."),
                                     QStringLiteral("ms"),
                                     QStringLiteral("5000"));
    QCommandLineOption usernameOption({QStringLiteral("u"), QStringLiteral("username")}, QStringLiteral("Username for JSONRPC.Authenticate."), QStringLiteral("username"));
    QCommandLineOption passwordOption({QStringLiteral("w"), QStringLiteral("password")}, QStringLiteral("Password for JSONRPC.Authenticate."), QStringLiteral("password"));

    parser.addOption(hostOption);
    parser.addOption(sslOption);
    parser.addOption(portOption);
    parser.addOption(timeoutOption);
    parser.addOption(usernameOption);
    parser.addOption(passwordOption);
    parser.process(app);

    const bool useSsl = parser.isSet(sslOption);
    const int defaultPort = useSsl ? 2222 : 2223;

    int port = defaultPort;
    bool isPortValid = true;
    if (parser.isSet(portOption)) {
        port = parser.value(portOption).toInt(&isPortValid);
    }
    if (!isPortValid || port <= 0 || port > 65535) {
        std::cerr << "Invalid --port value. Expected 1-65535.\n";
        return 1;
    }

    bool isTimeoutValid = false;
    const int timeoutMs = parser.value(timeoutOption).toInt(&isTimeoutValid);
    if (!isTimeoutValid || timeoutMs <= 0) {
        std::cerr << "Invalid --timeout value. Expected positive milliseconds.\n";
        return 1;
    }

    nymea::EngineOptions engineOptions;
    engineOptions.host = parser.value(hostOption);
    engineOptions.useSsl = useSsl;
    engineOptions.port = port;
    engineOptions.timeoutMs = timeoutMs;
    engineOptions.username = parser.value(usernameOption).toStdString();
    engineOptions.password = parser.value(passwordOption).toStdString();
    engineOptions.appVersion = app.applicationVersion().toStdString();

    nymea::Engine engine(engineOptions);
    return engine.run();
}
