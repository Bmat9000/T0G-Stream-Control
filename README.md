# T0G Stream Control

**T0G Stream Control** is a native Windows plugin for OBS Studio that brings TikTok LIVE and Twitch stream controls into OBS. The project is designed around a modular architecture so streaming, platform integrations, chat, vertical output, credentials, diagnostics, and future features can be developed independently without turning the plugin into one large component.

> **Project status:** The core T0G Stream Control OBS plugin features are working. **Chat Merger and chat/LIVE event features are coming soon and are actively being worked on right now.** Some integrations depend on account access and third-party platform behavior that can change over time.

## What T0G Stream Control Does

T0G Stream Control is intended to keep the tools needed for a multi-platform stream inside OBS instead of requiring several separate windows or applications.

Current project components include:

- Native OBS Studio dock/plugin.
- TikTok LIVE stream/session integration.
- Twitch integration.
- Shared stream title and game/category controls.
- TikTok RTMP/output handling.
- Manual RTMP fallback support.
- Aitum Vertical integration for vertical TikTok output.
- **Coming soon / in active development:** Combined Twitch + TikTok Chat Merger dock.
- **Coming soon / in active development:** Read-only TikTok LIVE chat/event ingestion.
- **Coming soon / in active development:** TikTok LIVE gifts and other LIVE activity/events.
- **Coming soon / in active development:** Twitch chat integration.
- Saved plugin settings and credential storage.
- Stream presets and session state.
- Preflight checks before starting outputs.
- Runtime/plugin diagnostics and loader checks.
- Windows installer and automated builds.

## Feature Status

### ✅ What It Has Now

The core **T0G Stream Control** streaming features are working, including the native OBS dock, TikTok LIVE/stream controls, Twitch integration, stream title and game/category controls, TikTok RTMP/output handling, manual RTMP fallback, Aitum Vertical integration, saved settings/credentials, presets, preflight checks, and diagnostics.

### 🚧 Coming Soon — Chat Read

**Chat Read is actively being worked on right now.** This will allow the Chat Merger to receive and display supported chat and LIVE activity inside OBS, including:

- Twitch chat messages.
- TikTok LIVE chat messages.
- TikTok LIVE gifts and supported LIVE events.
- A combined Twitch + TikTok feed in the Chat Merger.

The TikTok chat/event connection is being developed as **read-only first**.

### 🔜 Coming Later — Chat Write

**Chat Write is planned after Chat Read.** The goal is to add a message box to the Chat Merger so messages can be sent from one place.

The planned controls are:

- **Twitch** — send to Twitch chat.
- **TikTok** — send to TikTok chat when a reliable supported implementation is available.
- **Both** — send the same message to both platforms when both write connections are available.

TikTok Chat Write is **not currently implemented** and is not being advertised as working. It will only be added if the project has a reliable way to send TikTok LIVE chat messages.

## Chat Merger — Coming Soon

> **🚧 IN ACTIVE DEVELOPMENT:** Chat Merger, Twitch chat, TikTok LIVE chat, gifts, and related LIVE event features are currently being worked on and are **coming soon**. The rest of the core T0G Stream Control streaming functionality is working.

The **Chat Merger** is being built to provide one OBS dock for supported Twitch and TikTok LIVE activity.

TikTok is currently treated as **read-only** on the chat side. The plugin can receive supported TikTok LIVE messages/events, but T0G Stream Control does not currently send chat messages back to TikTok.

This separation is intentional. Read-only TikTok event handling can continue to be expanded without depending on unofficial message-sending behavior.

## TikTok LIVE

The TikTok side of the plugin is responsible for the TikTok-specific stream workflow and LIVE integration.

Depending on the account and available TikTok/Streamlabs access, the project can work with TikTok stream/session information, RTMP destinations, stream keys, LIVE metadata, chat, and LIVE events.

TikTok and Streamlabs can change private or undocumented behavior at any time. Features that rely on those systems may require updates after platform changes.

## Twitch

Twitch support is kept in its own service module. This allows Twitch authentication, metadata, chat, and related features to remain isolated from the TikTok implementation.

The goal is for the OBS dock to provide shared controls where possible while each platform keeps its own backend logic.

## Vertical Streaming / Aitum Vertical

T0G Stream Control includes integration code for **Aitum Vertical** so a vertical TikTok workflow can coexist with a normal landscape stream.

This makes it possible to build a setup where Twitch uses the normal landscape OBS output while TikTok uses a vertical scene/output managed through the vertical workflow.

Aitum Vertical is a separate third-party OBS plugin and is not included with T0G Stream Control.

## Modular Architecture

The OBS plugin is intentionally split into focused modules. Current source areas include:

- **T0G Stream Dock** — main OBS dock and user controls.
- **TikTok Service** — TikTok LIVE/session logic.
- **TikTok Output** — TikTok output handling.
- **Twitch Service** — Twitch-specific integration.
- **Chat Merger** — unified chat/event presentation.
- **Aitum Vertical** — vertical-output integration.
- **Manual RTMP Output** — fallback/manual output handling.
- **Credential Store** — saved credentials/settings support.
- **Stream Presets** — reusable stream configuration.
- **Session State** — runtime stream state.
- **Preflight Check** — validation before starting.
- **Loader Probe** — plugin/runtime diagnostics.
- **T0G Settings** — plugin configuration.

Keeping these pieces separate makes individual features easier to test, replace, disable, and update.

## Full Setup Guide

Follow these steps from top to bottom for a first-time setup.

### 1. Download T0G Stream Control

The Windows build produces:

`T0G-Stream-Control-Setup.exe`

[**Download the latest T0G Stream Control installer**](https://github.com/Bmat9000/T0G-TikTok-Live-Tools/releases/download/t0g-stream-control-latest/T0G-Stream-Control-Setup.exe)

Close OBS Studio before running the installer.

### 2. Windows May Warn That the Installer Is Unrecognized or Not Trusted

T0G Stream Control is currently distributed **without a paid code-signing certificate**.

Because the installer is not digitally signed yet, Windows/SmartScreen or your browser may show a warning such as **Unknown Publisher**, **Windows protected your PC**, **unrecognized app**, or another unsupported/not-trusted warning.

That warning does not automatically mean the installer contains malware. It means Windows cannot verify the publisher through a trusted code-signing certificate.

Code-signing certificates cost money and the project does not currently have one. Since the project source code is available in this repository for users to inspect, build, and review, signing has not been treated as a requirement yet. If enough users care about having a signed installer, code signing can be added later.

**Only download the installer from this repository's official release/download link.** Do not bypass security warnings for copies downloaded from random websites or reuploads.

If Windows SmartScreen displays **Windows protected your PC**, verify that you downloaded the installer from this repository before choosing **More info** and **Run anyway**.

### 3. Run the Installer

1. Run `T0G-Stream-Control-Setup.exe`.
2. Allow administrator permission if Windows asks for it.
3. Confirm/select your OBS Studio installation directory.
4. Complete the installation.
5. Start OBS Studio.
6. Open the **T0G Stream Control** dock from OBS if it is not already visible.

OBS is normally installed under `Program Files`, which is why administrator permission may be required.

If OBS reports **Plugins Not Loaded** or the T0G plugin does not appear, check the OBS log and make sure the plugin and its required runtime DLLs were installed into the correct OBS directories.

### 4. Set Up Streamlabs for TikTok

The TikTok workflow uses the Streamlabs/TikTok access that the original project was built around.

You need a TikTok account with the required LIVE/Streamlabs streaming access.

1. Install Streamlabs Desktop if you do not already have it.
2. Open Streamlabs.
3. Sign in/connect using the TikTok account you want to stream from.
4. Make sure that account has access to TikTok LIVE through Streamlabs.
5. Complete any TikTok or Streamlabs authorization that is requested.
6. Once the account is connected and working in Streamlabs, return to OBS and T0G Stream Control.

The original project included the ability to retrieve the Streamlabs/TikTok information needed to create the TikTok LIVE session and obtain the RTMP destination/stream key. T0G Stream Control builds on that foundation and moves the workflow into the OBS plugin.

If your TikTok account does not have the required LIVE/Streamlabs access, the plugin cannot create access that TikTok has not granted to the account.

### 5. Configure TikTok in T0G Stream Control

Open the T0G Stream Control settings/dock in OBS and configure the TikTok side of your stream.

The TikTok module handles the TikTok LIVE/session workflow and the RTMP information used by the TikTok output. Manual RTMP support is also available as a fallback when you already have a TikTok server URL and stream key.

Treat your TikTok stream key, Streamlabs token, cookies, and other authentication information as passwords. Never post them publicly.

### 6. Configure Twitch

Connect/configure the Twitch account you want to stream from in T0G Stream Control.

Twitch is handled by its own module so Twitch authentication, metadata, and streaming controls remain separate from the TikTok implementation.

Once configured, the shared controls can be used for supported stream information such as the stream title and game/category.

### 7. Set Up Aitum Vertical for TikTok

If you want TikTok to use a vertical layout while Twitch uses your normal landscape OBS stream, install and configure **Aitum Vertical**.

T0G Stream Control includes an Aitum integration module, but Aitum Vertical itself is a separate OBS plugin and is not bundled with this installer.

Set up your vertical scenes in Aitum/OBS before your first multi-platform stream.

A typical setup is:

- **Twitch:** normal landscape OBS output.
- **TikTok:** vertical Aitum output.
- **T0G Stream Control:** manages the platform-specific stream workflow and controls from OBS.

### 8. Check Your Stream Information

Before going live:

1. Confirm the correct Twitch and TikTok accounts are configured.
2. Enter/check your stream title.
3. Enter/check your game or category.
4. Confirm the TikTok RTMP/session information is available.
5. Confirm your Twitch output is configured.
6. Confirm your Aitum vertical scene/output if you are using vertical TikTok.
7. Run/check the available T0G preflight and status information.
8. Make sure OBS is not reporting plugin or output errors.

### 9. Go Live

Once everything passes your checks, start the outputs you want to use.

The intended setup allows the normal landscape stream and the TikTok vertical stream to be controlled from the same OBS-based workflow instead of constantly switching between separate applications.

### 10. Chat Features

The streaming/control side is working now. The **Chat Merger is the part currently under active development**.

**Chat Read — coming soon:** Twitch chat, TikTok LIVE chat, TikTok gifts/LIVE events, and the combined Twitch + TikTok feed.

**Chat Write — planned after Chat Read:** a message box with Twitch / TikTok / Both selections. TikTok writing will only be marked as supported after a reliable implementation exists.

## Requirements

- Windows.
- OBS Studio.
- Supported TikTok LIVE/streaming access for TikTok features.
- A Twitch account for Twitch features.
- Aitum Vertical when using the Aitum/vertical workflow.
- Internet access for platform integrations.

Individual platform features may require additional account authorization.

## Building From Source

The native plugin source is located in:

`obs-plugin/`

The plugin uses CMake and C++/Qt components compatible with OBS Studio plugin development. CI/build support lives alongside the plugin and under the repository's GitHub Actions configuration.

The repository also still contains files from the original Streamlabs TikTok stream-key project because T0G Stream Control began from that codebase and continues to preserve attribution and relevant legacy components.

## Repository Layout

```text
T0G-TikTok-Live-Tools/
├── .github/          GitHub Actions / automated builds
├── docs/             Project documentation
├── installer/        Windows installer files
├── obs-plugin/       Native T0G Stream Control OBS plugin
│   ├── ci/           Plugin CI/build support
│   └── src/          Modular plugin source
├── Stream.py         Original/legacy Streamlabs project component
├── StreamLabsTikTokStreamKeyGenerator.py
├── TokenRetriever.py
├── Updater.py
└── README.md
```

## Security

Treat stream keys, OAuth tokens, access tokens, cookies, and other account credentials as secrets.

Do not post them in issues, screenshots, logs, Discord messages, or commits. If a credential is accidentally exposed, revoke or rotate it through the relevant platform.

## Original Project & Credits

T0G Stream Control did **not** start from scratch.

This repository was originally based on **Loukious' StreamLabsTikTokStreamKeyGenerator**, which provided the original Streamlabs/TikTok stream-key generation work that this project was built from.

**Original project:** [Loukious/StreamLabsTikTokStreamKeyGenerator](https://github.com/Loukious/StreamLabsTikTokStreamKeyGenerator)

**Original author:** [Loukious](https://github.com/Loukious)

A huge thank you and full credit to **Loukious** for the original project and foundation. T0G Stream Control has since expanded the project toward a native, modular OBS Studio plugin with Twitch integration, TikTok LIVE tooling, chat/event handling, vertical-stream support, diagnostics, installation tooling, and additional stream controls.

The presence of new T0G features does not remove or replace the attribution owed to the original project.

## Third-Party Projects

T0G Stream Control integrates with or interacts with third-party software and services, including:

- OBS Studio
- TikTok / TikTok LIVE
- Twitch
- Streamlabs
- Aitum Vertical

Those projects and services are owned and maintained by their respective developers/companies. T0G Stream Control is not an official TikTok, Twitch, Streamlabs, Aitum, or OBS project.

## License

This repository is licensed under the **GNU General Public License v3.0 (GPL-3.0)**. See `LICENSE.txt` for the complete license text.

Any original upstream licensing and attribution requirements must continue to be respected.

## Development Notes

T0G Stream Control is under active development. Features may change as the plugin is tested against OBS updates and changes made by TikTok, Twitch, Streamlabs, or Aitum.

When reporting a problem, include the relevant OBS log/error information but remove stream keys, tokens, cookies, and other private credentials before sharing it.
