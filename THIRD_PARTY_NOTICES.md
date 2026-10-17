# Third-Party Notices

Except where noted below, Macro OSC source code and first-party assets are
licensed under the GNU Affero General Public License version 3 or later. See
`LICENSE`.

## JUCE

Macro OSC is built with JUCE as an external dependency configured by `JUCE_DIR`;
JUCE is not vendored in this repository.

JUCE framework modules are dual-licensed under the GNU Affero General Public
License version 3 and the commercial JUCE license. See the JUCE license terms in
the JUCE source distribution or at:

- https://github.com/juce-framework/JUCE/blob/master/LICENSE.md
- https://juce.com/legal/juce-8-licence/

## Mutable Instruments Braids/stmlib

Files under `Source/ThirdParty/Mutable/` are derived from Mutable Instruments
Braids/stmlib sources. The included files carry MIT license notices from Emilie
Gillet, and the upstream license text is preserved at:

- `Source/ThirdParty/Mutable/LICENSE`

## Airwindows distortion kernels

Files under `Source/ThirdParty/Airwindows/` are SDK-free adaptations of 13
Airwindows distortion algorithms by Chris Johnson, imported from Lodo commit
`9173fb9`. The upstream revision is `d22a25b7f7c0c8c05e9f3ae480e7f1da9769f49d`.
The MIT license is preserved in `Source/ThirdParty/Airwindows/LICENSE`; adaptation
details and the mapping from product names to algorithms are in that directory's
`README.md`. The accompanying Lodo filter, distortion wrappers and control
profiles under `Source/DSP/` are imported from the same Lodo revision.

## Geist and Geist Mono

Bundled files:

- `Source/Assets/Geist-Regular.ttf`
- `Source/Assets/Geist-Medium.ttf`
- `Source/Assets/GeistMono-Regular.ttf`

Copyright 2024 The Geist Project Authors
(https://github.com/vercel/geist-font).

These font files are licensed under the SIL Open Font License, Version 1.1. The
license text is included at:

- `Source/Assets/Licenses/Geist-OFL.txt`

## DSEG

Bundled file:

- `Source/Assets/DSEG14Modern-Regular.ttf`

Copyright (c) 2020, keshikan (https://www.keshikan.net), with Reserved Font
Name "DSEG".

This font file is licensed under the SIL Open Font License, Version 1.1. The
license text is included at:

- `Source/Assets/Licenses/DSEG-OFL-1.1.txt`
