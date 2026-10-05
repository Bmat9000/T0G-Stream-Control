# T0G Live Tools Architecture

## Products

### Standalone
The existing Python/PySide application remains the compatibility and recovery tool for Streamlabs/TikTok LIVE authentication and RTMP retrieval.

### OBS Plugin
`obs-plugin/` is the primary T0G streaming experience. It will own the OBS dock, platform controls, output orchestration, status, and future Aitum integration.

## Security rules

- Never print OAuth tokens or stream keys.
- Never commit credentials.
- Hide stream keys by default.
- Prefer session-only credentials or an OS credential store instead of plaintext configuration.
- Keep Twitch and TikTok outputs independent so one platform cannot overwrite the other's destination.

## Development order

Dock -> TikTok auth -> TikTok LIVE creation -> automatic TikTok output -> Twitch metadata -> Aitum/vertical output -> unified Go Live -> monitoring/Stream Deck actions.
