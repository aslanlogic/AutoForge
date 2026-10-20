# AutoForge

Tools for message databases on CAN, CAN FD, UART and UDP networks
(a network runs **one** of these protocols). Documents are `.ffxml` (XML).

| Folder | What it is |
|---|---|
| `core/` | Static library, **QtCore only**: the data structures, `Database`, `.ffxml` reading/writing, `ConsistencyChecker`. Shared by all programs, so they always agree on the format. |
| `FrameForge/` | The database **editor** (GUI), in the spirit of Vector CANdb++. |
| `DiagProbe/` | *Planned, not created yet:* the **traffic analyser** — decodes real frames into messages and signal values using a `.ffxml`. This is the main goal of the project. |

Never add widget/GUI code to `core/` (its `.pro` has `QT = core`, so the build would fail anyway).

## Working with the user

- **Implement only when the user says so.** Discuss and propose first.
- **Small, reviewable steps.** The user reviews every change. Split big
  features into steps and stop after each one.
- **Ask before building when the data model is ambiguous.** (A design with all
  four protocols configured side by side was rejected and reverted: a network
  has exactly one active protocol.)
- Match the surrounding code: plain, explanatory comments; simple names.

## Build

Qt **5.15.3**, C++17, **qmake**. No other libraries. `AutoForge.pro` is a
`subdirs` project that builds `core` first, then the programs.

```bash
mkdir -p build && cd build && qmake ../AutoForge.pro && make -j8 && ./FrameForge/FrameForge
```

- New source files go into the `SOURCES`/`HEADERS` of their folder's `.pro`
  (kept in alphabetical order); new icons into `FrameForge/resources.qrc`.
- A program uses the library with `include(../core/core.pri)` (include path,
  linking, relink-on-change, Windows debug/release folders). A new program
  (DiagProbe) also needs an entry in `AutoForge.pro` with `<name>.depends = core`.

## Architecture

`core/` holds the data structs, `Database`, `xmlstorage` and `ConsistencyChecker`;
everything else below lives in `FrameForge/`.

```
Database  ->  item models  ->  views
(the data)    (adapters)       (widgets)
```

- **Data structs** (plain, no widgets): `network.h`, `node.h`, `message.h`,
  `signal.h`, `valuetable.h`. Rules and limits live next to them as inline
  functions (`maxFrameId`, `isValidLength`, `checkMessageAgainstNetwork`,
  `checkSignalLayout`, `signalBits`, ...). Dialogs, the file reader and the
  consistency checker all reuse these, so they always agree.
- **`Database`** (`database.h/.cpp`) owns everything. Every object has a unique
  `id` (one shared counter). For each kind there are `all…()`, `find…(id)`,
  `indexOf…(id)`, `add…()`, `update…()`, `remove…()` and Qt signals
  `…AboutToBeAdded(index)`, `…Added(id)`, `…Changed(id)`, `…AboutToBeRemoved(index)`,
  `…Removed(id)`, plus `aboutToBeReset()`/`wasReset()` for `replaceAll()` (file open).
  The "about to" signals exist so item models can call `beginInsertRows` etc.
  `find…()` returns a pointer that is only valid until the next change — copy it.
- **Item models** read from `Database`, hold no copies (except the node Tx/Rx
  caches in `NavigationModel`, see its comments):
  - `NavigationModel` — the left tree: headings Network, Network Nodes
    (each node has Tx/Rx Messages folders), Messages, Signals, Value Tables.
    Rows report `ItemKindRole` and `ObjectIdRole`; the main window decides
    everything from those. Uses a packed `internalId` scheme (see the top of
    `navigationmodel.cpp`).
  - `NavigationFilterModel` — search filter + A–Z sorting for the tree. Indexes
    from the tree must be `mapToSource()`d.
  - `SignalTableModel`, `MessageTableModel`, `NodeTableModel`,
    `ValueTableTableModel` — the "all items" tables in the right pane (sorted
    through a `QSortFilterProxyModel`, `SortRole` gives numbers as numbers).
- **`MainWindow`** — left pane (search + tree), right pane (`QStackedWidget`
  with one view per item kind), menus (File, Tools). After **any** database
  change it calls `refreshRightPane()`, which redraws the shown view or falls
  back to the empty page if the object was deleted.
- **Dialogs** (`…dialog.cpp`) edit a copy of one object and return it; the
  caller applies it to `Database`. They validate in `accept()` and keep the
  dialog open with a warning on bad input. Combo-box item order matches the
  enum order (`static_cast` between them).
- **Read-only views** (`…propertieswidget.cpp`) use helpers in `propertytable.h`.
- **`xmlstorage.cpp`** — reads/writes `.ffxml`. References between objects are
  saved **by name** (ids are not saved) and resolved after the whole file is
  read. Loading validates and refuses bad files with a clear message; the
  database is only replaced if the whole file was fine.
- **`ConsistencyChecker`** (Tools → Consistency Check, F7) — reads the
  database and returns errors/warnings; `ConsistencyDialog` shows them.

## Domain rules (decided with the user)

- A network has exactly **one protocol**. Messages have no protocol of their own;
  they follow the network. On CAN/CAN FD a message has an ID format
  (standard 11-bit / extended 29-bit). ID and length limits come from the network
  (UART/UDP: the largest ID follows the network's ID size).
- Changing the network is refused if existing messages would not fit.
- **UART framing:** `[ID] [DLC, 1 byte] [payload] [CRC]`, COBS-encoded, ended
  with `0x00`. The CRC covers ID+DLC+payload (before COBS). ID and CRC are
  little-endian. Fixed CRC variants: CRC-8/SMBUS, CRC-16/CCITT-FALSE,
  CRC-32/ISO-HDLC (details and check values in `network.h`). UART payload ≤ 255.
- **Start bits** follow the DBC convention (Intel = LSB, Motorola = MSB); see
  `signalBits()` in `signal.h`.
- **Deleting is refused while in use:** a signal placed in a message, a node
  that transmits/receives, a value table used by a signal. `Database` refuses
  too, not just the UI.
- A node cannot be both transmitter and receiver of the same message.
- Names are unique per kind; new items get `New_<Kind>_<n>` (first free number).
- Value tables are shared; a signal links to one (`valueTableId`). The signal
  dialog shows its values read-only.

## Testing

There is no test project in the repo. Changes have been verified with small
throw-away Qt programs (built outside the repo) that include the sources with
`#define private public`, run with `QT_QPA_PLATFORM=offscreen`, use
`QAbstractItemModelTester` on the models, drive dialogs via `QTimer`, and grab
screenshots with `QWidget::grab()`. Notes:
- Send `QMouseEvent`s directly for drags; `QTest::mouseMove` does nothing offscreen in Qt 5.
- For `QTableView`/`QTableWidget` sorting: set `setSortIndicator(-1, …)` *before*
  `setSortingEnabled(true)`, otherwise it sorts by column 0 immediately.

## Not done yet

- DiagProbe, the traffic analyser (decoding, live/recorded sources, trace view) — the main goal.
  Decoding logic that both programs could use belongs in `core/`.
- Undo, Delete key, per-signal receivers, signals under messages in the tree,
  jumping from a consistency-check row to the object, DBC import/export, code generation.
