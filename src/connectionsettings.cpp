// SPDX-License-Identifier: GPL-3.0-or-later

#include "connectionsettings.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

#include <unistd.h>

namespace nymea {

QString ConnectionSettings::settingsPath() const
{
    if (geteuid() == 0) {
        return QStringLiteral("/var/lib/nymea/nymea-cli.conf");
    }

    const QString configRoot = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (!configRoot.isEmpty()) {
        return QDir(configRoot).filePath(QStringLiteral("nymea-cli.conf"));
    }

    const QString homePath = QDir::homePath();
    return QDir(homePath).filePath(QStringLiteral(".config/nymea/nymea-cli.conf"));
}

std::optional<SavedConnection> ConnectionSettings::loadConnectionByEndpoint(const QString& host, int port, bool useSsl) const
{
    QSettings settings(settingsPath(), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Connections"));

    const QString normalizedHost = host.trimmed();
    const QStringList connectionGroups = settings.childGroups();
    for (const QString& groupName : connectionGroups) {
        settings.beginGroup(groupName);
        const auto connection = loadCurrentConnection(settings, groupName);
        settings.endGroup();

        if (!connection.has_value()) {
            continue;
        }

        if (connection->host.compare(normalizedHost, Qt::CaseInsensitive) == 0 && connection->port == port && connection->useSsl == useSsl) {
            settings.endGroup();
            return connection;
        }
    }

    settings.endGroup();
    return std::nullopt;
}

std::optional<SavedConnection> ConnectionSettings::loadConnectionByUuid(const QUuid& hostUuid) const
{
    if (hostUuid.isNull()) {
        return std::nullopt;
    }

    QSettings settings(settingsPath(), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Connections"));
    const QString groupName = groupNameForHostUuid(hostUuid);
    if (!settings.childGroups().contains(groupName)) {
        settings.endGroup();
        return std::nullopt;
    }

    settings.beginGroup(groupName);
    const auto connection = loadCurrentConnection(settings, groupName);
    settings.endGroup();
    settings.endGroup();
    return connection;
}

bool ConnectionSettings::saveConnection(const SavedConnection& connection, QString& errorMessage) const
{
    errorMessage.clear();

    if (connection.hostUuid.isNull()) {
        errorMessage = QStringLiteral("Cannot save connection without host uuid.");
        return false;
    }

    if (!ensureSettingsDirectory(errorMessage)) {
        return false;
    }

    QSettings settings(settingsPath(), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Connections"));
    settings.beginGroup(groupNameForHostUuid(connection.hostUuid));
    settings.setValue(QStringLiteral("Name"), connection.name);
    settings.setValue(QStringLiteral("Host"), connection.host);
    settings.setValue(QStringLiteral("Port"), connection.port);
    settings.setValue(QStringLiteral("UseSsl"), connection.useSsl);
    settings.setValue(QStringLiteral("Token"), connection.token);
    if (connection.useSsl && !connection.certificateFingerprint.isEmpty()) {
        settings.setValue(QStringLiteral("CertificateFingerprint"), connection.certificateFingerprint);
    } else {
        settings.remove(QStringLiteral("CertificateFingerprint"));
    }
    settings.endGroup();
    settings.endGroup();
    settings.sync();

    if (settings.status() != QSettings::NoError) {
        errorMessage = QStringLiteral("Failed to write settings file %1.").arg(settingsPath());
        return false;
    }

    return true;
}

bool ConnectionSettings::clearToken(const QUuid& hostUuid, QString& errorMessage) const
{
    errorMessage.clear();

    if (hostUuid.isNull()) {
        return true;
    }

    if (!ensureSettingsDirectory(errorMessage)) {
        return false;
    }

    QSettings settings(settingsPath(), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Connections"));
    settings.beginGroup(groupNameForHostUuid(hostUuid));
    settings.remove(QStringLiteral("Token"));
    settings.endGroup();
    settings.endGroup();
    settings.sync();

    if (settings.status() != QSettings::NoError) {
        errorMessage = QStringLiteral("Failed to update settings file %1.").arg(settingsPath());
        return false;
    }

    return true;
}

bool ConnectionSettings::ensureSettingsDirectory(QString& errorMessage) const
{
    errorMessage.clear();

    const QFileInfo fileInfo(settingsPath());
    QDir directory(fileInfo.absolutePath());
    if (directory.exists()) {
        return true;
    }

    if (!directory.mkpath(QStringLiteral("."))) {
        errorMessage = QStringLiteral("Failed to create settings directory %1.").arg(directory.absolutePath());
        return false;
    }

    return true;
}

std::optional<SavedConnection> ConnectionSettings::loadCurrentConnection(QSettings& settings, const QString& groupName) const
{
    SavedConnection connection;
    connection.hostUuid = QUuid(QStringLiteral("{%1}").arg(groupName));
    if (connection.hostUuid.isNull()) {
        return std::nullopt;
    }

    connection.name = settings.value(QStringLiteral("Name")).toString();
    connection.host = settings.value(QStringLiteral("Host")).toString();
    connection.port = settings.value(QStringLiteral("Port")).toInt();
    connection.useSsl = settings.value(QStringLiteral("UseSsl")).toBool();
    connection.token = settings.value(QStringLiteral("Token")).toString();
    connection.certificateFingerprint = settings.value(QStringLiteral("CertificateFingerprint")).toString();
    return connection;
}

QString ConnectionSettings::groupNameForHostUuid(const QUuid& hostUuid) const
{
    return hostUuid.toString(QUuid::WithoutBraces);
}

} // namespace nymea
