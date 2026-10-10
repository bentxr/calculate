#!/usr/bin/env python3
"""Checks, in a headless browser, that the device's on-screen keyboard can only come up while the
Keyboard button is on.

A browser offers that keyboard when an editable element that does not decline it (inputmode "none")
takes the focus, so the checks are about the page's focus. Whatever is tapped, nothing of the kind takes
it, except at one place, the Keyboard button, and a second tap there takes it away again. A physical
keyboard types in both states. The button is found by tapping, not by coordinates, so the layout is free
to change.

Usage: tests/browser/device_keyboard.py build/wasm   (needs chromedriver and its browser)"""
import json, os, socket, subprocess, sys, time, urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
PROFILES = {  # name: (width, height, touch)
    "phone": (390, 844, True),
    "tablet": (820, 1180, True),
    "desktop": (1280, 800, False),
}
STEP = 20  # of the tapped grid, in CSS pixels: finer than any button
CONTROL, ENTER = chr(0xE009), chr(0xE007)  # WebDriver's codes for these keys

# Run in the page. `focused` is the element that really has the focus, through shadow roots; `invites`
# says whether a browser would offer its keyboard for it.
PAGE = """
const focused = () => { let e = document.activeElement; while (e && e.shadowRoot && e.shadowRoot.activeElement) e = e.shadowRoot.activeElement; return e; };
const editable = e => !!e && (e.isContentEditable || ((e.tagName === 'INPUT' || e.tagName === 'TEXTAREA') && !e.readOnly && !e.disabled));
const invites = e => editable(e) && e.inputMode !== 'none';
const described = e => !e ? 'nothing' : e.tagName.toLowerCase() + (e.isContentEditable ? ' contenteditable' : '') + ' inputmode=' + (e.inputMode || 'unset');
"""
# Counts every time such an element takes the focus, however briefly. A focus that moves inside a shadow
# tree is not reported outside it, so every tree is watched.
WATCH = PAGE + """
window.invitations = 0;
const watch = root => {
    root.addEventListener('focusin', event => { if (invites(event.composedPath()[0])) ++window.invitations; }, true);
    for (const e of root.querySelectorAll('*')) if (e.shadowRoot) watch(e.shadowRoot);
};
watch(document);
return true;"""


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def wait_for(what, check, seconds=60):
    end = time.time() + seconds
    while time.time() < end:
        try:
            if check():
                return
        except OSError:
            pass
        time.sleep(0.2)
    raise SystemExit(f"timed out waiting for {what}")


class Page:
    def __init__(self, driver, app, width, height, touch):
        self.driver, self.app, self.size, self.touch = driver, app, (width, height), touch
        options = {"args": ["--headless=new", f"--window-size={width},{height}", "--no-sandbox"]}
        if touch:
            options["mobileEmulation"] = {"deviceMetrics": {"width": width, "height": height, "pixelRatio": 2.0, "touch": True}}
        capabilities = {"capabilities": {"alwaysMatch": {"browserName": "chrome", "goog:chromeOptions": options}}}
        self.base = "/session/" + self.call("POST", "/session", capabilities)["sessionId"]
        self.cdp("Browser.grantPermissions", {"origin": app.rsplit("/", 1)[0], "permissions": ["clipboardReadWrite", "clipboardSanitizedWrite"]})
        self.cdp("Emulation.setFocusEmulationEnabled", {"enabled": True})
        self.load()

    def call(self, method, path, body=None):
        data = None if body is None else json.dumps(body).encode()
        request = urllib.request.Request(self.driver + path, data=data, method=method, headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(request) as response:
            return json.load(response)["value"]

    def close(self):
        self.call("DELETE", self.base)

    def cdp(self, command, parameters=None):
        return self.call("POST", self.base + "/goog/cdp/execute", {"cmd": command, "params": parameters or {}})

    def js(self, script):
        return self.call("POST", self.base + "/execute/sync", {"script": PAGE + script, "args": []})

    def load(self):  # a fresh page, started and watched
        self.call("POST", self.base + "/url", {"url": self.app})
        wait_for("the app to start", lambda: self.js(
            "const s = document.querySelector('#screen'); return !!s && getComputedStyle(s).display !== 'none' && focused() !== document.body;"))
        time.sleep(1)
        self.call("POST", self.base + "/execute/sync", {"script": WATCH, "args": []})

    def press(self, x, y):  # a finger's tap, or a click where there is a mouse
        if self.touch:
            self.cdp("Input.dispatchTouchEvent", {"type": "touchStart", "touchPoints": [{"x": x, "y": y}]})
            self.cdp("Input.dispatchTouchEvent", {"type": "touchEnd", "touchPoints": []})
        else:
            for kind in ("mousePressed", "mouseReleased"):
                self.cdp("Input.dispatchMouseEvent", {"type": kind, "x": x, "y": y, "button": "left", "clickCount": 1})

    def keys(self, text, control=False):  # a physical keyboard
        steps = [{"type": "keyDown", "value": CONTROL}] if control else []
        for c in text:
            steps += [{"type": "keyDown", "value": c}, {"type": "keyUp", "value": c}]
        if control:
            steps.append({"type": "keyUp", "value": CONTROL})
        self.call("POST", self.base + "/actions", {"actions": [{"type": "key", "id": "keyboard", "actions": steps}]})
        time.sleep(0.3)

    def escape(self):  # closes a menu or a popup, quickly
        for kind in ("rawKeyDown", "keyUp"):
            self.cdp("Input.dispatchKeyEvent", {"type": kind, "key": "Escape", "code": "Escape", "windowsVirtualKeyCode": 27})

    def clipboard(self, text=None):
        script = ("const done = arguments[arguments.length - 1];"
                  + ("navigator.clipboard.readText()" if text is None else "navigator.clipboard.writeText(arguments[0])")
                  + ".then(t => done(t === undefined ? 'written' : t), e => done('ERROR ' + e));")
        return self.call("POST", self.base + "/execute/async", {"script": script, "args": [] if text is None else [text]})

    def focus(self):  # what has the focus now, and how often the keyboard has been invited so far
        return self.js("return {invites: invites(focused()), is: described(focused()), invitations: window.invitations};")


def check(name, page):
    failures = []

    def expect(condition, message):
        if not condition:
            failures.append(f"{name}: {message}")
        return condition

    # Digits only: what a headless browser types for other keys depends on its keyboard layout.
    def typed(keys, result, when):  # typing gives the result, read back by copying it
        page.keys(keys + ENTER)
        page.keys("c", control=True)
        copied = page.clipboard()
        expect(copied == result, f"{when}, typing {keys} Enter and copying gave '{copied}', not '{result}'")

    focus = page.focus()
    expect(not focus["invites"], f"at start the focus invites the device keyboard: {focus['is']}")

    # Whatever is tapped, nothing invites the keyboard, except at one place: there a second tap undoes the first.
    places, invitations = [], page.focus()["invitations"]
    width, height = page.size
    for x, y in [(x, y) for y in range(STEP // 2, height, STEP) for x in range(STEP // 2, width, STEP)]:
        page.press(x, y)
        focus = page.focus()
        if not focus["invites"] and focus["invitations"] == invitations:
            page.escape()  # whatever else the tap opened
            continue
        places.append((x, y))
        page.press(x, y)
        again = page.focus()
        invitations = again["invitations"]
        if not expect(not again["invites"],
                      f"a tap at {x},{y} gave the focus to {focus['is']}, which invites the device keyboard, and a second tap did not take it away"):
            return failures
    if not expect(places, "no tap anywhere invited the device keyboard: the Keyboard button was not found"):
        return failures
    xs, ys = [x for x, _ in places], [y for _, y in places]
    expect(max(xs) - min(xs) <= 5 * STEP and max(ys) - min(ys) <= 3 * STEP,
           f"more than one place invites the device keyboard: taps at {places}")
    button = (sum(xs) // len(xs), sum(ys) // len(ys))

    # The button, on a fresh page: on gives the focus to a text input, off takes it away, time after time,
    # and a physical keyboard types and pastes in both states.
    page.load()
    for press in (1, 3):
        page.press(*button)
        focus = page.focus()
        expect(focus["invites"], f"press {press} of the Keyboard button did not invite the device keyboard: the focus is on {focus['is']}")
        typed("7", "7", "with the keyboard on")
        page.clipboard("41")
        page.keys("v", control=True)
        typed("1", "411", "with the keyboard on, after pasting 41")
        page.press(*button)
        focus = page.focus()
        expect(not focus["invites"], f"press {press + 1} of the Keyboard button left the focus on {focus['is']}, which invites the device keyboard")
        typed("8", "8", "with the keyboard off again")
        page.clipboard("52")
        page.keys("v", control=True)
        typed("3", "523", "with the keyboard off again, after pasting 52")
    return failures


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    serve_port, driver_port = free_port(), free_port()
    server = subprocess.Popen([sys.executable, os.path.join(HERE, "..", "..", "tools", "serve.py"), sys.argv[1], str(serve_port)],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    driver = subprocess.Popen(["chromedriver", f"--port={driver_port}"], stdout=subprocess.DEVNULL)
    failures = []
    try:
        app, driver_url = f"http://127.0.0.1:{serve_port}/calculate.html", f"http://127.0.0.1:{driver_port}"
        wait_for("the server", lambda: urllib.request.urlopen(app).status == 200)
        wait_for("chromedriver", lambda: urllib.request.urlopen(driver_url + "/status").status == 200)
        for name, (width, height, touch) in PROFILES.items():
            started = time.time()
            page = Page(driver_url, app, width, height, touch)
            try:
                found = check(name, page)
            finally:
                page.close()
            print(f"{name}: {'ok' if not found else 'FAILED'} ({time.time() - started:.0f} s)", flush=True)
            failures += found
    finally:
        driver.terminate()
        server.terminate()
    for failure in failures:
        print(failure)
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
