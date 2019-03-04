/*************************************************************************/
/*   GS_LOCK.H                                                           */
/*************************************************************************/


#ifndef _gs_lock_h
#define _gs_lock_h 1


/*
L' oggetto della classe C_LOCK è composto da tre elementi:

1.    Nome Autocad dell' entità(Object).
2.    Handle dell' entità nel dwg originale.
3.    Nome del dwg originale.

Le principali funzioni di accesso a questa classe sono:

1.    is_locked(Object, N_area):
         Questa funzione dovrà, attraverso le funzioni ADE ricavare l'handle
         originale dell' entità passata come parametro, e dovrà fare una 
         interrogazione del database GS_LOCKED cercando quest' handle con il
         rispettivo numero di area di lavoro, e a seconda della presenza nel 
         database restituirà vero o falso.
         Nel caso che trovasse già il record con l' handle cercata il numero 
         di area determinerà la modificabilità dell' oggetto.
         
2.    set_locked(Object, N_area):
         Questa funzione dovrà, attraverso le funzioni ADE ricavare l'handle
         originale dell' entità passata come parametro, e quindi inserirà un 
         nuovo record in GS_LOCKED, con l'area di lavoro passata.
 
3.    set_unlocked(Object, N_area):
         Questa funzione dovrà, attraverso le funzioni ADE ricavare l'handle
         originale dell' entità passata come parametro, e quindi cancellera il 
         record in GS_LOCKED, con l'area di lavoro passata.
*/

class C_LOCK
{
   private:
      
      ads_name Object;                                 // Nome dell' entità
      int      cls;                                    // Codice classe
      int      sub;                                    // Codice sottoclasse
      TCHAR    id[MAX_LEN_HANDLE + DWG_FILE_NAME_LEN - 1]; // handle + filename per spaghetti
                                                       // codice chiave per altre classi
      
      int      Status;                                 // stato del lock: FREE, NEW,
                                                       // TEMPORARY_LOCKED, PERMANENT_LOCKED
      long     Session;                                // Codice sessione di appartenenza
                                                       // (-1 se la sessione non esiste più)
      C_STRING DateTime;                               // Data  e ora del lock

      int FindForId(_RecordsetPtr &pRs, bool *IsRecordsetCloseable);              
      int get_info(int *Found);              

   public:

      C_LOCK();              // Costruttore
      C_LOCK(ads_name _ent);
     ~C_LOCK() { };          // Distruttore    

      int set_locked(ads_name obj, int *result, int mode = TEMPORARY_LOCKED);              
      int set_locked(int *result, int mode = TEMPORARY_LOCKED);              
      int set_unlocked(ads_name obj);            
      int is_locked(ads_name entita, int *result, int OnlyByOther = GS_GOOD);
      int is_locked(int *result, int OnlyByOther = GS_GOOD);
      int WhoHasIt(ads_name entity, int *result, C_STRING *handenti = NULL,
                   C_STRING *filename = NULL);

      // accesso ai membri
      int  get_cls(void);
      int  get_sub(void);
      long  get_session(void);
      TCHAR *get_DateTime(void); 
      int  get_status(void);
      void set_ClsSubId(int ClsCode, int SubCode, const TCHAR *IdCode);    
      void set_ClsSubId(int ClsCode, int SubCode, long IdCode);    
      int  set_ent(ads_name _ent);    
};        


int gsc_unlockallObjs(C_PROJECT *pPrj, C_WRK_SESSION *pSession);
void gsddwhohasit(void);
int gsc_whohasit(ads_name entity, C_ID *ClassIdentif = NULL, long *nArea = NULL,
                 C_STRING *DateTimeLock = NULL, C_STRING *handenti = NULL,
                 C_STRING *filename = NULL);
long gsc_setSelSetLocked(ads_name GrSelection, int mode = TEMPORARY_LOCKED,
                         int CounterToVideo = GS_BAD);

int gsc_SubstractObjectLocked(C_SELSET *SStoFilter);

int gsc_PrepareSelLockWhereKey(C_DBCONNECTION *pConn, C_STRING &TableRef, _CommandPtr &pCmd);
                                                                           
#endif