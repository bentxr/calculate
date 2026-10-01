# Hosting the web version

The web version runs its calculations on a worker thread, which browsers only allow on
*cross-origin isolated* pages. The server must send two headers with every file, and serve
`.wasm` files with the `application/wasm` type.

Files: `calculate.html` (the page), `calculate.js`, `calculate.wasm`, `qtloader.js`.
They must stay together in one directory.

## nginx

```nginx
location /calculate/ {
    add_header Cross-Origin-Opener-Policy same-origin always;
    add_header Cross-Origin-Embedder-Policy require-corp always;
    types { application/wasm wasm; }
}
```

## Apache

```apache
<Directory "/var/www/calculate">
    Header always set Cross-Origin-Opener-Policy "same-origin"
    Header always set Cross-Origin-Embedder-Policy "require-corp"
</Directory>
AddType application/wasm .wasm
```

## Embedding in another page

Give the calculator its own page rather than an `<iframe>`. An iframe works only if the *parent*
page is cross-origin isolated too, and every resource on the parent then needs the
same headers (or `crossorigin` attributes), which usually breaks other content.

## Checking it

Open the page's developer console and evaluate `crossOriginIsolated`: it must be `true`.
To test locally: `tools/serve.py build/wasm` in the source repository.
