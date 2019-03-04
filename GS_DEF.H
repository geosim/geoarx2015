/*************************************************************************/
/*   GS_DEF.H                                                            */
/*************************************************************************/

#ifndef _gs_def_h
#define _gs_def_h 1

#ifndef _ACED_H
   #include <aced.h>
#endif

// Estensioni dei files usate da GEosim
// rst      = file delle regole per la visita topologica, contiene come calcolare la resistenza al passaggio
// qry      = file di query, contiene condizioni spaziali e/o SQL (come i file di AutoCAD MAP)
// leg      = file di legenda, contiene i parametri per il fltro tematico
// sns      = file per ricerca per filtro a sensori, contien i parametri per la ricerca a sensori
// contour  = file per il calcolo delle curve di livello
// ldm      = file per la definizione di un Layer Dispaly Model (modello di visualizzazione di un piano)
// lyr      = file contenente la configurazione di un piano (layer)
// qgs      = file di progetto Quantum GIS
// gs_x_qgs = file di configuerazione per la creazione di un progetto QGIS a prtore dalla banca dati GS

#define   FALSE              0
#define   TRUE               1

#define   GS_EMPTYSTR        _T("")
#define   GS_EOS             _T('\0')
#define   GS_LFSTR           _T("\n")

#define   GS_BAD               0      // non cambiare questa macro !!!
#define   GS_GOOD              1
#define   GS_CAN               2

#define   NORECURSIVE          0
#define   RECURSIVE            1

#define   LITTLE_STEP         10
#define   MEDIUM_STEP        100
#define   LARGE_STEP        1000
#define   VERY_LARGE_STEP  10000

#define   ENV_GEO_DIR   _T("GEOSIM")  // nome variabile di sistema contenente
                                      // direttorio di GEOsim       
#define   ENV_WORK_DIR  _T("GEOWORK") // nome variabile di sistema contenente
                                      // direttorio di lavoro       

#define   GEO_PROFILE_NAME _T("geosim2015") // nome del profilo caricato con GEOsim


#define   ELEMENTS(array) (sizeof(array)/sizeof((array)[0]))

#define   PI        3.14159265358979323846

#define   UI_LOADFILE_FLAGS      4 + 16 // 1 = Indicates a request to create a new file
#define   UI_SAVEFILE_FLAGS      1 + 4  // 4 = Enables the user to enter an arbitrary filename extension
                                        // 16 = Interprets the second argument to acedGetFileD() as a path or directory name, not a filename

#define ads_2Dpoint_set(from, to) (*(to) = *(from), (to)[1] = (from)[1], (to)[2] = 0)
#define ads_point_set_from_AcGePoint2d(from, to) ((to)[0] = (from).x, (to)[1] = (from).y, (to)[2] = 0)
#define ads_point_set_from_AcGePoint3d(from, to) ((to)[0] = (from).x, (to)[1] = (from).y, (to)[2] = (from).z)
#define ads_2Dpoint_set_from_AcGePoint3d(from, to) ((to)[0] = (from).x, (to)[1] = (from).y, (to)[2] = 0)
#define AcGePoint3d_set_from_ads_point(from, to) ((to).x = (from)[0], (to).y = (from)[1], (to).z = (from)[2])
#define AcGePoint2d_set_from_ads_point(from, to) ((to).x = (from)[0], (to).y = (from)[1])

#define ads_point_clear(in) (*(in) = 0, (in)[1] = 0, (in)[2] = 0)

#define Xor(in1, in2)  (in1 && !in2 || !in1 && in2)

#define gsc_min(in1, in2)  ((in1 < in2) ? in1 : in2)
#define gsc_max(in1, in2)  ((in1 > in2) ? in1 : in2)

#define ads_2Dpoint_equal(in1, in2)  ((in1)[0] == (in2)[0] && \
                                      (in1)[1] == (in2)[1])
#define ads_point_equal(in1, in2)  ((in1)[0] == (in2)[0] && \
                                    (in1)[1] == (in2)[1] && \
                                    (in1)[2] == (in2)[2])
#define gsc_num_equal(in1, in2) (((in1 > in2) ? in1-in2 : in2-in1) <= get_TOLERANCE())

#define gsc_2Dpoint_equal(in1, in2) ((((in1)[0] > (in2)[0]) ? (in1)[0]-(in2)[0] : (in2)[0]-(in1)[0]) <= get_TOLERANCE() && \
                                     (((in1)[1]>(in2)[1]) ? (in1)[1]-(in2)[1] : (in2)[1]-(in1)[1]) <= get_TOLERANCE())
#define gsc_point_equal(in1, in2) ((((in1)[0] > (in2)[0]) ? (in1)[0]-(in2)[0] : (in2)[0]-(in1)[0]) <= get_TOLERANCE() && \
                                   (((in1)[1] > (in2)[1]) ? (in1)[1]-(in2)[1] : (in2)[1]-(in1)[1]) <= get_TOLERANCE() && \
                                   (((in1)[2] > (in2)[2]) ? (in1)[2]-(in2)[2] : (in2)[2]-(in1)[2]) <= get_TOLERANCE())

#define gsc_time_diff(t1, t2) ((t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000))


/* LUNGHEZZA MASSIMA MESSAGGI DI STAMPA */
#define   MAX_LEN_MSG  255 + 1 

/* DEFINIZIONE NOME APPLICAZIONE GEOsim PER EED */
#define   GEO_APP_ID _T("GS")

/* DEFINIZIONE NOME APPLICAZIONE ACAD PER ASE */
#define   ASE_APP_ID _T("DCO15")

/* DEFINIZIONI PER GESTIONE DEI REGISTRI DI ACAD */
#define   MAX_LEN_ENTRY        128 + 1
#define   MAX_LEN_VALUE        128 + 1
#define   MAX_LEN_SECTION      4096
#define   MAX_LEN_DRVNAME      MAX_LEN_ENTRY // DA CANCELLARE

/* DEFINIZIONI PER GESTIONE DATABASE */ 
#define   ACCESS_DATASOURCE    _T("Microsoft Access 2010.UDL")
#define   GEOSIM_DATASOURCE    ACCESS_DATASOURCE
#define   ENVIR_PREFIX         _T("GEOSIM_")
#define   CLASS_ENVIR_PREFIX   _T("GEOSIM_CLASS_")
#define   MAX_LEN_FIELDNAME    64 + 1 // DIMENSIONE MAX. IN ACCESS
#define   MAX_LEN_FIELDTYPE    24 + 1
#define   MAX_LEN_FIELD        254 + 1

#define   PG_DBMSNAME          _T("POSTGRESQL")
#define   ORACLE_DBMSNAME      _T("ORACL")   // oracle diretto ritorna "ORACLE", via odbc ritorna "ORAC"
#define   ACCESS_DBMSNAME      _T("MS JET")
#define   SQLSERVER_DBMSNAME   _T("SQLSERVER") // da verificare
#define   MYSQL_DBMSNAME       _T("MYSQL")     // da verificare

// DEFINIZIONI PER INDICI TABELLE
enum IndexTypeEnum
{
   INDEX_UNIQUE           = 0,
   INDEX_NOUNIQUE         = 1,
   SPATIAL_INDEX_UNIQUE   = 2,
   SPATIAL_INDEX_NOUNIQUE = 3,
   PRIMARY_KEY            = 4
};


/* DEFINIZIONI DIMENSIONI CAMPI IN ACCESS 2003 */
#define   ACCESS_MAX_LEN_FIELDNUMERIC     19 
#define   ACCESS_MAX_LEN_FIELDNUMERIC_DEC 8 
#define   ACCESS_MAX_LEN_FIELDCHAR        255  
#define   ACCESS_MAX_LEN_FIELDDATE        10  

/* DEFINIZIONI PER GESTIONE PARAMETRI SQL (IN/OUT) */
#define   MAX_LEN_PARAMNAME    254 + 1

/* DEFINIZIONI PER GESTIONE SCRIPT SQL */
#define   MAX_LEN_SCRIPTROW    254 + 1
#define   MAX_LEN_SQL_STM      254 + 1
                
/* DEFINIZIONI PER I FORMATI DI IMPORT\EXPORT DATI */
#define   DELIMITED            1
#define   SDF_HEADER           2
#define   SDF                  3

/* DEFINIZIONI PER LA GESTIONE DEI TENTATIVI IN CASO DI ERRORI */
#define   ONETEST              0   // solo un tentativo
#define   MORETESTS            1   // più tentativi

/* DEFINIZIONI PER LA GESTIONE DEI RESBUF */
typedef struct resbuf *presbuf;

// DEFINIZIONI PER ARRAY DI AcDbEntity
typedef AcArray<AcDbEntity*> AcDbEntityPtrArray;

// LUNGHEZZA MASSIMA NOME TABELLA DATI OGGETTO
#define   MAX_LEN_ODTABLE_NAME  25

/* DEFINIZIONI PER LA GESTIONE UTENTI */
#define   CODE_ID                1  
#define   LOG_ID                 2
#define   PWD_ID                 3
#define   MODE_ID                4
#define   FLAGS_ID               5
#define   FREE_ID                6
#define   DEFAULT_LOGIN          GS_EMPTYSTR
#define   DEFAULT_PASSWORD       GS_EMPTYSTR

// Valori di default usati da gsInitGeopwd()
#define   MAX_LEN_LOGIN          31 + 1   
#define   MAX_LEN_PWD            31 + 1
#define   LEN_MODE               1
#define   LEN_FLAGS              10

// Nome del file .ACCDB vuoto usato come "campione" per generare
// altri database per ACCESS
#define   ACCESSSAMPLEDB _T("MICROSOFT ACCESS 2010 SAMPLE.ACCDB")

// Nome del file .ACCDB contenente la lista dei progetti
#define   ACCESSGEOMAINDB     _T("GEOsim.ACCDB")
// Nome del file .ACCDB usato come "campione" per generare 
// il database contenente la lista dei progetti
#define   ACCESSGEOSIMMAINSAMPLEDB _T("ACCESS 2010 SAMPLE GEOSIM.ACCDB")

// Nome del file .ACCDB contenente le tabelle del progetto (nel caso di ACCESS)
#define   ACCESSGEOPRJDB       _T("GS_PRJ.ACCDB")
// Nome del catlogo contenente le tabelle del progetto (nel caso di POSTGIS o ORACLE)
#define   GEOPRJCATALOG       _T("GS_PRJ")
// Nome del file .ACCDB usato come "campione" per generare 
// il database contenente le tabelle del progetto
#define   ACCESSGEOPRJSAMPLEDB _T("ACCESS 2010 SAMPLE GS_PRJ.ACCDB")

// Nome del file .ACCDB contenente le tabelle del progetto
#define   GEOWRKSESSIONDB      _T("GS_WRKSESSION.ACCDB")
// Nome del file .ACCDB usato come "campione" per generare 
// il database contenente le tabelle della sessione di lavoro
#define   ACCESSGEOWRKSESSIONSAMPLEDB _T("ACCESS 2010 SAMPLE GS_WRKSESSION.ACCDB")

// Nome del file .ACCDB contenente le coordinate dei files dwf
#define   ACCESSGEOWHIPDB       _T("GS_WHIP.ACCDB")
// Nome del file .ACCDB usato come "campione" per generare 
// il database contenente le coordinate dei files dwf
#define   ACCESSGEOWHIPSAMPLEDB _T("ACCESS 2010 SAMPLE GS_WHIP.ACCDB")

#define   GEOPWD        _T("GEOPWD")      // Nome del file per login & password utenti
                                          // abilitazione utenti
#define   GEOCOM        _T("GEOCOM")      // File comandi utente

#define   GEOUSRDIR     _T("USR")         // subdirectory di GEOsimAppl::GEODIR e del progetto per i files
                                          // di abilitazione utenti ai dati di GEOsim
#define   GEOTHMDIR     _T("THM")         // subdirectory di GEOsimAppl::GEODIR per i tematismi
#define   GEOEXT        _T("EXT")         // subdirectory di GEOsimAppl::GEODIR per le simulazioni
#define   GEODATALINKDIR _T("DATA LINKS") // subdirectory di GEOsimAppl::GEODIR per i files di Data Links 
#define   GEOTEMPDIR    _T("TEMP")        // subdirectory di GEOsimAppl::WORKDIR per files temporanei
#define   GEOQRYDIR     _T("QRY")         // subdirectory del progetto per i files di query
#define   GEOVISDIR     _T("VIS")         // subdirectory del progetto per i set di visibilità
#define   GEOSELCLSDIR  _T("SEL")         // subdirectory del progetto per la selezione classi
#define   GEOSUPPORTDIR _T("SUPPORT")     // subdirectory del progetto per i files di supporto (vari)
#define   GEOTEMPSESSIONDIR _T("TEMP")    // subdirectory temporanea della sessione di lavoro
#define   GEOOLDSESSIONDIR  _T("OLD")     // subdirectory old della sessione di lavoro
#define   GEOCUSTOMDIR  _T("CUSTOM")      // subdirectory GEOsimAppl::GEODIR per i files di personalizzazione
#define   GEOLISPDIR    _T("LISP")        // subdirectory GEOsimAppl::WORKDIR per i files lisp
#define   GEODCLDIR     _T("DCL")         // subdirectory GEOsimAppl::WORKDIR per i files dcl
#define   GEOSLIDEDIR   _T("SLIDE")       // subdirectory GEOsimAppl::WORKDIR per i files sld
#define   GEOSAMPLEDIR  _T("SAMPLE")      // subdirectory GEOsimAppl::GEODIR per i files "campione"
#define   GEODOCDIR     _T("DOCUMENTATION") // subdirectory GEOsimAppl::WORKDIR per i files di documentazione


#define   GEOEXTERN  _T("EXTERN.GEO")  // file dei tipi di classi simulazioni di GEOsim
#define   GEOTLINE   _T("GEOSIM")      // file dei tipi linea di GEOsim (.LIN)
#define   GEOHATCH   _T("ACAD")        // file dei riempimenti di GEOsim (.PAT)
#define   GEOTHM     _T("GEOTHM.DWG")  // file del sottodirettorio thm con blocchi,tipilinea e stili testo GEOsim
#define   USRTHMS    _T("USRTHM*.DWG") // files del sottodirettorio thm con blocchi,tipilinea e stili testo utente

#define   GEOPROTDWG _T("SAMPLE.DWG")   // file dwg prototipo per la creazione di un disegno nuovo

#define   GS_INI_FILE _T("GEOSIM.INI") // file di inizializzazione di GEOsim in GEOsimAppl::WORKDIR
#define   GS_INI_LABEL _T("GEOsim")    // Nome della sezione di Geosim in GS_INI_FILE
#define   GS_NET_LABEL _T("GEOsim Net Nodes") // Nome della sezione di Geosim Net nei registri di ACAD
#define   ADE_NET_LABEL _T("Aliases")         // Nome della sezione di Ade    Net in ADE.INI
#define   GS_FOR_FILTER _T("GS_4FILT") // nome della tabella temporanea su cui
                                       // effettuare il filtro (deve
                                       // essere lungo 8 per non entrare in 
                                       // conflitto con i nomi di tabelle 
                                       // lunghi max.7)
#define   GS_VERIFIED _T("GS_VERIF") // nome della tabella temporanea contenente le
								             // entità gruppi di cui si è già verificata l'esistenza
#define   GS_SCR_UNFREEZE _T("UNFREEZE.SCR") // nome del file (+ estensione) script per lo 
                                             // scongelamento o il recupero di un'area di lavoro
#define   GS_SCRIPT _T("GS.SCR")     // nome di file (+ estensione) script generico
#define   GS_SCR_EXIT _T("EXIT")     // nome di file script per l'uscita da una sessione
#define   GS_LOG _T("GEOSIM.LOG")    // nome del file di log
#define   GS_GENERIC_DCL   _T("GS.DCL")         // nome del file con dcl generiche
#define   GEO_SAVESS_FILE  _T("SAVESS.GEO")     // nome del file contenente il gruppo di selezione
                                                // GEOsimAppl::SAVE_SS delle entità modificate
#define   GEO_SSDEL_FILE   _T("DELSS.GEO")      // nome del file contenente il gruppo di selezione
                                                // GEOsimAppl::SAVE_SS delle entità cancellate               
#define   GEO_LSFILTER_FILE _T("LSFILTER.GEO")  // nome del file contenente il link-set
                                                // GS_LSFILTER delle entità filtrate               

#define   GS_ALTER_FAS_OBJS_FILE _T("SSMODFAS.GEO") // nome del file contenente il gruppo di 
                                                    // selezione degli oggetti a cui è stata
                                                    // variata la FAS (tramite evidenziazione risultato
                                                    // filtro o estrazione con FAS default/utente)

#define   GS_NOTIFY_FILE _T("NOTIFY.GEO")       // nome del file contenente la lista dei computer a cui 
                                                // spedire delle notifiche in GEOsimAppl::GEODIR
#define   GS_NOTIFY_BATCH_FILE _T("GS_MESSENGER.BAT") // nome del file batch per notifica in GEOsimAppl::WORKDIR

#define   GS_LISP_FILE _T("GSL.GSL")           // nome del file di default contenente le 
                                               // funzioni lisp per GEOsim

#define   MAX_LEN_COMMANDNAME    15    // Max lunghezza comando utente

#define   INITIAL_PWD    _T("geo")
#define   SUPER_USER     _T('s')
#define   FREE_USER      _T('#')

typedef unsigned char BYTE;

#define   LICENSE_PASSWORD _T("Te lo meni")  // password di crittografazione

/* FILE SIMULAZIONE */
#define   SIM_FILE      _T("SIMUL.GEO")

/* DEFINIZIONI PATH */                                
#define   DWG_FILE_NAME_LEN    8 + 1 + 3 + 1 // "12345678.123"

/* DEFINIZIONI TEMATISMI */
#define   COLOR_BYLAYER         256
#define   COLOR_BYBLOCK         0
#define   LINETYPE_BYLAYER      _T("BYLAYER")
#define   LINETYPE_BYBLOCK      _T("BYBLOCK")

#define   SYMBOL_NAME_LEN       254
#define   MAX_LEN_LAYERNAME     SYMBOL_NAME_LEN + 1
#define   MAX_LEN_BLOCKNAME     SYMBOL_NAME_LEN + 1
#define   MAX_LEN_LINETYPENAME  SYMBOL_NAME_LEN + 1
#define   MAX_LEN_TEXTSTYLENAME SYMBOL_NAME_LEN + 1
#define   MAX_LEN_HATCHNAME     SYMBOL_NAME_LEN + 1
#define   MAX_LEN_DIMSTYLENAME  SYMBOL_NAME_LEN + 1

/* DEFINIZIONI TABELLE GEOsim */
#define   MAX_LEN_COORDNAME    SYMBOL_NAME_LEN + 1
#define   KEY_TYPE_ENUM        adSmallInt
#define   LEN_KEY_ATTR         5
#define   DEFAULT_KEY_ATTR     _T("GS_ID")  // non cambiare questa macro !!!

/* DEFINIZIONI PER TABELLE CLASSI */
#define   CLASS_KEY_TYPE_ENUM  adInteger
#define   CLASS_LEN_KEY_ATTR   10

#define   DEFAULT_TEXT_ATTR    _T("TEXT")
#define   LEN_TEXT_ATTR        30

#define   DEFAULT_DWGCOORD     GS_EMPTYSTR
#define   DEFAULT_DBGRAPHCOORD _T("-1")
#define   DEFAULT_TEXTSTYLE    _T("STANDARD")
#define   DEFAULT_LINETYPE     _T("BYLAYER")
#define   DEFAULT_COLOR        COLOR_BYLAYER // BYLAYER
#define   DEFAULT_BLOCK        _T("GEOSIM")
#define   DEFAULT_LAYER        _T("0")
#define   DEFAULT_HATCH        GS_EMPTYSTR
#define   DEFAULT_DIMSTYLE     _T("STANDARD")

/* DEFINIZIONE PER GESTIRE I DWG */
#define   DEFAULT_DWG_DIM       3

/* DEFINIZIONI PER TABELLA LISTA DEI PROGETTI */
#define   PRJS_TABLE_NAME                 _T("GS_PRJ")
#define   MAX_LEN_PRJNAME                 ACCESS_MAX_LEN_FIELDCHAR + 1

/* DEFINIZIONI PER TABELLA LISTA DEI SRID */
#define   SRID_TABLE_NAME                 _T("GS_SRID")

/* DEFINIZIONI PER TABELLA LISTA DELLE CLASSI */
#define   CLASSES_TABLE_NAME              _T("GS_CLASS")
#define   MAX_LEN_CHECKNAME               ACCESS_MAX_LEN_FIELDCHAR + 1
#define   MAX_LEN_CLASSNAME               MAX_LEN_CHECKNAME - LEN_KEY_ATTR

/* DEFINIZIONI PER TABELLA LISTA SESSIONI DI LAVORO */
#define   WRKSESSIONS_TABLE_NAME          _T("GS_WRKSESSION")
#define   MAX_LEN_WRKSESSION_NAME         ACCESS_MAX_LEN_FIELDCHAR + 1
#define   MAX_LEN_CHECK                   4  + 1
#define   MAX_LEN_CHECKER                 4  + 1

/* DEFINIZIONI PER TABELLA INFORMAZIONI SULLE CARATTERISTICHE GRAFICHE */
/* DI DEFAULT DELLE CLASSI */
#define   GRAPH_CLASS_TABLE_NAME          _T("GS_CLASS_GRAPH")

/* DEFINIZIONI PER TABELLA INFORMAZIONI SULLA SORGENTE DATI GRAFICA DELLE CLASSI */
#define   CLASS_GPH_DATA_SRC_TABLE_NAME   _T("GS_CLASS_GRAPH_INFO")

/* DEFINIZIONI PER TABELLA LISTA TABELLE SECONDARIE */
#define   SECONDARIES_TABLE_NAME          _T("GS_SEC")

/* DEFINIZIONI PER TABELLA CONNESSIONI */
#define   CONNECTIONS_TABLE_NAME          _T("GS_CONCT")

/* DEFINIZIONI PER TABELLA DETTAGLI ATTRIBUTI */
#define   ATTRIBUTES_TABLE_NAME           _T("GS_ATTR")
#define   BACKUP_ATTRIBUTES_TABLE_NAME    _T("GS_ATTRB")
#define   GRAPH_ATTRIBUTES_TABLE_NAME     _T("GS_ATTR_GRAPH")
#define   BACKUP_GRAPH_ATTRIBUTES_TABLE_NAME _T("GS_ATTR_GRAPHB")

/* DEFINIZIONI PER TABELLA RELAZIONI GRUPPI */
#define   GROUPS_TABLE_NAME               _T("GS_GROUP")

/* DEFINIZIONI PER TABELLA PROTOTIPI DEI LAYER */
#define   LAYER_MODEL_TABLE_NAME          _T("GS_LAYER_MODEL")

/* DEFINIZIONI PER TABELLA SET DI CLASSI */
#define   SET_CLASS_TABLE_NAME            _T("GS_SET_CLASS")
#define   SET_CLASS_TREE_TABLE_NAME       _T("GS_SET_CLASS_TREE")

/* DEFINIZIONI PER TABELLA LOCK */
#define   LOCKS_TABLE_NAME                _T("GS_LOCK")

/* DEFINIZIONI PER TABELLA LINK GRAFICA-DATABASE */
#define   GS_LINK                         _T("GS_LINK")

/* DEFINIZIONI PER TABELLA ENTITA' REALMENTE CANCELLATE */
#define   GS_DELETED                      _T("GS_DELETED")

#define   MAX_LEN_HANDLE                  16 + 1

/* DEFINIZIONI PER CATEGORIE DATI GEOsim */
#define   CAT_SIMPLEX                     1  // SEMPLICE
#define   CAT_GROUP                       2  // GRUPPO
#define   CAT_GRID                        4  // GRIGLIA
#define   CAT_EXTERN                      5  // SIMULAZIONE
#define   CAT_SUBCLASS                    6  // SOTTOCLASSE
#define   CAT_SPAGHETTI                   7  // SPAGHETTI
#define   CAT_SECONDARY                   8  // TABELLE SECONDARIE

/* DEFINIZIONI PER CLASSI GEOsim */
#define   TYPE_POLYLINE                   1  // POLILINEA
#define   TYPE_TEXT                       2  // TESTO
#define   TYPE_NODE                       3  // NODO
#define   TYPE_SURFACE                    4  // SUPERFICIE
#define   TYPE_GRID                       1  // GRIGLIA
#define   TYPE_GROUP                      1  // GRUPPO
#define   TYPE_SPAGHETTI                  0  // SPAGHETTI (tipo misto)

/* DEFINIZIONI COLLEGAMENTI PER CLASSI GEOsim */
#define   NO_CONCT_CONTROL                0  // SENZA CONTROLLO DI CONNESSIONI
#define   CONCT_START_END                 1  // INIZIO-FINE DI UNA POLILINEA
#define   CONCT_MIDDLE                    2  // INTERMEDIO DI UNA POLILINEA
#define   CONCT_POINT                     4  // PUNTO DI UN BLOCCO, TESTO
#define   CONCT_VERTEX                    8  // VERTICE DI UNA POLILINEA
#define   CONCT_ANY_POINT                16  // QUALSIASI PUNTO
#define   CONCT_CENTROID                 32  // CENTROIDE DI UNA SUPERFICIE
#define   NO_OVERLAP                     64  // SENZA SOVRAPPOSIZIONE
#define   SOFT_SPLIT                    128  // SPEZZA LA LINEA AGGREGANDO LE 2 LINEE
#define   HARD_SPLIT                    256  // SPEZZA LA LINEA CREANDO 2 LINEE DISTINTE

/* DEFINIZIONI PER VALIDITA' E CALCOLO ATTRIBUTI GEOsim */
#define   MAX_LSP_FUNCNAME                ACCESS_MAX_LEN_FIELDCHAR + 1

// NUMERO DECIMALE IN CORRISPONDENZA DEL BIT DI MODIFICABILITA' ATTRIBUTO
#define   BIT_MOD_VALUES_LIST             65536
#define   BIT_MOD_CAPTION                 32768
#define   BIT_MOD_VIS_ON_TOOLTIP          16384
#define   BIT_MOD_INPUT_MASK              8192
#define   BIT_MOD_GPH_PREFIX_SUFFIX       4096
#define   BIT_MOD_VALIDATION_ERR_MSG      2048
#define   BIT_MOD_DESCR                   1024
#define   BIT_DEL_ATTR                    512
#define   BIT_MOD_NAME                    256
#define   BIT_MOD_TYPE                    128
#define   BIT_MOD_LEN                     64
#define   BIT_MOD_DEC                     32
#define   BIT_MOD_VIS                     16
#define   BIT_MOD_MAND                    8
#define   BIT_MOD_CALC                    4
#define   BIT_MOD_VALID                   2
#define   BIT_MOD_DEF                     1

/* DEFINIZIONI PER SALVATAGGIO DELLE CLASSI SU FILE-PROFILE */ 
#define   SEZ_PROFILE_LEN        30
//                                 class_id   , sub, cat, type,  class_name      abil, sel,   usr_cmds
#define   ID_PROFILE_LEN         LEN_KEY_ATTR+1 +2+1 +1+1 +1+1 +MAX_LEN_CLASSNAME +1+1 +1+1 + ACCESS_MAX_LEN_FIELDCHAR
//                               color   hatch               line                   style                   layer               block               width elevation,scale,rotation,h_text,thickness,dir_dwg,vis_attrib,coord,  file_ref_block          ref_block           file_graph_calc
#define   FAS_PROFILE_LEN        3 + 1 + MAX_LEN_HATCHNAME + MAX_LEN_LINETYPENAME + MAX_LEN_TEXTSTYLENAME + MAX_LEN_LAYERNAME + MAX_LEN_BLOCKNAME + 6*(10+1) + ACCESS_MAX_LEN_FIELDCHAR + MAX_LEN_FIELDNAME + MAX_LEN_COORDNAME + ACCESS_MAX_LEN_FIELDCHAR + MAX_LEN_BLOCKNAME + ACCESS_MAX_LEN_FIELDCHAR
//                               dir                     driver            key_attrib          last
#define   INFO_PROFILE_LEN       ACCESS_MAX_LEN_FIELDCHAR + MAX_LEN_DRVNAME + MAX_LEN_FIELDNAME + 10
#define   GRID_PROFILE_LEN       6*(10+1) // 6 numeri da 10 con un carattere per la ","
#define   INT_PROFILE_LEN        30
#define   INT_INT_PROFILE_LEN    50
#define   ATTRIB_PROFILE         _T(".A")
//                               nome                tipo                4*...               file di calcolo e verifica          + default
#define   ATTRIB_PROFILE_LEN     MAX_LEN_FIELDNAME + MAX_LEN_FIELDTYPE + 4*INT_PROFILE_LEN + 2*ACCESS_MAX_LEN_FIELDCHAR + 2*MAX_LSP_FUNCNAME + MAX_LEN_FIELD
#define   GS_CLASS_FILE            _T("CLASS.GEO")        // classi estratte nella sessione di lavoro
#define   CLASSES_LIST_SEZ         _T("CLASSES_LIST")     // sezione in GS_CLASS_FILE
#define   LAST_SITUATION           _T("LAST_SITUATION")   // sezione in GS_CLASS_FILE
#define   SECONDARIES_LIST_SEZ     _T("SECONDARIES_LIST") // sezione in GS_CLASS_FILE
#define   WRK_SESSIONS_AREAS_SEZ   _T("WRK_SESSIONS_AREAS_SEZ") // sezione in GS_CLASS_FILE

/* DEFINIZIONI LIVELLI DI UTENZE */ 
#define   SUPERUSR                        0			   // non cambiare questa macro !!!
#define   NORMALUSR                       1			   // non cambiare questa macro !!!

/* DEFINIZIONI LIVELLI DI ABILITAZIONI */ 
#define   READONLY                        1  // In sola lettura
#define   UPDATEABLE                      2  // Aggiornabile
#define   EXCLUSIVEBYANOTHER              3  // Usata in modo esclusivo da un'altro utente

/* DEFINIZIONI LIVELLI DI VISIBILITA' */ 
#define   INVISIBLE                       0  // Non cambiare questa macro che viene usata
                                             // insieme a READONLY e UPDATEABLE per abilitazioni a classi e progetti
#define   VISIBLE                         1

/* DEFINIZIONI LIVELLI DI SELEZIONE */ 
#define   DESELECTED                      0  // CLASSE DESELEZIONATA
#define   SELECTED                        1  // CLASSE SELEZIONATA PER ESTRAZIONE
#define   EXTRACTED                       2  // CLASSE GIA' ESTRATTA
#define   EXTR_SEL                        3  // CLASSE SELEZIONATA PER ESTRAZIONE
											            // E GIA' ESTRATTA
/* DEFINIZIONI MODALITA' DI ESTRAZIONE */ 
#define   PREVIEW                         1
#define   EXTRACTION                      2
#define   REPORT                          4

// DEFINIZIONE DE NOME DELLA QUERY SPAZIALE ADE DI DEFAULT
#define ADE_SPATIAL_QRY_NAME _T("spaz_estr")

/* DEFINIZIONI PER TABELLE SECONDARIE */
enum SecondaryTableTypesEnum
{
   GSExternalSecondaryTable = 0,
   GSInternalSecondaryTable = 1
};

/* DEFINIZIONI PER GESTIONE NODI RETE */
#define   MAX_LEN_HOSTNAME                254 + 1

/* DEFINIZIONI DEI SISTEMI OPERATIVI */
/* I CODICI DEI SISTEMI OPERATIVI NON POSSONE ESSERE UGUALI A GS_BAD */ 
enum OperatingSystemsEnum
{
   GS_OS_UNKNOWN         = 0,
   GS_OS_DOS             = 1,
   GS_OS_WINDOWS         = 2,
   GS_OS_WINDOWS_NT_2000 = 3,
   GS_OS_UNIX            = 4,
   GS_OS_WINDOWS_95_98   = 5,
   GS_OS_WINDOWS_XP      = 6,
   GS_OS_WINDOWS_VISTA   = 7
};

                                        
/* DEFINIZIONI DELLO STATUS (per SESSIONI DI LAVORO) */
#define   WRK_SESSION_ACTIVE              1
#define   WRK_SESSION_FREEZE              2
#define   WRK_SESSION_WAIT                3
#define   WRK_SESSION_SAVE                4
#define   WRK_SESSION_EXTR                5

#define   MAX_WRK_SESSION_PARAMNUMBER     3

#define   RECORD                          1        // non cambiare questa macro !

/* DEFINIZIONI DI OGGETTI */
#define   GRAPHICAL                       0        // non cambiare questa macro !
#define   DA_BLOCK                        1
#define   ALL                             2		   // non cambiare questa macro !
#define   GROUP                           3
#define   MAX_LEN_GRAPH_TYPE             30 + 1

/* DEFINIZIONI DEGLI STATI DELLE ENTITA' */
/* Non usare numeri negativi (da 0 a 8 sono da usare in modo esclusivo ) */
#define   UNMODIFIED                      1
#define   MODIFIED                        2
#define   ERASED                          4
#define   INSERTED                        8
#define   UNERASED                       16
#define   NOSAVE                         32 // flag a bit per evidenziare che l'entità
                                            // NON deve essere salvata

// FUNZIONI DI CALCOLO ATTRIBUTI DIRETTAMENTE DALLA GRAFICA
// Quando si inserisce una nuova funzione ricordarsi di aggiornare 
// il vettore VectorFromGraphGEOLispFunctions
#define   GS_LISP_AREA                    _T(".AREA")
#define   GS_LISP_BLOCKNAME               _T(".BLOCKNAME")
#define   GS_LISP_CENTROID                _T(".CENTROID")
#define   GS_LISP_COLOR                   _T(".COLOR")
#define   GS_LISP_ELEVATION               _T(".ELEVATION")
#define   GS_LISP_EPOINT                  _T(".EPOINT")
#define   GS_LISP_END_WIDTH               _T(".EWIDTH")
#define   GS_LISP_FINAL_NODE              _T(".FINAL_NODE")        // PER GRAFI
#define   GS_LISP_HEIGHT                  _T(".HEIGHT")
#define   GS_LISP_INITIAL_NODE            _T(".INITIAL_NODE")      // PER GRAFI
#define   GS_LISP_INS_POINT               _T(".INS_POINT")
#define   GS_LISP_LAYER                   _T(".LAYER")
#define   GS_LISP_LENGTH                  _T(".LENGTH")
#define   GS_LISP_LINETYPE                _T(".LINETYPE")
#define   GS_LISP_ROTATION                _T(".ROTATION")          // in radianti
#define   GS_LISP_ROTATION_DEGREE         _T(".ROTATION_DEGREE")   // in gradi
#define   GS_LISP_SPOINT                  _T(".SPOINT")
#define   GS_LISP_STYLE                   _T(".STYLE")
#define   GS_LISP_START_WIDTH             _T(".SWIDTH")
#define   GS_LISP_THICKNESS               _T(".THICKNESS")
#define   GS_LISP_WINDOW                  _T(".WINDOW")
#define   GS_LISP_X_EPOINT                _T(".X_EPOINT")
#define   GS_LISP_X_INS_POINT             _T(".X_INS_POINT")
#define   GS_LISP_X_SPOINT                _T(".X_SPOINT")
#define   GS_LISP_Y_EPOINT                _T(".Y_EPOINT")
#define   GS_LISP_Y_INS_POINT             _T(".Y_INS_POINT")
#define   GS_LISP_Y_SPOINT                _T(".Y_SPOINT")
#define   GS_LISP_Z_EPOINT                _T(".Z_EPOINT")
#define   GS_LISP_Z_INS_POINT             _T(".Z_INS_POINT")
#define   GS_LISP_Z_SPOINT                _T(".Z_SPOINT")


// FUNZIONI DI CALCOLO PER MODIFICARE LA GRAFICA
// Quando si inserisce una nuova funzione ricordarsi di aggiornare
// il vettore VectorToGraphGEOLispFunctions
#define   GS_LISP_CHANGEBLOCK             _T("CHANGEBLOCK")
#define   GS_LISP_CHANGECOLOR             _T("CHANGECOLOR")
#define   GS_LISP_CHANGECOLORATTR         _T("CHANGECOLORATTR")
#define   GS_LISP_CHANGEDYNBLOCKPROP      _T("CHANGEDYNBLOCKPROP")
#define   GS_LISP_CHANGEELEV              _T("CHANGEELEV")
#define   GS_LISP_CHANGEHATCH             _T("CHANGEHATCH")
#define   GS_LISP_CHANGEHTEXT             _T("CHANGEHTEXT")
#define   GS_LISP_CHANGEHTEXTATTR         _T("CHANGEHTEXTATTR")
#define   GS_LISP_CHANGELAYER             _T("CHANGELAYER")
#define   GS_LISP_CHANGELAYERATTR         _T("CHANGELAYERATTR")
#define   GS_LISP_CHANGELINETYPE          _T("CHANGELINETYPE")
#define   GS_LISP_CHANGEROTATION          _T("CHANGEROTATION")
#define   GS_LISP_CHANGESCALE             _T("CHANGESCALE")
#define   GS_LISP_CHANGETEXTSTYLE         _T("CHANGETEXTSTYLE")
#define   GS_LISP_CHANGETEXTSTYLEATTR     _T("CHANGETEXTSTYLEATTR")
#define   GS_LISP_CHANGETHICKNESS         _T("CHANGETHICKNESS")
#define   GS_LISP_CHANGEWIDTH             _T("CHANGEWIDTH")
#define   GS_LISP_MOVEATTR                _T("MOVEATTR")
#define   GS_LISP_MOVESHEET               _T("MOVESHEET")
#define   GS_LISP_VISATTR                 _T("VISATTR")
#define   GS_LISP_VISSHEET                _T("VISSHEET")


/* DEFINIZIONI DI MODALITA' DI OPERAZIONI */
#define   MANUAL                          0
#define   AUTO                            1

/* DEFINIZIONI TIPOLOGIA TABELLA */
#define   TEMP                            0
#define   OLD                             1
#define   SUPPORT                         2    // tabelle di appoggio

/* DEFINIZIONI VARIE */
#define DISPLAY_LIST_STEP                 19 // intervallo di richiesta premi un tasto

/* DEFINIZIONI PER ASE */
#define MAX_LPN_LEN                       30 + 1
#define MAX_DOR_LEN                       254 + 1

/* DEFINIZIONI PER L'EXPORT E L'IMPORT DI FUNZIONI E DATI PER LA DLL */
#define DllExport _declspec(dllexport)
#define DllImport _declspec(dllimport)

/* DEFINIZIONI VARIE PER GESTIONE COMANDI */ 
#define MAX_COMMAND_LEN                   90
#define MAX_LEN_DESCR_CMD                 50 + 1
#define GS_COMMANDS_LEN                   28

/* DEFINIZIONI CODICI OPERAZIONI */ 
#define NONE                              0
#define INSERT                            1
#define MODIFY                            2
#define ERASE                             3
#define INSERT_FIRST_DABLOCK              4
#define QUERY                             5
#define MODIFY_ON_STRUCT_UPD              6

/* PER IL LOCK AGLI OGGETTI */
#define FREE                              0
#define TEMPORARY_LOCKED                  1
#define NEW                               2
#define PERMANENT_LOCKED                  3

/* PER LA GESTIONE DEI DISEGNI ATTACCATI */
#define ACTIVATE                          1

/* PER LA PERSONALIZZAZIONE ("VIRTUALIZZAZIONE") DELLE FUNZIONI GEOSIM */
/* DEFINIZIONE PER IL MOMENTO DELLA CHIAMATA */
#define STR_BEFORE                        _T("BEFORE")
#define STR_AFTER                         _T("AFTER")
#define STR_SUBST                         _T("SUBST")

#define STR_INSERT                        _T("INSERT")
#define STR_UPDATE                        _T("UPDATE")
#define STR_ERASE                         _T("ERASE")

/* DEFINIZIONI POSSIBILI CAUSE DI MODIFICA OGGETTI GEOSIM */ 
#define UNKNOWN_MOD                       1
#define GEOMETRY_MOD                      2
#define FAS_MOD                           4
#define RECORD_MOD                        8
#define NO_EXTERN_ACTION                 16
#define ERASE_MOD                        32

/* DEFINIZIONI POSSIBILI LINGUE COMANDI GEOSIM */ 
#define LAN_ITALIAN                       1
#define LAN_BRITISH                       2
#define LAN_SPANISH                       3
#define LAN_FRENCH                        4
#define LAN_GERMAN                        5

// DEFINIZIONI PER LE FORME SPAZIALI
#define ALL_SPATIAL_COND               _T("all")
#define BUFFERFENCE_SPATIAL_COND       _T("bufferfence")
#define CIRCLE_SPATIAL_COND            _T("circle")
#define FENCE_SPATIAL_COND             _T("fence")
#define FENCEBUTSTARTEND_SPATIAL_COND  _T("fencebutstartend")
#define WINDOW_SPATIAL_COND            _T("window")
#define POINT_SPATIAL_COND             _T("point")
#define POLYGON_SPATIAL_COND           _T("polygon")
#define POLYLINE_SPATIAL_COND          _T("polyline")

// DEFINIZIONI DELLE MODALITA' DI SELEZIONE CON LE FORME SPAZIALI
#define INSIDE      0
#define CROSSING    1

// DEFINIZIONI DEGLI OPERATORI LOGICI TRA 2 INSIEMI
#define UNION        0
#define SUBTRACT_A_B 1
#define SUBTRACT_B_A 2
#define INTERSECT    3

// DEFINIZIONI DELLE MODALITA DI CREAZIONE TOPOLOGIE
#define TOPO_NODE         1
#define TOPO_NETWORK      2
#define TOPO_POLYGON      3

// DEFINIZIONI DEI TIPI E SOTTOTIPI OGGETTO DI PULIZIA NELLE MAPPE
// TIPI
#define SHORT_OBJ    1
#define CROSS        2
#define UNDERSHOOT   4
#define DUPLICATES   8
#define CLUSTER     16
#define PSEUDO      32
#define DANGLING    64
// SOTTOTIPI
#define DEG_ENT      1
#define SHORT_ENT    2
#define SHORT_SEGM   3
#define VERT_2_SEG   4
#define VERT_2_VERT  5

// DEFINIZIONI DELLE OPERAZIONI DI MANUTENZIONE SUI DWG 
enum DwgMaintenanceOpEnum
{
   REINDEX     =  1,
   UNLOCKOBJS  =  2,
   SAVEEXTENTS =  4,
   PURGE       =  8,
   RECOVER     = 16
};


enum ValuesListTypeEnum
{
   NOEXIST = 0,
   TAB     = 1,   // lista di scelta obbligata a due colonne 
   REF     = 2,   // lista di scelta a due colonne
   FDF     = 3,   // lista di scelta obbligata ad una colonna
   DEF     = 4    // lista di scelta ad una colonna
};

// DEFINIZIONI DELLE MODALITA' OPERATIVE DI ALTERAZIONE 
// DELLA GRAFICA DELLE ENTITA'
enum GraphSettingsEnum
{
   GSNoneSetting          = 0,
   GSLayerSetting         = 1,
   GSBlockNameSetting     = 2,
   GSTextStyleSetting     = 4,
   GSBlockScaleSetting    = 8,
   GSLineTypeSetting      = 16,
   GSWidthSetting         = 32,
   GSColorSetting         = 64,
   GSTextHeightSetting    = 128,
   GSHatchNameSetting     = 256,
   GSThicknessSetting     = 512,
   GSElevationSetting     = 1024,
   GSRotationSetting      = 2048,
   GSLineTypeScaleSetting = 4096,
   GSHighlightSetting     = 8192,
   GSZoomSetting          = 16384,
   GSSaveAlteredObjs      = 32768,
   GSBlinkingSetting      = 65536,
   GSHatchLayerSetting    = 131072,
   GSHatchColorSetting    = 262144,
   GSHatchScaleSetting    = 524288,
   GSHatchRotationSetting = 1048576,
   GSDimensionStyleSetting = 2097152,
   GSDABlockLayerSetting   = 4194304
};


// DEFINIZIONI DELLE UNITA' DI MISURA DEGLI ANGOLI 
// (non cambiare i valori esistenti perchè c'è la funzione CHANGEROTATION che fa riferimento a questi codici)
enum RotationMeasureUnitsEnum
{
   GSNoneRotationUnit            = 0,
   GSClockwiseDegreeUnit         = 1,
   GSCounterClockwiseDegreeUnit  = 2,
   GSClockwiseRadiantUnit        = 3,
   GSCounterClockwiseRadiantUnit = 4,
   GSClockwiseGonsUnit           = 5,
   GSCounterClockwiseGonsUnit    = 6,
   GSTopobaseGonsUnit            = 7
};


// DEFINIZIONI DELLE SORGENTI GRAFICHE
enum GraphDataSourceEnum
{
   GSNoneGphDataSource = 0,
   GSDwgGphDataSource  = 1,
   GSDBGphDataSource   = 2
};


// DEFINIZIONI DEI MODI DI INTERAZIONE CON L'UTENTE
enum UserInteractionModeEnum
{
   GSNoInteraction     = 0,
   GSTextInteraction   = 1,
   GSWindowInteraction = 2
};


// DEFINIZIONI DELLE MODALITA' DI BACKUP E RESTORE
enum BackUpModeEnum
{
   GSCreateBackUp  = 1,
   GSRemoveBackUp  = 2,
   GSRestoreBackUp = 3
};


// DEFINIZIONI DELLE DIMENSIONI GEOMETRICHE
enum GeomDimensionEnum
{
   GS_2D = 2,
   GS_3D = 3
};


// DEFINIZIONI DEI TIPI DI SEGMENTAZIONE DINAMICA
enum DynamicSegmentationTypeEnum
{
   GSNoneDynSegmentation     = 0,
   GSPunctualDynSegmentation = 1,
   GSLinearDynSegmentation   = 2
};


// DEFINIZIONI DEI TIPI DI COPIA DATI
enum DataCopyTypeEnum
{
   GSStructureAndDataCopyType = 0,
   GSStructureOnlyCopyType    = 1,
   GSDataLinkCopyType         = 2
};
   

// DEFINIZIONI DEI TIPI DI OGGETTI DEL MODELLO DATI DI GEOSIM
enum GSDataModelObjectTypeEnum // non cambiare queste macro
{
   GSNone         = -1,
   GSProject      = 0,
   GSClassSet     = 1,
   GSClass        = 2,
   GSSubClass     = 3,
   GSSecondaryTab = 4 
};


// DEFINIZIONI DEGLI IDENTIFICATORI DI BITMAP
enum GSBitmapTypeEnum
{
   GSAscendingOrderBmp_8X8        = 0,
   GSDescendingOrderBmp_8X8       = 1,
   GSExtractedLockedBmp_32X16     = 2,
   GSExtractedLockedBmp_16X16     = 3,
   GSExtractedUnLockedBmp_32X16   = 4,
   GSExtractedUnLockedBmp_16X16   = 5,
   GSUnextractedLockedBmp_32X16   = 6,
   GSUnextractedLockedBmp_16X16   = 7,
   GSUnextractedUnLockedBmp_32X16 = 8,
   GSUnextractedUnLockedBmp_16X16 = 9,

   GSGridMaskBmp_16X16            = 10,
   GSGridMaskBmp_32X16            = 11,
   GSGroupMaskBmp_16X16           = 12,
   GSGroupMaskBmp_32X16           = 13,
   GSNodeMaskBmp_16X16            = 14,
   GSNodeMaskBmp_32X16            = 15,
   GSPolylineMaskBmp_16X16        = 16,
   GSPolylineMaskBmp_32X16        = 17,
   GSSimulationMaskBmp_16X16      = 18,
   GSSimulationMaskBmp_32X16      = 19,
   GSSpaghettiMaskBmp_16X16       = 20,
   GSSpaghettiMaskBmp_32X16       = 21,
   GSSurfaceMaskBmp_16X16         = 22,
   GSSurfaceMaskBmp_32X16         = 23,
   GSTextMaskBmp_16X16            = 24,
   GSTextMaskBmp_32X16            = 25
};

// DEFINIZIONI DEI TIPI DI SISTEMI DI COORDINATE
enum GSSRIDTypeEnum
{
   GSSRID_None     = 0,
   GSSRID_Autodesk = 1,
   GSSRID_EPSG     = 2,
   GSSRID_Oracle   = 3,
   GSSRID_ESRI     = 4
};

enum GSSRIDUnitEnum
{
   GSSRIDUnit_None      = 0,
   GSSRIDUnit_Meter     = 1,
   GSSRIDUnit_Degree    = 2,
   GSSRIDUnit_Mile      = 3,
   GSSRIDUnit_Inch      = 4,
   GSSRIDUnit_Feet      = 5,
   GSSRIDUnit_Kilometer = 6
};

// DEFINIZIONI DEI LIVELLI DI ACCESSO AI DATI
enum GSDataPermissionTypeEnum
{
   GSNonePermission = -1,  // nessun permesso esplicito (ereditato da altro utente)
   GSInvisibleData  = 0,   // invisibile
   GSReadOnlyData   = 1,   // in sola lettura
   GSUpdateableData = 2,   // in modifica
   GSExclusiveUseByAnother = 3 // in uso esclusivo da parte di un altro utente
};

// DEFINIZIONI DEI FORMATI DI COLORE
// (non cambiare i valori esistenti perchè vengono scritti su DB)
enum GSFormatColorEnum
{
   GSNoneFormatColor              = 0,
   GSAutoCADColorIndexFormatColor = 1, // [0-256]
   GSHTMLFormatColor              = 2, // #FFFFFF
   GSHexFormatColor               = 3, // FFFFFF
   GSRGBDecColonFormatColor       = 4, // 255,255,255
   GSRGBDecBlankFormatColor       = 5, // 255 255 255
   GSRGBDXFFormatColor            = 6, // numero RGB, es. 16711680 = FF0000 = 255 rosso, 0 verde, 0 blu
   GSRGBCOLORREFFormatColor       = 7  // numero BGR, es. 16711680 = FF0000 = 255 blue, 0 verde, 0 rosso
   
};

// DEFINIZIONE DELLE OPERAZIONI CHE DEVE FARE LA FINESTRA DI ATTESA
enum GSWaitDlgInstanceOp
{
   GSWaitDlgInstanceEndThread              = 0,
   GSWaitDlgInstanceAppendDlgComponent     = 1,
   GSWaitDlgInstanceRemoveLastDlgComponent = 2,
   GSWaitDlgInstanceSetMsg                 = 3,
   GSWaitDlgInstanceSetCounter             = 4,
   GSWaitDlgInstanceSetPerc                = 5
};


/*************************************************************************/
/*                                                                       */
/*************************************************************************/

#endif