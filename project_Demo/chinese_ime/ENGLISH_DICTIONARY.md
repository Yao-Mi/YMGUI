# English dictionary provenance

`english_words.bin` is generated from the English `large` word list in
[`wordfreq` 3.1.1](https://github.com/rspeer/wordfreq). The source package and
its data are distributed under Apache License 2.0:

```text
Copyright 2022 Robyn Speer

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```

YMGUI keeps unique lowercase ASCII alphabetic entries of 2 through 31 bytes.
The resulting file contains 288996 words. `wordfreq` supplies the source order
from most frequent to least frequent; the generator converts that rank to a
monotonic integer used only for candidate ordering. Runtime user selections
still take precedence over this initial order.
