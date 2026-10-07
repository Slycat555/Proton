# Proton 10 + Codecs

A fork of [Valve's Proton 10](https://github.com/ValveSoftware/Proton/tree/proton_10.0)
that adds the video and audio codecs Valve leaves out, so in-game videos and audio
play correctly — **including outside the Steam client** (Heroic, Lutris, umu, etc.).

Without it, many games show a **TV test pattern** instead of cutscenes, or play
**videos and music with no sound**.

| | Valve Proton 10 | This fork |
|---|---|---|
| H.264, HEVC, AAC, WMV/VC-1, WMA, WMA Pro, MPEG-1/2/4 decoders | ❌ | ✅ |
| `.wmv` / `.asf` and `.mpg` / `.vob` containers | ❌ | ✅ |
| Cutscenes outside the Steam client | Test pattern / silence | ✅ Plays |
| xWMA audio (XACT / XAudio2) outside Steam | Silent | ✅ Plays |
| WMA audio in Media Foundation videos | Silent | ✅ Plays |
| Build fails if a codec goes missing | — | ✅ |

---

## Why this exists

Formats such as H.264, AAC, WMV and WMA are patent-encumbered, so Valve does not
ship decoders for them. Instead, Proton relies on a **media converter**:

1. Proton records the media a game plays.
2. Valve transcodes it to open formats (AV1 / Opus) and delivers it via the Steam client.
3. At playback, Proton swaps in the transcoded version — or, if none exists yet,
   a placeholder (the test pattern, or silence).

This works inside Steam, but breaks in several ways elsewhere:

- **No decoders.** FFmpeg was built with everything disabled, and gst-libav's ASF
  demuxer is registered at rank `NONE`, so `.wmv` files could not even be opened.
- **The converter outranks real decoders.** Proton's audio converter is registered
  one rank above FFmpeg's. Outside Steam it fails on startup
  (`MEDIACONV_AUDIO_DUMP_FILE not set`) and Wine never falls back, so audio is silent.
- **A Wine bug, normally hidden by the converter.** Wine's WMA decoder did not
  implement `GetInputCurrentType` / `GetOutputCurrentType`. The Media Foundation
  source reader calls these while setting up audio, so apps disabled the audio
  track and played videos silently.

## What's changed

**Build (`Makefile.in`)**
- FFmpeg is built with all native decoders, demuxers and parsers, and **shipped**
  (Valve built it only to link gst-libav). Encoders, muxers, networking, devices,
  hardware acceleration and autodetected system libraries stay disabled.
- Adds **gst-plugins-ugly** (`asfdemux`, RealMedia, DVD LPCM/subtitles) and enables
  **`mpegdemux`** in gst-plugins-bad.
- Adds a **build-time codec check** (see below).

**Wine (`wine` submodule, `winegstreamer`)**
- Real decoders are preferred over the Proton media converter in both the
  Media Foundation transform path and decodebin; the converter is only a fallback.
- Implements `GetInputCurrentType` / `GetOutputCurrentType` on the WMA decoder.

Everything else is unchanged from Valve's `proton_10.0` branch.

## Installation

### From a release

1. Download `proton-10-codecs.tar.gz` from the [Releases](../../releases) page.
2. Extract it into Steam's compatibility tools folder:
   ```sh
   mkdir -p ~/.local/share/Steam/compatibilitytools.d
   tar -xzf proton-10-codecs.tar.gz -C ~/.local/share/Steam/compatibilitytools.d
   ```
   Flatpak Steam uses
   `~/.var/app/com.valvesoftware.Steam/data/Steam/compatibilitytools.d` instead.
3. Restart Steam / your launcher.

### Building it yourself

See [Building](#building) below.

## Usage

**Steam:** Game → Properties → Compatibility → *Force the use of a specific Steam
Play compatibility tool* → **proton-10-codecs**.

**Heroic:** Game settings → *Wine Version* → **proton-10-codecs**.

**Lutris:** Configure → Runner options → *Wine version* → **proton-10-codecs**.

**umu-launcher:**
```sh
PROTONPATH=~/.local/share/Steam/compatibilitytools.d/proton-10-codecs umu-run game.exe
```

If you previously ran the game with another Proton build and videos still fail,
delete the `gstreamer-1.0` folder in the game's prefix (it is only a plugin cache).

## Building

Building uses Valve's Steam Runtime SDK container, so you need **Podman** (recommended)
or **Docker**, plus `git`, `make` and roughly **50 GB** of free disk space.
A first build takes about 1–3 hours; later builds are incremental.

```sh
git clone --recurse-submodules -b codecs https://github.com/Slycat555/Proton.git proton-codecs
mkdir proton-codecs-build && cd proton-codecs-build
../proton-codecs/configure.sh --enable-ccache --build-name=proton-10-codecs
make redist
```

The result is in `redist/`; copy it to `compatibilitytools.d/proton-10-codecs`
(or run `make install` to install it into `~/.steam/root/compatibilitytools.d`).

Notes:
- **SELinux** (Fedora, Bazzite, Bluefin, …): add `--relabel-volumes` to `configure.sh`
  if it reports *"The container cannot access files"*.
- **Running from a Flatpak terminal or editor** (e.g. Flatpak VS Code): run the
  commands on the host, e.g. `flatpak-spawn --host make redist`.
- The `wine` submodule uses a relative URL (`../wine`), so this repository expects
  a matching fork of [ValveSoftware/wine](https://github.com/ValveSoftware/wine)
  under the same account.

For all other build targets and options (debug builds, `make deploy`,
`make module=...`), see Valve's [original README](README.upstream.md).

## Codec check

Every `make redist`, `make deploy` and `make install` runs
[`codec-check`](codec-check/codec_check.c) inside the Steam Runtime container
against the packaged plugins, for both **i386** and **x86_64**. The build fails
unless each format below has a working, real (non-Proton-converter) decoder or demuxer:

| Audio | Video | Containers |
|---|---|---|
| WMA v2 / xWMA | WMV3 (WMV9) | ASF / WMV |
| WMA Pro | VC-1 | MP4 / MOV |
| AAC | H.264 | AVI |
| MP3 | HEVC | Matroska / WebM |
| MS ADPCM | MPEG-2 | MPEG-PS (`.mpg`, `.vob`) |
| Vorbis | MPEG-4 Part 2 | |
| Opus | VP8, Theora | |

Run it on its own with `make codec-check`. To cover a new format, add a line to
the `checks[]` table in `codec-check/codec_check.c`.

This verifies that the codecs are present. It does not test Wine's playback code
paths, so bugs like the WMA decoder issue above still need testing in a game.

## Troubleshooting

Collect a log by launching the game with:

```sh
PROTON_LOG=1 GST_DEBUG=3 %command%
```

(in Heroic/Lutris, set these as environment variables). The log is written to
`~/steam-<appid>.log`. Useful things to look for:

- `Failed to find any element factory matching ...` followed by
  `Failed to create winegstreamer transform` — a missing decoder.
- `protonmediaconverter` errors — the media converter is being used instead of a
  real decoder.
- `fixme:wmadec` / `fixme:mfplat` ... `stub!` — an unimplemented Wine function.

Please include the log when opening an issue.

## Legal

This project bundles FFmpeg and GStreamer plugins that decode patent-encumbered
formats (e.g. H.264, HEVC, AAC, WMV, WMA). Depending on where you live, using or
distributing these decoders may require patent licenses. You are responsible for
complying with the laws of your jurisdiction.

FFmpeg is built **without** `--enable-gpl` or `--enable-nonfree`, so it remains
LGPL-2.1+. Proton is licensed under the terms in [LICENSE](LICENSE) and
[LICENSE.proton](LICENSE.proton); bundled components keep their own licenses.

This project is not affiliated with or endorsed by Valve. "Proton" and "Steam" are
trademarks of Valve Corporation.

## Credits

- [Valve and CodeWeavers](https://github.com/ValveSoftware/Proton) for Proton and Wine.
- The [FFmpeg](https://ffmpeg.org) and [GStreamer](https://gstreamer.freedesktop.org) projects.
- [GE-Proton](https://github.com/GloriousEggroll/proton-ge-custom), which pioneered
  shipping codecs with Proton.
