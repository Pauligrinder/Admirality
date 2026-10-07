#include <QCoreApplication>
#include <QTimer>

#include "wilmaclient.h"
#include "wilmaservice.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName(QStringLiteral("org.admirality"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("org.admirality"));
    QCoreApplication::setApplicationName(QStringLiteral("harbour-admirality"));

    QCoreApplication app(argc, argv);
    WilmaClient client;
    WilmaService service(&client);
    Q_UNUSED(service)
    QTimer::singleShot(0, &client, SLOT(restoreSession()));
    return app.exec();
}
