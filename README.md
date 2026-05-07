# Pack Track

A native shipment-tracking application for the Flipper Zero — a clean, glanceable interface for keeping tabs on packages from your wrist-pocket hacker's tool. Built in C against the Flipper firmware's native GUI, input, and synchronization primitives, Pack Track delivers a desktop-quality tracking experience on a 128×64 monochrome display.

> **Status:** UI reference implementation. The application ships with a curated set of demonstration shipments and is structured to make integration with a live carrier backend (HTTP companion app, sub-GHz relay, or BLE-tethered host) a drop-in extension.

---

## Highlights

- **Multi-carrier ready.** First-class display support for UPS, USPS, FedEx, and DHL out of the box, with a generic schema that accepts arbitrary carriers and tracking-number formats.
- **Five-state lifecycle.** Models the full shipment journey — *Pending*, *In Transit*, *Out for Delivery*, *Delivered*, and *Exception* — each with a distinctive iconographic glyph rendered directly to the canvas.
- **Two-pane navigation.** A scrollable summary list pairs with a dedicated detail screen exposing carrier, full tracking ID, last-known location, and timestamp of the most recent update.
- **Pixel-tuned UI.** Hand-laid out for the Flipper's 128×64 OLED: header rule, inverted-row selection, right-aligned status column, position counter, and a footer hint on the detail view.
- **Concurrency-safe state.** All view-state mutations run under a `FuriMutex`, so the input handler and the render callback never race on a partially-updated frame.
- **Lightweight footprint.** Runs in a 2 KB stack with no external dependencies beyond the standard Flipper SDK records (`gui`, `input`, C standard library).

---

## Installation

### Build with ufbt (recommended)

```bash
# From the project root
ufbt
ufbt launch    # build, upload, and start on the connected Flipper
```

Drop `package_tracker.fap` into `apps/Tools/` on your Flipper's microSD if you'd rather sideload manually.

### Build inside the firmware tree

Clone Pack Track into `applications_user/package_tracker/` of your firmware checkout (official, Momentum, Unleashed, or RogueMaster) and run the firmware build as usual. The application is registered as an external FAP and appears under **Apps → Tools → Pack Track**.

---

## Usage

| Screen | Input | Action |
|--------|-------|--------|
| List | ▲ / ▼ | Move selection (scrolls automatically beyond four visible rows) |
| List | OK | Open detail view for the highlighted shipment |
| List | BACK (short) | Exit the application |
| Detail | ◀ / ▶ | Page between shipments without returning to the list |
| Detail | BACK (short) | Return to the list |
| Anywhere | BACK (long) | Force-exit |

The list view shows a status glyph, a human-readable label, and a short status code per row, plus a `current/total` counter in the header. The detail view promotes the label to the primary font and lays out carrier, tracking number, last reported location, and timestamp on individual rows.

---

## Architecture

Pack Track is intentionally compact — a single translation unit, ~290 lines — organized around the standard Flipper event-loop pattern:

```
┌─────────────────┐   InputEvent   ┌────────────────────┐
│  ViewPort input │ ─────────────▶ │ FuriMessageQueue   │
└─────────────────┘                └─────────┬──────────┘
                                             │
                                             ▼
                                   ┌────────────────────┐
                                   │  Main event loop   │
                                   │  (mutates state)   │
                                   └─────────┬──────────┘
                                             │ view_port_update
                                             ▼
                                   ┌────────────────────┐
                                   │ render_callback    │
                                   │ → draw_list /      │
                                   │   draw_detail      │
                                   └────────────────────┘
```

Key types live at the top of `package_tracker.c`:

- `PackageStatus` — enum of the five lifecycle states.
- `Package` — immutable record of carrier, tracking ID, label, last-update timestamp, location, and status.
- `Screen` — discriminator for the active view (list vs. detail).
- `TrackerState` — runtime UI state: current screen, selected index, scroll offset, and the mutex guarding them.
- `TrackerEvent` — message posted from the input ISR-style callback into the main loop's queue.

Rendering is split into two pure functions, `draw_list` and `draw_detail`, both invoked from a single `render_callback` that acquires the state mutex before touching the canvas. Status glyphs are drawn procedurally with `canvas_draw_disc`, `canvas_draw_circle`, `canvas_draw_line`, and `canvas_draw_dot` — no bitmap assets required.

---

## Project layout

```
flipper-pack-track/
├── application.fam        # FAP manifest (app id, entry point, metadata)
├── package_tracker.c      # Application source (UI, state, event loop)
└── README.md
```

The manifest registers Pack Track as an external `Tools`-category FAP with entry point `package_tracker_app`, a 2 KB stack, and a dependency on the `gui` record.

---

## Extending Pack Track

The data model is deliberately decoupled from the rendering layer. To wire in real shipments, replace the static `packages[]` array with a runtime collection populated from your source of truth. Suggested integrations:

- **Companion-app polling.** A desktop or mobile app pushes updates over USB serial (`furi_hal_cdc`) or BLE; the FAP refreshes its in-memory list on each notification.
- **HTTP-bridged tracking.** Pair with an ESP32/ESP8266 dev board (Wi-Fi Devboard or any UART-attached MCU) that proxies carrier APIs and streams JSON updates to the Flipper.
- **Local persistence.** Persist the list to `/ext/apps_data/package_tracker/` via the `Storage` record so shipments survive reboots.
- **Editable entries.** Add an "Add tracking number" submenu using `DialogEx` and the on-screen keyboard from `gui/modules/text_input.h`.

The `Package` struct uses `const char*` fields to keep the demo allocation-free; a dynamic implementation should swap these for `FuriString` or owned `char*` buffers and free them on teardown.

---

## Compatibility

Targets the upstream Flipper Zero firmware API and is portable across the major community distributions (Official, Momentum, Unleashed, RogueMaster). No hardware peripherals beyond the GUI and input subsystems are touched.

---

## License

Released under the MIT License. See the project's commit history for authorship.
