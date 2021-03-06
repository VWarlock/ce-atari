//--------------------------------------------------
#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <support.h>

#include <stdint.h>
#include <stdio.h>

#include "stdlib.h"
#include "acsi.h"
#include "hdd_if.h"
#include "keys.h"
#include "global.h"
#include "find_ce.h"
//--------------------------------------------------

void showHomeScreen(void);
void sendKeyDown(BYTE key, BYTE keyDownCommand);
void refreshScreen(void);							
void setResolution(void);
void showConnectionErrorMessage(void);
void showMoreStreamIfNeeded(void);

BYTE atariKeysToSingleByte(BYTE vkey, BYTE key);
BYTE ce_identify(BYTE ACSI_id);

void retrieveIsUpdateScreen(char *stream);
BYTE isUpdateScreen;

void cosmoSoloConfig(void);
//--------------------------------------------------
BYTE deviceID;                          // bus ID from 0 to 7
BYTE cosmosExNotCosmoSolo;              // 0 means CosmoSolo, 1 means CosmosEx
//--------------------------------------------------

#define BUFFER_SIZE         (4*512)
BYTE myBuffer[BUFFER_SIZE];
BYTE *pBuffer;

BYTE prevCommandFailed;

//--------------------------------------------------
int main(void)
{
	DWORD scancode;
	BYTE key, vkey, res;
	WORD timeNow, timePrev;
	DWORD toEven;
	void *OldSP;
    BYTE keyDownCommand = CFG_CMD_KEYDOWN;

    isUpdateScreen = FALSE;
    
	OldSP = (void *) Super((void *)0);  			                // supervisor mode  
	
	prevCommandFailed = 0;
	
	// ---------------------- 
	// create buffer pointer to even address 
	toEven = (DWORD) &myBuffer[0];
  
	if(toEven & 0x0001)       // not even number? 
		toEven++;
  
	pBuffer = (BYTE *) toEven; 
	
	// ---------------------- 
	// search for device on the ACSI / SCSI bus 
	deviceID = 0;

	Clear_home();
    res = Supexec(findDevice);

    if(res != TRUE) {
        Super((void *)OldSP);  			      			            // user mode 
        return 0;
    }
    
    //------------------
    // if the device is CosmoSolo, go this way
    if(cosmosExNotCosmoSolo == FALSE) {
        cosmoSoloConfig();
        Super((void *)OldSP);  			      			            // user mode 
        return 0;
    }
	
    // ----------------- 
    // if the device is CosmosEx, do the remote console config
	setResolution();							                    // send the current ST resolution for screen centering 
	
	showHomeScreen();							                    // get the home screen 
	
	// use Ctrl + C to quit 
	timePrev = Tgettime();
	
	while(1) {
		res = Cconis();						                        // see if there's something waiting from keyboard 
		
		if(res == 0) {							                    // nothing waiting from keyboard? 
			timeNow = Tgettime();
			
			if((timeNow - timePrev) > 0) {		                    // check if time changed (2 seconds passed) 
				timePrev = timeNow;
				
				sendKeyDown(0, keyDownCommand);                     // display a new stream (if something changed) 
			}
			
			continue;							                    // try again 
		}

		scancode = Bconin(DEV_CONSOLE); 		                    // get char form keyboard, no echo on screen 

		vkey	= (scancode>>16)	& 0xff;
		key		=  scancode			& 0xff;

		key		= atariKeysToSingleByte(vkey, key);	                // transform BYTE pair into single BYTE

        if(key == KEY_F8) {                                         // should switch between config and linux console?
            Clear_home();                                           // clear the screen
        
            if(keyDownCommand == CFG_CMD_KEYDOWN) {                 // currently showing normal config?
                Cursor_on();                                        // turn on cursor            
                keyDownCommand = CFG_CMD_LINUXCONSOLE_GETSTREAM;    // switch to linux console
                sendKeyDown(KEY_ENTER, keyDownCommand);             // send enter to show the command line
            } else {                                                // showing linux console? 
                keyDownCommand = CFG_CMD_KEYDOWN;                   // switch to normal config
                refreshScreen();                                    // refresh the screen
            }
            
            continue;
        }
		
		if(key == KEY_F10) {						                // should quit? 
			break;
		}
		
		if(key == KEY_F5 && keyDownCommand == CFG_CMD_KEYDOWN) {    // should refresh? and are we on the config part, not the linux console part? 
			refreshScreen();
			continue;
		}
		
		sendKeyDown(key, keyDownCommand);			                // send this key to device 
	}
	
    Super((void *)OldSP);  			      			                // user mode 
	return 0;
}
//--------------------------------------------------
void sendKeyDown(BYTE key, BYTE keyDownCommand)
{
	BYTE cmd[] = {0, 'C', 'E', HOSTMOD_CONFIG, keyDownCommand, 0};
	
	cmd[0] = (deviceID << 5); 						// cmd[0] = ACSI_id + TEST UNIT READY (0)	
	cmd[5] = key;									// store the pressed key to cmd[5] 
  
	memset(pBuffer, 0, 512);               			// clear the buffer 
  
	(*hdIf.cmd)(1, cmd, 6, pBuffer, 3);             // issue the KEYDOWN command and show the screen stream 
    
	if(!hdIf.success || hdIf.statusByte != OK) {    // if failed, return FALSE 
		showConnectionErrorMessage();
		return;
	}
	
	if(prevCommandFailed != 0) {					// if previous ACSI command failed, do some recovery 
		prevCommandFailed = 0;
		
		setResolution();
		showHomeScreen();
	}
	
    retrieveIsUpdateScreen((char *) pBuffer);       // get the flag isUpdateScreen from the end of the stream
	(void) Cconws((char *) pBuffer);				// now display the buffer
    
    if(keyDownCommand == CFG_CMD_LINUXCONSOLE_GETSTREAM) {  // if we're on the linux console stream, possibly show more data
        showMoreStreamIfNeeded();                           // if there's a marker about more data, fetch it
    }
}
//--------------------------------------------------
void showMoreStreamIfNeeded(void)
{
	BYTE cmd[] = {0, 'C', 'E', HOSTMOD_CONFIG, CFG_CMD_LINUXCONSOLE_GETSTREAM, 0};
	
	cmd[0] = (deviceID << 5); 						    // cmd[0] = ACSI_id + TEST UNIT READY (0)	
	cmd[5] = 0;									        // no key pressed 
  
    while(1) {
        if(pBuffer[ (3 * 512) - 1 ] == LINUXCONSOLE_NO_MORE_DATA) {     // no more data? quit
            break;
        }
    
        memset(pBuffer, 0, 3 * 512);               	    // clear the buffer 
  
        (*hdIf.cmd)(1, cmd, 6, pBuffer, 3);             // issue the KEYDOWN command and show the screen stream 
    
        if(!hdIf.success || hdIf.statusByte != OK) {    // if failed, return FALSE 
            return;
        }

        (void) Cconws((char *) pBuffer);				// now display the buffer
    }
} 

void showHomeScreen(void)							
{
	BYTE cmd[] = {0, 'C', 'E', HOSTMOD_CONFIG, CFG_CMD_GO_HOME, 0};
	
	cmd[0] = (deviceID << 5); 						// cmd[0] = ACSI_id + TEST UNIT READY (0)	
	memset(pBuffer, 0, 512);               			// clear the buffer 
  
	(*hdIf.cmd)(1, cmd, 6, pBuffer, 3);             // issue the GO_HOME command and show the screen stream 
    
	if(!hdIf.success || hdIf.statusByte != OK) {    // if failed, return FALSE 
		showConnectionErrorMessage();
		return;
	}
	
	if(prevCommandFailed != 0) {					// if previous ACSI command failed, do some recovery 
		prevCommandFailed = 0;
		
		setResolution();
		showHomeScreen();
	}
	
    retrieveIsUpdateScreen((char *) pBuffer);       // get the flag isUpdateScreen from the end of the stream
	(void) Cconws((char *) pBuffer);				// now display the buffer
}
//--------------------------------------------------
void refreshScreen(void)							
{
	BYTE cmd[] = {0, 'C', 'E', HOSTMOD_CONFIG, CFG_CMD_REFRESH, 0};
	
	cmd[0] = (deviceID << 5); 						// cmd[0] = ACSI_id + TEST UNIT READY (0)	
	memset(pBuffer, 0, 512);               			// clear the buffer 
  
	(*hdIf.cmd)(1, cmd, 6, pBuffer, 3);             // issue the REFRESH command and show the screen stream 
    
	if(!hdIf.success || hdIf.statusByte != OK) {    // if failed, return FALSE 
		showConnectionErrorMessage();
		return;
	}
	
    retrieveIsUpdateScreen((char *) pBuffer);       // get the flag isUpdateScreen from the end of the stream
	(void) Cconws((char *) pBuffer);				// now display the buffer
}
//--------------------------------------------------
void setResolution(void)							
{
	BYTE cmd[] = {0, 'C', 'E', HOSTMOD_CONFIG, CFG_CMD_SET_RESOLUTION, 0};
	
	cmd[0] = (deviceID << 5); 						// cmd[0] = ACSI_id + TEST UNIT READY (0)	
	cmd[5] = Getrez();
	memset(pBuffer, 0, 512);               			// clear the buffer 
  
	(*hdIf.cmd)(1, cmd, 6, pBuffer, 1);              // issue the SET RESOLUTION command 
}
//--------------------------------------------------
void showConnectionErrorMessage(void)
{
	Clear_home();
    
    if(isUpdateScreen == FALSE) {
        (void) Cconws("Communication with CosmosEx failed.\n\rWill try to reconnect in a while.\n\r\n\rTo quit to desktop, press F10\n\r");
    } else {
        (void) Cconws("\33pCosmosEx device is updating...\n\rPlease wait and do not turn off!\33q\n\r");
    }
	
	prevCommandFailed = 1;
}
//--------------------------------------------------
BYTE atariKeysToSingleByte(BYTE vkey, BYTE key)
{
	WORD vkeyKey;

	if(key >= 32 && key < 127) {		// printable ASCII key? just return it 
		return key;
	}
	
	if(key == 0) {						// will this be some non-ASCII key? convert it 
		switch(vkey) {
			case 0x48: return KEY_UP;
			case 0x50: return KEY_DOWN;
			case 0x4b: return KEY_LEFT;
			case 0x4d: return KEY_RIGHT;
			case 0x52: return KEY_INSERT;
			case 0x47: return KEY_HOME;
			case 0x62: return KEY_HELP;
			case 0x61: return KEY_UNDO;
			case 0x3b: return KEY_F1;
			case 0x3c: return KEY_F2;
			case 0x3d: return KEY_F3;
			case 0x3e: return KEY_F4;
			case 0x3f: return KEY_F5;
			case 0x40: return KEY_F6;
			case 0x41: return KEY_F7;
			case 0x42: return KEY_F8;
			case 0x43: return KEY_F9;
			case 0x44: return KEY_F10;
			default: return 0;			// unknown key 
		}
	}
	
	vkeyKey = (((WORD) vkey) << 8) | ((WORD) key);		// create a WORD with vkey and key together 
	
	switch(vkeyKey) {					// some other no-ASCII key, but check with vkey too 
		case 0x011b: return KEY_ESC;
		case 0x537f: return KEY_DELETE;
		case 0x0e08: return KEY_BACKSP;
		case 0x0f09: return KEY_TAB;
		case 0x1c0d: return KEY_ENTER;
		case 0x720d: return KEY_ENTER;
        case 0x2e03: return KEY_CTRL_C;
        case 0x250b: return KEY_CTRL_K;
        case 0x260c: return KEY_CTRL_L;
        case 0x1615: return KEY_CTRL_U;
	}

	return 0;							// unknown key 
}
//--------------------------------------------------
void retrieveIsUpdateScreen(char *stream)
{
    WORD i;
    
    for(i=0; i<BUFFER_SIZE; i++) {
        if(stream[i] == 0) {            // end of stream? good
            break;
        }
    }
    
    if(i == BUFFER_SIZE) {              // reached end of buffer? not update screen (possibly)
        isUpdateScreen = FALSE;
        return;
    }
    
    if(stream[i + 1] == 1) {            // if the flag after the stream is equal to 1, then it's update screen
        isUpdateScreen = TRUE;
    } else {                            // it's not update screen
        isUpdateScreen = FALSE;
    }
}
//--------------------------------------------------
void cosmoSoloConfig(void)
{
    (void) Cconws("\n\rCurrent device ID is: ");
    Cconout('0' + deviceID);
    (void) Cconws("\n\rPlease enter new device ID (0 - 7)\n\ror any other key to quit.");
    (void) Cconws("\n\rEnter new device ID : ");
    
    BYTE key = Cnecin();
    
    if(key >= '0' && key <= '7') {
        BYTE newId = key - '0';
        BYTE cmd[] = {0, 'C', 'S', deviceID, newId, 0};
        
        cmd[0] = (deviceID << 5); 						    // cmd[0] = ACSI_id + TEST UNIT READY (0)	
  
        (*hdIf.cmd)(ACSI_READ, cmd, 6, pBuffer, 1);
        
        if(!hdIf.success || hdIf.statusByte == OK) {
            (void) Cconws("\n\rNew ID was successfully set.\n\r");
        } else {
            (void) Cconws("\n\rFailed to set new ID.\n\r");
        }
    } else {
        (void) Cconws("\n\rTerminating without setting device ID.\n\r");
    }
    
    sleep(3);
}
//--------------------------------------------------
void logMsg(char *logMsg)
{
//    if(showLogs) {
//        (void) Cconws(logMsg);
//    }
}
//--------------------------------------------------
void logMsgProgress(DWORD current, DWORD total)
{
//    (void) Cconws("Progress: ");
//    showHexDword(current);
//    (void) Cconws(" out of ");
//    showHexDword(total);
//    (void) Cconws("\n\r");
}
//--------------------------------------------------


