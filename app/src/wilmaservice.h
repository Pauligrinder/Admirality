#ifndef WILMASERVICE_H
#define WILMASERVICE_H

#include <QObject>
#include <QString>
#include <QVariantMap>

class QTimer;
class WilmaClient;

// Session-bus API shared by the Sailfish UI and the Events View widgets.
// Same shape as Helmsman: GetState(), then a no-argument StateChanged
// signal. The widget calls GetState again when that signal arrives.
class WilmaService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.admirality.Wilma")
    Q_CLASSINFO("D-Bus Introspection", ""
"  <interface name=\"org.admirality.Wilma\">\n"
"    <method name=\"GetState\">\n"
"      <arg direction=\"out\" type=\"s\"/>\n"
"    </method>\n"
"    <method name=\"NormalizeSchoolUrl\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"raw\"/>\n"
"      <arg direction=\"out\" type=\"s\"/>\n"
"    </method>\n"
"    <method name=\"Refresh\"/>\n"
"    <method name=\"LoadSchools\"/>\n"
"    <method name=\"Login\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"username\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"password\"/>\n"
"    </method>\n"
"    <method name=\"SubmitOtp\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"code\"/>\n"
"    </method>\n"
"    <method name=\"Logout\"/>\n"
"    <method name=\"SelectSchool\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"url\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"name\"/>\n"
"    </method>\n"
"    <method name=\"SelectSchoolUrl\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"raw\"/>\n"
"    </method>\n"
"    <method name=\"SelectRole\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"roleId\"/>\n"
"    </method>\n"
"    <method name=\"LoadMessages\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"folder\"/>\n"
"    </method>\n"
"    <method name=\"OpenMessage\">\n"
"      <arg direction=\"in\" type=\"i\" name=\"messageId\"/>\n"
"    </method>\n"
"    <method name=\"OpenNews\">\n"
"      <arg direction=\"in\" type=\"i\" name=\"newsId\"/>\n"
"    </method>\n"
"    <method name=\"RestoreSession\"/>\n"
"    <method name=\"SetPollMode\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"mode\"/>\n"
"    </method>\n"
"    <method name=\"AcknowledgeNotes\"/>\n"
"    <method name=\"AcknowledgeNews\"/>\n"
"    <method name=\"AcknowledgeGrades\"/>\n"
"    <method name=\"AcknowledgeHomework\"/>\n"
"    <method name=\"AcknowledgeExams\"/>\n"
"    <method name=\"OpenView\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"view\"/>\n"
"    </method>\n"
"    <method name=\"ClearOpenView\"/>\n"
"    <signal name=\"StateChanged\"/>\n"
"  </interface>\n"
"")

public:
    explicit WilmaService(WilmaClient *client, QObject *parent = nullptr);

public slots:
    Q_SCRIPTABLE QString GetState();
    Q_SCRIPTABLE QString NormalizeSchoolUrl(const QString &raw);
    Q_SCRIPTABLE void Refresh();
    Q_SCRIPTABLE void LoadSchools();
    Q_SCRIPTABLE void Login(const QString &username, const QString &password);
    Q_SCRIPTABLE void SubmitOtp(const QString &code);
    Q_SCRIPTABLE void Logout();
    Q_SCRIPTABLE void SelectSchool(const QString &url, const QString &name);
    Q_SCRIPTABLE void SelectSchoolUrl(const QString &raw);
    Q_SCRIPTABLE void SelectRole(const QString &roleId);
    Q_SCRIPTABLE void LoadMessages(const QString &folder);
    Q_SCRIPTABLE void OpenMessage(int messageId);
    Q_SCRIPTABLE void OpenNews(int newsId);
    Q_SCRIPTABLE void RestoreSession();
    Q_SCRIPTABLE void SetPollMode(const QString &mode);
    Q_SCRIPTABLE void AcknowledgeNotes();
    Q_SCRIPTABLE void AcknowledgeNews();
    Q_SCRIPTABLE void AcknowledgeGrades();
    Q_SCRIPTABLE void AcknowledgeHomework();
    Q_SCRIPTABLE void AcknowledgeExams();
    Q_SCRIPTABLE void OpenView(const QString &view);
    Q_SCRIPTABLE void ClearOpenView();

signals:
    Q_SCRIPTABLE void StateChanged();

private slots:
    void publish();
    void schedule();

private:
    void postNotification(const QString &title, const QString &body);
    QString stateJson() const;
    bool registerService();

    WilmaClient *m_client;
    QTimer *m_timer;
    QString m_lastJson;
    int m_loginSucceededEpoch;
    int m_otpEpoch;
    int m_loginFailedEpoch;
    int m_restoreEpoch;
    int m_notificationEpoch;
    int m_openViewEpoch;
    bool m_restoreOk;
    QString m_openView;
    QString m_loginFailedMessage;
    QString m_notificationTitle;
    QString m_notificationBody;
};

#endif
