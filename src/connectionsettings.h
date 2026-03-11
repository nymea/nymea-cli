// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QSettings>
#include <QString>
#include <QUuid>

#include <optional>

namespace nymea {

struct SavedConnection
{
    QUuid hostUuid;
    QString name;
    QString host;
    int port = 0;
    bool useSsl = false;
    QString token;
    QString certificateFingerprint;
};

class ConnectionSettings
{
public:
    QString settingsPath() const;

    std::optional<SavedConnection> loadConnectionByEndpoint(const QString& host, int port, bool useSsl) const;
    std::optional<SavedConnection> loadConnectionByUuid(const QUuid& hostUuid) const;

    bool saveConnection(const SavedConnection& connection, QString& errorMessage) const;
    bool clearToken(const QUuid& hostUuid, QString& errorMessage) const;

private:
    bool ensureSettingsDirectory(QString& errorMessage) const;
    std::optional<SavedConnection> loadCurrentConnection(QSettings& settings, const QString& groupName) const;
    QString groupNameForHostUuid(const QUuid& hostUuid) const;
};

} // namespace nymea
