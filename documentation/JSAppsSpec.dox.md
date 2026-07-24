# BusyBar JS Application Framework — Specification

## Overview

User-developed JS apps (JerryScript) deployed as folders under `/ext/user_apps/`
(planned rename from `user_assets`). Discovered at boot, launched from APPS menu
or web interface. Apps use the `@busy-app/busy-lib` API contract — the on-device
JerryScript runtime provides a port of this library.

**busy-lib**: npm package `@busy-app/busy-lib` (repo: `busy-app/busylib-ts`).
Provides `BusyBar` (HTTP client), types matching the firmware OpenAPI schemas,
`StateStream` (WebSocket), and `ScreenRenderer` (WebGL2). On-device, only the
`BusyBar` API surface is ported — `StateStream` and `ScreenRenderer` are
web-only.

**Archive handling**: Uses `microtar` (already in the project as a submodule) for
`.tgz` unpacking.

---

## Distribution & Installation

Apps are distributed as `.tgz` archives. The firmware manages unpacking,
validation, and version-aware installation.

**Upload**: `POST /api/apps/install` with the `.tgz` as binary body. The
archive filename is irrelevant — the firmware derives identity from the manifest
inside.

**Installation flow**:
1. Receive `.tgz`, unpack to a **temporary location** (`/tmp/app_install/`).
2. Validate: `appmeta/manifest.json` exists and is valid; no path traversal in
   archive entries (`../` rejected); `scripts/main.js` exists (hardcoded entry point).
3. If validation fails → delete temp, return error.
4. If `/ext/user_apps/{id}/` already exists → compare `version` in the new manifest
   against the existing one. Overwrite only if **new version > existing version**
   (semver comparison). If not newer → return 409.
5. Atomically move temp → `/ext/user_apps/{id}/` (delete old if overwriting).
6. If move fails (disk full, etc.) → delete temp, return 503.

**Atomicity**: Since unpacking happens in a temp location, a failed install
never leaves a partial app folder. The old app (if any) remains intact until the
final move.

**Removal**: `DELETE /api/apps/remove?application_name=...` deletes the app
folder and its config file at `/ext/apps_data/jsrunner/{id}.json`.

**Discovery**: After install/remove, the App Service re-scans `/ext/user_apps/`
and updates its index. The APPS menu and `/api/apps/list` reflect the change
immediately.

**Stock apps** (Clock, Weather) ship unpacked in a read-only system partition
and are discovered alongside user-installed apps. They cannot be removed via the
API.

---

## Package Format

**Storage root**: `/ext/user_apps/` (planned rename from `user_assets`).

```
/ext/user_apps/
└── com.example.weather/
    ├── appmeta/
    │   ├── manifest.json         # required
    │   └── settings.json         # optional — settings descriptor
    ├── scripts/
    │   └── main.js               # hardcoded entry point
    ├── images/
    ├── animations/
    └── sounds/
```

Apps reference resources with paths relative to the app folder root
(e.g., `"images/icon.png"`). The runtime resolves them against
`{storage_root}/{application_name}/`.

---

## Manifest (`appmeta/manifest.json`)

```jsonc
{
    "format_version": 1,                     // JSON structure version (firmware bumps on breaking changes)
    "id": "com.example.weather",             // unique ID, ^[a-zA-Z0-9._-]+$
    "name": "Weather",                        // display name
    "version": "1.2.0",                      // semver
    "description": "Weather forecast",       // optional
    "author": "BusyBar Team",                // optional
    "icon": "images/icon.png",               // optional — stock "?" if absent
    "preview": "images/bg.png",              // optional — stock image if absent
    "settings": "appmeta/settings.json",      // optional — no settings submenu if absent
    "debug": false                            // optional — defaults to false; if true, only visible in debug mode
}
```

---

## Settings Descriptor (`appmeta/settings.json`)

Declares configurable parameters: types, constraints, defaults.
Consumed by device UI, web interface, JS runtime, and companion apps.

Sections are UI-only grouping; storage keys are flat (`{app_id}.{field_id}`).

### Structure

```jsonc
{
    "format_version": 1,    // JSON structure format version (firmware bumps on breaking format changes)
    "version": 1,           // schema version (developer bumps to discard stored values)
    "sections": [{
        "id": "display",
        "label": "Display",
        "description": "Appearance settings.",  // optional
        "icon": "palette",                      // optional
        "fields": [
            { "id": "show_date", "type": "boolean", "label": "Show Date", "default": true },
            { "id": "theme", "type": "enum", "label": "Theme", "default": "auto",
              "options": [{"value":"light","label":"Light"},{"value":"dark","label":"Dark"}] },
            { "id": "refresh", "type": "number", "label": "Refresh (min)", "default": 30,
              "min": 5, "max": 120, "step": 5 },
            { "id": "city", "type": "string", "label": "City", "default": "",
              "min_length": 1, "max_length": 64 },
            { "id": "accent", "type": "color", "label": "Accent", "default": "#0099FF" },
            { "id": "alarm", "type": "time", "label": "Alarm", "default": "08:00" }
        ]
    }]
}
```

### Field Types

All editable types use the `VarItemList` widget — the standard on-device input
component. Editing flow: OK enters edit mode on a field, encoder Up/Down changes
the value, OK or Back exits (commits).

| `type` | TS type | VarItemList widget | Constraints | Storage format |
|--------|---------|-------------------|-------------|----------------|
| `boolean` | `boolean` | `switch` — OK toggles On/Off | — | `true` / `false` |
| `number` | `number` | `spinbox` — encoder adjusts value | `min`, `max` (inclusive); `step` | JSON number |
| `string` | `string` | **Display-only** | `min_length`, `max_length` (bytes); `sensitive` | JSON string |
| `enum` | `"a" \| "b"` | `selector` — encoder cycles choices | `options: [{value, label}]` | option `value` string |
| `color` | `string` | **Display-only** | `#RRGGBB[AA]` | hex string |
| `time` | `string` | `timebox` — encoder sets hh:mm | `HH:MM[:SS]` | `"HH:MM"` string |

**Not editable on-device**: `string` and `color` — no text input widget exists
(`TextInput`/`ByteInput` not implemented; LVGL keyboard disabled). These are
editable only via the web interface.

**Validation (device)**: no regex — only length, range, enum membership, and
format-specific checks. Save is all-or-nothing: any field fails → entire batch
rejected with per-field errors. Load with invalid stored values → drop field,
use default, log warning.

---

## Runtime Config Values (`/ext/apps_data/jsrunner/{app_id}.json`)

Firmware-managed. One file per app, stored outside the app folder.

```jsonc
{ "format_version": 1, "version": 1, "values": { "show_date": true, "theme": "dark" } }
```

| Field | Type | Description |
|-------|------|-------------|
| `format_version` | `integer` | JSON structure format version (firmware bumps on breaking changes). |
| `version` | `integer` | The `settings.json` schema version this data was saved under. |
| `values` | `object` | Flat key-value map of field IDs to current values. |

**Lifecycle**:
1. First launch → auto-created from defaults.
2. Load → merge stored + defaults, validate; drop invalid fields, warn.
3. Save → validate all; atomic overwrite on success; reject batch on failure.
4. Version mismatch → discard stored, recreate from defaults.

**On app update**: When a newer app version is installed, the existing config
file is **preserved** if `settings.json` has the same schema `version`. If the
schema `version` changed, the old config is discarded per rule 4 above. Defaults
from the new `settings.json` are only used for fields not present in the
preserved config.

---

## App Lifecycle

- **Discovery**: App Service scans `/ext/user_apps/` on boot + fs changes. Indexes valid manifests.
- **No settings**: select → launch immediately. Back → APPS list.
- **Has settings**: select → submenu (Start / Setup). Back → submenu.
- **Other exit** (gallett switch, BUSY timer, calendar): app backgrounds; resumes on next APPS visit.
- **On-device settings editor**: uses `VarItemList` widget. Fields rendered as:
  `boolean` → switch, `number` → spinbox, `enum` → selector, `time` → timebox.
  OK enters edit mode on a field; encoder adjusts the value; OK/Back exits.
  `string` and `color` are display-only (`***` if sensitive). Full editing in web UI.
- **Web interface** (`http://{device_ip}`): APPS section lists apps with Start/Settings. Full form editing.

---

## On-Device busy-lib Port

The on-device JerryScript runtime must expose the `BusyBar` client API surface
from `@busy-app/busy-lib`. Unlike the npm package (which makes HTTP calls), the
on-device port calls firmware services directly — no HTTP loopback.

Required API surface (subset of `@busy-app/busy-lib`):

| Module | Key types/functions |
|--------|---------------------|
| **Settings** | `load()`, `save(values)` — backed by settings validator + `/ext/apps_data/jsrunner/{app_id}.json` |
| **Display** | `DisplayDrawParams`, `draw(params)`, `clear()` — maps to canvas service |
| **Audio** | `AudioVolumeInfo`, `play(file)`, `stop()`, `volume()` — maps to audio service |
| **Network** | `WifiNetwork`, `WifiConnectParams`, `NetworkInterfaceInfo` — maps to Wi-Fi service |
| **Device** | `VersionInfo`, `Status` (power, system, firmware), `HttpAccessInfo`, `AccountInfo`, `TimezoneItem` |
| **Storage** | `StorageListElement`, `StorageFileElement` — maps to storage service |
| **Input** | `on(event, callback)` — Start, Back, encoder events (via pubsub, not HTTP) |
| **HTTP** | `fetch(url, options)` — HTTPS via mbedtls (C bridge, not HTTP loopback) |

**Not ported on-device** (web-only): `StateStream`, `ScreenRenderer`, `BSB_Frame`,
`LEDRenderer`, `LocalStateStream`, `StreamLifecycle`, `DataStatus`,
`ConnectionStatus`, `ProcessedState`, `ProcessedFrame`, `SmartHomePairingInfo`,
`UpdateStatus`, `BSB_Update`.

The on-device port uses the same TypeScript types as `@busy-app/busy-lib`.
A full list of required types is in the [busy-lib on-device port spec] (TBD:
`documentation/busylib-ondevice.md`).

---

## What Needs To Be Built

| # | Component | Location |
|---|-----------|----------|
| 1 | **JerryScript runtime** — standalone engine (FreeRTOS task), C-to-JS bridge, busy-lib port, HTTPS (mbedtls), input (pubsub) | `applications/services/js_srv/` |
| 2 | **App Service** — scans `/ext/user_apps/`, validates manifests, in-memory index | `applications/services/app_srv/` |
| 3 | **Settings validator** — loads `settings.json`, validates values, reads/writes `/ext/apps_data/jsrunner/{app_id}.json` | `applications/services/app_srv/` |
| 4 | **`api_apps.c`** — HTTP endpoints: install (POST .tgz), list (GET), settings (GET/PUT), launch (POST), remove (DELETE). Follows existing handler pattern. Uses `microtar` for archive unpacking. | `applications/services/web_server/http_api/` |
| 5 | **`apps.yaml`** — OpenAPI spec for new endpoints | `applications/services/web_server/openapi/` |
| 6 | **APPS menu GUI** — gallett integration, submenu, on-device settings editor | GUI module |
| 7 | **Storage migration** — rename `user_assets` → `user_apps`, one-time boot migration | Existing module updates |
| 8 | **Stock apps** — Clock, Weather | `assets/apps/` |
| 9 | **Web UI: APPS section** — calls `/api/apps/*` | `assets/www/` |
| 10 | **busy-lib on-device port spec** — full type list, API contract for the JerryScript port | `documentation/busylib-ondevice.md` |

### Suggested Order

```
Phase 1: js_srv → app_srv → storage migration → stock apps
Phase 2: settings validator → api_apps.c → apps.yaml
Phase 3: APPS menu GUI → web UI → busy-lib on-device spec
```

---

## Design Decisions

| Decision | Rationale |
|----------|-----------|
| Folder deployment | No extraction step on-device. |
| Separate schema & values | Schema is dev-authored (read-only); values are runtime state. |
| Flat field IDs | Sections can reorganize without data migration. |
| Nuke-and-rebuild versioning | No field-level migration on constrained device. |
| VarItemList for on-device editing | `boolean` (switch), `number` (spinbox), `enum` (selector), `time` (timebox) editable. `string`/`color` require text input — web-only. |
| No regex validation | Expensive on embedded. Length/range/format checks cover common cases. |
| `sensitive` = UI hint | No encryption at this layer. |
| All-or-nothing save | Prevents inconsistent state. |
| busy-lib port, not HTTP loopback | On-device calls go direct to firmware services. Same types as `@busy-app/busy-lib`. |
