#ifndef WILMACLIENT_H
#define WILMACLIENT_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMap>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;
class QNetworkCookieJar;

class WilmaClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
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
    explicit WilmaClient(QObject *parent = nullptr);
    ~WilmaClient() override;

    QString appVersion() const;
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
    int roleUnreadTotal(const QString &roleId) const;

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
    void onReplyFinished();

private:
    enum RequestKind {
        RequestNone,
        RequestIndexJson,
        RequestLoginJson,
        RequestToken,
        RequestLoginHtml,
        RequestLoginPage,
        RequestPostLogin,
        RequestMfa,
        RequestAccount,
        RequestRoles,
        RequestHomeHtml,
        RequestMessages,
        RequestOverview,
        RequestNewsList,
        RequestNewsItem,
        RequestMessageItem,
        RequestAttendance,
        RequestRoleUnreadProbe
    };

    void setBusy(bool busy);
    void setLoggedIn(bool loggedIn);
    void setError(const QString &message);
    void setStatus(const QString &text);
    void setNeedsOtp(bool needsOtp);
    void clearError();
    void saveSettings();
    void loadSettings();
    void applySessionCookie();
    void ensureLoginCookie(const QString &sessionId);
    void ingestReplyCookies(QNetworkReply *reply);
    QString sessionCookie() const;
    QString loginCookie() const;
    QString cookieValueFromReply(QNetworkReply *reply, const QByteArray &name) const;
    QByteArray cookieHeader() const;
    void applyRequestCookies(QNetworkRequest *request) const;
    void startLogin();
    void postIndexJson(const QString &sessionId);
    void postHtmlLogin(const QString &sessionId);
    void fetchLoginPageForFields();
    void tryIndexJsonLogin();
    void retryLogin(const QString &reason, bool wrongPassword);
    void completeLogin(bool fetchMessages = true);
    void handleLoginBody(QNetworkReply *reply,
                         const QByteArray &body,
                         const QUrl &url,
                         const QUrl &redirectUrl);
    void continueAfterLoginRedirect(const QUrl &redirectUrl);
    QUrl resolveRedirect(const QNetworkReply *reply) const;
    void failLogin(const QString &message);
    void loadBundledSchools();
    void parseSchoolsJson(const QByteArray &data);
    QNetworkReply *get(const QString &path, RequestKind kind);
    QNetworkReply *postForm(const QString &path,
                            const QUrlQuery &form,
                            RequestKind kind);
    QString rolePath(const QString &path) const;
    bool usesRolePrefix(RequestKind kind) const;
    void rememberRoleFromUrl(const QUrl &url);
    void applyRolesJson(const QByteArray &body);
    void applyRolesFromHtml(const QString &html, const QUrl &pageUrl);
    void ensureRoleSelected();
    void fetchRoles();
    void setRole(const QString &roleId, const QString &roleName);
    void finishRestore(bool loggedIn);
    void emitMessageNotifications(const QJsonArray &messages, bool firstPoll);
    void applyMessageList(const QJsonArray &messages);
    void applyMessageListHtml(const QString &html);
    QJsonArray extractMessagesArray(const QJsonDocument &doc) const;
    void applyOverview(const QJsonObject &obj);
    void applyNewsList(const QByteArray &body);
    void applyAttendance(const QByteArray &body, const QString &contentType);
    void applyLessonNotes(const QVariantList &items);
    void applyMessageDetail(int messageId, const QByteArray &body, const QString &contentType);
    void applyNewsDetail(int newsId, const QByteArray &body, const QString &contentType);
    void clearHomeData();
    QString messagesListPath() const;
    void setRefreshing(bool refreshing);
    void setDetailBusy(bool busy);
    void armPollTimer();
    int msecsUntilSchoolPoll() const;
    void markMessageRead(int messageId);
    void absorbFresh(const QString &category, const QVariantList &hits);
    void acknowledgeCategory(const QString &category);
    int freshCount(const QString &category) const;
    void endRefreshIfMarked(QNetworkReply *reply);
    bool isInvalidSession(int status, const QByteArray &body) const;
    bool handleContentAuthFailure(int status, const QByteArray &body);
    int currentUnreadStuff() const;
    void setRoleScore(const QString &roleId, int score);
    void startRoleUnreadProbes();
    void handleRoleUnreadProbe(QNetworkReply *reply, const QByteArray &body);
    void maybeAutoSelectHottestRole();

    QNetworkAccessManager *m_nam;
    QNetworkCookieJar *m_cookies;
    QTimer m_pollTimer;
    bool m_busy;
    bool m_loggedIn;
    bool m_restoring;
    bool m_needsOtp;
    bool m_firstMessagePoll;
    bool m_loginInProgress;
    int m_loginTry;
    int m_postLoginHops;
    QString m_error;
    QString m_status;
    QString m_schoolUrl;
    QString m_schoolName;
    QString m_username;
    QString m_password;
    QString m_displayName;
    QString m_roleId;
    QString m_roleName;
    QString m_messageFolder;
    QString m_pollMode;
    QString m_sessionId;
    QString m_loginSessionId;
    QString m_mfaFormkey;
    QString m_htmlSessionId;
    QMap<QString, QString> m_loginFields;
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
    int m_unreadCount;
    int m_pendingRefresh;
    bool m_refreshing;
    bool m_detailBusy;
    bool m_freshReady;
    QSet<int> m_seenMessageIds;
    QSet<int> m_locallyReadIds;
    QHash<QString, QSet<QString>> m_knownKeys;
    QHash<QString, QSet<QString>> m_unseenKeys;
    QSet<QString> m_seededCategories;
    int m_roleProbePending;
    bool m_roleScoresChanged;
    QHash<QString, int> m_roleUnreadScores;
    QHash<QString, int> m_roleFreshTotals;
};

#endif
