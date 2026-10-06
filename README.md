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

## Download

### Windows Installer

The Windows build produces:

`T0G-Stream-Control-Setup.exe`

Download the current installer from the project's latest installer release:

[**Download T0G Stream Control for Windows**](https://github.com/Bmat9000/T0G-TikTok-Live-Tools/releases/download/t0g-stream-control-latest/T0G-Stream-Control-Setup.exe)

The installer is published by the OBS plugin build workflow so the link can continue pointing to the current installer.

## Installation

1. Close OBS Studio.
2. Download `T0G-Stream-Control-Setup.exe`.
3. Run the installer.
4. Allow administrator permission when requested. OBS is normally installed under `Program Files`, so installation may require elevation.
5. Select or confirm your OBS Studio installation.
6. Finish the installation.
7. Start OBS Studio.
8. Open the T0G Stream Control dock from OBS if it is not already visible.

If OBS reports that the plugin failed to load, check the OBS log and verify that the plugin and its required runtime DLLs were installed into the correct OBS directories.

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
