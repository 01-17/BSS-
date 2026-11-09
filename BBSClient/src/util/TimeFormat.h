#pragma once

// 时间展示层：服务端所有时间字段都是 UTC 的 "yyyy-MM-ddTHH:mm:ssZ"。
// 前端一律"原样保存、展示时转本地"，不做写回，避免时区被反复换算产生漂移。

#include <QDateTime>
#include <QString>

namespace bbs {
namespace timefmt {

// 统一展示格式：yyyy-MM-dd HH:mm（本地时区）
QString toLocalDisplay(const QString& utcIso);

// 解析 UTC ISO 字符串；失败返回无效 QDateTime
QDateTime parseUtc(const QString& utcIso);

// 当前本地时间（同展示格式），用于"最后刷新"等辅助信息
QString nowLocalDisplay();

} // namespace timefmt
} // namespace bbs
