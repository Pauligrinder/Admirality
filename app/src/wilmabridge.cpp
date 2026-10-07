#include "wilmabridge.h"

#include <QDBusInterface>
#include <QDBusReply>
#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

namespace {

const char *kService = "org.admirality.harbour-admirality";
const char *kPath = "/wilma";
const char *kInterface = "org.admirality.Wilma";

bool setString(QString *target, const QString &value)
{
    if (*target == value)
        return false;
    *target = value;
    return true;
}

bool setBool(bool *target, bool value)
{
    if (*target == value)
        return false;
    *target = value;
    return true;
}

bool setInt(int *target, int value)
{
    if (*target == value)
        return false;
    *target = value;
    return true;
}

QString compact(const QJsonValue &value)
{
    if (value.isArray())
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    if (value.isObject())
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    return QString();
}

QString localNormalize(const QString &raw)
{
    QString value = raw.trimmed();
    if (value.isEmpty())
        return QString();
    if (!value.contains(QStringLiteral("://"))) {
        if (!value.contains(QLatin1Char('.')))
            value.append(QStringLiteral(".inschool.fi"));
        value.prepend(QStringLiteral("https://"));
    }
    while (value.endsWith(QLatin1Char('/')) && value.length() > 1)
        value.chop(1);
    return value;
}

}

WilmaBridge::WilmaBridge(QObject *parent)
    : QObject(parent)
    , m_retry(new QTimer(this))
    , m_bus(QDBusConnection::sessionBus())
    , m_serviceReady(false)
    , m_signalHooked(false)
    , m_haveEpochs(false)
    , m_busy(false)
    , m_loggedIn(false)
    , m_restoring(false)
    , m_needsOtp(false)
    , m_refreshing(false)
    , m_detailBusy(false)
    , m_hasSchool(false)
    , m_hasCredentials(false)
    , m_unreadCount(0)
    , m_freshNoteCount(0)
    , m_freshNewsCount(0)
    , m_freshGradeCount(0)
    , m_freshHomeworkCount(0)
    , m_freshExamCount(0)
    , m_loginSucceededEpoch(0)
    , m_otpEpoch(0)
    , m_loginFailedEpoch(0)
    , m_restoreEpoch(0)
    , m_notificationEpoch(0)
    , m_messageFolder(QStringLiteral("inbox"))
    , m_pollMode(QStringLiteral("15min"))
{
    m_retry->setInterval(2000);
    connect(m_retry, SIGNAL(timeout()), this, SLOT(tryConnect()));
    m_retry->start();
    QTimer::singleShot(0, this, SLOT(tryConnect()));
}

QString WilmaBridge::appVersion() const { return QStringLiteral(APP_VERSION); }
bool WilmaBridge::serviceReady() const { return m_serviceReady; }
bool WilmaBridge::busy() const { return m_busy; }
bool WilmaBridge::loggedIn() const { return m_loggedIn; }
bool WilmaBridge::restoringSession() const { return m_restoring; }
bool WilmaBridge::needsOtp() const { return m_needsOtp; }
QString WilmaBridge::errorMessage() const { return m_error; }
QString WilmaBridge::statusText() const { return m_status; }
QString WilmaBridge::schoolUrl() const { return m_schoolUrl; }
QString WilmaBridge::schoolName() const { return m_schoolName; }
QString WilmaBridge::schoolHost() const { return m_schoolHost; }
QString WilmaBridge::username() const { return m_username; }
QString WilmaBridge::password() const { return m_password; }
QString WilmaBridge::sessionId() const { return m_sessionId; }
QString WilmaBridge::displayName() const { return m_displayName; }
QString WilmaBridge::roleId() const { return m_roleId; }
QString WilmaBridge::roleName() const { return m_roleName; }
QString WilmaBridge::messageFolder() const { return m_messageFolder; }
int WilmaBridge::unreadCount() const { return m_unreadCount; }
QVariantList WilmaBridge::schools() const { return m_schools; }
QVariantList WilmaBridge::roles() const { return m_roles; }
QVariantList WilmaBridge::messages() const { return m_messages; }
QVariantList WilmaBridge::news() const { return m_news; }
QVariantList WilmaBridge::schedule() const { return m_schedule; }
QVariantList WilmaBridge::todaySchedule() const
{
    QVariantList today;
    const QString iso = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
    for (const QVariant &entry : m_schedule) {
        if (entry.toMap().value(QStringLiteral("date")).toString() == iso)
            today.append(entry);
    }
    return today;
}
QVariantList WilmaBridge::exams() const { return m_exams; }
QVariantList WilmaBridge::homework() const { return m_homework; }
QVariantList WilmaBridge::lessonNotes() const { return m_lessonNotes; }
int WilmaBridge::lessonNotesActionCount() const
{
    int n = 0;
    for (const QVariant &entry : m_lessonNotes) {
        if (entry.toMap().value(QStringLiteral("needsAction")).toBool())
            n += 1;
    }
    return n;
}
QVariantList WilmaBridge::grades() const { return m_grades; }
QVariantMap WilmaBridge::currentMessage() const { return m_currentMessage; }
QVariantMap WilmaBridge::currentNews() const { return m_currentNews; }
bool WilmaBridge::refreshing() const { return m_refreshing; }
bool WilmaBridge::detailBusy() const { return m_detailBusy; }
bool WilmaBridge::hasSchool() const { return m_hasSchool; }
bool WilmaBridge::hasCredentials() const { return m_hasCredentials; }
QString WilmaBridge::pollMode() const { return m_pollMode; }
int WilmaBridge::freshNoteCount() const { return m_freshNoteCount; }
int WilmaBridge::freshNewsCount() const { return m_freshNewsCount; }
int WilmaBridge::freshGradeCount() const { return m_freshGradeCount; }
int WilmaBridge::freshHomeworkCount() const { return m_freshHomeworkCount; }
int WilmaBridge::freshExamCount() const { return m_freshExamCount; }

QString WilmaBridge::normalizeSchoolUrl(const QString &raw) const
{
    return localNormalize(raw);
}

void WilmaBridge::invoke(const QString &method, const QVariantList &args)
{
    if (!m_bus.isConnected())
        return;
    QDBusMessage message = QDBusMessage::createMethodCall(
                QString::fromLatin1(kService),
                QString::fromLatin1(kPath),
                QString::fromLatin1(kInterface),
                method);
    message.setArguments(args);
    m_bus.call(message, QDBus::NoBlock);
}

void WilmaBridge::loadSchools() { invoke(QStringLiteral("LoadSchools")); }
void WilmaBridge::selectSchool(const QString &url, const QString &name)
{
    invoke(QStringLiteral("SelectSchool"), QVariantList() << url << name);
}
void WilmaBridge::selectSchoolUrl(const QString &raw)
{
    invoke(QStringLiteral("SelectSchoolUrl"), QVariantList() << raw);
}
void WilmaBridge::login(const QString &username, const QString &password)
{
    invoke(QStringLiteral("Login"), QVariantList() << username << password);
}
void WilmaBridge::submitOtp(const QString &code)
{
    invoke(QStringLiteral("SubmitOtp"), QVariantList() << code);
}
void WilmaBridge::restoreSession() { invoke(QStringLiteral("RestoreSession")); }
void WilmaBridge::logout() { invoke(QStringLiteral("Logout")); }
void WilmaBridge::pollMessages() { invoke(QStringLiteral("Refresh")); }
void WilmaBridge::refreshHome() { invoke(QStringLiteral("Refresh")); }
void WilmaBridge::loadMessages(const QString &folder)
{
    invoke(QStringLiteral("LoadMessages"), QVariantList() << folder);
}
void WilmaBridge::selectRole(const QString &roleId)
{
    invoke(QStringLiteral("SelectRole"), QVariantList() << roleId);
}
void WilmaBridge::openMessage(int messageId)
{
    bool changed = false;
    int unread = 0;
    for (int i = 0; i < m_messages.size(); ++i) {
        QVariantMap map = m_messages.at(i).toMap();
        if (map.value(QStringLiteral("id")).toInt() == messageId
                && map.value(QStringLiteral("unread")).toBool()) {
            map.insert(QStringLiteral("unread"), false);
            m_messages[i] = map;
            changed = true;
        }
        if (map.value(QStringLiteral("unread")).toBool())
            unread += 1;
    }
    if (changed) {
        m_messagesJson.clear();
        emit messagesChanged();
    }
    if (m_unreadCount != unread) {
        m_unreadCount = unread;
        emit unreadCountChanged();
    }
    invoke(QStringLiteral("OpenMessage"), QVariantList() << messageId);
}
void WilmaBridge::openNews(int newsId)
{
    invoke(QStringLiteral("OpenNews"), QVariantList() << newsId);
}
void WilmaBridge::setPollMode(const QString &mode)
{
    invoke(QStringLiteral("SetPollMode"), QVariantList() << mode);
}
void WilmaBridge::acknowledgeNotes() { invoke(QStringLiteral("AcknowledgeNotes")); }
void WilmaBridge::acknowledgeNews() { invoke(QStringLiteral("AcknowledgeNews")); }
void WilmaBridge::acknowledgeGrades() { invoke(QStringLiteral("AcknowledgeGrades")); }
void WilmaBridge::acknowledgeHomework() { invoke(QStringLiteral("AcknowledgeHomework")); }
void WilmaBridge::acknowledgeExams() { invoke(QStringLiteral("AcknowledgeExams")); }

void WilmaBridge::setServiceReady(bool ready)
{
    if (m_serviceReady == ready)
        return;
    m_serviceReady = ready;
    emit serviceReadyChanged();
    if (ready && m_retry->interval() != 15000) {
        m_retry->setInterval(15000);
    } else if (!ready && m_retry->interval() != 2000) {
        m_retry->setInterval(2000);
    }
}

void WilmaBridge::tryConnect()
{
    m_bus = QDBusConnection::sessionBus();
    if (!m_bus.isConnected()) {
        setServiceReady(false);
        return;
    }
    if (!m_signalHooked) {
        m_signalHooked = m_bus.connect(QString::fromLatin1(kService),
                                       QString::fromLatin1(kPath),
                                       QString::fromLatin1(kInterface),
                                       QStringLiteral("StateChanged"),
                                       this, SLOT(onStateChanged()));
    }
    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kInterface),
                         m_bus);
    if (!iface.isValid()) {
        setServiceReady(false);
        return;
    }
    const QDBusReply<QString> reply = iface.call(QDBus::Block, QStringLiteral("GetState"));
    if (!reply.isValid()) {
        setServiceReady(false);
        return;
    }
    applyState(reply.value());
    setServiceReady(true);
}

void WilmaBridge::onStateChanged()
{
    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kInterface),
                         m_bus);
    if (!iface.isValid()) {
        setServiceReady(false);
        return;
    }
    const QDBusReply<QString> reply = iface.call(QDBus::Block, QStringLiteral("GetState"));
    if (!reply.isValid()) {
        setServiceReady(false);
        return;
    }
    applyState(reply.value());
    setServiceReady(true);
}

void WilmaBridge::applyState(const QString &json)
{
    const QJsonObject root = QJsonDocument::fromJson(json.toUtf8()).object();
    if (root.isEmpty())
        return;

    if (setBool(&m_busy, root.value(QStringLiteral("busy")).toBool()))
        emit busyChanged();
    if (setBool(&m_loggedIn, root.value(QStringLiteral("loggedIn")).toBool()))
        emit loggedInChanged();
    if (setBool(&m_restoring, root.value(QStringLiteral("restoringSession")).toBool()))
        emit restoringSessionChanged();
    if (setBool(&m_needsOtp, root.value(QStringLiteral("needsOtp")).toBool()))
        emit needsOtpChanged();
    if (setString(&m_error, root.value(QStringLiteral("errorMessage")).toString()))
        emit errorMessageChanged();
    if (setString(&m_status, root.value(QStringLiteral("statusText")).toString()))
        emit statusTextChanged();
    if (setString(&m_schoolUrl, root.value(QStringLiteral("schoolUrl")).toString())
            | setString(&m_schoolHost, root.value(QStringLiteral("schoolHost")).toString())
            | setBool(&m_hasSchool, root.value(QStringLiteral("hasSchool")).toBool()))
        emit schoolUrlChanged();
    if (setString(&m_schoolName, root.value(QStringLiteral("schoolName")).toString()))
        emit schoolNameChanged();
    if (setString(&m_username, root.value(QStringLiteral("username")).toString()))
        emit usernameChanged();
    const bool passwordChangedFlag = setString(&m_password, root.value(QStringLiteral("password")).toString());
    const bool credentialsChangedFlag = setBool(&m_hasCredentials, root.value(QStringLiteral("hasCredentials")).toBool());
    if (passwordChangedFlag)
        emit passwordChanged();
    if (passwordChangedFlag || credentialsChangedFlag)
        emit credentialsChanged();
    if (setString(&m_sessionId, root.value(QStringLiteral("sessionId")).toString()))
        emit sessionIdChanged();
    if (setString(&m_displayName, root.value(QStringLiteral("displayName")).toString()))
        emit displayNameChanged();
    if (setString(&m_roleId, root.value(QStringLiteral("roleId")).toString()))
        emit roleIdChanged();
    if (setString(&m_roleName, root.value(QStringLiteral("roleName")).toString()))
        emit roleNameChanged();
    if (setString(&m_messageFolder, root.value(QStringLiteral("messageFolder")).toString()))
        emit messageFolderChanged();
    if (setInt(&m_unreadCount, root.value(QStringLiteral("unreadCount")).toInt()))
        emit unreadCountChanged();
    if (setBool(&m_refreshing, root.value(QStringLiteral("refreshing")).toBool()))
        emit refreshingChanged();
    if (setBool(&m_detailBusy, root.value(QStringLiteral("detailBusy")).toBool()))
        emit detailBusyChanged();
    if (setString(&m_pollMode, root.value(QStringLiteral("pollMode")).toString()))
        emit pollModeChanged();

    bool fresh = false;
    fresh |= setInt(&m_freshNoteCount, root.value(QStringLiteral("freshNoteCount")).toInt());
    fresh |= setInt(&m_freshNewsCount, root.value(QStringLiteral("freshNewsCount")).toInt());
    fresh |= setInt(&m_freshGradeCount, root.value(QStringLiteral("freshGradeCount")).toInt());
    fresh |= setInt(&m_freshHomeworkCount, root.value(QStringLiteral("freshHomeworkCount")).toInt());
    fresh |= setInt(&m_freshExamCount, root.value(QStringLiteral("freshExamCount")).toInt());
    if (fresh)
        emit freshCountsChanged();

    const QString schoolsJson = compact(root.value(QStringLiteral("schools")));
    if (schoolsJson != m_schoolsJson) {
        m_schoolsJson = schoolsJson;
        m_schools = root.value(QStringLiteral("schools")).toArray().toVariantList();
        emit schoolsChanged();
    }
    const QString rolesJson = compact(root.value(QStringLiteral("roles")));
    if (rolesJson != m_rolesJson) {
        m_rolesJson = rolesJson;
        m_roles = root.value(QStringLiteral("roles")).toArray().toVariantList();
        emit rolesChanged();
    }
    const QString messagesJson = compact(root.value(QStringLiteral("messages")));
    if (messagesJson != m_messagesJson) {
        m_messagesJson = messagesJson;
        m_messages = root.value(QStringLiteral("messages")).toArray().toVariantList();
        emit messagesChanged();
    }
    const QString newsJson = compact(root.value(QStringLiteral("news")));
    if (newsJson != m_newsJson) {
        m_newsJson = newsJson;
        m_news = root.value(QStringLiteral("news")).toArray().toVariantList();
        emit newsChanged();
    }
    const QString scheduleJson = compact(root.value(QStringLiteral("schedule")));
    if (scheduleJson != m_scheduleJson) {
        m_scheduleJson = scheduleJson;
        m_schedule = root.value(QStringLiteral("schedule")).toArray().toVariantList();
        emit scheduleChanged();
    }
    const QString examsJson = compact(root.value(QStringLiteral("exams")));
    if (examsJson != m_examsJson) {
        m_examsJson = examsJson;
        m_exams = root.value(QStringLiteral("exams")).toArray().toVariantList();
        emit examsChanged();
    }
    const QString homeworkJson = compact(root.value(QStringLiteral("homework")));
    if (homeworkJson != m_homeworkJson) {
        m_homeworkJson = homeworkJson;
        m_homework = root.value(QStringLiteral("homework")).toArray().toVariantList();
        emit homeworkChanged();
    }
    const QString notesJson = compact(root.value(QStringLiteral("lessonNotes")));
    if (notesJson != m_lessonNotesJson) {
        m_lessonNotesJson = notesJson;
        m_lessonNotes = root.value(QStringLiteral("lessonNotes")).toArray().toVariantList();
        emit lessonNotesChanged();
    }
    const QString gradesJson = compact(root.value(QStringLiteral("grades")));
    if (gradesJson != m_gradesJson) {
        m_gradesJson = gradesJson;
        m_grades = root.value(QStringLiteral("grades")).toArray().toVariantList();
        emit gradesChanged();
    }
    const QString messageJson = compact(root.value(QStringLiteral("currentMessage")));
    if (messageJson != m_currentMessageJson) {
        m_currentMessageJson = messageJson;
        m_currentMessage = root.value(QStringLiteral("currentMessage")).toObject().toVariantMap();
        emit currentMessageChanged();
    }
    const QString newsItemJson = compact(root.value(QStringLiteral("currentNews")));
    if (newsItemJson != m_currentNewsJson) {
        m_currentNewsJson = newsItemJson;
        m_currentNews = root.value(QStringLiteral("currentNews")).toObject().toVariantMap();
        emit currentNewsChanged();
    }

    const int loginEpoch = root.value(QStringLiteral("loginSucceededEpoch")).toInt();
    const int otpEpoch = root.value(QStringLiteral("otpEpoch")).toInt();
    const int failedEpoch = root.value(QStringLiteral("loginFailedEpoch")).toInt();
    const int restoreEpoch = root.value(QStringLiteral("restoreEpoch")).toInt();
    const int notificationEpoch = root.value(QStringLiteral("notificationEpoch")).toInt();
    if (!m_haveEpochs) {
        m_loginSucceededEpoch = loginEpoch;
        m_otpEpoch = otpEpoch;
        m_loginFailedEpoch = failedEpoch;
        m_restoreEpoch = restoreEpoch;
        m_notificationEpoch = notificationEpoch;
        m_haveEpochs = true;
        return;
    }
    if (loginEpoch > m_loginSucceededEpoch) {
        m_loginSucceededEpoch = loginEpoch;
        emit loginSucceeded();
    }
    if (otpEpoch > m_otpEpoch) {
        m_otpEpoch = otpEpoch;
        emit otpRequired();
    }
    if (failedEpoch > m_loginFailedEpoch) {
        m_loginFailedEpoch = failedEpoch;
        emit loginFailed(root.value(QStringLiteral("loginFailedMessage")).toString());
    }
    if (restoreEpoch > m_restoreEpoch) {
        m_restoreEpoch = restoreEpoch;
        emit restoreFinished(root.value(QStringLiteral("restoreOk")).toBool());
    }
    if (notificationEpoch > m_notificationEpoch) {
        m_notificationEpoch = notificationEpoch;
        emit notificationReceived(root.value(QStringLiteral("notificationTitle")).toString(),
                                  root.value(QStringLiteral("notificationBody")).toString(),
                                  QVariantMap());
    }
}
