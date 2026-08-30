# Vendored donor sources

VulkanLife imports selected features from other open-source Second Life
viewers. This document records the exact donor revisions so merges can be
repeated, diffed, and audited.

## Donors

| Donor | Repository | Branch | Pinned commit |
|---|---|---|---|
| Phoenix Firestorm | `FirestormViewer/phoenix-firestorm` | `master` | `89d84df58377c4622f367d3c419d5d67e4948153` |
| Aperture Viewer | `ApertureViewer/Aperture-Viewer` | `dev` | `933ecb09cc09398ffe99151ede11043d1a75df76` |

## Imported features and provenance

### From Firestorm (pinned commit above)

- **RLVa (Restrained Love API)** — `indra/newview/rlv*.{h,cpp}` plus
  integration call sites across `indra/newview`. Original author:
  Kitty Barnett. Per-file license headers are preserved verbatim.
- **Area Search** — `indra/newview/fsareasearch.{h,cpp}`,
  `indra/newview/fsareasearchmenu.{h,cpp}`, related XUI
  (`floater_fs_area_search.xml`) and strings. Copyright (c) 2012 Techwolf
  Lupindo, viewerlgpl.
- **Pie menu** — `indra/newview/piemenu.{h,cpp}`, `pieslice.{h,cpp}`,
  `pieseparator.{h,cpp}`, `pieautohide.{h,cpp}`, `menu_pie_*.xml` and related
  colors/fonts/sounds. Copyright (C) 2010 Linden Research, Inc. and (C) 2011
  Zi Ree @ Second Life, viewerlgpl.
- **Large chat input buffers** — UTF-8-aware message splitting
  (FIRE-787 pattern) applied to `send_chat_from_viewer` and
  `LLIMModel::sendMessage`; XUI input length raised to 4096.

### From Aperture Viewer (pinned commit above)

- **Procedural starfield** — `indra/newview/llvowlsky.{h,cpp}` and
  `indra/newview/lldrawpoolwlsky.{h,cpp}` changes (marked `<AP:WW>` in the
  donor source) plus `starsF.glsl` / `starsV.glsl` changes under
  `indra/newview/app_settings/shaders/class1/`. Copyright (C) 2025,
  William Weaver (paperwork) @ Second Life, viewerlgpl.

## Rules for vendored code

1. Keep vendored files byte-close to the donor revision to simplify future
   merges; local edits belong at call sites, not inside vendored logic.
2. Never strip or alter copyright/license headers in vendored files. The
   pre-commit `opensource-license` and `copyright` hooks must pass for them.
3. When updating a donor pin, record the new commit in this file and in
   `NOTICE`, and re-audit the diff for new third-party copyrights.
4. Local modifications inside vendored files, when unavoidable, are wrapped
   in `<VL:...>` / `</VL:...>` marker comments so they are easy to find and
   re-apply after donor refreshes.
