/**********************************************************
Name: GS_UTILY.CPP

Module description: File funzioni di utilita' generica
                    (operazioni su file,su stringhe .. ecc..) 
            
Author: Poltini Roberto

(c) Copyright 1995-2015 by IREN ACQUA GAS S.p.A.

**********************************************************/


/**********************************************************/
/*   INCLUDE                                              */
/**********************************************************/


#include "StdAfx.h"         // MFC core and standard components
#include "afxdialogex.h"

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")

#include "resource.h"

#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <string.h>      /*  per strcmp() */
#include <direct.h>      /*  per _rmdir() _chdir() etc... */
#include <fcntl.h>       /*  per _open()  */
#include <sys\stat.h>    /*  per _open()  */
#include <share.h>       /*  per apertura files in condivisione _sopen()*/
#include <errno.h>
#include <ctype.h>
#include <math.h>
#include <winGDI.h>
//#include <shfolder.h>      // per SHGetFolderPath
#include "ShlObj.h"      // per SHGetFolderPath

#include <adslib.h>      //  per malloc() realloc() -> ads_malloc()
#include "acedads.h"     //  per adsw_acadMainWnd
#include <aced.h>
#include <acprofile.h>
#include <acarray.h>
#include "AcExtensionModule.h" // per CAcModuleResourceOverride
#include "core_rxmfcapi.h" // per acedSetStatusBarProgressMeter
#include "acedCmdNF.h" // per acedCommandS e acedCmdS
#include <htmlhelp.h>
#include <wininet.h>


#include "..\gs_def.h" // definizioni globali
#include "gs_error.h"
#include "gs_list.h"
#include "gs_utily.h"
#include "gs_netw.h"
#include "gs_init.h"
#include "gs_resbf.h"
#include "adsdlg.h"
#include <adeads.h>
#include "gs_ade.h"
#include "gs_ase.h"       // gestione links
#include "gs_dbref.h"     // prototipi funzioni gestione riferimenti a db
#include "gs_class.h"
#include "gs_graph.h"
#include "gs_prjct.h"
#include "gs_filtr.h"     // per gruppo di selezione GS_LSFILTER
#include "gs_user.h"
#include "gs_attbl.h"
#include "gs_opcod.h"             

#include "GSresource.h" 

//extern "C" HWND adsw_acadMainWnd();


#if defined(GSPROFILEFILE) // se versione per debugging
   #include <sys/timeb.h>
   #include <time.h>
#endif


// per google
typedef struct Coordinate
{
  double latitude;
  double longitude;
} Coordinate;


/*********************************************************/
/* FUNCTIONS */
/*********************************************************/


#include "afxwin.h"
void rel_resbuf(presbuf *, presbuf *, presbuf *, presbuf *);
TCHAR *filetype (unsigned);
static void CALLB horiz_slider(ads_callback_packet *dcl);
static void CALLB confirmSecurity_accept_ok(ads_callback_packet *dcl);
static void CALLB confirm_accept_ok(ads_callback_packet *dcl);
static void CALLB confirm_cancel(ads_callback_packet *dcl);
static void CALLB confirm_no_button(ads_callback_packet *dcl);

int gsc_ParseDateTime(const TCHAR *DateTime, COleDateTime &OleDateTime);
int gsc_ParseDate(const TCHAR *Date, COleDateTime &OleDateTime);
int gsc_ParseTime(const TCHAR *Time, COleDateTime &OleDateTime);

int gsc_setWaitFor(const TCHAR *sez, int prj, C_INT_LIST &ClsList);

int gsc_getLocaleShortDateFormat4DgtYear(C_STRING &str);
int gsc_getLocaleMonetarySymbol(C_STRING &str);
int gsc_getLocalePositiveMonetaryMode(C_STRING &str);
int gsc_getLocaleNegativeMonetaryMode(C_STRING &str);

int gsc_CreateEmptyUnicodeFile(C_STRING &Path);
int gsc_CreateEmptyUnicodeFile(const TCHAR *pathfile);


/*************************************************************************/
/*.doc  int gsc_path_conv(path)  */
/*+   
   Traduce un path relativo in un path completo (controllando
   la correttezza ma non l'esistenza) secondo la notazione
   vista dal sistema operativo. La chiamata a questa funzione e'
   necessaria prima di ogni chiamata a funzioni di sistema 'C' per
   operazioni su file e directory.
   Parametri:
   C_STRING &path;   in ingresso puo'definire un path relativo o completo,
                     in uscita puntera' al path completo convertito.

   Ritorna GS_GOOD se il path definito da path e' valido GS_BAD altrimenti.
-*/
/*************************************************************************/
int gsc_path_conv(C_STRING &path)
{
   // Rileva e traduce eventuali connessioni in rete
   return gsc_nethost2drive(path);
}
int gsc_path_conv(char path[], int maxlen)
{
   if (!path) { GS_ERR_COD = eGSInvalidPath; return GS_BAD; }
      
   // Rileva e traduce eventuali connessioni in rete
   return gsc_nethost2drive(path, maxlen);
}   
int gsc_path_conv(TCHAR path[], int maxlen)
{
   if (!path) { GS_ERR_COD = eGSInvalidPath; return GS_BAD; }
      
   // Rileva e traduce eventuali connessioni in rete
   return gsc_nethost2drive(path, maxlen);
}   
//-----------------------------------------------------------------------//
int gsc_validdir(C_STRING &dir)
{
   size_t   i, len;
   C_STRING TempBuffer;
   TCHAR    insieme_vietato[] = _T("?\"<>|"), *pStart, *pSlash;

   // traduco eventuale <alias>
   if (gsc_nethost2drive(dir) == GS_BAD) return GS_BAD;

   len = dir.len();   
  
   for (i = 0; i < len; i++)
      if (wcschr(insieme_vietato, dir.get_chr(i)) != NULL)
         { GS_ERR_COD = eGSInvalidPath; return GS_BAD; }

   pStart = dir.get_name();
   // verifico che non esista uno spazio dopo lo slash
   while ((pSlash = wcschr(pStart, _T('\\'))) || (pSlash = wcschr(pStart, _T('/'))))
   {
      if (*(++pSlash) == _T(' ')) { GS_ERR_COD = eGSInvalidPath; return GS_BAD; }
      pStart = pSlash;
   }

   TempBuffer = dir;
   // verifico correttezza cercando se esiste il corrispondente alias
   // nel caso in cui dir non era espresso tramite <alias>
   return gsc_drive2nethost(TempBuffer);
}


/*************************************************************************/
/*.doc  int gs_dir_exist(path) <external> */ 
/*+   
   Ritorna TRUE se la directory esiste altrimenti NIL.
-*/
/*************************************************************************/
int gs_dir_exist(void)
{
   presbuf  arg = acedGetArgs();
   C_STRING dir;
   
   acedRetNil();

   if (arg == NULL) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }  // nessun param passato
   if (arg->restype != RTSTR) { GS_ERR_COD = eGSInvRBType; return RTERROR; }
   dir = arg->resval.rstring;
   if (gsc_dir_exist(dir, ONETEST) == GS_GOOD) acedRetT();

   return RTNORM;
}


/*************************************************************************/
/*.doc  int gsc_dir_exist(path) */ 
/*+   
  Traduce un path realativo in un path completo secondo la notazione
  vista dal sistema operativo, controllando oltre che la correttezza
  la sua ESISTENZA (ESCLUSO L'ULTIMO ELEMENTO).
  Questa funzione richiamata gsc_path_conv(). 
  Parametri:
  TCHAR *path;    in ingresso puo definire un path relativo o completo,
                  in uscita puntera' al path completo convertito.
  int retest;     se MORETESTS -> in caso di errore riprova n volte 
                  con i tempi di attesa impostati poi ritorna GS_BAD,
                  ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                  (default = MORETESTS)
   
   Ritorna GS_GOOD se la directory prima del path esiste, GS_BAD altrimenti.
-*/
/*************************************************************************/
int gsc_dir_exist(C_STRING &path, int retest)
   { return gsc_dir_exist(path.get_name(), retest); }
int gsc_dir_exist(TCHAR *path, int retest)
{
   C_STRING full_path, buffer, drive, dir, fname;
   size_t   len;
    
   full_path = path;
   // Controlla Correttezza Path
   if (gsc_nethost2drive(full_path)==GS_BAD) return GS_BAD; 
                       
   // Controlla Esistenza Directory
   gsc_splitpath(full_path.get_name(), &drive, &dir, &fname);
   if ((len = dir.len()) > 1) dir.set_chr(_T('\0'), len - 1);
   gsc_makepath(buffer, drive.get_name(), dir.get_name(), GS_EMPTYSTR, GS_EMPTYSTR);

   if (retest == MORETESTS)
   {
      int tentativi;

      do
      {
         tentativi = 1;

         do
         {  // se non è il primo tentativo
            if (tentativi > 1) gsc_wait(GEOsimAppl::GLOBALVARS.get_WaitTime()); // attesa tra un tentativo e l'altro
            
            if (_waccess(buffer.get_name(), 0) != -1) 
               { wcscpy(path, full_path.get_name()); return GS_GOOD; }
            
            tentativi++;
         }
         while (tentativi <= GEOsimAppl::GLOBALVARS.get_NumTest());
      
         // setto l'errore GEOsim
         GS_ERR_COD = eGSPathNoExist;
         
         if (gsc_dderroradmin(_T("GEOSIM"), buffer.get_name()) != GS_GOOD)
            return GS_BAD; // l'utente vuole abbandonare
      }
      while (1);
   }
   else
      if (_waccess(buffer.get_name(), 0) == -1)
         { GS_ERR_COD = eGSPathNoExist; return GS_BAD; }

   wcscpy(path, full_path.get_name());
  
   return GS_GOOD;
}   


/*************************************************************************/
/*.doc  int gs_path_exist(path) <external> */ 
/*+   
   Ritorna TRUE se la directory i il file esiste altrimenti NIL.
-*/
/*************************************************************************/
int gs_path_exist(void)
{
   presbuf  arg = acedGetArgs();
   C_STRING dir;
   
   acedRetNil();

   if (arg == NULL) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }  // nessun param passato
   if (arg->restype != RTSTR) { GS_ERR_COD = eGSInvRBType; return RTERROR; }
   dir = arg->resval.rstring;
   if (gsc_path_exist(dir) == GS_GOOD) acedRetT();

   return RTNORM;
}


/*************************************************************************/
/*.doc  int gsc_path_exist(path) */ 
/*+   
   Traduce un path realativo in un path completo secondo la notazione
   vista dal sistema operativo, controllando oltre che la correttezza
   la sua ESISTENZA (COMPRESO L'ULTIMO ELEMENTO).
   char *TCHAR;  -> in ingresso puo definire un path relativo o completo,
                  in uscita puntera' al path completo convertito.
   int PrintError;   Se il flag = GS_GOOD in caso di errore viene
                     stampato il messaggio relativo altrimenti non
                     viene stampato alcun messaggio (default = GS_GOOD)
   Ritorna int -> GS_GOOD se il path completo esiste
                  GS_BAD altrimenti.
-*/
/*************************************************************************/
int gsc_path_exist(C_STRING &path, int PrintError)
{
   return gsc_path_exist(path.get_name(), PrintError);
}
int gsc_path_exist(const TCHAR *path, int PrintError)
{
   C_STRING tmp_path;
   
   if (!path || wcslen(path) == 0) return GS_BAD;
   tmp_path = path;
   // Controlla Correttezza Path
   if (gsc_nethost2drive(tmp_path, PrintError) == GS_BAD) return GS_BAD; 
   // Controlla Esistenza Path
   if (_waccess(tmp_path.get_name(), 0) == -1) return GS_BAD;

   return GS_GOOD;
}


/*************************************************************************/
/*.doc  int gsc_drive_exist(path)                                        */ 
/*+   
   Verifica che il drive relativo ad un path esiste. La funzione viene usata per
   determinare se una macchina è accesa o spenta.
   Parametri:
   const TCHAR *path;   
   
   Ritorna GS_GOOD se il drive della path esiste, GS_BAD altrimenti.
-*/
/*************************************************************************/
int gsc_drive_exist(const TCHAR *path)
{
   C_STRING tmp_path, drive;
   
   if (!path) return GS_BAD;

   tmp_path = path;

   // Controlla Correttezza Path
   if (gsc_nethost2drive(tmp_path) == GS_BAD) return GS_BAD; 
   gsc_splitpath(tmp_path.get_name(), &drive);
   // Controlla Esistenza Path
   if (_waccess(drive.get_name(), 0) == -1) return GS_BAD;

   return GS_GOOD;
}  


/*************************************************************************/
/*.doc  int gsc_mkdir(dirname)  */
/*+   
  Crea la directory 'dir'.
  Parametri:
  C_STRING &dirname;
  oppure
  const TCHAR *dirname;  path del direttorio da creare  (es. c:\dir1)
  
  Ritorna GS_GOOD se la directory e' creata correttamente, GS_BAD altrimenti.
-*/
/*************************************************************************/
int gsc_mkdir(C_STRING &dirname)
   { return gsc_mkdir(dirname.get_name()); }
int gsc_mkdir(const TCHAR *dirname)
{
   C_STRING TmpDir;
   TCHAR    *pSlash, *pStart;
   
   if (!dirname) { GS_ERR_COD = eGSInvalidPath; return GS_BAD; }

   // direttorio già esistente
   if (gsc_path_exist(dirname) == GS_GOOD) return GS_GOOD;
      
   TmpDir = dirname;
   if (gsc_nethost2drive(TmpDir) == GS_BAD) return GS_BAD; 

   if ((pSlash = TmpDir.at(_T('\\'))) || (pSlash = TmpDir.at(_T('/'))))
   {
      while ((pStart = pSlash + 1) &&
             ((pSlash = wcschr(pStart, _T('\\'))) || (pSlash = wcschr(pStart, _T('/')))))
      {
         *pSlash = _T('\0');
         if (_waccess(TmpDir.get_name(), 0) == -1)
            // create a new directory
            if (_wmkdir(TmpDir.get_name()) != 0) { GS_ERR_COD = eGSInvalidPath; return GS_BAD; }
         *pSlash = _T('\\');
      }
   }
   
   // create a new directory
   if (_wmkdir(TmpDir.get_name()) != 0) { GS_ERR_COD = eGSInvalidPath; return GS_BAD; }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_mkdir_from_path <internal> */
/*+
  Questa funzione, dato un path completo, crea il direttorio se non era 
  già stato creato.
  Parametri:
  const TCHAR *path; Path completo
  int retest;        se MORETESTS -> in caso di errore riprova n volte 
                     con i tempi di attesa impostati poi ritorna GS_BAD,
                     ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                     (default = MORETESTS)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_mkdir_from_path(C_STRING &path, int retest)
   { return gsc_mkdir_from_path(path.get_name(), retest); }
int gsc_mkdir_from_path(const TCHAR *path, int retest)
{  
   C_STRING TmpDir, TempPath(path);

   if (gsc_dir_exist(TempPath, ONETEST) == GS_GOOD) return GS_GOOD; // esisteva

   // estraggo il direttorio dal path completo
   if (gsc_dir_from_path(TempPath, TmpDir) == GS_BAD) return GS_BAD;

   // creo il direttorio
   if (gsc_mkdir(TmpDir.get_name()) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*************************************************************************/
/*.doc  int gsc_rmdir(dirname)  
/*+   
  Cancella la directory 'dir'
  Parametri:
  const TCHAR *dirname;    Definisce il path relativo o completo della dir.
  int retest;              se MORETESTS -> in caso di errore riprova n volte 
                           con i tempi di attesa impostati poi ritorna GS_BAD,
                           ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                           (default = MORETESTS)

  Ritorna GS_GOOD se la directory e' cancellata correttamente, GS_BAD altrimenti.
-*/
/*************************************************************************/
int gsc_rmdir(C_STRING &dirname, int retest)
   { return gsc_rmdir(dirname.get_name(), retest); }
int gsc_rmdir(const TCHAR *dirname, int retest)
{  
   C_STRING tmpdir;

   tmpdir = dirname;

   if (gsc_path_exist(tmpdir) == GS_BAD) return GS_GOOD; // non c'era

   // Conversione directory
   if (gsc_nethost2drive(tmpdir) == GS_BAD) return GS_BAD; 
  
   if (retest == MORETESTS)
   {
      int tentativi;

      do
      {
         tentativi = 1;

         do
         {  // se non è il primo tentativo
            if (tentativi > 1) gsc_wait(GEOsimAppl::GLOBALVARS.get_WaitTime()); // attesa tra un tentativo e l'altro
            
            // remove existing directory
            if (_wrmdir(tmpdir.get_name()) == 0) return GS_GOOD; // tutto OK
            tentativi++;
         }
         while (tentativi <= GEOsimAppl::GLOBALVARS.get_NumTest());
      
         // setto l'errore GEOsim
         GS_ERR_COD = (errno == EACCES) ? eGSFileLocked : eGSNotDel;
         
         if (gsc_dderroradmin(_T("GEOSIM"), tmpdir.get_name()) != GS_GOOD) return GS_BAD; // l'utente vuole abbandonare
      }
      while (1);
   }
   else
      // remove existing directory
      if (_wrmdir(tmpdir.get_name()) != 0)
      {
         TCHAR Msg[MAX_LEN_MSG + 50];

         GS_ERR_COD = (errno == EACCES) ? eGSFileLocked : eGSNotDel;
         swprintf(Msg, MAX_LEN_MSG + 50, _T("Dir <%s> not removed."), tmpdir.get_name());
         gsc_write_log(Msg);

			return GS_BAD;
      }

   return GS_GOOD; 
}


/*********************************************************/
/*.doc gsc_rmdir_from_path <internal> */
/*+
  Questa funzione, dato un path completo, rimuove il direttorio se era 
  vuoto.
  Parametri:
  const TCHAR *path;    Path completo
  int retest;           se MORETESTS -> in caso di errore riprova n volte 
                        con i tempi di attesa impostati poi ritorna GS_BAD,
                        ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                        (default = MORETESTS)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_rmdir_from_path(C_STRING &path, int retest)
   { return gsc_rmdir_from_path(path.get_name()); }
int gsc_rmdir_from_path(const TCHAR *path, int retest)
{  
   C_STRING dir;

   // estraggo il direttorio dal path completo
   if (gsc_dir_from_path(path, dir) == GS_BAD) return GS_BAD;

   // cancello il direttorio
   if (gsc_path_exist(dir) == GS_GOOD) // se esisteva
      if (gsc_rmdir(dir.get_name(), retest) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_dir_from_path <internal> */
/*+
  Questa funzione, dato un path completo, ricava il direttorio.
  Parametri:
  TCHAR *path;    path completo
  TCHAR dir[];    direttorio
  int   len_dir;  lunghezza vettore direttorio

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gs_dir_from_path(void)
{
   presbuf  arg = acedGetArgs();
   C_STRING dir;

   acedRetNil(); 
   if (!arg || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (gsc_dir_from_path(arg->resval.rstring, dir) == GS_BAD) return RTERROR;
   acedRetStr(dir.get_name());

   return RTNORM;
}
int gsc_dir_from_path(C_STRING &path, C_STRING &dir)
   { return gsc_dir_from_path(path.get_name(), dir); }
int gsc_dir_from_path(const TCHAR *path, C_STRING &dir)
{  
   C_STRING _dir;
   TCHAR    last_ch;

   gsc_splitpath(path, &dir, &_dir);
   dir += _dir;
   // Ultimo carattere del direttorio
   last_ch = dir.get_chr(dir.len() - 1);
   // Elimino ultimo slash 
   if (last_ch == _T('\\') || last_ch == _T('/'))
      dir.set_chr(_T('\0'), dir.len() - 1);

   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_create_dir                          <external> */
/*+
  Questa funzione LISP crea un nuovo direttorio
  Parametri passati <Nome del direttorio>
  Ritorna TRUE se la directory esiste altrimenti NIL.
-*/  
/*********************************************************/
int gs_create_dir(void)
{
   presbuf arg = acedGetArgs();

   acedRetNil(); 
   if (!arg || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (gsc_mkdir(arg->resval.rstring) == GS_BAD) return RTERROR;
   acedRetT();

   return RTNORM;
}

/*********************************************************/
/*.doc gs_get_tmp_filename <external> */
/*+
  Questa funzione LISP crea un path completo di un file temporaneo
  leggendo un direttorio (se il direttorio non esiste lo crea).
  Parametri:
  <dir_rif><prefix><ext>
        
  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR.
-*/  
/*********************************************************/
int gs_get_tmp_filename(void)
{
   presbuf arg = acedGetArgs();
   TCHAR   *dir_rif, *prefix, *ext, *filetemp;

   acedRetNil();

   // dir_rif
   if (!arg || arg->restype != RTSTR || (dir_rif = arg->resval.rstring) == NULL)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   // prefix
   if (!(arg = arg->rbnext) || arg->restype != RTSTR || (prefix = arg->resval.rstring) == NULL)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   // ext
   if (!(arg = arg->rbnext) || arg->restype != RTSTR || (ext = arg->resval.rstring) == NULL)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (gsc_get_tmp_filename(dir_rif, prefix, ext, &filetemp) == GS_BAD) return RTERROR;
   acedRetStr(filetemp);
   free(filetemp);

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_get_tmp_filename <external> */
/*+
  Questa funzione crea un path completo di un file temporaneo
  leggendo un direttorio (se il direttorio non esiste lo crea).
  Parametri:
  const TCHAR *dir_rif; direttorio (es. "c:\subdir")
  const TCHAR *prefix;  prefisso   (es. "file")
  const TCHAR *ext;     estensione (es.  ".txt")
  TCHAR **filetemp;     path completo file temporaneo (es. "c:\subdir\file1.txt")
        
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_get_tmp_filename(const TCHAR *dir_rif, const TCHAR *prefix, 
                         const TCHAR *ext, TCHAR **filetemp)
{
   C_STRING tmpnam, path;
   long     indice = 0;
   TCHAR    ch;

   if (!filetemp || !dir_rif || !prefix || !ext)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   *filetemp = NULL;

   // creo il direttorio
   if (gsc_mkdir(dir_rif) == GS_BAD) return GS_BAD;
   // Ultimo carattere del direttorio
   ch = dir_rif[wcslen(dir_rif) - 1];

   do 
   {  // ciclo di tentativi
      tmpnam = prefix;
      // la prima volta provo con il nome originale poi aggiungo un indice incrementale
      if (indice > 0) 
      {
         tmpnam += _T('-');
         tmpnam += indice++;
      }
      else indice++;
      tmpnam += ext;

      path = dir_rif;
      if (ch != _T('\\') && ch != _T('/')) path += _T('\\');
      path += tmpnam;
   }
   while (gsc_path_exist(path) == GS_GOOD);

   // Controlla Correttezza Path
   if (gsc_nethost2drive(path) == GS_BAD) return RTERROR;

   if ((*filetemp = gsc_tostring(path.get_name())) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   return GS_GOOD;
}
int gsc_get_tmp_filename(const TCHAR *dir_rif, const TCHAR *prefix, const TCHAR *ext, 
                         C_STRING &filetemp)
{
   TCHAR *dummy;

   if (gsc_get_tmp_filename(dir_rif, prefix, ext, &dummy) == GS_BAD) return GS_BAD;
   filetemp.paste(dummy);
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_SetFileTime                        <external> */
/*+
  Questa funzione setta la data di creazione, di ultimo accesso e di
  ultima scrittura di un file.
  Parametri:
  HANDLE hFile;            handle to the file
  CONST FILETIME *lpTime;  se = NULL viene usata la data/ora corrente (default = NULL)

  oppure

  C_STRING &Path;          path del file
  CONST FILETIME *lpTime;  se = NULL viene usata la data/ora corrente (default = NULL)
        
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_SetFileTime(HANDLE hFile, CONST FILETIME *lpTime)
{
   BOOL Result;

   if (!lpTime)
   {
      FILETIME Now;
   
      GetSystemTimeAsFileTime(&Now);

      Result = SetFileTime(hFile, &Now, &Now, &Now);
   }
   else
      Result = SetFileTime(hFile, lpTime, lpTime, lpTime);

   return (Result) ? GS_GOOD : GS_BAD;
}
int gsc_SetFileTime(C_STRING &Path, CONST FILETIME *lpTime)
{
   errno_t  err;
   FILE     *pFile;
   C_STRING tmp_path;
   int      Result;

   tmp_path = Path;

   // Controlla Correttezza Path
   if (gsc_nethost2drive(tmp_path) == GS_BAD) return GS_BAD;

   if ((err = _wfopen_s(&pFile, tmp_path.get_name(), _T("r+"))) == HFILE_ERROR)
      { GS_ERR_COD = eGSOpenFile; return GS_BAD; }
   Result = gsc_SetFileTime(pFile, lpTime);
   if (gsc_fclose(pFile) == GS_BAD) return GS_BAD;

   return Result;
}
 

/*************************************************************************/
/*.doc  int gsc_geosimdir(path)  */
/*+   
   Memorizza in 'path' il path completo della directory contenente
   GEOSIM memorizzata nella variabile di ambiente 'GEOSIM'.   
   C_STRING &dir; puntatore a stringa gia' allocata esternamente.

   Ritorna GS_GOOD la directory e' valida, GS_BAD altrimenti.
-*/
/*************************************************************************/
int gsc_geosimdir(C_STRING &dir)
{
   TCHAR *env;

   if ((env = _wgetenv(ENV_GEO_DIR)) == NULL) // non alloca
      { GS_ERR_COD = eGSVarNotDef; return GS_BAD; }
   if (gsc_path_exist(env) == GS_BAD)
      { GS_ERR_COD = eGSPathNoExist; return GS_BAD; }

   dir = env;
   // se l'ultima lettera è \ oppure / la elimino
   if (dir.get_chr(dir.len() - 1) == _T('\\') || 
       dir.get_chr(dir.len() - 1) == _T('/'))
      dir.set_chr(_T('\0'), dir.len() - 1);

   return GS_GOOD;
}

/*************************************************************************/
/*.doc  int gsc_geoworkdir(path)  */
/*+   
   Memorizza in 'path' il path completo della directory di lavoro
   letto dalla variabile di ambiente GEOWORK.
   Parametri:
   C_STRING &dir;    puntatore a stringa gia' allocata esternamente.

   Ritorna GS_GOOD la directory e' valida, GS_BAD altrimenti.
-*/
/*************************************************************************/
int gsc_geoworkdir(C_STRING &dir)
{
   TCHAR *env;

   if ((env = _wgetenv(ENV_WORK_DIR)) == NULL) // non alloca
      { GS_ERR_COD = eGSVarNotDef; return GS_BAD; }
   if (gsc_path_exist(env) == GS_BAD)
      { GS_ERR_COD = eGSPathNoExist; return GS_BAD; }

   dir = env;

   return GS_GOOD;
}

  
/*************************************************************************/
/*.doc  int gsc_copyfile
/*+   
  Copia il contenuto di un file in un altro che eventualmente crea, 
  i paramentri possono indicare anche i path relativi o completi.
  Parametri:
  const TCHAR *sourcefile; nome del file sorgente.
  const TCHAR *targetfile; nome del file destinazione.
  int retest;              se MORETESTS -> in caso di errore riprova n volte 
                           con i tempi di attesa impostati poi ritorna GS_BAD,
                           ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                           (default = MORETESTS)
  
  Ritorna GS_GOOD se il file e' stato copiato correttamente, GS_BAD altrimenti.
-*/
/*************************************************************************/
int gsc_copyfile(C_STRING &Source, C_STRING &Target, int retest)
{
   return gsc_copyfile(Source.get_name(), Target.get_name(), retest);
}
int gsc_copyfile(const TCHAR *Source, const TCHAR *Target, int retest)
{
   C_STRING tmpSource, tmpTarget, Buff;
   int      Result;

   tmpSource = Source;
   tmpTarget = Target;

   // Controlla Correttezza Path
   if (gsc_nethost2drive(tmpSource) == GS_BAD || gsc_nethost2drive(tmpTarget) == GS_BAD)
      return GS_BAD;

   if (gsc_path_exist(tmpSource) == GS_BAD) { GS_ERR_COD = eGSFileNoExisting; return GS_BAD; }
   if (gsc_dir_exist(tmpTarget) == GS_BAD) { GS_ERR_COD = eGSInvalidPath; return GS_BAD; }
   
   if (retest == MORETESTS)
   {
      int tentativi;

      do
      {
         tentativi = 1;
         do
         {  // se non è il primo tentativo
            if (tentativi > 1) gsc_wait(GEOsimAppl::GLOBALVARS.get_WaitTime()); // attesa tra un tentativo e l'altro
            
            Result = CopyFile(tmpSource.get_name(), tmpTarget.get_name(), FALSE);

            if (Result != 0) return Result;
            tentativi++;
         }
         while (tentativi <= GEOsimAppl::GLOBALVARS.get_NumTest());
      
         // setto l'errore GEOsim
         if (errno == EACCES) GS_ERR_COD = eGSFileLocked;
         else GS_ERR_COD = eGSNotCopied;
         
         Buff = tmpSource;
         Buff += _T(" -> ");
         Buff += tmpTarget;
         if (gsc_dderroradmin(_T("GEOSIM"), Buff.get_name()) != GS_GOOD)
            return -1; // l'utente vuole abbandonare
      }
      while (1);
   }
   else
   {
      Result = CopyFile(tmpSource.get_name(), tmpTarget.get_name(), FALSE);

      if (Result == -1)
      {
         if (errno == EACCES) GS_ERR_COD = eGSFileLocked;
         else GS_ERR_COD = eGSNotCopied;
      }
   }

   return Result;
}

  
/*************************************************************************/
/*.doc  int gsc_appendfile
/*+   
  Aggiunge il contenuto di un file in un altro, 
  i paramentri possono indicare anche i path relativi o completi.
  Parametri:
  C_STRING &Source;  nome del file sorgente (esistente).
  C_STRING &Target;  nome del file destinazione.
  
  Ritorna GS_GOOD se il file e' stato copiato correttamente, GS_BAD altrimenti.
-*/
/*************************************************************************/
int gsc_appendfile(C_STRING &Source, C_STRING &Target)
{
   FILE     *fSrc, *fDst;
   C_STRING tmpSource, tmpTarget;
   TCHAR    Buff[100];
   size_t   numRead, numWritten;

   tmpSource = Source;
   tmpTarget = Target;

   // Controlla Correttezza Path
   if (gsc_nethost2drive(tmpSource) == GS_BAD || gsc_nethost2drive(tmpTarget) == GS_BAD)
      return GS_BAD;

   if (gsc_path_exist(tmpSource) == GS_BAD) { GS_ERR_COD = eGSFileNoExisting; return GS_BAD; }
   if (gsc_path_exist(tmpTarget) == GS_BAD)
      return gsc_copyfile(tmpSource, tmpTarget);
   
   if ((fSrc = gsc_fopen(tmpSource, _T("rb"))) == NULL) return GS_BAD;
   if ((fDst = gsc_fopen(tmpTarget, _T("ab"))) == NULL)
      { gsc_fclose(fDst); return GS_BAD; }

   do
   {
      numRead    = fread(Buff, sizeof(TCHAR), 100, fSrc);
      numWritten = fwrite(Buff, sizeof(TCHAR), numRead, fDst);
      if (numRead != numWritten)
         { gsc_fclose(fSrc); gsc_fclose(fDst); return GS_BAD; }
   }
   while (!feof(fSrc) && !feof(fDst));

   if (gsc_fclose(fSrc) == GS_BAD) 
      { gsc_fclose(fDst); return GS_BAD; }
   
   return gsc_fclose(fDst);
}

  
/*************************************************************************/
/*.doc gsc_identicalFiles
/*+   
  Verifica che il contenuto dei 2 files sia identico.
  I paramentri possono indicare anche i path relativi o completi.
  Parametri:
  const TCHAR *File1;       nome del primo file.
  const TCHAR *File2;       nome del secondo file.
  
  Ritorna GS_GOOD se i files sono identici, GS_BAD altrimenti.
-*/
/*************************************************************************/
int gsc_identicalFiles(C_STRING &File1, C_STRING &File2)
{
   return gsc_identicalFiles(File1.get_name(), File2.get_name());
}
int gsc_identicalFiles(const TCHAR *File1, const TCHAR *File2)
{
   C_STRING tmpFile1, tmpFile2;
   FILE     *file1, *file2;
   TCHAR    Buff1[100], Buff2[100];
   size_t   numRead1, numRead2, i;
   int      Identical = GS_GOOD;

   tmpFile1 = File1;
   tmpFile2 = File2;

   // Controlla Correttezza Path
   if (gsc_nethost2drive(tmpFile1) == GS_BAD || gsc_nethost2drive(tmpFile2) == GS_BAD)
      return GS_BAD;

   if (gsc_path_exist(tmpFile1) == GS_BAD || gsc_path_exist(tmpFile2) == GS_BAD)
      { GS_ERR_COD = eGSFileNoExisting; return GS_BAD; }

   if ((file1 = gsc_fopen(tmpFile1, _T("rb"))) == NULL) return GS_BAD;
   if ((file2 = gsc_fopen(tmpFile2, _T("rb"))) == NULL)
      { gsc_fclose(file1); return GS_BAD; }

   do
   {
      numRead1 = fread(Buff1, sizeof(TCHAR), 100, file1);
      numRead2 = fread(Buff2, sizeof(TCHAR), 100, file2);
      if (numRead1 != numRead2) { Identical = GS_BAD; break; }
      if (numRead1 < 100 && (ferror(file1) != 0 || ferror(file2) != 0))
         { Identical = GS_BAD; break; }
      
      for (i = 0; i < numRead1; i++) // ciclo di comparazione buffer
         if (Buff1[i] != Buff2[i]) break;
         
      if (i < numRead1) { Identical = GS_BAD; break; }
   }
   while (!feof(file1) && !feof(file2));

   if (gsc_fclose(file1) == GS_BAD)
      { gsc_fclose(file2); return GS_BAD; }
   if (gsc_fclose(file2) == GS_BAD) return GS_BAD;

   return Identical;
}


/*************************************************************************/
/*.doc gsc_isUnicodeFile()
/*+   
  Verifica che il file sia in formato UNICODE.
  Parametri:
  FILE *File;  Puntatore a file

  Ritorna true se il file è UNICODE altrimenti false.
-*/
/*************************************************************************/
bool gsc_isUnicodeFile(FILE *File)
{
   TCHAR Buffer[1];
   long  CurPos;
   bool  Res = false;

   CurPos = ftell(File);
   fseek(File, 0, SEEK_SET); // mi posiziono all'inizio del file

   if (fread(Buffer, sizeof(TCHAR), 1, File) == 1)
   {
      WORD wBOM = 0xFEFF; // UTF16-LE BOM(FFFE)
      if (Buffer[0] == wBOM) Res = true;
   }

   fseek(File, CurPos, SEEK_SET); // Torno nella posizione precedente

   return Res;
}


/*************************************************************************/
/*.doc gsc_sopen()
/*+   
  Apre un file nelle modalità previste da "_sopen" con conversione
  automatica della path del file da aprire.
  Parametri:
  const TCHAR *path;
  int oflag;
  int shflag;
  int pmode;
  int retest;     se MORETESTS -> in caso di errore riprova n volte 
                  con i tempi di attesa impostati poi ritorna GS_BAD,
                  ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                  (default = MORETESTS)
-*/
/*************************************************************************/
int gsc_sopen(const TCHAR *path, int oflag, int shflag, int pmode, int retest)
{
   C_STRING tmp_path;
   int      Result;

   if (!path) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   tmp_path = path;

   // Controlla Correttezza Path
   if (gsc_nethost2drive(tmp_path) == GS_BAD) return -1;

   if (retest == MORETESTS)
   {
      int tentativi = 1;

      do
      {
         do
         {  // se non è il primo tentativo
            if (tentativi > 1) gsc_wait(GEOsimAppl::GLOBALVARS.get_WaitTime()); // attesa tra un tentativo e l'altro
            
            if (pmode < 0) Result = _wsopen(tmp_path.get_name(), oflag, shflag);
            else Result = _wsopen(tmp_path.get_name(), oflag, shflag, pmode);       

            if (Result != -1) return Result;
            tentativi++;
         }
         while (tentativi <= GEOsimAppl::GLOBALVARS.get_NumTest());
      
         // setto l'errore GEOsim
         if (errno == EACCES) GS_ERR_COD = eGSFileLocked;
         else GS_ERR_COD = eGSOpenFile;
         
         if (gsc_dderroradmin(_T("GEOSIM"), tmp_path.get_name()) != GS_GOOD) return -1; // l'utente vuole abbandonare
      }
      while (1);
   }
   else
   {
      if (pmode < 0) Result = _wsopen(tmp_path.get_name(), oflag, shflag);
      else Result = _wsopen(tmp_path.get_name(), oflag, shflag, pmode);       

      if (Result == -1)
      {
         if (errno == EACCES) GS_ERR_COD = eGSFileLocked;
         else GS_ERR_COD = eGSOpenFile;
      }
   }

   return Result;
}


/*************************************************************************/
/*.doc gsc_sopen()
/*+   
  Apre un file nelle modalità previste da "_wfopen" con conversione
  automatica della path del file da aprire.
  Parametri:
  const TCHAR *path;
  const TCHAR *mode; "r" = Opens for reading. If the file does not exist or 
                           cannot be found, the fopen call fails.
                     "w" = Opens an empty file for writing. If the given file
                           exists, its contents are destroyed.
                     "a" = Opens for writing at the end of the file (appending)
                           without removing the EOF marker before writing new data
                           to the file; creates the file first if it doesn't exist.
                     "r+" = Opens for both reading and writing. (The file must exist.)
                     "w+" = Opens an empty file for both reading and writing. 
                            If the given file exists, its contents are destroyed.
                     "a+" = Opens for reading and appending; the appending operation
                            includes the removal of the EOF marker before new data is 
                            written to the file and the EOF marker is restored after
                            writing is complete; creates the file first if it doesn't exist.
  int retest;     se MORETESTS -> in caso di errore riprova n volte 
                  con i tempi di attesa impostati poi ritorna GS_BAD,
                  ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                  (default = MORETESTS)
  bool *Unicode;
-*/
/*************************************************************************/
FILE *gsc_fopen(C_STRING &path, const TCHAR *mode, int retest, bool *Unicode)
   { return gsc_fopen(path.get_name(), mode, retest, Unicode); }
FILE *gsc_fopen(const TCHAR *path, const TCHAR *mode, int retest, bool *Unicode)
{
   C_STRING    tmp_path, _mode(mode);
   FILE        *Result;
   bool        UnicodeOpenMode;
   const TCHAR *p;

   if (!path || !mode) { GS_ERR_COD = eGSInvalidArg; return NULL; }

   UnicodeOpenMode = (Unicode == NULL || (Unicode && *Unicode == false)) ? false : true;

   tmp_path = path;

   // Controlla Correttezza Path
   if (gsc_nethost2drive(tmp_path) == GS_BAD) return NULL;

   // se il file doveva essere creato in unicode
   if ((p = wcschr(mode, _T('w'))) && UnicodeOpenMode)
   {
      _wremove(tmp_path.get_name());
      if (gsc_CreateEmptyUnicodeFile(tmp_path) == GS_BAD) return NULL;

      if (*(p + 1) == _T('+')) _mode.strtran("w", "r");
      else _mode.strtran("w", "r+");
   }

   if (UnicodeOpenMode)
      _mode += _T(",ccs=UNICODE");

   if (retest == MORETESTS)
   {
      int tentativi;

      do
      {
         tentativi = 1;
         do
         {  // se non è il primo tentativo
            if (tentativi > 1) gsc_wait(GEOsimAppl::GLOBALVARS.get_WaitTime()); // attesa tra un tentativo e l'altro
            if ((Result = _wfopen(tmp_path.get_name(), _mode.get_name())) != NULL)
               break;

            tentativi++;
         }
         while (tentativi <= GEOsimAppl::GLOBALVARS.get_NumTest());
      
         if (Result) break;

         // setto l'errore GEOsim
         if (errno == EACCES) GS_ERR_COD = eGSFileLocked;
         else GS_ERR_COD = eGSOpenFile;
         
         // l'utente vuole abbandonare
         if (gsc_dderroradmin(_T("GEOSIM"), tmp_path.get_name()) != GS_GOOD) 
            return NULL;
      }
      while (1);
   }
   else
      if ((Result = _wfopen(tmp_path.get_name(), _mode.get_name())) == NULL)
         switch (errno)
         {
            case ENOENT:
               GS_ERR_COD = eGSFileNoExisting;
               break;
            case EACCES:
               GS_ERR_COD = eGSFileLocked;
               break;
            default:
               GS_ERR_COD = eGSOpenFile;
         }

   if (Result == NULL) return NULL;

   bool IsUnicode = gsc_isUnicodeFile(Result);

   // Se la modalità di apertura non corrisponde a com'è veramente il file
   if (UnicodeOpenMode != IsUnicode)
   { 
      // Chiudo e riapro il file nell'altra modalità
      if (Unicode) *Unicode = IsUnicode;
      gsc_fclose(Result);
      return gsc_fopen(path, mode, retest, &IsUnicode);
   }

   return Result;
}
int gsc_fclose(FILE *file)
{
   if (fclose(file) == EOF) { GS_ERR_COD = eGSCloseFile; return GS_BAD; }
   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_filesize
/*+   
  Calcola la lunghezza del file.
  Parametri:
  FILE stream: handle file

  Ritorna la lunghezza del file o -1 in caso di errore.
-*/
/*************************************************************************/
long gsc_filesize(FILE *stream)
{
  long curpos, length;

  curpos = ftell(stream);
  fseek(stream, 0L, SEEK_END);
  length = ftell(stream);
  fseek(stream, curpos, SEEK_SET);

  return length;
}

  
/*************************************************************************/
/*.doc gsc_load_dialog
/*+   
  Apre un file DCL con conversione automatica della path del file.
  Parametri:
  const TCHAR *dclfile;     file name DCL (input)
  int         *dcl_id;      identificatore del file DCL (out)

  Ritorna RTNORM in caso di successo, altrimenti RTERROR.
-*/
/*************************************************************************/
int gsc_load_dialog(C_STRING &dclfile, int *dcl_id)
   { return gsc_load_dialog(dclfile.get_name(), dcl_id); }
int gsc_load_dialog(const TCHAR *dclfile, int *dcl_id)
{
   C_STRING tmp_path;

   if (!dclfile || !dcl_id) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   tmp_path = dclfile;

   // Controlla correttezza e converte Path
   if (gsc_nethost2drive(tmp_path) == GS_BAD) return RTERROR;
   if (ads_load_dialog(tmp_path.get_name(), dcl_id) != RTNORM)
      { GS_ERR_COD = eGSAbortDCL; return RTERROR; }

   return RTNORM;
}


/*************************************************************************/
/*.doc int gsc_copydir
/*+   
   Copia il contenuto di una directory (secondo il valore di what es. "*.*")
   in un'altra directory che eventualmente crea.
   Parametri:
   TCHAR *source_dir;	direttorio sorgente
   TCHAR *what;			quali files copiare
   TCHAR *dest_dir;	   direttorio di destinazione
   int   recursive;     RECURSIVE -> copia anche i sottodirettori (default = RECURSIVE)
   TCHAR *except;		   quali files NON devono essere copiati (default = NULL)

   Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
int gsc_copydir(TCHAR *source_dir, TCHAR *what, TCHAR *dest_dir, int recursive, 
                TCHAR *except)
{
   C_STRING pathfile, subdir, destfile;
   presbuf  filename, fileattr, pname, pattr, ExceptList = NULL;
   long     totale, indice;
   
   // creo il direttorio
   if (gsc_mkdir(dest_dir) == GS_BAD) return GS_BAD;
      
   subdir = source_dir;
   subdir += _T('\\');

   pathfile = subdir;
   pathfile += what;

   totale = gsc_adir(pathfile.get_name(), &filename, NULL, NULL, &fileattr);
   pname = filename;
   pattr = fileattr;

   // Se indicato carico la lista dei files da non copiare
   if (except)
   {
      pathfile = subdir;
      pathfile += except;
      gsc_adir(pathfile.get_name(), &ExceptList);
   }

   for (indice = 0; indice < totale; indice++) 
   {
      pname = pname->rbnext;
      pattr = pattr->rbnext;

      pathfile = subdir;
      pathfile += pname->resval.rstring;

      destfile = dest_dir;
      destfile += _T('\\');
      destfile += pname->resval.rstring;

      if (*(pattr->resval.rstring + 4) == _T('D')) // direttorio
      {
         if (recursive == RECURSIVE && *(pname->resval.rstring) != _T('.')) // escludo . e ..
            // copio anche il sottodirettorio
            if (gsc_copydir(pathfile.get_name(), what, destfile.get_name(), recursive) == GS_BAD)
            {
               acutRelRb(filename); acutRelRb(fileattr);
               if (ExceptList) acutRelRb(fileattr);
               return GS_BAD;
            }
      }
      else
      {
         if (!gsc_rbsearch(pname->resval.rstring, ExceptList))
            if (gsc_copyfile(pathfile, destfile) == GS_BAD)
            {
               acutRelRb(filename); acutRelRb(fileattr);
               if (ExceptList) acutRelRb(fileattr);
               return GS_BAD;
            }
      }
   }

   if (filename) acutRelRb(filename);
   if (fileattr) acutRelRb(fileattr);
   if (ExceptList) acutRelRb(ExceptList);
   
   return GS_GOOD;
}


/*************************************************************************/
/*.doc  int gsc_delfile(path)  */
/*+   
  Cancella il file 'path'
  Parametri:
  const TCHAR *path;	definisce il path relativo o completo del file.
  int retest;        se MORETESTS -> in caso di errore riprova n volte 
                     con i tempi di attesa impostati poi ritorna GS_BAD,
                     ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                     (default = MORETESTS)
   
  Ritorna GS_GOOD se il file e' stato cancellato correttamente altrimenti GS_BAD.
-*/
/*************************************************************************/
int gs_delfile(void)
{
   presbuf arg = acedGetArgs();

   if (!arg || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (gsc_delfile(arg->resval.rstring) == GS_BAD) return RTERROR;

   return RTNORM;
}
int gsc_delfile(C_STRING &path, int retest)
   { return gsc_delfile(path.get_name(), retest); }
int gsc_delfile(const TCHAR *path, int retest)
{
   C_STRING tmp_path;
   
   if (!path || wcslen(path) == 0) return GS_GOOD;

   tmp_path = path;

   // Controlla Correttezza Path
   if (gsc_nethost2drive(tmp_path) == GS_BAD) return GS_BAD; 
   
   if (retest == MORETESTS)
   {
      int tentativi;

      do
      {
         tentativi = 1;

         do
         {  // se non è il primo tentativo
            if (tentativi > 1) gsc_wait(GEOsimAppl::GLOBALVARS.get_WaitTime()); // attesa tra un tentativo e l'altro
            
            if (_wremove(tmp_path.get_name()) == 0) return GS_GOOD; // tutto OK
            tentativi++;
         }
         while (tentativi <= GEOsimAppl::GLOBALVARS.get_NumTest());
      
         // setto l'errore GEOsim
         GS_ERR_COD = (errno == EACCES) ? eGSFileLocked : eGSNotDel;
         
         if (gsc_dderroradmin(_T("GEOSIM"), tmp_path.get_name()) != GS_GOOD) return GS_BAD; // l'utente vuole abbandonare
      }
      while (1);
   }
   else
      if (_wremove(tmp_path.get_name()) != 0)
      {
         TCHAR Msg[MAX_LEN_MSG + 50];

         GS_ERR_COD = (errno == EACCES) ? eGSFileLocked : eGSNotDel;
         swprintf(Msg, MAX_LEN_MSG + 50, _T("File <%s> not deleted."), tmp_path.get_name());
         gsc_write_log(Msg);

			return GS_BAD;
      }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_delfiles                           <external> */
/*+
  Questa funzione, dato una maschera, cancella tutti i file che la rispettano.
  Parametri:
  TCHAR  *mask;      maskera dei files da cancellare
  int retest;        se MORETESTS -> in caso di errore riprova n volte 
                     con i tempi di attesa impostati poi ritorna GS_BAD,
                     ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                     (default = MORETESTS)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_delfiles(const TCHAR *mask, int retest)
{       
   C_STRING pathfile, drive, dir;
   presbuf  filename, fileattr, pname, pattr;
   long     totale, indice;

   gsc_splitpath(mask, &drive, &dir);

   totale = gsc_adir(mask, &filename, NULL, NULL, &fileattr);
   pname = filename;
   pattr = fileattr;
   
   for (indice = 0; indice < totale; indice++) 
   {
      pname = pname->rbnext;
      pattr = pattr->rbnext;
      
      pathfile = drive;
      pathfile += dir;
      pathfile += pname->resval.rstring;

      if (*(pattr->resval.rstring + 4) != _T('D')) // NON è un direttorio          
         if (gsc_delfile(pathfile, ONETEST) == GS_BAD)
            { acutRelRb(filename); acutRelRb(fileattr); return GS_BAD; }
   }
   if (filename != NULL) acutRelRb(filename);
   if (fileattr != NULL) acutRelRb(fileattr);
      
   return GS_GOOD;
}           


/*************************************************************************/
/*.doc  int gsc_renfile(vecchio, nuovo)  */
/*+   
   Rinomina il file 'vecchio' in 'nuovo'.
   Parametri:
   TCHAR *OldName;   nome vecchio file.
   TCHAR *NewName;   nome nuovo file.
   Ritorna int -> GS_GOOD se il file e' stato rinominato correttamente
                  GS_BAD altrimenti.
   N.B:: funziona solo per DOS E WIN                  
-*/
/*************************************************************************/
int gsc_renfile(C_STRING &OldName, C_STRING &NewName)
{
   return gsc_renfile(OldName.get_name(), NewName.get_name());
}
int gsc_renfile(TCHAR *OldName, TCHAR *NewName)
{
   C_STRING tmp_OldName, tmp_NewName;

   tmp_OldName = OldName;
   tmp_NewName = NewName;
   if (gsc_path_exist(tmp_OldName) == GS_BAD) return GS_BAD;
   if (gsc_path_exist(tmp_NewName) == GS_GOOD)
      { GS_ERR_COD = eGSNotRen; return GS_BAD; }
   if (_wrename(tmp_OldName.get_name(), tmp_NewName.get_name()) != 0)
      { GS_ERR_COD = eGSNotRen; return GS_BAD; }
      
   return GS_GOOD;
}

                         
/*************************************************************************/
/*.doc  long gsc_filedim(path)  */
/*+
   Ritorna la dimensione del file definito da 'path'.   
   Parametri:
   const TCHAR *path;   Definisce il path relativo o completo del file.
   
   Ritorna long -> lunghezza in byte di un file se esiste 
                   altrimenti ritorna -1
-*/
/*************************************************************************/
long gsc_filedim(const TCHAR *path)
{
   C_STRING     tmp_path;
   struct _stat FileHan;

   tmp_path = path;
   if (gsc_nethost2drive(tmp_path) == GS_BAD) return GS_BAD;
   if (_wstat(tmp_path.get_name(), &FileHan) == -1)
      { GS_ERR_COD = eGSOpenFile; return GS_BAD; }

   return FileHan.st_size;
}


/*************************************************************************/
/*.doc  gsc_strcat                                                       */
/*+
   concatena due stringhe allo stesso modo di strcat() ponendo il risultato
   in first, controllando ed eventualmente riallocando la memoria necessaria
   per contenere interamente la stringa concatenata.
   Parametri:
   TCHAR *first -> deve essere un puntatore ad una stringa allocata in maniera
                  dinamica (per eventuale realloc) oppure un puntatore a NULL.
   TCHAR *second-> deve essere un puntatore ad una stringa allocata in maniera
                  statica dinamica o costante (contenente '\0')

   Ritorna *TCHAR puntatore alla stringa concatenta contenuta nella memoria
   puntata da first.
   (WARNING: Alloca memoria!!)
-*/
/*************************************************************************/
TCHAR *gsc_strcat(TCHAR *first, const TCHAR *second)
{
   size_t len;
   TCHAR  *result = NULL;

   if (!first)
   {
      if (!second) return NULL;

      len = wcslen(second) + 1; 
      if ((result = (TCHAR *) malloc(sizeof(TCHAR) * len)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
      wcscpy(result, second);
   }
   else
   { 
      if (!second) return first;

      len = wcslen(first) + wcslen(second) + 1; 
      if ((result = (TCHAR *) realloc(first, len * sizeof(TCHAR))) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
      result = wcscat(result, second);
   }
   
   return result;
}     
TCHAR *gsc_strcat(TCHAR *first, TCHAR second)
{
   TCHAR *stringa, *result;

   if ((stringa = (TCHAR *) malloc(sizeof(TCHAR) * 2)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
   stringa[0] = second;
   stringa[1] = _T('\0');
   result = gsc_strcat(first, stringa);
   free(stringa);

   return result;
}     

/*************************************************************************/
/*.doc  TCHAR *gsc_tostring(in)  */
/*+
   Converte il parametro 'in' (riconoscendo il suo tipo) in una stringa
   restituendo il puntatore ad essa allocando dinamicamente la meomoria
   necessaria all'interno della funzione.
   Parametri:
   in -> puo' essere una variabile di uno dei seguenti tipi :
         short, int, long, float, double, *TCHAR
         in quest'ultimo caso ritona una copia della stringa.
   
   Ritorna puntatore alla stringa contenente la conversione.
   (WARNING: Alloca memoria!!)
-*/
/*************************************************************************/
TCHAR *gsc_tostring(short in)         /* SHORT */
{
   TCHAR buf[20];

   gsc_itoa((int)in, buf, 10);
   return gsc_strcat(NULL ,buf);
}

TCHAR *gsc_tostring(int in)           /* INT */
{
   TCHAR buf[20];

   gsc_itoa(in, buf, 10);
   return gsc_strcat(NULL, buf);
}

TCHAR *gsc_tostring(long in)          /* LONG */
{
   TCHAR buf[30];

   gsc_ltoa(in, buf, 10);
   return gsc_strcat(NULL, buf);
}

TCHAR *gsc_tostring(float in)         // FLOAT
{
   return gsc_tostring((double) in);        // DOUBLE
}
TCHAR *gsc_tostring(float in, int digits, int dec)         // FLOAT
{
   if (dec == -1) // uso la precisione di autocad
   {
      resbuf AcadPrec;

      acedGetVar(_T("LUPREC"), &AcadPrec);
      dec = AcadPrec.resval.rint;
   }

   return gsc_tostring((double) in, digits, dec);        // DOUBLE
}

TCHAR *gsc_tostring(double in)        // DOUBLE
{
//   TCHAR buf[50], *p_sep, *out = NULL;
//   int   nDec = 0, pos;

//   Mask[10];

//   Primo tentativo fallito
//   if (in >= pow(10, 20)) // numero con 20 cifre
//      { GS_ERR_COD = eGSInvalidArg; return NULL; }
//   resbuf AcadPrec;
//   if (acedGetVar(_T("LUPREC"), &AcadPrec)!=RTNORM)
//      { GS_ERR_COD = eGSVarNotDef; return NULL; }
//   swprintf(Mask, 10, _T("%%.%df"), AcadPrec.resval.rint);
//   swprintf(buf, 50, Mask, in);

//   Secondo tentativo fallito
//   non funziona con questo numero: 12345678901234.12
//   swprintf(buf, 50, _T("%16f"), in);

//   Terzo tentativo fallito
//   conto quanti decimali ha il numero (non va perchè la sottrazione da risultati errati)
//   double decPart, intPart;
//   modf(in, &intPart);
//   decPart = in - (long)intPart;
//   while (decPart > 0)
//   {
//      nDec++;
//      decPart = decPart * 10;
//      modf(decPart, &intPart);
//      decPart = decPart - (long)intPart;
//   }
//   swprintf(Mask, 10, _T("%%.%df"), nDec);
//   swprintf(buf, 50, Mask, in);

   // non funziona con questo numero: 85.71 !!! ma non so come risolverlo...
   //_gcvt(in, 16, buf); // uso 16 perchè con numero maggiori dà problemi (prova con 3456.90)

//   gsc_ltrim(buf);
//   pos = wcslen(buf) - 1;
//   if ((p_sep = wcschr(buf, _T('.'))) != NULL) // ha dei decimali
//      while (&(buf[pos]) != p_sep && buf[pos] == _T('0')) buf[pos--] = _T('\0');

//   if (buf[pos] == _T('.')) buf[pos] = _T('\0');
//   if ((out = gsc_strcat(out,buf)) == NULL) return NULL;  

   //C_STRING        Buf;
   //static C_STRING DecSep;
   //_variant_t      var;

   //gsc_set_variant_t(var, in);
   //var.ChangeType(VT_BSTR); // Conversione da numero a stringa
   //Buf = (TCHAR *) _bstr_t(var);

   //if (DecSep.get_name() == NULL)
   //   if (gsc_getLocaleDecimalSep(DecSep) == GS_BAD) return NULL;
   // 
   //// sostituisco il carattere separatore dei decimali con il punto
   //Buf.strtran(DecSep.get_name(), _T("."));
   //return Buf.cut();

   static TCHAR _buffer[1000];  // make sure this is big enough!!!
   swprintf_s(_buffer, sizeof(_buffer), _T("%.99f"), in);
   gsc_rtrim(_buffer, _T('0'));
   gsc_rtrim(_buffer, _T('.'));
   return gsc_tostring(_buffer);
}
TCHAR *gsc_tostring(double in, int digits, int dec)        // DOUBLE
{
   TCHAR  buf[50], frm[10], *p_sep;
   size_t pos;

   if (dec == -1) // uso la precisione di autocad
   {
      resbuf AcadPrec;

      acedGetVar(_T("LUPREC"), &AcadPrec);
      dec = AcadPrec.resval.rint;
   }

//   if (in >= pow(10, 20)) // numero con 20 cifre
//     { GS_ERR_COD = eGSInvalidArg; return NULL; }
   
   if (digits > 16 || digits <= 0) digits = 16;
   swprintf(frm, 10, _T("%%%d.%df"), digits, (dec < 0) ? 0 : dec); // se dec < 0 impongo 0
   swprintf(buf, 50, frm, in);

   gsc_ltrim(buf);
   pos = wcslen(buf) - 1;
   if ((p_sep = wcschr(buf, _T('.'))) != NULL) // ha dei decimali
      while (&(buf[pos]) != p_sep && buf[pos] == _T('0')) buf[pos--] = _T('\0');

   if (buf[pos] == _T('.')) buf[pos] = _T('\0');
   return gsc_strcat(NULL, buf);  
}

TCHAR *gsc_tostring(const TCHAR *in) // CHAR
{
   return gsc_strcat(NULL, in);
}

TCHAR *gsc_tostring(ads_point pt) // punto AutoCAD
{
   TCHAR *out = NULL, *dummy;

   if ((out = gsc_tostring(pt[X])) == NULL) return NULL;
   if ((out = gsc_strcat(out, _T(","))) == NULL) return NULL;
   if ((dummy = gsc_tostring(pt[Y])) == NULL) return NULL;
   if ((out = gsc_strcat(out, dummy)) == NULL) { free(dummy); return NULL; }
   free(dummy);
   if ((out = gsc_strcat(out, _T(","))) == NULL) return NULL;
   if ((dummy = gsc_tostring(pt[Z])) == NULL) return NULL;
   if ((out = gsc_strcat(out, dummy)) == NULL) { free(dummy); return NULL; }
   free(dummy);

   return out;
}

TCHAR *gsc_tostring(ads_point pt, int digits, int dec) // punto AutoCAD
{
   TCHAR *out = NULL, *dummy;

   if ((out = gsc_tostring(pt[X], digits, dec)) == NULL) return NULL;
   if ((out = gsc_strcat(out, _T(","))) == NULL) return NULL;
   if ((dummy = gsc_tostring(pt[Y], digits, dec)) == NULL) return NULL;
   if ((out = gsc_strcat(out, dummy)) == NULL) { free(dummy); return NULL; }
   free(dummy);
   if ((out = gsc_strcat(out, _T(","))) == NULL) return NULL;
   if ((dummy = gsc_tostring(pt[Z], digits, dec)) == NULL) return NULL;
   if ((out = gsc_strcat(out, dummy)) == NULL) { free(dummy); return NULL; }
   free(dummy);

   return out;
}

TCHAR *gsc_tostring_format(double in, int digits, int dec)        /* DOUBLE */
{
   TCHAR buf[50], frm[10];
   
   if (dec == -1) // uso la precisione di autocad
   {
      resbuf AcadPrec;

      acedGetVar(_T("LUPREC"), &AcadPrec);
      dec = AcadPrec.resval.rint;
   }

   if (in >= pow(10.0, digits)) // numero con 7 cifre
   {
      in = in / pow(10.0, digits);

      if (digits<10)
         swprintf(frm, 10, _T("%%%d.%dfE+0%d"), 1, (dec < 0) ? 0 : dec, digits); // se dec < 0 impongo 0
      else
         swprintf(frm, 10, _T("%%%d.%dfE+%d"), 1, (dec < 0) ? 0 : dec, digits); // se dec < 0 impongo 0
   }
   else
      swprintf(frm, 10, _T("%%%d.%df"), digits, (dec < 0) ? 0 : dec); // se dec < 0 impongo 0

   swprintf(buf, 50, frm, in);
   gsc_ltrim(buf);
   
   return gsc_tostring(buf);
}


/*********************************************************/
/*.doc gsc_Str2Pt                             <internal> */
/*+
  Questa funzione converte una stringa che memorizza un
  punto nel formato "x, y, z" in un punto (ads_point).
  Parametri
  TCHAR *string;    stringa da elaborare
  ads_point pt;     punto

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_Str2Pt(TCHAR *string, ads_point pt)
{
   int      n_coord;
   C_STRING StrPoint;
   TCHAR    *pStr;
   
   if (!string) { GS_ERR_COD = eGSInvLispString; return GS_BAD; }
   StrPoint = string;
   pStr = StrPoint.get_name();

   if ((n_coord = gsc_strsep(pStr, _T('\0')) + 1) < 2 || n_coord > 3)
      { GS_ERR_COD = eGSInvLispString; return GS_BAD; }

   pt[X] = _wtof(pStr);
   while (*pStr != _T('\0')) pStr++; pStr++;
   pt[Y] = _wtof(pStr);
   while (*pStr != _T('\0')) pStr++; pStr++;
   if (n_coord == 3)
   {
      pt[Z] = _wtof(pStr);
      while(*pStr != _T('\0')) pStr++; pStr++;
   }
   
   return GS_GOOD;
}


/*************************************************************************/
/*.doc  gsc_strtran                                                      */  
/*+
  La funzione riceve tre stringhe e restituisce una nuova stringa che 
  corrisponde alla sostituzione nella prima stringa di tutte le occorrenze
  della seconda stringa con la terza.
  Parametri:
  TCHAR *source;
  TCHAR *sub;
  TCHAR *con;
  int   sensitive;      sensibile al maiuscolo (default = TRUE)

  (WARNING: alloca memoria)
-*/
/*************************************************************************/
TCHAR *gsc_strtran(TCHAR *source, TCHAR *sub, TCHAR *con, int sensitive)
{
   TCHAR  *target, *next, mystr[1000];                                  
   size_t so, ta, i;
  
   so  = wcslen(sub);
   ta  = wcslen(con);

   target = mystr;    // puntano alla stessa stringa
    
   while ((next = gsc_strstr(source, sub, sensitive)) != NULL)   // cerca next 'sub'
   {
      while (source < next)   // Ricopia in 'mystr' fino a next 'sub'
         { *target = *source; target++; source++; }
      for (i = 0; i < ta; i++)     // Ricopia in 'mystr' 'con'
         { *target = con[i]; target++; }
      source += so;           // Salta alla fine di next 'sub'
   }

   while (*source != _T('\0'))  // Ricopia fino alla fine
      { *target = *source; target++; source++; }
   *target = _T('\0');

   target = NULL;
   target = gsc_strcat(target, mystr);   
   
   return target;
}   


/*************************************************************************/
/*.doc  int gs_strsep                                                    */  
/*+
  La funzione sostituisce alcuni caratteri con degli altri.
  Parametri:
  TCHAR *string;  stringa da elaborare
  TCHAR ch;       carattere con cui si effettua la sostituzione
  TCHAR ric;      carattere da ricercare (default ',')

  Restituisce il numero delle sostituzioni effettuate.
-*/
/*************************************************************************/
int gsc_strsep(TCHAR *string, TCHAR ch, TCHAR ric)
{
   int i = 0, cont = 0;

   if (string == NULL) return 0;                          
    
   while (string[i] != _T('\0'))
   {
      if (string[i] == ric) { cont++; string[i] = ch; }
      i++;
   }
    
   return cont;   
}    
    
/*************************************************************************/
/*.doc gsc_ltrim <external> /*
/*+
  Questa funzione cancella gli spazi a sinistra del testo nella stringa.
  Parametri:
  TCHAR *stringa;   stringa da modificare.
-*/  
/*************************************************************************/
TCHAR *gsc_ltrim(TCHAR *stringa)
{
   TCHAR *indice1, *indice2;
   short more = 1;

   if (stringa == NULL) return stringa;

   for (indice1 = stringa; *indice1 != _T('\0'); indice1++)
      if (*indice1 != _T(' ')) break;

   for (indice2 = stringa; more; indice2++)
   {
      *indice2 = *indice1;
      if (*(indice1++) == _T('\0')) more = 0;
   }

   return stringa;
}

                               
/*************************************************************************/
/*.doc gsc_rtrim <external> /*
/*+
  Questa funzione cancella gli spazi a destra del testo nella stringa.
  Parametri:
  TCHAR *stringa;   stringa da modificare.
  TCHAR charToTrim; carattere da eliminare (defaiult = ' ')
-*/  
/*************************************************************************/
TCHAR *gsc_rtrim(TCHAR *stringa, TCHAR charToTrim)
{
   TCHAR *indice;

   if (!stringa || *stringa == _T('\0')) return stringa;

   for (indice = stringa + wcslen(stringa) - 1; indice != stringa; indice--)
      if (*indice == charToTrim) *indice = _T('\0');
      else break;

   return stringa;
}

                               
/*************************************************************************/
/*.doc gsc_alltrim <external> /*
/*+
  Questa funzione cancella gli spazi a destra e a sinistra del testo 
  nella stringa.
  Parametri:
  TCHAR *stringa;   stringa da modificare.
-*/  
/*************************************************************************/
TCHAR *gsc_alltrim(TCHAR *stringa)
{
   if (gsc_ltrim(stringa) == 0) return stringa;
   if (gsc_rtrim(stringa) == 0) return stringa;

   return stringa;
}
                               

/*************************************************************************/
/*.doc gsc_wait <external> /*
/*+
  Questa funzione ferma il processo per un certo numero di
  secondi.
  Parametri:
  int secondi;   numero di secondi.
-*/  
/*************************************************************************/
void gsc_wait(int secondi)
{
   HCURSOR PrevCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
   Sleep(secondi * 1000); // millisecondi
   SetCursor(PrevCursor);
}


/*************************************************************************/
/*.doc  int gsc_toupper(stringa)  */
/*+
   converte una stringa in maiuscolo.
   Parametri:
   TCHAR *stringa;    deve essere un puntatore ad una stringa allocata in 
                     maniera statica dinamica o costante (contenente '\0')
-*/
/*************************************************************************/
int gsc_toupper(TCHAR *stringa)
{
   if (stringa) _wcsupr(stringa);

   return GS_GOOD;
}     
int gsc_toupper(char *stringa)
{
   if (stringa) _strupr(stringa);

   return GS_GOOD;
}     


/*************************************************************************/
/*.doc  int gsc_tolower(stringa)  */
/*+
   converte una stringa in minuscolo.
   Parametri:
   TCHAR *stringa;    deve essere un puntatore ad una stringa allocata in 
                     maniera statica dinamica o costante (contenente '\0')
-*/
/*************************************************************************/
int gsc_tolower(TCHAR *stringa)
{
   if (stringa) _wcslwr(stringa);

   return GS_GOOD;
}     


/*********************************************************/
/*.doc gsc_adir <external> /*
/*+
  Questa funzione legge il contenuto di una directory e lo memorizza in 5
  possibili liste resbuf avente le seguenti strutture:
  lista 1 :(filename1 filename2 ... filenamen).
  lista 2 :(size1 size2 ... sizen).
  lista 3 :(datetime1 datetime2 ... datetimen).  
  lista 5 :(attribute1 attribute2 ... attributen).
  Se uno di questi puntatori a liste == NULL non verrà considerato.
  Le date sono espresse come nel seguente esempio:
  Wed Jan 02 02:03:55 1980\n\0
                            
  Parametri:
  TCHAR   *path;                 nome del direttorio (con maschera)
  C_STR_LIST *pFileNameList;     lista resbuf contenente i nomi dei file (default = NULL)
  C_LONG_LIST *pFileSizeList;    lista resbuf contenente le dimensioni dei file (default = NULL)
  C_STR_LIST *pFileDatetimeList; lista resbuf contenente le date e i tempi dei file (default = NULL)
  C_STR_LIST *pFileAttrList;     lista resbuf contenente gli attributi dei file (default = NULL)
                                 (vedi funzione filetype()).
  bool    OnlyFiles;             Opzionale. Flag che se vero filtra solo i files 
                                 tralasciando le cartelle

  Restituisce il numero di file in caso di successo altrimenti -1.
-*/  
/*********************************************************/
long gsc_adir(const TCHAR *path, C_STR_LIST *pFileNameList, C_LONG_LIST *pFileSizeList,
              C_STR_LIST *pFileDatetimeList, C_STR_LIST *pFileAttrList, bool OnlyFiles)
{
   struct   _wfinddata_t DescrFile;
   long     nfile = 0;
   intptr_t hfile;
   TCHAR    buf[26]; // vedi _wctime_s
   C_STRING TempPath, AttribStr;

   if (pFileNameList)     pFileNameList->remove_all();
   if (pFileSizeList)     pFileSizeList->remove_all();
   if (pFileDatetimeList) pFileDatetimeList->remove_all();
   if (pFileAttrList)     pFileAttrList->remove_all();
   
   if (!path) { GS_ERR_COD = eGSInvalidPath; return -1; }
      
   TempPath = path;
   if (gsc_dir_exist(TempPath, ONETEST) == GS_BAD) return -1;

   if ((hfile = _wfindfirst(TempPath.get_name(), &DescrFile)) == -1L) return 0;
   
   do
   {
      // attributi del file
      if ((AttribStr.paste(filetype(DescrFile.attrib))) == NULL)
         { _findclose(hfile); return -1; }

      if (OnlyFiles) // Scarto le directory
         if (AttribStr.get_chr(5) == _T('D')) continue;

      if (pFileNameList)      // nome del file 
         if (pFileNameList->add_tail_str(DescrFile.name) == GS_BAD)
            { _findclose(hfile); return -1; }
   
      if (pFileSizeList)      // dimensione del file 
         if (pFileSizeList->add_tail_long(DescrFile.size) == GS_BAD)
            { _findclose(hfile); return -1; }

      if (pFileDatetimeList)  // date time del file
      {
         _wctime_s(buf, 26, &(DescrFile.time_write));
         if (pFileDatetimeList->add_tail_str(buf) == GS_BAD)
            { _findclose(hfile); return -1; }
      }

      if (pFileAttrList)      // attributi del file
         if (pFileAttrList->add_tail_str(AttribStr.get_name()) == GS_BAD)
            { _findclose(hfile); return -1; }

      nfile++;
   }
   while (_wfindnext(hfile, &DescrFile) == 0);

   _findclose(hfile);
   
   return nfile;
}
long gsc_adir(const TCHAR *path, presbuf *filename, presbuf *filesize,
              presbuf *filedatetime, presbuf *fileattr)
{
   struct   _wfinddata_t descrfile;
   long     nfile = 0;
   intptr_t hfile;
   presbuf  plastname, plastsize, plastdatetime, plastattr;
   TCHAR    *stringa;
   C_STRING tmppath;

   if (filename != NULL) *filename = NULL;
   if (filesize != NULL) *filesize = NULL;
   if (filedatetime != NULL) *filedatetime = NULL;
   if (fileattr != NULL) *fileattr = NULL;
   
   if (!path) { GS_ERR_COD = eGSInvalidPath; return -1; }
      
   tmppath = path;
   if (gsc_dir_exist(tmppath, ONETEST)==GS_BAD) return -1;

   if ((hfile = _wfindfirst(tmppath.get_name(), &descrfile)) == -1L) return 0;
   
   if (filename != NULL)  // nome del file 
   {
      if ((stringa = gsc_tostring(descrfile.name)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return -1; }
      if ((plastname = *filename = acutBuildList(RTLB, RTSTR, stringa, 0)) == NULL)
         { free(stringa); GS_ERR_COD = eGSOutOfMem; return -1; }
      plastname = plastname->rbnext;
      free(stringa);
   }
   
   if (filesize != NULL)  // dimensione del file 
   {  
      if ((plastsize = *filesize = acutBuildList(RTLB, RTLONG, descrfile.size, 0)) == NULL)
      {
         rel_resbuf(filename, filesize, filedatetime, fileattr);
         GS_ERR_COD = eGSOutOfMem; return -1;
      }
      plastsize = plastsize->rbnext;
   }

   if (filedatetime != NULL)  // date time del file 
   {
      if ((stringa = gsc_tostring(_wctime(&(descrfile.time_write)))) == NULL)
      {
         rel_resbuf(filename, filesize, filedatetime, fileattr);
         GS_ERR_COD = eGSOutOfMem; return -1;
      }
      if ((plastdatetime = *filedatetime = acutBuildList(RTLB, RTSTR, stringa, 0)) == NULL)
      {
         free(stringa);
         rel_resbuf(filename, filesize, filedatetime, fileattr);
         GS_ERR_COD = eGSOutOfMem; return -1;
      }
      plastdatetime = plastdatetime->rbnext;
      free(stringa);
   }
   
   if (fileattr != NULL)  // attributi del file 
   {
      if ((stringa = filetype(descrfile.attrib)) == NULL)
      {
         rel_resbuf(filename, filesize, filedatetime, fileattr);
         GS_ERR_COD = eGSUnknown; return -1;
      }
      if ((plastattr = *fileattr = acutBuildList(RTLB, RTSTR, stringa, 0)) == NULL)
      {
         free(stringa);
         rel_resbuf(filename, filesize, filedatetime, fileattr);
         GS_ERR_COD = eGSOutOfMem; return -1;
      }
      free(stringa);
      plastattr = plastattr->rbnext;
   }   
  
   nfile = 1;
   while (_wfindnext(hfile, &descrfile) == 0)
   {
      if (filename != NULL)  // nome del file 
      {
         if ((stringa = gsc_tostring(descrfile.name)) == NULL)
         { 
            rel_resbuf(filename, filesize, filedatetime, fileattr);
            GS_ERR_COD = eGSOutOfMem;
            _findclose(hfile);
            return -1;
         }      
         if ((plastname->rbnext = acutBuildList(RTSTR, stringa, 0)) == NULL)
         { 
            free(stringa);
            rel_resbuf(filename, filesize, filedatetime, fileattr);
            GS_ERR_COD = eGSOutOfMem;
            _findclose(hfile);
            return -1;
         }
         free(stringa);
         plastname = plastname->rbnext;
      }
      
      if (filesize != NULL)  // dimensione del file 
      {  
         if ((plastsize->rbnext = acutBuildList(RTLONG, descrfile.size, 0)) == NULL)
         {
            rel_resbuf(filename, filesize, filedatetime, fileattr);
            GS_ERR_COD = eGSOutOfMem;
            _findclose(hfile);
            return -1;
         }
         plastsize = plastsize->rbnext;
      }

      if (filedatetime != NULL)  // date time del file 
      {
         if ((stringa = gsc_tostring(_wctime(&(descrfile.time_write)))) == NULL)
         {
            rel_resbuf(filename, filesize, filedatetime, fileattr);
            GS_ERR_COD = eGSOutOfMem;
            _findclose(hfile);
            return -1;
         }
         if ((plastdatetime->rbnext = acutBuildList(RTSTR, stringa, 0)) == NULL)
         {
            free(stringa);
            rel_resbuf(filename, filesize, filedatetime, fileattr);
            GS_ERR_COD = eGSOutOfMem;
            _findclose(hfile);
            return -1;
         }
         free(stringa);
         plastdatetime = plastdatetime->rbnext;
      }
      
      if (fileattr != NULL)  // attributi del file 
      {
         if ((stringa = filetype(descrfile.attrib)) == NULL)
         {
            rel_resbuf(filename, filesize, filedatetime, fileattr);
            GS_ERR_COD = eGSOutOfMem;
            _findclose(hfile);
            return -1;
         }
         if ((plastattr->rbnext = acutBuildList(RTSTR, stringa, 0)) == NULL)
         {
            free(stringa);
            rel_resbuf(filename, filesize, filedatetime, fileattr);
            GS_ERR_COD = eGSOutOfMem;
            _findclose(hfile);
            return -1;
         }
         free(stringa);
         plastattr = plastattr->rbnext;
      }

      nfile++;
   }
   _findclose(hfile);

   if (filename != NULL)  // nome del file 
   {
      if ((plastname->rbnext = acutBuildList(RTLE, 0)) == NULL)
      { 
         rel_resbuf(filename, filesize, filedatetime, fileattr);
         GS_ERR_COD = eGSOutOfMem; return -1;
      }
   }
   if (filesize != NULL)  // size del file 
   {
      if ((plastsize->rbnext = acutBuildList(RTLE, 0)) == NULL)
      { 
         rel_resbuf(filename, filesize, filedatetime, fileattr);
         GS_ERR_COD = eGSOutOfMem; return -1;
      }
   }
   if (filedatetime != NULL)  // datetime del file 
   {
      if ((plastdatetime->rbnext = acutBuildList(RTLE, 0)) == NULL)
      { 
         rel_resbuf(filename, filesize, filedatetime, fileattr);
         GS_ERR_COD = eGSOutOfMem; return -1;
      }
   }
   if (fileattr != NULL)  // attributi del file 
   {
      if ((plastattr->rbnext = acutBuildList(RTLE, 0)) == NULL)
      { 
         rel_resbuf(filename, filesize, filedatetime, fileattr);
         GS_ERR_COD = eGSOutOfMem; return -1;
      }
   }
   
   return nfile;
}


/*********************************************************/
/*.doc rel_resbuf <internal> */
/*+
  Questa funzione rilascia delle liste resbuf e inizializza i puntatori a NULL.
  Viene usata da gsc_adir.
  Parametri:
  presbuf *filename;
  presbuf *filesize;
  presbuf *filedate;
  presbuf *filetime;
  presbuf *fileattr;
-*/  
/*********************************************************/
void rel_resbuf(presbuf *filename, presbuf *filesize, presbuf *filedatetime,
                presbuf *fileattr)
{ 
   if (filename != NULL) 
      if (*filename != NULL) { acutRelRb(*filename); *filename = NULL; }
   if (filesize != NULL)
      if (*filesize != NULL) { acutRelRb(*filesize); *filesize = NULL; }
   if (filedatetime != NULL)
      if (*filedatetime != NULL) { acutRelRb(*filedatetime); *filedatetime = NULL; }
   if (fileattr != NULL)
      if (*fileattr != NULL) { acutRelRb(*fileattr); *fileattr = NULL; }
}


/*********************************************************/
/*.doc filetype <internal> */
/*+
  Questa funzione riceve un unsigned ricavato dal campo attrib della
  struttura _finddata_t e ritorna una stringa rappresentante una
  maschera di stato del file:
  
  1 carattere:   R -> Readonly   altrimenti -> .
  2 carattere:   H -> Hidden     altrimenti -> . 
  3 carattere:   S -> System     altrimenti -> . 
  4 carattere:   A -> Archive    altrimenti -> . 
  5 carattere:   D -> Directory  altrimenti -> . 
  6 carattere:   N -> Normal     altrimenti -> . 
  
  Viene usata da gsc_adir.
  Ritorna una stringa di un carattere indicante il tipo o in caso di 
  malfunzionamento NULL.
  N.B. Alloca memoria
-*/  
/*********************************************************/
TCHAR *filetype(unsigned attrib)
{                          
   TCHAR *p;
   
   if ((p = (TCHAR *) malloc(sizeof(TCHAR) * (6 + 1))) == NULL)
   { GS_ERR_COD = eGSOutOfMem; return NULL; }
   // Read only
   if (attrib & _A_RDONLY) p[0] = _T('R'); else p[0] = _T('.');
   // Hidden
   if (attrib & _A_HIDDEN) p[1] = _T('H'); else p[1] = _T('.');
   // System
   if (attrib & _A_SYSTEM) p[2] = _T('S'); else p[2] = _T('.');
   // Archive
   if (attrib & _A_ARCH) p[3] = _T('A'); else p[3] = _T('.');
   // Directory
   if (attrib & _A_SUBDIR) p[4] = _T('D'); else p[4] = _T('.');
   // Normal
   if (attrib & _A_NORMAL) swprintf(p + 5, 6 + 1, _T("N")); else swprintf(p + 5, 6 + 1, _T("."));
   
   return p;
}   
  

/*********************************************************/
/*.doc gsc_delall <external> */
/*+
  Questa funzione, dato un direttorio, cancella tutti i file del direttorio e rimuove 
  il direttorio stesso se l'opzione remove=TRUE, mentre se l'opzione recursive = RECURSIVE
  saranno cancellati anche i sottodirettori con i loro files.
  Parametri:
  TCHAR *dir;        direttorio da cancellare
  int   recursive;   se = RECURSIVE cancella anche i sottodirettori,
                     se = NORECURSIVE solo il direttorio indicato
  int   remove       se = TRUE cancella anche la directory,
                     se = FALSE cancella solo il contenuto (default = TRUE)
  int   retest;      se MORETESTS -> in caso di errore riprova n volte 
                     con i tempi di attesa impostati poi ritorna GS_BAD,
                     ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                     (default = MORETESTS)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gs_delall(void)
{       
   presbuf arg;
   int     recursive, removeDir = TRUE, retest = GS_GOOD;
   TCHAR   *dir = NULL;

   acedRetNil();
   arg = acedGetArgs();

   // direttorio
   if (arg == NULL || arg->restype != RTSTR)
      { GS_ERR_COD=eGSInvalidArg; return RTERROR; }
   if ((dir = gsc_tostring(arg->resval.rstring)) == NULL)
      { free(dir); GS_ERR_COD = eGSOutOfMem; return RTERROR; }
   // flag di ricorsività
   if ((arg = arg->rbnext) == NULL || arg->restype != RTSHORT) // Errore argomenti
      { free(dir); GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   recursive = arg->resval.rint;
   // flag di rimozione direttori
   if ((arg = arg->rbnext) != NULL)
   {
      if (arg->restype != RTSHORT) // Errore argomenti
         { free(dir); GS_ERR_COD = eGSInvalidArg; return RTERROR; }
      else removeDir = arg->resval.rint;

      if ((arg = arg->rbnext) != NULL)
         if (arg->restype != RTSHORT) // Errore argomenti
            { free(dir); GS_ERR_COD = eGSInvalidArg; return RTERROR; }
         else retest = arg->resval.rint;
   }

   if (gsc_delall(dir, recursive, removeDir, retest) == GS_BAD) { free(dir); return RTERROR; }
   free(dir);
   acedRetT();

   return RTNORM;
}
int gsc_delall(TCHAR *dir, int recursive, int removeDir, int retest)
{       
   C_STRING pathfile, subdir;
   presbuf  filename, fileattr, pname, pattr;
   long     totale, indice;

   subdir = dir;
   if (dir[gsc_strlen(dir)-1] != _T('\\'))
      subdir += _T('\\');

   pathfile = subdir;
   pathfile += _T("*.*");

   totale = gsc_adir(pathfile.get_name(), &filename, NULL, NULL, &fileattr);
   pname = filename;
   pattr = fileattr;
   
   for (indice = 0; indice < totale; indice++) 
   {
      pname = pname->rbnext;
      pattr = pattr->rbnext;
      
      pathfile = subdir;
      pathfile += pname->resval.rstring;

      if (*(pattr->resval.rstring + 4) == _T('D'))  // direttorio          
      {
         if (recursive == RECURSIVE && *(pname->resval.rstring) != _T('.'))  // escludo . e ..
            // cancello anche il sottodirettorio
            if (gsc_delall(pathfile.get_name(), recursive, removeDir, retest) == GS_BAD)
               { acutRelRb(filename); acutRelRb(fileattr); return GS_BAD; }
      }
      else
         if (gsc_delfile(pathfile, retest) == GS_BAD)
            { acutRelRb(filename); acutRelRb(fileattr); return GS_BAD; }
   }
   if (filename != NULL) acutRelRb(filename);
   if (fileattr != NULL) acutRelRb(fileattr);
   
   if (removeDir)
      if (gsc_rmdir(dir, retest) == GS_BAD) return GS_BAD;
   
   return GS_GOOD;
}           


///////////////////////////////////////////////////////////////////////////
// INIZIO FUNZIONI PER I FILE DI PROFILO
///////////////////////////////////////////////////////////////////////////


//-----------------------------------------------------------------------//
// GSC_GET_PROFILE //
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc gsc_read_profile                                  */
/*+
  Lettura di un file di profilo in una lista di stringhe, ciascuna stringa
  rappresenta una riga del file.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_read_profile_auxilium(C_PROFILE_SECTION_BTREE &Sections, TCHAR *Row, C_BPROFILE_SECTION **pSection)
{
   TCHAR    *pEntryValue;
   size_t   RowLen;
   C_STRING dummy;

   gsc_ltrim(Row);
   if ((RowLen = gsc_strlen(Row)) == 0) return GS_GOOD;

   if (Row[0] == _T('[') && Row[RowLen - 1] == _T(']'))
   {
      Row[RowLen - 1] = _T('\0'); // fine stringa
      Row++;
      dummy = Row;
      dummy.alltrim();
      if (Sections.add(dummy.get_name()) == GS_BAD) return GS_BAD;
      *pSection = (C_BPROFILE_SECTION *) Sections.get_cursor();
   }
   else
   {
      if (!(*pSection))
      {
         if (Sections.add(_T("")) == GS_BAD) return GS_BAD;
         *pSection = (C_BPROFILE_SECTION *) Sections.get_cursor();
      }
      
      if (!(pEntryValue = gsc_strstr(Row, _T("=")))) return GS_BAD;       
      *pEntryValue = _T('\0'); // fine stringa
      pEntryValue++;
      
      dummy = Row;
      dummy.alltrim();
      if ((*pSection)->get_ptr_EntryList()->add(dummy.get_name()) == GS_BAD) return GS_BAD;

      dummy = pEntryValue;
      dummy.alltrim();
      ((C_B2STR *) (*pSection)->get_ptr_EntryList()->get_cursor())->set_name2(dummy.get_name());
   }

   return GS_GOOD;
}
int gsc_read_profile(const TCHAR *Path, C_PROFILE_SECTION_BTREE &Sections, bool *Unicode)
{
   bool   _Unicode;
   long   FileSize;
   size_t OffSet = gsc_strlen(_T("\r\n"));
   FILE   *file;
   TCHAR  *stream, *Row, *EndRow;
   C_BPROFILE_SECTION *pSection = NULL;

   Sections.remove_all();
   if ((file = gsc_fopen(Path, _T("rb"), ONETEST, &_Unicode)) == NULL) return GS_BAD;
   if (Unicode) *Unicode = _Unicode;

   if ((FileSize = gsc_filesize(file)) == -1) return GS_BAD;
   if (FileSize == 0) // file vuoto
      { gsc_fclose(file); return GS_GOOD; }
   if((stream = (TCHAR *) calloc((size_t) (FileSize + 2), sizeof(TCHAR))) == NULL)
      { gsc_fclose(file); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   if (gsc_fread(stream, (size_t) FileSize, file, _Unicode) == NULL)
      { gsc_fclose(file); return GS_BAD; }
   if (gsc_fclose(file) == GS_BAD) return GS_BAD;

   Row = stream;
   if (_Unicode) Row++; // salto il primo carattere che dice che si tratta di file unicode

   while ((EndRow = gsc_strstr(Row, _T("\r\n"))) != NULL) // cerca il ritorno a capo
   {
      *EndRow = _T('\0'); // fine stringa
    
      if (gsc_read_profile_auxilium(Sections, Row, &pSection) == GS_BAD) return GS_BAD;

      Row = EndRow + OffSet;
   }

   if (gsc_read_profile_auxilium(Sections, Row, &pSection) == GS_BAD) return GS_BAD;

   free(stream);
 
   return GS_GOOD;
}
int gsc_read_profile(C_STRING &Path, C_PROFILE_SECTION_BTREE &Sections, bool *Unicode)
   { return gsc_read_profile(Path.get_name(), Sections, Unicode); }
int gsc_write_profile(const TCHAR *Path, C_PROFILE_SECTION_BTREE &Sections, bool Unicode)
{
   C_STRING           Buffer;
   FILE               *file;
   C_BPROFILE_SECTION *pSection = (C_BPROFILE_SECTION *) Sections.go_top();
   C_B2STR            *pEntry;

   if ((file = gsc_fopen(Path, _T("wb"), MORETESTS, &Unicode)) == NULL) return GS_BAD;

   while (pSection)
   {
      Buffer = _T("[");
      Buffer += pSection->get_name();
      Buffer += _T("]\r\n");

      pEntry = (C_B2STR *) pSection->get_ptr_EntryList()->go_top();
      while (pEntry)
      {
         Buffer += pEntry->get_name();
         Buffer += _T("=");
         Buffer += pEntry->get_name2();
         Buffer += _T("\r\n");

         pEntry = (C_B2STR *) pSection->get_ptr_EntryList()->go_next();
      }

      if (Unicode)
      {
         if (fwrite(Buffer.get_name(), sizeof(TCHAR), Buffer.len(), file) < (unsigned int) Buffer.len())
            { gsc_fclose(file); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
      }
      else
      {
         char *strBuff = gsc_UnicodeToChar(Buffer.get_name());
         if (strBuff)
         {
            if (fwrite(strBuff, sizeof(char), strlen(strBuff), file) < strlen(strBuff))
               { free(strBuff); gsc_fclose(file); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
            free(strBuff);
         }
      }

      pSection = (C_BPROFILE_SECTION *) Sections.go_next();
   }

   return gsc_fclose(file);
}
int gsc_write_profile(C_STRING &Path, C_PROFILE_SECTION_BTREE &Sections, bool Unicode)
   { return gsc_write_profile(Path.get_name(), Sections, Unicode); }


/*********************************************************/
/*.doc gsc_get_profile                                   */
/*+
  Sono due funzioni : una riceve il nome di un file da aprire, l'altra
  riceve il puntatore a FILE gia' aperto.
  Inoltre ricevono come parametri due stringhe contenenti il nome della
  sezione  e della un entry ricercata e un puntatore a stringa (con 
  eventuale dimensione, solo se gia' allocata) la quale se e' NULL viene
  allocata all'interno della procedura.
  In questa stringa sara' caricato il contenuto dell'entry richiesto.
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_get_profile(TCHAR *filename, const TCHAR *sez, const TCHAR *entry,
                    TCHAR **target, size_t dim, long start)
{
   FILE *file;
   bool Unicode;

   if ((file = gsc_fopen(filename, _T("rb"), MORETESTS, &Unicode)) == NULL) 
      return GS_BAD;
   if (gsc_get_profile(file, sez, entry, target, dim, start, Unicode) == GS_BAD)
        { gsc_fclose(file); return GS_BAD; }
   if (gsc_fclose(file) == GS_BAD) return GS_BAD;
   
   return GS_GOOD;
}
int gsc_get_profile(FILE *file, const TCHAR *sez, const TCHAR *entry,
                    TCHAR **target, size_t dim, long start, bool Unicode)
{
   size_t len_profile = 0, len_sez, len_entry;
   int    free_flag = 0;

   if (!file || !sez || !entry) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   len_sez   = wcslen(sez); 
   len_entry = wcslen(entry);

   if (start>3) // Per velocizzare la ricerca
   {            // Comincia la ricerca dall'inizio-linea dopo 'start'
      if (fseek(file,start-3,SEEK_SET)!=0)
             { GS_ERR_COD=eGSReadFile; return GS_BAD; }
      gsc_goto_next_line(file, Unicode);
   }
   else
   {
      if (fseek(file,0,SEEK_SET)!=0)
         { GS_ERR_COD=eGSReadFile; return GS_BAD; }
   }

      // Posiziona il puntatore file all'ENTRY della SEZ desiderata //
   if (gsc_find_sez(file, sez, (int) len_sez, NULL, Unicode) == GS_BAD)
      { GS_ERR_COD = eGSSezNotFound; return GS_BAD; }
   if (gsc_find_entry(file, entry, &len_profile, NULL, Unicode) == GS_BAD)
      { GS_ERR_COD = eGSEntryNotFound; return GS_BAD; }

       // Alloca il buffer TARGET se non e' gia' stato alloccato //
   if (*target==NULL)
   {
      if ((*target = (TCHAR *)malloc((len_profile + 1) * sizeof(TCHAR)))==NULL)
                { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
      free_flag=1;
   }
   else              // Il buffer TARGET non e' sufficiente //
      if (len_profile > dim) { GS_ERR_COD = eGSBuffNotSuf; return GS_BAD; }

    // Legge 'len_profile' caratteri da file
   if (len_profile > 0)
      gsc_fread(*target, len_profile, file, Unicode);

   (*target)[len_profile] = _T('\0');

   if (ferror(file)!=0)   // In caso ERRORE di lettura file //
   {                       // Disalloco TARGET solo se allocato localmente
      if (free_flag) free(target);
      GS_ERR_COD = eGSReadFile;
      return GS_BAD;
   }
   gsc_alltrim(*target);

   // poichè la funzione "GetPrivateProfileString" non legge i doppi apici
   // e i singoli apici iniziali e finali (nel senso che li elimina se 
   // sono il primo e l'ultimo carattere della stringa)
   // sono costretto a eliminare se esistono a inizio e fine stringa un "
   if (*target && len_profile > 4 && 
       ((*target)[0] == _T('\"') || (*target)[0] == _T('\'')) &&
       (*target)[len_profile - 1] == _T('\"') || (*target)[len_profile - 1] == _T('\''))
   {
      C_STRING Buffer((TCHAR *) ((*target) + 1));
      gsc_strcpy((*target), Buffer.get_name(), len_profile);
      (*target)[len_profile - 2] = _T('\0');
   }

   return GS_GOOD;
}
int gsc_get_profile(C_STRING &Path, const TCHAR *sez, const TCHAR *entry, 
                    C_STRING &target)
{
   C_STRING tmp_path(Path);
   TCHAR    Buffer[1024 + 1];

   // Controlla Correttezza Path
   if (gsc_nethost2drive(tmp_path) == GS_BAD) return NULL;

   // la funzione "GetPrivateProfileString" non legge i doppi apici
   // e i singoli apici iniziali e finali (nel senso che li elimina se 
   // sono il primo e l'ultimo carattere della stringa)
   if (GetPrivateProfileString(sez, entry, GS_EMPTYSTR, Buffer, 1024, tmp_path.get_name()) == 0)
   {
      target = GS_EMPTYSTR;
      return GS_BAD;
   }
   else
      target = Buffer;

   return GS_GOOD;
}
int gsc_get_profile(FILE *file, long FileSectionPos, const TCHAR *entry,
                    C_STRING &target, bool Unicode)
{
   size_t len_profile = 0, len_entry;
   TCHAR  *buffer;

   if (!file || !entry) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   len_entry = wcslen(entry);
   fseek(file, FileSectionPos, SEEK_SET);  // Posiziono il cursore sull'inizio della sezione

   // Posiziona il puntatore file all'ENTRY della SEZ desiderata
   if (gsc_find_entry(file, entry, &len_profile, NULL, Unicode) == GS_BAD)
      { GS_ERR_COD = eGSEntryNotFound; return GS_BAD; }

   // Alloca il buffer TARGET se non e' gia' stato allocato
   if ((buffer = (TCHAR *) malloc((len_profile + 1) * sizeof(TCHAR))) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   // Legge 'len_profile' caratteri da file
   gsc_fread(buffer, len_profile, file, Unicode);

   buffer[len_profile] = _T('\0');

   if (ferror(file) != 0)       // In caso ERRORE di lettura file
   {
      free(buffer);
      GS_ERR_COD = eGSReadFile;
      return GS_BAD;
   }
   
   target.paste(buffer);
   target.alltrim();

   // poichè la funzione "GetPrivateProfileString" non legge i doppi apici
   // e i singoli apici iniziali e finali (nel senso che li elimina se 
   // sono il primo e l'ultimo carattere della stringa)
   // sono costretto a eliminare se esistono a inizio e fine stringa un "
   if (target.len() > 4 && 
       (target.get_chr(0) == _T('\"') || target.get_chr(0) == _T('\'')) &&
       (target.get_chr(target.len() - 1) == _T('\"') || target.get_chr(target.len() - 1) == _T('\'')))
      target.removePrefixSuffix(_T("\""), _T("\""));

   return GS_GOOD;
}
int gsc_get_profile(FILE *file, const TCHAR *sez, const TCHAR *entry,
                    C_STRING &target, bool Unicode)
{
   long FileSectionPos;

   if (!file || !sez) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   // Posiziona il puntatore file all'ENTRY della SEZ desiderata
   if (gsc_find_sez(file, sez, wcslen(sez), &FileSectionPos, Unicode) == GS_BAD)
      { GS_ERR_COD = eGSSezNotFound; return GS_BAD; }

   return gsc_get_profile(file, FileSectionPos, entry, target, Unicode);
}
//int gsc_get_profile(FILE *file, const TCHAR *sez, const TCHAR *entry,
//                    C_STRING &target, bool Unicode)
//{
//   int   len_profile = 0, len_sez, len_entry;
//   TCHAR *buffer;
//
//   if (!file || !sez || !entry) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
//
//   len_sez   = wcslen(sez);
//   len_entry = wcslen(entry);
//
//   // Posiziona il puntatore file all'ENTRY della SEZ desiderata
//   if (gsc_find_sez(file, sez, len_sez, NULL, Unicode) == GS_BAD)
//      { GS_ERR_COD = eGSSezNotFound; return GS_BAD; }
//   if (gsc_find_entry(file, entry, &len_profile, NULL, Unicode) == GS_BAD)
//      { GS_ERR_COD = eGSEntryNotFound; return GS_BAD; }
//
//   // Alloca il buffer TARGET se non e' gia' stato allocato
//   if ((buffer = (TCHAR *) malloc((len_profile + 1) * sizeof(TCHAR))) == NULL)
//      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
//
//   // Legge 'len_profile' caratteri da file
//   gsc_fread(buffer, len_profile, file, Unicode);
//
//   buffer[len_profile] = _T('\0');
//
//   if (ferror(file) != 0)       // In caso ERRORE di lettura file
//   {
//      free(buffer);
//      GS_ERR_COD = eGSReadFile;
//      return GS_BAD;
//   }
//   
//   target.paste(buffer);
//   target.alltrim();
//
//   // poichè la funzione "GetPrivateProfileString" non legge i doppi apici
//   // e i singoli apici iniziali e finali (nel senso che li elimina se 
//   // sono il primo e l'ultimo carattere della stringa)
//   // sono costretto a eliminare se esistono a inizio e fine stringa un "
//   if (target.len() > 4 && 
//       (target.get_chr(0) == _T('\"') || target.get_chr(0) == _T('\'')) &&
//       (target.get_chr(target.len() - 1) == _T('\"') || target.get_chr(target.len() - 1) == _T('\'')))
//      target.removePrefixSuffix(_T("\""), _T("\""));
//
//   return GS_GOOD;
//}


/*********************************************************/
/*.doc gsc_get_profile                                   */
/*+
  Questa funzione legge da un file testo tipo profile tutte le entry
  di una sezione specificata. 
  Parametri:
  FILE        *file;        Puntatore a file aperto
  const TCHAR *sez;         Nome sezione
  C_STRING    &EntryList;   Lista delle entry con i valori
  bool        Unicode;      Flag che determina se il contenuto del file è in 
                            formato UNICODE o ANSI (default = false)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_get_profile(FILE *file, const TCHAR *sez, C_2STR_LIST &EntryList,
                    bool Unicode)
{
   C_STRING Buffer, EntryName;
   C_2STR   *pItem;
   TCHAR    *p;
   long     pos;

   EntryList.remove_all();
   if (!file || !sez) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   pos = ftell(file);                 // posizione attuale
   if (fseek(file, 0, SEEK_SET) != 0) // vado a inizio file
      { GS_ERR_COD = eGSReadFile; return GS_BAD; }

   // Posiziona il puntatore file alla sezione desiderata
   if (gsc_find_sez(file, sez, wcslen(sez), NULL, Unicode) == GS_BAD)
      { GS_ERR_COD = eGSSezNotFound; return GS_BAD; }

   while (gsc_readline(file, Buffer, Unicode) == GS_GOOD)
   {
      if (Buffer.get_name() && Buffer.get_chr(0) == _T('[')) break; // Trovata fine sezione
      if ((p = Buffer.at(_T('='))) != NULL)
      {
         EntryName.set_name(Buffer.get_name(), 0, (int) (p - Buffer.get_name() - 1));
         EntryName.alltrim();
         if ((pItem = new C_2STR(EntryName.get_name(), gsc_alltrim(p + 1))) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         EntryList.add_tail(pItem);
      }
   }

   fseek(file, pos, SEEK_SET);  // Ritorna posizione iniziale

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_get_profile                                   */
/*+
  Questa funzione legge da un file testo tipo profile tutte le sezioni.
  Parametri:
  FILE       *file;        Puntatore a file aperto
  C_STR_LIST &SezList;     Lista delle sezioni
  bool        Unicode;      Flag che determina se il contenuto del file è in 
                            formato UNICODE o ANSI (default = false)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_get_profile(FILE *file, C_STR_LIST &SezList, bool Unicode)
{
   C_STRING Buffer, SezName;
   C_STR    *pItem;
   long     pos;

   if (!file) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   pos = ftell(file);                 // posizione attuale
   if (fseek(file, 0, SEEK_SET) != 0) // vado a inizio file
      { GS_ERR_COD = eGSReadFile; return GS_BAD; }

   while (gsc_readline(file, Buffer, Unicode) == GS_GOOD)
   {
      if (Buffer.get_name() && Buffer.get_chr(0) == _T('[') && Buffer.get_chr(Buffer.len() - 1))
      {
         // Trovata inizio sezione
         SezName = Buffer.get_name() + 1;
         SezName.set_chr(_T('\0'), Buffer.len() - 2);
         if ((pItem = new C_STR(SezName.get_name())) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         SezList.add_tail(pItem);
      }
   }

   fseek(file, pos, SEEK_SET);  // Ritorna posizione iniziale

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
// GSC_SET_PROFILE //
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc gsc_CreateEmptyUnicodeFile             <internal> */
/*+
  Funzione che crea un file unicode vuoto inseresndo una intestazione
  di identificazione UNICODE.
  Parametri:
  C_STRING &Path;          Path completo + nome file
  oppure
  const TCHAR *pathfile;	Path completo + nome file

  Restituisce handle di file in caso di successo altrimenti NULL.
-*/  
/*********************************************************/
int gsc_CreateEmptyUnicodeFile(C_STRING &Path)
{
   return gsc_CreateEmptyUnicodeFile(Path.get_name());
}
int gsc_CreateEmptyUnicodeFile(const TCHAR *pathfile)
{
   WORD     wBOM = 0xFEFF; // UTF16-LE BOM(FFFE)
   DWORD    NumberOfBytesWritten;
   C_STRING tmp_path(pathfile);

   // Controlla Correttezza Path
   if (gsc_nethost2drive(tmp_path) == GS_BAD) return GS_BAD;

   HANDLE hFile = ::CreateFile(tmp_path.get_name(), GENERIC_WRITE, 0, 
                               NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
   if (hFile == NULL) return GS_BAD;
   if (::WriteFile(hFile, &wBOM, sizeof(WORD), &NumberOfBytesWritten, NULL) == FALSE)
      { ::CloseHandle(hFile); return GS_BAD; }
   
   return (::CloseHandle(hFile) == TRUE) ? GS_GOOD : GS_BAD;
}


/*********************************************************/
/*.doc gsc_open_profile */
/*+
  Apre il file profile in lettura e scrittura.
  Parametri:
  const TCHAR *pathfile;	Path completo + nome file profile
  int  mode;               Modalità di apertura del file: UPDATEABLE oppure READONLY
                           (default = UPDATEABLE)
  int retest;              se MORETESTS -> in caso di errore riprova n volte 
                           con i tempi di attesa impostati poi ritorna GS_BAD,
                           ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                           (default = MORETESTS)
  bool *Unicode;           Flag di input ed output (determina il formato del file da
                           aprire)

  Restituisce handle di file in caso di successo altrimenti NULL.
-*/  
/*********************************************************/
FILE *gsc_open_profile(C_STRING &pathfile, int mode, int retest, bool *Unicode)
   { return gsc_open_profile(pathfile.get_name(), mode, retest, Unicode); }
FILE *gsc_open_profile(const TCHAR *pathfile, int mode, int retest, bool *Unicode)
{
   FILE     *file;
   C_STRING filename;
                              
   if (!pathfile || wcslen(pathfile) == 0)
      { GS_ERR_COD = eGSInvalidArg; return NULL; }
   filename = pathfile;

   if (gsc_path_exist(filename) == GS_GOOD)
   {
      if (mode == UPDATEABLE)
      {
         if ((file = gsc_fopen(filename, _T("r+b"), retest, Unicode)) == NULL)
            return NULL;
      }
      else
         if ((file = gsc_fopen(filename, _T("rb"), retest, Unicode)) == NULL)
            return NULL;
   }
   else
      if (mode == UPDATEABLE)
      {  
         if ((file = gsc_fopen(filename, _T("w+b"), retest, Unicode)) == NULL) return NULL;
      }
      else
         { GS_ERR_COD = eGSOpenFile; return NULL; }

   return file;
}


/*********************************************************/
/*.doc gsc_close_profile */
/*+
  Chiude il file profile.
  Parametri:
  FILE *file;		file profile
  
  Restituisce GS_GOOD in caso di successo, GS_BAD altrimenti.
-*/  
/*********************************************************/
int gsc_close_profile(FILE *file)
{
   if (gsc_fclose(file) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_set_profile */
/*+
  Sono due funzioni : una riceve il nome di un file da aprire, l'altra
  riceve il puntatore a FILE gia' aperto.
  Inoltre ricevono come parametri tre stringhe contenenti il nome della
  sezione e della un entry da settare ed il contenuto da assegnarvi.
  Se la sezione non esisiste questa viene creata in fondo al file.
  Se l'entry non esiste questo viene inserito in testa alla sezione
  corrispondente.
  Parametri:
  C_STRING    &filename;
  const TCHAR *sez;
  const TCHAR *entry;
  const TCHAR *value;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_set_profile(C_STRING &filename, const TCHAR *sez, const TCHAR *entry,
                    int value, bool Unicode)
{
   C_STRING buff;

   buff = value;
   return gsc_set_profile(filename.get_name(), sez, entry, buff.get_name(), Unicode); 
}
int gsc_set_profile(C_STRING &filename, const TCHAR *sez, const TCHAR *entry,
                    long value, bool Unicode)
{
   C_STRING buff;

   buff = value;
   return gsc_set_profile(filename.get_name(), sez, entry, buff.get_name(), Unicode); 
}
int gsc_set_profile(C_STRING &filename, const TCHAR *sez, const TCHAR *entry,
                    double value, bool Unicode)
{
   C_STRING buff;

   buff = value;
   return gsc_set_profile(filename.get_name(), sez, entry, buff.get_name(), Unicode); 
}
int gsc_set_profile(C_STRING &filename, const TCHAR *sez, const TCHAR *entry,
                    const TCHAR *value, bool Unicode)
   { return gsc_set_profile(filename.get_name(), sez, entry, value, Unicode); }
int gsc_set_profile(TCHAR *filename, const TCHAR *sez, const TCHAR *entry,
                    const TCHAR *value, bool Unicode)
{
   C_STRING tmp_path(filename), Buffer(value);

   // Controlla Correttezza Path
   if (gsc_nethost2drive(tmp_path) == GS_BAD) return GS_BAD;

   // poichè la funzione "GetPrivateProfileString" non legge i doppi apici
   // e i singoli apici iniziali e finali (nel senso che li elimina se 
   // sono il primo e l'ultimo carattere della stringa)
   // sono costretto a inserire a inizio e fine stringa un "
   Buffer.alltrim();
   if (Buffer.len() > 2 &&
       ((Buffer.get_chr(0) == _T('\"') && Buffer.get_chr(Buffer.len() - 1) == _T('\"')) ||
       (Buffer.get_chr(0) == _T('\'') && Buffer.get_chr(Buffer.len() - 1) == _T('\''))))
      Buffer.addPrefixSuffix(_T("\""), _T("\""));

   if (gsc_path_exist(tmp_path) == GS_BAD && Unicode)
      // se non esiste il file WritePrivateProfileString lo crea in ANSI
      // quindi ne creo uno vuoto in UNICODE
      gsc_CreateEmptyUnicodeFile(tmp_path); // Creo file UNICODE

   if (WritePrivateProfileString(sez, entry, Buffer.get_name(), tmp_path.get_name()) == 0)
   {
      DWORD dw = GetLastError();
      #if defined(GSDEBUG) // se versione per debugging
         ads_printf(_T("\nError n.%u on WritePrivateProfileString function"), dw);
      #endif

      return GS_BAD;
   }
   
   return GS_GOOD;
}


int gsc_set_profile(FILE *file, const TCHAR *sez, const TCHAR *entry,
                    const TCHAR *source, long start, bool Unicode)
{
   size_t   len_profile = 0, i, len_sez, len_entry, len_source;
   long     pos_entry, pos_insert;
   C_STRING Buffer;

   // Errori sui parametri in ingresso
   if (!file || !sez || !entry || !source)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   // per compatibilità con "GetPrivateProfileString"
   // poichè la funzione "GetPrivateProfileString" non legge i doppi apici
   // e i singoli apici iniziali e finali (nel senso che li elimina se 
   // sono il primo e l'ultimo carattere della stringa)
   // sono costretto a inserire a inizio e fine stringa un "
   Buffer = source;
   if (Buffer.len() > 2 &&
       ((Buffer.get_chr(0) == _T('\"') && Buffer.get_chr(Buffer.len() - 1) == _T('\"')) ||
       (Buffer.get_chr(0) == _T('\'') && Buffer.get_chr(Buffer.len() - 1) == _T('\''))))
      Buffer.addPrefixSuffix(_T("\""), _T("\""));

   len_sez    = wcslen(sez);
   len_entry  = wcslen(entry);
   len_source = Buffer.len();
                
   // Errori sui parametri in ingresso
   if (len_sez==0 || len_entry==0) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if (start>3)     // Per velocizzare la ricerca //
   {                // Comincia la ricerca dall'inizio-linea dopo 'start' //
      if (fseek(file, start - 3, SEEK_SET) != 0)
             { GS_ERR_COD = eGSReadFile; return GS_BAD; }
      gsc_goto_next_line(file, Unicode);
   }
   else
   {
      if (fseek(file, 0, SEEK_SET) != 0)
         { GS_ERR_COD = eGSReadFile; return GS_BAD; }
   }
   
   // Posiziona il puntatore file all'ENTRY della SEZ desiderata
   if (gsc_find_sez(file, sez, len_sez, NULL, Unicode) == GS_BAD)   // NEW SEZ & ENTRY //
       return gsc_append_profile(file, sez, entry, Buffer.get_name(), len_source, Unicode);
   
   pos_insert=ftell(file);
   
   if (gsc_find_entry(file, entry, &len_profile, &pos_entry, Unicode) == GS_BAD)
   {
      if (feof(file)!=0)      // APPEND NEW ENTRY //
         { return gsc_append_profile(file,NULL,entry,Buffer.get_name(),len_source, Unicode); }
      else                
      {                    // INSERT NEW ENTRY //
         fseek(file,pos_insert,SEEK_SET);
         return gsc_insert_entry(file,entry,Buffer.get_name(),len_source,0, Unicode);
      }
   }

   if (len_source > len_profile) // INSERT DI UNA LINEA PIU' LUNGA
   {
      if (len_profile > 999) { GS_ERR_COD = eGSStringTooLong; return GS_BAD; }

      fseek(file,pos_entry,SEEK_SET);   // Torna ad inizio linea //
      return gsc_insert_entry(file,entry,Buffer.get_name(),len_source,1, Unicode);
   }
   
   // SOVRASCRIVE LA LINEA CORRENTE
   fseek(file,0,SEEK_CUR);
   gsc_fputs(Buffer.get_name(),file, Unicode);
   for (i = len_source; i < len_profile; i++) gsc_fputc(_T(' '), file, Unicode);

   // Cancella lo spazio rimanente
   if (ferror(file)) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   
   return GS_GOOD;
}
int gsc_set_profile(FILE *file, const TCHAR *sez, C_2STR_LIST &EntryList,
                    bool Unicode)
{
   C_2STR *pEntry;

   pEntry = (C_2STR *) EntryList.get_head();
   while (pEntry)
   {
      if (gsc_set_profile(file, sez, pEntry->get_name(), 
                          (pEntry->get_name2()) ? pEntry->get_name2() : GS_EMPTYSTR, 
                          0, Unicode) == GS_BAD)
         return GS_BAD;
      pEntry = (C_2STR *) EntryList.get_next();
   }

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
// GSC_FIND_SEZ //
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc gsc_find_sez */
/*+
  Riceve il puntatore a FILE gia' aperto, stringa contenete nome sezione e
  sua lunghezza.
  Posiziona il puntatore nel file all'inizio della prima entry della 
  sezione desiderata. Nella variabile 'pos' scrivera' la posizione di inizio
  della sezione. 
  Parametri:
  FILE        *file;
  const TCHAR *sez;
  size_t         len;
  long        *pos;
  bool        Unicode;      Flag che determina se il contenuto del file è in 
                            formato UNICODE o ANSI (default = false)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_find_sez(FILE *file, const TCHAR *sez, size_t len, long *pos, bool Unicode)
{
   size_t i;
   int    chOk = GS_BAD, result = GS_GOOD;
   TCHAR  ch;

   if (ferror(file) != 0) return GS_BAD;

   // Posiziono il puntatore all' inizio del file
   if (fseek(file, 0L, SEEK_SET) != 0) return GS_BAD;
   
   do
   {
      ch = gsc_readchar(file, Unicode);

      if (pos != NULL) *pos = ftell(file);      

      if (ch == WEOF) break;
     
      if (ch != _T('[')) continue;
      else
      {
         // Verifico che la sezione trovata sia quella cercata
         for (i = 0; i < len; i++)
         {
            chOk = GS_BAD;

            ch = gsc_readchar(file, Unicode);

            if (ch == WEOF) { result = GS_BAD; break; }
            if (towupper(ch) != towupper(sez[i])) break;
            chOk = GS_GOOD;
         }
         if (result == GS_BAD) break;

         // A questo punto, se i = len posso verificare la chiusa quadra ']'
         if (i == len && chOk == GS_GOOD)
         {
            ch = gsc_readchar(file, Unicode);

            if (ch == _T(']'))
               { gsc_goto_next_line(file, Unicode); return GS_GOOD; }
         }
      }
   }
   while (1);
   
   GS_ERR_COD = eGSSezNotFound;

   return GS_BAD;
}


/*********************************************************/
/*.doc gsc_find_entry */
/*+
  Riceve il puntatore a FILE gia' aperto, stringa contente nome entry e
  sua lunghezza.
  Posiziona il puntatore nel file all'inizio dell'entry desiderato.
  Nella variabile  'pos' (se non e' un puntatore a NULL) memorizzera' la
  posizione dell'inizio riga dell'entry ed in 'val' il numero di BYTE
  nel file riservati all'entry.
  Parametri:
  FILE        *file;          puntatore a file
  const TCHAR *entry;         nome dell'entry da cercare
  size_t      *len_profile;   lunghezza stringa del valore dell'entry (out)
  long        *pos;           posizione all'interno del file (out)
  bool        Unicode;        Flag che determina se il contenuto del file è in 
                              formato UNICODE o ANSI (default = false)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_find_entry(FILE *file, const TCHAR *entry, size_t *len_profile, long *pos,
                   bool Unicode)
{
   int    chOk = GS_BAD;
   size_t i, EntryLen = wcslen(entry);
   TCHAR  result, ch;
   long   start;

   while (feof(file) == 0)
   {
      if (pos) *pos = ftell(file);    // Memorizza inizio linea

      while ((ch = gsc_readchar(file, Unicode)) == _T(' ')) {} // Salta spazi bianchi

      i = 0;
      // Verifico che la entry trovata sia quella cercata
      while (ch != WEOF && ch != _T('=') && ch != _T('[') && i < EntryLen &&
             towupper(ch) == towupper(entry[i]))
      {
         ch = gsc_readchar(file, Unicode);
         i++;
      }

      if (ch == _T('[') || ch == WEOF)
         { GS_ERR_COD = eGSEntryNotFound; return GS_BAD; } // Trovata fine SEZ

      if (i == EntryLen)
      {
         if (ch == _T(' '))
            while ((ch = gsc_readchar(file, Unicode)) == _T(' ')) {} // Salta spazi bianchi

         chOk = (ch == _T('=')) ? GS_GOOD : GS_BAD;

         if (chOk == GS_GOOD) // Se trovata ENTRY giusta
         {
            // Numero di byte non specificato nell'ENTRY
            *len_profile = 0;         // Conta i bytes fino a fine linea
            start = ftell(file);
            while ((result = gsc_readchar(file, Unicode)) != _T('\r') && result != WEOF)
               (*len_profile)++;
            if (fseek(file, start, SEEK_SET) != 0)
               { GS_ERR_COD = eGSReadFile; return GS_BAD; }

            return GS_GOOD;
         }
      }

      gsc_goto_next_line(file, Unicode);
   }

   GS_ERR_COD = eGSEntryNotFound;

   return GS_BAD;
}


//-----------------------------------------------------------------------//
// GSC_APPEND_PROFILE //
//-----------------------------------------------------------------------//

int gsc_append_profile(FILE *file, const TCHAR *sez, const TCHAR *entry,
                       const TCHAR *source, size_t len, bool Unicode)
{
   C_STRING Buff;

   if (len > 990) { GS_ERR_COD = eGSStringTooLong; return GS_BAD; }

   fseek(file, 0, SEEK_END);

   if (sez != NULL)
   {
      Buff = _T("\r\n["); // New line + '[' + SEZ + ']' + New line
      Buff += sez;
      Buff += _T("]\r\n");

      gsc_fputs(Buff, file, Unicode);
   }

   if (entry!=NULL)
   {
      Buff = entry; // ENTRY + '='
      Buff += _T('=');
      if (source!=NULL) Buff += source;
      Buff += _T("\r\n"); // New line

      gsc_fputs(Buff, file, Unicode);
   }
  
   if (ferror(file) != 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
// GSC_INSERT_ENTRY //
//-----------------------------------------------------------------------//

int gsc_insert_entry(FILE *file, const TCHAR *entry, const TCHAR *source, size_t len, 
                     int flag, bool Unicode)
{
   long     pos_profile, pos_start, pos_end, n_byte, nChars;
   C_STRING StrToIns;

   if (!entry) return GS_GOOD;
   if (len>990) { GS_ERR_COD = eGSStringTooLong; return GS_BAD; }

   pos_profile=ftell(file);            // Posizione PROFILE

   if (flag==0)           // INSERISCE NEW PROFILE //
      { pos_start=pos_profile; }
   else                   // RISCRIVE PROFILE CORRENTE //
      { gsc_goto_next_line(file, Unicode); pos_start=ftell(file); }

   fseek(file,0,SEEK_END);
   pos_end = ftell(file);                // Posizione fine file
   fseek(file, pos_start, SEEK_SET);

   n_byte = pos_end - pos_start;           // Num. bytes da shiftare

   StrToIns = entry;                       // ENTRY=
   StrToIns += _T('=');
   if (source != NULL) StrToIns += source; // SOURCE
   StrToIns += _T("\r\n");                 // New line

   if (Unicode)
   {
      TCHAR *buffer;

      nChars = n_byte / sizeof(TCHAR);

      if ((buffer = (TCHAR *) malloc((size_t) nChars * sizeof(TCHAR))) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

      fread(buffer, sizeof(TCHAR), (size_t) nChars, file);  // Legge blocco da shiftare
      fseek(file, pos_profile, SEEK_SET);

      gsc_fputs(StrToIns, file, Unicode);

      fwrite(buffer, sizeof(TCHAR), (size_t) nChars, file); // Riscrive blocco
      free(buffer);
   }
   else
   {
      char *buffer;

      nChars = n_byte / sizeof(char);

      if ((buffer = (char *) malloc((size_t) nChars * sizeof(char))) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

      fread(buffer, sizeof(char), (size_t) nChars, file);  // Legge blocco da shiftare
      fseek(file, pos_profile, SEEK_SET);

      gsc_fputs(StrToIns, file, Unicode);

      fwrite(buffer, sizeof(char), (size_t) nChars, file); // Riscrive blocco
      free(buffer);
   }

   if (ferror(file) != 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_delete_sez */
/*+
  Sono due funzioni : una riceve il nome di un file da aprire, l'altra
  riceve il puntatore a FILE gia' aperto.
  Inoltre ricevono come parametro una stringa contenente il nome della
  sezione da concellare (comprese tutte le sue entry).
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_delete_sez(TCHAR *filename, const TCHAR *sez)
{
   FILE *file;
   bool Unicode;

   if ((file = gsc_fopen(filename, _T("r+b"), MORETESTS, &Unicode)) == NULL)
      return GS_BAD;
   if (gsc_delete_sez(file, sez, Unicode) == GS_BAD)
        { gsc_fclose(file); return GS_BAD; }
   if (gsc_fclose(file) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}

int gsc_delete_sez(FILE *file, const TCHAR *sez, bool Unicode)
{
   int    handle;
   size_t len_sez;
   TCHAR  ch;
   long   pos_start = 0, pos_end, n_byte, pos;
   
   // Errori sui parametri in ingresso
   if (!file || !sez) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   len_sez = wcslen(sez);       // Errori sui parametri in ingresso
   if (len_sez == 0 ) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   
   // Posiziona il puntatore file all'ENTRY della SEZ desiderata
   if (gsc_find_sez(file, sez, len_sez, &pos, Unicode) == GS_BAD)
      { GS_ERR_COD = eGSSezNotFound; return GS_BAD; }
   
   // Trova posizione inizio sezione successiva
   do
   {
      ch = gsc_readchar(file, Unicode);
      if (ch == WEOF) break;
      pos_start = ftell(file);      
      if (ch == _T('[')) break;
   }
   while (1);

   fseek(file, 0, SEEK_END);
   pos_end = ftell(file);            // Posizione fine file //
   
   handle = _fileno(file);           // Riduco dimensione file //

   // codice aggiunto da Caterina CONTROLLARE
   // se non trova la sezione successiva, allora si tratta dell'ultima sezione
   if (ch == WEOF)
   {
	   // ciclo a ritroso finchè trovo un carattere valido, per settare pos
	   pos--;
	   fseek(file, pos, SEEK_SET); 
	   if ((ch = gsc_readchar(file, Unicode)) == _T('['))
		   pos--;
	   fseek(file, pos, SEEK_SET);         // si posiziona un carattere prima 
	      				   				      // dell'inizio della sezione 
	   while (((ch = gsc_readchar(file, Unicode)) == _T(' ')) || (ch == _T('\n')))
	   {
		   pos--;
	      fseek(file, pos, SEEK_SET);    //si posiziona un carattere prima 
	   }
	   _chsize(handle, pos);
   }
   else
   {
      n_byte = pos_end - pos_start;            // Num. bytes da shiftare

      if (Unicode)
      {
         long nChars = n_byte / sizeof(TCHAR);
         TCHAR *buffer;

         if ((buffer = (TCHAR *) malloc((size_t) nChars * sizeof(TCHAR))) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

         fseek(file, pos_start, SEEK_SET);    // Legge blocco da shiftare //
         fread(buffer, sizeof(TCHAR),(size_t) nChars, file); 

         fseek(file, pos, SEEK_SET);  // Ritorna posizione iniziale //
         fwrite(buffer, sizeof(TCHAR),(size_t) nChars, file);     // Riscrive blocco //
         free(buffer);
      }
      else
      {
         long nChars = n_byte / sizeof(char);
         char *buffer;

         if ((buffer = (char *) malloc((size_t) nChars * sizeof(char))) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

         fseek(file, pos_start, SEEK_SET);    // Legge blocco da shiftare //
         fread(buffer, sizeof(char),(size_t) nChars, file); 

         fseek(file, pos, SEEK_SET);  // Ritorna posizione iniziale //
         fwrite(buffer, sizeof(char),(size_t) nChars, file);     // Riscrive blocco //
         free(buffer);
      }

      handle = _fileno(file);           // Riduco dimensione file //
      _chsize(handle, _tell(handle));
   }
   
   if (ferror(file) != 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
// GSC_DELETE_ENTRY  //
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc gsc_delete_entry */
/*+
  Sono due funzioni : una riceve il nome di un file da aprire, l'altra
  riceve il puntatore a FILE gia' aperto.
  Inoltre ricevono come parametri due stringhe contenenti il nome della
  sezione e della un entry da concellare.
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_delete_entry(C_STRING &filename, const TCHAR *sez, const TCHAR *entry)
{
   return gsc_delete_entry(filename.get_name(), sez, entry);
}
int gsc_delete_entry(TCHAR *filename, const TCHAR *sez, const TCHAR *entry)
{
   FILE *file;
   bool Unicode;

   if ((file = gsc_fopen(filename, _T("r+b"), MORETESTS, &Unicode)) == NULL)
      return GS_BAD;
   if (gsc_delete_entry(file, sez, entry, Unicode)==GS_BAD)
      { gsc_fclose(file); return GS_BAD; }
   if (gsc_fclose(file) == GS_BAD) return GS_BAD;
   return GS_GOOD;
}
              
int gsc_delete_entry(FILE *file, const TCHAR *sez, const TCHAR *entry,
                     bool Unicode)
{
   int    handle;
   size_t len, len_sez, len_entry;
   long   pos_start, pos_end, n_byte, pos;
   
   // Errori sui parametri in ingresso //
   if (file == NULL || sez == NULL || entry == NULL )
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   len_sez = wcslen(sez); 
   len_entry = wcslen(entry);
   
   // Errori sui parametri in ingresso //
   if (len_sez == 0 || len_entry == 0)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   
   // Posiziono il cursore all' inizio del file
   if (fseek(file, 0L, SEEK_SET) != 0)
      { GS_ERR_COD = eGSReadFile; return GS_BAD; }

   // Posiziona il puntatore file all'ENRTY della SEZ desiderata
   if (gsc_find_sez(file, sez, len_sez, NULL, Unicode) == GS_BAD)
      { GS_ERR_COD = eGSSezNotFound; return GS_BAD; }
   if (gsc_find_entry(file, entry, &len, &pos, Unicode) == GS_BAD)
      { GS_ERR_COD = eGSEntryNotFound; return GS_BAD; }

   gsc_goto_next_line(file, Unicode);
   pos_start = ftell(file);                // Posizione inizio line successiva

   fseek(file, 0, SEEK_END);
   pos_end = ftell(file);                  // Posizione fine file
   n_byte = pos_end - pos_start;           // Num. bytes da shiftare 
   if (n_byte > 0) // Num. bytes da shiftare
   {
      if (Unicode)
      {
         long nChars = n_byte / sizeof(TCHAR);
         TCHAR *buffer;

         if ((buffer = (TCHAR *) malloc((size_t) nChars * sizeof(TCHAR))) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

         fseek(file, pos_start, SEEK_SET);    // Legge blocco da shiftare
         fread(buffer, sizeof(TCHAR), (size_t) nChars, file); 

         fseek(file, pos, SEEK_SET);  // Ritorna posizione iniziale
         fwrite(buffer, sizeof(TCHAR), (size_t) nChars, file);     // Riscrive blocco 
         free(buffer);
      }
      else
      {
         long nChars = n_byte / sizeof(char);
         char *buffer;

         if ((buffer = (char *) malloc((size_t) nChars * sizeof(char))) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

         fseek(file, pos_start, SEEK_SET);    // Legge blocco da shiftare
         fread(buffer, sizeof(char), (size_t) nChars, file); 

         fseek(file, pos, SEEK_SET);  // Ritorna posizione iniziale
         fwrite(buffer, sizeof(char), (size_t) nChars, file);     // Riscrive blocco 
         free(buffer);
      }
   }
   else
   {
      fseek(file, pos, SEEK_SET);  // Ritorna posizione iniziale

      // Riscrive blocco
      if (Unicode)
         fwrite(GS_EMPTYSTR, sizeof(TCHAR), (size_t) 0, file);
      else
         fwrite("", sizeof(char), (size_t) 0, file);
   }
   
   handle =_fileno(file);           // Riduco dimensione file
   _chsize(handle, _tell(handle));
   
   if (ferror(file) != 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   
   return GS_GOOD;
}


//-----------------------------------------------------------------------//
// GSC_GOTO_NEXT_LINE //
//-----------------------------------------------------------------------//

int gsc_goto_next_line(FILE *file, bool Unicode)
{
   if (Unicode)
   {
      TCHAR ch;
      while ((ch = fgetwc(file)) != _T('\n'))
         if (ch == WEOF) return GS_BAD;
   }
   else
   {
      char ch;
      while ((ch = fgetc(file)) != _T('\n'))
         if (ch == EOF) return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_write_log <internal> */
/*+
  Questa funzione, scrive un messaggio nel file di log di GEOsim.
  Parametri:
  C_STRING &Msg;     Messaggio
  oppure
  const TCHAR *Msg;  Messaggio
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_write_log(C_STRING &Msg)
   { return gsc_write_log(Msg.get_name()); }
int gsc_write_log(const TCHAR *Msg)
{  
   C_STRING PathFile, DateTime;
   FILE     *f;

   // scrivo solo se la variabile globale "logfile" = GS_GOOD
   if (GEOsimAppl::GLOBALVARS.get_LogFile() == GS_BAD) return GS_GOOD;
   
   if (!Msg) return GS_GOOD;

   PathFile = GEOsimAppl::WORKDIR;
   PathFile += _T('\\');
   PathFile += GS_LOG;

   if ((f = gsc_fopen(PathFile, _T("a"))) == NULL) return GS_BAD;
   // scrivo data e ora
   gsc_current_DateTime(DateTime);
   if (fwprintf(f, _T("%s - User n.%d, login <%s> "), DateTime.get_name(),
                GEOsimAppl::GS_USER.code, (TCHAR *) GEOsimAppl::GS_USER.log) < 0)
      { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   if (fwprintf(f, _T("%s\n"), Msg) < 0)
      { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   if (gsc_fclose(f) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_NotValidGraphObj_write_log         <internal> */
/*+
  Questa funzione, scrive un messaggio nel file di log di GEOsim
  relativo ad un oggetto grafico non valido.
  Parametri:
  ads_name ent;     Oggetto grafico
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_get_NotValidGraphObjMsg(AcDbEntity *pObj, C_STRING &Msg)
{
   ads_point point;

   if (gsc_get_firstPoint(pObj, point) == GS_BAD) return GS_BAD;
   Msg = gsc_msg(89); // "Oggetto grafico non valido nel punto "
   Msg += point;
   
   return GS_GOOD;
}
int gsc_get_NotValidGraphObjMsg(ads_name ent, C_STRING &Msg)
{
   ads_point point;

   if (gsc_get_firstPoint(ent, point) == GS_BAD) return GS_BAD;
   Msg = gsc_msg(89); // "Oggetto grafico non valido nel punto "
   Msg += point;
   
   return GS_GOOD;
}
int gsc_NotValidGraphObj_write_log(ads_name ent)
{
   C_STRING Msg;

   if (gsc_get_NotValidGraphObjMsg(ent, Msg) == GS_BAD) return GS_BAD;
   return gsc_write_log(Msg);
}

/*********************************************************/
// PROFILING
/*********************************************************/
double gsc_get_profile_curr_time(void)
{
#if defined(GSPROFILEFILE)
   struct _timeb t;
   _ftime(&t);
   return t.time + (double)(t.millitm)/1000;
#else
   return 0;
#endif
}
int gsc_profile_int_func(double t1, int result, double t2, const char *fName)
{
#if defined(GSPROFILEFILE)
   gsc_write_profiling(fName, (t2 > t1) ? t2 - t1 : t1 -t2);
#endif
   return result;
}

/*********************************************************/
/*.doc gsc_write_profiling                    <internal> */
/*+
  Questa funzione, scrive un messaggio per la profilazione di una funzione.
  Parametri:
  const TCHAR *FunctionName;
  double       Timing;    
-*/  
/*********************************************************/
void gsc_clear_profiling(void)
{  
#if defined(GSPROFILEFILE)
   gsc_delfile(GSPROFILEFILE, ONETEST);
#endif
   return;
}
void gsc_write_profiling(const char *FunctionName, double Timing)
{  
#if defined(GSPROFILEFILE)
   FILE *f;p
   bool UnicodeOpenMode = false;

   if ((f = gsc_fopen(GSPROFILEFILE, _T("a"), ONETEST, &UnicodeOpenMode)) == NULL) return;
   fprintf(f, "%s\t%f\n", FunctionName, Timing);
   gsc_fclose(f);
#endif
   return;
}


/*********************************************************/
/*.doc gsc_strcmp <internal> */
/*+
  Questa funzione, compara due stringhe che possono anche essere NULL.
  Parametri:
  TCHAR *str1;        stringa 1
  TCHAR *str2;        stringa 2
  int sensitive;      sensibile al maiuscolo (default = TRUE)

  Restituisce come strcmp del C standard (un numero < 0 se str1 < str2,
  0 se str1 e ste2 sono uguali, un numero > 0 se str1 > str2). 
-*/  
/*********************************************************/
int gsc_strcmp(const TCHAR *str1, const TCHAR *str2, int sensitive)
{  
   if (sensitive)
   {
      if (str1)
         return (str2) ? wcscmp(str1, str2) : 1;
      else
         return (str2) ? -1 : 0;
   }
   else
   {
      if (str1)
         return (str2) ? wcsicmp(str1, str2) : 1;
      else
         return (str2) ? -1 : 0;
   }

   return 0;
}


/*********************************************************/
/*.doc gsc_strlen <external> */
/*+
  Questa funzione, ritorna la lunghezza di una stringa che può anche essere NULL.
  Parametri:
  const TCHAR *str;         stringa

  Restituisce la lunghezza di una stringa. 
-*/  
/*********************************************************/
inline size_t gsc_strlen(const TCHAR *str)
   { return (!str) ? 0 : wcslen(str); }


/*****************************************************************************/
/*.doc gsc_strcpy <external> */
/*+
  Questa funzione, copia una stringa in un'altra, badando che non venga
  superata la capacità della stringa di destinazione. In questo caso la stringa
  verrà troncata e, comunque verrà garantito il carattere '\0' a fine stringa.
  Parametri:
  TCHAR       *dest;     stringa di destinazione
  const TCHAR *source;   stringa sorgente
  size_t      dim;       dimensione di allocazione della stringa <dest>

  Restituisce la lunghezza di una stringa. 
-*/  
/*****************************************************************************/
inline TCHAR *gsc_strcpy(TCHAR *dest, const TCHAR *source, size_t dim)
{
   if (source)
   {
      wcsncpy(dest, source, dim - 1);
      dest[dim - 1] = _T('\0');
   }
   return dest;
}
inline char *gsc_strcpy(char *dest, const char *source, size_t dim)
{
   if (source)
   {
      strncpy(dest, source, dim - 1);
      dest[dim - 1] = '\0';
   }
   return dest;
}


/*************************************************************************/
/*.doc gsc_getstringmask <external>  */
/*+
  La funzione è analoga ad ads_getstring con la prerogativa di mascherare 
  i caratteri di input mediante un carattere passato, a scelta dell'utente
  (es. '*', '#').Un esempio di uso della funzione può essere la richiesta
  di password.
  Parametri:
  int cronly;                come per ads_getstring
  TCHAR *prompt;             come per ads_getstring
  TCHAR *result;             come per ads_getstring  
  TCHAR mask;                carattere di maschera
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/
/*************************************************************************/
int gsc_getstringmask(int cronly, TCHAR *prompt, TCHAR *result, TCHAR mask)
{
   int passwdchar, Result, Type = 0, contchar = 0;
   struct resbuf passwdlist;

   if (result == NULL) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; } 
   wcscpy(result, GS_EMPTYSTR);
   if (prompt != NULL) acutPrintf(prompt);
  
   do
   {
      Result = acedGrRead(2, &Type, &passwdlist);

      if (Result != RTNORM)
         if (Result == RTCAN) return RTCAN;
         else return RTERROR;

      if (passwdlist.restype != RTSHORT) break;

      passwdchar = passwdlist.resval.rint;

      switch (passwdchar)
      {
         case 8: case 127:                   // tasto backspace , tasto del
            if (contchar>0)
            {
               result[--contchar] = _T('\0');
               acutPrintf(_T("\b \b"));
            }
            break;
         case 13:                            // tasto enter
            return GS_GOOD;
         case 32:                            // tasto blank
            if (!cronly) return GS_GOOD;
         case 999:                           // tasti non definiti
         case -4: case -5: case -6: case -7: // tasti direzionali
            break;
         default:
            if ((contchar + 1) < 132) // ads_getstring accetta al massimo stringhe di 132 caratt.
            {
               result[contchar++] = (TCHAR) passwdchar;
               result[contchar] = _T('\0');
               acutPrintf(_T("*"));
            }
      }
      
   }
   while (TRUE);
  
   return GS_GOOD;
}


//-----------------------------------------------------------------------//
// FUNZIONI STANDARD DA RIDEFINIRE PER COMPATIBILTA' UNIX-DOS  INIZIO    //
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc gsc_strstr */
/*+
  Funzione C strstr che però non va in UNIX
  ritorna il puntatore alla prima occorrenza a sinistra della stringa 
  cercata <str2> nella stringa in cui cercare <str1>.
  Parametri:
  const TCHAR *str1;      stringa in cui cercare
  const TCHAR *str2;      stringa da cercare
  int sensitive;   sensibile al maiuscolo (default = TRUE)

-*/  
/*********************************************************/
TCHAR *gsc_strstr(TCHAR *str1, const TCHAR *str2, int sensitive)
{
   if (!str1 || *str1 == _T('\0') || !str2 || *str2 == _T('\0'))
      return NULL;

   if (sensitive) return wcsstr(str1, str2);
   else
   {
      C_STRING cpy1, cpy2;
      TCHAR    *p;

      cpy1 = str1;
      cpy2 = str2;
      cpy1.toupper();
      cpy2.toupper();

      if ((p = wcsstr(cpy1.get_name(), cpy2.get_name())) == NULL) return NULL;
      p = str1 + (p - cpy1.get_name());

      return wcsstr(p, p);
   }
}


/*********************************************************/
/*.doc gsc_strrstr */
/*+
  Funzione C strstr che però non va in UNIX
  itorna il puntatore alla prima occorrenza a destra della stringa 
  cercata <str2> nella stringa in cui cercare <str1>.
  Parametri:
  const TCHAR *str1;      stringa in cui cercare
  const TCHAR *str2;      stringa da cercare
  int sensitive;   sensibile al maiuscolo (default = TRUE)

-*/  
/*********************************************************/
TCHAR *gsc_strrstr(TCHAR *str1, const TCHAR *str2, int sensitive)
{
   TCHAR *pPrev, *p;

   if (!str1 || *str1 == _T('\0') || !str2 || *str2 == _T('\0'))
      return NULL;

   if (sensitive)
   {
      pPrev = wcsstr(str1, str2);

      if (!pPrev) return NULL;

      while (pPrev[1] != _T('\0') && (p = wcsstr(pPrev + 1, str2)) != NULL)
         pPrev = p;

      return pPrev;
   }
   else
   {
      C_STRING cpy1, cpy2;
      TCHAR    *p;

      cpy1 = str1;
      cpy2 = str2;
      cpy1.toupper();
      cpy2.toupper();

      pPrev = wcsstr(cpy1.get_name(), cpy2.get_name());

      if (!pPrev) return NULL;

      while (pPrev[1] != _T('\0') && (p = wcsstr(pPrev + 1, cpy2.get_name())) != NULL)
         pPrev = p;

      return (pPrev) ? str1 + (pPrev - cpy1.get_name()) : NULL;
   }
}


TCHAR *gsc_itoa(int value, TCHAR *string, int radix)
{
   return _itow(value, string, radix);
}


TCHAR *gsc_ltoa(int value, TCHAR *string, int radix)
{
   return _ltow(value, string, radix);
}


void gsc_makepath(C_STRING &path, const TCHAR *drive, const TCHAR *dir,
                  const TCHAR *file, const TCHAR *ext)
{
   TCHAR buffer[_MAX_PATH];

   _wmakepath(buffer, drive, dir, file, ext);
   path = buffer;
}
void gsc_makepath(TCHAR *path, TCHAR *drive, TCHAR *dir, TCHAR *file, TCHAR *ext)
{
   _wmakepath(path, drive, dir, file, ext);
}


/*********************************************************/
/*.doc gsc_splitpath */
/*+
  Funzione C _splitpath che però non va in UNIX.
  Estrae i componenti di un path completo.
  Parametri:
  const TCHAR *path;
  C_STRING   *drive;
  C_STRING   *dir;
  C_STRING   *file;
  C_STRING   *ext;
-*/  
/*********************************************************/
int gs_splitpath(void)
{
   C_STRING  drive, dir, file, ext;
   presbuf   arg = acedGetArgs();
   C_RB_LIST rblist;

   acedRetNil();

   if (!arg || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   gsc_splitpath(arg->resval.rstring, &drive, &dir, &file, &ext);
   
   if (drive.len() > 0) rblist << acutBuildList(RTSTR, drive.get_name(), 0);
   else rblist << acutBuildList(RTNIL, 0);

   if (dir.len() > 0) rblist += acutBuildList(RTSTR, dir.get_name(), 0);
   else rblist += acutBuildList(RTNIL, 0);

   if (file.len() > 0) rblist += acutBuildList(RTSTR, file.get_name(), 0);
   else rblist << acutBuildList(RTNIL, 0);

   if (ext.len() > 0) rblist += acutBuildList(RTSTR, ext.get_name(), 0);
   else rblist += acutBuildList(RTNIL, 0);

   rblist.LspRetList();

   return RTNORM;
}
void gsc_splitpath(TCHAR *path, TCHAR *drive, TCHAR *dir, TCHAR *file, TCHAR *ext)
   { _wsplitpath(path,drive,dir,file,ext); }
void gsc_splitpath(C_STRING &path, C_STRING *drive, C_STRING *dir,
                   C_STRING *file, C_STRING *ext)
   { gsc_splitpath(path.get_name(), drive, dir, file, ext); }
void gsc_splitpath(const TCHAR *path, C_STRING *drive, C_STRING *dir,
                   C_STRING *file, C_STRING *ext)
{
   TCHAR    *pDrive = NULL, *pDir = NULL, *pFile = NULL, *pExt = NULL;
   C_STRING ConvPath;

   if (!path) return;

   ConvPath = path;
   gsc_nethost2drive(ConvPath, GS_BAD);

   do
   {
      if (drive)
         if ((pDrive = (TCHAR *) malloc(_MAX_DRIVE * sizeof(TCHAR))) == NULL)
            { GS_ERR_COD = eGSOutOfMem; break; }

      if (dir)
         if ((pDir = (TCHAR *) malloc(_MAX_DIR * sizeof(TCHAR))) == NULL)
            { GS_ERR_COD = eGSOutOfMem; break; }

      if (file)
         if ((pFile = (TCHAR *) malloc(_MAX_FNAME * sizeof(TCHAR))) == NULL)
            { GS_ERR_COD = eGSOutOfMem; break; }

      if (ext)
         if ((pExt = (TCHAR *) malloc(_MAX_EXT * sizeof(TCHAR))) == NULL)
            { GS_ERR_COD = eGSOutOfMem; break; }

      _wsplitpath(ConvPath.get_name(), pDrive, pDir, pFile, pExt);

      if (drive) if (drive->set_name(pDrive) == GS_BAD) break;
      if (dir)   if (dir->set_name(pDir) == GS_BAD) break;
      if (file)  if (file->set_name(pFile) == GS_BAD) break;
      if (ext)   if (ext->set_name(pExt) == GS_BAD) break;
   }
   while (0);

   if (pDrive) free(pDrive);
   if (pDir) free(pDir);
   if (pFile) free(pFile);
   if (pExt) free(pExt);
}


/*********************************************************/
/*.doc gsc_fullpath                           <external> */
/*+
  Funzione C _wfullpath che però non va in UNIX.
  Crea un percorso assoluto o completo per il percorso relativo specificato.
  Parametri:
  C_STRING &absPath;    (output) percorso assoluto o completo
  C_STRING &relPath;    (input) percorso relativo
-*/  
/*********************************************************/
TCHAR *gsc_fullpath(C_STRING &absPath, C_STRING &relPath)
{
   TCHAR tmpBuffer[_MAX_PATH];

   if (_wfullpath(tmpBuffer, relPath.get_name(), _MAX_PATH) == NULL) return NULL;
   absPath = tmpBuffer;
   
   return absPath.get_name();
}
TCHAR *gsc_fullpath(TCHAR *buffer, const TCHAR *path, size_t maxlen)
{
   return _wfullpath(buffer, path, maxlen);
}


//-----------------------------------------------------------------------//
// FUNZIONI STANDARD DA RIDEFINIRE PER COMPATIBILTA' UNIX-DOS  FINE      //
//-----------------------------------------------------------------------//


/*************************************************************************/
/*.doc (new 2) gs_getconfirm <internal>  */
/*+
  Funzione chiamata da lisp per conferma generica, in input riceve la stringa 
  e opzionale la risposta di default. 
  Parametri:
  lista resbuf:
   <prompt> [<rispdefault>[<Security>]]
                   
  Restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/
/*************************************************************************/
int gs_getconfirm(void)
{
   presbuf arg = acedGetArgs();
   int     result, rispdefault = GS_BAD, SecurityFlag = GS_BAD;
   TCHAR   *prompt;
   
   acedRetNil();

   if (!arg) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   // primo param prompt da stampare (obbligatorio)
   if (arg->restype != RTSTR) { GS_ERR_COD = eGSInvRBType; return RTERROR; }   
   prompt = arg->resval.rstring;
   
   // leggo secondo param che corrisponde alla risposta di default
   if ((arg = arg->rbnext) != NULL) 
   {
      if (arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
      rispdefault = arg->resval.rint;

      // leggo terzo param che corrisponde alla richiesta di password (opzionale)
      if ((arg = arg->rbnext) != NULL)
      {
         if (arg->restype != RTSHORT) { GS_ERR_COD = eGSInvRBType; return RTERROR; }   
         SecurityFlag = arg->resval.rint;
      }
   }

   // leggo terzo param che corrisponde alla richiesta di password (opzionale)
   if (gsc_getconfirm(prompt, &result, rispdefault, SecurityFlag) == GS_BAD)
      return RTERROR;

   if (result == GS_GOOD) acedRetT();
   
   return RTNORM;
}


/*************************************************************************/
/*.doc (new 2) gsc_getconfirm <internal>  */
/*+
  Funzione utilizzata ogni qual volta si debba chiedere una conferma
  generica, in input riceve la stringa da visualizzare e setta la risposta
  come secondo parametro, il terzo parametro è la risposta
  di default. 
  Parametri:
  const TCHAR *prompt (input) 
  int  *result        (output) GS_GOOD per "SI", GS_BAD per "NO"
  int   rispdefault   (input)  GS_GOOD per "SI", GS_BAD per "NO"
  int SecurityFlag    GS_GOOD = Richiesta di Password utente corrente (solo nel caso di "si")
                      GS_BAD  = senza richieste aggiuntive (default 0)
                   
  Restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/
/*************************************************************************/
int gsc_getconfirm(const TCHAR *prompt, int *result, int rispdefault, int SecurityFlag)
{                                     
   TCHAR    buff[133] = GS_EMPTYSTR;
   C_STRING stringa;
   int      risposta = 0;

   if (!prompt || !result) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (rispdefault != GS_GOOD && rispdefault != GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   *result = rispdefault;
   stringa += prompt;
   stringa += _T(" [");
   if (rispdefault == GS_GOOD)
   {
      stringa += _T('<');
      stringa += gsc_msg(165); // "si"
      stringa += _T(">/");
      stringa += gsc_msg(166); // "no"
   }
   else
   {
      stringa += gsc_msg(165); // "si"
      stringa += _T("/<");
      stringa += gsc_msg(166); // "no"
      stringa += _T('>');
   }
   stringa += _T("]: ");

   if (acedInitGet(6, gsc_msg(164))!= RTNORM)  // "Si No"
      { GS_ERR_COD = eGSUnknown; return GS_BAD; }

   if ((risposta = acedGetKword(stringa.get_name(), buff)) == RTERROR)
      { GS_ERR_COD = eGSUnknown; return GS_BAD; }
   else
   {
      if (risposta == RTCAN) return RTCAN;     // control-break
      if (gsc_strcmp(buff, GS_EMPTYSTR) == 0) return GS_GOOD; // enter
      *result = (gsc_strcmp(buff, gsc_msg(165)) == 0) ? GS_GOOD : GS_BAD; // "si"
   }
   if (SecurityFlag == GS_GOOD && *result == GS_GOOD)
   {
      TCHAR Password[133];

      acutPrintf(GS_LFSTR);
      if (gsc_getstringmask(TRUE, gsc_msg(129), Password, _T('*')) != GS_GOOD) // "Password: "
         { GS_ERR_COD = eGSInvalidPwd; return RTERROR; }
      // verifico che la password sia corretta
      if (gsc_strcmp(Password, (TCHAR *) GEOsimAppl::GS_USER.pwd) != 0) 
         { GS_ERR_COD = eGSInvalidPwd; return RTERROR; }
   }

   return GS_GOOD;
}



/*************************************************************************/
/*.doc (new 2) gsddgetconfirm()  */
/*+
  Chiede conferma con interfaccia DCL.
  Parametri:
  Lista resbuf <msg>[<default>[<Security>]]

  <defaul>     0 = No; 1= Si (default No)
  <Security>   1 = Richiesta di Password utente corrente (solo nel caso di "si");
               0 = senza richieste aggiuntive (default 0)

  Ritorna al LISP T se la risposta è "Si" altrimenti nil (nel caso di conferma
  senza richiesta di password; ritorna la password o nil (nel caso di conferma
  con richiesta password)
-*/
/*************************************************************************/
int gs_ddgetconfirm(void)
{
   presbuf arg = acedGetArgs();
   int     result, rispdefault = GS_BAD, SecurityFlag = GS_BAD;
   TCHAR   *prompt;
   
   acedRetNil();

   if (!arg) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   // primo param prompt da stampare (obbligatorio)
   if (arg->restype != RTSTR) { GS_ERR_COD = eGSInvRBType; return RTERROR; }   
   prompt = arg->resval.rstring;
   
   // leggo secondo param che corrisponde alla risposta di default (opzionale)
   if ((arg = arg->rbnext) != NULL) 
   {
      if (arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
      rispdefault = arg->resval.rint;
      
      // leggo terzo param che corrisponde alla richiesta di password (opzionale)
      if ((arg = arg->rbnext) != NULL)
      {
         if (arg->restype != RTSHORT) { GS_ERR_COD = eGSInvRBType; return RTERROR; }   
         SecurityFlag = arg->resval.rint;
      }
   }
   
   if (gsc_ddgetconfirm(prompt, &result, rispdefault, SecurityFlag) == GS_BAD)
      return RTERROR;

   if (result == GS_GOOD)
      if (SecurityFlag) acedRetStr((TCHAR *) GEOsimAppl::GS_USER.pwd);
      else acedRetT();

   return RTNORM;
}


/*************************************************************************/
/*.doc gsc_ddgetconfirm()                                                */
/*+
  Gestisce l'interfaccia DCL di richiesta conferma dopo una scelta
  Parametri:
  const TCHAR *prompt        (input) 

  int  *result        (output) Caso di TastoNO = GS_BAD:
                                    GS_GOOD per "SI", GS_BAD per "NO" 
                      (output) Caso di TastoNO = GS_GOOD:
                                    GS_GOOD per "SI", GS_BAD per "NO", GS_CAN per "CANCEL" 

  int  rispdefault    (input)  GS_GOOD per "SI", GS_BAD per "NO"
                               (default = GS_BAD)
  int SecurityFlag    GS_GOOD = Richiesta di Password utente corrente (solo nel caso di "si")
                      GS_BAD = senza richieste aggiuntive (default = GS_BAD)
  int TastoNo         se GS_GOOD la DCL avrà 3 tasti: SI-NO-CANCEL altrimenti SI-CANCEL
                      (default = GS_BAD)
  int ScrollBar;      Se = GS_GOOD il messaggio sarà visualizzato in una list-box
                      (previsto per messaggi lunhi; default = GS_BAD), in questo caso
                      la stringa dovrà essere separata da '\n'.

  Restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/
/*************************************************************************/
int gsc_ddgetconfirm(const TCHAR *prompt, int *result, int rispdefault, int SecurityFlag,
                     int TastoNO, int ScrollBar)
{
   C_STRING  path;
   TCHAR     buff[10], etichetta[10];   
   ads_hdlg  dcl_id;
   int       status, numrighe, i, dcl_file, res = GS_BAD;
   size_t    lung;
   presbuf   p;
   C_RB_LIST promptList;

   if (prompt == NULL || result==NULL) { GS_ERR_COD=eGSInvalidArg; return GS_BAD; }
   if (rispdefault != GS_GOOD && rispdefault != GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if (TastoNO == GS_BAD && ScrollBar == GS_BAD && SecurityFlag == GS_BAD)
   {
      UINT uType = MB_YESNO + MB_ICONQUESTION + 
                   ((rispdefault == GS_GOOD) ? MB_DEFBUTTON1 : MB_DEFBUTTON2);
      if (MessageBox(NULL, prompt, _T("GEOsim"), uType) == IDYES)
         *result = GS_GOOD;
      else
         *result = GS_BAD;

      return GS_GOOD;
   }

   lung = wcslen(prompt);

   if (ScrollBar == GS_BAD)
   {
      if (lung > 150) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      if ((p = gsc_splitIntoNumRows(prompt, 50)) == NULL) return GS_BAD; 
      promptList << p;

      if ((numrighe = gsc_length(p)) == -1) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }     
      if (numrighe > 3) numrighe = 3;
   }
   else
   {
      TCHAR    ch;
      C_STRING Row;

      i = 0;
      if ((promptList << acutBuildList(RTLB, 0)) == NULL) return GS_BAD;

      while ((ch = prompt[i++]) != _T('\0'))
         if (ch == _T('\n'))
         {
            if ((promptList += acutBuildList(RTSTR, (Row.get_name()) ? Row.get_name() : GS_EMPTYSTR, 0)) == NULL)
               return GS_BAD;
            Row.clear();
         }
         else Row += ch;

      if (Row.get_name())
         if ((promptList += acutBuildList(RTSTR, Row.get_name(), 0)) == NULL) return GS_BAD;

      if ((promptList += acutBuildList(RTLE, 0)) == NULL) return GS_BAD;
   }

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\") + GS_GENERIC_DCL;
   if (gsc_load_dialog(path, &dcl_file) != RTNORM) return GS_BAD;

   do
   {
      if (SecurityFlag == GS_GOOD)
      {
         if (ScrollBar == GS_GOOD)
         {
            if (ads_new_dialog(_T("GeoConfSecurity"), dcl_file, (CLIENTFUNC)NULLCB, &dcl_id) != RTNORM ||
                dcl_id == NULL)
               { GS_ERR_COD = eGSAbortDCL; break; }
         }
         else
            if (ads_new_dialog(_T("GeoConfSecurity"), dcl_file, (CLIENTFUNC)NULLCB, &dcl_id) != RTNORM ||
                dcl_id == NULL)
               { GS_ERR_COD = eGSAbortDCL; break; }

         if (ads_mode_tile(dcl_id, _T("passw_usr"), MODE_SETFOCUS) != RTNORM)
            { GS_ERR_COD = eGSAbortDCL; break; }
         if (ads_action_tile(dcl_id, _T("accept"), confirmSecurity_accept_ok) != RTNORM)
            { GS_ERR_COD = eGSAbortDCL; break; }
      }
      else
      {
         if (TastoNO == GS_BAD)
         {
            if (ScrollBar == GS_GOOD)
            {
               if (ads_new_dialog(_T("geoconfScrollBar"), dcl_file, (CLIENTFUNC)NULLCB, &dcl_id) != RTNORM ||
                   dcl_id == NULL)
                  { GS_ERR_COD = eGSAbortDCL; break; }
               if (ads_action_tile(dcl_id, _T("horiz_slider"), horiz_slider) != RTNORM ||     // slider orizzontale
                   ads_client_data_tile(dcl_id, _T("horiz_slider"), &promptList) != RTNORM)
                  { GS_ERR_COD = eGSAbortDCL; break; }
            }
            else
               if (ads_new_dialog(_T("geoconf"), dcl_file, (CLIENTFUNC)NULLCB, &dcl_id) != RTNORM ||
                   dcl_id == NULL)
                  { GS_ERR_COD = eGSAbortDCL; break; }

            if (ads_action_tile(dcl_id, _T("accept"), confirm_accept_ok) != RTNORM)
               { GS_ERR_COD = eGSAbortDCL; break; }
            if (ads_action_tile(dcl_id, _T("cancel"), confirm_cancel) != RTNORM)
               { GS_ERR_COD = eGSAbortDCL; break; }
         }
         else
         {
            if (ScrollBar == GS_GOOD)
            {
               if (ads_new_dialog(_T("geoconfScrollBar_withTastoNO"), dcl_file, (CLIENTFUNC)NULLCB, &dcl_id) != RTNORM ||
                   dcl_id == NULL)
                  { GS_ERR_COD = eGSAbortDCL; break; }
            }
            else
               if (ads_new_dialog(_T("geoconf_withTastoNO"), dcl_file, (CLIENTFUNC)NULLCB, &dcl_id) != RTNORM ||
                   dcl_id == NULL)
                  { GS_ERR_COD = eGSAbortDCL; break; }

            if (ads_action_tile(dcl_id, _T("accept"), confirm_accept_ok) != RTNORM)
               { GS_ERR_COD = eGSAbortDCL; break; }
            if (ads_action_tile(dcl_id, _T("no_butt"), confirm_no_button) != RTNORM)
               { GS_ERR_COD = eGSAbortDCL; break; }
            if (ads_action_tile(dcl_id, _T("cancel"), confirm_cancel) != RTNORM)
               { GS_ERR_COD = eGSAbortDCL; break; }
         }

         if (rispdefault == GS_GOOD)
         {
            if (ads_mode_tile(dcl_id, _T("accept"), MODE_SETFOCUS) != RTNORM)
               { GS_ERR_COD = eGSAbortDCL; break; }
         }
         else
            if (ads_mode_tile(dcl_id, _T("cancel"), MODE_SETFOCUS) != RTNORM)
               { GS_ERR_COD = eGSAbortDCL; break; }
      }

      res = GS_GOOD;

      if (ScrollBar == GS_GOOD)
      {
         ads_start_list(dcl_id, _T("conf"), LIST_NEW, 0);
         i = 0;
         while ((p = promptList.nth(i++)) != NULL)
            gsc_add_list(p->resval.rstring);
         ads_end_list();
      }
      else
         for (i = 0; i <= numrighe - 1; i++)
         {
            if ((p = promptList.nth(i)) != NULL)
            {
               swprintf(etichetta, 10, _T("%s%s"), _T("conf"), gsc_itoa(i + 1, buff, 10));
               if (ads_set_tile(dcl_id, etichetta, p->resval.rstring) != RTNORM)
                  { GS_ERR_COD = eGSAbortDCL; res = GS_BAD; break; }
            }
         }
      
      if (res == GS_BAD) break;
      res = GS_BAD;
   
      if (ads_start_dialog(dcl_id, &status) != RTNORM)
         { ads_unload_dialog(dcl_file); GS_ERR_COD = eGSAbortDCL; break; }

      if (TastoNO == GS_BAD)
      {
         *result = (status == DLGOK) ? GS_GOOD : GS_BAD;
      }
      else
      {
         if (status == DLGOK)
            *result = GS_GOOD;
         else if (status == DLGCANCEL)
            *result = GS_CAN;
         else if (status == DLGSTATUS)
            *result = GS_BAD;
      }

      res = GS_GOOD;
   }
   while (0);

   ads_unload_dialog(dcl_file);

   return res;
}


/*************************************************************************/
/*.doc (new 2) redraw_promptList() <internal>                            */
/*+
   Parametri:
   ads_hdlg dcl_id;        handle DCL attiva
   C_RB_LIST *promptList;

   Visualizza nella DCL la lista dei messaggi
-*/
/*************************************************************************/
int redraw_List(ads_hdlg dcl_id, C_RB_LIST *promptList)
{
   presbuf p;
   int     i = 0, n, PrevPos;
   TCHAR   offset[5], *pStr;

   // posizione della riga corrente
   ads_get_tile(dcl_id, _T("conf"), offset, 4);
   PrevPos = _wtoi(offset);
   
   // offset di spostamento dello slider
   ads_get_tile(dcl_id, _T("horiz_slider"), offset, 4);
   n = _wtoi(offset);

   if ((ads_start_list(dcl_id, _T("conf"), LIST_NEW, 0)) != RTNORM)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   while ((p = promptList->nth(i++)) != NULL)
   {
      pStr = p->resval.rstring;
      if (wcslen(pStr) > (unsigned int) n) gsc_add_list(pStr + n);
      else gsc_add_list(GS_EMPTYSTR);
   }
   if (ads_end_list() != RTNORM) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   // riposiziono la riga che era corrente
   ads_set_tile(dcl_id, _T("conf"), _itow(PrevPos, offset, 10));
   
   return GS_GOOD;
}

// ACTION TILE : click su slider orizzontale "horiz_slider" per dialog "geoconf"
static void CALLB horiz_slider(ads_callback_packet *dcl)
{
   redraw_List(dcl->dialog, (C_RB_LIST *) dcl->client_data);
}


// ACTION TILE : click su tasto OK per dialog "geoconf" //
static void CALLB confirm_accept_ok(ads_callback_packet *dcl)
{
   ads_done_dialog(dcl->dialog, DLGOK);
   return;
} 


// ACTION TILE : click su tasto NO per dialog "geoconf" //
static void CALLB confirm_no_button(ads_callback_packet *dcl)
{
   ads_done_dialog(dcl->dialog, DLGSTATUS);
   return;
} 


// ACTION TILE : click su tasto OK per dialog "GeoConfSecurity" //
static void CALLB confirmSecurity_accept_ok(ads_callback_packet *dcl)
{
   TCHAR Password[MAX_LEN_PWD]; 

   if (ads_get_tile(dcl->dialog, _T("passw_usr"), Password, MAX_LEN_PWD) != RTNORM)
      return; 
   // verifico che la password sia corretta
   if (gsc_strcmp(Password, (TCHAR *) GEOsimAppl::GS_USER.pwd) != 0) 
   {  // "*Errore* Utente non riconosciuto"
      ads_set_tile(dcl->dialog, _T("error"), gsc_msg(127));
      return; 
   }
   ads_done_dialog(dcl->dialog, DLGOK);

   return;
} 

// ACTION TILE : click su tasto CANCEL //
static void CALLB confirm_cancel(ads_callback_packet *dcl)
{
   ads_done_dialog(dcl->dialog, DLGCANCEL);
   return;
} 


/*************************************************************************/
/*.doc gsc_ddalert()                                                     */
/*+
   Dialog per la visualizzazione di un messaggio di errore
-*/
/*************************************************************************/
int gs_ddalert(void)
{
   presbuf arg = acedGetArgs();

   acedRetNil();

   if (!arg || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   gsc_ddalert(arg->resval.rstring);

   acedRetT();

   return RTNORM;
}
void gsc_ddalert(TCHAR *prompt, HWND hWnd)
{
   MessageBox(hWnd, prompt, _T("GEOsim"), MB_OK + MB_ICONWARNING);
   return;
}


/*************************************************************************/
/*.doc (new 2) gsc_splitIntoNumRows                                      */
/*+
   Spezza una stringa in tante sottostringhe.
   parametri:
   const TCHAR *stringa;  (input) stringa da spezzare;
         maxriga (input) lunghezza delle sottostringhe;
-*/
/*************************************************************************/
presbuf gsc_splitIntoNumRows(const TCHAR *stringa, int maxriga)
{
   const TCHAR *pStartWord = stringa;
   TCHAR       *Buffer;
   const TCHAR *pEndWord;
   C_RB_LIST   RowList;
   int         nSpaces;
   size_t      iBuff, WordLen;

   if ((Buffer = (TCHAR *) malloc((maxriga + 1) * sizeof(TCHAR))) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   Buffer[0] = _T('\0');

   if ((RowList << acutBuildList(RTLB, 0)) == NULL) 
      { free(Buffer); return NULL; }
      
   while (pStartWord)
   {      
      if ((pEndWord = wcschr(pStartWord, _T(' '))) != NULL) // leggo una parola
         WordLen  = pEndWord - pStartWord;
      else
         WordLen  = wcslen(pStartWord);

      do
      {
         if ((iBuff = wcslen(Buffer)) == 0) nSpaces = 0;

         if ((int) (iBuff + WordLen + nSpaces) <= maxriga) // se sta nella riga
         {
            pStartWord -= nSpaces;
            wcsncpy(Buffer + iBuff, pStartWord, WordLen + nSpaces);
            Buffer[iBuff + WordLen + nSpaces] = _T('\0');
            WordLen = 0;
         }
         else
         { 
            if ((int) WordLen <= maxriga) // se puo' stare nella riga successiva
            {  // Aggiungo la riga alla lista
               if ((RowList += acutBuildList(RTSTR, Buffer, 0)) == NULL) 
                  { free(Buffer); return NULL; }
               // Creo nuova riga
               wcsncpy(Buffer, pStartWord, WordLen);
               Buffer[WordLen] = _T('\0');
               WordLen = 0;
            }
            else // la spezzo 
            {
               // inserisco eventuali spazi
               pStartWord -= nSpaces;
               wcsncpy(Buffer + iBuff, pStartWord, maxriga - iBuff);
               Buffer[maxriga] = _T('\0');
               pStartWord += maxriga - iBuff;
               WordLen    -= maxriga - iBuff - nSpaces;
               // Aggiungo la riga alla lista
               if ((RowList += acutBuildList(RTSTR, Buffer, 0)) == NULL)
                  { free(Buffer); return NULL; }
               Buffer[0] = _T('\0');
            }
         }
      }
      while (WordLen > 0);
      
      nSpaces = 0;
      if (pEndWord)
      {  // vado alla parola successsiva contando gli spazi tra le parole
         while (pEndWord && *pEndWord == _T(' '))
            { pEndWord++; nSpaces++; }
         pStartWord = (*pEndWord == _T('\0')) ? NULL : pEndWord;
      }
      else
         pStartWord = NULL;
   }

   if (wcslen(Buffer) > 0)
      // Aggiungo la riga alla lista
      if ((RowList += acutBuildList(RTSTR, Buffer, 0)) == NULL)
         { free(Buffer); return NULL; }

   free(Buffer);
   RowList.ReleaseAllAtDistruction(GS_BAD);
   return RowList.get_head();
}


/*************************************************************************/
/*.doc (new 2) gsc_ddinput()                                             */
/*+
  Dialog per la richiesta di un valore.
  Parametri:
  const TCHAR *Msg;     Messaggio da visualizzare
  C_STRING    &Out;     Risultato
  const TCHAR *Def;     Valore di default (default = NULL)

  Restituisce GS_BAD in caso di errore, GS_CAN se l'utente ha interrotto 
  l'operazione, GS_GOOD se tutto OK.
-*/
/*************************************************************************/
int gs_ddinput(void)
{
   presbuf  arg = acedGetArgs();
   TCHAR    *Msg = NULL, *Def = NULL;
   C_STRING Value;
   int      res;

   acedRetNil();

   if (arg)
   {  // messaggio
      if (arg->restype == RTSTR) Msg = arg->resval.rstring;
      if ((arg = arg->rbnext) != NULL) // valore di default
         if (arg->restype == RTSTR) Def = arg->resval.rstring;
   }

   res = gsc_ddinput(Msg, Value, Def);
   
   switch (res)
   {
      case GS_CAN: return RTCAN;
      case GS_BAD: return RTERROR;
      case GS_GOOD:
         acedRetStr(Value.get_name());
         return RTNORM;
   }
   
   return RTNORM;
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "input" su controllo "Value"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_input_Value(ads_callback_packet *dcl)
{
   if (dcl->value && wcslen(dcl->value) > 0)
      ads_mode_tile(dcl->dialog, _T("accept"), MODE_SETFOCUS);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "input" su controllo "calendary"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_input_calendary(ads_callback_packet *dcl)
{
   int      *isSQLParam = (int *) dcl->client_data;
   C_STRING result;
   TCHAR    buffer[TILE_STR_LIMIT];

   ads_get_tile(dcl->dialog, _T("Value"), buffer, TILE_STR_LIMIT); 
   result = buffer;
   if (gsc_ddSuggestDBValue(result, adDBTimeStamp) == GS_BAD) return;

   if (*isSQLParam)
   {
      C_STRING dummy;
      if (gsc_getSQLDateTime(result.get_name(), dummy) == GS_BAD) return;
      result = dummy;
   }
   ads_set_tile(dcl->dialog, _T("Value"), result.get_name());
   ads_mode_tile(dcl->dialog, _T("accept"), MODE_SETFOCUS);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "input" su controllo "calc"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_input_calc(ads_callback_packet *dcl)
{
   int      *isSQLParam = (int *) dcl->client_data;
   C_STRING result;
   TCHAR    buffer[TILE_STR_LIMIT];

   ads_get_tile(dcl->dialog, _T("Value"), buffer, TILE_STR_LIMIT); 
   result = buffer;
   if (gsc_ddSuggestDBValue(result, adNumeric) == GS_BAD) return;

   if (*isSQLParam)
   {
      double n;
      if (gsc_conv_Number(result.get_name(), &n) == GS_BAD) return;
      result.paste(gsc_tostring(n));
   }
   ads_set_tile(dcl->dialog, _T("Value"), result.get_name());
   ads_mode_tile(dcl->dialog, _T("accept"), MODE_SETFOCUS);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "input" su controllo "accept"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_input_accept(ads_callback_packet *dcl)
{
   C_STRING *pValue = (C_STRING *) dcl->client_data;
   TCHAR     buffer[TILE_STR_LIMIT];

   ads_get_tile(dcl->dialog, _T("Value"), buffer, TILE_STR_LIMIT); 
   pValue->set_name(buffer);
   ads_done_dialog(dcl->dialog, DLGOK);
}


/*************************************************************************/
/*.doc (new 2) gsc_ddinput()                                             */
/*+
  Dialog per la richiesta di un valore.
  Parametri:
  const TCHAR *Msg;         Messaggio da visualizzare
  C_STRING    &Out;         Risultato
  const TCHAR *Def;         Valore di default (default = NULL)
  int         isSQLParam;   TRUE se richiesto un parametro SQL (cambia la 
                            sintassi del valore ritornato)
  int         WithSuggestionButtons; se TRUE compaiono anche i bottoni di suggerimento
                                     per il calendario e la calcolatrice (default = TRUE)

  Restituisce GS_BAD in caso di errore, GS_CAN se l'utente ha interrotto 
  l'operazione, GS_GOOD se tutto OK.
-*/
/*************************************************************************/
int gsc_ddinput(const TCHAR *Msg, C_STRING &Out, const TCHAR *Def, int isSQLParam, int WithSuggestionButtons)
{
   C_STRING  path;   
   ads_hdlg  dcl_id;
   int       dcl_file, status;

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\") + GS_GENERIC_DCL;
   if (gsc_load_dialog(path, &dcl_file) != RTNORM) return GS_BAD;

   if (ads_new_dialog((WithSuggestionButtons == FALSE) ? _T("input_simplex") : _T("input"), 
                      dcl_file, (CLIENTFUNC)NULLCB, &dcl_id) != RTNORM ||
         dcl_id == NULL)
      { ads_unload_dialog(dcl_file); GS_ERR_COD = eGSAbortDCL; return GS_BAD; }

   if (Msg)
      if (ads_set_tile(dcl_id, _T("Msg"), Msg) != RTNORM)
         { ads_unload_dialog(dcl_file); GS_ERR_COD = eGSAbortDCL; return GS_BAD; }

   if (Def)
      if (ads_set_tile(dcl_id, _T("Value"), Def) != RTNORM)
         { ads_unload_dialog(dcl_file); GS_ERR_COD = eGSAbortDCL; return GS_BAD; }

   ads_action_tile(dcl_id, _T("Value"), dcl_input_Value);

   ads_action_tile(dcl_id, _T("calendary"), dcl_input_calendary);
   ads_client_data_tile(dcl_id, _T("calendary"), &isSQLParam);

   ads_action_tile(dcl_id, _T("calc"), dcl_input_calc);  
   ads_client_data_tile(dcl_id, _T("calc"), &isSQLParam);

   ads_action_tile(dcl_id, _T("accept"), dcl_input_accept);
   ads_client_data_tile(dcl_id, _T("accept"), &Out);

   if (ads_start_dialog(dcl_id, &status) != RTNORM)
      { ads_unload_dialog(dcl_file); GS_ERR_COD = eGSAbortDCL; return GS_BAD; }

   if (status == DLGCANCEL) return GS_CAN;
   
   ads_unload_dialog(dcl_file);

   return GS_GOOD;
}


/*************************************************************************/
/*.doc (new 2) gsc_grd2rad                                               */
/*+
   Conversione da gradi in radianti.
   parametri:
   double grd;         gradi
-*/
/*************************************************************************/
double gsc_grd2rad(double grd) { return grd*PI/180; }


/*************************************************************************/
/*.doc (new 2) gsc_rad2grd                                               */
/*+
   Conversione da radianti in gradi.
   parametri:
   double rad;         radianti
-*/
/*************************************************************************/
double gsc_rad2grd(double rad) { return rad * 180 / PI; }


/*************************************************************************/
/*.doc gsc_gons2rad                                                      */
/*+
   Conversione da gons in radianti.
   parametri:
   double gons;         gons
-*/
/*************************************************************************/
double gsc_gons2rad(double gons) { return gons * (PI / 2) / 100; }


/*************************************************************************/
/*.doc gsc_rad2gons                                                      */
/*+
   Conversione da radianti in gons.
   parametri:
   double rad;         radianti
-*/
/*************************************************************************/
double gsc_rad2gons(double rad) { return (rad * 400) / (PI * 2); }


/*************************************************************************/
/*.doc (new 2) gsc_getRotMeasureUnitsDescription                         */
/*+
   Funzione che restituisce la descrizione del'unità di misura della rotazione.
   Parametri:
   RotationMeasureUnitsEnum rotation_unit;   unità di misura della rotazione

   Restituisce la descrizione se tutto va bene, NULL in caso di errore.
   N.B. Alloca memoria.
-*/                                             
/*************************************************************************/
TCHAR *gsc_getRotMeasureUnitsDescription(RotationMeasureUnitsEnum rotation_unit)
{
   C_STRING Descr;

   switch (rotation_unit)
   {
      case GSNoneRotationUnit:
         return NULL;
      case GSClockwiseDegreeUnit:
         Descr = gsc_msg(219); // "gradi"
         Descr += _T(" ");
         Descr += gsc_msg(233); // "senso orario"
         return Descr.cut();
      case GSCounterClockwiseDegreeUnit:
         Descr = gsc_msg(219); // "gradi"
         Descr += _T(" ");
         Descr += gsc_msg(257); // "senso antiorario"
         return Descr.cut();
      case GSClockwiseRadiantUnit:
         Descr = gsc_msg(221); // "radianti"
         Descr += _T(" ");
         Descr += gsc_msg(233); // "senso orario"
         return Descr.cut();
      case GSCounterClockwiseRadiantUnit:
         Descr = gsc_msg(221); // "radianti"
         Descr += _T(" ");
         Descr += gsc_msg(257); // "senso antiorario"
         return Descr.cut();
      case GSClockwiseGonsUnit:
         Descr = gsc_msg(226); // "gons"
         Descr += _T(" ");
         Descr += gsc_msg(233); // "senso orario"
         return Descr.cut();
      case GSCounterClockwiseGonsUnit: // cambio unità (gons->rad->gradi)
         Descr = gsc_msg(226); // "gons"
         Descr += _T(" ");
         Descr += gsc_msg(257); // "senso antiorario"
         return Descr.cut();
      case GSTopobaseGonsUnit:
         Descr = gsc_msg(226); // "gons"
         Descr += _T(" ");
         Descr += gsc_msg(1042); // "topobase"
         return Descr.cut();
      default :
         return NULL;
   }
}


/*************************************************************************/
/*.doc (new 2) gsc_getColorFormatDescription                             */
/*+
   Funzione che restituisce la descrizione del formato del colore.
   Parametri:
   GSFormatColorEnum color_format;   unità di misura della rotazione

   Restituisce la descrizione se tutto va bene, NULL in caso di errore.
   N.B. Alloca memoria.
-*/                                             
/*************************************************************************/
TCHAR *gsc_getColorFormatDescription(GSFormatColorEnum color_format)
{
   C_STRING Descr;

   switch (color_format)
   {
      case GSNoneFormatColor:
         return NULL;
      case GSAutoCADColorIndexFormatColor:
         Descr = gsc_msg(1035); // "codice autocad ([0-256])"
         return Descr.cut();
      case GSHTMLFormatColor:
         Descr = gsc_msg(1036); // "HTML (es. #FFFFFF)"
         return Descr.cut();
      case GSHexFormatColor:
         Descr = gsc_msg(1037); // "RGB in esadecimale (es. #FFFFFF)"
         return Descr.cut();
      case GSRGBDecColonFormatColor:
         Descr = gsc_msg(1038); // "RGB con virgola separatrice (es. 255,255,255)"
         return Descr.cut();
      case GSRGBDecBlankFormatColor:
         Descr = gsc_msg(1039); // "RGB con spazio separatore (es. 255 255 255)"
         return Descr.cut();
      case GSRGBDXFFormatColor:
         Descr = gsc_msg(1040); // "RGB in decimale (es. 16711680 per rosso)"
         return Descr.cut();
      case GSRGBCOLORREFFormatColor:
         Descr = gsc_msg(1041); // "BRG in decimale (es. 16711680 per blu)"
         return Descr.cut();
      default :
         return NULL;
   }
}


/*************************************************************************/
/*.doc (new 2) gsc_radix2decimal                                         */
/*+
   Funzione che trasforma un numero con radice nota (compreso tra 1 e 36)
   in numero decimale.
   Parametri:
   TCHAR *Source;     numero sorgente
   int   SourceRadix;  base del numero sorgente
   long  *Decimal;     numero decimale

   Restituisce GS_GOOD se tutto va bene, GS_BAD in caso di errore. 
-*/                                             
/*************************************************************************/
int gsc_radix2decimal(TCHAR *Source, int SourceRadix, long *Decimal)
{
   size_t exp;
   char   *SourceChar, *p;

   SourceChar = p = gsc_UnicodeToChar(Source);
   if (!p) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   gsc_toupper(p);

   exp = strlen(p) - 1;
   *Decimal = 0;

   while (*p != _T('\0'))
   {
      if (isdigit(*p))    // se è un numero
         *Decimal = *Decimal + (*p - 48) * (long) pow((double) SourceRadix, (int) exp--);  // "9" = 57(ASCII)-48 = 9
      else
         *Decimal = *Decimal + (*p - 55) * (long) pow((double) SourceRadix, (int) exp--);  // "A" = 65(ASCII)-55 = 10 
      p++;
   } 
   free(SourceChar);

   return GS_GOOD;
} 


/*************************************************************************/
/*.doc (new 2) gsc_decimal2radix                                         */
/*+
   Funzione che trasforma un numero decimale in un numero con radice nota
   (compreso tra 1 e 36).
   Parametri:
   long  Decimal;    numero decimale
   int   DestRadix;  base del numero destinazione
   TCHAR *Dest;      numero destinazione

   Restituisce GS_GOOD se tutto va bene, GS_BAD in caso di errore. 
   N.B. Alloca memoria
-*/                                             
/*************************************************************************/
int gsc_decimal2radix(long Decimal, int DestRadix, TCHAR **Dest)
{
   TCHAR  *str = NULL, dummy;
   int    number = Decimal;
   ldiv_t div;

   if (Decimal < 0) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   *Dest = NULL;

   while (number >= DestRadix)
   {
      div = ldiv(number, DestRadix);
      if (div.rem < 10)
         div.rem = div.rem + 48; // "9" = 57(ASCII)-48 = 9
      else
         div.rem = div.rem + 55; // "A" = 65(ASCII)-55 = 10

      dummy = gsc_CharToUnicode((char) div.rem);
      str = gsc_strcat(str, dummy);

      number = div.quot;
   }

   if (number < 10)
   {
      dummy = gsc_CharToUnicode((char) (number + 48));
      *Dest = gsc_strcat(*Dest, dummy); // "9" = 57(ASCII)-48 = 9
   }
   else
   {
      dummy = gsc_CharToUnicode((char) (number + 55));
      *Dest = gsc_strcat(*Dest, dummy); // "A" = 65(ASCII)-55 = 10                                      
   }

   if (str)
   {
      for (int i = (int) wcslen(str) - 1; i >= 0; i--)
         *Dest = gsc_strcat(*Dest, str[i]);
      free(str);
   }

   return GS_GOOD;        
} 


/*************************************************************************/
/*.doc (new 2) gsc_base362long(TCHAR *number_base36, long *number)        */
/*+
   Funzione che trasforma un numero in base 36 in decimale.
   Parametri:
   TCHAR *number_base36        numero in base 36 in input
   long *number               numero decimale in output

   Restituisce GS_GOOD se tutto va bene, GS_BAD in caso di errore. 
-*/                                             
/*************************************************************************/
int gsc_base362long(TCHAR* number_base36, long* number)
{
   return gsc_radix2decimal(number_base36, 36, number);
} 


/*************************************************************************/
/*.doc (new 2) gsc_get_UnitCoordConvertionFactor                             */
/*+
   Funzione che restituisce il fattore moltiplicativo per la conversione tra unità in caso 
   di successo, 0 in caso di errore, 1 se non serve conversione.
   Parametri:
   GSSRIDUnitEnum Source;  unità di partenza
   GSSRIDUnitEnum Dest;    unità di arrivo
-*/                                             
/*************************************************************************/
double gsc_get_UnitCoordConvertionFactor(GSSRIDUnitEnum Source, GSSRIDUnitEnum Dest)
{
   // se non sono note entrambe le unità
   if (Source == GSSRIDUnit_None || Dest == GSSRIDUnit_None)
      return 0;

   // se le unità sono le stesse
   if (Source == Dest)
      return 1;

   switch (Source)
   {
      case GSSRIDUnit_Meter:
         switch (Dest)
         {
            case GSSRIDUnit_Degree:
               // da metri ad angoli sessadecimali
               // un primo = 1855 metri
               // sessanta primi fanno un grado
               // 1855 * 60 = 111300 metri per un grado
               // se divido i metri per 111300 ottengo il numero di gradi sessadecimali
               return (double) 1.0 / 111300.0;
            case GSSRIDUnit_Mile:
               // da metri a miglia
               return (double) 1.0 / 1609.344;
            case GSSRIDUnit_Inch:
               // da metri a pollici
               return (double) 39.37007874;
            case GSSRIDUnit_Feet:
               // da metri a piedi
               return (double) 3.2808399;
            case GSSRIDUnit_Kilometer:
               // da metri a kilometri
               return (double) 1.0 / 1000.0;
         }
      case GSSRIDUnit_Degree:
         switch (Dest)
         {
            case GSSRIDUnit_Meter:
               // da angoli a metri
               return (double) 111300.0;
            case GSSRIDUnit_Mile:
               // da angoli a miglia
               return (double) 111300.0 / 1609.344;
            case GSSRIDUnit_Inch:
               // da angoli a pollici
               return (double) 111300.0 * 39.37007874;
            case GSSRIDUnit_Feet:
               // da angoli a piedi
               return (double) 111300.0 * 3.2808399;
            case GSSRIDUnit_Kilometer:
               // da angoli a kilometri
               return (double) 111300.0 / 1000.0;
         }
      case GSSRIDUnit_Mile:
         switch (Dest)
         {
            case GSSRIDUnit_Meter:
               // da miglia a metri
               return (double) 1609.344;
            case GSSRIDUnit_Degree:
               // da miglia a angoli
               return (double) 1609.344 / 111300.0;
            case GSSRIDUnit_Inch:
               // da miglia a pollici
               return (double) 1609.344 * 39.37007874;
            case GSSRIDUnit_Feet:
               // da miglia a piedi
               return (double) 1609.344 * 3.2808399;
            case GSSRIDUnit_Kilometer:
               // da miglia a kilometri
               return (double) 1609.344 / 1000.0;
         }
      case GSSRIDUnit_Inch:
         switch (Dest)
         {
            case GSSRIDUnit_Meter:
               // da pollici a metri
               return (double) 1.0 / 39.37007874;
            case GSSRIDUnit_Degree:
               // da pollici a angoli
               return (double) 1.0 / (39.37007874 * 111300.0);
            case GSSRIDUnit_Mile:
               // da pollici a miglia
               return (double) 1.0 / (39.37007874 * 1609.344);
            case GSSRIDUnit_Feet:
               // da pollici a piedi
               return (double) 1.0 / 39.37007874 * 3.2808399;
            case GSSRIDUnit_Kilometer:
               // da pollici a kilometri
               return (double) 1.0 / (39.37007874 * 1000.0);
         }
      case GSSRIDUnit_Feet:
         switch (Dest)
         {
            case GSSRIDUnit_Meter:
               // da piedi a metri
               return (double) 1.0 / 3.2808399;
            case GSSRIDUnit_Degree:
               // da piedi a angoli
               return (double) 1.0 / (3.2808399 * 111300.0);
            case GSSRIDUnit_Mile:
               // da piedi a miglia
               return (double) 1.0 / (3.2808399 * 1609.344);
            case GSSRIDUnit_Inch:
               // da piedi a pollici
               return (double) 1.0 / 3.2808399 * 39.37007874;
            case GSSRIDUnit_Kilometer:
               // da piedi a kilometri
               return (double) 1.0 / (3.2808399 * 1000.0);
         }
      case GSSRIDUnit_Kilometer:
         switch (Dest)
         {
            case GSSRIDUnit_Meter:
               // da kilometri a metri
               return (double) 1000.0;
            case GSSRIDUnit_Degree:
               // da kilometri a angoli
               return (double) 1000.0 / 111300.0;
            case GSSRIDUnit_Mile:
               // da kilometri a miglia
               return (double) 1000.0 / 1609.344;
            case GSSRIDUnit_Inch:
               // da kilometri a pollici
               return (double) 1000.0 * 39.37007874;
            case GSSRIDUnit_Feet:
               // da kilometri a piedi
               return (double) 1000.0 / 3.2808399;
         }
   }

   return (double) 0.0;
}


/*************************************************************************/
/*.doc gsc_round                                                         */
/*+
   Funzione che arrotonda un numero decimale con un numero con n decimali.
   Parametri:
   double n;        numero da arrotondare
   int dec;         numero di decimali con cui arrotondare (se -1 non fa niente)

   Restituisce il numero arrotondato.
-*/                                             
/*************************************************************************/
double gsc_round(double n, int dec)
{
   if (n == 0) return n;
   if (dec == -1) return n; // nessun arrotondamento

   double        sign = fabs(n) / n; // we obtain the sign to calculate positive always
   double        tempval = fabs(n * pow((double)10, (double)dec)); // shift decimal places
   unsigned long tempint = (unsigned long) tempval;
   double        decimalpart = tempval - tempint; // obtain just the decimal part

   if (decimalpart >= 0.5) // next integer number if greater or equal to 0.5
      tempval = ceil(tempval);
   else
      tempval = floor(tempval);//otherwise stay in the current integer part

   return (tempval * pow((double)10, - (double)dec)) * sign; //shift again to the normal decimal places
}


/*************************************************************************/
/*.doc gsc_truncate                                                      */
/*+
   Funzione che tronca un numero decimale con un numero con n decimali.
   Parametri:
   double n;        numero da troncare
   int dec;         numero di decimali a cui troncare (se -1 non fa niente)

   Restituisce il numero arrotondato.
-*/                                             
/*************************************************************************/
double gsc_truncate(double n, int dec)
{
   if (n == 0) return n;
   if (dec == -1) return n; // nessun troncamento

   double        sign = fabs(n) / n; // we obtain the sign to calculate positive always
   double        tempval = fabs(n * pow((double)10, (double)dec)); // shift decimal places
   unsigned long tempint = (unsigned long) tempval;

   return (tempint * pow((double)10, - (double)dec)) * sign; //shift again to the normal decimal places
}


/******************************************************************************/
/*.doc (new 2) gsc_setfilenamecode                                            */
/*+
    Questa funzione, dato un progetto, una classe ed un direttorio completo di path,
    crea un nome di file codificato in base36 (nome successivo all'ultimo 
    esistente nel direttorio, relativo al progetto ed alla classe passati)
    completo di path.
    Parametri:
    int   prj               progetto
    int   cl                classe
    TCHAR *dir              path completo del direttorio 
    TCHAR **filename_base36 path completo codificato per il nuovo file
                   
    Restituisce GS_GOOD se tutto va bene, GS_BAD in caso di errore. 
-*/                                             
/*****************************************************************************/
int gsc_setfilenamecode(int prj, int cl, TCHAR *dir, TCHAR **filename_base36)
{
   int   prjpos  = 2;
   TCHAR *prjstr = NULL;

   if (gsc_long2base36(&prjstr, (long) prj, prjpos) == GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   int   clpos  = 3;
   TCHAR *clstr = NULL;
   if (gsc_long2base36(&clstr, (long) cl, clpos) == GS_BAD)
      { free(prjstr); GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   C_STRING prefix;
   prefix = prjstr;
   prefix += clstr;

   gsc_tempfilebase36(dir, prefix.get_name(), _T(".DWG"), filename_base36);

   free(prjstr);
   free(clstr);

   return GS_GOOD;        
}


/*****************************************************************************/
/*.doc (new 2) gsc_tempfilebase36
/*+
   Questa funzione, dato un direttorio completo di path, un prefisso come 
   nome di file, una estensione per il file, crea un nome di file 
   codificato in base36 (nome successivo all'ultimo esistente nel direttorio,
   relativo al prefisso ed all'estensione) completo di path.
   Se il direttorio non esiste lo crea.
   Parametri:
   const TCHAR *dir_rif       direttorio
   const TCHAR *prefix        prefisso (nome di file)
   TCHAR *ext                 estensione del file
   TCHAR **filetempbase36     path completo codificato per il nuovo file
           
   Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/                                             
/*****************************************************************************/
int gsc_tempfilebase36(const TCHAR *dir_rif, const TCHAR *prefix, TCHAR *ext,
                       TCHAR **filetempbase36)
{
   C_STRING tmpnam, path;
   TCHAR    *unique_word = NULL;
   long     indice = 1;

   if (filetempbase36 == NULL) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   *filetempbase36 = NULL;
   if (prefix == NULL) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (ext == NULL) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   // creo il direttorio
   if (gsc_mkdir(dir_rif) == GS_BAD) return GS_BAD;

   do 
   {  // ciclo di tentativi
      if (gsc_long2base36(&unique_word, indice, 2) == GS_BAD)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

      indice++;

      tmpnam = prefix;
      tmpnam += unique_word;
      tmpnam += ext;

      path = dir_rif;
      path += _T('\\');
      path += tmpnam;

      free(unique_word);
      unique_word=NULL;

   } while (gsc_path_exist(path) == GS_GOOD);

   *filetempbase36 = gsc_strcat(*filetempbase36, path.get_name());

   if (*filetempbase36 == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_conv_Number
/*+
  Questa funzione converte un numero al formato del sistema windows e
  viceversa.
  Se la funzione non "comprende" il formato sorgente ritorna errore.
  Parametri:
  double   NumberSource;   Numero sorgente
  C_STRING &NumberDest;    Risultato
  oppure
  C_STRING &NumberSource;  Numero sorgente
  double   *NumberDest;    Risultato

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/                                             
/******************************************************************************/
int gs_conv_Number(void)
{
   presbuf  arg = acedGetArgs();
   double   Number;
   
   acedRetNil();
   if (!arg) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (arg->restype == RTSTR) // da stringa a numero
   {      
      if (gsc_conv_Number(arg->resval.rstring, &Number) == GS_BAD) return RTERROR;
      acedRetReal(Number);
   }
   else // da numero a stringa
   {
      C_STRING StrNumber;

      if (gsc_rb2Dbl(arg, &Number) == GS_BAD) return RTERROR;

      if ((arg = arg->rbnext) && arg->restype == RTSHORT && arg->resval.rint > 0)
      {
         int len = arg->resval.rint, dec = -1;

         if ((arg = arg->rbnext) && arg->restype == RTSHORT && arg->resval.rint >= 0)
            dec = arg->resval.rint;

         if (gsc_conv_Number(Number, StrNumber, len, dec) == GS_BAD) return RTERROR;
      }
      else
         if (gsc_conv_Number(Number, StrNumber) == GS_BAD) return RTERROR;

      acedRetStr(StrNumber.get_name());
   }

   return RTNORM;
}
int gsc_conv_Number(double NumberSource, C_STRING &NumberDest)
{
   DOUBLE Num = NumberSource;
   BSTR   pbstrOut = NULL;

   if (VarBstrFromR8(Num, LOCALE_USER_DEFAULT, 0, &pbstrOut) != 0)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   NumberDest = (TCHAR*) _bstr_t(pbstrOut);
   if (pbstrOut) SysFreeString(pbstrOut); // CoTaskMemFree(pbstrOut);

   return GS_GOOD;
}
int gsc_conv_Number(double NumberSource, C_STRING &NumberDest, int len, int dec)
{
   DOUBLE Num;
   BSTR   pbstrOut = NULL;
   TCHAR  buf[100], Mask[10];

   if (dec < 0)
      swprintf(Mask, 10, _T("%%%df"), len);
   else
      swprintf(Mask, 10, _T("%%%d.%df"), len, dec);

   swprintf(buf, 100, Mask, NumberSource);
   Num = _wtof(buf);

   if (VarBstrFromR8(Num, LOCALE_USER_DEFAULT, 0, &pbstrOut) != 0)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   NumberDest = (TCHAR*) _bstr_t(pbstrOut);
   if (pbstrOut) SysFreeString(pbstrOut);

   return GS_GOOD;
}
int gsc_conv_Number(const TCHAR *NumberSource, double *NumberDest)
{
   TCHAR  *dummy;
   size_t len;

   if (!NumberSource || !NumberDest) { GS_ERR_COD = eGSInvalidArg; return NULL; }
      
   len = wcslen(NumberSource) + 1;
   if ((dummy = (TCHAR *) malloc(len * sizeof(TCHAR))) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
   wcscpy_s(dummy, len, NumberSource);
   if (VarR8FromStr(dummy, LOCALE_USER_DEFAULT, 0, NumberDest) != 0)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   return GS_GOOD;
}
int gsc_conv_Number(presbuf NumberSource, C_STRING &NumberDest)
{
   switch (NumberSource->restype)
   {
      case RTREAL :
         if (gsc_conv_Number(NumberSource->resval.rreal, NumberDest) == GS_BAD) return GS_BAD;
         break;
      case RTSHORT :
         if (gsc_conv_Number((double)NumberSource->resval.rint, NumberDest) == GS_BAD) return GS_BAD;
         break;
      case RTLONG :
         if (gsc_conv_Number((double)NumberSource->resval.rlong, NumberDest) == GS_BAD) return GS_BAD;
         break;
      case RTSTR :
      {
         double Num;

         if (gsc_conv_Number(NumberSource->resval.rstring, &Num) == GS_BAD) return GS_BAD;
         if (gsc_conv_Number(Num, NumberDest) == GS_BAD) return GS_BAD;
         break;
      }
      case RTT :
         if (gsc_conv_Number(1.0, NumberDest) == GS_BAD) return GS_BAD;
         break;
      case RTNIL :
         if (gsc_conv_Number(0.0, NumberDest) == GS_BAD) return GS_BAD;
         break;
      default :
         return GS_BAD;
   }

   return GS_GOOD;
}
int gsc_conv_Number(presbuf NumberSource, C_STRING &NumberDest, int len, int dec)
{
   switch (NumberSource->restype)
   {
      case RTREAL :
         if (gsc_conv_Number(NumberSource->resval.rreal, NumberDest, len, dec) == GS_BAD) return GS_BAD;
         break;
      case RTSHORT :
         if (gsc_conv_Number((double)NumberSource->resval.rint, NumberDest, len, dec) == GS_BAD) return GS_BAD;
         break;
      case RTLONG :
         if (gsc_conv_Number((double)NumberSource->resval.rlong, NumberDest, len, dec) == GS_BAD) return GS_BAD;
         break;
      case RTSTR :
      {
         double Num;

         if (gsc_conv_Number(NumberSource->resval.rstring, &Num) == GS_BAD) return GS_BAD;
         if (gsc_conv_Number(Num, NumberDest, len, dec) == GS_BAD) return GS_BAD;
         break;
      }
      case RTT :
         if (gsc_conv_Number(1.0, NumberDest, len, dec) == GS_BAD) return GS_BAD;
         break;
      case RTNIL :
         if (gsc_conv_Number(0.0, NumberDest, len, dec) == GS_BAD) return GS_BAD;
         break;
      default :
         return GS_BAD;
   }

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_conv_Currency
/*+
  Questa funzione converte una valuta al formato del sistema windows e
  viceversa.
  Se la funzione non "comprende" il formato sorgente ritorna errore.
  Parametri:
  double   CurrencySource;   Numero sorgente
  C_STRING &CurrencyDest;    Risultato
  oppure
  C_STRING &CurrencySource;  Numero sorgente
  double   *CurrencyDest;    Risultato

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/                                             
/******************************************************************************/
int gs_conv_Currency(void)
{
   presbuf  arg = acedGetArgs();
   double   Currency;
   
   acedRetNil();
   if (!arg) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (arg->restype == RTSTR) // da stringa a numero
   {      
      if (gsc_conv_Currency(arg->resval.rstring, &Currency) == GS_BAD) return RTERROR;
      acedRetReal(Currency);
   }
   else // da numero a stringa
   {
      C_STRING StrCurrency;

      if (gsc_rb2Dbl(arg, &Currency) == GS_BAD) return RTERROR;
      if (gsc_conv_Currency(Currency, StrCurrency) == GS_BAD) return RTERROR;
      acedRetStr(StrCurrency.get_name());
   }

   return RTNORM;
}
int gsc_conv_Currency(double CurrencySource, C_STRING &CurrencyDest)
{
   CY       pcyOut;
   DOUBLE   Curr = CurrencySource;
   BSTR     pbstrOut = NULL;
   C_STRING Prefix;
   static C_STRING MonetarySymbol;
   static C_STRING PositiveMonetaryMode;
   static C_STRING NegativeMonetaryMode;

   if (MonetarySymbol.get_name() == NULL)
      if (gsc_getLocaleMonetarySymbol(MonetarySymbol) == GS_BAD) return GS_BAD;

   if (VarCyFromR8(Curr, &pcyOut) != 0)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   // La funzione non aggiunge il simbolo della valuta
   if (VarBstrFromCy(pcyOut, LOCALE_USER_DEFAULT, 0, &pbstrOut) != 0)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   CurrencyDest = pbstrOut;
   if (pbstrOut) SysFreeString(pbstrOut);

   if (CurrencySource >= 0)
   {
      if (PositiveMonetaryMode.get_name() == NULL)
         if (gsc_getLocalePositiveMonetaryMode(PositiveMonetaryMode) == GS_BAD)
            return GS_BAD;
      
      switch (_wtoi(PositiveMonetaryMode.get_name()))
      {
         case 0: // 0  $1    Prefix, no separation.
            Prefix = MonetarySymbol;
            Prefix += CurrencyDest;
            CurrencyDest = Prefix;
            break;
         case 2: // 2  $ 1   Prefix, 1-character separation.
            Prefix = MonetarySymbol;
            Prefix += _T(' ');
            Prefix += CurrencyDest;
            CurrencyDest = Prefix;
            break;
         case 3: // 3  1 $   Suffix, 1-character separation
            CurrencyDest += _T(' ');
         case 1: // 1  1$    Suffix, no separation.
            CurrencyDest += MonetarySymbol;
            break;
      }
   }  
   else
   {
      if (NegativeMonetaryMode.get_name() == NULL)
         if (gsc_getLocaleNegativeMonetaryMode(NegativeMonetaryMode) == GS_BAD)
            return GS_BAD;

      // anche i prodotti OFFICE se ne sbattono i marroni...
      switch (_wtoi(NegativeMonetaryMode.get_name()))
      {
         case 0: // 0  ($1.1)    Prefix, no separation.
            Prefix = _T('(');
            Prefix += MonetarySymbol;
            Prefix += CurrencyDest.get_name() + 1; // salto il primo carattere = '-'
            Prefix += _T(')');
            CurrencyDest = Prefix;
				break;
         case 1: // 1  -$1.1
            Prefix = _T('-');
            Prefix += MonetarySymbol;
            Prefix += CurrencyDest.get_name() + 1; // salto il primo carattere = '-'
            CurrencyDest = Prefix;
				break;
         case 2: // 2  $-1.1    
            Prefix = MonetarySymbol;
            Prefix += CurrencyDest.get_name();
            CurrencyDest = Prefix;
				break;
         case 3: // 3  $1.1- 
            Prefix = MonetarySymbol;
            Prefix += CurrencyDest.get_name() + 1; // salto il primo carattere = '-'
            Prefix += _T('-');
            CurrencyDest = Prefix;
            break;
         case 4: // 4  (1.1$)    Suffix, no separation.
            Prefix = _T('(');
            Prefix += CurrencyDest.get_name() + 1; // salto il primo carattere = '-'
            Prefix += MonetarySymbol;
            Prefix += _T(')');
            CurrencyDest = Prefix;
				break;
         case 6: // 6  1.1-$ 
            Prefix = CurrencyDest.get_name() + 1; // salto il primo carattere = '-'
            Prefix += _T('-');
            Prefix += MonetarySymbol;
            CurrencyDest = Prefix;
				break;
         case 7: // 7  1.1$- 
            Prefix = CurrencyDest.get_name() + 1; // salto il primo carattere = '-'
            Prefix += MonetarySymbol;
            Prefix += _T('-');
            CurrencyDest = Prefix;
				break;
         case 8: // 8    -1.1 $    Suffix, 1-character separation
            CurrencyDest += _T(' ');
         case 5: // 5  -1.1$
            CurrencyDest += MonetarySymbol;
				break;
         case 9: // 1   -$ 1.1
            Prefix = _T('-');
            Prefix += MonetarySymbol;
            Prefix += _T(' ');
            Prefix += CurrencyDest.get_name() + 1; // salto il primo carattere = '-'
            CurrencyDest = Prefix;
				break;
         case 10: // 10  1.1 $-
            Prefix += CurrencyDest.get_name() + 1; // salto il primo carattere = '-'
            Prefix += _T(' ');
            Prefix += MonetarySymbol;
            Prefix += _T('-');
            CurrencyDest = Prefix;
				break;
         case 11: // 11  $ 1.1-
            Prefix = MonetarySymbol;
            Prefix += _T(' ');
            Prefix += CurrencyDest.get_name() + 1; // salto il primo carattere = '-'
            Prefix += _T('-');
            CurrencyDest = Prefix;
				break;
         case 12: // 12  $ -1.1
            Prefix = MonetarySymbol;
            Prefix += _T(' ');
            Prefix += CurrencyDest;
            CurrencyDest = Prefix;
            break;
         case 14: // 14  ($ 1.1)            
            Prefix = _T('(');
            Prefix += MonetarySymbol;
            Prefix += _T(' ');
            Prefix += CurrencyDest.get_name() + 1; // salto il primo carattere = '-'
            Prefix += _T(')');
            CurrencyDest = Prefix;
            break;
         case 13: // 13  1.1- $
            Prefix = CurrencyDest.get_name() + 1; // salto il primo carattere = '-'
            Prefix += _T("- ");
            Prefix += MonetarySymbol;
            CurrencyDest = Prefix;
            break;
         case 15: // 5  (1.1 $)
            Prefix = _T('(');
            Prefix += CurrencyDest.get_name() + 1; // salto il primo carattere = '-'
            Prefix += _T(' ');
            Prefix += MonetarySymbol;
            Prefix += _T(')');
            CurrencyDest = Prefix;
				break;
      }
   }

   return GS_GOOD;
}
int gsc_conv_Currency(TCHAR *CurrencySource, double *CurrencyDest)
{
   CY     pcyOut;
   DOUBLE pdblOut;

   if (VarCyFromStr(CurrencySource, LOCALE_USER_DEFAULT, 0, &pcyOut) != 0)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (VarR8FromCy(pcyOut, &pdblOut) != 0) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   *CurrencyDest = pdblOut;

   return GS_GOOD;
}
int gsc_conv_Currency(presbuf CurrencySource, C_STRING &CurrencyDest)
{
   switch (CurrencySource->restype)
   {
      case RTREAL :
         if (gsc_conv_Currency(CurrencySource->resval.rreal, CurrencyDest) == GS_BAD) return GS_BAD;
         break;
      case RTSHORT :
         if (gsc_conv_Currency((double)CurrencySource->resval.rint, CurrencyDest) == GS_BAD) return GS_BAD;
         break;
      case RTLONG :
         if (gsc_conv_Currency((double)CurrencySource->resval.rlong, CurrencyDest) == GS_BAD) return GS_BAD;
         break;
      case RTSTR :
      {
         double Num;

         if (gsc_conv_Currency(CurrencySource->resval.rstring, &Num) == GS_BAD) return GS_BAD;
         if (gsc_conv_Currency(Num, CurrencyDest) == GS_BAD) return GS_BAD;
         break;
      }
      case RTT :
         if (gsc_conv_Currency(1.0, CurrencyDest) == GS_BAD) return GS_BAD;
         break;
      case RTNIL :
         if (gsc_conv_Currency(0.0, CurrencyDest) == GS_BAD) return GS_BAD;
         break;
      default :
         return GS_BAD;
   }

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_conv_Bool
/*+
  Questa funzione converte un booleano al formato di GEOsim e
  viceversa.
  Se la funzione non "comprende" il formato sorgente ritorna errore.
  Parametri:
  int BoolSource;          Booleano sorgente
  C_STRING &BoolDest;      Risultato
  oppure
  const TCHAR *BoolSource; Booleano sorgente
  int *BoolDest;           Risultato

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/                                             
/******************************************************************************/
int gs_conv_Bool(void)
{
   presbuf  arg = acedGetArgs();
   C_STRING BoolDest;

   acedRetNil();
   if (!arg) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   switch (arg->restype)
   {
      case RTSTR: // da stringa a T/nil
      {
         int res;

         if (gsc_conv_Bool(arg->resval.rstring, &res) == GS_GOOD)
            if (res == 1) acedRetT();
         break;
      }
      case RTT:   // da T a stringa
         if (gsc_conv_Bool(1, BoolDest) == GS_GOOD) acedRetStr(BoolDest.get_name());
         break;
      case RTNIL: // da nil a stringa
         if (gsc_conv_Bool(0, BoolDest) == GS_GOOD) acedRetStr(BoolDest.get_name());
         break;
      case RTSHORT: // da numero a stringa
         if (gsc_conv_Bool(arg->resval.rint, BoolDest) == GS_GOOD) acedRetStr(BoolDest.get_name());
         break;
      case RTREAL: // da numero a stringa
         if (gsc_conv_Bool(int(arg->resval.rreal), BoolDest) == GS_GOOD) acedRetStr(BoolDest.get_name());
         break;
      default:
         GS_ERR_COD = eGSInvalidArg;
         return RTERROR;
   }

   return RTNORM;
}
int gsc_conv_Bool(int BoolSource, C_STRING &BoolDest)
{
   if (BoolSource == 0) // Falso
      switch (GEOsimAppl::GLOBALVARS.get_BoolFmt())
      {
         case 0: // No
            BoolDest = gsc_msg(775); break;
         case 1: // Falso
            BoolDest = gsc_msg(799); break;
         case 2: // Off
            BoolDest = gsc_msg(679); break;
         case 3: // 0
            BoolDest = _T('0'); break;
      }
   else // Vero
      switch (GEOsimAppl::GLOBALVARS.get_BoolFmt())
      {
         case 0: // Sì
            BoolDest = gsc_msg(774); break;
         case 1: // Vero
            BoolDest = gsc_msg(798); break;
         case 2: // On
            BoolDest = gsc_msg(678); break;
         case 3: // 1
            BoolDest = _T('1'); break;
      }

   return GS_GOOD;
}
int gsc_conv_Bool(TCHAR *BoolSource, int *BoolDest)
{
   double Num;

   if (!BoolSource) *BoolDest = FALSE;
   else
   if (gsc_conv_Number(BoolSource, &Num) == GS_GOOD && Num != 0) // Numero diverso da 0
       *BoolDest = TRUE;
   else
   if (gsc_strcmp(BoolSource, gsc_msg(774), FALSE) == 0) // Sì
       *BoolDest = TRUE;
   else
   if (wcslen(BoolSource) == 1 &&
       towupper(gsc_msg(774)[0]) == towupper(BoolSource[0])) // S
       *BoolDest = TRUE;
   else
   if (gsc_strcmp(BoolSource, gsc_msg(845), FALSE) == 0) // True
       *BoolDest = TRUE;
   else
   if (wcslen(BoolSource) == 1 &&
       towupper(gsc_msg(845)[0]) == towupper(BoolSource[0])) // T
       *BoolDest = TRUE;
   else
   if (gsc_strcmp(BoolSource, gsc_msg(798), FALSE) == 0) // Vero
       *BoolDest = TRUE;
   else
   if (wcslen(BoolSource) == 1 &&
       towupper(gsc_msg(798)[0]) == towupper(BoolSource[0])) // V
       *BoolDest = TRUE;
   else
   if (gsc_strcmp(BoolSource, gsc_msg(678), FALSE) == 0) // On
       *BoolDest = TRUE;
   else
   if (wcslen(BoolSource) == 1 &&
       towupper(gsc_msg(678)[0]) == towupper(BoolSource[0])) // O
       *BoolDest = TRUE;
   else 
       *BoolDest = FALSE;

   return GS_GOOD;
}
int gsc_conv_Bool(presbuf BoolSource, C_STRING &BoolDest)
{
   switch (BoolSource->restype)
   {
      case RTREAL :
         if (gsc_conv_Bool((int)BoolSource->resval.rreal, BoolDest) == GS_BAD) return GS_BAD;
         break;
      case RTSHORT :
         if (gsc_conv_Bool(BoolSource->resval.rint, BoolDest) == GS_BAD) return GS_BAD;
         break;
      case RTLONG :
         if (gsc_conv_Bool((int)BoolSource->resval.rlong, BoolDest) == GS_BAD) return GS_BAD;
         break;
      case RTSTR :
      {
         int Bool;

         if (gsc_conv_Bool(BoolSource->resval.rstring, &Bool) == GS_BAD) return GS_BAD;
         if (gsc_conv_Bool(Bool, BoolDest) == GS_BAD) return GS_BAD;
         break;
      }
      case RTT :
         if (gsc_conv_Bool(1, BoolDest) == GS_BAD) return GS_BAD;
         break;
      case RTNIL :
         if (gsc_conv_Bool(0, BoolDest) == GS_BAD) return GS_BAD;
         break;
      case RTNONE :
         BoolDest = GS_EMPTYSTR;
         break;
      default :
         return GS_BAD;
   }

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_conv_Point
/*+
  Questa funzione converte un punto al formato del sistema windows.
  Parametri:
  ads_point PtSource;   Punto sorgente
  C_STRING &PtDest;     Risultato

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/                                             
/******************************************************************************/
int gsc_conv_Point(ads_point PtSource, C_STRING &PtDest)
{
   C_STRING StrNumber, ListSep;

   if (gsc_getLocaleListSep(ListSep) == GS_BAD) return GS_BAD;

   if (gsc_conv_Number(PtSource[X], StrNumber) == GS_BAD) return GS_BAD;
   PtDest = StrNumber; PtDest += ListSep;
   if (gsc_conv_Number(PtSource[Y], StrNumber) == GS_BAD) return GS_BAD;
   PtDest += StrNumber; PtDest += ListSep;
   if (gsc_conv_Number(PtSource[Z], StrNumber) == GS_BAD) return GS_BAD;
   PtDest += StrNumber;

    return GS_GOOD;
}


/******************************************************************************/
/*.doc gs_conv_DateTime_2_WinFmt
/*+
  Questa funzione converte una data al formato del sistema windows.
  Se la funzione non "comprende" il formato sorgente ritorna errore.
  Parametri:
  const TCHAR *DateTimeSource;    Data e ora sorgente
  C_STRING    &DateTimeDest;      Risultato

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. La funzione non riesce ad interpretare la data se questa è espressa in
       formato esteso con il nome del giorno della settimana.
-*/                                             
/******************************************************************************/
int gs_conv_DateTime_2_WinFmt(void)
{
   presbuf  arg = acedGetArgs();
   C_STRING DateTimeDest;

   acedRetNil();
   if (!arg || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (gsc_conv_DateTime_2_WinFmt(arg->resval.rstring, DateTimeDest) == GS_BAD) return RTERROR;
   acedRetStr(DateTimeDest.get_name());
   
   return RTNORM;
}
int gsc_conv_DateTime_2_WinFmt(const TCHAR *DateTimeSource, C_STRING &DateTimeDest)
{
   COleDateTime dummyOleDateTime;
   CString      dummyStr, Frm;
   int          IsTime, IsDate;

   if (!DateTimeSource) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (wcslen(DateTimeSource) == 0) { DateTimeDest.clear(); return GS_GOOD; }

   if (gsc_ParseDateTime(DateTimeSource, dummyOleDateTime) == GS_BAD) return GS_BAD;
   
   // verifico se <dummyOleDateTime> contiene un'ora
   IsTime = (dummyOleDateTime.GetHour() != 0 || dummyOleDateTime.GetMinute() != 0 ||
             dummyOleDateTime.GetSecond() != 0) ? TRUE : FALSE;

   // verifico se <dummyOleDateTime> contiene una data
   IsDate = (dummyOleDateTime.GetYear() != 1899 || dummyOleDateTime.GetMonth() != 12 ||
             dummyOleDateTime.GetDay() != 30) ? TRUE : FALSE;

   // formattazioni usate:
   // "%X"  = Time representation for current locale
   // "%x"  = Date representation for current locale
   // "%#x" = Long date representation for current locale
   // "%c"  = Date and time representation for current locale
   // "%#c" = Long date and time representation for current locale
   if (IsDate)
      if (IsTime)
         Frm = (GEOsimAppl::GLOBALVARS.get_ShortDate() == GS_GOOD) ? _T("%c") : _T("%#c");
      else
         Frm = (GEOsimAppl::GLOBALVARS.get_ShortDate() == GS_GOOD) ? _T("%x") : _T("%#x");
   else
      if (IsTime) Frm = _T("%X");
      else Frm = GS_EMPTYSTR; // Non era niente...

   try
   {
      dummyStr = dummyOleDateTime.Format(Frm); // può sfondare quindi messa tra try e catch
   }
   catch (...)
   {
      return GS_BAD;
   }

   DateTimeDest = (LPCTSTR) dummyStr;

   return GS_GOOD;
}
int gsc_conv_DateTime_2_WinFmt(presbuf DateTimeSource, C_STRING &DateTimeDest)
{
   if (DateTimeSource->restype != RTSTR || DateTimeSource->resval.rstring == NULL) return GS_BAD;

   return gsc_conv_DateTime_2_WinFmt(DateTimeSource->resval.rstring, DateTimeDest);
}


/******************************************************************************/
/*.doc gs_conv_DateTime_2_WinShortFmt
/*+
  Questa funzione converte una data al formato breve del sistema windows.
  Se la funzione non "comprende" il formato sorgente ritorna errore.
  Parametri:

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. La funzione non riesce ad interpretare la data se questa è espressa in
       formato esteso con il nome del giorno della settimana.
-*/                                             
/******************************************************************************/
int gs_conv_DateTime_2_WinShortFmt(void)
{
   presbuf  arg = acedGetArgs();
   C_STRING DateTimeDest;

   acedRetNil();
   if (!arg || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (gsc_getWinShortDateTime(arg->resval.rstring, DateTimeDest) == GS_BAD) return RTERROR;
   acedRetStr(DateTimeDest.get_name());
   
   return RTNORM;
}
int gsc_getWinShortDateTime(const TCHAR *DateTime, C_STRING &ShortDateTime)
{
   COleDateTime dummyOleDateTime;
   CString      dummyStr, Frm;
   int          IsTime, IsDate;

   if (gsc_ParseDateTime(DateTime, dummyOleDateTime) == GS_BAD) return GS_BAD;

   // verifico se <dummyOleDateTime> contiene un'ora
   IsTime = (dummyOleDateTime.GetHour() != 0 || dummyOleDateTime.GetMinute() != 0 ||
             dummyOleDateTime.GetSecond() != 0) ? TRUE : FALSE;

   // verifico se <dummyOleDateTime> contiene una data
   IsDate = (dummyOleDateTime.GetYear() != 1899 || dummyOleDateTime.GetMonth() != 12 ||
             dummyOleDateTime.GetDay() != 30) ? TRUE : FALSE;

   // "%c"  = Date and time representation for current locale
   // "%x"  = Date representation for current locale
   // "%X"  = Time representation for current locale
   if (IsDate)
      if (IsTime) Frm = _T("%c");
      else Frm = _T("%x");
   else
      if (IsTime) Frm = _T("%X");
      else Frm = GS_EMPTYSTR; // Non era niente...

   try
   {
      dummyStr = dummyOleDateTime.Format(Frm); // può sfondare quindi messa tra try e catch
   }
   catch (...)
   {
      return GS_BAD;
   }

   ShortDateTime = (LPCTSTR) dummyStr;

   return GS_GOOD;
}
int gsc_getWinShortDateTime4DgtYear(const TCHAR *DateTime, C_STRING &ShortDateTime)
{
   COleDateTime dummyOleDateTime;
   CString      dummyStr, Frm;
   int          IsTime, IsDate;

   if (gsc_ParseDateTime(DateTime, dummyOleDateTime) == GS_BAD) return GS_BAD;

   // verifico se <dummyOleDateTime> contiene un'ora
   IsTime = (dummyOleDateTime.GetHour() != 0 || dummyOleDateTime.GetMinute() != 0 ||
             dummyOleDateTime.GetSecond() != 0) ? TRUE : FALSE;

   // verifico se <dummyOleDateTime> contiene una data
   IsDate = (dummyOleDateTime.GetYear() != 1899 || dummyOleDateTime.GetMonth() != 12 ||
             dummyOleDateTime.GetDay() != 30) ? TRUE : FALSE;

   // "%X"  = Time representation for current locale
   if (IsDate)
   {
      C_STRING Fmt4DgtYear;

      if (gsc_getLocaleShortDateFormat4DgtYear(Fmt4DgtYear) == GS_BAD)
         return GS_BAD;

      // giorno
      if (Fmt4DgtYear.at(_T("dddddd"), FALSE)) Fmt4DgtYear.strtran(_T("dddddd"), _T("%d"), FALSE);
      else if (Fmt4DgtYear.at(_T("ddddd"), FALSE)) Fmt4DgtYear.strtran(_T("ddddd"), _T("%d"), FALSE);
      else if (Fmt4DgtYear.at(_T("dddd"), FALSE)) Fmt4DgtYear.strtran(_T("dddd"), _T("%d"), FALSE);
      else if (Fmt4DgtYear.at(_T("ddd"), FALSE)) Fmt4DgtYear.strtran(_T("ddd"), _T("%d"), FALSE);
      else if (Fmt4DgtYear.at(_T("dd"), FALSE)) Fmt4DgtYear.strtran(_T("dd"), _T("%d"), FALSE);
      else if (Fmt4DgtYear.at(_T("d"), FALSE)) Fmt4DgtYear.strtran(_T("d"), _T("%d"), FALSE);
      // mese
      if (Fmt4DgtYear.at(_T("mmmm"), FALSE)) Fmt4DgtYear.strtran(_T("mmmm"), _T("%m"), FALSE);
      else if (Fmt4DgtYear.at(_T("mmm"), FALSE)) Fmt4DgtYear.strtran(_T("mmm"), _T("%m"), FALSE);
      else if (Fmt4DgtYear.at(_T("mm"), FALSE)) Fmt4DgtYear.strtran(_T("mm"), _T("%m"), FALSE);
      else if (Fmt4DgtYear.at(_T("m"), FALSE)) Fmt4DgtYear.strtran(_T("m"), _T("%m"), FALSE);
      // anno
      if (Fmt4DgtYear.at(_T("yyyy"), FALSE)) Fmt4DgtYear.strtran(_T("yyyy"), _T("%Y"), FALSE);
      else if (Fmt4DgtYear.at(_T("yyy"), FALSE)) Fmt4DgtYear.strtran(_T("yyy"), _T("%Y"), FALSE);
      else if (Fmt4DgtYear.at(_T("yy"), FALSE)) Fmt4DgtYear.strtran(_T("yy"), _T("%Y"), FALSE);

      Frm = Fmt4DgtYear.get_name();
      if (IsTime) Frm += _T(" %X");
   }
   else
      if (IsTime) Frm = _T("%X");
      else Frm = GS_EMPTYSTR; // Non era niente...

   try
   {
      dummyStr = dummyOleDateTime.Format(Frm); // può sfondare quindi messa tra try e catch
   }
   catch (...)
   {
      return GS_BAD;
   }

   ShortDateTime = (LPCTSTR) dummyStr;

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gs_convTime_2_WinFmt
/*+
  Questa funzione converte una ora al formato del sistema windows.
  Se la funzione non "comprende" il formato sorgente ritorna errore.
  Parametri:
  const TCHAR *TimeSource;    Ora sorgente
  C_STRING    &TimeDest;      Risultato

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/                                             
/******************************************************************************/
int gs_conv_Time_2_WinFmt(void)
{
   presbuf  arg = acedGetArgs();
   C_STRING TimeDest;

   acedRetNil();
   if (!arg || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (gsc_conv_Time_2_WinFmt(arg->resval.rstring, TimeDest) == GS_BAD) return RTERROR;
   acedRetStr(TimeDest.get_name());
   
   return RTNORM;
}
int gsc_conv_Time_2_WinFmt(const TCHAR *TimeSource, C_STRING &TimeDest)
{
   COleDateTime dummyOleDateTime;
   CString      dummyStr;

   if (!TimeSource) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (wcslen(TimeSource) == 0) { TimeDest.clear(); return GS_GOOD; }

   if (gsc_ParseTime(TimeSource, dummyOleDateTime) == GS_BAD) return GS_BAD;
   
   try
   {
      // "%X"  = Time representation for current locale
      dummyStr = dummyOleDateTime.Format(_T("%X")); // può sfondare quindi messa tra try e catch
   }
   catch (...)
   {
      return GS_BAD;
   }

   TimeDest = (LPCTSTR) dummyStr;

   return GS_GOOD;
}
int gsc_conv_Time_2_WinFmt(presbuf TimeSource, C_STRING &TimeDest)
{
   if (TimeSource->restype != RTSTR || TimeSource->resval.rstring == NULL) return GS_BAD;

   return gsc_conv_Time_2_WinFmt(TimeSource->resval.rstring, TimeDest);
}


/******************************************************************************/
/*.doc gsc_getGEOsimDateTime
/*+
  Questa funzione converte una data-ora al formato ANSI SQL.
  Se la funzione non "comprende" il formato sorgente ritorna errore.
  Parametri:
  const TCHAR *DateTime;         Data-ora sorgente
  C_STRING    &GEOsimDateTime;   Risultato

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/                                             
/******************************************************************************/
int gsc_getGEOsimDateTime(const TCHAR *DateTime, C_STRING &GEOsimDateTime)
{
   COleDateTime dummyOleDateTime;
   int          IsTime, IsDate;

   if (gsc_ParseDateTime(DateTime, dummyOleDateTime) == GS_BAD) return GS_BAD;

   // verifico se <dummyOleDateTime> contiene un'ora
   IsTime = (dummyOleDateTime.GetHour() != 0 || dummyOleDateTime.GetMinute() != 0 ||
             dummyOleDateTime.GetSecond() != 0) ? TRUE : FALSE;

   // verifico se <dummyOleDateTime> contiene una data
   IsDate = (dummyOleDateTime.GetYear() != 1899 || dummyOleDateTime.GetMonth() != 12 ||
             dummyOleDateTime.GetDay() != 31) ? TRUE : FALSE;

   if (IsDate)
   {
      GEOsimDateTime = dummyOleDateTime.GetDay();
      GEOsimDateTime += _T('/');
      GEOsimDateTime += dummyOleDateTime.GetMonth();
      GEOsimDateTime += _T('/');
      GEOsimDateTime += dummyOleDateTime.GetYear();

      if (IsTime)
      {
         GEOsimDateTime += _T(' ');
         GEOsimDateTime += dummyOleDateTime.GetHour();
         GEOsimDateTime += _T(':');
         GEOsimDateTime += dummyOleDateTime.GetMinute();
         GEOsimDateTime += _T(':');
         GEOsimDateTime += dummyOleDateTime.GetSecond();
      }
   }
   else
      if (IsTime)
      {
         GEOsimDateTime = dummyOleDateTime.GetHour();
         GEOsimDateTime += _T(':');
         GEOsimDateTime += dummyOleDateTime.GetMinute();
         GEOsimDateTime += _T(':');
         GEOsimDateTime += dummyOleDateTime.GetSecond();
      }
      else 
         GEOsimDateTime = GS_EMPTYSTR;

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_getYearDateTime
/*+
  Questa funzione estra l'anno come numero esteso da una stringa contenente una data.
  Se la funzione non "comprende" il formato sorgente ritorna errore.
  Parametri:
  const TCHAR *DateTime;    Data in forma stringa

  Restituisce un numero in caso di successo altrimenti restituisce -1.
  N.B. La funzione non riesce ad interpretare la data se questa è espressa in
       formato esteso con il nome del giorno della settimana.
-*/                                             
/******************************************************************************/
int gsc_getYearDateTime(const TCHAR *DateTime)
{
   COleDateTime dummyOleDateTime;
   int          Year;
 
   if (gsc_ParseDateTime(DateTime, dummyOleDateTime) == GS_BAD) return GS_BAD;
   return ((Year = dummyOleDateTime.GetYear()) == COleDateTime::error) ? -1 : Year;
}


/******************************************************************************/
/*.doc gsc_getMonthDateTime
/*+
  Questa funzione estra il mese da una stringa contenente una data.
  Se la funzione non "comprende" il formato sorgente ritorna errore.
  Parametri:
  const TCHAR *DateTime;    Data in forma stringa

  Restituisce un numero in caso di successo altrimenti restituisce -1.
  N.B. La funzione non riesce ad interpretare la data se questa è espressa in
       formato esteso con il nome del giorno della settimana.
-*/                                             
/******************************************************************************/
int gsc_getMonthDateTime(const TCHAR *DateTime)
{
   COleDateTime dummyOleDateTime;
   int          Month;
 
   if (gsc_ParseDateTime(DateTime, dummyOleDateTime) == GS_BAD) return GS_BAD;
   return ((Month = dummyOleDateTime.GetMonth()) == COleDateTime::error) ? -1 : Month;
}


/******************************************************************************/
/*.doc gsc_getDayDateTime
/*+
  Questa funzione estra il giorno da una stringa contenente una data.
  Se la funzione non "comprende" il formato sorgente ritorna errore.
  Parametri:
  const TCHAR *DateTime;    Data in forma stringa

  Restituisce un numero in caso di successo altrimenti restituisce -1.
  N.B. La funzione non riesce ad interpretare la data se questa è espressa in
       formato esteso con il nome del giorno della settimana.
-*/                                             
/******************************************************************************/
int gsc_getDayDateTime(const TCHAR *DateTime)
{
   COleDateTime dummyOleDateTime;
   int          Day;
 
   if (gsc_ParseDateTime(DateTime, dummyOleDateTime) == GS_BAD) return GS_BAD;
   return ((Day = dummyOleDateTime.GetDay()) == COleDateTime::error) ? -1 : Day;
}


/******************************************************************************/
/*.doc gsc_getSQLDateTime
/*+
  Questa funzione converte una data\ora nel formato SQL ISO 8601 usato anche da
  AutoCAD MAP (yyyy-mm-dd hh:mm:ss).
  Parametri:
  const TCHAR *DateTime;       Data e ora originale
  C_STRING &MAPSQLDateTime;   Data e ora nel formato SQL di MAP

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. La funzione non riesce ad interpretare la data se questa è espressa in
       formato esteso con il nome del giorno della settimana.
-*/                                             
/******************************************************************************/
int gsc_getSQLDateTime(const TCHAR *DateTime, C_STRING &MAPSQLDateTime)
{
   COleDateTime dummyOleDateTime;
   C_STRING     dummyStr;
   size_t       i;

   if (gsc_ParseDateTime(DateTime, dummyOleDateTime) == GS_BAD) return GS_BAD;

   MAPSQLDateTime.clear();
   dummyStr = dummyOleDateTime.GetYear();
   for (i = 0; i < (4 - dummyStr.len()); i++) MAPSQLDateTime += _T('0');
   MAPSQLDateTime += dummyOleDateTime.GetYear();
   MAPSQLDateTime += _T('-');

   dummyStr = dummyOleDateTime.GetMonth();
   for (i = 0; i < (2 - dummyStr.len()); i++) MAPSQLDateTime += _T('0');
   MAPSQLDateTime += dummyOleDateTime.GetMonth();
   MAPSQLDateTime += _T('-');

   dummyStr = dummyOleDateTime.GetDay();
   for (i = 0; i < (2 - dummyStr.len()); i++) MAPSQLDateTime += _T('0');
   MAPSQLDateTime += dummyOleDateTime.GetDay();
   MAPSQLDateTime += _T(' ');

   dummyStr = dummyOleDateTime.GetHour();
   for (i = 0; i < (2 - dummyStr.len()); i++) MAPSQLDateTime += _T('0');
   MAPSQLDateTime += dummyOleDateTime.GetHour();
   MAPSQLDateTime += _T(':');

   dummyStr = dummyOleDateTime.GetMinute();
   for (i = 0; i < (2 - dummyStr.len()); i++) MAPSQLDateTime += _T('0');
   MAPSQLDateTime += dummyOleDateTime.GetMinute();
   MAPSQLDateTime += _T(':');

   dummyStr = dummyOleDateTime.GetSecond();
   for (i = 0; i < (2 - dummyStr.len()); i++) MAPSQLDateTime += _T('0');
   MAPSQLDateTime += dummyOleDateTime.GetSecond();

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_getSQLDate
/*+
  Questa funzione converte una data nel formato SQL ISO 8601 usato anche da
  AutoCAD MAP (yyyy-mm-dd).
  Parametri:
  const TCHAR *Date;      Data originale
  C_STRING &MAPSQLDate;   Data nel formato SQL di MAP

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. La funzione non riesce ad interpretare la data se questa è espressa in
       formato esteso con il nome del giorno della settimana.
-*/                                             
/******************************************************************************/
int gsc_getSQLDate(const TCHAR *Date, C_STRING &MAPSQLDate)
{
   COleDateTime dummyOleDate;
   C_STRING     dummyStr;
   size_t       i;

   if (gsc_ParseDateTime(Date, dummyOleDate) == GS_BAD) return GS_BAD;

   MAPSQLDate.clear();
   dummyStr = dummyOleDate.GetYear();
   for (i = 0; i < (4 - dummyStr.len()); i++) MAPSQLDate += _T('0');
   MAPSQLDate += dummyOleDate.GetYear();
   MAPSQLDate += _T('-');

   dummyStr = dummyOleDate.GetMonth();
   for (i = 0; i < (2 - dummyStr.len()); i++) MAPSQLDate += _T('0');
   MAPSQLDate += dummyOleDate.GetMonth();
   MAPSQLDate += _T('-');

   dummyStr = dummyOleDate.GetDay();
   for (i = 0; i < (2 - dummyStr.len()); i++) MAPSQLDate += _T('0');
   MAPSQLDate += dummyOleDate.GetDay();

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_getSQLTime
/*+
  Questa funzione converte una ora nel formato SQL ISO 8601 usato anche da
  AutoCAD MAP (hh:mm:ss).
  Parametri:
  const TCHAR *Time;      Ora originale
  C_STRING &MAPSQLTime;   Ora nel formato SQL di MAP

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/                                             
/******************************************************************************/
int gsc_getSQLTime(const TCHAR *Time, C_STRING &MAPSQLTime)
{
   COleDateTime dummyOleTime;
   C_STRING     dummyStr;
   size_t       i;

   if (gsc_ParseDateTime(Time, dummyOleTime) == GS_BAD) return GS_BAD;

   dummyStr = dummyOleTime.GetHour();
   for (i = 0; i < (2 - dummyStr.len()); i++) MAPSQLTime += _T('0');
   MAPSQLTime += dummyOleTime.GetHour();
   MAPSQLTime += _T(':');

   dummyStr = dummyOleTime.GetMinute();
   for (i = 0; i < (2 - dummyStr.len()); i++) MAPSQLTime += _T('0');
   MAPSQLTime += dummyOleTime.GetMinute();
   MAPSQLTime += _T(':');

   dummyStr = dummyOleTime.GetSecond();
   for (i = 0; i < (2 - dummyStr.len()); i++) MAPSQLTime += _T('0');
   MAPSQLTime += dummyOleTime.GetSecond();

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_ParseDateTime
/*+
  Questa funzione effettua il parse di una data-ora.
  Parametri:
  const TCHAR  *DateTime;
  COleDateTime &OleDateTime;

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/                                             
/******************************************************************************/
int gsc_ParseDateTime(const TCHAR *DateTime, COleDateTime &OleDateTime)
{
   if (!DateTime) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if (OleDateTime.ParseDateTime(DateTime) == 0)
      { GS_ERR_COD = eGSInvalidDateType; return GS_BAD; }

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_Parse
/*+
  Questa funzione effettua il parse di una data.
  Parametri:
  const TCHAR  *Date;
  COleDateTime &OleDateTime;

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/                                             
/******************************************************************************/
int gsc_ParseDate(const TCHAR *Date, COleDateTime &OleDateTime)
{
   if (!Date) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if (OleDateTime.ParseDateTime(Date, VAR_DATEVALUEONLY) == 0)
      { GS_ERR_COD = eGSInvalidDateType; return GS_BAD; }

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_ParseTime
/*+
  Questa funzione effettua il parse di un ora.
  Parametri:
  const TCHAR  *Time;
  COleDateTime &OleDateTime;

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/                                             
/******************************************************************************/
int gsc_ParseTime(const TCHAR *Time, COleDateTime &OleDateTime)
{
   if (!Time) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if (OleDateTime.ParseDateTime(Time, VAR_TIMEVALUEONLY) == 0)
      { GS_ERR_COD = eGSInvalidTimeType; return GS_BAD; }

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_current_DateTime
/*+
  Questa funzione ritorna la data e l'ora corrente nel formato breve impostato da
  windows (forzato anno a 4 cifre).
  Parametri:
  C_STRING &DateTime;
-*/                                             
/******************************************************************************/
void gsc_current_DateTime(C_STRING &DateTime)
{
   COleDateTime dummyOleDateTime;
   CString      dummyStr;

   dummyOleDateTime = COleDateTime::GetCurrentTime();
   // "%c"  = Date and time representation for current locale
   dummyStr         = dummyOleDateTime.Format(_T("%c")); // può sfondare quindi messa tra try e catch

   gsc_getWinShortDateTime4DgtYear((LPCTSTR) dummyStr, DateTime);
}


/******************************************************************************/
/*.doc gsc_current_Date
/*+
  Questa funzione ritorna la data corrente nel formato impostato da
  windows.
  Parametri:
  C_STRING &Date;
-*/                                             
/******************************************************************************/
void gsc_current_Date(C_STRING &Date)
{
   COleDateTime dummyOleDateTime;
   CString      dummyStr;

   dummyOleDateTime = COleDateTime::GetCurrentTime();
   // "%x"  = Date representation for current locale
   dummyStr         = dummyOleDateTime.Format(_T("%x"));

   gsc_getWinShortDateTime4DgtYear((LPCTSTR) dummyStr, Date);
}


/******************************************************************************/
/*.doc gsc_current_Time
/*+
  Questa funzione ritorna l'ora corrente nel formato impostato da
  windows.
  Parametri:
  C_STRING &Time;
-*/                                             
/******************************************************************************/
void gsc_current_Time(C_STRING &Time)
{
   COleDateTime dummyOleDateTime;
   CString      dummyStr;

   dummyOleDateTime = COleDateTime::GetCurrentTime();
   // "%X"  = Time representation for current locale
   dummyStr         = dummyOleDateTime.Format(_T("%X"));
   Time             = (LPCTSTR) dummyStr;
}


// Funzione di appoggio per "gsc_ddChooseDate"
INT_PTR CALLBACK MonthCalDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
   static SYSTEMTIME *psysDate;

   switch (msg)
   {
      case WM_INITDIALOG:
      {
         INITCOMMONCONTROLSEX icex;
         CRect ContainerRect, Rect;
         HWND  hCtrlWnd;
        
         SetWindowText(hwnd, gsc_msg(444)); // "GEOsim - Scegliere una data"

         psysDate = (SYSTEMTIME *) lParam;

         // Load the window class for month picker, date picker, time picker, updown
         icex.dwSize = sizeof(icex);
         icex.dwICC  = ICC_DATE_CLASSES;
         InitCommonControlsEx(&icex);

         // Create the month calendar.
         hCtrlWnd = CreateWindowEx(0,
                                   MONTHCAL_CLASS,
                                   GS_EMPTYSTR,
                                   WS_BORDER | WS_CHILD | WS_VISIBLE,
                                   0,0,0,0, // resize it later
                                   hwnd,
                                   NULL,
                                   g_hInst,
                                   NULL);

         // Get the size of container window
         GetWindowRect(hwnd, &ContainerRect);
         // Get the size required to show an entire month.
         MonthCal_GetMinReqRect(hCtrlWnd, &Rect);

         int OffSetX = (ContainerRect.Width() - Rect.Width()) / 2;
         int OffSetY = 10; // Arbitrary value
         SetWindowPos(hCtrlWnd, NULL, OffSetX, OffSetY,
                      Rect.Width(), Rect.Height(),
                      SWP_NOZORDER);

         // Set colors for aesthetics.
         MonthCal_SetColor(hCtrlWnd, MCSC_BACKGROUND, RGB(175,175,175));
         MonthCal_SetColor(hCtrlWnd, MCSC_MONTHBK, RGB(248,245,225));

         // Set the date
         MonthCal_SetCurSel(hCtrlWnd, psysDate);

         return 0;
      }
      case WM_NOTIFY:
      {
         LPNMHDR hdr = (LPNMHDR) lParam;

         if (hdr->code == MCN_SELCHANGE)
         {
            LPNMSELCHANGE lpNMSelChange = (LPNMSELCHANGE) lParam;

            psysDate->wYear  = lpNMSelChange->stSelStart.wYear;
            psysDate->wMonth = lpNMSelChange->stSelStart.wMonth;
            psysDate->wDay   = lpNMSelChange->stSelStart.wDay;
         }
         break;
      }
      case WM_COMMAND:
      {
         if (wParam == IDOK)
            EndDialog(hwnd, IDOK);
         else
         if (wParam == IDCANCEL)
            EndDialog(hwnd, IDCANCEL);

         break;
      }
   }

   return 0;
}


/******************************************************************************/
/*.doc gsc_ddChooseDate
/*+
  Questa funzione permette la scelta di una data tramite finestra.
  Parametri:
  C_STRING &DateTime;
  oppure
  COleDateTime &DateTime;
-*/                                             
/******************************************************************************/
void WINAPI gsc_ddChooseDate(COleDateTime &DateTime)
{
   // When resource from this ARX app is needed, just
   // instantiate a local CAcModuleResourceOverride
   CAcModuleResourceOverride myResources;

   HWND         hwndOwner;
   INT_PTR      res;
   CWnd         *pAcadWindow = CWnd::FromHandlePermanent(adsw_acadMainWnd()), *pChildWindow;
   SYSTEMTIME   sysDate;
   COleDateTime mDateTime;
   
   if (DateTime.GetStatus() == COleDateTime::valid)
   {
      sysDate.wYear  = DateTime.GetYear();
      sysDate.wMonth = DateTime.GetMonth();
      sysDate.wDay   = DateTime.GetDay();
      sysDate.wHour  = sysDate.wMinute = sysDate.wSecond = sysDate.wMilliseconds = 0;
   }
   else
      GetSystemTime(&sysDate);

   if ((pChildWindow = pAcadWindow->GetActiveWindow()) != NULL)
      hwndOwner = pChildWindow->m_hWnd;
   else
      hwndOwner = pAcadWindow->m_hWnd;

   // Create a model dialog box to hold the control.
   res = DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_DIALOG1), hwndOwner, MonthCalDlgProc, (LPARAM) (&sysDate));

   if (res == IDOK)
      DateTime.SetDateTime(sysDate.wYear, sysDate.wMonth, sysDate.wDay, 0, 0, 0);

   return;
}
void WINAPI gsc_ddChooseDate(C_STRING &DateTime)
{
   COleDateTime OleDateTime;
   CString      dummyStr;

   if (gsc_ParseDateTime(DateTime.get_name(), OleDateTime) == GS_BAD)
      OleDateTime.SetDate(0, 0 ,0); // invalido la data

   gsc_ddChooseDate(OleDateTime);
   // "%x"  = Date representation for current locale
   try
   {
      dummyStr = OleDateTime.Format(_T("%x")); // può sfondare quindi messa tra try e catch
   }
   catch (...)
   {
      DateTime.clear();
      return;
   }

   gsc_getWinShortDateTime4DgtYear((LPCTSTR) dummyStr, DateTime);
}


/******************************************************************************/
/*.doc gsc_getDateTimeSpanFormat                                              */
/*+
   Questa funzione ottiene una formattazione stringa del lasso di tempo.
   Per non appesantire il messaggio vengono visualizzate al massimo 2 unità di misura
   (es se si tratta di 3 giorni e 2 ore e 5 min vengono visualizzate solo giorni ed ore)
   Parametri:
   TCHAR *buffer; Stringa
   FILE  *fp;     puntatore al file.
   bool  Unicode; Flag che determina se il contenuto del file è in 
                  formato UNICODE o ANSI (default = false)
   
   Restituisce WEOF se il file è in End of file altrimenti il carattere letto.
-*/                                             
/******************************************************************************/
void gsc_getDateTimeSpanFormat(COleDateTimeSpan &Span, C_STRING &Msg)
{
   LONG Days = Span.GetDays(), Hours = Span.GetHours(), Minutes = Span.GetMinutes(), Seconds = Span.GetSeconds();

   Msg.clear();

   if (Span.GetStatus() != COleDateTimeSpan::valid) return;

   if (Days > 0)
   {
      Msg += Days; 
      Msg += _T(" "); 
      if (Days == 1) Msg += gsc_msg(471); // giorno
      else Msg += gsc_msg(451); // giorni
   }
   if (Hours > 0)
   {
      if (Days > 0) Msg += _T(", ");
      Msg += Hours;
      Msg += _T(" "); 
      if (Hours == 1) Msg += gsc_msg(473); // ora
      else Msg += gsc_msg(484); // ore
   }
   if (Days == 0 && Minutes > 0) // I minuti compaiono solo se non ci sono giorni
   {
      if (Hours > 0) Msg += _T(", ");
      Msg += Minutes;
      Msg += _T(" "); 
      if (Minutes == 1) Msg += gsc_msg(485); // minuto
      else Msg += gsc_msg(486); // minuti
   }
   if (Days == 0 && Hours == 0) // I secondi compaiono solo se non ci sono giorni nè ore
   {
      if (Seconds > 0)
      {
         if (Minutes > 0) Msg += _T(", ");
         Msg += Seconds;
         Msg += _T(" "); 
         if (Seconds == 1) Msg += gsc_msg(487); // secondo
         else Msg += gsc_msg(488); // secondi
      }
      else // 0 secondi
         if (Minutes == 0)
         {
            Msg += _T("0 "); 
            Msg += gsc_msg(488); // secondi
         }
   }
}


/******************************************************************************/
/*.doc gsc_fputs                                                              */
/*+
   Questa funzione scrive una stringa nel file. 
   Parametri:
   TCHAR *buffer; Stringa
   FILE  *fp;     puntatore al file.
   bool  Unicode; Flag che determina se il contenuto del file è in 
                  formato UNICODE o ANSI (default = false)
   
   Restituisce WEOF se il file è in End of file altrimenti il carattere letto.
-*/                                             
/******************************************************************************/
int gsc_fputs(C_STRING &Buffer, FILE *file, bool Unicode)
{
   return gsc_fputs(Buffer.get_name(), file, Unicode);
}
int gsc_fputs(TCHAR *Buffer, FILE *file, bool Unicode)
{
   int Result; 

   if (Unicode)
      Result = fputws(Buffer, file);
   else
   {
      char *strBuff = gsc_UnicodeToChar(Buffer);

      Result = fputs(strBuff, file);
      if (strBuff) free(strBuff);
   }

   return Result;
}


/******************************************************************************/
/*.doc gsc_fputc                                                              */
/*+
   Questa funzione scrive un carattere nel file. 
   Parametri:
   TCHAR Buffer;  Carattere
   FILE  *fp;     puntatore al file.
   bool  Unicode; Flag che determina se il contenuto del file è in 
                  formato UNICODE o ANSI (default = false)
   
   Restituisce WEOF se il file è in End of file altrimenti il carattere letto.
-*/                                             
/******************************************************************************/
int gsc_fputc(TCHAR Buffer, FILE *file, bool Unicode)
{
   int Result; 

   if (Unicode)
      Result = fputwc(Buffer, file);
   else
   {
      char strBuff = gsc_UnicodeToChar(Buffer);
      Result = fputc(strBuff, file);
   }

   return Result;
}


/******************************************************************************/
/*.doc gsc_readchar                                                           */
/*+
   Questa funzione legge un carattere dal file. 
   Parametri:
   FILE  *fp;     puntatore al file.
   bool  Unicode; Flag che determina se il contenuto del file è in 
                  formato UNICODE o ANSI (default = false)
   
   Restituisce WEOF se il file è in End of file altrimenti il carattere letto.
-*/                                             
/******************************************************************************/
TCHAR gsc_readchar(FILE *fp, bool Unicode)
{
   TCHAR ch;

   if (Unicode)
      ch = fgetwc(fp);
   else
   {
      char ANSIch;

      if ((ANSIch = fgetc(fp)) == EOF) ch = WEOF;
      else ch = gsc_CharToUnicode(ANSIch);
   }

   return ch;
}


/******************************************************************************/
/*.doc gsc_fread                                                           */
/*+
   Questa funzione legge dei caratteri da un file. 
   Parametri:
   TCHAR *buffer; Buffer di destinazione
   size_t count;  N. caratteri da leggere
   FILE  *fp;     puntatore al file.
   bool  Unicode; Flag che determina se il contenuto del file è in 
                  formato UNICODE o ANSI (default = false)
   
   Restituisce WEOF se il file è in End of file altrimenti il carattere letto.
-*/                                             
/******************************************************************************/
size_t gsc_fread(TCHAR *buffer, size_t count, FILE *fp, bool Unicode)
{
   size_t Res;

   if (Unicode)
      Res = fread(buffer, sizeof(TCHAR), count, fp);
   else
   {
      char  *dummy;
      TCHAR *dummyUnicode;

      dummy = (char *) malloc((count + 1) * sizeof(char));
      Res = fread(dummy, sizeof(char), count, fp);
      dummy[count] = '\0';
      dummyUnicode = gsc_CharToUnicode(dummy);
      free(dummy);
      wcsncpy(buffer, dummyUnicode, count);
      free(dummyUnicode);
   }
   
   return Res;
}


/******************************************************************************/
/*.doc gsc_readline                                                           */
/*+
   Questa funzione legge il contenuto di un file dal valore puntato in quel
   momento al primo end of line. 
   Parametri:
   FILE  *fp           puntatore al file.
   TCHAR **rigaletta   stringa letta.
   bool  Unicode;      Flag che determina se il contenuto del file è in 
                       formato UNICODE o ANSI (default = false)
   
   Restituisce WEOF se il file è in End of file altrimenti GS_GOOD(1).
   N.B.: Alloca spazio in memoria.                
-*/                                             
/******************************************************************************/
int gsc_readline(FILE *fp, TCHAR **rigaletta, bool Unicode)
{
   C_STRING buffer;
   int      result;

   if ((result = gsc_readline(fp, buffer, Unicode)) != GS_GOOD) return result;
   if (buffer.get_name()) *rigaletta = gsc_tostring(buffer.get_name());
   else *rigaletta = gsc_tostring(GS_EMPTYSTR);

   return GS_GOOD;
}
int gsc_readline(FILE *fp, C_STRING &buffer, bool Unicode)
{
   bool Stop = false;

   buffer.clear();
   if (Unicode)
   {
      TCHAR ch;

      while (!Stop && (ch = fgetwc(fp)) != WEOF)
         switch (ch)
         {
            case 0xFEFF: // wBOM
               break;
            case _T('\r'):
               break;
            case _T('\n'):
               Stop = true; 
               break;
            default:
               buffer += ch;
         }
   }
   else // Se NON Unicode
   {
      char ch;

      while (!Stop && (ch = fgetc(fp)) != EOF)
         switch (ch)
         {
            case '\r':
               break;
            case '\n':
               Stop = true; 
               break;
            default:
               buffer += ch;
         }
   }

   if (feof(fp) == 0) return GS_GOOD;

   return (buffer.len() > 0) ? GS_GOOD : WEOF;
}


/******************************************************************************/
/*.doc (new 2) gsc_is_numeric <external>  */
/*+
   Questa funzione restituisce GS_GOOD se la stringa è un numero altrimenti
   restituisce GS_BAD.
   Parametri:
   TCHAR *string;
-*/                                             
/******************************************************************************/
int gsc_is_numeric(const TCHAR *string)
{
   size_t i, len, start;
   TCHAR  DataValue[MAX_LEN_FIELD];

   if (!string) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   gsc_strcpy(DataValue, string, MAX_LEN_FIELD);
   gsc_alltrim(DataValue);

   if ((len = wcslen(DataValue)) == 0) return GS_GOOD;
   start = (DataValue[0] == _T('-') || DataValue[0] == _T('+')) ? 1 : 0; // salta il segno iniziale

   for (i = start; i < len; i++)
      if (iswdigit(DataValue[i]) == 0 && DataValue[i] != _T('.')) return GS_BAD;

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_CharToUnicode                                         <external>  */
/*+
  Questa funzione restituisce il carattere unicode risultato della conversione da un
  carattere char.
  Parametri:
  const char CharCharacter;

  Restituisce il carattere unicode. 
-*/                                             
/*****************************************************************************/
TCHAR gsc_CharToUnicode(const char CharCharacter)
{
   TCHAR UnicodeCharacter;
   
   mbtowc(&UnicodeCharacter, &CharCharacter, MB_CUR_MAX);

   return UnicodeCharacter;
}


/*****************************************************************************/
/*.doc gsc_CharToUnicode                                         <external>  */
/*+
  Questa funzione restituisce la stringa unicode risultato della conversione da una
  stringa di char.
  Parametri:
  const char *CharString;

  Restituisce la stringa unicode in caso di successo altrimenti restituisce NULL. 
  N.B. Alloca memoria.
-*/                                             
/*****************************************************************************/
TCHAR *gsc_CharToUnicode(const char *CharString)
{
   TCHAR   *UnicodeString = NULL;
   size_t  len = 0;
   errno_t err;
   
   if (!CharString) return NULL;

   // Conto quanti caratteri devo allocare per una stringa Unicode
	mbstowcs_s(&len, NULL, 0, CharString, 1 + 2 * strlen(CharString));
   if ((UnicodeString = (TCHAR *) malloc((len + 1) * sizeof(TCHAR))) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
	err = mbstowcs_s(&len, UnicodeString, len + 1, CharString, len + 1);
   if (err != 0) { free(UnicodeString); UnicodeString = NULL; }

   return UnicodeString;
}


/*****************************************************************************/
/*.doc gsc_UnicodeToChar                                         <external>  */
/*+
  Questa funzione restituisce il carattere char risultato della conversione da un
  carattere unicode.
  Parametri:
  const TCHAR UnicodeCharacter;

  Restituisce il carattere unicode. 
-*/                                             
/*****************************************************************************/
char gsc_UnicodeToChar(const TCHAR UnicodeCharacter)
{
   int  i;
   char CharCharacter;
   
   if (wctomb_s(&i, &CharCharacter, 1, UnicodeCharacter) != 0) return '\0';

   return CharCharacter;
}


/*****************************************************************************/
/*.doc gsc_UnicodeToChar                                         <external>  */
/*+
  Questa funzione restituisce la stringa char risultato della conversione da una
  stringa di unicode.
  Parametri:
  const TCHAR *UnicodeString;

  Restituisce la stringa char in caso di successo altrimenti restituisce NULL. 
  N.B. Alloca memoria.
-*/                                             
/*****************************************************************************/
char *gsc_UnicodeToChar(const TCHAR *UnicodeString)
{
   char *CharString = NULL;
   size_t  len = 0, err;
   
   if (!UnicodeString) return NULL;

   // Conto quanti caratteri devo allocare per una stringa Unicode
   len = wcstombs((char *) NULL, UnicodeString, (size_t) (1 + 2 * wcslen(UnicodeString)));
   if ((CharString = (char *) malloc((len + 1) * sizeof(char))) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
	err = wcstombs(CharString, UnicodeString, len + 1);
   if (err == -1) { free(CharString); CharString = NULL; }

   return CharString;
}


/**************************************************************************/
/*.doc (new 2) gsc_mainentnext(ads_name ent)                              */
/*+
   Funzione come ads_entnext solo che considera solo le entità principali
   saltando le sotto-entità (es. vertici di una polilinea)
   Parametri:
   ads_name ent;     entità grafica
   ads_name next;    entità grafica principale successiva

   Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/                                             
/**************************************************************************/
int gsc_mainentnext(ads_name ent, ads_name next)
{
   int       result = GS_GOOD;
   C_RB_LIST lista;
   presbuf   p;
   ads_name  ent_ndx;

   if (ent == NULL || ads_name_nil(ent))
   {
      if (ads_entnext(NULL, next) != RTNORM) return GS_BAD;
   }
   else
   {
      ads_name_set(ent, ent_ndx);
   
      do
      {
         if (ads_entnext(ent_ndx, ent_ndx) != RTNORM) { result = GS_BAD; break; }
         if ((lista << acdbEntGet(ent_ndx)) == NULL)
            { result = GS_BAD; GS_ERR_COD = eGSInvEntityOp; break; }
         if ((p = lista.SearchType(0)) == NULL)
            { result = GS_BAD; GS_ERR_COD = eGSInvRBType; break; }
      }
      // salto tutte le entità che non sono principali
      while (gsc_strcmp(p->resval.rstring, _T("ATTEDEF")) == 0 ||
             gsc_strcmp(p->resval.rstring, _T("ATTRIB")) == 0 ||
             gsc_strcmp(p->resval.rstring, _T("BODY")) == 0 ||
             gsc_strcmp(p->resval.rstring, _T("OLEFRAME")) == 0 ||
             gsc_strcmp(p->resval.rstring, _T("SEQEND")) == 0 ||
             gsc_strcmp(p->resval.rstring, _T("VERTEX")) == 0 ||
             gsc_strcmp(p->resval.rstring, _T("VIEWPORT")) == 0);

      if (result == GS_GOOD) ads_name_set(ent_ndx, next);
   }

   return result;
}


/***************************************************************/
/*.doc (new 2) gsc_AcadLanguage(int *lan)                      */
/*+
   Funzione che restituisce la ligua di AutoCAD.
   Parametri:
   int *lan;  parametro di output, (ITALIAN, ENGLISH...)

   Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/                                             
/***************************************************************/
int gsc_GetAcadLanguage(int *lan)
{
   TCHAR *str = NULL;
   int   res = GS_BAD, version = 0;

   do
   {
      // Verifico se l' AutoCAD è ITALIANO
      if (acedGetCName(_T("PLINEA"), &str) == RTNORM)
      {
         if (gsc_strcmp(str, _T("_PLINE")) == 0)
         {
            version = LAN_ITALIAN;
            res = GS_GOOD;
            break;
         }
      }
      else
      {
         // Verifico se l' AutoCAD è INGLESE
         if (ads_getcname(_T("PLINE"), &str) == RTNORM)
         {
            if (gsc_strcmp(str, _T("_PLINE")) == 0)
            {
               version = LAN_BRITISH;
               res = GS_GOOD;
               break;
            }
         }
      }
      // Per verificare le altre lingue usare si puo usare anche un comando diverso
      break;
   }
   while (0);
   
   if (str) free(str);
   
   if (res == GS_GOOD) *lan = version;

   return res;
}


/***************************************************************/
/*.doc gsc_GetAcadLocation						                     */
/*+
   Funzione che restituisce la path di installazione di AutoCAD.
   Parametri:
   C_STRING &Dir;

   Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/                                             
/***************************************************************/
int gsc_GetAcadLocation(C_STRING &Dir)
{
	C_STRING dir;
   TCHAR    Source[511 + 1]; // vedi manuale acad (funzione "acedFindFile")

   if (acedFindFile(_T("ACAD.EXE"), Source) != RTNORM) return GS_BAD;
	gsc_splitpath(Source, &Dir, &dir);
   Dir += dir;

   return GS_GOOD;
}


/****************************************************************************/
/*.doc gsc_callCmd                                                          */
/*+
  Chiama un comando AutoCAD con i parametri specificati.
  Parametri:
  TCHAR   *pCmdName;        Nome comando
  struct resbuf *pParms;   Lista di parametri

  Ritorna RTNORM in caso di successo altrimenti RTERROR o RTCAN.
-*/
/****************************************************************************/
int gsc_callCmd(TCHAR *pCmdName, ...)
{
   C_RB_LIST Params;
   va_list   type_for_ellipsis;
   int       rbType;

   va_start(type_for_ellipsis, pCmdName); // inizio stack dei parametri

   // Because of parameter passing conventions in C:
   // use mode=int for char, and short types
   // use mode=double for float types
   // use a pointer for array types

   while ((rbType = va_arg(type_for_ellipsis, int)) != 0)
      switch (rbType)
      {
         case RTNONE:    // No result
         case RTVOID:    // Blank symbol
         case RTLB:      // list begin
         case RTLE:      // list end
         case RTDOTE:    // dotted pair
         case RTNIL:     // nil
         case RTT:       // T atom
            if ((Params += acutBuildList(rbType, 0)) == NULL)
               { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            break;
         case RTREAL:    // Real number
         case RTANG:     // Angle
         case RTORINT:   // Orientation
            if ((Params += acutBuildList(rbType, va_arg(type_for_ellipsis, ads_real), 0)) == NULL)
               { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            break;
         case RTENAME:   // Entity name
            #if !defined(_WIN64) && !defined(__LP64__) && !defined(_AC64)
               if ((Params += acutBuildList(rbType, va_arg(type_for_ellipsis, __w64 long *), 0)) == NULL)
                  { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            #else
               if ((Params += acutBuildList(rbType, va_arg(type_for_ellipsis, __int64 *), 0)) == NULL)
                  { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            #endif
            break;
         case RTPICKS:   // Pick set
         {
            C_SELSET ss;
            ads_name dummy;

            #if !defined(_WIN64) && !defined(__LP64__) && !defined(_AC64)
               if (ss.add_selset(va_arg(type_for_ellipsis, __w64 long *)) == GS_BAD) return GS_BAD;
            #else
               if (ss.add_selset(va_arg(type_for_ellipsis, __int64 *)) == GS_BAD) return GS_BAD;
            #endif

            ss.get_selection(dummy);
            if ((Params += acutBuildList(rbType, dummy, 0)) == NULL)
               { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            ss.ReleaseAllAtDistruction(GS_BAD);
            break;
         }
         case RTPOINT:   // 2D point X and Y only
         case RT3DPOINT: // 3D point - X, Y, and Z
            if ((Params += acutBuildList(rbType, va_arg(type_for_ellipsis, ads_real *), 0)) == NULL)
               { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            break;
         case RTLONG:    // Long integer
            #ifdef __LP64__
               if ((Params += acutBuildList(rbType, va_arg(type_for_ellipsis, int), 0)) == NULL)
                  { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            #else
               if ((Params += acutBuildList(rbType, va_arg(type_for_ellipsis, long), 0)) == NULL)
                  { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            #endif

            break;
         case RTSHORT:   // Short integer
            if ((Params += acutBuildList(rbType, va_arg(type_for_ellipsis, short), 0)) == NULL)
               { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            break;
         case RTSTR:     // String
            if ((Params += acutBuildList(rbType, va_arg(type_for_ellipsis, ACHAR *), 0)) == NULL)
               { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            break;
         default:
            GS_ERR_COD = eGSInvRBType;
            return GS_BAD;
      }

   va_end(type_for_ellipsis); // inizio stack dei parametri

   return gsc_callCmd(pCmdName, Params.get_head());
}
int gsc_callCmd(TCHAR *pCmdName, struct resbuf *pParms) 
{
   presbuf AcadCommand;        // The ASE command item
   int     status;             // Command status
   int     OldEcho, OldCmdDia, OldOSMode;

   gsc_set_echo(0, &OldEcho);     // Temporary CMDECHO setting (OFF)
   gsc_set_cmddia(0, &OldCmdDia); // Temporary CMDDIA setting (OFF)
   gsc_set_osmode(0, &OldOSMode); // Temporary OSNAP setting (OFF)
   
   // Chain the calling parameters
   AcadCommand = acutBuildList(RTSTR, pCmdName, 0);
   AcadCommand->rbnext = pParms;

   GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = true; // usato nell'evento beginSave
   status = acedCmdS(AcadCommand);     // Execute command
   GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = false;

   AcadCommand->rbnext = NULL;
   acutRelRb(AcadCommand);

   gsc_set_echo(OldEcho);     // Reset CMDECHO
   gsc_set_cmddia(OldCmdDia); // Reset CMDDIA
   gsc_set_osmode(OldOSMode); // Reset OSMODE

   if (status != RTNORM) GS_ERR_COD = eGSAdsCommandErr;
   
   return status;
}


/**************************************************************/
/*.doc gsc_FindSupportFiles                                   */
/*+
   Funzione che restituisce la path completa di ricerca di un
   file di supporto per lista di valori di un attributo nella directory
   indicata.
   L'ordine di ricerca dei file è TAB, REF, FDF, DEF.
  
   Parametri:
   const TCHAR *AttName;      Nome dell'attributo (input)
   int prj;                  Codice progetto di appartenenza (input)
   int cls;                  Codice classe di appartenenza (input)
   int sub;                  Codice sottoclasse di appartenenza (input)
   int sec;                  Codice tabella secondaria di appartenenza (input)
   C_STRING &OutPath;        Path completa del file se esistente (output)
   ValuesListTypeEnum *type; Tipo del file esistente (output)

   Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/                                             
/**************************************************************/
int gsc_FindSupportFiles(const TCHAR *AttName,
                         int prj, int cls, int sub, int sec,
                         C_STRING &OutPath, ValuesListTypeEnum *type)
{
   C_PROJECT *pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj);
   C_STRING  Dir;
   TCHAR     *dummy = NULL;

   // Direttorio Support di GEOSIM
   Dir = GEOsimAppl::GEODIR;
   Dir += _T("\\");
   Dir += GEOSUPPORTDIR;
   Dir += _T("\\");
   if (gsc_FindSupportFiles(Dir.get_name(), AttName, prj, cls, sub, sec,
                            OutPath, type) == GS_GOOD)
      return GS_GOOD;

   // Cerco prima nella cartella server e poi in quella del progetto per dare priorità 
   // alle liste nel server gestite da interfaccia grafica in geo_ui.

   // Direttorio del progetto
   if (pPrj)
   {
      Dir = pPrj->get_dir();
      Dir += _T("\\");
      Dir += GEOSUPPORTDIR;
      Dir += _T("\\");
      if (gsc_FindSupportFiles(Dir.get_name(), AttName, prj, cls, sub, sec,
                               OutPath, type) == GS_GOOD)
         return GS_GOOD;
   }

   // Direttorio Support di GEOWORK
   Dir = GEOsimAppl::WORKDIR;
   Dir += _T("\\");
   Dir += GEOSUPPORTDIR;
   Dir += _T("\\");
   if (gsc_FindSupportFiles(Dir.get_name(), AttName, prj, cls, sub, sec,
                            OutPath, type) == GS_GOOD)
      return GS_GOOD;

   return GS_BAD;
}
int gsc_FindSupportFiles(const TCHAR *dir, const TCHAR *AttName,
                         int prj, int cls, int sub, int sec,
                         C_STRING &OutPath, ValuesListTypeEnum *type)
{
   C_STRING path, dirett;
   C_INFO   *pInfo = NULL;
   C_STRING tipoTAB, tipoREF, tipoFDF, tipoDEF;
   C_STRING progetto, classe, sottoclasse, secondaria; 
   C_STRING BaseName, Name2search;
   
   // Inizializzazioni varie
   tipoTAB = _T(".Tab");
   tipoREF = _T(".Ref");
   tipoFDF = _T(".Fdf");
   tipoDEF = _T(".Def");
   // Progetto
   progetto = _T("_PRJ");
   progetto += prj; 
   // Classe
   classe = _T("_CLS");
   classe += cls; 
   // Eventuale sottoclasse
   if (sub > 0)
      { sottoclasse = _T("_SUB"); sottoclasse += sub; }
   // Eventuale secondaria
   if (sec > 0)
      { secondaria = _T("_SEC"); secondaria += sec; }

   // -------------------------------------------------------------------
   // 1° controllo costruisco il nome completo ATTRIB_PRJ1_CLS2_SUB4_SEC5
   if ((sub > 0) && (sec > 0))
   {
      BaseName = dir;
      BaseName += AttName;
      BaseName += progetto;
      BaseName += classe;
      BaseName += sottoclasse;
      BaseName += secondaria;

      // Prima il TAB
      Name2search = BaseName;
      Name2search += tipoTAB;
      // Verifico l' esistenza del file
      if (gsc_path_exist(Name2search.get_name()) == GS_GOOD)
         { *type = TAB; OutPath = Name2search; return GS_GOOD; }

      // Poi il REF
      Name2search = BaseName;
      Name2search += tipoREF;
      // Verifico l' esistenza del file
      if (gsc_path_exist(Name2search.get_name()) == GS_GOOD)
         { *type = REF; OutPath = Name2search; return GS_GOOD; }

      // Poi il FDF
      Name2search = BaseName;
      Name2search += tipoFDF;
      // Verifico l' esistenza del file
      if (gsc_path_exist(Name2search.get_name()) == GS_GOOD)
         { *type = FDF; OutPath = Name2search; return GS_GOOD; }

      // Poi il DEF
      Name2search = BaseName;
      Name2search += tipoDEF;
      // Verifico l' esistenza del file
      if (gsc_path_exist(Name2search.get_name()) == GS_GOOD)
         { *type = DEF; OutPath = Name2search; return GS_GOOD; }
   }

   // -------------------------------------------------------------------
   // 2° controllo costruisco il nome solo ATTRIB_PRJ1_CLS2_SUB4
   if (sub > 0) 
   {
      BaseName = dir;
      BaseName += AttName;
      BaseName += progetto;
      BaseName += classe;
      BaseName += sottoclasse;

      // Prima il TAB   
      Name2search = BaseName;
      Name2search += tipoTAB;
      // Verifico l' esistenza del file
      if (gsc_path_exist(Name2search.get_name()) == GS_GOOD)
         { *type = TAB; OutPath = Name2search; return GS_GOOD; }

      // Poi il REF
      Name2search = BaseName;
      Name2search += tipoREF;
      // Verifico l' esistenza del file
      if (gsc_path_exist(Name2search.get_name()) == GS_GOOD)
         { *type = REF; OutPath = Name2search; return GS_GOOD; }

      // Poi il FDF
      Name2search = BaseName;
      Name2search += tipoFDF;
      // Verifico l' esistenza del file
      if (gsc_path_exist(Name2search.get_name()) == GS_GOOD)
         { *type = FDF; OutPath = Name2search; return GS_GOOD; }

      // Poi il DEF
      Name2search = BaseName;
      Name2search += tipoDEF;
      // Verifico l' esistenza del file
      if (gsc_path_exist(Name2search.get_name()) == GS_GOOD)
         { *type = DEF; OutPath = Name2search; return GS_GOOD; }
   }

   // -------------------------------------------------------------------
   // 3° controllo costruisco il nome solo ATTRIB_PRJ1_CLS2_SEC5
   if (sec > 0) 
   {
      BaseName = dir;
      BaseName += AttName;
      BaseName += progetto;
      BaseName += classe;
      BaseName += secondaria;

      // Prima il TAB
      Name2search = BaseName;
      Name2search += tipoTAB;
      // Verifico l' esistenza del file
      if (gsc_path_exist(Name2search.get_name()) == GS_GOOD)
         { *type = TAB; OutPath = Name2search; return GS_GOOD; }

      // Poi il REF
      Name2search = BaseName;
      Name2search += tipoREF;
      // Verifico l' esistenza del file
      if (gsc_path_exist(Name2search.get_name()) == GS_GOOD)
         { *type = REF; OutPath = Name2search; return GS_GOOD; }

      // Poi il FDF
      Name2search = BaseName;
      Name2search += tipoFDF;
      // Verifico l' esistenza del file
      if (gsc_path_exist(Name2search.get_name()) == GS_GOOD)
         { *type = FDF; OutPath = Name2search; return GS_GOOD; }

      // Poi il DEF
      Name2search = BaseName;
      Name2search += tipoDEF;
      // Verifico l' esistenza del file
      if (gsc_path_exist(Name2search.get_name()) == GS_GOOD)
         { *type = DEF; OutPath = Name2search; return GS_GOOD; }
   }

   // -------------------------------------------------------------------
   // 4° controllo costruisco il nome solo ATTRIB_PRJ1_CLS2 
   BaseName = dir;
   BaseName += AttName;
   BaseName += progetto;
   BaseName += classe;

   // Prima il TAB
   Name2search = BaseName;
   Name2search += tipoTAB;
   // Verifico l' esistenza del file
   if (gsc_path_exist(Name2search.get_name()) == GS_GOOD)
      { *type = TAB; OutPath = Name2search; return GS_GOOD; }

   // Poi il REF
   Name2search = BaseName;
   Name2search += tipoREF;
   // Verifico l' esistenza del file
   if (gsc_path_exist(Name2search.get_name()) == GS_GOOD)
      { *type = REF; OutPath = Name2search; return GS_GOOD; }

   // Poi il FDF
   Name2search = BaseName;
   Name2search += tipoFDF;
   // Verifico l' esistenza del file
   if (gsc_path_exist(Name2search.get_name()) == GS_GOOD)
      { *type = FDF; OutPath = Name2search; return GS_GOOD; }

   // Poi il DEF
   Name2search = BaseName;
   Name2search += tipoDEF;
   // Verifico l' esistenza del file
   if (gsc_path_exist(Name2search.get_name()) == GS_GOOD)
      { *type = DEF; OutPath = Name2search; return GS_GOOD; }

   // -------------------------------------------------------------------
   // 5° controllo costruisco il nome solo ATTRIB_PRJ1
   BaseName = dir;
   BaseName += AttName;
   BaseName += progetto;
   // Prima il TAB
   Name2search = BaseName;
   Name2search += tipoTAB;
   // Verifico l' esistenza del file
   if (gsc_path_exist(Name2search.get_name()) == GS_GOOD)
      { *type = TAB; OutPath = Name2search; return GS_GOOD; }

   // Poi il REF
   Name2search = BaseName;
   Name2search += tipoREF;
   // Verifico l' esistenza del file
   if (gsc_path_exist(Name2search.get_name()) == GS_GOOD)
      { *type = REF; OutPath = Name2search; return GS_GOOD; }

   // Poi il FDF
   Name2search = BaseName;
   Name2search += tipoFDF;
   // Verifico l' esistenza del file
   if (gsc_path_exist(Name2search.get_name()) == GS_GOOD)
      { *type = FDF; OutPath = Name2search; return GS_GOOD; }

   // Poi il DEF
   Name2search = BaseName;
   Name2search += tipoDEF;
   // Verifico l' esistenza del file
   if (gsc_path_exist(Name2search.get_name()) == GS_GOOD)
      { *type = DEF; OutPath = Name2search; return GS_GOOD; }

   // -------------------------------------------------------------------
   // 6° controllo costruisco il nome solo ATTRIB
   BaseName = dir;
   BaseName += AttName;

   // Prima il TAB
   Name2search = BaseName;
   Name2search += tipoTAB;
   // Verifico l' esistenza del file
   if (gsc_path_exist(Name2search.get_name()) == GS_GOOD)
      { *type = TAB; OutPath = Name2search; return GS_GOOD; }

   // Poi il REF
   Name2search = BaseName;
   Name2search += tipoREF;
   // Verifico l' esistenza del file
   if (gsc_path_exist(Name2search.get_name()) == GS_GOOD)
      { *type = REF; OutPath = Name2search; return GS_GOOD; }

   // Poi il FDF
   Name2search = BaseName;
   Name2search += tipoFDF;
   // Verifico l' esistenza del file
   if (gsc_path_exist(Name2search.get_name()) == GS_GOOD)
      { *type = FDF; OutPath = Name2search; return GS_GOOD; }

   // Poi il DEF
   Name2search = BaseName;
   Name2search += tipoDEF;
   // Verifico l' esistenza del file
   if (gsc_path_exist(Name2search.get_name()) == GS_GOOD)
      { *type = DEF; OutPath = Name2search; return GS_GOOD; }

   return GS_BAD;
}


/**************************************************************/
/*.doc gsc_CopySupportFile                                   */
/*+
   Funzione che copia un file di supporto per lista di valori di 
   un attributo per renderlo disponibile per una altra classe
   (o sottoclasse o tabella secondaria).
   Parametri:
   const TCHAR *AttName;   Nome dell'attributo (input)
   int SrcPrj;             Codice progetto sorgente
   int SrcCls;             Codice classe sorgente
   int SrcSub;             Codice sottoclasse sorgente
   int SrcSec;             Codice tabella secondaria sorgente
   int DstPrj;             Codice progetto destinazione
   int DstCls;             Codice classe destinazione
   int DstSub;             Codice sottoclasse destinazione
   int DstSec;             Codice tabella secondaria destinazione

   Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/                                             
/**************************************************************/
int gsc_CopySupportFile(const TCHAR *AttName, int SrcPrj, int SrcCls, int SrcSub, int SrcSec,
                        int DstPrj, int DstCls, int DstSub, int DstSec)
{
   C_STRING           SrcSupportFile, DstSupportFile, FileName, Ext;
   ValuesListTypeEnum FileType;

   // Cerco il file di supporto destinazione
   if (gsc_FindSupportFiles(AttName, DstPrj, DstCls, DstSub, DstSec,
                            DstSupportFile, &FileType) == GS_GOOD)
      // Se esiste già esco
      return GS_GOOD;

   // Cerco il file di supporto sorgente
   if (gsc_FindSupportFiles(AttName, SrcPrj, SrcCls, SrcSub, SrcSec,
                            SrcSupportFile, &FileType) == GS_BAD) return GS_GOOD;

   gsc_splitpath(SrcSupportFile, NULL, NULL, &FileName, &Ext);

   // Direttorio Support di GEOSIM
   DstSupportFile = GEOsimAppl::GEODIR;
   DstSupportFile += _T("\\");
   DstSupportFile += GEOSUPPORTDIR;
   DstSupportFile += _T("\\");
   DstSupportFile += AttName;

   if (FileName.at(_T("_PRJ"), FALSE) && DstPrj > 0)
   {
      DstSupportFile += _T("_PRJ");
      DstSupportFile += DstPrj; 
   }

   if (FileName.at(_T("_CLS"), FALSE) && DstCls > 0) // lista di valori solo per quella classe
   {
      DstSupportFile += _T("_CLS");
      DstSupportFile += DstCls;
   }

   if (FileName.at(_T("_SUB"), FALSE) && DstSub > 0) // lista di valori solo per quella sotto-classe
   {
      DstSupportFile += _T("_SUB");
      DstSupportFile += DstSub;
   }
   if (FileName.at(_T("_SEC"), FALSE) && DstSec > 0) // lista di valori solo per quella secondaria
   {
      DstSupportFile += _T("_SEC");
      DstSupportFile += DstSec;
   }
   DstSupportFile += Ext; 

   // Se esiste già esco
   if (gsc_path_exist(DstSupportFile) == GS_GOOD) return GS_GOOD;

   // Copio il file rinominandolo
   return gsc_copyfile(SrcSupportFile, DstSupportFile);
}


/**************************************************************/
/*.doc gsc_DelSupportFile                                     */
/*+
   Funzione che cancella un file di supporto per lista di valori di 
   un attributo di una classe (o sottoclasse o tabella secondaria).
   Parametri:
   const TCHAR *AttName;   Nome dell'attributo (input)
   int Prj;                Codice progetto
   int Cls;                Codice classe
   int Sub;                Codice sottoclasse
   int Sec;                Codice tabella secondaria

   Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/                                             
/**************************************************************/
int gsc_DelSupportFile(const TCHAR *AttName, int Prj, int Cls, int Sub, int Sec)
{
   C_STRING           SupportFile, FileName, Ext;
   ValuesListTypeEnum FileType;

   // Cerco il file di supporto
   if (gsc_FindSupportFiles(AttName, Prj, Cls, Sub, Sec,
                            SupportFile, &FileType) != GS_GOOD)
      // Se non esiste esco
      return GS_GOOD;

   gsc_splitpath(SupportFile, NULL, NULL, &FileName, &Ext);

   // lista di valori solo per tutte le classi quando ne avevo indicato una sola
   if (FileName.at(_T("_CLS"), FALSE) == NULL && Cls > 0)
      return GS_GOOD;

   // lista di valori solo per tutte le sottoclassi quando ne avevo indicato una sola
   if (FileName.at(_T("_SUB"), FALSE) == NULL && Sub > 0)
      return GS_GOOD;

   // lista di valori per tutte le secondarie della classe quando ne avevo indicato una sola
   if (FileName.at(_T("_SEC"), FALSE) == NULL && Sec > 0)
      return GS_GOOD;

   return gsc_delfile(SupportFile);
}


/*********************************************************/
/*.doc GetSSLisp                              <internal> */
/*+
  Funzione che legge un simbolo dall'ambiente LISP.
  Parametri:
  const TCHAR *str;

  Ritorna RTNORM in caso di successo altrimenti RTERROR o RTCAN
-*/  
/*********************************************************/
static struct resbuf* GetSSLisp(const TCHAR *str)
{
   presbuf result;

   if (!str || wcslen(str) == 1) return NULL;
   if (wcsicmp(_T("SSFILTER"), str) == 0)
      if (GS_LSFILTER.ptr_SelSet()->length() == 0)
         return NULL;
      else
      {
         ads_name ss;

         GS_LSFILTER.ptr_SelSet()->get_selection(ss);
         return acutBuildList(RTPICKS, ss, 0);
      }
   
   if (str[0] != _T('!')) return NULL;
   // This function can be used in the ARX program environment only when AutoCAD
   // sends the message kInvkSubrMsg to the application
   if (acedGetSym(str + 1, &result) != RTNORM) return NULL;
   if (!result) acutPrintf(_T("nil"));

   return result;
}


/*********************************************************/
/*.doc gs_AskSelSet                           <external> */
/*+
  Funzione LISP come ssget senza parametri 
  con la capacità di leggere gruppi di selezione.

  Ritorna RTNORM in caso di successo altrimenti RTERROR o RTCAN
-*/  
/*********************************************************/
int gs_AskSelSet(void)
{
   ads_name ss;
   int      result;

   acedRetNil(); 
   if ((result = gsc_ssget(NULL, NULL, NULL, NULL, ss)) == RTNORM)
      acedRetName(ss, RTPICKS);

   return result;
}


/*********************************************************/
/*.doc gsc_ssget                              <external> */
/*+
   Come acedSSGet con la capacità di leggere gruppi di selezione
   o entità dall'ambiente LISP se i primi 4 parametri sono NULL.

  Ritorna RTNORM in caso di successo altrimenti RTERROR o RTCAN
-*/  
/*********************************************************/
int gsc_ssget(const TCHAR *str, const void *pt1, const void *pt2,
              const struct resbuf *filter, ads_name ss)
{
   if (!str && !pt1 && !pt2 && !filter)
   {
      int ret;
      
      ads_ssSetOtherCallbackPtr(GetSSLisp);
      ret = acedSSGet(_T(":?"), pt1, pt2, filter, ss);
      ads_ssSetOtherCallbackPtr(NULL);

      return ret;
   }
   else return acedSSGet(str, pt1, pt2, filter, ss);
}
int gsc_ssget(const TCHAR *str, const void *pt1, const void *pt2,
              const struct resbuf *filter, C_SELSET &ss)
{
   ads_name ss1;
   int      res;

   if ((res = gsc_ssget(str, pt1, pt2, filter, ss1)) == RTNORM)
      ss << ss1;
   else ss.clear();

   return res;
}


/*********************************************************************/
/*.doc gsc_getClsTypeDescr                                    <external> */
/*+
  Questa funzione restituisce una stringa che descrive il tipo
  della classe di GEOsim.
  Parametri:
  int      Type;     tipo di classe GEOsim
  C_STRING &tipo;    descrizione del tipo
  
  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_getClsTypeDescr(int Type, C_STRING &StrType)
{
   switch (Type)
   {
      case TYPE_POLYLINE:
         StrType = gsc_msg(68);    // "Polilinea"
         break;
      case TYPE_TEXT:
         StrType = gsc_msg(14);    // "Testo"
         break;
      case TYPE_NODE:
         StrType = gsc_msg(530);   // "Nodo"
         break;
      case TYPE_SURFACE:
         StrType = gsc_msg(535);   // "Superficie"
         break;
      default:
         return GS_BAD;
   }

   return GS_GOOD;   
}


/*********************************************************************/
/*.doc getClsCategoryDescr                                    <external> */
/*+
  Questa funzione restituisce una stringa che descrive la categoria
  della classe di GEOsim.
  Parametri:
  int      Type;     tipo di classe GEOsim
  C_STRING &tipo;    descrizione del tipo
  
  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int getClsCategoryDescr(int Category, C_STRING &StrCategory)
{
   switch (Category)
   {
      case CAT_SIMPLEX:
         StrCategory = gsc_msg(623);    // "Semplice"
         break;
      case CAT_GROUP:
         StrCategory = gsc_msg(533);    // "Gruppo"
         break;
      case CAT_GRID:
         StrCategory = gsc_msg(531);    // "Griglia"
         break;
      case CAT_EXTERN:
         StrCategory = gsc_msg(536);    // "Simulazione"
         break;
      case CAT_SUBCLASS:
         StrCategory = gsc_msg(537);    // "Sottoclasse"
         break;
      case CAT_SPAGHETTI:         
         StrCategory = gsc_msg(534);    // "Spaghetti"
         break;
      case CAT_SECONDARY:         
         StrCategory = gsc_msg(538);    // "Tab.Secondaria"
         break;
      default:
         return GS_BAD;
   }

   return GS_GOOD;   
}

int gsc_getClsCategoryTypeDescr(int category, int type, C_STRING &tp_class)
{
   switch (category)
   {
      case CAT_SIMPLEX:
         if (gsc_getClsTypeDescr(type, tp_class) == GS_BAD) return GS_BAD;
         break;
      case CAT_GROUP:
         tp_class = gsc_msg(533);                    // "Gruppo"
         break;
      case CAT_GRID:
         tp_class = gsc_msg(531);                    // "Griglia"
         break;
      case CAT_EXTERN:
         tp_class = gsc_msg(536);                    // "Simulazione"
         break;
      case CAT_SUBCLASS:
         if (gsc_getClsTypeDescr(type, tp_class) == GS_BAD) return GS_BAD;
         //tp_class = gsc_msg(537);                    // "Sottoclasse"
         break;
      case CAT_SPAGHETTI:         
         tp_class = gsc_msg(534);                    // "Spaghetti"
         break;
      case CAT_SECONDARY:         
         tp_class = gsc_msg(538);                    // "Tab. Secondaria"
         break;
      default:
         return GS_BAD;
   }

   return GS_GOOD;   
}


/*********************************************************************/
/*.doc gsc_getSecTypeDescr                                <external> */
/*+
  Questa funzione restituisce una stringa che descrive il tipo
  della classe di GEOsim.
  Parametri:
  int      Type;     tipo di classe GEOsim
  C_STRING &tipo;    descrizione del tipo
  
  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_getSecTypeDescr(SecondaryTableTypesEnum Type, C_STRING &StrType)
{
   switch (Type)
   {
      case GSExternalSecondaryTable:
         StrType = gsc_msg(298);    // "esterna"
         break;
      case GSInternalSecondaryTable:
         StrType = gsc_msg(304);    // "GEOsim"
         break;
      default:
         return GS_BAD;
   }

   return GS_GOOD;   
}


/*********************************************************************/
/*.doc gsc_getConnectDescr                                <external> */
/*+
  Questa funzione restituisce una stringa che descrive le connessioni.
  Parametri:
  int      Connect;        flag bit a bit delle connessioni
  C_STRING &StrConnect;    descrizione delle connessioni
  
  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
void gsc_getConnectDescr(int Connect, C_STRING &StrConnect)
{
   if (Connect & NO_CONCT_CONTROL)
   {
      StrConnect = gsc_msg(67); // "Nessuna connessione"
      return;
   }

   StrConnect.clear();

   if (Connect & CONCT_START_END)
   {
      if (StrConnect.len() > 0) StrConnect += _T(" + ");
      StrConnect += gsc_msg(237); // "inizio/fine"
   }
   if (Connect & CONCT_MIDDLE)
   {
      if (StrConnect.len() > 0) StrConnect += _T(" + ");
      StrConnect += gsc_msg(354); // "punto intermedio"
   }
   if (Connect & CONCT_POINT)
   {
      if (StrConnect.len() > 0) StrConnect += _T(" + ");
      StrConnect += gsc_msg(355); // "punto di inserimento"
   }
   if (Connect & CONCT_VERTEX)
   {
      if (StrConnect.len() > 0) StrConnect += _T(" + ");
      StrConnect += gsc_msg(378); // "vertice di polilinea"
   }
   if (Connect & CONCT_ANY_POINT)
   {
      if (StrConnect.len() > 0) StrConnect += _T(" + ");
      StrConnect += gsc_msg(379); // "qualsiasi punto"
   }
   if (Connect & CONCT_CENTROID)
   {
      if (StrConnect.len() > 0) StrConnect += _T(" + ");
      StrConnect += gsc_msg(380); // "centroide"
   }
   if (Connect & NO_OVERLAP)
   {
      if (StrConnect.len() > 0) StrConnect += _T(" + ");
      StrConnect += gsc_msg(384); // "Nessuna sovrapposizione"
   }
   if (Connect & SOFT_SPLIT)
   {
      if (StrConnect.len() > 0) StrConnect += _T(" + ");
      StrConnect += gsc_msg(475); // "Spezza solo geometria"
   }
   if (Connect & HARD_SPLIT)
   {
      if (StrConnect.len() > 0) StrConnect += _T(" + ");
      StrConnect += gsc_msg(476); // "Dividi entità"
   }
}


/*********************************************************************/
/*.doc gsc_help                                           <external> */
/*+
  Questa funzione apro il file di help html per un detereminato contesto.
  Parametri:
  int iTopic;           Codice di contesto
  HWND WndCaller;       Finestra chiamante
  TCHAR *szFilename;    Nome del file di help html; opzionale (default = NULL).
                        Se non indicato sarà utilizzato il file di help html di GEOsim.
  
  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gs_help(void)
{
   int     iTopic = 0;
   TCHAR   *szFilename = NULL;
   presbuf arg = acedGetArgs();

   acedRetNil();
   if (arg)
   {
      if (arg->restype != RTSHORT) { GS_ERR_COD = eGSInvRBType; return RTERROR; }
      iTopic = arg->resval.rint;

      if ((arg = arg->rbnext))
      {
         if (arg->restype != RTSTR) { GS_ERR_COD = eGSInvRBType; return RTERROR; }
         szFilename = arg->resval.rstring;
      }
   }

   gsc_help(iTopic, NULL, szFilename);
   acedRetT();
   
   return RTNORM;
}
void gsc_help(int iTopic, HWND WndCaller, TCHAR *szFilename)
{
   C_STRING tmp_path;

   if (!szFilename)
      tmp_path = GEOsimAppl::WORKDIR + _T('\\') + GEODOCDIR + _T('\\') + _T("GEOSIM.CHM");
   else
      tmp_path = szFilename;

   // Controlla Correttezza Path
   if (gsc_nethost2drive(tmp_path) == GS_BAD) return;
   if (WndCaller == NULL) WndCaller = adsw_acadMainWnd();

   if (iTopic == 0)
      HtmlHelp(WndCaller, tmp_path.get_name(), HH_DISPLAY_TOC, DWORD(iTopic));
   else
      HtmlHelp(WndCaller, tmp_path.get_name(), HH_HELP_CONTEXT, DWORD(iTopic));

   return;
}


/*********************************************************/
/*.doc gsc_dd_get_existingDir                 <external> */
/*+                                                                       
  Permette la selezione di un direttorio esistente.
  Parametri:
  const TCHAR *Title;       Titolo (se = NULL; viene inserito testo standard)
  const TCHAR *CurrentDir;  Direttorio corrente (se = NULL; si usa la root "desktop")
  C_STRING   &OutDir;       Direttorio corrente scelto dall'utente

  Restituisce GS_GOOD caso di successo oppure GS_CAN per rinuncia o GS_BAD
  per errore. 
-*/  
/*********************************************************/
int gs_dd_get_existingDir(void)
{
   TCHAR    *Title = NULL, *CurrentDir = NULL;
   C_STRING OutDir;
   presbuf  arg = acedGetArgs();
   int      result;

   acedRetNil();
   if (arg)
   {
      if (arg->restype == RTSTR) Title = arg->resval.rstring;
      if ((arg = arg->rbnext))
         if (arg->restype == RTSTR) CurrentDir = arg->resval.rstring;
   }
   if ((result = gsc_dd_get_existingDir(Title, CurrentDir, OutDir)) == GS_BAD) return RTERROR;
   if (result == GS_GOOD) acedRetStr(OutDir.get_name());
   
   return RTNORM;
}
int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
   if (uMsg == BFFM_INITIALIZED)
   {
      SetWindowText(hwnd, gsc_msg(759)); // "Sfoglia per cartelle"
      if (lpData) SendMessage(hwnd, BFFM_SETSELECTION, 1, lpData);
   }
   return 0;
}
int gsc_dd_get_existingDir(const TCHAR *Title, const TCHAR *CurrentDir, C_STRING &OutDir) 
{ 
   BROWSEINFO   bi; 
   TCHAR        lpBuffer[MAX_PATH]; 
   LPITEMIDLIST pidlDirectories;  // PIDL for Directories 
   LPITEMIDLIST pidlBrowse;       // PIDL selected by user 
   CWnd         *pAcadWindow = CWnd::FromHandlePermanent(adsw_acadMainWnd()), *pChildWindow;
   HWND         hwnd;
   LPMALLOC     pIMalloc;
   C_STRING     InternalCurrDir;

   if (pAcadWindow == NULL) return GS_BAD;

	if (CoGetMalloc(MEMCTX_TASK, &pIMalloc) != NOERROR)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   if ((pChildWindow = pAcadWindow->GetActiveWindow()) != NULL)
      hwnd = pChildWindow->m_hWnd;
   else
      hwnd = pAcadWindow->m_hWnd;

   // Get the PIDL for the Programs folder. 
   if (!SUCCEEDED(SHGetSpecialFolderLocation(hwnd, CSIDL_DRIVES, &pidlDirectories)))
      { pIMalloc->Release(); return GS_BAD; }

   InternalCurrDir = CurrentDir;
   InternalCurrDir.alltrim();
   InternalCurrDir.strtran(_T("/"), _T("\\")); // converto i caratteri '/' in '\\'
   if (InternalCurrDir.len() > 0)
      if (InternalCurrDir.get_chr(InternalCurrDir.len() - 1) == _T('\\'))
         InternalCurrDir.set_chr(_T('\0'), InternalCurrDir.len() - 1); // elimino ultimo '\\'

   // Fill in the BROWSEINFO structure. 
   bi.hwndOwner      = hwnd; 
   bi.pidlRoot       = pidlDirectories; 
   bi.pszDisplayName = lpBuffer; 
   bi.lpszTitle      = (Title) ? Title : gsc_msg(758); // ""Selezionare la cartella."
   bi.ulFlags        = BIF_RETURNONLYFSDIRS + BIF_DONTGOBELOWDOMAIN; 
   bi.lpfn           = BrowseCallbackProc; 
   bi.lParam         = (LPARAM) InternalCurrDir.get_name(); 
 
    // Browse for a folder and return its PIDL. 
   pidlBrowse = SHBrowseForFolder(&bi); 

   pIMalloc->Free(pidlDirectories);

   if (pidlBrowse != NULL)
   {
      if (SHGetPathFromIDList(pidlBrowse, lpBuffer)) OutDir = lpBuffer;
      pIMalloc->Free(pidlBrowse);
      pIMalloc->Release();
   }
   else  // l'utente ha rinunciato alla selezione
   {
      pIMalloc->Release();
      return GS_CAN;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_DocumentExecute                    <external> */
/*+                                                                       
  Lancia la shell aprendo un documento.
  Parametri:
  const TCHAR *Document;   Path completa del documento da aprire o indirizzo
                           internet.
  int Sync;                Flag se = TRUE il processo viene avviato in modo sincrono
                           (Default = TRUE)

  Restituisce GS_GOOD caso di successo oppure GS_BAD per errore. 
-*/  
/*********************************************************/
int gsc_DocumentExecute(const TCHAR *Document, int Sync)
{
   CWnd *pAcadWindow = CWnd::FromHandlePermanent(adsw_acadMainWnd()), *pChildWindow;
   HWND hwnd;

   if ((pChildWindow = pAcadWindow->GetActiveWindow()) != NULL)
      hwnd = pChildWindow->m_hWnd;
   else
      hwnd = pAcadWindow->m_hWnd;

   SHELLEXECUTEINFO ShExeInfo = { sizeof(ShExeInfo), SEE_MASK_NOCLOSEPROCESS, hwnd, _T("open"),
                                  Document, NULL, NULL, SW_SHOWNORMAL, NULL };

   if (ShellExecuteEx(&ShExeInfo) == 0)
   {
      switch ((long) ShExeInfo.hInstApp)
      {
         case SE_ERR_FNF :
            GS_ERR_COD = eGSFileNoExisting;           
            break;
         case SE_ERR_PNF :
            GS_ERR_COD = eGSInvalidPath;
            break;
         case SE_ERR_OOM :
            GS_ERR_COD = eGSOutOfMem;
            break;
         case SE_ERR_ASSOCINCOMPLETE :
         case SE_ERR_NOASSOC :
            GS_ERR_COD = eGSNoFileAssoc;
            break;
         case SE_ERR_DDETIMEOUT :
         case SE_ERR_DDEFAIL :
         case SE_ERR_DDEBUSY :
            GS_ERR_COD = eGSDDEFailed;
            break;
         case SE_ERR_ACCESSDENIED :
         case SE_ERR_SHARE :
         case SE_ERR_DLLNOTFOUND :
         default :
            GS_ERR_COD = eGSOpenFile;
            break;
      }
      return GS_BAD;
   }

   if (Sync)
   {
      DWORD dummy; // finchè esiste il processo (attesa attiva)
      while (GetExitCodeProcess(ShExeInfo.hProcess, &dummy) != 0)
         if (dummy != STILL_ACTIVE) break;
         else Sleep(500); // Aspetta mezzo secondo
   }

   return GS_GOOD;
}

/*********************************************************/
/*.doc gsc_getLocale...                       <external> */
/*+                                                                       
  Funzione che legge l'impostazione locale di window per la
  lettura di:
  - Separatore decimale (gsc_getLocaleDecimalSep)
  - Separatore di lista (gsc_getLocaleListSep)

  C_STRING   &str;      valore letto

  Restituisce GS_GOOD caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_getLocaleInfo(LCTYPE LCType, C_STRING &str)
{
   TCHAR value[255];

   if (GetLocaleInfo(LOCALE_USER_DEFAULT, LCType, value, 255) == 0)
      return GS_BAD;
   str = value;

   return GS_GOOD;
}

// decimal separator
int gsc_getLocaleDecimalSep(C_STRING &str)
   { return gsc_getLocaleInfo(LOCALE_SDECIMAL, str); }
// thousand separator
int gsc_getLocaleThousandSep(C_STRING &str)
   { return gsc_getLocaleInfo(LOCALE_STHOUSAND, str); }
// digit grouping x;y;z;...
int gsc_getLocaleDigitGrouping(C_INT_LIST &GrpList)
{
   C_STRING str;
   TCHAR    *p;
   int      n, i;
   C_INT    *pGrp;

   GrpList.remove_all();
   if (gsc_getLocaleInfo(LOCALE_SGROUPING, str) == GS_BAD) return GS_BAD;
   p = str.get_name();
   n = gsc_strsep(p, _T('\0')) + 1;
   for (i = 1; i < n; i++)
   {
      if ((pGrp = new C_INT(_wtoi(p))) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      GrpList.add_tail(pGrp);
      while (*p != _T('\0')) p++; p++;
   }

   return GS_GOOD;
}
// leading zeros for decimal
int gsc_getLocaleLeadingZeros(C_STRING &str)
   { return gsc_getLocaleInfo(LOCALE_ILZERO, str); }
// negative number mode
// "0"     (1.1)
// "1"     -1.1
// "2"     - 1.1
// "3"     1.1-
// "4"     1.1 -
int gsc_getLocaleNegativeMode(C_STRING &str)
   { return gsc_getLocaleInfo(LOCALE_INEGNUMBER, str); }

// local monetary symbol
int gsc_getLocaleMonetarySymbol(C_STRING &str)
   { return gsc_getLocaleInfo(LOCALE_SCURRENCY, str); }
// local monetary decimal separator
int gsc_getLocaleMonetaryDecimalSep(C_STRING &str)
   { return gsc_getLocaleInfo(LOCALE_SMONDECIMALSEP, str); }
// local monetary thousand separator
int gsc_getLocaleMonetaryThousandSep(C_STRING &str)
   { return gsc_getLocaleInfo(LOCALE_SMONTHOUSANDSEP, str); }
// local monetary digit grouping x;y;z;...
int gsc_getLocaleMonetary(C_STRING &str)
   { return gsc_getLocaleInfo(LOCALE_SMONGROUPING, str); }
// local Positive monetary mode
// "0"  $1    Prefix, no separation.
// "1"  1$    Suffix, no separation.
// "2"  $ 1   Prefix, 1-character separation.
// "3"  1 $   Suffix, 1-character separation
int gsc_getLocalePositiveMonetaryMode(C_STRING &str)
   { return gsc_getLocaleInfo(LOCALE_ICURRENCY, str); }
// local Negative monetary mode
// "0"    ($1.1)
// "1"    -$1.1
// "2"    $-1.1
// "3"    $1.1-
// "4"    (1.1$)
// "5"    -1.1$
// "6"    1.1-$
// "7"    1.1$-
// "8"    -1.1 $ (space before $)
// "9"    -$ 1.1 (space after $)
// "10"   1.1 $- (space before $)
// "11"   $ 1.1- (space after $) 
// "12"   $ -1.1 (space after $) 
// "13"   1.1- $ (space before $) 
// "14"   ($ 1.1) (space after $) 
// "15"   (1.1 $) (space before $) 
int gsc_getLocaleNegativeMonetaryMode(C_STRING &str)
   { return gsc_getLocaleInfo(LOCALE_INEGCURR, str); }

// list item separator
int gsc_getLocaleListSep(C_STRING &str)
   { return gsc_getLocaleInfo(LOCALE_SLIST, str); }

// short date format string
int gsc_getLocaleShortDateFormat(C_STRING &str)
   { return gsc_getLocaleInfo(LOCALE_SSHORTDATE, str); }
// short date format string con anno espresso a 4 cifre
int gsc_getLocaleShortDateFormat4DgtYear(C_STRING &str)
{
   if (gsc_getLocaleShortDateFormat(str) == GS_BAD) return GS_BAD;
   // Mi assicuro che l'anno sia di 4 cifre
   if (str.at(_T("yyyy"), FALSE) == NULL)
      // se è espresso con 3 cifre
      if (str.at(_T("yyy"), FALSE)) str.strtran(_T("yyy"), _T("yyyy"), FALSE);
      else // altrimenti se è espresso con 2 cifre
         if (str.at(_T("yy"), FALSE)) str.strtran(_T("yy"), _T("yyyy"), FALSE);
      else // altrimenti se è espresso con 1 cifre
         if (str.at(_T("y"), FALSE)) str.strtran(_T("y"), _T("yyyy"), FALSE);
      else return GS_BAD;

   // Se non è specificato il giorno o il mese
   if (str.at(_T("d"), FALSE) == NULL || str.at(_T("m"), FALSE) == NULL)
      return GS_BAD;

   return GS_GOOD;
}
// long date format string
int gsc_getLocaleLongDateFormat(C_STRING &str)
   { return gsc_getLocaleInfo(LOCALE_SLONGDATE, str); }
// date separator
int gsc_getLocaleDateSep(C_STRING &str)
   { return gsc_getLocaleInfo(LOCALE_SDATE, str); }
// time format string
int gsc_getLocaleTimeFormat(C_STRING &str)
   { return gsc_getLocaleInfo(LOCALE_STIMEFORMAT, str); }
// time separator
int gsc_getLocaleTimeSep(C_STRING &str)
   { return gsc_getLocaleInfo(LOCALE_STIME, str); }


/*********************************************************/
/*.doc gsc_LoadOleDBIniStrFromFile            <external> */
/*+                                                                       
  Funzione che legge da un file .UDL la stringa di inizializzazione
  del collegamento OLEDB.
  const TCHAR *Path;      Path del file .UDL

  Restituisce la stringa di configurazione in caso di successo altrimenti NULL.
  N.B. Alloca memoria.
-*/  
/*********************************************************/
TCHAR *gsc_LoadOleDBIniStrFromFile(const TCHAR *Path)
{
	USES_CONVERSION;
   IDataInitialize *DataInit = NULL;
   LPOLESTR        pInitializationString;
   C_STRING        full_path;
   TCHAR           *dummy;

   full_path = Path;
   // Controlla Correttezza Path (potrebbe essere una stringa di connessione)
   if (gsc_nethost2drive(full_path, GS_BAD) == GS_BAD) return NULL; 

   if (CoCreateInstance(CLSID_MSDAINITIALIZE, NULL, CLSCTX_INPROC_SERVER, 
                        IID_IDataInitialize, (LPVOID *)&DataInit) != S_OK)
      return NULL;

   if (DataInit->LoadStringFromStorage(T2COLE(full_path.get_name()), &pInitializationString) != S_OK)
   {
      // forse il file è crittografato quindi lo scrivo temporaneamente descriptato, lo leggo e lo cancello
      C_STRING Dir, DekriptFile;

      Dir = GEOsimAppl::CURRUSRDIR;
      Dir += _T('\\');
      Dir += GEOTEMPDIR;
      if (gsc_get_tmp_filename(Dir.get_name(), _T("DEKRIPT"), _T(".UDL"), DekriptFile) == GS_BAD)
         return NULL;
      if (gsc_DeKriptFile(full_path, DekriptFile) == GS_BAD)
         { DataInit->Release(); return NULL; }

      if (DataInit->LoadStringFromStorage(T2COLE(DekriptFile.get_name()), &pInitializationString) != S_OK)
      {
         gsc_delfile(DekriptFile, ONETEST);
         DataInit->Release();
         return NULL;
      }
      gsc_delfile(DekriptFile, ONETEST);
   }

   dummy = gsc_tostring(OLE2T(pInitializationString));

   CoTaskMemFree(pInitializationString);
	DataInit->Release();

   return dummy;
}


/*********************************************************/
/*.doc gsc_WriteOleDBIniStrToFile             <external> */
/*+                                                                       
  Funzione che scrive in un file .UDL la stringa di inizializzazione
  del collegamento OLEDB.
  const TCHAR *ConnStr;   Stringa di connessione al database
  const TCHAR *Path;      Path del file .UDL

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_WriteOleDBIniStrToFile(const TCHAR *ConnStr, const TCHAR *Path)
{
	USES_CONVERSION;
   IDataInitialize *DataInit = NULL;
   C_STRING        full_path;

   full_path = Path;
   // Controlla Correttezza Path
   if (gsc_nethost2drive(full_path) == GS_BAD) return GS_BAD; 

   if (CoCreateInstance(CLSID_MSDAINITIALIZE, NULL, CLSCTX_INPROC_SERVER, 
                        IID_IDataInitialize, (LPVOID *)&DataInit) != S_OK)
      return GS_BAD;

   if (DataInit->WriteStringToStorage(T2COLE(full_path.get_name()), T2COLE(ConnStr),
                                      CREATE_ALWAYS) != S_OK)
      { DataInit->Release(); return GS_BAD; }

	DataInit->Release();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_SearchACADUDLFile                  <external> */
/*+                                                                       
  Funzione che cerca un file .UDL nel direttorio dei "data source file" 
  di AutoCAD che abbia un prefisso noto (normalmente il nome del DBMS).
  Fra tutti i files con prefisso noto viene restituito quello avente le
  stesse proprietà di collegamento indicate.
  Parametri:
  const TCHAR *Prefix;   Prefisso
  const TCHAR *ConnStr;  Stringa di connessione

  Restituisce il nome del file in caso di successo altrimenti NULL.
  N.B. Alloca memoria.
-*/  
/*********************************************************/
TCHAR *gsc_SearchACADUDLFile(const TCHAR *Prefix, const TCHAR *ConnStr)
{
   C_STRING Dir, Path, CompConnStr;
   long     n_adir;
   presbuf  p, filename;
   TCHAR    *Result = NULL;

   Dir = GEOsimAppl::ASE->getAcadDsPath(); // dir dei data source file
   Dir += _T('\\');

   Path = Dir;
   if (Prefix) Path += Prefix;
   Path += _T("*.UDL");

   if ((n_adir = gsc_adir(Path.get_name(), &filename)) > 0)
   {
      p = filename;
      p = p->rbnext;
      while (p && p->restype == RTSTR)
      {
         Path = Dir;
         Path += p->resval.rstring;

         // leggo proprietà di connessione
         CompConnStr.paste(gsc_LoadOleDBIniStrFromFile(Path.get_name()));
         if (CompConnStr.comp(ConnStr) == 0)
         {
            Result = gsc_tostring(p->resval.rstring);
            break;
         }
         p = p->rbnext;
      }
      acutRelRb(filename);
   }

   return Result;
}


/*********************************************************/
/*.doc gsc_CreateACADUDLFile                  <external> */
/*+                                                                       
  Funzione che scrive un nuovo file .UDL nel direttorio dei 
  "data source file" avente un prefisso noto.
  Parametri:
  const TCHAR *Prefix;   Prefisso
  const TCHAR *ConnStr;  Stringa di connessione

  Restituisce il nome del nuovo file in caso di successo altrimenti NULL.
  N.B. Alloca memoria.
-*/  
/*********************************************************/
TCHAR *gsc_CreateACADUDLFile(const TCHAR *Prefix, const TCHAR *ConnStr)
{
   TCHAR *Result;

   // dir dei data source file
   if (gsc_get_tmp_filename(GEOsimAppl::ASE->getAcadDsPath(), Prefix, _T(".UDL"), &Result) == GS_BAD)
      return NULL;
   if (gsc_WriteOleDBIniStrToFile(ConnStr, Result) == GS_BAD)
      { free(Result); return NULL; }

   return Result;
}


/*********************************************************/
/*.doc gsc_long2base36                        <external> */
/*+                           
   N.B. Rifatta da Paolino dopo baco Italcogim di 
   trasformazione numeri per nome dwg di GEOsim.

   Funzione che dato un numero decimale restituisce la 
   stringa che contiene la sua trasformazione in base 36 e
   gli spazi giusti per completare il nome del file di GEOsim.

   Parametri:
      TCHAR  **number_base36  risultato della trasformazione;
      long     number         numero da convertire da decimale a base36;
      size_t   lenStr         lunghezza totale della stringa.

  Restituisce GS_GOOD caso di successo oppure GS_BAD. 
-*/  
/*********************************************************/
int gsc_long2base36(TCHAR** number_base36, long number, size_t lenStr)
{
   TCHAR    *res = NULL;
   size_t   i = 0, len = 0;
   C_STRING str, result;

   // Per prima cosa verifico il parametro
   if ((number < 0) || (number >= (int) pow(36.0, (int) lenStr)))
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   // Richiamo ore la funzione base di trasformazione del numero da decimale a base 36
   if (gsc_decimal2radix(number, 36, &res) == GS_BAD) return GS_BAD;

   if (res != NULL)
   {
      str = res;
      free(res);
      // Se la conversione è avvenuta regolarmente allora vedo se aggiungere gli '0'
      len = str.len();
      result.clear();
      for (i = 0; i < (lenStr - len); i++)
         result += _T('0');
      result += str;
      *number_base36 = gsc_tostring(result.get_name());
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_ddSuggestDBValue                   <external> */
/*+                           
   Funzione che suggerisce un valore da inserire in DataBase
   tramite interfaccia grafica.
   Per tipo carattere viene proposta la scelta di una path di file.
   Per tipo numerico viene attivata la calcolatrice.
   Per tipo data viene attivato un calendario.

   Parametri:
   C_STRING &Value;
   DataTypeEnum DataType;

   Restituisce GS_GOOD caso di successo oppure GS_BAD. 
-*/  
/*********************************************************/
int gsc_ddSuggestDBValue(C_STRING &Value, DataTypeEnum DataType)
{
   if (gsc_DBIsNumeric(DataType) == GS_GOOD)
   {
      if (OpenClipboard(NULL) != 0) // Apertura OK
      {
         EmptyClipboard();
         CloseClipboard();

         if (gsc_DocumentExecute(_T("CALC.EXE"), TRUE) == GS_BAD) return GS_BAD;
         if (OpenClipboard(NULL) != 0) // Apertura OK
         {
            UINT Format = EnumClipboardFormats(0);

            if (Format == CF_UNICODETEXT || Format == CF_TEXT)
            {
               HANDLE m_hMemory = GetClipboardData(Format);

               if (Format == CF_UNICODETEXT)
               {
               	USES_CONVERSION;
                  LPOLESTR pstr = (LPOLESTR) GlobalLock(m_hMemory);
                  Value = OLE2T(pstr);
               }
               else
               {
                  LPSTR pstr = (LPSTR) GlobalLock(m_hMemory);
                  Value = pstr;
               }

               GlobalUnlock(m_hMemory);
            }
   
            CloseClipboard();
         }
      }
   }
   else
   if (gsc_DBIsChar(DataType) == GS_GOOD)
   {
      C_STRING dummy(Value), LastPath;

      if (dummy.len() == 0)
         // Se non esiste alcun file precedente
         if (gsc_getPathInfoFromINI(_T("LastSuggestedFile"), LastPath) == GS_GOOD &&
             gsc_dir_exist(LastPath, ONETEST) == GS_GOOD)
            dummy = LastPath;

      // Controlla Correttezza Path
      if (gsc_nethost2drive(dummy) == GS_BAD) return GS_BAD; 

      // "GEOsim - Seleziona file"
      if (gsc_GetFileD(gsc_msg(645), dummy, GS_EMPTYSTR, 4, Value) == RTNORM)
      {
         if (gsc_drive2nethost(Value) == GS_BAD) return GS_BAD;
         // memorizzo la scelta in GEOSIM.INI per riproporla la prossima volta
         gsc_setPathInfoToINI(_T("LastSuggestedFile"), Value);
      }
   }
   else
   if (gsc_DBIsDate(DataType) == GS_GOOD || gsc_DBIsTimestamp(DataType) == GS_GOOD)
      gsc_ddChooseDate(Value);

   return GS_GOOD;
}


/*************************************************************************/
/*.doc int gsc_getInfoFromINI                                            */
/*+ 
   Funzione che restituisce il valore della <entry> dal file di
   inizializzazione di GEOsim. Se non specificato verrà usato il
   file GS_INI_FILE nei seguenti percorsi:
   1) nella cartella della sessione corrente (se esistente)
   2) nella cartella locale dell'utente corrente
   3) nella cartella del progetto
   3) nella cartella server

   Le entry fin'ora utilizzate sono:
   tutte quelle usate dalla funzione gsc_getPathInfoFromINI
   LastUsedPrj
   LastCheckSessionOnLogin

   Parametri:
   const TCHAR *entry;
   TCHAR       *value;      stringa di output
   int         dim_value;   dimensione di allocamento di <value>
   C_STRING    *IniFile;    Opzionale; file di inizializzazione, se non viene
                            fornito viene usato quello standard di GEOsim (default = NULL)
   
   oppure

   const TCHAR *entry;
   C_STRING    &value;
   C_STRING    *IniFile;    Opzionale; file di inizializzazione, se non viene
                            fornito viene usato quello standard di GEOsim (default = NULL)

   Ritorna GS_BAD in caso di errore, GS_GOOD altrimenti.
-*/
/*************************************************************************/
int gsc_getInfoFromINI(const TCHAR *entry, TCHAR *value, int dim_value, C_STRING *IniFile)
{
   C_STRING target;

   if (gsc_getInfoFromINI(entry, target, IniFile) == GS_BAD) return GS_BAD;
   gsc_strcpy(value, target.get_name(), dim_value);

   return GS_GOOD;
}
int gsc_getInfoFromINI(const TCHAR *entry, C_STRING &value, C_STRING *IniFile)
{
   C_STRING pathfile;

   if (IniFile) // se viene passato come parametro
      pathfile = IniFile->get_name();
   else
   {
      // se esiste una sessione di lavoro corrente
      if (GS_CURRENT_WRK_SESSION)
      {  
         // leggo da GEOSIM.INI nella cartella della sessione di lavoro
         pathfile = GS_CURRENT_WRK_SESSION->get_dir();
         pathfile += _T('\\');
         pathfile += GS_INI_FILE;
         // se esiste il file e la entry nel file
         if (gsc_path_exist(pathfile) == GS_GOOD &&
             gsc_get_profile(pathfile, GS_INI_LABEL, entry, value) == GS_GOOD)
            { value.alltrim(); return GS_GOOD; }
      }
         
      // leggo da GEOSIM.INI nella cartella locale dell'utente corrente
      pathfile = GEOsimAppl::CURRUSRDIR; 
      pathfile += _T('\\');
      pathfile += GS_INI_FILE;
      // se esiste il file e la entry nel file
      if (gsc_path_exist(pathfile) == GS_GOOD &&
          gsc_get_profile(pathfile, GS_INI_LABEL, entry, value) == GS_GOOD)
         { value.alltrim(); return GS_GOOD; }

      // se esiste una sessione di lavoro corrente
      if (GS_CURRENT_WRK_SESSION)
      {  
         // leggo da GEOSIM.INI nella cartella del progetto corrente
         pathfile = GS_CURRENT_WRK_SESSION->get_pPrj()->get_dir();
         pathfile += _T('\\');
         pathfile += GS_INI_FILE;
         // se esiste il file e la entry nel file
         if (gsc_path_exist(pathfile) == GS_GOOD &&
             gsc_get_profile(pathfile, GS_INI_LABEL, entry, value) == GS_GOOD)
            { value.alltrim(); return GS_GOOD; }
      }

      // leggo da GEOSIM.INI nella cartella server
      pathfile = GEOsimAppl::GEODIR; 
      pathfile += _T('\\');
      pathfile += GS_INI_FILE;
   }

   if (gsc_path_exist(pathfile) == GS_BAD) return GS_BAD;
   if (gsc_get_profile(pathfile, GS_INI_LABEL, entry, value) == GS_BAD)
      return GS_BAD;
   value.alltrim();

   return GS_GOOD;
}


/*************************************************************************/
/*.doc int   gsc_setInfoToINI                                            */
/*+ 
   Funzione che scrive il valore della <entry> nel file di
   inizializzazione di GEOsim. Se non specificato verrà usato il
   file GS_INI_FILE nei seguenti percorsi:
   1) nella cartella della sessione corrente (se esistente)
   2) nella cartella locale dell'utente corrente
   Parametri:
   const TCHAR *entry;
   TCHAR       *value;
   C_STRING    *IniFile;    Opzionale; file di inizializzazione, se non viene
                            fornito viene usato quello standard di GEOsim (default = NULL)
   
   oppure

   const TCHAR *entry;
   C_STRING    &value;
   C_STRING    *IniFile;    Opzionale; file di inizializzazione, se non viene
                            fornito viene usato quello standard di GEOsim (default = NULL)

   Ritorna GS_BAD in caso di errore, GS_GOOD altrimenti.
-*/
/*************************************************************************/
int gsc_setInfoToINI(const TCHAR *entry, C_STRING &value, C_STRING *IniFile)
{
   return gsc_setInfoToINI(entry, value.get_name(), IniFile);
}
int gsc_setInfoToINI(const TCHAR *entry, TCHAR *value, C_STRING *IniFile)
{
   C_STRING pathfile;

   if (IniFile) // se viene passato come parametro
      pathfile = IniFile->get_name();
   else
   {
      // se esiste una sessione di lavoro corrente
      if (GS_CURRENT_WRK_SESSION)
      {  // scrivo in GEOSIM.INI nella cartella della sessione di lavoro
         pathfile = GS_CURRENT_WRK_SESSION->get_dir();
         pathfile += _T('\\');
         pathfile += GS_INI_FILE;

         return gsc_set_profile(pathfile.get_name(), GS_INI_LABEL, entry, value);
      }

      pathfile = GEOsimAppl::CURRUSRDIR; // Directory locale dell'utente corrente
      pathfile += _T('\\');
      pathfile += GS_INI_FILE;
   }

   return gsc_set_profile(pathfile.get_name(), GS_INI_LABEL, entry, value);
}


/*************************************************************************/
/*.doc int gsc_getPathInfoFromINI e gsc_setPathInfoToINI                 */
/*+ 
   Funzione che scrive e legge delle path nel file di inizializzazione di GEOsim.
   Le entry fin'ora utilizzate sono:
   LastCartFile
   LastClassStruExportFile
   LastDefinitionClassFile
   LastFilterExportFile
   LastLayerConfigFile
   LastLispFile
   LastKeyDWG
   LastRefBlockFile
   LastSelClassesFile
   LastSpatialQryFile
   LastSQLDir
   LastSQLQryFile
   LastThmQryFile
   LastINIPlotFile
   LastSuggestedFile
   LastSensorFile
   LastUsedLyrDisplayModelFile
   LastUsedTopoResistanceFile

   Parametri:
   (gsc_getPathInfoFromINI <entry>[IniFile])
   (gsc_setPathInfoToINI <entry><value>[IniFile])

   Ritorna GS_BAD in caso di errore, GS_GOOD altrimenti.
   N.B. se esiste una sessione di lavoro corrente il file di inizializzazione di
        GEOsim verrà cercato nel direttorio del progetto corrente.
-*/
/*************************************************************************/
int gs_getPathInfoFromINI(void)
{
   presbuf  arg = acedGetArgs();
   TCHAR    *entry;
   C_STRING value;
   
   acedRetNil();

   if (!arg) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (arg->restype != RTSTR) { GS_ERR_COD = eGSInvRBType; return RTERROR; }
   entry = arg->resval.rstring;

   if ((arg = arg->rbnext) && arg->restype == RTSTR) // file INI opzionale
   {
      C_STRING IniFile(arg->resval.rstring);
      if (gsc_getPathInfoFromINI(entry, value, &IniFile) == GS_BAD) return RTNORM;
   }
   else
      if (gsc_getPathInfoFromINI(entry, value) == GS_BAD) return RTNORM;

   acedRetStr(value.get_name());

   return RTNORM;
}
int gsc_getPathInfoFromINI(const TCHAR *entry, C_STRING &value, C_STRING *IniFile)
{  
   if (gsc_getInfoFromINI(entry, value, IniFile) == GS_BAD) return GS_BAD;
   if (gsc_nethost2drive(value) == GS_BAD) value = GS_EMPTYSTR;
 
   return GS_GOOD;
}
int gs_setPathInfoToINI(void)
{
   presbuf  arg = acedGetArgs();
   TCHAR    *entry;
   C_STRING value;
   
   acedRetNil();
   if (!arg) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (arg->restype != RTSTR) { GS_ERR_COD = eGSInvRBType; return RTERROR; }
   entry = arg->resval.rstring;

   if (!(arg = arg->rbnext)) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (value.paste(gsc_rb2str(arg)) == NULL)
      { GS_ERR_COD = eGSInvRBType; return RTERROR; }

   if ((arg = arg->rbnext) && arg->restype == RTSTR) // file INI opzionale
   {
      C_STRING IniFile(arg->resval.rstring);
      if (gsc_setPathInfoToINI(entry, value, &IniFile) == GS_BAD) return RTNORM;
   }
   else
      if (gsc_setPathInfoToINI(entry, value) == GS_BAD) return RTNORM;
   acedRetT();

   return RTNORM;
}
int gsc_setPathInfoToINI(const TCHAR *entry, C_STRING &value, C_STRING *IniFile)
{
   C_STRING dummy(value);
   
   if (gsc_drive2nethost(dummy) == GS_BAD) return GS_BAD;
   return gsc_setInfoToINI(entry, dummy, IniFile);
}


/*********************************************************/
// INIZIO FUNZIONI DI NOTIFICA IN RETE                   //
/*********************************************************/


/*********************************************************/
/*.doc gsc_getComputerName                   <external> */
/*+                           
   Funzione che ritorna il nome del computer.
   Parametri:
   C_STRING &Value;

   Restituisce GS_GOOD caso di successo oppure GS_BAD. 
-*/  
/*********************************************************/
int gsc_getComputerName(C_STRING &ComputerName)
{
   LPTSTR lpszSystemInfo;     // pointer to system information string 
   DWORD  cchBuff = 256;
   TCHAR  tchBuffer[256];  // buffer for expanded string
    
   lpszSystemInfo = tchBuffer; 
 
   // Get and display the name of the computer. 
   if (GetComputerName(lpszSystemInfo, &cchBuff) == 0) return GS_BAD;
   ComputerName = lpszSystemInfo;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_setWaitFor                         <external> */
/*+                           
   Funzione che salva su file una lista di classi in attesa sul computer attuale.
   Parametri:
   const TCHAR *sez;        Nome sezione ("WAIT FOR EXTRACTION" o "WAIT FOR SAVE")
   int         prj;         Codice progetto
   C_INT_LIST  &ClsList;    Lista classi

   Restituisce GS_GOOD caso di successo oppure GS_BAD. 
-*/  
/*********************************************************/
int gsc_setWaitFor(const TCHAR *sez, int prj, C_INT_LIST &ClsList)
{
   C_STRING   FileName, ComputerName, dummy;
   FILE       *file;
   C_STR_LIST StrClsList;
   C_STR      *pStrCls;
   C_INT      *pCls;
   C_PROJECT  *pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj);
   int        PrevErr = GS_ERR_COD;

   if (gsc_getComputerName(ComputerName) == GS_BAD) return GS_BAD;
   
   FileName = pPrj->get_dir();
   FileName += _T('\\');
   FileName += GEOSUPPORTDIR;

   gsc_mkdir(FileName); // per sicurezza

   FileName += _T('\\');
   FileName += GS_NOTIFY_FILE;

   if (gsc_path_exist(FileName) == GS_GOOD)
   {  // Leggo eventuale lista di classi già memorizzate
      if ((file = gsc_open_profile(FileName, READONLY)) != NULL)
      {
         if (gsc_get_profile(file, sez, ComputerName.get_name(), dummy) == GS_BAD)
             GS_ERR_COD = PrevErr;
         gsc_close_profile(file);
         if (StrClsList.from_str(dummy.get_name()) == GS_BAD)
            { GS_ERR_COD = PrevErr; return GS_BAD; }
      }
   }

   // Aggiungo le classi passate come parametro (senza duplicazioni)
   pCls = (C_INT *) ClsList.get_head();
   while (pCls)
   {
      dummy = pCls->get_key();
      if (StrClsList.search_name(dummy.get_name()) == NULL)
      {
         if ((pStrCls = new C_STR(dummy.get_name())) == NULL)
            { GS_ERR_COD = PrevErr; return GS_BAD; }
         StrClsList.add_tail(pStrCls);
      }

      pCls = (C_INT *) ClsList.get_next();
   }

   // Scrivo le classi nel file
   dummy = StrClsList.to_str();

   if (gsc_set_profile(FileName.get_name(), sez, ComputerName.get_name(), dummy.get_name()) == GS_BAD)
      { GS_ERR_COD = PrevErr; return GS_BAD; }
   GS_ERR_COD = PrevErr;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_NotifyWaitFor                      <external> */
/*+                           
   Funzione che notifica un messaggio ai computer in attesa tranne a se stessi.
   Parametri:
   int        prj;         Codice progetto
   C_INT_LIST &ClsList;    Lista classi
   const TCHAR *sez;       Nome sezione ("WAIT FOR EXTRACTION" o "WAIT FOR SAVE")
   const TCHAR *msg;       Messaggio da inviare

   Restituisce GS_GOOD caso di successo oppure GS_BAD. 
-*/  
/*********************************************************/
int gsc_NotifyWaitFor(int prj, C_INT_LIST &ClsList, const TCHAR *sez, const TCHAR *msg)
{
   C_STRING    FileName, Msg, dummy, Buffer, MyComputerName;
   C_2STR_LIST EntryList;
   C_2STR      *pEntry;
   FILE        *file;
   C_STR_LIST  StrClsList;
   C_INT       *pCls;
   int         ToNotify, PrevErr = GS_ERR_COD, ReWrite = FALSE;
   C_PROJECT   *pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj);
   bool        Unicode;

   if (gsc_getComputerName(MyComputerName) == GS_BAD) return GS_BAD;
   
   FileName = pPrj->get_dir();
   FileName += _T('\\');
   FileName += GEOSUPPORTDIR;

   gsc_mkdir(FileName); // per sicurezza

   FileName += _T('\\');
   FileName += GS_NOTIFY_FILE;

   if ((file = gsc_open_profile(FileName, READONLY, MORETESTS, &Unicode)) == NULL)
      { GS_ERR_COD = PrevErr; return GS_BAD; }
   if (gsc_get_profile(file, sez, EntryList, Unicode) == GS_BAD)
      { gsc_close_profile(file); GS_ERR_COD = PrevErr; return GS_BAD; }
   if (gsc_close_profile(file) == GS_BAD)
      { GS_ERR_COD = PrevErr; return GS_BAD; }

   pEntry = (C_2STR *) EntryList.get_head();
   while (pEntry)
   {  // per ogni computer in attesa
      if (StrClsList.from_str(pEntry->get_name2()) == GS_BAD)
         { GS_ERR_COD = PrevErr; return GS_BAD; }

      ToNotify = FALSE;
      Buffer   = GS_EMPTYSTR;
      pCls = (C_INT *) ClsList.get_head();
      while (pCls)
      {
         dummy = pCls->get_key();
         if (StrClsList.search_name(dummy.get_name()) != NULL)
         {
            ToNotify = TRUE;
            ReWrite  = TRUE;
            StrClsList.remove_at();
            pCls = (C_INT *) ClsList.get_cursor();
         }
         else
            pCls = (C_INT *) ClsList.get_next();
      }
      Buffer.paste(StrClsList.to_str());
      pEntry->set_name2(Buffer.get_name());

      // Se si deve notificare (non a se stessi)
      if (ToNotify && MyComputerName.comp(pEntry->get_name()) != 0)
         gsc_Notify(pEntry->get_name(), msg); // notifica

      pEntry = (C_2STR *) EntryList.get_next();
   }

   // Riscrivo il file senza le classi appena considerate
   if (ReWrite)
   {
      if ((file = gsc_open_profile(FileName)) == NULL)
         { GS_ERR_COD = PrevErr; return GS_BAD; }
      if (gsc_set_profile(file, sez, EntryList) == GS_BAD)
         { gsc_close_profile(file); GS_ERR_COD = PrevErr; return GS_BAD; }
      if (gsc_close_profile(file) == GS_BAD) 
         { GS_ERR_COD = PrevErr; return GS_BAD; }
   }

   GS_ERR_COD = PrevErr;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_isWaitingFor                       <external> */
/*+                           
   Funzione che verifica se una lista di classi è già in attesa.
   Parametri:
   int        prj;         Codice progetto
   C_INT_LIST &ClsList;    Lista classi
   const TCHAR *sez;       Nome sezione ("WAIT FOR EXTRACTION" o "WAIT FOR SAVE")

   Restituisce GS_GOOD caso di successo oppure GS_BAD. 
-*/  
/*********************************************************/
int gsc_isWaitingFor(int prj, C_INT_LIST &ClsList, const TCHAR *sez)
{
   C_STRING   FileName, ComputerName, dummy;
   FILE       *file;
   C_STR_LIST StrClsList;
   C_INT      *pCls;
   C_PROJECT  *pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj);
   int        PrevErr = GS_ERR_COD;

   if (gsc_getComputerName(ComputerName) == GS_BAD)
      { GS_ERR_COD = PrevErr; return GS_BAD; }
     
   FileName = pPrj->get_dir();
   FileName += _T('\\');
   FileName += GEOSUPPORTDIR;

   gsc_mkdir(FileName); // per sicurezza

   FileName += _T('\\');
   FileName += GS_NOTIFY_FILE;

   if (gsc_path_exist(FileName) == GS_GOOD)
   {  // Leggo eventuale lista di classi già memorizzate
      if ((file = gsc_open_profile(FileName, READONLY)) != NULL)
      {
         if (gsc_get_profile(file, sez, ComputerName.get_name(), dummy) == GS_BAD)
            GS_ERR_COD = PrevErr;
         gsc_close_profile(file);
         if (StrClsList.from_str(dummy.get_name()) == GS_BAD)
            { GS_ERR_COD = PrevErr; return GS_BAD; }
      }
   }

   // Verifico tutte le classi passate come parametro
   pCls = (C_INT *) ClsList.get_head();
   while (pCls)
   {
      dummy = pCls->get_key();
      if (StrClsList.search_name(dummy.get_name()) == NULL)
         { GS_ERR_COD = PrevErr; return GS_BAD; }

      pCls = (C_INT *) ClsList.get_next();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_setWaitForExtraction               <external> */
/*+                           
   Funzione che salva su file una lista di classi in attesa di essere estratte
   sul computer attuale.
   Parametri:
   int        prj;         Codice progetto
   C_INT_LIST &ClsList;    Lista classi

   Restituisce GS_GOOD caso di successo oppure GS_BAD. 
-*/  
/*********************************************************/
int gsc_setWaitForExtraction(int prj, int cls)
{
   C_INT_LIST ClsList;
   C_INT      *pCls;

   if ((pCls = new C_INT(cls)) == NULL) return GS_BAD;
   ClsList.add_tail(pCls);

   return gsc_setWaitForExtraction(prj, ClsList);
}
int gsc_setWaitForExtraction(int prj, C_INT_LIST &ClsList)
   { return gsc_setWaitFor(_T("WAIT FOR EXTRACTION"), prj, ClsList); }


/*********************************************************/
/*.doc gsc_NotifyWaitForExtraction            <external> */
/*+                           
   Funzione che notifica un messaggio ai computer in attesa di estrazione.
   Parametri:
   int         prj;         Codice progetto
   C_INT_LIST  &ClsList;    Lista classi
   const TCHAR *msg;        Messaggio da inviare

   Restituisce GS_GOOD caso di successo oppure GS_BAD. 
-*/  
/*********************************************************/
int gsc_NotifyWaitForExtraction(int prj, int cls, const TCHAR *msg)
{
   C_INT_LIST ClsList;
   C_INT      *pCls;

   if ((pCls = new C_INT(cls)) == NULL) return GS_BAD;
   ClsList.add_tail(pCls);

   return gsc_NotifyWaitForExtraction(prj, ClsList, msg);
}
int gsc_NotifyWaitForExtraction(int prj, C_INT_LIST &ClsList, const TCHAR *msg)
   { return gsc_NotifyWaitFor(prj, ClsList, _T("WAIT FOR EXTRACTION"), msg); }


/*********************************************************/
/*.doc gsc_isWaitingForExtraction             <external> */
/*+                           
   Funzione che verifica se la lista delle classi è già in attesa dell'estrazione.
   Parametri:
   int        prj;         Codice progetto
   C_INT_LIST &ClsList;    Lista classi

   Restituisce GS_GOOD caso di successo oppure GS_BAD. 
-*/  
/*********************************************************/
int gsc_isWaitingForExtraction(int prj, int cls)
{
   C_INT_LIST ClsList;
   C_INT      *pCls;

   if ((pCls = new C_INT(cls)) == NULL) return GS_BAD;
   ClsList.add_tail(pCls);

   return gsc_isWaitingForExtraction(prj, ClsList);
}
int gsc_isWaitingForExtraction(int prj, C_INT_LIST &ClsList)
   { return gsc_isWaitingFor(prj, ClsList, _T("WAIT FOR EXTRACTION")); }


/*********************************************************/
/*.doc gsc_setWaitForSave                     <external> */
/*+                           
   Funzione che salva su file una lista di classi in attesa di essere salvate
   sul computer attuale.
   Parametri:
   int        prj;         Codice progetto
   C_INT_LIST &ClsList;    Lista classi

   Restituisce GS_GOOD caso di successo oppure GS_BAD. 
-*/  
/*********************************************************/
int gsc_setWaitForSave(int prj, int cls)
{
   C_INT_LIST ClsList;
   C_INT      *pCls;

   if ((pCls = new C_INT(cls)) == NULL) return GS_BAD;
   ClsList.add_tail(pCls);

   return gsc_setWaitForSave(prj, ClsList);
}
int gsc_setWaitForSave(int prj, C_INT_LIST &ClsList)
   { return gsc_setWaitFor(_T("WAIT FOR SAVE"), prj, ClsList); }


/*********************************************************/
/*.doc gsc_NotifyWaitForSave                  <external> */
/*+                           
   Funzione che notifica un messaggio ai computer in attesa del salvataggio.
   Parametri:
   int         prj;         Codice progetto
   C_INT_LIST  &ClsList;    Lista classi
   const TCHAR *msg;        Messaggio da inviare

   Restituisce GS_GOOD caso di successo oppure GS_BAD. 
-*/  
/*********************************************************/
int gsc_NotifyWaitForSave(int prj, int cls, const TCHAR *msg)
{
   C_INT_LIST ClsList;
   C_INT      *pCls;

   if ((pCls = new C_INT(cls)) == NULL) return GS_BAD;
   ClsList.add_tail(pCls);

   return gsc_NotifyWaitForSave(prj, ClsList, msg);
}
int gsc_NotifyWaitForSave(int prj, C_INT_LIST &ClsList, const TCHAR *msg)
   { return gsc_NotifyWaitFor(prj, ClsList, _T("WAIT FOR SAVE"), msg); }


/*********************************************************/
/*.doc gsc_isWaitingForSave                   <external> */
/*+                           
   Funzione che verifica se la lista delle classi è già in attesa del salvataggio.
   Parametri:
   int        prj;         Codice progetto
   C_INT_LIST &ClsList;    Lista classi

   Restituisce GS_GOOD caso di successo oppure GS_BAD. 
-*/  
/*********************************************************/
int gsc_isWaitingForSave(int prj, int cls)
{
   C_INT_LIST ClsList;
   C_INT      *pCls;

   if ((pCls = new C_INT(cls)) == NULL) return GS_BAD;
   ClsList.add_tail(pCls);

   return gsc_isWaitingForSave(prj, ClsList);
}
int gsc_isWaitingForSave(int prj, C_INT_LIST &ClsList)
   { return gsc_isWaitingFor(prj, ClsList, _T("WAIT FOR SAVE")); }


/*********************************************************/
/*.doc gsc_Notify                             <external> */
/*+                           
   Funzione che notifica un messaggio ad un computer.
   Parametri:
   const TCHAR *computer;      Nome computer
   const TCHAR *msg;           Messaggio da inviare

   Restituisce GS_GOOD caso di successo oppure GS_BAD. 
-*/  
/*********************************************************/
int gsc_Notify(const TCHAR *computer, const TCHAR *msg)
{
   C_STRING BatchFile;

   BatchFile = GEOsimAppl::WORKDIR;
   BatchFile += _T('\\');
   BatchFile += GS_NOTIFY_BATCH_FILE;
   BatchFile += _T(" \"");
   BatchFile += computer;
   BatchFile += _T("\" \"");
   BatchFile += msg;
   BatchFile += _T("\"");

   // (le applicazioni win32 possono usare anche "CreateProcess")
   WinExec(CW2A(BatchFile.get_name()), SW_HIDE);

   return GS_GOOD;
}


/*********************************************************/
// FINE FUNZIONI DI NOTIFICA IN RETE                   //
/*********************************************************/


/******************************************************************************/
/*.doc gsc_DisplaySplashWindow
/*+
  Questa funzione visualizza la finestra di presentazione di GEOsim.
-*/                                             
/******************************************************************************/

/////////////////////////////////////////////////////////////////////////////
// CSplashWnd dialog

class CSplashWnd : public CDialog
{
public:
   CSplashWnd() { hBitmap = NULL; }
   virtual ~CSplashWnd() { if (hBitmap) DeleteObject(hBitmap); }

	BOOL Create(CWnd* pParent);

	enum { IDD = IDD_SPLASH };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

   HBITMAP  hBitmap;

	virtual BOOL OnInitDialog();
	DECLARE_MESSAGE_MAP()
public:
   //Static mImage;
};

BEGIN_MESSAGE_MAP(CSplashWnd, CDialog)
END_MESSAGE_MAP()

BOOL CSplashWnd::Create(CWnd* pParent)
{
	if (!CDialog::Create(CSplashWnd::IDD, pParent)) return FALSE;

	return TRUE;
}

void CSplashWnd::DoDataExchange(CDataExchange* pDX)
{
   CDialog::DoDataExchange(pDX);
}

BOOL CSplashWnd::OnInitDialog()
{
   CStatic  *pImage;
   C_STRING Path;

   CDialog::OnInitDialog();

   pImage = (CStatic *) GetDlgItem(IDC_IMAGE);

   Path = GEOsimAppl::WORKDIR;
   Path += _T("\\SPLASH.BMP");

   if ((hBitmap = (HBITMAP) LoadImage(NULL, Path.get_name(), IMAGE_BITMAP, 0, 0,
                                      LR_DEFAULTSIZE | LR_LOADFROMFILE)))
   {
      CRect rectWnd;

      pImage->SetWindowPos(NULL, 0, 0, 0, 0, SWP_NOSIZE);
      pImage->SetBitmap(hBitmap);
      pImage->ShowWindow(SW_SHOW);
      pImage->GetWindowRect(rectWnd);
      SetWindowPos(NULL, 0, 0, rectWnd.Width(), rectWnd.Height(), SWP_FRAMECHANGED);
      CenterWindow();
   }

	return TRUE;
}

static CSplashWnd *pSplash;

// Funzione di appoggio per "gsc_DisplaySplashWindow"
void CALLBACK EXPORT SplashTimerProc(HWND hWnd, UINT nMsg, UINT_PTR nIDEvent, DWORD dwTime)
{
	// timeout expired, destroy the splash window
   KillTimer(hWnd, nIDEvent);
	DestroyWindow(hWnd);
   delete pSplash;
}

void WINAPI gsc_DisplaySplash(void)
{
   // When resource from this ARX app is needed, just
   // instantiate a local CAcModuleResourceOverride
   CAcModuleResourceOverride myResources;

   CWnd *pAcadWindow = CWnd::FromHandlePermanent(adsw_acadMainWnd()), *pWindow;

   if ((pWindow = pAcadWindow->GetActiveWindow()) == NULL) pWindow = pAcadWindow;

   if ((pSplash = new CSplashWnd) == NULL) return;
   if (pSplash->Create(pWindow))
	{
		pSplash->ShowWindow(SW_SHOW);
		pSplash->UpdateWindow();
		pSplash->SetTimer(1, 2000, SplashTimerProc);
	}
   else
      delete pSplash;

   return;
}


/*********************************************************/
// INIZIO FUNZIONI PER GESTIONE BITMAP                   //
/*********************************************************/


/*********************************************************/
/*.doc gsc_WriteDIB                           <external> */
/*+                           
   Funzione che scrive una bitmap indipendente dal dispositivo di 
   visualizzazione (device-independent bitmap = DIB) in un file.
   Parametri:
   const TCHAR *Path;    Path del file di Bitmap
   HANDLE hDIB;         Puntatore alla DIB

   Restituisce GS_GOOD caso di successo oppure GS_BAD. 
-*/  
/*********************************************************/
int gsc_WriteDIB(const TCHAR *Path, HANDLE hDIB)
{
	BITMAPFILEHEADER	 hdr;
	LPBITMAPINFOHEADER lpbi;

	if (!hDIB) return GS_BAD;

	CFile file;
	if(!file.Open( Path, CFile::modeWrite|CFile::modeCreate) )
		return GS_BAD;

	lpbi = (LPBITMAPINFOHEADER) GlobalLock(hDIB);

	int nColors = 1 << lpbi->biBitCount;

	// Fill in the fields of the file header 
	hdr.bfType		 = ((WORD) ('M' << 8) | 'B');	// is always "BM"
	hdr.bfSize		 = (DWORD) GlobalSize (hDIB) + sizeof( hdr );
	hdr.bfReserved1 = 0;
	hdr.bfReserved2 = 0;
	hdr.bfOffBits   = (DWORD) (sizeof( hdr ) + lpbi->biSize +
			            nColors * sizeof(RGBQUAD));

	// Write the file header 
	file.Write( &hdr, sizeof(hdr) );

	// Write the DIB header and the bits 
	file.Write( lpbi, (UINT) GlobalSize(hDIB) );

   GlobalUnlock(hDIB);

	return GS_GOOD;
}


// DDBToDIB		- Creates a DIB from a DDB
// bitmap		- Device dependent bitmap
// dwCompression	- Type of compression - see BITMAPINFOHEADER
// pPal			- Logical palette
HANDLE DDBToDIB(CBitmap& bitmap, DWORD dwCompression, CPalette* pPal)
{
	BITMAP			    bm;
	BITMAPINFOHEADER	 bi;
	LPBITMAPINFOHEADER lpbi;
	DWORD			       dwLen;
	HANDLE			    hDIB;
	HANDLE			    handle;
	HDC 			       hDC;
	HPALETTE		       hPal;

	ASSERT(bitmap.GetSafeHandle());

	// The function has no arg for bitfields
	if( dwCompression == BI_BITFIELDS)
		return NULL;

	// If a palette has not been supplied use defaul palette
   if (pPal == NULL)
		hPal = (HPALETTE) GetStockObject(DEFAULT_PALETTE);
   else
   {
      hPal = (HPALETTE) pPal->GetSafeHandle();
	   if (hPal==NULL)
		   hPal = (HPALETTE) GetStockObject(DEFAULT_PALETTE);
   }

	// Get bitmap information
	bitmap.GetObject(sizeof(bm),(LPSTR)&bm);

	// Initialize the bitmapinfoheader
	bi.biSize		    = sizeof(BITMAPINFOHEADER);
	bi.biWidth		    = bm.bmWidth;
	bi.biHeight        = bm.bmHeight;
	bi.biPlanes 	    = 1;
	bi.biBitCount	    = bm.bmPlanes * bm.bmBitsPixel;
	bi.biCompression	 = dwCompression;
	bi.biSizeImage	    = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed		 = 0;
	bi.biClrImportant	 = 0;

	// Compute the size of the  infoheader and the color table
	int nColors = (1 << bi.biBitCount);
	if( nColors > 256 )
		nColors = 0;
	dwLen  = bi.biSize + nColors * sizeof(RGBQUAD);

	// We need a device context to get the DIB from
	hDC = GetDC(NULL);
	hPal = SelectPalette(hDC,hPal,FALSE);
	RealizePalette(hDC);

	// Allocate enough memory to hold bitmapinfoheader and color table
	hDIB = GlobalAlloc(GMEM_FIXED,dwLen);

	if (!hDIB){
		SelectPalette(hDC,hPal,FALSE);
		ReleaseDC(NULL,hDC);
		return NULL;
	}

	lpbi = (LPBITMAPINFOHEADER)hDIB;

	*lpbi = bi;

	// Call GetDIBits with a NULL lpBits param, so the device driver 
	// will calculate the biSizeImage field 
	GetDIBits(hDC, (HBITMAP)bitmap.GetSafeHandle(), 0L, (DWORD)bi.biHeight,
			(LPBYTE)NULL, (LPBITMAPINFO)lpbi, (DWORD)DIB_RGB_COLORS);

	bi = *lpbi;

	// If the driver did not fill in the biSizeImage field, then compute it
	// Each scan line of the image is aligned on a DWORD (32bit) boundary
	if (bi.biSizeImage == 0){
		bi.biSizeImage = ((((bi.biWidth * bi.biBitCount) + 31) & ~31) / 8)
						* bi.biHeight;

		// If a compression scheme is used the result may infact be larger
		// Increase the size to account for this.
		if (dwCompression != BI_RGB)
			bi.biSizeImage = (bi.biSizeImage * 3) / 2;
	}

	// Realloc the buffer so that it can hold all the bits
	dwLen += bi.biSizeImage;
	if (handle = GlobalReAlloc(hDIB, dwLen, GMEM_MOVEABLE))
		hDIB = handle;
	else{
		GlobalFree(hDIB);

		// Reselect the original palette
		SelectPalette(hDC,hPal,FALSE);
		ReleaseDC(NULL,hDC);
		return NULL;
	}

	// Get the bitmap bits
	lpbi = (LPBITMAPINFOHEADER)hDIB;

	// FINALLY get the DIB
	BOOL bGotBits = GetDIBits( hDC, (HBITMAP)bitmap.GetSafeHandle(),
				0L,				// Start scan line
				(DWORD)bi.biHeight,		// # of scan lines
				(LPBYTE)lpbi 			// address for bitmap bits
				+ (bi.biSize + nColors * sizeof(RGBQUAD)),
				(LPBITMAPINFO)lpbi,		// address of bitmapinfo
				(DWORD)DIB_RGB_COLORS);		// Use RGB for color table

	if( !bGotBits )
	{
		GlobalFree(hDIB);

		SelectPalette(hDC,hPal,FALSE);
		ReleaseDC(NULL,hDC);
		return NULL;
	}

	SelectPalette(hDC,hPal,FALSE);
	ReleaseDC(NULL,hDC);
	return hDIB;
}


void gsc_CaptureWindowScreen(HWND hWnd, CBitmap &srcImage)
{
   CDC     screenDC, memDC;
   CBitmap *oldImage = new CBitmap; 
   HDC     hscreenDC; 
   CSize   sizeClient; 
   CRect   WindowRect, ClientRect;

   GetWindowRect(hWnd, &WindowRect);
   GetClientRect(hWnd, &ClientRect);

   hscreenDC = GetDC(hWnd); 
   screenDC.Attach(hscreenDC); 
   memDC.CreateCompatibleDC(&screenDC); 
   //sizeClient.cx = screenDC.GetDeviceCaps(HORZRES); 
   //sizeClient.cy = screenDC.GetDeviceCaps(VERTRES); 
   //screenDC.DPtoLP(&sizeClient); 

   //srcImage.CreateCompatibleBitmap(&screenDC,sizeClient.cx,sizeClient.cy); 
   srcImage.CreateCompatibleBitmap(&screenDC,ClientRect.Width(),ClientRect.Height()); 
   oldImage = memDC.SelectObject(&srcImage); 
   memDC.BitBlt(0,0,ClientRect.Width(),ClientRect.Height(),&screenDC,0,0,SRCCOPY); 

   ReleaseDC(hWnd, hscreenDC); 
   DeleteDC(hscreenDC);
}


/*********************************************************/
// FINE FUNZIONI PER GESTIONE BITMAP                     //
// INIZIO FUNZIONI PER LICENZE IN RETE                   //
/*********************************************************/


/*********************************************************/
/*.doc gsc_Kript                              <external> */
/*+                           
   Funzione che crittografa una stringa utilizzando una parola
   segreta.
   Parametri:
   TCHAR *str;            stringa da decriptare
   size_t str_len;        lunghezza stringa da decriptare
   const TCHAR *PassWord;

   Restituisce GS_GOOD caso di successo oppure GS_BAD. 
-*/  
/*********************************************************/
void gsc_Kript(TCHAR *str, size_t str_len, const TCHAR *PassWord)
{
   C_STRING _PassWord;
   int      i = 0;
   size_t   len = str_len;
   TCHAR    *p = str;

   if (gsc_strlen(PassWord) == 0) _PassWord = LICENSE_PASSWORD;
   else _PassWord = PassWord;

   // Il seguente codice cripta una stringa basandosi selle proprietà
   // dell'operazione di XOR rilevabili direttamente dalla sua tabella di verità
   while (len-- > 0)
   {
      *p = *p ^ _PassWord.get_chr(i);
      i++;
      if (_PassWord.get_chr(i) == _T('\0')) i = 0;
      p++;
   }
}


/*********************************************************/
/*.doc gsc_DeKript                           <external> */
/*+                           
   Funzione che decrittografa una stringa utilizzando una parola
   segreta.
   Parametri:
   TCHAR *str;            stringa da decriptare
   size_t str_len;        lungheza stringa da decriptare
   const TCHAR *PassWord;

   Restituisce GS_GOOD caso di successo oppure GS_BAD. 
-*/  
/*********************************************************/
void gsc_DeKript(TCHAR *str, size_t str_len, const TCHAR *PassWord)
{
   // In pratica applicando due volte consecutive la funzione gsc_Kript
   // ad una stessa stringa, la si ottiene come se non fosse mai stata modificata
   gsc_Kript(str, str_len, PassWord);
}


/*********************************************************/
/*.doc gsc_KriptFile                          <external> */
/*+                           
   Funzione che crittografa il contenuto di un file utilizzando una parola
   segreta.
   Parametri:
   C_STRING &Source;            percorso del file da criptare
   C_STRING &Target;            percorso del file criptato
   const TCHAR *PassWord;

   Restituisce GS_GOOD caso di successo oppure GS_BAD. 
-*/  
/*********************************************************/
int gsc_KriptFile(C_STRING &Source, C_STRING &Target, const TCHAR *PassWord)
{
   FILE     *fSrc, *fDst;
   C_STRING tmpSource, tmpTarget;
   TCHAR    Buff[100];
   size_t   numRead, numWritten;

   tmpSource = Source;
   tmpTarget = Target;

   // Controlla Correttezza Path
   if (gsc_nethost2drive(tmpSource) == GS_BAD || gsc_nethost2drive(tmpTarget) == GS_BAD)
      return GS_BAD;

   if (gsc_path_exist(tmpSource) == GS_BAD) { GS_ERR_COD = eGSFileNoExisting; return GS_BAD; }
   
   if ((fSrc = gsc_fopen(tmpSource, _T("rb"))) == NULL) return GS_BAD;
   if ((fDst = gsc_fopen(tmpTarget, _T("wb"))) == NULL)
      { gsc_fclose(fDst); return GS_BAD; }

   do
   {
      numRead    = fread(Buff, sizeof(TCHAR), 100, fSrc);
      gsc_Kript(Buff, numRead, PassWord);
      numWritten = fwrite(Buff, sizeof(TCHAR), numRead, fDst);
      if (numRead != numWritten)
         { gsc_fclose(fSrc); gsc_fclose(fDst); return GS_BAD; }
   }
   while (!feof(fSrc) && !feof(fDst));

   if (gsc_fclose(fSrc) == GS_BAD) 
      { gsc_fclose(fDst); return GS_BAD; }
   
   return gsc_fclose(fDst);
}
int gs_KriptFile(void)
{
   presbuf  arg = acedGetArgs();
   C_STRING Source, Target, PassWord;
   
   acedRetNil();

   if (!arg || arg->restype != RTSTR) { GS_ERR_COD= eGSInvalidArg; return RTERROR; }
   Source = arg->resval.rstring;
   if (!(arg = arg->rbnext) || arg->restype != RTSTR) { GS_ERR_COD= eGSInvalidArg; return RTERROR; }
   Target = arg->resval.rstring;
   // Opzionale
   if ((arg = arg->rbnext) && arg->restype == RTSTR) PassWord = arg->resval.rstring;

   if (gsc_KriptFile(Source, Target, PassWord.get_name()) == GS_BAD) return RTERROR;
   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_DeKriptFile                        <external> */
/*+                           
   Funzione che decrittografa un file utilizzando una parola
   segreta.
   Parametri:
   C_STRING &Source;            percorso del file da decriptare
   C_STRING &Target;            percorso del file decriptato
   const TCHAR *PassWord;

   Restituisce GS_GOOD caso di successo oppure GS_BAD. 
-*/  
/*********************************************************/
int gsc_DeKriptFile(C_STRING &Source, C_STRING &Target, const TCHAR *PassWord)
{
   // In pratica applicando due volte consecutive la funzione gsc_Kript
   // ad una stessa stringa, la si ottiene come se non fosse mai stata modificata
   return gsc_KriptFile(Source, Target, PassWord);
}
int gs_DeKriptFile(void)
{
   return gs_KriptFile();
}


/*********************************************************/
/*.doc gsc_splitLicStartDate                  <external> */
/*+                           
   Funzione che riceve una stringa rappresentante una data 
   nel formato aaaa-mm-gg e che ritorna anno, mese, giorno 
   in formato numerico.
   Parametri:
   TCHAR *Date;   Data nel formato aaaa/mm/gg
   int   *Year;   Anno
   int   *Month;  Mese
   int   *Day;    Giorno

   Restituisce GS_GOOD in caso di successo altrimenti ritorna GS_BAD.
-*/  
/*********************************************************/
int gsc_splitLicStartDate(TCHAR *Date, int *Year, int *Month, int *Day)
{
   C_INT_LIST   MyList;
   COleDateTime dummyOleDateTime;
   
   if (MyList.from_str(Date, _T('/')) == GS_BAD)
      { GS_ERR_COD = eGSInvalidDateType; return GS_BAD; }   
   if (MyList.get_count() != 3) { GS_ERR_COD = eGSInvalidDateType; return GS_BAD; }

   // anno
   *Year = ((C_INT *) MyList.get_head())->get_key();
   // mese
   *Month = ((C_INT *) MyList.get_next())->get_key();
   // giorno
   *Day = ((C_INT *) MyList.get_next())->get_key();

   // Controllo la data
   if (dummyOleDateTime.SetDate(*Year, *Month, *Day) == 1 ||
       dummyOleDateTime.GetStatus() != COleDateTimeSpan::valid)
      { GS_ERR_COD = eGSInvalidDateType; return GS_BAD; }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getMaxLicNum                       <external> */
/*+                           
   Funzione che legge dal file delle licenze il numero massimo
   di licenze GEOsim attivabili in contemporanea.

   Restituisce il numero di licenze. 
-*/  
/*********************************************************/
int gsc_getMaxLicNum(void)
{
   C_STRING       Path;
   TCHAR          Buff[100];
   FILE           *f;
   size_t         str_len;
   int            Year, Month, Day;
   C_STR_LIST     DummyStrList;
   C_STR          *pDummyStr;
   C_INT_STR_LIST MaxLicNumStartDateList;
   C_INT_STR      *pMaxLicNumStartDate;

   Path = GEOsimAppl::GEODIR;
   Path += _T('\\');
   Path += GEOUSRDIR;
   Path += _T("\\AUTH");

   if ((f = gsc_fopen(Path.get_name(), _T("rb"), ONETEST)) == NULL)
      { GS_ERR_COD = eGSNoLicense; return 0; }
   str_len = fread(Buff, sizeof(TCHAR), 100 - 1, f);
   if (ferror(f)) { gsc_fclose(f); return 0; }
   gsc_fclose(f);
   Buff[str_len] = _T('\0'); // chiudo la stringa all'ultimo carattere
   gsc_DeKript(Buff, str_len);

   if (DummyStrList.from_str(Buff, _T('-')) == GS_BAD) return 0;
   if ((pDummyStr = (C_STR *) DummyStrList.get_head()) == NULL) return 0; 
   while (pDummyStr)
   {
      if (pDummyStr->isDigit() == GS_BAD) break; // si ferma il ciclo
      if ((pMaxLicNumStartDate = new C_INT_STR()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return 0; }    
      pMaxLicNumStartDate->set_key(_wtoi(pDummyStr->get_name())); // Num di licenze
      MaxLicNumStartDateList.add_tail(pMaxLicNumStartDate);

      pDummyStr = (C_STR *) DummyStrList.get_next();
      // Verifico data
      if (pDummyStr && gsc_splitLicStartDate(pDummyStr->get_name(), &Year, &Month, &Day) == GS_GOOD)
         pMaxLicNumStartDate->set_name(pDummyStr->get_name()); // data di partenza
      else
         // Accetto data nulla solo se la lista ha una sola coppia
         if (DummyStrList.get_count() > 2)
            { GS_ERR_COD = eGSInvalidDateType; return 0; }
         else
            pMaxLicNumStartDate->set_name(_T("1900/01/01"));

      pDummyStr = (C_STR *) DummyStrList.get_next();
   }

   C_STRING RefData;

   // ordino in modo decrescente per data espressa come aaaa-mm-aa
   MaxLicNumStartDateList.sort_name(true, false);

   COleDateTime dummyOleDateTime;
   dummyOleDateTime = COleDateTime::GetCurrentTime(); // data attuale
   RefData = dummyOleDateTime.GetYear();
   RefData += _T('/');
   RefData += dummyOleDateTime.GetMonth();
   RefData += _T('/');
   RefData += dummyOleDateTime.GetDay();

   pMaxLicNumStartDate = (C_INT_STR *) MaxLicNumStartDateList.get_head();
   while (pMaxLicNumStartDate)
   {
      if (gsc_strcmp(pMaxLicNumStartDate->get_name(), RefData.get_name()) <= 0)
         break;
      pMaxLicNumStartDate = (C_INT_STR *) MaxLicNumStartDateList.get_next();
   }      

   return (pMaxLicNumStartDate) ? pMaxLicNumStartDate->get_key() : 0;
}
int gs_getMaxLicNum(void)
{
   acedRetInt(gsc_getMaxLicNum());
   return RTNORM;
}


/*********************************************************/
/*.doc gsc_setMaxLicNum                       <external> */
/*+                           
   Funzione che scrive in un file il numero massimo di licenze
   GEOsim attivabili in contemporanea.
   Parametri:
   C_STRING &Path;                     Path del file di licenze
   C_INT_STR_LIST &LicMaxNumStartDateList; Lista di numeri interi da interpretare 
                                           come segue:
                                           1 num = numero massimo di licenze 
                                                   attivabili contemporaneamente
                                           2 str = "aaaa/mm/gg" data di inizio 
                                                   licenza. Opzionale solo se 
                                                   la lista contiene un solo 
                                                   elemento.
   Restituisce GS_GOOD in caso di successo altrimenti ritorna GS_BAD.
-*/  
/*********************************************************/
int gsc_setMaxLicNum(C_STRING &Path, C_INT_STR_LIST &MaxLicNumStartDateList)
{
   FILE      *f;
   C_STRING  LicStr, PartialLicStr, StrDate;
   int       Day, Month, Year;
   C_INT_STR *pMaxLicNumStartDate;
   
   // ordino in modo crescente per data espressa come aaaa-mm-aa
   MaxLicNumStartDateList.sort_name();
   
   pMaxLicNumStartDate = (C_INT_STR *) MaxLicNumStartDateList.get_head();
   if (!pMaxLicNumStartDate) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   while (pMaxLicNumStartDate)
   {
      if (LicStr.get_name()) LicStr += _T('-'); // aggiungo separatore
      // Numero licenza
      PartialLicStr = pMaxLicNumStartDate->get_key();
      PartialLicStr += _T('-');
      // Data di inizio
      StrDate = pMaxLicNumStartDate->get_name();
      // Verifico data
      if (gsc_splitLicStartDate(StrDate.get_name(), &Year, &Month, &Day) == GS_GOOD)
      {
         PartialLicStr += Year;
         PartialLicStr += _T('/');
         PartialLicStr += Month;
         PartialLicStr += _T('/');
         PartialLicStr += Day;
      }
      else
         // Accetto data nulla solo se la lista ha un solo elemento
         if (MaxLicNumStartDateList.get_count() > 1)
            { GS_ERR_COD = eGSInvalidDateType; return GS_BAD; }

      LicStr += PartialLicStr;
      pMaxLicNumStartDate = (C_INT_STR *) pMaxLicNumStartDate->get_next();
   }
 
   if (LicStr.len() > 100) return GS_BAD;
   gsc_Kript(LicStr.get_name(), LicStr.len());
   
   bool Unicode = true;
   if ((f = gsc_fopen(Path.get_name(), _T("wb"), ONETEST, &Unicode)) == NULL)
      return GS_BAD;
   fwrite(LicStr.get_name(), sizeof(TCHAR), LicStr.len(), f);
   if (ferror(f)) { gsc_fclose(f); return GS_BAD; }
   
   return gsc_fclose(f);
}
int gs_setMaxLicNum(void)
{
   presbuf        arg = acedGetArgs();
   C_STRING       Path;
   C_INT_STR_LIST MaxLicNumStartDateList;

   // Es. Lisp 
   // 1 licenza permanente
   // (bidibibodibibu "c:\\auth" (list (list 1 "")))
   // 1 licenza a partire dal 8 gennaio 2007
   // (bidibibodibibu "c:\\auth" (list (list 1 "2007-01-08")))
   // 1 licenza a partire dal 8 gennaio 2007, 4 licenze dal 8 febbraio, e 
   // di nuovo 1 licenza dal 8 marzo
   // (bidibibodibibu "c:\\auth" (list (list 1 "2007/01/08") (list 4 "2007/02/08") (list 1 "2007/03/08")))
   
   acedRetNil();

   if (!arg || arg->restype != RTSTR) { GS_ERR_COD= eGSInvalidArg; return RTERROR; }
   Path = arg->resval.rstring;

   if (!(arg = arg->rbnext) || MaxLicNumStartDateList.from_rb(arg) == GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (gsc_setMaxLicNum(Path, MaxLicNumStartDateList) == GS_BAD) return RTERROR;
   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_getActualGEOsimUsrList             <external> */
/*+                           
   Funzione che legge l'elenco degli utenti (nomi dei computer)
   che stanno usando GEOsim.
   Parametri:
   C_STR_LIST &UsrList;    Lista degl iutenti (nomi dei computer) che stanno
                           utilizzando GEOsim

   Restituisce GS_GOOD in caso di successo altrimenti ritorna GS_BAD.
-*/  
/*********************************************************/
int gsc_getActualGEOsimUsrList(C_STR_LIST &UsrList)
{
   int      Tot;
   presbuf  FileNameLst = NULL, FileAttrLst = NULL;
   C_STRING Dir, LicMask, Name;
   C_STR    *pUsr;
  
   Dir = GEOsimAppl::GEODIR;
   Dir += _T('\\');
   Dir += GEOUSRDIR;
   Dir += _T('\\');

   LicMask += Dir;
   LicMask += _T("*.LIC");

   UsrList.remove_all();

   // verifico quanti utenti stanno usando GEOsim
   if ((Tot = gsc_adir(LicMask.get_name(), &FileNameLst, NULL, NULL, &FileAttrLst)) > 0)
   {
      presbuf  pName = FileNameLst, pAttr = FileAttrLst;
      C_STRING LicFile;

      for (int i = 0; i < Tot; i++) 
      {
         pName = pName->rbnext;
         pAttr = pAttr->rbnext;
      
         LicFile = Dir;
         LicFile += pName->resval.rstring;

         if (*(pAttr->resval.rstring + 4) != _T('D'))  // non è una cartella
         {
            // se il file è bloccato qualcuno sta usando GEOsim
            if (gsc_delfile(LicFile, ONETEST) == GS_BAD)
            {
               gsc_splitpath(LicFile, NULL, NULL, &Name);

               if ((pUsr = new C_STR(Name.get_name())) == NULL)
                  { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
               UsrList.add_tail(pUsr);
            }
         }
      }
      
      acutRelRb(FileNameLst);
      acutRelRb(FileAttrLst);
   }

   return GS_GOOD;
}
int gs_getActualGEOsimUsrList(void)
{
   C_STR_LIST UsrList;
   C_RB_LIST  RbList;

   acedRetNil();

   if (gsc_getActualGEOsimUsrList(UsrList) == GS_BAD) return RTERROR;

   if ((RbList << UsrList.to_rb()) == NULL) return RTERROR;
   RbList.LspRetList();

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_startAutorization                  <external> */
/*+                           
   Funzione che verifica se ci sono ancora licenze disponibili
   quindi, in caso affermativo, si segna come computer che sta
   usando GEOsim.

   Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_startAutorization(void)
{
   int        LicMaxNum;
   C_STRING   LicFile, ComputerName;
   C_STR_LIST UsrList;

   // leggo il numero limite di licenze
   if ((LicMaxNum = gsc_getMaxLicNum()) == 0) return GS_BAD;

   if (gsc_getActualGEOsimUsrList(UsrList) == GS_BAD) return GS_BAD;
 
   if (gsc_getComputerName(ComputerName) == GS_BAD) return GS_BAD;
   // se sto già usando GEOsim
   if (UsrList.search_name(ComputerName.get_name(), FALSE) != NULL) return GS_GOOD;

   if (UsrList.get_count() >= LicMaxNum) { GS_ERR_COD = eGSLicenseExceeded; return GS_BAD; }

   // mi segno come utente che sta usando GEOsim
   LicFile = GEOsimAppl::GEODIR;
   LicFile += _T('\\');
   LicFile += GEOUSRDIR;
   LicFile += _T('\\');
   LicFile += ComputerName;
   LicFile += _T(".LIC");

   return (gsc_fopen(LicFile.get_name(), _T("w"), ONETEST) == NULL) ? GS_BAD : GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
// INIZIO - FUNZIONI PER GESTIONE PROFILI DI AUTOCAD
///////////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc gsc_getGEOsimProfileRegistryKey        <external> */
/*+                           
   Funzione che restituisce il profilo corrente di autocad.

   Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_getGEOsimProfileRegistryKey(C_STRING &CurrProfile)
{
   TCHAR *pstrKey = NULL;
   
   acProfileManagerPtr()->ProfileRegistryKey(pstrKey, GEO_PROFILE_NAME);
   if (pstrKey)
   {
      CurrProfile = pstrKey;
      free(pstrKey);
      return GS_GOOD;
   }

   return GS_BAD;
}


/*********************************************************/
/*.doc gsc_getGEOsimQNEWTEMPLATERegistryKey   <external> */
/*+                           
   Funzione che restituisce la chiave il profilo corrente di autocad.
   Parametri:
   C_STRING &Value;

   Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_getGEOsimQNEWTEMPLATERegistryKey(C_STRING &Value)
{
   C_STRING CurrProfile;
   HKEY     hKey;
   TCHAR    *p, Buf[500];
   DWORD    dwBufLen = 500;
   LONG     Res;

   Value.clear();
   
   if (gsc_getGEOsimProfileRegistryKey(CurrProfile) == GS_BAD) return GS_BAD;
   CurrProfile += _T("\\General");

   if ((p = CurrProfile.at(_T("HKEY_CURRENT_USER"), FALSE)) && 
        p == CurrProfile.get_name())
   {
      p += (wcslen(_T("HKEY_CURRENT_USER")) + 1);
      if (RegOpenKeyEx(HKEY_CURRENT_USER, p,
                       0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
        return GS_BAD;
   }
   else
   if ((p = CurrProfile.at(_T("HKEY_LOCAL_MACHINE"), FALSE)) && 
        p == CurrProfile.get_name())
   {
      p += (wcslen(_T("HKEY_LOCAL_MACHINE")) + 1);
      if (RegOpenKeyEx(HKEY_CURRENT_USER, p,
                       0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
        return GS_BAD;
   }
   
   Res = RegQueryValueEx(hKey, _T("QnewTemplate"), NULL, NULL, (LPBYTE) Buf, &dwBufLen); 
   RegCloseKey(hKey);

   if (Res != ERROR_SUCCESS) return GS_BAD;

   Value = Buf;
   Value.alltrim();

   return (Value.len() > 0) ? GS_GOOD : GS_BAD;
}


/*********************************************************/
/*.doc gsc_getGEOsimUsrAppDataPath   <external> */
/*+                           
   Funzione che restituisce il percorso dell'utente corrente
   per i dati dell'applicazione GEOsim.
   Parametri:
   C_STRING &Value;

   Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_getGEOsimUsrAppDataPath(C_STRING &Value)
{
   TCHAR dummy[MAX_PATH];

   Value.clear();
   // Path delle applicazione con opzione di crearla se non esiste
   // da vista in poi microsoft consiglia di usare SHGetKnownFolderPath
   if (SHGetFolderPath(NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, dummy) == E_FAIL)
      return GS_BAD;
   Value = dummy;
   Value += _T("\\");
   Value += GEO_PROFILE_NAME;
   gsc_mkdir(Value);

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_isCurrentDWGModified   <external> */
/*+                           
   Funzione che restituisce il percorso dell'utente corrente
   per i dati dell'applicazione GEOsim.
   Parametri:
   C_STRING &Value;

   Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
bool gsc_isCurrentDWGModified(void)
{
   resbuf rbDBMOD;

   // la variabile DBMOD indica lo stato di modifica del disegno corrente
   // se = 0 il disegno non è stato cambiato
   acedGetVar(_T("DBMOD"), &rbDBMOD);

   return (rbDBMOD.restype == RTSHORT && rbDBMOD.resval.rint != 0) ? true : false;
}


/*********************************************************/
/*.doc gsc_add_list                           <external> */
/*+                           
   Funzione che aggiunge una riga al controllo DCL tipo list-box.
   Il processo di creazione di una lista deve incominciare da ads_start_list
   e dopo n chiamate alla gsc_add_list deve terminare con ads_end_list.
   La funzione si è resa necessaria perchè la stringa non deve superare i 255
   caratteri altrimenti autocad genera un errore.
   Parametri:
   const ACHAR *item;
   oppure
   C_STRING &item;

   Restituisce RTNORM in caso di successo altrimenti RTERROR.
-*/  
/*********************************************************/
int gsc_add_list(const ACHAR *item)
{
   C_STRING Temp(item);
   
   return gsc_add_list(Temp);
}
int gsc_add_list(C_STRING &item)
{
   C_STRING Temp;
   size_t   Len = item.len();

   if (Len == 0)
      Temp = _T("");
   else if (Len > 255)
   {
      Temp.set_name(item.get_name(), 0, 251);
      Temp += _T("...");
   }
   else 
      Temp = item;

   return ads_add_list(Temp.get_name());
}


/*********************************************************/
/*.doc gsc_GetFileD   <external> */
/*+                           
   Funzione che chiede tramite interfaccia agrafica il percorso 
   di un file.
   Parametri:
   const ACHAR *title;     Input dialog box caption
   C_STRING &defawlt;      Input default file name
   const ACHAR *ext;       Input default file extension
   int flags;              Input bitmap for dialog behavior
                           (usare la macro UI_LOADFILE_FLAGS = 4+16 o
                            UI_SAVEFILE_FLAGS = 1+4)
   C_STRING &result;       Output file or folder name

   Restituisce RTNORM in caso di successo altrimenti RTERROR.
-*/  
/*********************************************************/
int gsc_GetFileD(const ACHAR *title, C_STRING &defawlt, const ACHAR *ext,
                  int flags, C_STRING &Path)
{
   return gsc_GetFileD(title, defawlt.get_name(), ext, flags, Path);
}
int gsc_GetFileD(const ACHAR *title, const ACHAR *defawlt, const ACHAR *ext,
                  int flags, C_STRING &Path)
{
   struct resbuf Rb;
   int    Result;

   HINSTANCE hOld = AfxGetResourceHandle();

   AfxSetResourceHandle(acedGetAcadResourceInstance());

   Rb.restype = RTSTR;
   Rb.rbnext = NULL;

   if ((Result = acedGetFileD(title, defawlt, ext, flags, &Rb)) == RTNORM)
   {
      Path = Rb.resval.rstring;
      free(Rb.resval.rstring);
   }
   else
      Path.clear();

   AfxSetResourceHandle(hOld);

   return Result;
}


/*********************************************************/
/*.doc gsc_print_mem_status                   <external> */
/*+                           
   Funzione che stampa lo stato della memoria.
-*/  
/*********************************************************/
void gsc_print_mem_status(bool extendexInfo)
{
  MEMORYSTATUSEX statex;

  // Use to convert bytes to KB
  int DIV = 1024;
  // Specify the width of the field in which to print the numbers. 
  // The asterisk in the format specifier "%*I64d" takes an integer 
  // argument and uses it to pad and right justify the number.
  int WIDTH = 7;

  statex.dwLength = sizeof (statex);
  GlobalMemoryStatusEx (&statex);

  acutPrintf(_T("\n----------------- Memory Status -----------------\n"));
  acutPrintf(_T("There is %*ld percent of memory in use.\n"),
             WIDTH, statex.dwMemoryLoad);
  if (extendexInfo) acutPrintf(_T("There are %*I64d total Kbytes of physical memory.\n"),
                               WIDTH, statex.ullTotalPhys/DIV);
  if (extendexInfo) acutPrintf(_T("There are %*I64d free Kbytes of physical memory.\n"),
                               WIDTH, statex.ullAvailPhys/DIV);
  acutPrintf(_T("There are %*I64d used Kbytes of physical memory.\n"),
             WIDTH, (statex.ullTotalPhys-statex.ullAvailPhys)/DIV);
  if (extendexInfo) acutPrintf(_T("There are %*I64d total Kbytes of paging file.\n"),
                               WIDTH, statex.ullTotalPageFile/DIV);
  if (extendexInfo) acutPrintf(_T("There are %*I64d free Kbytes of paging file.\n"),
                               WIDTH, statex.ullAvailPageFile/DIV);
  if (extendexInfo) acutPrintf(_T("There are %*I64d used Kbytes of paging file.\n"),
                               WIDTH, (statex.ullTotalPageFile-statex.ullAvailPageFile)/DIV);
  if (extendexInfo) acutPrintf(TEXT("There are %*I64d total Kbytes of virtual memory.\n"),
                               WIDTH, statex.ullTotalVirtual/DIV);
  if (extendexInfo) acutPrintf(_T("There are %*I64d free Kbytes of virtual memory.\n"),
                               WIDTH, statex.ullAvailVirtual/DIV);
  acutPrintf(_T("There are %*I64d used Kbytes of virtual memory.\n"),
             WIDTH, (statex.ullTotalVirtual-statex.ullAvailVirtual)/DIV);

  // Show the amount of extended memory available.
  if (extendexInfo) acutPrintf(TEXT("There are %*I64d free Kbytes of extended memory.\n"),
                               WIDTH, statex.ullAvailExtendedVirtual/DIV);
}


/*********************************************************/
/*.doc gsc_set_variant_t                      <external> */
/*+                           
   Funzione che setta un valore ad un oggetto _variant_t.
   Si è resa necessaria questa funzione perchè se si impostava un _variant_t
   a bool e poi ad un long questo rimaneva bool..
-*/  
/*********************************************************/
void gsc_set_variant_t(_variant_t &Value, bool NewValue)
   { Value.Clear(); Value.ChangeType(VT_BOOL); Value = NewValue; }
void gsc_set_variant_t(_variant_t &Value, short NewValue)
   { Value.Clear(); Value.ChangeType(VT_I2); Value = NewValue; }
void gsc_set_variant_t(_variant_t &Value, int NewValue)
   { Value.Clear(); Value.ChangeType(VT_INT); Value = NewValue; }
void gsc_set_variant_t(_variant_t &Value, long NewValue)
   { Value.Clear(); Value.ChangeType(VT_I4); Value = NewValue; }
void gsc_set_variant_t(_variant_t &Value, double NewValue)
   { Value.Clear(); Value.ChangeType(VT_R8); Value = NewValue; }
void gsc_set_variant_t(_variant_t &Value, const TCHAR *NewValue)
   {
      Value.Clear(); 
      Value.ChangeType(VT_BSTR); 
      Value = NewValue; 
}
void gsc_set_variant_t(_variant_t &Value, C_STRING &NewValue)
   { gsc_set_variant_t(Value, NewValue.get_name()); }
void gsc_set_variant_t(_variant_t &Value, COleDateTime NewValue)
   { Value.Clear(); Value.ChangeType(VT_DATE); Value = (DATE) NewValue; }


//-----------------------------------------------------------------------//
// Funzioni per gestione EED della classe C_EED                          //
//-----------------------------------------------------------------------//


// costruttore
C_EED::C_EED()
{
   cls    = sub = 0;
   num_el = 1;
   ads_name_clear(ent);
   initial_node = final_node = 0;
   vis   = VISIBLE;
   gs_id = 0;
}
C_EED::C_EED(ads_name name)
{
   cls    = sub = 0;
   num_el = 1;
   ads_name_clear(ent);
   initial_node = final_node = 0;
   vis   = INVISIBLE;
   gs_id = 0;
   load(name);
}


/*********************************************************/
/*.doc C_EED::load                                       */
/*+
  Questa funzione carica i dati estesi della applicazione GEOsim legati ad
  un oggetto grafico. Viene settato: il codice della classe, della sottoclasse,
  il fattore di aggregazione e il riferimento al nodo iniziale e finale (se
  l'oggetto è una polilinea appartenente ad una classe simulazione).
  Parametri:
  ads_name name;    oggetto grafico
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_EED::load(ads_name name)
{
   AcDbObject   *pObj;
   AcDbObjectId objId;
   
   if (acdbGetObjectId(objId, name) != Acad::eOk)
      { GS_ERR_COD = eGSInvalidEED; return GS_BAD; }  
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvalidEED; return GS_BAD; }
   int Res = load(pObj);
   if (pObj->close() != Acad::eOk) return GS_BAD;

   return Res;
}
int C_EED::load(const AcDbObject *pObj)
{
   C_RB_LIST lst;

   lst << pObj->xData(GEO_APP_ID);
   if (load(lst.get_head()) == GS_BAD) return GS_BAD;
   acdbGetAdsName(ent, pObj->objectId());

   return GS_GOOD;
}
int C_EED::load(presbuf lst)
{
   presbuf rb = lst;

   cls 			 = sub = 0; 
	num_el 		 = 1; 
   initial_node = final_node = 0;
   vis          = VISIBLE;
   gs_id        = 0;
   ads_name_clear(ent);

   if (!rb) { GS_ERR_COD = eGSInvalidEED; return GS_BAD; }

   // Se si sta passando un resbuf relativo alla descrizione completa dell'entità
   if (rb->restype != 1001)
   {
      if ((rb = gsc_rbsearch(-3, lst)) == NULL ||        // cerco intestazione EED
          (rb = gsc_EEDsearch(GEO_APP_ID, rb)) == NULL)  // cerco l'etichetta di GEOsim
         { GS_ERR_COD = eGSInvalidEED; return GS_BAD; }

      presbuf pRbHandle;
      TCHAR   handle[MAX_LEN_HANDLE];

      if ((pRbHandle = gsc_rbsearch(5, lst)) == NULL) // cerco handle
         { GS_ERR_COD = eGSInvalidEED; return GS_BAD; }
      gsc_strcpy(handle, pRbHandle->resval.rstring, MAX_LEN_HANDLE);
      acdbHandEnt(handle, ent);
   }

   // APPLICAZIONE GEOSIM
   if (rb->restype != 1001) { GS_ERR_COD = eGSInvalidEED; return GS_BAD; }
   if (gsc_strcmp(rb->resval.rstring, GEO_APP_ID) != 0) { GS_ERR_COD = eGSInvalidEED; return GS_BAD; }
   if ((rb = rb->rbnext) == NULL) { GS_ERR_COD = eGSInvalidEED; return GS_BAD; }
   
   // Codice classe
   if (rb->restype != 1070) { GS_ERR_COD=eGSInvalidEED; return GS_BAD; }
   cls = rb->resval.rint;
   if ((rb = rb->rbnext) == NULL) return GS_GOOD;

   // Codice sottoclasse
   if (rb->restype == 1070)
   {
      sub = rb->resval.rint;
      if ((rb = rb->rbnext) == NULL) return GS_GOOD; 
   }
   
   // Numero di aggregazioni
   if (rb->restype == 1071)
   {
      num_el = rb->resval.rlong;
      if ((rb = rb->rbnext) == NULL) return GS_GOOD; 
   }

   // Flag di visibilità (in forma carattere)
   if (rb->restype == 1000)
   {
      vis = (rb->resval.rstring[0] == _T('F')) ? INVISIBLE : VISIBLE;
      if ((rb = rb->rbnext) == NULL) return GS_GOOD; 
   }

   // Lettura eventuale del nodo iniziale, nodo finale (solo per simulazioni per
   // entità di tipo polilinea)
   if (rb->restype==1002 && gsc_strcmp(rb->resval.rstring, _T("{")) == 0)  // inizio lista
   {
      if ((rb = rb->rbnext) == NULL) { GS_ERR_COD = eGSInvalidEED; return GS_BAD; }
      // NODO INIZIALE
      if (rb->restype != 1071) { GS_ERR_COD = eGSInvalidEED; return GS_BAD; }
      initial_node = rb->resval.rlong;
      if ((rb = rb->rbnext) == NULL) { GS_ERR_COD = eGSInvalidEED; return GS_BAD; } 
      // NODO FINALE
      if (rb->restype != 1071) { GS_ERR_COD = eGSInvalidEED; return GS_BAD; } 
      final_node = rb->resval.rlong;
      if ((rb = rb->rbnext) == NULL) return GS_GOOD; // "}"
      if ((rb = rb->rbnext) == NULL) return GS_GOOD;
   }

   // Lettura gs_id
   if (rb->restype == AcDb::kDxfXdReal) gs_id = (long) rb->resval.rreal;

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
int C_EED::clear()
{
   AcDbObjectId objId;
  
   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }  

   return clear(objId);
}
int C_EED::clear(AcDbObjectId &objId)
{
   AcDbObject       *pObj;
   static C_RB_LIST XData;

   if (XData.get_head() == NULL)
      if ((XData << acutBuildList(1001, GEO_APP_ID, 0)) == NULL)
         return GS_BAD;
   
   if (acdbOpenObject(pObj, objId, AcDb::kForWrite) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (pObj->setXData(XData.get_head()) != Acad::eOk) 
      { pObj->close(); GS_ERR_COD = eGSInvalidEED; return GS_BAD; }
   if (pObj->close() != Acad::eOk) return GS_BAD;

   return GS_GOOD;
}
int C_EED::clear(ads_name name)
{
   if (load(name) == GS_BAD) return GS_BAD;
   return clear();
}

/*********************************************************/
/*.doc C_EED::save                            <external> */
/*+
  Funzione che salva le entità estese in un oggetto grafico.
  Parametri:
  AcDbObject *pObj;   Oggetto già aperto in scrittura

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_EED::save(AcDbObject *pObj)
{
   Acad::ErrorStatus Res;
   C_RB_LIST         arg;

   if (pObj == NULL) return GS_BAD;

   Res = pObj->upgradeOpen();

   if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   if ((arg << acutBuildList(AcDb::kDxfRegAppName, GEO_APP_ID, 
                             AcDb::kDxfXdInteger16, cls, 0)) == NULL)
      return GS_BAD;

   // SOTTOCLASSE SE DIVERSO DA 0
   if (sub != 0)
      if ((arg += acutBuildList(AcDb::kDxfXdInteger16, sub, 0)) == NULL) return GS_BAD;

   // NUMERO ELEMENTI SE DIVERSO DA 1
   if (num_el != 1)
      if ((arg += acutBuildList(AcDb::kDxfXdInteger32, num_el, 0)) == NULL) return GS_BAD;

   // Flag di visibilità (in forma carattere)
   if (vis == INVISIBLE)
      if (gsc_is_DABlock(pObj) == GS_GOOD)
         if ((arg += acutBuildList(AcDb::kDxfXdAsciiString, _T("F"), 0)) == NULL) return GS_BAD;

   // Scrittura eventuale nodo iniziale, nodo finale (solo per simulazioni per
   // entità di tipo polilinea)
   if (initial_node != 0 || final_node != 0)
      if ((arg += acutBuildList(AcDb::kDxfXdControlString, _T("{"),
                                AcDb::kDxfXdInteger32, initial_node,
                                AcDb::kDxfXdInteger32, final_node,
                                AcDb::kDxfXdControlString, _T("}"), 0)) == NULL)
         return GS_BAD;

   // gs_id
   if (gs_id != 0)
      if ((arg += acutBuildList(AcDb::kDxfXdReal, (double) gs_id, 0)) == NULL) return GS_BAD;

   if (pObj->setXData(arg.get_head()) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   return GS_GOOD;      
}


/*********************************************************/
/*.doc C_EED::save                            <external> */
/*+
  Funzione che salva le entità estese in un oggetto grafico.
  Parametri:
  int AddEnt2SaveSS;  Flag di salvataggio opzionale; se = GS_GOOD 
                      l'oggetto viene inserito nel gruppo di selezione 
                      del salvataggio (default = GS_GOOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_EED::save(int AddEnt2SaveSS)
{
   AcDbObject   *pObj;
   AcDbObjectId objId;

   if (ads_name_nil(ent)) return GS_BAD;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvalidEED; return GS_BAD; }  
   if (acdbOpenObject(pObj, objId, AcDb::kForWrite) != Acad::eOk)
      { GS_ERR_COD = eGSInvalidEED; return GS_BAD; }
   
   if (save(pObj) == GS_BAD) return GS_BAD;

   if (pObj->close() != Acad::eOk) return GS_BAD;

   if (AddEnt2SaveSS == GS_GOOD)
      // Aggiungo in GEOsimAppl::SAVE_SS per salvataggio
      if (gsc_addEnt2savess(ent) == GS_BAD) return GS_BAD;

   return GS_GOOD;      
}
//-----------------------------------------------------------------------//
int C_EED::save(ads_name newent, int AddEnt2SaveSS)
{
   ads_name_set(newent,ent); 
   return save(AddEnt2SaveSS);
}
//-----------------------------------------------------------------------//
// aggiorno la EED su tutto il gruppo di selezione
int C_EED::save_selset(C_SELSET &SelSet)
{
   ads_name ss;
   SelSet.get_selection(ss);
   return save_selset(ss);
}
int C_EED::save_selset(ads_name selset)
{
   long     i = 0;
   ads_name ent;

   while (acedSSName(selset, i++, ent) == RTNORM)
      if (save(ent) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}

//-----------------------------------------------------------------------//
// aggiorno solo il numero di aggregazioni
int C_EED::save_aggr(int n)
{
   if (load(ent) == GS_BAD) return GS_BAD;
   num_el = n;

   return save();
}
//-----------------------------------------------------------------------//
int C_EED::save_aggr(C_SELSET &SelSet, int n)
{
   ads_name entity;
   long ndx = 0;

   while (SelSet.entname(ndx++, entity) == GS_GOOD)
      if (save_aggr(entity, n) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}
int C_EED::save_aggr(ads_name newent, int n)
{
   ads_name_set(newent, ent); 
   return save_aggr(n);
}
//-----------------------------------------------------------------------//
// aggiorno solo il flag di visibilità
int C_EED::save_vis(int flag)
{
   if (load(ent) == GS_BAD) return GS_BAD;
   vis = flag;

   return save();
}
//-----------------------------------------------------------------------//
int C_EED::save_vis(ads_name newent, int flag)
{
   ads_name_set(newent, ent); 
   return save_vis(flag);
}
//-----------------------------------------------------------------------//
int C_EED::set_cls(int in)
{ cls=in; return save(); }   

int C_EED::set_sub(int in)
{ sub=in; return save(); }   

int C_EED::set_num_el(int in)
{ num_el=in; return save(); }

int C_EED::set_initial_node(long in)
{ initial_node=in; return save(); }

int C_EED::set_final_node(long in)
{ final_node=in; return save(); }
   
int C_EED::set_initialfinal_node(long init, long final)
{ initial_node=init; final_node=final; return save(); }

int C_EED::set_cls(ads_name name,int in)
{ 
   if (load(name) == GS_BAD) return GS_BAD;
   cls = in;
   return save();
}
   
int C_EED::set_sub(ads_name name,int in)
{
   if (load(name) == GS_BAD) return GS_BAD;
   sub = in;
   return save();
}
   
int C_EED::set_num_el(ads_name name, int in)
{
   if (load(name) == GS_BAD) return GS_BAD;
   num_el = in;
   return save();
}
   
int C_EED::set_initial_node(ads_name name, long in)
{
   if (load(name) == GS_BAD) return GS_BAD;
   initial_node = in;
   return save();
}

int C_EED::set_final_node(ads_name name, long in)
{ 
   if (load(name) == GS_BAD) return GS_BAD;
   final_node = in;
   return save();
}

int C_EED::set_initialfinal_node(ads_name name, long init, long final)
{
   if (load(name) == GS_BAD) return GS_BAD;
   initial_node = init;
   final_node   = final;
   return save();
}

int C_EED::operator = (C_EED &eed)
{
   cls = eed.cls;
   sub = eed.sub;
   num_el = eed.num_el;
   ads_name_set(eed.ent, ent);
   final_node = eed.final_node;
   initial_node = eed.initial_node;
   vis = eed.vis;

   return GS_GOOD; 
}


/*********************************************************/
/*.doc gs_ChangeEEDAggr                      <external> */
/*+
  Funzione LISP di utilità che modifica il fattore di aggregazione
  della EED di GEOsim.
  Parametri:
  Lista di resbuf (<SelSet> <nuovo fattore di aggr.>)
-*/  
/*********************************************************/
int gs_ChangeEEDAggr(void)
{
   presbuf  arg = acedGetArgs();
   C_EED    eed;
   C_SELSET ss;

   acedRetNil(); 

   if (!arg || arg->restype != RTPICKS) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   ss.add_selset(arg->resval.rlname);
   if (!(arg = arg->rbnext) || arg->restype != RTSHORT)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (eed.save_aggr(ss, arg->resval.rint) == GS_BAD) return RTERROR;
     
   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_ChangeEEDCls                        <external> */
/*+
  Funzione LISP di utilità che modifica il codice classe della EED di GEOsim.
  Parametri:
  Lista di resbuf (<SelSet> <nuovo codice classe>)
-*/  
/*********************************************************/
int gs_ChangeEEDCls(void)
{
   presbuf  arg = acedGetArgs();
   C_EED    eed;
   C_SELSET ss;
   long		ndx = 0;
   ads_name entity;

   acedRetNil(); 

   if (!arg || arg->restype != RTPICKS) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   ss.add_selset(arg->resval.rlname);
   if (!(arg = arg->rbnext) || arg->restype != RTSHORT)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   while (ss.entname(ndx++, entity) == GS_GOOD)
	  eed.set_cls(entity, arg->resval.rint);
     
   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_ChangeODCls                        <external> */
/*+
  Funzione LISP di utilità che modifica il nome della tabella dati oggetto di GEOsim.
  Parametri:
  Lista di resbuf (<SelSet> <oldOD> <newOD>)
-*/  
/*********************************************************/
int gs_ChangeODCls(void) // 2016
{
   presbuf  arg = acedGetArgs();
   C_STRING oldOD, newOD;
   C_SELSET ss;
   long		ndx = 0;
   ads_name entity;

   acedRetNil(); 

   if (!arg || arg->restype != RTPICKS) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   ss.add_selset(arg->resval.rlname);
   if (!(arg = arg->rbnext) || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   oldOD = arg->resval.rstring;
   if (!(arg = arg->rbnext) || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   newOD = arg->resval.rstring;

   // cambio il nome della tabella OD

   // creo la nuova tabella OD se non esiste
   gsc_setODTable(newOD);

   while (ss.entname(ndx++, entity) == GS_GOOD)
      // L'entità viene aggiunta anche in GEOsimAppl::SAVE_SS
      if (gsc_renODTable(entity, oldOD, newOD) == GS_BAD)
         return RTERROR;
     
   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_ClearGSMarker                       <external> */
/*+
  Funzione LISP di utilità che cancella la EED e il link di GEOsim.
  Parametri:
  Lista di resbuf <SelSet>
  codice progetto <Prj> (opzionale)
-*/  
/*********************************************************/
int gs_ClearGSMarker(void)
{
   presbuf  arg = acedGetArgs();
   C_EED    eed;
   long		ndx = 0;
   int      Prj = 0;
   ads_name ent, ss;
   C_STRING ODTableName;

   acedRetNil(); 

   if (!arg || arg->restype != RTPICKS) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   ads_name_set(arg->resval.rlname, ss);
   if ((arg = arg->rbnext) != NULL) // codice progetto opzionale
      gsc_rb2Int(arg, &Prj);

   if (Prj == 0)
      if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
      else Prj = GS_CURRENT_WRK_SESSION->get_PrjId(); // Uso il progetto corrente

	while (acedSSName(ss, ndx++, ent) == RTNORM)
   {
      if (eed.load(ent) == GS_GOOD)
      {
         gsc_getODTableName(Prj, eed.cls, eed.sub, ODTableName);
         gsc_delID2ODTable(ent, ODTableName);
	      eed.clear();
      }
   }

   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_ClearAdeLock                       <external> */
/*+
  Funzione LISP di utilità che cancella la EED per il lock di ADE.
  Parametri:
  Lista di resbuf <SelSet>
-*/  
/*********************************************************/
int gs_ClearAdeLock(void)
{
   presbuf          arg = acedGetArgs();
   long		        ndx = 0;
   AcDbObject       *pObj;
   AcDbObjectId     objId;
   static C_RB_LIST XData;
   ads_name         ent;

   if (XData.get_head() == NULL)
      if ((XData << acutBuildList(1001, _T("ADE_ENTITY_LOCK"), 0)) == NULL)
         return GS_BAD;

	while (acedSSName(arg->resval.rlname, ndx++, ent) == RTNORM)
   {
      if (acdbGetObjectId(objId, ent) != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }  
      if (acdbOpenObject(pObj, objId, AcDb::kForWrite) != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      if (pObj->setXData(XData.get_head()) != Acad::eOk) 
         { pObj->close(); GS_ERR_COD = eGSInvalidEED; return GS_BAD; }
      if (pObj->close() != Acad::eOk) return GS_BAD;
   }

   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_get_eed                             <external> */
/*+
  Funzione LISP di utilità che restituisce i valori della lista di
  dati estesi di un oggetto grafico di GEOsim.
  Parametri:
  <ent>
  
  Restituisce una lista (<cls> <sub> <fattore di aggr.>) in caso di
  successo altrimenti nil.
-*/  
/*********************************************************/
int gs_get_eed(void)
{
   presbuf   arg = acedGetArgs();
   C_RB_LIST ret;
   C_EED     eed;

   acedRetNil(); 
   if (!arg || arg->restype != RTENAME) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (eed.load(arg->resval.rlname) == GS_BAD) return RTNORM;
   ret << acutBuildList(RTSHORT, eed.cls, RTSHORT, eed.sub, RTSHORT, eed.num_el, 0);    
   ret.LspRetList();

   return RTNORM;
}


///////////////////////////////////////////////////////////////////////////
// INIZIO FUNZIONI PER BITMAP
///////////////////////////////////////////////////////////////////////////


/*************************************************************************/
/*.doc gsc_ChangeColorToBmp                                              */
/*+
  Funzione per variare il colore di una bitmap.
  Parametri:
  HBITMAP hBitmap;      Handle della bitmap
  COLORREF crFrom;      Colore da variare
  COLORREF crTo;        Nuovo colore

  La funzione restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
int gsc_ChangeColorToBmp(HBITMAP hBitmap, COLORREF crFrom, COLORREF crTo)
{
   register int cx, cy;
   BITMAP       bm;
   HDC          hdcBmp, hdcMask;
   HBITMAP      hbmMask, hbmOld1, hbmOld2;
   HBRUSH       hBrush, hbrOld;
   DWORD        DSPDxax = 0x00E20746L;

   if (!hBitmap) return GS_BAD;      

   GetObject (hBitmap, sizeof (bm), &bm);     
   cx = bm.bmWidth;     
   cy = bm.bmHeight;      
   hbmMask = CreateBitmap(cx, cy, 1, 1, NULL);     
   hdcMask = CreateCompatibleDC(NULL);     
   hdcBmp  = CreateCompatibleDC(NULL);     
   hBrush  = CreateSolidBrush(crTo);      

   if (!hdcMask || !hdcBmp || !hBrush || !hbmMask)     
   {         
      DeleteObject(hbmMask);         
      DeleteObject(hBrush);         
      DeleteDC(hdcMask);         
      DeleteDC(hdcBmp);         
      return GS_BAD;     
   }      
   
   hbmOld1 = (HBITMAP) SelectObject (hdcBmp,  hBitmap);     
   hbmOld2 = (HBITMAP) SelectObject (hdcMask, hbmMask);     
   hbrOld  = (HBRUSH) SelectObject (hdcBmp, hBrush);      

   SetBkColor(hdcBmp, crFrom);
   BitBlt(hdcMask, 0, 0, cx, cy, hdcBmp,  0, 0, SRCCOPY);     
   SetBkColor(hdcBmp, RGB(255,255,255));     
   BitBlt(hdcBmp,  0, 0, cx, cy, hdcMask, 0, 0, DSPDxax);      

   SelectObject(hdcBmp,  hbmOld1);     
   SelectObject(hdcMask, hbmOld2);     
   DeleteDC(hdcBmp);     
   DeleteDC(hdcMask);     
   DeleteObject(hBrush);     
   DeleteObject(hbmMask);      

   return GS_GOOD; 
}


/*************************************************************************/
/*.doc gsc_get_Bitmap                                                    */
/*+
  Funzione che restituisce vari tipi di bitmap.
  Parametri:
  GSBitmapTypeEnum BitmapType;   Tipo di bitmap da restituire
  CBitmap          &CBMP;        bitmap

  La funzione restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
int gsc_get_Bitmap(GSBitmapTypeEnum BitmapType, CBitmap &CBMP)
{
   UINT nIDResource;
   // When resource from this ARX app is needed, just
   // instantiate a local CAcModuleResourceOverride
   CAcModuleResourceOverride myResources;

   switch (BitmapType)
   {
      case GSAscendingOrderBmp_8X8:
         nIDResource = IDB_ASCENDING_ORDER_8X8;
         break;
      case GSDescendingOrderBmp_8X8:        
         nIDResource = IDB_DESCENDING_ORDER_8X8;
         break;
      case GSExtractedLockedBmp_32X16:
         nIDResource = IDB_EXTRACTEDLOCKED_32X16;
         break;
      case GSExtractedUnLockedBmp_32X16:
         nIDResource = IDB_EXTRACTEDUNLOCKED_32X16;
         break;
      case GSUnextractedLockedBmp_32X16:
         nIDResource = IDB_UNEXTRACTEDLOCKED_32X16;
         break;
      case GSUnextractedUnLockedBmp_32X16:
         nIDResource = IDB_UNEXTRACTEDUNLOCKED_32X16;
         break;
      case GSGridMaskBmp_16X16:
         nIDResource = IDB_GRID_MASK_16X16;
         break;
      case GSGridMaskBmp_32X16:
         nIDResource = IDB_GRID_MASK_32X16;
         break;
      case GSGroupMaskBmp_16X16:
         nIDResource = IDB_GROUP_MASK_16X16;
         break;
      case GSGroupMaskBmp_32X16:
         nIDResource = IDB_GROUP_MASK_32X16;
         break;
      case GSNodeMaskBmp_16X16:
         nIDResource = IDB_NODE_MASK_16X16;
         break;
      case GSNodeMaskBmp_32X16:
         nIDResource = IDB_NODE_MASK_32X16;
         break;
      case GSPolylineMaskBmp_16X16:
         nIDResource = IDB_POLYLINE_MASK_16X16;
         break;
      case GSPolylineMaskBmp_32X16:
         nIDResource = IDB_POLYLINE_MASK_32X16;
         break;
      case GSSimulationMaskBmp_16X16:
         nIDResource = IDB_SIMULATION_MASK_16X16;
         break;
      case GSSimulationMaskBmp_32X16:
         nIDResource = IDB_SIMULATION_MASK_32X16;
         break;
      case GSSpaghettiMaskBmp_16X16:
         nIDResource = IDB_SPAGHETTI_MASK_16X16;
         break;
      case GSSpaghettiMaskBmp_32X16:
         nIDResource = IDB_SPAGHETTI_MASK_32X16;
         break;
      case GSSurfaceMaskBmp_16X16:
         nIDResource = IDB_SURFACE_MASK_16X16;
         break;
      case GSSurfaceMaskBmp_32X16:
         nIDResource = IDB_SURFACE_MASK_32X16;
         break;
      case GSTextMaskBmp_16X16:
         nIDResource = IDB_TEXT_MASK_16X16;
         break;
      case GSTextMaskBmp_32X16:
         nIDResource = IDB_TEXT_MASK_32X16;
         break;
      default:
         return GS_BAD;
   }

   HBITMAP hBitmap=LoadBitmap(_AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCE(nIDResource));
   HGDIOBJ obj = CBMP.Detach();
   if (obj) ::DeleteObject(obj);
   if (CBMP.Attach(hBitmap) == false) return GS_BAD;

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_close_AcDbEntities                                            */
/*+
  Funzione che chiude tutti gli oggetti del vettore.
  Parametri:
  AcDbEntityPtrArray &EntArray; vettore di entities

  La funzione restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
int gsc_close_AcDbEntities(AcDbEntityPtrArray &EntArray)
{
   for (int i = 0; i < EntArray.length(); i++) 
      if (EntArray[i]->close() != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_CenterDialog                                                  */
/*+
  Funzione che centra una dialog.
  Parametri:
  CWnd *MyDialogPtr; puntatore alla dialog
-*/
/*************************************************************************/
void gsc_CenterDialog(CWnd *MyDialogPtr)
{
   CPoint   Point;
   CRect    DialogRect;
   CRect    ParentRect;
   int      nWidth;
   int      nHeight;
   CWnd     *DesktopWindow = NULL;
   CWnd     *MainWindow;

   // Get the size of the dialog box.
   MyDialogPtr->GetWindowRect(DialogRect);

   // Get the main window.
   MainWindow = AfxGetApp()->m_pMainWnd;

   // Determine if the main window exists. This can be useful when
   // the application creates the dialog box before it creates the
   // main window. If it does exist, retrieve its size to center
   // the dialog box with respect to the main window.
   if (MainWindow != NULL)
      MainWindow->GetClientRect(ParentRect);
   // If the main window does not exist, center with respect to
   // the desktop window.
   else
   {
      DesktopWindow = MyDialogPtr->GetDesktopWindow();
      DesktopWindow->GetWindowRect(ParentRect);
   }

   // Calculate the height and width for MoveWindow().
   nWidth = DialogRect.Width();
   nHeight = DialogRect.Height();

   // Find the center point and convert to screen coordinates.
   Point.x = ParentRect.Width() / 2;
   Point.y = ParentRect.Height() / 2;
   if (MainWindow)
      MainWindow->ClientToScreen(&Point);
   else
      DesktopWindow->ClientToScreen(&Point);

   // Calculate the new X, Y starting point.
   Point.x -= nWidth / 2;
   Point.y -= nHeight / 2;

   // Move the window.
   MyDialogPtr->MoveWindow(Point.x, Point.y, nWidth, nHeight);
}


//////////////////////////////////////////////////////////////////////////
// INIZIO FUNZIONI DI GEOCODING
//////////////////////////////////////////////////////////////////////////


/*************************************************************************/
/*.doc gsc_readXMLGoogleData                                               */
/*+
  Funzione che legge i dati XML dal serizio di google map.
  Parametri:
  HINTERNET handle;  connessione internet
  C_STRING &Xml;     stringa risultato

  La funzione restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
int gsc_readXMLGoogleData(HINTERNET handle, C_STRING &Xml)
{
   Xml.clear();
   if (handle == NULL) return GS_BAD;

   const int MAXBLOCKSIZE = 1000;
   byte buffer[MAXBLOCKSIZE] = {0};
   char strBuffer[MAXBLOCKSIZE];
   ULONG Number = 1;

   while (InternetReadFile(handle, buffer, MAXBLOCKSIZE - 1, &Number)  && Number > 0)
   {           
      gsc_strcpy(strBuffer, (const char*) buffer, Number+1);
      strBuffer[Number]= '\0';
      Xml += strBuffer;
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_getAddressFromLL                                               */
/*+
  Funzione che chiede al servizio di google un indirizzo data una coordinata in LL.
  Parametri:
  double _lat;          latitudine
  double _long;         longitudine
  C_STRING &GoogleKey;  Chiave di Google
  C_STRING &Address;    Indirizzo (outout)

  La funzione restituisce GS_GOOD in caso di successo, GS_CAN se non è possibile
  usare il servizio di google per eccesso di richieste (max 2500 / gg) altrimenti GS_BAD.
-*/
/*************************************************************************/
int gsc_getAddressFromLL(double _lat, double _long, C_STRING &GoogleKey, C_STRING &Address)
{
   C_STRING PrefixUrl, Url, Xml;
   
   Address.clear();
   PrefixUrl = _T("https://maps.googleapis.com/maps/api/geocode/xml?latlng=");
   PrefixUrl += _lat;
   PrefixUrl += _T(",");
   PrefixUrl += _long;
   PrefixUrl += _T("&language=it&key=");
   PrefixUrl += GoogleKey;

   // prima provo a cercare un indirizzo
   Url = PrefixUrl;
   Url += _T("&result_type=street_address&address_component_type=street_address");
   //Url = _T("https://maps.googleapis.com/maps/api/geocode/xml?latlng=44.4337100,8.9603700&result_type=route&address_component_type=route&key=AIzaSyC6M3Pbbjdrgtl8QZjuJPK-1JdAJD5oEgA");
   //https://maps.googleapis.com/maps/api/geocode/xml?latlng=44.4337100,8.9603700&language=it&result_type=street_address&address_component_type=street_address&key=AIzaSyC6M3Pbbjdrgtl8QZjuJPK-1JdAJD5oEgA

   HINTERNET hSession = InternetOpen(_T("GEOsim"), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
   if (hSession == NULL) return GS_BAD;

   HINTERNET handle2 = InternetOpenUrl(hSession, Url.get_name(), NULL, 0, INTERNET_FLAG_DONT_CACHE, 0);
   if (handle2 != NULL)
   {
      gsc_readXMLGoogleData(handle2, Xml);
      InternetCloseHandle(handle2);
   }

   if (Xml.at(_T("<status>OK</status>")) == NULL)
   {
      // se non trovo niente provo a cercare solo la via
      Url = PrefixUrl;
      Url += _T("&result_type=route&address_component_type=route");

      HINTERNET handle2 = InternetOpenUrl(hSession, Url.get_name(), NULL, 0, INTERNET_FLAG_DONT_CACHE, 0);
      if (handle2 != NULL)
      {
         gsc_readXMLGoogleData(handle2, Xml);
         InternetCloseHandle(handle2);
      }
      InternetCloseHandle(hSession);

      if (Xml.at(_T("<status>OK</status>")) == NULL)
         if (Xml.at(_T("<status>OVER_QUERY_LIMIT</status>"))) return GS_CAN;
         else return GS_BAD;
   }
   InternetCloseHandle(hSession);
   
   TCHAR *pStart, *pEnd;
   
   if ((pStart = wcsstr(Xml.get_name(), _T("<formatted_address>"))) == NULL) return GS_BAD;
   pStart += gsc_strlen(_T("<formatted_address>"));
   if ((pEnd = wcsstr(pStart, _T("</formatted_address>"))) == NULL) return GS_BAD;
   Address.set_name(pStart, 0, (int) (pEnd - pStart - 1));

   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_getAddressFromLL                    <external> */
/*+
  Questa funzione lisp chiede al servizio di google un indirizzo data una coordinata in LL.
  Parametri:
  (<punto><chiave google>)

  Ritorna un punto, -1 se non è possibile usare il servizio di google per eccesso di richieste (max 2500 / gg)
  o nil in caso di errore.
-*/  
/*********************************************************/
int gs_getAddressFromLL(void)
{
   presbuf  arg = acedGetArgs();
   C_STRING Address, GoogleKey;
   double   _lat, _long;
   int      res;
   
   acedRetNil();

   if (arg->restype != RTPOINT && arg->restype != RT3DPOINT)
      { GS_ERR_COD = eGSInvRBType; return RTERROR; }
   _lat = arg->resval.rpoint[Y];
   _long = arg->resval.rpoint[X];

   if (!(arg = arg->rbnext) || arg->restype != RTSTR )
      { GS_ERR_COD = eGSInvRBType; return RTERROR; }
   GoogleKey = arg->resval.rstring;
   if ((res =gsc_getAddressFromLL(_lat, _long, GoogleKey, Address)) == GS_GOOD)
      acedRetStr(Address.get_name());
   else
      if (res == GS_CAN)
         acedRetInt(-1);

   return RTNORM;
}


/*************************************************************************/
/*.doc gsc_getLLFromAddress                                               */
/*+
  Funzione che chiede al servizio di google una coordinata LL dato un indirizzo.
  Parametri:
  C_STRING &Address;    Indirizzo
  C_STRING &GoogleKey;  Chiave di Google
  double &_lat;         latitudine (outout)
  double &_long;        longitudine (outout)

  La funzione restituisce GS_GOOD in caso di successo, GS_CAN se non è possibile
  usare il servizio di google per eccesso di richieste (max 2500 / gg) altrimenti GS_BAD.
-*/
/*************************************************************************/
int gsc_getLLFromAddress(C_STRING &Address, C_STRING &GoogleKey, double &_lat, double &_long)
{
   C_STRING   Url, Xml;
   C_STR_LIST wordList;
   C_STR      *pWord;
   
   _lat = 0.0;
   _long = 0.0;

   Url = _T("https://maps.googleapis.com/maps/api/geocode/xml?address=");
   if (wordList.from_str(Address.get_name(), _T(' ')) == GS_BAD) return GS_BAD;
   pWord = (C_STR *) wordList.get_head();
   while (pWord)
   {
      if (pWord->len() > 0)
      {
         Url += pWord->get_name();
         pWord = (C_STR *) wordList.get_next();
         if (pWord && pWord->len() > 0) Url += _T("+");
      }
      else
         pWord = (C_STR *) wordList.get_next();
   }
   Url += _T("&language=it&key=");
   Url += GoogleKey;

   HINTERNET hSession = InternetOpen(_T("GEOsim"), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
   if (hSession == NULL) return GS_BAD;

   HINTERNET handle2 = InternetOpenUrl(hSession, Url.get_name(), NULL, 0, INTERNET_FLAG_DONT_CACHE, 0);
   if (handle2 != NULL)
   {
      gsc_readXMLGoogleData(handle2, Xml);
      InternetCloseHandle(handle2);
   }
   InternetCloseHandle(hSession);

   if (Xml.at(_T("<status>OK</status>")) == NULL)
      if (Xml.at(_T("<status>OVER_QUERY_LIMIT</status>"))) return GS_CAN;
      else return GS_BAD;
   
   TCHAR *pStart, *pEnd, *p;
   C_STRING strLat, strLng;
   
   if ((pStart = wcsstr(Xml.get_name(), _T("<geometry>"))) == NULL) return GS_BAD;
   if ((p = wcsstr(pStart, _T("<location>"))) == NULL) return GS_BAD;

   if ((pStart = wcsstr(p, _T("<lat>"))) == NULL) return GS_BAD;
   pStart += gsc_strlen(_T("<lat>"));
   if ((pEnd = wcsstr(pStart, _T("</lat>"))) == NULL) return GS_BAD;
   strLat.set_name(pStart, 0, (int) (pEnd - pStart - 1));
   _lat = strLat.tof();

   if ((pStart = wcsstr(p, _T("<lng>"))) == NULL) return GS_BAD;
   pStart += gsc_strlen(_T("<lng>"));
   if ((pEnd = wcsstr(pStart, _T("</lng>"))) == NULL) return GS_BAD;
   strLng.set_name(pStart, 0, (int) (pEnd - pStart - 1));
   _long = strLng.tof();

   return GS_GOOD;
}
/*********************************************************/
/*.doc gs_getLLFromAddress                    <external> */
/*+
  Questa funzione lisp chiede al servizio di google una coordinata LL dato un indirizzo.
  Parametri:
  (<address><chiave google>)

  Ritorna un punto , -1 se non è possibile usare il servizio di google per eccesso di richieste (max 2500 / gg)
  o nil in caso di errore.
-*/  
/*********************************************************/
int gs_getLLFromAddress(void)
{
   presbuf  arg = acedGetArgs();
   C_STRING Address, GoogleKey;
   double   _lat, _long;
   int      res;
   
   acedRetNil();
   if (arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvRBType; return RTERROR; }
   Address = arg->resval.rstring;
   if (!(arg = arg->rbnext) || arg->restype != RTSTR )
      { GS_ERR_COD = eGSInvRBType; return RTERROR; }
   GoogleKey = arg->resval.rstring;
   if ((res = gsc_getLLFromAddress(Address, GoogleKey, _lat, _long)) == GS_GOOD)
   {
      ads_point pt;
      pt[X] = _long;
      pt[Y] = _lat;
      pt[Z] = 0.0;
      acedRetPoint(pt);
   }
   else
      if (res == GS_CAN)
         acedRetInt(-1);

   return RTNORM;
}



/* Encodes a single int32 to its polyline equivelent.
 val is the int to encode.
 result is the area in memory to write the result to.
 length is the amount of length that the encoding used.
 
 Discussion: the polylines can use up to 6 bytes of memory so result must 
 have at least this amount of space available. */
void encodedIntValue (int val, char *result, unsigned int *length)
{
  bool isNeg = val < 0;
  /* Shift the value right by 1 to make room for the sign bit on the right 
     hand side. */
  val <<= 1;
  
  if (isNeg) {
    /* As the value is stored as a twos compliment value small values have a 
       lot of bits set so not the value. This will also flip the value of the
       sign bit so when we come to decode the value we will know that it is 
       negative. */
    val = ~val;
  }
  
  unsigned int count = 0;
  
  do {
    /* get the smallest 5 bits from our value and add them to the charaters. */
    result[count] = val & 0x1f;
    
    /* We've saved the last 5 bits we can remove them from the value. We shift
       the value by 5 meaning that the next 5 bits that we need to save will be
       at the end of the value. */
    val >>= 5;
    
    if (val) {
      /* We have more bits to encode, so we need to set the continuation bit. */
      result[count] |= 0x20;
    }
    
    result[count] += 63;
    
    ++count;
  } while (val);

  if (length)
    *length = count;
}

/* Encodes a sersion of location Coordinates to a C string and passes the
   string out as the result. */
char *copyEncodedLocationsString (Coordinate *coords, unsigned int coordsCount)
{
  int previousIntLat = 0;
  int previousIntLng = 0;
  unsigned int resultCount = 0;
  
  /* To ensure that we can encode one value we need at least 10 characters, 
     which is why we plus 7. */
  unsigned resultLength = 3 * coordsCount + 7;
  /* Hopefully this should be enough that we don't have to allocate more data
     later. */
  char *result = (char *) malloc (sizeof(char) * resultLength);
  
  
  int intLat;
  int intLng;
  
  unsigned int encodedLen;
  
  for (unsigned int i = 0; i < coordsCount; ++i) {
    /* Convert the current latitude and longitude to their integer 
       representation. */
    intLat = (int) gsc_round(coords[i].latitude * 1e5, 0);
    intLng = (int) gsc_round(coords[i].longitude * 1e5, 0);
    
    /* Encode the difference between the current integer representation, and
       the previous integer representaion. */
    encodedIntValue (intLat - previousIntLat,
                     result + resultCount,
                     &encodedLen);
    resultCount += encodedLen;
    
    /* Then do the same for the latitudes. */
    encodedIntValue (intLng - previousIntLng,
                     result + resultCount,
                     &encodedLen);
    resultCount += encodedLen;
    
    previousIntLat = intLat;
    previousIntLng = intLng;
    
    if (resultLength - resultCount < 10) {
      /* In this case we may overfun our results buffer, so we need to allocate
       more memory. */
      resultLength += (coordsCount - i) * 3 + 7;
      result = (char *) realloc (result, resultLength);
    }
  }
  
  result[resultCount] = '\0';
  result = (char *) realloc(result, sizeof(char) * (resultCount + 1));
  
  return result;
}

/* Decodes the first int32 from the given polyline pointed to by chars. */
int decodeDifferenceVal (char *chars, unsigned int *usedChars)
{
  unsigned int i = 0;
  int result = 0;
  char currentByte;
  
  do {
    currentByte = chars[i] - 63;
    result |= (uint32_t)(currentByte & 0x1f) << (5 * i);
    ++i;
  } while (currentByte & 0x20);
  
  if (result & 1) {
    result = ~result;
  }
  
  result >>= 1;
  
  *usedChars = i;
  return result;
}

/* Decodes the polyline c string back into its coordinates. */
Coordinate *decodeLocationsString (char *polylineString, unsigned int *locsCount)
{
  if (!polylineString) return NULL;
  int charsCount = (int) strlen (polylineString);
  if (!charsCount) return NULL;
  
  unsigned int resultLength = charsCount / 3 + 2;
  Coordinate *result = (Coordinate *) malloc (sizeof(Coordinate) * resultLength);
  
  unsigned totalUsedChars = 0;
  unsigned usedChars = 0;
  unsigned resultCount = 0;
  
  int intLat = 0;
  int intLng = 0;
  
  do {
    intLat += decodeDifferenceVal (polylineString + totalUsedChars, &usedChars);
    totalUsedChars += usedChars;
    
    intLng += decodeDifferenceVal (polylineString + totalUsedChars, &usedChars);
    totalUsedChars += usedChars;
    
    result[resultCount].latitude  = intLat * 1e-5;
    result[resultCount].longitude = intLng * 1e-5;
    
    ++resultCount;
    
    if (resultCount >= resultLength - 1) {
      resultLength += (charsCount - totalUsedChars) / 3 + 2;
      result = (Coordinate *) realloc (result, resultLength * sizeof(Coordinate));
    }
    
  } while (totalUsedChars != charsCount);
  
  result = (Coordinate *) realloc (result, resultCount * sizeof(Coordinate));
  *locsCount = resultCount;
  return result;
}


/*************************************************************************/
/*.doc gsc_URLForOriginKeyPointDestination                              */
/*+
  Funzione che aggiunge ad una stringa (result) la parte di url relativa
  al punto di origine, i punti intermedi e quello di destinazione.
  Parametri:
  C_POINT_LIST &Pts;       Lista di punti
  int maxPts;              Numero massimo di punti (attualmente Google accetta max 25 punti,
                           23 keypoint + origine e destinazione)
  C_STRING &result;       (outout)

  La funzione restituisce il numero di punti letti da Pts in caso di successo altrimenti -1.
-*/
/*************************************************************************/
int gsc_URLForOriginKeyPointDestination(C_POINT_LIST &Pts, int maxPts, C_STRING &result)
{
   C_POINT  *pPt;
   int      i = 1;
   unsigned int coordsCount = 0;
   Coordinate *coords = NULL;

   // https://maps.googleapis.com/maps/api/directions/xml?origin=sydney,au&destination=perth,au&waypoints=via:enc:lexeF{~wsZejrPjtye@:&key=YOUR_API_KEY
   
   // il cursore corrente è l'origine
   if (!(pPt = (C_POINT *) Pts.get_cursor())) return -1;
   result += _T("origin=");
   result += pPt->y(); result += _T(","); result += pPt->x(); // lat, lon
   // punti intermedi
   pPt = (C_POINT *) Pts.get_next();
   while (pPt)
   {
      i++;
      if (pPt->get_next()) // non è l'ultimo punto
         if (i < maxPts) // punto intermedio
         {
            coordsCount++;
            if (coords)
               coords = (Coordinate *) realloc (coords, coordsCount * sizeof(Coordinate));
            else
               coords = (Coordinate *) malloc (sizeof(Coordinate));
            
            coords[coordsCount - 1].latitude  = pPt->y();
            coords[coordsCount - 1].longitude = pPt->x();
         }
         else // è l'ultimo punto perchè sono arrivato a maxPts
            break;
      else // è l'ultimo punto
         break;

      pPt = (C_POINT *) Pts.get_next();
   }

   if (!pPt) { if (coords) free(coords); return -1; }

   result += _T("&destination=");
   result += pPt->y(); result += _T(","); result += pPt->x(); // lat, lon

   if (coords) // se ci sono waypoints
   {
      char *wayPts;

      if ((wayPts = copyEncodedLocationsString(coords, coordsCount)) == NULL)
         { free(coords); return -1; }
      result += _T("&waypoints=via:enc:");
      result += wayPts;
      result += _T(":");
      free(wayPts);
      free(coords);
   }
   
   return i;
}


/*************************************************************************/
/*.doc gsc_getPtsDirectionsFromGoogle                                    */
/*+
  Funzione che invia la richiesta a google e legge un percorso.
  Parametri:
  C_STRING &Url;
  C_POINT_LIST &result;    Lista dei punti che formano il percorso (outout)

  La funzione restituisce GS_GOOD in caso di successo, GS_CAN se non è possibile
  usare il servizio di google per eccesso di richieste (max 2500 / gg) altrimenti GS_BAD.
-*/
/*************************************************************************/
int gsc_getPtsDirectionsFromGoogle(C_STRING &Url, C_POINT_LIST &result)
{
   Coordinate *coords;
   unsigned int coordsCount;
   C_STRING  polylineStringUnicode, Xml;
   char      *polylineString;
   HINTERNET hSession = InternetOpen(_T("GEOsim"), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
   if (hSession == NULL) return GS_BAD;

   HINTERNET handle2 = InternetOpenUrl(hSession, Url.get_name(), NULL, 0, INTERNET_FLAG_DONT_CACHE, 0);
   if (handle2 != NULL)
   {
      gsc_readXMLGoogleData(handle2, Xml);
      InternetCloseHandle(handle2);
   }
   InternetCloseHandle(hSession);

   if (Xml.at(_T("<status>OK</status>")) == NULL)
      if (Xml.at(_T("<status>OVER_QUERY_LIMIT</status>"))) return GS_CAN;
      else return GS_BAD;
   
   TCHAR *pStart, *pEnd, *p;

   if ((pStart = wcsstr(Xml.get_name(), _T("<route>"))) == NULL) return GS_BAD;
   if ((p = wcsstr(pStart, _T("<overview_polyline>"))) == NULL) return GS_BAD;

   if ((pStart = wcsstr(p, _T("<points>"))) == NULL) return GS_BAD;
   pStart += gsc_strlen(_T("<points>"));
   if ((pEnd = wcsstr(pStart, _T("</points>"))) == NULL) return GS_BAD;
   polylineStringUnicode.set_name(pStart, 0, (int) (pEnd - pStart - 1));

   if (!(polylineString = gsc_UnicodeToChar(polylineStringUnicode.get_name()))) return GS_BAD;
   if ((coords = decodeLocationsString(polylineString, &coordsCount)) == NULL)
      { free(polylineString); return GS_BAD; }
   free(polylineString);

   result.remove_all();
   for (unsigned int i = 0; i < coordsCount; i++)
      result.add_point(coords[i].longitude, coords[i].latitude, 0.0);


   //result.remove_all();
   //p = pStart;
   //while ((p = wcsstr(p, _T("<step>"))))
   //{
   //   if ((pStart = wcsstr(p, _T("<points>"))) == NULL) return GS_BAD;
   //   pStart += gsc_strlen(_T("<points>"));
   //   if ((pEnd = wcsstr(pStart, _T("</points>"))) == NULL) return GS_BAD;
   //   polylineStringUnicode.set_name(pStart, 0, (int) (pEnd - pStart - 1));

   //   if (!(polylineString = gsc_UnicodeToChar(polylineStringUnicode.get_name()))) return GS_BAD;
   //   if ((coords = decodeLocationsString(polylineString, &coordsCount)) == NULL)
   //      { free(polylineString); return GS_BAD; }
   //   free(polylineString);

   //   if (result.get_count() == 0)
   //      for (unsigned int i = 0; i < coordsCount; i++)
   //         result.add_point(coords[i].longitude, coords[i].latitude, 0.0);
   //   else
   //      for (unsigned int i = 1; i < coordsCount; i++)
   //         result.add_point(coords[i].longitude, coords[i].latitude, 0.0);
   //   p = pEnd;
   //}

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_getLLDirectionFromPts                                           */
/*+
  Funzione che chiede al servizio di google un percorso passante per un max 25 punti.
  Le coordinate in input e output sono in LL.
  Parametri:
  C_POINT_LIST &Pts;       Lista di punti
  C_STRING &GoogleKey;     Chiave di Google
  C_POINT_LIST &result;    Lista dei punti che formano il percorso (outout)

  La funzione restituisce GS_GOOD in caso di successo, GS_CAN se non è possibile
  usare il servizio di google per eccesso di richieste (max 2500 / gg) altrimenti GS_BAD.
-*/
/*************************************************************************/
int gsc_getLLDirectionFromPts(C_POINT_LIST &Pts, C_STRING &GoogleKey, C_POINT_LIST &result)
{
   C_STRING     Url;
   int          ptsCount = Pts.get_count(), google_result;
   int          maxPts = 25; // attualmente Google accetta max 25 punti, 23 keypoint + origine e destinazione (deve essere > 2)
   C_POINT_LIST partial;
   C_POINT      *ptRef = NULL, *pPt, *tail;
   //maxPts = 5; test

   if (ptsCount < 2) return GS_BAD;
   result.remove_all();
   // https://maps.googleapis.com/maps/api/directions/xml?origin=sydney,au&destination=perth,au&waypoints=via:enc:lexeF{~wsZejrPjtye@:&key=YOUR_API_KEY
   tail = (C_POINT *) Pts.get_tail();
   Pts.get_head();
   while (Pts.get_cursor() != tail) // se non sono arrivato all'ultimo punto
   {
      Url = _T("https://maps.googleapis.com/maps/api/directions/xml?");

      if (gsc_URLForOriginKeyPointDestination(Pts, maxPts, Url) == -1) return GS_BAD;

      Url += _T("&language=it&key=");
      Url += GoogleKey;
      // leggo il percorso
      google_result = gsc_getPtsDirectionsFromGoogle(Url, partial);
      if (google_result != GS_GOOD) return google_result;

      pPt = (C_POINT *) partial.get_head();
      if (ptRef)
      {  // scorro il risultato parziale fino al punto ptRef
         while (pPt)
            // uso una tolleranza di 1e-6
            if (((pPt->x() > ptRef->x()) ? pPt->x()-ptRef->x() : ptRef->x()-pPt->x()) <= 1e-5 &&
                ((pPt->y()>ptRef->y()) ? pPt->y()-ptRef->y() : ptRef->y()-pPt->y()) <= 1e-5)
            {
               pPt = (C_POINT *) partial.get_next();
               break;
            }
            else
               pPt = (C_POINT *) partial.get_next();
      }

      // aggiungo i punti alla lista result      
      while (pPt)
      {
         result.add_point(pPt->x(), pPt->y(), 0.0);
         pPt = (C_POINT *) partial.get_next();
      }

      if (Pts.get_cursor() != tail)
      {
         Pts.get_prev(); // torno indietro di un punto per dare la direzione
         ptRef = (C_POINT *) result.get_tail();
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_getLLDirectionFromPts                 <external> */
/*+
  Questa funzione lisp chiede al servizio di google un percorso che passi
  attraverso una lista di punti. Le coordinate in input e output sono in LL.
  Parametri:
  (<pt list><chiave google>)

  Ritorna una lista di punti che formano il percorso (LL) in caso di successo, -1 
  se non è possibile usare il servizio di google per eccesso di richieste (max 2500 / gg)
  o nil in caso di errore.
-*/  
/*********************************************************/
int gs_getLLDirectionFromPts(void)
{
   presbuf      arg = acedGetArgs();
   C_POINT_LIST Pts, result;
   C_STRING     GoogleKey;
   int          res;
   
   acedRetNil();

   if (!arg || arg->restype != RTLB)
      { GS_ERR_COD = eGSInvRBType; return RTERROR; }
   while ((arg = arg->rbnext) && (arg->restype == RTPOINT || arg->restype == RT3DPOINT))
      Pts.add_point(arg->resval.rpoint);
   if (arg->restype != RTLE)
      { GS_ERR_COD = eGSInvRBType; return RTERROR; }

   if (!(arg = arg->rbnext) || arg->restype != RTSTR )
      { GS_ERR_COD = eGSInvRBType; return RTERROR; }
   GoogleKey = arg->resval.rstring;
   if ((res = gsc_getLLDirectionFromPts(Pts, GoogleKey, result)) == GS_GOOD)
   {
      presbuf pRb = result.to_rb();
      acedRetList(pRb);
      acutRelRb(pRb); 
   }
   else
      if (res == GS_CAN)
         acedRetInt(-1);

   return RTNORM;
}



/*********************************************************/
/*.doc gs_getGUID                             <external> */
/*+
  Questa funzione lisp restituisce una stringa uivoca GUID.
  -*/  
/*********************************************************/
int gsc_getGUID(C_STRING &Res)
{
   GUID guid;
   HRESULT hr = CoCreateGuid(&guid); 

   if (hr != S_OK) return GS_BAD;

   // Convert the GUID to a string
   RPC_WSTR guidStr;
   if (UuidToString(&guid, &guidStr) != RPC_S_OK)
      return GS_BAD;

   Res = (LPTSTR) guidStr;
   RpcStringFree(&guidStr);
   return GS_GOOD;
}
int gs_getGUID(void)
{
   C_STRING guid;

   if (gsc_getGUID(guid) != GS_GOOD) 
   {
      acedRetNil();
      return RTERROR;
   }

   acedRetStr(guid.get_name());
   return RTNORM;
}
