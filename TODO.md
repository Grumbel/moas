# TODO / Handoff

## Input status
### Mouse — OK
- Position via SDL_MOUSEMOTION + RenderWindowToLogical → 320×200
- Left/Right/Middle buttons
- Edge latch (Last*) at start of MouseCheck
- GetClickOrEnter / Wait call MouseCheck

### Keyboard — was broken, now wired
Previously: kbhit/getch always 0; KeyScanCode never set (DOS IRQ only).

Now:
- SDL KEYDOWN/UP → XT scancodes into KeyScanCode (make/break +128)
- LookKeys arrows / Space / Enter / Tab / F5 / F7 work via KeyScanCode
- kbhit/getch queue for ASCII + extended (0, nn) for arrows/F-keys

### Quit
- Q, Ctrl+C, Alt+F4, window close

## Still open
- Heap corruption root cause
- Graphics dirty-rect
- Sound
