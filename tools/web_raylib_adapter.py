"""Narrow, fail-closed web repairs applied only to our pinned raylib copy.

Upstream notices remain intact; the checked-in adapter is plainly identified.
Do not patch the downloaded SDK or Homebrew/native raylib.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
INPUTS=[Path(__file__),ROOT/'web/raylib_web_surface.inc']

def replace_body(text,signature,body):
    marker=signature+'\n{'
    if text.count(marker)!=1:
        raise ValueError('Pinned raylib adapter mismatch: '+signature)
    start=text.index(marker)+len(signature)+1
    end=text.index('\n}',start)+2
    return text[:start]+'{\n'+body+'\n}'+text[end:]

def apply(source):
    path=source/'platforms/rcore_web.c'; text=path.read_text()
    marker='static PlatformData platform = { 0 };   // Platform specific data'
    if text.count(marker)!=1:raise ValueError('Pinned raylib platform state changed')
    text=text.replace(marker,marker+'\n\n// Duxcel: synchronized CSS/DPR surface and precise browser wheel adapter.\n#include "duxcel_web_surface.inc"')
    marker='    glfwDefaultWindowHints(); // Set default windows hints'
    if text.count(marker)!=1:raise ValueError('Pinned GLFW initialization changed')
    text=text.replace(marker,'    dx_web_install_surface();\n'+marker)
    text=replace_body(text,'static void WindowSizeCallback(GLFWwindow *window, int width, int height)',
        '    (void)window; (void)width; (void)height;\n    DxWebApplySurface(true,true);')
    text=replace_body(text,'static void WindowContentScaleCallback(GLFWwindow *window, float scalex, float scaley)',
        '    (void)window; (void)scalex; (void)scaley;\n    DxWebApplySurface(true,true);')
    text=replace_body(text,'static EM_BOOL EmscriptenResizeCallback(int eventType, const EmscriptenUiEvent *event, void *userData)',
        '    (void)eventType; (void)event; (void)userData;\n    DxWebApplySurface(true,true);\n    return 0;')
    text=replace_body(text,'Vector2 GetWindowScaleDPI(void)',
        '    return (Vector2){CORE.Window.screen.width ? (float)CORE.Window.render.width/CORE.Window.screen.width : 1,\n                     CORE.Window.screen.height ? (float)CORE.Window.render.height/CORE.Window.screen.height : 1};')
    marker='        CORE.Window.currentFbo.height = fbHeight;'
    if text.count(marker)!=1:raise ValueError('Pinned raylib initial framebuffer changed')
    text=text.replace(marker,marker+'\n\n        DxWebApplySurface(true,false); // before rlgl initializes its physical framebuffer')
    path.write_text(text)
    (source/'platforms/duxcel_web_surface.inc').write_text(INPUTS[1].read_text())
    # GetRenderWidth/Height are already physical. Upstream TakeScreenshot scales
    # them a second time; keep native behavior outside this web-only copy.
    path=source/'rcore.c'; text=path.read_text()
    marker='    if (FLAG_IS_SET(CORE.Window.flags, FLAG_WINDOW_HIGHDPI)) scale = GetWindowScaleDPI();'
    if text.count(marker)!=1:raise ValueError('Pinned raylib screenshot path changed')
    text=text.replace(marker,'    // Duxcel web: render dimensions already include the bounded backing scale.')
    path.write_text(text)
