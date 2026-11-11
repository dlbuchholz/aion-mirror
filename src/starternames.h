#pragma once

// The account types and categories a new user starts with are shown in the language of the
// texts, and follow it when that changes, until the user renames them; from then on they're the
// user's own. Their English name is kept in the "starter" column for that.
namespace StarterNames {

// Gives every user's starters the name in the current language. One that would take the name of
// another of the user's own keeps its name. Returns false when the database couldn't be updated.
bool followLanguage();

} // namespace StarterNames
