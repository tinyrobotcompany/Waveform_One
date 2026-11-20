# Listening history and optional services

On the paired phone, open Settings → Listening history → Enable for this session.
History is **off by default on every device boot**. It stores up to 50 entries in
RAM, oldest first evicted, with title/artist/album, first observed time when the
clock is valid, and repeated-recognition duration. There is no audio in history,
no new flash storage, no background export, and no change to recognition cadence.
View / refresh reads the device's current list. Delete clears it; Disable and
delete also stops collection. Restart/OTA clears it. Export JSON explicitly
saves a copy in the phone's downloads; deleting device history does not delete
that file. Every paired phone can read/delete enabled history. Treat pairing
as access to personal listening data and revoke phones you no longer trust.

An entry requires 20 seconds between repeated accepted matches of the same
artist/title, no more than 15 seconds apart. Repeated display/artwork callbacks
do not add time. Silence/reset, a backward track offset or an evidence gap
starts a new candidate. This is a conservative acoustic observation log, **not
proof of complete playback, exact song-start time or a gapless listening diary**.
Missing matches can split a play; unidentified music is absent. Time is left
unknown when the device clock was not valid at first observation.

## Discogs

Each row has a Find on Discogs link using artist and album/title. It opens a
catalogue search only when tapped; the query then goes to Discogs. No Discogs
account, token, collection mutation or background API request is used. A search
result is not a verified pressing/edition or a scrobble. Discogs' [API terms](https://support.discogs.com/hc/en-us/articles/360009334593-API-Terms-of-Use)
describe catalogue/collection access; this feature does not claim an unsupported
listening-history API (reviewed 19 November 2026).

## Last.fm: explicit desktop export bridge

The first Last.fm integration is an optional Python 3 command-line bridge on
a Mac/Linux computer, **not an unattended on-device scrobbler**. No Last.fm
credentials cross the device's local HTTP connection. Obtain your own Last.fm
application key/secret and check your permitted API usage before using it.
The interactive login uses Last.fm's authorization page, not your password.

```sh
python3 tools/history_bridge/lastfm.py login
python3 tools/history_bridge/lastfm.py preview /path/to/waveform-history.json
python3 tools/history_bridge/lastfm.py enqueue /path/to/waveform-history.json --confirm-listened
python3 tools/history_bridge/lastfm.py submit
python3 tools/history_bridge/lastfm.py status
```

Preview is offline. Enqueue requires confirmation that the tracks were actually
listened to; review/correct the exported metadata first. It also rejects unknown
or future timestamps, unknown/short durations, and insufficient evidence.
Last.fm's [scrobbling rules](https://www.last.fm/api/scrobbling) require tracks
longer than 30 seconds and at least half the duration or four minutes listened,
whichever comes first. Acoustic observations cannot prove that condition, hence
the separate owner confirmation. Last.fm receives artist, title, album,
observation timestamp and duration only when **submit** is explicitly run.

The bridge keeps a private local SQLite queue/receipt store and private account
file under `~/.config/waveform-history/` (file permissions 0600). Up to 1,000
queued events/receipts are retained; the tool refuses new entries at that limit
rather than silently forgetting deduplication. No automatic cleanup or service
runs in the background. Per-boot event IDs plus account identity deduplicate
reimported exports. Last.fm rejection/ignored responses are not reported as
successful. Explicit temporary service errors are delayed with exponential
backoff; run submit later to retry. A timeout/crash can have an ambiguous
outcome: it is marked uncertain and **never blindly retried**. Check your Last.fm
profile, then resolve the event ID shown by status:

```sh
python3 tools/history_bridge/lastfm.py resolve EVENT_ID --outcome accepted
# Only if you verified it was not delivered:
python3 tools/history_bridge/lastfm.py resolve EVENT_ID --outcome not-delivered
```

Do not delete receipts and reimport old exports: that removes duplicate
protection. `delete-local-data` deletes the bridge account/queue, not exported
files or remote Last.fm records. Revoke authorization and delete remote history
through Last.fm separately. Credentials are access-controlled files, not encrypted
at rest. Existing paired-phone controls, browser rendering and queue tests use
fixtures; live account authorization/scrobbling and physical P4 acceptance are
not claimed. Official [authentication](https://www.last.fm/api/authentication)
and [request signing](https://www.last.fm/api/webauth) reviewed 19 November 2026.
