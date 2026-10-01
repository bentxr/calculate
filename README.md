# calculate

A scientific calculator that shows every result together with its error and the digits you can
trust. It is a Qt 6 app that runs on the desktop and in the browser (WebAssembly), on top of the
[calculate-core](https://github.com/bentxr/calculate-core) engine.

## Build

Requirements: CMake 3.25+, Ninja, a C++17 compiler and Qt 6.11 (Widgets, LinguistTools).

The engine is fetched from `../calculate-core` when that directory exists next to this one. The
`-ci` presets (`desktop-ci`, `wasm-ci`) fetch it from GitHub instead.

Desktop:

```bash
cmake --preset desktop && cmake --build --preset desktop && ctest --preset desktop
build/desktop/calculate
```

Browser: needs the Emscripten SDK (4.0.7) and the Qt for WebAssembly *multithread* kit, plus a
desktop Qt of the same version as the build host.

```bash
source /path/to/emsdk/emsdk_env.sh
export QT_WASM=/path/to/Qt/6.11.2/wasm_multithread
export QT_HOST=/path/to/Qt/6.11.2/gcc_64
cmake --preset wasm && cmake --build --preset wasm
tools/serve.py build/wasm      # http://127.0.0.1:8765/calculate.html
```

To host the web version on your own server, see [docs/hosting.md](docs/hosting.md).
