# Replay clips test update

## Added
- Reconnect Chat button in Chat Merger to manually reconnect Twitch chat/events and TikTok chat without restarting OBS or the streams.
- Replay Buffers & Clips section in the main T0G settings.
- Shared clip duration choices: 15, 30, 60, 120 and 300 seconds.
- Start Replay Buffers, Stop Replay Buffers and Save Both Clips shortcuts, with click-and-press assignment and OBS hotkey registration.
- One Auto start/stop toggle, enabled by default, following actual stream activity.
- Landscape and vertical buffer status and warnings when a buffer cannot save.

## Improved
- TikTok connection settings and the Update Title / Game action explain that changing TikTok stream details requires Automatic / Streamlabs; manual RTMP only sends the stream.
- Replay controls reuse OBS and compatible Aitum Vertical buffers and their existing clip folders.
- Replay preferences and shortcuts persist between OBS sessions and plugin updates.
- Test branch builds produce a separate downloadable installer artifact.

## Fixed
- Stream Control and Chat Merger now appear in the OBS Docks menu, so either panel can be reopened after closing it.
