# TODO / Handoff

## CLI / debug
```
moas --help
moas --debug=wait,input,window
moas --skip-intro --debug=script
MOAS_DEBUG=all moas
moas --proc=0 --addr=0
```

Flags: script, input, wait, window, load, all
--skip-intro skips Execute(3,0)

## Still stuck on title
Run with: `nix run . -- --debug=wait,input,window,script`
and share stderr lines around the hang.
