#include <sailfishapp.h>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QQmlContext>
#include <QQuickView>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

#include "wilmabridge.h"

int main(int argc, char *argv[])
{
    // Must match [X-Sailjail] in harbour-admirality.desktop *exactly*
    // (harbour-admirality, not harbour-admiralty). A mismatch writes
    // outside the jail and settings vanish when the process is killed.
    QCoreApplication::setOrganizationName(QStringLiteral("org.admirality"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("org.admirality"));
    QCoreApplication::setApplicationName(QStringLiteral("harbour-admirality"));

    QGuiApplication *app = SailfishApp::application(argc, argv);
    // SailfishApp may overwrite names from the binary; force desktop values again.
    app->setOrganizationName(QStringLiteral("org.admirality"));
    app->setOrganizationDomain(QStringLiteral("org.admirality"));
    app->setApplicationName(QStringLiteral("harbour-admirality"));

    const QString conf = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(conf);
    qDebug() << "Admirality: org=" << app->organizationName()
             << "app=" << app->applicationName()
             << "home=" << QDir::homePath()
             << "appConfig=" << conf
             << "appData=" << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    WilmaBridge wilmaClient;
    QQuickView *view = SailfishApp::createView();
    view->rootContext()->setContextProperty(QStringLiteral("appVersion"),
                                            QStringLiteral(APP_VERSION));
    view->rootContext()->setContextProperty(QStringLiteral("wilmaClient"),
                                            &wilmaClient);
    view->setSource(SailfishApp::pathTo(QStringLiteral("qml/harbour-admirality.qml")));
    view->show();

    return app->exec();
}
