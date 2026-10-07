#ifndef WILMABRIDGE_H
#define WILMABRIDGE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QDBusConnection>

class QTimer;

// App-side view of the Wilma loader. The daemon owns the network session;
// this object keeps the same properties the QML pages already use.
class WilmaBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(bool serviceReady READ serviceReady NOTIFY serviceReadyChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY loggedInChanged)
    Q_PROPERTY(bool restoringSession READ restoringSession NOTIFY restoringSessionChanged)
    Q_PROPERTY(bool needsOtp READ needsOtp NOTIFY needsOtpChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString schoolUrl READ schoolUrl NOTIFY schoolUrlChanged)
    Q_PROPERTY(QString schoolName READ schoolName NOTIFY schoolNameChanged)
    Q_PROPERTY(QString schoolHost READ schoolHost NOTIFY schoolUrlChanged)
    Q_PROPERTY(QString username READ username NOTIFY usernameChanged)
    Q_PROPERTY(QString password READ password NOTIFY passwordChanged)
    Q_PROPERTY(QString sessionId READ sessionId NOTIFY sessionIdChanged)
    Q_PROPERTY(QString displayName READ displayName NOTIFY displayNameChanged)
    Q_PROPERTY(QString roleId READ roleId NOTIFY roleIdChanged)
    Q_PROPERTY(QString roleName READ roleName NOTIFY roleNameChanged)
    Q_PROPERTY(QString messageFolder READ messageFolder NOTIFY messageFolderChanged)
    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY unreadCountChanged)
    Q_PROPERTY(QVariantList schools READ schools NOTIFY schoolsChanged)
    Q_PROPERTY(QVariantList roles READ roles NOTIFY rolesChanged)
    Q_PROPERTY(QVariantList messages READ messages NOTIFY messagesChanged)
    Q_PROPERTY(QVariantList news READ news NOTIFY newsChanged)
    Q_PROPERTY(QVariantList schedule READ schedule NOTIFY scheduleChanged)
    Q_PROPERTY(QVariantList todaySchedule READ todaySchedule NOTIFY scheduleChanged)
    Q_PROPERTY(QVariantList exams READ exams NOTIFY examsChanged)
    Q_PROPERTY(QVariantList homework READ homework NOTIFY homeworkChanged)
    Q_PROPERTY(QVariantList lessonNotes READ lessonNotes NOTIFY lessonNotesChanged)
    Q_PROPERTY(int lessonNotesActionCount READ lessonNotesActionCount NOTIFY lessonNotesChanged)
    Q_PROPERTY(QVariantList grades READ grades NOTIFY gradesChanged)
    Q_PROPERTY(QVariantMap currentMessage READ currentMessage NOTIFY currentMessageChanged)
    Q_PROPERTY(QVariantMap currentNews READ currentNews NOTIFY currentNewsChanged)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY refreshingChanged)
    Q_PROPERTY(bool detailBusy READ detailBusy NOTIFY detailBusyChanged)
    Q_PROPERTY(bool hasSchool READ hasSchool NOTIFY schoolUrlChanged)
    Q_PROPERTY(bool hasCredentials READ hasCredentials NOTIFY credentialsChanged)
    Q_PROPERTY(QString pollMode READ pollMode NOTIFY pollModeChanged)
    Q_PROPERTY(int freshNoteCount READ freshNoteCount NOTIFY freshCountsChanged)
    Q_PROPERTY(int freshNewsCount READ freshNewsCount NOTIFY freshCountsChanged)
    Q_PROPERTY(int freshGradeCount READ freshGradeCount NOTIFY freshCountsChanged)
    Q_PROPERTY(int freshHomeworkCount READ freshHomeworkCount NOTIFY freshCountsChanged)
    Q_PROPERTY(int freshExamCount READ freshExamCount NOTIFY freshCountsChanged)

public:
    explicit WilmaBridge(QObject *parent = nullptr);

    QString appVersion() const;
    bool serviceReady() const;
    bool busy() const;
    bool loggedIn() const;
    bool restoringSession() const;
    bool needsOtp() const;
    QString errorMessage() const;
    QString statusText() const;
    QString schoolUrl() const;
    QString schoolName() const;
    QString schoolHost() const;
    QString username() const;
    QString password() const;
    QString sessionId() const;
    QString displayName() const;
    QString roleId() const;
    QString roleName() const;
    QString messageFolder() const;
    int unreadCount() const;
    QVariantList schools() const;
    QVariantList roles() const;
    QVariantList messages() const;
    QVariantList news() const;
    QVariantList schedule() const;
    QVariantList todaySchedule() const;
    QVariantList exams() const;
    QVariantList homework() const;
    QVariantList lessonNotes() const;
    int lessonNotesActionCount() const;
    QVariantList grades() const;
    QVariantMap currentMessage() const;
    QVariantMap currentNews() const;
    bool refreshing() const;
    bool detailBusy() const;
    bool hasSchool() const;
    bool hasCredentials() const;
    QString pollMode() const;
    int freshNoteCount() const;
    int freshNewsCount() const;
    int freshGradeCount() const;
    int freshHomeworkCount() const;
    int freshExamCount() const;

    Q_INVOKABLE QString normalizeSchoolUrl(const QString &raw) const;

public slots:
    void loadSchools();
    void selectSchool(const QString &url, const QString &name);
    void selectSchoolUrl(const QString &raw);
    void login(const QString &username, const QString &password);
    void submitOtp(const QString &code);
    void restoreSession();
    void logout();
    void pollMessages();
    void refreshHome();
    void loadMessages(const QString &folder = QString());
    void selectRole(const QString &roleId);
    void openMessage(int messageId);
    void openNews(int newsId);
    void setPollMode(const QString &mode);
    void acknowledgeNotes();
    void acknowledgeNews();
    void acknowledgeGrades();
    void acknowledgeHomework();
    void acknowledgeExams();

signals:
    void serviceReadyChanged();
    void busyChanged();
    void loggedInChanged();
    void restoringSessionChanged();
    void needsOtpChanged();
    void errorMessageChanged();
    void statusTextChanged();
    void schoolUrlChanged();
    void schoolNameChanged();
    void usernameChanged();
    void passwordChanged();
    void credentialsChanged();
    void sessionIdChanged();
    void displayNameChanged();
    void roleIdChanged();
    void roleNameChanged();
    void messageFolderChanged();
    void unreadCountChanged();
    void schoolsChanged();
    void rolesChanged();
    void messagesChanged();
    void newsChanged();
    void scheduleChanged();
    void examsChanged();
    void homeworkChanged();
    void lessonNotesChanged();
    void gradesChanged();
    void currentMessageChanged();
    void currentNewsChanged();
    void refreshingChanged();
    void detailBusyChanged();
    void pollModeChanged();
    void freshCountsChanged();
    void restoreFinished(bool loggedIn);
    void loginSucceeded();
    void otpRequired();
    void loginFailed(const QString &message);
    void notificationReceived(const QString &title,
                              const QString &message,
                              const QVariantMap &data);

private slots:
    void tryConnect();
    void onStateChanged();

private:
    void invoke(const QString &method, const QVariantList &args = QVariantList());
    void applyState(const QString &json);
    void setServiceReady(bool ready);

    QTimer *m_retry;
    QDBusConnection m_bus;
    bool m_serviceReady;
    bool m_signalHooked;
    bool m_haveEpochs;
    bool m_busy;
    bool m_loggedIn;
    bool m_restoring;
    bool m_needsOtp;
    bool m_refreshing;
    bool m_detailBusy;
    bool m_hasSchool;
    bool m_hasCredentials;
    int m_unreadCount;
    int m_freshNoteCount;
    int m_freshNewsCount;
    int m_freshGradeCount;
    int m_freshHomeworkCount;
    int m_freshExamCount;
    int m_loginSucceededEpoch;
    int m_otpEpoch;
    int m_loginFailedEpoch;
    int m_restoreEpoch;
    int m_notificationEpoch;
    QString m_error;
    QString m_status;
    QString m_schoolUrl;
    QString m_schoolName;
    QString m_schoolHost;
    QString m_username;
    QString m_password;
    QString m_sessionId;
    QString m_displayName;
    QString m_roleId;
    QString m_roleName;
    QString m_messageFolder;
    QString m_pollMode;
    QString m_schoolsJson;
    QString m_rolesJson;
    QString m_messagesJson;
    QString m_newsJson;
    QString m_scheduleJson;
    QString m_examsJson;
    QString m_homeworkJson;
    QString m_lessonNotesJson;
    QString m_gradesJson;
    QString m_currentMessageJson;
    QString m_currentNewsJson;
    QVariantList m_schools;
    QVariantList m_roles;
    QVariantList m_messages;
    QVariantList m_news;
    QVariantList m_schedule;
    QVariantList m_exams;
    QVariantList m_homework;
    QVariantList m_lessonNotes;
    QVariantList m_grades;
    QVariantMap m_currentMessage;
    QVariantMap m_currentNews;
};

#endif
