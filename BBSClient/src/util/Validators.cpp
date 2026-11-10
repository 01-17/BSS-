#include "util/Validators.h"

#include <QDate>
#include <QVector>

namespace bbs {
namespace validators {

namespace {

bool isAsciiUsernameChar(QChar ch)
{
    const ushort code = ch.unicode();
    const bool lower = code >= 'a' && code <= 'z';
    const bool upper = code >= 'A' && code <= 'Z';
    const bool digit = code >= '0' && code <= '9';
    return lower || upper || digit || code == '_';
}

bool containsNul(const QString& value)
{
    for (int i = 0; i < value.size(); ++i) {
        if (value.at(i).unicode() == 0)
            return true;
    }
    return false;
}

} // namespace

int codePointCount(const QString& value)
{
    return value.toUcs4().size();
}

QString trimUnicode(const QString& value)
{
    int begin = 0;
    int end = value.size();
    while (begin < end && value.at(begin).isSpace())
        ++begin;
    while (end > begin && value.at(end - 1).isSpace())
        --end;
    return value.mid(begin, end - begin);
}

Check text(const QString& value, const QString& field, int minLength, int maxLength)
{
    if (containsNul(value))
        return Check::fail(QStringLiteral("%1包含非法字符").arg(field));

    const QString normalized = trimUnicode(value);
    const int length = codePointCount(normalized);
    if (length < minLength || length > maxLength) {
        if (minLength > 0) {
            return Check::fail(
                QStringLiteral("%1长度必须为 %2-%3 个字符").arg(field).arg(minLength).arg(maxLength));
        }
        return Check::fail(QStringLiteral("%1长度不能超过 %2 个字符").arg(field).arg(maxLength));
    }
    return Check::pass(normalized);
}

Check username(const QString& value)
{
    if (containsNul(value))
        return Check::fail(QStringLiteral("用户名包含非法字符"));

    const QString normalized = trimUnicode(value);
    const int length = codePointCount(normalized);
    if (length < 4 || length > 20)
        return Check::fail(QStringLiteral("用户名长度必须为 4-20 个字符"));

    for (int i = 0; i < normalized.size(); ++i) {
        if (!isAsciiUsernameChar(normalized.at(i)))
            return Check::fail(QStringLiteral("用户名只能包含字母、数字和下划线"));
    }
    return Check::pass(normalized);
}

Check password(const QString& value, const QString& field)
{
    if (containsNul(value))
        return Check::fail(QStringLiteral("%1包含非法字符").arg(field));

    const int length = codePointCount(value);
    if (length < 8 || length > 64)
        return Check::fail(QStringLiteral("%1长度必须为 8-64 个字符").arg(field));
    return Check::pass(value);
}

Check loginPassword(const QString& value)
{
    if (value.isEmpty())
        return Check::fail(QStringLiteral("请输入密码"));
    return Check::pass(value);
}

Check nickname(const QString& value)
{
    return text(value, QStringLiteral("昵称"), 1, 30);
}

Check boardName(const QString& value)
{
    return text(value, QStringLiteral("版块名称"), 1, 30);
}

Check boardDescription(const QString& value)
{
    return text(value, QStringLiteral("版块描述"), 0, 200);
}

Check postTitle(const QString& value)
{
    return text(value, QStringLiteral("帖子标题"), 1, 100);
}

Check postContent(const QString& value)
{
    return text(value, QStringLiteral("帖子内容"), 1, 5000);
}

Check replyContent(const QString& value)
{
    return text(value, QStringLiteral("回复内容"), 1, 1000);
}

Check reason(const QString& value)
{
    return text(value, QStringLiteral("操作理由"), 1, 200);
}

Check keyword(const QString& value)
{
    return text(value, QStringLiteral("搜索关键词"), 0, 100);
}

bool isValidDate(const QString& value)
{
    // 服务端只接受 yyyy-MM-dd 且必须能构造出真实日期
    if (value.size() != 10)
        return false;
    if (value.at(4) != QLatin1Char('-') || value.at(7) != QLatin1Char('-'))
        return false;
    for (int i = 0; i < value.size(); ++i) {
        if (i == 4 || i == 7)
            continue;
        if (!value.at(i).isDigit())
            return false;
    }
    const QDate date = QDate::fromString(value, QStringLiteral("yyyy-MM-dd"));
    return date.isValid();
}

} // namespace validators
} // namespace bbs
