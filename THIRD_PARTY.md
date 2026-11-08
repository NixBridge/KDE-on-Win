# Third-party provenance and licenses

The root GPL-3.0-or-later license applies to new, independently authored integration code. Per-file SPDX identifiers and upstream license notices take precedence for vendored or derived files. Modifications and patches to an upstream component are distributed under that component's applicable license, not silently relicensed by the root LICENSE.

| Component | Source / pin | License handling |
| --- | --- | --- |
| Plasma Windows base | https://github.com/ChPu437/plasma-windows at 7078fc706f780c4c28e90f222467a04d816df48b | Original LICENSE retained under upstream/plasma-windows; individual files include LGPL and GPL SPDX notices. |
| KDE Plasma / Frameworks patches | Official KDE releases identified in upstream-versions.json and source-verification.json | Retain each modified file's upstream SPDX license. Target release sources remain the authority for component notices. |
| Qt and other runtime dependencies | KDE Craft recipes pinned in build-lock.json | Not vendored as dependency binaries in this Git repository; runtime distribution must retain their notices and satisfy their respective licenses. |
| GNU GPL v3 text | SPDX license-list-data GPL-3.0-or-later.txt | Unmodified license text follows the separate AI authorship notice in the root LICENSE. |

Upstream authors' copyright notices are preserved. Do not replace them with this project's name when editing vendored source. Build caches, private logs, VM archives and upstream Git metadata are excluded from the repository.

AI authorship disclosure is recorded in LICENSE and AI_NOTICE.md; it adds no distribution conditions and does not apply to upstream authorship.
