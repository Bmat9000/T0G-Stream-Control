# T0G Stream Control — Chat Merger Update

## Added

- Choose a separate text color for each Twitch and TikTok chat/event type, alongside its card background color. Text colors are saved for future OBS sessions and apply to new cards.
- Twitch follower events in the Chat Merger. Reconnect your Twitch account after updating to grant follower access.
- Local Test buttons for Twitch and TikTok event types so you can preview cards without going live.
- Chat text size controls (8–32 px) and card duration controls (5–600 seconds, with a 60-second default).

## Improved

- Twitch messages display role badge labels and native Twitch emote images, with Unicode emoji support retained.
- Twitch activity cards use more specific event labels for new subs, resubs, gifted subs, Bits/Cheers, and raids.
- Chat previews and incoming cards respect saved event visibility settings.

## Fixed

- Corrected Qt 6 compatibility issues in the updated chat renderer so the Windows plugin builds with Unicode text and automatic message sizing.

Close OBS, run **T0G-Stream-Control-Setup.exe**, then reopen OBS. Display changes apply to newly received cards.
