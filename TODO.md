# TODO / Handoff

## From user debug log
- Clicks work; GetClickOrEnter dismisses text (op=84)
- After AddPerson x3 and op=50 (Found return), script ends
- Further clicks obj=-1 — persons not hittable

## Root cause
AddPerson read `sizeof(struct PERSON)` on 64-bit into GAME.1 records of **236 bytes**
(stride 240). Same class of bug as MAPFILE. Fixed via PersonDisk.

## Debug CLI remains
`--debug=wait,input,window,script`
