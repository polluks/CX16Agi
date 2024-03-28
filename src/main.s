    .importzp sreg
    .include "init.s"
    .include "commandLoop.s"
    .include "logicCommands.s"
    .include "codeWindow.s"
    .include "global.s"
    .include "graphicsAsm.s"
    .include "pictureAsm.s"
    .include "helpersAsm.s"
    .include "viewAsm.s"
    .include "spriteMemoryManagerAsm.s"
    .include "pictureAsm.s"
    .export _executeLogic
    .export _b6InitInterpreter
    .export _loadAndIncWinCode
    .export _incCodeBy
    .export codeWindow
    .export _codeBank
    .export _logDebugVal1
    .export _logDebugVal2
    .export _logDebugVal3
    .export _logDebugVal4
    .export _logDebugVal5
    .export _logDebugVal6
    .export _b6ClearBackground
    .export _toDraw
    .export _drawWhere
    .export _bFloodQstore
    .export _bFloodPicGetPixel
    .export _floatDivision
    .export _bFloodOkToFill
    .export _bFloodAgiFill
    .export _b6SetAndWaitForIrqStateAsm
    .export _displayTextAddressToCopyTo
    .export _vSyncCounter
    .export _lastBoxLines
    .export _lastBoxStartLine
    .export _textBuffer1
    .export _textBuffer2
    .export _interpolationBuffer
    .export _currentTextBuffer
    .export _trampoline
    .export _b4DrawStraightLineAlongX
    .export _b4DrawStraightLineAlongY
    .export _viewHeaderBuffer
    .export _loopHeaderBuffer
    .export _b9CelToVera
    .export _b6InitGraphics
    .export _b6InitIrq
    .export _b6TellMeTheAddressPlease
    .export _bESpritesUpdatedBuffer
    .export _bESpriteAddressTableMiddle
    .export _bESpriteAllocTable
    .export _bEAllocateSpriteMemory32
    .export _bEAllocateSpriteMemory64
    .export _bEAllocateSpriteMemoryBulk
    .export _bEBulkAllocatedAddresses
    .export _bECellToVeraBulk
    .export _bEToBlitCelArray
    .export _bESpritesUpdatedBufferPointer
    .export _bEClearSpriteAttributes
    .export _bESplitCel
    .export _b11FillClean