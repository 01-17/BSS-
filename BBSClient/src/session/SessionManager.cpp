#include "session/SessionManager.h"

#include "net/ApiClient.h"

namespace bbs {

SessionManager::SessionManager(ApiClient* api, QObject* parent)
    : QObject(parent)
    , m_api(api)
{
}

void SessionManager::applyLogin(const LoginResult& result)
{
    m_token = result.token;
    m_profile = result.user;
    if (m_api)
        m_api->setToken(m_token);
    emit profileChanged();
}

void SessionManager::updateProfile(const UserProfile& profile)
{
    m_profile = profile;
    emit profileChanged();
}

void SessionManager::clear()
{
    const bool had = !m_token.isEmpty() || m_profile.userId > 0;
    m_token.clear();
    m_profile = UserProfile();
    if (m_api)
        m_api->clearToken();
    if (had)
        emit signedOut();
}

bool SessionManager::isAdmin() const
{
    return m_profile.roleCode == QStringLiteral("admin");
}

bool SessionManager::isMember() const
{
    return m_profile.roleCode == QStringLiteral("member");
}

bool SessionManager::canModerate(qint64 boardId) const
{
    if (!m_profile.isActive())
        return false;
    if (isAdmin())
        return true;
    return m_profile.moderates(boardId);
}

} // namespace bbs
