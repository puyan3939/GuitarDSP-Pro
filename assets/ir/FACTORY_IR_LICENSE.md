# GuitarDSP-Pro Factory Cabinet IR provenance

Factory IR pack: **Jester Dyne Productions – Jester's Brutal IR Pack 1.0**

Original publisher page:
https://www.jester-dyne-productions.com/brutal-ir-pack/

The original publisher describes the pack as free for commercial use. Darwin's Cat / OrbitCab additionally maintains an explicit per-asset license ledger identifying **Jesters Brutal Pack 1.0 as CC0 / public domain and cleared to bundle + redistribute commercially**.

GuitarDSP-Pro obtains the 48 kHz / 24-bit / mono captures from the following pinned public GitHub source:

- Repository: `darwinscat/orbitcab`
- Commit: `9081c0bdd84b325836d56aaebdb3955dbd9ccc0c`
- Source directory: `resources/ir/`
- License proof in that repository: `docs/ASSET-LICENSES.md`

The Git commit is pinned so the Factory IR source cannot silently change between builds. CMake also prints the downloaded source archive SHA-256 into the build log for reproducibility.

## Capture set

The pack contains 15 cabinet impulse responses. GuitarDSP-Pro installs the 48 kHz mono, 24-bit WAV captures without modifying their sample data. The pack documentation describes a modified oversized Behringer BG412S 4x12 cabinet using Celestion Vintage 30, Celestion Rockdriver Jr G12F-60, and Eminence DV-77 speakers, captured with several dynamic microphones. Exact speaker/microphone assignment for an individual creatively-named IR is not asserted by GuitarDSP-Pro unless supplied by the original pack metadata.

Installed captures:

1. `1_Cookie_Monster.wav`
2. `2_Darth_Genocider.wav`
3. `3_Kitten_Slayer.wav`
4. `4_Kaiju_Tamer.wav`
5. `5_Iceburn_Suicide.wav`
6. `6_Vertical_Lip_Stabber.wav`
7. `7_Manslaughter_Joe.wav`
8. `8_Big_Bubba.wav`
9. `9_Devils_Cunnilingus.wav`
10. `10_October_32th.wav`
11. `11_Wumbo.wav`
12. `12_World_Collider.wav`
13. `13_Cannibal_Choir.wav`
14. `14_Cathode_Ray_Fleshburn.wav`
15. `15_Impaler_Jim.wav`

GuitarDSP-Pro's 1024 / 2048 / FULL selector controls how much of the selected capture is passed to JUCE Convolution. FULL preserves the original long capture and room tail; the shorter modes are intended for lower-cost guitar-cab processing and A/B tests.

Speaker, microphone, cabinet, and product names are descriptive references only. GuitarDSP-Pro is not affiliated with or endorsed by those manufacturers, Jester Dyne Productions, or Darwin's Cat.
