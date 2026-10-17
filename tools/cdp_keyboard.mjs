// Test-driver protocol construction only; never included in the application.
// Chromium 150 forwards nativeVirtualKeyCode verbatim to its macOS NSEvent
// builder. Windows virtual codes are not macOS physical codes: Alt=18 would
// describe Digit1, and Digit9=57 would describe CapsLock. Leave the optional
// native field absent, as Puppeteer's CDP keyboard does, to target the page
// without inventing an OS event for unhandled browser-menu fallback.
// https://github.com/chromium/chromium/blob/150.0.7871.46/content/browser/devtools/protocol/input_handler.cc#L1147-L1182
// https://github.com/puppeteer/puppeteer/blob/main/packages/puppeteer-core/src/cdp/Input.ts
const modifierBits = {Alt: 1, Control: 2, Meta: 4, Shift: 8};

/** Build one renderer-directed, trusted CDP key phase. Modifiers describe the
 * other held keys; this key's own modifier bit is added on down/removed on up.
 * Callers must still send both phases and retain ownership until keyUp replies.
 * Text is explicit so shortcut tests do not accidentally type into the sheet.
 */
export function cdpKeyEvent(type, key, code, windowsVirtualKeyCode,
    {text = '', modifiers = 0, autoRepeat = false} = {}) {
    if (type !== 'keyDown' && type !== 'rawKeyDown' && type !== 'keyUp')
        throw new TypeError('Expected keyDown, rawKeyDown or keyUp');
    if (typeof key !== 'string' || !key || typeof code !== 'string' || !code)
        throw new TypeError('Expected nonempty key and physical code');
    if (!Number.isInteger(windowsVirtualKeyCode) || windowsVirtualKeyCode < 0 || windowsVirtualKeyCode > 65535)
        throw new TypeError('Invalid Windows virtual key code');
    if (!Number.isInteger(modifiers) || modifiers < 0 || modifiers > 15 || typeof text !== 'string')
        throw new TypeError('Invalid modifiers or text');
    const up = type === 'keyUp', bit = modifierBits[key] ?? 0;
    modifiers = up ? modifiers & ~bit : modifiers | bit;
    // Control/Alt/Meta chords carry key identity but do not insert text. Shift
    // text, when requested, is supplied by the caller's selected key/layout.
    if (up || type === 'rawKeyDown' || modifiers & ~8) text = '';
    const location = /^(Alt|Control|Meta|Shift)Left$/.test(code) ? 1 :
        /^(Alt|Control|Meta|Shift)Right$/.test(code) ? 2 : code.startsWith('Numpad') ? 3 : 0;
    const event = {type: up ? 'keyUp' : text ? 'keyDown' : 'rawKeyDown',
        key, code, windowsVirtualKeyCode, modifiers, location};
    if (!up) {
        event.autoRepeat = Boolean(autoRepeat);
        if (location === 3) event.isKeypad = true;
        if (text) { event.text = text; event.unmodifiedText = text; }
    }
    return event;
}
