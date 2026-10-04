# T0G Stream Control — OBS Plugin

Native OBS Studio plugin work lives here. The first milestone provides the **T0G Stream Control** dock and establishes the plugin structure without changing the working standalone application.

## Planned dock flow

1. Connect Twitch and TikTok.
2. Enter one stream title and game/category.
3. Update metadata for enabled platforms.
4. Create the TikTok LIVE session and obtain its RTMP destination.
5. Configure the TikTok output automatically.
6. Start Twitch landscape and TikTok vertical outputs.
7. Display live/output health in the dock.

The standalone application remains available while this plugin is developed.

## Current milestone

- Native OBS module scaffold.
- Qt dock scaffold.
- Shared title/game/audience controls.
- Twitch/TikTok enable controls and status placeholders.
- Update and Go Live controls ready for backend wiring.

Authentication, API writes, RTMP output configuration, and Aitum integration are intentionally the next milestones.
