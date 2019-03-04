/**********************************************************/
/*   GS_OPCOD.H                                           */
/**********************************************************/

/**********************************************************/
/*   OPERATION CODES                                      */
/* AVENDO A DISPOSIZIONE FINO A 80 CODICI
/* DAL NUMERO 1 AL NUMERO 69 SI TRATTA DI CODICI OPERAZIONI DI GEOSIM
/* DAL 70 AL 79 SONO CODICI DI OPERAZIONI UTENTE
/**********************************************************/

   #define opCreatePrj       1      // Solo per superutente
   #define opDelPrj          2      // Solo per superutente
   #define opModPrj          3      // Solo per superutente
   #define opBackupPrj       4      // Solo per superutente
   #define opCopyPrj         5      // Solo per superutente
   
   #define opCreateClass     6      // Solo per superutente
   #define opDelClass        7      // Solo per superutente
   #define opModClass        8      // Solo per superutente
   #define opCopyClass       9      // Solo per superutente
	#define opDestroyClass   43      // Solo per superutente
   #define opSynchronizeClass 52    // Solo per superutente
   #define opDWGOptimization 53     // Solo per superutente
   
   #define opCreateSec      22      // Solo per superutente
   #define opDelSec         23      // Solo per superutente
   #define opModSec         24      // Solo per superutente
   #define opCopySec        25      // Solo per superutente
   #define opCreateLinkSec  26      // Solo per superutente
   #define opDelLinkSec     27      // Solo per superutente
	#define opDestroySec     44      // Solo per superutente

   #define opCreateUsr      10      // Solo per superutente
   #define opDelUsr         11      // Solo per superutente
   #define opModUsr         12      // Solo per superutente
   #define opModPwd         13

   #define opConv2ActualVer 45      // Solo per superutente

   #define opCreateArea     14 
   #define opDelArea        15
   #define opSelArea        16      // scongelamento/recupero sessione
   #define opCloseArea      17      // Esce dalla sessione
   #define opSaveArea       19      // salva i dati
   #define opSelAreaClass   21      // seleziona classi da estrarre
   #define opFreezeArea     28      // congela sessione

   #define opInsEntity      29 
   #define opModEntity      30
   #define opAggrEntity     31
   #define opDelEntity      32
   #define opFilterEntity   33
   #define opQryEntity      34
   #define opImportEntity   42
   #define opExport         48

   #define opInsSecondary   35 
   #define opModSecondary   36
   #define opDelSecondary   37
   #define opQrySecondary   38

   #define opAttEdit        39
   #define opAttInvisib     40
   #define opAttVisib       46
   #define opSetVisib       47      // Gestione set di visibilità
   #define opScaleAttrText  50

   #define opModDefTabRef   41

   #define opCreateTopo     49

   #define opWhip           51

// Last used operation code = 53