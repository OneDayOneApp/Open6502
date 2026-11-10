.segment "CODE"
; Ctrl-C checking is a host terminal concern in 6502EMU.  Returning here
; keeps BASIC from falling through into its STOP handler.
ISCNTC:
        rts
