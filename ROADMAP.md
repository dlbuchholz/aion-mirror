# Roadmap

- Add encrypted backups and reliable restore.
- Investigate at-rest encryption
- Strengthen transaction and account validation and reconciliation.
- Add recurring transactions.
- Improve budgets and budget-vs-actual views.
- Add spending and cash-flow reports.
- Improve CSV import/export and duplicate handling.
- Improve usability and accessibility.
- Investigate bank synchronization.
- Continue expanding tests and CI checks.
- Add Linux and macOS packages (see [Builds](#builds) below).
- Investigate an iPhone version

## Transactions

- Transfers between your own accounts that don't count as income, spending or towards budgets: a
  built-in "Transfer" type, or categories you can mark like that.
- More filters: a specific month (for example June), and income or expenses only.
- show expenses in red instead of the neutral colour.

## Accounts and users

- Delete accounts, either with all their transactions or after moving them to another account.
- Icons for account types (checking, savings, credit card, …).
- Change a user's name and username.

## Budget

- Budget rows and "Spending without a budget" show the month's transactions of that category, or
  link to them.

## Categories

- Icons for categories.

## Look and feel

- Logo: does the main window and start window need it in the title bar when the sidebar already shows
  it?
- About: the name and version next to the logo repeat what the technical information below says.
- Accessibility settings, for example UI scaling.
- Translations: English and German, following the system language by default, changeable in Settings.

## Builds

Windows is built, tested, packaged (zip and installer) and released by CI; see
[CONTRIBUTING.md](CONTRIBUTING.md#builds-and-releases).

- Linux AppImage: needs a second build image on an older base (Ubuntu 22.04) with the official Qt
  and `linuxdeploy`, so it runs on older distributions too.
- macOS: a `.app` in a `.dmg`, built on a Mac as a second runner.
- Add `mingw32-nsis` to the build image, so the Windows job doesn't install it on every run.
- Smaller Windows build: ICU is 37 of about 80 MB. Fedora's Qt needs it; Qt's official Windows build
  doesn't use it. Options: build with the official Qt (made with another MinGW version, so test
  well), build Qt without ICU, or build a smaller ICU data file with only the languages Aion needs.
  Leaving out unused image format plugins would save a few MB more.

## Later

- Contracts (phone plans, insurance, …): when they started, when they probably end and what they
  charge per interval, so Aion can expect those transactions. Budgets could take them into account.
  After rules and Reports.