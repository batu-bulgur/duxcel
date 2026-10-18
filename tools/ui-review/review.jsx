import React, { useState } from 'react';
import { createRoot } from 'react-dom/client';
import { Agentation } from 'agentation';

const trail = [];
let lastAppState = {};
const outsideReview = element => element instanceof Element && !element.closest('agentation-toolbar,[data-agentation-portal],#duxcel-review-tools');
const text = selector => document.querySelector(selector)?.textContent?.trim().slice(0, 240) || null;
function snapshot() {
  const app = document.querySelector('.duxcel-app');
  if (!app) return {};
  return {
    viewport: { width: innerWidth, height: innerHeight, devicePixelRatio },
    ribbon: app.dataset.ribbonMode, authoring: app.dataset.authoring, uiScale: app.dataset.uiScale,
    focusMode: app.classList.contains('focus-mode'), sheet: text('.sheet-entry.selected'),
    tab: text('[role="tab"][aria-selected="true"]'), activeCell: text('.cell-address'),
    gridZoom: document.querySelector('button[title="Reset canvas zoom"]')?.textContent?.trim(),
    selection: [...app.querySelectorAll('[role="gridcell"][aria-selected="true"]')].slice(0, 12).map(el => el.id),
    openDialogs: [...app.querySelectorAll('[role="dialog"],[role="menu"]')].map(el => el.getAttribute('aria-label') || el.textContent?.slice(0, 160)),
    panes: [...app.querySelectorAll('aside')].map(el => ({ label: el.getAttribute('aria-label'), width: Math.round(el.getBoundingClientRect().width) })),
    shelves: [...app.querySelectorAll('.shelf')].map(el => el.textContent?.trim().slice(0, 200)),
    scroll: [...app.querySelectorAll('.data-grid')].map(el => ({ left: Math.round(el.scrollLeft), top: Math.round(el.scrollTop) })),
  };
}
function track(event) {
  const target = event.target;
  if (!outsideReview(target) || !target.closest('.duxcel-app')) return;
  const control = target.closest('button,input,select,[role="gridcell"],[role="separator"],[data-drop-zone]') || target;
  const entry = { at: new Date().toISOString(), type: event.type, target: control.getAttribute('aria-label') || control.getAttribute('title') || control.id || control.className?.toString().slice(0, 80) };
  if (event.type.startsWith('pointer') || event.type === 'contextmenu') {
    entry.position = { x: Math.round(event.clientX), y: Math.round(event.clientY) };
    entry.modifiers = { shift: event.shiftKey, alt: event.altKey, ctrl: event.ctrlKey, meta: event.metaKey };
  }
  if (event.type === 'keydown') {
    // Record navigation/commands, never typed text or clipboard payloads.
    if (!['Escape', 'Enter', 'Tab', 'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'F6'].includes(event.key) && !event.metaKey && !event.ctrlKey && !event.altKey) return;
    entry.key = `${event.metaKey ? 'Cmd+' : ''}${event.ctrlKey ? 'Ctrl+' : ''}${event.shiftKey ? 'Shift+' : ''}${event.altKey ? 'Alt+' : ''}${event.key}`;
  }
  trail.push(entry); if (trail.length > 24) trail.shift();
  // Sample after the app has applied its own event. No polling or pointermove logging.
  requestAnimationFrame(() => { lastAppState = snapshot(); });
}
for (const type of ['pointerdown', 'pointerup', 'click', 'contextmenu', 'keydown', 'change']) document.addEventListener(type, track, true);
window.addEventListener('resize', () => { lastAppState = snapshot(); });
function togglePinMode() {
  const root = document.querySelector('agentation-toolbar')?.shadowRoot;
  const button = root && [...root.querySelectorAll('button')].find(el => ['Start feedback mode', 'Exit'].includes(el.getAttribute('aria-label')) && el.getClientRects().length);
  button?.click();
}
// Agentation's default Cmd+Shift+F conflicts with Duxcel Focus mode. This review-only
// shortcut changes modes without a pointerdown outside the menu being discussed.
window.addEventListener('keydown', event => {
  if ((event.metaKey || event.ctrlKey) && event.shiftKey && event.code === 'Period') {
    event.preventDefault(); event.stopImmediatePropagation(); lastAppState = snapshot(); togglePinMode();
  }
}, true);

function ReviewTools() {
  const [message, setMessage] = useState('Local review');
  async function capture(annotation) {
    try {
      const response = await fetch('/__review/context', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ annotation, context: { state: Object.keys(lastAppState).length ? lastAppState : snapshot(), atSubmission: snapshot(), recentActions: [...trail] } }) });
      if (!response.ok) throw new Error('Save failed');
      const issue = await response.json(); setMessage(`${issue.id} saved`);
    } catch { setMessage('State save failed — copy note as backup'); }
  }
  return <>
    <div id="duxcel-review-tools" data-feedback-toolbar="" onPointerDown={event => { event.preventDefault(); event.stopPropagation(); }} style={{ position: 'fixed', right: 12, bottom: 78, zIndex: 2147483646, font: '11px "IBM Plex Sans", sans-serif', display: 'flex', alignItems: 'center', gap: 8, background: '#f7f9fb', color: '#4d6274', padding: '4px 7px', border: '1px solid #bccbd5', borderRadius: 3, boxShadow: '0 1px 4px #0001' }}>
      <button aria-label="Toggle pinpoint feedback" title="Pin an open menu · Cmd/Ctrl+Shift+." onClick={() => { lastAppState = snapshot(); togglePinMode(); }} style={{font:'inherit',padding:0,border:0,background:'none',color:'#285f86',cursor:'pointer'}}>Pin feedback</button><span role="status">{message}</span><a href="/__review/" target="_blank" rel="noreferrer" style={{ color: '#285f86' }}>Queue</a>
    </div>
    <Agentation appName="Duxcel · UI reference review" endpoint={`${location.origin}/__agentation`} enableKeyboardShortcuts={false} onAnnotationAdd={capture} />
  </>;
}
const mount = document.createElement('div');
mount.id = 'duxcel-review-mount';
document.body.append(mount);
createRoot(mount).render(<ReviewTools />);
