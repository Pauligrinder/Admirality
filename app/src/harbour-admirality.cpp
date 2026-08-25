#include <sailfishapp.h>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickView>

#include "wilmaclient.h"

int main(int argc, char *argv[])
{
    QGuiApplication *app = SailfishApp::application(argc, argv);
    app->setOrganizationName(QStringLiteral("org.admirality"));
    app->setApplicationName(QStringLiteral("harbour-admirality"));

    WilmaClient wilmaClient;
    QQuickView *view = SailfishApp::createView();
    view->rootContext()->setContextProperty(QStringLiteral("appVersion"),
                                            QStringLiteral(APP_VERSION));
    view->rootContext()->setContextProperty(QStringLiteral("wilmaClient"),
                                            &wilmaClient);
    view->setSource(SailfishApp::pathTo(QStringLiteral("qml/harbour-admirality.qml")));
    view->show();

    return app->exec();
}
