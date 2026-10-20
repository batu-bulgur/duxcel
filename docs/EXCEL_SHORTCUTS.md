# Excel shortcuts — feature and interaction inventory

This is an **editable feature inventory** starting from Excel shortcuts. Each shortcut expresses a user intent; platform alternatives and repeated references point to the same `F…` feature through separate shortcut records. Completing a feature includes its menu/ribbon entry, data operation, undo, persistence and acceptance behavior, as applicable.

Source: [Microsoft — Keyboard shortcuts in Excel](https://support.microsoft.com/en-us/accessibility/excel/keyboard-shortcuts-in-excel), accessed **20 October 2026**. Key combinations and platform distinctions come from that reference. Command labels, Duxcel mappings, priorities and acceptance cards are authored for this project. The source article is not reproduced. Keep IDs stable when the source changes: append new records and retire removed entries with a reason instead of renumbering them.

## Scope and tracking

Initial inventory: **333 feature records and 745 shortcut/context records** — Mac 245, Windows 297, web 157, iOS 32, Android 14. These counts include source repetitions and contexts expanded from Windows summary tables; they are not 745 independent features.

Mac is the primary native target. Windows and web records support portability and feature planning. iOS/Android entries are reference material, not a mobile-target commitment. All shortcut tables on the source page are represented; Windows Function/Other rows containing multiple behaviors are expanded into separate mappings. OS/Fn/layout caveats outside the tables are recorded below. The browser shortcut-override table describes a host setting rather than a key combination; check it separately during web acceptance.

**Feature status** belongs in the shared feature table: `PRESENT` means an implemented source path exists; `PARTIAL` means a bounded subset or known behavior difference; `MISSING` means the capability is absent from the principal path inspected; `REVIEW` means feature-level assessment is pending; `SCOPE DECISION` means an Excel area has not been accepted into product scope; `OS` means host responsibility. `PRESENT` and `PARTIAL` do not establish physical-input or product acceptance. No product tests ran for this inventory; source inspection bounds current-behavior claims.

**Shortcut status** is tracked separately for every mapping: `REVIEW`, `DIFFERENT` for known Duxcel assignment differences, `AMBIGUOUS` for source problems, `OS`, and eventually `BOUND` or `VERIFIED`. Finding a Primary-key path does not establish that the reference's Control alternative works on Mac. Two bindings for the same feature can have different acceptance results. Before marking `VERIFIED`, add platform/build, starting state, actual input, expected/observed result and evidence to its card.

The feature table's **UI hierarchy** is an initial mapping against the current guide/design. `[candidate]` and unreviewed placements are design proposals. This column does not establish an Excel ribbon location. The shortcut page does not provide every command's ribbon path, so the separate **Excel ribbon/UI path** field remains `Unverified`. Grid navigation and OS actions need not have ribbon buttons. The icon field accepts semantic icon/asset and appearance notes; `—` means not designed. Do not modify generated brand derivatives for this inventory.

Single ownership: feature status lives here, context-specific binding/acceptance in the shortcut record, ordering and dependencies in [ROADMAP](ROADMAP.md#excel-compatibility-and-feature-inventory), and general product limits in [STATUS](STATUS.md). [INTERACTIONS](design/INTERACTIONS.md) owns shared behavior rules; [DESIGN](design/DESIGN.md) owns visual language and workspace regions. [ARCHITECTURE](ARCHITECTURE.md) explains implementation and ownership. This shortcut reference is not the entire product backlog: autosave/recovery, source overlays and SQL/Marks features remain roadmap work even without shortcuts.

## Initial behaviors and known differences

| Card | Priority | Work | Current Duxcel behavior / decision boundary |
| --- | --- | --- | --- |
| C01 | P0 | Data boundaries with Cmd+Arrow / Shift+Cmd+Arrow | Grid uses ready object/result bounds; manual occupied/empty boundaries are missing. A key mapping change alone is insufficient. |
| C02 | P0 | Separate Find, Go To, filter and Focus | Cmd+F opens filter rather than Find. Cmd+Shift+F is Focus rather than Excel Mac filter. F5 refreshes analysis rather than Go To. New assignments need a decision. |
| C03 | P0 | Cell editing lifecycle and selection | F2, Enter/Tab, Shift reverse directions and Escape; accept manual ranges, read-only sources and object titles separately. |
| C04 | P0 | Clipboard, clear, insert/delete and undo | Clear is distinct from deleting cells; TSV paste from Paste Special; axis edits from shifting cells. Require one atomic command/undo/data result. |
| C05 | P0 | Save, Save As, Close and Quit | Native dirty guard exists. F12 conflicts with diagnostics; browser workbook IO and crash recovery remain open. |
| C06 | P1 | Ribbon, menus, focus and disabled states | Pointer/keytip/shortcut invoke the same command; preserve access in narrow/overflow/Focus layouts. Keyboard access is not screen-reader acceptance. |
| C07 | P1 | Fill, structure and formatting | Fill down/right, typed values, formula reference movement, hide/unhide and format scope are separate work items. |
| C08 | P2 | Formulas and advanced Excel areas | No general formula engine. VBA/Power Pivot/Copilot/drawing have no product-scope decision; do not block basic grid work on them. |

This ordering applies to this inventory. Data-loss/recovery work and existing persistence dependencies remain on the roadmap.

W01 in [ROADMAP.md](ROADMAP.md#workbook-trd-implementation-slices) delivers the manual-range gesture subset of C03/C04: one outer selection outline, border or Option/Alt-interior move, grabbed-cell anchoring, click threshold, Escape/no-op and existing atomic Undo/Redo/destination guards. It does not establish Shift insert-drop (F154), general formulas or floating analytical placement. Physical-device/human acceptance remains separate from synthetic App input and C checks.

### Source ambiguities and platform rules

- `MAC-15-11`: the previous-sheet row also says Ctrl+PageDown; Option+Left is an alternative. This possible documentation error has not been silently changed to Ctrl+PageUp.
- `MAC-20-01`: the Copy row gives Ctrl+V as an alternative, while the frequently-used table gives Ctrl+C. Do not implement Ctrl+V as Copy.
- `MAC-16-20`: the source action cell for Ctrl+E is empty. Another platform's Flash Fill entry does not establish the Mac behavior.
- `WEB-34-06`: the source assigns Esc to the next-menu-command action. Verify the semantic/key mismatch in Excel.
- Mac Fn/media settings can change F1–F12 delivery. Punctuation from the US-layout reference is not verified against a physical Turkish keyboard. Cmd, Ctrl and Option are distinct modifiers; do not bulk-convert Ctrl to Cmd.
- Leave browser Cmd/Ctrl+W, browser menus and other OS shortcuts to the host. Do not assume Duxcel implements Excel web's shortcut-override setting. Provide visible menu/keytip alternatives. iOS/Android reference entries do not decide native macOS bindings.
- Cmd+T means Table in the grid and reference-type cycling while editing a formula. Cmd+Return differs across data entry, comments and AI contexts. Resolve commands by focus; retain separate records for distinct contexts sharing a combination.

## Shared feature list

Each row has a stable ID. Identical command labels share a feature; similar commands with different scopes remain separate, including chart objects/chart sheets, clear/delete, Find/command search and Paste Special variants. Keep completed entries. The icon field can contain `icon: …; label: …; appearance: …`; move longer notes into a card below.

| ID | Feature | Status | Excel ribbon/UI path | Duxcel UI hierarchy / candidate placement | Icon / appearance | Limit / remaining work |
| --- | --- | --- | --- | --- | --- | --- |
| F001 | Paste | PARTIAL | Unverified | Home > Clipboard > Paste | — | Worksheet paste; imported source is read-only |
| F002 | Copy | PARTIAL | Unverified | Home > Clipboard > Copy | — | Cached/worker TSV; no complete Excel clipboard type support |
| F003 | Clear contents | PARTIAL | Unverified | Home > Editing > Clear contents | — | Manual range; distinct from deleting cells |
| F004 | Save | PARTIAL | Unverified | File > Save | — | Native .dxls; browser workbook Save unavailable |
| F005 | Undo | PARTIAL | Unverified | Edit > Undo / Home > History | — | Focused text history and worksheet history have separate scopes |
| F006 | Redo | PARTIAL | Unverified | Edit > Redo / Home > History | — | Not persistent session history |
| F007 | Cut | PARTIAL | Unverified | Home > Clipboard > Cut | — | Inspect manual-range and focused-editor boundaries |
| F008 | Bold | PARTIAL | Unverified | Home > Font > Bold | — | Cmd+B; supported format scopes; mixed/imported limitations |
| F009 | Print | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F010 | VBA editor | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F011 | Fill down | REVIEW | Unverified | Home > Clipboard / Editing [candidate] | — | Ordinary Paste or range move does not establish this feature |
| F012 | Fill right | REVIEW | Unverified | Home > Clipboard / Editing [candidate] | — | Ordinary Paste or range move does not establish this feature |
| F013 | Insert cells | PARTIAL | Unverified | Home > Cells > Insert / context | — | Axis insertion exists; shift-cells dialog is a separate feature |
| F014 | Delete cells | PARTIAL | Unverified | Home > Cells > Delete / context | — | Safe axis deletion exists; shifting cells is a separate feature |
| F015 | Calculate all workbooks | MISSING | Unverified | Formulas > Calculate all workbooks [candidate] | — | No general cell formula engine; SQL analytics is not equivalent |
| F016 | Close window | PARTIAL | Unverified | File > Close | — | Native Cmd+W dirty guard; browser Cmd+W belongs to host |
| F017 | Quit application | PRESENT | Unverified | Duxcel > Quit | — | Native Cmd+Q; does not imply crash recovery |
| F018 | Go To | MISSING | Unverified | Home > Editing > Go To / address | — | F5 currently refreshes analysis; Move destination is not Go To |
| F019 | Format Cells | REVIEW | Unverified | Home > Font / Alignment / Number [candidate] | — | Map existing formatting options and scopes individually |
| F020 | Find and Replace | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F021 | Paste Special | REVIEW | Unverified | Home > Clipboard / Editing [candidate] | — | Ordinary Paste or range move does not establish this feature |
| F022 | Underline | PARTIAL | Unverified | Home > Font > Underline | — | Cmd+U; distinguish editor and formatting focus |
| F023 | Italic | PARTIAL | Unverified | Home > Font > Italic | — | Cmd+I; scope limited by the format model |
| F024 | New workbook | PRESENT | Unverified | File > New | — | Dirty guard exists; untitled crash recovery remains open |
| F025 | Workbook from template | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F026 | Save As | PARTIAL | Unverified | File > Save As | — | Native; F12 conflicts with diagnostics |
| F027 | Help | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F028 | Select All | PARTIAL | Unverified | Edit > Select All / Grid | — | Containing-region ladder; not all Excel contexts accepted |
| F029 | Toggle filter | PARTIAL | Unverified | Data > Filter > Toggle | — | Filter infrastructure exists; Cmd+Shift+F conflicts with Focus |
| F030 | Ribbon display | PARTIAL | Unverified | View > Ribbon display | — | Ctrl+F1 menu; Excel Mac Cmd+Option+R binding is separate |
| F031 | Open file | PARTIAL | Unverified | File > Open | — | Native workbook; browser workbook Open unavailable; source import separate |
| F032 | Spelling | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F033 | Thesaurus | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F034 | Insert function | MISSING | Unverified | Formulas > Insert function [candidate] | — | No general cell formula engine; SQL analytics is not equivalent |
| F035 | Define name | REVIEW | Unverified | Formulas [candidate] | — | Keep SQL queries distinct from cell formulas |
| F036 | Add or reply to comment | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F037 | Create names from selection | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F038 | Insert worksheet | PRESENT | Unverified | Insert > Worksheet / Workbook > + | — | Shift+F11; distinct from New workbook |
| F039 | Print preview | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F040 | Full screen | REVIEW | Unverified | Platform window / View [candidate] | — | Separate OS and application ownership |
| F041 | Next application | OS | Unverified | macOS / host | — | Leave to platform; do not intercept as an application shortcut |
| F042 | Previous application | OS | Unverified | macOS / host | — | Leave to platform; do not intercept as an application shortcut |
| F043 | System screenshot | OS | Unverified | macOS / host | — | Leave to platform; do not intercept as an application shortcut |
| F044 | Minimize window | REVIEW | Unverified | Platform window / View [candidate] | — | Separate OS and application ownership |
| F045 | Maximize or restore window | REVIEW | Unverified | Platform window / View [candidate] | — | Separate OS and application ownership |
| F046 | Hide application | OS | Unverified | macOS / host | — | Leave to platform; do not intercept as an application shortcut |
| F047 | Next control | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F048 | Previous control | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F049 | Cancel dialog | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F050 | Default button | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F051 | Close menu or dialog | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F052 | Directional cell navigation | PARTIAL | Unverified | Grid > Navigation | — | grid.c navigate; object/editor/popup focus are separate |
| F053 | Data region boundary | PARTIAL | Unverified | Grid > Navigation > Data edge | — | Manual occupied/empty boundary behavior missing; C01 |
| F054 | Beginning of row | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F055 | Beginning of sheet | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F056 | Last used cell | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F057 | Page down | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F058 | Page up | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F059 | Page right | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F060 | Page left | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F061 | Next worksheet | PARTIAL | Unverified | Workbook > Next sheet | — | Ctrl+Tab exists; Excel Mac alternatives need separate checks |
| F062 | Previous worksheet | PARTIAL | Unverified | Workbook > Previous sheet | — | Ctrl+Shift+Tab exists; source Ctrl+PageDown is questionable |
| F063 | Reveal active cell | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F064 | Find | MISSING | Unverified | Home > Editing > Find | — | Cmd+F currently opens filter; Find needs its own contract |
| F065 | Focus search | MISSING | Unverified | Home > Editing > Find | — | Cmd+F currently opens filter; C02 |
| F066 | Unlocked cells | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F067 | Horizontal scrolling | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F068 | Edit cell | PARTIAL | Unverified | Grid > Edit / context | — | F2; manual text; no source/pivot writeback |
| F069 | Commit and move down | REVIEW | Unverified | Cell editor / formula bar [candidate] | — | Verify text focus and cell navigation separately |
| F070 | Line break in cell | REVIEW | Unverified | Cell editor / formula bar [candidate] | — | Verify text focus and cell navigation separately |
| F071 | Fill selection with current entry | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F072 | Commit and move up | REVIEW | Unverified | Cell editor / formula bar [candidate] | — | Verify text focus and cell navigation separately |
| F073 | Commit and move right | REVIEW | Unverified | Cell editor / formula bar [candidate] | — | Verify text focus and cell navigation separately |
| F074 | Commit and move left | REVIEW | Unverified | Cell editor / formula bar [candidate] | — | Verify text focus and cell navigation separately |
| F075 | Cancel edit | REVIEW | Unverified | Cell editor / formula bar [candidate] | — | Verify text focus and cell navigation separately |
| F076 | Delete preceding character | REVIEW | Unverified | Cell editor / formula bar [candidate] | — | Verify text focus and cell navigation separately |
| F077 | Delete following character | REVIEW | Unverified | Cell editor / formula bar [candidate] | — | Verify text focus and cell navigation separately |
| F078 | Delete to end of line | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F079 | Directional text navigation | REVIEW | Unverified | Cell editor / formula bar [candidate] | — | Verify text focus and cell navigation separately |
| F080 | Beginning of text line | REVIEW | Unverified | Cell editor / formula bar [candidate] | — | Verify text focus and cell navigation separately |
| F081 | Insert note | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F082 | Edit note | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F083 | Insert comment | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F084 | Reply to comment | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F085 | Unnamed source action: Ctrl+E | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F086 | Formula tooltips | MISSING | Unverified | Formulas > Formula tooltips [candidate] | — | No general cell formula engine; SQL analytics is not equivalent |
| F087 | Formula bar size | REVIEW | Unverified | Formulas [candidate] | — | Keep SQL queries distinct from cell formulas |
| F088 | Backspace while editing | REVIEW | Unverified | Cell editor / formula bar [candidate] | — | Verify text focus and cell navigation separately |
| F089 | Confirm entry | REVIEW | Unverified | Cell editor / formula bar [candidate] | — | Verify text focus and cell navigation separately |
| F090 | Array formula | MISSING | Unverified | Formulas > Array formula [candidate] | — | No general cell formula engine; SQL analytics is not equivalent |
| F091 | Function arguments | MISSING | Unverified | Formulas > Function arguments [candidate] | — | No general cell formula engine; SQL analytics is not equivalent |
| F092 | Insert hyperlink | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F093 | Edit at end of cell | REVIEW | Unverified | Cell editor / formula bar [candidate] | — | Verify text focus and cell navigation separately |
| F094 | Calculate active worksheet | MISSING | Unverified | Formulas > Calculate active worksheet [candidate] | — | No general cell formula engine; SQL analytics is not equivalent |
| F095 | Context menu | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F096 | Start formula | MISSING | Unverified | Formulas > Start formula [candidate] | — | No general cell formula engine; SQL analytics is not equivalent |
| F097 | Cycle reference type | MISSING | Unverified | Formulas > Cycle reference type [candidate] | — | No general cell formula engine; SQL analytics is not equivalent |
| F098 | AutoSum | MISSING | Unverified | Formulas > AutoSum [candidate] | — | No general cell formula engine; SQL analytics is not equivalent |
| F099 | Current date | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F100 | Current time | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F101 | Value from cell above | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F102 | Show formulas | MISSING | Unverified | Formulas > Show formulas [candidate] | — | No general cell formula engine; SQL analytics is not equivalent |
| F103 | Formula from cell above | MISSING | Unverified | Formulas > Formula from cell above [candidate] | — | No general cell formula engine; SQL analytics is not equivalent |
| F104 | AutoComplete | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F105 | Smart Lookup | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F106 | Full recalculation | MISSING | Unverified | Formulas > Full recalculation [candidate] | — | No general cell formula engine; SQL analytics is not equivalent |
| F107 | Rebuild dependencies and calculate | MISSING | Unverified | Formulas > Rebuild dependencies and calculate [candidate] | — | No general cell formula engine; SQL analytics is not equivalent |
| F108 | Create table | PARTIAL | Unverified | Insert > Table | — | Source table import exists; manual range-to-Table conversion is separate |
| F109 | System character picker | OS | Unverified | macOS / host | — | Leave to platform; do not intercept as an application shortcut |
| F110 | Increase font size | REVIEW | Unverified | Home > Font / Alignment / Number [candidate] | — | Map existing formatting options and scopes individually |
| F111 | Decrease font size | REVIEW | Unverified | Home > Font / Alignment / Number [candidate] | — | Map existing formatting options and scopes individually |
| F112 | Center alignment | REVIEW | Unverified | Home > Font / Alignment / Number [candidate] | — | Map existing formatting options and scopes individually |
| F113 | Left alignment | REVIEW | Unverified | Home > Font / Alignment / Number [candidate] | — | Map existing formatting options and scopes individually |
| F114 | Cell style | REVIEW | Unverified | Home > Font / Alignment / Number [candidate] | — | Map existing formatting options and scopes individually |
| F115 | General number format | REVIEW | Unverified | Home > Font / Alignment / Number [candidate] | — | Map existing formatting options and scopes individually |
| F116 | Currency format | REVIEW | Unverified | Home > Font / Alignment / Number [candidate] | — | Map existing formatting options and scopes individually |
| F117 | Percentage format | REVIEW | Unverified | Home > Font / Alignment / Number [candidate] | — | Map existing formatting options and scopes individually |
| F118 | Scientific format | REVIEW | Unverified | Home > Font / Alignment / Number [candidate] | — | Map existing formatting options and scopes individually |
| F119 | Date format | REVIEW | Unverified | Home > Font / Alignment / Number [candidate] | — | Map existing formatting options and scopes individually |
| F120 | Time format | REVIEW | Unverified | Home > Font / Alignment / Number [candidate] | — | Map existing formatting options and scopes individually |
| F121 | Number format | REVIEW | Unverified | Home > Font / Alignment / Number [candidate] | — | Map existing formatting options and scopes individually |
| F122 | Outline border | REVIEW | Unverified | Home > Font / Alignment / Number [candidate] | — | Map existing formatting options and scopes individually |
| F123 | Right border | REVIEW | Unverified | Home > Font / Alignment / Number [candidate] | — | Map existing formatting options and scopes individually |
| F124 | Left border | REVIEW | Unverified | Home > Font / Alignment / Number [candidate] | — | Map existing formatting options and scopes individually |
| F125 | Top border | REVIEW | Unverified | Home > Font / Alignment / Number [candidate] | — | Map existing formatting options and scopes individually |
| F126 | Bottom border | REVIEW | Unverified | Home > Font / Alignment / Number [candidate] | — | Map existing formatting options and scopes individually |
| F127 | Remove border | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F128 | Strikethrough | REVIEW | Unverified | Home > Font / Alignment / Number [candidate] | — | Map existing formatting options and scopes individually |
| F129 | Hide column | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F130 | Unhide column | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F131 | Hide row | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F132 | Unhide row | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F133 | Paste while editing | REVIEW | Unverified | Cell editor / formula bar [candidate] | — | Verify text focus and cell navigation separately |
| F134 | Extend selection by one cell | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F135 | Extend selection to data boundary | PARTIAL | Unverified | Grid > Selection > Data edge | — | Manual occupancy resolver missing; C01 |
| F136 | Extend selection to row start | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F137 | Extend selection to sheet start | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F138 | Extend selection to last used cell | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F139 | Select column | PARTIAL | Unverified | Grid > Headers > Select column | — | Mac reference uses Ctrl+Space; Duxcel uses Primary+Space |
| F140 | Select row | PRESENT | Unverified | Grid > Headers > Select row | — | Shift+Space path; physical input acceptance pending |
| F141 | Visible cells only | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F142 | Reduce selection to active cell | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F143 | Extend selection one page down | REVIEW | Unverified | Platform window / View [candidate] | — | Separate OS and application ownership |
| F144 | Extend selection one page up | REVIEW | Unverified | Platform window / View [candidate] | — | Separate OS and application ownership |
| F145 | Object visibility | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F146 | Extend selection mode | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F147 | Add nonadjacent selection | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F148 | Select current array | REVIEW | Unverified | Formulas [candidate] | — | Keep SQL queries distinct from cell formulas |
| F149 | Select row differences | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F150 | Select direct precedents | MISSING | Unverified | Formulas > Select direct precedents [candidate] | — | No general cell formula engine; SQL analytics is not equivalent |
| F151 | Select all precedents | MISSING | Unverified | Formulas > Select all precedents [candidate] | — | No general cell formula engine; SQL analytics is not equivalent |
| F152 | Select direct dependents | MISSING | Unverified | Formulas > Select direct dependents [candidate] | — | No general cell formula engine; SQL analytics is not equivalent |
| F153 | Select all dependents | MISSING | Unverified | Formulas > Select all dependents [candidate] | — | No general cell formula engine; SQL analytics is not equivalent |
| F154 | Drag and insert at destination | PARTIAL | Unverified | Grid > Range border / Move | — | Range move exists; Shift insert-drop is a separate behavior |
| F155 | Move down within selection | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F156 | Move up within selection | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F157 | Move right within selection | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F158 | Move left within selection | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F159 | Cycle selection corner | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F160 | Group | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F161 | Ungroup | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F162 | Insert chart sheet | MISSING | Unverified | Insert > Chart sheet [candidate] | — | A same-sheet chart object does not provide this capability |
| F163 | Navigate chart elements | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F164 | Multi-column sort | PARTIAL | Unverified | Data > Sort > Multiple columns | — | Real worker SQL; inspect Excel binding separately |
| F165 | Field filter menu | PARTIAL | Unverified | Data > Filter / column header | — | Alt+Down path exists; pivot page-field equivalence is unproved |
| F166 | Outline visibility | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F167 | AI: send request | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F168 | AI: cancel | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F169 | AI: accept changes | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F170 | AI: switch mode | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F171 | AI: navigate responses | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F172 | AI: confirm files | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F173 | AI: dictation | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F174 | AI: open pane | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F175 | AI: grid/pane focus | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F176 | Insert or edit note | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F177 | Save dialog | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F178 | Close window or dialog | REVIEW | Unverified | Platform window / View [candidate] | — | Separate OS and application ownership |
| F179 | Search Sheet dialog | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F180 | Cycle region focus | PARTIAL | Unverified | Workspace > Focus traversal | — | F6 / Shift+F6; keyboard focus is not screen-reader support |
| F181 | Macros | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F182 | Action options menu | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F183 | Legacy macro sheet | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F184 | Power Query editor | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F185 | Drawing mode | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F186 | Home tab | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F187 | Fill color | REVIEW | Unverified | Home > Font / Alignment / Number [candidate] | — | Map existing formatting options and scopes individually |
| F188 | Insert tab | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F189 | Page Layout tab | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F190 | Data tab | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F191 | View tab | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F192 | Border menu | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F193 | Delete column | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F194 | Formulas tab | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F195 | Command search | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F196 | File menu | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F197 | Review tab | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F198 | Start keytips | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F199 | Ribbon control focus | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F200 | Directional ribbon navigation | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F201 | Focused control tooltip | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F202 | Activate button | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F203 | Open command list | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F204 | Open button menu | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F205 | Next menu command | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F206 | Enter submenu | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F207 | Change ribbon group | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F208 | Previous cell or control | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F209 | Cell up | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F210 | Cell down | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F211 | Cell left | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F212 | Cell right | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F213 | End navigation mode | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F214 | Scroll Lock viewport start | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F215 | Next unlocked cell | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F216 | Data validation list | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F217 | Floating object focus | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F218 | Exit object focus | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F219 | Zoom in | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F220 | Zoom out | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F221 | Font dialog | REVIEW | Unverified | Home > Font / Alignment / Number [candidate] | — | Map existing formatting options and scopes individually |
| F222 | Quick Analysis | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F223 | Workbook statistics | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F224 | Paste Special: all | REVIEW | Unverified | Home > Clipboard / Editing [candidate] | — | Ordinary Paste or range move does not establish this feature |
| F225 | Paste Special: formulas | REVIEW | Unverified | Home > Clipboard / Editing [candidate] | — | Ordinary Paste or range move does not establish this feature |
| F226 | Paste Special: values | REVIEW | Unverified | Home > Clipboard / Editing [candidate] | — | Ordinary Paste or range move does not establish this feature |
| F227 | Paste Special: formats | REVIEW | Unverified | Home > Clipboard / Editing [candidate] | — | Ordinary Paste or range move does not establish this feature |
| F228 | Paste Special: comments and notes | REVIEW | Unverified | Home > Clipboard / Editing [candidate] | — | Ordinary Paste or range move does not establish this feature |
| F229 | Paste Special: validation | REVIEW | Unverified | Home > Clipboard / Editing [candidate] | — | Ordinary Paste or range move does not establish this feature |
| F230 | Paste Special: all with theme | REVIEW | Unverified | Home > Clipboard / Editing [candidate] | — | Ordinary Paste or range move does not establish this feature |
| F231 | Paste Special: without borders | REVIEW | Unverified | Home > Clipboard / Editing [candidate] | — | Ordinary Paste or range move does not establish this feature |
| F232 | Paste Special: column widths | REVIEW | Unverified | Home > Clipboard / Editing [candidate] | — | Ordinary Paste or range move does not establish this feature |
| F233 | Paste Special: formulas and number formats | REVIEW | Unverified | Home > Clipboard / Editing [candidate] | — | Ordinary Paste or range move does not establish this feature |
| F234 | Paste Special: values and number formats | REVIEW | Unverified | Home > Clipboard / Editing [candidate] | — | Ordinary Paste or range move does not establish this feature |
| F235 | Select entire sheet | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F236 | Add next sheet to selection | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F237 | Add previous sheet to selection | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F238 | Select all objects | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F239 | Select current region | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F240 | First menu command | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F241 | Repeat last action | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F242 | Expand group | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F243 | Collapse group | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F244 | End of text | REVIEW | Unverified | Cell editor / formula bar [candidate] | — | Verify text focus and cell navigation separately |
| F245 | Select to end of text | REVIEW | Unverified | Cell editor / formula bar [candidate] | — | Verify text focus and cell navigation separately |
| F246 | Error options | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F247 | Insert argument names | MISSING | Unverified | Formulas > Insert argument names [candidate] | — | No general cell formula engine; SQL analytics is not equivalent |
| F248 | Flash Fill | REVIEW | Unverified | Home > Clipboard / Editing [candidate] | — | Ordinary Paste or range move does not establish this feature |
| F249 | Insert chart | PARTIAL | Unverified | Insert > Chart / Analyze | — | Same-sheet object; Excel chart sheet is a separate capability |
| F250 | Paste name | REVIEW | Unverified | Formulas [candidate] | — | Keep SQL queries distinct from cell formulas |
| F251 | Next data form record | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F252 | Cancel refresh | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F253 | Refresh active sheet data | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F254 | Refresh all data | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F255 | Power Pivot: context menu | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F256 | Power Pivot: select table | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F257 | Power Pivot: copy | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F258 | Power Pivot: delete table | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F259 | Power Pivot: move table | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F260 | Power Pivot: rename table | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F261 | Power Pivot: save | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F262 | Power Pivot: redo | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F263 | Power Pivot: undo | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F264 | Power Pivot: select column | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F265 | Power Pivot: select row | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F266 | Power Pivot: select to column end | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F267 | Power Pivot: select to column start | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F268 | Power Pivot: select to row end | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F269 | Power Pivot: select to row start | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F270 | Power Pivot: previous table | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F271 | Power Pivot: next table | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F272 | Power Pivot: table start | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F273 | Power Pivot: table end | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F274 | Power Pivot: row start | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F275 | Power Pivot: row end | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F276 | Power Pivot: column start | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F277 | Power Pivot: column end | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F278 | Power Pivot: cancel | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F279 | Power Pivot: filter | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F280 | Power Pivot: Go To | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F281 | Power Pivot: calculate | SCOPE DECISION | Unverified | Excel-specific area [product scope undecided] | — | Not a current Duxcel feature or automatic scope commitment |
| F282 | Formula editor focus | REVIEW | Unverified | Formulas [candidate] | — | Keep SQL queries distinct from cell formulas |
| F283 | Restore window | REVIEW | Unverified | Platform window / View [candidate] | — | Separate OS and application ownership |
| F284 | Next workbook window | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F285 | Navigate workbook windows | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F286 | Move window | REVIEW | Unverified | Platform window / View [candidate] | — | Separate OS and application ownership |
| F287 | Resize window | REVIEW | Unverified | Platform window / View [candidate] | — | Separate OS and application ownership |
| F288 | Page Layout view | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F289 | Normal view | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F290 | Page Break Preview | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F291 | Change ribbon tab | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F292 | Submenu navigation | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F293 | Directional menu navigation | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F294 | Dialog option navigation | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F295 | Last menu command | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F296 | Activate button or checkbox | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F297 | Window system menu | REVIEW | Unverified | Platform window / View [candidate] | — | Separate OS and application ownership |
| F298 | Next dialog tab or workbook window | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F299 | Previous dialog tab or workbook window | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F300 | Cell or page down | REVIEW | Unverified | Platform window / View [candidate] | — | Separate OS and application ownership |
| F301 | Cell or page up | REVIEW | Unverified | Platform window / View [candidate] | — | Separate OS and application ownership |
| F302 | Next match | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F303 | Previous match | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F304 | Insert row above | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F305 | Insert column left | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F306 | Apply filter | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F307 | Reapply filter | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F308 | Clear and edit | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F309 | End of text line | REVIEW | Unverified | Cell editor / formula bar [candidate] | — | Verify text focus and cell navigation separately |
| F310 | Extend text selection right | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F311 | Select to text start | REVIEW | Unverified | Cell editor / formula bar [candidate] | — | Verify text focus and cell navigation separately |
| F312 | Extend text selection left | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F313 | Open object menu | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F314 | Close object menu | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F315 | Follow hyperlink | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F316 | Rotate object left | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F317 | Rotate object right | REVIEW | Unverified | Placement decision pending | — | Inspect feature behavior and input mapping |
| F318 | Next nonadjacent range | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F319 | Previous nonadjacent range | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F320 | Region control focus | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F321 | Switch worksheet | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F322 | Read row header | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F323 | Read row before active cell | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F324 | Read row after active cell | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F325 | Read column header | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F326 | Read column before active cell | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F327 | Read column after active cell | REVIEW | Unverified | Grid / Workbook [candidate] | — | Separate manual/source/Table/Pivot contexts |
| F328 | Dialog navigation options | REVIEW | Unverified | Ribbon / menus / focus [candidate] | — | Inspect focus and enabled conditions |
| F329 | Beginning of text | REVIEW | Unverified | Cell editor / formula bar [candidate] | — | Verify text focus and cell navigation separately |
| F330 | Previous paragraph | REVIEW | Unverified | Cell editor / formula bar [candidate] | — | Verify text focus and cell navigation separately |
| F331 | Next paragraph | REVIEW | Unverified | Cell editor / formula bar [candidate] | — | Verify text focus and cell navigation separately |
| F332 | Next word | REVIEW | Unverified | Cell editor / formula bar [candidate] | — | Verify text focus and cell navigation separately |
| F333 | Previous word | REVIEW | Unverified | Cell editor / formula bar [candidate] | — | Verify text focus and cell navigation separately |

## Shortcut records

`MAC/WIN/WEB/IOS/AND` identifies the platform. The next two numbers identify the initial source section and row. WIN-11 and WIN-12 use record order after expanding multi-behavior source rows. Keep these IDs stable through future source reorderings. `/` separates Excel alternatives; `,` separates sequential keytip steps. Alternatives initially share one behavior record. If alternatives receive different test results, split them into `.a/.b` records linked to the original ID. Apart from the four ambiguities above, this inventory makes no claim to correct the reference's key assignments.

### MAC · Frequently used

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| MAC-13-01 | F001 · Paste | Cmd+V / Ctrl+V | REVIEW |
| MAC-13-02 | F002 · Copy | Cmd+C / Ctrl+C | REVIEW |
| MAC-13-03 | F003 · Clear contents | Delete | REVIEW |
| MAC-13-04 | F004 · Save | Cmd+S / Ctrl+S | REVIEW |
| MAC-13-05 | F005 · Undo | Cmd+Z / Ctrl+Z | REVIEW |
| MAC-13-06 | F006 · Redo | Cmd+Y / Ctrl+Y / Cmd+Shift+Z | REVIEW |
| MAC-13-07 | F007 · Cut | Cmd+X / Ctrl+X / Shift+ ForwardDelete | REVIEW |
| MAC-13-08 | F008 · Bold | Cmd+B / Ctrl+B | REVIEW |
| MAC-13-09 | F009 · Print | Cmd+P / Ctrl+P | REVIEW |
| MAC-13-10 | F010 · VBA editor | Option+F11 | REVIEW |
| MAC-13-11 | F011 · Fill down | Cmd+D / Ctrl+D | REVIEW |
| MAC-13-12 | F012 · Fill right | Cmd+R / Ctrl+R | REVIEW |
| MAC-13-13 | F013 · Insert cells | Ctrl+Shift+= | REVIEW |
| MAC-13-14 | F014 · Delete cells | Cmd+- / Ctrl+- | REVIEW |
| MAC-13-15 | F015 · Calculate all workbooks | Cmd+= / F9 | REVIEW |
| MAC-13-16 | F016 · Close window | Cmd+W / Ctrl+W | REVIEW |
| MAC-13-17 | F017 · Quit application | Cmd+Q | REVIEW |
| MAC-13-18 | F018 · Go To | Ctrl+G / F5 | DIFFERENT — see feature note |
| MAC-13-19 | F019 · Format Cells | Cmd+1 / Ctrl+1 | REVIEW |
| MAC-13-20 | F020 · Find and Replace | Ctrl+H / Cmd+Shift+H | REVIEW |
| MAC-13-21 | F021 · Paste Special | Cmd+Ctrl+V / Ctrl+Option+V / Cmd+Option+V | REVIEW |
| MAC-13-22 | F022 · Underline | Cmd+U | REVIEW |
| MAC-13-23 | F023 · Italic | Cmd+I / Ctrl+I | REVIEW |
| MAC-13-24 | F024 · New workbook | Cmd+N / Ctrl+N | REVIEW |
| MAC-13-25 | F025 · Workbook from template | Cmd+Shift+P | REVIEW |
| MAC-13-26 | F026 · Save As | Cmd+Shift+S / F12 | DIFFERENT — see feature note |
| MAC-13-27 | F027 · Help | F1 / Cmd+/ | REVIEW |
| MAC-13-28 | F028 · Select All | Cmd+A / Cmd+Shift+Space | REVIEW |
| MAC-13-29 | F029 · Toggle filter | Cmd+Shift+F / Ctrl+Shift+L | DIFFERENT — see feature note |
| MAC-13-30 | F030 · Ribbon display | Cmd+Option+R | DIFFERENT — see feature note |
| MAC-13-31 | F031 · Open file | Cmd+O / Ctrl+O | REVIEW |
| MAC-13-32 | F032 · Spelling | F7 | REVIEW |
| MAC-13-33 | F033 · Thesaurus | Shift+F7 | REVIEW |
| MAC-13-34 | F034 · Insert function | Shift+F3 | REVIEW |
| MAC-13-35 | F035 · Define name | Cmd+F3 | REVIEW |
| MAC-13-36 | F036 · Add or reply to comment | Cmd+Return | REVIEW |
| MAC-13-37 | F037 · Create names from selection | Cmd+Shift+F3 | REVIEW |
| MAC-13-38 | F038 · Insert worksheet | Shift+F11 | REVIEW |
| MAC-13-39 | F039 · Print preview | Cmd+P / Ctrl+P | REVIEW |

### MAC · Windows and dialogs

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| MAC-14-01 | F030 · Ribbon display | Cmd+Option+R | DIFFERENT — see feature note |
| MAC-14-02 | F040 · Full screen | Cmd+Ctrl+F | REVIEW |
| MAC-14-03 | F041 · Next application | Cmd+Tab | OS |
| MAC-14-04 | F042 · Previous application | Shift+ Cmd +Tab | OS |
| MAC-14-05 | F016 · Close window | Cmd+W | REVIEW |
| MAC-14-06 | F043 · System screenshot | Shift+Cmd+3 | OS |
| MAC-14-07 | F044 · Minimize window | Ctrl+F9 | REVIEW |
| MAC-14-08 | F045 · Maximize or restore window | Ctrl+F10 / Cmd+F10 | REVIEW |
| MAC-14-09 | F046 · Hide application | Cmd+H | OS |
| MAC-14-10 | F047 · Next control | Tab | REVIEW |
| MAC-14-11 | F048 · Previous control | Shift+Tab | REVIEW |
| MAC-14-12 | F049 · Cancel dialog | Esc | REVIEW |
| MAC-14-13 | F050 · Default button | Return | REVIEW |
| MAC-14-14 | F051 · Close menu or dialog | Esc | REVIEW |

### MAC · Grid navigation and scrolling

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| MAC-15-01 | F052 · Directional cell navigation | Arrows | REVIEW |
| MAC-15-02 | F053 · Data region boundary | Cmd+Arrow | REVIEW |
| MAC-15-03 | F054 · Beginning of row | Home; MacBook: Fn+Left | REVIEW |
| MAC-15-04 | F055 · Beginning of sheet | Ctrl+Home; MacBook: Ctrl+Fn+Left | REVIEW |
| MAC-15-05 | F056 · Last used cell | Ctrl+End; MacBook: Ctrl+Fn+Right | REVIEW |
| MAC-15-06 | F057 · Page down | PageDown; MacBook: Fn+Down | REVIEW |
| MAC-15-07 | F058 · Page up | PageUp; MacBook: Fn+Up | REVIEW |
| MAC-15-08 | F059 · Page right | Option+PageDown; MacBook: Fn+Option+Down | REVIEW |
| MAC-15-09 | F060 · Page left | Option+PageUp; MacBook: Fn+Option+Up | REVIEW |
| MAC-15-10 | F061 · Next worksheet | Ctrl+PageDown / Option+Right | DIFFERENT — see feature note |
| MAC-15-11 | F062 · Previous worksheet | Ctrl+PageDown / Option+Left | AMBIGUOUS + DIFFERENT — see source note |
| MAC-15-12 | F063 · Reveal active cell | Ctrl+Delete | REVIEW |
| MAC-15-13 | F018 · Go To | Ctrl+G | DIFFERENT — see feature note |
| MAC-15-14 | F064 · Find | Ctrl+F / Shift+F5 | DIFFERENT — see feature note |
| MAC-15-15 | F065 · Focus search | Cmd+F | DIFFERENT — see feature note |
| MAC-15-16 | F066 · Unlocked cells | Tab | REVIEW |
| MAC-15-17 | F067 · Horizontal scrolling | Shift, then scroll the mouse wheel up for left, down for right | REVIEW |

### MAC · Data entry

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| MAC-16-01 | F068 · Edit cell | F2 | REVIEW |
| MAC-16-02 | F069 · Commit and move down | Return | REVIEW |
| MAC-16-03 | F070 · Line break in cell | Option+Return or Ctrl+Option+Return | REVIEW |
| MAC-16-04 | F071 · Fill selection with current entry | Cmd+Return / Ctrl+Return | REVIEW |
| MAC-16-05 | F072 · Commit and move up | Shift+Return | REVIEW |
| MAC-16-06 | F073 · Commit and move right | Tab | REVIEW |
| MAC-16-07 | F074 · Commit and move left | Shift+Tab | REVIEW |
| MAC-16-08 | F075 · Cancel edit | Esc | REVIEW |
| MAC-16-09 | F076 · Delete preceding character | Delete | REVIEW |
| MAC-16-10 | F077 · Delete following character | ForwardDelete; MacBook: Fn+Delete | REVIEW |
| MAC-16-11 | F078 · Delete to end of line | Ctrl+ ForwardDelete; MacBook: Ctrl+Fn+Delete | REVIEW |
| MAC-16-12 | F079 · Directional text navigation | Arrows | REVIEW |
| MAC-16-13 | F080 · Beginning of text line | Home; MacBook: Fn+Left | REVIEW |
| MAC-16-14 | F081 · Insert note | Shift+F2 | REVIEW |
| MAC-16-15 | F082 · Edit note | Shift+F2 | REVIEW |
| MAC-16-16 | F083 · Insert comment | Cmd+Shift+F2 | REVIEW |
| MAC-16-17 | F084 · Reply to comment | Cmd+Shift+F2 | REVIEW |
| MAC-16-18 | F011 · Fill down | Ctrl+D / Cmd+D | REVIEW |
| MAC-16-19 | F012 · Fill right | Ctrl+R / Cmd+R | REVIEW |
| MAC-16-20 | F085 · Unnamed source action: Ctrl+E | Ctrl+E | AMBIGUOUS — see source note |
| MAC-16-21 | F035 · Define name | Ctrl+L | REVIEW |

### MAC · Cells and formula bar

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| MAC-17-01 | F086 · Formula tooltips | Ctrl+Option+P | REVIEW |
| MAC-17-02 | F068 · Edit cell | F2 | REVIEW |
| MAC-17-03 | F087 · Formula bar size | Ctrl+Shift+U | REVIEW |
| MAC-17-04 | F088 · Backspace while editing | Delete | REVIEW |
| MAC-17-05 | F089 · Confirm entry | Return | REVIEW |
| MAC-17-06 | F090 · Array formula | Shift+Cmd+Return / Ctrl+Shift+Return | REVIEW |
| MAC-17-07 | F075 · Cancel edit | Esc | REVIEW |
| MAC-17-08 | F091 · Function arguments | Ctrl+A | REVIEW |
| MAC-17-09 | F092 · Insert hyperlink | Cmd+K / Ctrl+K | REVIEW |
| MAC-17-10 | F093 · Edit at end of cell | Ctrl+U | REVIEW |
| MAC-17-11 | F034 · Insert function | Shift+F3 | REVIEW |
| MAC-17-12 | F094 · Calculate active worksheet | Shift+F9 | REVIEW |
| MAC-17-13 | F095 · Context menu | Shift+F10 | REVIEW |
| MAC-17-14 | F096 · Start formula | = | REVIEW |
| MAC-17-15 | F097 · Cycle reference type | Cmd+T / F4 | REVIEW |
| MAC-17-16 | F098 · AutoSum | Shift+Cmd+T | REVIEW |
| MAC-17-17 | F099 · Current date | Ctrl+; | REVIEW |
| MAC-17-18 | F100 · Current time | Cmd+; | REVIEW |
| MAC-17-19 | F101 · Value from cell above | Ctrl+Shift+Inch mark/Straight double quote (") | REVIEW |
| MAC-17-20 | F102 · Show formulas | Ctrl+` | REVIEW |
| MAC-17-21 | F103 · Formula from cell above | Ctrl+' | REVIEW |
| MAC-17-22 | F104 · AutoComplete | Option+Down | REVIEW |
| MAC-17-23 | F035 · Define name | Ctrl+L | REVIEW |
| MAC-17-24 | F105 · Smart Lookup | Ctrl+Option+Cmd+L | REVIEW |
| MAC-17-25 | F106 · Full recalculation | Ctrl+Option+F9 | REVIEW |
| MAC-17-26 | F107 · Rebuild dependencies and calculate | Ctrl+Option+Shift+F9 | REVIEW |

### MAC · Formatting

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| MAC-18-01 | F068 · Edit cell | F2 | REVIEW |
| MAC-18-02 | F108 · Create table | Cmd+T / Ctrl+T | REVIEW |
| MAC-18-03 | F070 · Line break in cell | Cmd+Option+Return / Ctrl+Option+Return | REVIEW |
| MAC-18-04 | F109 · System character picker | Ctrl+Cmd+Space | OS |
| MAC-18-05 | F110 · Increase font size | Shift+Cmd+&gt; | REVIEW |
| MAC-18-06 | F111 · Decrease font size | Shift+Cmd+&lt; | REVIEW |
| MAC-18-07 | F112 · Center alignment | Cmd+E | REVIEW |
| MAC-18-08 | F113 · Left alignment | Cmd+L | REVIEW |
| MAC-18-09 | F114 · Cell style | Shift+Cmd+L | REVIEW |
| MAC-18-10 | F019 · Format Cells | Cmd+1 | REVIEW |
| MAC-18-11 | F115 · General number format | Ctrl+Shift+~ | REVIEW |
| MAC-18-12 | F116 · Currency format | Ctrl+Shift+$ | REVIEW |
| MAC-18-13 | F117 · Percentage format | Ctrl+Shift+% | REVIEW |
| MAC-18-14 | F118 · Scientific format | Ctrl+Shift+^ | REVIEW |
| MAC-18-15 | F119 · Date format | Ctrl+Shift+# | REVIEW |
| MAC-18-16 | F120 · Time format | Ctrl+Shift+@ | REVIEW |
| MAC-18-17 | F121 · Number format | Ctrl+Shift+! | REVIEW |
| MAC-18-18 | F122 · Outline border | Cmd+Option+0 | REVIEW |
| MAC-18-19 | F123 · Right border | Cmd+Option+Right | REVIEW |
| MAC-18-20 | F124 · Left border | Cmd+Option+Left | REVIEW |
| MAC-18-21 | F125 · Top border | Cmd+Option+Up | REVIEW |
| MAC-18-22 | F126 · Bottom border | Cmd+Option+Down | REVIEW |
| MAC-18-23 | F127 · Remove border | Cmd+Option+Hyphen | REVIEW |
| MAC-18-24 | F008 · Bold | Cmd+B | REVIEW |
| MAC-18-25 | F023 · Italic | Cmd+I | REVIEW |
| MAC-18-26 | F022 · Underline | Cmd+U | REVIEW |
| MAC-18-27 | F128 · Strikethrough | Shift+Cmd+X | REVIEW |
| MAC-18-28 | F129 · Hide column | Cmd+) / Ctrl+) | REVIEW |
| MAC-18-29 | F130 · Unhide column | Shift+Cmd+) / Ctrl+Shift+) | REVIEW |
| MAC-18-30 | F131 · Hide row | Cmd+( / Ctrl+( | REVIEW |
| MAC-18-31 | F132 · Unhide row | Shift+Cmd+( / Ctrl+Shift+( | REVIEW |
| MAC-18-32 | F093 · Edit at end of cell | Ctrl+U | REVIEW |
| MAC-18-33 | F075 · Cancel edit | Esc | REVIEW |
| MAC-18-34 | F088 · Backspace while editing | Delete | REVIEW |
| MAC-18-35 | F133 · Paste while editing | Cmd+V | REVIEW |
| MAC-18-36 | F089 · Confirm entry | Return | REVIEW |
| MAC-18-37 | F071 · Fill selection with current entry | Cmd+Return / Ctrl+Return | REVIEW |
| MAC-18-38 | F090 · Array formula | Shift+Cmd+Return / Ctrl+Shift+Return | REVIEW |
| MAC-18-39 | F091 · Function arguments | Ctrl+A | REVIEW |

### MAC · Creating selections

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| MAC-19-01 | F134 · Extend selection by one cell | Shift+Arrow | REVIEW |
| MAC-19-02 | F135 · Extend selection to data boundary | Shift+Cmd+Arrow | REVIEW |
| MAC-19-03 | F136 · Extend selection to row start | Shift+Home; MacBook: Shift+Fn+Left | REVIEW |
| MAC-19-04 | F137 · Extend selection to sheet start | Ctrl+Shift+Home; MacBook: Ctrl+Shift+Fn+Left | REVIEW |
| MAC-19-05 | F138 · Extend selection to last used cell | Ctrl+Shift+End; MacBook: Ctrl+Shift+Fn+Right | REVIEW |
| MAC-19-06 | F139 · Select column | Ctrl+Space | DIFFERENT — see feature note |
| MAC-19-07 | F140 · Select row | Shift+Space | REVIEW |
| MAC-19-08 | F028 · Select All | Cmd+A | REVIEW |
| MAC-19-09 | F141 · Visible cells only | Shift+Cmd+Asterisk (*) | REVIEW |
| MAC-19-10 | F142 · Reduce selection to active cell | Shift+Backspace (ForwardDelete not) | REVIEW |
| MAC-19-11 | F143 · Extend selection one page down | Shift+PageDown; MacBook: Shift+Fn+Down | REVIEW |
| MAC-19-12 | F144 · Extend selection one page up | Shift+PageUp; MacBook: Shift+Fn+Up | REVIEW |
| MAC-19-13 | F145 · Object visibility | Ctrl+6 | REVIEW |
| MAC-19-14 | F146 · Extend selection mode | F8 | REVIEW |
| MAC-19-15 | F147 · Add nonadjacent selection | Shift+F8 | REVIEW |
| MAC-19-16 | F148 · Select current array | Ctrl+/ | REVIEW |
| MAC-19-17 | F149 · Select row differences | Ctrl+\ | REVIEW |
| MAC-19-18 | F150 · Select direct precedents | Ctrl+Shift+[ | REVIEW |
| MAC-19-19 | F151 · Select all precedents | Ctrl+Shift+{ | REVIEW |
| MAC-19-20 | F152 · Select direct dependents | Ctrl+] | REVIEW |
| MAC-19-21 | F153 · Select all dependents | Ctrl+Shift+} | REVIEW |

### MAC · Working with a selection

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| MAC-20-01 | F002 · Copy | Cmd+C / Ctrl+V | AMBIGUOUS — see source note |
| MAC-20-02 | F001 · Paste | Cmd+V / Ctrl+V | REVIEW |
| MAC-20-03 | F007 · Cut | Cmd+X / Ctrl+X | REVIEW |
| MAC-20-04 | F003 · Clear contents | Delete | REVIEW |
| MAC-20-05 | F014 · Delete cells | Ctrl+Hyphen | REVIEW |
| MAC-20-06 | F005 · Undo | Cmd+Z | REVIEW |
| MAC-20-07 | F129 · Hide column | Cmd+) / Ctrl+) | REVIEW |
| MAC-20-08 | F130 · Unhide column | Cmd+Shift+) / Ctrl+Shift+) | REVIEW |
| MAC-20-09 | F131 · Hide row | Cmd+( / Ctrl+( | REVIEW |
| MAC-20-10 | F132 · Unhide row | Cmd+Shift+( / Ctrl+Shift+( | REVIEW |
| MAC-20-11 | F154 · Drag and insert at destination | Shift+drag (insert at destination); drag (cut and move) | REVIEW |
| MAC-20-12 | F155 · Move down within selection | Return | REVIEW |
| MAC-20-13 | F156 · Move up within selection | Shift+Return | REVIEW |
| MAC-20-14 | F157 · Move right within selection | Tab | REVIEW |
| MAC-20-15 | F158 · Move left within selection | Shift+Tab | REVIEW |
| MAC-20-16 | F159 · Cycle selection corner | Ctrl+. | REVIEW |
| MAC-20-17 | F160 · Group | Cmd+Shift+K | REVIEW |
| MAC-20-18 | F161 · Ungroup | Cmd+Shift+J | REVIEW |

### MAC · Chart

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| MAC-21-01 | F162 · Insert chart sheet | F11 | REVIEW |
| MAC-21-02 | F163 · Navigate chart elements | Arrows | REVIEW |

### MAC · Sort, filter and pivot

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| MAC-22-01 | F164 · Multi-column sort | Cmd+Shift+R | REVIEW |
| MAC-22-02 | F029 · Toggle filter | Cmd+Shift+F / Ctrl+Shift+L | DIFFERENT — see feature note |
| MAC-22-03 | F165 · Field filter menu | Option+Down | REVIEW |

### MAC · Outline

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| MAC-23-01 | F166 · Outline visibility | Ctrl+8 | REVIEW |
| MAC-23-02 | F131 · Hide row | Ctrl+9 | REVIEW |
| MAC-23-03 | F132 · Unhide row | Ctrl+Shift+( | REVIEW |
| MAC-23-04 | F129 · Hide column | Ctrl+0 | REVIEW |
| MAC-23-05 | F130 · Unhide column | Ctrl+Shift+) | REVIEW |

### MAC · Copilot

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| MAC-24-01 | F167 · AI: send request | Return | REVIEW |
| MAC-24-02 | F168 · AI: cancel | Esc | REVIEW |
| MAC-24-03 | F169 · AI: accept changes | Cmd+Return | REVIEW |
| MAC-24-04 | F170 · AI: switch mode | Cmd+Shift+M | REVIEW |
| MAC-24-05 | F171 · AI: navigate responses | Option+C | REVIEW |
| MAC-24-06 | F172 · AI: confirm files | Cmd+J | REVIEW |
| MAC-24-07 | F173 · AI: dictation | Option+Shift+H | REVIEW |
| MAC-24-08 | F174 · AI: open pane | Cmd+Ctrl+I | REVIEW |
| MAC-24-09 | F175 · AI: grid/pane focus | Shift+F6 | REVIEW |

### MAC · Function keys

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| MAC-25-01 | F027 · Help | F1 | REVIEW |
| MAC-25-02 | F068 · Edit cell | F2 | REVIEW |
| MAC-25-03 | F176 · Insert or edit note | Shift+F2 | REVIEW |
| MAC-25-04 | F036 · Add or reply to comment | Cmd+Shift+F2 | REVIEW |
| MAC-25-05 | F177 · Save dialog | Option+F2 | REVIEW |
| MAC-25-06 | F034 · Insert function | Shift+F3 | REVIEW |
| MAC-25-07 | F035 · Define name | Cmd+F3 | REVIEW |
| MAC-25-08 | F178 · Close window or dialog | Cmd+F4 | REVIEW |
| MAC-25-09 | F018 · Go To | F5 | DIFFERENT — see feature note |
| MAC-25-10 | F064 · Find | Shift+F5 | DIFFERENT — see feature note |
| MAC-25-11 | F179 · Search Sheet dialog | Ctrl+F5 | REVIEW |
| MAC-25-12 | F180 · Cycle region focus | F6 or Shift+F6 | REVIEW |
| MAC-25-13 | F032 · Spelling | F7 | REVIEW |
| MAC-25-14 | F033 · Thesaurus | Shift+F7 / Ctrl+Option+Cmd+R | REVIEW |
| MAC-25-15 | F146 · Extend selection mode | F8 | REVIEW |
| MAC-25-16 | F147 · Add nonadjacent selection | Shift+F8 | REVIEW |
| MAC-25-17 | F181 · Macros | Option+F8 | REVIEW |
| MAC-25-18 | F015 · Calculate all workbooks | F9 | REVIEW |
| MAC-25-19 | F094 · Calculate active worksheet | Shift+F9 | REVIEW |
| MAC-25-20 | F044 · Minimize window | Ctrl+F9 | REVIEW |
| MAC-25-21 | F095 · Context menu | Shift+F10 | REVIEW |
| MAC-25-22 | F182 · Action options menu | Option+Shift+F10 | REVIEW |
| MAC-25-23 | F045 · Maximize or restore window | Ctrl+F10 / Cmd+F10 | REVIEW |
| MAC-25-24 | F162 · Insert chart sheet | F11 | REVIEW |
| MAC-25-25 | F038 · Insert worksheet | Shift+F11 | REVIEW |
| MAC-25-26 | F183 · Legacy macro sheet | Cmd+F11 | REVIEW |
| MAC-25-27 | F010 · VBA editor | Option+F11 | REVIEW |
| MAC-25-28 | F026 · Save As | F12 | DIFFERENT — see feature note |
| MAC-25-29 | F031 · Open file | Cmd+F12 | REVIEW |
| MAC-25-30 | F184 · Power Query editor | Option+F12 | REVIEW |

### MAC · Drawing

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| MAC-26-01 | F185 · Drawing mode | Cmd+Ctrl+Z | REVIEW |

### WIN · Frequently used

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WIN-00-01 | F016 · Close window | Ctrl+W | REVIEW |
| WIN-00-02 | F031 · Open file | Ctrl+O | REVIEW |
| WIN-00-03 | F186 · Home tab | Alt+H | REVIEW |
| WIN-00-04 | F004 · Save | Ctrl+S | REVIEW |
| WIN-00-05 | F002 · Copy | Ctrl+C | REVIEW |
| WIN-00-06 | F001 · Paste | Ctrl+V | REVIEW |
| WIN-00-07 | F005 · Undo | Ctrl+Z | REVIEW |
| WIN-00-08 | F003 · Clear contents | Delete | REVIEW |
| WIN-00-09 | F187 · Fill color | Alt+H, H | REVIEW |
| WIN-00-10 | F007 · Cut | Ctrl+X | REVIEW |
| WIN-00-11 | F188 · Insert tab | Alt+N | REVIEW |
| WIN-00-12 | F008 · Bold | Ctrl+B | REVIEW |
| WIN-00-13 | F112 · Center alignment | Alt+H, A, C | REVIEW |
| WIN-00-14 | F189 · Page Layout tab | Alt+P | REVIEW |
| WIN-00-15 | F190 · Data tab | Alt+A | REVIEW |
| WIN-00-16 | F191 · View tab | Alt+W | REVIEW |
| WIN-00-17 | F095 · Context menu | Shift+F10 or / Menu | REVIEW |
| WIN-00-18 | F192 · Border menu | Alt+H, B | REVIEW |
| WIN-00-19 | F193 · Delete column | Alt+H, D, C | REVIEW |
| WIN-00-20 | F194 · Formulas tab | Alt+M | REVIEW |
| WIN-00-21 | F131 · Hide row | Ctrl+9 | REVIEW |
| WIN-00-22 | F129 · Hide column | Ctrl+0 | REVIEW |

### WIN · Ribbon tab access

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WIN-01-01 | F195 · Command search | Alt+Q, text | REVIEW |
| WIN-01-02 | F196 · File menu | Alt+F | REVIEW |
| WIN-01-03 | F186 · Home tab | Alt+H | REVIEW |
| WIN-01-04 | F188 · Insert tab | Alt+N | REVIEW |
| WIN-01-05 | F189 · Page Layout tab | Alt+P | REVIEW |
| WIN-01-06 | F194 · Formulas tab | Alt+M | REVIEW |
| WIN-01-07 | F190 · Data tab | Alt+A | REVIEW |
| WIN-01-08 | F197 · Review tab | Alt+R | REVIEW |
| WIN-01-09 | F191 · View tab | Alt+W | REVIEW |

### WIN · Ribbon navigation

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WIN-02-01 | F198 · Start keytips | Alt / F10; for tab Arrows | REVIEW |
| WIN-02-02 | F199 · Ribbon control focus | Tab or Shift+Tab | REVIEW |
| WIN-02-03 | F200 · Directional ribbon navigation | Arrows | REVIEW |
| WIN-02-04 | F201 · Focused control tooltip | Ctrl+Shift+F10 | REVIEW |
| WIN-02-05 | F202 · Activate button | Space or Enter | REVIEW |
| WIN-02-06 | F203 · Open command list | Down | REVIEW |
| WIN-02-07 | F204 · Open button menu | Alt+Down | REVIEW |
| WIN-02-08 | F205 · Next menu command | Down | REVIEW |
| WIN-02-09 | F030 · Ribbon display | Ctrl+F1 | REVIEW |
| WIN-02-10 | F095 · Context menu | Shift+F10 / Menu | REVIEW |
| WIN-02-11 | F206 · Enter submenu | Left | REVIEW |
| WIN-02-12 | F207 · Change ribbon group | Ctrl+Left or Right | REVIEW |

### WIN · Grid navigation

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WIN-03-01 | F208 · Previous cell or control | Shift+Tab | REVIEW |
| WIN-03-02 | F209 · Cell up | Up | REVIEW |
| WIN-03-03 | F210 · Cell down | Down | REVIEW |
| WIN-03-04 | F211 · Cell left | Left | REVIEW |
| WIN-03-05 | F212 · Cell right | Right | REVIEW |
| WIN-03-06 | F053 · Data region boundary | Ctrl+Arrow | REVIEW |
| WIN-03-07 | F213 · End navigation mode | End, Arrow | REVIEW |
| WIN-03-08 | F056 · Last used cell | Ctrl+End | REVIEW |
| WIN-03-09 | F138 · Extend selection to last used cell | Ctrl+Shift+End | REVIEW |
| WIN-03-10 | F214 · Scroll Lock viewport start | Home+Scroll lock | REVIEW |
| WIN-03-11 | F055 · Beginning of sheet | Ctrl+Home | REVIEW |
| WIN-03-12 | F057 · Page down | PageDown | REVIEW |
| WIN-03-13 | F061 · Next worksheet | Ctrl+PageDown | REVIEW |
| WIN-03-14 | F059 · Page right | Alt+PageDown | REVIEW |
| WIN-03-15 | F058 · Page up | PageUp | REVIEW |
| WIN-03-16 | F060 · Page left | Alt+PageUp | REVIEW |
| WIN-03-17 | F062 · Previous worksheet | Ctrl+PageUp | REVIEW |
| WIN-03-18 | F215 · Next unlocked cell | Tab | REVIEW |
| WIN-03-19 | F216 · Data validation list | Alt+Down | REVIEW |
| WIN-03-20 | F217 · Floating object focus | Ctrl+Alt+5, then the Tab repeatedly | REVIEW |
| WIN-03-21 | F218 · Exit object focus | Esc | REVIEW |
| WIN-03-22 | F067 · Horizontal scrolling | Ctrl+Shift, then scroll your mouse wheel up to go left, down to go right | REVIEW |
| WIN-03-23 | F219 · Zoom in | Ctrl+Alt+= | REVIEW |
| WIN-03-24 | F220 · Zoom out | Ctrl+Alt+Minus sign (-) | REVIEW |

### WIN · Cell formatting and editing

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WIN-04-01 | F019 · Format Cells | Ctrl+1 | REVIEW |
| WIN-04-02 | F221 · Font dialog | Ctrl+Shift+F | REVIEW |
| WIN-04-03 | F068 · Edit cell | F2 | REVIEW |
| WIN-04-04 | F176 · Insert or edit note | Shift+F2 / Shift+F2 | REVIEW |
| WIN-04-05 | F036 · Add or reply to comment | Ctrl+Shift+F2 / Ctrl+Shift+F2 | REVIEW |
| WIN-04-06 | F013 · Insert cells | Ctrl+Shift+Plus sign (+) | REVIEW |
| WIN-04-07 | F014 · Delete cells | Ctrl+Minus sign (-) | REVIEW |
| WIN-04-08 | F100 · Current time | Ctrl+Shift+Colon (:) | REVIEW |
| WIN-04-09 | F099 · Current date | Ctrl+; | REVIEW |
| WIN-04-10 | F102 · Show formulas | Ctrl+` | REVIEW |
| WIN-04-11 | F103 · Formula from cell above | Ctrl+' | REVIEW |
| WIN-04-12 | F007 · Cut | Ctrl+X | REVIEW |
| WIN-04-13 | F002 · Copy | Ctrl+C | REVIEW |
| WIN-04-14 | F001 · Paste | Ctrl+V | REVIEW |
| WIN-04-15 | F021 · Paste Special | Ctrl+Alt+V | REVIEW |
| WIN-04-16 | F023 · Italic | Ctrl+I or Ctrl+3 | REVIEW |
| WIN-04-17 | F008 · Bold | Ctrl+B or Ctrl+2 | REVIEW |
| WIN-04-18 | F022 · Underline | Ctrl+U or Ctrl+4 | REVIEW |
| WIN-04-19 | F128 · Strikethrough | Ctrl+5 | REVIEW |
| WIN-04-20 | F145 · Object visibility | Ctrl+6 | REVIEW |
| WIN-04-21 | F122 · Outline border | Ctrl+Shift+Ampersand sign (&) | REVIEW |
| WIN-04-22 | F127 · Remove border | Ctrl+Shift+Underscore (_) | REVIEW |
| WIN-04-23 | F166 · Outline visibility | Ctrl+8 | REVIEW |
| WIN-04-24 | F011 · Fill down | Ctrl+D | REVIEW |
| WIN-04-25 | F115 · General number format | Ctrl+Shift+Tilde sign (~) | REVIEW |
| WIN-04-26 | F116 · Currency format | Ctrl+Shift+$ | REVIEW |
| WIN-04-27 | F117 · Percentage format | Ctrl+Shift+% | REVIEW |
| WIN-04-28 | F118 · Scientific format | Ctrl+Shift+Caret sign (^) | REVIEW |
| WIN-04-29 | F119 · Date format | Ctrl+Shift+# | REVIEW |
| WIN-04-30 | F120 · Time format | Ctrl+Shift+At sign (@) | REVIEW |
| WIN-04-31 | F121 · Number format | Ctrl+Shift+! | REVIEW |
| WIN-04-32 | F092 · Insert hyperlink | Ctrl+K | REVIEW |
| WIN-04-33 | F032 · Spelling | F7 | REVIEW |
| WIN-04-34 | F222 · Quick Analysis | Ctrl+Q | REVIEW |
| WIN-04-35 | F108 · Create table | Ctrl+L or Ctrl+T | REVIEW |
| WIN-04-36 | F223 · Workbook statistics | Ctrl+Shift+G | REVIEW |

### WIN · Paste Special options

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WIN-05-01 | F224 · Paste Special: all | A | REVIEW |
| WIN-05-02 | F225 · Paste Special: formulas | F | REVIEW |
| WIN-05-03 | F226 · Paste Special: values | V | REVIEW |
| WIN-05-04 | F227 · Paste Special: formats | T | REVIEW |
| WIN-05-05 | F228 · Paste Special: comments and notes | C | REVIEW |
| WIN-05-06 | F229 · Paste Special: validation | N | REVIEW |
| WIN-05-07 | F230 · Paste Special: all with theme | H | REVIEW |
| WIN-05-08 | F231 · Paste Special: without borders | X | REVIEW |
| WIN-05-09 | F232 · Paste Special: column widths | W | REVIEW |
| WIN-05-10 | F233 · Paste Special: formulas and number formats | R | REVIEW |
| WIN-05-11 | F234 · Paste Special: values and number formats | U | REVIEW |

### WIN · Selections and actions

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WIN-06-01 | F235 · Select entire sheet | Ctrl+A or Ctrl+Shift+Space | REVIEW |
| WIN-06-02 | F236 · Add next sheet to selection | Ctrl+Shift+PageDown | REVIEW |
| WIN-06-03 | F237 · Add previous sheet to selection | Ctrl+Shift+PageUp | REVIEW |
| WIN-06-04 | F134 · Extend selection by one cell | Shift+Arrow | REVIEW |
| WIN-06-05 | F135 · Extend selection to data boundary | Ctrl+Shift+Arrow | REVIEW |
| WIN-06-06 | F146 · Extend selection mode | F8 | REVIEW |
| WIN-06-07 | F147 · Add nonadjacent selection | Shift+F8 | REVIEW |
| WIN-06-08 | F070 · Line break in cell | Alt+Enter | REVIEW |
| WIN-06-09 | F071 · Fill selection with current entry | Ctrl+Enter | REVIEW |
| WIN-06-10 | F072 · Commit and move up | Shift+Enter | REVIEW |
| WIN-06-11 | F139 · Select column | Ctrl+Space | REVIEW |
| WIN-06-12 | F140 · Select row | Shift+Space | REVIEW |
| WIN-06-13 | F238 · Select all objects | Ctrl+Shift+Space | REVIEW |
| WIN-06-14 | F137 · Extend selection to sheet start | Ctrl+Shift+Home | REVIEW |
| WIN-06-15 | F028 · Select All | Ctrl+A or Ctrl+Shift+Space | REVIEW |
| WIN-06-16 | F239 · Select current region | Ctrl+Shift+Asterisk sign (*) | REVIEW |
| WIN-06-17 | F240 · First menu command | Home | REVIEW |
| WIN-06-18 | F241 · Repeat last action | Ctrl+Y | REVIEW |
| WIN-06-19 | F005 · Undo | Ctrl+Z | REVIEW |
| WIN-06-20 | F242 · Expand group | While hovering over the collapsed items, press and hold the Shift key and scroll down. | REVIEW |
| WIN-06-21 | F243 · Collapse group | While hovering over the expanded items, press and hold the Shift key and scroll up. | REVIEW |

### WIN · Data and formulas

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WIN-07-01 | F086 · Formula tooltips | Ctrl+Alt+P | REVIEW |
| WIN-07-02 | F068 · Edit cell | F2 | REVIEW |
| WIN-07-03 | F087 · Formula bar size | Ctrl+Shift+U | REVIEW |
| WIN-07-04 | F075 · Cancel edit | Esc | REVIEW |
| WIN-07-05 | F069 · Commit and move down | Enter | REVIEW |
| WIN-07-06 | F244 · End of text | Ctrl+End | REVIEW |
| WIN-07-07 | F245 · Select to end of text | Ctrl+Shift+End | REVIEW |
| WIN-07-08 | F015 · Calculate all workbooks | F9 | REVIEW |
| WIN-07-09 | F094 · Calculate active worksheet | Shift+F9 | REVIEW |
| WIN-07-10 | F106 · Full recalculation | Ctrl+Alt+F9 | REVIEW |
| WIN-07-11 | F107 · Rebuild dependencies and calculate | Ctrl+Alt+Shift+F9 | REVIEW |
| WIN-07-12 | F246 · Error options | Alt+Shift+F10 | REVIEW |
| WIN-07-13 | F091 · Function arguments | Ctrl+A | REVIEW |
| WIN-07-14 | F247 · Insert argument names | Ctrl+Shift+A | REVIEW |
| WIN-07-15 | F098 · AutoSum | Alt+= | REVIEW |
| WIN-07-16 | F248 · Flash Fill | Ctrl+E | REVIEW |
| WIN-07-17 | F097 · Cycle reference type | F4 | REVIEW |
| WIN-07-18 | F034 · Insert function | Shift+F3 | REVIEW |
| WIN-07-19 | F101 · Value from cell above | Ctrl+Shift+Straight quotation mark (") | REVIEW |
| WIN-07-20 | F249 · Insert chart | Alt+F1 | REVIEW |
| WIN-07-21 | F162 · Insert chart sheet | F11 | REVIEW |
| WIN-07-22 | F035 · Define name | Alt+M, M, D | REVIEW |
| WIN-07-23 | F250 · Paste name | F3 | REVIEW |
| WIN-07-24 | F251 · Next data form record | Enter | REVIEW |
| WIN-07-25 | F181 · Macros | Alt+F8 | REVIEW |
| WIN-07-26 | F010 · VBA editor | Alt+F11 | REVIEW |
| WIN-07-27 | F184 · Power Query editor | Alt+F12 | REVIEW |

### WIN · External data refresh

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WIN-08-01 | F252 · Cancel refresh | Esc | REVIEW |
| WIN-08-02 | F253 · Refresh active sheet data | Ctrl+F5 | REVIEW |
| WIN-08-03 | F254 · Refresh all data | Ctrl+Alt+F5 | REVIEW |

### WIN · Power Pivot

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WIN-09-01 | F255 · Power Pivot: context menu | Shift+F10 | REVIEW |
| WIN-09-02 | F256 · Power Pivot: select table | Ctrl+A | REVIEW |
| WIN-09-03 | F257 · Power Pivot: copy | Ctrl+C | REVIEW |
| WIN-09-04 | F258 · Power Pivot: delete table | Ctrl+D | REVIEW |
| WIN-09-05 | F259 · Power Pivot: move table | Ctrl+M | REVIEW |
| WIN-09-06 | F260 · Power Pivot: rename table | Ctrl+R | REVIEW |
| WIN-09-07 | F261 · Power Pivot: save | Ctrl+S | REVIEW |
| WIN-09-08 | F262 · Power Pivot: redo | Ctrl+Y | REVIEW |
| WIN-09-09 | F263 · Power Pivot: undo | Ctrl+Z | REVIEW |
| WIN-09-10 | F264 · Power Pivot: select column | Ctrl+Space | REVIEW |
| WIN-09-11 | F265 · Power Pivot: select row | Shift+Space | REVIEW |
| WIN-09-12 | F266 · Power Pivot: select to column end | Shift+PageDown | REVIEW |
| WIN-09-13 | F267 · Power Pivot: select to column start | Shift+PageUp | REVIEW |
| WIN-09-14 | F268 · Power Pivot: select to row end | Shift+End | REVIEW |
| WIN-09-15 | F269 · Power Pivot: select to row start | Shift+Home | REVIEW |
| WIN-09-16 | F270 · Power Pivot: previous table | Ctrl+PageUp | REVIEW |
| WIN-09-17 | F271 · Power Pivot: next table | Ctrl+PageDown | REVIEW |
| WIN-09-18 | F272 · Power Pivot: table start | Ctrl+Home | REVIEW |
| WIN-09-19 | F273 · Power Pivot: table end | Ctrl+End | REVIEW |
| WIN-09-20 | F274 · Power Pivot: row start | Ctrl+Left | REVIEW |
| WIN-09-21 | F275 · Power Pivot: row end | Ctrl+Right | REVIEW |
| WIN-09-22 | F276 · Power Pivot: column start | Ctrl+Up | REVIEW |
| WIN-09-23 | F277 · Power Pivot: column end | Ctrl+Down | REVIEW |
| WIN-09-24 | F278 · Power Pivot: cancel | Ctrl+Esc | REVIEW |
| WIN-09-25 | F279 · Power Pivot: filter | Alt+Down | REVIEW |
| WIN-09-26 | F280 · Power Pivot: Go To | F5 | REVIEW |
| WIN-09-27 | F281 · Power Pivot: calculate | F9 | REVIEW |

### WIN · Copilot

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WIN-10-01 | F167 · AI: send request | Enter | REVIEW |
| WIN-10-02 | F168 · AI: cancel | Esc | REVIEW |
| WIN-10-03 | F169 · AI: accept changes | Ctrl+Enter | REVIEW |
| WIN-10-04 | F170 · AI: switch mode | Ctrl+Shift+M | REVIEW |
| WIN-10-05 | F171 · AI: navigate responses | Alt+C | REVIEW |
| WIN-10-06 | F172 · AI: confirm files | Ctrl+J | REVIEW |
| WIN-10-07 | F173 · AI: dictation | Alt+Shift+H | REVIEW |
| WIN-10-08 | F174 · AI: open pane | Alt+C | REVIEW |
| WIN-10-09 | F175 · AI: grid/pane focus | F6 | REVIEW |

### WIN · Expanded function-key bindings

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WIN-11-01 | F027 · Help | F1 | REVIEW |
| WIN-11-02 | F030 · Ribbon display | Ctrl+F1 | REVIEW |
| WIN-11-03 | F249 · Insert chart | Alt+F1 | REVIEW |
| WIN-11-04 | F038 · Insert worksheet | Alt+Shift+F1 | REVIEW |
| WIN-11-05 | F040 · Full screen | Ctrl+Shift+F1 | REVIEW |
| WIN-11-06 | F068 · Edit cell | F2 | REVIEW |
| WIN-11-07 | F176 · Insert or edit note | Shift+F2 | REVIEW |
| WIN-11-08 | F282 · Formula editor focus | Ctrl+F2 | REVIEW |
| WIN-11-09 | F250 · Paste name | F3 | REVIEW |
| WIN-11-10 | F034 · Insert function | Shift+F3 | REVIEW |
| WIN-11-11 | F241 · Repeat last action | F4 | REVIEW |
| WIN-11-12 | F097 · Cycle reference type | F4 | REVIEW |
| WIN-11-13 | F016 · Close window | Ctrl+F4 | REVIEW |
| WIN-11-14 | F017 · Quit application | Alt+F4 | REVIEW |
| WIN-11-15 | F018 · Go To | F5 | REVIEW |
| WIN-11-16 | F283 · Restore window | Ctrl+F5 | REVIEW |
| WIN-11-17 | F180 · Cycle region focus | F6 / Shift+F6 | REVIEW |
| WIN-11-18 | F284 · Next workbook window | Ctrl+F6 | REVIEW |
| WIN-11-19 | F285 · Navigate workbook windows | Ctrl+Shift+F6 | REVIEW |
| WIN-11-20 | F032 · Spelling | F7 | REVIEW |
| WIN-11-21 | F286 · Move window | Ctrl+F7 | REVIEW |
| WIN-11-22 | F146 · Extend selection mode | F8 | REVIEW |
| WIN-11-23 | F147 · Add nonadjacent selection | Shift+F8 | REVIEW |
| WIN-11-24 | F287 · Resize window | Ctrl+F8 | REVIEW |
| WIN-11-25 | F181 · Macros | Alt+F8 | REVIEW |
| WIN-11-26 | F015 · Calculate all workbooks | F9 | REVIEW |
| WIN-11-27 | F094 · Calculate active worksheet | Shift+F9 | REVIEW |
| WIN-11-28 | F106 · Full recalculation | Ctrl+Alt+F9 | REVIEW |
| WIN-11-29 | F107 · Rebuild dependencies and calculate | Ctrl+Alt+Shift+F9 | REVIEW |
| WIN-11-30 | F044 · Minimize window | Ctrl+F9 | REVIEW |
| WIN-11-31 | F198 · Start keytips | F10 | REVIEW |
| WIN-11-32 | F095 · Context menu | Shift+F10 | REVIEW |
| WIN-11-33 | F246 · Error options | Alt+Shift+F10 | REVIEW |
| WIN-11-34 | F045 · Maximize or restore window | Ctrl+F10 | REVIEW |
| WIN-11-35 | F162 · Insert chart sheet | F11 | REVIEW |
| WIN-11-36 | F038 · Insert worksheet | Shift+F11 | REVIEW |
| WIN-11-37 | F010 · VBA editor | Alt+F11 | REVIEW |
| WIN-11-38 | F026 · Save As | F12 | REVIEW |

### WIN · Expanded other-key bindings

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WIN-12-01 | F198 · Start keytips | Alt | REVIEW |
| WIN-12-02 | F288 · Page Layout view | Alt, W, P | REVIEW |
| WIN-12-03 | F289 · Normal view | Alt, W, L | REVIEW |
| WIN-12-04 | F290 · Page Break Preview | Alt, W, I | REVIEW |
| WIN-12-05 | F052 · Directional cell navigation | Arrows | REVIEW |
| WIN-12-06 | F053 · Data region boundary | Ctrl+Arrows | REVIEW |
| WIN-12-07 | F134 · Extend selection by one cell | Shift+Arrows | REVIEW |
| WIN-12-08 | F135 · Extend selection to data boundary | Ctrl+Shift+Arrows | REVIEW |
| WIN-12-09 | F291 · Change ribbon tab | Left / Right | REVIEW |
| WIN-12-10 | F292 · Submenu navigation | Left / Right | REVIEW |
| WIN-12-11 | F293 · Directional menu navigation | Up / Down | REVIEW |
| WIN-12-12 | F294 · Dialog option navigation | Arrows | REVIEW |
| WIN-12-13 | F203 · Open command list | Down / Alt+Down | REVIEW |
| WIN-12-14 | F088 · Backspace while editing | Backspace | REVIEW |
| WIN-12-15 | F003 · Clear contents | Backspace / Delete | REVIEW |
| WIN-12-16 | F077 · Delete following character | Delete | REVIEW |
| WIN-12-17 | F213 · End navigation mode | End, Arrows | REVIEW |
| WIN-12-18 | F295 · Last menu command | End | REVIEW |
| WIN-12-19 | F056 · Last used cell | Ctrl+End | REVIEW |
| WIN-12-20 | F244 · End of text | Ctrl+End | REVIEW |
| WIN-12-21 | F138 · Extend selection to last used cell | Ctrl+Shift+End | REVIEW |
| WIN-12-22 | F245 · Select to end of text | Ctrl+Shift+End | REVIEW |
| WIN-12-23 | F069 · Commit and move down | Enter | REVIEW |
| WIN-12-24 | F251 · Next data form record | Enter | REVIEW |
| WIN-12-25 | F202 · Activate button | Enter | REVIEW |
| WIN-12-26 | F050 · Default button | Enter | REVIEW |
| WIN-12-27 | F070 · Line break in cell | Alt+Enter | REVIEW |
| WIN-12-28 | F071 · Fill selection with current entry | Ctrl+Enter | REVIEW |
| WIN-12-29 | F072 · Commit and move up | Shift+Enter | REVIEW |
| WIN-12-30 | F075 · Cancel edit | Esc | REVIEW |
| WIN-12-31 | F051 · Close menu or dialog | Esc | REVIEW |
| WIN-12-32 | F054 · Beginning of row | Home | REVIEW |
| WIN-12-33 | F214 · Scroll Lock viewport start | Home | REVIEW |
| WIN-12-34 | F240 · First menu command | Home | REVIEW |
| WIN-12-35 | F055 · Beginning of sheet | Ctrl+Home | REVIEW |
| WIN-12-36 | F137 · Extend selection to sheet start | Ctrl+Shift+Home | REVIEW |
| WIN-12-37 | F057 · Page down | PageDown | REVIEW |
| WIN-12-38 | F059 · Page right | Alt+PageDown | REVIEW |
| WIN-12-39 | F061 · Next worksheet | Ctrl+PageDown | REVIEW |
| WIN-12-40 | F236 · Add next sheet to selection | Ctrl+Shift+PageDown | REVIEW |
| WIN-12-41 | F058 · Page up | PageUp | REVIEW |
| WIN-12-42 | F060 · Page left | Alt+PageUp | REVIEW |
| WIN-12-43 | F062 · Previous worksheet | Ctrl+PageUp | REVIEW |
| WIN-12-44 | F237 · Add previous sheet to selection | Ctrl+Shift+PageUp | REVIEW |
| WIN-12-45 | F154 · Drag and insert at destination | Shift+drag | REVIEW |
| WIN-12-46 | F296 · Activate button or checkbox | Space | REVIEW |
| WIN-12-47 | F139 · Select column | Ctrl+Space | REVIEW |
| WIN-12-48 | F140 · Select row | Shift+Space | REVIEW |
| WIN-12-49 | F028 · Select All | Ctrl+Shift+Space | REVIEW |
| WIN-12-50 | F238 · Select all objects | Ctrl+Shift+Space | REVIEW |
| WIN-12-51 | F297 · Window system menu | Alt+Space | REVIEW |
| WIN-12-52 | F212 · Cell right | Tab | REVIEW |
| WIN-12-53 | F066 · Unlocked cells | Tab | REVIEW |
| WIN-12-54 | F047 · Next control | Tab | REVIEW |
| WIN-12-55 | F208 · Previous cell or control | Shift+Tab | REVIEW |
| WIN-12-56 | F048 · Previous control | Shift+Tab | REVIEW |
| WIN-12-57 | F298 · Next dialog tab or workbook window | Ctrl+Tab | REVIEW |
| WIN-12-58 | F299 · Previous dialog tab or workbook window | Ctrl+Shift+Tab | REVIEW |

### WEB · Frequently used

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WEB-32-01 | F018 · Go To | Ctrl+G | REVIEW |
| WEB-32-02 | F300 · Cell or page down | PageDown or Down | REVIEW |
| WEB-32-03 | F301 · Cell or page up | PageUp or Up | REVIEW |
| WEB-32-04 | F009 · Print | Ctrl+P | REVIEW |
| WEB-32-05 | F002 · Copy | Ctrl+C | REVIEW |
| WEB-32-06 | F001 · Paste | Ctrl+V | REVIEW |
| WEB-32-07 | F007 · Cut | Ctrl+X | REVIEW |
| WEB-32-08 | F005 · Undo | Ctrl+Z | REVIEW |
| WEB-32-09 | F031 · Open file | Ctrl+O | REVIEW |
| WEB-32-10 | F016 · Close window | Ctrl+W | REVIEW |
| WEB-32-11 | F026 · Save As | Alt+F2 | REVIEW |
| WEB-32-12 | F064 · Find | Ctrl+F or Shift+F3 | REVIEW |
| WEB-32-13 | F008 · Bold | Ctrl+B | REVIEW |
| WEB-32-14 | F095 · Context menu | Shift+F10 / Menu | REVIEW |
| WEB-32-15 | F195 · Command search | Alt+Q | REVIEW |
| WEB-32-16 | F302 · Next match | Shift+F4 | REVIEW |
| WEB-32-17 | F303 · Previous match | Ctrl+Shift+F4 | REVIEW |
| WEB-32-18 | F249 · Insert chart | Alt+F1 | REVIEW |
| WEB-32-19 | F198 · Start keytips | Alt+. | REVIEW |

### WEB · Ribbon tab access

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WEB-33-01 | F195 · Command search | Alt+Win, Q, text | REVIEW |
| WEB-33-02 | F196 · File menu | Alt+Win, F | REVIEW |
| WEB-33-03 | F186 · Home tab | Alt+Win, H | REVIEW |
| WEB-33-04 | F188 · Insert tab | Alt+Win, N | REVIEW |
| WEB-33-05 | F190 · Data tab | Alt+Win, A | REVIEW |
| WEB-33-06 | F197 · Review tab | Alt+Win, R | REVIEW |
| WEB-33-07 | F191 · View tab | Alt+Win, W | REVIEW |

### WEB · Ribbon navigation

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WEB-34-01 | F198 · Start keytips | Alt+Win; access key for tab / Tab | REVIEW |
| WEB-34-02 | F199 · Ribbon control focus | Enter, then Tab / Shift+Tab | REVIEW |
| WEB-34-03 | F202 · Activate button | Space or Enter | REVIEW |
| WEB-34-04 | F203 · Open command list | Space or Enter | REVIEW |
| WEB-34-05 | F204 · Open button menu | Alt+Down | REVIEW |
| WEB-34-06 | F205 · Next menu command | Esc | AMBIGUOUS — see source note |

### WEB · Cell operations

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WEB-35-01 | F304 · Insert row above | Alt+Win, H, I, R | REVIEW |
| WEB-35-02 | F305 · Insert column left | Alt+Win, H, I, C | REVIEW |
| WEB-35-03 | F007 · Cut | Ctrl+X | REVIEW |
| WEB-35-04 | F002 · Copy | Ctrl+C | REVIEW |
| WEB-35-05 | F001 · Paste | Ctrl+V | REVIEW |
| WEB-35-06 | F005 · Undo | Ctrl+Z | REVIEW |
| WEB-35-07 | F006 · Redo | Ctrl+Y | REVIEW |
| WEB-35-08 | F070 · Line break in cell | Alt+Enter | REVIEW |
| WEB-35-09 | F092 · Insert hyperlink | Ctrl+K | REVIEW |
| WEB-35-10 | F108 · Create table | Ctrl+L | REVIEW |
| WEB-35-11 | F034 · Insert function | Shift+F3 | REVIEW |
| WEB-35-12 | F110 · Increase font size | Ctrl+Shift+&gt; | REVIEW |
| WEB-35-13 | F111 · Decrease font size | Ctrl+Shift+&lt; | REVIEW |
| WEB-35-14 | F306 · Apply filter | Alt+Win, A, T | REVIEW |
| WEB-35-15 | F307 · Reapply filter | Ctrl+Alt+L | REVIEW |
| WEB-35-16 | F029 · Toggle filter | Ctrl+Shift+L | REVIEW |

### WEB · Data entry

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WEB-36-01 | F069 · Commit and move down | Enter | REVIEW |
| WEB-36-02 | F072 · Commit and move up | Shift+Enter | REVIEW |
| WEB-36-03 | F073 · Commit and move right | Tab | REVIEW |
| WEB-36-04 | F074 · Commit and move left | Shift+Tab | REVIEW |
| WEB-36-05 | F075 · Cancel edit | Esc | REVIEW |

### WEB · Text and cell editing

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WEB-37-01 | F068 · Edit cell | F2 | REVIEW |
| WEB-37-02 | F097 · Cycle reference type | F4 | REVIEW |
| WEB-37-03 | F003 · Clear contents | Delete | REVIEW |
| WEB-37-04 | F308 · Clear and edit | Backspace | REVIEW |
| WEB-37-05 | F080 · Beginning of text line | Home | REVIEW |
| WEB-37-06 | F309 · End of text line | End | REVIEW |
| WEB-37-07 | F310 · Extend text selection right | Shift+Right | REVIEW |
| WEB-37-08 | F311 · Select to text start | Shift+Home | REVIEW |
| WEB-37-09 | F245 · Select to end of text | Shift+End | REVIEW |
| WEB-37-10 | F312 · Extend text selection left | Shift+Left | REVIEW |
| WEB-37-11 | F135 · Extend selection to data boundary | Ctrl+Shift+Right or Ctrl+Shift+Left | REVIEW |
| WEB-37-12 | F099 · Current date | Ctrl+; | REVIEW |
| WEB-37-13 | F100 · Current time | Ctrl+Shift+; | REVIEW |
| WEB-37-14 | F103 · Formula from cell above | Ctrl+' | REVIEW |
| WEB-37-15 | F101 · Value from cell above | Ctrl+Shift+' | REVIEW |
| WEB-37-16 | F247 · Insert argument names | Ctrl+Shift+A | REVIEW |

### WEB · Formatting

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WEB-38-01 | F008 · Bold | Ctrl+B | REVIEW |
| WEB-38-02 | F023 · Italic | Ctrl+I | REVIEW |
| WEB-38-03 | F022 · Underline | Ctrl+U | REVIEW |
| WEB-38-04 | F227 · Paste Special: formats | Shift+Ctrl+V | REVIEW |
| WEB-38-05 | F122 · Outline border | Ctrl+Shift+Ampersand (&) | REVIEW |
| WEB-38-06 | F121 · Number format | Ctrl+Shift+1 | REVIEW |
| WEB-38-07 | F120 · Time format | Ctrl+Shift+2 | REVIEW |
| WEB-38-08 | F119 · Date format | Ctrl+Shift+3 | REVIEW |
| WEB-38-09 | F116 · Currency format | Ctrl+Shift+4 | REVIEW |
| WEB-38-10 | F117 · Percentage format | Ctrl+Shift+5 | REVIEW |
| WEB-38-11 | F118 · Scientific format | Ctrl+Shift+6 | REVIEW |
| WEB-38-12 | F122 · Outline border | Ctrl+Shift+7 | REVIEW |
| WEB-38-13 | F019 · Format Cells | Ctrl+1 | REVIEW |

### WEB · Grid navigation

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WEB-39-01 | F209 · Cell up | Up or Shift+Enter | REVIEW |
| WEB-39-02 | F210 · Cell down | Down or Enter | REVIEW |
| WEB-39-03 | F212 · Cell right | Right or Tab | REVIEW |
| WEB-39-04 | F054 · Beginning of row | Home | REVIEW |
| WEB-39-05 | F055 · Beginning of sheet | Ctrl+Home | REVIEW |
| WEB-39-06 | F056 · Last used cell | Ctrl+End | REVIEW |
| WEB-39-07 | F057 · Page down | PageDown | REVIEW |
| WEB-39-08 | F058 · Page up | PageUp | REVIEW |
| WEB-39-09 | F053 · Data region boundary | Ctrl+Right or Ctrl+Left | REVIEW |
| WEB-39-10 | F180 · Cycle region focus | Ctrl+F6 | REVIEW |
| WEB-39-11 | F291 · Change ribbon tab | Tab; to open tab Enter | REVIEW |
| WEB-39-12 | F038 · Insert worksheet | Shift+F11 | REVIEW |
| WEB-39-13 | F061 · Next worksheet | Alt+Ctrl+PageDown | REVIEW |
| WEB-39-14 | F061 · Next worksheet | Ctrl+PageDown | REVIEW |
| WEB-39-15 | F062 · Previous worksheet | Alt+Ctrl+PageUp | REVIEW |
| WEB-39-16 | F062 · Previous worksheet | Ctrl+PageUp | REVIEW |

### WEB · Objects

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WEB-40-01 | F313 · Open object menu | Alt+Down | REVIEW |
| WEB-40-02 | F314 · Close object menu | Alt+Up | REVIEW |
| WEB-40-03 | F315 · Follow hyperlink | Ctrl+Enter | REVIEW |
| WEB-40-04 | F082 · Edit note | Shift+F2 | REVIEW |
| WEB-40-05 | F084 · Reply to comment | Ctrl+Shift+F2 | REVIEW |
| WEB-40-06 | F316 · Rotate object left | Alt+Left | REVIEW |
| WEB-40-07 | F317 · Rotate object right | Alt+Right | REVIEW |

### WEB · Cell and axis selection

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WEB-41-01 | F134 · Extend selection by one cell | Shift+Arrows | REVIEW |
| WEB-41-02 | F139 · Select column | Ctrl+Space | REVIEW |
| WEB-41-03 | F140 · Select row | Shift+Space | REVIEW |
| WEB-41-04 | F135 · Extend selection to data boundary | Ctrl+Shift+Right or Ctrl+Shift+Left | REVIEW |
| WEB-41-05 | F147 · Add nonadjacent selection | Shift+F8 | REVIEW |
| WEB-41-06 | F013 · Insert cells | Ctrl+Plus sign (+) | REVIEW |
| WEB-41-07 | F014 · Delete cells | Ctrl+Minus sign (-) | REVIEW |
| WEB-41-08 | F131 · Hide row | Ctrl+9 | REVIEW |
| WEB-41-09 | F132 · Unhide row | Ctrl+Shift+9 | REVIEW |
| WEB-41-10 | F129 · Hide column | Ctrl+0 | REVIEW |
| WEB-41-11 | F130 · Unhide column | Ctrl+Shift+0 | REVIEW |

### WEB · Navigation within selection

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WEB-42-01 | F155 · Move down within selection | Enter | REVIEW |
| WEB-42-02 | F156 · Move up within selection | Shift+Enter | REVIEW |
| WEB-42-03 | F157 · Move right within selection | Tab | REVIEW |
| WEB-42-04 | F158 · Move left within selection | Shift+Tab | REVIEW |
| WEB-42-05 | F142 · Reduce selection to active cell | Shift+Backspace | REVIEW |
| WEB-42-06 | F063 · Reveal active cell | Ctrl+Backspace | REVIEW |
| WEB-42-07 | F159 · Cycle selection corner | Ctrl+. | REVIEW |
| WEB-42-08 | F318 · Next nonadjacent range | Ctrl+Alt+Right | REVIEW |
| WEB-42-09 | F319 · Previous nonadjacent range | Ctrl+Alt+Left | REVIEW |
| WEB-42-10 | F138 · Extend selection to last used cell | Ctrl+Shift+End | REVIEW |
| WEB-42-11 | F137 · Extend selection to sheet start | Ctrl+Shift+Home | REVIEW |

### WEB · Calculation

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WEB-43-01 | F015 · Calculate all workbooks | F9 | REVIEW |
| WEB-43-02 | F107 · Rebuild dependencies and calculate | Ctrl+Shift+Alt+F9 | REVIEW |
| WEB-43-03 | F253 · Refresh active sheet data | Alt+F5 | REVIEW |
| WEB-43-04 | F254 · Refresh all data | Ctrl+Alt+F5 | REVIEW |
| WEB-43-05 | F098 · AutoSum | Alt+= | REVIEW |
| WEB-43-06 | F248 · Flash Fill | Ctrl+E | REVIEW |

### WEB · Copilot

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WEB-44-01 | F167 · AI: send request | Enter or Return | REVIEW |
| WEB-44-02 | F168 · AI: cancel | Esc | REVIEW |
| WEB-44-03 | F169 · AI: accept changes | Ctrl+Enter or Cmd+Return | REVIEW |
| WEB-44-04 | F170 · AI: switch mode | Ctrl+Shift+M or Cmd+Shift+M | REVIEW |
| WEB-44-05 | F171 · AI: navigate responses | Alt+C or Option+C | REVIEW |
| WEB-44-06 | F172 · AI: confirm files | Ctrl+J or Cmd+J | REVIEW |
| WEB-44-07 | F173 · AI: dictation | Alt+Shift+H or Option+Shift+H | REVIEW |
| WEB-44-08 | F174 · AI: open pane | Alt+C or Cmd+Ctrl+I | REVIEW |
| WEB-44-09 | F175 · AI: grid/pane focus | F6 or Shift+F6 | REVIEW |

### WEB · Accessibility

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| WEB-45-01 | F180 · Cycle region focus | Ctrl+F6 or Ctrl+Shift+F6 | REVIEW |
| WEB-45-02 | F320 · Region control focus | Tab or Shift+Tab | REVIEW |
| WEB-45-03 | F195 · Command search | Alt+Q | REVIEW |
| WEB-45-04 | F198 · Start keytips | Alt+Win | REVIEW |
| WEB-45-05 | F068 · Edit cell | F2 | REVIEW |
| WEB-45-06 | F018 · Go To | Ctrl+G | REVIEW |
| WEB-45-07 | F321 · Switch worksheet | Ctrl+Alt+PageUp or Ctrl+Alt+PageDown | REVIEW |
| WEB-45-08 | F095 · Context menu | Shift+F10 or Menu | REVIEW |
| WEB-45-09 | F322 · Read row header | Ctrl+Alt+Shift+T | REVIEW |
| WEB-45-10 | F323 · Read row before active cell | Ctrl+Alt+Shift+Home | REVIEW |
| WEB-45-11 | F324 · Read row after active cell | Ctrl+Alt+Shift+End | REVIEW |
| WEB-45-12 | F325 · Read column header | Ctrl+Alt+Shift+H | REVIEW |
| WEB-45-13 | F326 · Read column before active cell | Ctrl+Alt+Shift+PageUp | REVIEW |
| WEB-45-14 | F327 · Read column after active cell | Ctrl+Alt+Shift+PageDown | REVIEW |
| WEB-45-15 | F328 · Dialog navigation options | Ctrl+Alt+Space | REVIEW |

### IOS · Navigation

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| IOS-27-01 | F212 · Cell right | Tab | REVIEW |
| IOS-27-02 | F052 · Directional cell navigation | Arrows | REVIEW |
| IOS-27-03 | F061 · Next worksheet | Option+Right | REVIEW |
| IOS-27-04 | F062 · Previous worksheet | Option+Left | REVIEW |

### IOS · Formatting

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| IOS-28-01 | F122 · Outline border | Cmd +Option+0 | REVIEW |
| IOS-28-02 | F127 · Remove border | Cmd +Option+- | REVIEW |
| IOS-28-03 | F129 · Hide column | Cmd +0 | REVIEW |
| IOS-28-04 | F131 · Hide row | Ctrl+9 | REVIEW |
| IOS-28-05 | F130 · Unhide column | Shift+ Cmd +0 or Shift+Ctrl+) | REVIEW |
| IOS-28-06 | F132 · Unhide row | Shift+Ctrl+9 or Shift+Ctrl+( | REVIEW |

### IOS · Cells and text

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| IOS-29-01 | F212 · Cell right | Tab | REVIEW |
| IOS-29-02 | F079 · Directional text navigation | Arrows | REVIEW |
| IOS-29-03 | F002 · Copy | Cmd +C | REVIEW |
| IOS-29-04 | F001 · Paste | Cmd +V | REVIEW |
| IOS-29-05 | F007 · Cut | Cmd +X | REVIEW |
| IOS-29-06 | F005 · Undo | Cmd +Z | REVIEW |
| IOS-29-07 | F006 · Redo | Cmd +Y or Cmd +Shift+Z | REVIEW |
| IOS-29-08 | F008 · Bold | Cmd +B | REVIEW |
| IOS-29-09 | F023 · Italic | Cmd +I | REVIEW |
| IOS-29-10 | F022 · Underline | Cmd +U | REVIEW |
| IOS-29-11 | F028 · Select All | Cmd +A | REVIEW |
| IOS-29-12 | F134 · Extend selection by one cell | Shift+Left or Right | REVIEW |
| IOS-29-13 | F070 · Line break in cell | Cmd +Option+Return or Ctrl+Option+Return | REVIEW |
| IOS-29-14 | F080 · Beginning of text line | Cmd +Left | REVIEW |
| IOS-29-15 | F309 · End of text line | Cmd +Right | REVIEW |
| IOS-29-16 | F329 · Beginning of text | Cmd +Up | REVIEW |
| IOS-29-17 | F244 · End of text | Cmd +Down | REVIEW |
| IOS-29-18 | F330 · Previous paragraph | Option+Up | REVIEW |
| IOS-29-19 | F331 · Next paragraph | Option+Down | REVIEW |
| IOS-29-20 | F332 · Next word | Option+Right | REVIEW |
| IOS-29-21 | F333 · Previous word | Option+Left | REVIEW |
| IOS-29-22 | F098 · AutoSum | Shift+ Cmd +T | REVIEW |

### AND · Navigation

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| AND-30-01 | F212 · Cell right | Tab | REVIEW |
| AND-30-02 | F052 · Directional cell navigation | Up, Down, Left, or Right | REVIEW |

### AND · Cell operations

| Shortcut ID | Feature | Excel combination | Duxcel binding / acceptance |
| --- | --- | --- | --- |
| AND-31-01 | F004 · Save | Ctrl+S | REVIEW |
| AND-31-02 | F002 · Copy | Ctrl+C | REVIEW |
| AND-31-03 | F001 · Paste | Ctrl+V | REVIEW |
| AND-31-04 | F007 · Cut | Ctrl+X | REVIEW |
| AND-31-05 | F005 · Undo | Ctrl+Z | REVIEW |
| AND-31-06 | F006 · Redo | Ctrl+Y | REVIEW |
| AND-31-07 | F008 · Bold | Ctrl+B | REVIEW |
| AND-31-08 | F023 · Italic | Ctrl+I | REVIEW |
| AND-31-09 | F022 · Underline | Ctrl+U | REVIEW |
| AND-31-10 | F028 · Select All | Ctrl+A | REVIEW |
| AND-31-11 | F064 · Find | Ctrl+F | REVIEW |
| AND-31-12 | F070 · Line break in cell | Alt+Enter | REVIEW |

## Detailed behavior and design cards

### C01 — Data boundary and extended selection

- **Features / shortcuts:** Data region boundary and Extend selection to data boundary; `MAC-15-02`, `MAC-19-02` and Windows/web counterparts.
- **Excel ribbon location:** Not applicable to direct grid navigation. Verify selection-tool menu locations separately.
- **Duxcel placement:** Grid input → shared navigation/occupancy resolver. No toolbar icon is required; provide Help/shortcut text and a visible active selection.
- **Appearance:** Keep active cell, fixed anchor and moving extent distinct. Scroll reveals the destination. Selection outlines remain visible in read-only and ordinary contexts. Colors and measurements follow DESIGN.
- **Scope:** Empty sheet, contiguous manual cells, gaps, distant islands, source/Table/Pivot, filtered/sorted data and uncached rows. Measure Excel Mac occupancy rules for NULL, empty VARCHAR, formatting-only cells and future formulas returning empty text. A viewport Page boundary is not a data boundary.
- **Data/undo/persistence:** Navigation does not dirty data/formats or enter undo history. Resolve targets against the data/model revision. Any necessary bounded engine query retains cancellation/stale safety; do not assume each key requires a query.
- **Acceptance:** Four directions and Shift variants from each starting position; repeated presses, reverse selection, object edges, sources exceeding 256 rows and no grid input leakage from menus/editors. Record pure C target/anchor checks, hidden integrated native events and actual Excel Mac/physical QA separately.
- **Evidence:** No new behavior has been implemented or verified. Sources: [grid navigation](../src/grid/grid.c), `set_grid_navigation` and `select_region` in [app.c](../src/app/app.c), and [existing grid tests](../tests/test_grid.c).

### C02 — Find / Go To / filter / Focus

- **Excel ribbon path:** Detailed Find/Go To/Filter group/button paths have not been verified against a separate official UI reference.
- **Candidate Duxcel hierarchy:** Home > Editing > Find / Replace / Go To; Data > Filter; View > Focus. Use distinct command identities; menus, ribbon and shortcuts invoke the same handler.
- **Icon/appearance:** Magnifying glass for Find, funnel for filter and a distinct workspace symbol for Focus are candidates. No asset is selected. Tooltips include name, assigned shortcut and disabled reason. Make search scope visible for selection and cell-editor contexts.
- **Pending decision:** Adopting Excel Cmd+F and Cmd+Shift+F changes current filter/Focus assignments. F5 Go To conflicts with refresh; F12 Save As with diagnostics (C05). This document changes no bindings.
- **Data contract:** Find changes selection/view. A source filter changes the analytical definition and invalidates dependent results. Replace needs separate write scopes for manual cells, source overlays and formulas.
- **Acceptance:** Resolve the same key with grid, cell editor, filter popup, source SQL editor and modal focus. Escape closes the innermost draft. Test no match, next/previous match, offscreen results, undo and durable Replace separately.
- **Evidence:** Current bindings are in [USER_GUIDE](USER_GUIDE.md#shortcuts-and-interaction), `shortcuts`/`app_command` in [app.c](../src/app/app.c), and the [macOS adapter](../src/platform/macos.m). New UI or bindings have not been verified.

### C03 — Editing and selection lifecycle

- **UI:** Active grid cell → edit; formula bar/context menu express the same intent. F2 may rename an object title or edit manual text; focus must be explicit.
- **Icon/appearance:** Distinguish edit caret, text selection, grid anchor and pending/error state. Reserve tooltip/accessibility names; no icon is selected.
- **Data:** Commit is one worksheet operation. Escape discards the draft. Editor Undo and worksheet Undo have separate scopes. Explain read-only imported cells rather than silently doing nothing. Introduce typed/manual/formula/source-overlay support in bounded slices.
- **Acceptance:** Enter/Shift+Enter/Tab/Shift+Tab within selections and single cells; click-away commit, multiline input, Turkish UTF-8/IME, empty-input semantics, dirty state, Undo/Redo and Save/Open for actual data changes. Compare Excel's default movement-direction setting separately.

### C04 — Clipboard and structural edits

- **UI:** Home > Clipboard; Home > Cells / header context. Clear contents, Delete cells, Delete rows and Delete columns are distinct commands.
- **Icon/appearance:** Clipboard group, Paste split-menu and disabled reasons; icons remain unselected. Explain occupied-destination and source-interior rejection.
- **Data:** Paste/Fill/Move are atomic. Preserve NULL/empty/literal NULL, quoting and exact integer/DECIMAL values. Formula Fill needs reference movement. Source editing requires overlay/identity support. Format-only paste does not change values.
- **Acceptance:** Clipboard permission denial, offscreen selection, mixed source/manual/pivot ranges, partial failure, undo and persistence. Successful TSV paste does not establish Paste Special.

### C05 — Document lifecycle

- **UI:** File > Open / Save / Save As / Close; macOS Duxcel > Quit. Separate native and browser host commands.
- **Icon/appearance:** Document name and dirty/recoverable/saved state; Save icon/tooltip; Save / Don't Save / Cancel dialog. Icon choice is pending.
- **Data:** Distinguish saved revision from latest edited revision. Preserve the model on IO failure/cancellation. A close guard is not crash recovery. Recovery and autosave remain roadmap work even without shortcuts here.
- **Acceptance:** Untitled/named/clean/dirty/IO error, close versus quit, newer edits during Save, source access, native reopen, unsupported browser paths and the F12 conflict decision. Crash recovery has separate acceptance.

### New feature card template

Copy this block and link `F…`, shortcut IDs and the roadmap batch. Short notes can remain in the table; a long card is not mandatory for every simple navigation action.

```text
Card / feature ID:
Purpose and expected user behavior:
Excel platform/version + official reference:
Shortcut ID / combination / active focus / precondition:
Excel ribbon path: unverified / reference + full path / not applicable
Duxcel current path / proposed path:
Icon: existing asset or semantic candidate; license/provenance:
Label / tooltip / accessibility name:
Hierarchy: tab > group > command > submenu; overflow/context equivalent:
Appearance: normal / hover / focused / selected / disabled / pending / error:
Data command and target identity:
Model/engine effects and invalidation/revision:
Undo/Redo; Save/Open; recovery effect or reason not applicable:
Native / browser / OS conflicts and alternative access:
Starting state → actual input → expected result → side effects:
Implementation / test files; fixture and limits:
Feedback report IDs / batch / fix reference:
Evidence: date, source/binary, platform, real or synthetic input, result:
Human acceptance / open issue:
```

## Maintenance and completion

1. Link feedback to the relevant feature/shortcut ID; add a feature if needed. Visual evidence alone does not establish data correctness.
2. Group reports with the same root cause into a roadmap batch. Link shortcut repetitions to the same fix instead of implementing each row separately.
3. Define the command, visible UI placement and data effects in its acceptance card. Keep out-of-scope Excel areas at `SCOPE DECISION`; do not promise them automatically.
4. Implement, run focused checks in affected layers and update the binding record with evidence. Finding a handler or passing one unit test does not establish `VERIFIED`.
5. Close the ROADMAP batch. Update STATUS if general capability changed and USER_GUIDE if a user control changed. Do not copy the detailed acceptance report into three documents. Temporary screenshots/logs/hashes stay under ignored `build/tests/results/`.
