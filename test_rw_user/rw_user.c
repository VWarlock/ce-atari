#include <time.h>
#define FALSE      0
/* ------------------------------------------ */
BYTE *buffOUT;
int WriteSector(DWORD Sector, BYTE count);
void dumpBuffer(BYTE *buf, int offset);
int inquiry(void);
	DWORD scancode;
	(void) Cconws("\33E");
	for(i=0; i<BUFFER_SIZE; i++)
			j = WriteSector(0, 1);
			if(j != 0) {
		}
	return 0;


	return res;
	if(res != 0)
	for(i=0; i<(512 * count); i++) {
	return 0;
DWORD superGetTicks(void)