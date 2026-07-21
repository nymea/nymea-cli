// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <utility>
#include <vector>

namespace nymea {

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

std::vector<ApiBrowserItem> buildApiBrowserItems(const QJsonObject& introspection);
std::vector<ApiBrowserItem> filterApiBrowserItems(const std::vector<ApiBrowserItem>& items, const QString& search);
int apiBrowserItemIndex(const std::vector<ApiBrowserItem>& items, const QString& section, const QString& name);

} // namespace nymea
