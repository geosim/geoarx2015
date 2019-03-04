/*********************************************************/
/* GS_INIT.H */
/*********************************************************/

/*********************************************************/
/* TYPEDEFS */
/*********************************************************/

#ifndef _gs_init_h
#define _gs_init_h 1


#ifndef _gs_list_h
   #include "gs_list.h"
#endif

#ifndef _gs_ase_h
   #include "gs_ase.h"
#endif

#ifndef _gs_cmd_h
   #include "gs_cmd.h"
#endif

#ifndef _gs_netw_h
   #include "gs_netw.h"
#endif

#ifndef _gs_user_h
   #include "gs_user.h"
#endif

#ifndef _gs_prjct_h
   #include "gs_prjct.h"
#endif

#ifndef _gs_setv_h
   #include "gs_setv.h"
#endif

#ifndef _gs_graph_h
   #include "gs_graph.h"
#endif

#ifndef _gs_thm_h
   #include "gs_thm.h" // per classe C_LAYER_DISPLAY_MODEL
#endif

#include "gs_waitdlg.h"


// Inizializzazioni globali di GEOSIM
//-----------------------------------------------------------------------//   
class C_INIT
{
   public:
   C_INIT();
   ~C_INIT() {}
   DllExport int get_AddEntityToSaveSet(void);
   DllExport int set_AddEntityToSaveSet(int Value);
   DllExport int get_AutoHighlight(void);
   DllExport int set_AutoHighlight(int Value);
   DllExport int get_AlignHighlightedFASOnSave(void);
   DllExport int set_AlignHighlightedFASOnSave(int Value);
   DllExport int get_AutoSyncro(void);
   DllExport int set_AutoSyncro(int Value);
   DllExport int get_AutoZoom(void);
   DllExport int set_AutoZoom(int Value);
   DllExport double get_AutoZoomMinXDim(void);
   DllExport int    set_AutoZoomMinXDim(double Value);
   DllExport double get_AutoZoomMinYDim(void);
   DllExport int    set_AutoZoomMinYDim(double Value);
   DllExport int get_BoolFmt(void);
   DllExport int set_BoolFmt(int SetBoolFmt);
   DllExport int get_DimAssoc(void);
   DllExport int set_DimAssoc(int Value);
   DllExport int get_DynamicExtraction(void);
   DllExport int set_DynamicExtraction(int Value);
   DllExport int get_InsXScale(void);
   DllExport int set_InsXScale(int SetInsXScale);
   DllExport int get_InsYScale(void);
   DllExport int set_InsYScale(int SetInsYScale);
   DllExport int get_InsRotaz(void);
   DllExport int set_InsRotaz(int SetRotaz);
   DllExport int get_InsHText(void);
   DllExport int set_InsHText(int SetHText);
   DllExport int get_InsPos(void);
   DllExport int set_InsPos(int SetPos);
   DllExport int get_LogFile(void);
   DllExport int set_LogFile(int SetHText);
   DllExport int get_NumTest(void);
   DllExport int set_NumTest(int Value);
   DllExport int get_ShortDate(void);
   DllExport int set_ShortDate(int SetShortDate);
   DllExport int get_UpdGraphOnExtract(void);
   DllExport int set_UpdGraphOnExtract(int Value);
   DllExport int get_UpdGraphOnExtractSim(void);
   DllExport int set_UpdGraphOnExtractSim(int Value);
   DllExport int get_AddLPTOnExtract(void);
   DllExport int set_AddLPTOnExtract(int Value);
   DllExport int get_WaitTime(void);
   DllExport int set_WaitTime(int Value);
   DllExport int get_SelectPreviousExtractedClasses(void);
   DllExport int set_SelectPreviousExtractedClasses(int Value);

   DllExport int Load(TCHAR *pathfile = NULL);
   DllExport int Save(TCHAR *pathfile = NULL);
   DllExport int Copy(C_INIT &Dest);

   private:
   int AddEntityToSaveSet;   // se = GS_GOOD si inseriscono le entità nuove, modificate,
                             // cancellate nel gruppo di selezione del salvataggio GEOsimAppl::SAVE_SS
   int AddLPTOnExtract;      // se = GS_GOOD aggiunge agli oggetti grafici estratti e 
                             // collegati a DB un link tipo LPT di ASE (in fase di
                             // salvataggio sarà rimosso)
   int AlignHighlightedFASOnSave; // se = GS_GOOD aggiorna la grafica degli oggetti
                                  // evidenziati (es. dopo il filtro) in fase di salvataggio
   int AutoHighlight;        // se = GS_GOOD il pannello della sessione di lavoro
                             // evidenzia le entità correntemente selezionate
   int AutoSyncro;           // se = GS_GOOD parte il riallinemaento in automatico
                             // su rettore InputContextReactor all'evento beginQuiescentState
                             // (in pratica quando AutoCAD è in stato "quiete")
   int AutoZoom;             // se = GS_GOOD il pannello della sessione di lavoro
                             // fa lo zoom sulle entità correntemente selezionate
   double AutoZoomMinXDim;   // Dimensione minima X della finestra di zoom
   double AutoZoomMinYDim;   // Dimensione minima Y della finestra di zoom
   int BoolFmt;              // Formato valore booleano:
                             // se = 0 -> Sì/No
                             // se = 1 -> Vero/Falso
                             // se = 2 -> On/Off
                             // se = 3 -> 1/0
   int DimAssoc;             // Modalità di quotatura impostata nella sessione di lavoro
                             // se = 0 -> Esplosa
                             // se = 1 -> Non associativa
                             // se = 2 -> Associativa
   int DynamicExtraction;    // se = GS_GOOD su pan o zoom estrae automaticamente 
                             // la grafica
   int ins_htext;            // altezza testo automatica (AUTO) o manuale (MANUAL)
                             // se AUTO altezza testo = valore default della classe
   int ins_pos;              // posizione automatica (AUTO) o manuale (MANUAL)
                             // se AUTO allora per oggetti:
                             // Oggetti puntuali = punto di inserimento
                             // Oggetti lineari = punto medio tratto più lungo
                             // Oggetti superficie = centroide
   int ins_rotaz;            // rotazione automatica (AUTO) o manuale (MANUAL)
                             // se AUTO rotazione = valore default della classe
   int ins_x_scale, ins_y_scale; // scala x e y automatica (AUTO) o manuale (MANUAL)
                                 // se AUTO scala x = scala y = valore default della classe
                                 // "ins_y_scale" è considerata se "ins_x_scale" = MANUAL
   int logfile;              // se = GS_GOOD scrive messaggi in file LOG di GEOsim, 
                             // MAP e AutoCAD
   int NumTest;              // tentativi da fare prima di segnalare l'errore
   int ShortDate;            // se = GS_GOOD data breve atrimenti data estesa
                             // il formato è letto dalle impostazioni di windows
   int UpdGraphOnExtract;    // se = GS_GOOD aggiorna la grafica leggendo i dati
                             // dai database in fase di estrazione
   int UpdGraphOnExtractSim; // se = GS_GOOD aggiorna la grafica delle classi simulazioni 
                             // leggendo i dati dai database in fase di estrazione
   int WaitTime;             // secondi di attesa fra due tentativi
   int SelectPreviousExtractedClasses; // se = GS_GOOD alla creazione di una nuova sessione di lavoro
							   		         // seleziona le classi estratte nella sessione precedente
};


class GEOsimAppl
{
   private:
   
   static int init_dir(void);

   public:

	GEOsimAppl();
   ~GEOsimAppl();

	static int init(void);                        // Inizializzazione applicazione
   static int initByUser(void);                  // Inizializzazione applicazione per un 
                                                 // utente specifico
   static void init_cmd_vector(void);            // Inizializza il vettore comandi di GEOsim
                                                 // abilitabili/disabilitabili
   static int terminate(void);                   // Termina applicazione
   static int restore_netpath(void);             // Carica gli alias di rete

   static void TerminateSQL(C_DBCONNECTION *pConn); // Termina una connessione SQL
 
   static CAseAppl *ASE;                         // Riferimento all'ambiente ASE
   static CAsiAppl *ASI;                         // Riferimento all'ambiente ASI
	static C_DBCONNECTION_LIST DBCONNECTION_LIST; // Lista delle connessioni OLE-DB di GEOsim
   static C_RB_LIST ASE_APP_ID_LIST;             // Lista descrittiva per obj-data di ASE
   static C_RB_LIST APP_ID_LIST;                 // Lista descrittiva per obj-data identificativa
                                                 // delle  entità di GEOsim
   static C_STR_LIST OD_TABLENAME_LIST;          // Lista dei nomi delle tabelle OD definite
   static C_CMDLIST CMDLIST;                     // Lista dei comandi di GEOsim
   static struct cmd_code GS_COMMANDS[GS_COMMANDS_LEN]; // Vettore dei comandi di GEOsim
                                                 // abilitabili/disabilitabili
	static C_STRING GEODIR;                       // Directory banca dati centrale
	static C_STRING WORKDIR;                      // Directory di lavoro locale
	static C_STRING CURRUSRDIR;                   // Directory locale dell'utente corrente
	static C_INIT GLOBALVARS;                     // Variabili di ambiente GEOsim lette dal file GEOSIM.INI
   static C_ALIAS_LIST GS_NET_LIST;              // Lista degli alias di rete
	static C_LIST_PRJ PROJECTS;                   // Lista dei progetti di GEOsim
   static C_USER GS_USER;                        // Utente corrente
   static C_SELSET SAVE_SS;                      // Gruppo di selezione entità da salvare nella sessione di lavoro
                                                 // dall'ultimo riallineamento
   static C_SELSET REFUSED_SS;                   // Gruppo di selezione degli oggetti rifiutati
                                                 // dall'ultimo riallineamento
   static C_SET_VISIB_LIST *ACTIVE_VIS_ATTRIB_SET;  // Set di visibilità degli attributi attivo
   static int LAST_CLS;                          // Ultima classe di entità usata
   static int LAST_SUB;                          // Ultima sottoclasse di entità usata

   static double TOLERANCE;                      // Tolleranza nel confronto tra coordinate

   static C_DYNAMIC_QRY_ATTR *DYNAMIC_QRY_ATTR;  // Puntatore C_DYNAMIC_QRY_ATTR per 
                                                 // l'interrogazione dinamica
   static C_LAYER_DISPLAY_MODEL LAYER_DISPLAY_MODEL;  // C_LAYER_DISPLAY_MODEL 
                                                      // per il display model dei layer
   static AcRx::AppMsgCode LastAppMsgCode;
   static C_STRING LAST_STARTED_CMD;

   //static C_WAIT_DLG_INSTANCE *pWAIT_DLG_INSTANCE;
   static C_WAIT_DLG_INSTANCE WAIT_DLG_INSTANCE;
};


extern    int gs_GetVar(void);
extern    int gs_SetVar(void);
DllExport resbuf *gsc_GetVar(TCHAR *string);
DllExport int gsc_SetVar(TCHAR *string, resbuf *rb);

extern int  gsc_init_for_user();


/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/
                             

DllExport C_INIT* get_GS_GLOBALVAR();
DllExport C_USER *get_CURRENT_USER();
DllExport CAseAppl *get_ASE();
DllExport C_DBCONNECTION_LIST* get_pDBCONNECTION_LIST();
DllExport TCHAR *get_GEODIR();
DllExport TCHAR *get_WORKDIR();
DllExport TCHAR *get_CURRUSRDIR();
DllExport double get_TOLERANCE();
DllExport int gsc_initByUser(); // per caterina (modulo report)

int gs_get_savess(void);
void gsevidsavess(void);
int gsc_addSS2savess(C_SELSET &SelSet);
int gsc_addSS2savess(ads_name ss);
int gsc_addEnt2savess(ads_name ent);
int gs_reset_SAVE_SS(void);
DllExport C_SELSET* get_SAVE_SS();
DllExport C_SELSET* get_REFUSED_SS();
DllExport int get_LAST_CLS();
DllExport int get_LAST_SUB();
DllExport C_LAYER_DISPLAY_MODEL *get_LAYER_DISPLAY_MODEL();

int gs_get_ssrefused(void);
void gsevidssrefused(void);

void gsRestoreDBConnection(void);

#endif