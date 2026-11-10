#include "util/TimeFormat.h"

#include <QTimeZone>

namespace bbs {
namespace timefmt {

QDateTime parseUtc(const QString& utcIso)
{
    const QString trimmed = utcIso.trimmed();
    if (trimmed.isEmpty())
        return QDateTime();

    // Qt::ISODate 能识别结尾的 Z（Qt5/Qt6 均如此），解析结果仍带 UTC 时区；
    // 为稳妥起见显式指定 UTC，避免宿主系统时区把 Z 误当本地时间。
    QDateTime parsed = QDateTime::fromString(trimmed, Qt::ISODate);
    if (!parsed.isValid())
        parsed = QDateTime::fromString(trimmed, QStringLiteral("yyyy-MM-ddTHH:mm:ssZ"));
    if (!parsed.isValid())
        parsed = QDateTime::fromString(trimmed, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!parsed.isValid())
        return QDateTime();

    parsed.setTimeSpec(Qt::UTC);
    return parsed;
}

QString toLocalDisplay(const QString& utcIso)
{
    if (utcIso.trimmed().isEmpty())
        return QStringLiteral("—");

    const QDateTime utc = parseUtc(utcIso);
    if (!utc.isValid())
        return utcIso.trimmed(); // 无法解析时原样展示，绝不编造时间

    const QDateTime local = utc.toLocalTime();
    return local.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

QString nowLocalDisplay()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

} // namespace timefmt
} // namespace bbs
