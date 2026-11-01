<!-- Measured at cc63d816acbe726a2850e6016105f0ed5a3569e2 by scripts/coverage.sh -->
# Test coverage

![Coverage](docs/coverage-badge.svg)

**90%** · 10443 / 11627 lines · 43 test suites · 396 test cases · 2026-11-01 · `cc63d81`

## Files

| File | Coverage | Lines | Covered by |
|---|---:|---:|---|
| [src/main.cpp](src/main.cpp) | 0% | 0 / 60 | *none* |
| [src/updatecheck.cpp](src/updatecheck.cpp) | 0% | 0 / 5 | *none* |
| [src/categorycolors.cpp](src/categorycolors.cpp) | 51% | 20 / 39 | categorysettings, dashboardpage, mainwindow and 4 more |
| [src/db/databasemanager.cpp](src/db/databasemanager.cpp) | 71% | 30 / 42 | aboutwidget, account, addaccountwidget and 33 more |
| [src/mainwindow.cpp](src/mainwindow.cpp) | 72% | 93 / 129 | mainwindow, rendering |
| [src/widgets/reportspage.cpp](src/widgets/reportspage.cpp) | 77% | 906 / 1179 | mainwindow, rendering, reportspage |
| [src/widgets/addaccountwidget.cpp](src/widgets/addaccountwidget.cpp) | 79% | 30 / 38 | addaccountwidget |
| [src/widgets/transactionspage.cpp](src/widgets/transactionspage.cpp) | 82% | 633 / 774 | mainwindow, rendering, transactionspage |
| [src/rule.h](src/rule.h) | 83% | 5 / 6 | applyrulesdialog, balance, csvimport and 4 more |
| [src/transaction.cpp](src/transaction.cpp) | 86% | 236 / 276 | account, addaccountwidget, applyrulesdialog and 22 more |
| [src/db/migrationmanager.cpp](src/db/migrationmanager.cpp) | 87% | 125 / 144 | aboutwidget, account, addaccountwidget and 33 more |
| [src/user.cpp](src/user.cpp) | 87% | 55 / 63 | account, addaccountwidget, applyrulesdialog and 26 more |
| [src/account.cpp](src/account.cpp) | 89% | 225 / 254 | account, addaccountwidget, applyrulesdialog and 25 more |
| [src/commandline.cpp](src/commandline.cpp) | 89% | 17 / 19 | commandline |
| [src/startwindow.cpp](src/startwindow.cpp) | 90% | 274 / 303 | branding, rendering, startwindow |
| [src/balance.cpp](src/balance.cpp) | 91% | 20 / 22 | balance, csvimport, rendering, user_category |
| [src/db/backup.cpp](src/db/backup.cpp) | 91% | 155 / 170 | backup, backupbeforeupdate, databasesession and 5 more |
| [src/widgets/accounttypessettings.cpp](src/widgets/accounttypessettings.cpp) | 91% | 122 / 134 | categorysettings, mainwindow, rendering, settingspage |
| [src/dialog/renamedpayeesdialog.cpp](src/dialog/renamedpayeesdialog.cpp) | 92% | 79 / 86 | rulessettings |
| [src/rule.cpp](src/rule.cpp) | 92% | 424 / 459 | applyrulesdialog, balance, csvimport and 6 more |
| [src/widgets/dashboardpage.cpp](src/widgets/dashboardpage.cpp) | 92% | 474 / 514 | dashboardpage, mainwindow, rendering |
| [src/category.cpp](src/category.cpp) | 93% | 163 / 176 | account, addaccountwidget, applyrulesdialog and 25 more |
| [src/csvimport.cpp](src/csvimport.cpp) | 93% | 287 / 308 | balance, csvimport |
| [src/dialog/deleteaccountdialog.cpp](src/dialog/deleteaccountdialog.cpp) | 93% | 109 / 117 | deleteaccount |
| [src/dialog/importmanagerdialog.cpp](src/dialog/importmanagerdialog.cpp) | 93% | 980 / 1056 | balance, rendering |
| [src/widgets/backupsettings.cpp](src/widgets/backupsettings.cpp) | 93% | 202 / 218 | backup, mainwindow, rendering and 3 more |
| [src/widgets/categoriessettings.cpp](src/widgets/categoriessettings.cpp) | 93% | 382 / 409 | categorysettings, mainwindow, rendering, settingspage |
| [src/widgets/rulessettings.cpp](src/widgets/rulessettings.cpp) | 93% | 556 / 601 | mainwindow, rendering, rulessettings, settingspage |
| [src/widgets/sidebarwidget.cpp](src/widgets/sidebarwidget.cpp) | 93% | 384 / 413 | mainwindow, rendering, sidebarwidget |
| [src/dialog/balancecorrectiondialog.cpp](src/dialog/balancecorrectiondialog.cpp) | 94% | 93 / 99 | balance, rendering |
| [src/dialog/editaccountdialog.cpp](src/dialog/editaccountdialog.cpp) | 94% | 50 / 53 | editaccount |
| [src/spreadsheet.cpp](src/spreadsheet.cpp) | 94% | 319 / 341 | balance, spreadsheet |
| [src/transactionlistmodel.cpp](src/transactionlistmodel.cpp) | 94% | 157 / 167 | mainwindow, rendering, transactionlistmodel, transactionspage |
| [src/widgets/aboutwidget.cpp](src/widgets/aboutwidget.cpp) | 94% | 94 / 100 | aboutwidget, branding, mainwindow and 2 more |
| [src/widgets/budgeteditor.cpp](src/widgets/budgeteditor.cpp) | 94% | 173 / 184 | budgetpage, mainwindow, rendering |
| [src/widgets/transactioneditor.cpp](src/widgets/transactioneditor.cpp) | 94% | 358 / 381 | mainwindow, rendering, transactionspage |
| [src/db/databasesession.cpp](src/db/databasesession.cpp) | 95% | 118 / 124 | backupbeforeupdate, databasesession, restore and 4 more |
| [src/widgets/settingspage.cpp](src/widgets/settingspage.cpp) | 95% | 359 / 376 | mainwindow, rendering, settingspage |
| [src/dialog/applyrulesdialog.cpp](src/dialog/applyrulesdialog.cpp) | 96% | 104 / 108 | applyrulesdialog |
| [src/dialog/rawsettingsdialog.cpp](src/dialog/rawsettingsdialog.cpp) | 96% | 78 / 81 | rawsettingsdialog |
| [src/icons.cpp](src/icons.cpp) | 96% | 52 / 54 | aboutwidget, branding, budgetpage and 10 more |
| [src/report.cpp](src/report.cpp) | 96% | 301 / 315 | mainwindow, rendering, report and 3 more |
| [src/widgets/budgetpage.cpp](src/widgets/budgetpage.cpp) | 96% | 358 / 373 | budgetpage, mainwindow, rendering |
| [src/dashboard.cpp](src/dashboard.cpp) | 97% | 91 / 94 | balance, dashboard, dashboardpage and 2 more |
| [src/systeminfo.cpp](src/systeminfo.cpp) | 97% | 57 / 59 | aboutwidget, branding, commandline and 5 more |
| [src/widgets/categorytag.cpp](src/widgets/categorytag.cpp) | 97% | 34 / 35 | applyrulesdialog, balance, categorysettings and 4 more |
| [src/settings.cpp](src/settings.cpp) | 98% | 305 / 311 | aboutwidget, applyrulesdialog, backup and 23 more |
| [src/budget.cpp](src/budget.cpp) | 99% | 171 / 172 | balance, budget, budgetpage and 6 more |
| [src/theme.h](src/theme.h) | 99% | 91 / 92 | aboutwidget, backup, balance and 16 more |
| [src/budget.h](src/budget.h) | 100% | 1 / 1 | budget, budgetpage, dashboardpage, rendering |
| [src/category.h](src/category.h) | 100% | 6 / 6 | applyrulesdialog, balance, categorysettings and 7 more |
| [src/csvimport.h](src/csvimport.h) | 100% | 1 / 1 | balance, csvimport |
| [src/csvparser.cpp](src/csvparser.cpp) | 100% | 36 / 36 | balance, csvimport, csvparser |
| [src/dashboard.h](src/dashboard.h) | 100% | 1 / 1 | dashboard, dashboardpage, mainwindow, rendering |
| [src/db/databasemanager.h](src/db/databasemanager.h) | 100% | 1 / 1 | applyrulesdialog, balance, budgetpage and 11 more |
| [src/db/migrationmanager.h](src/db/migrationmanager.h) | 100% | 1 / 1 | databasesession, migrationmanager, startwindow |
| [src/dialog/renamedpayeesdialog.h](src/dialog/renamedpayeesdialog.h) | 100% | 1 / 1 | rulessettings |
| [src/money.cpp](src/money.cpp) | 100% | 30 / 30 | account, addaccountwidget, applyrulesdialog and 21 more |
| [src/report.h](src/report.h) | 100% | 9 / 9 | mainwindow, rendering, report and 2 more |
| [src/settings.h](src/settings.h) | 100% | 1 / 1 | aboutwidget, applyrulesdialog, backup and 23 more |
| [src/transactionlistmodel.h](src/transactionlistmodel.h) | 100% | 1 / 1 | mainwindow, rendering, transactionlistmodel, transactionspage |
| [src/widgets/categorytag.h](src/widgets/categorytag.h) | 100% | 1 / 1 | balance, categorysettings, mainwindow and 3 more |
| [src/widgets/reportspage.h](src/widgets/reportspage.h) | 100% | 2 / 2 | reportspage |
| [src/widgets/rulessettings.h](src/widgets/rulessettings.h) | 100% | 1 / 1 | rulessettings |
| [src/widgets/sidebarwidget.h](src/widgets/sidebarwidget.h) | 100% | 1 / 1 | sidebarwidget |

## Not covered yet

- **src/main.cpp** (60 lines): [21–27](src/main.cpp#L21-L27), [29–30](src/main.cpp#L29-L30), [32–33](src/main.cpp#L32-L33), [36–39](src/main.cpp#L36-L39), [41–42](src/main.cpp#L41-L42), [45](src/main.cpp#L45), [47](src/main.cpp#L47), [49](src/main.cpp#L49), [52–53](src/main.cpp#L52-L53), [55–57](src/main.cpp#L55-L57), [60–62](src/main.cpp#L60-L62), [65–68](src/main.cpp#L65-L68), [70](src/main.cpp#L70), [73–83](src/main.cpp#L73-L83), [85–87](src/main.cpp#L85-L87), [89–91](src/main.cpp#L89-L91), [93–97](src/main.cpp#L93-L97), [99–103](src/main.cpp#L99-L103)
- **src/updatecheck.cpp** (5 lines): [8](src/updatecheck.cpp#L8), [10–13](src/updatecheck.cpp#L10-L13)
- **src/categorycolors.cpp** (19 lines): [21–38](src/categorycolors.cpp#L21-L38), [49](src/categorycolors.cpp#L49)
- **src/db/databasemanager.cpp** (12 lines): [17](src/db/databasemanager.cpp#L17), [19–21](src/db/databasemanager.cpp#L19-L21), [25](src/db/databasemanager.cpp#L25), [50–52](src/db/databasemanager.cpp#L50-L52), [55–58](src/db/databasemanager.cpp#L55-L58)
- **src/mainwindow.cpp** (36 lines): [34–36](src/mainwindow.cpp#L34-L36), [38](src/mainwindow.cpp#L38), [40–42](src/mainwindow.cpp#L40-L42), [44](src/mainwindow.cpp#L44), [95–97](src/mainwindow.cpp#L95-L97), [100–102](src/mainwindow.cpp#L100-L102), [107–109](src/mainwindow.cpp#L107-L109), [138–145](src/mainwindow.cpp#L138-L145), [147–148](src/mainwindow.cpp#L147-L148), [150](src/mainwindow.cpp#L150), [152–159](src/mainwindow.cpp#L152-L159)
- **src/widgets/reportspage.cpp** (273 lines): [54–56](src/widgets/reportspage.cpp#L54-L56), [59–61](src/widgets/reportspage.cpp#L59-L61), [122–128](src/widgets/reportspage.cpp#L122-L128), [130–140](src/widgets/reportspage.cpp#L130-L140), [143–151](src/widgets/reportspage.cpp#L143-L151), [154–156](src/widgets/reportspage.cpp#L154-L156), [160–165](src/widgets/reportspage.cpp#L160-L165), [167–178](src/widgets/reportspage.cpp#L167-L178), [180–183](src/widgets/reportspage.cpp#L180-L183), [185](src/widgets/reportspage.cpp#L185), [229–231](src/widgets/reportspage.cpp#L229-L231), [233](src/widgets/reportspage.cpp#L233), [248–251](src/widgets/reportspage.cpp#L248-L251), [268–271](src/widgets/reportspage.cpp#L268-L271), [275–276](src/widgets/reportspage.cpp#L275-L276), [279–289](src/widgets/reportspage.cpp#L279-L289), [292–295](src/widgets/reportspage.cpp#L292-L295), [297–299](src/widgets/reportspage.cpp#L297-L299), [301–307](src/widgets/reportspage.cpp#L301-L307), [310](src/widgets/reportspage.cpp#L310), [312](src/widgets/reportspage.cpp#L312), [314–318](src/widgets/reportspage.cpp#L314-L318), [322–324](src/widgets/reportspage.cpp#L322-L324), [377–385](src/widgets/reportspage.cpp#L377-L385), [388–400](src/widgets/reportspage.cpp#L388-L400), [424–430](src/widgets/reportspage.cpp#L424-L430), [434–439](src/widgets/reportspage.cpp#L434-L439), [441–443](src/widgets/reportspage.cpp#L441-L443), [445–449](src/widgets/reportspage.cpp#L445-L449), [452](src/widgets/reportspage.cpp#L452), [454](src/widgets/reportspage.cpp#L454), [456–460](src/widgets/reportspage.cpp#L456-L460), [467–468](src/widgets/reportspage.cpp#L467-L468), [512–514](src/widgets/reportspage.cpp#L512-L514), [516](src/widgets/reportspage.cpp#L516), [531–534](src/widgets/reportspage.cpp#L531-L534), [546–551](src/widgets/reportspage.cpp#L546-L551), [554–555](src/widgets/reportspage.cpp#L554-L555), [558–566](src/widgets/reportspage.cpp#L558-L566), [569–574](src/widgets/reportspage.cpp#L569-L574), [576–583](src/widgets/reportspage.cpp#L576-L583), [586](src/widgets/reportspage.cpp#L586), [588](src/widgets/reportspage.cpp#L588), [590–593](src/widgets/reportspage.cpp#L590-L593), [595–599](src/widgets/reportspage.cpp#L595-L599), [607–609](src/widgets/reportspage.cpp#L607-L609), [611](src/widgets/reportspage.cpp#L611), [627–635](src/widgets/reportspage.cpp#L627-L635), [637–647](src/widgets/reportspage.cpp#L637-L647), [722–725](src/widgets/reportspage.cpp#L722-L725), [727](src/widgets/reportspage.cpp#L727), [941](src/widgets/reportspage.cpp#L941), [1062–1067](src/widgets/reportspage.cpp#L1062-L1067), [1328–1332](src/widgets/reportspage.cpp#L1328-L1332), [1368–1379](src/widgets/reportspage.cpp#L1368-L1379), [1413–1417](src/widgets/reportspage.cpp#L1413-L1417), [1466–1470](src/widgets/reportspage.cpp#L1466-L1470)
- **src/widgets/addaccountwidget.cpp** (8 lines): [26–27](src/widgets/addaccountwidget.cpp#L26-L27), [37–38](src/widgets/addaccountwidget.cpp#L37-L38), [41–42](src/widgets/addaccountwidget.cpp#L41-L42), [49–50](src/widgets/addaccountwidget.cpp#L49-L50)
- **src/widgets/transactionspage.cpp** (141 lines): [67–70](src/widgets/transactionspage.cpp#L67-L70), [168–171](src/widgets/transactionspage.cpp#L168-L171), [189–195](src/widgets/transactionspage.cpp#L189-L195), [248–250](src/widgets/transactionspage.cpp#L248-L250), [253–256](src/widgets/transactionspage.cpp#L253-L256), [265–266](src/widgets/transactionspage.cpp#L265-L266), [272–277](src/widgets/transactionspage.cpp#L272-L277), [279](src/widgets/transactionspage.cpp#L279), [301–302](src/widgets/transactionspage.cpp#L301-L302), [354–355](src/widgets/transactionspage.cpp#L354-L355), [378–381](src/widgets/transactionspage.cpp#L378-L381), [419–422](src/widgets/transactionspage.cpp#L419-L422), [424](src/widgets/transactionspage.cpp#L424), [491–493](src/widgets/transactionspage.cpp#L491-L493), [531–534](src/widgets/transactionspage.cpp#L531-L534), [551](src/widgets/transactionspage.cpp#L551), [566](src/widgets/transactionspage.cpp#L566), [584](src/widgets/transactionspage.cpp#L584), [667–674](src/widgets/transactionspage.cpp#L667-L674), [702–704](src/widgets/transactionspage.cpp#L702-L704), [742](src/widgets/transactionspage.cpp#L742), [808](src/widgets/transactionspage.cpp#L808), [819–823](src/widgets/transactionspage.cpp#L819-L823), [826–830](src/widgets/transactionspage.cpp#L826-L830), [832–833](src/widgets/transactionspage.cpp#L832-L833), [836–839](src/widgets/transactionspage.cpp#L836-L839), [841–850](src/widgets/transactionspage.cpp#L841-L850), [853–873](src/widgets/transactionspage.cpp#L853-L873), [925](src/widgets/transactionspage.cpp#L925), [962–963](src/widgets/transactionspage.cpp#L962-L963), [966–967](src/widgets/transactionspage.cpp#L966-L967), [985–989](src/widgets/transactionspage.cpp#L985-L989), [991](src/widgets/transactionspage.cpp#L991), [993–998](src/widgets/transactionspage.cpp#L993-L998), [1000–1005](src/widgets/transactionspage.cpp#L1000-L1005), [1023–1024](src/widgets/transactionspage.cpp#L1023-L1024), [1044–1045](src/widgets/transactionspage.cpp#L1044-L1045)
- **src/rule.h** (1 lines): [17](src/rule.h#L17)
- **src/transaction.cpp** (40 lines): [91](src/transaction.cpp#L91), [154](src/transaction.cpp#L154), [267–268](src/transaction.cpp#L267-L268), [281](src/transaction.cpp#L281), [285–286](src/transaction.cpp#L285-L286), [295](src/transaction.cpp#L295), [317](src/transaction.cpp#L317), [332–333](src/transaction.cpp#L332-L333), [335–336](src/transaction.cpp#L335-L336), [338](src/transaction.cpp#L338), [342](src/transaction.cpp#L342), [348–350](src/transaction.cpp#L348-L350), [352–355](src/transaction.cpp#L352-L355), [357–358](src/transaction.cpp#L357-L358), [360](src/transaction.cpp#L360), [370–371](src/transaction.cpp#L370-L371), [387](src/transaction.cpp#L387), [392](src/transaction.cpp#L392), [394–396](src/transaction.cpp#L394-L396), [398–400](src/transaction.cpp#L398-L400), [403–405](src/transaction.cpp#L403-L405), [410](src/transaction.cpp#L410), [437](src/transaction.cpp#L437)
- **src/db/migrationmanager.cpp** (19 lines): [41–45](src/db/migrationmanager.cpp#L41-L45), [87–88](src/db/migrationmanager.cpp#L87-L88), [104](src/db/migrationmanager.cpp#L104), [117](src/db/migrationmanager.cpp#L117), [141–144](src/db/migrationmanager.cpp#L141-L144), [147–150](src/db/migrationmanager.cpp#L147-L150), [213](src/db/migrationmanager.cpp#L213), [216](src/db/migrationmanager.cpp#L216)
- **src/user.cpp** (8 lines): [29–30](src/user.cpp#L29-L30), [49–50](src/user.cpp#L49-L50), [64–65](src/user.cpp#L64-L65), [72–73](src/user.cpp#L72-L73)
- **src/account.cpp** (29 lines): [33](src/account.cpp#L33), [45](src/account.cpp#L45), [60](src/account.cpp#L60), [87–88](src/account.cpp#L87-L88), [120–121](src/account.cpp#L120-L121), [137–138](src/account.cpp#L137-L138), [159–161](src/account.cpp#L159-L161), [181–183](src/account.cpp#L181-L183), [189–190](src/account.cpp#L189-L190), [238](src/account.cpp#L238), [261–262](src/account.cpp#L261-L262), [286](src/account.cpp#L286), [319–321](src/account.cpp#L319-L321), [334](src/account.cpp#L334), [361–363](src/account.cpp#L361-L363), [391](src/account.cpp#L391)
- **src/commandline.cpp** (2 lines): [36](src/commandline.cpp#L36), [47](src/commandline.cpp#L47)
- **src/startwindow.cpp** (29 lines): [356–361](src/startwindow.cpp#L356-L361), [363](src/startwindow.cpp#L363), [365–370](src/startwindow.cpp#L365-L370), [372–373](src/startwindow.cpp#L372-L373), [375–378](src/startwindow.cpp#L375-L378), [380–383](src/startwindow.cpp#L380-L383), [395–400](src/startwindow.cpp#L395-L400)
- **src/balance.cpp** (2 lines): [18–19](src/balance.cpp#L18-L19)
- **src/db/backup.cpp** (15 lines): [31](src/db/backup.cpp#L31), [35–36](src/db/backup.cpp#L35-L36), [38](src/db/backup.cpp#L38), [62](src/db/backup.cpp#L62), [104–106](src/db/backup.cpp#L104-L106), [127–128](src/db/backup.cpp#L127-L128), [147–148](src/db/backup.cpp#L147-L148), [150](src/db/backup.cpp#L150), [225](src/db/backup.cpp#L225), [239](src/db/backup.cpp#L239)
- **src/widgets/accounttypessettings.cpp** (12 lines): [43](src/widgets/accounttypessettings.cpp#L43), [140–141](src/widgets/accounttypessettings.cpp#L140-L141), [144–145](src/widgets/accounttypessettings.cpp#L144-L145), [156](src/widgets/accounttypessettings.cpp#L156), [159–162](src/widgets/accounttypessettings.cpp#L159-L162), [171–172](src/widgets/accounttypessettings.cpp#L171-L172)
- **src/dialog/renamedpayeesdialog.cpp** (7 lines): [91–92](src/dialog/renamedpayeesdialog.cpp#L91-L92), [104–106](src/dialog/renamedpayeesdialog.cpp#L104-L106), [110–111](src/dialog/renamedpayeesdialog.cpp#L110-L111)
- **src/rule.cpp** (35 lines): [58](src/rule.cpp#L58), [60](src/rule.cpp#L60), [72](src/rule.cpp#L72), [74](src/rule.cpp#L74), [126](src/rule.cpp#L126), [170](src/rule.cpp#L170), [182](src/rule.cpp#L182), [192](src/rule.cpp#L192), [199](src/rule.cpp#L199), [206](src/rule.cpp#L206), [213](src/rule.cpp#L213), [220](src/rule.cpp#L220), [316](src/rule.cpp#L316), [346](src/rule.cpp#L346), [351](src/rule.cpp#L351), [363–364](src/rule.cpp#L363-L364), [496–497](src/rule.cpp#L496-L497), [515–516](src/rule.cpp#L515-L516), [523](src/rule.cpp#L523), [535](src/rule.cpp#L535), [547–548](src/rule.cpp#L547-L548), [594](src/rule.cpp#L594), [606–608](src/rule.cpp#L606-L608), [612–613](src/rule.cpp#L612-L613), [624](src/rule.cpp#L624), [634–635](src/rule.cpp#L634-L635), [651](src/rule.cpp#L651)
- **src/widgets/dashboardpage.cpp** (40 lines): [61–64](src/widgets/dashboardpage.cpp#L61-L64), [67–70](src/widgets/dashboardpage.cpp#L67-L70), [72](src/widgets/dashboardpage.cpp#L72), [226–228](src/widgets/dashboardpage.cpp#L226-L228), [235–238](src/widgets/dashboardpage.cpp#L235-L238), [285–288](src/widgets/dashboardpage.cpp#L285-L288), [290–292](src/widgets/dashboardpage.cpp#L290-L292), [294–300](src/widgets/dashboardpage.cpp#L294-L300), [303](src/widgets/dashboardpage.cpp#L303), [305](src/widgets/dashboardpage.cpp#L305), [307–311](src/widgets/dashboardpage.cpp#L307-L311), [490](src/widgets/dashboardpage.cpp#L490), [516](src/widgets/dashboardpage.cpp#L516), [643](src/widgets/dashboardpage.cpp#L643)
- **src/category.cpp** (13 lines): [53](src/category.cpp#L53), [90](src/category.cpp#L90), [137](src/category.cpp#L137), [143–145](src/category.cpp#L143-L145), [167–168](src/category.cpp#L167-L168), [182](src/category.cpp#L182), [190](src/category.cpp#L190), [206–208](src/category.cpp#L206-L208)
- **src/csvimport.cpp** (21 lines): [34–35](src/csvimport.cpp#L34-L35), [267–268](src/csvimport.cpp#L267-L268), [360–362](src/csvimport.cpp#L360-L362), [372–375](src/csvimport.cpp#L372-L375), [449–454](src/csvimport.cpp#L449-L454), [463–466](src/csvimport.cpp#L463-L466)
- **src/dialog/deleteaccountdialog.cpp** (8 lines): [53](src/dialog/deleteaccountdialog.cpp#L53), [56](src/dialog/deleteaccountdialog.cpp#L56), [132](src/dialog/deleteaccountdialog.cpp#L132), [142–143](src/dialog/deleteaccountdialog.cpp#L142-L143), [162–164](src/dialog/deleteaccountdialog.cpp#L162-L164)
- **src/dialog/importmanagerdialog.cpp** (76 lines): [254–256](src/dialog/importmanagerdialog.cpp#L254-L256), [258–261](src/dialog/importmanagerdialog.cpp#L258-L261), [274](src/dialog/importmanagerdialog.cpp#L274), [303–304](src/dialog/importmanagerdialog.cpp#L303-L304), [345–347](src/dialog/importmanagerdialog.cpp#L345-L347), [357–358](src/dialog/importmanagerdialog.cpp#L357-L358), [366](src/dialog/importmanagerdialog.cpp#L366), [368–372](src/dialog/importmanagerdialog.cpp#L368-L372), [374](src/dialog/importmanagerdialog.cpp#L374), [392–393](src/dialog/importmanagerdialog.cpp#L392-L393), [462](src/dialog/importmanagerdialog.cpp#L462), [488–490](src/dialog/importmanagerdialog.cpp#L488-L490), [545](src/dialog/importmanagerdialog.cpp#L545), [671–673](src/dialog/importmanagerdialog.cpp#L671-L673), [675–677](src/dialog/importmanagerdialog.cpp#L675-L677), [679](src/dialog/importmanagerdialog.cpp#L679), [744](src/dialog/importmanagerdialog.cpp#L744), [784](src/dialog/importmanagerdialog.cpp#L784), [815](src/dialog/importmanagerdialog.cpp#L815), [857](src/dialog/importmanagerdialog.cpp#L857), [973](src/dialog/importmanagerdialog.cpp#L973), [1129–1130](src/dialog/importmanagerdialog.cpp#L1129-L1130), [1160](src/dialog/importmanagerdialog.cpp#L1160), [1175–1176](src/dialog/importmanagerdialog.cpp#L1175-L1176), [1182–1184](src/dialog/importmanagerdialog.cpp#L1182-L1184), [1243–1245](src/dialog/importmanagerdialog.cpp#L1243-L1245), [1256–1261](src/dialog/importmanagerdialog.cpp#L1256-L1261), [1264–1270](src/dialog/importmanagerdialog.cpp#L1264-L1270), [1272–1276](src/dialog/importmanagerdialog.cpp#L1272-L1276), [1301–1302](src/dialog/importmanagerdialog.cpp#L1301-L1302), [1319–1320](src/dialog/importmanagerdialog.cpp#L1319-L1320), [1328](src/dialog/importmanagerdialog.cpp#L1328), [1330](src/dialog/importmanagerdialog.cpp#L1330)
- **src/widgets/backupsettings.cpp** (16 lines): [39](src/widgets/backupsettings.cpp#L39), [42–43](src/widgets/backupsettings.cpp#L42-L43), [46–50](src/widgets/backupsettings.cpp#L46-L50), [123](src/widgets/backupsettings.cpp#L123), [147–148](src/widgets/backupsettings.cpp#L147-L148), [224](src/widgets/backupsettings.cpp#L224), [243](src/widgets/backupsettings.cpp#L243), [252–254](src/widgets/backupsettings.cpp#L252-L254)
- **src/widgets/categoriessettings.cpp** (27 lines): [148–153](src/widgets/categoriessettings.cpp#L148-L153), [173–174](src/widgets/categoriessettings.cpp#L173-L174), [341](src/widgets/categoriessettings.cpp#L341), [444–447](src/widgets/categoriessettings.cpp#L444-L447), [489–490](src/widgets/categoriessettings.cpp#L489-L490), [505–507](src/widgets/categoriessettings.cpp#L505-L507), [511](src/widgets/categoriessettings.cpp#L511), [514–516](src/widgets/categoriessettings.cpp#L514-L516), [529–530](src/widgets/categoriessettings.cpp#L529-L530), [536–538](src/widgets/categoriessettings.cpp#L536-L538)
- **src/widgets/rulessettings.cpp** (45 lines): [41–42](src/widgets/rulessettings.cpp#L41-L42), [44](src/widgets/rulessettings.cpp#L44), [176–181](src/widgets/rulessettings.cpp#L176-L181), [243–252](src/widgets/rulessettings.cpp#L243-L252), [270–276](src/widgets/rulessettings.cpp#L270-L276), [278–279](src/widgets/rulessettings.cpp#L278-L279), [340–341](src/widgets/rulessettings.cpp#L340-L341), [363](src/widgets/rulessettings.cpp#L363), [394–397](src/widgets/rulessettings.cpp#L394-L397), [399](src/widgets/rulessettings.cpp#L399), [668–669](src/widgets/rulessettings.cpp#L668-L669), [697](src/widgets/rulessettings.cpp#L697), [717–719](src/widgets/rulessettings.cpp#L717-L719), [756–758](src/widgets/rulessettings.cpp#L756-L758)
- **src/widgets/sidebarwidget.cpp** (29 lines): [82](src/widgets/sidebarwidget.cpp#L82), [281–282](src/widgets/sidebarwidget.cpp#L281-L282), [284–286](src/widgets/sidebarwidget.cpp#L284-L286), [288](src/widgets/sidebarwidget.cpp#L288), [319](src/widgets/sidebarwidget.cpp#L319), [343](src/widgets/sidebarwidget.cpp#L343), [377–379](src/widgets/sidebarwidget.cpp#L377-L379), [428–435](src/widgets/sidebarwidget.cpp#L428-L435), [519–523](src/widgets/sidebarwidget.cpp#L519-L523), [527–529](src/widgets/sidebarwidget.cpp#L527-L529), [554](src/widgets/sidebarwidget.cpp#L554)
- **src/dialog/balancecorrectiondialog.cpp** (6 lines): [24](src/dialog/balancecorrectiondialog.cpp#L24), [83](src/dialog/balancecorrectiondialog.cpp#L83), [102](src/dialog/balancecorrectiondialog.cpp#L102), [127](src/dialog/balancecorrectiondialog.cpp#L127), [132–133](src/dialog/balancecorrectiondialog.cpp#L132-L133)
- **src/dialog/editaccountdialog.cpp** (3 lines): [71–73](src/dialog/editaccountdialog.cpp#L71-L73)
- **src/spreadsheet.cpp** (22 lines): [50](src/spreadsheet.cpp#L50), [215](src/spreadsheet.cpp#L215), [220](src/spreadsheet.cpp#L220), [234](src/spreadsheet.cpp#L234), [284](src/spreadsheet.cpp#L284), [306–309](src/spreadsheet.cpp#L306-L309), [319](src/spreadsheet.cpp#L319), [329–330](src/spreadsheet.cpp#L329-L330), [382](src/spreadsheet.cpp#L382), [399–400](src/spreadsheet.cpp#L399-L400), [402](src/spreadsheet.cpp#L402), [421](src/spreadsheet.cpp#L421), [423–424](src/spreadsheet.cpp#L423-L424), [468](src/spreadsheet.cpp#L468), [473–474](src/spreadsheet.cpp#L473-L474)
- **src/transactionlistmodel.cpp** (10 lines): [42–43](src/transactionlistmodel.cpp#L42-L43), [56](src/transactionlistmodel.cpp#L56), [88](src/transactionlistmodel.cpp#L88), [103–105](src/transactionlistmodel.cpp#L103-L105), [133](src/transactionlistmodel.cpp#L133), [162–163](src/transactionlistmodel.cpp#L162-L163)
- **src/widgets/aboutwidget.cpp** (6 lines): [47](src/widgets/aboutwidget.cpp#L47), [123–127](src/widgets/aboutwidget.cpp#L123-L127)
- **src/widgets/budgeteditor.cpp** (11 lines): [176](src/widgets/budgeteditor.cpp#L176), [216–217](src/widgets/budgeteditor.cpp#L216-L217), [230–233](src/widgets/budgeteditor.cpp#L230-L233), [235–237](src/widgets/budgeteditor.cpp#L235-L237), [239](src/widgets/budgeteditor.cpp#L239)
- **src/widgets/transactioneditor.cpp** (23 lines): [67–68](src/widgets/transactioneditor.cpp#L67-L68), [206–207](src/widgets/transactioneditor.cpp#L206-L207), [246](src/widgets/transactioneditor.cpp#L246), [445–446](src/widgets/transactioneditor.cpp#L445-L446), [466–469](src/widgets/transactioneditor.cpp#L466-L469), [471–473](src/widgets/transactioneditor.cpp#L471-L473), [475](src/widgets/transactioneditor.cpp#L475), [481–485](src/widgets/transactioneditor.cpp#L481-L485), [487–489](src/widgets/transactioneditor.cpp#L487-L489)
- **src/db/databasesession.cpp** (6 lines): [153](src/db/databasesession.cpp#L153), [156–157](src/db/databasesession.cpp#L156-L157), [166–168](src/db/databasesession.cpp#L166-L168)
- **src/widgets/settingspage.cpp** (17 lines): [497](src/widgets/settingspage.cpp#L497), [499–501](src/widgets/settingspage.cpp#L499-L501), [504–505](src/widgets/settingspage.cpp#L504-L505), [508–510](src/widgets/settingspage.cpp#L508-L510), [528](src/widgets/settingspage.cpp#L528), [530–536](src/widgets/settingspage.cpp#L530-L536)
- **src/dialog/applyrulesdialog.cpp** (4 lines): [51](src/dialog/applyrulesdialog.cpp#L51), [139](src/dialog/applyrulesdialog.cpp#L139), [149–150](src/dialog/applyrulesdialog.cpp#L149-L150)
- **src/dialog/rawsettingsdialog.cpp** (3 lines): [21](src/dialog/rawsettingsdialog.cpp#L21), [93](src/dialog/rawsettingsdialog.cpp#L93), [112](src/dialog/rawsettingsdialog.cpp#L112)
- **src/icons.cpp** (2 lines): [26](src/icons.cpp#L26), [95](src/icons.cpp#L95)
- **src/report.cpp** (14 lines): [88](src/report.cpp#L88), [127](src/report.cpp#L127), [205–206](src/report.cpp#L205-L206), [274–275](src/report.cpp#L274-L275), [312–313](src/report.cpp#L312-L313), [325–326](src/report.cpp#L325-L326), [365–366](src/report.cpp#L365-L366), [410–411](src/report.cpp#L410-L411)
- **src/widgets/budgetpage.cpp** (15 lines): [77–80](src/widgets/budgetpage.cpp#L77-L80), [146–149](src/widgets/budgetpage.cpp#L146-L149), [151](src/widgets/budgetpage.cpp#L151), [357](src/widgets/budgetpage.cpp#L357), [443](src/widgets/budgetpage.cpp#L443), [475–476](src/widgets/budgetpage.cpp#L475-L476), [485–486](src/widgets/budgetpage.cpp#L485-L486)
- **src/dashboard.cpp** (3 lines): [17](src/dashboard.cpp#L17), [21–22](src/dashboard.cpp#L21-L22)
- **src/systeminfo.cpp** (2 lines): [25](src/systeminfo.cpp#L25), [36](src/systeminfo.cpp#L36)
- **src/widgets/categorytag.cpp** (1 lines): [30](src/widgets/categorytag.cpp#L30)
- **src/settings.cpp** (6 lines): [78–79](src/settings.cpp#L78-L79), [171](src/settings.cpp#L171), [174](src/settings.cpp#L174), [176](src/settings.cpp#L176), [362](src/settings.cpp#L362)
- **src/budget.cpp** (1 lines): [221](src/budget.cpp#L221)
- **src/theme.h** (1 lines): [184](src/theme.h#L184)

## Test suites

<details><summary><b>test_aboutwidget</b> · 2 test cases · 8 files</summary>

[tests/test_aboutwidget.cpp](tests/test_aboutwidget.cpp)

- Shows every entry
- Showing again keeps the rows

</details>

<details><summary><b>test_account</b> · 32 test cases · 7 files</summary>

[tests/test_account.cpp](tests/test_account.cpp)

- Test balances are per account
- Test create account
- Test create account invalid type rolls back
- Test create account empty name rejected
- Test create account whitespace name rejected
- Test create account empty type rejected
- Test create account very long name round trips
- Test create account negative initial balance creates transaction and balance
- Test create account zero initial balance creates no transaction
- Test create account initial deposit transaction inserted
- Test create account initial transaction is in initial balance category
- Test create account without initial balance category has no category
- Test create account twice returns distinct ids and both persist
- Test get account types contains seeded types
- Test add account type duplicate name fails
- Test account type crud add edit delete
- Test edit account type nonexistent id returns false
- Test get all accounts for user filters by user
- Test get all accounts for user no accounts returns empty
- Test get all account types contains checking with numeric id
- Test delete account cascades transactions
- Test delete account type removes row
- Test delete account type in use is restricted
- Test delete account type in use does not remove row
- Test delete account nonexistent returns false
- Test delete account removes from balances view
- Test get account by id balance reflects transactions
- Test get account by id not found returns invalid
- Test update transaction keeps the hash for duplicate detection
- Test update transaction rejects temporary id
- Test update transaction rejects unknown id
- Test delete transaction unknown id returns false

</details>

<details><summary><b>test_addaccountwidget</b> · 3 test cases · 8 files</summary>

[tests/test_addaccountwidget.cpp](tests/test_addaccountwidget.cpp)

- Offers the account types
- Creates the account with its starting balance
- Account without starting balance has no transaction

</details>

<details><summary><b>test_applyrulesdialog</b> · 5 test cases · 15 files</summary>

[tests/test_applyrulesdialog.cpp](tests/test_applyrulesdialog.cpp)

- Previews only what would change
- Changes the transactions when confirmed
- Previews and applies renames
- Says so when nothing would change
- Balance adjustments keep their category

</details>

<details><summary><b>test_backup</b> · 7 test cases · 12 files</summary>

[tests/test_backup.cpp](tests/test_backup.cpp)

- Name follows the database and the date
- Copy has the data and later changes stay out
- Replaces an older backup
- Wont replace the database itself
- A failed backup says so and leaves nothing
- Button saves where chosen and adds the extension
- Cancelling writes nothing

</details>

<details><summary><b>test_backupbeforeupdate</b> · 8 test cases · 6 files</summary>

[tests/test_backupbeforeupdate.cpp](tests/test_backupbeforeupdate.cpp)

- A new database gets no backup
- An up to date database gets no backup
- An older database is backed up as it was then updated
- Without a backup the database stays as it was
- Keeps the last five
- A newer database is refused and left alone
- Opens the last database again after a refusal
- Manager knows what its looking at

</details>

<details><summary><b>test_balance</b> · 15 test cases · 25 files</summary>

[tests/test_balance.cpp](tests/test_balance.cpp)

- Balance on a day includes that day
- Corrections go either way
- Corrections arent income or spending
- Dialog adds the difference
- Import dialog compares with the banks balance
- Import uses the rules when ticked
- Import reads the ticked sheets of a spreadsheet
- Sheets with slightly other headers are matched by name
- A spreadsheet with one sheet has nothing to choose
- A damaged spreadsheet keeps the file before
- Import wizard checks each step
- Many file categories stay compact
- Wizard can finish when everything is already imported
- Importer can create the account
- Dialog checks an earlier day

</details>

<details><summary><b>test_branding</b> · 3 test cases · 10 files</summary>

[tests/test_branding.cpp](tests/test_branding.cpp)

- Logo is built in
- App icon has the sizes windows asks for
- Start window and about show the logo

</details>

<details><summary><b>test_budget</b> · 13 test cases · 9 files</summary>

[tests/test_budget.cpp](tests/test_budget.cpp)

- Monthly limit applies from its month on
- Changing the monthly limit keeps earlier months
- Single month value overrides only that month
- Removing only this month
- Removing from this month on
- Spending counts only the months expenses of the user
- More back than spent goes below zero
- Unbudgeted spending lists the rest
- Starting balances arent spending
- Transfers arent spending
- Budgets belong to their user
- Renaming a category keeps its budget
- Month keys

</details>

<details><summary><b>test_budgetpage</b> · 10 test cases · 16 files</summary>

[tests/test_budgetpage.cpp](tests/test_budgetpage.cpp)

- Shows budgets with totals
- Overspending is shown as over
- Empty month explains what to do
- Adding a budget for every month
- Changing only this month
- Removing only this month
- Removing a single month needs no choice
- Unbudgeted spending suggests a limit
- Navigates between months
- Invalid limit shows an error

</details>

<details><summary><b>test_categorysettings</b> · 12 test cases · 13 files</summary>

[tests/test_categorysettings.cpp](tests/test_categorysettings.cpp)

- Lists categories in groups with built in ones locked
- Opening a category keeps the list where it is
- Adds a category with a colour
- Eighteen colours in hue order and two rows
- Preview shows the tag in both themes
- Rejects duplicate and empty names
- Edits name type and colour
- Merges after confirming
- Delete can be cancelled
- Search filters the list
- Account types can be added renamed and deleted when unused
- Shows only the users own categories and account types

</details>

<details><summary><b>test_commandline</b> · 4 test cases · 2 files</summary>

[tests/test_commandline.cpp](tests/test_commandline.cpp)

- Version prints the version and exits
- Help lists the options and exits
- No options starts normally
- Unknown options dont stop the start

</details>

<details><summary><b>test_csvimport</b> · 22 test cases · 13 files</summary>

[tests/test_csvimport.cpp](tests/test_csvimport.cpp)

- Detects delimiter
- Detects decimal separator
- Auto maps english and german headers
- Auto map uses each column once
- Parses dates
- Parses amounts
- Decides transaction type
- Builds transactions and explains failures
- Imports and skips duplicates
- Dry run writes nothing
- Maps the balance column
- Stated balance is the one after the newest row
- Dry run reports the balance before and after
- Import adds the balance correction
- Correction in a dry run is rolled back
- Lists the files categories
- Edited transactions are still recognised on the next import
- Rules rename payees and the next import still knows them
- Rules fill in categories or replace the files when allowed
- Creates new categories on import only
- Unreadable rows abort unless partial import
- Database errors roll back

</details>

<details><summary><b>test_csvparser</b> · 3 test cases · 1 files</summary>

[tests/test_csvparser.cpp](tests/test_csvparser.cpp)

- Parses quoted fields and escaped quotes
- Parses multiline and empty fields
- Rejects unterminated quote

</details>

<details><summary><b>test_dashboard</b> · 10 test cases · 12 files</summary>

[tests/test_dashboard.cpp](tests/test_dashboard.cpp)

- Month totals and balance
- Cash flow covers six months ending now
- Last month is compared up to the same day
- Spending by category is sorted and includes uncategorised
- Recent transactions newest first without future ones
- Starting balances arent income or spending
- Transfers move money but arent income or spending
- Money back in an expense category isnt income
- Other users dont count
- Greeting follows the time of day

</details>

<details><summary><b>test_dashboardpage</b> · 7 test cases · 18 files</summary>

[tests/test_dashboardpage.cpp](tests/test_dashboardpage.cpp)

- Greets by first name
- Tiles show this month
- Overspending is called out
- Empty cards invite
- Budgets show the closest to the limit first
- Clicking a category or transaction asks to show it
- Quick actions are forwarded

</details>

<details><summary><b>test_databasesession</b> · 4 test cases · 7 files</summary>

[tests/test_databasesession.cpp](tests/test_databasesession.cpp)

- Create migrates and remembers the file
- Create refuses existing files
- Open switches between databases
- Rejects files that are not aion databases

</details>

<details><summary><b>test_deleteaccount</b> · 12 test cases · 12 files</summary>

[tests/test_deleteaccount.cpp](tests/test_deleteaccount.cpp)

- Moving keeps the transactions and adds up the balance
- Moving points rules at the target
- Moving needs another account of the same user
- Deleting removes a rule that needs the account
- Deleting leaves any match rule its other conditions
- Deleting removes an any match rule with nothing left
- Deleting leaves other accounts rules alone
- A new account doesnt inherit rules
- Dialog moves by default
- Dialog can delete the transactions
- Dialog for the last account can only delete
- Dialog without transactions has nothing to choose

</details>

<details><summary><b>test_editaccount</b> · 6 test cases · 9 files</summary>

[tests/test_editaccount.cpp](tests/test_editaccount.cpp)

- Changes name and type and keeps the balance
- Refuses an empty name and unknown things
- Refuses another users type
- Taken names ignore case and the account itself
- Dialog starts with the account and saves
- Dialog wont save an empty or taken name

</details>

<details><summary><b>test_integration</b> · 2 test cases · 5 files</summary>

[tests/test_integration.cpp](tests/test_integration.cpp)

- Test foreign keys are enabled
- Test foreign keys remain enabled after reopen

</details>

<details><summary><b>test_logging</b> · 2 test cases · 7 files</summary>

[tests/test_logging.cpp](tests/test_logging.cpp)

- Saving a transaction logs none of it
- A failed save logs the error but not the values

</details>

<details><summary><b>test_mainwindow</b> · 6 test cases · 39 files</summary>

[tests/test_mainwindow.cpp](tests/test_mainwindow.cpp)

- Applying only redoes what changed
- Starts on the dashboard
- Sidebar switches pages and accounts
- Dashboard shortcuts open the right place
- Changed transactions reach the other pages
- Edited categories reach the other pages

</details>

<details><summary><b>test_migrationmanager</b> · 4 test cases · 3 files</summary>

[tests/test_migrationmanager.cpp](tests/test_migrationmanager.cpp)

- Upgrading from every version keeps the data
- Failed migration rolls back statements
- Semicolons in comments dont split statements
- Partially initialized version one can recover

</details>

<details><summary><b>test_money</b> · 4 test cases · 2 files</summary>

[tests/test_money.cpp](tests/test_money.cpp)

- Plain string is stable
- Parses plain decimals
- Converts spin box values exactly
- Transaction hash matches the old format

</details>

<details><summary><b>test_rawsettingsdialog</b> · 4 test cases · 3 files</summary>

[tests/test_rawsettingsdialog.cpp](tests/test_rawsettingsdialog.cpp)

- Lists every setting sorted by name
- Search finds names and values
- Apply saves edits and cancel does not
- Ok saves and closes

</details>

<details><summary><b>test_rendering</b> · 4 test cases · 45 files</summary>

[tests/test_rendering.cpp](tests/test_rendering.cpp)

- Every page draws in both themes
- Dashboard chart shows months on hover
- Dialogs draw in both themes
- System theme follows the operating system

</details>

<details><summary><b>test_report</b> · 8 test cases · 2 files</summary>

[tests/test_report.cpp](tests/test_report.cpp)

- Presets are whole months ending with this one
- Custom ranges are turned round
- Compares with the same length before
- Cuts into days weeks and months
- Labels
- Comparisons in words
- Changes in percent
- Preset keys

</details>

<details><summary><b>test_reportdata</b> · 14 test cases · 10 files</summary>

[tests/test_reportdata.cpp](tests/test_reportdata.cpp)

- Spending per category largest first
- Transfers and balance adjustments are left out
- Compares with the range before
- One account only
- Income and spending per month
- Short ranges go by week
- Kept only when something was kept
- Balance day by day up to today
- One category month by month
- Trend averages only over what has begun
- Payees largest first with spellings together
- Budgets added up over the months
- Budgets take whole months that have begun
- Other users are left out

</details>

<details><summary><b>test_reportspage</b> · 10 test cases · 17 files</summary>

[tests/test_reportspage.cpp](tests/test_reportspage.cpp)

- Starts with six months and remembers the choice
- Custom ranges are asked for and remembered
- Cards open their report
- Accounts can be chosen
- Spending by category on the card and in detail
- Income vs spending on the card and in detail
- Balance over time on the card and in detail
- Top payees on the card and in detail
- Budget vs actual on the card and in detail
- Category trend follows the largest or the chosen category

</details>

<details><summary><b>test_restore</b> · 11 test cases · 13 files</summary>

[tests/test_restore.cpp](tests/test_restore.cpp)

- Inspect counts whats inside
- Inspect refuses what cant be restored
- Restore brings back the data and saves what was there
- The oldest automatic backup can be restored
- A bad backup changes nothing
- The open database cant restore itself
- A backup that wont open puts the data back
- List shows this databases backups newest first
- Restoring from a file asks then says so
- A file that cant be restored isnt offered
- Rows in the list restore

</details>

<details><summary><b>test_rule</b> · 21 test cases · 11 files</summary>

[tests/test_rule.cpp](tests/test_rule.cpp)

- Text conditions ignore case and spaces
- The match index counts like the rules match
- Payee or description looks at both
- Amount type and account
- All or any condition
- The first matching rule wins
- Changes skip protected and unchanged transactions
- Applying saves the new categories
- Drafts start from the payee then description then amount
- Drafts leave out reference numbers at the end
- Drafts from several use the words their payees share
- Drafts also look at descriptions and take the most specific text
- Transactions are added to rules that any condition is enough for
- Renaming keeps the banks name for matching and duplicates
- Each action takes the first rule that has it
- Applying renames and keeps the original
- Rules need an action and keep their rename without a category
- Rules are stored with their conditions and order
- Rules can have a name or be shown by their conditions
- Users dont see or change each others rules
- Rules follow their category

</details>

<details><summary><b>test_rulessettings</b> · 15 test cases · 22 files</summary>

[tests/test_rulessettings.cpp](tests/test_rulessettings.cpp)

- Opening and saving dont count every rule again
- Starts empty and creates a rule
- Removing a condition shows an icon
- Rules can set transfer but not balance adjustments
- Incomplete rules are not saved
- Amounts use the money format and can be a range
- Named rules show their name and long rules fit the list
- Rules can be edited reordered and deleted
- Rules can rename with or without a category
- Changing a rules rename offers to update its names
- Opening a rule keeps the list where it is
- A rule can start from a draft
- A draft says how many of its transactions it matches
- Adding to a rule edits that rule
- Describes every kind of condition

</details>

<details><summary><b>test_scheduledbackup</b> · 13 test cases · 11 files</summary>

[tests/test_scheduledbackup.cpp](tests/test_scheduledbackup.cpp)

- Defaults to every day keeping seven
- Is due follows the schedule
- Opening backs up the first time then not again that day
- Off means no backups
- A database without users gets none
- Unchanged since the last one is skipped
- Changed since the last one is backed up
- Not due yet is skipped even when changed
- Keeps as many as set and leaves other kinds alone
- A failed backup still opens the database
- A failed backup is remembered until one works
- The section warns about a failure while the schedule is on
- The section shows and saves the choices

</details>

<details><summary><b>test_settings</b> · 12 test cases · 5 files</summary>

[tests/test_settings.cpp](tests/test_settings.cpp)

- Currency symbols
- Separators and formatting
- Settings round trip and delimiter fallback
- Enum values and boolean round trips
- Malformed values use fallbacks
- Formatting boundaries and other settings
- Missing keys use documented defaults
- Restore defaults keeps window state
- The number format belongs to the database
- Category suggestions are on until turned off
- Recent databases are distinct newest first and limited
- Sidebar state defaults and round trip

</details>

<details><summary><b>test_settingspage</b> · 10 test cases · 23 files</summary>

[tests/test_settingspage.cpp](tests/test_settingspage.cpp)

- Shows the stored values
- Changes wait for apply
- Revert restores the stored values
- Custom currency symbol
- Category colours in overviews can be turned off
- Category suggestions can be turned off
- Sidebar budget sort is chosen here or in the sidebar
- Create rule shows the rules section with the draft
- Shows categories added elsewhere after reload
- Categories and account types have their own sections and save right away

</details>

<details><summary><b>test_sidebarwidget</b> · 10 test cases · 16 files</summary>

[tests/test_sidebarwidget.cpp](tests/test_sidebarwidget.cpp)

- Shows accounts with balances
- Clicks request accounts pages and actions
- Shows the user
- Budgets show this months spending
- Shows the most used budgets unless all are wanted
- Budgets follow the chosen sort
- Shows when there are no budgets
- Folded sections show a summary and are remembered
- Shows the logo and name and keeps the logo when collapsed
- Collapsing is remembered

</details>

<details><summary><b>test_spreadsheet</b> · 6 test cases · 1 files</summary>

[tests/test_spreadsheet.cpp](tests/test_spreadsheet.cpp)

- Knows spreadsheets by their name
- Reads excel sheets with their values
- Excel dates can count from1904
- Reads libre office sheets
- Matches other sheets columns by name
- Explains what cant be read

</details>

<details><summary><b>test_startupdatabase</b> · 5 test cases · 6 files</summary>

[tests/test_startupdatabase.cpp](tests/test_startupdatabase.cpp)

- Opens the last database without a warning
- Opens the default without a warning on a first start
- Warns when the last database is gone
- Warns when the last database is from a newer version
- Says why when no database can be opened

</details>

<details><summary><b>test_startwindow</b> · 9 test cases · 14 files</summary>

[tests/test_startwindow.cpp](tests/test_startwindow.cpp)

- Empty database opens add form
- Shows the build version
- Adding a user opens it
- Remember stores the user
- Menu lists recent databases and forgets missing ones
- Switching shows the other databases users
- The remembered user belongs to its database
- A broken database file is explained
- Invalid or duplicate users show an error

</details>

<details><summary><b>test_systeminfo</b> · 3 test cases · 6 files</summary>

[tests/test_systeminfo.cpp](tests/test_systeminfo.cpp)

- Masks the home folder in both separator styles
- Version text shows the commit in debug builds
- Report lists build and database details

</details>

<details><summary><b>test_transactionlistmodel</b> · 9 test cases · 4 files</summary>

[tests/test_transactionlistmodel.cpp](tests/test_transactionlistmodel.cpp)

- Groups by day newest first
- Exposes names and signed amounts
- Totals follow filter
- Totals leave out transfers and balance adjustments
- Refunds lower the spending instead of adding income
- Search matches payee category and account
- Category filter includes no category
- Collapsed days keep header with summary
- Payees are distinct

</details>

<details><summary><b>test_transactionspage</b> · 21 test cases · 20 files</summary>

[tests/test_transactionspage.cpp](tests/test_transactionspage.cpp)

- New transaction uses shown account
- Saving writes immediately and closes editor
- Rules suggest the category of new transactions
- Rules offer a new payee name without changing it
- Restoring gives back the banks name
- Selecting one transaction opens editor
- Plain clicks open but only ctrl selects
- Selecting multiple shows bulk bar and closes editor
- Switching the account keeps the selection bar right
- Searching keeps the selection bar right
- Ctrl and shift on day header select the day
- Zero amount is rejected
- Fast entry starts the next transaction
- Open transaction clears filters and unfolds its day
- Show category this month filters the list
- Show category in range shows the days as a chip
- Bulk changes stay quick
- A failed bulk delete changes nothing
- Deleting asks first
- Deletes the selection
- Sets the category of the selection

</details>

<details><summary><b>test_user_category</b> · 15 test cases · 9 files</summary>

[tests/test_user_category.cpp](tests/test_user_category.cpp)

- Auto generates user id
- Lists users by name and finds by username
- Rejects duplicate user fields
- Colours are stored
- Usage counts transactions and budgets
- Merging moves transactions and budgets
- Merging keeps the targets own budgets
- Deleting can keep the transactions
- New users start with their own defaults
- Default categories have their own colours
- Transfer is built in but not a balance adjustment
- Users dont see or change each others categories
- Account types belong to their user
- Starting balances and corrections use the account users categories
- Category crud and validation

</details>

