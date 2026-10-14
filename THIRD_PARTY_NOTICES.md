# Third-party notices

The vendored nlohmann JSON single header and its exact MIT license are in
`third_party/nlohmann/`. The source was taken byte-for-byte from the native
engine at `ff32c520dd4ccd9073c9dc001a180e886d50ab63`; the build verifies its
SHA-256 recorded in `native/dependencies.lock.json`.

The process-v2 wire convention was studied in the previously MIT-distributed
native engine snapshot at `ff32c520dd4ccd9073c9dc001a180e886d50ab63` and the
public MyWallpaper add-on manifest contract. DEC-0160 now identifies the native
engine and its internal SDK as proprietary; this add-on links neither of them.
This original companion implements the existing host protocol for external
observation. It ships no
Windhawk, MinHook or injected agent binary. The local MIT license preserves the
MyWallpapers copyright notice. The engine's current proprietary notice is not
replaced, relabeled or included as an MIT runtime component.

Windhawk animation mods were studied as implementation evidence, pinned to
`8d60b4c4113a7f0a18b4b48a5443a199acf54805`. Their source code was not copied
into this add-on. In particular, no GPL Wobbly/DWM implementation is linked,
adapted or included. See `docs/source-study.md` for the mechanisms and limits.
