// Browser platform policy only. GLFW still receives each event and C owns its
// navigation behavior; the browser must not also scroll or redispatch it.
const navigationKeys=new Set(['ArrowUp','ArrowDown','ArrowLeft','ArrowRight','Home','End','PageUp','PageDown']);
const installations=new WeakMap();

/** Unmodified/Shift navigation and the ribbon's bare Alt key are claimed.
 * Modified printable input still reaches composition/browser handling.
 * Repeated installation shares
 * one listener and disposer. Disposing either
 * handle removes that installation; disposal is idempotent, and reinstalling
 * afterward creates a new independent handle. */
export function installCanvasInput(canvas){
    if(!canvas?.addEventListener||!canvas?.removeEventListener)throw new TypeError('A canvas event target is required.');
    const installed=installations.get(canvas);if(installed)return installed;
    const keydown=event=>{
        if(event.target!==canvas||canvas.ownerDocument?.activeElement!==canvas||event.isComposing||event.keyCode===229)return;
        // Alt opens C keytips. Letting the browser also activate its menu can
        // redispatch the following ribbon letter through Cocoa menu routing.
        if(event.key==='Alt'&&!event.ctrlKey&&!event.metaKey){if(!event.defaultPrevented)event.preventDefault();return;}
        if(event.ctrlKey||event.altKey||event.metaKey)return;
        if(navigationKeys.has(event.key)&&!event.defaultPrevented)event.preventDefault();
    };
    // GLFW's window-capture listener queues the key before this bubble listener.
    canvas.addEventListener('keydown',keydown,{capture:false,passive:false});
    const dispose=()=>{
        if(installations.get(canvas)!==dispose)return;
        canvas.removeEventListener('keydown',keydown);installations.delete(canvas);
    };
    installations.set(canvas,dispose);return dispose;
}
