.include "global.s"
.include "codeWindow.s"
.include "debug.s"
.include "irqAsm.s"
.include "graphicsAsm.s"

; Import required C variables
.import _logicEntryAddressesLow
.import _logicEntryAddressesHigh

.segment "BANKRAM06"
_b6InitInterpreter:
    stz ZP_TMP
    stz ZP_TMP + 1
    stz ZP_PTR_LF 
    stz ZP_PTR_LF + 1
    stz ZP_PTR_LE  
    stz ZP_PTR_LE + 1
    stz ZP_PTR_PLF_HIGH
    stz ZP_PTR_PLF_HIGH + 1
    stz ZP_PTR_PLF_LOW 
    stz ZP_PTR_PLF_LOW + 1
    stz ZP_PTR_B1
    stz ZP_PTR_B1 + 1
    stz ZP_PTR_B2 
    stz ZP_PTR_B2 + 1
    stz ZP_PTR_DISP
    stz ZP_PTR_DISP + 1
   
    lda _logicEntryAddressesLow
    sta ZP_PTR_PLF_LOW
    lda _logicEntryAddressesLow + 1
    sta ZP_PTR_PLF_LOW + 1

    lda _logicEntryAddressesHigh
    sta ZP_PTR_PLF_HIGH
    lda _logicEntryAddressesHigh + 1
    sta ZP_PTR_PLF_HIGH + 1
rts
.segment "CODE" ;Not sure why this is needed TODO:Fix