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
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QUrl>
#include <QDate>
#include <QDateTime>
#include <QTime>
#include <QRegularExpression>
#include <QHash>
#include <QMap>
#include <QDebug>
#include <algorithm>

namespace {

const char *kUserAgent =
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/123.0 Safari/537.36";

// Sailjail only bind-mounts:
//   ~/.config/org.admirality/harbour-admirality
//   ~/.local/share/org.admirality/harbour-admirality
// Those names MUST match [X-Sailjail] in the desktop file. Do not derive
// them from QCoreApplication::applicationName() — "admiralty" vs
// "admirality" writes outside the jail and is discarded on process kill.
// ConfigLocation/GenericDataLocation are used because they do not append
// org/app themselves (AppConfigLocation does, and follows the Qt names).
QString joinPath(const QString &dir, const QString &file)
{
    if (dir.endsWith(QLatin1Char('/')))
        return dir + file;
    return dir + QLatin1Char('/') + file;
}

QString sailjailSubdir(QStandardPaths::StandardLocation location)
{
    const QString base = QStandardPaths::writableLocation(location);
    const QString dir = joinPath(base, QStringLiteral("org.admirality/harbour-admirality"));
    QDir().mkpath(dir);
    return dir;
}

QString settingsFilePath()
{
    return joinPath(sailjailSubdir(QStandardPaths::ConfigLocation),
                    QStringLiteral("harbour-admirality.conf"));
}

QString appDataMirrorPath()
{
    return joinPath(sailjailSubdir(QStandardPaths::GenericDataLocation),
                    QStringLiteral("settings.ini"));
}

QStringList legacySettingsPaths()
{
    const QString cfg = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    const QString data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QStringList paths;
    paths << joinPath(QDir::homePath(), QStringLiteral(".config/harbour-admirality.conf"));
    paths << joinPath(QDir::homePath(), QStringLiteral(".config/harbour-admiralty.conf"));
    paths << joinPath(cfg, QStringLiteral("org.admirality/harbour-admiralty/harbour-admiralty.conf"));
    paths << joinPath(cfg, QStringLiteral("harbour-admirality/harbour-admirality.conf"));
    paths << joinPath(data, QStringLiteral("org.admirality/harbour-admiralty/settings.ini"));
    paths << joinPath(data, QStringLiteral("harbour-admirality/settings.ini"));
    return paths;
}

void copySettingsKeys(QSettings *from, QSettings *to)
{
    if (!from || !to)
        return;
    const QStringList keys = from->allKeys();
    for (const QString &key : keys)
        to->setValue(key, from->value(key));
}

void writeSchoolSidecar(const QString &url, const QString &name)
{
    const QString path = joinPath(sailjailSubdir(QStandardPaths::ConfigLocation),
                                  QStringLiteral("school.url"));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Admirality: cannot write" << path << f.errorString();
        return;
    }
    f.write(url.toUtf8());
    f.write("\n");
    f.write(name.toUtf8());
    f.write("\n");
    f.flush();
}

bool readSchoolSidecar(QString *url, QString *name)
{
    QFile f(joinPath(sailjailSubdir(QStandardPaths::ConfigLocation),
                     QStringLiteral("school.url")));
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QList<QByteArray> lines = f.readAll().split('\n');
    if (lines.isEmpty() || lines.at(0).trimmed().isEmpty())
        return false;
    *url = QString::fromUtf8(lines.at(0).trimmed());
    if (name && lines.size() > 1)
        *name = QString::fromUtf8(lines.at(1).trimmed());
    return true;
}

bool schoolUrlEmpty(const QSettings &settings)
{
    return !settings.contains(QStringLiteral("schoolUrl"))
            || settings.value(QStringLiteral("schoolUrl")).toString().isEmpty();
}

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

QMap<QString, QString> extractHiddenFields(const QString &html)
{
    QMap<QString, QString> fields;
    QRegularExpression inputRe(QStringLiteral("<input[^>]+>"),
                               QRegularExpression::CaseInsensitiveOption);
    QRegularExpression nameRe(QStringLiteral("name=['\"]([^'\"]+)['\"]"),
                              QRegularExpression::CaseInsensitiveOption);
    QRegularExpression valueRe(QStringLiteral("value=['\"]([^'\"]*)['\"]"),
                               QRegularExpression::CaseInsensitiveOption);
    QRegularExpression typeRe(QStringLiteral("type=['\"]([^'\"]+)['\"]"),
                              QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatchIterator it = inputRe.globalMatch(html);
    while (it.hasNext()) {
        const QString tag = it.next().captured(0);
        const QRegularExpressionMatch nameMatch = nameRe.match(tag);
        if (!nameMatch.hasMatch())
            continue;
        const QString name = nameMatch.captured(1);
        if (name.compare(QStringLiteral("Login"), Qt::CaseInsensitive) == 0
                || name.compare(QStringLiteral("Password"), Qt::CaseInsensitive) == 0)
            continue;
        QString type = QStringLiteral("text");
        const QRegularExpressionMatch typeMatch = typeRe.match(tag);
        if (typeMatch.hasMatch())
            type = typeMatch.captured(1).toLower();
        if (type != QLatin1String("hidden") && type != QLatin1String("submit"))
            continue;
        const QRegularExpressionMatch valueMatch = valueRe.match(tag);
        fields.insert(name, valueMatch.hasMatch() ? valueMatch.captured(1) : QString());
    }
    return fields;
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

QString fromCodePoint(uint cp)
{
    if (cp == 0 || cp > 0x10FFFF)
        return QString();
    if (cp == 0xA0)
        return QStringLiteral(" ");
    if (cp <= 0xFFFF)
        return QString(QChar(static_cast<ushort>(cp)));
    const uint ucs4[] = { cp };
    return QString::fromUcs4(ucs4, 1);
}

QString namedHtmlEntity(const QString &name)
{
    static QHash<QString, QString> entities;
    if (entities.isEmpty()) {
        const struct { const char *name; const char *utf8; } table[] = {
            { "nbsp", " " }, { "amp", "&" }, { "lt", "<" }, { "gt", ">" },
            { "quot", "\"" }, { "apos", "'" }, { "mdash", "—" }, { "ndash", "–" },
            { "hellip", "…" }, { "lsquo", "‘" }, { "rsquo", "’" },
            { "ldquo", "“" }, { "rdquo", "”" }, { "laquo", "«" }, { "raquo", "»" },
            { "bull", "•" }, { "middot", "·" }, { "deg", "°" }, { "euro", "€" },
            { "pound", "£" }, { "copy", "©" }, { "reg", "®" }, { "trade", "™" },
            { "times", "×" }, { "divide", "÷" }, { "plusmn", "±" }, { "micro", "µ" },
            { "para", "¶" }, { "sect", "§" }, { "shy", "" },
            { "Agrave", "À" }, { "Aacute", "Á" }, { "Acirc", "Â" }, { "Atilde", "Ã" },
            { "Auml", "Ä" }, { "Aring", "Å" }, { "AElig", "Æ" }, { "Ccedil", "Ç" },
            { "Egrave", "È" }, { "Eacute", "É" }, { "Ecirc", "Ê" }, { "Euml", "Ë" },
            { "Igrave", "Ì" }, { "Iacute", "Í" }, { "Icirc", "Î" }, { "Iuml", "Ï" },
            { "ETH", "Ð" }, { "Ntilde", "Ñ" }, { "Ograve", "Ò" }, { "Oacute", "Ó" },
            { "Ocirc", "Ô" }, { "Otilde", "Õ" }, { "Ouml", "Ö" }, { "Oslash", "Ø" },
            { "Ugrave", "Ù" }, { "Uacute", "Ú" }, { "Ucirc", "Û" }, { "Uuml", "Ü" },
            { "Yacute", "Ý" }, { "THORN", "Þ" }, { "szlig", "ß" },
            { "agrave", "à" }, { "aacute", "á" }, { "acirc", "â" }, { "atilde", "ã" },
            { "auml", "ä" }, { "aring", "å" }, { "aelig", "æ" }, { "ccedil", "ç" },
            { "egrave", "è" }, { "eacute", "é" }, { "ecirc", "ê" }, { "euml", "ë" },
            { "igrave", "ì" }, { "iacute", "í" }, { "icirc", "î" }, { "iuml", "ï" },
            { "eth", "ð" }, { "ntilde", "ñ" }, { "ograve", "ò" }, { "oacute", "ó" },
            { "ocirc", "ô" }, { "otilde", "õ" }, { "ouml", "ö" }, { "oslash", "ø" },
            { "ugrave", "ù" }, { "uacute", "ú" }, { "ucirc", "û" }, { "uuml", "ü" },
            { "yacute", "ý" }, { "thorn", "þ" }, { "yuml", "ÿ" }
        };
        const int n = static_cast<int>(sizeof(table) / sizeof(table[0]));
        for (int i = 0; i < n; ++i)
            entities.insert(QString::fromLatin1(table[i].name),
                            QString::fromUtf8(table[i].utf8));
    }
    return entities.value(name);
}

QString decodeHtmlEntities(const QString &text)
{
    if (!text.contains(QLatin1Char('&')))
        return text;

    QRegularExpression re(QStringLiteral("&(#x[0-9A-Fa-f]+|#\\d+|[A-Za-z][A-Za-z0-9]+);"));
    QString out;
    int last = 0;
    QRegularExpressionMatchIterator it = re.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        out += text.mid(last, match.capturedStart() - last);
        const QString body = match.captured(1);
        QString decoded;
        if (body.startsWith(QLatin1String("#x")) || body.startsWith(QLatin1String("#X"))) {
            bool ok = false;
            const uint cp = body.mid(2).toUInt(&ok, 16);
            if (ok)
                decoded = fromCodePoint(cp);
        } else if (body.startsWith(QLatin1Char('#'))) {
            bool ok = false;
            const uint cp = body.mid(1).toUInt(&ok, 10);
            if (ok)
                decoded = fromCodePoint(cp);
        } else {
            decoded = namedHtmlEntity(body);
        }
        out += decoded.isEmpty() ? match.captured(0) : decoded;
        last = match.capturedEnd();
    }
    out += text.mid(last);
    return out;
}

QString jsonString(const QJsonObject &obj, const QStringList &keys)
{
    for (const QString &key : keys) {
        const QJsonValue value = obj.value(key);
        if (value.isString() && !value.toString().isEmpty())
            return decodeHtmlEntities(value.toString());
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

QString htmlToText(QString html)
{
    html.replace(QRegularExpression(QStringLiteral("<br\\s*/?>"),
                                    QRegularExpression::CaseInsensitiveOption),
                 QStringLiteral("\n"));
    html.replace(QRegularExpression(QStringLiteral("</p>"),
                                    QRegularExpression::CaseInsensitiveOption),
                 QStringLiteral("\n"));
    html.replace(QRegularExpression(QStringLiteral("</div>"),
                                    QRegularExpression::CaseInsensitiveOption),
                 QStringLiteral("\n"));
    html.replace(QRegularExpression(QStringLiteral("</h[1-6]>"),
                                    QRegularExpression::CaseInsensitiveOption),
                 QStringLiteral("\n"));
    html.replace(QRegularExpression(QStringLiteral("<script[\\s\\S]*?</script>"),
                                    QRegularExpression::CaseInsensitiveOption),
                 QString());
    html.replace(QRegularExpression(QStringLiteral("<style[\\s\\S]*?</style>"),
                                    QRegularExpression::CaseInsensitiveOption),
                 QString());
    html.replace(QRegularExpression(QStringLiteral("<[^>]+>")), QString());
    html = decodeHtmlEntities(html);
    html.replace(QRegularExpression(QStringLiteral("[ \\t]+")), QStringLiteral(" "));
    html.replace(QRegularExpression(QStringLiteral("\\n{3,}")), QStringLiteral("\n\n"));
    return html.trimmed();
}

QDate parseWilmaDate(const QString &raw)
{
    const QString text = raw.trimmed();
    if (text.isEmpty())
        return QDate();
    QDate iso = QDate::fromString(text, Qt::ISODate);
    if (iso.isValid())
        return iso;
    iso = QDate::fromString(text, QStringLiteral("yyyy-MM-dd"));
    if (iso.isValid())
        return iso;
    QDate fi = QDate::fromString(text, QStringLiteral("d.M.yyyy"));
    if (fi.isValid())
        return fi;
    fi = QDate::fromString(text, QStringLiteral("dd.MM.yyyy"));
    if (fi.isValid())
        return fi;
    return QDate();
}

QDateTime parseWilmaDateTime(const QJsonValue &value)
{
    if (value.isDouble()) {
        const qint64 n = value.toVariant().toLongLong();
        if (n > 100000000000LL)
            return QDateTime::fromMSecsSinceEpoch(n);
        if (n > 0)
            return QDateTime::fromTime_t(static_cast<uint>(n));
        return QDateTime();
    }
    const QString text = value.toString().trimmed();
    if (text.isEmpty())
        return QDateTime();

    QRegularExpression msRe(QStringLiteral("/Date\\((\\d+)\\)/"));
    const QRegularExpressionMatch msMatch = msRe.match(text);
    if (msMatch.hasMatch())
        return QDateTime::fromMSecsSinceEpoch(msMatch.captured(1).toLongLong());

    QDateTime iso = QDateTime::fromString(text, Qt::ISODate);
    if (iso.isValid())
        return iso;
    iso = QDateTime::fromString(text, QStringLiteral("yyyy-MM-ddTHH:mm:ss"));
    if (iso.isValid())
        return iso;

    QDateTime fi = QDateTime::fromString(text, QStringLiteral("d.M.yyyy HH:mm"));
    if (fi.isValid())
        return fi;
    fi = QDateTime::fromString(text, QStringLiteral("dd.MM.yyyy HH:mm"));
    if (fi.isValid())
        return fi;
    fi = QDateTime::fromString(text, QStringLiteral("d.M.yyyy H:mm"));
    if (fi.isValid())
        return fi;

    const QDate date = parseWilmaDate(text);
    if (date.isValid())
        return QDateTime(date);
    return QDateTime();
}

QJsonValue firstJsonValue(const QJsonObject &obj, const QStringList &keys)
{
    for (const QString &key : keys) {
        if (obj.contains(key))
            return obj.value(key);
    }
    return QJsonValue();
}

QString formatDisplayDate(const QDate &date)
{
    if (!date.isValid())
        return QString();
    return date.toString(Qt::DefaultLocaleShortDate);
}

QString formatDisplayDateTime(const QDateTime &dt)
{
    if (!dt.isValid())
        return QString();
    if (dt.time() == QTime(0, 0, 0) && dt.timeSpec() != Qt::OffsetFromUTC)
        return dt.date().toString(Qt::DefaultLocaleShortDate);
    return dt.toString(Qt::DefaultLocaleShortDate);
}

QString firstTeacherName(const QJsonArray &teachers)
{
    if (teachers.isEmpty())
        return QString();
    const QJsonObject t = teachers.at(0).toObject();
    const QString name = jsonString(t, QStringList()
                                    << QStringLiteral("LongCaption")
                                    << QStringLiteral("TeacherName")
                                    << QStringLiteral("Caption")
                                    << QStringLiteral("Name"));
    return name;
}

QString todayIso()
{
    return QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
}

bool looksLikeHtml(const QByteArray &body)
{
    const QByteArray trimmed = body.trimmed().left(32).toLower();
    return trimmed.startsWith("<!doctype") || trimmed.startsWith("<html")
            || trimmed.startsWith("<!doctype html");
}

bool looksLikeLoginForm(const QString &text)
{
    return text.contains(QStringLiteral("loginFailed"), Qt::CaseInsensitive)
            || (text.contains(QStringLiteral("Login"), Qt::CaseInsensitive)
                && text.contains(QStringLiteral("Password"), Qt::CaseInsensitive)
                && (text.contains(QStringLiteral("name=\"Login\""), Qt::CaseInsensitive)
                    || text.contains(QStringLiteral("name='Login'"), Qt::CaseInsensitive)
                    || text.contains(QStringLiteral("id=\"Login\""), Qt::CaseInsensitive)));
}

QString pathAndQuery(const QUrl &url)
{
    QString path = url.path();
    if (path.isEmpty())
        path = QStringLiteral("/");
    if (!url.query().isEmpty())
        path += QLatin1Char('?') + url.query();
    return path;
}

bool isRedirectStatus(int status)
{
    return status == 301 || status == 302 || status == 303
            || status == 307 || status == 308;
}

QString roleIdFromText(const QString &text)
{
    QRegularExpression re(QStringLiteral("/!(\\d+)(?:/|$)"));
    const QRegularExpressionMatch match = re.match(text);
    if (match.hasMatch())
        return match.captured(1);
    const QString trimmed = text.trimmed();
    if (QRegularExpression(QStringLiteral("^\\d+$")).match(trimmed).hasMatch())
        return trimmed;
    if (trimmed.startsWith(QLatin1Char('!')))
        return trimmed.mid(1);
    return QString();
}

bool isPasswdRole(const QJsonValue &type)
{
    if (type.isDouble())
        return type.toInt() == 7;
    const QString text = type.toString().trimmed().toLower();
    return text == QLatin1String("passwd") || text == QLatin1String("7");
}

QString roleNameFromObject(const QJsonObject &obj, const QString &fallback)
{
    const QString name = jsonString(obj, QStringList()
                                    << QStringLiteral("Name")
                                    << QStringLiteral("name")
                                    << QStringLiteral("Caption")
                                    << QStringLiteral("caption"));
    return name.isEmpty() ? fallback : name;
}

QString messageSender(const QJsonObject &obj)
{
    QString sender = jsonString(obj, QStringList()
                                << QStringLiteral("Sender")
                                << QStringLiteral("sender")
                                << QStringLiteral("SenderName")
                                << QStringLiteral("FromName"));
    if (!sender.isEmpty())
        return sender;
    if (obj.value(QStringLiteral("Sender")).isObject()) {
        sender = jsonString(obj.value(QStringLiteral("Sender")).toObject(),
                            QStringList() << QStringLiteral("Name")
                                          << QStringLiteral("name")
                                          << QStringLiteral("Caption"));
        if (!sender.isEmpty())
            return sender;
    }
    QJsonArray recipients = obj.value(QStringLiteral("Recipients")).toArray();
    if (recipients.isEmpty())
        recipients = obj.value(QStringLiteral("recipients")).toArray();
    if (recipients.isEmpty())
        recipients = obj.value(QStringLiteral("Senders")).toArray();
    if (!recipients.isEmpty()) {
        const QJsonObject first = recipients.at(0).toObject();
        sender = jsonString(first, QStringList()
                            << QStringLiteral("Name")
                            << QStringLiteral("name")
                            << QStringLiteral("Caption"));
    }
    return sender;
}

QJsonArray messagesArrayFromObject(const QJsonObject &obj)
{
    if (obj.value(QStringLiteral("Messages")).isArray())
        return obj.value(QStringLiteral("Messages")).toArray();
    if (obj.value(QStringLiteral("messages")).isArray())
        return obj.value(QStringLiteral("messages")).toArray();
    if (obj.value(QStringLiteral("payload")).isArray())
        return obj.value(QStringLiteral("payload")).toArray();
    if (obj.value(QStringLiteral("payload")).isObject())
        return messagesArrayFromObject(obj.value(QStringLiteral("payload")).toObject());
    if (obj.value(QStringLiteral("Payload")).isObject())
        return messagesArrayFromObject(obj.value(QStringLiteral("Payload")).toObject());
    return QJsonArray();
}

QString htmlAttr(const QString &attrs, const QString &name)
{
    QRegularExpression re(QStringLiteral("\\b%1\\s*=\\s*['\"]([^'\"]*)['\"]")
                          .arg(QRegularExpression::escape(name)),
                          QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(attrs);
    return match.hasMatch() ? match.captured(1) : QString();
}

int htmlColspan(const QString &attrs)
{
    const int span = htmlAttr(attrs, QStringLiteral("colspan")).toInt();
    return span > 0 ? span : 1;
}

QString cellPlainText(QString inner)
{
    inner.replace(QRegularExpression(QStringLiteral("<[^>]+>")), QString());
    return decodeHtmlEntities(inner).trimmed();
}

QString typeClassFrom(const QString &classes)
{
    QRegularExpression re(QStringLiteral("\\bat-tp\\d+\\b"),
                          QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(classes);
    return match.hasMatch() ? match.captured(0).toLower() : QString();
}

bool noteNeedsAction(const QString &typeLabel)
{
    const QString text = typeLabel.toLower();
    return text.contains(QStringLiteral("selvit"))
            || text.contains(QStringLiteral("unexplained"))
            || text.contains(QStringLiteral("selvittämät"));
}

QString noteTone(const QString &typeLabel, const QString &typeCode)
{
    const QString text = (typeLabel + QLatin1Char(' ') + typeCode).toLower();
    const bool good = text.contains(QStringLiteral("kiitos"))
            || text.contains(QStringLiteral("thank"))
            || text.contains(QStringLiteral("positiv"))
            || text.contains(QStringLiteral("myönte"))
            || text.contains(QStringLiteral("myonte"))
            || text.contains(QStringLiteral("ansiok"))
            || text.contains(QStringLiteral("kehu"))
            || text.contains(QStringLiteral("praise"))
            || text.contains(QStringLiteral("reward"))
            || text.contains(QStringLiteral("aktiiv"));
    if (good)
        return QStringLiteral("good");

    const bool unauthorized = text.contains(QStringLiteral("luvaton"))
            || text.contains(QStringLiteral("selvittäm"))
            || text.contains(QStringLiteral("unexplained"));
    const bool excused = !unauthorized
            && (text.contains(QStringLiteral("selvitetty"))
                || text.contains(QStringLiteral("luvall"))
                || text.contains(QStringLiteral("permission"))
                || text.contains(QStringLiteral("explained"))
                || text.contains(QStringLiteral("sairaus")));
    const bool bad = unauthorized
            || text.contains(QStringLiteral("poissa"))
            || text.contains(QStringLiteral("absence"))
            || text.contains(QStringLiteral("absent"))
            || text.contains(QStringLiteral("myöh"))
            || text.contains(QStringLiteral("myoh"))
            || text.contains(QStringLiteral("late"))
            || text.contains(QStringLiteral("häir"))
            || text.contains(QStringLiteral("hairi"))
            || text.contains(QStringLiteral("disrupt"))
            || text.contains(QStringLiteral("huomaut"))
            || text.contains(QStringLiteral("varoitus"))
            || text.contains(QStringLiteral("negatiiv"))
            || text.contains(QStringLiteral("huono"));
    if (bad && !excused)
        return QStringLiteral("bad");
    return QStringLiteral("neutral");
}

QString noteDotColor(const QString &tone)
{
    if (tone == QLatin1String("bad"))
        return QStringLiteral("#e53935");
    if (tone == QLatin1String("good"))
        return QStringLiteral("#43a047");
    return QStringLiteral("#9e9e9e");
}

void insertNoteTone(QVariantMap *item, const QString &typeLabel, const QString &typeCode)
{
    const QString tone = noteTone(typeLabel, typeCode);
    const QString dot = noteDotColor(tone);
    item->insert(QStringLiteral("tone"), tone);
    item->insert(QStringLiteral("dotColor"), dot);
    item->insert(QStringLiteral("color"), dot);
}

QString normalizePollMode(const QString &mode)
{
    if (mode == QLatin1String("hour")
            || mode == QLatin1String("3hours")
            || mode == QLatin1String("schoolday")
            || mode == QLatin1String("15min"))
        return mode;
    return QStringLiteral("15min");
}

int intervalForPollMode(const QString &mode)
{
    if (mode == QLatin1String("hour"))
        return 60 * 60 * 1000;
    if (mode == QLatin1String("3hours"))
        return 3 * 60 * 60 * 1000;
    return 15 * 60 * 1000;
}

QTime parseClock(const QString &text)
{
    const QString trimmed = text.trimmed();
    QTime time = QTime::fromString(trimmed, QStringLiteral("H:mm"));
    if (!time.isValid())
        time = QTime::fromString(trimmed, QStringLiteral("HH:mm"));
    if (!time.isValid())
        time = QTime::fromString(trimmed, QStringLiteral("H.mm"));
    return time;
}

QVariantMap lessonNoteFromJson(const QJsonObject &obj)
{
    QVariantMap item;
    const QDate date = parseWilmaDate(jsonString(obj, QStringList()
                                                 << QStringLiteral("Date")
                                                 << QStringLiteral("date")
                                                 << QStringLiteral("Day")));
    const QString iso = date.isValid()
            ? date.toString(QStringLiteral("yyyy-MM-dd"))
            : jsonString(obj, QStringList() << QStringLiteral("Date") << QStringLiteral("date"));
    item.insert(QStringLiteral("date"), iso);
    item.insert(QStringLiteral("dateLabel"), formatDisplayDate(date));
    item.insert(QStringLiteral("start"),
                jsonString(obj, QStringList() << QStringLiteral("Start") << QStringLiteral("start")));
    item.insert(QStringLiteral("end"),
                jsonString(obj, QStringList() << QStringLiteral("End") << QStringLiteral("end")));
    item.insert(QStringLiteral("subject"),
                jsonString(obj, QStringList()
                           << QStringLiteral("CourseName")
                           << QStringLiteral("Course")
                           << QStringLiteral("CourseCode")
                           << QStringLiteral("ShortCaption")
                           << QStringLiteral("subject")));
    const QString typeLabel = jsonString(obj, QStringList()
                                         << QStringLiteral("TypeName")
                                         << QStringLiteral("Type")
                                         << QStringLiteral("Name")
                                         << QStringLiteral("Caption")
                                         << QStringLiteral("typeLabel"));
    item.insert(QStringLiteral("typeLabel"), typeLabel);
    item.insert(QStringLiteral("typeCode"),
                jsonString(obj, QStringList()
                           << QStringLiteral("TypeCode")
                           << QStringLiteral("Code")
                           << QStringLiteral("TypeShort")));
    item.insert(QStringLiteral("teacher"),
                jsonString(obj, QStringList()
                           << QStringLiteral("Teacher")
                           << QStringLiteral("TeacherName")
                           << QStringLiteral("teacher")));
    item.insert(QStringLiteral("comment"),
                jsonString(obj, QStringList()
                           << QStringLiteral("ClarificationText")
                           << QStringLiteral("Clarification")
                           << QStringLiteral("Comment")
                           << QStringLiteral("Text")
                           << QStringLiteral("Info")));
    item.insert(QStringLiteral("needsAction"), noteNeedsAction(typeLabel));
    insertNoteTone(&item,
                   typeLabel,
                   item.value(QStringLiteral("typeCode")).toString());
    return item;
}

QJsonArray lessonNotesArrayFromObject(const QJsonObject &obj)
{
    if (obj.value(QStringLiteral("Observations")).isArray())
        return obj.value(QStringLiteral("Observations")).toArray();
    if (obj.value(QStringLiteral("observations")).isArray())
        return obj.value(QStringLiteral("observations")).toArray();
    if (obj.value(QStringLiteral("Notes")).isArray())
        return obj.value(QStringLiteral("Notes")).toArray();
    if (obj.value(QStringLiteral("notes")).isArray())
        return obj.value(QStringLiteral("notes")).toArray();
    if (obj.value(QStringLiteral("LessonNotes")).isArray())
        return obj.value(QStringLiteral("LessonNotes")).toArray();
    if (obj.value(QStringLiteral("payload")).isObject())
        return lessonNotesArrayFromObject(obj.value(QStringLiteral("payload")).toObject());
    if (obj.value(QStringLiteral("payload")).isArray())
        return obj.value(QStringLiteral("payload")).toArray();
    return QJsonArray();
}

QVariantList lessonNotesFromJsonDocument(const QJsonDocument &doc)
{
    QJsonArray list;
    if (doc.isArray())
        list = doc.array();
    else if (doc.isObject())
        list = lessonNotesArrayFromObject(doc.object());
    QVariantList items;
    for (const QJsonValue &value : list) {
        if (!value.isObject())
            continue;
        items.append(lessonNoteFromJson(value.toObject()));
    }
    return items;
}

QHash<QString, QString> parseAttendanceLegend(const QString &html)
{
    QHash<QString, QString> legend;
    QRegularExpression re(QStringLiteral(
                "<td[^>]*class=['\"]([^'\"]*at-tp\\d+[^'\"]*)['\"][^>]*>\\s*([^<]*)\\s*</td>\\s*"
                "<td[^>]*>\\s*([^<]+)"),
                          QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator it = re.globalMatch(html);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString cls = typeClassFrom(match.captured(1));
        const QString name = decodeHtmlEntities(match.captured(3)).trimmed();
        if (!cls.isEmpty() && !name.isEmpty())
            legend.insert(cls, name);
    }
    return legend;
}

QHash<QString, QString> parseAttendanceColors(const QString &html)
{
    QHash<QString, QString> colors;
    QRegularExpression re(QStringLiteral("(?:TD\\.)?(at-tp\\d+)[^{]*\\{[^}]*background-color:\\s*(#[0-9A-Fa-f]{3,8})"),
                          QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator it = re.globalMatch(html);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        colors.insert(match.captured(1).toLower(), match.captured(2));
    }
    return colors;
}

QList<int> parseHourMap(const QString &tableHtml)
{
    QList<int> hours;
    QRegularExpression theadRe(QStringLiteral("<thead[^>]*>([\\s\\S]*?)</thead>"),
                               QRegularExpression::CaseInsensitiveOption);
    QString head = theadRe.match(tableHtml).captured(1);
    if (head.isEmpty())
        head = tableHtml.left(4000);
    QRegularExpression thRe(QStringLiteral("<th([^>]*)>([\\s\\S]*?)</th>"),
                            QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator it = thRe.globalMatch(head);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        bool ok = false;
        const int hour = cellPlainText(match.captured(2)).toInt(&ok);
        if (!ok || hour < 0 || hour > 23)
            continue;
        const int span = htmlColspan(match.captured(1));
        for (int i = 0; i < span; ++i)
            hours.append(hour);
    }
    return hours;
}

QString padHour(int hour)
{
    return QStringLiteral("%1:00").arg(hour, 2, 10, QLatin1Char('0'));
}

QString padHourEnd(int hour)
{
    return QStringLiteral("%1:45").arg(hour, 2, 10, QLatin1Char('0'));
}

QVariantList parseAttendanceHtml(const QString &html)
{
    const QHash<QString, QString> legend = parseAttendanceLegend(html);
    const QHash<QString, QString> colors = parseAttendanceColors(html);
    Q_UNUSED(colors);
    QVariantList items;
    QRegularExpression tableRe(QStringLiteral("<table[^>]*>([\\s\\S]*?)</table>"),
                               QRegularExpression::CaseInsensitiveOption);
    QRegularExpression rowRe(QStringLiteral("<tr[^>]*>([\\s\\S]*?)</tr>"),
                             QRegularExpression::CaseInsensitiveOption);
    QRegularExpression tdRe(QStringLiteral("<td([^>]*)>([\\s\\S]*?)</td>"),
                            QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatchIterator tables = tableRe.globalMatch(html);
    while (tables.hasNext()) {
        const QString table = tables.next().captured(1);
        const QList<int> hourMap = parseHourMap(table);
        if (hourMap.isEmpty())
            continue;

        QRegularExpressionMatchIterator rows = rowRe.globalMatch(table);
        while (rows.hasNext()) {
            const QString row = rows.next().captured(1);
            QStringList attrList;
            QStringList textList;
            QRegularExpressionMatchIterator tds = tdRe.globalMatch(row);
            while (tds.hasNext()) {
                const QRegularExpressionMatch td = tds.next();
                attrList.append(td.captured(1));
                textList.append(cellPlainText(td.captured(2)));
            }
            if (attrList.size() < 3)
                continue;

            const QDate date = parseWilmaDate(textList.value(1));
            if (!date.isValid())
                continue;
            const QString iso = date.toString(QStringLiteral("yyyy-MM-dd"));
            int gridCol = 0;
            for (int i = 2; i < attrList.size(); ++i) {
                const QString attrs = attrList.at(i);
                const int span = htmlColspan(attrs);
                const QString cls = typeClassFrom(htmlAttr(attrs, QStringLiteral("class")));
                if (!cls.isEmpty()) {
                    QString title = decodeHtmlEntities(htmlAttr(attrs, QStringLiteral("title"))).trimmed();
                    QString subject;
                    QString typeLabel;
                    QString teacher = textList.value(i);
                    if (!title.isEmpty()) {
                        QString rest = title;
                        const int semi = rest.indexOf(QLatin1Char(';'));
                        if (semi > 0) {
                            subject = rest.left(semi).trimmed();
                            rest = rest.mid(semi + 1).trimmed();
                        }
                        const int slash = rest.lastIndexOf(QStringLiteral(" /"));
                        if (slash >= 0) {
                            typeLabel = rest.left(slash).trimmed();
                            teacher = rest.mid(slash + 2).trimmed();
                        } else if (typeLabel.isEmpty()) {
                            typeLabel = rest;
                        }
                    }
                    if (typeLabel.isEmpty())
                        typeLabel = legend.value(cls);
                    QVariantMap item;
                    item.insert(QStringLiteral("date"), iso);
                    item.insert(QStringLiteral("dateLabel"), formatDisplayDate(date));
                    if (gridCol >= 0 && gridCol < hourMap.size())
                        item.insert(QStringLiteral("start"), padHour(hourMap.at(gridCol)));
                    else
                        item.insert(QStringLiteral("start"), QString());
                    const int endIndex = gridCol + span - 1;
                    if (endIndex >= 0 && endIndex < hourMap.size())
                        item.insert(QStringLiteral("end"), padHourEnd(hourMap.at(endIndex)));
                    else
                        item.insert(QStringLiteral("end"), QString());
                    item.insert(QStringLiteral("subject"), subject);
                    item.insert(QStringLiteral("typeLabel"), typeLabel);
                    item.insert(QStringLiteral("typeCode"), textList.value(i));
                    item.insert(QStringLiteral("teacher"), teacher);
                    item.insert(QStringLiteral("comment"), QString());
                    item.insert(QStringLiteral("needsAction"), noteNeedsAction(typeLabel));
                    insertNoteTone(&item, typeLabel, textList.value(i));
                    items.append(item);
                }
                gridCol += span;
            }
        }
    }
    return items;
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
    , m_loginInProgress(false)
    , m_loginTry(0)
    , m_postLoginHops(0)
    , m_messageFolder(QStringLiteral("inbox"))
    , m_pollMode(QStringLiteral("15min"))
    , m_unreadCount(0)
    , m_pendingRefresh(0)
    , m_roleProbePending(0)
    , m_roleScoresChanged(false)
    , m_refreshing(false)
    , m_detailBusy(false)
    , m_freshReady(false)
{
    m_nam->setCookieJar(m_cookies);
    m_pollTimer.setSingleShot(false);
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
QString WilmaClient::sessionId() const { return m_sessionId; }
QString WilmaClient::displayName() const { return m_displayName; }
QString WilmaClient::roleId() const { return m_roleId; }
QString WilmaClient::roleName() const { return m_roleName; }
QString WilmaClient::messageFolder() const { return m_messageFolder; }
int WilmaClient::unreadCount() const { return m_unreadCount; }
QVariantList WilmaClient::schools() const { return m_schools; }
QVariantList WilmaClient::roles() const { return m_roles; }
QVariantList WilmaClient::messages() const { return m_messages; }
QVariantList WilmaClient::news() const { return m_news; }
QVariantList WilmaClient::schedule() const { return m_schedule; }
QVariantList WilmaClient::todaySchedule() const
{
    QVariantList today;
    const QString iso = todayIso();
    for (const QVariant &entry : m_schedule) {
        const QVariantMap map = entry.toMap();
        if (map.value(QStringLiteral("date")).toString() == iso)
            today.append(entry);
    }
    return today;
}
QVariantList WilmaClient::exams() const { return m_exams; }
QVariantList WilmaClient::homework() const { return m_homework; }
QVariantList WilmaClient::lessonNotes() const { return m_lessonNotes; }
int WilmaClient::lessonNotesActionCount() const
{
    int n = 0;
    for (const QVariant &entry : m_lessonNotes) {
        if (entry.toMap().value(QStringLiteral("needsAction")).toBool())
            n += 1;
    }
    return n;
}
QVariantList WilmaClient::grades() const { return m_grades; }
QVariantMap WilmaClient::currentMessage() const { return m_currentMessage; }
QVariantMap WilmaClient::currentNews() const { return m_currentNews; }
bool WilmaClient::refreshing() const { return m_refreshing; }
bool WilmaClient::detailBusy() const { return m_detailBusy; }
bool WilmaClient::hasSchool() const { return !m_schoolUrl.isEmpty(); }
bool WilmaClient::hasCredentials() const { return !m_username.isEmpty() && !m_password.isEmpty(); }
QString WilmaClient::pollMode() const { return m_pollMode; }
int WilmaClient::freshCount(const QString &category) const
{
    return m_unseenKeys.value(category).size();
}
int WilmaClient::freshNoteCount() const { return freshCount(QStringLiteral("notes")); }
int WilmaClient::freshNewsCount() const { return freshCount(QStringLiteral("news")); }
int WilmaClient::freshGradeCount() const { return freshCount(QStringLiteral("grades")); }
int WilmaClient::freshHomeworkCount() const { return freshCount(QStringLiteral("homework")); }
int WilmaClient::freshExamCount() const { return freshCount(QStringLiteral("exams")); }

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
    const bool schoolChanged = (m_schoolUrl != normalized);
    if (schoolChanged) {
        const QString previousUrl = m_schoolUrl;
        m_schoolUrl = normalized;
        emit schoolUrlChanged();
        if (!previousUrl.isEmpty()) {
            m_pollTimer.stop();
            setLoggedIn(false);
            m_sessionId.clear();
            emit sessionIdChanged();
            m_loginSessionId.clear();
            m_password.clear();
            emit passwordChanged();
            emit credentialsChanged();
            m_displayName.clear();
            emit displayNameChanged();
            m_roleId.clear();
            emit roleIdChanged();
            m_roleName.clear();
            emit roleNameChanged();
            m_roles.clear();
            emit rolesChanged();
            clearHomeData();
            m_cookies->setCookiesFromUrl(QList<QNetworkCookie>(), QUrl(previousUrl));
        }
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
    // Never replace a stored password with an empty one — GetState no longer
    // exposes the password, and the UI used to echo that empty value back via
    // Login(), wiping credentials on disk.
    if (!password.isEmpty())
        m_password = password;
    emit usernameChanged();
    emit passwordChanged();
    emit credentialsChanged();
    if (m_username.isEmpty() || m_password.isEmpty()) {
        failLogin(QStringLiteral("Wilma login failed. Check the username and password."));
        return;
    }
    saveSettings();
    m_loginTry = 0;
    m_loginInProgress = false;
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
    if (m_loggedIn) {
        emit restoreFinished(true);
        return;
    }
    m_restoring = true;
    emit restoringSessionChanged();

    if (m_schoolUrl.isEmpty()) {
        finishRestore(false);
        return;
    }

    // After a process kill the WebView cookie jar is empty and a saved
    // Wilma2SID is often rejected. Prefer a credential re-login so the
    // session is rebuilt instead of bouncing to the login screen.
    if (hasCredentials()) {
        setStatus(QStringLiteral("Signing in…"));
        m_loginTry = 0;
        m_loginInProgress = false;
        startLogin();
        return;
    }

    applySessionCookie();
    if (!m_sessionId.isEmpty()) {
        setStatus(QStringLiteral("Restoring session…"));
        setBusy(true);
        get(QStringLiteral("/api/v1/accounts/me/roles"), RequestRoles);
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
    emit credentialsChanged();
    m_sessionId.clear();
    emit sessionIdChanged();
    m_loginSessionId.clear();
    m_displayName.clear();
    emit displayNameChanged();
    m_roleId.clear();
    emit roleIdChanged();
    m_roleName.clear();
    emit roleNameChanged();
    m_roles.clear();
    emit rolesChanged();
    m_messageFolder = QStringLiteral("inbox");
    emit messageFolderChanged();
    m_unreadCount = 0;
    emit unreadCountChanged();
    m_seenMessageIds.clear();
    clearHomeData();
    m_firstMessagePoll = true;
    m_loginInProgress = false;
    m_loginTry = 0;
    m_postLoginHops = 0;
    setNeedsOtp(false);
    m_mfaFormkey.clear();
    m_loginFields.clear();
    m_cookies->setCookiesFromUrl(QList<QNetworkCookie>(), QUrl(m_schoolUrl));
    saveSettings();
    setStatus(QString());
    clearError();
}

void WilmaClient::pollMessages()
{
    if (m_loggedIn && !m_schoolUrl.isEmpty() && !m_busy && !m_loginInProgress && !m_refreshing)
        refreshHome();
    if (m_pollMode == QLatin1String("schoolday"))
        armPollTimer();
}

void WilmaClient::refreshHome()
{
    if (!m_loggedIn || m_schoolUrl.isEmpty() || m_loginInProgress)
        return;
    if (m_roleId.isEmpty()) {
        fetchRoles();
        return;
    }
    if (!m_lessonNotes.isEmpty()) {
        m_lessonNotes.clear();
        emit lessonNotesChanged();
    }
    m_pendingRefresh = 5;
    setRefreshing(true);
    QNetworkReply *messages = get(messagesListPath(), RequestMessages);
    QNetworkReply *overview = get(QStringLiteral("/overview"), RequestOverview);
    QNetworkReply *news = get(QStringLiteral("/news"), RequestNewsList);
    QNetworkReply *attendance = get(QStringLiteral("/attendance/view"), RequestAttendance);
    const QDate prev = QDate::currentDate().addDays(-7);
    QNetworkReply *attendancePrev = get(QStringLiteral("/attendance/view?date=%1.%2.%3")
                                        .arg(prev.day())
                                        .arg(prev.month())
                                        .arg(prev.year()),
                                        RequestAttendance);
    messages->setProperty("refresh", true);
    overview->setProperty("refresh", true);
    news->setProperty("refresh", true);
    attendance->setProperty("refresh", true);
    attendancePrev->setProperty("refresh", true);
}

void WilmaClient::loadMessages(const QString &folder)
{
    QString name = folder.trimmed().toLower();
    if (name == QLatin1String("sent"))
        name = QStringLiteral("outbox");
    if (name.isEmpty())
        name = QStringLiteral("inbox");
    if (m_messageFolder != name) {
        m_messageFolder = name;
        emit messageFolderChanged();
    }
    if (!m_loggedIn || m_schoolUrl.isEmpty())
        return;
    get(messagesListPath(), RequestMessages);
}

void WilmaClient::selectRole(const QString &roleId)
{
    const QString id = roleIdFromText(roleId);
    if (id.isEmpty())
        return;
    QString name = id;
    for (const QVariant &entry : m_roles) {
        const QVariantMap map = entry.toMap();
        if (map.value(QStringLiteral("id")).toString() == id) {
            name = map.value(QStringLiteral("name")).toString();
            break;
        }
    }
    const bool changed = (m_roleId != id);
    setRole(id, name);
    if (changed && m_loggedIn) {
        clearHomeData();
        refreshHome();
    }
}

void WilmaClient::openMessage(int messageId)
{
    if (messageId <= 0 || m_schoolUrl.isEmpty())
        return;
    markMessageRead(messageId);
    m_currentMessage.clear();
    m_currentMessage.insert(QStringLiteral("id"), messageId);
    emit currentMessageChanged();
    setDetailBusy(true);
    QNetworkReply *reply = get(QStringLiteral("/messages/%1").arg(messageId), RequestMessageItem);
    reply->setProperty("itemId", messageId);
}

void WilmaClient::openNews(int newsId)
{
    if (newsId <= 0 || m_schoolUrl.isEmpty())
        return;
    m_currentNews.clear();
    m_currentNews.insert(QStringLiteral("id"), newsId);
    emit currentNewsChanged();
    setDetailBusy(true);
    QNetworkReply *reply = get(QStringLiteral("/news/%1").arg(newsId), RequestNewsItem);
    reply->setProperty("itemId", newsId);
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
    const QUrl redirectUrl = resolveRedirect(reply);
    ingestReplyCookies(reply);
    rememberRoleFromUrl(url);
    rememberRoleFromUrl(redirectUrl);

    if (reply->error() != QNetworkReply::NoError
            && kind != RequestIndexJson
            && kind != RequestLoginPage
            && kind != RequestAccount
            && kind != RequestRoles
            && kind != RequestHomeHtml
            && kind != RequestLoginJson
            && kind != RequestLoginHtml
            && kind != RequestPostLogin
            && kind != RequestMessages
            && kind != RequestOverview
            && kind != RequestNewsList
            && kind != RequestNewsItem
            && kind != RequestMessageItem
            && kind != RequestAttendance) {
        if (kind == RequestToken) {
            // Prefer index_json if the HTML login endpoints are unavailable.
            tryIndexJsonLogin();
            return;
        }
        endRefreshIfMarked(reply);
        setBusy(false);
        const QString message = reply->errorString();
        setError(message);
        if (kind == RequestMfa) {
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
        if (isRedirectStatus(status) && redirectUrl.isValid()) {
            get(pathAndQuery(redirectUrl), RequestIndexJson);
            return;
        }
        if (status >= 200 && status < 300) {
            const QJsonObject obj = QJsonDocument::fromJson(body).object();
            QString sessionId = jsonString(obj, QStringList()
                                           << QStringLiteral("SessionID")
                                           << QStringLiteral("SESSIONID")
                                           << QStringLiteral("Wilma2LoginID"));
            if (sessionId.isEmpty())
                sessionId = cookieValueFromReply(reply, QByteArray("Wilma2LoginID"));
            if (sessionId.isEmpty())
                sessionId = loginCookie();
            if (!sessionId.isEmpty()) {
                postIndexJson(sessionId);
                return;
            }
            qWarning() << "Admirality index_json: no SessionID status=" << status
                       << "bodyLen=" << body.size()
                       << "error=" << reply->errorString();
        } else {
            qWarning() << "Admiralty index_json: bad status=" << status
                       << "error=" << reply->errorString()
                       << "bodyLen=" << body.size();
        }
        retryLogin(QStringLiteral("Could not start a Wilma login session"), false);
        break;
    case RequestLoginJson:
        if (status == 403
                || (reply->error() != QNetworkReply::NoError
                    && status != 302 && status != 303)
                || (status > 0 && (status < 200 || status >= 400)
                    && status != 303 && status != 302)) {
            QString detail = QString::fromUtf8(body);
            const QJsonObject err = QJsonDocument::fromJson(body).object()
                    .value(QStringLiteral("error")).toObject();
            if (!err.isEmpty()) {
                detail = jsonString(err, QStringList()
                                    << QStringLiteral("message")
                                    << QStringLiteral("description"));
            }
            if (detail.isEmpty())
                detail = reply->errorString();
            if (detail.isEmpty())
                detail = QStringLiteral("Wilma login failed. Check the username and password.");
            const bool wrongPassword = QString::fromUtf8(body)
                    .contains(QStringLiteral("loginFailed"), Qt::CaseInsensitive);
            retryLogin(detail, wrongPassword);
            break;
        }
        handleLoginBody(reply, body, url, redirectUrl);
        break;
    case RequestLoginPage: {
        if (isRedirectStatus(status) && redirectUrl.isValid()) {
            if (m_postLoginHops >= 8) {
                tryIndexJsonLogin();
                break;
            }
            m_postLoginHops += 1;
            get(pathAndQuery(redirectUrl), RequestLoginPage);
            break;
        }
        const QString html = QString::fromUtf8(body);
        m_loginFields = extractHiddenFields(html);
        m_htmlSessionId = m_loginFields.value(QStringLiteral("SESSIONID"));
        if (m_htmlSessionId.isEmpty())
            m_htmlSessionId = extractNamedInput(html, QStringLiteral("SESSIONID"));
        if (m_htmlSessionId.isEmpty())
            m_htmlSessionId = loginCookie();
        if (m_htmlSessionId.isEmpty())
            get(QStringLiteral("/token"), RequestToken);
        else
            postHtmlLogin(m_htmlSessionId);
        break;
    }
    case RequestToken: {
        if (isRedirectStatus(status) && redirectUrl.isValid()) {
            get(pathAndQuery(redirectUrl), RequestToken);
            break;
        }
        QString sessionId;
        const QJsonObject obj = QJsonDocument::fromJson(body).object();
        sessionId = jsonString(obj, QStringList()
                               << QStringLiteral("Wilma2LoginID")
                               << QStringLiteral("SessionID")
                               << QStringLiteral("SESSIONID"));
        if (sessionId.isEmpty())
            sessionId = loginCookie();
        if (sessionId.isEmpty()) {
            tryIndexJsonLogin();
            return;
        }
        postHtmlLogin(sessionId);
        break;
    }
    case RequestLoginHtml:
        if (status == 403) {
            retryLogin(QStringLiteral("Wilma login was rejected. Retrying…"), false);
            return;
        }
        handleLoginBody(reply, body, url, redirectUrl);
        break;
    case RequestPostLogin: {
        if (isRedirectStatus(status) && redirectUrl.isValid()) {
            continueAfterLoginRedirect(redirectUrl);
            return;
        }
        const QString text = QString::fromUtf8(body);
        if (looksLikeLoginForm(text) && sessionCookie().isEmpty()) {
            retryLogin(QStringLiteral("Wilma login failed. Check the username and password."),
                       false);
            return;
        }
        const QString formkey = extractMfaFormkey(text);
        if (!formkey.isEmpty()) {
            if (m_sessionId.isEmpty())
                m_sessionId = sessionCookie();
            if (!m_sessionId.isEmpty())
                emit sessionIdChanged();
            m_loginInProgress = false;
            m_mfaFormkey = formkey;
            saveSettings();
            setBusy(false);
            setNeedsOtp(true);
            setStatus(QStringLiteral("Verification required"));
            emit otpRequired();
            return;
        }
        if (sessionCookie().isEmpty()) {
            retryLogin(QStringLiteral("Wilma login failed. Check the username and password."), false);
            return;
        }
        completeLogin();
        break;
    }
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
    case RequestRoles: {
        if (handleContentAuthFailure(status, body))
            break;
        applyRolesJson(body);
        if (m_roles.isEmpty() && m_roleId.isEmpty()) {
            get(QStringLiteral("/"), RequestHomeHtml);
            break;
        }
        ensureRoleSelected();
        if (!m_loggedIn)
            completeLogin(false);
        else
            setBusy(false);
        if (m_loggedIn)
            refreshHome();
        break;
    }
    case RequestHomeHtml: {
        if (handleContentAuthFailure(status, body))
            break;
        applyRolesFromHtml(QString::fromUtf8(body), url);
        ensureRoleSelected();
        if (!m_loggedIn)
            completeLogin(false);
        else
            setBusy(false);
        if (m_loggedIn)
            refreshHome();
        break;
    }
    case RequestMessages: {
        if (handleContentAuthFailure(status, body))
            break;
        const QString messageText = QString::fromUtf8(body);
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        QJsonArray messages = extractMessagesArray(doc);
        if (messages.isEmpty() && looksLikeHtml(body))
            applyMessageListHtml(messageText);
        else
            applyMessageList(messages);
        qDebug() << "Admirality messages:"
                 << "status=" << status
                 << "role=" << m_roleId
                 << "folder=" << m_messageFolder
                 << "count=" << m_messages.size()
                 << "url=" << url.toString();
        if (!m_loggedIn)
            completeLogin(false);
        else
            setBusy(false);
        if (!messages.isEmpty())
            emitMessageNotifications(messages, m_firstMessagePoll || m_restoring);
        m_firstMessagePoll = false;
        if (m_restoring)
            finishRestore(true);
        endRefreshIfMarked(reply);
        break;
    }
    case RequestOverview: {
        if (handleContentAuthFailure(status, body))
            break;
        if (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300) {
            const QJsonDocument doc = QJsonDocument::fromJson(body);
            if (doc.isObject())
                applyOverview(doc.object());
        }
        endRefreshIfMarked(reply);
        break;
    }
    case RequestNewsList: {
        if (handleContentAuthFailure(status, body))
            break;
        if (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300)
            applyNewsList(body);
        endRefreshIfMarked(reply);
        break;
    }
    case RequestAttendance: {
        if (handleContentAuthFailure(status, body))
            break;
        if (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300)
            applyAttendance(body, QString::fromUtf8(reply->rawHeader("Content-Type")));
        qDebug() << "Admirality attendance:"
                 << "status=" << status
                 << "role=" << m_roleId
                 << "count=" << m_lessonNotes.size()
                 << "url=" << url.toString();
        endRefreshIfMarked(reply);
        break;
    }
    case RequestMessageItem: {
        setDetailBusy(false);
        if (handleContentAuthFailure(status, body))
            break;
        applyMessageDetail(reply->property("itemId").toInt(), body,
                           QString::fromUtf8(reply->rawHeader("Content-Type")));
        break;
    }
    case RequestNewsItem: {
        setDetailBusy(false);
        if (handleContentAuthFailure(status, body))
            break;
        applyNewsDetail(reply->property("itemId").toInt(), body,
                        QString::fromUtf8(reply->rawHeader("Content-Type")));
        break;
    }
    case RequestRoleUnreadProbe: {
        handleRoleUnreadProbe(reply, body);
        break;
    }
    default:
        setBusy(false);
        break;
    }
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
    QSettings settings(settingsFilePath(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("schoolUrl"), m_schoolUrl);
    settings.setValue(QStringLiteral("schoolName"), m_schoolName);
    settings.setValue(QStringLiteral("username"), m_username);
    // Keep a previously saved password if the in-memory one was cleared.
    if (!m_password.isEmpty())
        settings.setValue(QStringLiteral("password"), m_password);
    settings.setValue(QStringLiteral("sessionId"), m_sessionId);
    settings.setValue(QStringLiteral("displayName"), m_displayName);
    settings.setValue(QStringLiteral("roleId"), m_roleId);
    settings.setValue(QStringLiteral("roleName"), m_roleName);
    settings.setValue(QStringLiteral("pollMode"), m_pollMode);
    settings.setValue(QStringLiteral("freshReady"), m_freshReady);
    QVariantList seen;
    for (int id : m_seenMessageIds)
        seen.append(id);
    settings.setValue(QStringLiteral("seenMessageIds"), seen);
    QVariantList locallyRead;
    for (int id : m_locallyReadIds)
        locallyRead.append(id);
    settings.setValue(QStringLiteral("locallyReadIds"), locallyRead);
    const QStringList categories = QStringList()
            << QStringLiteral("notes")
            << QStringLiteral("news")
            << QStringLiteral("grades")
            << QStringLiteral("homework")
            << QStringLiteral("exams");
    for (const QString &category : categories) {
        QStringList known;
        const QSet<QString> knownSet = m_knownKeys.value(category);
        for (const QString &key : knownSet)
            known.append(key);
        QStringList unseen;
        const QSet<QString> unseenSet = m_unseenKeys.value(category);
        for (const QString &key : unseenSet)
            unseen.append(key);
        settings.setValue(QStringLiteral("known/") + category, known);
        settings.setValue(QStringLiteral("unseen/") + category, unseen);
        settings.setValue(QStringLiteral("seeded/") + category,
                          m_seededCategories.contains(category));
    }
    // Flush immediately so a process kill right after login still keeps the session.
    settings.sync();
    const QFileInfo info(settings.fileName());
    if (settings.status() != QSettings::NoError || !info.exists() || info.size() == 0)
        qWarning() << "Admirality: failed to save settings to" << settings.fileName()
                   << "status" << settings.status()
                   << "exists" << info.exists()
                   << "size" << info.size();
    else
        qDebug() << "Admirality: saved settings to" << settings.fileName()
                 << "size" << info.size()
                 << "school=" << m_schoolUrl;

    QSettings mirror(appDataMirrorPath(), QSettings::IniFormat);
    for (const QString &key : settings.allKeys())
        mirror.setValue(key, settings.value(key));
    mirror.sync();
    writeSchoolSidecar(m_schoolUrl, m_schoolName);
}

void WilmaClient::loadSettings()
{
    QSettings settings(settingsFilePath(), QSettings::IniFormat);

    // Prefer AppData mirror if the sailjail config file is empty.
    if (schoolUrlEmpty(settings)) {
        QSettings mirror(appDataMirrorPath(), QSettings::IniFormat);
        if (!schoolUrlEmpty(mirror)) {
            copySettingsKeys(&mirror, &settings);
            settings.sync();
            qDebug() << "Admirality: restored settings from AppData mirror";
        }
    }

    if (schoolUrlEmpty(settings)) {
        const QStringList legacyPaths = legacySettingsPaths();
        for (const QString &path : legacyPaths) {
            QSettings legacy(path, QSettings::IniFormat);
            if (!schoolUrlEmpty(legacy)) {
                copySettingsKeys(&legacy, &settings);
                settings.sync();
                qDebug() << "Admirality: migrated settings from" << legacy.fileName()
                         << "to" << settings.fileName();
                break;
            }
        }
    }

    if (schoolUrlEmpty(settings)) {
        QSettings legacyDefault;
        if (!schoolUrlEmpty(legacyDefault)) {
            copySettingsKeys(&legacyDefault, &settings);
            settings.sync();
            qDebug() << "Admirality: migrated settings from" << legacyDefault.fileName()
                     << "to" << settings.fileName();
        }
    }

    m_schoolUrl = trimSlash(settings.value(QStringLiteral("schoolUrl")).toString());
    m_schoolName = settings.value(QStringLiteral("schoolName")).toString();
    if (m_schoolUrl.isEmpty()) {
        QString sidecarUrl;
        QString sidecarName;
        if (readSchoolSidecar(&sidecarUrl, &sidecarName)) {
            m_schoolUrl = trimSlash(sidecarUrl);
            if (m_schoolName.isEmpty())
                m_schoolName = sidecarName;
            qDebug() << "Admirality: restored school from sidecar" << m_schoolUrl;
        }
    }
    m_username = settings.value(QStringLiteral("username")).toString();
    m_password = settings.value(QStringLiteral("password")).toString();
    m_sessionId = settings.value(QStringLiteral("sessionId")).toString();
    m_displayName = settings.value(QStringLiteral("displayName")).toString();
    m_roleId = roleIdFromText(settings.value(QStringLiteral("roleId")).toString());
    m_roleName = settings.value(QStringLiteral("roleName")).toString();
    const QVariantList seen = settings.value(QStringLiteral("seenMessageIds")).toList();
    for (const QVariant &id : seen)
        m_seenMessageIds.insert(id.toInt());
    const QVariantList locallyRead = settings.value(QStringLiteral("locallyReadIds")).toList();
    for (const QVariant &id : locallyRead) {
        if (id.toInt() > 0)
            m_locallyReadIds.insert(id.toInt());
    }
    m_pollMode = normalizePollMode(settings.value(QStringLiteral("pollMode")).toString());
    m_freshReady = settings.value(QStringLiteral("freshReady")).toBool();
    const QStringList categories = QStringList()
            << QStringLiteral("notes")
            << QStringLiteral("news")
            << QStringLiteral("grades")
            << QStringLiteral("homework")
            << QStringLiteral("exams");
    for (const QString &category : categories) {
        QSet<QString> knownSet;
        const QStringList known = settings.value(QStringLiteral("known/") + category).toStringList();
        for (const QString &key : known)
            knownSet.insert(key);
        QSet<QString> unseenSet;
        const QStringList unseen = settings.value(QStringLiteral("unseen/") + category).toStringList();
        for (const QString &key : unseen)
            unseenSet.insert(key);
        m_knownKeys.insert(category, knownSet);
        m_unseenKeys.insert(category, unseenSet);
        if (settings.value(QStringLiteral("seeded/") + category).toBool())
            m_seededCategories.insert(category);
    }
    applySessionCookie();
    qDebug() << "Admirality: loaded settings from" << settings.fileName()
             << "school=" << m_schoolUrl
             << "user=" << m_username
             << "hasSession=" << !m_sessionId.isEmpty();
}

void WilmaClient::applySessionCookie()
{
    if (m_schoolUrl.isEmpty() || m_sessionId.isEmpty())
        return;
    QNetworkCookie cookie(QByteArray("Wilma2SID"), m_sessionId.toUtf8());
    cookie.setPath(QStringLiteral("/"));
    cookie.setSecure(true);
    m_cookies->setCookiesFromUrl(QList<QNetworkCookie>() << cookie, QUrl(m_schoolUrl));
}

void WilmaClient::ensureLoginCookie(const QString &sessionId)
{
    m_loginSessionId = sessionId;
    if (m_schoolUrl.isEmpty() || sessionId.isEmpty())
        return;
    // Also try the jar; Cookie headers are applied explicitly in get/postForm.
    QNetworkCookie cookie(QByteArray("Wilma2LoginID"), sessionId.toUtf8());
    cookie.setPath(QStringLiteral("/"));
    cookie.setSecure(true);
    m_cookies->setCookiesFromUrl(QList<QNetworkCookie>() << cookie, QUrl(m_schoolUrl));
}

void WilmaClient::ingestReplyCookies(QNetworkReply *reply)
{
    if (!reply)
        return;

    // Prefer raw Set-Cookie lines so we can strip SameSite for old Qt parsers.
    const QList<QNetworkReply::RawHeaderPair> pairs = reply->rawHeaderPairs();
    QList<QNetworkCookie> parsed;
    for (const QNetworkReply::RawHeaderPair &pair : pairs) {
        if (QString::fromLatin1(pair.first).compare(QStringLiteral("Set-Cookie"),
                                                    Qt::CaseInsensitive) != 0)
            continue;
        QByteArray line = pair.second;
        QRegularExpression sameSiteRe(QStringLiteral(";\\s*SameSite=[^;]*"),
                                      QRegularExpression::CaseInsensitiveOption);
        line = QString::fromUtf8(line).remove(sameSiteRe).toUtf8();
        parsed.append(QNetworkCookie::parseCookies(line));
    }
    if (parsed.isEmpty()) {
        const QVariant header = reply->header(QNetworkRequest::SetCookieHeader);
        if (header.isValid())
            parsed = qvariant_cast<QList<QNetworkCookie>>(header);
    }
    if (!parsed.isEmpty())
        m_cookies->setCookiesFromUrl(parsed, reply->url());

    // Keep our own copy of session cookies — jar storage is unreliable on SFOS Qt.
    const QString sid = cookieValueFromReply(reply, QByteArray("Wilma2SID"));
    if (!sid.isEmpty() && sid != m_sessionId) {
        m_sessionId = sid;
        emit sessionIdChanged();
    }
    const QString loginId = cookieValueFromReply(reply, QByteArray("Wilma2LoginID"));
    if (!loginId.isEmpty())
        m_loginSessionId = loginId;
}

QString WilmaClient::sessionCookie() const
{
    if (!m_sessionId.isEmpty())
        return m_sessionId;
    const QList<QNetworkCookie> cookies = m_cookies->cookiesForUrl(QUrl(m_schoolUrl));
    for (const QNetworkCookie &cookie : cookies) {
        if (cookie.name() == "Wilma2SID")
            return QString::fromUtf8(cookie.value());
    }
    return QString();
}

QString WilmaClient::loginCookie() const
{
    if (!m_loginSessionId.isEmpty())
        return m_loginSessionId;
    const QList<QNetworkCookie> cookies = m_cookies->cookiesForUrl(QUrl(m_schoolUrl));
    for (const QNetworkCookie &cookie : cookies) {
        if (cookie.name() == "Wilma2LoginID")
            return QString::fromUtf8(cookie.value());
    }
    return QString();
}

QString WilmaClient::cookieValueFromReply(QNetworkReply *reply, const QByteArray &name) const
{
    if (!reply)
        return QString();

    const QVariant header = reply->header(QNetworkRequest::SetCookieHeader);
    if (header.isValid()) {
        const QList<QNetworkCookie> cookies = qvariant_cast<QList<QNetworkCookie>>(header);
        for (const QNetworkCookie &cookie : cookies) {
            if (cookie.name() == name)
                return QString::fromUtf8(cookie.value());
        }
    }

    const QString prefix = QString::fromLatin1(name) + QLatin1Char('=');
    const QList<QNetworkReply::RawHeaderPair> pairs = reply->rawHeaderPairs();
    for (const QNetworkReply::RawHeaderPair &pair : pairs) {
        if (QString::fromLatin1(pair.first).compare(QStringLiteral("Set-Cookie"),
                                                    Qt::CaseInsensitive) != 0)
            continue;
        const QString line = QString::fromUtf8(pair.second);
        if (!line.startsWith(prefix, Qt::CaseInsensitive)) {
            // Could be "name=value" after other attrs — match at start of cookie-pair.
            continue;
        }
        const int end = line.indexOf(QLatin1Char(';'));
        return end >= 0 ? line.mid(prefix.size(), end - prefix.size())
                        : line.mid(prefix.size());
    }

    // Fallback regex anywhere in Set-Cookie (handles unusual formatting).
    QRegularExpression re(QStringLiteral("(?:^|[\\r\\n,])\\s*%1=([^;\\r\\n]*)")
                          .arg(QString::fromLatin1(name)),
                          QRegularExpression::CaseInsensitiveOption);
    for (const QNetworkReply::RawHeaderPair &pair : pairs) {
        if (QString::fromLatin1(pair.first).compare(QStringLiteral("Set-Cookie"),
                                                    Qt::CaseInsensitive) != 0)
            continue;
        const QRegularExpressionMatch match = re.match(QString::fromUtf8(pair.second));
        if (match.hasMatch())
            return match.captured(1).trimmed();
        // Single Set-Cookie line without leading delimiter
        if (QString::fromUtf8(pair.second).startsWith(prefix, Qt::CaseInsensitive)) {
            const QString line = QString::fromUtf8(pair.second);
            const int end = line.indexOf(QLatin1Char(';'));
            return end >= 0 ? line.mid(prefix.size(), end - prefix.size())
                            : line.mid(prefix.size());
        }
    }
    return QString();
}

QByteArray WilmaClient::cookieHeader() const
{
    QByteArray header;
    if (!m_loginSessionId.isEmpty())
        header += "Wilma2LoginID=" + m_loginSessionId.toUtf8();
    if (!m_sessionId.isEmpty()) {
        if (!header.isEmpty())
            header += "; ";
        header += "Wilma2SID=" + m_sessionId.toUtf8();
    }
    return header;
}

void WilmaClient::applyRequestCookies(QNetworkRequest *request) const
{
    if (!request)
        return;
    const QByteArray header = cookieHeader();
    if (!header.isEmpty())
        request->setRawHeader("Cookie", header);
}

void WilmaClient::startLogin()
{
    if (m_loginInProgress)
        return;
    if (m_schoolUrl.isEmpty()) {
        failLogin(QStringLiteral("Choose a Wilma first"));
        return;
    }
    m_loginInProgress = true;
    m_postLoginHops = 0;
    setBusy(true);
    setStatus(QStringLiteral("Signing in…"));
    clearError();
    setNeedsOtp(false);
    m_mfaFormkey.clear();
    m_loginFields.clear();
    m_htmlSessionId.clear();
    m_loginSessionId.clear();
    // Drop stale jar cookies / saved SID so success means a fresh Wilma2SID.
    m_cookies->setCookiesFromUrl(QList<QNetworkCookie>(), QUrl(m_schoolUrl));
    if (!m_sessionId.isEmpty()) {
        m_sessionId.clear();
        emit sessionIdChanged();
    }
    fetchLoginPageForFields();
}

void WilmaClient::retryLogin(const QString &reason, bool wrongPassword)
{
    if (wrongPassword || m_loginTry >= 2) {
        failLogin(reason);
        return;
    }
    m_loginTry += 1;
    qDebug() << "Admirality login retry" << m_loginTry << reason;
    setStatus(QStringLiteral("Retrying sign-in…"));
    if (m_loginTry == 1) {
        tryIndexJsonLogin();
        return;
    }
    m_loginInProgress = false;
    startLogin();
}

void WilmaClient::postIndexJson(const QString &sessionId)
{
    ensureLoginCookie(sessionId);
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
    ensureLoginCookie(sessionId);
    QUrlQuery form;
    for (auto it = m_loginFields.constBegin(); it != m_loginFields.constEnd(); ++it)
        form.addQueryItem(it.key(), it.value());
    form.removeQueryItem(QStringLiteral("Login"));
    form.removeQueryItem(QStringLiteral("Password"));
    form.removeQueryItem(QStringLiteral("SESSIONID"));
    form.addQueryItem(QStringLiteral("Login"), m_username);
    form.addQueryItem(QStringLiteral("Password"), m_password);
    form.addQueryItem(QStringLiteral("SESSIONID"), sessionId);
    if (!form.hasQueryItem(QStringLiteral("submit")))
        form.addQueryItem(QStringLiteral("submit"), QStringLiteral("Kirjaudu sisään"));
    if (!form.hasQueryItem(QStringLiteral("returnpath")))
        form.addQueryItem(QStringLiteral("returnpath"), QString());
    postForm(QStringLiteral("/login"), form, RequestLoginHtml);
}

void WilmaClient::fetchLoginPageForFields()
{
    get(QStringLiteral("/login"), RequestLoginPage);
}

void WilmaClient::tryIndexJsonLogin()
{
    setStatus(QStringLiteral("Signing in…"));
    get(QStringLiteral("/index_json"), RequestIndexJson);
}

void WilmaClient::completeLogin(bool fetchMessages)
{
    const QString sid = sessionCookie();
    if (sid.isEmpty()) {
        failLogin(QStringLiteral("Wilma login failed. Check the username and password."));
        return;
    }
    if (m_sessionId != sid) {
        m_sessionId = sid;
        emit sessionIdChanged();
    }
    applySessionCookie();
    m_loginSessionId.clear();
    m_loginInProgress = false;
    m_loginTry = 0;
    m_postLoginHops = 0;
    setNeedsOtp(false);
    m_mfaFormkey.clear();
    setLoggedIn(true);
    saveSettings();
    setStatus(QStringLiteral("Signed in"));
    clearError();
    armPollTimer();

    // Leave the splash as soon as auth succeeds. Waiting for /messages/list used
    // to race the 15s splash timeout and strand the user on LoginPage after kill.
    const bool wasRestoring = m_restoring;
    if (wasRestoring)
        finishRestore(true);
    else
        emit loginSucceeded();

    get(QStringLiteral("/api/v1/accounts/me"), RequestAccount);
    if (fetchMessages)
        fetchRoles();
}

QUrl WilmaClient::resolveRedirect(const QNetworkReply *reply) const
{
    if (!reply)
        return QUrl();
    QUrl redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (redirect.isEmpty()) {
        const QByteArray location = reply->rawHeader("Location");
        if (!location.isEmpty())
            redirect = QUrl(QString::fromUtf8(location));
    }
    if (redirect.isEmpty())
        return QUrl();
    if (redirect.isRelative())
        redirect = reply->url().resolved(redirect);
    return redirect;
}

void WilmaClient::failLogin(const QString &message)
{
    m_loginInProgress = false;
    setBusy(false);
    setStatus(QStringLiteral("Sign-in failed"));
    setError(message);
    emit loginFailed(message);
    if (m_restoring)
        finishRestore(false);
    // One delayed retry — covers transient Wilma/CDN blips after a daemon restart.
    if (hasCredentials() && !m_schoolUrl.isEmpty() && m_loginTry >= 2) {
        QTimer::singleShot(8000, this, [this]() {
            if (m_loggedIn || m_loginInProgress || !hasCredentials())
                return;
            m_loginTry = 0;
            startLogin();
        });
    }
}

void WilmaClient::continueAfterLoginRedirect(const QUrl &redirectUrl)
{
    if (!redirectUrl.isValid()) {
        if (!sessionCookie().isEmpty())
            completeLogin();
        else
            retryLogin(QStringLiteral("Wilma login failed. Check the username and password."), false);
        return;
    }
    if (m_postLoginHops >= 8) {
        if (!sessionCookie().isEmpty())
            completeLogin();
        else
            retryLogin(QStringLiteral("Wilma login did not finish"), false);
        return;
    }
    m_postLoginHops += 1;
    get(pathAndQuery(redirectUrl), RequestPostLogin);
}

void WilmaClient::handleLoginBody(QNetworkReply *reply,
                                  const QByteArray &body,
                                  const QUrl &url,
                                  const QUrl &redirectUrl)
{
    const QString text = QString::fromUtf8(body);
    QString sid = cookieValueFromReply(reply, QByteArray("Wilma2SID"));
    if (sid.isEmpty())
        sid = sessionCookie();
    const QString location = redirectUrl.isValid()
            ? redirectUrl.toString()
            : url.toString();
    const bool failed = location.contains(QStringLiteral("loginfailed"), Qt::CaseInsensitive)
            || text.contains(QStringLiteral("loginFailed"), Qt::CaseInsensitive);

    qDebug() << "Admirality login:"
             << "status=" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
             << "location=" << location
             << "sid=" << !sid.isEmpty()
             << "failed=" << failed;

    if (failed && sid.isEmpty()) {
        retryLogin(QStringLiteral("Wilma login failed. Check the username and password."), false);
        return;
    }

    if (!sid.isEmpty()) {
        if (m_sessionId != sid) {
            m_sessionId = sid;
            emit sessionIdChanged();
        }
        applySessionCookie();
        // MFA challenge lives on the post-login redirect page, not the 302 body.
        if (redirectUrl.isValid()
                && !location.contains(QStringLiteral("loginfailed"), Qt::CaseInsensitive)) {
            continueAfterLoginRedirect(redirectUrl);
            return;
        }
        const QString formkey = extractMfaFormkey(text);
        if (!formkey.isEmpty()) {
            m_loginInProgress = false;
            m_mfaFormkey = formkey;
            saveSettings();
            setBusy(false);
            setNeedsOtp(true);
            setStatus(QStringLiteral("Verification required"));
            emit otpRequired();
            return;
        }
        completeLogin();
        return;
    }

    // Some Wilma frontends set Wilma2SID only after the redirect is followed.
    if (redirectUrl.isValid() && !failed) {
        continueAfterLoginRedirect(redirectUrl);
        return;
    }

    if (!location.isEmpty() && location != url.toString())
        retryLogin(QStringLiteral("Wilma login failed (%1). Check the username and password.")
                   .arg(location), false);
    else
        retryLogin(QStringLiteral("Wilma login failed. Check the username and password."), false);
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
    const QString resolved = usesRolePrefix(kind) ? rolePath(path) : path;
    QUrl url(m_schoolUrl + resolved);
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", kUserAgent);
    request.setRawHeader("Referer", m_schoolUrl.toUtf8() + "/");
    if (kind == RequestMessageItem) {
        // The HTML message page is what Wilma treats as opening the message,
        // which marks it read. JSON-only fetches leave it unread.
        request.setRawHeader("Accept", "text/html,application/json;q=0.8,*/*;q=0.5");
    } else {
        request.setRawHeader("Accept", "application/json, text/html;q=0.9,*/*;q=0.8");
    }
    applyRequestCookies(&request);
    const bool autoFollow = kind != RequestLoginPage
            && kind != RequestPostLogin
            && kind != RequestToken
            && kind != RequestIndexJson;
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         autoFollow ? QNetworkRequest::NoLessSafeRedirectPolicy
                                    : QNetworkRequest::ManualRedirectPolicy);
#elif QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, autoFollow);
#endif
    qDebug() << "Admirality GET" << url.toString() << "kind=" << kind;
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
    request.setRawHeader("Referer", (m_schoolUrl + QStringLiteral("/login")).toUtf8());
    request.setRawHeader("Origin", m_schoolUrl.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    applyRequestCookies(&request);
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
#elif QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, false);
#endif
    QNetworkReply *reply = m_nam->post(request, form.query(QUrl::FullyEncoded).toUtf8());
    reply->setProperty("kind", static_cast<int>(kind));
    connect(reply, SIGNAL(finished()), this, SLOT(onReplyFinished()));
    return reply;
}

QString WilmaClient::rolePath(const QString &path) const
{
    if (m_roleId.isEmpty() || path.startsWith(QLatin1String("/!")))
        return path;
    QString suffix = path;
    if (!suffix.startsWith(QLatin1Char('/')))
        suffix.prepend(QLatin1Char('/'));
    return QStringLiteral("/!%1%2").arg(m_roleId, suffix);
}

bool WilmaClient::usesRolePrefix(RequestKind kind) const
{
    switch (kind) {
    case RequestMessages:
    case RequestOverview:
    case RequestNewsList:
    case RequestNewsItem:
    case RequestMessageItem:
    case RequestAttendance:
        return true;
    case RequestRoleUnreadProbe:
        return false;
    default:
        return false;
    }
}

void WilmaClient::rememberRoleFromUrl(const QUrl &url)
{
    if (!url.isValid())
        return;
    const QString id = roleIdFromText(url.path());
    if (id.isEmpty())
        return;
    QString name = m_roleName;
    if (name.isEmpty())
        name = id;
    for (const QVariant &entry : m_roles) {
        const QVariantMap map = entry.toMap();
        if (map.value(QStringLiteral("id")).toString() == id) {
            name = map.value(QStringLiteral("name")).toString();
            break;
        }
    }
    setRole(id, name);
}

void WilmaClient::applyRolesJson(const QByteArray &body)
{
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    QJsonArray roles;
    if (doc.isArray()) {
        roles = doc.array();
    } else if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        if (obj.value(QStringLiteral("payload")).isArray())
            roles = obj.value(QStringLiteral("payload")).toArray();
        else if (obj.value(QStringLiteral("Payload")).isArray())
            roles = obj.value(QStringLiteral("Payload")).toArray();
        else if (obj.value(QStringLiteral("Roles")).isArray())
            roles = obj.value(QStringLiteral("Roles")).toArray();
        else if (obj.value(QStringLiteral("roles")).isArray())
            roles = obj.value(QStringLiteral("roles")).toArray();
    }

    QVariantList items;
    for (const QJsonValue &value : roles) {
        const QJsonObject obj = value.toObject();
        const QJsonValue type = obj.contains(QStringLiteral("type"))
                ? obj.value(QStringLiteral("type"))
                : obj.value(QStringLiteral("Type"));
        if (isPasswdRole(type))
            continue;
        QString id = roleIdFromText(jsonString(obj, QStringList()
                                               << QStringLiteral("slug")
                                               << QStringLiteral("Slug")
                                               << QStringLiteral("href")
                                               << QStringLiteral("Href")));
        if (id.isEmpty()) {
            int primus = jsonInt(obj, QStringLiteral("PrimusId"));
            if (primus <= 0)
                primus = jsonInt(obj, QStringLiteral("primusId"));
            if (primus <= 0)
                continue;
            id = QString::number(primus);
        }
        QVariantMap item;
        item.insert(QStringLiteral("id"), id);
        item.insert(QStringLiteral("name"), roleNameFromObject(obj, id));
        items.append(item);
    }
    if (items != m_roles) {
        m_roles = items;
        emit rolesChanged();
    }
}

void WilmaClient::applyRolesFromHtml(const QString &html, const QUrl &pageUrl)
{
    QRegularExpression hrefRe(QStringLiteral("href=['\"]([^'\"]+/!\\d+[^'\"]*)['\"]"),
                              QRegularExpression::CaseInsensitiveOption);
    QMap<QString, QString> found;
    QRegularExpressionMatchIterator it = hrefRe.globalMatch(html);
    while (it.hasNext()) {
        const QString href = it.next().captured(1);
        const QString id = roleIdFromText(href);
        if (id.isEmpty() || found.contains(id))
            continue;
        found.insert(id, id);
    }
    rememberRoleFromUrl(pageUrl);
    if (found.isEmpty() && !m_roleId.isEmpty())
        found.insert(m_roleId, m_roleName.isEmpty() ? m_roleId : m_roleName);

    if (found.isEmpty())
        return;
    QVariantList items;
    for (auto itFound = found.constBegin(); itFound != found.constEnd(); ++itFound) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), itFound.key());
        item.insert(QStringLiteral("name"), itFound.value());
        items.append(item);
    }
    if (items != m_roles) {
        m_roles = items;
        emit rolesChanged();
    }
}

void WilmaClient::ensureRoleSelected()
{
    if (!m_roleId.isEmpty()) {
        for (const QVariant &entry : m_roles) {
            const QVariantMap map = entry.toMap();
            if (map.value(QStringLiteral("id")).toString() == m_roleId) {
                const QString name = map.value(QStringLiteral("name")).toString();
                if (!name.isEmpty() && name != m_roleName)
                    setRole(m_roleId, name);
                return;
            }
        }
        return;
    }
    if (m_roles.isEmpty())
        return;
    const QVariantMap first = m_roles.first().toMap();
    setRole(first.value(QStringLiteral("id")).toString(),
            first.value(QStringLiteral("name")).toString());
}

void WilmaClient::fetchRoles()
{
    get(QStringLiteral("/api/v1/accounts/me/roles"), RequestRoles);
}

void WilmaClient::setRole(const QString &roleId, const QString &roleName)
{
    const QString id = roleIdFromText(roleId);
    if (id.isEmpty())
        return;
    bool changed = false;
    if (m_roleId != id) {
        m_roleId = id;
        emit roleIdChanged();
        changed = true;
    }
    if (!roleName.isEmpty() && m_roleName != roleName) {
        m_roleName = roleName;
        emit roleNameChanged();
        changed = true;
    }
    if (changed)
        saveSettings();
}

QString WilmaClient::messagesListPath() const
{
    const QString folder = m_messageFolder.toLower();
    if (folder == QLatin1String("archive"))
        return QStringLiteral("/messages/list/archive");
    if (folder == QLatin1String("outbox") || folder == QLatin1String("sent"))
        return QStringLiteral("/messages/list/outbox");
    if (folder == QLatin1String("drafts"))
        return QStringLiteral("/messages/list/drafts");
    return QStringLiteral("/messages/list");
}

QJsonArray WilmaClient::extractMessagesArray(const QJsonDocument &doc) const
{
    if (doc.isArray())
        return doc.array();
    if (doc.isObject())
        return messagesArrayFromObject(doc.object());
    return QJsonArray();
}

void WilmaClient::applyMessageListHtml(const QString &html)
{
    QVariantList items;
    QRegularExpression linkRe(QStringLiteral("<a[^>]+href=['\"][^'\"]*/messages/(\\d+)[^'\"]*['\"][^>]*>([\\s\\S]*?)</a>"),
                              QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator it = linkRe.globalMatch(html);
    QSet<int> seen;
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const int id = match.captured(1).toInt();
        const QString subject = htmlToText(match.captured(2));
        if (id <= 0 || seen.contains(id) || subject.isEmpty())
            continue;
        seen.insert(id);
        QVariantMap item;
        item.insert(QStringLiteral("id"), id);
        item.insert(QStringLiteral("subject"), subject);
        item.insert(QStringLiteral("sender"), QString());
        item.insert(QStringLiteral("time"), QString());
        item.insert(QStringLiteral("unread"), false);
        items.append(item);
    }
    if (items == m_messages)
        return;
    m_messages = items;
    emit messagesChanged();
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
        armPollTimer();
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
        const bool serverUnread = messageUnread(obj);
        if (m_locallyReadIds.contains(id) && !serverUnread)
            m_locallyReadIds.remove(id);
        const bool unreadFlag = serverUnread && !m_locallyReadIds.contains(id);
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

void WilmaClient::clearHomeData()
{
    if (!m_messages.isEmpty()) {
        m_messages.clear();
        emit messagesChanged();
    }
    if (!m_news.isEmpty()) {
        m_news.clear();
        emit newsChanged();
    }
    if (!m_schedule.isEmpty()) {
        m_schedule.clear();
        emit scheduleChanged();
    }
    if (!m_exams.isEmpty()) {
        m_exams.clear();
        emit examsChanged();
    }
    if (!m_homework.isEmpty()) {
        m_homework.clear();
        emit homeworkChanged();
    }
    if (!m_lessonNotes.isEmpty()) {
        m_lessonNotes.clear();
        emit lessonNotesChanged();
    }
    if (!m_grades.isEmpty()) {
        m_grades.clear();
        emit gradesChanged();
    }
    if (!m_currentMessage.isEmpty()) {
        m_currentMessage.clear();
        emit currentMessageChanged();
    }
    if (!m_currentNews.isEmpty()) {
        m_currentNews.clear();
        emit currentNewsChanged();
    }
    m_pendingRefresh = 0;
    setRefreshing(false);
    setDetailBusy(false);
}

void WilmaClient::setRefreshing(bool refreshing)
{
    if (m_refreshing == refreshing)
        return;
    m_refreshing = refreshing;
    emit refreshingChanged();
}

void WilmaClient::setDetailBusy(bool busy)
{
    if (m_detailBusy == busy)
        return;
    m_detailBusy = busy;
    emit detailBusyChanged();
}

void WilmaClient::setPollMode(const QString &mode)
{
    const QString normalized = normalizePollMode(mode);
    if (m_pollMode == normalized)
        return;
    m_pollMode = normalized;
    emit pollModeChanged();
    saveSettings();
    armPollTimer();
}

void WilmaClient::acknowledgeCategory(const QString &category)
{
    if (m_unseenKeys.value(category).isEmpty())
        return;
    m_unseenKeys.insert(category, QSet<QString>());
    emit freshCountsChanged();
    saveSettings();
}

void WilmaClient::acknowledgeNotes() { acknowledgeCategory(QStringLiteral("notes")); }
void WilmaClient::acknowledgeNews() { acknowledgeCategory(QStringLiteral("news")); }
void WilmaClient::acknowledgeGrades() { acknowledgeCategory(QStringLiteral("grades")); }
void WilmaClient::acknowledgeHomework() { acknowledgeCategory(QStringLiteral("homework")); }
void WilmaClient::acknowledgeExams() { acknowledgeCategory(QStringLiteral("exams")); }

void WilmaClient::markMessageRead(int messageId)
{
    if (messageId <= 0)
        return;
    m_locallyReadIds.insert(messageId);
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
    if (changed)
        emit messagesChanged();
    if (m_unreadCount != unread) {
        m_unreadCount = unread;
        emit unreadCountChanged();
    }
    saveSettings();
}

void WilmaClient::absorbFresh(const QString &category, const QVariantList &hits)
{
    QSet<QString> current;
    QVariantList arrived;
    QSet<QString> &known = m_knownKeys[category];
    for (const QVariant &entry : hits) {
        const QVariantMap map = entry.toMap();
        const QString key = map.value(QStringLiteral("key")).toString();
        if (key.isEmpty() || current.contains(key))
            continue;
        current.insert(key);
        if (!m_freshReady) {
            known.insert(key);
            continue;
        }
        if (!known.contains(key)) {
            known.insert(key);
            if (m_seededCategories.contains(category))
                arrived.append(entry);
        }
    }

    if (!m_freshReady)
        return;

    if (!m_seededCategories.contains(category)) {
        m_seededCategories.insert(category);
        m_unseenKeys.insert(category, QSet<QString>());
        saveSettings();
        return;
    }

    QSet<QString> still;
    const QSet<QString> unseen = m_unseenKeys.value(category);
    for (const QString &key : unseen) {
        if (current.contains(key))
            still.insert(key);
    }
    for (const QVariant &entry : arrived)
        still.insert(entry.toMap().value(QStringLiteral("key")).toString());

    const bool changed = still != unseen;
    m_unseenKeys.insert(category, still);
    if (!arrived.isEmpty()) {
        const QVariantMap first = arrived.first().toMap();
        QString title = first.value(QStringLiteral("title")).toString();
        if (arrived.size() > 1)
            title = QStringLiteral("%1 (%2)").arg(title).arg(arrived.size());
        QVariantMap data;
        data.insert(QStringLiteral("category"), category);
        data.insert(QStringLiteral("count"), arrived.size());
        emit notificationReceived(title,
                                  first.value(QStringLiteral("body")).toString(),
                                  data);
        emit freshCountsChanged();
        saveSettings();
    } else if (changed) {
        emit freshCountsChanged();
        saveSettings();
    }
}

void WilmaClient::armPollTimer()
{
    m_pollTimer.stop();
    if (!m_loggedIn)
        return;
    if (m_pollMode == QLatin1String("schoolday")) {
        m_pollTimer.setSingleShot(true);
        const int wait = msecsUntilSchoolPoll();
        m_pollTimer.setInterval(qMax(1000, wait));
    } else {
        m_pollTimer.setSingleShot(false);
        m_pollTimer.setInterval(intervalForPollMode(m_pollMode));
    }
    m_pollTimer.start();
}

int WilmaClient::msecsUntilSchoolPoll() const
{
    const QDateTime now = QDateTime::currentDateTime();
    QMap<QString, QPair<QTime, QTime>> days;
    for (const QVariant &entry : m_schedule) {
        const QVariantMap map = entry.toMap();
        const QString iso = map.value(QStringLiteral("date")).toString();
        const QTime start = parseClock(map.value(QStringLiteral("start")).toString());
        QTime end = parseClock(map.value(QStringLiteral("end")).toString());
        if (iso.isEmpty() || !start.isValid())
            continue;
        if (!end.isValid())
            end = start.addSecs(45 * 60);
        QPair<QTime, QTime> span = days.value(iso, qMakePair(start, end));
        if (!days.contains(iso))
            span = qMakePair(start, end);
        if (start < span.first)
            span.first = start;
        if (end > span.second)
            span.second = end;
        days.insert(iso, span);
    }

    QList<QDateTime> instants;
    if (days.isEmpty()) {
        QDate date = now.date();
        for (int i = 0; i < 14; ++i) {
            if (date.dayOfWeek() <= 5) {
                instants.append(QDateTime(date, QTime(7, 45)));
                instants.append(QDateTime(date, QTime(15, 15)));
            }
            date = date.addDays(1);
        }
    } else {
        for (auto it = days.constBegin(); it != days.constEnd(); ++it) {
            const QDate date = QDate::fromString(it.key(), QStringLiteral("yyyy-MM-dd"));
            if (!date.isValid())
                continue;
            instants.append(QDateTime(date, it.value().first).addSecs(-15 * 60));
            instants.append(QDateTime(date, it.value().second).addSecs(15 * 60));
        }
    }
    std::sort(instants.begin(), instants.end());
    for (const QDateTime &when : instants) {
        const qint64 ms = now.msecsTo(when);
        if (ms > 5000)
            return static_cast<int>(qMin(ms, qint64(7) * 24 * 60 * 60 * 1000));
    }
    return 60 * 60 * 1000;
}

void WilmaClient::endRefreshIfMarked(QNetworkReply *reply)
{
    if (!reply || !reply->property("refresh").toBool())
        return;
    if (m_pendingRefresh > 0)
        m_pendingRefresh -= 1;
    if (m_pendingRefresh <= 0) {
        m_pendingRefresh = 0;
        setRefreshing(false);
        if (!m_freshReady && m_loggedIn) {
            m_freshReady = true;
            const QStringList categories = QStringList()
                    << QStringLiteral("notes")
                    << QStringLiteral("news")
                    << QStringLiteral("grades")
                    << QStringLiteral("homework")
                    << QStringLiteral("exams");
            for (const QString &category : categories)
                m_seededCategories.insert(category);
            saveSettings();
        }
        if (m_loggedIn)
            startRoleUnreadProbes();
    }
}

int WilmaClient::currentUnreadStuff() const
{
    return m_unreadCount
            + freshNoteCount()
            + freshNewsCount()
            + freshGradeCount()
            + freshHomeworkCount()
            + freshExamCount();
}

int WilmaClient::roleUnreadTotal(const QString &roleId) const
{
    return m_roleUnreadScores.value(roleIdFromText(roleId), 0);
}

void WilmaClient::setRoleScore(const QString &roleId, int score)
{
    const QString id = roleIdFromText(roleId);
    if (id.isEmpty())
        return;
    if (m_roleUnreadScores.value(id, -1) == score)
        return;
    m_roleUnreadScores.insert(id, score);
    m_roleScoresChanged = true;
}

void WilmaClient::startRoleUnreadProbes()
{
    if (!m_loggedIn || m_schoolUrl.isEmpty() || m_roles.size() <= 1 || m_roleProbePending > 0)
        return;

    const int fresh = freshNoteCount() + freshNewsCount() + freshGradeCount()
            + freshHomeworkCount() + freshExamCount();
    if (!m_roleId.isEmpty()) {
        m_roleFreshTotals.insert(m_roleId, fresh);
        setRoleScore(m_roleId, m_unreadCount + fresh);
    }

    for (const QVariant &entry : m_roles) {
        const QString id = roleIdFromText(entry.toMap().value(QStringLiteral("id")).toString());
        if (id.isEmpty() || id == m_roleId)
            continue;
        m_roleProbePending += 1;
        QNetworkReply *reply = get(QStringLiteral("/!%1/messages/list").arg(id),
                                   RequestRoleUnreadProbe);
        reply->setProperty("probeRoleId", id);
    }
    if (m_roleProbePending <= 0)
        maybeAutoSelectHottestRole();
}

void WilmaClient::handleRoleUnreadProbe(QNetworkReply *reply, const QByteArray &body)
{
    if (m_roleProbePending > 0)
        m_roleProbePending -= 1;

    const QString roleId = roleIdFromText(reply
                                          ? reply->property("probeRoleId").toString()
                                          : QString());
    if (!roleId.isEmpty()) {
        int unread = 0;
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        const QJsonArray messages = extractMessagesArray(doc);
        for (const QJsonValue &value : messages) {
            if (messageUnread(value.toObject()))
                unread += 1;
        }
        const int fresh = m_roleFreshTotals.value(roleId, 0);
        setRoleScore(roleId, unread + fresh);
    }

    if (m_roleProbePending <= 0) {
        m_roleProbePending = 0;
        maybeAutoSelectHottestRole();
    }
}

void WilmaClient::maybeAutoSelectHottestRole()
{
    if (!m_roleScoresChanged || m_roles.size() <= 1 || !m_loggedIn)
        return;
    m_roleScoresChanged = false;

    QString bestId = m_roleId;
    int bestScore = roleUnreadTotal(m_roleId);
    for (const QVariant &entry : m_roles) {
        const QString id = roleIdFromText(entry.toMap().value(QStringLiteral("id")).toString());
        if (id.isEmpty())
            continue;
        const int score = roleUnreadTotal(id);
        if (score > bestScore) {
            bestScore = score;
            bestId = id;
        }
    }
    if (bestScore <= 0 || bestId.isEmpty() || bestId == m_roleId)
        return;
    qDebug() << "Admirality auto-select role" << bestId << "score=" << bestScore;
    selectRole(bestId);
}

bool WilmaClient::isInvalidSession(int status, const QByteArray &body) const
{
    if (status == 401)
        return true;
    const QString text = QString::fromUtf8(body);
    if (text.contains(QStringLiteral("loginFailed"), Qt::CaseInsensitive))
        return true;
    if (looksLikeLoginForm(text))
        return true;
    return false;
}

bool WilmaClient::handleContentAuthFailure(int status, const QByteArray &body)
{
    if (m_loginInProgress)
        return isInvalidSession(status, body) || status == 403 || status == 401;
    // Unprefixed content GETs 403 until /!primusId/ is known. Swallow those
    // without wiping Wilma2SID — fetchRoles() is already in flight after login.
    if (status == 403 && m_roleId.isEmpty() && !isInvalidSession(status, body)) {
        m_pendingRefresh = 0;
        setRefreshing(false);
        setDetailBusy(false);
        return true;
    }
    if (!isInvalidSession(status, body))
        return false;
    m_pendingRefresh = 0;
    setRefreshing(false);
    setDetailBusy(false);
    if (hasCredentials()) {
        startLogin();
        return true;
    }
    if (m_restoring)
        finishRestore(false);
    return true;
}

void WilmaClient::applyMessageList(const QJsonArray &messages)
{
    QVariantList items;
    items.reserve(messages.size());
    for (const QJsonValue &value : messages) {
        const QJsonObject obj = value.toObject();
        int id = jsonInt(obj, QStringLiteral("Id"));
        if (id <= 0)
            id = jsonInt(obj, QStringLiteral("id"));
        if (id <= 0)
            id = jsonInt(obj, QStringLiteral("MessageId"));
        if (id <= 0)
            id = jsonInt(obj, QStringLiteral("WilmaId"));
        if (id <= 0)
            continue;
        QVariantMap item;
        item.insert(QStringLiteral("id"), id);
        item.insert(QStringLiteral("subject"),
                    jsonString(obj, QStringList() << QStringLiteral("Subject")
                                                 << QStringLiteral("subject")
                                                 << QStringLiteral("Title")));
        item.insert(QStringLiteral("sender"), messageSender(obj));
        const QDateTime sent = parseWilmaDateTime(firstJsonValue(obj, QStringList()
                    << QStringLiteral("TimeStamp")
                    << QStringLiteral("Timestamp")
                    << QStringLiteral("Time")
                    << QStringLiteral("time")
                    << QStringLiteral("Date")
                    << QStringLiteral("Sent")
                    << QStringLiteral("SentAt")
                    << QStringLiteral("timestamp")));
        item.insert(QStringLiteral("time"), formatDisplayDateTime(sent));
        const bool serverUnread = messageUnread(obj);
        if (m_locallyReadIds.contains(id) && !serverUnread)
            m_locallyReadIds.remove(id);
        item.insert(QStringLiteral("unread"), serverUnread && !m_locallyReadIds.contains(id));
        item.insert(QStringLiteral("folder"),
                    jsonString(obj, QStringList() << QStringLiteral("Folder")
                                                 << QStringLiteral("folder")));
        items.append(item);
    }
    if (items == m_messages)
        return;
    m_messages = items;
    emit messagesChanged();
}

void WilmaClient::applyOverview(const QJsonObject &raw)
{
    QJsonObject obj = raw;
    if (obj.value(QStringLiteral("payload")).isObject())
        obj = obj.value(QStringLiteral("payload")).toObject();
    else if (obj.value(QStringLiteral("Payload")).isObject())
        obj = obj.value(QStringLiteral("Payload")).toObject();

    QVariantList schedule;
    const QJsonArray rawSchedule = obj.value(QStringLiteral("Schedule")).toArray();
    for (const QJsonValue &value : rawSchedule) {
        const QJsonObject entry = value.toObject();
        const QJsonArray dates = entry.value(QStringLiteral("DateArray")).toArray();
        const QJsonArray groups = entry.value(QStringLiteral("Groups")).toArray();
        const int day = jsonInt(entry, QStringLiteral("Day"));
        const QString start = jsonString(entry, QStringList() << QStringLiteral("Start"));
        const QString end = jsonString(entry, QStringList() << QStringLiteral("End"));
        for (const QJsonValue &dateValue : dates) {
            const QDate date = parseWilmaDate(dateValue.toString());
            const QString iso = date.isValid()
                    ? date.toString(QStringLiteral("yyyy-MM-dd"))
                    : dateValue.toString();
            for (const QJsonValue &groupValue : groups) {
                const QJsonObject group = groupValue.toObject();
                QVariantMap item;
                item.insert(QStringLiteral("date"), iso);
                item.insert(QStringLiteral("dateLabel"), formatDisplayDate(date));
                item.insert(QStringLiteral("dayOfWeek"), day);
                item.insert(QStringLiteral("start"), start);
                item.insert(QStringLiteral("end"), end);
                item.insert(QStringLiteral("subject"),
                            jsonString(group, QStringList()
                                       << QStringLiteral("FullCaption")
                                       << QStringLiteral("Caption")
                                       << QStringLiteral("ShortCaption")));
                item.insert(QStringLiteral("subjectCode"),
                            jsonString(group, QStringList() << QStringLiteral("ShortCaption")));
                item.insert(QStringLiteral("teacher"),
                            firstTeacherName(group.value(QStringLiteral("Teachers")).toArray()));
                item.insert(QStringLiteral("isToday"), iso == todayIso());
                schedule.append(item);
            }
        }
    }

    QVariantList exams;
    QVariantList homework;
    const QString today = todayIso();
    const QJsonArray groups = obj.value(QStringLiteral("Groups")).toArray();
    for (const QJsonValue &groupValue : groups) {
        const QJsonObject group = groupValue.toObject();
        const QString subject = jsonString(group, QStringList()
                                           << QStringLiteral("CourseName")
                                           << QStringLiteral("Caption"));
        const QString subjectCode = jsonString(group, QStringList() << QStringLiteral("CourseCode"));
        const QString teacher = firstTeacherName(group.value(QStringLiteral("Teachers")).toArray());

        const QJsonArray rawExams = group.value(QStringLiteral("Exams")).toArray();
        for (const QJsonValue &examValue : rawExams) {
            const QJsonObject exam = examValue.toObject();
            const QString grade = jsonString(exam, QStringList() << QStringLiteral("Grade"));
            if (!grade.isEmpty())
                continue;
            const QDate date = parseWilmaDate(jsonString(exam, QStringList() << QStringLiteral("Date")));
            const QString iso = date.isValid() ? date.toString(QStringLiteral("yyyy-MM-dd")) : QString();
            if (!iso.isEmpty() && iso < today)
                continue;
            QVariantMap item;
            item.insert(QStringLiteral("id"), jsonInt(exam, QStringLiteral("Id")));
            item.insert(QStringLiteral("date"), iso);
            item.insert(QStringLiteral("dateLabel"), formatDisplayDate(date));
            item.insert(QStringLiteral("name"),
                        jsonString(exam, QStringList() << QStringLiteral("Caption")
                                                     << QStringLiteral("Name")));
            item.insert(QStringLiteral("subject"), subject);
            item.insert(QStringLiteral("subjectCode"), subjectCode);
            item.insert(QStringLiteral("topic"),
                        jsonString(exam, QStringList() << QStringLiteral("Topic")));
            item.insert(QStringLiteral("teacher"), teacher);
            exams.append(item);
        }

        const QJsonArray rawHomework = group.value(QStringLiteral("Homework")).toArray();
        for (const QJsonValue &hwValue : rawHomework) {
            const QJsonObject hw = hwValue.toObject();
            QString text = jsonString(hw, QStringList() << QStringLiteral("Homework"));
            text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
            text = text.trimmed();
            if (text.isEmpty())
                continue;
            const QDate date = parseWilmaDate(jsonString(hw, QStringList() << QStringLiteral("Date")));
            QVariantMap item;
            item.insert(QStringLiteral("date"),
                        date.isValid() ? date.toString(QStringLiteral("yyyy-MM-dd"))
                                       : jsonString(hw, QStringList() << QStringLiteral("Date")));
            item.insert(QStringLiteral("dateLabel"), formatDisplayDate(date));
            item.insert(QStringLiteral("subject"), subject);
            item.insert(QStringLiteral("subjectCode"), subjectCode);
            item.insert(QStringLiteral("homework"), text);
            item.insert(QStringLiteral("teacher"), teacher);
            homework.append(item);
        }
    }

    QVariantList grades;
    const QJsonArray rawGrades = obj.value(QStringLiteral("Exams")).toArray();
    for (const QJsonValue &examValue : rawGrades) {
        const QJsonObject exam = examValue.toObject();
        const QString grade = jsonString(exam, QStringList() << QStringLiteral("Grade"));
        if (grade.isEmpty())
            continue;
        const QDate date = parseWilmaDate(jsonString(exam, QStringList() << QStringLiteral("Date")));
        QVariantMap item;
        item.insert(QStringLiteral("id"), jsonInt(exam, QStringLiteral("ExamId")));
        if (item.value(QStringLiteral("id")).toInt() <= 0)
            item.insert(QStringLiteral("id"), jsonInt(exam, QStringLiteral("Id")));
        item.insert(QStringLiteral("date"),
                    date.isValid() ? date.toString(QStringLiteral("yyyy-MM-dd")) : QString());
        item.insert(QStringLiteral("dateLabel"), formatDisplayDate(date));
        item.insert(QStringLiteral("name"),
                    jsonString(exam, QStringList() << QStringLiteral("Name")));
        item.insert(QStringLiteral("subject"),
                    jsonString(exam, QStringList() << QStringLiteral("CourseTitle")
                                                 << QStringLiteral("Course")));
        item.insert(QStringLiteral("grade"), grade);
        item.insert(QStringLiteral("info"),
                    jsonString(exam, QStringList() << QStringLiteral("Info")));
        item.insert(QStringLiteral("teacher"),
                    firstTeacherName(exam.value(QStringLiteral("Teachers")).toArray()));
        grades.append(item);
    }

    if (m_schedule != schedule) {
        m_schedule = schedule;
        emit scheduleChanged();
    }
    if (m_exams != exams) {
        m_exams = exams;
        emit examsChanged();
    }
    if (m_homework != homework) {
        m_homework = homework;
        emit homeworkChanged();
    }
    if (m_grades != grades) {
        m_grades = grades;
        emit gradesChanged();
    }

    QVariantList gradeHits;
    for (const QVariant &entry : m_grades) {
        const QVariantMap map = entry.toMap();
        QString key = map.value(QStringLiteral("id")).toString();
        if (key.isEmpty() || key == QLatin1String("0")) {
            key = map.value(QStringLiteral("date")).toString()
                    + QLatin1Char('|') + map.value(QStringLiteral("subject")).toString()
                    + QLatin1Char('|') + map.value(QStringLiteral("grade")).toString();
        }
        QVariantMap hit;
        hit.insert(QStringLiteral("key"), key);
        hit.insert(QStringLiteral("title"), QStringLiteral("New grade"));
        hit.insert(QStringLiteral("body"),
                   QStringLiteral("%1 %2").arg(map.value(QStringLiteral("subject")).toString(),
                                               map.value(QStringLiteral("grade")).toString()).trimmed());
        gradeHits.append(hit);
    }
    absorbFresh(QStringLiteral("grades"), gradeHits);

    QVariantList homeworkHits;
    for (const QVariant &entry : m_homework) {
        const QVariantMap map = entry.toMap();
        const QString key = map.value(QStringLiteral("date")).toString()
                + QLatin1Char('|') + map.value(QStringLiteral("subject")).toString()
                + QLatin1Char('|') + map.value(QStringLiteral("homework")).toString();
        QVariantMap hit;
        hit.insert(QStringLiteral("key"), key);
        hit.insert(QStringLiteral("title"), QStringLiteral("New homework"));
        hit.insert(QStringLiteral("body"), map.value(QStringLiteral("subject")).toString());
        homeworkHits.append(hit);
    }
    absorbFresh(QStringLiteral("homework"), homeworkHits);

    QVariantList examHits;
    for (const QVariant &entry : m_exams) {
        const QVariantMap map = entry.toMap();
        QString key = map.value(QStringLiteral("id")).toString();
        if (key.isEmpty() || key == QLatin1String("0")) {
            key = map.value(QStringLiteral("date")).toString()
                    + QLatin1Char('|') + map.value(QStringLiteral("name")).toString()
                    + QLatin1Char('|') + map.value(QStringLiteral("subject")).toString();
        }
        QVariantMap hit;
        hit.insert(QStringLiteral("key"), key);
        hit.insert(QStringLiteral("title"), QStringLiteral("New exam"));
        hit.insert(QStringLiteral("body"), map.value(QStringLiteral("name")).toString().isEmpty()
                   ? map.value(QStringLiteral("subject")).toString()
                   : map.value(QStringLiteral("name")).toString());
        examHits.append(hit);
    }
    absorbFresh(QStringLiteral("exams"), examHits);

    if (m_pollMode == QLatin1String("schoolday") && m_loggedIn)
        armPollTimer();

    const QVariantList overviewNotes = lessonNotesFromJsonDocument(QJsonDocument(obj));
    if (!overviewNotes.isEmpty() && m_lessonNotes.isEmpty())
        applyLessonNotes(overviewNotes);
}

void WilmaClient::applyNewsList(const QByteArray &body)
{
    QVariantList items;
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    QJsonArray list;
    if (doc.isArray()) {
        list = doc.array();
    } else if (doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.value(QStringLiteral("payload")).isObject())
            obj = obj.value(QStringLiteral("payload")).toObject();
        if (obj.value(QStringLiteral("News")).isArray())
            list = obj.value(QStringLiteral("News")).toArray();
        else if (obj.value(QStringLiteral("news")).isArray())
            list = obj.value(QStringLiteral("news")).toArray();
    }

    if (!list.isEmpty()) {
        for (const QJsonValue &value : list) {
            const QJsonObject obj = value.toObject();
            int id = jsonInt(obj, QStringLiteral("Id"));
            if (id <= 0)
                id = jsonInt(obj, QStringLiteral("id"));
            if (id <= 0)
                continue;
            QVariantMap item;
            item.insert(QStringLiteral("id"), id);
            item.insert(QStringLiteral("title"),
                        jsonString(obj, QStringList() << QStringLiteral("Title")
                                                     << QStringLiteral("title")));
            item.insert(QStringLiteral("subtitle"),
                        jsonString(obj, QStringList() << QStringLiteral("Subtitle")
                                                     << QStringLiteral("subtitle")));
            item.insert(QStringLiteral("author"),
                        jsonString(obj, QStringList() << QStringLiteral("Author")
                                                     << QStringLiteral("author")));
            const QDateTime published = parseWilmaDateTime(firstJsonValue(obj, QStringList()
                        << QStringLiteral("Published")
                        << QStringLiteral("published")
                        << QStringLiteral("Date")
                        << QStringLiteral("date")));
            item.insert(QStringLiteral("published"), formatDisplayDateTime(published));
            items.append(item);
        }
    } else {
        const QString html = QString::fromUtf8(body);
        QRegularExpression linkRe(QStringLiteral("<a[^>]+href=['\"][^'\"]*/news/(\\d+)[^'\"]*['\"][^>]*>([^<]+)</a>"),
                                  QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatchIterator it = linkRe.globalMatch(html);
        QSet<int> seen;
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            const int id = match.captured(1).toInt();
            if (id <= 0 || seen.contains(id))
                continue;
            seen.insert(id);
            QVariantMap item;
            item.insert(QStringLiteral("id"), id);
            item.insert(QStringLiteral("title"), htmlToText(match.captured(2)));
            item.insert(QStringLiteral("subtitle"), QString());
            item.insert(QStringLiteral("author"), QString());
            item.insert(QStringLiteral("published"), QString());
            items.append(item);
        }
    }

    if (items != m_news) {
        m_news = items;
        emit newsChanged();
    }

    QVariantList hits;
    for (const QVariant &entry : m_news) {
        const QVariantMap map = entry.toMap();
        const QString key = map.value(QStringLiteral("id")).toString();
        QVariantMap hit;
        hit.insert(QStringLiteral("key"), key);
        hit.insert(QStringLiteral("title"), QStringLiteral("New announcement"));
        hit.insert(QStringLiteral("body"), map.value(QStringLiteral("title")).toString());
        hits.append(hit);
    }
    absorbFresh(QStringLiteral("news"), hits);
}

void WilmaClient::applyAttendance(const QByteArray &body, const QString &contentType)
{
    QVariantList items;
    const bool json = contentType.contains(QStringLiteral("json"), Qt::CaseInsensitive)
            || (!looksLikeHtml(body) && (body.trimmed().startsWith('{')
                                        || body.trimmed().startsWith('[')));
    if (json)
        items = lessonNotesFromJsonDocument(QJsonDocument::fromJson(body));
    if (items.isEmpty())
        items = parseAttendanceHtml(QString::fromUtf8(body));
    if (items.isEmpty())
        return;
    applyLessonNotes(items);
}

void WilmaClient::applyLessonNotes(const QVariantList &items)
{
    QSet<QString> seen;
    QVariantList merged;
    const QVariantList sources[2] = { m_lessonNotes, items };
    for (int s = 0; s < 2; ++s) {
        for (const QVariant &entry : sources[s]) {
            const QVariantMap map = entry.toMap();
            const QString key = QStringLiteral("%1|%2|%3|%4")
                    .arg(map.value(QStringLiteral("date")).toString(),
                         map.value(QStringLiteral("start")).toString(),
                         map.value(QStringLiteral("typeLabel")).toString(),
                         map.value(QStringLiteral("subject")).toString());
            if (seen.contains(key))
                continue;
            seen.insert(key);
            merged.append(entry);
        }
    }

    for (int i = 0; i < merged.size(); ++i) {
        for (int j = i + 1; j < merged.size(); ++j) {
            const QVariantMap left = merged.at(i).toMap();
            const QVariantMap right = merged.at(j).toMap();
            const QString leftDate = left.value(QStringLiteral("date")).toString();
            const QString rightDate = right.value(QStringLiteral("date")).toString();
            const bool swap = (rightDate > leftDate)
                    || (rightDate == leftDate
                        && right.value(QStringLiteral("start")).toString()
                        > left.value(QStringLiteral("start")).toString());
            if (swap)
                qSwap(merged[i], merged[j]);
        }
    }

    if (merged != m_lessonNotes) {
        m_lessonNotes = merged;
        emit lessonNotesChanged();
    }

    QVariantList hits;
    for (const QVariant &entry : m_lessonNotes) {
        const QVariantMap map = entry.toMap();
        const QString key = QStringLiteral("%1|%2|%3|%4")
                .arg(map.value(QStringLiteral("date")).toString(),
                     map.value(QStringLiteral("start")).toString(),
                     map.value(QStringLiteral("typeLabel")).toString(),
                     map.value(QStringLiteral("subject")).toString());
        QVariantMap hit;
        hit.insert(QStringLiteral("key"), key);
        hit.insert(QStringLiteral("title"), QStringLiteral("New lesson note"));
        const QString label = map.value(QStringLiteral("typeLabel")).toString();
        const QString subject = map.value(QStringLiteral("subject")).toString();
        hit.insert(QStringLiteral("body"), subject.isEmpty() ? label
                                                             : (label.isEmpty() ? subject
                                                                                : label + QStringLiteral(" · ") + subject));
        hits.append(hit);
    }
    absorbFresh(QStringLiteral("notes"), hits);
}

void WilmaClient::applyMessageDetail(int messageId, const QByteArray &body, const QString &contentType)
{
    QVariantMap item;
    item.insert(QStringLiteral("id"), messageId);
    const bool json = contentType.contains(QStringLiteral("json"), Qt::CaseInsensitive)
            || (!looksLikeHtml(body) && body.trimmed().startsWith('{'));
    if (json) {
        const QJsonObject obj = QJsonDocument::fromJson(body).object();
        const QJsonObject payload = obj.contains(QStringLiteral("payload"))
                ? obj.value(QStringLiteral("payload")).toObject()
                : obj;
        item.insert(QStringLiteral("subject"),
                    jsonString(payload, QStringList() << QStringLiteral("Subject")
                                                     << QStringLiteral("subject")));
        item.insert(QStringLiteral("sender"),
                    jsonString(payload, QStringList() << QStringLiteral("Sender")
                                                     << QStringLiteral("sender")
                                                     << QStringLiteral("SenderName")));
        const QDateTime sent = parseWilmaDateTime(firstJsonValue(payload, QStringList()
                    << QStringLiteral("TimeStamp")
                    << QStringLiteral("Timestamp")
                    << QStringLiteral("Time")
                    << QStringLiteral("Date")));
        item.insert(QStringLiteral("time"), formatDisplayDateTime(sent));
        const QString content = jsonString(payload, QStringList()
                                           << QStringLiteral("Content")
                                           << QStringLiteral("content")
                                           << QStringLiteral("Body")
                                           << QStringLiteral("body"));
        item.insert(QStringLiteral("content"), htmlToText(content));
    } else {
        const QString html = QString::fromUtf8(body);
        QRegularExpression titleRe(QStringLiteral("<h1[^>]*>([\\s\\S]*?)</h1>"),
                                   QRegularExpression::CaseInsensitiveOption);
        item.insert(QStringLiteral("subject"), htmlToText(titleRe.match(html).captured(1)));

        QRegularExpression senderRe(QStringLiteral("(?:Lähettäjä|Sender)</th>\\s*<td[^>]*>([\\s\\S]*?)</td>"),
                                    QRegularExpression::CaseInsensitiveOption);
        item.insert(QStringLiteral("sender"), htmlToText(senderRe.match(html).captured(1)));

        QRegularExpression sentRe(QStringLiteral("(?:Lähetetty|Sent)</th>\\s*<td[^>]*>([\\s\\S]*?)</td>"),
                                  QRegularExpression::CaseInsensitiveOption);
        item.insert(QStringLiteral("time"), htmlToText(sentRe.match(html).captured(1)));

        QString content;
        QRegularExpression hiddenRe(QStringLiteral("<div[^>]*class=['\"][^'\"]*ckeditor[^'\"]*['\"][^>]*>([\\s\\S]*?)</div>"),
                                    QRegularExpression::CaseInsensitiveOption);
        content = htmlToText(hiddenRe.match(html).captured(1));
        if (content.isEmpty()) {
            QRegularExpression innerRe(QStringLiteral("<div[^>]*class=['\"][^'\"]*inner[^'\"]*['\"][^>]*>([\\s\\S]*?)</div>"),
                                       QRegularExpression::CaseInsensitiveOption);
            content = htmlToText(innerRe.match(html).captured(1));
        }
        if (content.isEmpty()) {
            QRegularExpression areaRe(QStringLiteral("id=['\"]page-content-area['\"][^>]*>([\\s\\S]*?)</div>"),
                                      QRegularExpression::CaseInsensitiveOption);
            content = htmlToText(areaRe.match(html).captured(1));
        }
        item.insert(QStringLiteral("content"), content);
    }

    if (item.value(QStringLiteral("subject")).toString().isEmpty()) {
        for (const QVariant &entry : m_messages) {
            const QVariantMap listed = entry.toMap();
            if (listed.value(QStringLiteral("id")).toInt() == messageId) {
                if (item.value(QStringLiteral("subject")).toString().isEmpty())
                    item.insert(QStringLiteral("subject"), listed.value(QStringLiteral("subject")));
                if (item.value(QStringLiteral("sender")).toString().isEmpty())
                    item.insert(QStringLiteral("sender"), listed.value(QStringLiteral("sender")));
                if (item.value(QStringLiteral("time")).toString().isEmpty())
                    item.insert(QStringLiteral("time"), listed.value(QStringLiteral("time")));
                break;
            }
        }
    }
    if (item.value(QStringLiteral("content")).toString().isEmpty())
        item.insert(QStringLiteral("content"),
                    QStringLiteral("Could not load this message. Open the Wilma site from the menu."));

    m_currentMessage = item;
    emit currentMessageChanged();
}

void WilmaClient::applyNewsDetail(int newsId, const QByteArray &body, const QString &contentType)
{
    QVariantMap item;
    item.insert(QStringLiteral("id"), newsId);
    const bool json = contentType.contains(QStringLiteral("json"), Qt::CaseInsensitive)
            || (!looksLikeHtml(body) && (body.trimmed().startsWith('{') || body.trimmed().startsWith('[')));
    if (json) {
        QJsonObject obj = QJsonDocument::fromJson(body).object();
        if (obj.contains(QStringLiteral("payload")))
            obj = obj.value(QStringLiteral("payload")).toObject();
        item.insert(QStringLiteral("title"),
                    jsonString(obj, QStringList() << QStringLiteral("Title")
                                                 << QStringLiteral("title")));
        item.insert(QStringLiteral("subtitle"),
                    jsonString(obj, QStringList() << QStringLiteral("Subtitle")
                                                 << QStringLiteral("subtitle")));
        item.insert(QStringLiteral("author"),
                    jsonString(obj, QStringList() << QStringLiteral("Author")
                                                 << QStringLiteral("author")));
        const QDateTime published = parseWilmaDateTime(firstJsonValue(obj, QStringList()
                    << QStringLiteral("Published")
                    << QStringLiteral("published")
                    << QStringLiteral("Date")));
        item.insert(QStringLiteral("published"), formatDisplayDateTime(published));
        const QString content = jsonString(obj, QStringList()
                                           << QStringLiteral("Content")
                                           << QStringLiteral("content")
                                           << QStringLiteral("Body")
                                           << QStringLiteral("body"));
        item.insert(QStringLiteral("content"), htmlToText(content));
    } else {
        const QString html = QString::fromUtf8(body);
        QRegularExpression titleRe(QStringLiteral("<h1[^>]*>([\\s\\S]*?)</h1>"),
                                   QRegularExpression::CaseInsensitiveOption);
        QString title = htmlToText(titleRe.match(html).captured(1));
        if (title.isEmpty()) {
            QRegularExpression docTitle(QStringLiteral("<title>([\\s\\S]*?)</title>"),
                                        QRegularExpression::CaseInsensitiveOption);
            title = htmlToText(docTitle.match(html).captured(1));
            if (title.endsWith(QStringLiteral(" - Wilma")))
                title.chop(8);
        }
        item.insert(QStringLiteral("title"), title.trimmed());

        QRegularExpression subRe(QStringLiteral("<p[^>]*class=['\"][^'\"]*sub-text[^'\"]*['\"][^>]*>([\\s\\S]*?)</p>"),
                                 QRegularExpression::CaseInsensitiveOption);
        item.insert(QStringLiteral("subtitle"), htmlToText(subRe.match(html).captured(1)));
        item.insert(QStringLiteral("author"), QString());
        item.insert(QStringLiteral("published"), QString());

        QString content;
        QRegularExpression newsRe(QStringLiteral("id=['\"]news-content['\"][^>]*>([\\s\\S]*?)</div>"),
                                  QRegularExpression::CaseInsensitiveOption);
        content = htmlToText(newsRe.match(html).captured(1));
        if (content.isEmpty()) {
            QRegularExpression areaRe(QStringLiteral("id=['\"]page-content-area['\"][^>]*>([\\s\\S]*?)</div>"),
                                      QRegularExpression::CaseInsensitiveOption);
            content = htmlToText(areaRe.match(html).captured(1));
        }
        item.insert(QStringLiteral("content"), content);
    }

    if (item.value(QStringLiteral("title")).toString().isEmpty()) {
        for (const QVariant &entry : m_news) {
            const QVariantMap listed = entry.toMap();
            if (listed.value(QStringLiteral("id")).toInt() == newsId) {
                item.insert(QStringLiteral("title"), listed.value(QStringLiteral("title")));
                if (item.value(QStringLiteral("subtitle")).toString().isEmpty())
                    item.insert(QStringLiteral("subtitle"), listed.value(QStringLiteral("subtitle")));
                if (item.value(QStringLiteral("published")).toString().isEmpty())
                    item.insert(QStringLiteral("published"), listed.value(QStringLiteral("published")));
                break;
            }
        }
    }
    if (item.value(QStringLiteral("content")).toString().isEmpty())
        item.insert(QStringLiteral("content"),
                    QStringLiteral("Could not load this bulletin. Open the Wilma site from the menu."));

    m_currentNews = item;
    emit currentNewsChanged();
}
