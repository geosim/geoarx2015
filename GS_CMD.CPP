/**********************************************************
Name: GS_CMD

Module description: File contenente le funzioni ed i comandi AutoCAD
                    modificati per GEOsim 
            
Author: Poltini Roberto & io e pure io e pure la luce !!
                                                       
(c) Copyright 1995-2015 by IREN ACQUA GAS  S.p.A.


Modification history:
              
Notes and restrictions on use: 
**********************************************************/


/*********************************************************/
/* INCLUDES */
/*********************************************************/


#include "stdafx.h"         // MFC core and standard components

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")


#define      PI                     3.14159265358979323846
#define      DEGRAD                 (PI/180.0)  

// per gestione dei vertici delle polyline per comandp PEDIT
#define      CRESCENTE              1
#define      DECRESCENTE            0

#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <string.h>
#include <fcntl.h>       /*  per _open()  */
#include <sys\stat.h>    /*  per _open()  */
#include "adslib.h"
#include "adsdlg.h"   
#include "acedCmdNF.h" // per acedCommandS e acedCmdS
#include "dbapserv.h"

#include "..\gs_def.h" // definizioni globali
#include "gs_utily.h"     // (gsc_strcat, gsc_tostring)
#include "gs_resbf.h"     // (gsc_nth)
#include "gs_error.h"     // codici errori
#include "gs_list.h"
#include "gs_ase.h"       // gestione database
#include "gs_init.h"      // direttori globali
#include "gs_opcod.h"
#include "gs_user.h"   
#include "gs_dbref.h"   
#include "gs_ade.h"
#include "gs_cmd.h"       
#include "gs_conv.h"
#include "gs_class.h"     
#include "gs_prjct.h"     
#include "gs_area.h"     
#include "gs_find.h"
#include "gs_graph.h"     
#include "gs_grid.h"
#include "gs_query.h"     
#include "gs_attbl.h"     
#include "gs_lisp.h"     
#include "gs_topo.h"     
#include "gs_impor.h"     
#include "gs_lock.h"
#include "gs_plot.h"
#include "gs_sec.h"
#include "gs_evid.h"
#include "gs_whip.h"


ACRX_NO_CONS_DEFINE_MEMBERS(C_GSEditorReactor, AcEditorReactor);



typedef struct 
{
   TCHAR *cmd;      // nome comando (inglese)
   int   flag;      // flag di modifica
} cmd_flag_type_def;

// in ordine alfabetico
static cmd_flag_type_def CMD_FLAG[] =
{  // nome comando       flag di modifica
   {_T("ATTEDIT"),           RECORD_MOD + FAS_MOD},   
   {_T("GRIP_STRETCH"),      GEOMETRY_MOD},   
   {_T("MOVE"),              GEOMETRY_MOD},   
   {_T("PEDIT"),             GEOMETRY_MOD + FAS_MOD},   
   {_T("ROTATE"),            GEOMETRY_MOD},   
   {_T("SCALE"),             GEOMETRY_MOD},   
   {_T("STRETCH"),           GEOMETRY_MOD}
};

static int LEN_CMD_FLAG = ELEMENTS(CMD_FLAG);


//-----------------------------------------------------------------------------
// Classe C_CMDLIST
//-----------------------------------------------------------------------------


C_CMDLIST::C_CMDLIST()
{
   int   lan = 0, i = 0, index = 0;

   typedef struct 
   {
      TCHAR *name;
      int   (*fptr)();
      int   flag;          // controllo 1° Bit operativita su spazio carta no-si
                           // controllo 2° Bit comando disponibile con o senza login
   } ftblent;

   typedef struct 
   {
      TCHAR *MultiName[2];
      int   (*fptr)();
      int   flag;          // controllo 1° Bit operativita su spazio carta no-si
                           // controllo 2° Bit comando disponibile con o senza login
   } MultiLanFunTbl;

   typedef struct 
   {
      TCHAR           *MultiName[2];
      AcRxFunctionPtr fptr;
      bool            LoginNeeded;
      Adesk::Int32    flag;
   } MultiLanCmdTbl;

   // Flag di funzionamento come descritto sotto:
   // ACRX_CMD_MODAL       = Command cannot be invoked while another command is active
   // ACRX_CMD_TRANSPARENT = Command can be invoked while another command is active.
   //                        ads_sssetfirst() cannot be used when this flag is set
   // ACRX_CMD_USEPICKSET   = When the pickfirst set is retrieved, it is cleared within AutoCAD.
   //                         Command is able to retrieve the pickfirst set via the functions ads_ssgetfirst() or ads_ssget("I.").
   //                         Command can set the pickfirst set via ads_sssetfirst(), but it only stays set until the command ends.
   //                         Command cannot retrieve nor set grips.
   //                        Objects in these sets are redrawn with the proper grip handles and highlighting upon completion of the command. 
   // ACRX_CMD_NOPAPERSPACE = Command cannot be invoked when in paperspace


   // Il seguente vettore contiene i nomi dei comandi Geosim nelle varie lingue supportate
   MultiLanCmdTbl VectorGeosimCmds[] =
   //  Italiano           Inglese                           Nome            Login    Flag di funzionamento
   //                                                      interno        necessaria  
   {  
      // comando per gli account di accesso
      {{_T("gsInitGeopwd"),       _T("gsInitGeopwd")     }, gsInitGeopwd,        true, ACRX_CMD_MODAL},
      {{_T("gsPrintGeopwd"),      _T("gsPrintGeopwd")    }, gsPrintGeopwd,       false, ACRX_CMD_MODAL},
      // Comando fasullo per attivare il riallineamento di modifiche apportate con acad
      {{_T("gsacadalign"),       _T("gsacadalign")       }, gsdummyalign,        true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE},
      // Gruppi di selezione SAVE_SS 
      {{_T("gsevidsavess"),      _T("gsevidsavess")      }, gsevidsavess,        true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE},
      // Gruppi di selezione REFUSED_SS 
      {{_T("gsevidssrefused"),   _T("gsevidssrefused")   }, gsevidssrefused,     true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE},
      // Reattori 
      {{_T("gsreactorsoff"),     _T("gsreactorsoff")     }, gsreactorsoff,       true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE},
      // Connessioni OLE_DB 
      {{_T("gsrestoredbconnection"), _T("gsrestoredbconnection")}, gsRestoreDBConnection, true, ACRX_CMD_MODAL},
      // Login
      {{_T("gslogin"),           _T("gslogin")           }, gslogin,             false, ACRX_CMD_MODAL},
      // Gestione Utenti
      // Creazione
      {{_T("gscreausr"),         _T("gscreausr")         }, gscreausr,           true, ACRX_CMD_MODAL},
      // Modifica
      {{_T("gsmodpwd"),          _T("gsmodpwd")          }, gsmodpwd,            true, ACRX_CMD_MODAL},
      // cancellazione
      {{_T("gsdelusr"),          _T("gsdelusr")          }, gsdelusr,            true, ACRX_CMD_MODAL},
      // Gestione Progetti
      // Creazione
      {{_T("gscreateprj"),       _T("gscreateprj")       }, gscreateprj,         true, ACRX_CMD_MODAL},
      // Cancellazione
      {{_T("gsdelprj"),          _T("gsdelprj")          }, gsdelprj,            true, ACRX_CMD_MODAL},
      // Gestione Aree
      {{_T("gsextractareaclass"), _T("gsextractareaclass")}, gsextractareaclass, true, ACRX_CMD_TRANSPARENT | ACRX_CMD_NOPAPERSPACE},
      // Allineamento Database
      {{_T("gsddclassalign"),    _T("gsddclassalign")    }, gsddclassalign,      true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE},
      // Inserimento Entità
      {{_T("gshatch"),           _T("gshatch")           }, gshatch,             true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE | ACRX_CMD_USEPICKSET | ACRX_CMD_REDRAW},
      // Esportazione Entità
      {{_T("gscopy2AcadObjs"),   _T("gscopy2AcadObjs")   }, gscopy2AcadObjs,     true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE | ACRX_CMD_USEPICKSET | ACRX_CMD_REDRAW},
      {{_T("gswblock"),          _T("gswblock")          }, gswblock,            true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE | ACRX_CMD_USEPICKSET | ACRX_CMD_REDRAW},
      // Interrogazione e modifica attributi
      {{_T("gsddwhohasit"),      _T("gsddwhohasit")      }, gsddwhohasit,        true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE | ACRX_CMD_USEPICKSET | ACRX_CMD_REDRAW},
      {{_T("gsdddynamicquery"),  _T("gsdddynamicquery")  }, gsDynamicQueryAttr,  true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE | ACRX_CMD_USEPICKSET | ACRX_CMD_REDRAW},

      {{_T("gsdbcopy"),          _T("gsdbcopy")          }, gsdbcopy,            true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE | ACRX_CMD_USEPICKSET | ACRX_CMD_REDRAW},
      {{_T("gssetdbdefault"),    _T("gssetdbdefault")    }, gssetdbdefault,      true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE | ACRX_CMD_USEPICKSET | ACRX_CMD_REDRAW},

      {{_T("gsaddmemberstogrp"), _T("gsaddmemberstogrp") }, gsaddmemberstogrp,   true, ACRX_CMD_MODAL | ACRX_CMD_USEPICKSET | ACRX_CMD_REDRAW}, // grip mode
      {{_T("gsremovememberstogrp"), _T("gsremovememberstogrp") }, gsremovememberstogrp, true, ACRX_CMD_MODAL | ACRX_CMD_USEPICKSET | ACRX_CMD_REDRAW}, // grip mode

      {{_T("gsddfilter"),        _T("gsddfilter")        }, gsddfilter,          true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE | ACRX_CMD_USEPICKSET | ACRX_CMD_REDRAW},
      {{_T("gsddfilterevid"),    _T("gsddfilterevid")    }, gsddfilterevid,      true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE | ACRX_CMD_USEPICKSET | ACRX_CMD_REDRAW},

      {{_T("gsddthemafilter"),   _T("gsddthemafilter")   }, gsddthemafilter,     true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE | ACRX_CMD_USEPICKSET | ACRX_CMD_REDRAW},

      {{_T("gsddsensor"),        _T("gsddsensor")        }, gsddsensor,          true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE | ACRX_CMD_USEPICKSET | ACRX_CMD_REDRAW},

      {{_T("gsddSecValueTransfer"), _T("gsddSecValueTransfer")}, gsddSecValueTransfer, true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE | ACRX_CMD_USEPICKSET | ACRX_CMD_REDRAW},
      {{_T("gsddSecDynamicSegmentationAnalyzer"), _T("gsddSecDynamicSegmentationAnalyzer")}, gsddSecDynamicSegmentationAnalyzer, true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE | ACRX_CMD_USEPICKSET | ACRX_CMD_REDRAW},

      {{_T("gsinvisib"),         _T("gsinvisib")         }, gsinvisib,           true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE | ACRX_CMD_USEPICKSET | ACRX_CMD_REDRAW},
      {{_T("gsvisib"),           _T("gsvisib")           }, gsvisib,             true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE | ACRX_CMD_USEPICKSET | ACRX_CMD_REDRAW},
      {{_T("gsInvOverlapAttr"),  _T("gsInvOverlapAttr")  }, gsInvOverlapAttr,    true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE | ACRX_CMD_USEPICKSET | ACRX_CMD_REDRAW},
      {{_T("gsTopDrawOrderAttr"), _T("gsTopDrawOrderAttr")}, gsTopDrawOrderAttr, true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE | ACRX_CMD_USEPICKSET | ACRX_CMD_REDRAW},
      // Gestione Tematismi
      {{_T("gsrefreshthm"),      _T("gsrefreshthm")      }, gsrefreshthm,        true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE},
      {{_T("gsddLayerModelList"), _T("gsddLayerModelList")}, gsddLayerModelList, true, ACRX_CMD_MODAL},
      // Funzioni che utilizzano GeoLISP
      {{_T("gslisp"),            _T("gslisp")            }, gslisp,             true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE},
      {{_T("gsgslreload"),       _T("gsgslreload")       }, gsGslReload,         true, ACRX_CMD_MODAL},
      {{_T("gsgslgsreload"),     _T("gsgslgsreload")     }, gsGslGsReload,       true, ACRX_CMD_MODAL},
      {{_T("gsgslprjreload"),    _T("gsgslprjreload")    }, gsGslPrjReload,      true, ACRX_CMD_MODAL},
      // Funzione per la gestione dei set di visibilità
      {{_T("gsddsetvisib"),      _T("gsddsetvisib")      }, gsddsetvisib,        true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE},
      // Funzioni Plottaggio
      {{_T("gsddplot"),          _T("gsddplot")          }, gsddplot,            true, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE},
      {{_T("gsdelplot"),         _T("gsdelplot")         }, gsdelplot,           true, ACRX_CMD_MODAL},
      {{_T("gsexitplot"),        _T("gsexitplot")        }, gsexitplot,          true, ACRX_CMD_MODAL},
      {{_T("gsviewplot"),        _T("gsviewplot")        }, gsviewplot,          true, ACRX_CMD_MODAL},
      // comandi di utilità
      {{_T("gsAttachExtractDetach"), _T("gsAttachExtractDetach")}, gsAttachExtractDetach, false, ACRX_CMD_MODAL | ACRX_CMD_USEPICKSET | ACRX_CMD_REDRAW}
   };

   // Il seguente vettore contiene i nomi dei comandi di Acad ridefiniti da Geosim
   MultiLanCmdTbl VectorAcadCmds[] =
   {
   //  Italiano           Inglese         Nome      Login    Flag di funzionamento
   //                                    interno  necessaria  
      {{_T("apri")      , _T("open")   }, gsopen,   false, ACRX_CMD_MODAL},
      {{_T("cancella")  , _T("erase")  }, gserase,  false, ACRX_CMD_MODAL | ACRX_CMD_USEPICKSET | ACRX_CMD_REDRAW},
      {{_T("copia")     , _T("copy")   }, gscopy,   false, ACRX_CMD_MODAL | ACRX_CMD_USEPICKSET | ACRX_CMD_REDRAW},
      {{_T("spezza")    , _T("break")  }, gsbreak,  false, ACRX_CMD_MODAL | ACRX_CMD_USEPICKSET | ACRX_CMD_REDRAW},
      {{_T("nuovo")     , _T("new")    }, gsnew,    false, ACRX_CMD_MODAL},
      {{_T("salva")     , _T("save")   }, gssave,   false, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE},
      {{_T("salvavel")  , _T("qsave")  }, gsqsave,  false, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE},
      {{_T("salvacome") , _T("saveas") }, gssaveas, false, ACRX_CMD_MODAL | ACRX_CMD_NOPAPERSPACE}
   };


   // Il seguente vettore contiene i nomi delle funzioni LISP
   ftblent VectorLispFunction[] =
   {
   //  Inglese                    Nome interno               Flag di funzionamento
   //                                                        1° Bit = operativita su spazio carta no (0) - si (1)
   //                                                        2° Bit = comando disponibile solo con login (0) o senza login (1)
      // Funzione di Test 
      {_T("gs_prova"),            gs_prova,                  3},
      // Funzioni per gestione licenze
      {_T("bidibibodibibu"),      gs_setMaxLicNum,           3},
      {_T("gs_getMaxLicNum"),     gs_getMaxLicNum,           3},
      {_T("gs_getActualGEOsimUsrList"), gs_getActualGEOsimUsrList, 3},
      // Funzioni di utilità ADE
      {_T("gs_ade_qldelquery"),   gs_ade_qldelquery,         0},
      {_T("gs_ade_qlloadqry"),    gs_ade_qlloadqry,          0},
      {_T("gs_ade_qlqrygetid"),   gs_ade_qlqrygetid,         0},
      {_T("gs_ade_qrylist"),      gs_ade_qrylist,            0},
      // Funzioni di utilità
      {_T("gs_KriptFile"),        gs_KriptFile,              0},
      {_T("gs_DeKriptFile"),      gs_DeKriptFile,            0},
      {_T("gs_create_dir"),       gs_create_dir,             0},
      {_T("gs_UndefineAcadCmds"), gs_UndefineAcadCmds,       1},
      {_T("gs_getPathInfoFromINI"), gs_getPathInfoFromINI,   0},
      {_T("gs_setPathInfoToINI"), gs_setPathInfoToINI,       0},
      {_T("gs_dir_exist"),        gs_dir_exist,              0},
      {_T("gs_path_exist"),       gs_path_exist,             0},
      {_T("gs_dir_from_path"),    gs_dir_from_path,          0},
      {_T("gs_delall"),           gs_delall,                 0},
      {_T("gs_getconfirm"),       gs_getconfirm,             0},
      {_T("gs_ddgetconfirm"),     gs_ddgetconfirm,           0},
      {_T("gs_ddalert"),          gs_ddalert,                2},
      {_T("gs_ddinput"),          gs_ddinput,                2},
      {_T("gs_get_window"),       gs_get_window,             2},
      {_T("gs_AskSelSet"),        gs_AskSelSet,              2},
      {_T("gs_getODTables"),      gs_getODTables,            0},
      {_T("gs_getGUID"),          gs_getGUID,                3},      
      // Conversione banca dati
      {_T("gs_conv_alias"),       gs_conv_alias,             0},
      {_T("gs_change_group_link"), gs_change_group_link,     0},
      {_T("gs_UpdateGasSimStru"), gs_UpdateGasSimStru,       0},
      {_T("gs_UpdateIrenSimStru"), gs_UpdateIrenSimStru,     0},
      // {"gs_zoom_in",          gs_zoom_in,                0}, // non usata
      {_T("gs_save_class_sel"),   gs_save_class_sel,         0},
      {_T("gs_load_class_sel"),   gs_load_class_sel,         0},
      // {"gs_crea_grid",        gs_crea_grid,              0}, // non usata
      {_T("gs_get_tmp_filename"), gs_get_tmp_filename,       2},
      {_T("gs_is_graph_calc_to_recalc"), gs_is_graph_calc_to_recalc, 0},
      {_T("gs_delfile"),          gs_delfile,                2},
      {_T("gs_help"),             gs_help,                   2},
      {_T("gs_dd_get_existingDir"), gs_dd_get_existingDir,   2},
      // funzioni di conversione 
      {_T("gs_conv_DateTime_2_WinFmt"), gs_conv_DateTime_2_WinFmt, 2},
      {_T("gs_conv_DateTime_2_WinShortFmt"), gs_conv_DateTime_2_WinShortFmt, 2},
      {_T("gs_conv_Number"),      gs_conv_Number,            2},
      {_T("gs_conv_Currency"),    gs_conv_Currency,          2},
      {_T("gs_conv_Bool"),        gs_conv_Bool,              2},
      // Reattori
      {_T("gs_enable_reactors"),  gs_enable_reactors,        0},
      {_T("gs_disable_reactors"), gs_disable_reactors,       0},
      {_T("gs_is_reactorsoff"),   gs_is_reactorsoff,         0},
      // Login 
      {_T("gs_login"),            gs_login,                  2},
      // Gestione Utenti
      // Creazione
      {_T("gs_creausr"),          gs_creausr,                0},
      // Interrogazione
      {_T("gs_superuser"),        gs_superuser,              0},
      {_T("gs_whoami"),           gs_whoami,                 2},
      {_T("gs_chkusr"),           gs_chkusr,                 0},
      {_T("gs_getusr"),           gs_getusr,                 0},
      {_T("gs_getusrcomm"),       gs_getusrcomm,             0},
      {_T("gs_getgscomm"),        gs_getgscomm,              0},
      {_T("gs_getusrprj"),        gs_getusrprj,              0},
      {_T("gs_getusrclass"),      gs_getusrclass,            0},
      {_T("gs_getusrprj"),        gs_getusrprj,              0},
      {_T("gs_getPrjPermissions"), gs_getPrjPermissions,     0},    
      {_T("gs_getusrclass"),      gs_getusrclass,            0},
      {_T("gs_getClassPermissions"), gs_getClassPermissions, 0},    
      // {"gs_chkclsvis",        gs_chkclsvis,              0}, // non usata
      // {"gs_getfamily",        gs_getfamily,              0}, // non usata
      {_T("gs_checkop"),          gs_checkop,                0},
      // Modifica
      {_T("gs_setusrprj"),        gs_setusrprj,              0},
      {_T("gs_setusrcomm"),       gs_setusrcomm,             0},
      {_T("gs_setusrclass"),      gs_setusrclass,            0},
      {_T("gs_modpwd"),           gs_modpwd,                 0},
      // Cancellazione
      {_T("gs_delusr"),           gs_delusr,                 0},
      // Gestione Progetti
      // Interrogazione
      {_T("gs_rprj"),             gs_rprj,                   0},
      {_T("gs_current_prj"),      gs_current_prj,            1},
      {_T("gs_getLastUsedPrj"),   gs_getLastUsedPrj,         1},
      {_T("gs_setLastUsedPrj"),   gs_setLastUsedPrj,         1},
      {_T("gs_reindexmaintab"),   gs_reindexmaintab,         1},
      // Creazione
      {_T("gs_createprj"),        gs_createprj,              0},
      // Modifica
      {_T("gs_modprj"),           gs_modprj,                 0},
      // Cancellazione
      {_T("gs_delprj"),           gs_delprj,                 0},
      // Gestione Tematismi
      {_T("gs_getdefaultFAS"),    gs_getdefaultFAS,          0},
      {_T("gs_LoadGEOsimThm"),    gs_LoadGEOsimThm,          3},
      {_T("gs_SetColorDialog"),   gs_SetColorDialog,         3},
      {_T("gs_ColorToString"),    gs_ColorToString,          3},
      {_T("gs_StringToColor"),    gs_StringToColor,          3},
      {_T("gs_getgeosimltype"),   gs_getgeosimltype,         0},
      {_T("gs_ddsellinetype"),    gs_ddsellinetype,          0},
      {_T("gs_DefineTxtLineType"), gs_DefineTxtLineType,     0},
      {_T("gs_getgeosimhatch"),   gs_getgeosimhatch,         0},
      {_T("gs_ddselhatch"),       gs_ddselhatch,             0},
      {_T("gs_getBlockListWithAttributes"), gs_getBlockListWithAttributes, 0},
      {_T("gs_getGEOsimBlockList"), gs_getGEOsimBlockList,   0},
      {_T("gs_getBlockList"),     gs_getBlockList,           0},
      {_T("gs_uiselblock"),       gs_uiselblock,             0},
      {_T("gs_getgeosimstyle"),   gs_getgeosimstyle,         0},
      {_T("gs_ddseltextstyles"),  gs_ddseltextstyles,        0},
      {_T("gs_getgeosimcoord"),   gs_getgeosimcoord,         0},
      {_T("gs_validcoord"),       gs_validcoord,             0},
      {_T("gs_load_layer_status"), gs_load_layer_status,     1},
      {_T("gs_save_layer_status"), gs_save_layer_status,     1},
      {_T("gs_ddsellayers"),       gs_ddsellayers,           1},
      {_T("gs_getDependeciesOnLayer"), gs_getDependeciesOnLayer, 3},
      {_T("gs_getLayerModelList"), gs_getLayerModelList,     3},
      {_T("gs_setLayerModel"),    gs_setLayerModel,          3},
      {_T("gs_ddLayerModel"),     gs_ddLayerModel,           3},
      {_T("gs_getgeosimDimStyle"), gs_getgeosimDimStyle,     0},      
      {_T("gs_ddSelDimStyles"),   gs_ddSelDimStyles,         0},      
      
      // Gestione DataBase
      {_T("gs_CreateACCDB"),      gs_CreateACCDB,            3},
      {_T("gs_existtable"),       gs_existtable,             3},
      {_T("gs_readstruct"),       gs_readstruct,             3},
      {_T("gs_readrows"),         gs_readrows,               3},
      {_T("gs_exesql"),           gs_exesql,                 3},
      {_T("gs_getUDLPropertiesDescrFromFile"), gs_getUDLPropertiesDescrFromFile, 3},
      {_T("gs_getDBConnection"),  gs_getDBConnection,        3},
      {_T("gs_getCatSchInfoFromFile"), gs_getCatSchInfoFromFile, 3},
      {_T("gs_getUDLList"),       gs_getUDLList,             3},
      {_T("gs_getProviderTypeList"), gs_getProviderTypeList, 3},
      {_T("gs_Descr2ProviderDescr"), gs_Descr2ProviderDescr, 3},
      {_T("gs_SplitFullRefTable"), gs_SplitFullRefTable,     3},
      {_T("gs_getFullRefTable"),  gs_getFullRefTable,        3},
      {_T("gs_PropListFromConnStr"), gs_PropListFromConnStr, 3},
      {_T("gs_PropListToConnStr"), gs_PropListToConnStr,     3},
      {_T("gs_IsValidFieldName"), gs_IsValidFieldName,       3},
      {_T("gs_DBIsBlob"),         gs_DBIsBlob,               3},
      {_T("gs_DBIsBoolean"),      gs_DBIsBoolean,            3},
      {_T("gs_DBIsChar"),         gs_DBIsChar,               3},
      {_T("gs_DBIsCurrency"),     gs_DBIsCurrency,           3},
      {_T("gs_DBIsDate"),         gs_DBIsDate,               3},
      {_T("gs_DBIsTimestamp"),    gs_DBIsTimestamp,          3},
      {_T("gs_DBIsNumeric"),      gs_DBIsNumeric,            3},
      {_T("gs_DBIsNumericWithDecimals"), gs_DBIsNumericWithDecimals, 3},
      {_T("gs_IsCompatibleType"), gs_IsCompatibleType,       3},
      {_T("gs_getSRIDList"),      gs_getSRIDList,            3},
      // Gestione Link DataBase-Grafica e fattore aggregazione (per manutenzione dati)
      {_T("gs_ChangeEEDAggr"),    gs_ChangeEEDAggr,          0},
      {_T("gs_ChangeEEDCls"),     gs_ChangeEEDCls,           0},
      {_T("gs_ChangeODCls"),      gs_ChangeODCls,            0},
      {_T("gs_ClearGSMarker"),    gs_ClearGSMarker,          0},
      {_T("gs_get_eed"),          gs_get_eed,                0},
      {_T("gs_ClearAdeLock"),     gs_ClearAdeLock,           0},
      // Gestione tabelle secondarie
      // Interrogazione
      {_T("gs_getdefaultattrsec"), gs_getdefaultattrsec,     0},
      {_T("gs_get_tab_sec"),      gs_get_tab_sec,            0},
      {_T("gs_rtab_sec"),         gs_rtab_sec,               0},
      {_T("gs_getSinthClsSecondaryTabList"), gs_getSinthClsSecondaryTabList, 0},
      {_T("gs_get_sec_attrib_list"), gs_get_sec_attrib_list, 0},
      {_T("gs_get_sec_info"),     gs_get_sec_info,           0},
      // Creazione
      {_T("gs_create_tab_sec"),   gs_create_tab_sec,         0},
      {_T("gs_link_tab_sec"),     gs_link_tab_sec,           0},
      // Modifica
      {_T("gs_mod_sec_link"),     gs_mod_sec_link,           0},
      {_T("gs_mod_sec_vis"),      gs_mod_sec_vis,            0},
      {_T("gs_mod_sec_name"),     gs_mod_sec_name,           0},
      {_T("gs_mod_sec_descr"),    gs_mod_sec_descr,          0},
      {_T("gs_mod_sec_dynamic_segmentation"), gs_mod_sec_dynamic_segmentation, 0},
      // Copia
      {_T("gs_copy_sec"),         gs_copy_sec,               0},
      // Cancellazione
      {_T("gs_del_tab_sec"),      gs_del_tab_sec,            0},
      // Distruzione Incondizionata
      {_T("gs_destroysec"),       gs_destroysec,             0},
      // Gestione Classi
      // Interrogazione
      {_T("gs_getlisttypeclass"), gs_getlisttypeclass,       0},
      {_T("gs_getdefaultattr"),   gs_getdefaultattr,         0},
      {_T("gs_getDefaultGraphInfo"), gs_getDefaultGraphInfo, 0},
      {_T("gs_getAttrListFromBlock"), gs_getAttrListFromBlock, 0},
      {_T("gs_getAttrListFromOD"), gs_getAttrListFromOD,     0},
      {_T("gs_getAttrListFromDB"), gs_getAttrListFromDB,     0},
      {_T("gs_rclass"),           gs_rclass,                 0},
      {_T("gs_getSinthClassList"), gs_getSinthClassList,     0},
      {_T("gs_get_group"),        gs_get_group,              0},
      {_T("gs_get_class"),        gs_get_class,              0},
      {_T("gs_get_id"),           gs_get_id,                 0},
      {_T("gs_get_info"),         gs_get_info,               0},
      {_T("gs_get_fas"),          gs_get_fas,                0},
      {_T("gs_get_gph_info"),     gs_get_gph_info,           0},
      {_T("gs_get_grid"),         gs_get_grid,               0},
      {_T("gs_get_connect_list"), gs_get_connect_list,       0},
      {_T("gs_get_group_list"),   gs_get_group_list,         0},
      {_T("gs_get_attrib_list"),  gs_get_attrib_list,        0},
      {_T("gs_get_default_values"), gs_get_default_values,   0},
      {_T("gs_get_dwg_list"),     gs_get_dwg_list,   0},
      {_T("gs_is_inarea"),        gs_is_inarea,              0},
      {_T("gs_ddselect_class"),   gs_ddselect_class,         0},
      {_T("gs_what_is_graph_updateable"), gs_what_is_graph_updateable, 0},
      // Creazione
      {_T("gs_createsimplex"),    gs_createsimplex,          0},
      {_T("gs_createspaghetti"),  gs_createspaghetti,        0},
      {_T("gs_creategroup"),    gs_creategroup,          0},
      {_T("gs_creategrid"),       gs_creategrid,             0},
      {_T("gs_createextern"),     gs_createextern,           0},
      // Creazione automatica di una lista di classi 
      {_T("gs_AutomaticCreationClass"), gs_AutomaticCreationClass, 0},
      {_T("gs_class_write_stru"), gs_class_write_stru,       1},
      // Modifica
      {_T("gs_mod_id"),           gs_mod_id,                 0},
      {_T("gs_mod_fas"),          gs_mod_fas,                0},
      {_T("gs_mod_dbgph_info"),   gs_mod_dbgph_info,         0},
      {_T("gs_ddSetLayerModelOnFAS"), gs_ddSetLayerModelOnFAS, 0},
      {_T("gs_mod_coord"),        gs_mod_coord,              0},
      {_T("gs_mod_connect_list"), gs_mod_connect_list,       0},
      {_T("gs_mod_stru"),         gs_mod_stru,               0},
      {_T("gs_dealloc_mod_stru"), gs_dealloc_mod_stru,       0},
      {_T("gs_alloc_mod_stru"),   gs_alloc_mod_stru,         0},
      {_T("gs_add_attr"),         gs_add_attr,               0},
      {_T("gs_del_attr"),         gs_del_attr,               0},
      {_T("gs_mod_attr"),         gs_mod_attr,               0},
      // Copia
      {_T("gs_copy_class"),       gs_copy_class,             0},
      // Cancellazione
      {_T("gs_delclass"),         gs_delclass,               0},
      // Distruzione Incondizionata
      {_T("gs_destroyclass"),     gs_destroyclass,           0},
      // Reindicizzazione
      {_T("gs_checkclass"),       gs_checkclass,             0},
      // Sincronizzazione  
      {_T("gs_synchronizeclass"), gs_synchronizeclass,       0},
      // Ottimizzazione DWG
      {_T("gs_dwgoptimizationclass"), gs_dwgoptimizationclass, 0},
      {_T("gs_dwgdisplayext"),        gs_DisplayDWGExt,        0},
      // Multiutenza - sblocco classe
      {_T("gs_share_class"),      gs_share_class,            0},
      // Report strutture
      {_T("gs_ClassesStructReport"), gs_ClassesStructReport, 0},
      // Griglia
      {_T("gs_DisplayGridBorder"), gs_DisplayGridBorder,     0},
      {_T("gs_DisplayGridPts"),   gs_DisplayGridPts,         0},
      {_T("gs_DisplayGridCells"), gs_DisplayGridCells,       0},
      {_T("gs_DisplayGridContours"), gs_DisplayGridContours, 0},
      {_T("gs_GridSpatialInterpolationIDW"), gs_GridSpatialInterpolationIDW, 0},
      {_T("gs_ObjectsOnGrid"),    gs_ObjectsOnGrid,          0},
      {_T("gs_SetAttribEntityFromGrid"), gs_SetAttribEntityFromGrid, 0},
      {_T("gs_InitIterpolateDisplayContoursGrid"), gs_InitIterpolateDisplayContoursGrid, 0},
      {_T("gs_DisplayValleyOrRidge"), gs_DisplayValleyOrRidge, 0},
      // Set di classi
      {_T("gs_move_classes_to_class_set"), gs_move_classes_to_class_set, 2},
      // Gestione sessioni di lavoro
      // Interrogazione
      {_T("gs_ruserWrkSession"),  gs_ruserWrkSession,        0},
      {_T("gs_currentWrkSession"), gs_currentWrkSession,     0},
      {_T("gs_rWrkSessionClass"), gs_rWrkSessionClass,       0},
      {_T("gs_drawWrkSessionspatialareas"), gs_drawWrkSessionspatialareas, 0},
      {_T("gs_is_frozen_curr_session"), gs_is_frozen_curr_session, 0},
      // Creazione
      {_T("gs_get_NextWrkSessionId"), gs_get_NextWrkSessionId, 0},
      {_T("gs_create_WrkSession"), gs_create_WrkSession,     0},
      {_T("gs_sel_WrkSession_class"), gs_sel_WrkSession_class, 0},
      {_T("gs_clsextrinWrkSession"), gs_clsextrinWrkSession, 0},
      {_T("gs_getLastExtractedClassesFromINI"), gs_getLastExtractedClassesFromINI, 0},
      {_T("gs_ClsSQLQryDel"), gs_ClsSQLQryDel, 0},
      {_T("gs_ClsSQLQrySave"), gs_ClsSQLQrySave, 0},
      // Cancellazione
      {_T("gs_del_WrkSession"),   gs_del_WrkSession,         0},
      {_T("gs_destroy_session"),  gs_destroysession,         0},
      // Recupero
      {_T("gs_retrieve_WrkSession"), gs_retrieve_WrkSession, 0},
      // Salvataggio banca dati centrale
      {_T("gs_save_WrkSession"),  gs_save_WrkSession,        0},
      {_T("gs_check_geoms_for_save"), gs_check_geoms_for_save, 0},
      {_T("gs_check_mpolygon_by_topology"), gs_check_mpolygon_by_topology, 0},

      // Congelamento
      {_T("gs_freeze_WrkSession"), gs_freeze_WrkSession,     0},
      // Uscita
      {_T("gs_exit_WrkSession"),  gs_exit_WrkSession,        0},
      // scongelamento
      {_T("gs_thaw_WrkSession"),  gs_thaw_WrkSession,        0},
      {_T("gs_thaw_WrkSession2"), gs_thaw_WrkSession2,       0},                // funzione di appoggio

      // Gruppi di selezione SAVE_SS 
      {_T("gs_get_savess"),       gs_get_savess,             0},
      // Gruppi di selezione REFUSED_SS 
      {_T("gs_get_ssrefused"),    gs_get_ssrefused,          0},
      // Inserimento Entità
      {_T("gs_insert_ent"),       gs_insert_ent,             0},
      // Aggregazione e disgregazione Entità
      {_T("gs_aggr_data"),        gs_aggr_data,              0},
      {_T("gs_disaggr_data"),     gs_disaggr_data,           0},
      // Interrogazione e modifica attributi entità
		{_T("gs_get_group_by_member"), gs_get_group_by_member, 1},
      {_T("gs_filter_sql"), 		 gs_filter_sql,             0},
      {_T("gs_filter_fas"), 		 gs_filter_fas,             0},
      {_T("gs_impost_estrqry"),   gs_impost_estrqry,         0},
      {_T("gs_find_addr"),		    gs_find_addr,              0},
      {_T("gs_GetSpatialFromSQL"), gs_GetSpatialFromSQL,     0},
      {_T("gs_scal_attr_text"), 	 gs_scal_attr_text,         0},    
                        
		{_T("gs_query_data"),	    gs_query_data,             0},
		{_T("gs_upd_data"),			 gs_upd_data,               0},
		{_T("gs_upd_data_fromdb"),  gs_upd_data_fromdb,        0},
		{_T("gs_upd_grid_data"),    gs_upd_grid_data,          0},
		{_T("gs_ins_data"),			 gs_ins_data,               0},
		{_T("gs_erase_data"),		 gs_erase_data,             0},
      {_T("gs_do_sql_filter"),    gs_do_sql_filter,          0},
      {_T("gs_set_ssfilter"),     gs_set_ssfilter,           0},
      {_T("gs_get_ssfilter"),     gs_get_ssfilter,           0},
      {_T("gs_get_keylistfilter"), gs_get_KeyListFilter,     0},
      {_T("gs_get_clsfilter"),    gs_get_clsfilter,          0},
      {_T("gs_filterPrepareStatistics"), gs_filterPrepareStatistics, 0},
      {_T("gs_filterGetStatistics"), gs_filterGetStatistics, 0},
      {_T("gs_filterTerminateStatistics"), gs_filterTerminateStatistics, 0},
      {_T("gs_filterExport2File"), gs_filterExport2File,     0},
      {_T("gs_filterExport2Db"),  gs_filterExport2Db,        0},
      {_T("gs_sensorExport2File"), gs_sensorExport2File,     0},
      {_T("gs_sensorExport2Db"),  gs_sensorExport2Db,        0},
      // Importazione Entità
      {_T("gs_import"),           gs_import,                 0},
      // Visibilità/Invisibilità blocchi attributi visibili
      {_T("gs_invisib"),          gs_invisib,                0},
      {_T("gs_visib"),            gs_visib,                  0},
      {_T("gs_is_DABlock"),       gs_is_DABlock,             0},
      // Funzioni Amministrazione rete
      {_T("gs_drive2nethost"),    gs_drive2nethost,          0},
      // Funzione per variabili globali
      {_T("gs_setvar"),           gs_SetVar,                 0},
      {_T("gs_getvar"),           gs_GetVar,                 0},
      // Funzione di conversione dati vers. precedente di GEOsim
      {_T("gs_MainTabsConv2ActualVer"),  gs_MainTabsConv2ActualVer,  0},
      {_T("gs_MainTabsConv2DB"),         gs_MainTabsConv2DB,         0},
      {_T("gs_ClsConv2ActualVer"),       gs_ClsConv2ActualVer,       0},
      {_T("gs_PurgeLPTs"),               gs_PurgeLPTs,               0},
      {_T("gs_PurgeODTables"),           gs_PurgeODTables,           0},
      {_T("gs_RenameODField"),           gs_RenameODField,           0},
      {_T("gs_setActualGEOsimVersion"),  gs_setActualGEOsimVersion,  0},
      {_T("gs_isCompatibGEOsimVersion"), gs_isCompatibGEOsimVersion, 0},
      // Funzioni che utilizzano GeoLISP
      {_T("gs_calc_attr"),        gs_calc_attr,              0},
      {_T("gs_LoadFuncFromGraphList"), gs_LoadFuncFromGraphList, 1},
      {_T("gs_LoadFuncToGraphList"), gs_LoadFuncToGraphList, 1},      
      {_T("gs_LoadFuncNameRemList"), gs_LoadFuncNameRemList, 1},
      // Funzioni per la gestione dei set di visibilità 
      {_T("gs_rsetv"),            gs_rsetv,                  0},
      {_T("gs_activesetv"),       gs_activesetv,             0},
      {_T("gs_current_setv"),     gs_current_setv,           0},
      // Funzioni per la gestione della topologia di simulazione 
      {_T("gs_topocreate"),       gs_topocreate,             0},
      // funzioni per la generazione di file DWF/DWG
      {_T("gs_whip"),             gs_whip,                   0},
      {_T("gs_extract_4_whip"),   gs_extract_4_whip,         0},
      // funzioni per la generazione viste per la parte WEB
      {_T("gs_pgview"),           gs_pgview,                 0},
      // funzioni per l'esportazione in shape
      {_T("gs_shapeExport"),      gs_shapeExport,            0},
      // funzioni per Google Earth
      {_T("gs_setGoogleEarthODRecord"), gs_setGoogleEarthODRecord, 0},
      // funzioni per la generazione sistema di storicizzazione per le classi
      {_T("gs_CreateHistorySystem"), gs_CreateHistorySystem, 0},
      {_T("gs_DisableHistorySystem"), gs_DisableHistorySystem, 0},
      {_T("gs_EnableHistorySystem"), gs_EnableHistorySystem, 0},
      // funzioni per la creazione 'automatica' di tematismi
      {_T("gs_themafilterfromfile"), gs_themafilterfromfile, 0},
      {_T("gs_ddChooseGraphSettings"), gs_ddChooseGraphSettings, 0},
      // Funzioni di appoggio all' importazione di oggetti in GEOsim
      {_T("gs_getODTablesfromSS"), gs_getODTablesfromSS,      0},
      {_T("gs_getLPNfromSS"),     gs_getLPNfromSS,            0},
      {_T("gs_setLPNstatusOnMemory"), gs_setLPNstatusOnMemory, 0},
      {_T("gs_getLPNstatusFromMemory"), gs_getLPNstatusFromMemory, 0},
      {_T("gs_AcadObjToPline"),   gs_AcadObjToPline,         0},
      {_T("gs_AcadObjToBlock"),   gs_AcadObjToBlock,         0},
      {_T("gs_AcadObjToText"),    gs_AcadObjToText,          0},
      // Funzioni di appoggio alla generazione automatica delle classi
      {_T("gs_fromDclToRB"), gs_fromDclToRB, 0},
      {_T("gs_readAcpfile"), gs_readAcpfile, 0},

      // Funzioni di geocoding
      {_T("gs_getAddressFromLL"),      gs_getAddressFromLL, 0},
      {_T("gs_getLLFromAddress"),      gs_getLLFromAddress, 0},
      {_T("gs_getLLDirectionFromPts"), gs_getLLDirectionFromPts, 0},
      {_T("gs_GrDrawCross"),           gs_GrDrawCross, 0},
      
      {_T("gs_plot_preview"), gs_plot_preview, 0}

};

   // Inizializzo exfun AcadCmds
   exfun         = NULL;
   LenExfun      = 0;
   GEOsimCmds    = NULL;
   LenGEOsimCmds = 0;
   AcadCmds      = NULL;
   LenAcadCmds   = 0;

   // costruisco il vettore comando-funzione
   if (gsc_GetAcadLanguage(&lan) == GS_BAD) return;

   switch (lan)
   {
      case LAN_ITALIAN:
         index = 0;
         break;
      default:
         index = 1;
         break;
   }
   
   // caricamento vettore funzioni GEOsim LISP
   LenExfun = ELEMENTS(VectorLispFunction);  
   // alloco exfun
   if ((exfun = (FunTable *) calloc(LenExfun, sizeof(FunTable))) == NULL)
      return; 
   // caricamento vettore
   for (i = 0; i < ELEMENTS(VectorLispFunction); i++)
   {
      exfun[i].name = VectorLispFunction[i].name;
      exfun[i].fptr = VectorLispFunction[i].fptr;
      exfun[i].flag = VectorLispFunction[i].flag;
   }

   // caricamento vettore comandi GEOsim proprietari
   LenGEOsimCmds = ELEMENTS(VectorGeosimCmds);
   // alloco GEOsimCmds
   if ((GEOsimCmds = (CmdTable *) calloc(LenGEOsimCmds, sizeof(CmdTable))) == NULL)
      return; 
   // caricamento del vettore
   for (i = 0; i < LenGEOsimCmds; i++)
   {
      GEOsimCmds[i].LocalName = VectorGeosimCmds[i].MultiName[0];
      GEOsimCmds[i].GlobalName = VectorGeosimCmds[i].MultiName[1];
      GEOsimCmds[i].fptr = VectorGeosimCmds[i].fptr;
      GEOsimCmds[i].LoginNeeded = VectorGeosimCmds[i].LoginNeeded;
      GEOsimCmds[i].flag = VectorGeosimCmds[i].flag;
   }

   // caricamento vettore comandi AutoCAD rifatti
   LenAcadCmds = ELEMENTS(VectorAcadCmds);
   // alloco AcadCmds
   if ((AcadCmds = (CmdTable *) calloc(LenAcadCmds, sizeof(CmdTable))) == NULL)
      return; 
   // caricamento del vettore
   for (i = 0; i < LenAcadCmds; i++)
   {
      AcadCmds[i].LocalName = VectorAcadCmds[i].MultiName[0];
      AcadCmds[i].GlobalName = VectorAcadCmds[i].MultiName[1];
      AcadCmds[i].fptr = VectorAcadCmds[i].fptr;
      AcadCmds[i].LoginNeeded = VectorAcadCmds[i].LoginNeeded;
      AcadCmds[i].flag = VectorAcadCmds[i].flag;
   }

   CmdRunningFromInternalCall = false;
}

// Distruttore
C_CMDLIST::~C_CMDLIST()
{
   if (exfun) free(exfun);
   if (GEOsimCmds) free(GEOsimCmds);
   if (AcadCmds) free(AcadCmds);
}


/*************************************************************************/
/*.doc C_CMDLIST::funcLoad()                                             */
/*
    This function is called to define all function names in the ADS
    function table.  Each named function will be callable from lisp or
    invokable from another ADS application.
*/
/*************************************************************************/
int C_CMDLIST::funcLoad()
{
   for (int i = 0; i < LenExfun; i++) 
      if (!acedDefun(exfun[i].name.get_name(), i)) return RTERROR;
    
   return RTNORM;
}


/*************************************************************************/
/*.doc C_CMDLIST::funcUnload()                                           */
/*  
    This function is called to undefine all function names in the ADS
    function table.  Each named function will be removed from the
    AutoLISP hash table.
*/
/*************************************************************************/
int C_CMDLIST::funcUnload()
{
   int i;

   // se c'era una sessione di GEOsim aperta la chiudo
   if (GS_CURRENT_WRK_SESSION) gsc_ExitCurrSession(GS_GOOD);

   // Undefine each function we defined
   for (i = 0; i < LenExfun; i++) 
      acedUndef(exfun[i].name.get_name(), i);

   return RTNORM;
}


/*************************************************************************/
/*.doc C_CMDLIST::get_ptr_fun(int val)                                   */
/*  
    Questa funzione resrtituisce il puntatore alla funzione val-esima.
    Parametri: 
    int val;   Posizione nel vettore delle funzioni.
*/
/*************************************************************************/
FunTable* C_CMDLIST::get_ptr_fun(int val)
{
   acedRetNil();

   if (val >= LenExfun) return NULL;

   // se ci si trova nello spazio carta   
   if (gsc_isPaperSpaceCurrent())
      if (!(exfun[val].flag & 1))         // comando non abilitato allo spazio carta.
         { GS_ERR_COD = eGSPaperSpaceFound; return NULL; }
                         
   if (gsc_whoami(NULL) == GS_BAD)
      if (!(exfun[val].flag & 2))         // comando non abilitato senza login.
         return NULL;
	
   return &(exfun[val]);
}


/*************************************************************************/
/*.doc C_CMDLIST::GEOsimCmdsLoad()                                       */
/*
  Registra i comandi di GEOsim
*/
/*************************************************************************/
int C_CMDLIST::GEOsimCmdsLoad(void)
{
   // register my autocad commands
   for (int i = 0; i < LenGEOsimCmds; i++)
      acedRegCmds->addCommand(_T("GEOSIM"),
                              GEOsimCmds[i].GlobalName.get_name(), 
                              GEOsimCmds[i].LocalName.get_name(),
                              GEOsimCmds[i].flag,
                              GEOsimCmds[i].fptr);
    
   return RTNORM;
}


/***********************************************************/
/*.doc C_CMDLIST::UndefineAcadCmds()                       */
/*+
   Funzione che esegue l' UNDEFINE dei comandi di ACAD
   che sono stati rifatti in GEOsim.

   Restituisce GS_GOOD in caso di successo altrimenti GS_BAD. 
   N.B. qusta funzione NON può essere usata mentre c'è una finestra aperta.
-*/  
/***********************************************************/
int gs_UndefineAcadCmds(void)
{
   // Eseguo undefine dei comandi primitivi di AutoCAD
   return GEOsimAppl::CMDLIST.UndefineAcadCmds();
}
int C_CMDLIST::UndefineAcadCmds()
{
   int      last_echo, OldCmdDia;
   int      i;

   if (gsc_set_echo(0, &last_echo) == GS_BAD) return GS_BAD;
   gsc_set_cmddia(0, &OldCmdDia);

   // disabilito UNDO
   //UndoState = gsc_SetUndoRecording(FALSE);

   for (i = 0; i < LenAcadCmds; i++)
      //if (acedCmdUndefine(AcadCmds[i].LocalName.get_name(), 1) == 0) // undefine
      if (acedCommandS(RTSTR, _T("_.UNDEFINE"), RTSTR, AcadCmds[i].LocalName.get_name(), 0) != RTNORM)
      { 
         gsc_set_echo(last_echo); 
         gsc_set_cmddia(OldCmdDia);
         //gsc_SetUndoRecording(UndoState); 
         GS_ERR_COD = eGSErrorExecutingCommand;
         return GS_BAD;
      }

   //gsc_SetUndoRecording(UndoState); // Ripristina situazione UNDO precedente
   gsc_set_echo(last_echo);
   gsc_set_cmddia(OldCmdDia);

   // register my autocad commands
   for (i = 0; i < LenAcadCmds; i++)
      acedRegCmds->addCommand(_T("GEOSIM"),
                              AcadCmds[i].GlobalName.get_name(), 
                              AcadCmds[i].LocalName.get_name(),
                              AcadCmds[i].flag,
                              AcadCmds[i].fptr);
  
   return GS_GOOD;
}


/***********************************************************/
/*.doc C_CMDLIST::RedefineAcadCmds()                       */
/*+
   Funzione che esegue il REDEFINE dei comandi di ACAD
   che sono stati rifatti in GEOsim.

   Restituisce GS_GOOD in caso di successo altrimenti GS_BAD. 
   N.B. questa funzione NON può essere usata mentre c'è una finestra aperta.
-*/  
/***********************************************************/
int C_CMDLIST::RedefineAcadCmds()
{
   int   last_echo, OldCmdDia;

   gsc_set_echo(0, &last_echo);
   gsc_set_cmddia(0, &OldCmdDia);

   // disabilito UNDO
   //UndoState = gsc_SetUndoRecording(FALSE);

   for (int i = 0; i < LenAcadCmds; i++)
      //if (acedCmdUndefine(AcadCmds[i].LocalName.get_name(), 0) == 0) // redefine
      if (acedCommandS(RTSTR, _T("_.REDEFINE"), RTSTR, AcadCmds[i].LocalName.get_name(), 0) != RTNORM)
      { 
         gsc_set_echo(last_echo); 
         gsc_set_cmddia(OldCmdDia);
         //gsc_SetUndoRecording(UndoState); 
         GS_ERR_COD = eGSErrorExecutingCommand;
         return GS_BAD;
      }

   //gsc_SetUndoRecording(UndoState); // Ripristina situazione UNDO precedente
   gsc_set_echo(last_echo);
   gsc_set_cmddia(OldCmdDia);

   return GS_GOOD;
}


/*************************************************************************/
/*.doc C_CMDLIST::doFunc                                      <internal> */
/*
    This function is called to invoke the function which has the
    registerd function code that is obtained from  ads_getfuncode.  The
    function will return RTERROR if the function code is invalid, or
    RSERR if the invoked function fails to return RTNORM.  The value
    RSRSLT will be returned if the function code is valid and the
    invoked subroutine returns RTNORM.
*/
/*************************************************************************/
int C_CMDLIST::doFunc()
{
   int      val, rc = GS_BAD, old_reactor_abilit;
   FunTable *pfun;

   acedRetNil();

   if ((val = acedGetFunCode()) < 0) return RTERROR;

   do
   {
      if ((pfun = get_ptr_fun(val)) == NULL) break;

      // disattivo il controllo sul reattore
      old_reactor_abilit = gsc_disable_reactors();

		GS_ERR_COD = eGSNoError;
      
      if (GS_CURRENT_WRK_SESSION)
         // prova lo sbloccaggio di tutte le classi della sessione corrente che
         // erano in attesa di essere sbloccate ma che per motivi legati alla
         // multiutenza non è stato possibile sbloccare.
         GS_CURRENT_WRK_SESSION->UnlockWaitingClsList();

      // se NON si tratta di uno dei seguenti comandi o funzioni LISP
      if (pfun->fptr != &gs_freeze_WrkSession &&
          pfun->fptr != &gs_ddgetconfirm) 
         // controllo se ci sono oggetti da riallineare
         gsc_align_data_base();

      rc = (*(pfun->fptr))();          // richiamo la funzione

      // se non è una chiamata a funzione per attivare/disattivare i reattori
      if (!(gsc_strcmp(_T("gs_disable_reactors"), pfun->name.get_name()) == 0 ||
           gsc_strcmp(_T("gs_enable_reactors"), pfun->name.get_name()) == 0))
         // ripristino il controllo sul reattore come in precedenza
         if (old_reactor_abilit == GS_GOOD) gsc_enable_reactors();
   }
   while (0);

   if (rc == RTERROR || rc == GS_BAD)
      gsc_print_error();
   else
      if (rc == RTCAN || rc == GS_CAN)
         acutPrintf(gsc_msg(220)); // "\n*Annulla*\n"

   return ((rc == RTNORM) ? RSRSLT:RSERR);
}


/***********************************************************/
/*.doc C_CMDLIST::StartCmd                                 */
/*+
   Funzione che esegue delle azioni necessarie prima del comando GEOsim.
-*/  
/***********************************************************/
void C_CMDLIST::StartCmd(void)
{
   // disattivo il controllo sul reattore
   PrevReactorAbilit = gsc_disable_reactors();

   GS_ERR_COD = eGSNoError;
      
   if (GS_CURRENT_WRK_SESSION)
   {
      // prova lo sbloccaggio di tutte le classi della sessione corrente che
      // erano in attesa di essere sbloccate ma che per motivi legati alla
      // multiutenza non è stato possibile sbloccare.
      GS_CURRENT_WRK_SESSION->UnlockWaitingClsList();

      // controllo se ci sono oggetti da riallineare
      gsc_align_data_base();
   }
}


/***********************************************************/
/*.doc C_CMDLIST::EndCmd                                */
/*+
   Funzione che esegue delle azioni necessarie dopo
   l'esecuzione con successo del comando GEOsim.
-*/  
/***********************************************************/
void C_CMDLIST::EndCmd(void)
{
   // ripristino il controllo sul reattore come in precedenza
   if (PrevReactorAbilit == GS_GOOD) gsc_enable_reactors();
}


/***********************************************************/
/*.doc C_CMDLIST::ErrorCmd                                 */
/*+
   Funzione che esegue delle azioni necessarie dopo
   l'esecuzione con errore del comando GEOsim.
-*/  
/***********************************************************/
void C_CMDLIST::ErrorCmd(void)
{
   // ripristino il controllo sul reattore come in precedenza
   if (PrevReactorAbilit == GS_GOOD) gsc_enable_reactors();
   gsc_print_error();
}


/***********************************************************/
/*.doc C_CMDLIST::CancelCmd                                */
/*+
   Funzione che esegue delle azioni necessarie dopo
   l'esecuzione con annullamento del comando GEOsim.
-*/  
/***********************************************************/
void C_CMDLIST::CancelCmd(void)
{
   // ripristino il controllo sul reattore come in precedenza
   if (PrevReactorAbilit == GS_GOOD) gsc_enable_reactors();
   acutPrintf(gsc_msg(220)); // "\n*Annulla*\n"
}


//---------------------------------------------------------------------------//
// MEMORIZZA LA LISTA DEGLI HANDLES NOTIFICATI PER UNA CERTA CLASSE          //
// LA CLASSE DERIVA DA C_INT PERCHE' IL NUMERO INTERO DEFINISCE LA MODIFICA  //
// EFFETTUATA SUGLI OGGETTI CHE VIENE AGGIORNATA IN MODO INCREMENTALE        //
// CIOE' SE VIENE EFFETTUATA UNA MODIFICA DI GEOMETRIA (ES. COPY) E ,        //
// SUCCESSIVAMENTE UNA MODIFICA DI FAS (ES. CHANGE COLOR) IL NUMERO DEFINISCE //
// ENTRAMBE LE MODIFICHE.                                                    //
// - GEOMETRY_MOD (modifica di geometria)                                    //
// - FAS_MOD      (modifica di solo aspetto grafico)                         //
// - RECORD_MOD   (modifica ai dati alfanumerici, es. attrib. visib.)        //
// - UNKNOWN_MOD  (modifica di cui non si conosce la causa)                  //
//---------------------------------------------------------------------------//
class C_REACTOR_CLS : public C_INT
{
   friend class C_REACTOR_CLS_LIST;

   public :
      C_CLASS    *p_cls;
      C_STR_BTREE handle_list; // lista ordinata di handle

      C_REACTOR_CLS() { p_cls = NULL; }
      C_REACTOR_CLS(C_CLASS *in, const TCHAR *handle, const TCHAR *cmd)
      {
         p_cls = in;
         add_handle(handle, cmd);
      }
      virtual ~C_REACTOR_CLS() {} // chiama ~C_INT

      int add_handle(const TCHAR *handle, const TCHAR *cmd = NULL);
      int remove_handle(const TCHAR *handle);
      int get_selection(C_SELSET &SelSet, int inclusion = ERASED + 1);
      int subtract(C_REACTOR_CLS &ToSubtrac);
};

//---------------------------------------------------------------------------//
// MEMORIZZA LA LISTA DELLE CLASSI I CUI OGGETTI HANNO SUBITO NOTIFICHE      //
//---------------------------------------------------------------------------//
class C_REACTOR_CLS_LIST : public C_LIST
{
   public :
      C_REACTOR_CLS_LIST() {}
      ~C_REACTOR_CLS_LIST() {}  // chiama ~C_LIST
      
      int add_handle(C_CLASS *in, const TCHAR *handle, const TCHAR *cmd = NULL);
      int remove_handle(C_CLASS *in, const TCHAR *handle);
      int get_selection(C_SELSET &SelSet, int inclusion = ERASED + 1);
      int subtract(C_REACTOR_CLS_LIST &ToSubtrac);
};


/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/


C_GSDBReactor     *GSDBReactor = NULL;       // reattore al database grafico di ACAD
C_GSEditorReactor *GSEditorReactor = NULL;

int GS_CHECK_DBREACTOR = GS_GOOD; 
// se GS_GOOD -> reattori abilitati,  se GS_BAD -> reattori non abilitati,
// se GS_CAN -> reattori non abilitati e NON è più possibile riattivare i reattori 
// finchè ci si trova in una sessione di lavoro (all'uscita di una sessione di lavoro 
// i reattori vengono automaticamente riabilitati)

// Lista delle classi, dei comandi e degli oggetti che sono stati notificati dal reattore
// per i tre tipi di operazione: modifica, cancellazione e inserimento
static C_REACTOR_CLS_LIST GS_OBJS_INSERTED, GS_OBJS_ERASED, GS_OBJS_MODIFIED;
static C_REACTOR_CLS_LIST GS_OBJS_UNDELETED; // solo per simulazioni

// Gruppo di selezione degli oggetti sconosciuti modificati
// che a volte vengono modificati senza EED e senza LPT
// e successivamente vengono rimodificati, riacquisendo EED e LPT
static C_SELSET GS_UNKNOWN_OBJS_MODIFIED;

// Gruppo di selezione degli oggetti sconosciuti inseriti
// che a volte vengono inseriti (vedi break su 3DPOLY) senza EED e senza LPT
// e successivamente vengono modificati, riacquisendo EED e LPT
static C_SELSET GS_UNKNOWN_OBJS_INSERTED;

// Variabili globali per Import Entità
static int     SelClass = 0;
static int     ConvertEntToFas = GS_GOOD;
static int     OkImportEntGeosim = GS_BAD;


/*********************************************************/
/* PRIVATE FUNCTIONS */
/*********************************************************/

int gsc_change_text(double h_text, int val_layer, int val_save, 
						  C_CLASS *pCls, C_SELSET &SelSet);
int gsc_change_simb_attr(TCHAR *what, double scala, double altezza,int val_sc_attr,
                         int val_h_attr, int val_layer, int val_save, C_CLASS *pCls,
								 C_SELSET &SelSet);
int gsc_scal_attr_text(double v_sc_simb, double v_sc_attr, double v_hattr, double v_htext,
                       int f_sc_simb, int f_sc_attr, int f_h_attr, int f_h_text,
                       int f_layer, int f_saveobj, C_SELSET &SelSet);

int gsc_align_for_modified(int CounterToVideo = FALSE);
int gsc_align_for_erased(int CounterToVideo = FALSE);
int gsc_align_for_inserted(int CounterToVideo = FALSE);

int gsc_eraseMultiObjs(C_SELSET &SelSet);
int gsc_eraseSingleObj(ads_name ent);


/*********************************************************/

#define HELP_CONTEXT_STR_import  "Importaoggetti"

// Utilità
int getPolylineVertexValues(ads_name Polyline, long XVertex, presbuf *Vvalues);
int WhatIsVisibleVertexOnCurrentScreen(ads_name polyline, long NumVertOfPline,
                                       int TypeOfSequence, long *nVertex);


int printdxf(presbuf eb)
{
   int rt;

   while(TRUE)
   {
      if (eb == NULL) return RTNONE;
      if ((eb->restype >= 0) && (eb->restype <= 9))
         rt = RTSTR ;
      else if ((eb->restype >= 10) && (eb->restype <= 19))
         rt = RT3DPOINT;
      else if ((eb->restype >= 38) && (eb->restype <= 59))
         rt = RTREAL ;
      else if ((eb->restype >= 60) && (eb->restype <= 79))
         rt = RTSHORT ;
      else if ((eb->restype >= 210) && (eb->restype <= 239))
         rt = RT3DPOINT ;
      else if (eb->restype < 0)
         // Entity name (or other sentinel)
         rt = eb->restype;
      else
         rt = RTNONE;
      switch (rt)
      {
         case RTSHORT:
            acutPrintf(_T("(%d . %d)\n"), eb->restype, eb->resval.rint);
            break;
         case RTREAL:
            acutPrintf(_T("(%d . %0.3f)\n"), eb->restype, eb->resval.rreal);
            break;
         case RTSTR:
            acutPrintf(_T("(%d . \"%s\")\n"), eb->restype, eb->resval.rstring);
            break;
         case RT3DPOINT:
            acutPrintf(_T("(%d . %0.3f %0.3f %0.3f)\n"), eb->restype,
                       eb->resval.rpoint[X], eb->resval.rpoint[Y],
                       eb->resval.rpoint[Z]);
            break;
         case RTNONE:
            acutPrintf(_T("(%d . Unknown type)\n"), eb->restype);
            break;
         case -1:
         case -2: 
            /* First block entity */
            acutPrintf(_T("(%d . <Entity name: %8lx>)\n"),
                       eb->restype, eb->resval.rlname[0]);
      }   

      if (eb->rbnext != NULL)
         eb = eb->rbnext;
      else
         eb = NULL;
   }
   
   return eb->restype;
}


/*********************************************************/
/*.doc gsc_is_acad_main_entity <external> */
/*+
   Restituisce TRUE se il resbuf rappresenta un'intestazione di entita`
   di AutoCAD altrimenti restituisce FALSE.
   Parametri:
   ads_name ent;     oggetto grafico
-*/  
/*********************************************************/
int gsc_is_acad_main_entity(ads_name ent)
{
   resbuf *rb1,*rb2;
   int ret;
 
   if ((rb1 = acdbEntGet(ent)) == NULL)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   if ((rb2 = gsc_rbsearch(0, rb1)) == NULL)
      { acutRelRb(rb1); GS_ERR_COD = eGSInvRBType; return GS_BAD; }

   ret = gsc_is_acad_main_entity(rb2);
   acutRelRb(rb1);

   return ret;
}
int gsc_is_acad_main_entity(presbuf gigi)
{
   if (gigi == NULL)
      return FALSE;
   else
   {
      do
      {
         if (gigi->restype == 0)
            return gsc_is_acad_main_entity(gigi->resval.rstring);
   		if (gigi->rbnext != NULL)
	   	   gigi = gigi->rbnext;
	      else
		      return FALSE;
	   }
	   while (TRUE);
   }

   return FALSE;
}
int gsc_is_acad_main_entity(C_STRING &DXFType)
   { return gsc_is_acad_main_entity(DXFType.get_name()); }
int gsc_is_acad_main_entity(const TCHAR *DXFType)
{
   if (gsc_strcmp(DXFType, _T("3DFACE"))     == 0 ||
       gsc_strcmp(DXFType, _T("3DSOLID"))    == 0 ||
       gsc_strcmp(DXFType, _T("ARC"))        == 0 ||
       gsc_strcmp(DXFType, _T("CIRCLE"))     == 0 ||
       gsc_strcmp(DXFType, _T("DIMENSION"))  == 0 ||
       gsc_strcmp(DXFType, _T("ELLIPSE"))    == 0 ||
       gsc_strcmp(DXFType, _T("HATCH"))      == 0 ||
       gsc_strcmp(DXFType, _T("IMAGE"))      == 0 ||
       gsc_strcmp(DXFType, _T("INSERT"))     == 0 ||
       gsc_strcmp(DXFType, _T("LEADER"))     == 0 ||
       gsc_strcmp(DXFType, _T("LINE"))       == 0 ||
       gsc_strcmp(DXFType, _T("MLINE"))      == 0 ||
       gsc_strcmp(DXFType, _T("MPOLYGON"))   == 0 ||
       gsc_strcmp(DXFType, _T("MTEXT"))      == 0 ||
       gsc_strcmp(DXFType, _T("POINT"))      == 0 ||
       gsc_strcmp(DXFType, _T("POLYLINE"))   == 0 ||
       gsc_strcmp(DXFType, _T("LWPOLYLINE")) == 0 ||
       gsc_strcmp(DXFType, _T("RAY"))        == 0 ||
       gsc_strcmp(DXFType, _T("SHAPE"))      == 0 ||
       gsc_strcmp(DXFType, _T("SOLID"))      == 0 ||
       gsc_strcmp(DXFType, _T("SPLINE"))     == 0 ||
       gsc_strcmp(DXFType, _T("TEXT"))       == 0 ||
       gsc_strcmp(DXFType, _T("TOLERANCE"))  == 0 ||
       gsc_strcmp(DXFType, _T("TRACE"))      == 0 ||
       gsc_strcmp(DXFType, _T("XLINE"))      == 0)
      return TRUE;
   else
      return FALSE;
}


/*********************************************************/
/*.doc gsc_Cmd2Flag                           <internal> */
/*+
  Questa funzione ritorna un flag indicante il tipo di modifica
  che può aver causato un comando noto.
  Parametri:
  const TCHAR *cmd;      nome comando che ha invocato la notifica
-*/  
/*********************************************************/
static int gsc_Cmd2Flag(const TCHAR *cmd)
{  // in ordine alfabetico
   int        min = 0, max = LEN_CMD_FLAG - 1, i, prev = -1, res;
   static int Last = (LEN_CMD_FLAG - 1) / 2;

   if (!cmd) return UNKNOWN_MOD;

   // provo con l'ultimo comando cercato
   if (gsc_strcmp(CMD_FLAG[Last].cmd, cmd) == 0) return CMD_FLAG[Last].flag;

   i = (max + min) / 2;
   while (prev != i)
   {
      if ((res = gsc_strcmp(cmd, CMD_FLAG[i].cmd)) == 0)
      {
         Last = i;
         return CMD_FLAG[i].flag;
      }
      prev = i;
      
      if (res > 0) min = i;
      else if (res < 0) max = i;

      i = (max + min) / 2;
   }
   
   if (res > 0) // verifico l'ultimo elemento
      if (gsc_strcmp(CMD_FLAG[LEN_CMD_FLAG - 1].cmd, cmd) == 0)
      {
         Last = LEN_CMD_FLAG - 1;
         return CMD_FLAG[LEN_CMD_FLAG - 1].flag;
      }

   return UNKNOWN_MOD;
}


/////////////////////////////////////////////////////////////////////////
// INIZIO FUNZIONI CLASSE C_REACTOR_CLS                             
/////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc C_REACTOR_CLS::add_handle              <internal> */
/*+
  Questa funzione aggiunge un elemento alla lista degli handle
  senza duplicazioni. Inoltre setta la tipologia di modifica in 
  modo incrementale.
  Parametri:
  const TCHAR *handle;   handle dell'entità notificata
  const TCHAR *cmd;      nome comando che ha invocato la notifica

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_REACTOR_CLS::add_handle(const TCHAR *handle, const TCHAR *cmd)
{
   handle_list.add(handle);
   set_key(gsc_Cmd2Flag(cmd) | get_key());
   return GS_GOOD;
}


/*********************************************************/
/*.doc C_REACTOR_CLS::remove_handle           <internal> */
/*+
  Questa funzione cancella un elemento dalla lista degli handles.
  Parametri:
  const TCHAR *handle;   handle dell'entità notificata

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_REACTOR_CLS::remove_handle(const TCHAR *handle)
{
   return handle_list.remove(handle);
}


/*********************************************************/
/*.doc C_REACTOR_CLS::get_selection           <internal> */
/*+
  Questa funzione ottiene un gruppo di selezione dalla lista degli
  handle
  Parametri:
  C_SELSET &SelSet;     gruppo di selezione
  int inclusion;        Flag; se = ERASED la funzione ritorna un gruppo di selezione con 
                        inclusi eventuali oggetti grafici cancellati (default = ERASED + 1) 

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_REACTOR_CLS::get_selection(C_SELSET &SelSet, int inclusion)
{
   ads_name entity;
   C_BSTR   *p = (C_BSTR *) handle_list.go_top();

   SelSet.clear();
   while (p)
   {
      // ricavo l'entità 
      if (acdbHandEnt(p->get_name(), entity) == RTNORM)
         // verifico esistenza grafica
         if (gsc_IsErasedEnt(entity) == GS_GOOD)  // è stata cancellata
         {
            if (inclusion == ERASED)
               if (gsc_UnEraseEnt(entity) == GS_GOOD) // la rispristino
                  SelSet.add(entity);
         }
         else
            SelSet.add(entity);

      p = (C_BSTR *) handle_list.go_next();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_REACTOR_CLS::subtract                <internal> */
/*+
  Questa funzione elimina gli handle degli oggetti in comune 
  con un altro oggetto C_REACTOR_CLS.
  Parametri:
  C_REACTOR_CLS &ToSubtract;

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_REACTOR_CLS::subtract(C_REACTOR_CLS &ToSubtrac)
{
   C_BSTR *pToSubtrac;

   pToSubtrac = (C_BSTR *) ToSubtrac.handle_list.go_top();
   while (pToSubtrac)
   {
      remove_handle(pToSubtrac->get_name());
      pToSubtrac = (C_BSTR *) ToSubtrac.handle_list.go_next();
   }

   return GS_GOOD;
}


/////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI CLASSE C_REACTOR_CLS                                  //
// INIZIO FUNZIONI CLASSE C_REACTOR_CLS_LIST                           //
/////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc C_REACTOR_CLS_LIST::add_handle         <internal> */
/*+
  Questa funzione aggiunge un elemento di una classe GEOsim nota
  alla lista degli handle senza duplicazioni. Inoltre setta la 
  tipologia di modifica in modo incrementale.
  Parametri:
  C_CLASS *in;          Classe di GEOsim
  const TCHAR *handle;  Handle dell'entità notificata
  const TCHAR *cmd;     Nome comando che ha invocato la notifica

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_REACTOR_CLS_LIST::add_handle(C_CLASS *in, const TCHAR *handle, const TCHAR *cmd)
{
   C_REACTOR_CLS *p = (C_REACTOR_CLS *) get_cursor();

   // prima provo con l'attuale posizione del cursore
   if (p && p->p_cls == in)
      return p->add_handle(handle, cmd);

   p = (C_REACTOR_CLS *) get_head();
   while (p)
      if (p->p_cls == in) return p->add_handle(handle, cmd);
      else p = (C_REACTOR_CLS *) get_next();

   if (!p)
   {
      if ((p = new C_REACTOR_CLS(in, handle, cmd)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      add_tail(p);
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_REACTOR_CLS_LIST::remove_handle         <internal> */
/*+
  Questa funzione cancella un elemento dalla lista degli handles.
  Parametri:
  C_CLASS *in;          classe di GEOsim
  const TCHAR *handle;  handle dell'entità notificata

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_REACTOR_CLS_LIST::remove_handle(C_CLASS *in, const TCHAR *handle)
{
   C_REACTOR_CLS *p = (C_REACTOR_CLS *) get_cursor();

   // prima provo con l'attuale posizione del cursore
   if (p && p->p_cls == in)
      return p->remove_handle(handle);

   p = (C_REACTOR_CLS *) get_head();
   while (p)
      if (p->p_cls == in) return p->remove_handle(handle);
      else p = (C_REACTOR_CLS *) get_next();

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_REACTOR_CLS_LIST::get_selection      <internal> */
/*+
  Questa funzione ottiene un gruppo di selezione dalla lista degli
  handle
  Parametri:
  C_SELSET &SelSet;     gruppo di selezione
  int inclusion;        Flag; se = ERASED la funzione ritorna un gruppo di selezione con 
                        inclusi eventuali oggetti grafici cancellati (default = ERASED + 1) 

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_REACTOR_CLS_LIST::get_selection(C_SELSET &SelSet, int inclusion)
{
   C_REACTOR_CLS *p = (C_REACTOR_CLS *) get_cursor();
   C_SELSET      SelSet1;

   p = (C_REACTOR_CLS *) get_head();
   while (p)
   {
      if (p->get_selection(SelSet1, inclusion) == GS_BAD) return GS_BAD;
      if (SelSet.add_selset(SelSet1) == GS_BAD) return GS_BAD;

      p = (C_REACTOR_CLS *) get_next();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_REACTOR_CLS_LIST::subtract           <internal> */
/*+
  Questa funzione elimina gli handle degli oggetti in comune 
  con un altro oggetto C_REACTOR_CLS_LIST.
  Parametri:
  C_REACTOR_CLS_LIST &ToSubtract;

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_REACTOR_CLS_LIST::subtract(C_REACTOR_CLS_LIST &ToSubtrac)
{
   C_REACTOR_CLS *p, *pToSubtrac;

   p = (C_REACTOR_CLS *) get_head();
   while (p)
   {
      pToSubtrac = (C_REACTOR_CLS *) ToSubtrac.get_head();
      while (pToSubtrac)
      {
         if (p->p_cls == pToSubtrac->p_cls) p->subtract(*pToSubtrac);
         pToSubtrac = (C_REACTOR_CLS *) ToSubtrac.get_next();
      }

      p = (C_REACTOR_CLS *) get_next();
   }

   return GS_GOOD;
}


/////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI CLASSE C_REACTOR_CLS_LIST                             //
// INIZIO FUNZIONI CLASSE C_GSDBReactor                                //
/////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc C_GSDBReactor::objectErased <external> */
/*+
  Questa funzione viene invocata quando si verifica l'evento "cancellazione"
  di un oggetto dal database grafico corrente di ACAD
  Parametri:
  const AcDbDatabase *dwg;    puntatore al database grafico
  const AcDbObject *dbObj;    puntatore a oggetto
  Adesk::Boolean pEased;      flag (TRUE = Erased, FALSE = Unerased)
-*/  
/*********************************************************/
void C_GSDBReactor::objectErased(const AcDbDatabase *dwg, const AcDbObject *dbObj,
                                 Adesk::Boolean pErased)
{
   C_EED        ent_EED;
   C_CLASS      *pclass;
   TCHAR        handbuf[MAX_LEN_HANDLE];
   AcDbHandle   objHand;
   AcRxClass    *WhatIs;

   if (gsc_is_enable_reactors() == GS_BAD || GS_CURRENT_WRK_SESSION == NULL) return;
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd() == GS_BAD) return;
   if (acdbHostApplicationServices()->workingDatabase() != dwg) return;

   // considero solo oggetti grafici gestiti da GEOsim
   if ((WhatIs = dbObj->isA()) == NULL) return;

   // solo entità grafiche (INSERT, POLYLINE...)
   if (!gsc_is_acad_main_entity(WhatIs->dxfName())) return;

   // leggo identificatore della classe di appartenenza
   if (ent_EED.load(dbObj) == GS_BAD) return;

   // cerco classe
   if (ent_EED.cls == 0 ||
       (pclass = GS_CURRENT_WRK_SESSION->find_class(ent_EED.cls, ent_EED.sub)) == NULL ||
       pclass->is_extracted() == GS_BAD || pclass->ptr_id()->abilit != GSUpdateableData)
      return;

   dbObj->getAcDbHandle(objHand);
   objHand.getIntoAsciiBuffer(handbuf);

   if (pErased)
   {
      // aggiungo handle nella lista delle entità cancellate
      if (GS_OBJS_ERASED.add_handle(pclass, handbuf) == GS_BAD) return;

      #if defined(GSDEBUG) // se versione per debugging
         acutPrintf(_T("\nObject erased - Name=%s, Dxf Name=%s, Handle=%s"),
                    dbObj->isA()->name(), dbObj->isA()->dxfName(), handbuf);
      #endif
   }
   else // undo di una cancellazione
   {
      // la cancello dalla lista delle entità cancellate
      GS_OBJS_ERASED.remove_handle(pclass, handbuf);
      // aggiungo handle nella lista delle entità modificate
      GS_OBJS_MODIFIED.add_handle(pclass, handbuf);
      
      // se si tratta di link di sottoclassi (escludo i blocchi DA) 
      // devo creare un gruppo apposta per ripristinare poi anche la topologia
      if (gsc_strcmp(dbObj->isA()->dxfName(), _T("INSERT")) != 0 &&
          pclass->get_type() == TYPE_POLYLINE && 
          pclass->is_subclass() == GS_GOOD)
         // aggiungo handle nella lista delle entità ripristinate
         GS_OBJS_UNDELETED.add_handle(pclass, handbuf);

      #if defined(GSDEBUG) // se versione per debugging
         acutPrintf(_T("\nObject modified - Name=%s, Dxf Name=%s, Handle=%s"),
                    dbObj->isA()->name(), dbObj->isA()->dxfName(), handbuf);
      #endif
   }

   return;
}


/*********************************************************/
/*.doc C_GSDBReactor::objectModified <external> */
/*+
  Questa funzione viene invocata quando si verifica l'evento "modifica"
  di un oggetto dal database grafico corrente di ACAD
  Parametri:
  const AcDbDatabase *dwg;    puntatore al database grafico
  const AcDbObject *dbObj;    puntatore a oggetto
-*/  
/*********************************************************/
void C_GSDBReactor::objectModified(const AcDbDatabase *dwg, const AcDbObject *dbObj)
{
   ads_name     entity;
   AcDbObjectId objId;
   C_EED        ent_EED;
   C_CLASS      *pclass;
   C_STRING     DXFType;
   AcDbHandle   objHand;
   TCHAR        handbuf[MAX_LEN_HANDLE];
   C_RB_LIST    pRb;
   AcRxClass    *WhatIs;

   if (gsc_is_enable_reactors() == GS_BAD || GS_CURRENT_WRK_SESSION == NULL) return;
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd() == GS_BAD) return;
   if (acdbHostApplicationServices()->workingDatabase() != dwg) return;

   objId = dbObj->objectId();

   // considero solo oggetti grafici gestiti da GEOsim
   if ((WhatIs = dbObj->isA()) == NULL) return;
   DXFType = WhatIs->dxfName();

   if (DXFType.comp(_T("ATTRIB")) == 0)
   {
      // cerco il blocco padre
      if (acdbGetAdsName(entity, dbObj->ownerId()) != Acad::eOk) return;

      if (ent_EED.load(entity) != GS_GOOD)     // leggo identificatore della classe di appartenenza
      {
         GS_UNKNOWN_OBJS_MODIFIED.add(entity); // lo memorizzo in un gruppo speciale
         return;
      }

      // cerco classe
      if (ent_EED.cls == 0 ||
          (pclass = GS_CURRENT_WRK_SESSION->find_class(ent_EED.cls, ent_EED.sub)) == NULL ||
          pclass->is_extracted() == GS_BAD || pclass->ptr_id()->abilit != GSUpdateableData)
         return;

      gsc_enthand(entity, handbuf);

      #if defined(GSDEBUG) // se versione per debugging
         acutPrintf(_T("\nObject modified, Dxf Name=ATTDEF, Handle=%s"), handbuf);
      #endif
   }
   else // solo entità grafiche ("INSERT" o "POLYLINE" o "TEXT" ...)
   if (gsc_is_acad_main_entity(DXFType.get_name()))
   {
      // leggo identificatore della classe di appartenenza
      if (ent_EED.load(dbObj) == GS_BAD)
      {  // lo memorizzo in un gruppo speciale
         if (acdbGetAdsName(entity, objId) == Acad::eOk) GS_UNKNOWN_OBJS_MODIFIED.add(entity);
         return;
      }

      // cerco classe
      if (ent_EED.cls == 0 || 
          (pclass = GS_CURRENT_WRK_SESSION->find_class(ent_EED.cls, ent_EED.sub)) == NULL ||
          pclass->is_extracted() == GS_BAD || pclass->ptr_id()->abilit != GSUpdateableData)
         return;

      if (acdbGetAdsName(entity, objId) != Acad::eOk) return;

      dbObj->getAcDbHandle(objHand);
      objHand.getIntoAsciiBuffer(handbuf);

      #if defined(GSDEBUG) // se versione per debugging
         acutPrintf(_T("\nObject modified - Name=%s, Dxf Name=%s, Handle=%s"),
                    dbObj->isA()->name(), dbObj->isA()->dxfName(), handbuf);
      #endif
   }
   else return; // entità non valida

   struct resbuf rb;
   if (acedGetVar(_T("CMDNAMES"), &rb) == RTNORM && rb.restype == RTSTR)
   {  // aggiungo handle nella lista delle entità modificate
      GS_OBJS_MODIFIED.add_handle(pclass, handbuf, rb.resval.rstring);
      free(rb.resval.rstring);
   }
   else
      // aggiungo handle nella lista delle entità modificate
      GS_OBJS_MODIFIED.add_handle(pclass, handbuf);

   // se l'entità era stata inserita come nuova sconosciuta
   if (GS_UNKNOWN_OBJS_INSERTED.is_member(entity) == GS_GOOD)
   {
      // aggiungo handle nella lista delle entità inserite
      if (GS_OBJS_INSERTED.add_handle(pclass, handbuf) == GS_BAD) return;

      #if defined(GSDEBUG) // se versione per debugging
         acutPrintf(_T("\nObject new - Name=%s, Dxf Name=%s, Handle=%s"),
                    dbObj->isA()->name(), dbObj->isA()->dxfName(), handbuf);
      #endif

      GS_UNKNOWN_OBJS_INSERTED.subtract_ent(entity);
   }

   return;
}


/*********************************************************/
/*.doc C_GSDBReactor::objectAppended <external> */
/*+
  Questa funzione viene invocata quando si verifica l'evento "inserimento"
  di un oggetto dal database grafico corrente di ACAD
  Parametri:
  const AcDbDatabase *dwg;    puntatore al database grafico
  const AcDbObject *dbObj;    puntatore a oggetto
-*/  
/*********************************************************/
void C_GSDBReactor::objectAppended(const AcDbDatabase *dwg, const AcDbObject *dbObj)
{
   C_EED      ent_EED;
   C_CLASS    *pclass;
   TCHAR      handbuf[MAX_LEN_HANDLE];
   AcDbHandle objHand;
   AcRxClass  *WhatIs;
   ads_name   entity;

   if (gsc_is_enable_reactors() == GS_BAD || GS_CURRENT_WRK_SESSION == NULL) return;
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd() == GS_BAD) return;
   if (acdbHostApplicationServices()->workingDatabase() != dwg) return;

   // considero solo oggetti grafici gestiti da GEOsim
   if ((WhatIs = dbObj->isA()) == NULL) return;

   // solo entità grafiche (INSERT, POLYLINE...)
   if (!gsc_is_acad_main_entity(WhatIs->dxfName())) return;

   // leggo identificatore della classe di appartenenza
   if (ent_EED.load(dbObj) == GS_BAD)
   {  // lo memorizzo in un gruppo speciale
      if (acdbGetAdsName(entity, dbObj->objectId()) == Acad::eOk) GS_UNKNOWN_OBJS_INSERTED.add(entity);
      return;
   }

   // cerco classe
   if (ent_EED.cls == 0 ||
       (pclass = GS_CURRENT_WRK_SESSION->find_class(ent_EED.cls, ent_EED.sub)) == NULL ||
       pclass->is_extracted() == GS_BAD || pclass->ptr_id()->abilit != GSUpdateableData)
      return;

   dbObj->getAcDbHandle(objHand);
   objHand.getIntoAsciiBuffer(handbuf);

   // aggiungo handle nella lista delle entità inserite
   if (GS_OBJS_INSERTED.add_handle(pclass, handbuf) == GS_BAD) return;

   #if defined(GSDEBUG) // se versione per debugging
      acutPrintf(_T("\nObject new - Name=%s, Dxf Name=%s, Handle=%s"),
                 dbObj->isA()->name(), dbObj->isA()->dxfName(), handbuf);
   #endif

   return;
}


/*********************************************************/
/*.doc gsc_reactors_ClearObjs                 <external> */
/*+
  La funzione esegue elimina eventuali oggetti rimasti nelle liste dei reattori.
-*/  
/*********************************************************/
void gsc_reactors_ClearObjs(void)
{
   GS_OBJS_INSERTED.remove_all();
   GS_OBJS_MODIFIED.remove_all();
   GS_OBJS_ERASED.remove_all();
}


/*********************************************************/
/*.doc gsc_align_data_base <external> */
/*+
  La funzione esegue il riallineamento della banca dati.
  Parametri:

  Restituisce il il numero di entità trattate in caso di successo 
  altrimenti restituisce -1.
-*/  
/*********************************************************/
long gsc_align_data_base(void)
{
	long       mod, del, ins;
   int        result = GS_GOOD;
   C_LINK_SET LinkSet;

   if (!GS_CURRENT_WRK_SESSION) return 0;
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd() == GS_BAD) return 0;

   if (GS_UNKNOWN_OBJS_MODIFIED.length() > 0)
   {
      ads_name ent;
      long     i = 0;

      // Ciclo per verificare che gli oggetti siano veramente sconosciuti
      // (questo perchè durante le notifiche dei reattori può succedere che alcuni
      // oggetti perdano il collegamento di GEOsim e poi lo riacquisiscano con 2 notifiche 
      // successive)
      while (GS_UNKNOWN_OBJS_MODIFIED.entname(i, ent) == GS_GOOD)
         if (GS_CURRENT_WRK_SESSION->find_class(ent) != NULL)
            GS_UNKNOWN_OBJS_MODIFIED.subtract_ent(ent);
         else i++;

      if (LinkSet.erase(GS_UNKNOWN_OBJS_MODIFIED, GS_GOOD) == GS_BAD) return -1;
      GS_UNKNOWN_OBJS_MODIFIED.clear();
   }

   GS_UNKNOWN_OBJS_INSERTED.clear();

   // devo ripristinare la topologia dei link di simulazioni
   if (GS_OBJS_UNDELETED.get_count() > 0)
   {
      C_REACTOR_CLS *p = (C_REACTOR_CLS *) GS_OBJS_UNDELETED.get_head();
      C_SELSET      SelSet;

      while (p)
      {
         if (p->get_selection(SelSet) == GS_GOOD)
         {
            C_TOPOLOGY topo;
            ads_name   ent;
            long       i = 0, Key;
            C_CLASS    *pClass = p->p_cls;

            topo.set_type(TYPE_POLYLINE);    // tipologia di tipo rete
            topo.set_cls(GS_CURRENT_WRK_SESSION->find_class(pClass->ptr_id()->code)); // per classe madre

            while (SelSet.entname(i++, ent) == GS_GOOD)
               if (pClass->getKeyValue(ent, &Key) == GS_GOOD)
                  topo.editlink(pClass->ptr_id()->sub_code, Key, ent); // Aggiorno la topologia
         }

         p = (C_REACTOR_CLS *) GS_OBJS_UNDELETED.get_next();
      }
      GS_OBJS_UNDELETED.remove_all();
   }

   ins = GS_OBJS_INSERTED.get_count();
   mod = GS_OBJS_MODIFIED.get_count();
   del = GS_OBJS_ERASED.get_count();
   if (ins > 0 || mod > 0 || del > 0)
   {
      int      result_mod = GS_GOOD, result_del = GS_GOOD, result_ins = GS_GOOD;
      C_SELSET SelSet, SelSet1;

      // resetto il gruppo di selezione degli oggetti non riallineabili
      GEOsimAppl::REFUSED_SS.clear();

      // elimino dai nuovi quelli cancellati perchè nel caso di cancellazione
      // dopo l'inserimento non avrebbe efficacia in quanto l'inserimento duplica l'oggetto
      GS_OBJS_INSERTED.subtract(GS_OBJS_ERASED);
      
      // creo un unico gruppo di selezione
      if (GS_OBJS_INSERTED.get_selection(SelSet) == GS_BAD) return -1;
      
      // rimuovo dagli oggetti inseriti i blocchi DA che si riferiscono ad oggetti non
      // modificabili (e che quindi varierebbero il fattore di aggregazione)
      if (SelSet.length() > 0)
      { 
         ads_name ent;
         long     i = 0, Key;
         C_CLASS  *pCls;
         int      ToLock = GS_GOOD, TryToExtractPartialEnt = GS_GOOD, RecalculateSS = FALSE;
         TCHAR    Handle[MAX_LEN_HANDLE];
         C_STRING ODTableName;

         while (SelSet.entname(i++, ent) == GS_GOOD)
            if (gsc_is_DABlock(ent) == GS_GOOD)
               // Ricavo il puntatore alla classe e gli oggetti grafici
               // dell'entità relativa all'oggetto in questione che deve collegamento a DB
               if ((pCls = GS_CURRENT_WRK_SESSION->find_class(ent)) &&
                   pCls->ptr_info() != NULL)
               {
                  gsc_getODTableName(GS_CURRENT_WRK_SESSION->get_PrjId(),
                                     pCls->ptr_id()->code, pCls->ptr_id()->sub_code,
                                     ODTableName);

                  if (gsc_getIDfromODTable(ent, ODTableName, &Key) == GS_GOOD &&
                      pCls->get_SelSet(Key, SelSet1) == GS_GOOD)
                  {
                     // se Entità GEOsim NON aggiornabile (con tentativo di estrazione totale)
                     if (pCls->is_updateableSS(Key, SelSet1, NULL, ToLock, TryToExtractPartialEnt) == GS_BAD)
                        if (gsc_enthand(ent, Handle) == GS_GOOD)
                        {
                           C_LINK GSLink;
                           C_EED  GSeed;

                           GS_OBJS_INSERTED.remove_handle(pCls, Handle);
                           RecalculateSS = TRUE;
                           GSeed.clear(ent);
                        }
                  }
               }

         if (RecalculateSS == TRUE) 
            // devo ricreare il gruppo di selezione SelSet
            if (GS_OBJS_INSERTED.get_selection(SelSet) == GS_BAD) return -1;
      }
      // fine ciclo per togliere i blocchi DA inseriti relativi ad entità non modificabili

      if (GS_OBJS_MODIFIED.get_selection(SelSet1) == GS_BAD) return -1;
      if (SelSet.add_selset(SelSet1) == GS_BAD) return -1;
      if (GS_OBJS_ERASED.get_selection(SelSet1, ERASED) == GS_BAD) return -1;
      if (SelSet.add_selset(SelSet1) == GS_BAD) return -1;
      SelSet1.clear();

      // per ciascun oggetto deve essere allineato il link 
      // nella tabella GS_LINK che in caso di UNDO o REDO potrebbe variare
      if (LinkSet.RefreshSS(SelSet, GS_GOOD) == GS_BAD) return -1;
      SelSet.clear();

      acutPrintf(gsc_msg(311)); // "\nRiallineamento banca dati..."

      // controllo se ci sono oggetti da riallineare in inserimento
      // N.B. questa funzione deve essere la prima delle "gsc_align_for_..."
      if (ins > 0)
      {
         acutPrintf(_T("\n\n%s..."), gsc_msg(339)); // "Inserimento entità"
         result_ins = gsc_align_for_inserted(GS_GOOD);
      }

      // controllo se ci sono oggetti da riallineare in modifica
      // N.B. questa funzione deve essere la seconda delle "gsc_align_for_..."
      if (mod > 0)
      {
         acutPrintf(_T("\n\n%s..."), gsc_msg(340)); // "Modifica entità"
         result_mod = gsc_align_for_modified(GS_GOOD);
      }

      // controllo se ci sono oggetti da riallineare in cancellazione
      // N.B. questa funzione deve essere l'ultima delle "gsc_align_for_..."
      if (del > 0)
      {
         acutPrintf(_T("\n\n%s..."), gsc_msg(342)); // "Cancellazione entità"
         result_del = gsc_align_for_erased(GS_GOOD);
      }

      if (result_ins == GS_BAD || result_mod == GS_BAD || result_del == GS_BAD)
         result = GS_BAD;

      acutPrintf(gsc_msg(111)); // "\nTerminato.\n"
   }

   return ins + mod + del;
}


/*********************************************************/
/*.doc gsc_align_for_modified <external> */
/*+
  Questa funzione viene invocata quando si è verificato l'evento "modifica"
  di un oggetto dal database grafico corrente di ACAD. La funzione esegue
  il riallineamento della banca dati.
  Parametri:
  int CounterToVideo;    Flag per visualizzare il contatore di entità elaborate (default=GS_BAD)

  Restituisce il GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_align_for_modified(int CounterToVideo)
{
   C_SELSET      SelSet, ToAlign, PossibleIsolateNodes;
   C_REACTOR_CLS *pReactorCls;

   if (!GS_CURRENT_WRK_SESSION) return GS_GOOD;
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd() == GS_BAD) return GS_GOOD;

   // ciclo per ogni classe notificata
   while ((pReactorCls = (C_REACTOR_CLS *) GS_OBJS_MODIFIED.get_head()) != NULL)
   {
      // ricavo un gruppo di selezione
      if (pReactorCls->get_selection(SelSet) == GS_GOOD)
      {
         // se si tratta di entità delle classi simulazione 
         if (pReactorCls->p_cls->is_subclass() == GS_GOOD)
         {  // allineo la topologia (in caso di spostamento)
            gsc_topo_align_for_modified(SelSet, ToAlign, &(GEOsimAppl::REFUSED_SS), CounterToVideo);
            if (pReactorCls->p_cls->get_type() == TYPE_NODE)
               //               selset,change_fas,AttribValuesFromVideo,SS,CounterToVideo,tipo modifica
               gsc_class_align(ToAlign, GS_BAD, GS_GOOD, &(GEOsimAppl::REFUSED_SS),
                               CounterToVideo, GEOMETRY_MOD);

            PossibleIsolateNodes.add_selset(ToAlign);
         }   

         //              selset,change_fas,AttribValuesFromVideo,CounterToVideo,tipo modifica
         gsc_class_align(SelSet, GS_BAD, GS_GOOD, &(GEOsimAppl::REFUSED_SS),
                         CounterToVideo, pReactorCls->get_key());
      }

      GS_OBJS_MODIFIED.remove_at(); // la cancello dalla lista delle modificate
   }

   if (PossibleIsolateNodes.length() > 0)
      gsc_verify_topo_on_align_for_node(PossibleIsolateNodes, CounterToVideo);
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_align_for_erased <external> */
/*+
  Questa funzione viene invocata quando si è verificato l'evento "cancellazione"
  di un oggetto dal database grafico corrente di ACAD. La funzione esegue
  il riallineamento della banca dati.
  Parametri:
  int CounterToVideo;    Flag per visualizzare il contatore di entità elaborate (default=GS_BAD)

  Restituisce il GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_align_for_erased(int CounterToVideo)
{
   C_STR_LIST    HandleList;
   C_SELSET      SelSet, ToAlign, PossibleIsolateNodes, LinkToAlign;
   C_REACTOR_CLS *pReactorCls;

   if (!GS_CURRENT_WRK_SESSION) return GS_GOOD;
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd() == GS_BAD) return GS_GOOD;

   // ciclo per ogni classe notificata
   pReactorCls = (C_REACTOR_CLS *) GS_OBJS_ERASED.get_head();
   while (pReactorCls)
   {
      // ricavo un gruppo di selezione ripristinando gli oggetti a video 
      // (se fossero stati cancellati, saranno ricancellati successivamente)
      if (pReactorCls->get_selection(SelSet, ERASED) == GS_GOOD)
      {
         // se si tratta di entità delle classi simulazione 
         if (pReactorCls->p_cls->is_subclass() == GS_GOOD)
         {  // lascio solo la lista di handle delle principali
            long     i = 0;
            ads_name ent;
            TCHAR    Handle[MAX_LEN_HANDLE];

            while (SelSet.entname(i++, ent) == GS_GOOD)
               if (gsc_is_DABlock(ent) == GS_GOOD && gsc_enthand(ent, Handle) == GS_GOOD)
                  pReactorCls->remove_handle(Handle);
         }

         //              selset,change_fas,AttribValuesFromVideo,SS,CounterToVideo,tipo modifica
         gsc_class_align(SelSet, GS_BAD, GS_BAD, &(GEOsimAppl::REFUSED_SS), 
                         CounterToVideo, ERASE_MOD);
      }

      pReactorCls = (C_REACTOR_CLS *) GS_OBJS_ERASED.get_next();
   }

   // ciclo per ogni classe di simulazione notificata
   pReactorCls = (C_REACTOR_CLS *) GS_OBJS_ERASED.get_head();
   while (pReactorCls)
   {
      // se si tratta di entità delle classi simulazione 
      if (pReactorCls->p_cls->is_subclass() == GS_GOOD)
      {  // allineo la topologia (in caso di cancellazione)
         gsc_topo_align_for_erased(pReactorCls->handle_list, ToAlign, &(GEOsimAppl::REFUSED_SS), CounterToVideo);
         if (pReactorCls->p_cls->get_type() == TYPE_POLYLINE)
            PossibleIsolateNodes.add_selset(ToAlign);
         else
            LinkToAlign.add_selset(ToAlign);
      }    

      pReactorCls = (C_REACTOR_CLS *) GS_OBJS_ERASED.get_next();
   }
   
   if (PossibleIsolateNodes.length() > 0)
      gsc_verify_topo_on_align_for_node(PossibleIsolateNodes, CounterToVideo);

   if (LinkToAlign.length() > 0)
      //              selset,change_fas,AttribValuesFromVideo,SS,CounterToVideo,tipo modifica
      gsc_class_align(LinkToAlign, GS_BAD, GS_BAD, &(GEOsimAppl::REFUSED_SS), 
                      CounterToVideo, RECORD_MOD);

   GS_OBJS_ERASED.remove_all();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_align_for_inserted <external> */
/*+
  Questa funzione viene invocata quando si è verificato l'evento "inserimento"
  di un oggetto nel database grafico corrente di ACAD. La funzione esegue
  il riallineamento della banca dati.
  Parametri:
  int CounterToVideo;    Flag per visualizzare il contatore di entità elaborate (default=GS_BAD)

  Restituisce il GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_align_for_inserted(int CounterToVideo)
{
   C_SELSET      SelSet;
   C_REACTOR_CLS *pReactorCls;

   if (!GS_CURRENT_WRK_SESSION) return GS_GOOD;
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd() == GS_BAD) return GS_GOOD;

   // ciclo per ogni classe notificata
   pReactorCls = (C_REACTOR_CLS *) GS_OBJS_INSERTED.get_head();
   while (pReactorCls)
   {
      // le entità lineari delle classi simulazione vanno trattate dopo aver inserito tutti
      // le entità puntuali delle stesse classi
      if (pReactorCls->p_cls->is_subclass() == GS_GOOD &&
          pReactorCls->p_cls->get_type() == TYPE_POLYLINE)
      {
         pReactorCls = (C_REACTOR_CLS *) GS_OBJS_INSERTED.get_next();
         continue;
      }

      if (pReactorCls->get_selection(SelSet) == GS_GOOD)
         pReactorCls->p_cls->align_for_inserted(SelSet, CounterToVideo);

      // le entità puntuali delle classi simulazione che per esistere necessitano di 
      // essere topologicamente connesse ad altre entità dovranno essere ricontrollate
      // dopo il ciclo di inserimento delle entità lineari delle classi simulazione
      if (pReactorCls->p_cls->is_subclass() == GS_GOOD &&
          pReactorCls->p_cls->get_type() == TYPE_NODE &&
          pReactorCls->p_cls->ptr_connect_list()->is_to_be_connected() == GS_GOOD)
      {  // carico la lista degli handle dei nuovi oggetti inseriti da 
         // align_for_inserted per le sottoclassi
         long     i = 0;
         ads_name entity;
         TCHAR    Handle[MAX_LEN_HANDLE];

         while (SelSet.entname(i++, entity) == GS_GOOD)
         {
            gsc_enthand(entity, Handle);
            pReactorCls->add_handle(Handle);
         }

         pReactorCls = (C_REACTOR_CLS *) GS_OBJS_INSERTED.get_next();
         continue;
      }

      GS_OBJS_INSERTED.remove_at(); // la cancello dalla lista delle inserite
      pReactorCls = (C_REACTOR_CLS *) GS_OBJS_INSERTED.get_cursor();
   }

   // dopo aver inserito le entità puntuali delle classi simulazione
   // inserisco le entità lineari delle stesse classi
   pReactorCls = (C_REACTOR_CLS *) GS_OBJS_INSERTED.get_head();
   while (pReactorCls)
   {
      if (pReactorCls->p_cls->is_subclass() != GS_GOOD ||
          pReactorCls->p_cls->get_type() != TYPE_POLYLINE)
      {
         pReactorCls = (C_REACTOR_CLS *) GS_OBJS_INSERTED.get_next();
         continue;
      }

      if (pReactorCls->get_selection(SelSet) == GS_GOOD)
            pReactorCls->p_cls->align_for_inserted(SelSet, CounterToVideo);

      GS_OBJS_INSERTED.remove_at(); // la cancello dalla lista delle inserite
      pReactorCls = (C_REACTOR_CLS *) GS_OBJS_INSERTED.get_cursor();
   }

   // dopo aver inserito le entità puntuali delle classi simulazione ed anche
   // le entità lineari delle stesse classi si deve controllare che tutti i nodi
   // inseriti che per esistere necessitano di essere topologicamente connesse
   // ad altre entità non siano isolate
   pReactorCls = (C_REACTOR_CLS *) GS_OBJS_INSERTED.get_head();
   while (pReactorCls)
   {
      if (pReactorCls->p_cls->is_subclass() != GS_GOOD ||
          pReactorCls->p_cls->get_type() != TYPE_NODE ||
          pReactorCls->p_cls->ptr_connect_list()->is_to_be_connected() != GS_GOOD)
      {
         pReactorCls = (C_REACTOR_CLS *) GS_OBJS_INSERTED.get_next();
         continue;
      }

      if (pReactorCls->get_selection(SelSet) == GS_GOOD)
         gsc_verify_topo_on_align_for_node(SelSet, CounterToVideo);

      GS_OBJS_INSERTED.remove_at(); // la cancello dalla lista delle inserite
      pReactorCls = (C_REACTOR_CLS *) GS_OBJS_INSERTED.get_cursor();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_enable_reactors <external> */
/*+
  Questa funzione abilita i reattori di GEOsim.
  
  Ritorna lo stato precedente di abilitazione.
-*/  
/*********************************************************/
int gs_enable_reactors(void)
{
   if (gsc_enable_reactors() == GS_CAN) acedRetNil(); // operazione non possibile
   else acedRetT();
   
   return RTNORM;
}
int gsc_enable_reactors(void)
{
   int old = GS_CHECK_DBREACTOR;

   // se il reattore era in stato GS_CAN e ci si trova in una sessione di lavoro
   // non è più possibile riattivarlo
   if (old == GS_CAN)
   {
      if (GS_CURRENT_WRK_SESSION) return GS_CAN;
      else old = GS_BAD;  // forzo lo stato di disattivazione reattore
   }
   GS_CHECK_DBREACTOR = GS_GOOD;

   return old;
}


/*********************************************************/
/*.doc gsreactorsoff <external> */
/*+
  Questa funzione disabilita i reattori di GEOsim.
  
  N.B.: Se ci si trova in una sessione di lavoro, dopo questo comando non sarà
        possibile salvare le modifiche agli oggetti GEOsim.
-*/  
/*********************************************************/
void gsreactorsoff(void)
{
   int result = GS_BAD;

   GEOsimAppl::CMDLIST.StartCmd();

   acutPrintf(gsc_msg(526)); // "\nAttenzione, confermando questa operazione, GEOsim potrebbe generare dei"
   acutPrintf(gsc_msg(527)); // "\nmalfunzionamenti e non sarà più possibile salvare i dati della eventuale"
   acutPrintf(gsc_msg(528)); // "\nsessione di lavoro corrente !!!"
   if (gsc_getconfirm(gsc_msg(529), &result, GS_BAD) == GS_BAD) // "\nConfermi l'operazione."
      return GEOsimAppl::CMDLIST.EndCmd();
   if (result == GS_GOOD) gsc_reactors_off();

   return GEOsimAppl::CMDLIST.EndCmd();
}


/*********************************************************/
/*.doc gsc_reactors_off <external> */
/*+
  Questa funzione disabilita i reattori di GEOsim.
  
  N.B.: Se ci si trova in una sessione di lavoro, dopo questo comando non sarà
        possibile salvare le modifiche agli oggetti GEOsim.
-*/  
/*********************************************************/
int gsc_reactors_off(void)
{
   int old = GS_CHECK_DBREACTOR;

   GS_CHECK_DBREACTOR = GS_CAN;

   return old;
}


/*********************************************************/
/*.doc gs_is_reactorsoff <external> */
/*+
  Questa funzione verifica se i reattori di GEOsim  sono 
  stati disabilitati definitivamente oppure no.
-*/  
/*********************************************************/
int gs_is_reactorsoff(void)
{
   if (gsc_disable_reactors() == GS_CAN) acedRetNil(); 
   else acedRetT();
   
   return RTNORM;
}

/*********************************************************/
/*.doc gsc_disable_reactors <external> */
/*+
  Questa funzione disabilita i reattori di GEOsim.
-*/  
/*********************************************************/
int gs_disable_reactors(void)
{
   if (gsc_disable_reactors() == GS_CAN) acedRetNil(); // operazione non possibile
   else acedRetT();
   
   return RTNORM;
}
int gsc_disable_reactors(void)
{
   int old = GS_CHECK_DBREACTOR;

   // se il reattore era in stato GS_CAN e ci si trova in una sessione di lavoro
   // non è più possibile disattivarlo in stato di GS_BAD
   if (old == GS_CAN)
   {
      if (GS_CURRENT_WRK_SESSION) return GS_CAN;
      else old = GS_BAD;  // forzo lo stato di disattivazione reattore
   }

   GS_CHECK_DBREACTOR = GS_BAD;

   return old;
}


/*********************************************************/
/*.doc gsc_is_enable_reactors <external> */
/*+
  Questa funzione ritorna GS_GOOD se sono abilitati i reattori di GEOsim
  altrimenti ritorna GS_BAD.
-*/  
/*********************************************************/
int gsc_is_enable_reactors(void)
{
   return (GS_CHECK_DBREACTOR == GS_GOOD) ? GS_GOOD : GS_BAD;
}


/*******************************************************/
/*.doc int gs_import()                                 */
/*+                                                                   
  Funzione lisp per importare degli oggetti in GEOsim.
  Parametri:
  (<cls><import type><ss><importa ogg. GEOsim><fas di default>[<new fas>[<LPT info>[<OD info>[<import block attr>]]]]
  oppure
  (<cls><import type><Path>[<OLE_DB conn><key>]

  <cls>                 = Codice classe destinazione
  <import type>         = 0 -> da file, 1->da grafica

  <ss>                  = gruppo di selezione
  <Path>                = percorso completo file di testo

  <importa ogg. GEOsim> = 1 -> importa anche oggetti già di GEOsim
                          0 -> scarta oggetti già di GEOsim
  <fas di default>      = 1 -> applica la fas di default
                          0 -> lascia la fas immutata
  <new fas>             = (BitForChangeFAS (<FAS>))
  <LPT info>            = (<LPT list><del LPT>) || nil
  <OD info>             = (<OD list><del OD>) || nil
  <import block attr>   = 1 -> importa gli attributi dei blocchi
                          0 -> non importa gli attributi dei blocchi

  <LPT list> = lista dei LPT che possono essere utilizzati per 
               la lettura dei dati da DB esterno (<LP1><LPT2>...<LPTn>)
  <del LPN>  = 1 -> cancella il LPN usato per la lettura, dall'oggetto importato
               0 -> non cancella il LPN

  <OD list> = lista delle tabelle OD che possono essere utilizzate per 
              la lettura dei dati OD (<ODTable1><ODTable2>...<ODTablen>)
  <del OD>  = 1 -> cancella i dati OD usati per la lettura, dall'oggetto importato
              0 -> non cancella i dati OD

  <OLE_DB conn> = (("UDL_FILE" <Connection>) ("UDL_PROP" <Properties>) ("TABLE_REF" <TableRef>))
  <Connection> = <file UDL> | <stringa di connessione>
  <Properties> = stringa delle proprietà | ((<prop1><value>)(<prop1><value>)...)
  <TableRef>   = riferimento completo tabella | (<cat><schema><tabella>)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD. 
-*/  
/*********************************************************/
int gs_import()
{
   presbuf    arg, p;
   ads_name   selectionSet;
   int        importType = GS_BAD, CodClass, ObjGEOsimLoad = GS_BAD, doApplyFAS = GS_BAD;
   int        RemoveLPN = GS_BAD, RemoveOD = GS_BAD;
   C_STRING   filePath;
   C_CLASS    *pClass = NULL;
   C_STR_LIST LPNList, ODList;
   bool       ReadAttrBlk = false;

   // Leggo i parametri da lisp
   acedRetNil();
   arg = acedGetArgs();

   // Verifico che ci sia una sessione corrente
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return RTERROR; }

   // Il primo parametro è il codice della classe di destinazione 
   if ((p = gsc_nth(0, arg)) == NULL || gsc_rb2Int(p, &CodClass) == GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   // Il secondo parametro è la tipologia di importazione 
   if ((p = gsc_nth(1, arg)) == NULL || gsc_rb2Int(p, &importType) == GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (importType == 0) // import da file testo
   {
      C_DBCONNECTION *pConn = NULL;
      C_STRING       Table, KeyAttr;

      // Il terzo è la path completa del file di caratteristiche oggetti
      if ((p = gsc_nth(2, arg)) == NULL || p->restype != RTSTR)
         { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
      filePath.set_name(p->resval.rstring);
      
      // Il quarto opzionale esiste se si deve leggere un valore chiave dal file
      // e consiste in una connessione ole-db e un nome di attributo chiave contento
      // nella tabella indicata dalla connessione
      if ((p = gsc_nth(3, arg)) == NULL || p->restype != RTNIL)
      {
         // Legge nella lista dei parametri i riferimenti alla tabella
         if ((pConn = gsc_getConnectionFromLisp(p, &Table)) == NULL) return RTERROR;
         // Il quinto parametro è il nome dell'attributo chiave
         if ((p = gsc_nth(4, arg)) == NULL || p->restype != RTSTR)
            { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
         KeyAttr = p->resval.rstring;
      }

      // Ricavo il puntatore alla classe
      if ((pClass = GS_CURRENT_WRK_SESSION->find_class(CodClass, 0)) == NULL) return RTERROR; 
      // Richiamo la funzione base di import dati
      if (gsc_importFromFile(pClass, filePath, pConn, &Table, &KeyAttr) == GS_BAD) 
         return RTERROR; 
   }
   else // import dalla grafica 
   {
      long  BitForChangeFAS;
      C_FAS FAS;

      // Il terzo parametro è il gruppo di selezione
      if ((p = gsc_nth(2, arg)) == NULL || p->restype != RTPICKS)
         { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
      ads_name_set(p->resval.rlname, selectionSet);

      // Il quarto parametro è il flag che stabilisce se importare le entità di GEOsim
      if ((p = gsc_nth(3, arg)) == NULL || gsc_rb2Int(p, &ObjGEOsimLoad) == GS_BAD)
         { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

      // Il quinto parametro è il flag che stabilisce se applicare la FAS di destinazione
      if ((p = gsc_nth(4, arg)) == NULL || gsc_rb2Int(p, &doApplyFAS) == GS_BAD)
         { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

      // Ricavo il puntatore alla classe
      if ((pClass = GS_CURRENT_WRK_SESSION->find_class(CodClass, 0)) == NULL) return RTERROR;

      // leggo quali caratteristiche grafiche si possono modificare
      BitForChangeFAS = pClass->what_is_graph_updateable();

      // se superficie aggiungo l'opzione per inserire un riempimento 
      // (che se è controllato da una funzione grafica verrebbe inibito)
      if (pClass->get_type() == TYPE_SURFACE) BitForChangeFAS = BitForChangeFAS | GSHatchNameSetting;
      pClass->ptr_fas()->copy(&FAS);

      // Il sesto parametro è opzionale e rappresenta le informazioni
      // per alterare le caratteristiche grafiche
      if ((p = gsc_nth(5, arg)) != NULL)
      {
         if (p->restype == RTLB) // aperta tonda
         {
            if (!(p = p->rbnext)) return RTERROR;
            if (p->restype != RTNIL)
               if (gsc_rb2Lng(p, &BitForChangeFAS) == GS_BAD) return RTERROR;

            if (!(p = p->rbnext)) return RTERROR;
            if (p->restype != RTNIL)
               FAS.from_rb(p);
         }

         // Il settimo parametro è opzionale e rappresenta le informazioni
         // per importare dati via LPT
         if ((p = gsc_nth(6, arg)) != NULL)
         {
            if (p->restype == RTLB)
            {
               if ((p = p->rbnext) == NULL)
                  { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

               // Rappresenta la lista dei LPN validi
               if (p->restype == RTLB)
               {
                  if (LPNList.from_rb(p) == GS_BAD) return RTERROR;
                  p = gsc_scorri(p);
               }

               // Flag che determina se cancellare i LPN
               if ((p = p->rbnext) == NULL || gsc_rb2Int(p, &RemoveLPN) == GS_BAD)
                  { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
            }

            // L'ottavo è opzionale e rappresenta le informazioni
            // per importare dati OD
            if ((p = gsc_nth(7, arg)) != NULL)
            {
               if (p->restype == RTLB)
               {
                  if ((p = p->rbnext) == NULL)
                     { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

                  // Rappresenta la lista delle tabelle OD valide
                  if (p->restype == RTLB)
                  {
                     if (ODList.from_rb(p) == GS_BAD) return RTERROR;
                     p = gsc_scorri(p);
                  }

                  // Flag che determina se cancellare i dati OD
                  if ((p = p->rbnext) == NULL || gsc_rb2Int(p, &RemoveOD) == GS_BAD)
                     { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
               }

               // Il nono parametro è opzionale e rappresenta l'opzione di lettura dei
               // valori degli attributi dei blocchi da importare nel db
               if ((p = gsc_nth(8, arg)) != NULL && p->restype == RTSHORT)
                  if (p->resval.rint == 1) ReadAttrBlk = true;
            }
         }
      }

      // Se è stato stabilito che non si devono applicare variazioni grafiche
      if (doApplyFAS != GS_GOOD) BitForChangeFAS = GSNoneSetting;

      // Richiamo la funzione base di import dati
      if (gsc_importFromGraph(selectionSet, pClass, ObjGEOsimLoad, 
                              BitForChangeFAS, &FAS,
                              &LPNList, RemoveLPN, 
                              &ODList, RemoveOD,
                              ReadAttrBlk) == GS_BAD) return RTERROR; 
   }
   
   acedRetT();

   return RTNORM;
}


//-----------------------------------------------------------------------//
//////////////////  COMANDI DI AUTOCAD      INIZIO    /////////////////////
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc gserase                                <external> */
/*+
   Implementa il comando di cancellazione di una o più entità.
-*/  
/*********************************************************/
void gserase()
{
   C_SELSET SelSet;
   ads_name ss;
   long     PrevLenSelSet, LenSelSet;
   int      res;

   // per sicurezza verifico se esiste una sessione corrente non in attesa
   if (!GS_CURRENT_WRK_SESSION || GS_CURRENT_WRK_SESSION->get_status() == WRK_SESSION_WAIT)
   {
      if (gsc_ssget(NULL, NULL, NULL, NULL, SelSet) != RTNORM) { gsc_print_error(); return; }
      if (SelSet.length() == 0) return;

      SelSet.get_selection(ss);
      if (gsc_callCmd(_T("_.ERASE"), RTPICKS, ss, RTSTR, GS_EMPTYSTR, 0) != RTNORM)
         { gsc_print_error(); return; }
      return;
   }

   // verifico l'abilitazione dell'utente
   if (gsc_check_op(opDelEntity) == GS_BAD) { gsc_print_error(); return; }
   // verifico che l'area sia attiva e modificabile;
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD) { gsc_print_error(); return; }

   // seleziona le entità
   if (gsc_ssget(NULL, NULL, NULL, NULL, SelSet) != RTNORM) { gsc_print_error(); return; }
   PrevLenSelSet = SelSet.length();
   // Filtro le entità aggiornabili bloccandole
   if (SelSet.intersectUpdateable(UPDATEABLE, GS_GOOD, GS_GOOD) == GS_BAD)
      { gsc_print_error(); return; }
   
   LenSelSet = SelSet.length();
   if ((PrevLenSelSet - LenSelSet) > 0)
      // "\nEscluse dall'operazione %ld entità su %ld in quanto non modificabili."
      acutPrintf(gsc_msg(434), PrevLenSelSet - LenSelSet, PrevLenSelSet);

   if (LenSelSet == 0) return;

   GEOsimAppl::CMDLIST.StartCmd();

   // se l'utente ha selezionato una singola entità vado a verificare 
   // i gruppi
   if (LenSelSet == 1)
   {
      ads_name ent;

      SelSet.entname(0, ent);
      res = gsc_eraseSingleObj(ent);
   }
   else
      res = gsc_eraseMultiObjs(SelSet);

   if (res == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();
   else if (res == GS_CAN) return GEOsimAppl::CMDLIST.CancelCmd();
   
   return GEOsimAppl::CMDLIST.EndCmd();
}


/*********************************************************/
/*.doc gsc_eraseSingleObj                     <external> */
/*+
   Comando di cancellazione di un oggetto grafico in ausilio alla <gserase>.
   Se la classe dell'entità appartiene a gruppo viene richiesto di 
   selezionare se si desidera cancellare il gruppo o la classe semplice. 
   Nel caso in cui si scelga il gruppo viene richiesto di scegliere quale 
   gruppo (l'entità potrebbe essere collegato a diversi gruppi
    es. una via con tanti civici)
   Parametri:
   ads_name ent;  Oggetto grafico da cancellare
   
   Restituisce GS_GOOD in caso di successo altrimenti GS_BAD. 
-*/  
/*********************************************************/
int gsc_eraseSingleObj(ads_name ent)
{
   int      res, num_el;
   C_EED    eed;
   long     key;
   ads_name entOut;
   C_CLASS  *pCls;

   // se non esiste sessione di lavoro o se non si è ancora estratto o
   // se si tratta di un'entità che non appartiene a GEOsim
   if (eed.load(ent) == GS_BAD)
      return gsc_EraseEnt(ent);
 
   // Verifico i gruppi  
   res = gsc_select_entity(ent, &key, &num_el, entOut, &pCls);
   if (res != GS_GOOD) return res;

   // Start Undo
   if (gsc_startTransaction() == GS_BAD) return GS_BAD;

   if (pCls->ptr_fas()) // se la classe ha la grafica cancello solo l'elemento grafico
   {
      if (pCls->erase_data(ent) == GS_BAD) { gsc_abortTransaction(); return GS_BAD; }
   }
   else // se la classe NON ha la grafica cancello l'entità
      if (pCls->erase_data(key) == GS_BAD) { gsc_abortTransaction(); return GS_BAD; }

   // End Undo
   if (gsc_endTransaction() == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_eraseMultiObjs                     <external> */
/*+
   Comando di cancellazione di un gruppo di selezione in ausilio alla <gserase>.
   Se almeno un oggetto appartiene a una classe di entità collegata a gruppo 
   viene richiesto di selezionare (selezione multipla) una o più classi fra quelle
   a cui appartengono qli oggetti del gruppo di selezione.
   Parametri:
   C_SELSET SelSet;  Oggetto grafico da cancellare

   Restituisce GS_GOOD in caso di successo altrimenti GS_BAD. 
-*/  
/*********************************************************/
int gsc_eraseMultiObjs(C_SELSET &SelSet)
{
   C_INT_INT_LIST ClsSubList, SelectedCls;
   C_INT_INT      *pClsSub, *p;
   C_FAMILY_LIST  FamilyList;
   C_FAMILY       *pFamily;
   bool           Group = false, UnknownObjs;
   long           i, Key;
   ads_name       ent;
   C_CLASS        *pCls;
   C_PROJECT      *pPrj = GS_CURRENT_WRK_SESSION->get_pPrj();
   C_SELSET       EraseSS;
   int            old_reactor_abilit;

   // Leggo i legami tra le classi del progetto
   if (gsc_getfamily(pPrj, &FamilyList) == GS_BAD) return GS_BAD;

   // Leggo le classi a cui appartengono gli oggetti di SelSet
   if (SelSet.get_ClsCodeList(ClsSubList, &UnknownObjs) == GS_BAD) return GS_BAD;

   // Verifico se tra le classi di appartenenza ve ne sono di collegate a gruppi
   pClsSub = (C_INT_INT *) ClsSubList.get_head();
   while (pClsSub)
   {
      // Controllo se la classe fa parte di uno/più gruppi estratti
      pFamily = (C_FAMILY *) FamilyList.get_head();
      while (pFamily)
      {
         if (pFamily->relation.search_key(pClsSub->get_key()) &&
             (pCls = pPrj->find_class(pFamily->get_key())) &&
             pCls->is_extracted())
         {
            Group = true;
            // Se non ancora inserita in lista
            if (!ClsSubList.search_key(pFamily->get_key()))
            {
               if ((p = new C_INT_INT(pFamily->get_key(), 0)) == NULL)
                  { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
               ClsSubList.add_tail(p);
            }
         }

         pFamily = (C_FAMILY *) FamilyList.get_next();
      }

      pClsSub = (C_INT_INT *) pClsSub->get_next();
   }

   // Se ci sono degli oggetti che NON appartengono a GEOsim
   if (UnknownObjs)
   {
      // Aggiungo una classe fittizia (0, 0) che significa oggetti autocad
      if ((p = new C_INT_INT(0, 0)) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      ClsSubList.add_tail(p);
   }

   // Se tra le classi c'erano dei gruppi
   if (Group)
   {
      int  res;
      bool FirstTime = true;
      long DeletedGrp = 0;
      C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(342)); // "Cancellazione entità"

      // Seleziono quali classi elaborare
      if ((res = gsc_ddMultiSelectClass(ClsSubList, SelectedCls)) != GS_GOOD)
         return res;

      StatusLineMsg.Init(gsc_msg(310), LITTLE_STEP); // ogni 10 "%ld entità GEOsim elaborate."

      // Se sono state selezionate i gruppi le elaboro prima
      p = (C_INT_INT *) SelectedCls.get_head();
      while (p)
      {
         if (p->get_key() == 0)
         {
            p = (C_INT_INT *) SelectedCls.get_next();
            continue;
         }

         pCls = pPrj->find_class(p->get_key(), p->get_type());
         if (pCls->get_category() == CAT_GROUP)
         {
            C_PREPARED_CMD pTempCmd, pOldCmd;
            C_LONG_LIST    GroupIDList;
            C_LONG         *pGroupID;
            C_CLASS        *pMemberCls;

            // Preparo i comandi di ricerca
            if (pCls->prepare_reldata_where_member(pTempCmd, TEMP) == GS_BAD ||
                pCls->prepare_reldata_where_member(pOldCmd, OLD) == GS_BAD)
               return GS_BAD;

            // Cancello tutte i gruppi collegate agli oggetti di SelSet
            i = 0;           
            while (SelSet.entname(i++, ent) == GS_GOOD)
            {
               if ((pMemberCls = pPrj->find_class(ent)))
               {
                  // Controllo se la classe fa parte di un gruppo
                  pFamily = (C_FAMILY *) FamilyList.get_head();
                  while (pFamily)
                  {
                     if (pFamily->relation.search_key(pMemberCls->ptr_id()->code) &&
                        (pCls == pPrj->find_class(pFamily->get_key())))
                     {
                        // leggo codice chiave entità membro
                        if (pMemberCls->getKeyValue(ent, &Key) == GS_BAD) return GS_BAD;
                        // leggo la lista dei gruppi collegati
                        if (pCls->get_group_list(pTempCmd, pOldCmd,
						                                pMemberCls->ptr_id()->code, Key,
                                                  GroupIDList) == GS_BAD)
                           return GS_BAD;
                        // Cancello tutti i gruppi
                        pGroupID = (C_LONG *) GroupIDList.get_head();
                        while (pGroupID)
                        {
                           if (pCls->erase_data(pGroupID->get_id()) == GS_GOOD)
                           {
                              if (FirstTime)
                              {
                                 acutPrintf(gsc_msg(463)); // "\nCancellazione entità gruppi..."
                                 acutPrintf(GS_LFSTR);
                                 FirstTime = false;
                              }
                              // Ogni 10 stampo il contatore
                              StatusLineMsg.Set(++DeletedGrp); // "%ld entità GEOsim elaborate."
                           }

                           pGroupID = (C_LONG *) GroupIDList.get_next();
                        }
                     }

                     pFamily = (C_FAMILY *) FamilyList.get_next();
                  }
               }
            }

            // Se era stata cancellata qualche gruppo
            if (!FirstTime)
               StatusLineMsg.Set(gsc_msg(310), DeletedGrp); // "%ld entità GEOsim elaborate."
         }

         p = (C_INT_INT *) SelectedCls.get_next();
      }
      StatusLineMsg.End(gsc_msg(310), DeletedGrp); // "%ld entità GEOsim elaborate."
   }
   else
      ClsSubList.copy(&SelectedCls); // seleziono tutte le classi

   i = 0;
   while (SelSet.entname(i++, ent) == GS_GOOD)
      if ((pCls = pPrj->find_class(ent)))
      {  // Se la classe è stata selezionata
         if (SelectedCls.search(pCls->ptr_id()->code, pCls->ptr_id()->sub_code)) EraseSS.add(ent);
      }
      else // Classe fittizia (0, 0) che significa oggetti autocad
         if (SelectedCls.search(0, 0)) EraseSS.add(ent);

   // Start Undo
   if (gsc_startTransaction() == GS_BAD) return GS_BAD;

   // abilito i reattori per intrappolare le modifiche
   old_reactor_abilit = gsc_enable_reactors(); 
   // lancio il comando
   acutPrintf(GS_LFSTR);
   if (EraseSS.Erase() != GS_GOOD)
   {
      if (old_reactor_abilit != GS_GOOD) gsc_disable_reactors();
      gsc_abortTransaction();
      return GS_BAD;
   }
   // ripristino lo stato dei reattori
   if (old_reactor_abilit != GS_GOOD) gsc_disable_reactors();
   // controllo se ci sono oggetti da riallineare
   gsc_align_data_base();

   // End Undo
   if (gsc_endTransaction() == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gswblock                               <external> */
/*+
   Comando semplificato di esportazione di un blocco contenente
   entità GEOsim depurate dai loro marcatori.
   -*/  
/*********************************************************/
void gswblock(void)
{
   C_SELSET                     SelSet;
   C_EED                        eed;
   C_STRING                     ODTableName, OutDwg;
   long                         i;
   AcDbObjectIdArray            ObjIds;
   AcDbObjectId                 ObjId;
   AcDbDatabase                 *pTempDb, *pTempCleanedDb;
   AcDbBlockTable               *pBlockTable;
   AcDbBlockTableRecord         *pBlockTableRecord;
   AcDbBlockTableRecordIterator *pBlockIterator;
   AcDbEntity                   *pEnt;
   C_STATUSBAR_PROGRESSMETER    StatusBarProgressMeter(gsc_msg(234)); // "Esportazione dati"

   GEOsimAppl::CMDLIST.StartCmd();

   // "GEOsim - Seleziona file"
   if (gsc_GetFileD(gsc_msg(645), NULL, _T("dwg"), UI_SAVEFILE_FLAGS, OutDwg) != RTNORM)
      return GEOsimAppl::CMDLIST.CancelCmd();
   // verifico se il file esiste già, in caso positivo lo cancello
   if (gsc_path_exist(OutDwg) == GS_GOOD) gsc_delfile(OutDwg);

   // seleziona le entità
   if (gsc_ssget(NULL, NULL, NULL, NULL, SelSet) != RTNORM) return GEOsimAppl::CMDLIST.CancelCmd();

   // Ottengo un vettore di AcDbObjectId
   if (SelSet.get_AcDbObjectIdArray(ObjIds) == GS_BAD) return GEOsimAppl::CMDLIST.CancelCmd();
   SelSet.clear();
   // li copio in un db temporaneo
   if (acdbHostApplicationServices()->workingDatabase()->wblock(pTempDb, ObjIds, AcGePoint3d::kOrigin) != Acad::eOk)
      return GEOsimAppl::CMDLIST.CancelCmd();
   // Depuro gli oggetti dai marcatori di geosim
   if (pTempDb->getBlockTable(pBlockTable, AcDb::kForWrite) != Acad::eOk)
      { delete pTempDb; return GEOsimAppl::CMDLIST.CancelCmd(); }
   if (pBlockTable->getAt(ACDB_MODEL_SPACE, pBlockTableRecord, AcDb::kForWrite) != Acad::eOk)
      { delete pTempDb; return GEOsimAppl::CMDLIST.CancelCmd(); }
   pBlockTable->close();
   pBlockTableRecord->newIterator(pBlockIterator);
   StatusBarProgressMeter.Init(ObjIds.length());
   i = 0;
   ObjIds.removeAll();
   for (pBlockIterator->start(); !pBlockIterator->done(); pBlockIterator->step())
   {
      StatusBarProgressMeter.Set(++i);

      pBlockIterator->getEntity(pEnt, AcDb::kForRead);
      ObjIds.append(pEnt->objectId());

      if (eed.load(pEnt) == GS_GOOD)
      {
         pEnt->close();

         if (GS_CURRENT_WRK_SESSION)
         {
            gsc_getODTableName(GS_CURRENT_WRK_SESSION->get_PrjId(), 
                               eed.cls, eed.sub, ODTableName);
            gsc_delID2ODTable(pEnt->objectId(), ODTableName);
         }
         eed.clear(pEnt->objectId());
      }
      else
         pEnt->close();
   }
   delete pBlockIterator;
   pBlockTableRecord->close();

   StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

   // li copio puliti in un db temporaneo
   if (pTempDb->wblock(pTempCleanedDb, ObjIds, AcGePoint3d::kOrigin) != Acad::eOk)
      return GEOsimAppl::CMDLIST.CancelCmd();
   delete pTempDb;

   Acad::ErrorStatus es;
   // salvo il db temporaneo in un dwg 
   GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = true; // usato nell'evento beginSave
	if ((es = pTempCleanedDb->saveAs(OutDwg.get_name())) != Acad::eOk)
   {
      GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = false;
      delete pTempCleanedDb; 
      return GEOsimAppl::CMDLIST.CancelCmd();
   }
   delete pTempCleanedDb;
   GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = false;

   return GEOsimAppl::CMDLIST.EndCmd();


 //  C_SELSET                     SelSet;
 //  C_EED                        eed;
 //  C_STRING                     ODTableName, OutDwg;
 //  long                         i;
 //  AcDbObjectIdArray            ObjIds;
 //  AcDbObjectId                 ObjId;
 //  AcDbDatabase                 *pTempDb;
 //  AcDbBlockTable               *pBlockTable;
 //  AcDbBlockTableRecord         *pBlockTableRecord;
 //  AcDbBlockTableRecordIterator *pBlockIterator;
 //  AcDbEntity                   *pEnt;
 //  C_STATUSBAR_PROGRESSMETER    StatusBarProgressMeter;

 //  GEOsimAppl::CMDLIST.StartCmd();

 //  // "GEOsim - Seleziona file"
 //  if (gsc_GetFileD(gsc_msg(645), NULL, _T("dwg"), UI_SAVEFILE_FLAGS, OutDwg) != RTNORM)
 //     return GEOsimAppl::CMDLIST.CancelCmd();
 //  // verifico se il file esiste già, in caso positivo lo cancello
 //  if (gsc_path_exist(OutDwg) == GS_GOOD) gsc_delfile(OutDwg);

 //  // seleziona le entità
 //  if (gsc_ssget(NULL, NULL, NULL, NULL, SelSet) != RTNORM) return GEOsimAppl::CMDLIST.CancelCmd();

 //  // Ottengo un vettore di AcDbObjectId
 //  if (SelSet.get_AcDbObjectIdArray(ObjIds) == GS_BAD) return GEOsimAppl::CMDLIST.CancelCmd();
 //  SelSet.clear();
 //  // li copio in un db temporaneo
 //  if (acdbHostApplicationServices()->workingDatabase()->wblock(pTempDb, ObjIds, AcGePoint3d::kOrigin) != Acad::eOk)
 //     return GEOsimAppl::CMDLIST.CancelCmd();
 //  // Depuro gli oggetti dai marcatori di geosim
 //  if (pTempDb->getBlockTable(pBlockTable, AcDb::kForWrite) != Acad::eOk)
 //     { delete pTempDb; return GEOsimAppl::CMDLIST.CancelCmd(); }
 //  if (pBlockTable->getAt(ACDB_MODEL_SPACE, pBlockTableRecord, AcDb::kForWrite) != Acad::eOk)
 //     { delete pTempDb; return GEOsimAppl::CMDLIST.CancelCmd(); }
 //  pBlockTable->close();
 //  pBlockTableRecord->newIterator(pBlockIterator);
 //  StatusBarProgressMeter.Init(ObjIds.length());
 //  i = 0;
 //  for (pBlockIterator->start(); !pBlockIterator->done(); pBlockIterator->step())
 //  {
 //     StatusBarProgressMeter.Set(++i);

 //     pBlockIterator->getEntity(pEnt, AcDb::kForRead);
 //     if (eed.load(pEnt) == GS_GOOD)
 //     {
 //        pEnt->close();

 //        if (GS_CURRENT_WRK_SESSION)
 //        {
 //           gsc_getODTableName(GS_CURRENT_WRK_SESSION->get_PrjId(), 
 //                              eed.cls, eed.sub, ODTableName);
 //           gsc_delID2ODTable(pEnt->objectId(), ODTableName);
 //        }
 //        eed.clear(pEnt->objectId());
 //     }
 //     else
 //        pEnt->close();
 //  }
 //  delete pBlockIterator;
 //  pBlockTableRecord->close();

 //  StatusBarProgressMeter.End();

 //  Acad::ErrorStatus es;
 //  // salvo il db temporaneo in un dwg 
 //  GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = true; // usato nell'evento beginSave
	//if ((es = pTempDb->saveAs(OutDwg.get_name())) != Acad::eOk)
 //  {
 //     GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = false;
 //     delete pTempDb; 
 //     return GEOsimAppl::CMDLIST.CancelCmd();
 //  }
 //  delete pTempDb;
 //  GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = false;

 //  return GEOsimAppl::CMDLIST.EndCmd();
}


/*********************************************************/
/*.doc gscopy2AcadObjs                        <external> */
/*+
   Comando di copia di una o più entità eliminando agli oggetti 
   copiati le entità estese di GEOsim.
   -*/  
/*********************************************************/


typedef struct 
{
   ads_name last_ent;
   int      ClipPromptUpdate;
} copy_callbackData;


static Adesk::Boolean isCopy2AcadObjActive()
{
   struct resbuf rb;
   C_STRING ActualCmd;

   acedGetVar(_T("CMDNAMES"), &rb);
   ActualCmd = rb.resval.rstring;
   if (ActualCmd.endWith(_T("COPY")))
     return Adesk::kTrue;

    return Adesk::kFalse;
}
static void gscopy2AcadObjs_part2(copy_callbackData *pData)
{
   C_LINK   Link;
   C_EED    eed;
   C_CLASS  *pCls;
   int      prj = GS_CURRENT_WRK_SESSION->get_PrjId();
   C_STRING ODTableName;
   ads_name ent;

   ads_name_set(pData->last_ent, ent);

   // nel caso siano stati inseriti dei nuovi nuovi oggetti grafici
   while (gsc_mainentnext(ent, ent) == GS_GOOD)
   {
      if (eed.load(ent) == GS_GOOD)
      {
         // Ritorna il puntatore alla classe cercata
         if ((pCls = GS_CURRENT_WRK_SESSION->find_class(eed.cls, eed.sub)))
            if (pCls->get_category() == CAT_SPAGHETTI)
            {  // cancello identificatore degli spaghetti
               gsc_getODTableName(prj, eed.cls, eed.sub, ODTableName);
               if (gsc_delID2ODTable(ent, ODTableName) == GS_BAD)
                  { gsc_abortTransaction(); return GEOsimAppl::CMDLIST.ErrorCmd(); }
            }
            else
               if (Link.erase(ent) == GS_BAD)
                  { gsc_abortTransaction(); return GEOsimAppl::CMDLIST.ErrorCmd(); }

         if (eed.clear(ent) == GS_BAD) { gsc_abortTransaction(); return GEOsimAppl::CMDLIST.ErrorCmd(); }
      }
   }

   // Se tutto OK allora ripristino UNDO
   if (gsc_endTransaction() == GS_BAD)  
      { GS_ERR_COD = eGSUndoErr; return GEOsimAppl::CMDLIST.ErrorCmd(); }

   gsc_set_CLIPROMPTUPDATE(pData->ClipPromptUpdate);

   return GEOsimAppl::CMDLIST.EndCmd();
}
static int gscopy2AcadObjs_Callback(void * pData)
{
    if (acedCmdCWasCancelled())
    {
        GS_SELSET.clear();
        gsc_abortTransaction();
        return 0;
    }

    if (isCopy2AcadObjActive())
    {
        int rs = acedCommandC(&gscopy2AcadObjs_Callback, pData, RTSTR, PAUSE, 0);

        acedCallBackOnCancel();

        return 1;
    }

    GS_SELSET.clear();
    gscopy2AcadObjs_part2((copy_callbackData *) pData);

    return 0;
}
static void gscopy2AcadObjs(void)
{
   static copy_callbackData callbackData;
   ads_name ss;

   GEOsimAppl::CMDLIST.StartCmd();

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GEOsimAppl::CMDLIST.ErrorCmd(); }

   // Memorizzo ultima entita.
   if (acdbEntLast(callbackData.last_ent) != RTNORM) ads_name_clear(callbackData.last_ent);

   // seleziona le entità
   if (gsc_ssget(NULL, NULL, NULL, NULL, GS_SELSET) != RTNORM) return GEOsimAppl::CMDLIST.CancelCmd();

   // Start Undo
   if (gsc_startTransaction() == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();
   gsc_set_CLIPROMPTUPDATE(1, &callbackData.ClipPromptUpdate);

   GS_SELSET.get_selection(ss);
   if (acedCommandC(&gscopy2AcadObjs_Callback, (void *) &callbackData, RTSTR, _T("_.COPY"), RTPICKS, ss, RTSTR, GS_EMPTYSTR, RTSTR, PAUSE, 0) != RTNORM)
   { 
      GS_SELSET.clear();
      gsc_abortTransaction();
      return GEOsimAppl::CMDLIST.ErrorCmd();
   }

   return;
}


/*********************************************************/
/*.doc gscopy                                <external>  */
/*+
   Comando di copia di una o più entità.
   -*/  
/*********************************************************/
static Adesk::Boolean isCopyActive()
{
   struct resbuf rb;
   C_STRING ActualCmd;

   acedGetVar(_T("CMDNAMES"), &rb);
   ActualCmd = rb.resval.rstring;
   if (ActualCmd.comp(_T("COPY")))
     return Adesk::kTrue;

    return Adesk::kFalse;
}
static void gscopy_part2(copy_callbackData *pData)
{
   gsc_set_CLIPROMPTUPDATE(pData->ClipPromptUpdate);

   // disabilito i reattori
   int old_reactor_abilit = gsc_disable_reactors(); 
   // controllo se ci sono oggetti da riallineare
   gsc_align_data_base();
   // ripristino lo stato dei reattori
   if (old_reactor_abilit == GS_GOOD) gsc_enable_reactors();

   // End Undo
   if (gsc_endTransaction() == GS_BAD) { gsc_print_error(); return; }

   return;
}
static int gscopy_Callback(void * pData)
{
   if (acedCmdCWasCancelled())
   {
      GS_SELSET.clear(); 
      gsc_abortTransaction();
      return 0;
   }

   if (isCopyActive())
   {
      int rs = acedCommandC(&gscopy_Callback, pData, RTSTR, PAUSE, 0);

      acedCallBackOnCancel();

      return 1;
   }

   GS_SELSET.clear();
   gscopy_part2((copy_callbackData *) pData);

   return 0;
}
void gscopy()
{
   copy_callbackData callbackData;
   ads_name ss, ent;
   long     PrevLenSelSet, LenSelSet, i = 0;
   int      res, AttrBlkPresent = FALSE, GraphObjPresent = FALSE;

   // se esiste una sessione di lavoro in cui si è già estratta almeno una classe
   if (GS_CURRENT_WRK_SESSION && GS_CURRENT_WRK_SESSION->get_status() != WRK_SESSION_WAIT)
   {
      // verifico l'abilitazione dell'utente
      if (gsc_check_op(opModEntity) == GS_BAD)
         { gsc_print_error(); return; }
      // verifico che l'area sia attiva e modificabile
      if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD)
         { gsc_print_error(); return; }
   }

   // seleziona le entità
   if (gsc_ssget(NULL, NULL, NULL, NULL, GS_SELSET) != RTNORM) { gsc_print_error(); return; }

   while (GS_SELSET.entname(i++, ent) == GS_GOOD)
   {
      if (!AttrBlkPresent || !GraphObjPresent)
      {
         if (gsc_is_DABlock(ent) == GS_GOOD) AttrBlkPresent = TRUE;
         else GraphObjPresent = TRUE;
      }
      else break;
   }

   if (GraphObjPresent && AttrBlkPresent)
   {
      C_STRING msg, label;
      TCHAR    answer[MAX_COMMAND_LEN];

      // Chiedo se la copia deve essere filtrata
      msg += gsc_msg(93);       // "\nDigitare un'opzione "
      msg += _T('[');
      msg += gsc_msg(609);      // "Tutto"
      msg += _T('/');
      msg += gsc_msg(610);      // "Attributi"
      msg += _T("/<");
      msg += gsc_msg(611);      // "oggetti Principali"
      msg += _T(">]: ");

      label = gsc_msg(609);     // "Tutto"
      label += _T(' ');
      label += gsc_msg(610);    // "Attributi"
      label += _T(' ');
      label += gsc_msg(611);    // "oggetti Principali"

      acedInitGet(RSG_OTHER, label.get_name());	// "Tutto Attributi"
      res = acedGetKword(msg.get_name(), answer);
      
      if (res == RTERROR) { GS_SELSET.clear(); gsc_print_error(); return; }    // Input Errato
      else if (res == RTCAN) { GS_SELSET.clear(); gsc_print_error(); return; } // Input ^C
      else if (res == RTNONE)
         { if (GS_SELSET.intersectType(GRAPHICAL) == GS_BAD) { GS_SELSET.clear(); gsc_print_error(); return; } } // "Tutto"
      else
      {
         gsc_alltrim(answer);
         if (gsc_strcmp(answer, gsc_msg(610)) == 0) // "Attributi"
            { if (GS_SELSET.intersectType(DA_BLOCK) == GS_BAD) { GS_SELSET.clear(); gsc_print_error(); return; } }
         else if (gsc_strstr(gsc_msg(611), answer) != NULL) // "oggetti Principali"
            { if (GS_SELSET.intersectType(GRAPHICAL) == GS_BAD) { GS_SELSET.clear(); gsc_print_error(); return; } }
      }
   }

   PrevLenSelSet = GS_SELSET.length();
   // Filtro le entità aggiornabili bloccandole
   if (GS_SELSET.intersectUpdateable(UPDATEABLE, GS_GOOD, GS_GOOD) == GS_BAD)
      { GS_SELSET.clear(); gsc_print_error(); return; }
   
   LenSelSet = GS_SELSET.length();
   if ((PrevLenSelSet - LenSelSet) > 0)
      // "\nEscluse dall'operazione %ld entità su %ld in quanto non modificabili."
      acutPrintf(gsc_msg(434), PrevLenSelSet - LenSelSet, PrevLenSelSet);

   if (LenSelSet == 0) return;

   // Start Undo
   if (gsc_startTransaction() == GS_BAD) { GS_SELSET.clear(); gsc_print_error(); return; }

   gsc_set_CLIPROMPTUPDATE(1, &callbackData.ClipPromptUpdate);

   GS_SELSET.get_selection(ss);
   // lancio il comando
   if (acedCommandC(&gscopy_Callback, (void *) &callbackData, RTSTR, _T("_.COPY"), RTPICKS, ss, RTSTR, GS_EMPTYSTR, RTSTR, PAUSE, 0) != RTNORM)
      { GS_SELSET.clear(); gsc_abortTransaction(); gsc_print_error(); return; }

   return;
}


/*********************************************************/
/*.doc gsdbcopy                              <external>  */
/*+
   Comando di copia dei valori di una scheda a una o più entità.
-*/  
/*********************************************************/
void gsdbcopy()
{
   C_CLASS        *pSrcCls, *pDestCls;
   C_RB_LIST      OrigColValues, DestColValues;
   C_SELSET       SelSet;
   ads_name       ent;
   ads_point      point;
   int            ret, Changed;
   long           PrevLenSelSet, LenSelSet, i = 0, Key, Elaborated = 0;
   C_ATTRIB       *pSrcAttr, *pDstAttr;
   C_DBCONNECTION *pSrcConn;
   presbuf        rbField;

   GEOsimAppl::CMDLIST.StartCmd();

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GEOsimAppl::CMDLIST.ErrorCmd(); }
   // verifico l'abilitazione dell' utente;
   if (gsc_check_op(opModEntity) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();
   // verifico che l'area sia attiva e modificabile;
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

   do
   {
      acutPrintf(gsc_msg(436)); // "\nSelezionare entità origine: "
      acedInitGet(RSG_NONULL, GS_EMPTYSTR);
      while ((ret = acedEntSel(GS_EMPTYSTR, ent, point)) == RTERROR);
      if (ret == RTREJ) { GS_ERR_COD = eGSAdsCommandErr; return GEOsimAppl::CMDLIST.ErrorCmd(); }
      if (ret == RTCAN) return GEOsimAppl::CMDLIST.CancelCmd();

      if ((pSrcCls = GS_CURRENT_WRK_SESSION->find_class(ent)) == NULL) break;
      if (pSrcCls->ptr_info() && pSrcCls->ptr_attrib_list() &&
          pSrcCls->query_data(ent, OrigColValues) == GS_GOOD) break;
      else
         acutPrintf(gsc_msg(28)); // "\nSelezione entità non valida."
   }
   while (1);

   acutPrintf(gsc_msg(315)); // "\nEntità destinazione: "
   if (gsc_ssget(NULL, NULL, NULL, NULL, SelSet) != RTNORM)
      return GEOsimAppl::CMDLIST.CancelCmd();
   PrevLenSelSet = SelSet.length();
   // Filtro le entità aggiornabili bloccandole
   if (SelSet.intersectUpdateable(UPDATEABLE, GS_GOOD, GS_GOOD) == GS_BAD)
      return GEOsimAppl::CMDLIST.ErrorCmd();
   
   LenSelSet = SelSet.length();
   if ((PrevLenSelSet - LenSelSet) > 0)
      // "\nEscluse dall'operazione %ld entità su %ld in quanto non modificabili."
      acutPrintf(gsc_msg(434), PrevLenSelSet - LenSelSet, PrevLenSelSet);
   
   if (LenSelSet == 0) return GEOsimAppl::CMDLIST.EndCmd();

   // Start Undo
   if (gsc_startTransaction() == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

   GEOsimAppl::REFUSED_SS.clear();
   pSrcConn = pSrcCls->ptr_info()->getDBConnection(OLD);

   acutPrintf(_T("\n%s\n"), gsc_msg(314)); // "Copia valori di database..."
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(314)); // "Copia valori di database..."
   StatusBarProgressMeter.Init(LenSelSet);

   while (SelSet.entname(i++, ent) == GS_GOOD)
   {
      StatusBarProgressMeter.Set(i);

      if ((pDestCls = GS_CURRENT_WRK_SESSION->find_class(ent)) &&
          pDestCls->getKeyValue(ent, &Key) == GS_GOOD &&
          pDestCls->query_data(Key, DestColValues) == GS_GOOD)
      {
         Changed = FALSE;
         // ricavo i campi con stesso nome
         pSrcAttr = (C_ATTRIB *) pSrcCls->ptr_attrib_list()->get_head();
         while (pSrcAttr)
         {
            // escludo il campo chiave
            if (pSrcAttr->name.comp(pDestCls->ptr_info()->key_attrib, FALSE) != 0 &&
                (pDstAttr = (C_ATTRIB *) pDestCls->ptr_attrib_list()->search_name(pSrcAttr->name.get_name(), FALSE)))
               // Verifico la compatibilità dei tipi   
               if (pSrcConn->IsCompatibleType(pSrcAttr->type, pSrcAttr->len, pSrcAttr->dec,
                                              *(pDestCls->ptr_info()->getDBConnection(OLD)), pDstAttr->type, pDstAttr->len, pDstAttr->dec) != GS_BAD)
                  // sostituisco il valore
                  if ((rbField = OrigColValues.CdrAssoc(pSrcAttr->get_name())))
                     if (DestColValues.CdrAssocSubst(pDstAttr->get_name(), rbField) == GS_GOOD)
                        Changed = TRUE;

            pSrcAttr = (C_ATTRIB *) pSrcAttr->get_next();
         }

         if (Changed)
            if (pDestCls->upd_data(Key, DestColValues) == GS_GOOD) Elaborated++;
            else GEOsimAppl::REFUSED_SS.add(ent);
      }
   }

   StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

   // End Undo
   gsc_endTransaction();

   if (i >= 0)
      acutPrintf(gsc_msg(308), Elaborated, GEOsimAppl::REFUSED_SS.length()); // "\n%ld entità GEOsim elaborate, %ld scartate."

   return GEOsimAppl::CMDLIST.EndCmd();
}


/*********************************************************/
/*.doc gssetdbdefault                        <external>  */
/*+
   Comando per settare i valori di default temporanei (solo in memoria)
   di una classe di entità.
-*/  
/*********************************************************/
void gssetdbdefault(void)
{
   C_CLASS       *pCls;
   C_RB_LIST     ColValues, DefValues;
   ads_name      ent;
   ads_point     point;
   C_ATTRIB_LIST *pAttribList;
   C_ATTRIB      *pAttr;
   int           ret;
   presbuf       pValue;

   GEOsimAppl::CMDLIST.StartCmd();

   if (!GS_CURRENT_WRK_SESSION)
      { GS_ERR_COD = eGSNotCurrentSession; return GEOsimAppl::CMDLIST.ErrorCmd(); }
   // verifico l'abilitazione dell' utente;
   if (gsc_check_op(opInsEntity) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();
   // verifico che l'area sia attiva e modificabile;
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

   do
   {
      acutPrintf(gsc_msg(210)); // "\nSeleziona entità: "
      acedInitGet(RSG_NONULL, GS_EMPTYSTR);
      while ((ret = acedEntSel(GS_EMPTYSTR, ent, point)) == RTERROR);
      if (ret == RTREJ) { GS_ERR_COD = eGSAdsCommandErr; return GEOsimAppl::CMDLIST.ErrorCmd(); }
      if (ret == RTCAN) return GEOsimAppl::CMDLIST.CancelCmd();

      if ((pCls = GS_CURRENT_WRK_SESSION->find_class(ent)) == NULL) break;
      if (pCls->ptr_info() && (pAttribList = pCls->ptr_attrib_list()) &&
          pCls->query_data(ent, ColValues) == GS_GOOD) break;
      else
         acutPrintf(gsc_msg(28)); // "\nSelezione entità non valida."
   }
   while (1);
  
   // ciclo sugli attributi scartando quelli calcolati e quello chiave
   DefValues << acutBuildList(RTLB, 0);
   pAttr = (C_ATTRIB *) pAttribList->get_head();
   while (pAttr)
   {
      DefValues += acutBuildList(RTLB, RTSTR, pAttr->get_name(), 0);

      if (pAttr->is_calculated() == GS_BAD &&
          pCls->ptr_info()->key_attrib.comp(pAttr->get_name()) != 0 &&
          (pValue = ColValues.CdrAssoc(pAttr->get_name())))
         DefValues += gsc_copybuf(pValue);
      else
         DefValues += gsc_copybuf(pAttr->def);

      DefValues += acutBuildList(RTLE, 0);

      pAttr = (C_ATTRIB *) pAttr->get_next();
   }
   DefValues += acutBuildList(RTLE, 0);

   if (DefValues.GetCount() == 2) return GEOsimAppl::CMDLIST.EndCmd();

   if (pAttribList->TestAllCalc(DefValues) == GS_BAD ||
       pAttribList->TestAllValid(DefValues, &pAttr) == GS_BAD)
      return GEOsimAppl::CMDLIST.ErrorCmd();

   // setto i valori di default corretti
   if (pAttribList->set_default_values(DefValues) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

   return GEOsimAppl::CMDLIST.EndCmd();
}


/*********************************************************/
/*.doc gsaddmemberstogrp                     <external>  */
/*+
   Comando per aggiungere dei membri ad un gruppo.
-*/  
/*********************************************************/
void gsaddmemberstogrp()
{
   C_SELSET SelSet;
   ads_name ent;
   long     gs_id, ent_id, i = 0;
   int      res, num_el;
   C_CLASS  *pCls;
   C_GROUP  *pGrpCls;
   C_EED     eed;
   C_INT_INT *p_group;
   C_CLS_PUNT_LIST  grpExtractedClsList;
   C_2INT_LONG_LIST memberList_to_add;
   C_2INT_LONG      *pMember;

   // per sicurezza verifico se esiste una sessione corrente non in attesa
   if (!GS_CURRENT_WRK_SESSION || GS_CURRENT_WRK_SESSION->get_status() == WRK_SESSION_WAIT)
      { GS_ERR_COD = eGSNotCurrentSession; gsc_print_error(); return; }

   // creo una lista delle classi gruppo estratte
   if (GS_CURRENT_WRK_SESSION->get_pPrj()->extracted_class(grpExtractedClsList, CAT_GROUP) == GS_BAD)
      { gsc_print_error(); return; }
   if (grpExtractedClsList.get_count() == 0)
      { GS_ERR_COD = eGSNotClassExtr; gsc_print_error(); return; }

   // verifico l'abilitazione dell'utente
   if (gsc_check_op(opModEntity) == GS_BAD) { gsc_print_error(); return; }
   // verifico che l'area sia attiva e modificabile;
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD) { gsc_print_error(); return; }

   // seleziona le entità da aggiungere al gruppo
   acutPrintf(gsc_msg(702)); // "\nEntità da aggiungere al gruppo: "

   if (gsc_ssget(NULL, NULL, NULL, NULL, SelSet) != RTNORM) return;

   // Verifico i gruppi
   while (1)
   {
      acutPrintf(gsc_msg(1049)); // "\nSelezionare il gruppo da modificare: "
      res = gsc_select_entity(NULL, &gs_id, &num_el, ent, &pCls);
      if (res != GS_GOOD) return;
      if (pCls->get_category() == CAT_GROUP) break;
   }
   
   pGrpCls = (C_GROUP *) pCls;
   // Verifico il gruppo di entità da aggiungere al gruppo
   while (SelSet.entname(i++, ent) == GS_GOOD)
      // Oggetto di GEOsim la cui classe fa parte del gruppo ed ha un numero indefinito di istanze
      if (eed.load(ent) == GS_GOOD &&
          (p_group = (C_INT_INT *) pGrpCls->ptr_group_list()->search_key(eed.cls)) &&
          p_group->get_type() <= 0 &&
          gsc_getKeyValue(ent, &ent_id) == GS_GOOD)
      {
         // evito i duplicati
         if (memberList_to_add.search(eed.cls, 0, ent_id) == NULL)
         {
            if ((pMember = new C_2INT_LONG(eed.cls, 0, ent_id)) == NULL)
               { GS_ERR_COD = eGSOutOfMem; return; }
            memberList_to_add.add_tail(pMember);
         }
      }

   if (memberList_to_add.get_count() == 0) return;

   if (pGrpCls->add_members(gs_id, memberList_to_add) != GS_GOOD) gsc_print_error();
  
   return;
}


/*********************************************************/
/*.doc gsremovememberstogrp                  <external>  */
/*+
   Comando per rimuovere dei membri ad un gruppo.
-*/  
/*********************************************************/
void gsremovememberstogrp()
{
   C_SELSET SelSet;
   ads_name ent;
   long     gs_id, ent_id, i = 0;
   int      res, num_el;
   C_CLASS  *pCls;
   C_GROUP  *pGrpCls;
   C_EED     eed;
   C_INT_INT *p_group;
   C_CLS_PUNT_LIST  grpExtractedClsList;
   C_2INT_LONG_LIST memberList_to_add;
   C_2INT_LONG      *pMember;

   // per sicurezza verifico se esiste una sessione corrente non in attesa
   if (!GS_CURRENT_WRK_SESSION || GS_CURRENT_WRK_SESSION->get_status() == WRK_SESSION_WAIT)
      { GS_ERR_COD = eGSNotCurrentSession; gsc_print_error(); return; }

   // creo una lista delle classi gruppo estratte
   if (GS_CURRENT_WRK_SESSION->get_pPrj()->extracted_class(grpExtractedClsList, CAT_GROUP) == GS_BAD)
      { gsc_print_error(); return; }
   if (grpExtractedClsList.get_count() == 0)
      { GS_ERR_COD = eGSNotClassExtr; gsc_print_error(); return; }

   // verifico l'abilitazione dell'utente
   if (gsc_check_op(opModEntity) == GS_BAD) { gsc_print_error(); return; }
   // verifico che l'area sia attiva e modificabile;
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD) { gsc_print_error(); return; }

   // seleziona le entità da aggiungere al gruppo
   acutPrintf(gsc_msg(1050)); // "\nEntità da rimuovere dal gruppo: "

   if (gsc_ssget(NULL, NULL, NULL, NULL, SelSet) != RTNORM) return;
   // leggo la prima entità
   SelSet.entname(0, ent);

   // Verifico i gruppi
   while (1)
   {
      acutPrintf(gsc_msg(1049)); // "\nSelezionare il gruppo da modificare: "
      res = gsc_select_entity(ent, &gs_id, &num_el, ent, &pCls);
      if (res != GS_GOOD) return;
      if (pCls->get_category() == CAT_GROUP) break;
   }
   
   pGrpCls = (C_GROUP *) pCls;
   // Verifico il gruppo di entità da rimuovere dal gruppo
   while (SelSet.entname(i++, ent) == GS_GOOD)
      // Oggetto di GEOsim la cui classe fa parte del gruppo ed ha un numero indefinito di istanze
      if (eed.load(ent) == GS_GOOD &&
          (p_group = (C_INT_INT *) pGrpCls->ptr_group_list()->search_key(eed.cls)) &&
          p_group->get_type() <= 0 &&
          gsc_getKeyValue(ent, &ent_id) == GS_GOOD)
      {
         // evito i duplicati
         if (memberList_to_add.search(eed.cls, 0, ent_id) == NULL)
         {
            if ((pMember = new C_2INT_LONG(eed.cls, 0, ent_id)) == NULL)
               { GS_ERR_COD = eGSOutOfMem; return; }
            memberList_to_add.add_tail(pMember);
         }
      }

   if (memberList_to_add.get_count() == 0) return;

   if (pGrpCls->remove_members(gs_id, memberList_to_add) != GS_GOOD) gsc_print_error();
  
   return;
}


/*********************************************************/
/*.doc gsbreak                                <external> */
/*+
   Comando spezza di una entita.
-*/  
/*********************************************************/


typedef struct 
{
   ads_name ent;
   int      ClipPromptUpdate;
   int      has_visib_attrib;
   C_SELSET SelSet;
   C_CLASS  *pCls;
} break_callbackData;


static Adesk::Boolean isBreakActive()
{
   struct resbuf rb;
   C_STRING ActualCmd;

   acedGetVar(_T("CMDNAMES"), &rb);
   ActualCmd = rb.resval.rstring;
   if (ActualCmd.comp(_T("BREAK")))
     return Adesk::kTrue;

    return Adesk::kFalse;
}
static void gsbreak_part2(break_callbackData *pData)
{
   int PrevInsPos;

   gsc_set_CLIPROMPTUPDATE(pData->ClipPromptUpdate);

   PrevInsPos = GEOsimAppl::GLOBALVARS.get_InsPos();

   if (pData->has_visib_attrib == GS_GOOD)
   {
      long i = 0;
      ads_name DABlk;

      GEOsimAppl::GLOBALVARS.set_InsPos(AUTO);

      // disabilito i reattori
      int old_reactor_abilit = gsc_disable_reactors(); 
      // controllo se ci sono oggetti da riallineare
      gsc_align_data_base();
      // ripristino lo stato dei reattori
      if (old_reactor_abilit == GS_GOOD) gsc_enable_reactors();

      // Cancello le schede visibili dell'oggetto da spezzare
      while (pData->SelSet.entname(i++, DABlk) == GS_GOOD)
         // se esiste ancora la scheda visibile dell'oggetto da spezzare
         if (gsc_IsErasedEnt(DABlk) == GS_BAD)
            if (pData->pCls->erase_data(DABlk) == GS_BAD)
               { gsc_abortTransaction(); return GEOsimAppl::CMDLIST.ErrorCmd(); }

      // il break su polilinea non complanare cancella l'entità originale 
      // e ne crea 2 nuove
      // se esiste ancora l'oggetto da spezzare
      if (gsc_IsErasedEnt(pData->ent) == GS_BAD)
         pData->pCls->SetVisibDA(pData->ent, VISIBLE); // Accendo la scheda dell'entità spezzata
   }
   else
   {
      GEOsimAppl::GLOBALVARS.set_InsPos(MANUAL);

      // disabilito i reattori
      int old_reactor_abilit = gsc_disable_reactors(); 
      // controllo se ci sono oggetti da riallineare
      gsc_align_data_base();
      // ripristino lo stato dei reattori
      if (old_reactor_abilit == GS_GOOD) gsc_enable_reactors();
   }

   GEOsimAppl::GLOBALVARS.set_InsPos(PrevInsPos);

   // End Undo
   if (gsc_endTransaction() == GS_BAD) { gsc_print_error(); return; }
   gsc_set_CLIPROMPTUPDATE(pData->ClipPromptUpdate);

   return;
}
static int gsbreak_Callback(void * pData)
{
    if (acedCmdCWasCancelled())
    {
        gsc_abortTransaction();
        return 0;
    }

    if (isBreakActive())
    {
        int rs = acedCommandC(&gsbreak_Callback, pData, RTSTR, PAUSE, 0);

        acedCallBackOnCancel();

        return 1;
    }

    gsbreak_part2((break_callbackData *) pData);

    return 0;
}
void gsbreak()
{
   static break_callbackData callbackData;
   ads_point pt;
   int       res;
   C_EED     eed;

   // se esiste una sessione di lavoro in cui si è già estratta almeno una classe
   if (GS_CURRENT_WRK_SESSION && GS_CURRENT_WRK_SESSION->get_status() != WRK_SESSION_WAIT)
   {
      // verifico l'abilitazione dell'utente
      if (gsc_check_op(opModEntity) == GS_BAD)
         { gsc_print_error(); return; }
      // verifico che l'area sia attiva e modificabile
      if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD)
         { gsc_print_error(); return; }
   }

   // seleziona l'entità
   acutPrintf(gsc_msg(96)); // "\nSeleziona oggetto: "
   acedInitGet(RSG_NONULL, GS_EMPTYSTR);
   while ((res = acedEntSel(GS_EMPTYSTR, callbackData.ent, pt)) == RTERROR);
   if (res == RTREJ) { GS_ERR_COD = eGSAdsCommandErr; gsc_print_error(); return; }
   if (res == RTCAN) return;

   callbackData.has_visib_attrib = FALSE;

   // Oggetto di GEOsim
   if (eed.load(callbackData.ent) == GS_GOOD)
   {
      // Ricavo il puntatore alla classe dell' oggetto in questione
      if ((callbackData.pCls = GS_CURRENT_WRK_SESSION->find_class(eed.cls, eed.sub)) == NULL)
         { gsc_print_error(); return; }

      if (callbackData.pCls->ptr_info())
      {
         long Key;

         if (callbackData.pCls->get_Key_SelSet(callbackData.ent, &Key, callbackData.SelSet) == GS_BAD)
            { gsc_print_error(); return; }
         // Entità GEOsim aggiornabile con tentativo di estrazione
         if (callbackData.pCls->is_updateableSS(Key, callbackData.SelSet, NULL, GS_GOOD, GS_GOOD) == GS_BAD)
         {
            // "\nEscluse dall'operazione %ld entità su %ld in quanto non modificabili."
            acutPrintf(gsc_msg(434), 1, 1);
            return;
         }
         // l'oggetto ha attributi visibili
         if (callbackData.pCls->ptr_attrib_list()->is_visible() == GS_GOOD &&
             callbackData.SelSet.intersectType(DA_BLOCK) == GS_GOOD && callbackData.SelSet.length() > 0)
            callbackData.has_visib_attrib = TRUE;
      }
      else
         // Entità GEOsim aggiornabile
         if (callbackData.pCls->is_updateable(callbackData.ent, NULL, GS_GOOD) == GS_BAD)
         {
            // "\nEscluse dall'operazione %ld entità su %ld in quanto non modificabili."
            acutPrintf(gsc_msg(434), 1, 1);
            return;
         }
   }

   // Start Undo
   if (gsc_startTransaction() == GS_BAD) { gsc_print_error(); return; }

   gsc_set_CLIPROMPTUPDATE(1, &callbackData.ClipPromptUpdate);

   // lancio il comando
   if (acedCommandC(&gsbreak_Callback, (void *) &callbackData, RTSTR, _T("_.BREAK"), RTENAME, callbackData.ent, RTSTR, PAUSE, 0) != RTNORM)
      { gsc_abortTransaction(); gsc_print_error(); return; }

   return;
}
int gsc_break(ads_name ent, ads_point pt)
{
   C_SELSET  SelSet;
   int       old_reactor_abilit, has_visib_attrib = FALSE, PrevInsPos;
   C_CLASS   *pCls;
   C_EED     eed;

   // per sicurezza verifico se esiste una sessione corrente non in attesa
   if (!GS_CURRENT_WRK_SESSION || GS_CURRENT_WRK_SESSION->get_status() == WRK_SESSION_WAIT) 
      return GS_BAD;

   // verifico l'abilitazione dell' utente;
   if (gsc_check_op(opModEntity) == GS_BAD) return GS_BAD;
   // verifico che l'area sia attiva e modificabile;
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD) return GS_BAD;

   // Start Undo
   if (gsc_startTransaction() == GS_BAD) return RTERROR;

   // Oggetto di GEOsim
   if (eed.load(ent) == GS_GOOD)
   {
      // Ricavo il puntatore alla classe dell' oggetto in questione
      if ((pCls = GS_CURRENT_WRK_SESSION->find_class(eed.cls, eed.sub)) == NULL) return GS_BAD;

      if (pCls->ptr_info())
      {
         long Key;

         if (pCls->get_Key_SelSet(ent, &Key, SelSet) == GS_BAD) return GS_BAD;
         // Entità GEOsim aggiornabile con tentativo di estrazione
         if (pCls->is_updateableSS(Key, SelSet, NULL, GS_GOOD, GS_GOOD) == GS_BAD)
            return GS_CAN;
         // l'oggetto ha attributi visibili
         if (pCls->ptr_attrib_list()->is_visible() == GS_GOOD &&
             SelSet.intersectType(DA_BLOCK) == GS_GOOD && SelSet.length() > 0)
            has_visib_attrib = TRUE;
      }
      else
         // Entità GEOsim aggiornabile
         if (pCls->is_updateable(ent, NULL, GS_GOOD) == GS_BAD)
            return GS_CAN;
   }

   // abilito i reattori per intrappolare le modifiche
   old_reactor_abilit = gsc_enable_reactors(); 

   if (gsc_callCmd(_T("_.BREAK"), RTENAME, ent, RTPOINT, pt, RTPOINT, pt, 0) != RTNORM)
      return GS_BAD;

   // ripristino lo stato dei reattori
   if (old_reactor_abilit != GS_GOOD) gsc_disable_reactors();

   if (has_visib_attrib == GS_GOOD)
   {
      long i = 0;
      ads_name DABlk;

      // Cancello le schede visibili dell'oggetto da spezzare
      while (SelSet.entname(i++, DABlk) == GS_GOOD)
         if (pCls->erase_data(DABlk) == GS_BAD) { gsc_abortTransaction(); return GS_BAD; }

      PrevInsPos = GEOsimAppl::GLOBALVARS.get_InsPos();
      GEOsimAppl::GLOBALVARS.set_InsPos(AUTO);
      // il break su polilinea non complanare cancella l'entità originale 
      // e ne crea 2 nuove
      // se esiste ancora l'oggetto da spezzare
      if (gsc_IsErasedEnt(ent) == GS_BAD)
         pCls->SetVisibDA(ent, VISIBLE); // Accendo la scheda dell'entità spezzata
   }
   else
   {
      PrevInsPos = GEOsimAppl::GLOBALVARS.get_InsPos();
      GEOsimAppl::GLOBALVARS.set_InsPos(MANUAL);
   }

   // controllo se ci sono oggetti da riallineare
   gsc_align_data_base();

   GEOsimAppl::GLOBALVARS.set_InsPos(PrevInsPos);

   // End Undo
   if (gsc_endTransaction() == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_break                              <external> */
/*+
   Comando spezza di una entita.
   Parametri:
   Versione per oggetti di tipo esterno (classi C_SUB) che aggiorna la topologia
   ads_name entLink;        entità da spezzare
   ads_name entNode;        entità nodale di break

   Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_break(ads_name entLink, ads_name entNode)
{
   C_EED      eed;
   ads_name   EntLast;
   ads_point  pt;
   C_SELSET   NewSS, LinkSS;
   C_RB_LIST  ColValues, Descr, AdjNodes;
   C_CLS_PUNT_LIST lista_cls_punt;
   C_CLS_PUNT *p_Link_cls_punt, *p_Init_cls_punt, *p_Final_cls_punt;
   C_TOPOLOGY topo;
   presbuf    pAdjNode;
   C_CLASS    *pLinkSub, *pSub;
   int        result = GS_BAD, InitSub, FinalSub, MiddleSub, cls;
   long       Key, InitNode, FinalNode, MiddleNode;
   C_STRING   EntType;
   C_LINK     Link;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   // verifico il tipo di entità nodale
   if ((pSub = GS_CURRENT_WRK_SESSION->find_class(entNode)) == NULL)
      return GS_BAD;
   if (pSub->is_subclass() == GS_BAD)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
   if (pSub->getKeyValue(entNode, &MiddleNode) == GS_BAD) return GS_BAD;
   MiddleSub = pSub->ptr_id()->sub_code;
   if (gsc_get_firstPoint(entNode, pt) == GS_BAD) return GS_BAD;

   // verifico il tipo di entità link
   if ((pLinkSub = GS_CURRENT_WRK_SESSION->find_class(entLink)) == NULL)
      return GS_BAD;
   if (pSub->ptr_id()->code != pLinkSub->ptr_id()->code)
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
   if (pLinkSub->is_subclass() == GS_BAD) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
   // legge i dati del link
   if (pLinkSub->get_Key_SelSet(entLink, &Key, LinkSS) == GS_BAD) return GS_BAD;
   if (pLinkSub->query_data(Key, ColValues) == GS_BAD) return GS_BAD;
   // leggo il tipo di entità grafica
   if ((Descr << acdbEntGet(entLink)) == NULL) { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
   EntType = Descr.SearchType(0)->resval.rstring;
   cls = pLinkSub->ptr_id()->code;
  
   // necessito del nodo iniziale e finale
   topo.set_type(TYPE_POLYLINE);                      // tipologia di tipo rete
   topo.set_cls(GS_CURRENT_WRK_SESSION->find_class(cls)); // per classe madre
   if ((pAdjNode = topo.elemadj(Key, TYPE_NODE)) == NULL)
      { GS_ERR_COD = eGSNoNodeToConnect; return GS_BAD; }
   AdjNodes << pAdjNode; 

   // Nodo iniziale
   if ((pAdjNode = AdjNodes.nth(0)) == NULL ||
       gsc_rb2Int(gsc_nth(0, pAdjNode), &InitSub) == GS_BAD ||
       gsc_rb2Lng(gsc_nth(1, pAdjNode), &InitNode) == GS_BAD)
      return GS_BAD;

   // Nodo finale
   if ((pAdjNode = AdjNodes.nth(1)) == NULL ||
       gsc_rb2Int(gsc_nth(0, pAdjNode), &FinalSub) == GS_BAD ||
       gsc_rb2Lng(gsc_nth(1, pAdjNode), &FinalNode) == GS_BAD)
      return GS_BAD;

   if ((p_Link_cls_punt = new C_CLS_PUNT(pLinkSub)) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   lista_cls_punt.add_tail(p_Link_cls_punt);
   if ((p_Init_cls_punt = new C_CLS_PUNT()) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   lista_cls_punt.add_tail(p_Init_cls_punt);
   if ((p_Final_cls_punt = new C_CLS_PUNT()) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   lista_cls_punt.add_tail(p_Final_cls_punt);

   // Memorizzo ultima entita.
   if (acdbEntLast(EntLast) != RTNORM) { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_callCmd(_T("_.BREAK"), RTENAME, entLink, RTPOINT, pt, RTPOINT, pt, 0) != RTNORM)
      return GS_BAD;

   result = GS_GOOD;
   // creo gruppo di selezione delle entità nuove generate da "break"
   // uso questa tecnica perchè C_SUB::insert_data crea una copia
   // dell'oggetto grafico che produrrebbe un ciclo infinito
   while (gsc_mainentnext(EntLast, EntLast) == GS_GOOD)
      if (NewSS.add(EntLast) == GS_BAD) { result = GS_BAD; break; }
   if (result == GS_BAD) return GS_BAD;

   if (EntType.comp(_T("POLYLINE")) == 0)
   {  // viene cancellata l'entità originale e create 2 nuove entità
      // inserimento primo lato
      if (NewSS.entname(0, p_Link_cls_punt->ent) == GS_BAD) return GS_BAD;
      p_Init_cls_punt->cls = GS_CURRENT_WRK_SESSION->find_class(cls, InitSub);
      p_Init_cls_punt->gs_id = InitNode;
      p_Final_cls_punt->cls = GS_CURRENT_WRK_SESSION->find_class(cls, MiddleSub);
      p_Final_cls_punt->gs_id = MiddleNode;

      if (Link.erase(p_Link_cls_punt->ent) == GS_BAD) return GS_BAD;
      eed.clear(p_Link_cls_punt->ent);
      // inserisco nuova polilinea
      if (pLinkSub->ins_data(&lista_cls_punt, ColValues, NULL, INVISIBLE) == GS_BAD)
         return GS_BAD;

      // inserimento secondo lato
      if (NewSS.entname(1, p_Link_cls_punt->ent) == GS_BAD) return GS_BAD;
      p_Init_cls_punt->cls = GS_CURRENT_WRK_SESSION->find_class(cls, MiddleSub);
      p_Init_cls_punt->gs_id = MiddleNode;
      p_Final_cls_punt->cls = GS_CURRENT_WRK_SESSION->find_class(cls, FinalSub);
      p_Final_cls_punt->gs_id = FinalNode;

      if (Link.erase(p_Link_cls_punt->ent) == GS_BAD) return GS_BAD;
      eed.clear(p_Link_cls_punt->ent);
      // inserisco nuova polilinea
      if (pLinkSub->ins_data(&lista_cls_punt, ColValues, NULL, INVISIBLE) == GS_BAD)
         return GS_BAD;

      // ripristino per cancellare la vecchia polilinea 
      if (gsc_UnEraseEnt(entLink) != GS_GOOD) return GS_BAD;
      if (pLinkSub->erase_data(entLink) == GS_BAD) return GS_BAD;
   }
   else
   if (EntType.comp(_T("LWPOLYLINE")) == 0 || EntType.comp(_T("ARC")) == 0)
   {  // viene modificata l'entità originale e creata 1 nuova entità
      if (NewSS.entname(0, p_Link_cls_punt->ent) == GS_BAD) return GS_BAD;
      p_Init_cls_punt->cls = GS_CURRENT_WRK_SESSION->find_class(cls, MiddleSub);
      p_Init_cls_punt->gs_id = MiddleNode;
      p_Final_cls_punt->cls = GS_CURRENT_WRK_SESSION->find_class(cls, FinalSub);
      p_Final_cls_punt->gs_id = FinalNode;

      if (Link.erase(p_Link_cls_punt->ent) == GS_BAD) return GS_BAD;
      eed.clear(p_Link_cls_punt->ent);
      // inserisco nuova polilinea
      if (pLinkSub->ins_data(&lista_cls_punt, ColValues, NULL, INVISIBLE) == GS_BAD)
         return GS_BAD;

      // Aggiorno la polilinea rimasta
      C_TOPOLOGY topo;
      topo.set_type(TYPE_POLYLINE);                      // tipologia di tipo rete
      topo.set_cls(GS_CURRENT_WRK_SESSION->find_class(pLinkSub->ptr_id()->code)); // per classe madre                 
      if (topo.editlink(pLinkSub->ptr_id()->sub_code, Key, entLink,
                        InitSub, InitNode,
                        MiddleSub, MiddleNode) == GS_BAD) return GS_BAD;
      if (pLinkSub->upd_data(entLink, ColValues, &LinkSS) == GS_BAD) return GS_BAD;
      // se la sottoclasse ha almeno un attributo visibile
      if (pLinkSub->ptr_attrib_list()->is_visible() == GS_GOOD)
      {
         // Cancello la/e scheda/e rimasta/e perchè non è mai chiaro
         // a quale elemento sia rimasta legata.
         // leggo valore chiave e il gruppo di selezione
         LinkSS.intersectType(DA_BLOCK);
         if (LinkSS.length() > 0)
         {
            long i = 0;
            int  PrevInsPos   = GEOsimAppl::GLOBALVARS.get_InsPos();
            int  PrevInsRotaz = GEOsimAppl::GLOBALVARS.get_InsRotaz();

            while (LinkSS.entname(i++, EntLast) == GS_GOOD)
               if (pLinkSub->erase_data(EntLast) == GS_BAD) return GS_BAD;

            GEOsimAppl::GLOBALVARS.set_InsPos(AUTO);
            GEOsimAppl::GLOBALVARS.set_InsRotaz(AUTO);

            // Accendo le schede
            pLinkSub->SetVisibDA(entLink, VISIBLE); 
            pLinkSub->SetVisibDA(((C_CLS_PUNT *) lista_cls_punt.get_head())->ent, VISIBLE);

            GEOsimAppl::GLOBALVARS.set_InsPos(PrevInsPos);
            GEOsimAppl::GLOBALVARS.set_InsRotaz(PrevInsRotaz);      
         }
      }
   }

   // Eseguo un redraw per refresh video
   if (gsc_callCmd(_T("_.REDRAW"), 0) != RTNORM) return GS_BAD;

   return GS_GOOD;
}


// Utilita per PEDIT
/******************************************************************/
/*.doc (mod 2) getPolylineVertexValues <external> */
/*+
   Funzione di memorizzazione delle caratteristiche del vertice
   di una polyline.

   Parametri:
   ads_name    Polyline    polyline da analizzare
   long        XVertex     numero del vertice da analizzare 
   presbuf    *Vvalues     resbuf contenente le informazioni
   Il resbuf è schematizzato come segue:
   (  coordinate del vertice in considerazione,
      larghezza iniziale del segmento,
      larghezza finale del segmento,
      tangente al vertice
   )
   N.B. Attenzione alle modifiche nel resbuf di uscita,
   il codice è cablato sulla posizione delle informazioni nel resbuf,
   chiunque aggiunga delle ulteriori informazioni le memorizzi in coda a
   quelle già presenti.... e poi sono cazzi suoi.
                  
   Restituisce GS_GOOD se tutto OK altrimenti restituisce GS_BAD.
-*/  
/******************************************************************/
int getPolylineVertexValues(ads_name Polyline, long XVertex, presbuf *Vvalues)
{
   presbuf     rb1, rb2, rbTwo;
   ads_name    nxt;
   ads_real    startWidth, endWidth, Tangent;
   ads_point   pointVertex;
   long        contVertex = 0;
   C_RB_LIST   rbOne;

   if (XVertex < 1)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   // Attenzione !! il primo elemento è il numero 1.
   ads_name_set(Polyline, nxt);
   
   // verifico se si tratta di POLYLINE o LWPOLYLINE perchè la 14
   // le tratta in modo diverso
   if ((rbOne << acdbEntGet(Polyline)) == NULL)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
   if ((rbTwo = gsc_rbsearch(0, rbOne.get_head())) == NULL)
      { GS_ERR_COD = eGSInvRBType; return GS_BAD; }

   if (gsc_strcmp(rbTwo->resval.rstring, _T("POLYLINE")) == 0)
   {
      while (1) 
      {
         contVertex++;
         if (ads_entnext(nxt, nxt) == RTERROR)
            { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
         if ((rb1 = acdbEntGet(nxt)) == NULL)
            { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
         if ((rb2 = gsc_rbsearch(0, rb1)) == NULL)
            { acutRelRb(rb1); GS_ERR_COD = eGSInvRBType; return GS_BAD; }
         if (rb2->resval.rstring == NULL)
            { acutRelRb(rb1); GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
         if (gsc_strcmp(rb2->resval.rstring, _T("SEQEND")) == 0) break;
         if (gsc_strcmp(rb2->resval.rstring, _T("VERTEX")) != 0) 
            { acutRelRb(rb1); GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
         if (contVertex == XVertex)
         {
            // Cerca le coordinate del vertice
            if ((rb2 = gsc_rbsearch(10, rb1)) == NULL)
               { acutRelRb(rb1); GS_ERR_COD = eGSInvRBType; return GS_BAD; }
            ads_point_set(rb2->resval.rpoint, pointVertex);
            // Cerca la larghezza iniziale del vertice
            if ((rb2 = gsc_rbsearch(40, rb1)) == NULL)
               { acutRelRb(rb1); GS_ERR_COD = eGSInvRBType; return GS_BAD; }
            startWidth = rb2->resval.rreal;
            // Cerca la larghezza finale del vertice
            if ((rb2 = gsc_rbsearch(41, rb1)) == NULL)
               { acutRelRb(rb1); GS_ERR_COD = eGSInvRBType; return GS_BAD; }
            endWidth = rb2->resval.rreal;
            // Cerca il flag del controllo del vertice per tangente
            if ((rb2 = gsc_rbsearch(70, rb1)) == NULL)
               { acutRelRb(rb1); GS_ERR_COD = eGSInvRBType; return GS_BAD; }
            if ((rb2->resval.rint & 2) == 2) 
            {
               // Cerca la tangente finale del vertice
               if ((rb2 = gsc_rbsearch(50, rb1)) == NULL)
                  { acutRelRb(rb1); GS_ERR_COD = eGSInvRBType; return GS_BAD; }
               Tangent = rb2->resval.rreal;
            }
            else Tangent = -1;
            if ((*Vvalues = acutBuildList(RTLB, RTPOINT, pointVertex, 
                                                RTREAL,  startWidth,
                                                RTREAL,  endWidth,
                                                RTREAL,  Tangent, 
                                          RTLE, 0)) == NULL)
               { GS_ERR_COD = eGSOutOfMem; acutRelRb(rb1); return GS_BAD; }
            acutRelRb(rb1);

            return GS_GOOD;
         }
         else
            continue;
         if (rb1)
            acutRelRb(rb1);
      }
   }
   else if (gsc_strcmp(rbTwo->resval.rstring, _T("LWPOLYLINE")) == 0)
   {
      // vado alla ricerca del vertice desiderato
      contVertex = 0;
      do
      {
         if (rbOne.SearchNextType(10) != NULL)
         {
            contVertex++;
            if (contVertex == XVertex)
            {
               // punt ora punta alle coordinate del vertice voluto
               ads_point_set(rbOne.get_cursor()->resval.rpoint, pointVertex);
               // ricerco la larghezza iniziale
               if (rbOne.SearchNextType(40) != NULL)
                  startWidth = rbOne.get_cursor()->resval.rreal;
               // ricerco la larghezza finale 
               if (rbOne.SearchNextType(41) != NULL)
                  endWidth = rbOne.get_cursor()->resval.rreal;
               if ((*Vvalues = acutBuildList(RTLB, 
                                             RTPOINT, pointVertex, 
                                             RTREAL,  startWidth,
                                             RTREAL,  endWidth,
                                             RTREAL,  0.0,
                                             RTLE, 0)) == NULL)
                  { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
               break;
            }
         }
      }
      while (1);
   }
   else
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   return GS_GOOD;
}


/******************************************************************************/
/*.doc  gsc_getActualScreeRect                                                */
/*+                                           
   Funzione per restituisce le coordinate degli angoli basso-sinistra e alto-destra
   dello schermo corrente.
   Parametri:
   ads_point LeftLowerPt;
   ads_point RightUpperPt;   
                              
   Restituisce GS_GOOD se tutto OK altrimenti restituisce GS_BAD.
-*/  
/******************************************************************/
int gsc_getActualScreeRect(ads_point LeftLowerPt, ads_point RightUpperPt)
{
   resbuf    buffer;
   ads_point point;
   double    len_X = 0.0, len_Y = 0.0, dim_X = 0.0, dim_Y = 0.0;   
   double    X_center = 0.0, Y_center = 0.0;

   // Dimensioni dello schermo = Variabile SCREENSIZE ( Dim X, Dim Y )
   // lunghezza in unità della ordinata dello schermo = Variabile VIEWSIZE
   // lunghezza in unità dell' ascissa dello schermo:
   // proporzione VIEWSIZE : Dim Y = Incognita : Dim X
   /*   P2                    P3
           ------------------
          |                  |
          |                  |
          |        X         |
          |                  |
          |                  |
           ------------------
        P1                    P4        */

   // leggo variabile VIEWSIZE
   acedGetVar(_T("VIEWSIZE"), &buffer);              // lunghezza ordinata
   if (buffer.restype != RTREAL)
      { GS_ERR_COD = eGSCannotLoadSystemVariable; return GS_BAD; }
   len_Y = buffer.resval.rreal;

   // leggo variabile VIEWCTR ...   punto centrale dello schermo
   acedGetVar(_T("VIEWCTR"), &buffer);
   if (buffer.restype != RT3DPOINT)
      { GS_ERR_COD = eGSCannotLoadSystemVariable; return GS_BAD; } 
   ads_point_set(buffer.resval.rpoint, point);
   X_center = point[0];                       // dimensione ascissa
   Y_center = point[1];                       // dimensione ordinata

   // leggo variabile SCREENSIZE
   acedGetVar(_T("SCREENSIZE"), &buffer);
   if (buffer.restype != RTPOINT)
      { GS_ERR_COD = eGSCannotLoadSystemVariable; return GS_BAD; } 
   ads_point_set(buffer.resval.rpoint, point);

   dim_X = point[0];
   dim_Y = point[1];

   len_X = len_Y * dim_X / dim_Y;                // lunghezza ascissa
   // Calcolo coordinate schermo
   LeftLowerPt[X]  = X_center - ( len_X / 2 );
   LeftLowerPt[Y]  = Y_center - ( len_Y / 2 );
   RightUpperPt[X] = X_center + ( len_X / 2 );
   RightUpperPt[Y] = Y_center + ( len_Y / 2 );

   return GS_GOOD;
}


/********************************************************************************/
/*.doc (mod 2) IsVisibleVertexOnCurrentScreen(ads_name polyline, long *nVertex) */
/*+                                           

   Funzione per la verifica della visibilità di un vertice
   di una polyline nello schermo corrente.
   Serve al comando PEDIT per la gestione dei vertici, perchè 
   non sempre sono visibili nello schermo.

   Parametri:
   ads_name    polyline    polyline da verificare
   long       *nVertex     numero di vertice da verificare
   int         result      Flag per specificare se il vertice 
                           è visibile.
                              
   Attenzione !! la funzione può verificare un vertice qualsiasi
   passato come parametro nVertex, se si passa 0 come parametro
   la funzione cerca, nell'ordine dei vertici della polyline il
   primo vertice visibile e lo restituisce nello stesso parametro.

   Restituisce GS_GOOD se tutto OK altrimenti restituisce GS_BAD.
-*/  
/******************************************************************/
int IsVisibleVertexOnCurrentScreen(ads_name polyline, long *nVertex)
{
   presbuf   values;
   ads_point point, LeftLowerPt, RightUpperPt;
   double    X_min = 0.0, X_max = 0.0, Y_min = 0.0, Y_max = 0.0;   
   long      TotVertex = 0, CurrentVert = 0;
   int       result;

   // Leggo le coordinate limite delo schermo
   if (gsc_getActualScreeRect(LeftLowerPt, RightUpperPt) == GS_BAD)
      return GS_BAD;

   // Calcolo coordinate schermo
   X_min = LeftLowerPt[X];
   X_max = RightUpperPt[X];
   Y_min = LeftLowerPt[Y];
   Y_max = RightUpperPt[Y];

   result = GS_GOOD;
   // Attenzione che 1 corrisponde al primo vertice della Polyline
   // se specificato il vertice allora controllo solo quello
   if (*nVertex > 0)
   {
      do
      {
         // leggo le caratteristiche del vertice nVertex
         if (getPolylineVertexValues(polyline, *nVertex, &values) == GS_BAD)
            { result = GS_BAD; break; }
         if (!values)
            { result = GS_BAD; break; }
         ads_point_set(values->rbnext->resval.rpoint, point);
         // elaboro prima la ascissa
         if (point[0] < X_min) { *nVertex = 0; break; }
         if (point[0] > X_max) { *nVertex = 0; break; }
         // elaboro prima la ordinata
         if (point[1] < Y_min) { *nVertex = 0; break; }
         if (point[1] > Y_max) { *nVertex = 0; break; }
      }
      while(0);
      acutRelRb(values);
   }
   else
   {
      // calcolo il numero dei vertici della polyline
      if ((TotVertex = gsc_get_numvertex(polyline)) == 0)
         { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
      // elaboro tutti i vertici della polyline
      CurrentVert = 1;
      do
      {
         if (CurrentVert <= TotVertex)
         {
            // leggo le caratteristiche del vertice nVertex
            if (getPolylineVertexValues(polyline, CurrentVert, &values) == GS_BAD)
               { result = GS_BAD; break; }
            ads_point_set(values->rbnext->resval.rpoint, point);
            // elaboro prima la ascissa
            if (point[0] < X_min) { CurrentVert++; continue; }
            if (point[0] > X_max) { CurrentVert++; continue; }
            // elaboro prima la ordinata
            if (point[1] < Y_min) { CurrentVert++; continue; }
            if (point[1] > Y_max) { CurrentVert++; continue; }
            *nVertex = CurrentVert;
            acutRelRb(values);
            break;
         }
         else
            break;
      }
      while (1);
   }

   return result;
}


/************************************************************************/
/*.doc (mod 2) WhatIsVisibleVertexOnCurrentScreen(ads_name polyline,    */
/*                                                long *nVertex)        */
/*+                                           

   Funzione che restituisce il numero del vertice visibile sia in senso 
   crescente che decrescente di una polyline nello schermo corrente.
   Serve al comando PEDIT per la gestione dei vertici, perchè 
   non sempre sono visibili nello schermo.

   Parametri:
   ads_name    polyline    polyline da verificare
   long       *nVertex     numero di vertice da verificare
                              
   Attenzione !! la funzione può verificare un vertice qualsiasi
   passato come parametro nVertex, se si passa 0 come parametro
   la funzione cerca, nell'ordine dei vertici della polyline il
   primo vertice visibile e lo restituisce nello stesso parametro.
   
   Restituisce GS_GOOD se tutto OK altrimenti restituisce GS_BAD.
-*/  
/******************************************************************/
int WhatIsVisibleVertexOnCurrentScreen(ads_name polyline, long NumVertOfPline,
                                       int TypeOfSequence, long *nVertex)
{
   presbuf     values;
   resbuf      buffer;
   ads_point   point;
   double      len_X = 0.0, len_Y = 0.0, dim_X = 0.0, dim_Y = 0.0;   
   double      X_min = 0.0, X_max = 0.0, Y_min = 0.0, Y_max = 0.0;   
   double      X_center = 0.0, Y_center = 0.0;
   long        TotVertex = 0, CurrentVert = 0, CurVertex;
   int         result, res, Trovato;

   // Dimensioni dello schermo = Variabile SCREENSIZE ( Dim X, Dim Y )
   // lunghezza in unità della ordinata dello schermo = Variabile VIEWSIZE
   // lunghezza in unità dell' ascissa dello schermo:
   // proporzione VIEWSIZE : Dim Y = Incognita : Dim X
   /*   P2                    P3
           ------------------
          |                  |
          |                  |
          |        X         |
          |                  |
          |                  |
           ------------------
        P1                    P4        */

   // leggo variabile VIEWSIZE
   acedGetVar(_T("VIEWSIZE"), &buffer);              // lunghezza ordinata
   if (buffer.restype == RTREAL)
      len_Y = buffer.resval.rreal;
   else
      { GS_ERR_COD = eGSCannotLoadSystemVariable; return GS_BAD; }
   // leggo variabile VIEWCTR ...   punto centrale dello schermo
   acedGetVar(_T("VIEWCTR"), &buffer);
   if (buffer.restype == RT3DPOINT)
   {
      ads_point_set(buffer.resval.rpoint, point);
      X_center = point[0];                       // dimensione ascissa
      Y_center = point[1];                       // dimensione ordinata
   }
   else
      { GS_ERR_COD = eGSCannotLoadSystemVariable; return GS_BAD; } 
   // leggo variabile SCREENSIZE
   acedGetVar(_T("SCREENSIZE"), &buffer);
   if (buffer.restype == RTPOINT)
   {
      ads_point_set(buffer.resval.rpoint, point);
      dim_X = point[0];
      dim_Y = point[1];
   }
   else
      { GS_ERR_COD = eGSCannotLoadSystemVariable; return GS_BAD; } 
   len_X = len_Y * dim_X / dim_Y;                // lunghezza ascissa
   // Calcolo coordinate schermo
   X_min = X_center - ( len_X / 2 );
   X_max = X_center + ( len_X / 2 );
   Y_min = Y_center - ( len_Y / 2 );
   Y_max = Y_center + ( len_Y / 2 );

   result   = GS_GOOD;
   Trovato  = GS_BAD;
   // Attenzione che 1 corrisponde al primo vertice della Polyline
   // se specificato il vertice allora controllo solo quello
   if (*nVertex > 0)     // se specificato il vert. 
   {  
      CurVertex = *nVertex;
      do
      {
         res = GS_GOOD;
         // leggo le caratteristiche del vertice nVertex
         if (getPolylineVertexValues(polyline, CurVertex, &values) == GS_BAD)
            { result = GS_BAD; break; }
         if (!values)
            { result = GS_BAD; break; }
         ads_point_set(values->rbnext->resval.rpoint, point);
         do
         {
            // elaboro prima la ascissa
            if (point[0] < X_min) { /* *nVertex = 0; */ res = GS_BAD; break; }
            if (point[0] > X_max) { /* *nVertex = 0; */ res = GS_BAD; break; }
            // elaboro prima la ordinata
            if (point[1] < Y_min) { /* *nVertex = 0; */ res = GS_BAD; break; }
            if (point[1] > Y_max) { /* *nVertex = 0; */ res = GS_BAD; break; }
            Trovato = GS_GOOD;
            res = GS_GOOD;
            break;
         }
         while (1);

         if (res == GS_GOOD)
            { *nVertex = CurVertex; result = GS_GOOD; break; }
         else
         {
            if (TypeOfSequence == CRESCENTE)
            {
               if (CurVertex == NumVertOfPline)
               { *nVertex = -1; break; }
               else
                  CurVertex++;
            }
            else
            {
               if (CurVertex == 1)
                  { *nVertex = -1; break; }
               else
                  CurVertex--;
            }   
         }
      }
      while(1);
      acutRelRb(values);
   }
   else
   {
      // calcolo il numero dei vertici della polyline
      if ((TotVertex = gsc_get_numvertex(polyline)) == 0)
         { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
      // elaboro tutti i vertici della polyline
      CurrentVert = 1;
      do
      {
         if (CurrentVert <= TotVertex)
         {
            // leggo le caratteristiche del vertice nVertex
            if (getPolylineVertexValues(polyline, CurrentVert, &values) == GS_BAD)
               { result = GS_BAD; break; }
            ads_point_set(values->rbnext->resval.rpoint, point);
            // elaboro prima la ascissa
            if (point[0] < X_min) { CurrentVert++; continue; }
            if (point[0] > X_max) { CurrentVert++; continue; }
            // elaboro prima la ordinata
            if (point[1] < Y_min) { CurrentVert++; continue; }
            if (point[1] > Y_max) { CurrentVert++; continue; }
            *nVertex = CurrentVert;
            acutRelRb(values);
            break;
         }
         else
            break;
      }
      while (1);
   }

   return result;
}


/**************************************************************/
/*.doc gshatch                                   <external>   */
/*+
  Funzione che inserisce un riempimento alle entità di tipo
  superficie/spaghetti.
  C_SELSET *selset;    Gruppo di selezione
  TCHAR *hatch;        Nome riempimento
  double scale;        Scala del riempimento
  double rotation;     Rotazione del riempimento in gradi
  C-COLOR *pColor;     Opzionale, colore del riempimento. Se = NULL
                       verrà usato il colore di default della classe
  TCHAR *layer;        Opzionale, layer del riempimento. Se = NULL
                       verrà usato il layer di default della classe
  int CounterToVideo;  Flag, se = GS_GOOD stampa a video il numero di entità che si 
                       stanno elaborando (default = GS_BAD)
-*/  
/**************************************************************/
void gshatch(void)
{
   ads_name      SelSet;
   static TCHAR  hatch[MAX_LEN_HATCHNAME] = _T("?");
   TCHAR         answer[MAX_LEN_HATCHNAME], prompt[50];
   static double scale = 1, rotation = 0;
   double        TmpScale, TmpRotation;
   C_STRING      init, msg;
   int           res;

   GEOsimAppl::CMDLIST.StartCmd();

   msg = _T('\n');       
   msg += gsc_msg(602);      // "Nome riempimento [?/Solido] <"
   msg += hatch;
   msg += _T(">: ");

   init = gsc_msg(603);      // "?"
   init += _T(" ");
   init += gsc_msg(604);      // "Solido"

   acedInitGet(RSG_OTHER, init.get_name());	// "? Solido"
   res = acedGetKword(msg.get_name(), answer);
   if (res == RTNONE) gsc_strcpy(answer, hatch, MAX_LEN_HATCHNAME);  // Input return 
   else if (res == RTERROR) return GEOsimAppl::CMDLIST.ErrorCmd();   // Input Errato
   else if (res == RTCAN) return GEOsimAppl::CMDLIST.CancelCmd();      // Input ^C
   
   gsc_alltrim(answer);
   if (gsc_strcmp(answer, gsc_msg(603)) == 0) // "?"
   { // stampa lista dei riempimenti
      if (gsc_printGEOsimHatchList() == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();
      return GEOsimAppl::CMDLIST.EndCmd();
   }
   // controllo validità
   if (gsc_validhatch(answer) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();
   
   TmpScale    = scale;
   TmpRotation = rotation;
   if (gsc_strcmp(answer, gsc_msg(604), FALSE) != 0) // "Solido" compare case insensitive
   {  // chiedo scala
      swprintf(prompt, 50, gsc_msg(605), TmpScale); // "\nFattore di scala <%f>: "
      acedInitGet(RSG_NOZERO + RSG_NONEG, GS_EMPTYSTR);
      if ((res = ads_getreal(prompt, &TmpScale)) == RTCAN) return GEOsimAppl::CMDLIST.CancelCmd();
      // chiedo rotazione
      swprintf(prompt, 50, gsc_msg(19), TmpRotation); // "\nAngolo di rotazione <%f>: "
      if ((res = ads_getreal(prompt, &TmpRotation)) == RTCAN) return GEOsimAppl::CMDLIST.CancelCmd();
   }
  
   // selezione delle entità
   if (gsc_ssget(NULL, NULL, NULL, NULL, SelSet) != RTNORM) return GEOsimAppl::CMDLIST.CancelCmd();

   if (gsc_startTransaction() == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

   if (gsc_hatch(SelSet, answer, TmpScale, TmpRotation, NULL, NULL, GS_GOOD) == GS_BAD)
      { ads_ssfree(SelSet); gsc_abortTransaction(); return GEOsimAppl::CMDLIST.ErrorCmd(); }
   
   ads_ssfree(SelSet);
   gsc_strcpy(hatch, answer, MAX_LEN_HATCHNAME);
   scale    = TmpScale;
   rotation = TmpRotation;

   return GEOsimAppl::CMDLIST.EndCmd();
}
int gsc_hatch(ads_name SelSet, TCHAR *hatch, double scale, double rotation,
              C_COLOR *pColor, TCHAR *layer, int CounterToVideo)
{
   int      result = GS_GOOD;
   C_SELSET privateSS, AlignSS, entSS;
   long     key;
   ads_name entity;
   C_EED    eed;
   C_CLASS  *pCls;
   int       NoUpdateableWrkSessionErr = eGSNoError;

   // verifico l'abilitazione dell'utente
   if (gsc_check_op(opModEntity) == GS_BAD) return GS_BAD;
   if (GS_CURRENT_WRK_SESSION) GS_CURRENT_WRK_SESSION->isReadyToUpd(&NoUpdateableWrkSessionErr);
   else NoUpdateableWrkSessionErr = eGSNotCurrentSession;

   if (privateSS.add_selset(SelSet) == GS_BAD) return GS_BAD;

   // ciclo per filtrare oggetti validi (modificabili e di classe tipo superficie)
   // a cui viene associato il riempimento
   while (privateSS.entname(0, entity) == GS_GOOD)
   {
      // controllo se entità di GEOsim
      if (eed.load(entity) == GS_BAD) 
      {
         privateSS.subtract_ent(entity);
         entSS.add(entity);
         // Aggiorno i riempimenti modificando ls
         if (gsc_setHatchSS(entSS, hatch, scale, rotation, pColor, layer) == GS_BAD)
            { result = GS_BAD; break; }

         continue; 
      }

      if (NoUpdateableWrkSessionErr) { GS_ERR_COD = NoUpdateableWrkSessionErr; return GS_BAD; }

      // Ritorna il puntatore alla classe cercata
      if ((pCls = GS_CURRENT_WRK_SESSION->find_class(eed.cls, eed.sub)) == NULL)
   	   { result = GS_BAD; break; }

      if (pCls->get_type() == TYPE_SURFACE)
      {
         // leggo valore chiave e il gruppo di selezione
         if (pCls->get_Key_SelSet(entity, &key, entSS) == GS_BAD)
            { privateSS.subtract_ent(entity); continue; }
         // se entità non aggiornabile
         if (pCls->is_updateableSS(key, entSS, NULL, GS_GOOD, GS_GOOD) == GS_BAD)
         {
            privateSS.subtract_ent(entity);
            continue;
         }
         privateSS.subtract(entSS);
      }
      else
      if (pCls->get_category() == CAT_SPAGHETTI)
      {
         privateSS.subtract_ent(entity);
         entSS.clear();
         entSS.add(entity);
      }
      else
         { privateSS.subtract_ent(entity); continue; }

      // Aggiorno i riempimenti modificando ls
      if (gsc_UpdToDefHatch(entSS, hatch, scale, rotation,
                            (pColor) ? pColor : &(pCls->ptr_fas()->hatch_color),
                            (layer) ? layer : pCls->ptr_fas()->hatch_layer) == GS_BAD)
         { result = GS_BAD; break; }

      if (pCls->get_type() == TYPE_SURFACE)
         if (AlignSS.add_selset(entSS) == GS_BAD) return GS_BAD;
   }
   
   if (result == GS_BAD) return GS_BAD;

   GEOsimAppl::REFUSED_SS.clear();

   //                  selset,change_fas,AttribValuesFromVideo,SS,CounterToVideo,tipo modifica
   if (gsc_class_align(AlignSS, GS_BAD, GS_BAD, &(GEOsimAppl::REFUSED_SS), 
                       CounterToVideo, FAS_MOD) == -1)              
      return GS_BAD;

   return GS_GOOD;
}


/**************************************************************/
/*.doc gssaveas()                                  <external>   */
/*+
   Funzione di salva come ... non possibile in GEOsim 
-*/  
/**************************************************************/
void gssaveas()
{
   acutPrintf(gsc_msg(452));  // Comando non disponibile
   return;
}


/**************************************************************/
/*.doc gssave() e gsqsave                               <external>   */
/*+
   Funzione per il comando save di GEOsim 
-*/  
/**************************************************************/
void gssave()
{
   if (GS_CURRENT_WRK_SESSION) // se siamo in una sessione di lavoro di GEOsim
   {
      GEOsimAppl::CMDLIST.StartCmd();
      if (GS_CURRENT_WRK_SESSION->save() == GS_BAD) GEOsimAppl::CMDLIST.ErrorCmd();
      else GEOsimAppl::CMDLIST.EndCmd();
   }
}
void gsqsave() { gssave(); }


/**************************************************************/
/*.doc gsnew()                                    <external>   */
/*+
   Funzione per il comando new di GEOsim 
-*/  
/**************************************************************/
void gsnew()
{
   int      rispdef = 1, ris = GS_GOOD;
   C_STRING stringa;

   if (gsc_isCurrentDWGModified() && GS_CURRENT_WRK_SESSION) // disegno modificato
   {
		stringa = gsc_msg(644); // "Salvare le modifiche della sessione"
      stringa += _T(" ");
      stringa += GS_CURRENT_WRK_SESSION->get_name();

      // Chiedo la conferma per il salvataggio
      if (gsc_ddgetconfirm(stringa.get_name(), &ris, rispdef, GS_BAD, GS_GOOD) == GS_BAD) 
         return;
      
      // In base alla risposta salva oppure no
      if (ris == GS_GOOD)   
      {
         // controllo se ci sono oggetti da riallineare per il main ("dofun"
         // in GEOSIM.CPP" non effettua questo controllo nel caso del comando quit
         gsc_align_data_base();

         // Partenza del salvataggio di GEOsim
         if (GS_CURRENT_WRK_SESSION->save() == GS_BAD) return;
      }
   }

   if (ris == GS_GOOD || ris == GS_BAD)
      // A questo punto esco dalla sessione attiva
      if (gsc_ExitCurrSession() == GS_BAD)
         return;

   return;
}


/**************************************************************/
/*.doc gsopen                                    <external>   */
/*+
   Implementa il comando Open di AutoCAD
-*/  
/**************************************************************/
void gsopen()
{
   int      rispdef = 1, ris = GS_GOOD;
   C_STRING stringa, nameDwg;

   if (GS_CURRENT_WRK_SESSION) // se siamo in una sessione di lavoro di GEOsim
      // Il comando non è stato lanciato da GEOsim
      if (GEOsimAppl::CMDLIST.CmdRunningFromInternalCall == false)
      {
         acutPrintf(gsc_msg(452)); // "\nComando non disponibile."
         return;
      }

   GEOsimAppl::CMDLIST.StartCmd();

   if (gsc_isCurrentDWGModified()) // disegno modificato
   {
		stringa = gsc_msg(644); // "Salvare le modifiche della sessione" 
      stringa += _T(" ");
      stringa += GS_CURRENT_WRK_SESSION->get_name();

      // Chiedo la conferma per il salvataggio
      if (gsc_ddgetconfirm(stringa.get_name(), &ris, rispdef, GS_BAD, GS_GOOD) == GS_BAD) 
         return GEOsimAppl::CMDLIST.ErrorCmd();
   }

   if (ris == GS_GOOD || ris == GS_BAD)
   {
	   // dcl per la scelta del file da aprire "GEOsim - Seleziona file"
      if (gsc_GetFileD(gsc_msg(645), GS_CURRENT_WRK_SESSION->get_pPrj()->get_dir(), _T("dwg"),
                       8 + 16, nameDwg) != RTNORM)
         return GEOsimAppl::CMDLIST.ErrorCmd();

      if (ris == GS_GOOD)   // il disegno è stato salvato
      {
         // controllo se ci sono oggetti da riallineare per il main ("dofun"
         // in GEOSIM.CPP" non effettua questo controllo nel caso del comando quit
         gsc_align_data_base();

         // Partenza del salvataggio di GEOsim
         if (!GS_CURRENT_WRK_SESSION)
            { GS_ERR_COD = eGSNotCurrentSession; return GEOsimAppl::CMDLIST.ErrorCmd(); }
         if (GS_CURRENT_WRK_SESSION->save() == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

         if (acedCommandS(RTSTR, _T("_.OPEN"), RTSTR, nameDwg.get_name(), 0) != RTNORM)
            { GS_ERR_COD = eGSErrorExecutingCommand; return GEOsimAppl::CMDLIST.ErrorCmd(); }
      }
      else                  // il disegno non è stato salvato
      {
         if (acedCommandS(RTSTR, _T("_.OPEN"), RTSTR, _T("_Y"), RTSTR, nameDwg.get_name(), 0) != RTNORM)
            { GS_ERR_COD = eGSErrorExecutingCommand; return GEOsimAppl::CMDLIST.ErrorCmd(); }
      }
   }
   
   return GEOsimAppl::CMDLIST.EndCmd();
}


///////////////////////////////////////////////////////////////////////////
/* 
Questa funzione varia la scala dei blocchi,attributi e l'atezza testi-attributi
per le classi estratte. E' invocata dal LISP con parametri di passaggio
<gruppo di selezione> <val_sc_simb> <val_sc_attr> <val_h_attr> <val_h_text>
<f_sc_simb> <f_scal_attr> <f_h_attr> <f_h_text> <f_layer> <f_saveobj> .
*/                                                    
///////////////////////////////////////////////////////////////////////////
int gs_scal_attr_text(void)
{
   int      ris, f_layer, f_saveobj, f_sc_simb, f_scal_attr, f_h_attr, f_h_text;
   double   v_sc_simb, v_sc_attr, v_hattr, v_htext;
   presbuf  arg;
	C_SELSET SelObj;

   acedRetNil();

   // ricavo i valori impostati
   arg = acedGetArgs();

   if (arg == NULL) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (arg->restype == RTPICKS) // gruppo di selezione
      if (SelObj.add_selset(arg->resval.rlname) == GS_BAD) return RTERROR; 
   arg = arg->rbnext;
   if (arg == NULL || arg->restype != RTREAL) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   else v_sc_simb = arg->resval.rreal;     // scala blocchi
   arg = arg->rbnext;
   if (arg == NULL || arg->restype != RTREAL) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   else v_sc_attr = arg->resval.rreal;     // scala blocchi attributi
   arg = arg->rbnext;
   if (arg == NULL || arg->restype != RTREAL) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   else v_hattr = arg->resval.rreal;       // altezza attributi
   arg = arg->rbnext;
   if (arg == NULL || arg->restype != RTREAL) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   else v_htext = arg->resval.rreal;       // altezza testi
   arg = arg->rbnext;
   if (arg == NULL || arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   else f_sc_simb = arg->resval.rint;      // flag scala blocchi
   arg = arg->rbnext;
   if (arg == NULL || arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   else f_scal_attr = arg->resval.rint;    // flag scala blocchi attributi
   arg = arg->rbnext;
   if (arg == NULL || arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   else f_h_attr = arg->resval.rint;       // flag altezza testo attibuti
   arg = arg->rbnext;
   if (arg == NULL || arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   else f_h_text = arg->resval.rint;       // flag altezza testi
   arg = arg->rbnext;
   if (arg == NULL || arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   else f_layer = arg->resval.rint;        // flag stato del piano
   arg = arg->rbnext;
   if (arg == NULL || arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   else f_saveobj = arg->resval.rint;      // flag opzione di salvataggio

   if (SelObj.length() == 0)
	   // Seleziono le entità da modificare
	   if (gsc_ssget(NULL, NULL, NULL, NULL, SelObj) != RTNORM) return RTCAN;

   // Se almeno uno dei quattro valori di flag sono # 0 chiamo la funzione 
   // che effettua il cambio scala e/o altezza testo-attributi
   if ((f_sc_simb != 0) || (f_scal_attr != 0) || (f_h_attr != 0) || (f_h_text != 0)) 
		ris = gsc_scal_attr_text(v_sc_simb, v_sc_attr, v_hattr, v_htext, 
			                      f_sc_simb, f_scal_attr, f_h_attr, f_h_text,
				                   f_layer, f_saveobj, SelObj);
   else return RTNORM;

   if (ris == GS_CAN) { acedRetNil(); return RTNORM; }
   if (ris != GS_GOOD) return RTERROR;

   acedRetT();
     
   return RTNORM;
}


/*********************************************************************/
/*.doc gsc_scal_attr_text <internal> */
/*+
  Questa funzione riceve riceve i valori di scala per i simboli e/o gli
  attributi visibili e il valore di altezza pei i testi e/o gli attributi 
  e li modifica in grafica.
  Parametri:
  double v_sc_simb  : valore scala simboli
  double v_sc_attr  : valore scala attributi
  double v_hattr    : valore altezza attributi
  double v_htext    : valote altezza testi
  int  f_sc_simb    : flag che indica se considerare la scala simboli
                      Valori 0 = non considerare
                             1 = considerare
  int  f_sc_attr    : flag che indica se considerare la scala attributi
                      Valori 0 = non considerare
                             1 = considerare
  int  f_h_attr     : flag che indica se considerare l'altezza attributi
                      Valori 0 = non considerare
                             1 = considerare
  int  f_h_text     : flag che indica se considerare l'altezza testi
                      Valori 0 = non considerare
                             1 = considerare
  int  f_layer      : flag che dice se considerare lo stato del piano (ON/OFF 
                      Congelato/Scongelato) su cui giace il simbolo e gli attributi 
                      per operare o no i cambiamenti impostati.
                      Valori 0 = non considerare
                             1 = considerare
  int  f_saveobj    : flag che dice se inserire gli oggetti modificati nel gruppo di salvatggio
                      Valori 0 = non inserire gli oggetti
                             1 = inserisci gli oggetti
  C_SELSET &SelSet  : Gruppo di oggetti su cui operare

  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_scal_attr_text(double v_sc_simb, double v_sc_attr, double v_hattr, double v_htext,
                       int f_sc_simb, int f_sc_attr, int f_h_attr, int f_h_text,
                       int f_layer, int f_saveobj, C_SELSET &SelSet)
{
   int        cls_cat, cls_type, flag_text = 0;
   C_STR_LIST lista_attr_calc;
   C_CLS_PUNT_LIST extr_cls_list;
   C_CLS_PUNT *punt;
   C_FAS      *p_fas;
   C_CLASS    *extr_cls;        
   C_ID       *p_id;
   C_SELSET   GlobalSelSet;

   // verifico la validità dei dati impostati
   if (gsc_validscale(v_sc_simb) == GS_BAD) return GS_BAD;
   if (gsc_validhtext(v_htext) == GS_BAD) return GS_BAD;
   if (gsc_validscale(v_sc_attr) == GS_BAD) return GS_BAD;
   if (gsc_validhtext(v_hattr) == GS_BAD) return GS_BAD;

   // verifico l'abilitazione dell'utente;
   if (gsc_check_op(opScaleAttrText) == GS_BAD) return GS_BAD;

   // Se si è deciso di variare la dimensioni degli attributi visibili
   if ((f_sc_attr == TRUE) || (f_h_attr ==TRUE))
   {
      // Includo i blocchi di attributi visibili per gli oggetti principali
      SelSet.copy(GlobalSelSet);
      GlobalSelSet.add_DA();
   }

   // Start Undo
   if (gsc_startTransaction()==GS_BAD) return GS_BAD;

   // Ricavo la lista delle classi estratte del progetto corrente
   if (GS_CURRENT_WRK_SESSION->get_pPrj()->extracted_class(extr_cls_list) == GS_BAD)
      { gsc_abortTransaction(); return GS_BAD; }

   punt = (C_CLS_PUNT *) extr_cls_list.get_head();
   while (punt)
   { 
      // ricavo il puntatore alla classe estratta
      extr_cls = (C_CLASS *) (punt->get_class());
      p_id = extr_cls->ptr_id();
      cls_cat = p_id->category;

      // a seconda del tipo di classe estratta e in funzione dei valori
      // di scala e/o altezza testi impostati invoco la funzione del caso
      switch (cls_cat)
      {
         case CAT_SIMPLEX:
            p_fas = extr_cls->ptr_fas();
            cls_type = p_id->type;
            switch (cls_type)
            {
               case TYPE_POLYLINE:
                  // scalo gli attributi se almeno uno dei flag è # 0
                  if ((f_sc_attr == TRUE) || (f_h_attr == TRUE))
                     if (gsc_change_simb_attr(_T("$T"), v_sc_attr, v_hattr, f_sc_attr, 
                         f_h_attr, f_layer, f_saveobj, extr_cls, GlobalSelSet) == GS_BAD)
                         { gsc_abortTransaction(); return GS_BAD; }
                  break;
               case TYPE_TEXT:
                  // scalo i testi se flag attivo
                  if (f_h_text == TRUE)
                     if (gsc_change_text(v_htext, f_layer, f_saveobj, extr_cls, SelSet) == GS_BAD)
                        { gsc_abortTransaction(); return GS_BAD; }
                  break;
               case TYPE_NODE:
                  // scalo i simboli se flag attivo 
                  if ((v_sc_simb != 1.0) && (f_sc_simb == TRUE))
                     if (gsc_change_simb_attr (p_fas->block, v_sc_simb, 0.0, f_sc_attr,
                         f_h_attr, f_layer, f_saveobj, extr_cls, SelSet) == GS_BAD)
                        { gsc_abortTransaction(); return GS_BAD; }

                  // scalo gli attributi se almeno uno dei flag è # 0
                  if ((f_sc_attr == TRUE) || (f_h_attr ==TRUE))
                     if (gsc_change_simb_attr(_T("$T"), v_sc_attr, v_hattr, f_sc_attr,
                         f_h_attr, f_layer, f_saveobj, extr_cls, GlobalSelSet) == GS_BAD)
                        { gsc_abortTransaction(); return GS_BAD; }
                  break;
               case TYPE_SURFACE:
                  // scalo gli attributi se almeno uno dei flag è # 0
                  if ((f_sc_attr == TRUE) || (f_h_attr ==TRUE))
                     if (gsc_change_simb_attr(_T("$T"), v_sc_attr, v_hattr, f_sc_attr,
                         f_h_attr, f_layer, f_saveobj, extr_cls, GlobalSelSet) == GS_BAD)
                        { gsc_abortTransaction(); return GS_BAD; }
                  break;
            }
            break;
         case CAT_EXTERN:
            C_SUB  *p_sub;
            p_sub = (C_SUB*)(extr_cls->ptr_sub_list())->get_head();
            while (p_sub)
            {
               p_fas = p_sub->ptr_fas();
               cls_type = p_sub->get_type();
               switch (cls_type)
               {
                  case TYPE_POLYLINE:
                     // scalo gli attributi se almeno uno dei flag è # 0
                     if ((f_sc_attr == TRUE) || (f_h_attr ==TRUE))
                        if (gsc_change_simb_attr(_T("$T"), v_sc_attr, v_hattr, f_sc_attr,
                            f_h_attr, f_layer, f_saveobj, p_sub, GlobalSelSet) == GS_BAD)
                           { gsc_abortTransaction(); return GS_BAD; }
                     break;
                  case TYPE_TEXT:
                     // scalo i testi se flag attivo
                     if (f_h_text == TRUE)
                        if (gsc_change_text(v_htext, f_layer, f_saveobj, p_sub, SelSet) == GS_BAD)
                           { gsc_abortTransaction(); return GS_BAD; }
                        break;
                  case TYPE_NODE:
                     // scalo i simboli se flag attivo
                     if ((v_sc_simb != 1.0) && (f_sc_simb == TRUE))
                        if (gsc_change_simb_attr (p_fas->block, v_sc_simb, 0,  f_sc_attr,
                           f_h_attr, f_layer, f_saveobj, p_sub, SelSet) == GS_BAD)
                           { gsc_abortTransaction(); return GS_BAD; }

                     // scalo gli attributi se almeno uno dei flag è # 0
                     if ((f_sc_attr == TRUE) || (f_h_attr ==TRUE))
                        if (gsc_change_simb_attr(_T("$T"), v_sc_attr, v_hattr, f_sc_attr,
                            f_h_attr, f_layer, f_saveobj, p_sub, GlobalSelSet) == GS_BAD)
                           { gsc_abortTransaction(); return GS_BAD; }
                     break;

                  case TYPE_SURFACE:
                     // scalo gli attributi se almeno uno dei flag è # 0
                     if ((f_sc_attr == TRUE) || (f_h_attr ==TRUE))
                        if (gsc_change_simb_attr(_T("$T"), v_sc_attr, v_hattr, f_sc_attr,
                            f_h_attr, f_layer, f_saveobj, p_sub, GlobalSelSet) == GS_BAD)
                           { gsc_abortTransaction(); return GS_BAD; }
                     break;
               }
               p_sub = (C_SUB *) p_sub->get_next();
            }
            break;
      }
      punt = (C_CLS_PUNT *) punt->get_next();
   }

   // End Undo
   if (gsc_endTransaction()==GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************************/
/*.doc gsc_change_simb_attr <internal> */
/*+
  Questa funzione riceve il nome del blocco che deve essere modificato ed
  eventuale valore di scala degli attributi stessi e li modifica in grafica.
  Parametri:
  TCHAR *what      : nome blocco (anche di attributi) da modificare
  double scala     : valore scala (anche di attributi se scelto di modificarli)
  double altezza   : valore altezza attributi se scelto di modificarli 
  int   val_sc_attr: flag che discerne se scalare gli attributi oppure no
                     Valori: 0 = non scalare; 1 = scalare.
  int   val_h_attr : flag che discerne se variare l'altezza degli attributi oppure no
                     Valori: 0 = non variare; 1 = variare.
  int   val_layer  : valore che discerne se cosiderare lo stato del piano oppure no
                     Valori: 0 = non considerare;  1 = considerare
  int   val_save   : valore di abilitazione del salvataggio
                     Valori: 0 = non salvare gli oggetti modificati;
                             1 = salvare gli oggetti modificati;
  C_CLASS *pCls    : puntatore alla classe dell'oggetto 
  C_SELSET &SelSet;	Gruppo di oggetti su cui operare

  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_change_simb_attr(TCHAR *what, double scala, double altezza,int val_sc_attr,
                         int val_h_attr, int val_layer, int val_save, C_CLASS *pCls,
								 C_SELSET &SelSet)
{
  	ads_name   ent, ndx;
   C_SELSET   UpdSet, FiltrSet;
	long       i = 0, lungh1 = 0, lungh2 = 0, no_upd = 0, cnt_upd = 0;
   TCHAR      piano[MAX_LEN_LAYERNAME];
   int        link_obj, prj, isGraphCalculated, OldOp;
   ads_point  p_ins;
   presbuf    pRBName, pRBPoint, pRBText, pRBLayer, pRBFathLayer, prb;
   C_RB_LIST  entmask, FathAttrib, Attrib, ColValues;
   C_PREPARED_CMD_LIST CmdList;
	C_FAS      *pfas = pCls->ptr_fas();

   C_STRING   CompleteName;
   pCls->get_CompleteName(CompleteName);
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(1056), CompleteName.get_name()); // "Elaborazione classe <%s>"
	   
	if (!pfas) return GS_BAD;

   if (pCls->ptr_fas()->is_graph_calculated() == GS_BAD)
      isGraphCalculated = FALSE;
   else
   {
      isGraphCalculated = TRUE;
      // Preparo i comandi di lettura dei dati della classe dal temp/old
      if (pCls->prepare_data(CmdList) == GS_BAD) return GS_BAD;
   }

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   prj = GS_CURRENT_WRK_SESSION->get_PrjId();

   // se il blocco è $T cerco quelli appartenenti alla classe che sto trattando
   if (gsc_strcmp(what, _T("$T")) == NULL) link_obj = DA_BLOCK;
   else link_obj = GRAPHICAL;

   // Ricavo tutti i blocchi scheda della classe pCls 
   if (pCls->get_SelSet(FiltrSet, link_obj) != GS_GOOD) return GS_BAD;
	FiltrSet.intersect(SelSet);

   // Ricavo il gruppo di selezione dal link set se questo non è vuoto
   if ((lungh1 = FiltrSet.length()) == 0) return GS_GOOD;

   // verifico se il gruppo di selezione è vuoto o ritorna errore
   if (lungh1 == -1)
   {
      resbuf p;
      acedGetVar(_T("ERRNO"), &p);
      if (p.resval.rint != 0) return GS_BAD;
   }

   StatusLineMsg.Init(gsc_msg(70), LITTLE_STEP); // ogni 10 "%ld oggetti grafici elaborati."

   // se il blocco è diverso da $T testo lo stato del piano su cui giace il blocco
   if (gsc_strcmp(what, _T("$T")) != 0)
   {
      if (FiltrSet.entname(1, ent) == GS_GOOD)
      {
         if ((FathAttrib << acdbEntGet(ent)) == NULL) return GS_BAD;

         if (val_layer == 1)  
         {
            if ((pRBLayer = FathAttrib.SearchType(8)) != NULL)
                gsc_strcpy(piano, pRBLayer->resval.rstring, MAX_LEN_LAYERNAME);

            // se il piano è congelato, oppure OFF, oppure bloccato non eseguo operazioni
            if ((gsc_is_freezeLayer(piano) == GS_GOOD) ||
                (gsc_is_offLayer(piano) == GS_GOOD)    ||
                (gsc_is_lockedLayer(piano) == GS_GOOD))
               return GS_GOOD;
         }
      }
   }

	while (FiltrSet.entname(i++, ent) == GS_GOOD)
	{
      if ((FathAttrib << acdbEntGet(ent)) == NULL) return GS_BAD;
      
      // se il blocco è diverso da $T vario la scala XY dello stesso
      if (gsc_strcmp(what, _T("$T")) != 0)
      {
         // nuova scala relativa alla precedente
         if (gsc_set_scale(ent, scala, scala, 1, false) == GS_BAD) return GS_BAD;
      }
      else // altrimenti memorizzo il punto di inserimento dell'intero blocco attributi
      {
         if (((pRBPoint = FathAttrib.SearchType(10)) == NULL) || 
             ((pRBFathLayer = FathAttrib.SearchType(8)) == NULL))
            { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
         ads_point_set(pRBPoint->resval.rpoint, p_ins);
      }

      // se sto trattando un blocco $T ciclo sugli attributi del blocco
      if (gsc_strcmp(what, _T("$T")) == NULL)
      {
         ads_name_set(ent, ndx);
         while (ads_entnext(ndx, ndx) == RTNORM)
         {
            if ((Attrib << acdbEntGet(ndx)) == NULL) return GS_BAD;
            // vedo se è una etichetta di "SEQEND"
            if ((prb = Attrib.SearchType(0)) == NULL || 
               gsc_strcmp(prb->resval.rstring, _T("SEQEND")) == 0)
               break;
            // ho impostato il flag che dice di considerare lo stato dei Layer dove
            // risisedono gli attributi visibili
            if (val_layer == 1)  
            {
               if ((pRBLayer = Attrib.SearchType(8)) != NULL)
					{
						if (gsc_strcmp(pRBLayer->resval.rstring, _T("0"), FALSE) == NULL)
							gsc_strcpy(piano, pRBFathLayer->resval.rstring, MAX_LEN_LAYERNAME);
						else
							gsc_strcpy(piano, pRBLayer->resval.rstring, MAX_LEN_LAYERNAME);
					}
               // se il piano è congelato, oppure OFF, oppure bloccato non eseguo operazioni
               if ((gsc_is_freezeLayer(piano) == GS_GOOD) ||
                   (gsc_is_offLayer(piano) == GS_GOOD)    ||
                   (gsc_is_lockedLayer(piano) == GS_GOOD)) continue;
            }

            if ((pRBName  = Attrib.SearchType(2)) != NULL &&
                (pRBPoint = Attrib.SearchType(10)) != NULL &&
                (pRBText  = Attrib.SearchType(40)) != NULL)
            {
               // se flag attivo vario l'altezza attributi
               if (val_h_attr == TRUE) pRBText->resval.rreal = altezza;

               // scala # 1 e flag di cambio altezza = 1
               if ((scala != 1.0) && (val_sc_attr == TRUE))
               {
                  pRBText->resval.rreal = pRBText->resval.rreal * scala;
                  pRBPoint->resval.rpoint[Y] = (pRBPoint->resval.rpoint[Y] 
                                                - p_ins[Y]) * scala + p_ins[Y];
                  pRBPoint->resval.rpoint[X] = (pRBPoint->resval.rpoint[X] 
                                                - p_ins[X]) * scala + p_ins[X];
               }

               gsc_set_hText(ndx, pRBText->resval.rreal);
               gsc_set_firstPoint(ndx, pRBPoint->resval.rpoint);
            }
         }
      }

      if (val_save == 1)
      {
         if (UpdSet.add(ent) == GS_BAD) return GS_BAD;
      }
		else
		{
			// se esistono funzioni di calcolo grafico
			// le applico perchè potrebbero variare gli attributi visibili
			if (isGraphCalculated)
				if (pCls->query_data(ent, ColValues, &CmdList) == GS_GOOD)
				{
				 // Setto l'operazione corrente
             OldOp = set_GS_CURRENT_OPERATION(MODIFY);
				 pCls->graph_calc(ColValues);
				 // Ripristino la situazione precedente
             set_GS_CURRENT_OPERATION(OldOp);
				}

		}

      StatusLineMsg.Set(++cnt_upd); // "%ld oggetti grafici elaborati."
   }
   // evidenzio il numero di entità non modificabili
   StatusLineMsg.End(gsc_msg(70), cnt_upd); // "%ld oggetti grafici elaborati."

   if (cnt_upd != (i-1))
      acutPrintf(gsc_msg(434), (i-1-cnt_upd), i-1); // "\nEscluse dall'operazione %ld entità su %ld in quanto non modificabili."

   // se il gruppo di selezione degli oggetti da salvare non è vuoto e 
	// il numero di oggetti aggiornabili è uguale al numero di oggetti del gruppo
	// di selezione di partenza lancio la funzione di riallineamento degli oggetti
   if (((lungh2 = UpdSet.length()) > 0) && (val_save == 1))
   {  
      if (lungh1 == lungh2)
      {
         int previous = GEOsimAppl::GLOBALVARS.get_AddEntityToSaveSet();

         GEOsimAppl::GLOBALVARS.set_AddEntityToSaveSet(GS_GOOD);
         GEOsimAppl::REFUSED_SS.clear();

         if (gsc_class_align(UpdSet, GS_BAD, GS_BAD, &(GEOsimAppl::REFUSED_SS), 
                             GS_GOOD, FAS_MOD) == -1)
               { GEOsimAppl::GLOBALVARS.set_AddEntityToSaveSet(previous); return GS_BAD; }
         GEOsimAppl::GLOBALVARS.set_AddEntityToSaveSet(previous);
      }
   }

   return GS_GOOD;
}


/*********************************************************************/
/*.doc gsc_change_text <internal> */
/*+
  Questa funzione riceve il nome del testo che deve essere modificato e
  il valore di altezza dello stesso ed effettua la modifica in grafica.
  Parametri:
  float h_text     : valore altezza testi.
  int   val_layer  : valore che discerne se cosiderare lo stato del piano oppure no
                     Valori: 0 = non considerare;  1 = considerare
  int   val_save   : valore di abilitazione del salvataggio
                     Valori: 0 = non salvare gli oggetti modificati;
                             1 = salvare gli oggetti modificati;
  C_CLASS *pCls    : puntatore alla classe dell'oggetto 
  C_SELSET &SelSet : Gruppo di oggetti su cui operare

  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_change_text(double h_text, int val_layer, int val_save, 
						  C_CLASS *pCls, C_SELSET &SelSet)
{
  	ads_name   ent;
	long       cnt_upd = 0, i = 0, lungh1 = 0, lungh2 = 0, no_upd = 0;
   TCHAR      piano[MAX_LEN_LAYERNAME];
   presbuf    pRBLayer;
   C_SELSET   UpdSet, FiltrSet;
   C_RB_LIST  Attrib, ColValues;        
	C_PREPARED_CMD_LIST CmdList;
	C_FAS      *pfas = pCls->ptr_fas();
   int        prj, OldOp, isGraphCalculated;

   C_STRING   CompleteName;
   pCls->get_CompleteName(CompleteName);
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(1056), CompleteName.get_name()); // "Elaborazione classe <%s>"

   if (!pfas) return GS_BAD;

   if (pCls->ptr_fas()->is_graph_calculated() == GS_BAD)
      isGraphCalculated = FALSE;
   else
   {
      isGraphCalculated = TRUE;
      // Preparo i comandi di lettura dei dati della classe dal temp/old
      if (pCls->prepare_data(CmdList) == GS_BAD) return GS_BAD;
   }
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   prj = GS_CURRENT_WRK_SESSION->get_PrjId();

   // Cerco tutti i testi della classe passata
   SelSet.copyIntersectClsCode(FiltrSet, pCls->ptr_id()->code, pCls->ptr_id()->sub_code);
   FiltrSet.intersectType(GRAPHICAL);

   // Ricavo il gruppo di selezione dal link set se questo non è vuoto
   if ((lungh1 = FiltrSet.length()) == 0) return GS_GOOD;

   // verifico se il gruppo di selezione è vuoto o ritorna errore
   if (lungh1 == -1)
   {
      resbuf p;
      acedGetVar(_T("ERRNO"), &p);
      if (p.resval.rint != 0) return GS_BAD;
   }

   StatusLineMsg.Init(gsc_msg(70), LITTLE_STEP); // ogni 10 "%ld oggetti grafici elaborati."

	while (FiltrSet.entname(i++, ent) == GS_GOOD)
	{
      if ((Attrib << acdbEntGet(ent)) == NULL) return GS_BAD;

      // ho impostato il flag che dice di considerare lo stato dei Layer dove
      // risisedono gli attributi visibili
      if (val_layer == 1)  
      {
         if ((pRBLayer = Attrib.SearchType(8)) != NULL)
             gsc_strcpy(piano, pRBLayer->resval.rstring, MAX_LEN_LAYERNAME);

         // se il piano è congelato, oppure OFF, oppure bloccato non eseguo operazioni
         if ((gsc_is_freezeLayer(piano) == GS_GOOD) ||
             (gsc_is_offLayer(piano) == GS_GOOD)    ||
             (gsc_is_lockedLayer(piano) == GS_GOOD)) continue;
      }      
      gsc_set_hText(ent, h_text);

      if (val_save == 1)
      {
         if (UpdSet.add(ent) == GS_BAD) return GS_BAD;
      }
		else
		{
			// se esistono funzioni di calcolo grafico
			// le applico perchè potrebbero variare gli attributi visibili
			if (isGraphCalculated)
				if (pCls->query_data(ent, ColValues, &CmdList) == GS_GOOD)
				{
				 // Setto l'operazione corrente
				 OldOp = set_GS_CURRENT_OPERATION(MODIFY);
				 pCls->graph_calc(ColValues);
				 // Ripristino la situazione precedente
				 set_GS_CURRENT_OPERATION(OldOp);
				}

		}      

      StatusLineMsg.Set(++cnt_upd); // "%ld oggetti grafici elaborati."
   }
   StatusLineMsg.End(gsc_msg(70), cnt_upd); // "%ld oggetti grafici elaborati."

   // evidenzio il numero di entità non modificabili
   if (cnt_upd != (i - 1))
      acutPrintf(gsc_msg(434), (i-1-cnt_upd), i-1); // "\nEscluse dall'operazione %ld entità su %ld in quanto non modificabili."

   // se il gruppo di selezione degli oggetti da salvare non è vuoto e 
	// il numero di oggetti aggiornabili è uguale al numero di oggetti del gruppo
	// di selezione di partenza lancio la funzione di riallineamento degli oggetti
   if (((lungh2 = UpdSet.length()) > 0) && (val_save == 1))
   {  
      if (lungh1 == lungh2)
      {
         int previous = GEOsimAppl::GLOBALVARS.get_AddEntityToSaveSet();

         GEOsimAppl::GLOBALVARS.set_AddEntityToSaveSet(GS_GOOD);
         GEOsimAppl::REFUSED_SS.clear();
         if (gsc_class_align(UpdSet, GS_BAD, GS_BAD, &(GEOsimAppl::REFUSED_SS), 
                             GS_GOOD, FAS_MOD) == -1)
               { GEOsimAppl::GLOBALVARS.set_AddEntityToSaveSet(previous); return GS_BAD; }
         GEOsimAppl::GLOBALVARS.set_AddEntityToSaveSet(previous);
      }
   }

	return GS_GOOD;
}


/////////////////////////////////////////////////////////////
// INIZIO - REATTORE DI EDITOR PER CONTROLLARE QUIT
/////////////////////////////////////////////////////////////


C_GSEditorReactor::C_GSEditorReactor()
   { acedEditor->addReactor(this); }

C_GSEditorReactor::~C_GSEditorReactor()
   { acedEditor->removeReactor(this); }

void C_GSEditorReactor::beginSave(AcDbDatabase *dwg, const ACHAR* pIntendedName)
{
   if (GS_CURRENT_WRK_SESSION) // se siamo in una sessione di lavoro di GEOsim
      // Il comando non è stato lanciato da GEOsim
      if (GEOsimAppl::CMDLIST.CmdRunningFromInternalCall == false)
      {
         // abortisco il salvataggio di AutoCAD per sostituirlo con quello di GEOsim
         veto();
         acutPrintf(gsc_msg(412)); // "\nComando interrotto, usare un comando di GEOsim."

         if (GEOsimAppl::LAST_STARTED_CMD.comp(_T("SAVE"), FALSE) == 0 ||
             GEOsimAppl::LAST_STARTED_CMD.comp(_T("QSAVE"), FALSE) == 0 ||
             GEOsimAppl::LAST_STARTED_CMD.comp(_T("SAVEAS"), FALSE) == 0)
         {
            GEOsimAppl::CMDLIST.StartCmd();
            if (GS_CURRENT_WRK_SESSION->save() == GS_BAD) GEOsimAppl::CMDLIST.ErrorCmd();
            else GEOsimAppl::CMDLIST.EndCmd();
         }

         // se sono in fase di quit 
         if (GEOsimAppl::LAST_STARTED_CMD.comp(_T("QUIT"), FALSE) == 0)
            // la funzione quit veto() ha bloccato il quit che viene forzato dalla seguente
            acDocManager->sendStringToExecute(acDocManager->curDocument(), _T("(command \"_.QUIT\" \"_Y\")\n"));
      }
}

void C_GSEditorReactor::commandWillStart(const ACHAR* cmdStr)
{
   // Memorizzo il nome del comando che sta per iniziare
   GEOsimAppl::LAST_STARTED_CMD = cmdStr;
}
