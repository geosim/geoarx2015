/**********************************************************
Name: GS_NETW.CPP
                                   
Module description: File funzioni di base per la gestione
                    dei nodi di rete e conversione tra path logico
                    e path reale. 
            
Author: Roberto Poltini & Paolo De Sole

(c) Copyright 2003-2015 by IREN ACQUA GAS S.p.a.

**********************************************************/


/**************************************************************************/
/*  INCLUDE  */
/**************************************************************************/


#include "stdafx.h"         // MFC core and standard components

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")


#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "rxdefs.h"   
#include "adslib.h"   
#include <adeads.h>

#include "gs_opcod.h"     // codici delle operazioni
#include "..\gs_def.h" // definizioni globali
#include "gs_error.h"   
#include "gs_list.h"   
#include "gs_utily.h"
#include "gs_init.h"   
#include "gs_user.h"      // check_op     
#include "gs_sql.h"
#include "gs_netw.h"   
#include "gs_resbf.h"
#include "gs_class.h"
#include "gs_sec.h"
#include "gs_prjct.h"      // gestione progetti
#include "gs_graph.h"
#include "gs_cmd.h"


/**********************************************************************/
/*   PRIVATE VARIABLES */
/**********************************************************************/


/**********************************************************************/
/*   GLOBAL VARIABLES */
/**********************************************************************/


/**********************************************************************/
/*   PRIVATE FUNCTIONS */
/**********************************************************************/


int gsc_ConvAlias(const TCHAR *NewAlias, const TCHAR *OldAlias, int prj = -1,
                  int cls = -1, int sub = -1, int sec = -1, const TCHAR *AttribName = NULL);
int gsc_ConvUDLProperties(C_STRING &FullUDLFile, C_STRING &UDLProperties,
                          C_STRING &CpyOldAlias, C_STRING &CpyNewAlias);


/*************************************************************************/
/*.doc gsc_OSCode2OSName                                                  */
/*+
   Funzione che restituisce il nome del sistema operativo
   prendendo in input il codice che lo rappresenta.
   Parametri:
   enum OperatingSystemsEnum OSCode; Codice del sistema operativo
   
   Restituisce il nome del sistema operativo in caso di successo altrimenti NULL.
-*/
/*************************************************************************/
const TCHAR* gsc_OSCode2OSName(enum OperatingSystemsEnum OSCode)
{
   switch (OSCode)
   {
      case GS_OS_DOS:
         return gsc_msg(167); // "DOS"
      case GS_OS_WINDOWS:
         return gsc_msg(168); // "WINDOWS"
      case GS_OS_WINDOWS_NT_2000:
         return gsc_msg(169); // "WINDOWS NT/2000"
      case GS_OS_UNIX:
         return gsc_msg(170); // "UNIX"
      case GS_OS_WINDOWS_95_98:
         return gsc_msg(599); // "WINDOWS 95/98"
      case GS_OS_WINDOWS_XP:
         return gsc_msg(469); // "WINDOWS XP"
      case GS_OS_WINDOWS_VISTA:
         return gsc_msg(470); // "WINDOWS VISTA"
      default:
         GS_ERR_COD = eGSInvOpSys;
         return NULL; 
   }
}

/*************************************************************************/
/*.doc gsc_OSName2OSCode                                                 */
/*+
   Funzione che restituisce il codice del sistema operativo 
   prendendo in inpout il nome che lo rappresenta.
   Parametri
   const TCHAR *OSName;

   Ritorna il codice del sistema operativo
-*/
/*************************************************************************/
enum OperatingSystemsEnum gsc_OSName2OSCode(const TCHAR *OSName)
{
   if (_wcsicmp(OSName, gsc_msg(167)) == 0)       // "DOS"
      return GS_OS_DOS;
   else if (_wcsicmp(OSName, gsc_msg(168)) == 0)  // "WINDOWS"
      return GS_OS_WINDOWS;
   else if (_wcsicmp(OSName, gsc_msg(169)) == 0)  // "WINDOWS NT/2000"
      return GS_OS_WINDOWS_NT_2000;
   else if (_wcsicmp(OSName, gsc_msg(170)) == 0)  // "UNIX"
      return GS_OS_UNIX;
   else if (_wcsicmp(OSName, gsc_msg(599)) == 0)  // "WINDOWS 95/98"
      return GS_OS_WINDOWS_95_98;
   else if (_wcsicmp(OSName, gsc_msg(469)) == 0)  // "WINDOWS XP"
      return GS_OS_WINDOWS_XP;
   else if (_wcsicmp(OSName, gsc_msg(470)) == 0)  // "WINDOWS VISTA"
      return GS_OS_WINDOWS_VISTA;

   GS_ERR_COD = eGSInvOpSys;

   return GS_OS_UNKNOWN;
}                                                              


/**********************************************************************/
/*  C_ALIAS INIZIO                                                 */
/**********************************************************************/


//-----------------------------------------------------------------------//
int C_ALIAS::set_host(const TCHAR *in) 
{
   size_t len, lenhost;

   if ((in == NULL) || (wcslen(in) == 0))
   {
      if (host != NULL) { free(host); host = NULL; }
      return GS_GOOD;
   }
   len = wcslen(in);
   if (len == 0) { GS_ERR_COD = eGSStringNull; return GS_BAD; }
   lenhost = (host == NULL) ? 0 : wcslen(host);
   if (len > lenhost)
      if ((host = (TCHAR *) realloc(host, (len + 1) * sizeof(TCHAR))) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   wcscpy(host, in);
   
   return GS_GOOD;
}


//-----------------------------------------------------------------------//
int C_ALIAS::set_path(const TCHAR *in) 
{
   size_t len, lenpath;

   if ((in == NULL) || (wcslen(in) == 0))
   {
      if (path != NULL) { free(path); path = NULL; }
      return GS_GOOD;
   }
            
   len = wcslen(in);
   if (len == 0)           { GS_ERR_COD = eGSStringNull; return GS_BAD; }
   lenpath = (path == NULL) ? 0 : wcslen(path);
   if (len > lenpath)
      if ((path = (TCHAR *) realloc(path, (len + 1) * sizeof(TCHAR))) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   wcscpy(path, in);
   
   return GS_GOOD;
}


/*************************************************************************/
/*.doc (new 2) C_ALIAS::is_valid_path()                                  */
/*+
   Funzione che controlla la correttezza della path in base al sistema operativo
   ritorna:
      GS_BAD    errore      
      GS_GOOD   Ok, path corretta             
-*/
/*************************************************************************/
int C_ALIAS::is_valid_path()
{
   int Result = GS_GOOD;

   switch (op_sys)
   {
      case GS_OS_UNIX:
         if (!path || wcslen(path) == 0) Result = GS_BAD;
         break;
      default:
         if (!path || wcslen(path) > 2) Result = GS_BAD;
         else
            if (!iswalpha(path[0]) || path[1] != _T(':')) Result = GS_BAD;

         gsc_toupper(path);
   }

   if (Result == GS_BAD)
   {
      if (host) acutPrintf(gsc_msg(618), host); // \nLa path dell'alias <%s> non è valida (deve essere 2 caratteri, es. 'C:', 'D:'...).
      GS_ERR_COD = eGSInvalidPath;
   }

   return Result;
}


/*************************************************************************/
/*.doc (new 2) C_ALIAS::is_valid_host()                                  */
/*+
   Funzione che controlla la correttezza dell'host 
   ritorna:
      GS_GOOD  errore      
      GS_BAD   Ok, host corretto             
-*/
/*************************************************************************/
int C_ALIAS::is_valid_host()
{
   TCHAR  insieme_valido[] = _T("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz");
   size_t i, len;
   int    Result = GS_GOOD;

   do
   {
      if (!host) { Result = GS_BAD; break; }
      gsc_toupper(host);
      len = wcslen(host);   
      if (len >= MAX_LEN_HOSTNAME || wcslen(host) == 0) { Result = GS_BAD; break; }
      
      for (i = 0; i < len; i++)
         if (wcschr(insieme_valido, host[i]) == NULL)
            { GS_ERR_COD = eGSInvalidHost; Result = GS_BAD; break; }
      
      if (Result == GS_BAD) break;

      Result = GS_GOOD;
   }
   while (0);

   if (Result == GS_BAD)
   {
      acutPrintf(gsc_msg(619), host); // "\nL'alias <%s> non è valida (ammessi solo caratteri alfanumerici + '_')."
      GS_ERR_COD = eGSInvalidHost;
   }
   
   return Result;
}

/*************************************************************************/
/*.doc (new 2) C_ALIAS::is_valid()                    */
/*+
   Funzione che controlla la correttezza dei valori di 
   Path, Host, System
   ritorna:
      GS_BAD   errore      
      GS_GOOD  Ok, dati giusti             
-*/
/*************************************************************************/
int C_ALIAS::is_valid()
{
   if (is_valid_path()==GS_GOOD)
      if (is_valid_host()==GS_GOOD)
         return GS_GOOD;

   return GS_BAD;
}


/*************************************************************************/
/*.doc (new 2) C_ALIAS::save()                    */
/*+
   Funzione che salva un elemento della classe C_ALIAS 
   parametri:
   FILE *file;    puntatore al file di inizializzazione di GEOsim
   bool  Unicode; Flag che determina se il contenuto del file è in 
                  formato UNICODE o ANSI (default = false)
   
   Ritorna GS_BAD in caso di errore altrimenti GS_GOOD.
-*/
/*************************************************************************/
int C_ALIAS::save(FILE *file, bool Unicode)
{
   C_ALIAS *pogg;

   if ((pogg = GEOsimAppl::GS_NET_LIST.search_host(get_host()))==NULL)
   {
      // inserimento nuovo alias (scrivo su file) con funzioni di ADE
      if (ade_aliasadd(get_host(), get_path()) == RTERROR)
         { GS_ERR_COD = eGSInvalidAdeIni; return GS_BAD; }
      // inserimento nuovo alias (scrivo su file) sezione di GEOsim
      if (gsc_set_profile(file, GS_NET_LABEL, get_host(), 
                          gsc_OSCode2OSName(get_op_sys()), 0 , Unicode) == GS_BAD)
         return GS_BAD;

      if ((pogg = new C_ALIAS)==NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }      
      pogg->set_path(get_path());
      pogg->set_host(get_host());
      pogg->set_op_sys(get_op_sys());
      GEOsimAppl::GS_NET_LIST.add_tail(pogg);
   }
   else
   {
      // modifica di un alias (scrivo su file) con funzioni di ADE
      if (ade_aliasupdate(get_host(), get_path()) == RTERROR)
         { GS_ERR_COD = eGSInvalidAdeIni; return GS_BAD; }
      // modifica di un alias (scrivo su file) sezione di GEOsim
      if (gsc_set_profile(file, GS_NET_LABEL, get_host(), 
                          gsc_OSCode2OSName(get_op_sys()), 0, Unicode) == GS_BAD)
         return GS_BAD;
      pogg->set_path(get_path());
      pogg->set_host(get_host());
      pogg->set_op_sys(get_op_sys());
   }  

   return GS_GOOD;
}


/*************************************************************************/
/*.doc (new 2) C_ALIAS::del()                    */
/*+
   Funzione che cancella un elemento della classe C_ALIAS 
   parametri:
   FILE *file;  puntatore al file di inizializzazione di GEOsim
   bool  Unicode; Flag che determina se il contenuto del file è in 
                  formato UNICODE o ANSI (default = false)
   
   Ritorna GS_BAD in caso di errore altrimenti GS_GOOD.
-*/
/*************************************************************************/
int C_ALIAS::del(FILE *file, bool Unicode)
{
   C_ALIAS  *pogg;
   C_STRING Buffer;

   // cancellazione alias con funzioni di ADE
   // non funziona se ci sono DWG attaccati che utilizzano l'alias
   if (ade_aliasdelete(get_host()) == RTERROR)
      { GS_ERR_COD = eGSInvalidAdeIni; return GS_BAD; }
   // se esiste nella sezione
   if (gsc_get_profile(file, GS_NET_LABEL, get_host(), Buffer, Unicode) == GS_GOOD &&
       Buffer.len() > 0)
      // cancellazione alias dalla sezione di GEOsim
      if (gsc_delete_entry(file, GS_NET_LABEL, get_host(), Unicode) == GS_BAD)
         return GS_BAD;

   if ((pogg = (C_ALIAS *) GEOsimAppl::GS_NET_LIST.search_host(get_host())) == NULL) return GS_BAD;
   GEOsimAppl::GS_NET_LIST.remove(pogg);

   return GS_GOOD;
}


/**********************************************************************/
/*  C_ALIAS      FINE                                                 */
/*  C_ALIAS_LIST INIZIO                                               */
/**********************************************************************/


/*************************************************************************/
/*.doc (new 2) C_ALIAS_LIST::save()                                      */
/*+
   Funzione che  la lista degli Alias 
   parametri:
   <char *path> .. file Ade.ini(.. per ora)
   ritorna:
      GS_BAD   errore      
      GS_GOOD  Ok il salvataggio è andato bene             
-*/
/*************************************************************************/
int C_ALIAS_LIST::save()
{
   C_ALIAS  *punt;
   C_STRING pathfile;
   FILE     *file;
   bool     Unicode = false;

   // Questa funzione è utilizzabile solo se non esiste una sessione di lavoro attiva
   // e quindi non ci sono DWG attaccati perchè vengono richiamate alcune funzioni ADE
   // (es. "ade_aliasdelete") che non vanno e non restituiscono messaggio di errore
   // se ci sono dei DWG attaccati che utilizzano l'alias.
   if (GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSSessionsFound; return GS_BAD; }

   if (get_count() <= 0) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   // Apro il file di inizializzazione GEOSIM.INI
   pathfile = GEOsimAppl::CURRUSRDIR; // Directory locale dell'utente corrente;
   pathfile += _T('\\');
   pathfile += GS_INI_FILE;

   if ((file = gsc_open_profile(pathfile, UPDATEABLE, MORETESTS, &Unicode)) == NULL)
      return GS_BAD;

   // ciclo di aggiornamento e inserimento nuovi nodi
   punt = (C_ALIAS *) get_head();
   while (punt)
   {
      if (punt->save(file, Unicode) == GS_BAD)
         { gsc_close_profile(file); return GS_BAD; }
      punt =(C_ALIAS *) punt->get_next();      
   }

   // ciclo di cancellazione nodi
   punt = (C_ALIAS *) GEOsimAppl::GS_NET_LIST.get_head();
   while (punt)
   {
      if (search_host(punt->get_host()) == NULL)
      {
         if (punt->del(file, Unicode) == GS_BAD)
            { gsc_close_profile(file); return GS_BAD; }
         punt = (C_ALIAS *) GEOsimAppl::GS_NET_LIST.get_cursor();
      }
      else punt =(C_ALIAS *) punt->get_next();      
   }

   // Chiudo il file di inizializzazione GEOSIM.INI
   if (gsc_close_profile(file) == GS_BAD) return GS_BAD;

   // Salvo la lista degli host sul server
   saveHostsToSrv();

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
C_ALIAS *C_ALIAS_LIST::search_host(const TCHAR *in, int CaseSensitive)
{
   C_ALIAS *p;

   p = (C_ALIAS *) get_head();
   while (p != NULL)
   { 
      if (gsc_strcmp(p->get_host(), in, CaseSensitive) == 0) break;
      p = (C_ALIAS *) get_next();
   }

   return p;
}   

//-----------------------------------------------------------------------//
C_ALIAS *C_ALIAS_LIST::search_host_xptr(const TCHAR *in, C_ALIAS *pAlias,
                                        int CaseSensitive)
{
   C_ALIAS *p;

   if (CaseSensitive==GS_GOOD)
   {
      p = (C_ALIAS *) get_head();
      while (p != NULL)
      { 
         if (pAlias!=p && gsc_strcmp(p->get_host(), in) == 0) break;
         p = (C_ALIAS *) get_next();
      }
   }
   else
   {
      TCHAR dummy1[MAX_LEN_HOSTNAME], dummy2[MAX_LEN_HOSTNAME];

      wcscpy(dummy1, in);
      gsc_toupper(dummy1);
      p = (C_ALIAS *) get_head();
      while (p != NULL)
      {
         wcscpy(dummy2, p->get_host());
         gsc_toupper(dummy2);
         if (pAlias!=p && gsc_strcmp(dummy2, dummy1) == 0) break;
         p = (C_ALIAS *) get_next();
      }

   }
   
   if (p==NULL) GS_ERR_COD=eGSInvalidHost;

   return p;
}   


//-----------------------------------------------------------------------//
C_ALIAS *C_ALIAS_LIST::search_path(const TCHAR *in, int CaseSensitive)
{
   C_ALIAS *p;

   if (CaseSensitive==GS_GOOD)
   {
      p = (C_ALIAS *) get_head();
      while (p != NULL)
      { 
         if (gsc_strcmp(p->get_path(), in) == 0) break;
         p = (C_ALIAS *) get_next();
      }
   }
   else
   {
      TCHAR dummy1[MAX_LEN_HOSTNAME], dummy2[MAX_LEN_HOSTNAME];

      wcscpy(dummy1, in);
      gsc_toupper(dummy1);
      p = (C_ALIAS *) get_head();
      while (p != NULL)
      {
         wcscpy(dummy2, p->get_path());
         gsc_toupper(dummy2);
         if (gsc_strcmp(dummy2, dummy1) == 0) break;
         p = (C_ALIAS *) get_next();
      }
   }

   return p;
}   

//-----------------------------------------------------------------------//
C_ALIAS *C_ALIAS_LIST::search_path_xptr(const TCHAR *in, C_ALIAS *pAlias,
                                        int CaseSensitive)
{
   C_ALIAS *p;

   if (CaseSensitive==GS_GOOD)
   {
      p = (C_ALIAS *) get_head();
      while (p != NULL)
      { 
         if (pAlias!=p && gsc_strcmp(p->get_path(), in) == 0) break;
         p = (C_ALIAS *) get_next();
      }
   }
   else
   {
      TCHAR dummy1[MAX_LEN_HOSTNAME], dummy2[MAX_LEN_HOSTNAME];

      wcscpy(dummy1, in);
      gsc_toupper(dummy1);
      p = (C_ALIAS *) get_head();
      while (p != NULL)
      {
         wcscpy(dummy2, p->get_path());
         gsc_toupper(dummy2);
         if (pAlias!=p && gsc_strcmp(dummy2, dummy1) == 0) break;
         p = (C_ALIAS *) get_next();
      }

   }

   return p;
}   


//-----------------------------------------------------------------------//
int C_ALIAS_LIST::copy(C_ALIAS_LIST *out)
{
   C_ALIAS *new_node, *punt;

   if (out==NULL) { GS_ERR_COD = eGSNotAllocVar; return GS_BAD; }

   punt=(C_ALIAS*)get_head();
   out->remove_all();
   
   while(punt!=NULL)
   {
      if ((new_node= new C_ALIAS)==NULL)
         { remove_all(); GS_ERR_COD=eGSOutOfMem; return GS_BAD; }      
      new_node->set_path(punt->get_path());
      new_node->set_host(punt->get_host());
      new_node->set_op_sys(punt->get_op_sys());
      out->add_tail(new_node);
      punt=(C_ALIAS*)get_next();
   }

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
int C_ALIAS_LIST::add_to_mem(C_ALIAS *pAlias)
{
   if (!pAlias) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (pAlias->is_valid() == GS_BAD) return GS_BAD;

   // verifico eventuale esistenza di un path uguale (se UNIX case sensitive)
   if (search_path(pAlias->get_path(), 
                  (pAlias->get_op_sys() != GS_OS_UNIX) ? GS_BAD : GS_GOOD) != NULL)
   {  // "\nLa path <%s> avente alias <%s>, è stata scartata perchè già presente in lista."
      acutPrintf(gsc_msg(635), pAlias->get_host(), pAlias->get_path());
      GS_ERR_COD = eGSPathAlreadyExisting;
      return GS_BAD;
   }
   // verifico eventuale esistenza di un host uguale
   if (search_host(pAlias->get_host()) != NULL)
   {  // "\nL'alias <%s> avente path <%s>, è stato scartato perchè già presente in lista."
      acutPrintf(gsc_msg(288), pAlias->get_host(), pAlias->get_path());
      GS_ERR_COD = eGSAliasAlreadyExisting;
      return GS_BAD;
   }

   if (pAlias->get_op_sys() == GS_OS_UNIX)
   {
      C_STRING upperstr;
      C_ALIAS  *punt;

      // cerco il path maiuscolo e, se esiste, verifico che sia UNIX altrimenti errore
      upperstr = pAlias->get_path();
      upperstr.toupper();
      if ((punt = search_path(upperstr.get_name())) != NULL &&
           punt->get_op_sys() != GS_OS_UNIX)
         { GS_ERR_COD = eGSPathAlreadyExisting; return GS_BAD; }
   }

   add_tail(pAlias); 

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
int C_ALIAS_LIST::upd_to_mem(C_ALIAS *pOrig, C_ALIAS *pAlias)
{
   C_ALIAS *p;
   int      unix_sys;

   if (!pOrig || !pAlias) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (pAlias->is_valid() == GS_BAD) return GS_BAD;

   unix_sys = (pAlias->get_op_sys() != GS_OS_UNIX) ? GS_BAD : GS_GOOD;

   // verifico eventuale esistenza di un path uguale (se UNIX case sensitive)
   if ((p = search_path_xptr(pAlias->get_path(), pOrig, unix_sys)) != NULL)
      { GS_ERR_COD = eGSAliasAlreadyExisting; return GS_BAD; }
   // verifico eventuale esistenza di un host uguale (se UNIX case sensitive)
   if ((p = search_host_xptr(pAlias->get_host(), pOrig, unix_sys)) != NULL)
      { GS_ERR_COD = eGSAliasAlreadyExisting; return GS_BAD; }

   if (unix_sys == GS_GOOD)
   {
      C_STRING upperstr;
      C_ALIAS  *punt;

      // cerco il path maiuscolo e, se esiste, verifico che sia UNIX altrimenti errore
      upperstr = pAlias->get_path();
      upperstr.toupper();
      if ((punt = search_path_xptr(upperstr.get_name(), pOrig))!=NULL &&
           punt->get_op_sys() != GS_OS_UNIX)
         { GS_ERR_COD = eGSAliasAlreadyExisting; return GS_BAD; }

      // cerco l'host maiuscolo e, se esiste, verifico che sia UNIX altrimenti errore
      upperstr = pAlias->get_host();
      upperstr.toupper();
      if ((punt = search_host_xptr(upperstr.get_name(), pOrig))!=NULL &&
           punt->get_op_sys() != GS_OS_UNIX)
         { GS_ERR_COD = eGSAliasAlreadyExisting; return GS_BAD; }
   }
   pOrig->set_path(pAlias->get_path());
   pOrig->set_host(pAlias->get_host());
   pOrig->set_op_sys(pAlias->get_op_sys());

   return GS_GOOD;
}

//-----------------------------------------------------------------------//
int C_ALIAS_LIST::del_to_mem(C_ALIAS *pAlias)
{
   C_ALIAS *p;

   if (!pAlias) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (pAlias->is_valid() == GS_BAD) return GS_BAD;

   if ((p = search_path(pAlias->get_path())) == NULL)
      { GS_ERR_COD = eGSInvalidPath; return GS_BAD; }
   remove((C_NODE*) p);

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
int C_ALIAS_LIST::load()
{
   C_STRING desc_host;
   int      count=0, parola=0, indprinc=0, indtemp=0, cont=0;
   enum OperatingSystemsEnum OSCode = GS_OS_UNKNOWN;
   C_ALIAS  *punt;
   presbuf  p, prb;

   remove_all();
   // Chiamata funzione Ade che restituisce lista Alias
   if ((prb = p = ade_aliasgetlist()) == NULL) return GS_GOOD;

   while (p)
   {
      switch (p->restype)
      {
         case RTLB:    // parentesi aperta
         case RTLE:    // parentesi chiusa
            p = p->rbnext;
            break;
         case RTSTR:   // Alias
            if ((cont%2)!=0)
            {
               if ((punt = new C_ALIAS) == NULL) 
                  { GS_ERR_COD = eGSOutOfMem; return NULL; }
               punt->set_path(p->resval.rstring);
               punt->set_host(desc_host.get_name());
               if (add_to_mem(punt) == GS_BAD)
                  delete punt;
            }
            else desc_host = p->resval.rstring;
            p = p->rbnext;
            cont++;
            break;
         default:
            GS_ERR_COD = eGSInvRBType;
         return GS_BAD;
      }
   }
   if (prb) acutRelRb(prb);  

   // Lettura sezione GS_NET_LABEL = "GEOsim Net Nodes" nel file di
   // inizializzazione GEOSIM.INI
   FILE     *file;
   C_STRING pathfile, Buffer;
   bool     Unicode;

   pathfile = GEOsimAppl::CURRUSRDIR; // Directory locale dell'utente corrente;
   pathfile += _T('\\');
   pathfile += GS_INI_FILE;

   if ((file = gsc_open_profile(pathfile, READONLY, MORETESTS, &Unicode)) == NULL)
   {
      punt = (C_ALIAS *) get_head();
      while (punt)
      {  //"\nL'alias <%s> avente path <%s>, pur non avendo riferimento in GEOsim, è stato considerato per WINDOWS."
         acutPrintf(gsc_msg(439), punt->get_host(), punt->get_path());
         punt->set_op_sys(GS_OS_WINDOWS);
         punt = (C_ALIAS *) get_next();
      }
      return GS_GOOD;
   }

   punt = (C_ALIAS *) get_head();
   while (punt)
   {
      // se esiste nella sezione
      if (gsc_get_profile(file, GS_NET_LABEL, punt->get_host(), Buffer, Unicode) == GS_GOOD &&
          Buffer.len() > 0)
      {
         // Setto il sistema operativo
         if ((OSCode = gsc_OSName2OSCode(Buffer.get_name())) == GS_OS_UNKNOWN)
         {  //"\nL'alias <%s> avente path <%s>, pur non avendo riferimento in GEOsim, è stato considerato per WINDOWS."
            acutPrintf(gsc_msg(439), punt->get_host(), punt->get_path());
            punt->set_op_sys(GS_OS_WINDOWS);
         }
         else
            punt->set_op_sys(OSCode);
      }
      else // c'è un disallineamento
      {  //"\nL'alias <%s> avente path <%s>, pur non avendo riferimento in GEOsim, è stato considerato per WINDOWS."
         acutPrintf(gsc_msg(439), punt->get_host(), punt->get_path());
         punt->set_op_sys(GS_OS_WINDOWS);
      }

      punt = (C_ALIAS *) get_next();
   }

   // Chiudo il file di inizializzazione GEOSIM.INI
   return gsc_close_profile(file);
}


/*****************************************************************************/
/*.doc int C_ALIAS_LIST::loadHostsFromSrv()                       <internal> */
/*+
  Questa funzione carica la lista degli host registrati sul server
  di GEOsim dal file GEOSIM.INI nella cartella SUPPORT.
  La lista avrà solo gli host e i sistemi operativi settati
  opportunamente, le path saranno nulle.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int C_ALIAS_LIST::loadHostsFromSrv()
{
   // Lettura sezione GS_NET_LABEL = "GEOsim Net Nodes" nel file di
   // inizializzazione GEOSIM.INI sul server
   C_STRING    PathFile;
   FILE        *file;
   C_2STR_LIST EntryList;
   bool        Unicode;

   remove_all(); // svuoto la lista

   PathFile = GEOsimAppl::GEODIR; // Directory server di GEOsim
   PathFile += _T('\\');
   PathFile += GEOSUPPORTDIR;
   PathFile += _T('\\');
   PathFile += GS_INI_FILE;

   if ((file = gsc_open_profile(PathFile, READONLY, MORETESTS, &Unicode)) == NULL)
      return GS_GOOD;
   if (gsc_get_profile(file, GS_NET_LABEL, EntryList, Unicode) == GS_GOOD)
   {
      C_2STR  *pEntry = (C_2STR *) EntryList.get_head();
      C_ALIAS *pAlias;
      enum OperatingSystemsEnum OSCode;

      while (pEntry)
      {
         if ((pAlias = new C_ALIAS()) == NULL)
            { gsc_close_profile(file); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         pAlias->set_host(pEntry->get_name());
         // Setto il sistema operativo
         if ((OSCode = gsc_OSName2OSCode(pEntry->get_name2())) == GS_BAD)
            pAlias->set_op_sys(GS_OS_WINDOWS);
         else
            pAlias->set_op_sys(OSCode);

         add_tail(pAlias);

         pEntry = (C_2STR *) pEntry->get_next();
      }
   }

   // Chiudo il file di inizializzazione GEOSIM.INI
   return gsc_close_profile(file);
}


/*****************************************************************************/
/*.doc int C_ALIAS_LIST::saveHostsToSrv()                        <internal> */
/*+
  Questa funzione salva la lista degli host locali facendo una unione con quella
  degli host già registrati sul server di GEOsim.
  Il salvataggio viene effettuato nel file GEOSIM.INI nella cartella SUPPORT.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int C_ALIAS_LIST::saveHostsToSrv()
{
   // Lettura sezione GS_NET_LABEL = "GEOsim Net Nodes" nel file di
   // inizializzazione GEOSIM.INI sul server
   C_STRING     PathFile;
   FILE         *file;
   C_ALIAS_LIST ResultList;
   C_ALIAS      *pAlias, *p;
   bool         Unicode = false;

   ResultList.loadHostsFromSrv(); // carico la lista dal server

   // Unisco le due liste
   pAlias = (C_ALIAS *) get_head();
   while (pAlias)
   {
      if (!(p = ResultList.search_host(pAlias->get_host())))
      {
         if ((p = new C_ALIAS()) == NULL) // se non esiste lo aggiungo
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         ResultList.add_tail(p);
      }    
      p->set_host(pAlias->get_host());     // Setto il nome dell'host     
      p->set_op_sys(pAlias->get_op_sys()); // Setto il sistema operativo

      pAlias = (C_ALIAS *) get_next();
   }

   C_2STR_LIST EntryList;
   C_2STR      *pEntry;

   // Creo una lista C_2STR_LIST
   pAlias = (C_ALIAS *) ResultList.get_head();
   while (pAlias)
   {
      if (!(pEntry = new C_2STR(pAlias->get_host(), gsc_OSCode2OSName(pAlias->get_op_sys()))))
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      EntryList.add_tail(pEntry);

      pAlias = (C_ALIAS *) pAlias->get_next();
   }

   PathFile = GEOsimAppl::GEODIR; // Directory server di GEOsim
   PathFile += _T('\\');
   PathFile += GEOSUPPORTDIR;
   PathFile += _T('\\');
   PathFile += GS_INI_FILE;

   if ((file = gsc_open_profile(PathFile, UPDATEABLE, MORETESTS, &Unicode)) == NULL)
      return GS_GOOD;
   if (gsc_set_profile(file, GS_NET_LABEL, EntryList, Unicode) == GS_BAD)
      { gsc_close_profile(file); return GS_BAD; }

   // Chiudo il file di inizializzazione GEOSIM.INI
   return gsc_close_profile(file);
}


/**********************************************************************/
/*  C_ALIAS_LIST FINE                                                 */
/**********************************************************************/


/*************************************************************************/
/*.doc  int gsc_nethost2drive(char *path) */ 
/*+   
   Traduce un path sostituendo il nome della connessione in rete 
   (<nome macchina+directory condivise>) all'inizio del path con il drive 
   corrispondente (es. <MACCHINA1>\DIR -> C:\DIR oppure $GEOWORK$\DIR e 
   $GEOSIM$\DIR che si riferiscono rispettivamente al direttorio di lavoro
   locale e al direttorio di installazione della parte "server" di GEOsim).
   Paramertri:

   C_STRING &path;   Path da tradurre
   int PrintError;   Se il flag = GS_GOOD in caso di errore viene
                     stampato il messaggio relativo altrimenti non
                     viene stampato alcun messaggio (default = GS_GOOD)

   oppure:

   char *path;       Path da tradurre
   int maxlen;       Lunghezza max. stringa
   int PrintError;   Se il flag = GS_GOOD in caso di errore viene
                     stampato il messaggio relativo altrimenti non
                     viene stampato alcun messaggio (default = GS_GOOD)

   Ritorna GS_BAD in caso di errore GS_GOOD altrimenti.
-*/
/*************************************************************************/
int gsc_nethost2drive(C_STRING &path, int PrintError)
{
   C_STRING Prefix;
   C_ALIAS  *punt = NULL;
   int      i;

   if (path.len() == 0) return GS_GOOD;
   path.alltrim();

   if (path.get_chr(0) == _T('<'))
   {
      i = 0;
      while (path.get_chr(++i) != _T('>'))
      {
         if (path.get_chr(i) == _T('\0'))
            { GS_ERR_COD = eGSInvalidPath; return GS_BAD; }
         Prefix += path.get_chr(i);
      }
      if ((punt = GEOsimAppl::GS_NET_LIST.search_host(Prefix.get_name())) == NULL)
      {
         if (PrintError == GS_GOOD)
            acutPrintf(gsc_msg(524), Prefix.get_name(), path.get_name()); // Trovato Host sconosciuto: <%s> nel riferimento: <%s>
         GS_ERR_COD = eGSInvHostName; 
         return GS_BAD; 
      }
   }
   else
   if (path.get_chr(0) == _T('$')) // macro di $GEOWORK$ o $GEOSIM$
   {
      i = 0;
      while (path.get_chr(++i) != _T('$'))
      {
         if (path.get_chr(i) == _T('\0'))
            { GS_ERR_COD = eGSInvalidPath; return GS_BAD; }
         Prefix += path.get_chr(i);
      }

      if (Prefix.comp(ENV_WORK_DIR, FALSE) == 0) // no case sensitive ("GEOWORK")
         Prefix = GEOsimAppl::WORKDIR;
      else 
      if (Prefix.comp(ENV_GEO_DIR, FALSE) == 0) // no case sensitive ("GEOSIM")
         Prefix = GEOsimAppl::GEODIR;
      else
         { GS_ERR_COD = eGSInvHostName; return GS_BAD; }
   }
   else // o si tratta di un alias stile ADE (es. "COMPUTER1:\DIR\A.DWG")
   {    // oppure di un direttorio senza alias (es. "C:\DIR\A.DWG")
        // oppure di un direttorio senza drive (es. "\DIR\A.DWG")
      C_STRING TempPath;

      i = 0;
      while (path.get_chr(i) != _T('\0') && path.get_chr(i) != _T(':'))
      {
         Prefix += path.get_chr(i);
         i++;
      }

      // forse è espresso in forma esplicita (es "C:\DIR\A.DWG" o "\DIR\A.DWG")
      if (path.get_chr(i) == _T('\0') || // se non esiste drive
          Prefix.len() < 2)              // se il drive è indicato da una sola lettera
      {
         TCHAR Drive[_MAX_DRIVE], Dir[_MAX_DIR], File[_MAX_FNAME], Ext[_MAX_EXT];

         _wsplitpath(path.get_name(), Drive, Dir, File, Ext);

         path = Drive;
         path += Dir;
         path += File;
         path += Ext;

         return GS_GOOD;
      }

      // Ultima chance: forse è un alias stile ADE (es "COMPUTER1:\DIR\A.DWG")
      if ((punt = GEOsimAppl::GS_NET_LIST.search_host(Prefix.get_name())) == NULL)
      {  
         // allora sparati
         if (PrintError == GS_GOOD)
            acutPrintf(gsc_msg(524), Prefix.get_name(), path.get_name()); // Trovato Host sconosciuto: <%s> nel riferimento: <%s>
         GS_ERR_COD = eGSInvalidPath; 
         return GS_BAD; 
      }      
   }

   if (punt) // se si faceva riferimento ad un ALIAS
      switch (punt->get_op_sys())
      {
         case GS_OS_DOS:
         case GS_OS_WINDOWS:
         case GS_OS_WINDOWS_NT_2000:
         case GS_OS_UNIX:
         case GS_OS_WINDOWS_95_98:
         case GS_OS_WINDOWS_XP:
         case GS_OS_WINDOWS_VISTA:
            Prefix = punt->get_path(); break;
         default:
            GS_ERR_COD = eGSInvOpSys; return GS_BAD; 
      }

   Prefix += (path.get_name() + i + 1);
   // Controllo Correttezza Path
   if (gsc_fullpath(path, Prefix) == NULL)
      { GS_ERR_COD = eGSInvalidPath; return GS_BAD;  }

   return GS_GOOD;
}
int gsc_nethost2drive(char *path, int maxlen, int PrintError)
{
   C_STRING TempPath(path);
   char     *dummy;

   if (gsc_nethost2drive(TempPath, PrintError) == GS_BAD) return GS_BAD;
   dummy = gsc_UnicodeToChar(TempPath.get_name());
   gsc_strcpy(path, dummy, maxlen);
   if (dummy) free(dummy);

   return GS_GOOD;
}
int gsc_nethost2drive(TCHAR *path, int maxlen, int PrintError)
{
   C_STRING TempPath(path);

   if (gsc_nethost2drive(TempPath, PrintError) == GS_BAD) return GS_BAD;
   gsc_strcpy(path, TempPath.get_name(), maxlen);

   return GS_GOOD;
}


/*************************************************************************/
/*.doc  int gs_drive2nethost(char *path, int len_str) */ 
/*+   
   Traduce un path in path completo e sostituisce il nome del drive  
   all'inizio con nome della connessine in rete corrispondente
   (<nome macchina+directory condivise>).   
   Ritorna nil in caso di errore altrimenti stringa.
-*/
/*************************************************************************/
int gs_drive2nethost(void)
{
   C_STRING path;
   presbuf  arg = acedGetArgs();

   acedRetNil();

   if (!arg || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   path = arg->resval.rstring;

   if (gsc_drive2nethost(path) == GS_BAD) return RTERROR;
   
   acedRetStr(path.get_name());

   return RTNORM;
}


/*************************************************************************/
/*.doc  int gsc_drive2nethost(char *path, int len_str) */ 
/*+   
   Traduce un path in path completo e sostituisce il nome del drive  
   all'inizio con nome della connessine in rete corrispondente
   (<nome macchina+directory condivise>).   
   Ritorna GS_BAD in caso di errore GS_BAD altrimenti.
-*/
/*************************************************************************/
int gsc_drive2nethost(C_STRING &path)
{
   C_STRING tmp_path;
   TCHAR    *str, *delimiter;
   C_ALIAS  *punt;

   if (path.len() == 0) return GS_GOOD;
          
   tmp_path = path;
   tmp_path.alltrim();

   if (iswalpha(tmp_path.get_chr(0)) != 0 && tmp_path.get_chr(1) == _T(':'))
      tmp_path.set_chr((TCHAR) towupper(tmp_path.get_chr(0)), 0); // può essere DOS, WINDOWS o WINDOWS_NT
   else
      if (tmp_path.get_chr(0) == _T('<')) return GS_GOOD; // Alias già specificato
      else
         return GS_GOOD;  // non esiste un percorso quindi non posso tradurre

   punt = (C_ALIAS *) GEOsimAppl::GS_NET_LIST.get_head();
   while (punt)
   {
      if ((str = tmp_path.at(punt->get_path())) != NULL && str == tmp_path.get_name())
	   {
	      delimiter = str + wcslen(punt->get_path());
         break; // trovato il path
	   }
      punt = (C_ALIAS *) GEOsimAppl::GS_NET_LIST.get_next();
   }
   if (!punt) 
   { 
		// \nIl drive <%c%c> del path <%s> non ha un alias di riferimento.
      acutPrintf(gsc_msg(525), tmp_path.get_chr(0), tmp_path.get_chr(1), tmp_path.get_name());
      GS_ERR_COD = eGSInvalidHost; 
      return GS_BAD; 
   }

   path = _T("<");
   path += punt->get_host();
   path += _T(">");
   path += delimiter;

   return GS_GOOD;
}
int gsc_drive2nethost(char *path, int len_str)
{
   C_STRING TempPath(path);
   char     *dummy;

   if (gsc_drive2nethost(TempPath) == GS_BAD) return GS_BAD;
   dummy = gsc_UnicodeToChar(TempPath.get_name());
   gsc_strcpy(path, dummy, len_str);
   if (dummy) free(dummy);

   return GS_GOOD;
}
int gsc_drive2nethost(TCHAR *path, int len_str)
{
   C_STRING TempPath(path);

   if (gsc_drive2nethost(TempPath) == GS_BAD) return GS_BAD;
   gsc_strcpy(path, TempPath.get_name(), len_str);

   return GS_GOOD;
}


/*************************************************************************/
/*.doc  int gsc_ADEdrv2nethost                                           */ 
/*+   
   Converte il path in un formato compatibile alle ADE.
   Parametri:
   const char *path_input;       path con logico (es. "c:\dir")
   C_STRING *path_output;        path con alias (es. "COMPUTER1:\dir")

   Ritorna GS_BAD in caso di errore GS_BAD altrimenti.
-*/
/*************************************************************************/
int gsc_ADEdrv2nethost(const char *path_input, C_STRING &path_output)
{
   TCHAR *UnicodeString = gsc_CharToUnicode(path_input);  
   int   res = gsc_ADEdrv2nethost(UnicodeString, path_output);
   if (UnicodeString) free(UnicodeString);

   return res;
}
int gsc_ADEdrv2nethost(const TCHAR *path_input, C_STRING &path_output)
{
   C_STRING tmp_path;
   TCHAR    *str, *delimiter;
   C_ALIAS  *punt;

   if (!path_input) return GS_GOOD;
          
   tmp_path = path_input;
   tmp_path.alltrim();

   if (gsc_nethost2drive(tmp_path) == GS_BAD) return GS_BAD;

   if (iswalpha(path_input[0]) != 0 && path_input[1] == _T(':'))   
	   tmp_path.set_chr((TCHAR) towupper(tmp_path.get_chr(0)), 0); // può essere DOS, WINDOWS o WINDOWS_NT o WINDOWS_95
   else
   { 
      C_STRING dummy(path_input);
      if (gsc_fullpath(tmp_path, dummy) == NULL) return GS_BAD; // Path senza drive
   }

   punt = (C_ALIAS *) GEOsimAppl::GS_NET_LIST.get_head();
   while (punt != NULL)
   {
      if ((str = tmp_path.at(punt->get_path())) != NULL && str == tmp_path.get_name())
	   {
	      delimiter = str + wcslen(punt->get_path());
         break; // trovato il path
	   }
      punt = (C_ALIAS *) GEOsimAppl::GS_NET_LIST.get_next();
   }
   if (punt == NULL) 
   { 
      acutPrintf(gsc_msg(525), path_input[0], path_input[1], path_input); // \nIl drive <%c%c> del path <%s> non ha un alias di riferimento.
      GS_ERR_COD = eGSInvalidHost; 
      return GS_BAD; 
   }

   path_output = punt->get_host();
   path_output += _T(':');
   path_output += delimiter;
   // converto nel percorso eventuali "/" in "\"
   path_output.strtran(_T("/"), _T("\\"));

   return GS_GOOD;
}   


/*************************************************************************/
/*.doc  int gsc_ConvAlias                                                */ 
/*+   
   Converte un alias di rete con un nuovo alias nei progetti e classi 
   GEOsim indicati dai parametri.
   Per le classi e i progetti caricati in memoria vengono cambiati gli 
   alias anche in memoria.
   N.B.: Non devono esistere sessioni di lavoro. Assicurarsi che NON ci siano
   altri utenti oltre se stessi che stiano usando GEOsim.
   Parametri:
   const char *NewAlias;   Nuovo alias
   const char *OldAlias;   Vecchio alias da sostituire
   int prj;                codice progetto; se = -1  ->  tutti i progetti
                           (default = -1)
   int cls;                codice classe; se = -1  -> nessuna classe
                           (default = -1)
   int sub;                codice sottoclasse; se = -1  -> nessuna sottoclasse
                           (default = -1)
   int sec;                codice secondaria; se = -1  -> nessuna secondaria
                           (default = -1)
   const char *AttribName; Nome attributo contenente delle path (default = NULL)

   Ritorna GS_BAD in caso di errore GS_BAD altrimenti.
-*/
/*************************************************************************/
int gs_conv_alias(void)
{
   presbuf  arg = acedGetArgs();
   TCHAR    *NewAlias, *OldAlias;
   int      prj = -1, cls = -1, sub = -1, sec = -1;
   C_STRING AttribName;

   acedRetNil();

   if (!arg || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   NewAlias = arg->resval.rstring;

   if (!(arg = arg->rbnext) || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   OldAlias = arg->resval.rstring;

   if ((arg = arg->rbnext)) // codice progetto
   {
      if (arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
      prj = arg->resval.rint;

      if ((arg = arg->rbnext)) // codice classe
      {
         if (arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
         cls = arg->resval.rint;

         if ((arg = arg->rbnext)) // codice sottoclasse
         {
            if (arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
            sub = arg->resval.rint;

            if ((arg = arg->rbnext)) // codice secondaria
            {
               if (arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
               sec = arg->resval.rint;

               if ((arg = arg->rbnext)) // Nome attributo
               {
                  if (arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
                     AttribName = arg->resval.rstring;
               }
            }
         }
      }
   }

   if (gsc_ConvAlias(NewAlias, OldAlias, prj, cls, sub, sec, AttribName.get_name()) == GS_BAD)
      return RTERROR;

   acedRetT();

   return RTNORM;
}
int gsc_ConvAlias(const char *NewAlias, const char *OldAlias, int prj, int cls,
                  int sub, int sec, const char *AttribName)
{
   TCHAR *UnicodeStringNewAlias = gsc_CharToUnicode(NewAlias);  
   TCHAR *UnicodeStringOldAlias = gsc_CharToUnicode(OldAlias);  
   TCHAR *UnicodeStringAttribName = gsc_CharToUnicode(AttribName);
   int   res = gsc_ConvAlias(UnicodeStringNewAlias, UnicodeStringOldAlias,
                             prj, cls, sub, sec, UnicodeStringAttribName);
   if (UnicodeStringNewAlias) free(UnicodeStringNewAlias);
   if (UnicodeStringOldAlias) free(UnicodeStringOldAlias);
   if (UnicodeStringAttribName) free(UnicodeStringAttribName);

   return res;
}
int gsc_ConvAlias(const TCHAR *NewAlias, const TCHAR *OldAlias, int prj, int cls,
                  int sub, int sec, const TCHAR *AttribName)
{
   TCHAR          *str;
   C_STRING       statement, TableRef, CpyNewAlias, CpyOldAlias, dummy;
   C_STRING       ConnStrUDLFile;
   C_DBCONNECTION *pConn;
   _RecordsetPtr  pRs;
   C_RB_LIST      ColValues;
   int            result, toChange, Id;
   C_PROJECT      *pPrj;
   presbuf        p;

   if (!NewAlias || !OldAlias) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   // solo per superutenti
   if (gsc_check_op(opModPrj) == GS_BAD) return GS_BAD;
 
   // Non possono esserci sessioni di lavoro attive
   if (GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSSessionsFound; return GS_BAD; }

   CpyNewAlias = _T("<");
   CpyNewAlias += NewAlias;
   CpyNewAlias += _T(">");
   CpyNewAlias.toupper();

   CpyOldAlias = _T("<");
   CpyOldAlias += OldAlias;
   CpyOldAlias += _T(">");
   CpyOldAlias.toupper();

   /////////////////////////////////////////////////////////////////////////
   // GS_PRJ
   /////////////////////////////////////////////////////////////////////////
   if (gsc_getPrjTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;
   statement = _T("SELECT * FROM ");
   statement += TableRef;

   if (prj > 0)
   {
      statement += _T(" WHERE GS_ID=");
      statement += prj;
   }

   // leggo le righe della tabella
   if (pConn->OpenRecSet(statement, pRs, adOpenDynamic, adLockPessimistic) == GS_BAD)
      return GS_BAD;

   acutPrintf(_T("%s %s..."), gsc_msg(594), TableRef.get_name());  // "\nConversione"

   result = GS_GOOD;
   while (gsc_isEOF(pRs) == GS_BAD)
   {  // leggo riga da tabella
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { result = GS_BAD; break; }
      toChange = FALSE;

      p = ColValues.CdrAssoc(_T("DIR"));
      if (p->restype == RTSTR && p->resval.rstring)
      {
         str = gsc_strtran(p->resval.rstring, CpyOldAlias.get_name(),
                           CpyNewAlias.get_name(), FALSE);
         if (gsc_strcmp(str, p->resval.rstring, FALSE) != 0)
         {  // trovato alias da cambiare
            free(p->resval.rstring);
            p->resval.rstring = str;
            toChange = TRUE;
         }
         else if (str) free(str);
      }

      p = ColValues.CdrAssoc(_T("KEYDWG"));
      if (p->restype == RTSTR && p->resval.rstring)
      {
         str = gsc_strtran(p->resval.rstring, CpyOldAlias.get_name(),
                           CpyNewAlias.get_name(), FALSE);
         if (gsc_strcmp(str, p->resval.rstring, FALSE) != 0)
         {  // trovato alias da cambiare
            free(p->resval.rstring);
            p->resval.rstring = str;
            toChange = TRUE;
         }
         else if (str) free(str);
      }

      if (gsc_rb2Int(ColValues.CdrAssoc(_T("GS_ID")), &Id) == GS_BAD) 
         { result = GS_BAD; break; }

      pPrj = (C_PROJECT*) GEOsimAppl::PROJECTS.search_key(Id);

      if (toChange)
      {
         // modifico la riga del record-set
         if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD) { result = GS_BAD; break; }
      }

      gsc_Skip(pRs);
   }
   if (result == GS_BAD) { gsc_DBCloseRs(pRs); return GS_BAD; }

   if (gsc_DBCloseRs(pRs) == GS_BAD) return GS_BAD;
   // Ricarico la lista dei probetti
	if (GEOsimAppl::PROJECTS.restore_projects() == GS_BAD) return GS_BAD;

   if (prj > 0) pPrj = (C_PROJECT*) GEOsimAppl::PROJECTS.search_key(prj);
   else pPrj = (C_PROJECT*) GEOsimAppl::PROJECTS.get_head();
   
   result = GS_GOOD;
   while (pPrj)
   {
      acutPrintf(_T("%s %s..."), gsc_msg(594), pPrj->get_name());  // "\nConversione"

      /////////////////////////////////////////////////////////////////////////
      // GS_ATTR
      /////////////////////////////////////////////////////////////////////////
      if (pPrj->getAttribsTabInfo(&pConn, &TableRef) == GS_BAD)
         { result = GS_BAD; break; }
      statement = _T("SELECT * FROM ");
      statement += TableRef;

      // leggo le righe della tabella
      if (pConn->OpenRecSet(statement, pRs, adOpenDynamic, adLockPessimistic) == GS_BAD)
         { result = GS_BAD; break; }

      acutPrintf(_T("%s %s..."), gsc_msg(594), TableRef.get_name());  // "\nConversione"

      ColValues.remove_all();
      while (gsc_isEOF(pRs) == GS_BAD)
      {  // leggo riga da tabella
         if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { result = GS_BAD; break; }
         toChange = FALSE;

         p = ColValues.CdrAssoc(_T("FILE_VALID"));
         if (p->restype == RTSTR && p->resval.rstring)
         {
            str = gsc_strtran(p->resval.rstring, CpyOldAlias.get_name(),
                              CpyNewAlias.get_name(), FALSE);
            if (gsc_strcmp(str, p->resval.rstring, FALSE) != 0)
            {  // trovato alias da cambiare
               free(p->resval.rstring);
               p->resval.rstring = str;
               toChange = TRUE;
            }
            else if (str) free(str);
         }

         p = ColValues.CdrAssoc(_T("FILE_CALC"));
         if (p->restype == RTSTR && p->resval.rstring)
         {
            str = gsc_strtran(p->resval.rstring, CpyOldAlias.get_name(),
                              CpyNewAlias.get_name(), FALSE);
            if (gsc_strcmp(str, p->resval.rstring, FALSE) != 0)
            {  // trovato alias da cambiare
               free(p->resval.rstring);
               p->resval.rstring = str;
               toChange = TRUE;
            }
            else if (str) free(str);
         }
         
         if (toChange)
            // modifico la riga del record-set
            if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD) { result = GS_BAD; break; }

         gsc_Skip(pRs);
      }
      if (result == GS_BAD) { gsc_DBCloseRs(pRs); result = GS_BAD; break; }

      if (gsc_DBCloseRs(pRs) == GS_BAD) { result = GS_BAD; break; }

      /////////////////////////////////////////////////////////////////////////
      // GS_ATTR_GRAPH
      /////////////////////////////////////////////////////////////////////////
      if (pPrj->getGraphAttribsTabInfo(&pConn, &TableRef) == GS_BAD)
         { result = GS_BAD; break; }
      statement = _T("SELECT * FROM ");
      statement += TableRef;

      // leggo le righe della tabella
      if (pConn->OpenRecSet(statement, pRs, adOpenDynamic, adLockPessimistic) == GS_BAD)
         { result = GS_BAD; break; }

      acutPrintf(_T("%s %s..."), gsc_msg(594), TableRef.get_name());  // "\nConversione"

      ColValues.remove_all();
      while (gsc_isEOF(pRs) == GS_BAD)
      {  // leggo riga da tabella
         if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { result = GS_BAD; break; }
         toChange = FALSE;

         p = ColValues.CdrAssoc(_T("UDL_FILE"));
         if (p->restype == RTSTR && p->resval.rstring)
         {
            str = gsc_strtran(p->resval.rstring, CpyOldAlias.get_name(),
                              CpyNewAlias.get_name(), FALSE);
            if (gsc_strcmp(str, p->resval.rstring, FALSE) != 0)
            {  // trovato alias da cambiare
               free(p->resval.rstring);
               p->resval.rstring = str;
               toChange = TRUE;
            }
            else if (str) free(str);
         }

         p = ColValues.CdrAssoc(_T("UDL_PROP"));
         if (p->restype == RTSTR && p->resval.rstring)
         {
            str = gsc_strtran(p->resval.rstring, CpyOldAlias.get_name(),
                              CpyNewAlias.get_name(), FALSE);
            if (gsc_strcmp(str, p->resval.rstring, FALSE) != 0)
            {  // trovato alias da cambiare
               free(p->resval.rstring);
               p->resval.rstring = str;
               toChange = TRUE;
            }
            else if (str) free(str);
         }
         
         p = ColValues.CdrAssoc(_T("TABLE_REF"));
         if (p->restype == RTSTR && p->resval.rstring)
         {
            str = gsc_strtran(p->resval.rstring, CpyOldAlias.get_name(),
                              CpyNewAlias.get_name(), FALSE);
            if (gsc_strcmp(str, p->resval.rstring, FALSE) != 0)
            {  // trovato alias da cambiare
               free(p->resval.rstring);
               p->resval.rstring = str;
               toChange = TRUE;
            }
            else if (str) free(str);
         }

         if (toChange)
            // modifico la riga del record-set
            if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD) { result = GS_BAD; break; }

         gsc_Skip(pRs);
      }
      if (result == GS_BAD) { gsc_DBCloseRs(pRs); result = GS_BAD; break; }

      if (gsc_DBCloseRs(pRs) == GS_BAD) { result = GS_BAD; break; }

      /////////////////////////////////////////////////////////////////////////
      // GS_CLASS
      /////////////////////////////////////////////////////////////////////////
      if (pPrj->getClassesTabInfo(&pConn, &TableRef) == GS_BAD)
         { result = GS_BAD; break; }
      statement = _T("SELECT * FROM ");
      statement += TableRef;

      // leggo le righe della tabella
      if (pConn->OpenRecSet(statement, pRs, adOpenDynamic, adLockPessimistic) == GS_BAD)
         { result = GS_BAD; break; }

      acutPrintf(_T("%s %s..."), gsc_msg(594), TableRef.get_name());  // "\nConversione"

      ColValues.remove_all();
      while (gsc_isEOF(pRs) == GS_BAD)
      {  // leggo riga da tabella
         if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { result = GS_BAD; break; }
         toChange = FALSE;

         ConnStrUDLFile.clear();
         p = ColValues.CdrAssoc(_T("UDL_FILE"));
         if (p->restype == RTSTR && p->resval.rstring)
         {
            str = gsc_strtran(p->resval.rstring, CpyOldAlias.get_name(),
                              CpyNewAlias.get_name(), FALSE);
            ConnStrUDLFile = str;
            if (gsc_strcmp(str, p->resval.rstring, FALSE) != 0)
            {  // trovato alias da cambiare
               free(p->resval.rstring);
               p->resval.rstring = str;
               toChange = TRUE;
            }
            else if (str) free(str);
         }

         p = ColValues.CdrAssoc(_T("UDL_PROP"));
         if (p->restype == RTSTR && p->resval.rstring)
         {
            C_STRING UDLProps(p->resval.rstring);

            if (gsc_ConvUDLProperties(ConnStrUDLFile, UDLProps, CpyOldAlias, CpyNewAlias) == GS_BAD)
               { result = GS_BAD; break; }

            if (UDLProps.comp(p->resval.rstring, FALSE) != 0)
            {  // trovato alias da cambiare
               free(p->resval.rstring);
               p->resval.rstring = gsc_tostring(UDLProps.get_name());
               toChange = TRUE;
            }
         }

         p = ColValues.CdrAssoc(_T("TABLE_REF"));
         if (p->restype == RTSTR && p->resval.rstring)
         {
            str = gsc_strtran(p->resval.rstring, CpyOldAlias.get_name(),
                              CpyNewAlias.get_name(), FALSE);
            if (gsc_strcmp(str, p->resval.rstring, FALSE) != 0)
            {  // trovato alias da cambiare
               free(p->resval.rstring);
               p->resval.rstring = str;
               toChange = TRUE;
            }
            else if (str) free(str);
         }
         
         if (toChange)
            // modifico la riga del record-set
            if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD) { result = GS_BAD; break; }

         gsc_Skip(pRs);
      }
      if (result == GS_BAD) { gsc_DBCloseRs(pRs); result = GS_BAD; break; }

      if (gsc_DBCloseRs(pRs) == GS_BAD) { result = GS_BAD; break; }

      /////////////////////////////////////////////////////////////////////////
      // GS_CLASS_GRAPH
      /////////////////////////////////////////////////////////////////////////
      if (pPrj->getGraphClassesTabInfo(&pConn, &TableRef) == GS_BAD)
         { result = GS_BAD; break; }
      statement = _T("SELECT * FROM ");
      statement += TableRef;

      // leggo le righe della tabella
      if (pConn->OpenRecSet(statement, pRs, adOpenDynamic, adLockPessimistic) == GS_BAD)
         { result = GS_BAD; break; }

      acutPrintf(_T("%s %s..."), gsc_msg(594), TableRef.get_name());  // "\nConversione"

      ColValues.remove_all();
      while (gsc_isEOF(pRs) == GS_BAD)
      {  // leggo riga da tabella
         if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { result = GS_BAD; break; }
         toChange = FALSE;

         p = ColValues.CdrAssoc(_T("FILE_REFBL"));
         if (p->restype == RTSTR && p->resval.rstring)
         {
            str = gsc_strtran(p->resval.rstring, CpyOldAlias.get_name(),
                              CpyNewAlias.get_name(), FALSE);
            if (gsc_strcmp(str, p->resval.rstring, FALSE) != 0)
            {  // trovato alias da cambiare
               free(p->resval.rstring);
               p->resval.rstring = str;
               toChange = TRUE;
            }
            else if (str) free(str);
         }

         p = ColValues.CdrAssoc(_T("FILE_GPHCA"));
         if (p->restype == RTSTR && p->resval.rstring)
         {
            str = gsc_strtran(p->resval.rstring, CpyOldAlias.get_name(),
                              CpyNewAlias.get_name(), FALSE);
            if (gsc_strcmp(str, p->resval.rstring, FALSE) != 0)
            {  // trovato alias da cambiare
               free(p->resval.rstring);
               p->resval.rstring = str;
               toChange = TRUE;
            }
            else if (str) free(str);
         }
         
         if (toChange)
            // modifico la riga del record-set
            if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD) { result = GS_BAD; break; }

         gsc_Skip(pRs);
      }
      if (result == GS_BAD) { gsc_DBCloseRs(pRs); result = GS_BAD; break; }

      if (gsc_DBCloseRs(pRs) == GS_BAD) { result = GS_BAD; break; }

      /////////////////////////////////////////////////////////////////////////
      // GS_CLASS_GRAPH_INFO
      /////////////////////////////////////////////////////////////////////////
      if (pPrj->getClassGphDataSrcTabInfo(&pConn, &TableRef) == GS_BAD)
         { result = GS_BAD; break; }
      statement = _T("SELECT * FROM ");
      statement += TableRef;

      // leggo le righe della tabella
      if (pConn->OpenRecSet(statement, pRs, adOpenDynamic, adLockPessimistic) == GS_BAD)
         { result = GS_BAD; break; }

      acutPrintf(_T("%s %s..."), gsc_msg(594), TableRef.get_name());  // "\nConversione"

      ColValues.remove_all();
      while (gsc_isEOF(pRs) == GS_BAD)
      {  // leggo riga da tabella
         if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { result = GS_BAD; break; }
         toChange = FALSE;

         ConnStrUDLFile.clear();
         p = ColValues.CdrAssoc(_T("UDL_FILE"));
         if (p->restype == RTSTR && p->resval.rstring)
         {
            str = gsc_strtran(p->resval.rstring, CpyOldAlias.get_name(),
                              CpyNewAlias.get_name(), FALSE);
            ConnStrUDLFile = str;
            if (gsc_strcmp(str, p->resval.rstring, FALSE) != 0)
            {  // trovato alias da cambiare
               free(p->resval.rstring);
               p->resval.rstring = str;
               toChange = TRUE;
            }
            else if (str) free(str);
         }

         p = ColValues.CdrAssoc(_T("UDL_PROP"));
         if (p->restype == RTSTR && p->resval.rstring)
         {
            C_STRING UDLProps(p->resval.rstring);

            if (gsc_ConvUDLProperties(ConnStrUDLFile, UDLProps, CpyOldAlias, CpyNewAlias) == GS_BAD)
               { result = GS_BAD; break; }

            if (UDLProps.comp(p->resval.rstring, FALSE) != 0)
            {  // trovato alias da cambiare
               free(p->resval.rstring);
               p->resval.rstring = gsc_tostring(UDLProps.get_name());
               toChange = TRUE;
            }
         }

         p = ColValues.CdrAssoc(_T("TABLE_REF"));
         if (p->restype == RTSTR && p->resval.rstring)
         {
            str = gsc_strtran(p->resval.rstring, CpyOldAlias.get_name(),
                              CpyNewAlias.get_name(), FALSE);
            if (gsc_strcmp(str, p->resval.rstring, FALSE) != 0)
            {  // trovato alias da cambiare
               free(p->resval.rstring);
               p->resval.rstring = str;
               toChange = TRUE;
            }
            else if (str) free(str);
         }

         p = ColValues.CdrAssoc(_T("LBL_GRP_TABLE_REF"));
         if (p->restype == RTSTR && p->resval.rstring)
         {
            str = gsc_strtran(p->resval.rstring, CpyOldAlias.get_name(),
                              CpyNewAlias.get_name(), FALSE);
            if (gsc_strcmp(str, p->resval.rstring, FALSE) != 0)
            {  // trovato alias da cambiare
               free(p->resval.rstring);
               p->resval.rstring = str;
               toChange = TRUE;
            }
            else if (str) free(str);
         }

         p = ColValues.CdrAssoc(_T("LBL_TABLE_REF"));
         if (p->restype == RTSTR && p->resval.rstring)
         {
            str = gsc_strtran(p->resval.rstring, CpyOldAlias.get_name(),
                              CpyNewAlias.get_name(), FALSE);
            if (gsc_strcmp(str, p->resval.rstring, FALSE) != 0)
            {  // trovato alias da cambiare
               free(p->resval.rstring);
               p->resval.rstring = str;
               toChange = TRUE;
            }
            else if (str) free(str);
         }

         p = ColValues.CdrAssoc(_T("DIR_DWG"));
         if (p->restype == RTSTR && p->resval.rstring)
         {
            str = gsc_strtran(p->resval.rstring, CpyOldAlias.get_name(),
                              CpyNewAlias.get_name(), FALSE);
            if (gsc_strcmp(str, p->resval.rstring, FALSE) != 0)
            {  // trovato alias da cambiare
               free(p->resval.rstring);
               p->resval.rstring = str;
               toChange = TRUE;
            }
            else if (str) free(str);
         }
         
         if (toChange)
            // modifico la riga del record-set
            if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD) { result = GS_BAD; break; }

         gsc_Skip(pRs);
      }
      if (result == GS_BAD) { gsc_DBCloseRs(pRs); result = GS_BAD; break; }

      if (gsc_DBCloseRs(pRs) == GS_BAD) { result = GS_BAD; break; }

      /////////////////////////////////////////////////////////////////////////
      // GS_SEC
      /////////////////////////////////////////////////////////////////////////
      if (pPrj->getSecsTabInfo(&pConn, &TableRef) == GS_BAD)
         { result = GS_BAD; break; }
      statement = _T("SELECT * FROM ");
      statement += TableRef;

      // leggo le righe della tabella
      if (pConn->OpenRecSet(statement, pRs, adOpenDynamic, adLockPessimistic) == GS_BAD)
         { result = GS_BAD; break; }

      acutPrintf(_T("%s %s..."), gsc_msg(594), TableRef.get_name());  // "\nConversione"

      ColValues.remove_all();
      while (gsc_isEOF(pRs) == GS_BAD)
      {  // leggo riga da tabella
         if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { result = GS_BAD; break; }
         toChange = FALSE;

         ConnStrUDLFile.clear();
         p = ColValues.CdrAssoc(_T("UDL_FILE"));
         if (p->restype == RTSTR && p->resval.rstring)
         {
            str = gsc_strtran(p->resval.rstring, CpyOldAlias.get_name(),
                              CpyNewAlias.get_name(), FALSE);
            ConnStrUDLFile = str;
            if (gsc_strcmp(str, p->resval.rstring, FALSE) != 0)
            {  // trovato alias da cambiare
               free(p->resval.rstring);
               p->resval.rstring = str;
               toChange = TRUE;
            }
            else if (str) free(str);
         }

         p = ColValues.CdrAssoc(_T("UDL_PROP"));
         if (p->restype == RTSTR && p->resval.rstring)
         {
            C_STRING UDLProps(p->resval.rstring);

            if (gsc_ConvUDLProperties(ConnStrUDLFile, UDLProps, CpyOldAlias, CpyNewAlias) == GS_BAD)
               { result = GS_BAD; break; }

            if (UDLProps.comp(p->resval.rstring, FALSE) != 0)
            {  // trovato alias da cambiare
               free(p->resval.rstring);
               p->resval.rstring = gsc_tostring(UDLProps.get_name());
               toChange = TRUE;
            }
         }

         p = ColValues.CdrAssoc(_T("TABLE_REF"));
         if (p->restype == RTSTR && p->resval.rstring)
         {
            str = gsc_strtran(p->resval.rstring, CpyOldAlias.get_name(),
                              CpyNewAlias.get_name(), FALSE);
            if (gsc_strcmp(str, p->resval.rstring, FALSE) != 0)
            {  // trovato alias da cambiare
               free(p->resval.rstring);
               p->resval.rstring = str;
               toChange = TRUE;
            }
            else if (str) free(str);
         }
         
         if (toChange)
            // modifico la riga del record-set
            if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD) { result = GS_BAD; break; }

         gsc_Skip(pRs);
      }
      if (result == GS_BAD) { gsc_DBCloseRs(pRs); result = GS_BAD; break; }

      if (gsc_DBCloseRs(pRs) == GS_BAD) { result = GS_BAD; break; }
      
      /////////////////////////////////////////////////////////////////////////
      // WRKSESSIONS_TABLE_NAME
      /////////////////////////////////////////////////////////////////////////
      if (pPrj->getWrkSessionsTabInfo(&pConn, &TableRef) == GS_BAD)
         { result = GS_BAD; break; }
      statement = _T("SELECT * FROM ");
      statement += TableRef;

      // leggo le righe della tabella
      if (pConn->OpenRecSet(statement, pRs, adOpenDynamic, adLockPessimistic) == GS_BAD)
         { result = GS_BAD; break; }

      acutPrintf(_T("%s %s..."), gsc_msg(594), TableRef.get_name());  // "\nConversione"

      ColValues.remove_all();
      while (gsc_isEOF(pRs) == GS_BAD)
      {  // leggo riga da tabella
         if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { result = GS_BAD; break; }
         toChange = FALSE;

         p = ColValues.CdrAssoc(_T("DIR"));
         if (p->restype == RTSTR && p->resval.rstring)
         {
            str = gsc_strtran(p->resval.rstring, CpyOldAlias.get_name(),
                              CpyNewAlias.get_name(), FALSE);
            if (gsc_strcmp(str, p->resval.rstring, FALSE) != 0)
            {  // trovato alias da cambiare
               free(p->resval.rstring);
               p->resval.rstring = str;
               toChange = TRUE;
            }
            else if (str) free(str);
         }

         if (toChange)
            // modifico la riga del record-set
            if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD) { result = GS_BAD; break; }

         gsc_Skip(pRs);
      }
      if (result == GS_BAD) { gsc_DBCloseRs(pRs); result = GS_BAD; break; }

      if (gsc_DBCloseRs(pRs) == GS_BAD) { result = GS_BAD; break; }
      
      // se si doveva analizzare solo un progetto
      if (prj > 0) break;
      
      pPrj = (C_PROJECT*) GEOsimAppl::PROJECTS.get_next();
   }

   result = GS_BAD;
   do
   {
      // se si devono analizzare i dati di una particolare classe
      if (prj > 0 && cls > 0 && AttribName)
      {
         C_CLASS     *pCls;
         C_SECONDARY *pSec;

         pPrj = (C_PROJECT*) GEOsimAppl::PROJECTS.search_key(prj);

         // se si devono analizzare i dati di una particolare sottoclasse
         if ((pCls = pPrj->find_class(cls, (sub > 0) ? sub : 0)) == NULL) break;
         if (pCls->ptr_info() == NULL) { GS_ERR_COD = eGSInvClassType; break; }

         statement = _T("SELECT ");
         dummy = AttribName;
         // se si devono analizzare i dati di una particolare secondaria
         if (sec > 0)
         {
            if ((pSec = (C_SECONDARY *) pCls->find_sec(sec)) == NULL) break;
            // ricavo connessione OLE-DB
            if ((pConn = pSec->ptr_info()->getDBConnection(OLD)) == NULL) break;
            if (gsc_AdjSyntax(dummy, pConn->get_InitQuotedIdentifier(), pConn->get_FinalQuotedIdentifier(),
                              pConn->get_StrCaseFullTableRef()) == GS_BAD)
               break;
            statement += dummy;
            statement += _T(" FROM ");
            statement += pSec->ptr_info()->OldTableRef;
         }
         else
         {
            C_ATTRIB *pAttrib = (C_ATTRIB *) pCls->ptr_attrib_list()->search_name(AttribName, FALSE);

            // ricavo connessione OLE-DB
            if ((pConn = pCls->ptr_info()->getDBConnection(OLD)) == NULL) break;
            if (gsc_AdjSyntax(dummy, pConn->get_InitQuotedIdentifier(), pConn->get_FinalQuotedIdentifier(),
                              pConn->get_StrCaseFullTableRef()) == GS_BAD)
               break;
            if (pAttrib->is_visible() == GS_GOOD)
            {
               int conf = GS_BAD;

               // "Si sta tentando di variare i valori di un attributo visibile, database e grafica saranno disallineati. Continuare ?"
               if (gsc_ddgetconfirm(gsc_msg(846), &conf) == GS_BAD) 
                  { result = GS_BAD; break; }
               if (conf != GS_GOOD) { result = GS_GOOD; break; }
            }
            
            statement += dummy;
            statement += _T(" FROM ");
            statement += pCls->ptr_info()->OldTableRef;
         }            

         if (pConn->OpenRecSet(statement, pRs, adOpenDynamic, adLockPessimistic) == GS_BAD)
            break;
         p = NULL;
         ColValues.remove_all();
         while (gsc_isEOF(pRs) == GS_BAD)
         {  // leggo riga da tabella
            if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { result = GS_BAD; break; }

            if (!p) p = ColValues.CdrAssoc(AttribName);
            
            if (p->restype == RTSTR && p->resval.rstring)
            {
               str = gsc_strtran(p->resval.rstring, CpyOldAlias.get_name(),
                                 CpyNewAlias.get_name(), FALSE);
               if (gsc_strcmp(str, p->resval.rstring, FALSE) != 0)
               {  // trovato alias da cambiare
                  free(p->resval.rstring);
                  p->resval.rstring = str;
                  if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD) break;
               }
               else if (str) free(str);
            }

            gsc_Skip(pRs);
         }
         gsc_DBCloseRs(pRs);
      }

      result = GS_GOOD;
   }
   while (0);

   if (result == GS_GOOD)
      // inizializza le strutture usate da GEOsim che dipendono
      // dall'utente corrente.
      if (GEOsimAppl::initByUser() == GS_BAD) return GS_BAD;

   return result;
}


/*************************************************************************/
/*.doc gsc_ConvUDLProperties                                  <internal> */
/*+
  Funzione che una stringa indicancte le proprietà di
  connessione UDL con il nuovo ALIAS di rete.
  Parametri:
  C_STRING &UDLFile;      File di connessione UDL
  C_STRING &UDLProperties;       Stringa delle proprietà UDL (viene modificata)
  C_STRING &CpyOldAlias;         Vecchio alias
  C_STRING &CpyNewAlias          Nuovo alias

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
int gsc_ConvUDLProperties(C_STRING &UDLFile, C_STRING &UDLProperties,
                          C_STRING &CpyOldAlias, C_STRING &CpyNewAlias)
{
   C_2STR_LIST PropList;
   C_2STR      *pProp;
   C_STRING    FullUDLFile(UDLFile), NewProp;
   C_RB_LIST   DescrUDL;
   presbuf     pDescrUDL, pTypeDescrUDL, pNameDescrUDL;
   TCHAR       *PropName;
   int         i = 0;

   if (gsc_getUDLFullPath(FullUDLFile) == GS_BAD) return GS_GOOD;
   if (gsc_path_exist(FullUDLFile) == GS_BAD) return GS_GOOD;

   // Conversione path UDLProperties da assoluto in dir relativo
   if (gsc_PropListFromConnStr(UDLProperties.get_name(), PropList) == GS_BAD) return GS_BAD;

   // ((<Name>[<Descr.>][Resource Type][CATALOG || SCHEMA])
   //  (<Name>[<Descr.>][Resource Type][CATALOG || SCHEMA])...)
   DescrUDL << gsc_getUDLPropertiesDescrFromFile(FullUDLFile.get_name());

   while ((pDescrUDL = DescrUDL.nth(i++)))
      if ((pTypeDescrUDL = gsc_nth(2, pDescrUDL)) &&
          pTypeDescrUDL->restype == RTSTR)
      {
         if (gsc_DBstr2CatResType(pTypeDescrUDL->resval.rstring) == DirectoryRes ||
             gsc_DBstr2CatResType(pTypeDescrUDL->resval.rstring) == FileRes)
         {
            pNameDescrUDL = gsc_nth(0, pDescrUDL);
            if (pNameDescrUDL->restype == RTSTR)
            {
               // se si tratta di "[Extended Properties]"
               if ((PropName = gsc_strstr(pNameDescrUDL->resval.rstring, _T("[Extended Properties]"), FALSE)))
               {
                  C_2STR_LIST ExtendedPropList;
                  C_2STR      *pExtendedProp;

                  PropName += wcslen(_T("[Extended Properties]"));
             
                  if (!(pProp = (C_2STR *) PropList.search_name(_T("Extended Properties"), FALSE)))
                     { GS_ERR_COD = eGSUDLPropertyNotFound; return GS_BAD; }

                  if (gsc_PropListFromExtendedPropConnStr(pProp->get_name2(), ExtendedPropList) == GS_BAD)
                     { GS_ERR_COD = eGSUDLPropertyNotFound; return GS_BAD; }

                  if (!(pExtendedProp = (C_2STR *) ExtendedPropList.search_name(PropName, FALSE)))
                     { GS_ERR_COD = eGSUDLPropertyNotFound; return GS_BAD; }

                  NewProp.paste(gsc_strtran(pExtendedProp->get_name2(), CpyOldAlias.get_name(),
                                            CpyNewAlias.get_name(), FALSE));

                  pExtendedProp->set_name2(NewProp.get_name()); // setto nuova proprietà Extended
                  NewProp.paste(gsc_ExtendedPropListToConnStr(ExtendedPropList));
               }
               else
               {
                  if ((pProp = (C_2STR *) PropList.search_name(pNameDescrUDL->resval.rstring, FALSE)))
                     NewProp.paste(gsc_strtran(pProp->get_name2(), CpyOldAlias.get_name(),
                                               CpyNewAlias.get_name(), FALSE));
               }

					if (pProp) pProp->set_name2(NewProp.get_name()); // setto nuova proprietà
            }
         }
      }

   // modifico stringa di proprietà UDL
   UDLProperties.paste(gsc_PropListToConnStr(PropList));

   return GS_GOOD;
}