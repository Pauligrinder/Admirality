#include "wilmaservice.h"

#include "wilmaclient.h"

#include <QDBusConnection>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

namespace {

const char *kService = "org.admirality.harbour-admirality";
const char *kPath = "/wilma";

QJsonValue listValue(const QVariantList &list)
{
    return QJsonValue(QJsonArray::fromVariantList(list));
}

}

WilmaService::WilmaService(WilmaClient *client, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_timer(new QTimer(this))
    , m_loginSucceededEpoch(0)
    , m_otpEpoch(0)
    , m_loginFailedEpoch(0)
    , m_restoreEpoch(0)
    , m_notificationEpoch(0)
    , m_restoreOk(false)
{
    m_timer->setSingleShot(true);
    m_timer->setInterval(50);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(publish()));

    const char *slot = SLOT(schedule());
    connect(client, SIGNAL(busyChanged()), this, slot);
    connect(client, SIGNAL(loggedInChanged()), this, slot);
    connect(client, SIGNAL(restoringSessionChanged()), this, slot);
    connect(client, SIGNAL(needsOtpChanged()), this, slot);
    connect(client, SIGNAL(errorMessageChanged()), this, slot);
    connect(client, SIGNAL(statusTextChanged()), this, slot);
    connect(client, SIGNAL(schoolUrlChanged()), this, slot);
    connect(client, SIGNAL(schoolNameChanged()), this, slot);
    connect(client, SIGNAL(usernameChanged()), this, slot);
    connect(client, SIGNAL(passwordChanged()), this, slot);
    connect(client, SIGNAL(credentialsChanged()), this, slot);
    connect(client, SIGNAL(sessionIdChanged()), this, slot);
    connect(client, SIGNAL(displayNameChanged()), this, slot);
    connect(client, SIGNAL(roleIdChanged()), this, slot);
    connect(client, SIGNAL(roleNameChanged()), this, slot);
    connect(client, SIGNAL(messageFolderChanged()), this, slot);
    connect(client, SIGNAL(unreadCountChanged()), this, slot);
    connect(client, SIGNAL(schoolsChanged()), this, slot);
    connect(client, SIGNAL(rolesChanged()), this, slot);
    connect(client, SIGNAL(messagesChanged()), this, slot);
    connect(client, SIGNAL(newsChanged()), this, slot);
    connect(client, SIGNAL(scheduleChanged()), this, slot);
    connect(client, SIGNAL(examsChanged()), this, slot);
    connect(client, SIGNAL(homeworkChanged()), this, slot);
    connect(client, SIGNAL(lessonNotesChanged()), this, slot);
    connect(client, SIGNAL(gradesChanged()), this, slot);
    connect(client, SIGNAL(currentMessageChanged()), this, slot);
    connect(client, SIGNAL(currentNewsChanged()), this, slot);
    connect(client, SIGNAL(refreshingChanged()), this, slot);
    connect(client, SIGNAL(detailBusyChanged()), this, slot);
    connect(client, SIGNAL(pollModeChanged()), this, slot);
    connect(client, SIGNAL(freshCountsChanged()), this, slot);

    connect(client, &WilmaClient::loginSucceeded, this, [this]() {
        m_loginSucceededEpoch += 1;
        schedule();
    });
    connect(client, &WilmaClient::otpRequired, this, [this]() {
        m_otpEpoch += 1;
        schedule();
    });
    connect(client, &WilmaClient::loginFailed, this, [this](const QString &message) {
        m_loginFailedEpoch += 1;
        m_loginFailedMessage = message;
        schedule();
    });
    connect(client, &WilmaClient::restoreFinished, this, [this](bool loggedIn) {
        m_restoreEpoch += 1;
        m_restoreOk = loggedIn;
        schedule();
    });
    connect(client, &WilmaClient::notificationReceived, this,
            [this](const QString &title, const QString &message, const QVariantMap &) {
        m_notificationEpoch += 1;
        m_notificationTitle = title;
        m_notificationBody = message;
        postNotification(title, message);
        schedule();
    });

    registerService();
    schedule();
}

bool WilmaService::registerService()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QString::fromLatin1(kService))) {
        qWarning() << "Admirality: D-Bus name already taken";
        return false;
    }
    if (!bus.registerObject(QString::fromLatin1(kPath), this,
                            QDBusConnection::ExportScriptableSlots
                            | QDBusConnection::ExportScriptableSignals)) {
        qWarning() << "Admirality: could not export D-Bus object";
        bus.unregisterService(QString::fromLatin1(kService));
        return false;
    }
    qWarning() << "Admirality: D-Bus registered as" << kService;
    return true;
}

void WilmaService::schedule()
{
    if (!m_timer->isActive())
        m_timer->start();
}

void WilmaService::publish()
{
    const QString json = stateJson();
    if (json == m_lastJson)
        return;
    m_lastJson = json;
    emit StateChanged();
}

void WilmaService::postNotification(const QString &title, const QString &body)
{
    const QString summary = title.isEmpty() ? QStringLiteral("Wilma") : title;
    QDBusMessage message = QDBusMessage::createMethodCall(
                QStringLiteral("org.freedesktop.Notifications"),
                QStringLiteral("/org/freedesktop/Notifications"),
                QStringLiteral("org.freedesktop.Notifications"),
                QStringLiteral("Notify"));
    QVariantMap hints;
    hints.insert(QStringLiteral("desktop-entry"), QStringLiteral("harbour-admirality"));
    hints.insert(QStringLiteral("x-nemo-preview-summary"), summary);
    hints.insert(QStringLiteral("x-nemo-preview-body"), body);
    hints.insert(QStringLiteral("x-nemo-feedback"), QStringLiteral("chat_exists"));
    message << QStringLiteral("Admirality")
            << uint(0)
            << QStringLiteral("harbour-admirality")
            << summary
            << body
            << QStringList()
            << hints
            << int(0);
    QDBusConnection::sessionBus().call(message, QDBus::NoBlock);
}

QString WilmaService::stateJson() const
{
    QJsonObject root;
    root.insert(QStringLiteral("busy"), m_client->busy());
    root.insert(QStringLiteral("loggedIn"), m_client->loggedIn());
    root.insert(QStringLiteral("restoringSession"), m_client->restoringSession());
    root.insert(QStringLiteral("needsOtp"), m_client->needsOtp());
    root.insert(QStringLiteral("errorMessage"), m_client->errorMessage());
    root.insert(QStringLiteral("statusText"), m_client->statusText());
    root.insert(QStringLiteral("schoolUrl"), m_client->schoolUrl());
    root.insert(QStringLiteral("schoolName"), m_client->schoolName());
    root.insert(QStringLiteral("schoolHost"), m_client->schoolHost());
    root.insert(QStringLiteral("username"), m_client->username());
    root.insert(QStringLiteral("password"), m_client->password());
    root.insert(QStringLiteral("sessionId"), m_client->sessionId());
    root.insert(QStringLiteral("displayName"), m_client->displayName());
    root.insert(QStringLiteral("roleId"), m_client->roleId());
    root.insert(QStringLiteral("roleName"), m_client->roleName());
    root.insert(QStringLiteral("messageFolder"), m_client->messageFolder());
    root.insert(QStringLiteral("unreadCount"), m_client->unreadCount());
    root.insert(QStringLiteral("hasSchool"), m_client->hasSchool());
    root.insert(QStringLiteral("hasCredentials"), m_client->hasCredentials());
    root.insert(QStringLiteral("refreshing"), m_client->refreshing());
    root.insert(QStringLiteral("detailBusy"), m_client->detailBusy());
    root.insert(QStringLiteral("pollMode"), m_client->pollMode());
    root.insert(QStringLiteral("lessonNotesActionCount"), m_client->lessonNotesActionCount());
    root.insert(QStringLiteral("freshNoteCount"), m_client->freshNoteCount());
    root.insert(QStringLiteral("freshNewsCount"), m_client->freshNewsCount());
    root.insert(QStringLiteral("freshGradeCount"), m_client->freshGradeCount());
    root.insert(QStringLiteral("freshHomeworkCount"), m_client->freshHomeworkCount());
    root.insert(QStringLiteral("freshExamCount"), m_client->freshExamCount());
    root.insert(QStringLiteral("schools"), listValue(m_client->schools()));
    root.insert(QStringLiteral("roles"), listValue(m_client->roles()));
    root.insert(QStringLiteral("messages"), listValue(m_client->messages()));
    root.insert(QStringLiteral("news"), listValue(m_client->news()));
    root.insert(QStringLiteral("schedule"), listValue(m_client->schedule()));
    root.insert(QStringLiteral("exams"), listValue(m_client->exams()));
    root.insert(QStringLiteral("homework"), listValue(m_client->homework()));
    root.insert(QStringLiteral("lessonNotes"), listValue(m_client->lessonNotes()));
    root.insert(QStringLiteral("grades"), listValue(m_client->grades()));
    root.insert(QStringLiteral("currentMessage"), QJsonObject::fromVariantMap(m_client->currentMessage()));
    root.insert(QStringLiteral("currentNews"), QJsonObject::fromVariantMap(m_client->currentNews()));
    root.insert(QStringLiteral("loginSucceededEpoch"), m_loginSucceededEpoch);
    root.insert(QStringLiteral("otpEpoch"), m_otpEpoch);
    root.insert(QStringLiteral("loginFailedEpoch"), m_loginFailedEpoch);
    root.insert(QStringLiteral("loginFailedMessage"), m_loginFailedMessage);
    root.insert(QStringLiteral("restoreEpoch"), m_restoreEpoch);
    root.insert(QStringLiteral("restoreOk"), m_restoreOk);
    root.insert(QStringLiteral("notificationEpoch"), m_notificationEpoch);
    root.insert(QStringLiteral("notificationTitle"), m_notificationTitle);
    root.insert(QStringLiteral("notificationBody"), m_notificationBody);
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

QString WilmaService::GetState()
{
    m_lastJson = stateJson();
    return m_lastJson;
}

QString WilmaService::NormalizeSchoolUrl(const QString &raw)
{
    return m_client->normalizeSchoolUrl(raw);
}

void WilmaService::Refresh() { m_client->refreshHome(); }
void WilmaService::LoadSchools() { m_client->loadSchools(); }
void WilmaService::Login(const QString &username, const QString &password)
{
    m_client->login(username, password);
}
void WilmaService::SubmitOtp(const QString &code) { m_client->submitOtp(code); }
void WilmaService::Logout() { m_client->logout(); }
void WilmaService::SelectSchool(const QString &url, const QString &name)
{
    m_client->selectSchool(url, name);
}
void WilmaService::SelectSchoolUrl(const QString &raw) { m_client->selectSchoolUrl(raw); }
void WilmaService::SelectRole(const QString &roleId) { m_client->selectRole(roleId); }
void WilmaService::LoadMessages(const QString &folder) { m_client->loadMessages(folder); }
void WilmaService::OpenMessage(int messageId) { m_client->openMessage(messageId); }
void WilmaService::OpenNews(int newsId) { m_client->openNews(newsId); }
void WilmaService::RestoreSession() { m_client->restoreSession(); }
void WilmaService::SetPollMode(const QString &mode) { m_client->setPollMode(mode); }
void WilmaService::AcknowledgeNotes() { m_client->acknowledgeNotes(); }
void WilmaService::AcknowledgeNews() { m_client->acknowledgeNews(); }
void WilmaService::AcknowledgeGrades() { m_client->acknowledgeGrades(); }
void WilmaService::AcknowledgeHomework() { m_client->acknowledgeHomework(); }
void WilmaService::AcknowledgeExams() { m_client->acknowledgeExams(); }
