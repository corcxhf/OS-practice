# TinyCC Upstream

This directory vendors TinyCC so a normal clone of MyOS contains every source
file required by the default build.

- Upstream: `https://github.com/TinyCC/tinycc`
- Upstream commit: `a338258d309c888bde96b2d1f206299231a54ddf`
- Upstream branch at import: `mob`
- License: see `COPYING` and `RELICENSING`

The vendored tree includes MyOS-specific runtime, libc glue, and target changes.
Do not replace it with an unpinned checkout: review and reapply the MyOS changes
when updating to a newer upstream commit.
