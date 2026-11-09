#pragma once

// 校验层：全部口径严格对齐服务端 service/Validation.cpp 与 common/Util.cpp，
// 目的不是"更严格"，而是让前端在提交前给出与服务端完全一致的判断结果，
// 避免出现"前端通过、服务端 400"的体验断层。
//
// 服务端口径摘要：
//  - username：先 trimUnicode，再要求 4-20 个 ASCII 字母/数字/下划线；
//  - password：不 trim，按 Unicode 码点计数，8-64；
//  - 其它文本字段：先 trimUnicode，再按码点计数并校验区间（trim 后长度）；
//  - 所有文本禁止 NUL 字符（Qt 的 QString 天然不含 U+0000 结尾以外的 NUL，
//    但用户可从剪贴板粘贴 U+0000，这里显式拦截）。

#include <QString>

namespace bbs {
namespace validators {

// 校验结果：ok 为 false 时 error 可直接展示给用户；normalized 为应提交给服务端的值
struct Check
{
    bool ok = false;
    QString normalized;
    QString error;

    static Check pass(const QString& value)
    {
        Check result;
        result.ok = true;
        result.normalized = value;
        return result;
    }
    static Check fail(const QString& message)
    {
        Check result;
        result.ok = false;
        result.error = message;
        return result;
    }
};

// 与 Util::codePointCount 等价的码点计数
int codePointCount(const QString& value);

// 与 Util::trimUnicode 等价的去空白（首尾 Unicode 空白）
QString trimUnicode(const QString& value);

// 通用文本字段校验（对应 validation::text）
Check text(const QString& value, const QString& field, int minLength, int maxLength);

// 用户名（对应 validation::username）
Check username(const QString& value);

// 密码（对应 validation::password，注意：不 trim）
Check password(const QString& value, const QString& field);

// 登录时用于本地前置提示：密码只校验非空，长度交由服务端判定
// （登录接口同样会执行 password 校验，但本地不必因长度不符而阻断，
//  以免用户被"密码长度必须为 8-64 个字符"误导为账号信息错误）
Check loginPassword(const QString& value);

// 具体业务字段的便捷入口
Check nickname(const QString& value);           // 1-30
Check boardName(const QString& value);          // 1-30
Check boardDescription(const QString& value);   // 0-200
Check postTitle(const QString& value);          // 1-100
Check postContent(const QString& value);        // 1-5000
Check replyContent(const QString& value);       // 1-1000
Check reason(const QString& value);             // 1-200
Check keyword(const QString& value);            // 0-100

// 日期：yyyy-MM-dd 且为真实公历日期（对应 Util::validDate）
bool isValidDate(const QString& value);

} // namespace validators
} // namespace bbs
