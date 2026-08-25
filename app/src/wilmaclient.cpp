#include "wilmaclient.h"

#include <QNetworkAccessManager>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QFile>
#include <QUrl>
#include <QRegularExpression>
#include <QDebug>

namespace {

const int kPollIntervalMs = 3 * 60 * 1000;
const char *kUserAgent =
        "Mozilla/5.0 (Linux; Sailfish) AppleWebKit/537.36 Admirality/0.1";

QString trimSlash(QString value)
{
    value = value.trimmed();
    while (value.endsWith(QLatin1Char('/')) && value.length() > 1)
        value.chop(1);
    return value;
}

QString hostFromUrl(const QString &value)
{
    const QUrl url(value);
    return url.host().toLower();
}

QString extractNamedInput(const QString &html, const QString &name)
{
    const QString pattern = QStringLiteral(
                "<input[^>]*name=['\"]%1['\"][^>]*>").arg(QRegularExpression::escape(name));
    QRegularExpression tagRe(pattern, QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch tagMatch = tagRe.match(html);
    if (!tagMatch.hasMatch()) {
        const QString swapped = QStringLiteral(
                    "<input[^>]*value=['\"]([^'\"]*)['\"][^>]*name=['\"]%1['\"][^>]*>")
                .arg(QRegularExpression::escape(name));
        QRegularExpression swappedRe(swapped, QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch swappedMatch = swappedRe.match(html);
        if (swappedMatch.hasMatch())
            return swappedMatch.captured(1);
        return QString();
    }
    const QString tag = tagMatch.captured(0);
    QRegularExpression valueRe(QStringLiteral("value=['\"]([^'\"]*)['\"]"),
                               QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch valueMatch = valueRe.match(tag);
    return valueMatch.hasMatch() ? valueMatch.captured(1) : QString();
}

QString extractMfaFormkey(const QString &html)
{
    QRegularExpression re(QStringLiteral("id=['\"]mfa-formkey['\"][^>]*value=['\"]([^'\"]+)['\"]"),
                          QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(html);
    if (match.hasMatch())
        return match.captured(1);
    QRegularExpression swapped(QStringLiteral("value=['\"]([^'\"]+)['\"][^>]*id=['\"]mfa-formkey['\"]"),
                               QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch swappedMatch = swapped.match(html);
    return swappedMatch.hasMatch() ? swappedMatch.captured(1) : QString();
}

int jsonInt(const QJsonObject &obj, const QString &key)
{
    const QJsonValue value = obj.value(key);
    if (value.isDouble())
        return value.toInt();
    if (value.isString())
        return value.toString().toInt();
    return 0;
}

QString jsonString(const QJsonObject &obj, const QStringList &keys)
{
    for (const QString &key : keys) {
        const QJsonValue value = obj.value(key);
        if (value.isString() && !value.toString().isEmpty())
            return value.toString();
        if (value.isDouble())
            return QString::number(value.toDouble());
    }
    return QString();
}

bool messageUnread(const QJsonObject &obj)
{
    if (!obj.contains(QStringLiteral("Status")) && !obj.contains(QStringLiteral("status")))
        return false;
    const QJsonValue status = obj.contains(QStringLiteral("Status"))
            ? obj.value(QStringLiteral("Status"))
            : obj.value(QStringLiteral("status"));
    if (status.isBool())
        return status.toBool();
    if (status.isDouble())
        return status.toInt() != 0;
    if (status.isString()) {
        const QString text = status.toString().trimmed().toLower();
        return text == QLatin1String("1") || text == QLatin1String("true")
                || text == QLatin1String("unread");
    }
    return !status.isNull() && !status.isUndefined();
}

} // namespace

WilmaClient::WilmaClient(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_cookies(new QNetworkCookieJar(m_nam))
    , m_busy(false)
    , m_loggedIn(false)
    , m_restoring(false)
    , m_needsOtp(false)
    , m_firstMessagePoll(true)
    , m_unreadCount(0)
{
    m_nam->setCookieJar(m_cookies);
    m_pollTimer.setInterval(kPollIntervalMs);
    connect(&m_pollTimer, SIGNAL(timeout()), this, SLOT(pollMessages()));
    loadSettings();
    loadBundledSchools();
}

WilmaClient::~WilmaClient()
{
}

QString WilmaClient::appVersion() const
{
    return QStringLiteral(APP_VERSION);
}

bool WilmaClient::busy() const { return m_busy; }
bool WilmaClient::loggedIn() const { return m_loggedIn; }
bool WilmaClient::restoringSession() const { return m_restoring; }
bool WilmaClient::needsOtp() const { return m_needsOtp; }
QString WilmaClient::errorMessage() const { return m_error; }
QString WilmaClient::statusText() const { return m_status; }
QString WilmaClient::schoolUrl() const { return m_schoolUrl; }
QString WilmaClient::schoolName() const { return m_schoolName; }
QString WilmaClient::schoolHost() const { return hostFromUrl(m_schoolUrl); }
QString WilmaClient::username() const { return m_username; }
QString WilmaClient::password() const { return m_password; }
QString WilmaClient::displayName() const { return m_displayName; }
int WilmaClient::unreadCount() const { return m_unreadCount; }
QVariantList WilmaClient::schools() const { return m_schools; }
bool WilmaClient::hasSchool() const { return !m_schoolUrl.isEmpty(); }
bool WilmaClient::hasCredentials() const { return !m_username.isEmpty() && !m_password.isEmpty(); }

QString WilmaClient::normalizeSchoolUrl(const QString &raw) const
{
    QString value = raw.trimmed();
    if (value.isEmpty())
        return QString();
    if (!value.contains(QStringLiteral("://"))) {
        if (!value.contains(QLatin1Char('.')))
            value.append(QStringLiteral(".inschool.fi"));
        value.prepend(QStringLiteral("https://"));
    }
    return trimSlash(value);
}

void WilmaClient::loadSchools()
{
    loadBundledSchools();
}

void WilmaClient::selectSchool(const QString &url, const QString &name)
{
    const QString normalized = normalizeSchoolUrl(url);
    if (normalized.isEmpty())
        return;
    if (m_schoolUrl != normalized) {
        m_schoolUrl = normalized;
        emit schoolUrlChanged();
    }
    if (m_schoolName != name) {
        m_schoolName = name;
        emit schoolNameChanged();
    }
    saveSettings();
}

void WilmaClient::selectSchoolUrl(const QString &raw)
{
    const QString normalized = normalizeSchoolUrl(raw);
    QString name = hostFromUrl(normalized);
    if (name.endsWith(QStringLiteral(".inschool.fi")))
        name.chop(QStringLiteral(".inschool.fi").size());
    for (const QVariant &entry : m_schools) {
        const QVariantMap map = entry.toMap();
        if (trimSlash(map.value(QStringLiteral("url")).toString()) == normalized) {
            name = map.value(QStringLiteral("name")).toString();
            break;
        }
    }
    selectSchool(normalized, name);
}

void WilmaClient::login(const QString &username, const QString &password)
{
    m_username = username.trimmed();
    m_password = password;
    emit usernameChanged();
    emit passwordChanged();
    saveSettings();
    startLogin();
}

void WilmaClient::submitOtp(const QString &code)
{
    if (m_mfaFormkey.isEmpty()) {
        setError(QStringLiteral("No MFA challenge is pending"));
        return;
    }
    setBusy(true);
    setStatus(QStringLiteral("Verifying code…"));
    clearError();

    const QJsonObject payload{
        {QStringLiteral("otp"), code.trimmed()},
        {QStringLiteral("action"), QStringLiteral("login")}
    };
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("formkey"), m_mfaFormkey);
    form.addQueryItem(QStringLiteral("payload"),
                      QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact)));
    postForm(QStringLiteral("/api/v1/accounts/me/mfa/otp/check"), form, RequestMfa);
}

void WilmaClient::restoreSession()
{
    if (m_restoring)
        return;
    m_restoring = true;
    emit restoringSessionChanged();

    if (m_schoolUrl.isEmpty()) {
        finishRestore(false);
        return;
    }

    applySessionCookie();
    if (!m_sessionId.isEmpty()) {
        setStatus(QStringLiteral("Restoring session…"));
        setBusy(true);
        get(QStringLiteral("/messages/list"), RequestMessages);
        return;
    }

    if (hasCredentials()) {
        setStatus(QStringLiteral("Signing in…"));
        startLogin();
        return;
    }

    finishRestore(false);
}

void WilmaClient::logout()
{
    m_pollTimer.stop();
    setLoggedIn(false);
    m_password.clear();
    emit passwordChanged();
    m_sessionId.clear();
    m_displayName.clear();
    emit displayNameChanged();
    m_unreadCount = 0;
    emit unreadCountChanged();
    m_seenMessageIds.clear();
    m_firstMessagePoll = true;
    setNeedsOtp(false);
    m_mfaFormkey.clear();
    m_cookies->setCookiesFromUrl(QList<QNetworkCookie>(), QUrl(m_schoolUrl));
    saveSettings();
    setStatus(QString());
    clearError();
}

void WilmaClient::pollMessages()
{
    if (!m_loggedIn || m_schoolUrl.isEmpty() || m_busy)
        return;
    get(QStringLiteral("/messages/list"), RequestMessages);
}

void WilmaClient::onReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;
    reply->deleteLater();

    const RequestKind kind = static_cast<RequestKind>(reply->property("kind").toInt());
    const QByteArray body = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QUrl url = reply->url();
    const QString location = reply->attribute(QNetworkRequest::RedirectionTargetAttribute)
            .toUrl().toString();

    if (reply->error() != QNetworkReply::NoError
            && kind != RequestIndexJson
            && kind != RequestLoginPage
            && kind != RequestAccount) {
        if (kind == RequestMessages && m_restoring && hasCredentials()) {
            startLogin();
            return;
        }
        setBusy(false);
        const QString message = reply->errorString();
        setError(message);
        if (kind == RequestLoginJson || kind == RequestLoginHtml || kind == RequestMfa
                || kind == RequestToken) {
            emit loginFailed(message);
            if (m_restoring)
                finishRestore(false);
        } else if (m_restoring) {
            finishRestore(false);
        }
        return;
    }

    switch (kind) {
    case RequestIndexJson:
        if (status >= 200 && status < 300) {
            const QJsonObject obj = QJsonDocument::fromJson(body).object();
            const QString sessionId = jsonString(obj, QStringList()
                                                 << QStringLiteral("SessionID")
                                                 << QStringLiteral("SESSIONID")
                                                 << QStringLiteral("Wilma2LoginID"));
            if (!sessionId.isEmpty()) {
                postIndexJson(sessionId);
                return;
            }
        }
        fetchLoginPageForFields();
        break;
    case RequestLoginJson:
        handleLoginBody(body, url);
        break;
    case RequestLoginPage: {
        m_htmlSessionId = extractNamedInput(QString::fromUtf8(body), QStringLiteral("SESSIONID"));
        if (m_htmlSessionId.isEmpty())
            get(QStringLiteral("/token"), RequestToken);
        else
            postHtmlLogin(m_htmlSessionId);
        break;
    }
    case RequestToken: {
        QString sessionId;
        const QJsonObject obj = QJsonDocument::fromJson(body).object();
        sessionId = jsonString(obj, QStringList()
                               << QStringLiteral("Wilma2LoginID")
                               << QStringLiteral("SessionID")
                               << QStringLiteral("SESSIONID"));
        if (sessionId.isEmpty()) {
            const QList<QNetworkCookie> cookies = m_cookies->cookiesForUrl(QUrl(m_schoolUrl));
            for (const QNetworkCookie &cookie : cookies) {
                if (cookie.name() == "Wilma2LoginID")
                    sessionId = QString::fromUtf8(cookie.value());
            }
        }
        if (sessionId.isEmpty()) {
            setBusy(false);
            setError(QStringLiteral("Could not start a Wilma login session"));
            emit loginFailed(m_error);
            if (m_restoring)
                finishRestore(false);
            return;
        }
        postHtmlLogin(sessionId);
        break;
    }
    case RequestLoginHtml:
        handleLoginBody(body, url);
        break;
    case RequestMfa: {
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        const QJsonObject obj = doc.object();
        const bool ok = obj.value(QStringLiteral("payload")).toObject()
                .value(QStringLiteral("success")).toBool(obj.value(QStringLiteral("success")).toBool());
        if (!ok) {
            setBusy(false);
            setError(QStringLiteral("Invalid verification code"));
            emit loginFailed(m_error);
            return;
        }
        completeLogin();
        break;
    }
    case RequestAccount: {
        if (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300) {
            const QJsonObject obj = QJsonDocument::fromJson(body).object();
            const QJsonObject payload = obj.contains(QStringLiteral("payload"))
                    ? obj.value(QStringLiteral("payload")).toObject()
                    : obj;
            const QString first = jsonString(payload, QStringList() << QStringLiteral("firstname")
                                                                    << QStringLiteral("FirstName"));
            const QString last = jsonString(payload, QStringList() << QStringLiteral("lastname")
                                                                   << QStringLiteral("LastName"));
            const QString combined = QStringLiteral("%1 %2").arg(first, last).trimmed();
            if (!combined.isEmpty() && combined != m_displayName) {
                m_displayName = combined;
                emit displayNameChanged();
                saveSettings();
            }
        }
        setBusy(false);
        break;
    }
    case RequestMessages: {
        if (status == 401 || status == 403
                || QString::fromUtf8(body).contains(QStringLiteral("loginFailed"), Qt::CaseInsensitive)) {
            if (hasCredentials() && !m_loggedIn) {
                startLogin();
                return;
            }
            if (m_restoring) {
                if (hasCredentials()) {
                    startLogin();
                    return;
                }
                finishRestore(false);
                return;
            }
            setBusy(false);
            break;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        QJsonArray messages;
        if (doc.isArray()) {
            messages = doc.array();
        } else if (doc.isObject()) {
            const QJsonObject obj = doc.object();
            if (obj.value(QStringLiteral("Messages")).isArray())
                messages = obj.value(QStringLiteral("Messages")).toArray();
            else if (obj.value(QStringLiteral("messages")).isArray())
                messages = obj.value(QStringLiteral("messages")).toArray();
        }
        if (!m_loggedIn)
            completeLogin(false);
        else
            setBusy(false);
        emitMessageNotifications(messages, m_firstMessagePoll || m_restoring);
        m_firstMessagePoll = false;
        if (m_restoring)
            finishRestore(true);
        break;
    }
    default:
        setBusy(false);
        break;
    }

    Q_UNUSED(location);
}

void WilmaClient::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void WilmaClient::setLoggedIn(bool loggedIn)
{
    if (m_loggedIn == loggedIn)
        return;
    m_loggedIn = loggedIn;
    emit loggedInChanged();
}

void WilmaClient::setError(const QString &message)
{
    if (m_error == message)
        return;
    m_error = message;
    emit errorMessageChanged();
}

void WilmaClient::setStatus(const QString &text)
{
    if (m_status == text)
        return;
    m_status = text;
    emit statusTextChanged();
}

void WilmaClient::setNeedsOtp(bool needsOtp)
{
    if (m_needsOtp == needsOtp)
        return;
    m_needsOtp = needsOtp;
    emit needsOtpChanged();
}

void WilmaClient::clearError()
{
    setError(QString());
}

void WilmaClient::saveSettings()
{
    QSettings settings;
    settings.setValue(QStringLiteral("schoolUrl"), m_schoolUrl);
    settings.setValue(QStringLiteral("schoolName"), m_schoolName);
    settings.setValue(QStringLiteral("username"), m_username);
    settings.setValue(QStringLiteral("password"), m_password);
    settings.setValue(QStringLiteral("sessionId"), m_sessionId);
    settings.setValue(QStringLiteral("displayName"), m_displayName);
    QVariantList seen;
    for (int id : m_seenMessageIds)
        seen.append(id);
    settings.setValue(QStringLiteral("seenMessageIds"), seen);
}

void WilmaClient::loadSettings()
{
    QSettings settings;
    m_schoolUrl = trimSlash(settings.value(QStringLiteral("schoolUrl")).toString());
    m_schoolName = settings.value(QStringLiteral("schoolName")).toString();
    m_username = settings.value(QStringLiteral("username")).toString();
    m_password = settings.value(QStringLiteral("password")).toString();
    m_sessionId = settings.value(QStringLiteral("sessionId")).toString();
    m_displayName = settings.value(QStringLiteral("displayName")).toString();
    const QVariantList seen = settings.value(QStringLiteral("seenMessageIds")).toList();
    for (const QVariant &id : seen)
        m_seenMessageIds.insert(id.toInt());
    applySessionCookie();
}

void WilmaClient::applySessionCookie()
{
    if (m_schoolUrl.isEmpty() || m_sessionId.isEmpty())
        return;
    QNetworkCookie cookie(QByteArray("Wilma2SID"), m_sessionId.toUtf8());
    cookie.setPath(QStringLiteral("/"));
    cookie.setHttpOnly(true);
    const QUrl url(m_schoolUrl);
    cookie.setDomain(url.host());
    m_cookies->setCookiesFromUrl(QList<QNetworkCookie>() << cookie, url);
}

QString WilmaClient::sessionCookie() const
{
    const QList<QNetworkCookie> cookies = m_cookies->cookiesForUrl(QUrl(m_schoolUrl));
    for (const QNetworkCookie &cookie : cookies) {
        if (cookie.name() == "Wilma2SID")
            return QString::fromUtf8(cookie.value());
    }
    return m_sessionId;
}

void WilmaClient::startLogin()
{
    if (m_schoolUrl.isEmpty()) {
        setError(QStringLiteral("Choose a Wilma first"));
        emit loginFailed(m_error);
        if (m_restoring)
            finishRestore(false);
        return;
    }
    setBusy(true);
    setStatus(QStringLiteral("Signing in…"));
    clearError();
    setNeedsOtp(false);
    get(QStringLiteral("/index_json"), RequestIndexJson);
}

void WilmaClient::postIndexJson(const QString &sessionId)
{
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("Login"), m_username);
    form.addQueryItem(QStringLiteral("Password"), m_password);
    form.addQueryItem(QStringLiteral("SESSIONID"), sessionId);
    form.addQueryItem(QStringLiteral("CompleteJson"), QString());
    form.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    postForm(QStringLiteral("/index_json"), form, RequestLoginJson);
}

void WilmaClient::postHtmlLogin(const QString &sessionId)
{
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("Login"), m_username);
    form.addQueryItem(QStringLiteral("Password"), m_password);
    form.addQueryItem(QStringLiteral("SESSIONID"), sessionId);
    postForm(QStringLiteral("/login"), form, RequestLoginHtml);
}

void WilmaClient::fetchLoginPageForFields()
{
    get(QStringLiteral("/login"), RequestLoginPage);
}

void WilmaClient::completeLogin(bool fetchMessages)
{
    m_sessionId = sessionCookie();
    setNeedsOtp(false);
    m_mfaFormkey.clear();
    setLoggedIn(true);
    saveSettings();
    setStatus(QStringLiteral("Signed in"));
    clearError();
    m_pollTimer.start();
    get(QStringLiteral("/api/v1/accounts/me"), RequestAccount);
    if (fetchMessages)
        get(QStringLiteral("/messages/list"), RequestMessages);
    if (!m_restoring)
        emit loginSucceeded();
}

void WilmaClient::handleLoginBody(const QByteArray &body, const QUrl &url)
{
    const QString text = QString::fromUtf8(body);
    const QString sid = sessionCookie();
    const bool failed = url.toString().contains(QStringLiteral("loginfailed"), Qt::CaseInsensitive)
            || text.contains(QStringLiteral("loginFailed"), Qt::CaseInsensitive);

    if (failed && sid.isEmpty()) {
        setBusy(false);
        setError(QStringLiteral("Wilma login failed. Check the username and password."));
        emit loginFailed(m_error);
        if (m_restoring)
            finishRestore(false);
        return;
    }

    const QString formkey = extractMfaFormkey(text);
    if (!formkey.isEmpty() && !sid.isEmpty()) {
        m_sessionId = sid;
        m_mfaFormkey = formkey;
        saveSettings();
        setBusy(false);
        setNeedsOtp(true);
        setStatus(QStringLiteral("Verification required"));
        emit otpRequired();
        return;
    }

    if (!sid.isEmpty()) {
        completeLogin();
        return;
    }

    setBusy(false);
    setError(QStringLiteral("Wilma login failed. Check the username and password."));
    emit loginFailed(m_error);
    if (m_restoring)
        finishRestore(false);
}

void WilmaClient::loadBundledSchools()
{
    QFile file(QStringLiteral(":/data/tenant_list.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Admirality: missing bundled tenant list";
        return;
    }
    parseSchoolsJson(file.readAll());
}

void WilmaClient::parseSchoolsJson(const QByteArray &data)
{
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    const QJsonArray wilmat = doc.object().value(QStringLiteral("wilmat")).toArray();
    QVariantList schools;
    schools.reserve(wilmat.size());
    for (const QJsonValue &value : wilmat) {
        const QJsonObject obj = value.toObject();
        QVariantMap item;
        const QString url = trimSlash(obj.value(QStringLiteral("url")).toString());
        const QString name = obj.value(QStringLiteral("name")).toString();
        item.insert(QStringLiteral("url"), url);
        item.insert(QStringLiteral("name"), name);
        QStringList cities;
        const QJsonArray municipalities = obj.value(QStringLiteral("municipalities")).toArray();
        for (const QJsonValue &cityValue : municipalities) {
            const QJsonObject city = cityValue.toObject();
            const QString fi = city.value(QStringLiteral("name_fi")).toString();
            const QString sv = city.value(QStringLiteral("name_sv")).toString();
            if (!fi.isEmpty())
                cities.append(fi);
            if (!sv.isEmpty() && sv != fi)
                cities.append(sv);
        }
        item.insert(QStringLiteral("cities"), cities.join(QStringLiteral(", ")));
        item.insert(QStringLiteral("searchText"),
                    QStringLiteral("%1 %2 %3").arg(name, cities.join(QLatin1Char(' ')), url).toLower());
        schools.append(item);
    }
    if (schools == m_schools)
        return;
    m_schools = schools;
    emit schoolsChanged();
}

QNetworkReply *WilmaClient::get(const QString &path, RequestKind kind)
{
    QUrl url(m_schoolUrl + path);
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", kUserAgent);
    request.setRawHeader("Referer", m_schoolUrl.toUtf8() + "/");
    request.setRawHeader("Accept", "application/json, text/html;q=0.9,*/*;q=0.8");
    QNetworkReply *reply = m_nam->get(request);
    reply->setProperty("kind", static_cast<int>(kind));
    connect(reply, SIGNAL(finished()), this, SLOT(onReplyFinished()));
    return reply;
}

QNetworkReply *WilmaClient::postForm(const QString &path,
                                     const QUrlQuery &form,
                                     RequestKind kind)
{
    QUrl url(m_schoolUrl + path);
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", kUserAgent);
    request.setRawHeader("Referer", m_schoolUrl.toUtf8() + "/");
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    QNetworkReply *reply = m_nam->post(request, form.toString(QUrl::FullyEncoded).toUtf8());
    reply->setProperty("kind", static_cast<int>(kind));
    connect(reply, SIGNAL(finished()), this, SLOT(onReplyFinished()));
    return reply;
}

void WilmaClient::finishRestore(bool loggedIn)
{
    if (!m_restoring)
        return;
    m_restoring = false;
    emit restoringSessionChanged();
    setBusy(false);
    if (loggedIn) {
        setLoggedIn(true);
        m_pollTimer.start();
    }
    emit restoreFinished(loggedIn);
}

void WilmaClient::emitMessageNotifications(const QJsonArray &messages, bool firstPoll)
{
    int unread = 0;
    QList<QVariantMap> fresh;
    for (const QJsonValue &value : messages) {
        const QJsonObject obj = value.toObject();
        int id = jsonInt(obj, QStringLiteral("Id"));
        if (id <= 0)
            id = jsonInt(obj, QStringLiteral("id"));
        if (id <= 0)
            continue;
        const bool unseen = !m_seenMessageIds.contains(id);
        const bool unreadFlag = messageUnread(obj);
        if (unreadFlag)
            unread += 1;
        if (unseen)
            m_seenMessageIds.insert(id);
        if (!firstPoll && unseen) {
            QVariantMap item;
            item.insert(QStringLiteral("id"), id);
            item.insert(QStringLiteral("title"),
                        jsonString(obj, QStringList() << QStringLiteral("Subject")
                                                     << QStringLiteral("subject")));
            item.insert(QStringLiteral("body"),
                        jsonString(obj, QStringList() << QStringLiteral("Sender")
                                                     << QStringLiteral("sender")
                                                     << QStringLiteral("SenderName")));
            fresh.append(item);
        }
    }
    if (m_unreadCount != unread) {
        m_unreadCount = unread;
        emit unreadCountChanged();
    }
    saveSettings();
    for (const QVariantMap &item : fresh) {
        emit notificationReceived(item.value(QStringLiteral("title")).toString(),
                                  item.value(QStringLiteral("body")).toString(),
                                  item);
    }
}
