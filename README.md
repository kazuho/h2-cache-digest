h2-cache-digest
===============

An implementation of [Cache Digests for HTTP/2](https://datatracker.ietf.org/doc/draft-kazuho-h2-cache-digest/).

Copyright (c) 2015,2016 Kazuho Oku; provided under the MIT License.

h2-cache-digest-00
---

Simulator for I-D [h2-cache-digest-00](https://tools.ietf.org/id/draft-kazuho-h2-cache-digest-00.txt).

Generate a CACHE_DIGEST frame containing the URLs

```
% ./h2-cache-digest-00 --encode https://example.com/style.css https://example.com/jquery.js > frame.bin
```

Decode the CACHE_DIGEST frame.

```
% ./h2-cache-digest-00 --decode < frame.bin
N_log2: 1
P_log2: 8
Values: 356, 373
```

Pretend to be a server, that first requests the server to push `bootstrap.js`, `style.css`, and then `bootstrap.js`.
The server should cancel all pushes except the first push of `bootstrap.js`.

```
% ./h2-cache-digest-00 --push \
    https://example.com/bootstrap.js \
    https://example.com/style.css \
    https://example.com/bootstrap.js \
    < frame.bin 
https://example.com/bootstrap.js: should push; not cached
https://example.com/style.css: should NOT push; already cached
https://example.com/bootstrap.js: should NOT push; already cached
```
