#ifndef _UPDATE_H
#define _UPDATE_H

#include <map>

#include "global.h"
#include "version.h"

#define UPDATE_REMOTEURL    "http://joo.kie.sk/cosmosex/update/updatelist.csv"
#define UPDATE_LOCALPATH    "/tmp"
#define UPDATE_LOCALLIST    "/tmp/updatelist.csv"
#define UPDATE_SCRIPT       "/ce/update/doupdate.sh"
#define UPDATE_APP_PATH     "/ce/app"

#define UPDATE_USBFILE      "ce_update.zip"

#define UPDATE_STATE_IDLE           0
#define UPDATE_STATE_DOWNLOADING    1
#define UPDATE_STATE_DOWNLOAD_OK    2
#define UPDATE_STATE_DOWNLOAD_FAIL  3

#define REPORT_URL          "http://joo.kie.sk/cosmosex/update/report.php"

class Update
{
public:
    static Versions versions;
	
    static void initialize(void);
    static void deleteLocalUpdateComponents(void);
    static void downloadUpdateList(const char *remoteUrl);
    static void processUpdateList(void);

    static void downloadNewComponents(void);

    static int  state(void);
    static void stateGoIdle(void);
    static void stateGoDownloadOK(void);

    static bool createUpdateScript(void);
    static bool createFlashFirstFwScript(bool withLinuxRestart);
    static bool createUpdateXilinxScript(void);
    
    static bool checkForUpdateListOnUsb(std::string &updateFilePath);

    static void createNewScripts(void);                 // this does script update - blocking other processing
    static void createFloppyTestImage(void);

    static void startPackageDownloadIfAnyComponentNewer(void);
    
private:
    static int currentState;
    static const char *getPropperXilinxTag(void);
};

#endif // CCORETHREAD_H
