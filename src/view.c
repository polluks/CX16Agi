/***************************************************************************
** VIEW.C
**
** These functions load VIEWs into memory. VIEWs are not automatically
** stored into the VIEW table when they are loaded. The VIEW table only
** holds those VIEWs that have been allocated a slot via the set.view()
** AGI function. The views that are stored in the VIEW table appear to
** be those that are going to be animated. Compare this with add-to-pics
** which are placed on the screen but are never dealt with again, or
** inventory item VIEWs that are shown but not controlled (animated).
**
** (c) 1997 Lance Ewing - Original code (3 July 97)
**                      - Changes       (25 Aug 97)
**                                      (15 Jan 98)
**                                      (22 Jul 98)
***************************************************************************/

#include <string.h>
#include <stdlib.h>

#include "general.h"
#include "agifiles.h"
#include "view.h"

//#define VERBOSE_SWITCH_METADATA
//#define VERBOSE_GET_PALETTE
//#define VERBOSE_MOVE
//#define VERBOSE_SET_VIEW;
//#define VERBOSE_SET_LOOPS
//#define VERBOSE_SET_CEL
//#define VERBOSE_LOAD_VIEWS;
//#define VERBOSE_UPDATE_OBJECTS
//#define VERBOSE_DEBUG_SET_METADATA
//#define VERBOSE_DEBUG_BLIT

//#define VERBOSE_ALLOC_WATCH
//#define VERBOSE_ADD_TO_PIC;
//#define VERBOSE_DEBUG_NO_BLIT_CACHE TODO: Weird print statement corruption fix

#define MIN_SPRITE_PRIORITY 4
#define MAX_SPRITE_PRIORITY 15
#define NO_PRIORITIES (MAX_SPRITE_PRIORITY - MIN_SPRITE_PRIORITY)

#define BYTES_PER_SPRITE_UPDATE 7
#define SPRITE_UPDATED_BUFFER_SIZE  VIEW_TABLE_SIZE * BYTES_PER_SPRITE_UPDATE * 2
extern byte bESpritesUpdatedBuffer[SPRITE_UPDATED_BUFFER_SIZE];
extern byte* bESpritesUpdatedBufferPointer;

View* loadedViews = (View*)&BANK_RAM[LOADED_VIEW_START];
BITMAP* spriteScreen;

extern byte* var;
extern boolean* flag;
extern char string[12][40];
extern byte horizon;
extern int dirnOfEgo;

byte maxViewTable = 0;

#define MAX_INACTIVE_METADATA 10

//The data is the top two fields are all stored at the same bank alloced address on the bank in the bank field
typedef struct {
	VeraSpriteAddress** loopsVeraAddressesPointers;
	VeraSpriteAddress* veraAddresses;
	byte viewTableMetadataBank;
	byte viewNum;
	void* inactive; //Sometimes a single viewtab can have views swapped rapidly, we keep the previous ones here so we can easily switch between them
	byte inactiveBank;

} ViewTableMetadata;

extern byte bEBulkAllocatedAddresses[VIEW_TABLE_SIZE * sizeof(VeraSpriteAddress) * ALLOCATOR_BLOCK_SIZE_64];

void getViewTab(ViewTable* returnedViewTab, byte viewTabNumber)
{
	byte previousRamBank = RAM_BANK;

	RAM_BANK = VIEWTAB_BANK;

	*returnedViewTab = viewtab[viewTabNumber];

	RAM_BANK = previousRamBank;
}

void setViewTab(ViewTable* localViewtab, byte viewTabNumber)
{
	byte previousRamBank = RAM_BANK;
	RAM_BANK = VIEWTAB_BANK;

	viewtab[viewTabNumber] = *localViewtab;

	RAM_BANK = previousRamBank;
}

void getLoadedView(View* returnedLoadedView, byte loadedViewNumber)
{
	byte previousRamBank = RAM_BANK;

	RAM_BANK = LOADED_VIEW_BANK;

	*returnedLoadedView = loadedViews[loadedViewNumber];

	RAM_BANK = previousRamBank;
}

void setLoadedView(View* loadedView, byte loadedViewNumber)
{
	byte previousRamBank = RAM_BANK;

	RAM_BANK = LOADED_VIEW_BANK;

	loadedViews[loadedViewNumber] = *loadedView;

	RAM_BANK = previousRamBank;
}

void getLoadedLoop(View* loadedView, Loop* returnedLocalLoop, byte localLoopNumber)
{
	byte previousRamBank = RAM_BANK;

	RAM_BANK = loadedView->loopsBank;

	*returnedLocalLoop = loadedView->loops[localLoopNumber];

	RAM_BANK = previousRamBank;
}

void setLoadedLoop(View* loadedView, Loop* localLoop, byte localLoopNumber)
{
	byte previousRamBank = RAM_BANK;

	RAM_BANK = loadedView->loopsBank;

	loadedView->loops[localLoopNumber] = *localLoop;

	RAM_BANK = previousRamBank;
}

void getLoadedCel(Loop* loadedLoop, Cel* localCell, byte localCellNumber)
{
	byte previousRamBank = RAM_BANK;

	RAM_BANK = loadedLoop->celsBank;

	*localCell = loadedLoop->cels[localCellNumber];

	RAM_BANK = previousRamBank;
}

void setLoadedCel(Loop* loadedLoop, Cel* localCell, byte localCellNumber)
{
	byte previousRamBank = RAM_BANK;

	RAM_BANK = loadedLoop->celsBank;


	loadedLoop->cels[localCellNumber] = *localCell;

	RAM_BANK = previousRamBank;
}

#pragma bss-name (push, "BANKRAM09")
ViewTable viewtab[VIEW_TABLE_SIZE];
#pragma bss-name (pop)
#pragma bss-name (push, "BANKRAM0E")
ViewTableMetadata viewTableMetadata[SPRITE_SLOTS];

#define VIEWNO_TO_METADATA_NO_SET SPRITE_SLOTS + 1
byte viewTabNoToMetaData[MAXVIEW];


long nextSpriteAttribute;
byte nextSpriteSlot;
byte nextViewMetadataSlot;
#pragma bss-name (pop)

//Temp From Allogro
#define FONT_SIZE    224  
typedef struct FONT_8x8             /* a simple 8x8 font */
{
	unsigned char dat[FONT_SIZE][8];
} FONT_8x8;


typedef struct FONT_8x16            /* a simple 8x16 font */
{
	unsigned char dat[FONT_SIZE][16];
} FONT_8x16;


typedef struct FONT_PROP            /* a proportional font */
{
	BITMAP* dat[FONT_SIZE];
} FONT_PROP;


typedef struct FONT                 /* can be either */
{
	int height;
	union {
		FONT_8x8* dat_8x8;
		FONT_8x16* dat_8x16;
		FONT_PROP* dat_prop;
	} dat;
} FONT;

void textout(BITMAP* bmp, FONT* f, char* str, int x, int y, int color)
{

}

FONT* font;

//

extern void b9CelToVera(Cel* localCel, long veraAddress, byte bCol, byte drawingAreaWidth);

#pragma code-name (push, "BANKRAM0E")
#pragma wrapped-call (push, trampoline, SPRITE_METADATA_BANK)

void bETerminateSpriteBuffer()
{
	*bESpritesUpdatedBufferPointer++ = 0;
	*bESpritesUpdatedBufferPointer++ = 0;
}

void bEResetInactiveViewTableMetadata(ViewTableMetadata* localViewTableMetadata)
{
	boolean stop;
	byte i;
	ViewTableMetadata* inActivePtr = (ViewTableMetadata*)localViewTableMetadata->inactive;
	ViewTableMetadata localInactiveMetadata;

	stop = FALSE;
	for (i = 0; i < MAX_INACTIVE_METADATA && !stop; i++)
	{
		memCpyBanked((byte*)&localInactiveMetadata, (byte*)&inActivePtr[i], localViewTableMetadata->inactiveBank, sizeof(ViewTableMetadata));

		if (localInactiveMetadata.loopsVeraAddressesPointers)
		{
			b10BankedDealloc((byte*)localInactiveMetadata.loopsVeraAddressesPointers, localInactiveMetadata.inactiveBank);
		}
		else
		{
			stop = TRUE;
		}
	}
}

void bEResetViewTableMetadata()
{
	byte i, j, stop;
	ViewTableMetadata localInactiveMetadata;
	ViewTableMetadata* inActivePtr;

	for (i = 0; i < SPRITE_SLOTS; i++)
	{
		if (viewTableMetadata[i].loopsVeraAddressesPointers != NULL)
		{
			b10BankedDealloc((byte*)viewTableMetadata[i].loopsVeraAddressesPointers, viewTableMetadata[i].viewTableMetadataBank);
		}

		viewTableMetadata[i].loopsVeraAddressesPointers = NULL;
		viewTableMetadata[i].viewTableMetadataBank = NULL;
		viewTableMetadata[i].viewNum = 0;

		if (viewTableMetadata[i].inactive != NULL)
		{
			inActivePtr = (ViewTableMetadata*)viewTableMetadata[i].inactive;

			bEResetInactiveViewTableMetadata(&viewTableMetadata[i]);

			b10BankedDealloc((byte*)viewTableMetadata[i].inactive, viewTableMetadata[i].inactiveBank);
		}

		viewTableMetadata[i].inactive = NULL;
		viewTableMetadata[i].inactiveBank = NULL;
	}

	memset(&viewTabNoToMetaData[0], VIEWNO_TO_METADATA_NO_SET, MAXVIEW);
}
void bEResetSpritePointers()
{
	nextSpriteAttribute = SPRITE_ATTRIBUTES_START;
	nextSpriteSlot = 0;
	nextViewMetadataSlot = 0;
}
void bEResetSpritesUpdatedBuffer()
{
	memsetBanked(bESpritesUpdatedBuffer, 0, VIEW_TABLE_SIZE, SPRITE_UPDATED_BANK);
	bESpritesUpdatedBufferPointer = bESpritesUpdatedBuffer;
}

#pragma wrapped-call (pop)
#define MAX_SLOT_1_SIZED_SPRITE 64
#define MAX_SPRITE_SIZE 128
void bESetViewMetadata(View* localView, ViewTable* viewTable, byte viewNum, byte viewTabNo, byte viewMetadataSlot)
{
	byte i;
	byte maxVeraAddresses;
	ViewTableMetadata metadata;
	int loopVeraAddressesPointersSize;
	int veraAddressesSize;
	int totalAllocationSize;
	VeraSpriteAddress** addressBuffer = (VeraSpriteAddress**)GOLDEN_RAM_PARAMS_AREA;
	VeraSpriteAddress* veraAddressCounter;

#ifdef VERBOSE_DEBUG_SET_METADATA
	printf("The viewNum is %d\n", viewNum);
#endif

	if (nextSpriteSlot > SPRITE_SLOTS)
	{
		printf("Fail: Sprite slot too big");
	}

#ifdef VERBOSE_DEBUG_SET_METADATA
	printf("The max number of vera slots is %d \n", localView->veraSlots);
#endif

	maxVeraAddresses = localView->maxVeraSlots * localView->maxCels;

#ifdef VERBOSE_DEBUG_SET_METADATA
	printf("Max vera addresses is %d\n", maxVeraAddresses);
#endif

	if (viewMetadataSlot == VIEWNO_TO_METADATA_NO_SET)
	{
		viewMetadataSlot = nextViewMetadataSlot++;
	}

	metadata = viewTableMetadata[viewTabNo];

	viewTabNoToMetaData[viewTabNo] = viewMetadataSlot;

	if (metadata.loopsVeraAddressesPointers != NULL)
	{
		b10BankedDealloc((byte*)metadata.loopsVeraAddressesPointers, metadata.viewTableMetadataBank);

		//TODO: Deallocate vera as well
	}

	loopVeraAddressesPointersSize = localView->numberOfLoops * sizeof(VeraSpriteAddress*);
	veraAddressesSize = maxVeraAddresses * sizeof(VeraSpriteAddress) * localView->numberOfLoops;
	totalAllocationSize = loopVeraAddressesPointersSize + veraAddressesSize;

#ifdef VERBOSE_DEBUG_SET_METADATA
	printf("There are %d loops and %d maxCels\n", localView->numberOfLoops, localView->maxCels);
#endif

	metadata.loopsVeraAddressesPointers = (VeraSpriteAddress**)b10BankedAlloc(totalAllocationSize, &metadata.viewTableMetadataBank);
	metadata.veraAddresses = (VeraSpriteAddress*)metadata.loopsVeraAddressesPointers + loopVeraAddressesPointersSize;
	metadata.viewNum = viewNum;

#ifdef VERBOSE_DEBUG_SET_METADATA
	printf("Allocated the following addresses %p, %p. The size is %d + %d = %d. The bank is %d\n", metadata.loopVeraAddressesPointers, metadata.veraAddresses, loopVeraAddressesPointersSize, veraAddressesSize, totalAllocationSize, metadata.viewTableMetadataBank);
#endif
	viewTableMetadata[viewTabNo] = metadata;

#ifdef VERBOSE_DEBUG_SET_METADATA
	printf("Setting %p on bank %d to size %d\n", metadata.veraAddresses, metadata.viewTableMetadataBank, veraAddressesSize);
#endif
	memsetBanked(metadata.veraAddresses, 0, veraAddressesSize, metadata.viewTableMetadataBank);

	veraAddressCounter = metadata.veraAddresses;
	for (i = 0; i < localView->numberOfLoops; i++)
	{
		addressBuffer[i] = veraAddressCounter;

#ifdef VERBOSE_DEBUG_SET_METADATA
		printf("Address Setting %p to be %p\n", addressBuffer[i], veraAddressCounter);
#endif
#ifdef VERBOSE_DEBUG_BLIT
		printf("Address Setting %p to be %p\n", addressBuffer[i], veraAddressCounter);
#endif

		veraAddressCounter += maxVeraAddresses;
	}

	memCpyBanked((byte*)metadata.loopsVeraAddressesPointers, (byte*)addressBuffer, metadata.viewTableMetadataBank, localView->numberOfLoops * sizeof(VeraSpriteAddress**));

	viewTableMetadata[viewTabNo] = metadata;

#ifdef VERBOSE_DEBUG_SET_METADATA
	printf("The address of the buffer is %p\n", addressBuffer);
	printf("The address of the viewTableMD is %p\n", &viewTableMetadata[viewTabNo]);
#endif
}

#pragma wrapped-call (push, trampoline, SPRITE_METADATA_BANK)
byte bECreateSpritePalette(byte transparentColor)
{
	PaletteGetResult palleteGetResult;
	byte i;
	byte paletteColourLow, paletteColourHigh, paletteBlackLow, paletteBlackHigh;
	byte paletteSlot;
	long paletteWriteAddress;

#ifdef VERBOSE_GET_PALETTE
	printf("cs 1. trans color is %d\n", transparentColor);
#endif
	paletteSlot = bEGetPalette(BASE_SPRITE_ID + transparentColor, &palleteGetResult);

#ifdef VERBOSE_GET_PALETTE
	printf("cs 2. The palette slot is %d and the result is %d\n", paletteSlot, palleteGetResult);
#endif
	paletteWriteAddress = PALETTE_START + COLOURS_PER_PALETTE * BYTES_PER_PALETTE_COLOUR * paletteSlot;

	if (palleteGetResult == FailToAllocate)
	{
		return 0; //If we are out of palettes the best we can do is use the default
	}

	if (palleteGetResult == Allocated)
	{
		asm("sei");
		SET_VERA_ADDRESS(PALETTE_START, 0, 1);

		SET_VERA_ADDRESS_ABSOLUTE(paletteWriteAddress, 1, 1);

		for (i = 0; i < COLOURS_PER_PALETTE; i++)
		{

			READ_BYTE_VAR_FROM_ASSM(paletteColourLow, VERA_data0);
			READ_BYTE_VAR_FROM_ASSM(paletteColourHigh, VERA_data0);

#ifdef VERBOSE_GET_PALETTE
			printf("Read byte %p and %p\n", paletteColourLow, paletteColourHigh);
#endif

			if (i == 0)
			{
				paletteBlackLow = paletteColourLow;
				paletteBlackHigh = paletteColourHigh;
			}

			if (i == transparentColor)
			{
#ifdef VERBOSE_GET_PALETTE
				printf("The transparent colour is %d\n",i);
#endif
				WRITE_BYTE_VAR_TO_ASSM(paletteBlackLow, VERA_data1);
				WRITE_BYTE_VAR_TO_ASSM(paletteBlackHigh, VERA_data1);
			}
			else
			{
				WRITE_BYTE_VAR_TO_ASSM(paletteColourLow, VERA_data1);
				WRITE_BYTE_VAR_TO_ASSM(paletteColourHigh, VERA_data1);
			}

		}

		SET_VERA_ADDRESS(PALETTE_START, 0, 1); //Setting back to channel zero as if we don't printf can screw up our memory. This is a possible framework bug
		REENABLE_INTERRUPTS();
	}

	return paletteSlot;
}
#pragma wrapped-call (pop)

void bESwitchMetadata(ViewTable* localViewTab, View* localView, byte viewNum, byte entryNum)
{
	byte i, inactiveMetadataSlot, activeMetadataSlot;
	byte end = FALSE;
	ViewTableMetadata inActive, active;
	ViewTableMetadata* inActiveMetadataPointer;
	ViewTableMetadata localMetadata;

	localMetadata = viewTableMetadata[entryNum];
	inActiveMetadataPointer = (ViewTableMetadata*)localMetadata.inactive;

#ifdef  VERBOSE_SWITCH_METADATA
	printf("switch to %d\n", viewNum);
#endif

#ifdef  VERBOSE_SWITCH_METADATA
	printf("At the start the inactive is %p\n", localMetadata.inactive);
#endif

	if (localMetadata.inactive == NULL) //We have never done a metadata swap for this viewtab before
	{
		localMetadata.inactive = b10BankedAlloc(MAX_INACTIVE_METADATA * sizeof(ViewTableMetadata), &localMetadata.inactiveBank);

		inActiveMetadataPointer = (ViewTableMetadata*)localMetadata.inactive;

#ifdef  VERBOSE_SWITCH_METADATA
		printf("allocating %d bytes for extra metadata on bank %d\n", MAX_INACTIVE_METADATA * sizeof(ViewTableMetadata), localMetadata.inactiveBank);
#endif //  VERBOSE_SWITCH_METADATA


		memsetBanked(localMetadata.inactive, NULL, MAX_INACTIVE_METADATA * sizeof(ViewTableMetadata), localMetadata.inactiveBank);
	}

	for (i = 0; i < MAX_INACTIVE_METADATA && !end; i++) //Search through the inactive metadata list and see if the view we are trying to switch to is already there. When we reach the end of the list or we reach an entry with null loop pointers we know it doesn't exist
	{
		memCpyBanked((byte*)&inActive, (byte*)&(inActiveMetadataPointer)[i], localMetadata.inactiveBank, sizeof(ViewTableMetadata));
#ifdef  VERBOSE_SWITCH_METADATA
		printf("inactive copy copying to %p from %p on bank %d size %d\n", (byte*)&inActive, (byte*)&(inActiveMetadataPointer)[i], localMetadata.inactiveBank, sizeof(ViewTableMetadata));
#endif

		end = !inActive.loopsVeraAddressesPointers || inActive.viewNum == viewNum;

#ifdef  VERBOSE_SWITCH_METADATA
		printf("end check %d or %d = %d (%d)\n", !inActive.loopsVeraAddressesPointers, inActive.viewNum == viewNum, !inActive.loopsVeraAddressesPointers || inActive.viewNum == viewNum, end);
#endif
		inactiveMetadataSlot = i;
	}

	if (!end) //Is full
	{
		if (localMetadata.inactive != NULL)
		{
			bEResetInactiveViewTableMetadata(&localMetadata);

			b10BankedDealloc((byte*)inActiveMetadataPointer, localMetadata.inactiveBank);
			localMetadata.inactive = NULL;
			localMetadata.inactiveBank = NULL;

			bESwitchMetadata(localViewTab, localView, viewNum, entryNum); //Call itself with clear inactive metadata

			//TODO: Deallocate vera addresses

			return;
		}
	}

	end = FALSE;
	for (i = 0; i < MAX_INACTIVE_METADATA && !end; i++) //Search for inactive metadata in the list with the same rules as the active
	{
		memCpyBanked((byte*)&active, (byte*)&inActiveMetadataPointer[i], localMetadata.inactiveBank, sizeof(ViewTableMetadata));

#ifdef  VERBOSE_SWITCH_METADATA
		printf("copy to active. Copy to %p from  %p on bank %d size %d\n", &active, (byte*)&inActiveMetadataPointer[i], localMetadata.inactiveBank, sizeof(ViewTableMetadata));
#endif

		//Inactive search and active search may find the same empty slot, but if they do it doesn't matter because if the inactive slot is blank it will never be copied back to the inActiveMetadataPointer list. It will trigger the if (!inActive.loopsVeraAddressesPointers) if statement
		end = !active.loopsVeraAddressesPointers || active.loopsVeraAddressesPointers && active.viewNum == localMetadata.viewNum;

#ifdef  VERBOSE_SWITCH_METADATA
		printf("%d == %d\n", active.viewNum, localMetadata.viewNum);
		printf("end copy active %d or %d = %d (%d)\n", !active.loopsVeraAddressesPointers, active.loopsVeraAddressesPointers && active.viewNum == localMetadata.viewNum, !active.loopsVeraAddressesPointers || active.loopsVeraAddressesPointers && active.viewNum == localMetadata.viewNum, end);
#endif

		activeMetadataSlot = i;
	}

	if (!active.loopsVeraAddressesPointers) //If the active metadata is not currently in the list add it. If it is we don't need to do anymore
	{
		memCpyBanked((byte*)&inActiveMetadataPointer[activeMetadataSlot], (byte*)&localMetadata, localMetadata.inactiveBank, sizeof(ViewTableMetadata));

#ifdef  VERBOSE_SWITCH_METADATA
		printf("copy to inactive. copy to %p on bank %d  from %p size %d\n", &inActiveMetadataPointer[activeMetadataSlot], localMetadata.inactiveBank, (byte*)&localMetadata, sizeof(ViewTableMetadata));
#endif

	}

	if (!inActive.loopsVeraAddressesPointers) //If the inactive is not in the list we will clear the metadata so it can be allocated. However we keep the inactive list and bank
	{
		localMetadata.loopsVeraAddressesPointers = NULL;
		localMetadata.veraAddresses = NULL;
		localMetadata.viewNum = viewNum;
		localMetadata.viewTableMetadataBank = 0;
#ifdef  VERBOSE_SWITCH_METADATA
		printf("enacting clear\n");
#endif

		viewTableMetadata[entryNum] = localMetadata;
		bESetViewMetadata(localView, localViewTab, viewNum, entryNum, viewTabNoToMetaData[entryNum]);
		localMetadata = viewTableMetadata[entryNum];
	}
	else
	{
		memCpyBanked((byte*)&localMetadata, (byte*)&inActiveMetadataPointer[inactiveMetadataSlot], localMetadata.inactiveBank, sizeof(ViewTableMetadata));

#ifdef  VERBOSE_SWITCH_METADATA
		printf("in active exists. copy to %p from %p on bank %d size %d \n", &localMetadata, (byte*)&inActiveMetadataPointer[inactiveMetadataSlot], localMetadata.inactiveBank, sizeof(ViewTableMetadata));
#endif
	}

	viewTableMetadata[entryNum] = localMetadata;

#ifdef  VERBOSE_SWITCH_METADATA
	printf("the entryNum is %d\n", entryNum);
	printf("Setting %p on bank %p to be %p \n", &viewTableMetadata[entryNum], RAM_BANK, localMetadata);
#endif

#ifdef  VERBOSE_SWITCH_METADATA
	printf("At the end the inactive is %p\n", localMetadata.inactive);
#endif

#ifdef  VERBOSE_SWITCH_METADATA
	printf("End\n");
#endif
}

#define TO_BLIT_CEL_ARRAY_LENGTH 500
extern byte bEToBlitCelArray[TO_BLIT_CEL_ARRAY_LENGTH];
//Copy cels into array above first
extern void bECellToVeraBulk(SpriteAttributeSize allocationWidth, SpriteAttributeSize allocationHeight, byte noToBlit);
void bESetLoop(ViewTable* localViewTab, ViewTableMetadata* localMetadata, View* localView, VeraSpriteAddress* loopVeraAddresses)
{
	Loop localLoop;
	byte noToBlit;
	byte veraSpriteWidthAndHeight;
	AllocationSize allocationSize;

	getLoadedLoop(localView, &localLoop, localViewTab->currentLoop);

	noToBlit = localLoop.numberOfCels * localLoop.veraSlotsWidth * localLoop.veraSlotsHeight;

#ifdef VERBOSE_DEBUG_BLIT
	printf("Trying to copy to %p from %p. Number %d. \n ", localMetadata->loopsVeraAddressesPointers[localViewTab->currentLoop], bEBulkAllocatedAddresses, noToBlit);
#endif // VERBOSE_DEBUG_BLIT

#ifdef VERBOSE_DEBUG_BLIT
	printf("vera Sprite addresses %d * %d * %d = %d (%d)\n", localLoop.numberOfCels, localLoop.veraSlotsWidth, localLoop.veraSlotsHeight, localLoop.numberOfCels * localLoop.veraSlotsWidth * localLoop.veraSlotsHeight, noToBlit);
#endif

#ifdef VERBOSE_DEBUG_BLIT
	printf("Trying to allocate %d. Number %d\n", localLoop.allocationSize, noToBlit);
#endif

	if (localLoop.allocationHeight == SPR_ATTR_64 || localLoop.allocationWidth == SPR_ATTR_64)
	{
		allocationSize = SIZE_64;
	}
	else
	{
		allocationSize = SIZE_32;
	}

	if (!bEAllocateSpriteMemoryBulk(allocationSize, noToBlit))
	{
		printf("no sprite mem");
		exit(0);
	}

#ifdef VERBOSE_DEBUG_BLIT
	printf("The address of the buffer is %p\n ", bEBulkAllocatedAddresses);
	printf("loop vera is %p", loopVeraAddresses);
	printf("Trying to copy to %p on bank %d from %p on bank %d number %d.", (byte*)loopVeraAddresses, localMetadata->viewTableMetadataBank, bEBulkAllocatedAddresses, SPRITE_METADATA_BANK, noToBlit * sizeof(VeraSpriteAddress));
#endif
	enableHelpersDebugging = TRUE;
	memCpyBankedBetween((byte*)loopVeraAddresses, localMetadata->viewTableMetadataBank, bEBulkAllocatedAddresses, SPRITE_METADATA_BANK, noToBlit * sizeof(VeraSpriteAddress));
	enableHelpersDebugging = FALSE;

	memCpyBankedBetween(bEToBlitCelArray, SPRITE_METADATA_BANK, (byte*)localLoop.cels, localLoop.celsBank, localLoop.numberOfCels * sizeof(Cel));

#ifdef VERBOSE_DEBUG_BLIT
	printf("You are allocating %d.%d. It has a width of %d and height of %d. There are %d to blit\n", localViewTab->currentView, localViewTab->currentLoop, localLoop.allocationWidth, localLoop.allocationHeight, noToBlit);
#endif
	//Change this method
	bECellToVeraBulk(localLoop.allocationWidth, localLoop.allocationHeight, noToBlit);
}
#pragma code-name (pop)

#define ZP_SPRITE_STORE_PTR ZP_PTR_TMP_2

/***************************************************************************
** agi_blit
***************************************************************************/
void agiBlit(ViewTable* localViewTab, byte entryNum, boolean disableInterupts)
{
	View localView;
	Loop localLoop;
	Cel localCel;
	byte viewNum;
	byte previousBank;
	ViewTableMetadata localMetadata;
	VeraSpriteAddress* loopVeraAddresses;
	VeraSpriteAddress loopVeraAddress; //Put out here so it can be accessed by inline assembly without going via a C variable

	previousBank = RAM_BANK;

	RAM_BANK = SPRITE_METADATA_BANK;

	viewNum = localViewTab->currentView;

#ifdef VERBOSE_DEBUG_BLIT
	printf("The viewNum is %d and the loop is %d\n", viewNum, localViewTab->currentLoop);
#endif // VERBOSE_DEBUG_BLIT

	//#endif // VERBOSE_DEBUG_BLIT

	getLoadedView(&localView, viewNum);
	getLoadedLoop(&localView, &localLoop, localViewTab->currentLoop);
	getLoadedCel(&localLoop, &localCel, localViewTab->currentCel);

	if (viewTabNoToMetaData[entryNum] != VIEWNO_TO_METADATA_NO_SET && viewTableMetadata[entryNum].viewNum != viewNum)
	{
#ifdef VERBOSE_SWITCH_METADATA
		printf("switching to %d for entry %d\n", viewNum, entryNum);
#endif
		bESwitchMetadata(localViewTab, &localView, viewNum, entryNum);
	}

	if (viewTabNoToMetaData[entryNum] == VIEWNO_TO_METADATA_NO_SET) //Statement will be true if switched to another view for the first time in bESwitchMetadata
	{
#ifdef VERBOSE_DEBUG_NO_BLIT_CACHE
		printf("set Metadata %d. The vt is %d\n", localViewTab->viewData, entryNum);
#endif

		bESetViewMetadata(&localView, localViewTab, viewNum, entryNum, VIEWNO_TO_METADATA_NO_SET);
	}

	localMetadata = viewTableMetadata[entryNum];

#ifdef VERBOSE_DEBUG_BLIT
	printf("Address %p. The bank is %d\n", localMetadata.loopsVeraAddressesPointers[localViewTab->currentLoop], localMetadata.viewTableMetadataBank);
	printf("Checking %d.\n", localMetadata.loopsVeraAddressesPointers[localViewTab->currentLoop][0]);
#endif
	RAM_BANK = localMetadata.viewTableMetadataBank;
	loopVeraAddresses = localMetadata.loopsVeraAddressesPointers[localViewTab->currentLoop];

#ifdef VERBOSE_DEBUG_NO_BLIT_CACHE	
	printf("We are checking %p. It has a value of %u. The bank is %p and it should be %d\n", &loopVeraAddresses[0], loopVeraAddresses[0], RAM_BANK, localMetadata.viewTableMetadataBank);
	printf("The bank is %d\n", RAM_BANK);
#endif

	if (!loopVeraAddresses[0])
	{
		RAM_BANK = SPRITE_METADATA_BANK;

#ifdef VERBOSE_DEBUG_NO_BLIT_CACHE
		printf("loading view %d loop %d. The vt %p. It's position is %d,%d. v36 is %d\n", localViewTab->currentView, localViewTab->currentLoop, entryNum, localViewTab->xPos, localViewTab->yPos, var[36]);
#endif
		bESetLoop(localViewTab, &localMetadata, &localView, loopVeraAddresses);
	}

#ifdef VERBOSE_DEBUG_NO_BLIT_CACHE	
	RAM_BANK = localMetadata.viewTableMetadataBank;
	printf("we haved checked %p. It has a value of %p. The bank is %p and it should be %d\n", &loopVeraAddresses[0], loopVeraAddresses[0], RAM_BANK, localMetadata.viewTableMetadataBank);
	printf("The bank is %d\n", RAM_BANK);
#endif

	RAM_BANK = localMetadata.viewTableMetadataBank;
	loopVeraAddress = loopVeraAddresses[localViewTab->currentCel];

	RAM_BANK = SPRITE_UPDATED_BANK;

	if (disableInterupts)
	{
		asm("sei");
	}

	WRITE_INT_VAR_TO_ASSM((unsigned int)bESpritesUpdatedBufferPointer, ZP_SPRITE_STORE_PTR);


	//Put bytes into a buffer to be picked up by the irq see spriteIrqHandler.s (bEHandleSpriteUpdates)

	_assmUInt = loopVeraAddress;

	//Update here for blitting all parts

	//0 Vera Address Sprite Data Middle (Low will always be 0) (If both the first two bytes are zero that indicates the end of the buffer)
	asm("lda %v", _assmUInt);
	asm("and #$1F"); //Gets you the address bits 12:8 Which are the parts of the medium byte we need
	asm("asl"); //Gets bits 5:7 which are always zero
	asm("asl");
	asm("asl");
	asm("sta (%w)", ZP_SPRITE_STORE_PTR);

	//1 Vera Address Sprite Data High
	asm("lda %v", _assmUInt);
	asm("and #$E0");
	asm("lsr");
	asm("lsr");
	asm("lsr");
	asm("lsr");
	asm("lsr");
	asm("ldx %v + 1", _assmUInt);
	asm("beq @store"); //If the high byte is zero we don't need to worry about it
	asm("ora #$8"); //Keep the last three bits of the middle byte and have the forth byte high
	asm("@store: ldy #$1");
	asm("sta (%w),y", ZP_SPRITE_STORE_PTR);

	//2 x low
	_assmUInt = (byte)localViewTab->xPos;
	_assmByte = localCel.flipped;
	asm("ldy #$2");
	asm("lda %v", _assmUInt);
	asm("clc");
	asm("asl");
	asm("ldx %v", _assmByte);
	asm("bne @storeOnStackLow");
	asm("sta (%w),y", ZP_SPRITE_STORE_PTR);
	asm("bra @calculateHigh");
	asm("@storeOnStackLow: pha");

	//;3 x high
	asm("@calculateHigh: lda #$0");
	asm("rol");
	asm("ldy #$3");
	asm("cpx #$0");
	asm("bne @storeOnStackHigh");
	asm("sta (%w),y", ZP_SPRITE_STORE_PTR);
	asm("bra %g", yPos);
	asm("@storeOnStackHigh: pha");

	//x low and high if flipped (used in addition to what is above, if flipped the code above will put x on the stack so that further calculation can be done below)
moveXDueToFlipped:
	_assmByte = localCel.width;
	_assmByte2 = localLoop.allocationWidth;

	asm("lda %v", _assmByte);
	asm("asl");
	asm("sta %v", _assmByte);

	//Width 8
	asm("lda %v", _assmByte2);
	asm("cmp #%w", SPR_ATTR_8);
	asm("bne @check16");
	asm("lda #%w", MAX_8_WIDTH_OR_HEIGHT);
	asm("bra @takeWidthFromMaxWidth");

	//Width16
	asm("@check16: lda %v", _assmByte2);
	asm("cmp #%w", SPR_ATTR_16);
	asm("bne @check32");
	asm("lda #%w", MAX_16_WIDTH_OR_HEIGHT);
	asm("bra @takeWidthFromMaxWidth");

	//Width32
	asm("@check32: lda %v", _assmByte2);
	asm("cmp #%w", SPR_ATTR_32);
	asm("bne @set64");
	asm("lda #%w", MAX_32_WIDTH_OR_HEIGHT);
	asm("bra @takeWidthFromMaxWidth");

	//Width64
	asm("@set64: lda #%w", MAX_64_WIDTH_OR_HEIGHT);

	asm("@takeWidthFromMaxWidth: sec");
	asm("sbc %v", _assmByte); //Can't unset carry as we expect only 8 bit subtraction
	asm("sta %v", _assmByte);

	asm("pla");
	asm("tax");
	asm("pla");

	asm("sbc %v", _assmByte);
	asm("ldy #$2");
	asm("sta (%w),y", ZP_SPRITE_STORE_PTR);

	asm("txa");
	asm("sbc #$0");//Max width is only 8 bit
	asm("ldy #$3");
	asm("sta (%w),y", ZP_SPRITE_STORE_PTR);

	//4 y low (y high is always zero)
yPos: _assmByte = (byte)localViewTab->yPos;
	_assmByte2 = localCel.height - 1;

	asm("ldy #$4");
	asm("lda %v", _assmByte);

	asm("clc");
	asm("adc #%w", STARTING_ROW);

	asm("sec"); //Take away the height, as position in agi is the bottom left corner
	asm("sbc %v", _assmByte2);
	asm("sta (%w),y", ZP_SPRITE_STORE_PTR);

	//5 Flipped
	_assmUInt = (byte)localCel.flipped;
	asm("ldy #$5");
	asm("lda %v", _assmUInt);
	asm("ora #8"); //8 means in front of bitmap but behind text layers and not flipped, with a zero collision mask)
	asm("sta (%w),y", ZP_SPRITE_STORE_PTR);

	//6 Sprite Attr Size/Palette Offset
	_assmByte = localLoop.allocationWidth;
	_assmByte2 = localLoop.allocationHeight;
	_assmByte3 = localLoop.palette;

	asm("ldy #$6");
	asm("lda %v", _assmByte);
	asm("asl");
	asm("asl");
	asm("asl");
	asm("asl");
	asm("tax");
	asm("lda %v", _assmByte2);
	asm("asl");
	asm("asl");
	asm("asl");
	asm("asl");
	asm("asl");
	asm("asl");
	asm("sta %v", _assmByte2);
	asm("txa");
	asm("ora %v", _assmByte2);
	asm("clc"); //Might be less cycles to ora assmbyte 3 in instead. Investigate
	asm("adc %v", _assmByte3);

	asm("sta (%w),y", ZP_SPRITE_STORE_PTR);

	bESpritesUpdatedBufferPointer += BYTES_PER_SPRITE_UPDATE;

	if (disableInterupts)
	{
		REENABLE_INTERRUPTS();
	}

	//for (i = 0; i < w; i++) {
	//	for (j = 0; j < h; j++) {
	//		c = bmp->line[j][i];
	//		 Next line will be removed when error is found.
	//		if (((y + j) < 168) && ((x + i) < 160) && ((y + j) >= 0) && ((x + i) >= 0))

	//			if ((c != (trans + 1)) && (pNum >= priority->line[y + j][x + i])) {
	//				priority->line[y + j][x + i] = pNum;
	//				picture->line[y+j][x+i] = c;
	//				spriteScreen->line[y + j][x + i] = c;
	//			}
	//	}
	//}

	RAM_BANK = previousBank;
}

#pragma code-name (push, "BANKRAM09")
#pragma code-name (push, "BANKRAM09")
void b9ResetViewtabs()
{
	int entryNum;
	byte i;
	ViewTable localViewtab;

	for (entryNum = 0; entryNum < VIEW_TABLE_SIZE; entryNum++) {
		getViewTab(&localViewtab, entryNum);

		localViewtab.stepTime = 1;
		localViewtab.stepTimeCount = 1;
		localViewtab.xPos = 0;
		localViewtab.yPos = 0;
		localViewtab.currentView = 0;
		localViewtab.viewData = NULL;
		localViewtab.currentLoop = 0;
		localViewtab.numberOfLoops = 0;
		localViewtab.loopData = NULL;
		localViewtab.currentCel = 0;
		localViewtab.numberOfCels = 0;
		localViewtab.celData = NULL;
		localViewtab.xsize = 0;
		localViewtab.ysize = 0;
		localViewtab.stepSize = 1;
		localViewtab.cycleTime = 1;
		localViewtab.cycleTimeCount = 1;
		localViewtab.direction = 0;
		localViewtab.motion = 0;
		localViewtab.cycleStatus = 0;
		localViewtab.priority = 0;
		localViewtab.flags = 0;
		setViewTab(&localViewtab, entryNum);
	}
}
void b9Reset()
{
	bEResetSpritesUpdatedBuffer();
	bEResetViewTableMetadata();
	bEResetSpritePointers();
	bEResetSpriteMemoryManager();
	bEInitPaletteManager();
	b9ResetViewtabs();
	maxViewTable = 0;
}
void b9InitSpriteData()
{
	byte i;

	b9Reset();
}


void b9InitViews()
{
	int i, j;
	View localView;
	for (i = 0; i < 256; i++) {
		localView.description = 0;
		localView.loaded = FALSE;
		localView.loops = 0;
		localView.loopsBank = 0;
		localView.numberOfLoops = 0;
		localView.codeBlock = NULL;
		localView.codeBlockBank = 0;

		setLoadedView(&localView, i);
	}

	spriteScreen = create_bitmap(160, 168);
}

void b9InitObjects()
{
	b9ResetViewtabs();
	memsetBanked(viewTableMetadata, NULL, sizeof(ViewTableMetadata) * SPRITE_SLOTS, SPRITE_METADATA_BANK);
}

void b9ResetViews()     /* Called after new.room */
{
	int entryNum;
	ViewTable localViewtab;

	for (entryNum = 0; entryNum < VIEW_TABLE_SIZE; entryNum++) {
		getViewTab(&localViewtab, entryNum);

		localViewtab.flags &= ~(UPDATE | ANIMATED);
		setViewTab(&localViewtab, entryNum);
	}

	b9Reset();
}


#define VIEW_HEADER_BUFFER_SIZE 501
#define LOOP_HEADER_BUFFER_SIZE 501
#define VIEW_BUFFERS_BANK 9
extern byte viewHeaderBuffer[VIEW_HEADER_BUFFER_SIZE];
extern byte loopHeaderBuffer[LOOP_HEADER_BUFFER_SIZE];

#define POSITION_OF_NO_LOOPS 2
#define POSITION_OF_LOOPS_OFFSET 5
#define POSITION_OF_DESCRIPTION 3

#define NO_LOOPS_INDEX_BYTES_AVERAGE 14
#define NO_CELLS_INDEX_BYTES_AVERAGE 14

void setViewData(byte viewNum, AGIFile* tempAGI, View* localView)
{
	byte numberOfLoops;
	boolean isThereADescription;
	const char* description;
	int descriptionLength;
	byte loopsBank;
#ifdef VERBOSE_LOAD_VIEWS
	byte tmp[10];
#endif

	getLoadedView(localView, viewNum);

	if (!localView->loaded)
	{
		memCpyBankedBetween((byte*)&viewHeaderBuffer[0], VIEW_BUFFERS_BANK, tempAGI->code, tempAGI->codeBank, NO_LOOPS_INDEX_BYTES_AVERAGE + POSITION_OF_LOOPS_OFFSET); //Guessing at 7 loops, this should copy enough 99% of the time, and we can copy again once we have the first byte (loopCounter), is necessary . If there is less loops than we have just copied bytes which will be ignored
		numberOfLoops = viewHeaderBuffer[POSITION_OF_NO_LOOPS];

		if (numberOfLoops > NO_LOOPS_INDEX_BYTES_AVERAGE)
		{
			memCpyBankedBetween((byte*)&viewHeaderBuffer[0], VIEW_BUFFERS_BANK, tempAGI->code, tempAGI->codeBank, numberOfLoops * 2 + POSITION_OF_LOOPS_OFFSET);
		}


#ifdef VERBOSE_SET_VIEW
		printf("setting view number %d\n", viewNum);
		printf("there are %d loops\n", numberOfLoops);
#endif

		isThereADescription = (const char*)viewHeaderBuffer[POSITION_OF_DESCRIPTION] || viewHeaderBuffer[POSITION_OF_DESCRIPTION + 1];

		if (isThereADescription)
		{
			description = (const char*)(tempAGI->code + viewHeaderBuffer[POSITION_OF_DESCRIPTION] + viewHeaderBuffer[POSITION_OF_DESCRIPTION] * 256);
			descriptionLength = strLenBanked((char*)description, tempAGI->codeBank);

#ifdef VERBOSE_SET_VIEWS
			printf("Description length %d. The description is %s\n", descriptionLength, description);
#endif // VERBOSE_ALLOC_WATCH
		}
		else {

#ifdef VERBOSE_SET_VIEWS
			printf("there is no description");
#endif // VERBOSE_ALLOC_WATCH
			//TODO: Use /0
			description = (const char*)&tempAGI->code[POSITION_OF_DESCRIPTION]; //Going to be zero. We can do this even though we are not on the bank; there is no deference
		}

#ifdef VERBOSE_SET_VIEWS
		memCpyBanked(&tmp[0], (byte*)localView->description, localView->loopsBank, descriptionLength);
		if (tmp[0] == 0)
		{
			printf("The description is empty\n");
		}
		else
		{
			printf("The description is not empty\n");
			printf("It has a value of %s ", description);
		}
#endif

		localView->loaded = TRUE;
		localView->loops = (Loop*)b10BankedAlloc(numberOfLoops * sizeof(Loop), &loopsBank);

		localView->numberOfLoops = numberOfLoops;
		localView->description = description;
		localView->loopsBank = loopsBank;
		localView->maxCels = 0;
		localView->maxVeraSlots = 1;
	}
	else
	{
		printf("View %d is already loaded\n", viewNum);
	}

	localView->codeBlock = tempAGI->code;
	localView->codeBlockBank = tempAGI->codeBank;
	setLoadedView(&localView, viewNum);
}

#define POSITION_OF_NO_CELS 0
#define POSITION_OF_CELS_OFFSET 1
void setLoopData(AGIFile* tempAGI, View* localView, Loop* localLoop, byte* loopHeaderData, byte viewNum, byte loopNum)
{
	byte numberOfCels;
	byte celsBank;
#ifdef VERBOSE_LOAD_VIEWS
	byte tmp[10];
#endif

	if (localView->loaded) //If the view is loaded then so are all of the loops
	{
		getLoadedLoop(localView, localLoop, loopNum);

		memCpyBankedBetween(&loopHeaderBuffer[0], VIEW_BUFFERS_BANK, loopHeaderData, tempAGI->codeBank, NO_CELLS_INDEX_BYTES_AVERAGE + POSITION_OF_CELS_OFFSET); //Guessing at 7 cels, this should copy enough 99% of the time, and we can copy again once we have the first byte (loopCounter), is necessary . If there is less loops than we have just copied bytes which will be ignored
		numberOfCels = loopHeaderBuffer[POSITION_OF_NO_CELS];

#ifdef VERBOSE_SET_LOOPS
		printf("The numberOfCels is %d\n", numberOfCels);
#endif

		if (numberOfCels * 2 > NO_CELLS_INDEX_BYTES_AVERAGE)
		{
			memCpyBankedBetween((byte*)&loopHeaderBuffer[0], VIEW_BUFFERS_BANK, loopHeaderData, tempAGI->codeBank, numberOfCels * 2 + POSITION_OF_CELS_OFFSET);
		}

#ifdef VERBOSE_SET_LOOPS
		printf("setting loop number %d on view %d\n", loopNum, viewNum);
		printf("there are %d cels\n", numberOfCels);
		printf("The address of loop header buffer is %p\n", &loopHeaderBuffer[0]);
		printf("Loop header data is at %p, on bank %d\n", loopHeaderData, tempAGI->codeBank);
#endif

		localLoop->cels = (Cel*)b10BankedAlloc(numberOfCels * sizeof(Cel), &celsBank);
		localLoop->numberOfCels = numberOfCels;
		localLoop->celsBank = celsBank;
		setLoadedLoop(localView, localLoop, loopNum);
	}
	else
	{
		printf("Fail %d is not loaded\n", viewNum);
	}
}


byte b9VeraSlotsForWidthOrHeight(byte widthOrHeight)
{
	byte i;

	for (i = 1; i <= MAX_SPRITES_ROW_OR_COLUMN_SIZE; i++)
	{
		if (widthOrHeight <= MAX_64_WIDTH_OR_HEIGHT * i)
		{
			return  i;
		}
	}

	printf("H/W too big\n");

	exit(0);

	return 0;
}


#define POSITION_OF_CEL_WIDTH 0
#define POSITION_OF_CEL_HEIGHT 1
#define POSTION_OF_CEL_TRANSPARENCY_AND_MIRRORING 2
#define POSITION_OF_CEL_DATA 3
#define CEL_HEADER_SIZE 3

/**************************************************************************
** loadViewFile
**
** Purpose: Loads a VIEW file into memory storing it in the loadedViews
** array.
**************************************************************************/
void b9LoadViewFile(byte viewNum)
{
	AGIFile tempAGI;
	AGIFilePosType agiFilePosType;
	byte l, c, trans, i;
	View localView;
	Loop localLoop;
	Cel localCel;
	byte* cellPosition;
	int* loopOffsets = (int*)(viewHeaderBuffer + POSITION_OF_LOOPS_OFFSET);
	int* cellOffsets;
	byte celHeader[CEL_HEADER_SIZE];
	byte maxLoopVeraSlots = 1;
	byte currentLoopVeraSlots;

#ifdef VERBOSE_LOAD_VIEWS
	printf("Attempt to load viewNum %d\n", viewNum);
#endif // VERBOSE_LOAD_VIEWS

	getLogicDirectory(&agiFilePosType, &viewdir[viewNum]);
	b6LoadAGIFile(VIEW, &agiFilePosType, &tempAGI);

#ifdef VERBOSE_LOAD_VIEWS
	printf("loaded agiFile of total size %u\n", tempAGI.totalSize);
#endif

	setViewData(viewNum, &tempAGI, &localView);

#ifdef VERBOSE_LOAD_VIEWS
	printf("The view desc %s, loaded %d, loops %p, loopsBank %d, numberOfLoops %d\n", localView.description, localView.loaded, localView.loops, localView.loopsBank, localView.numberOfLoops);
	printf("The address of viewHeaderBuffer is %p", &viewHeaderBuffer[0]);
#endif // VERBOSE_LOAD_VIEWS

#define POSITION_BYTES 2
	for (l = 0; l < localView.numberOfLoops; l++) {
#ifdef VERBOSE_LOAD_VIEWS
		printf("Loading loop %d at %x\n", l, loopOffsets[l]);
#endif // VERBOSE_LOAD_VIEWS

#ifdef VERBOSE_LOAD_VIEWS
		printf("View code starts at %p. The loop offset is %x, we means we expect to find a loop at %x\n", tempAGI.code, loopOffsets[l], tempAGI.code + loopOffsets[l]);
#endif // VERBOSE_SET_LOOPS

		setLoopData(&tempAGI, &localView, &localLoop, tempAGI.code + loopOffsets[l], viewNum, l);
		cellOffsets = (int*)(loopHeaderBuffer + POSITION_OF_CELS_OFFSET);
		localLoop.allocationWidth = SPR_ATTR_8;
		localLoop.allocationHeight = SPR_ATTR_8;
		localLoop.veraSlotsWidth = 1;
		localLoop.veraSlotsHeight = 1;
		localLoop.palette = PALETTE_NOT_SET;

		for (c = 0; c < localLoop.numberOfCels; c++) {
			cellPosition = tempAGI.code + loopOffsets[l] + cellOffsets[c];
			memCpyBanked(celHeader, cellPosition, tempAGI.codeBank, CEL_HEADER_SIZE);

			getLoadedCel(&localLoop, &localCel, c);
			trans = celHeader[POSTION_OF_CEL_TRANSPARENCY_AND_MIRRORING];

			localCel.bitmapBank = tempAGI.codeBank;
			localCel.bmp = cellPosition + POSITION_OF_CEL_DATA;

#ifdef VERBOSE_LOAD_VIEWS
			printf("The address of the data is %p and the bank is %d\n", localCel.bmp, localCel.bitmapBank);
			printf("The cel is %d. The loop off is %p and the cell of is %d. The total is %p and the address is %p\n", c, loopOffsets[l], cellOffsets[c], loopOffsets[l] + cellOffsets[c], cellPosition);
#endif

			localCel.width = celHeader[POSITION_OF_CEL_WIDTH];
			localCel.height = celHeader[POSITION_OF_CEL_HEIGHT];
			localCel.flipped = (trans & 0x80) && (((trans & 0x70) >> 4) != l);
			localCel.transparency = trans & 0xF;

			if (localLoop.palette == PALETTE_NOT_SET)
			{
#ifdef VERBOSE_GET_PALETTE
				printf("lv 1. create palette for view %d loop %d\n", viewNum, l);
#endif
				localLoop.palette = bECreateSpritePalette(localCel.transparency);

#ifdef VERBOSE_GET_PALETTE
				printf("lv 1. set palette for view %d loop %d. palette %d\n", viewNum, l, localLoop.palette);
#endif
			}

#ifdef VERBOSE_LOAD_VIEWS
			printf("Local view %d.%d.%d is %d x %d, when width doubled %d x %d\n", viewNum, l, c, localCel.width, localCel.height, localCel.width * 2, localCel.height);
#endif

			//8 Is Default
			if (localCel.width * 2 > MAX_32_WIDTH_OR_HEIGHT && localLoop.allocationWidth < MAX_64_WIDTH_OR_HEIGHT)
			{
				localLoop.allocationWidth = SPR_ATTR_64;
			}
			else if (localCel.width * 2 > MAX_16_WIDTH_OR_HEIGHT && localLoop.allocationWidth < MAX_32_WIDTH_OR_HEIGHT)
			{
				localLoop.allocationWidth = SPR_ATTR_32;
			}
			else if (localCel.width * 2 > MAX_8_WIDTH_OR_HEIGHT && localLoop.allocationWidth < MAX_16_WIDTH_OR_HEIGHT)
			{
				localLoop.allocationWidth = SPR_ATTR_16;
			}

			////Height isn't doubled only width
			if (localCel.height > MAX_32_WIDTH_OR_HEIGHT && localLoop.allocationHeight < MAX_64_WIDTH_OR_HEIGHT)
			{
				localLoop.allocationHeight = SPR_ATTR_64;
			}
			else if (localCel.height > MAX_16_WIDTH_OR_HEIGHT && localLoop.allocationHeight < MAX_32_WIDTH_OR_HEIGHT)
			{
				localLoop.allocationHeight = SPR_ATTR_32;
			}
			else if (localCel.height > MAX_8_WIDTH_OR_HEIGHT && localLoop.allocationHeight < MAX_16_WIDTH_OR_HEIGHT)
			{
				localLoop.allocationHeight = SPR_ATTR_16;
			}

			localLoop.veraSlotsWidth = b9VeraSlotsForWidthOrHeight(localCel.width * 2);

			localLoop.veraSlotsHeight = b9VeraSlotsForWidthOrHeight(localCel.height);

#ifdef VERBOSE_LOAD_VIEWS
			printf("The viewNum is %d\n", viewNum);
			printf("The address of celHeader is %p\n", celHeader);
			printf("bitmapBank %d, bmp %p, height %d, width %d, flipped %d \n", localCel.bitmapBank, localCel.bmp, localCel.height, localCel.width, localCel.flipped);
#endif // VERBOSE_SET_CEL
			setLoadedCel(&localLoop, &localCel, c);
		}

		if (localLoop.numberOfCels > localView.maxCels)
		{
			localView.maxCels = localLoop.numberOfCels;
		}

		currentLoopVeraSlots = localLoop.veraSlotsWidth * localLoop.veraSlotsHeight;

#ifdef VERBOSE_LOAD_VIEWS
		printf("Current loop slots %d * %d = %d\n", localLoop.veraSlotsWidth, localLoop.veraSlotsHeight, currentLoopVeraSlots);
#endif

		if (currentLoopVeraSlots > localView.maxVeraSlots)
		{
			localView.maxVeraSlots = currentLoopVeraSlots;
		}

#ifdef VERBOSE_LOAD_VIEWS
		printf("view %d loop %d is allocated width and %d height %d\n", viewNum, l, localLoop.allocationWidth, localLoop.allocationHeight);
#endif


		setLoadedLoop(&localView, &localLoop, l);
	}
	setLoadedView(&localView, viewNum);
}

/***************************************************************************
** discardView
**
** Purpose: To deallocate memory associated with view number.
***************************************************************************/
void b9DiscardView(byte viewNum)
{
	byte l, c;
	View localView;
	Loop localLoop;
	Cel localCel;

	getLoadedView(&localView, viewNum);

	if (localView.loaded) {
		for (l = 0; l < localView.numberOfLoops; l++) {

			getLoadedLoop(&localView, &localLoop, l);

			for (c = 0; c < localLoop.numberOfCels; c++) {
				getLoadedCel(&localLoop, &localCel, c);
				localCel.bmp = NULL;
				localCel.height = 0;
				localCel.transparency = 0;
				localCel.width = 0;

				if (localCel.splitCelPointers)
				{
					b10BankedDealloc((byte*)localCel.splitCelPointers, localCel.splitCelBank);
				}
				localCel.splitCelPointers = NULL;

				setLoadedCel(&localLoop, &localCel, c);
			}
#ifdef VERBOSE_ALLOC_WATCH
			printf("dealloc cels bank %p address %p\n", localLoop.celBank, (byte*)localLoop.cels);
#endif
			b10BankedDealloc((byte*)localLoop.cels, localLoop.celsBank);
			localLoop.celsBank = 0;
			localLoop.cels = NULL;
			localLoop.numberOfCels = 0;
			setLoadedLoop(&localView, &localLoop, l);
		}

#ifdef VERBOSE_ALLOC_WATCH
		printf("dealloc loops bank %p address %p\n", localView.loopsBank, localView.loops);
#endif // VERBOSE_ALLOC_WATCH
		b10BankedDealloc((byte*)localView.loops, localView.loopsBank);
		localView.loaded = FALSE;

		b10BankedDealloc(localView.codeBlock, localView.codeBlockBank);
		localView.codeBlock = NULL;
		localView.codeBlockBank = 0;
		setLoadedView(&localView, viewNum);
	}
}

void b9SetCel(ViewTable* localViewtab, byte celNum)
{
	Loop temp;
	View localLoadedView;
	Cel localCel;

	getLoadedView(&localLoadedView, localViewtab->currentView);
	getLoadedLoop(&localLoadedView, &temp, localViewtab->currentLoop);
	getLoadedCel(&temp, &localCel, celNum);

	localViewtab->currentCel = celNum;
	localViewtab->xsize = localCel.width;
	localViewtab->ysize = localCel.height;
}

void b9SetLoop(ViewTable* localViewtab, byte loopNum)
{
	View temp;
	Loop loop;
	getLoadedView(&temp, localViewtab->currentView);
	getLoadedLoop(&temp, &loop, loopNum);

	localViewtab->currentLoop = loopNum;
	localViewtab->numberOfCels = loop.numberOfCels;
	/* Might have to set the cel as well */
	/* It's probably better to do that in the set_loop function */
}
/**************************************************************************
** addViewToTable
**
** Purpose: To add a loaded VIEW to the VIEW table.
**************************************************************************/
void b9AddViewToTable(ViewTable* localViewtab, byte viewNum, byte entryNum)
{
	View localView;
	Loop localLoop;

	getLoadedView(&localView, viewNum);
	getLoadedLoop(&localView, &localLoop, 0);

	localViewtab->currentView = viewNum;
	localViewtab->numberOfLoops = localView.numberOfLoops;
	b9SetLoop(localViewtab, 0);

	localViewtab->numberOfCels = localLoop.numberOfCels;
	b9SetCel(localViewtab, 0);
	/* Might need to set some more defaults here */

	if (maxViewTable < entryNum)
	{
		maxViewTable = entryNum;
	}
}

void b9AddToPic(int vNum, int lNum, int cNum, int x, int y, int pNum, int bCol)
{
	int i, j, trans, c, boxWidth;
	View localView;
	Loop localLoop;
	Cel localCel;

	getLoadedView(&localView, vNum);
	getLoadedLoop(&localView, &localLoop, lNum);
	getLoadedCel(&localLoop, &localCel, cNum);

#ifdef VERBOSE_ADD_TO_PIC
	printf("view %p loop %p cel %p\n", &localView, &localLoop, &localCel);
	printf("cel %d loaded %d bmp %p. View %d. Loop %d, Cel %d\n", cNum, localView.loaded, localCel.bmp, vNum, lNum, cNum);
	printf("x and y are (%d,%d). Adjusted Height %d. The address is %lx.\n ", x, y, y - localCel.height + 1, b2GetVeraPictureAddress(x, (y - localCel.height) + 1));
	printf("w %d h %d\n", localCel.width, localCel.height);
#endif // VERBOSE_ADD_TO_PIC

	b9CelToVera(&localCel, b4GetVeraPictureAddress(x, (y - localCel.height) + 1), bCol, BYTES_PER_ROW);

	//TODO: Finish implementing the priority and control line stuff
//
//	for (i = 0; i < w; i++) {
//		for (j = 0; j < h; j++) {
//			c = localCell.bmp->line[j][i];
//			if ((c != (trans + 1)) && (pNum >= priority->line[y + j][x + i])) {
//				priority->line[y + j][x + i] = pNum;
//				picture->line[y + j][x + i] = c;
//			}
//		}
//	}
//
//	/* Maybe the box height only extends to the next priority band */
//
//	boxWidth = ((h >= 7) ? 7 : h);
//	if (bCol < 4) rect(control, x, (y + h) - (boxWidth), (x + w) - 1, (y + h) - 1, bCol);
}
#pragma code-name (pop)
#pragma code-name (push, "BANKRAM0A")
#pragma wrapped-call (push, trampoline, VIEW_CODE_BANK_2)
void bACalcDirection(ViewTable* localViewtab)
{
	if (!(localViewtab->flags & FIXLOOP)) {
		if (localViewtab->numberOfLoops < 4) {

			switch (localViewtab->direction) {
			case 1: break;
			case 2: b9SetLoop(localViewtab, 0); break;
			case 3: b9SetLoop(localViewtab, 0); break;
			case 4: b9SetLoop(localViewtab, 0); break;
			case 5: break;
			case 6: b9SetLoop(localViewtab, 1); break;
			case 7: b9SetLoop(localViewtab, 1); break;
			case 8: b9SetLoop(localViewtab, 1); break;
			}
		}
		else {
			switch (localViewtab->direction) {
			case 1: b9SetLoop(localViewtab, 3); break;
			case 2: b9SetLoop(localViewtab, 0); break;
			case 3: b9SetLoop(localViewtab, 0);  break;
			case 4: b9SetLoop(localViewtab, 0); break;
			case 5: b9SetLoop(localViewtab, 2); break;
			case 6: b9SetLoop(localViewtab, 1); break;
			case 7: b9SetLoop(localViewtab, 1); break;
			case 8: b9SetLoop(localViewtab, 1); break;

			}
		}
	}
}
#pragma wrapped-call (pop)

/* Called by draw() */
void bADrawObject(int entryNum)
{
	word objFlags;
	int dummy;
	ViewTable localViewtab;

	if (entryNum == 4) {
		dummy = 1;
	}

	getViewTab(&localViewtab, entryNum);

	objFlags = localViewtab.flags;

	/* Determine priority for unfixed priorities */
	if (!(objFlags & FIXEDPRIORITY)) {
		if (localViewtab.yPos < 60)
			localViewtab.priority = 4;
		else
			localViewtab.priority = (localViewtab.yPos / 12 + 1);
	}

	bACalcDirection(&localViewtab);

#ifdef VERBOSE_DEBUG_BLIT
	printf("Called from draw object");
#endif // DEBUG

	setViewTab(&localViewtab, entryNum);
}

#pragma wrapped-call (push, trampoline, VIEW_CODE_BANK_2)
/***************************************************************************
** updateEgoDirection
**
** Purpose: To update var[6] when ego is moved with adjustPosition().
***************************************************************************/
void bAUpdateEgoDirection(int oldX, int oldY, int newX, int newY, ViewTable* viewTab)
{
	int dx = (newX - oldX);
	int dy = (newY - oldY);

	if ((dx == 0) && (dy == 0)) var[6] = dirnOfEgo = 0;
	if ((dx == 0) && (dy == -1)) var[6] = dirnOfEgo = 1;
	if ((dx == 1) && (dy == -1)) var[6] = dirnOfEgo = 2;
	if ((dx == 1) && (dy == 0)) var[6] = dirnOfEgo = 3;
	if ((dx == 1) && (dy == 1)) var[6] = dirnOfEgo = 4;
	if ((dx == 0) && (dy == 1)) var[6] = dirnOfEgo = 5;
	if ((dx == -1) && (dy == 1)) var[6] = dirnOfEgo = 6;
	if ((dx == -1) && (dy == 0)) var[6] = dirnOfEgo = 7;
	if ((dx == -1) && (dy == -1)) var[6] = dirnOfEgo = 8;

	bACalcDirection(viewTab);
}

/***************************************************************************
** adjustPosition
**
** Purpose: To adjust the given objects position so that it moves closer
** to the given position. The routine is similar to a line draw and is used
** for the move.obj. If the object is ego, then var[6] has to be updated.
***************************************************************************/
void bAAdjustPosition(ViewTable* viewTab, int fx, int fy, byte entryNum)
{
	int height, width, count, stepVal, dx, dy;
	fix32 x, y, addX, addY, x1, y1, x2, y2;
	int dummy;
	boolean xIsPos = TRUE, yIsPos = TRUE;

	/* Set up start and end points */
	x1 = b1FpFromInt(viewTab->xPos);
	y1 = b1FpFromInt(viewTab->yPos);
	x2 = b1FpFromInt(fx);
	y2 = b1FpFromInt(fy);

#ifdef VERBOSE_MOVE
	if (opCounter > 0x55c00 && entryNum == 0)
	{
		printf("x1 is %lu, y1 is %lu, x2 is %lu, y2 is %lu\n", x1, y1, x2, y2);
	}
#endif // VERBOSE_MOVE

	if (x1 > x2)
	{
		xIsPos = FALSE;
	}

	if (y1 > y2)
	{
		yIsPos = FALSE;
	}
	width = abs(fx - viewTab->xPos);
	height = abs(fy - viewTab->yPos);


#ifdef VERBOSE_MOVE
	if (opCounter > 0x55c00 && entryNum == 0)
	{
		printf("height is %d width is %d\n", height, width);
	}
#endif // VERBOSE_MOVE

	addX = (height == 0 ? b1FpFromInt(height) : b1Div(width, abs(height)));
	addY = (width == 0 ? b1FpFromInt(width) : b1Div(height, abs(width)));

#ifdef VERBOSE_MOVE
	if (opCounter > 0x55c00 && entryNum == 0)
	{
		printf("add x is %lu add y is %lu\n", addX, addY);
	}
#endif // VERBOSE_MOVE

	/* Will find the point on the line that is stepSize pixels away */
	if (abs(width) > abs(height)) {
		y = y1;

#ifdef VERBOSE_MOVE
		if (opCounter > 0x55c00 && entryNum == 0)
		{
			printf("width %d greater than height %d\n", width, height);
		}
#endif // VERBOSE_MOVE

		addX = (width == 0 ? 0 : b1Div(width, abs(width)));

#ifdef VERBOSE_MOVE
		if (opCounter > 0x55c00 && entryNum == 0)
		{
			printf("add x is %lu\n", addX);
		}
#endif // VERBOSE_MOVE


		if (xIsPos && addX == 0)
		{
			if (!yIsPos)
				viewTab->direction = 1;
			else
				viewTab->direction = 5;
		}
		else if (!xIsPos && addX == b1FpFromInt(1))
		{
			if (!yIsPos)
				viewTab->direction = 8;
			else if (yIsPos && y != 0)
				viewTab->direction = 6;
			else
				viewTab->direction = 7;
		}
		else //x Is Pos and x == 1
		{
			if (!yIsPos)
				viewTab->direction = 2;
			else if (yIsPos && addY != 0)
				viewTab->direction = 4;
			else
				viewTab->direction = 3;
		}

#ifdef VERBOSE_MOVE
		if (opCounter > 0x55c00 && entryNum == 0)
		{
			printf("add x is %lu add y is %lu. Direction is %d\n", addX, addY, viewTab->direction);
		}
#endif // VERBOSE_MOVE

		count = 0;
		stepVal = viewTab->stepSize;

#ifdef VERBOSE_MOVE
		if (opCounter > 0x55c00 && entryNum == 0)
		{
			printf("x1 is %lu, (%lu != %lu) && (%d < (%d + 1)) result: %d\n", x1, x, x2, count, stepVal, (x != x2) && (count < (stepVal + 1)));
		}
#endif // VERBOSE_MOVE

#ifdef VERBOSE_MOVE
		if (opCounter > 0x55c00 && entryNum == 0)
		{
			printf("x loop start\n");
			printf("the value of addX is %lu. which is roughly %d\n", addX, b1fpToInt(addX));
		}
#endif // VERBOSE_MOVE
		for (x = x1; (x != x2) && (count < (stepVal + 1)); xIsPos ? x += addX : x -= addX, count++) {

#ifdef VERBOSE_MOVE
			if (opCounter > 0x55c00 && entryNum == 0)
			{
				printf("before ceil x is %lu and y is %lu\n", x, y);
			}
#endif

			dx = b1CeilFix32(x);
			dy = b1CeilFix32(y);
			yIsPos ? y += addY : y -= addY;

#ifdef VERBOSE_MOVE
			if (opCounter > 0x55c00 && entryNum == 0)
			{
				printf("in loop x1 is %lu, (%lu != %lu) && (%d < (%d + 1)) result: %d\n", x1, x, x2, count, stepVal, (x != x2) && (count < (stepVal + 1)));
				printf("x is %lu\n", x);
				printf("In loop dx is %d and dy is %d\n", dx, dy);
			}

#endif // VERBOSE_MOVE
		}
#ifdef VERBOSE_MOVE
		if (opCounter > 0x55c00 && entryNum == 0)
		{
			printf("out loop x1 is %lu, (%lu != %lu) && (%d < (%d + 1)) result: %d\n", x1, x, x2, count, stepVal, (x != x2) && (count < (stepVal + 1)));
			printf("x is %lu and y is %lu\n", x, y);
		}

#endif // VERBOSE_MOVE

#ifdef VERBOSE_MOVE
		if (opCounter > 0x55c00 && entryNum == 0)
		{
			printf("(%lu == %lu) && (%d < (%d + 1)) result %d\n", x, x2, count, stepVal, (x == x2) && (count < (stepVal + 1)));
		}
#endif // VERBOSE_MOVE

		if ((x == x2) && (count < (stepVal + 1))) {
			dx = b1CeilFix32(x);
			dy = b1CeilFix32(y);
		}
	}
	else {
		x = x1;

#ifdef VERBOSE_MOVE
		if (opCounter > 0x55c00 && entryNum == 0)
		{
			printf("width %d less than or equal height %d\n", width, height);
		}
#endif // VERBOSE_MOVE


		addY = (height == 0 ? 0 : b1Div(height, abs(height)));

#ifdef VERBOSE_MOVE
		if (opCounter > 0x55c00 && entryNum == 0)
		{
			printf("add y is %lu. its address is %p\n", addY, &addY);
		}
#endif // VERBOSE_MOVE


		if (yIsPos && addY == 0)
		{
			if (!xIsPos)
				viewTab->direction = 7;
			else
				viewTab->direction = 3;
		}
		else if (!yIsPos && addY == b1FpFromInt(1))
		{
			if (!xIsPos)
				viewTab->direction = 8;
			else if (xIsPos && addX != 0)
				viewTab->direction = 2;
			else
				viewTab->direction = 1;
		}
		else //y Is Pos and y == 1
		{
			if (!xIsPos)
				viewTab->direction = 6;
			else if (xIsPos && addX != 0)
				viewTab->direction = 4;
			else
				viewTab->direction = 5;
		}

#ifdef VERBOSE_MOVE
		if (opCounter > 0x55c00 && entryNum == 0)
		{
			printf("xispos %d yispos %d addx is %lu addy is %lu\n", xIsPos, yIsPos, addX, addY);
			printf("view tab direction is %d\n", viewTab->direction);
		}
#endif // VERBOSE_MOVE

#ifdef VERBOSE_MOVE
		if (opCounter > 0x55c00 && entryNum == 0)
		{
			printf("add x is %lu add y is %lu\n", addX, addY);
		}
#endif // VERBOSE_MOVE

		count = 0;
		stepVal = viewTab->stepSize;


#ifdef VERBOSE_MOVE
		if (opCounter > 0x55c00 && entryNum == 0)
		{
			printf("y1 is %lu, (%lu != %lu) && (%d < (%d + 1)) result: %d\n", y1, y, y2, count, stepVal, (y != y2) && (count < (stepVal + 1)));
		}
#endif // VERBOSE_MOVE

		for (y = y1; (y != y2) && (count < (stepVal + 1)); yIsPos ? y += addY : y -= addY, count++) {
			dx = b1CeilFix32(x);

#ifdef VERBOSE_MOVE
			if (opCounter > 0x55c00 && entryNum == 0)
			{
				printf("before ceil x is %lu and y is %lu\n", x, y);
			}
#endif
			dy = b1CeilFix32(y);
			xIsPos ? x += addX : x -= addX;

#ifdef VERBOSE_MOVE
			if (opCounter > 0x55c00 && entryNum == 0)
			{
				printf("in loop y1 is %lu, (%lu != %lu) && (%d < (%d + 1)) result: %d\n", y1, y, y2, count, stepVal, (y != y2) && (count < (stepVal + 1)));
				printf("x is %lu\n", x);
				printf("In loop dx is %d and dy is %d\n", dx, dy);
			}

#endif // VERBOSE_MOVE
		}
#ifdef VERBOSE_MOVE
		if (opCounter > 0x55c00 && entryNum == 0)
		{
			printf("out loop y1 is %lu, (%lu != %lu) && (%d < (%d + 1)) result: %d\n", y1, y, y2, count, stepVal, (y != y2) && (count < (stepVal + 1)));
			printf("x is %lu and y is %lu\n", x, y);
		}

#endif // VERBOSE_MOVE

#ifdef VERBOSE_MOVE
		if (opCounter > 0x55c00 && entryNum == 0)
		{
			printf("(%lu == %lu) && (%d < (%d + 1)) result %d", y, y2, count, stepVal, (y == y2) && (count < (stepVal + 1)));
		}
#endif // VERBOSE_MOVE


		if ((y == y2) && (count < (stepVal + 1))) {
			dx = b1CeilFix32(x);
			dy = b1CeilFix32(y);
		}
	}

	viewTab->xPos = dx;
	viewTab->yPos = dy;

#ifdef VERBOSE_MOVE
	if (opCounter > 0x55c00 && entryNum == 0)
	{
		printf("dx is %d and dy is %d\n", dx, dy);
	}
#endif

	if (entryNum == 0) {
		bAUpdateEgoDirection(b1fpToInt(x1), b1fpToInt(y1), dx, dy, viewTab);
	}
}
#pragma wrapped-call (pop)

void bAFollowEgo(int entryNum) /* This needs to be more intelligent. */
{
	ViewTable localViewtab;

	getViewTab(&localViewtab, entryNum);

	bAAdjustPosition(&localViewtab, localViewtab.xPos, localViewtab.yPos, entryNum);

	setViewTab(&localViewtab, entryNum);
}

#pragma wrapped-call (push, trampoline, VIEW_CODE_BANK_2)
void bANormalAdjust(int entryNum, ViewTable* viewTab, int dx, int dy)
{
	int tempX, tempY, testX, startX, endX, waterCount = 0;

	tempX = (viewTab->xPos + dx);
	tempY = (viewTab->yPos + dy);

	if (entryNum == 0) {
		if (tempX < 0) {   /* Hit left edge */
			var[2] = 4;
			return;
		}
		if (tempX > (160 - viewTab->xsize)) {   /* Hit right edge */
			var[2] = 2;
			return;
		}
		if (tempY > 167) {   /* Hit bottom edge */
			var[2] = 3;
			return;
		}
		if (tempY < horizon) {   /* Hit horizon */
			var[2] = 1;
			return;
		}
	}
	else {   /* For all other objects */
		if (tempX < 0) {   /* Hit left edge */
			var[5] = 4;
			var[4] = entryNum;
			return;
		}
		if (tempX > (160 - viewTab->xsize)) {   /* Hit right edge */
			var[5] = 2;
			var[4] = entryNum;
			return;
		}
		if (tempY > 167) {   /* Hit bottom edge */
			var[5] = 3;
			var[4] = entryNum;
			return;
		}
		if (tempY < horizon) {   /* Hit horizon */
			var[5] = 1;
			var[4] = entryNum;
			return;
		}
	}

	if (entryNum == 0) {
		flag[3] = 0;
		flag[0] = 0;

		/* End points of the base line */
		startX = tempX;
		endX = startX + viewTab->xsize;
		for (testX = startX; testX < endX; testX++) {
			switch (control->line[tempY][testX]) {
			case 0: return;   /* Unconditional obstacle */
			case 1:
				if (viewTab->flags & IGNOREBLOCKS) break;
				return;    /* Conditional obstacle */
			case 3:
				waterCount++;
				break;
			case 2: flag[3] = 1; /* Trigger */
				viewTab->xPos = tempX;
				viewTab->yPos = tempY;
				return;
			}
		}
		if (waterCount == viewTab->xsize) {
			viewTab->xPos = tempX;
			viewTab->yPos = tempY;
			flag[0] = 1;
			return;
		}
	}
	else {
		/* End points of the base line */
		startX = tempX;
		endX = startX + viewTab->xsize;
		for (testX = startX; testX < endX; testX++) {
			if ((viewTab->flags & ONWATER) &&
				(control->line[tempY][testX] != 3)) {
				return;
			}
		}
	}

	viewTab->xPos = tempX;
	viewTab->yPos = tempY;
}
#pragma wrapped-call (pop)

#pragma code-name (pop)
#pragma code-name (push, "BANKRAM0B")

#pragma wrapped-call (push, trampoline, VIEW_CODE_BANK_3)
void bBUpdateObj(int entryNum)
{
	int oldX, oldY, celNum;
	word objFlags;
	ViewTable localViewtab;

	getViewTab(&localViewtab, entryNum);

	objFlags = localViewtab.flags;


	//if ((objFlags & ANIMATED) && (objFlags & DRAWN)) {

	   //if (objFlags & UPDATE) {

	if (objFlags & CYCLING) {
		localViewtab.cycleTimeCount++;
		if (localViewtab.cycleTimeCount >
			localViewtab.cycleTime) {
			localViewtab.cycleTimeCount = 1;
			celNum = localViewtab.currentCel;
			switch (localViewtab.cycleStatus) {
			case 0: /* normal.cycle */
				celNum++;
				if (celNum >= localViewtab.numberOfCels)
					celNum = 0;

				b9SetCel(&localViewtab, celNum);
				break;
			case 1: /* end.of.loop */
				celNum++;
				if (celNum >= localViewtab.numberOfCels) {
					flag[localViewtab.param1] = 1;
					/* localViewtab.flags &= ~CYCLING; */
				}
				else
					b9SetCel(&localViewtab, celNum);
				break;
			case 2: /* reverse.loop */
				celNum--;
				if (celNum < 0) {
					flag[localViewtab.param1] = 1;
					/* localViewtab.flags &= ~CYCLING; */
				}
				else
					b9SetCel(&localViewtab, celNum);
				break;
			case 3: /* reverse.cycle */
				celNum--;
				if (celNum < 0)
					celNum = localViewtab.numberOfCels - 1;
				b9SetCel(&localViewtab, celNum);
				break;
			}
		}
	} /* CYCLING */

	if (objFlags & MOTION) {
		localViewtab.stepTimeCount++;
		if (localViewtab.stepTimeCount >
			localViewtab.stepTime) {
			localViewtab.stepTimeCount = 1;
			switch (localViewtab.motion) {
			case 0: /* normal.motion */
				switch (localViewtab.direction) {
				case 0: break;
				case 1:bANormalAdjust(entryNum, &localViewtab, 0, -1); break;
				case 2:bANormalAdjust(entryNum, &localViewtab, 1, -1); break;
				case 3:bANormalAdjust(entryNum, &localViewtab, 1, 0); break;
				case 4:bANormalAdjust(entryNum, &localViewtab, 1, 1); break;
				case 5:bANormalAdjust(entryNum, &localViewtab, 0, 1); break;
				case 6:bANormalAdjust(entryNum, &localViewtab, -1, 1); break;
				case 7:bANormalAdjust(entryNum, &localViewtab, -1, 0); break;
				case 8:bANormalAdjust(entryNum, &localViewtab, -1, -1); break;
				}
				getViewTab(&localViewtab, entryNum);
				break;
			case 1: /* wander */
				oldX = localViewtab.xPos;
				oldY = localViewtab.yPos;
				switch (localViewtab.direction) {
				case 0: break;
				case 1:bANormalAdjust(entryNum, &localViewtab, 0, -1); break;
				case 2:bANormalAdjust(entryNum, &localViewtab, 1, -1); break;
				case 3:bANormalAdjust(entryNum, &localViewtab, 1, 0); break;
				case 4:bANormalAdjust(entryNum, &localViewtab, 1, 1); break;
				case 5:bANormalAdjust(entryNum, &localViewtab, 0, 1); break;
				case 6:bANormalAdjust(entryNum, &localViewtab, -1, 1); break;
				case 7:bANormalAdjust(entryNum, &localViewtab, -1, 0); break;
				case 8:bANormalAdjust(entryNum, &localViewtab, -1, -1); break;
				}
				getViewTab(&localViewtab, entryNum);
				if ((localViewtab.xPos == oldX) &&
					(localViewtab.yPos == oldY)) {
					localViewtab.direction = (rand() % 8) + 1;
				}
				break;
			case 2: /* follow.ego */
				break;
			case 3: /* move.obj */
				bAAdjustPosition(&localViewtab, localViewtab.param1,
					localViewtab.param2, entryNum);

				if ((localViewtab.xPos == localViewtab.param1) &&
					(localViewtab.yPos == localViewtab.param2)) {
					localViewtab.motion = 0;
					localViewtab.flags &= ~MOTION;
					flag[localViewtab.param4] = 1;
				}
				break;
			}
		}
	} /* MOTION */

 //} /* UPDATE */


	/* Determine priority for unfixed priorities */
	if (!(objFlags & FIXEDPRIORITY)) {
		if (localViewtab.yPos < 60)
			localViewtab.priority = 4;
		else
			localViewtab.priority = (localViewtab.yPos / 12 + 1);
	}


	if ((objFlags & ANIMATED) && (objFlags & DRAWN)) {
		/* Draw new cel onto picture\priority bitmaps */

#ifdef VERBOSE_DEBUG_BLIT
		printf("Called from update obj ");
#endif // DEBUG

	}

	setViewTab(&localViewtab, entryNum);
	show_mouse(NULL);
	stretch_sprite(agi_screen, spriteScreen, 0, 0, 640, 336);
	show_mouse(screen);
	b6ShowPicture();
}
#pragma wrapped-call (pop)

/* Called by force.update */
void bBUpdateObj2(int entryNum)
{
	int oldX, oldY, celNum;
	word objFlags;
	ViewTable localViewtab;

	getViewTab(&localViewtab, entryNum);

	objFlags = localViewtab.flags;


	//if ((objFlags & ANIMATED) && (objFlags & DRAWN)) {

	   //if (objFlags & UPDATE) {

	if (objFlags & CYCLING) {
		localViewtab.cycleTimeCount++;
		if (localViewtab.cycleTimeCount >
			localViewtab.cycleTime) {
			localViewtab.cycleTimeCount = 1;
			celNum = localViewtab.currentCel;

			setViewTab(&localViewtab, entryNum);
			switch (localViewtab.cycleStatus) {
			case 0: /* normal.cycle */
				celNum++;
				if (celNum >= localViewtab.numberOfCels)
					celNum = 0;
				b9SetCel(&localViewtab, celNum);
				break;
			case 1: /* end.of.loop */
				celNum++;
				if (celNum >= localViewtab.numberOfCels) {
					flag[localViewtab.param1] = 1;
					localViewtab.flags &= ~CYCLING;
				}
				else
					b9SetCel(&localViewtab, celNum);
				break;
			case 2: /* reverse.loop */
				celNum--;
				if (celNum < 0) {
					flag[localViewtab.param1] = 1;
					localViewtab.flags &= ~CYCLING;
				}
				else
					b9SetCel(&localViewtab, celNum);
				break;
			case 3: /* reverse.cycle */
				celNum--;
				if (celNum < 0)
					celNum = localViewtab.numberOfCels - 1;
				b9SetCel(&localViewtab, celNum);
				break;
			}
			getViewTab(&localViewtab, entryNum);
		}
	} /* CYCLING */

	if (objFlags & MOTION) {
		localViewtab.stepTimeCount++;
		if (localViewtab.stepTimeCount >
			localViewtab.stepTime) {
			localViewtab.stepTimeCount = 1;

			setViewTab(&localViewtab, entryNum);
			switch (localViewtab.motion) {
			case 0: /* normal.motion */
				switch (localViewtab.direction) {
				case 0: break;
				case 1:bANormalAdjust(entryNum, &localViewtab, 0, -1); break;
				case 2:bANormalAdjust(entryNum, &localViewtab, 1, -1); break;
				case 3:bANormalAdjust(entryNum, &localViewtab, 1, 0); break;
				case 4:bANormalAdjust(entryNum, &localViewtab, 1, 1); break;
				case 5:bANormalAdjust(entryNum, &localViewtab, 0, 1); break;
				case 6:bANormalAdjust(entryNum, &localViewtab, -1, 1); break;
				case 7:bANormalAdjust(entryNum, &localViewtab, -1, 0); break;
				case 8:bANormalAdjust(entryNum, &localViewtab, -1, -1); break;
				}
				break;
			case 1: /* wander */
				oldX = localViewtab.xPos;
				oldY = localViewtab.yPos;
				switch (localViewtab.direction) {
				case 0: break;
				case 1:bANormalAdjust(entryNum, &localViewtab, 0, -1); break;
				case 2:bANormalAdjust(entryNum, &localViewtab, 1, -1); break;
				case 3:bANormalAdjust(entryNum, &localViewtab, 1, 0); break;
				case 4:bANormalAdjust(entryNum, &localViewtab, 1, 1); break;
				case 5:bANormalAdjust(entryNum, &localViewtab, 0, 1); break;
				case 6:bANormalAdjust(entryNum, &localViewtab, -1, 1); break;
				case 7:bANormalAdjust(entryNum, &localViewtab, -1, 0); break;
				case 8:bANormalAdjust(entryNum, &localViewtab, -1, -1); break;
				}
				if ((localViewtab.xPos == oldX) &&
					(localViewtab.yPos == oldY)) {
					localViewtab.direction = (rand() % 8) + 1;
				}
				break;
			case 2: /* follow.ego */
				break;
			case 3: /* move.obj */
				bAAdjustPosition(&localViewtab, localViewtab.param1,
					localViewtab.param2, entryNum);
				if ((localViewtab.xPos == localViewtab.param1) &&
					(localViewtab.yPos == localViewtab.param2)) {
					localViewtab.motion = 0;
					localViewtab.flags &= ~MOTION;
					flag[localViewtab.param4] = 1;
				}
				break;
			}
		}
	} /* MOTION */

 //} /* UPDATE */

	/* Determine priority for unfixed priorities */
	if (!(objFlags & FIXEDPRIORITY)) {
		if (localViewtab.yPos < 60)
			localViewtab.priority = 4;
		else
			localViewtab.priority = (localViewtab.yPos / 12 + 1);
	}

	/* Draw new cel onto picture\priority bitmaps */
	/*
	agi_blit(localViewtab.celData->bmp,
	   localViewtab.xPos,
	   (localViewtab.yPos - localViewtab.ysize) + 1,
	   localViewtab.xsize,
	   localViewtab.ysize,
	   localViewtab.celData->transparency & 0x0f,
	   localViewtab.priority);
	*/
	//}

	if ((objFlags & ANIMATED) && (objFlags & DRAWN)) {
		/* Draw new cel onto picture\priority bitmaps */

#ifdef VERBOSE_DEBUG_BLIT
		printf("Called from update obj 2");
#endif // DEBUG
	}

	setViewTab(&localViewtab, entryNum);
	b6ShowPicture();
}

#pragma bss-name (push, "BANKRAM0B")
boolean prioritiesSeen[NO_PRIORITIES - 1];
#pragma bss-name (pop)

void bBUpdateObjects()
{
	int entryNum, celNum, oldX, oldY;
	byte i;
	word objFlags;
	ViewTable localViewtab;

	/* If the show.pic() command was executed, display the picture
	** with this object update.
	*/
	/*
	if (okToShowPic) {
	   okToShowPic = FALSE;
	   blit(picture, spriteScreen, 0, 0, 0, 0, 160, 168);
	} else {
	   clear(spriteScreen);
	}*/
	clear(spriteScreen);

	/******************* Place all background bitmaps *******************/
	for (entryNum = 0; entryNum < VIEW_TABLE_SIZE; entryNum++) {
		getViewTab(&localViewtab, entryNum);

		objFlags = localViewtab.flags;
		//if ((objFlags & ANIMATED) && (objFlags & DRAWN)) {
		   /* Add saved background to picture\priority bitmaps */
		//}
		setViewTab(&localViewtab, entryNum);
	}

	for (entryNum = 0; entryNum < VIEW_TABLE_SIZE; entryNum++) {
		getViewTab(&localViewtab, entryNum);
		objFlags = localViewtab.flags;

#ifdef VERBOSE_UPDATE_OBJECTS
		printf("Checking entry num %d it has objFlags of %d \n", entryNum, objFlags);
#endif // VERBOSE_UPDATE_OBJECTS

		if ((objFlags & ANIMATED) && (objFlags & DRAWN)) {

			if (objFlags & UPDATE) {

				if (objFlags & CYCLING) {

#ifdef VERBOSE_UPDATE_OBJECTS
					printf("Now inside %d\n", entryNum);
#endif // VERBOSE_UPDATE_OBJECTS

					localViewtab.cycleTimeCount++;
					if (localViewtab.cycleTimeCount >
						localViewtab.cycleTime) {
						localViewtab.cycleTimeCount = 1;
						celNum = localViewtab.currentCel;

						setViewTab(&localViewtab, entryNum);

						switch (localViewtab.cycleStatus) {
						case 0: /* normal.cycle */
							celNum++;
							if (celNum >= localViewtab.numberOfCels)
								celNum = 0;
							b9SetCel(&localViewtab, celNum);
							break;
						case 1: /* end.of.loop */
							//Debug Here
							celNum++;
							if (celNum >= localViewtab.numberOfCels) {

								flag[localViewtab.param1] = 1;
								/* localViewtab.flags &= ~CYCLING; */
							}
							else
								b9SetCel(&localViewtab, celNum);
							break;
						case 2: /* reverse.loop */
							celNum--;
							if (celNum < 0) {
								flag[localViewtab.param1] = 1;
								/* localViewtab.flags &= ~CYCLING; */
							}
							else
								b9SetCel(&localViewtab, celNum);
							break;
						case 3: /* reverse.cycle */
							celNum--;
							if (celNum < 0)
								celNum = localViewtab.numberOfCels - 1;
							b9SetCel(&localViewtab, celNum);
							break;
						}
						setViewTab(&localViewtab, entryNum);
					}
				} /* CYCLING */
			} /* UPDATE */

			/* Determine priority for unfixed priorities */
			if (!(objFlags & FIXEDPRIORITY)) {
				if (localViewtab.yPos < 60)
					localViewtab.priority = 4;
				else
					localViewtab.priority = (localViewtab.yPos / 12 + 1);
			}
		}

		setViewTab(&localViewtab, entryNum);
	}

	/* Draw all cels */

	memset(prioritiesSeen, FALSE, NO_PRIORITIES - 1);

	asm("sei");
	for (i = MAX_SPRITE_PRIORITY; i >= MIN_SPRITE_PRIORITY; i--)
	{
		if (i == MAX_SPRITE_PRIORITY || prioritiesSeen[i - MIN_SPRITE_PRIORITY - 1])
		{
			for (entryNum = 0; entryNum < VIEW_TABLE_SIZE; entryNum++) {
				getViewTab(&localViewtab, entryNum);

				objFlags = localViewtab.flags;
				if ((objFlags & ANIMATED) && (objFlags & DRAWN)) {
					/* Draw new cel onto picture\priority bitmaps */

					if (i == localViewtab.priority)
					{
						agiBlit(&localViewtab, entryNum, FALSE);
					}
					else if (i == MAX_SPRITE_PRIORITY)
					{
						prioritiesSeen[localViewtab.priority - MIN_SPRITE_PRIORITY - 1] = TRUE;
					}
				}
				setViewTab(&localViewtab, entryNum);
			}
		}
	}
	bETerminateSpriteBuffer();
	REENABLE_INTERRUPTS();

	show_mouse(NULL);
	stretch_sprite(agi_screen, spriteScreen, 0, 0, 640, 336);
	show_mouse(screen);
}

#pragma code-name (pop)
#pragma code-name (push, "BANKRAM0C")

void bCupdateObjects2()
{
	int entryNum, celNum, oldX, oldY;
	word objFlags;
	ViewTable localViewtab;

	/* Place all background bitmaps */
	for (entryNum = 0; entryNum < VIEW_TABLE_SIZE; entryNum++) {
		getViewTab(&localViewtab, entryNum);

		objFlags = localViewtab.flags;
		//if ((objFlags & ANIMATED) && (objFlags & DRAWN)) {
		   /* Add saved background to picture\priority bitmaps */
		//}

		setViewTab(&localViewtab, entryNum);
	}

	for (entryNum = 0; entryNum < VIEW_TABLE_SIZE; entryNum++) {
		getViewTab(&localViewtab, entryNum);

		objFlags = localViewtab.flags;

		if ((objFlags & ANIMATED) && (objFlags & DRAWN)) {

			if (objFlags & UPDATE) {

				if (objFlags & CYCLING) {
					localViewtab.cycleTimeCount++;
					if (localViewtab.cycleTimeCount >
						localViewtab.cycleTime) {
						localViewtab.cycleTimeCount = 1;
						celNum = localViewtab.currentCel;
						switch (localViewtab.cycleStatus) {
						case 0: /* normal.cycle */
							celNum++;
							if (celNum >= localViewtab.numberOfCels)
								celNum = 0;
							b9SetCel(&localViewtab, celNum);
							break;
						case 1: /* end.of.loop */
							celNum++;
							if (celNum >= localViewtab.numberOfCels) {
								flag[localViewtab.param1] = 1;
								localViewtab.flags &= ~CYCLING;
							}
							else
								b9SetCel(&localViewtab, celNum);
							break;
						case 2: /* reverse.loop */
							celNum--;
							if (celNum < 0) {
								flag[localViewtab.param1] = 1;
								localViewtab.flags &= ~CYCLING;
							}
							else
								b9SetCel(&localViewtab, celNum);
							break;
						case 3: /* reverse.cycle */
							celNum--;
							if (celNum < 0)
								celNum = localViewtab.numberOfCels - 1;
							b9SetCel(&localViewtab, celNum);
							break;
						}
					}
				} /* CYCLING */
	/*
				if (objFlags & MOTION) {
				   localViewtab.stepTimeCount++;
				   if (localViewtab.stepTimeCount >
					   localViewtab.stepTime) {
					  localViewtab.stepTimeCount = 1;
					  switch (localViewtab.motion) {
						 case 0: // normal.motion
							switch (localViewtab.direction) {
							   case 0: break;
							   case 1: normalAdjust(entryNum, 0, -1); break;
							   case 2: normalAdjust(entryNum, 1, -1); break;
							   case 3: normalAdjust(entryNum, 1, 0); break;
							   case 4: normalAdjust(entryNum, 1, 1); break;
							   case 5: normalAdjust(entryNum, 0, 1); break;
							   case 6: normalAdjust(entryNum, -1, 1); break;
							   case 7: normalAdjust(entryNum, -1, 0); break;
							   case 8: normalAdjust(entryNum, -1, -1); break;
							}
							break;
						 case 1: // wander
							oldX = localViewtab.xPos;
							oldY = localViewtab.yPos;
							switch (localViewtab.direction) {
							   case 0: break;
							   case 1: normalAdjust(entryNum, 0, -1); break;
							   case 2: normalAdjust(entryNum, 1, -1); break;
							   case 3: normalAdjust(entryNum, 1, 0); break;
							   case 4: normalAdjust(entryNum, 1, 1); break;
							   case 5: normalAdjust(entryNum, 0, 1); break;
							   case 6: normalAdjust(entryNum, -1, 1); break;
							   case 7: normalAdjust(entryNum, -1, 0); break;
							   case 8: normalAdjust(entryNum, -1, -1); break;
							}
							if ((localViewtab.xPos == oldX) &&
								(localViewtab.yPos == oldY)) {
							   localViewtab.direction = (rand() % 8) + 1;
							}
							break;
						 case 2: // follow.ego
							break;
						 case 3: // move.obj
							adjustPosition(entryNum, localViewtab.param1,
							   localViewtab.param2);
							if ((localViewtab.xPos == localViewtab.param1) &&
								(localViewtab.yPos == localViewtab.param2)) {
							   localViewtab.motion = 0;
							   localViewtab.flags &= ~MOTION;
							   flag[localViewtab.param4] = 1;
							}
							break;
					  }
				   }
				} // MOTION
	*/
			} /* UPDATE */

			/* Determine priority for unfixed priorities */
			if (!(objFlags & FIXEDPRIORITY)) {
				if (localViewtab.yPos < 60)
					localViewtab.priority = 4;
				else
					localViewtab.priority = (localViewtab.yPos / 12 + 1);
			}

			/* Draw new cel onto picture\priority bitmaps */
			/*
			agi_blit(localViewtab.celData->bmp,
			   localViewtab.xPos,
			   (localViewtab.yPos - localViewtab.ysize) + 1,
			   localViewtab.xsize,
			   localViewtab.ysize,
			   localViewtab.celData->transparency & 0x0f,
			   localViewtab.priority);
			*/
			setViewTab(&localViewtab, entryNum);
		}
	}
}

void bCCalcObjMotion()
{
	int entryNum, celNum, oldX, oldY, steps = 0;
	byte randomNum;
	word objFlags;
	ViewTable localViewtab;
	ViewTable localViewtab0;

	getViewTab(&localViewtab0, 0);

	for (entryNum = 0; entryNum < VIEW_TABLE_SIZE; entryNum++) {

		getViewTab(&localViewtab, entryNum);

		objFlags = localViewtab.flags;
		//Warning
		if ((objFlags & MOTION) && (objFlags & UPDATE) && (objFlags & DRAWN)) {

			localViewtab.stepTimeCount++;

			if (localViewtab.stepTimeCount >
				localViewtab.stepTime) {
				localViewtab.stepTimeCount = 1;

				switch (localViewtab.motion) {
				case 0: /* normal.motion */
					switch (localViewtab.direction) {
					case 0: break;
					case 1:bANormalAdjust(entryNum, &localViewtab, 0, -1); break;
					case 2:bANormalAdjust(entryNum, &localViewtab, 0, -1); break;
					case 3:bANormalAdjust(entryNum, &localViewtab, 1, 0); break;
					case 4:bANormalAdjust(entryNum, &localViewtab, 1, 1); break;
					case 5:bANormalAdjust(entryNum, &localViewtab, 0, 1); break;
					case 6:bANormalAdjust(entryNum, &localViewtab, -1, 1); break;
					case 7:bANormalAdjust(entryNum, &localViewtab, -1, 0); break;
					case 8:bANormalAdjust(entryNum, &localViewtab, -1, -1);
					}
					break;
				case 1: /* wander */
					oldX = localViewtab.xPos;
					oldY = localViewtab.yPos;
					switch (localViewtab.direction) {
					case 0: break;
					case 1:bANormalAdjust(entryNum, &localViewtab, 0, -1); break;
					case 2:bANormalAdjust(entryNum, &localViewtab, 0, -1); break;
					case 3:bANormalAdjust(entryNum, &localViewtab, 1, 0); break;
					case 4:bANormalAdjust(entryNum, &localViewtab, 1, 1); break;
					case 5:bANormalAdjust(entryNum, &localViewtab, 0, 1); break;
					case 6:bANormalAdjust(entryNum, &localViewtab, -1, 1); break;
					case 7:bANormalAdjust(entryNum, &localViewtab, -1, 0); break;
					case 8:bANormalAdjust(entryNum, &localViewtab, -1, -1); break;
					}
					if ((localViewtab.xPos == oldX) &&
						(localViewtab.yPos == oldY)) {
						randomNum = (byte)(rand() % 8) + 1;
						localViewtab.direction = 5;
					}
					break;
				case 2: /* follow.ego */
					bAFollowEgo(entryNum);
					if ((localViewtab.xPos == localViewtab0.xPos) &&
						(localViewtab.yPos == localViewtab0.yPos)) {
						localViewtab.motion = 0;
						localViewtab.flags &= ~MOTION;
						flag[localViewtab.param2] = 1;
						/* Not sure about this next line */
						localViewtab.stepSize = localViewtab.param1;
					}
					break;
				case 3: /* move.obj */
					if (flag[localViewtab.param4]) break;
					for (steps = 0; steps < localViewtab.stepSize; steps++) {
						bAAdjustPosition(&localViewtab,(int)localViewtab.param1,
							(int)localViewtab.param2, entryNum);
						if ((localViewtab.xPos == localViewtab.param1) &&
							(localViewtab.yPos == localViewtab.param2)) {
							/* These lines really are guess work */
							localViewtab.motion = 0;
							//localViewtab.flags &= ~MOTION;
							localViewtab.direction = 0;
							if (entryNum == 0) var[6] = 0;
							flag[localViewtab.param4] = 1;
							localViewtab.stepSize = localViewtab.param3;
							break;
						}
					}
					break;
				}
			}
		} /* MOTION */

		/* Automatic change of direction if needed */
		bACalcDirection(&localViewtab);

		setViewTab(&localViewtab, entryNum);
	}
}

#pragma code-name (pop)
#pragma code-name (push, "BANKRAM0D")

///***************************************************************************
//** showView
//**
//** Purpose: To display all the cells of VIEW.
//***************************************************************************/
//void bDShowView(int viewNum)
//{
//	int loopNum, celNum, maxHeight, totalWidth, totalHeight = 5;
//	int startX = 0, startY = 0;
//	BITMAP* temp = create_bitmap(800, 600);
//	BITMAP* scn = create_bitmap(320, 240);
//	char viewString[20], loopString[3];
//	boolean stillViewing = TRUE;
//
//	clear_to_color(temp, 15);
//
//	for (loopNum = 0; loopNum < loadedViews[viewNum].numberOfLoops; loopNum++) {
//		maxHeight = 0;
//		totalWidth = 25;
//		sprintf(loopString, "%2d", loopNum);
//		drawString(temp, loopString, 2, totalHeight, 0, 15);
//		for (celNum = 0; celNum < loadedViews[viewNum].loops[loopNum].numberOfCels; celNum++) {
//			if (maxHeight < loadedViews[viewNum].loops[loopNum].cels[celNum].height)
//				maxHeight = loadedViews[viewNum].loops[loopNum].cels[celNum].height;
//			//blit(loadedViews[viewNum].loops[loopNum].cels[celNum].bmp, temp,
//			//   0, 0, totalWidth, totalHeight,
//			//   loadedViews[viewNum].loops[loopNum].cels[celNum].width,
//			//   loadedViews[viewNum].loops[loopNum].cels[celNum].height);
//			stretch_blit(loadedViews[viewNum].loops[loopNum].cels[celNum].bmp,
//				temp, 0, 0,
//				loadedViews[viewNum].loops[loopNum].cels[celNum].width,
//				loadedViews[viewNum].loops[loopNum].cels[celNum].height,
//				totalWidth, totalHeight,
//				loadedViews[viewNum].loops[loopNum].cels[celNum].width * 2,
//				loadedViews[viewNum].loops[loopNum].cels[celNum].height);
//			totalWidth += loadedViews[viewNum].loops[loopNum].cels[celNum].width * 2;
//			totalWidth += 3;
//		}
//		if (maxHeight < 10) maxHeight = 10;
//		totalHeight += maxHeight;
//		totalHeight += 3;
//	}
//
//	if (strcmp(loadedViews[viewNum].description, "") != 0) {
//		int i, counter = 0, descLine = 0, maxLen = 0, strPos;
//		char* tempString = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];
//		char* string = loadedViews[viewNum].description;
//
//		totalHeight += 20;
//
//		for (i = 0; i < strlen(string); i++) {
//			if (counter++ > 30) {
//				for (strPos = strlen(tempString) - 1; strPos >= 0; strPos--)
//					if (tempString[strPos] == ' ') break;
//				tempString[strPos] = 0;
//				drawString(temp, tempString, 27, totalHeight + descLine, 0, 15);
//				sprintf(tempString, "%s%c", &tempString[strPos + 1], string[i]);
//				descLine += 8;
//				if (strPos > maxLen) maxLen = strPos;
//				counter = strlen(tempString);
//			}
//			else {
//				if (string[i] == 0x0A) {
//					sprintf(tempString, "%s\\n", tempString);
//					counter++;
//				}
//				else {
//					if (string[i] == '\"') {
//						sprintf(tempString, "%s\\\"", tempString);
//						counter++;
//					}
//					else {
//						sprintf(tempString, "%s%c", tempString, string[i]);
//					}
//				}
//			}
//		}
//		drawString(temp, tempString, 27, totalHeight + descLine, 0, 15);
//		rect(temp, 25, totalHeight - 2, 25 + (maxLen + 1) * 8 + 3,
//			(totalHeight - 2) + (descLine + 8) + 3, 4);
//	}
//
//	sprintf(viewString, "view.%d", viewNum);
//	rect(scn, 0, 0, 319, 9, 4);
//	rectfill(scn, 1, 1, 318, 8, 4);
//	drawString(scn, viewString, 130, 1, 0, 4);
//	rect(scn, 310, 9, 319, 230, 4);
//	rectfill(scn, 311, 10, 318, 229, 3);
//	rect(scn, 310, 230, 319, 239, 4);
//	drawChar(scn, 0xB1, 311, 231, 4, 3);
//	drawChar(scn, 0x18, 311, 11, 4, 3);
//	drawChar(scn, 0x19, 311, 222, 4, 3);
//	rect(scn, 0, 230, 310, 239, 4);
//	rectfill(scn, 1, 231, 309, 238, 3);
//	drawChar(scn, 0x1B, 2, 231, 4, 3);
//	drawChar(scn, 0x1A, 302, 231, 4, 3);
//	blit(temp, scn, 0, 0, 0, 10, 310, 220);
//	stretch_blit(scn, screen, 0, 0, 320, 240, 0, 0, 640, 480);
//
//	while (stillViewing) {
//		switch (readkey() >> 8) {
//		case KEY_ESC:
//			stillViewing = FALSE;
//			break;
//		case KEY_UP:
//			startY -= 5;
//			break;
//		case KEY_DOWN:
//			startY += 5;
//			break;
//		case KEY_LEFT:
//			startX -= 5;
//			break;
//		case KEY_RIGHT:
//			startX += 5;
//			break;
//		case KEY_PGUP:
//			startY -= 220;
//			break;
//		case KEY_PGDN:
//			startY += 220;
//			break;
//		case KEY_END:
//			startX = 489;
//			break;
//		case KEY_HOME:
//			startX = 0;
//			break;
//		}
//		if (startY < 0) startY = 0;
//		if (startY >= 380) startY = 379;
//		if (startX < 0) startX = 0;
//		if (startX >= 490) startX = 489;
//		stretch_blit(temp, screen, startX, startY, 310, 220, 0, 20, 620, 440);
//	}
//
//	destroy_bitmap(temp);
//	destroy_bitmap(scn);
//}
//
///***************************************************************************
//** showView2
//**
//** Purpose: To display AGI VIEWs and allow scrolling through the cells and
//** loops. You have to install the allegro keyboard handler to call this
//** function.
//***************************************************************************/
//void bDShowView2(int viewNum)
//{
//	int loopNum = 0, celNum = 0;
//	BITMAP* temp = create_bitmap(640, 480);
//	char viewString[20], loopString[20], celString[20];
//	boolean stillViewing = TRUE;
//
//	sprintf(viewString, "View number: %d", viewNum);
//
//	while (stillViewing) {
//		clear(temp);
//		textout(temp, font, viewString, 10, 10, 15);
//		sprintf(loopString, "Loop number: %d", loopNum);
//		sprintf(celString, "Cel number: %d", celNum);
//		textout(temp, font, loopString, 10, 18, 15);
//		textout(temp, font, celString, 10, 26, 15);
//		stretch_blit(loadedViews[viewNum].loops[loopNum].cels[celNum].bmp, temp,
//			0, 0, loadedViews[viewNum].loops[loopNum].cels[celNum].width,
//			loadedViews[viewNum].loops[loopNum].cels[celNum].height, 10, 40,
//			loadedViews[viewNum].loops[loopNum].cels[celNum].width * 4,
//			loadedViews[viewNum].loops[loopNum].cels[celNum].height * 3);
//		blit(temp, screen, 0, 0, 0, 0, 640, 480);
//		switch (readkey() >> 8) {  /* Scan code */
//		case KEY_UP:
//			loopNum++;
//			break;
//		case KEY_DOWN:
//			loopNum--;
//			break;
//		case KEY_LEFT:
//			celNum--;
//			break;
//		case KEY_RIGHT:
//			celNum++;
//			break;
//		case KEY_ESC:
//			stillViewing = FALSE;
//			break;
//		}
//
//		if (loopNum < 0) loopNum = loadedViews[viewNum].numberOfLoops - 1;
//		if (loopNum >= loadedViews[viewNum].numberOfLoops) loopNum = 0;
//		if (celNum < 0)
//			celNum = loadedViews[viewNum].loops[loopNum].numberOfCels - 1;
//		if (celNum >= loadedViews[viewNum].loops[loopNum].numberOfCels)
//			celNum = 0;
//	}
//
//	destroy_bitmap(temp);
//}

/***************************************************************************
** showObjectState
**
** This function shows the full on screen object state. It shows all view
** table variables and displays the current cell.
**
** Params:
**
**    objNum              Object number.
**
***************************************************************************/
void bDShowObjectState(int objNum)
{
	char* tempStr = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];
	BITMAP* temp = create_bitmap(640, 480);
	ViewTable localViewtab;

	getViewTab(&localViewtab, objNum);

	while (keypressed()) { /* Wait */ }

	show_mouse(NULL);
	blit(screen, temp, 0, 0, 0, 0, 640, 480);
	rectfill(screen, 10, 10, 630, 470, 0);
	rect(screen, 10, 10, 630, 470, 16);

	stretch_blit(localViewtab.celData->bmp, screen, 0, 0,
		localViewtab.xsize, localViewtab.ysize, 200, 18,
		localViewtab.xsize * 4, localViewtab.ysize * 4);

	sprintf(tempStr, "objNum: %d", objNum);
	textout(screen, font, tempStr, 18, 18, 8);
	sprintf(tempStr, "xPos: %d", localViewtab.xPos);
	textout(screen, font, tempStr, 18, 28, 8);
	sprintf(tempStr, "yPos: %d", localViewtab.yPos);
	textout(screen, font, tempStr, 18, 38, 8);
	sprintf(tempStr, "xSize: %d", localViewtab.xsize);
	textout(screen, font, tempStr, 18, 48, 8);
	sprintf(tempStr, "ySize: %d", localViewtab.ysize);
	textout(screen, font, tempStr, 18, 58, 8);
	sprintf(tempStr, "currentView: %d", localViewtab.currentView);
	textout(screen, font, tempStr, 18, 68, 8);
	sprintf(tempStr, "numOfLoops: %d", localViewtab.numberOfLoops);
	textout(screen, font, tempStr, 18, 78, 8);
	sprintf(tempStr, "currentLoop: %d", localViewtab.currentLoop);
	textout(screen, font, tempStr, 18, 88, 8);
	sprintf(tempStr, "numberOfCels: %d", localViewtab.numberOfCels);
	textout(screen, font, tempStr, 18, 98, 8);
	sprintf(tempStr, "currentCel: %d", localViewtab.currentCel);
	textout(screen, font, tempStr, 18, 108, 8);
	sprintf(tempStr, "stepSize: %d", localViewtab.stepSize);
	textout(screen, font, tempStr, 18, 118, 8);
	sprintf(tempStr, "stepTime: %d", localViewtab.stepTime);
	textout(screen, font, tempStr, 18, 128, 8);
	sprintf(tempStr, "stepTimeCount: %d", localViewtab.stepTimeCount);
	textout(screen, font, tempStr, 18, 138, 8);
	sprintf(tempStr, "cycleTime: %d", localViewtab.cycleTime);
	textout(screen, font, tempStr, 18, 148, 8);
	sprintf(tempStr, "cycleTimeCount: %d", localViewtab.cycleTimeCount);
	textout(screen, font, tempStr, 18, 158, 8);
	sprintf(tempStr, "direction: %d", localViewtab.direction);
	textout(screen, font, tempStr, 18, 168, 8);
	sprintf(tempStr, "priority: %d", localViewtab.priority);
	textout(screen, font, tempStr, 18, 178, 8);
	switch (localViewtab.motion) {
	case 0: /* Normal motion */
		sprintf(tempStr, "motion: normal motion");
		break;
	case 1: /* Wander */
		sprintf(tempStr, "motion: wander");
		break;
	case 2: /* Follow ego */
		sprintf(tempStr, "motion: follow ego");
		break;
	case 3: /* Move object */
		sprintf(tempStr, "motion: move object");
		break;
	default:
		sprintf(tempStr, "motion: unknown");
		break;
	}
	textout(screen, font, tempStr, 18, 188, 8);
	switch (localViewtab.cycleStatus) {
	case 0: /* Normal cycle */
		sprintf(tempStr, "cycleStatus: normal cycle");
		break;
	case 1: /* End of loop */
		sprintf(tempStr, "cycleStatus: end of loop");
		break;
	case 2: /* Reverse loop */
		sprintf(tempStr, "cycleStatus: reverse loop");
		break;
	case 3: /* Reverse cycle */
		sprintf(tempStr, "cycleStatus: reverse cycle");
		break;
	default:
		sprintf(tempStr, "cycleStatus: unknown");
		break;
	}
	textout(screen, font, tempStr, 18, 198, 8);

	while (!keypressed()) { /* Wait */ }

	blit(temp, screen, 0, 0, 0, 0, 640, 480);
	destroy_bitmap(temp);
	show_mouse(screen);
}

#pragma code-name (pop)
