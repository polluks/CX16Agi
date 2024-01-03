; Check if global definitions are included, if not, include them
.ifndef  IRQ_INC
IRQ_INC = 1

.include "global.s"
.include "globalGraphics.s"
.include "spriteIrqHandler.s"

.macro SEND_IRQ_COMMAND command, vSyncToCheck
sei
lda command
sta sendIrqCommand
lda _vSyncCounter
ldx _vSyncCounter + 1
sta vSyncToCheck
cli
.endmacro

.macro WAIT_FOR_NEXT_IRQ vSyncToCheck
.local @waitForIrq
.local @end
php

sei
lda vSyncToCheck
ldx vSyncToCheck + 1

@waitForIrq:
cli
wai

sei
cmp _vSyncCounter
bne @end
cpx _vSyncCounter + 1
beq @waitForIrq

@end:
plp
.endmacro

;Handlers
.segment "BANKRAM03"
handleDisplayText:
SET_VERA_ADDRESS_ABSOLUTE _displayTextAddressToCopyTo, #$0, #$2

lda _currentTextBuffer
sta ZP_TMP
lda _currentTextBuffer + 1
sta ZP_TMP + 1

@outerLoop:
ldy #$0
@innerLoop:
lda (ZP_TMP), y
beq @end ; If we get a zero that means a terminator stop

cmp #NEW_LINE
bne @storeToVera
jsr b3DisplayTextNewLine
bra @incrementInnerLoop ; If it is a new line we shouldn't store it to the VERA

@storeToVera:
sta VERA_data0

@incrementInnerLoop:
iny

@innerLoopCondition:
bne @innerLoop

@incrementOuterLoop:
clc ;Adding 256
lda ZP_TMP + 1; Ignore the lower bit always static. 
adc #$1
sta ZP_TMP + 1
bra @innerLoop
@end:
stz ZP_TMP
stz ZP_TMP + 1
rts

b3DisplayTextNewLine: ;Goes to the start of the next line of the VERA but maintains the column number of the first character of the first line
@newLine:
clc
lda #TILE_LAYER_WIDTH * 2
adc _displayTextAddressToCopyTo
sta _displayTextAddressToCopyTo
lda #$0
adc _displayTextAddressToCopyTo + 1
sta _displayTextAddressToCopyTo + 1

SET_VERA_ADDRESS_ABSOLUTE _displayTextAddressToCopyTo, #$0, #$2
rts

_displayTextAddressToCopyTo: .word $0
_displayTextAddressToCopyToHigh: .word $0
_currentTextBuffer: .word $0
.segment "BANKRAM06"
_b6InitIrq:
 ; backup default RAM IRQ vector
   lda IRQVec
   sta default_irq_vector
   lda IRQVec+1
   sta default_irq_vector+1

   ; overwrite RAM IRQ vector with custom handler address
   sei ; disable IRQ while vector is changing
   lda #<custom_irq_handler
   sta IRQVec
   lda #>custom_irq_handler
   sta IRQVec+1
   lda #VSYNC_BIT ; make VERA only generate VSYNC IRQs
   sta VERA_ien
   cli ; enable IRQ now that vector is properly set
rts

_b6SetAndWaitForIrqStateAsm:
sta @state
SEND_IRQ_COMMAND @state, @vSyncToCheck
WAIT_FOR_NEXT_IRQ @vSyncToCheck
rts
@state: .byte $0
@vSyncToCheck: .word $0

.segment "CODE"
IRQ_CMD_DONTCHANGE = 0
IRQ_CMD_BLACKSCREEN = 1
IRQ_CMD_TEXT_ONLY = 2
IRQ_CMD_NORMAL = 3
IRQ_CMD_DISPLAY_TEXT = 4

LAYER_0_1_SPRITES_ENABLE = $71
LAYER_0_1_SPRITES_DISABLE = $1
LAYER_0_SPRITES_DISABLE_1_ENABLE = $21

;0 Don't Change
;1 Blank Screen
;2 Text Only
;3 Return To Normal Display (Both layers enabled)
;4 Display Text (A command to display text, the text to display is stored in the buffer pointed to by _currentTextBuffer)

sendIrqCommand: .byte $0

;As above except it will never change to 0
currentIrqState: .byte $0

_vSyncCounter: .word $0
debugVSyncCounter: .word $0

custom_irq_handler:
lda RAM_BANK
sta @previousRamBank 

; continue to default IRQ handler
lda VERA_isr
and #VSYNC_BIT
beq @defaultIqr

@handleSpriteUpdates:
lda #SPRITE_UPDATES_BANK
sta RAM_BANK
jsr bEHandleSpriteUpdates

lda sendIrqCommand
tax

lda @jmpTableBank, x
sta RAM_BANK
txa
clc
asl
tax
jmp (@jmpTableIrq,x)


@displayText:
jsr handleDisplayText
bra @resetSetIrqState

@blankScreen:
lda #LAYER_0_1_SPRITES_DISABLE
sta VERA_dc_video
lda #IRQ_CMD_BLACKSCREEN
sta currentIrqState
bra @resetSetIrqState

@normal:
lda #LAYER_0_1_SPRITES_ENABLE
sta VERA_dc_video
lda #IRQ_CMD_NORMAL
sta currentIrqState
bra @resetSetIrqState

@textOnly:
lda #LAYER_0_SPRITES_DISABLE_1_ENABLE
sta VERA_dc_video
bra @resetSetIrqState

@resetSetIrqState:
lda #IRQ_CMD_DONTCHANGE
sta sendIrqCommand

@vSyncCounter:
inc _vSyncCounter
bne @defaultIqr
inc _vSyncCounter + 1

@defaultIqr:
lda @previousRamBank
sta RAM_BANK
jmp (default_irq_vector)
; RTI will happen after jump

@jmpTableIrq: ;In order of IRQ_CMDS
.addr @vSyncCounter
.addr @blankScreen
.addr @textOnly
.addr @normal
.addr @displayText

@jmpTableBank: .byte $0, $0, $0, $0, TEXT_BANK ;In order of IRQ_CMDS
@previousRamBank: .byte $0

.endif ; IRQ_INC

