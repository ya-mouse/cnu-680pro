/////////////////////////////////////////////
///
///  libgigarouter.h
///  by jkglee
///  2002.08.23
///
//////////////////////////////////////////////

void* GIGA_fopen(
    char *pszFileName, 
    char* pszMode);

int GIGA_GetSystemSetting(
    char *pszSection,
    char *pszItem,
    char *pszValueBuffer,
    int nValueBufferSize,
    int nMode );
    
int GIGA_UpdateSystemSetting(
    char *pszSection,
    char *pszItem,
    char *pszValueBuffer,
    int nMode );    

int GIGA_GetSystemStatus(
    char *pszItem,
    char *pszValueBuffer,
    int nValueBufferSize );
    
int GIGA_UpdateSystemStatus(
    char *pszItem,
    char *pszValueBuffer );


// by jkglee, 2002.11.20, add semaphore function ..
int GIGA_initsem(int semkey);
int GIGA_p(int semid);
int GIGA_v(int semid);
