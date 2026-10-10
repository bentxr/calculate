#pragma once

#include <QObject>

// The device's on-screen keyboard (an input method), decided in one place for the whole app. While it is
// not allowed no box takes an input method, whichever box it is and whenever it was made, so nothing
// tapped can bring the keyboard up. A physical keyboard types either way.
class DeviceKeyboard : public QObject {
    Q_OBJECT

public:
    // On the desktop the keyboard is physical and its dead keys are an input method: allowed. In the
    // browser a phone's keyboard would cover the keys: not until asked for.
#ifdef Q_OS_WASM
    static constexpr bool startAllowed = false;
#else
    static constexpr bool startAllowed = true;
#endif

    explicit DeviceKeyboard(QObject* parent = nullptr);

    bool allowed() const { return allowed_; }
    void setAllowed(bool allowed);  // shows or hides the keyboard for the box that has the focus, at once

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool allowed_ = startAllowed;
};
