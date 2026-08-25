#ifndef WILMACLIENT_H
#define WILMACLIENT_H

#include <QObject>
#include <QJsonArray>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

class QNetworkAccessManager;
class QNetworkReply;
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
    Q_PROPERTY(QString displayName READ displayName NOTIFY displayNameChanged)
    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY unreadCountChanged)
    Q_PROPERTY(QVariantList schools READ schools NOTIFY schoolsChanged)
    Q_PROPERTY(bool hasSchool READ hasSchool NOTIFY schoolUrlChanged)
    Q_PROPERTY(bool hasCredentials READ hasCredentials NOTIFY usernameChanged)

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
    QString displayName() const;
    int unreadCount() const;
    QVariantList schools() const;
    bool hasSchool() const;
    bool hasCredentials() const;

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
    void displayNameChanged();
    void unreadCountChanged();
    void schoolsChanged();
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
        RequestMfa,
        RequestAccount,
        RequestMessages
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
    QString sessionCookie() const;
    void startLogin();
    void postIndexJson(const QString &sessionId);
    void postHtmlLogin(const QString &sessionId);
    void fetchLoginPageForFields();
    void completeLogin(bool fetchMessages = true);
    void handleLoginBody(const QByteArray &body, const QUrl &url);
    void loadBundledSchools();
    void parseSchoolsJson(const QByteArray &data);
    QNetworkReply *get(const QString &path, RequestKind kind);
    QNetworkReply *postForm(const QString &path,
                            const QUrlQuery &form,
                            RequestKind kind);
    void finishRestore(bool loggedIn);
    void emitMessageNotifications(const QJsonArray &messages, bool firstPoll);

    QNetworkAccessManager *m_nam;
    QNetworkCookieJar *m_cookies;
    QTimer m_pollTimer;
    bool m_busy;
    bool m_loggedIn;
    bool m_restoring;
    bool m_needsOtp;
    bool m_firstMessagePoll;
    QString m_error;
    QString m_status;
    QString m_schoolUrl;
    QString m_schoolName;
    QString m_username;
    QString m_password;
    QString m_displayName;
    QString m_sessionId;
    QString m_mfaFormkey;
    QString m_htmlSessionId;
    QVariantList m_schools;
    int m_unreadCount;
    QSet<int> m_seenMessageIds;
};

#endif
