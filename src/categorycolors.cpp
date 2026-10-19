#include "categorycolors.h"
#include "theme.h"

#include <QHash>

namespace CategoryColors {

namespace {

// Light fill, mid tone and dark text of each colour ramp
struct Ramp {
    QColor light;
    QColor softer;                                   // Text on the dark tag
    QColor mid;
    QColor dark;
};

const QList<QPair<QString, Ramp>>& ramps() {
    static const QList<QPair<QString, Ramp>> list = {
        {"blue",   {QColor("#E6F1FB"), QColor("#B5D4F4"), QColor("#378ADD"), QColor("#0C447C")}},
        {"teal",   {QColor("#E1F5EE"), QColor("#9FE1CB"), QColor("#1D9E75"), QColor("#085041")}},
        {"green",  {QColor("#EAF3DE"), QColor("#C0DD97"), QColor("#639922"), QColor("#27500A")}},
        {"amber",  {QColor("#FAEEDA"), QColor("#FAC775"), QColor("#BA7517"), QColor("#633806")}},
        {"coral",  {QColor("#FAECE7"), QColor("#F5C4B3"), QColor("#D85A30"), QColor("#712B13")}},
        {"red",    {QColor("#FCEBEB"), QColor("#F7C1C1"), QColor("#E24B4A"), QColor("#791F1F")}},
        {"pink",   {QColor("#FBEAF0"), QColor("#F4C0D1"), QColor("#D4537E"), QColor("#72243E")}},
        {"purple", {QColor("#EEEDFE"), QColor("#CECBF6"), QColor("#7F77DD"), QColor("#3C3489")}},
        {"gray",   {QColor("#F1EFE8"), QColor("#D3D1C7"), QColor("#888780"), QColor("#444441")}},
    };
    return list;
}

} // namespace

QStringList names() {
    QStringList result;
    for (const auto& ramp : ramps()) result << ramp.first;
    return result;
}

QString label(const QString& name) {
    if (name.isEmpty()) return "No colour";
    return name.left(1).toUpper() + name.mid(1);
}

Swatch swatch(const QString& name) {
    const Theme::Colors colors = Theme::colors();
    for (const auto& ramp : ramps()) {
        if (ramp.first == name) {
            const Ramp& r = ramp.second;
            return Theme::isDark() ? Swatch{r.dark, r.softer, r.mid} : Swatch{r.light, r.dark, r.mid};
        }
    }
    return {colors.track, colors.textSecondary, colors.textMuted};
}

} // namespace CategoryColors
