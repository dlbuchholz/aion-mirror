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

// In hue order, so similar colours sit together in the picker. Stored by name, so the order can change.
const QList<QPair<QString, Ramp>>& ramps() {
    static const QList<QPair<QString, Ramp>> list = {
        {"red",     {QColor("#FCEBEB"), QColor("#F7C1C1"), QColor("#E24B4A"), QColor("#791F1F")}},
        {"coral",   {QColor("#FAECE7"), QColor("#F5C4B3"), QColor("#D85A30"), QColor("#712B13")}},
        {"orange",  {QColor("#FDEEDF"), QColor("#F8C79A"), QColor("#E5761F"), QColor("#6E330A")}},
        {"amber",   {QColor("#FAEEDA"), QColor("#FAC775"), QColor("#BA7517"), QColor("#633806")}},
        {"yellow",  {QColor("#FBF6D9"), QColor("#F2E08A"), QColor("#C9A80A"), QColor("#5C4B04")}},
        {"lime",    {QColor("#F1F7DC"), QColor("#D4E79A"), QColor("#8DB314"), QColor("#415508")}},
        {"green",   {QColor("#EAF3DE"), QColor("#C0DD97"), QColor("#639922"), QColor("#27500A")}},
        {"olive",   {QColor("#F1F1E2"), QColor("#D5D5A8"), QColor("#7F7F2A"), QColor("#3E3E12")}},
        {"teal",    {QColor("#E1F5EE"), QColor("#9FE1CB"), QColor("#1D9E75"), QColor("#085041")}},
        {"sky",     {QColor("#E3F4FA"), QColor("#A8DCEE"), QColor("#1E9CC8"), QColor("#0B4A61")}},
        {"blue",    {QColor("#E6F1FB"), QColor("#B5D4F4"), QColor("#378ADD"), QColor("#0C447C")}},
        {"indigo",  {QColor("#E8EAFB"), QColor("#C3C8F2"), QColor("#4F5BD5"), QColor("#26307A")}},
        {"purple",  {QColor("#EEEDFE"), QColor("#CECBF6"), QColor("#7F77DD"), QColor("#3C3489")}},
        {"magenta", {QColor("#FAE8F8"), QColor("#EDB8E6"), QColor("#B83DAA"), QColor("#5E1A57")}},
        {"pink",    {QColor("#FBEAF0"), QColor("#F4C0D1"), QColor("#D4537E"), QColor("#72243E")}},
        {"brown",   {QColor("#F3ECE6"), QColor("#D9C4B3"), QColor("#8B5E3C"), QColor("#47301D")}},
        {"slate",   {QColor("#EAEFF3"), QColor("#C3CFDA"), QColor("#5E7387"), QColor("#2B3946")}},
        {"gray",    {QColor("#F1EFE8"), QColor("#D3D1C7"), QColor("#888780"), QColor("#444441")}},
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
