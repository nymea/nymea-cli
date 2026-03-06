#pragma once

#include <QJsonObject>

#include <string>
#include <vector>

namespace nymea {

struct Thing
{
    std::string name;
    std::string id;
};

class ThingManager
{
public:
    void clear();
    void setStatus(const std::string& status);
    const std::string& status() const;
    const std::vector<Thing>& things() const;

    bool updateFromReply(const QJsonObject& reply, std::string& errorMessage);

private:
    std::vector<Thing> m_things;
    std::string m_status = "No thing list loaded.";
};

} // namespace nymea
