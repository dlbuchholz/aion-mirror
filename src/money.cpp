#include "money.h"

#include <QStringList>
#include <cmath>

namespace Money {

Cents fromDouble(double value) {
    return static_cast<Cents>(std::llround(value * 100.0));
}

double toDouble(Cents cents) {
    return static_cast<double>(cents) / 100.0;
}

QString toPlainString(Cents cents) {
    const Cents absolute = cents < 0 ? -cents : cents;
    return QString("%1%2.%3")
        .arg(cents < 0 ? "-" : "")
        .arg(absolute / 100)
        .arg(absolute % 100, 2, 10, QChar('0'));
}

std::optional<Cents> parsePlain(const QString& text) {
    const QStringList parts = text.split('.');
    if (text.isEmpty() || parts.size() > 2) {
        return std::nullopt;
    }
    const QString units = parts[0];
    const QString fraction = parts.size() > 1 ? parts[1] : QString();
    if (units.isEmpty() && fraction.isEmpty()) {
        return std::nullopt;
    }
    for (const QString& part : {units, fraction}) {
        for (QChar ch : part) {
            if (ch < u'0' || ch > u'9') return std::nullopt;  // isDigit() also takes other scripts' digits
        }
    }
    if (units.size() > 15) {
        return std::nullopt;                         // Far beyond any real amount; avoids overflow
    }

    Cents cents = (units.isEmpty() ? 0 : units.toLongLong()) * 100;
    cents += fraction.left(2).leftJustified(2, '0').toInt();
    if (fraction.size() > 2 && fraction[2] >= '5') {
        cents += 1;
    }
    return cents;
}

} // namespace Money
