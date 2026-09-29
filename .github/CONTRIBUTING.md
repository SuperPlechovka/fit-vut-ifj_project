# Přispívání

Pravidla a návody pro společnou práci na IFJ26.

- [Pravidla týmu](#pravidla-týmu) · [Prostředí](#prostředí)
- [Issues](#issues) · [Větve a commity](#větve-a-commity) · [PR a review](#pr-a-review)
- [Git tahák](#git-tahák) · [Kód a komentáře](#kód-a-komentáře)
- [Testování](#testování) · [Pravidla zadání](#pravidla-zadání)
- [Dokumentace a podklady](#dokumentace-a-podklady) · [AI a zdroje](#ai-a-zdroje)
- [Před mergem](#před-mergem)

## Pravidla týmu

- **Jeden úkol = jedna větev = jeden PR.** Velké úkoly rozděl.
- **Na `main` přímo nepracujeme.** Změny jdou přes PR a review.
- **Merge commit ano. Rebase, squash a force push ne.**
- Jeden commit = jedna logická změna. Kód musí zůstat přeložitelný.
- O změně rozhraní se domluv předem. Nečekej až na review.
- Testy a podklady dokumentace patří do Gitu.
- Binárky, cache, hesla, tokeny a školní nástroje do Gitu nepatří.
- Repo i řešení zůstávají během vývoje neveřejné.
- Každý musí rozumět celému řešení a umět vysvětlit vlastní práci.
- Blokuje tě problém? Napiš do issue, co jsi zkusil a s čím potřebuješ pomoct.

## Nastavení Gitu

Před prvním commitem nastav své jméno a e-mail. Příkazy spusť ve složce repozitáře:

```sh
git config --local user.name "Jméno Příjmení"
git config --local user.email "tvuj@email.cz"
```

- Použij své skutečné jméno.
- Pro přiřazení commitů k účtu použij e-mail přidaný na GitHubu nebo svůj GitHub noreply e-mail.
- Nastavení platí jen pro tento repozitář a nové commity.
- Každý člen si ho nastavuje u sebe; neukládá se do společných souborů.

Ověření:

```sh
git config --local user.name
git config --local user.email
```

## Prostředí

### Docker — Windows, macOS i Linux

- Potřebuješ Git a Docker s příkazem `docker compose`.
- Naklonuj repo: `git clone <URL_REPOZITARE>`.
- Přejdi do jeho složky a spusť:

```sh
docker compose build
docker compose run --rm dev
```

- Další příkazy `make` spouštěj uvnitř kontejneru.
- Pracovní složka je `/workspace`; změny se ukládají i na tvůj počítač.
- Uvnitř je GCC 16, Make, Python 3, clang-format, Cppcheck, Valgrind, Doxygen a Graphviz.
- Ukončení shellu: `exit`. Návrat: `docker compose run --rm dev`.
- Na Linuxu s jiným UID/GID použij:

```sh
LOCAL_UID=$(id -u) LOCAL_GID=$(id -g) docker compose run --rm dev
```

- Na ARM Macu ověř také x86-64 sestavení na Merlinovi.

### Bez Dockeru

- Sestavení: GNU Make a GCC 14/15/16 nebo Clang 19/23 podle zadání.
- Vývoj: Python 3.10+, clang-format, Cppcheck, Doxygen a Graphviz.
- Kontrola paměti: Valgrind.
- Stejná verze clang-format jako v Dockeru zabrání rozdílům ve formátování.
- Jiný překladač: `make CC=clang`.
- Po změně překladače nebo přepínačů nejdřív `make clean`.
- `make clean` smaže `build/`, `dist/` a program `ifj26`.

### Školní nástroje

- `ic26int` a `is_it_ok.sh` získej z Moodle nebo Merlinu.
- Ulož je například do ignorované složky `tools/local/`.
- Necommituj je a nešiř je bez svolení.
- Použij verzi pro svůj systém; v Dockeru linuxovou.

```sh
chmod +x tools/local/ic26int
make test IC26INT="$PWD/tools/local/ic26int"
```

- Cesta musí odpovídat skutečnému souboru.
- Nápověda: `ic26int --help`; aktuální pokyny jsou v Moodle.

## Issues

- Nejdřív ověř, že stejný úkol už neexistuje.
- Vyber formulář: **chyba**, **funkce/rozšíření** nebo **pracovní úkol**.
- Název má říct, co je potřeba udělat. Např. „Opravit návratový kód při redefinici“.
- **Cíl:** co má fungovat.
- **Rozsah:** co úkol zahrnuje.
- **Zadání:** příslušná část specifikace.
- **Hotovo, když:** jak výsledek ověříme.
- **Návaznosti:** na čem úkol závisí.
- U chyby přidej nejmenší vstup, který ji ukáže, a postup spuštění.
- Uveď očekávaný a skutečný výsledek včetně návratového kódu.
- Jeden úkol má jednoho hlavního řešitele; spolupracovat může více lidí.
- Rozšíření označ přesným identifikátorem a uveď jeho závislosti.
- Průběžně doplň rozhodnutí a překážky do komentářů.
- Hotové issue zavři přes související PR.

## Větve a commity

**Větev:** `typ/popis-zmeny`.

- `feature/lexer` — nová funkčnost.
- `fix/indentation` — oprava.
- `test/symtable` — testy.
- `docs/comments` — dokumentace.
- `chore/ci` — nástroje a nastavení.
- Malá písmena, bez diakritiky, spojovníky místo mezer.

**Commit:** `typ(oblast): stručný popis`.

- `feat(lexer): read identifiers`
- `fix(parser): reject empty block`
- `test(symtable): cover duplicate keys`
- `docs(parser): describe ownership`
- Oblast lze vynechat: `chore: update development tools`.
- Piš, co se změnilo. Nepoužívej `aaa`, `hotovo` nebo samotné `update`.
- Nemíchej nesouvisející změny do jednoho commitu.
- Před commitem projdi diff a spusť příslušné kontroly.
- Průběžný push je v pořádku. Rozpracovanou práci označ jako **Draft PR**.

## PR a review

- PR směřuje do `main` a řeší jednu hlavní věc.
- Název popisuje změnu, např. `fix(parser): reject empty block`.
- Vyplň části **Proč, Změny, Ověření, Dokumentace, Související issue**.
- Názvy těchto částí neměň; kontroluje je CI.
- V **Ověření** napiš konkrétní příkazy a jejich výsledky.
- U změn překladače přidej výsledek plných testů s `ic26int`.
- `Closes #42` uzavře hotové issue po merge. `Refs #42` jen odkazuje.
- Štítky dej i na PR; z issue se automaticky nepřenášejí.
- Používej odpovídající štítek: `bug`, `enhancement`, `documentation` nebo `dependencies`.
- Hotový PR přepni z Draft na **Ready for review** a požádej dalšího člena o kontrolu.
- Před merge: zelené CI, vyřešené diskuse a alespoň jedno schválení od dalšího člena.
- Vlastní PR si autor sám neschvaluje.
- Opravy po review přidej jako další commity.
- Použij **Create a merge commit**. Potom smaž sloučenou větev.

**Reviewer kontroluje:**

- Soulad s issue a specifikací.
- Rozhraní a návaznosti na ostatní moduly.
- Testy běžných, hraničních a chybových vstupů.
- Alokace, vlastnictví a uvolnění paměti.
- Návratové kódy a chybové cesty.
- Komentáře, hlavičky, autorství a označení AI kódu.

## Git tahák

### Začátek úkolu

Nejdřív `git status`. Rozdělané změny ulož commitem nebo stashem.

```sh
git switch main
git pull --ff-only
git switch -c feature/lexer
```

### Commit a push

```sh
git diff
git add -A
git diff --staged
git commit -m "feat(lexer): read identifiers"
git push -u origin feature/lexer
```

- `add -A` přidá vše včetně smazaných souborů. Vždy zkontroluj stage.
- Další push na stejné větvi: `git push`.
- Push sám nevytvoří PR; otevři ho na GitHubu.

### Aktualizace vlastní větve

Na své větvi, s uloženými změnami:

```sh
git fetch origin
git merge origin/main
```

- `fetch` stáhne historii; pracovní soubory nezmění.
- Konflikt: `git status` → opravit soubory → odstranit konfliktní značky → testy.
- Dokončení: `git add SOUBOR`, `git commit`, `git push`.
- Zrušení rozpracovaného merge: `git merge --abort`.

### Po merge PR

```sh
git switch main
git pull --ff-only
git branch -d feature/lexer
git fetch --prune
```

- Když `-d` odmítne smazat větev, ověř proč. Nepřecházej hned na `-D`.

### Stash a opravy

- `git status` — stav změn.
- `git stash push -u -m "rozpracovano"` — odloží změny i nové neignorované soubory.
- `git stash list` — seznam uložených změn.
- `git stash apply` — obnoví poslední stash a ponechá ho uložený.
- `git stash pop` — obnoví stash a při úspěchu ho smaže.
- `git stash drop 'stash@{0}'` — trvale smaže vybraný stash.
- `git restore --staged SOUBOR` — odebere soubor ze stage; změny ponechá.
- `git restore SOUBOR` — **zahodí neuložené změny mimo stage**.
- `git revert SHA` — novým commitem vrátí účinek staršího commitu; do mainu opět přes PR.
- `git log --oneline --graph --all` — přehled historie.
- `git reflog` — lokální historie pohybu HEAD; pomůže při omylu.
- Stash se pushem nezálohuje.
- `reset --hard` a `clean -fd` mohou smazat práci. Nepoužívej je naslepo.

### Worktrees — volitelné

- Větev = historie změn. Worktree = další pracovní složka stejného repa.
- Hodí se pro dva souběžné úkoly bez přepínání a stashe.

```sh
git fetch origin
git worktree add -b fix/parser ../ifj-parser origin/main
cd ../ifj-parser
```

- Pracuj jako v běžné složce. První push: `git push -u origin fix/parser`.
- `git worktree list` — seznam pracovních složek.
- `git worktree remove ../ifj-parser` — z jiné složky odstraní čistý worktree; větev zůstane.
- Stejná větev běžně nemůže být otevřená ve dvou worktrees současně.
- Sestavení proveď v každém worktree zvlášť.
- Na GitHub se posílají commity a větve, ne pracovní složky.

## Kód a komentáře

- `.c` a `.h` ukládej přímo do `src/`, bez podsložek.
- Hlavní funkce programu patří do `src/main.c`.
- Tabulka symbolů patří do `src/symtable.c` a `src/symtable.h`.
- Interní reprezentaci modulu skryj za jeho rozhraní.
- Používej `make format`; styl určuje `.clang-format`.
- Kontroluj alokace, vlastnictví paměti, uvolnění i chybové cesty.
- Komentář vysvětluje **účel, rozhraní nebo důvod**. Nepopisuj samozřejmé řádky.
- Každá funkce má stručný popis účelu a rozhraní.

### Hlavička souboru pro Doxygen

Každý `.c` a `.h` začni hlavičkou. Uveď skutečné autory a jejich loginy.

```c
/**
 * @file
 * @brief Stručný účel souboru.
 * @details Projekt: Implementace překladače IFJ26 (IFJ/IAL 2026/2027).
 * @author Jméno Příjmení (xlogin)
 */
```

- `@file` označí dokumentaci souboru; jméno se vezme automaticky.
- `@brief` je krátký popis.
- Dalšího autora přidej dalším `@author`, pokud na souboru skutečně pracoval.

### Komentáře funkcí

- Používej blok `/** ... */` před deklarací nebo definicí.
- Veřejné funkce popiš u deklarace v `.h`; stejný text nekopíruj do `.c`.
- Interní funkce popiš u definice v `.c`.
- `@param[in]` — vstup; `@param[out]` — výstup; `@param[in,out]` — obojí.
- Za `@param` uveď přesné jméno parametru a jeho význam.
- `@return` — význam výsledku; u návratového typu `void` vynech.
- `@retval` — význam konkrétní návratové hodnoty, pokud to pomůže.
- Uveď povolené hodnoty, chybové návraty a kdo uvolňuje vrácenou paměť.
- TODO spoj s issue: `TODO(#42): doplnit kontrolu rozsahu`.
- AI úsek označ samostatným komentářem s konkrétním promptem.
- Komentáře ověř přes `make docs`; HTML najdeš v `build/docs/html/index.html`.

## Testování

### Příkazy

| Příkaz | Výsledek |
| --- | --- |
| `make` | Sestaví `ifj26`. |
| `make format` | Opraví formátování C a hlaviček. |
| `make check` | Zkontroluje formátování a spustí Cppcheck. |
| `make unit` | Spustí testy modulů. |
| `make compile-tests` | Ověří návratové kódy překladu a hlavičku úspěšného výstupu. |
| `make system IC26INT=/cesta/ic26int` | Ověří překlad i spuštění a přesný výstup. |
| `make test IC26INT=/cesta/ic26int` | Spustí unit i plné systémové testy. |
| `make docs` | Vygeneruje programovou dokumentaci. |
| `make infra-test` | Ověří vývojové nástroje. |

- Cestu nahraď skutečnou cestou k interpretu. Je-li v `PATH`, stačí `make test`.
- Prázdná sada testů je chyba.
- Jeden běh testu má limit 10 sekund; výkonnostní testy řeš zvlášť.

### Unit testy

- Soubory: `tests/unit/test_*.c`.
- Každý testovací program má vlastní `main()`.
- Úspěch vrací `0`, chyba nenulový kód.
- Můžeš použít `assert`; nezapínej `NDEBUG`.
- Každý test se propojí se všemi `src/*.c` kromě `src/main.c`.
- Spustitelné testy vznikají v `build/unit/`.

### Systémové testy

Do `tests/system/` přidej soubory se stejným základem názvu:

| Soubor | Obsah |
| --- | --- |
| `nazev.ifj` | Vstup pro překladač. |
| `nazev.json` | Očekávané návratové kódy. |
| `nazev.in` | Vstup přeloženého programu; nepovinný. |
| `nazev.out` | Přesný výstup přeloženého programu; může být prázdný. |

Úspěšný překlad a běh — obsah `.json`:

```json
{"compiler_exit": 0, "interpreter_exit": 0}
```

Lexikální chyba — obsah `.json`:

```json
{"compiler_exit": 1}
```

- Při chybě překladu se interpret nespouští; `.in` a `.out` nejsou potřeba.
- Úspěšný překlad vždy potřebuje `interpreter_exit` a `.out`, i pro `compile-tests`.
- Běhová chyba: `compiler_exit` je `0`, `interpreter_exit` je očekávaný běhový kód.
- Chybějící `.in` znamená prázdný vstup.
- Mezery a nové řádky v `.ifj`, `.in` a `.out` neměň automaticky.
- Výstup se porovnává po bajtech; text diagnostiky se přesně neporovnává.
- Testy můžeš dělit do podsložek.
- Plná sada musí obsahovat alespoň jeden úspěšný překlad i běh interpretu.

### Co pokrýt

- Běžné, hraniční a chybové vstupy.
- Odsazení, nové řádky, komentáře, literály a escape sekvence.
- Syntaxi, prolog a vstupní funkci.
- Viditelnost, redefinice a tabulku symbolů.
- Typy, parametry, návratové hodnoty a rekurzi.
- Vestavěné funkce, přesný výstup a vybraná rozšíření.
- Správné chybové kódy, nejen přijetí nebo odmítnutí vstupu.

### Paměť

Nahraď `TVUJ_TEST.ifj` skutečným případem:

```sh
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all \
  --error-exitcode=97 ./ifj26 < tests/system/TVUJ_TEST.ifj > /dev/null
```

- Ověř i vstupy, které skončí chybou.
- Kód `97` zde značí chybu kontroly paměti; není to kód ze specifikace IFJ26.

### Co kontroluje CI

- Sestavení, formátování, Cppcheck, unit testy, překlad systémových případů a Doxygen.
- Používá stejné prostředí Dockeru jako lokální vývoj.
- **Školní interpret v CI není. Zelené CI nepotvrzuje správný běh vygenerovaného programu.**
- Před mergem změn překladače spusť plné testy lokálně nebo na Merlinovi. Výsledek napiš do PR.
- Bez zdrojových `.c` se kontroly překladače přeskočí; běží jen kontroly nástrojů a popisu PR.
- Od prvního `.c` už CI vyžaduje sestavení, testy a další kontroly výše.

## Pravidla zadání

- Platí aktuální zadání, specifikace a oficiální upřesnění v Moodle.
- Přehled vychází z dodaného zadání z 16. 9. 2026 a prezentace z 18. 9. 2026.
- Překladač: **C a pouze standardní knihovna C**. Výjimka pro testovací frameworky se týká testů.
- Sestavení: prosté `make`, bez nutnosti Dockeru či Pythonu.
- Vstup přes `stdin`, výsledek přes `stdout`, diagnostika přes `stderr`.
- Bez sítě, podprocesů a zápisu souborů, včetně `/tmp`.
- Zákaz zápisu se týká překladače; vývojové nástroje mohou vytvářet soubory.
- Lexikální analýza: konečný automat.
- Syntaxe mimo výrazy: LL gramatika; výrazy: precedenční analýza podle přednášek.
- Bez generátorů lexerů a parserů.
- Tabulka symbolů musí odpovídat registrované variantě a mít skrytou reprezentaci.
- `vv-BVS`: výškově vyvážený binární vyhledávací strom.
- `TRP-izp`: tabulka s rozptýlenými položkami a otevřeným adresováním.
- Samostatná obecná struktura, pokud ji použijete: `bst.c/.h` nebo `hashtable.c/.h`.
- Algoritmus neprobíraný v IAL vysvětlete v dokumentaci a u obhajoby.
- Rozšíření vybírejte po dohodě; dodržte jejich závislosti a přidejte je do README, issues a testů.
- Scala přijímá i konstrukce mimo IFJ26. Není jediným měřítkem správnosti testu.

### Návratové kódy překladače

| Kód | Význam |
| --- | --- |
| `0` | Úspěšný překlad. |
| `1` | Lexikální chyba. |
| `2` | Syntaktická chyba. |
| `3` | Nedefinovaný symbol nebo chybějící vstupní funkce. |
| `4` | Nepovolená redefinice. |
| `5` | Argumenty volání nebo parametry vstupní funkce. |
| `6` | Typová kompatibilita nebo odvození typu. |
| `7` | Zápis do nemodifikovatelné proměnné nebo parametru. |
| `10` | Ostatní sémantické chyby. |
| `99` | Interní chyba překladače. |

- Přesné případy a přednost chyb určuje specifikace §6.
- Kód `40` patří vybraným chybám při **běhu přeloženého programu**, ne překladu.
- Chyby samotného interpretu mají další kódy podle specifikace.

### Společné termíny

| Milník | Termín — český čas |
| --- | --- |
| Pokusné odevzdání | 19. 11. 2026, 23:59:59. |
| Finální odevzdání | **27. 11. 2026, 11:59:59 — před polednem.** |
| Tištěná dokumentace | 26. 11. až 1. 12. 2026, 8:00. |
| Obhajoba | 12.–13. výukový týden; přesný termín ve StudIS. |

- Aktuální oznámení mají přednost. Případný druhý pokus není zaručený.
- U obhajoby prezentuje náhodně vybraný člen; připravit se musí každý.

## Dokumentace a podklady

- Vlastní text a diagramy verzuj v `docs/`.
- Hotový dokument patří do kořene jako `dokumentace.pdf`; verzuj i toto PDF.
- Dokumentaci pište a upravujte sami, bez AI.
- Jazyk: čeština, slovenština nebo angličtina. Používej diakritiku.
- Popisujte konkrétní řešení; stručně a čitelně na A4.
- Programová dokumentace z Doxygenu nenahrazuje závěrečné PDF.
- Generované HTML necommituj a během neveřejného vývoje ho veřejně nesdílej.

**V PDF musí být:**

- První strana: jména, loginy a VUT ID členů; vedoucí tučně.
- Varianta, rozšíření, podíly a odkaz na repozitář.
- Rozdělení práce; zdůvodnění nerovnoměrných podílů.
- Spolupráce, review, testování a formátování.
- Vlastní schéma modulů, datových toků a předávaných struktur.
- Vlastní automat lexeru, LL gramatika a tabulka, precedenční tabulka.
- Stručný popis každého modulu `.c/.h`.
- Citace převzatých zdrojů, kódu a algoritmů.
- Použité AI nástroje a modely, postup, chyby a odmítnuté či upravené návrhy.
- Tištěná a elektronická verze musí být shodné.

**Autoři a podíly:**

- V `AUTHORS` udržujte skutečné členy týmu.
- `rozdeleni`: jeden řádek `VUT_ID:FIT_LOGIN:PROCENTA` na člena.
- Zahrňte i člena s nulovým podílem; celá procenta musí dohromady dát `100`.
- ID a loginy musí být unikátní; soubor má konce řádků LF i na posledním řádku.
- Bez komentářů, prázdných řádků a znaku `%`.
- Stejné podíly patří do PDF. Každý si je před obhajobou ověří ve StudIS.

**Podklady pro finální balení:**

- `Makefile`, `src/*.c`, `src/*.h`, vlastní `dokumentace.pdf` a `rozdeleni`.
- Soubory v `src/` bez podsložek, symlinků, mezer a diakritiky v názvech.
- Balicí nástroj povoluje ASCII písmena, číslice, podtržítka a tečky.
- Kód musí fungovat i bez `tools/`, Dockeru a Pythonu; ověřte prosté `make` na Merlinovi.
- Testy, zdroje dokumentace a týmové soubory zůstávají v Gitu; do výsledného ZIPu se nepřibalují.
- Automatická kontrola PDF ověřuje pouze jeho hlavičku, ne obsah dokumentace.

## AI a zdroje

- AI lze použít pro vysvětlení, nástroje, hledání chyb, testy a dovolené dílčí části kódu.
- **AI nesmí vytvořit celé řešení, závěrečnou dokumentaci ani její úpravy.**
- **Bez AI tvořte automaty, gramatiku, LL/precedenční tabulky a kód vyhledávací struktury.**
- Každý AI úsek kódu označ komentářem s konkrétním promptem.
- Zapisuj nástroj, model, způsob použití a ověření, chyby a odmítnuté návrhy.
- Vše převzaté musíš umět vysvětlit a ověřit.
- Zachovej skutečné autorství a citace. Platí také pravidla VUT pro AI.

## Před mergem

- [ ] Změna odpovídá issue a specifikaci.
- [ ] Prošel jsem diff; neobsahuje nesouvisející změny ani generované soubory.
- [ ] Kód se přeloží a projde `make check`.
- [ ] Doplnil jsem testy; u změn překladače prošly plné testy s `ic26int`.
- [ ] Zkontroloval jsem paměť a chybové cesty, kterých se změna týká.
- [ ] Komentáře, hlavičky a návody odpovídají změně; `make docs` projde.
- [ ] AI úseky a převzaté zdroje jsou označené.
- [ ] PR obsahuje konkrétní ověření, má zelené CI a schválené review.

U změn samotného textu stačí ověřit odkazy, náhled a správnost obsahu; napiš to do PR.
