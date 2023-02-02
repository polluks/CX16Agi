.ifndef  COMMAND_LOOP_INC
COMMAND_LOOP_INC = 1
LOGIC_ENTRY_PARAMETERS_OFFSET =  0

.include "global.s"
.include "logicCommands.s"
.include "codeWindow.s"

ZP_PTR_LF = $02
ZP_PTR_LE = $04

.segment "CODE"
startPos: .word $0
endPos:  .word $0
stillExecuting: .byte $1
lastCodeWasNonWindow: .byte FALSE
jumpOffset: .byte $0

.macro SET_BANK_TO_CODE_BANK
lda codeBank
sta RAM_BANK
.endmacro

.macro IF_HANDLER
        jmp startIfHandler
        stillProcessing: .byte $1
        @ch: .word $0

        startIfHandler:

        ifHandlerLoop:
        lda stillProcessing
        beq endIfHandlerLoop

        lda (ZP_PTR_CODE)

        cmp #$FF
        beq closingIfBracket

        cmp #$fd
        beq notMode

        cmp #$FC
        beq orMode

        @default:
            lda (ZP_PTR_CODE)
            asl
            sta jumpOffset

            INC_CODE
            
            stp
            LDA #LOGIC_COMMANDS_BANK
            sta RAM_BANK
            ldx jumpOffset
            jmp (jmpTableIf,x)
            
            returnFromOpCodeTrue:
            SET_BANK_TO_CODE_BANK
            jmp ifHandlerLoop
            returnFromOpCodeFalse:
            SET_BANK_TO_CODE_BANK
            lda #FALSE
            sta stillProcessing
            ;if (!orMode)
            jmp ifHandlerLoop
        
        closingIfBracket:
            ;toImplement
            ;jmp ifHandlerLoop

        notMode:
            ;toImplement
            ;jmp ifHandlerLoop

        orMode:
            ;toImplement

        ;jmp ifHandlerLoop
endIfHandlerLoop:
.endmacro

_commandLoop:
         jmp start
         entryPoint: .word $0
         codeSize: .word $0
         codeAtTimeOfLastBankSwitch: .byte $0

         start:
         sta   ZP_PTR_LF
         stx   ZP_PTR_LF  + 1

         lda   GOLDEN_RAM + PARAMETERS_WORK_AREA_GOLDEN_OFFSET + LOGIC_ENTRY_PARAMETERS_OFFSET
         ldx   GOLDEN_RAM + PARAMETERS_WORK_AREA_GOLDEN_OFFSET + LOGIC_ENTRY_PARAMETERS_OFFSET + 1
         sta   ZP_PTR_LE
         stx   ZP_PTR_LE  + 1
        
         GET_STRUCT_16 LOGIC_FILE_LOGIC_CODE_OFFSET, ZP_PTR_LF, startPos
         GET_STRUCT_16 LOGIC_FILE_LOGIC_CODE_SIZE_OFFSET, ZP_PTR_LF, codeSize
         GET_STRUCT_8 LOGIC_FILE_LOGIC_BANK_OFFSET, ZP_PTR_LF, codeBank
         GET_STRUCT_16 LOGIC_ENTRY_POINT_OFFSET, ZP_PTR_LE, entryPoint
         
         ADD_WORD_16 startPos,entryPoint,ZP_PTR_CODE
         ADD_WORD_16 startPos,codeSize,endPos
         
         jsr refreshCodeWindow
         mainLoop:

         GREATER_THAN_OR_EQ_16 ZP_PTR_CODE, endPos, endMainLoop
         lda stillExecuting
         cmp #TRUE
         beq @loopConditionSuccess
         jmp endMainLoop
         @loopConditionSuccess:
                 
        ; /* Emergency exit */
		; if (key[KEY_F12]) {
		; 	////lprintf("info: Exiting MEKA due to F12, logic: %d, posn: %d",
		; 		//logNum, currentLogic.currentPoint);
		; 	exit(0);
		; }
        lda (ZP_PTR_CODE)
        sta codeAtTimeOfLastBankSwitch

		; instructionCodeBank = getBankBasedOnCode(codeAtTimeOfLastBankSwitch);
        ; if (*code < 0xfe)
		; {
        ; }
        ;else {
                ;switch (codeAtTimeOfLastBankSwitch) {
			    ;case 0xfe: 
                ;case 0xff:
                INC_CODE
                LDA #TRUE

                IF_HANDLER
                ;}
            ;}


         SUB_WORD_16_IND ZP_PTR_CODE, startPos, LOGIC_ENTRY_CURRENT_POINT_OFFSET, ZP_PTR_LE
         
         stp
         jmp mainLoop
         endMainLoop:
         rts
.endif