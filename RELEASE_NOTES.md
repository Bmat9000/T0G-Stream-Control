# Clip shortcuts and Twitch login persistence

## Added
- Release output is saved to notepad.txt and opens in Notepad on failure, including setup errors.
- Local Windows build and release command: `release-nexus.cmd`, with installer output, release notes, and a troubleshooting log.
- Automatic renewal of saved Twitch device-login credentials when account validation reports an expired access token.

## Improved
- Automatic GitHub Actions builds are disabled; build and publish from your Windows PC instead.
- Replay hotkey changes made through OBS Settings are saved promptly instead of relying on plugin unload.
- Renewed Twitch credentials remain in Windows Credential Manager across OBS restarts and plugin updates.

## Fixed
- Plugin shutdown no longer overwrites saved clip bindings after OBS begins clearing hotkeys.
- Clip shortcuts assigned in T0G Settings are flushed to storage immediately.
- Temporary Twitch renewal failures preserve saved credentials for another attempt.
