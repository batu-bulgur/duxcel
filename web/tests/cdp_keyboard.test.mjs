import test from 'node:test';
import assert from 'node:assert/strict';
import {cdpKeyEvent} from './cdp_keyboard.mjs';

test('bare Alt has two balanced phases and no invented macOS event', () => {
    const down = cdpKeyEvent('keyDown', 'Alt', 'AltLeft', 18);
    const up = cdpKeyEvent('keyUp', 'Alt', 'AltLeft', 18, {modifiers: 1});
    assert.equal(down.type, 'rawKeyDown');
    assert.equal(down.modifiers, 1);
    assert.equal(down.location, 1);
    assert.equal(up.type, 'keyUp');
    assert.equal(up.modifiers, 0);
    for (const event of [down, up]) {
        assert.equal(Object.hasOwn(event, 'nativeVirtualKeyCode'), false);
        assert.equal(Object.hasOwn(event, 'isSystemKey'), false);
        assert.equal(Object.hasOwn(event, 'text'), false);
    }
});

test('all modifier keys keep other held modifiers and clear their own on up', () => {
    for (const [key, bit] of [['Alt', 1], ['Control', 2], ['Meta', 4], ['Shift', 8]]) {
        const down = cdpKeyEvent('keyDown', key, key + 'Right', 16, {modifiers: 15 & ~bit});
        const up = cdpKeyEvent('keyUp', key, key + 'Right', 16, {modifiers: 15});
        assert.equal(down.modifiers, 15);
        assert.equal(down.location, 2);
        assert.equal(up.modifiers, 15 & ~bit);
    }
});

test('shortcut letters, digits, arrows and Escape remain non-text key phases', () => {
    for (const [key, code, virtual] of [['h', 'KeyH', 72], ['9', 'Digit9', 57],
        ['ArrowUp', 'ArrowUp', 38], ['Escape', 'Escape', 27]]) {
        for (const type of ['keyDown', 'keyUp']) {
            const event = cdpKeyEvent(type, key, code, virtual);
            assert.equal(event.type, type === 'keyDown' ? 'rawKeyDown' : 'keyUp');
            assert.equal(event.windowsVirtualKeyCode, virtual);
            assert.equal(event.location, 0);
            assert.equal(Object.hasOwn(event, 'nativeVirtualKeyCode'), false);
            assert.equal(Object.hasOwn(event, 'text'), false);
        }
    }
});

test('explicit ordinary and shifted text is inserted only on keyDown', () => {
    const down = cdpKeyEvent('keyDown', 'A', 'KeyA', 65, {text: 'A', modifiers: 8});
    assert.equal(down.type, 'keyDown');
    assert.equal(down.text, 'A');
    assert.equal(down.unmodifiedText, 'A');
    const up = cdpKeyEvent('keyUp', 'A', 'KeyA', 65, {text: 'A', modifiers: 8});
    assert.equal(Object.hasOwn(up, 'text'), false);
    assert.equal(Object.hasOwn(up, 'autoRepeat'), false);
    assert.equal(up.modifiers, 8);
});

test('Control/Alt/Meta shortcuts never insert supplied text', () => {
    for (const modifiers of [1, 2, 4, 3, 15]) {
        const event = cdpKeyEvent('keyDown', 'a', 'KeyA', 65, {text: 'a', modifiers});
        assert.equal(event.type, 'rawKeyDown');
        assert.equal(Object.hasOwn(event, 'text'), false);
        assert.equal(event.modifiers, modifiers);
    }
});

test('keypad location and explicit repeats are represented on down', () => {
    const event = cdpKeyEvent('keyDown', 'Enter', 'NumpadEnter', 13, {autoRepeat: true});
    assert.equal(event.location, 3);
    assert.equal(event.isKeypad, true);
    assert.equal(event.autoRepeat, true);
    const raw = cdpKeyEvent('rawKeyDown', 'a', 'KeyA', 65, {text: 'a'});
    assert.equal(raw.type, 'rawKeyDown');
    assert.equal(Object.hasOwn(raw, 'text'), false);
});

test('invalid protocol phase and fields fail before sending', () => {
    assert.throws(() => cdpKeyEvent('char', 'a', 'KeyA', 65), TypeError);
    assert.throws(() => cdpKeyEvent('keyDown', '', 'KeyA', 65), TypeError);
    assert.throws(() => cdpKeyEvent('keyDown', 'a', 'KeyA', NaN), TypeError);
    assert.throws(() => cdpKeyEvent('keyDown', 'a', 'KeyA', 65, {modifiers: 16}), TypeError);
});
