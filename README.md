<div align="center">

# calculate

**A scientific calculator that shows every result with its error and with the digits you can trust. <br> A Qt 6 app for desktop and browser, built on the [**calculate-core**](https://github.com/bentxr/calculate-core) engine.**

[![ci](https://github.com/bentxr/calculate/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/bentxr/calculate/actions/workflows/ci.yml)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![Qt](https://img.shields.io/badge/Qt-6.11-41CD52?logo=qt&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.25%2B-064F8C?logo=cmake&logoColor=white)
![Status](https://img.shields.io/badge/status-early%20development-orange)
![License](https://img.shields.io/badge/license-TBD-lightgrey)

</div>

- **Honest results.** Every answer comes with a guaranteed error bound, the error actually
  measured, and the digits that can be trusted. The digits you can't trust are shown, in a different shade.
- **Choose your data type.** Compute in `float`, `double`, `long double`, exact fractions, or
  IEEE 754 quadruple, octuple and 512-bit precision.
- **Desktop and browser.** One C++17 code base. A native Linux app and WebAssembly for the web.
  

## On the math

The engine's [README](https://github.com/bentxr/calculate-core#reading-a-result) explains it in depth.

## Using it

- **Calculator and Statistics.** Pick the mode in the side panel. Statistics takes a list of
  values, one per line or separated by commas.
- **Keyboard or keys.** Type on the keyboard or press the keys. The arrows move through the
  boxes of a fraction or root, and up and down browse the history.
- **Memory.** `M+`, `M−`, `MC` and `Ans`, as on a regular calculator.
- **Slow calculations** run in the background, and can be cancelled.
- **Settings.** English or Spanish, and a light or dark theme. Both follow the system by default
  and change while the app runs.

## Build and test

Requires CMake ≥ 3.25, Ninja, a C++17 compiler and Qt 6.11 (Widgets and LinguistTools). The engine,
the test framework and the font are fetched automatically.

```bash
cmake --preset desktop-ci && cmake --build --preset desktop-ci && ctest --preset desktop-ci
build/desktop-ci/calculate
```

<details>
<summary>WebAssembly</summary>

You need the Emscripten SDK (4.0.7) and the Qt for WebAssembly *multithread* kit, plus a desktop
Qt of the same version.

```bash
source /path/to/emsdk/emsdk_env.sh
export QT_WASM=/path/to/Qt/6.11.2/wasm_multithread
export QT_HOST=/path/to/Qt/6.11.2/gcc_64
cmake --preset wasm-ci && cmake --build --preset wasm-ci
tools/serve.py build/wasm-ci      # http://127.0.0.1:8765/calculate.html
```

</details>

To work on the engine and the app together, clone `calculate-core` next to this repository and use
the `desktop` and `wasm` presets instead: they take the engine from `../calculate-core`.


## Status

Early development. 

## License

Not chosen yet. Until one is added, all rights are reserved.
