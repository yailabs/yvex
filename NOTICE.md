# Notices and attribution

YVEX is an independently MIT-licensed project in the YAI technology base.
Original YVEX code and documentation are licensed
under the [MIT license](LICENSE). Distributions retain both `LICENSE` and
`NOTICE.md`; the product package places them in `share/yvex/`.

## First-party ownership and brand

Copyright (c) 2026 Francesco Maiomascio for first-party material authored and
owned by him. Repository history records his authorship; it is not evidence
of a corporate assignment or of ownership of third-party material. Any other
contributors retain their applicable rights and notices.

YAI is the intended company and flagship product name. The legal entity has
not yet been incorporated and is not the present IP owner. No assignment,
trademark registration or patent ownership is asserted here. A later transfer
requires an actual agreement with the relevant rights holders before updating
ownership notices. YAI's proprietary/source-available terms do not apply to
YVEX or REPLAI, which remain MIT.

The obsolete name "YAI Labs" is not a current company or first-party rights
holder. Existing `yailabs` repository/model namespaces, `yai-labs.com` domain
references and immutable historical release labels remain technical or
historical identifiers, not claims that such a company owns this software.

## Model assets and external dependencies

Model weights, tokenizer assets, datasets, and model cards are acquired
separately and retain their respective upstream licenses and use conditions.
YVEX's MIT license does not relicense those assets. Source acquisition,
verification, or successful execution does not grant redistribution rights.

System libraries, NVIDIA CUDA and cuBLAS, provider tools, and SDKs installed
for integration tests retain their own licenses. They are not licensed by
YVEX merely because it links to or invokes them. A distributor that bundles
those components must retain their applicable notices and terms.

The terminal product statically links [REPLAI](https://github.com/mothx9/replai),
MIT, at revision `93d62f6d34cfb933a1f59407ade027152e1ef2ba` as pinned in
`config/replai.json`. This qualified revision attributes its first-party copyright
to Francesco Maiomascio. The original upstream license and the native build
receipt are staged under `share/licenses/replai/LICENSE` and the private prefix.
REPLAI and its Rust dependencies retain their respective upstream licenses.
Historical notices remain in their immutable historical artifacts; this repin
does not retrospectively change those artifacts or assign anyone's rights.
The REPLAI license alone is not a complete notice bundle for a statically linked
binary: distributors must also carry the notices required by the exact linked
Rust dependencies. The current build receipt/pin is provenance, not a complete
third-party compliance certification.

`make package` now labels that staged software candidate `UNQUALIFIED` for
distribution. The [artifact-bound legal gate](docs/releases/distribution-legal.md)
requires original recipient notices and corresponding source where applicable;
`make qualify-distribution DISTRIBUTION_PACKAGE=...` refuses unresolved policy
or missing/stale material. The product profile remains blocked until the exact
compiled contribution closure is retained. This is not a claim of completed
repository distribution readiness.

## Unicode data

The classification tables in `src/tokenizer/unicode.c` and normalization
tables in `src/tokenizer/normalization.c` derive from the
[Unicode Character Database 15.0.0](https://www.unicode.org/Public/15.0.0/ucd/ReadMe.txt).
Their Unicode copyright and permission notice follows, as preserved in
[Unicode's 2022 license](https://github.com/unicode-org/icu/blob/ff3514f257ea10afe7e710e9f946f68d256704b1/icu4c/LICENSE).
The notice covers the derived Unicode data; it does not imply that ICU is
bundled with YVEX.

```text
UNICODE, INC. LICENSE AGREEMENT - DATA FILES AND SOFTWARE

See Terms of Use <https://www.unicode.org/copyright.html>
for definitions of Unicode Inc.’s Data Files and Software.

NOTICE TO USER: Carefully read the following legal agreement.
BY DOWNLOADING, INSTALLING, COPYING OR OTHERWISE USING UNICODE INC.'S
DATA FILES ("DATA FILES"), AND/OR SOFTWARE ("SOFTWARE"),
YOU UNEQUIVOCALLY ACCEPT, AND AGREE TO BE BOUND BY, ALL OF THE
TERMS AND CONDITIONS OF THIS AGREEMENT.
IF YOU DO NOT AGREE, DO NOT DOWNLOAD, INSTALL, COPY, DISTRIBUTE OR USE
THE DATA FILES OR SOFTWARE.

COPYRIGHT AND PERMISSION NOTICE

Copyright © 1991-2022 Unicode, Inc. All rights reserved.
Distributed under the Terms of Use in https://www.unicode.org/copyright.html.

Permission is hereby granted, free of charge, to any person obtaining
a copy of the Unicode data files and any associated documentation
(the "Data Files") or Unicode software and any associated documentation
(the "Software") to deal in the Data Files or Software
without restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, and/or sell copies of
the Data Files or Software, and to permit persons to whom the Data Files
or Software are furnished to do so, provided that either
(a) this copyright and permission notice appear with all copies
of the Data Files or Software, or
(b) this copyright and permission notice appear in associated
Documentation.

THE DATA FILES AND SOFTWARE ARE PROVIDED "AS IS", WITHOUT WARRANTY OF
ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT OF THIRD PARTY RIGHTS.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN THIS
NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL
DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THE DATA FILES OR SOFTWARE.

Except as contained in this notice, the name of a copyright holder
shall not be used in advertising or otherwise to promote the sale,
use or other dealings in these Data Files or Software without prior
written authorization of the copyright holder.
```
