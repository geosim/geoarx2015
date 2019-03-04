/**********************************************************
Name: GS_USER.CPP

Module description: File funzioni per la gestione
                    degli utenti. 
            
Author: Roberto Poltini

(c) Copyright 1995-2015 by IREN ACQUA GAS S.p.A.

**********************************************************/

#include "stdafx.h"         // MFC core and standard components

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")

#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>       /*  per _open() */
#include <sys\stat.h>    /*  per _open() */
#include <share.h>       /*  per apertura files in condivisione */
#include <math.h>        /*  per pow()  */
#include <string.h>      /*  per strcat() strcmp()  */

#include "adslib.h"
#include "adsdlg.h"
#include <adeads.h>

#include "..\gs_def.h" // definizioni globali
#include "gs_error.h"
#include "gs_opcod.h"
#include "gs_utily.h"
#include "gs_ade.h"
#include "gs_resbf.h"
#include "gs_list.h"
#include "gs_ase.h"
#include "gs_netw.h"
#include "gs_class.h"
#include "gs_prjct.h"
#include "gs_init.h"
#include "gs_area.h"
#include "gs_cmd.h"
#include "gs_graph.h"
#include "gs_user.h"

#include "d2hMap.h" // doc to help


    /* Numero byte nell'HEADER di 'GEOpwd' per i campi di GS_LOG_HEAD */
#define LHL1    2   /* log_len   */
#define LHL2    2   /* pwd_len   */
#define LHL3    2   /* mode_len  */
#define LHL4    2   /* flags_len */
#define LHL5    2   /* max_usr   */
#define LHL6    2   /* num_usr   */

#define LOG_HEAD_LEN  LHL1+LHL2+LHL3+LHL4+LHL5+LHL6

//-----------------------------------------------------------------------//
//  GLOBAL VARIABLES                                                     //
//-----------------------------------------------------------------------//


//-----------------------------------------------------------------------//
//  FUNZIONI PRIVATE //
//-----------------------------------------------------------------------//


int print_users(int mode=ALL);
int gsc_setfamily(C_NODE *pPrj, int cod_class, int abilitaz, C_INT_INT_LIST *class_list);
static int gsc_setLastLogin(const TCHAR *login);
static int gsc_setCheckSessionOnLogin(int Value);

int gsc_createMapSuperUsr(const TCHAR *UserName, const TCHAR *Password);
int gsc_delMapUsr(const TCHAR *UserName);
int gsc_updMapUsrPwd(const TCHAR *UserName, const TCHAR *Password);
int permission_to_msg(GSDataPermissionTypeEnum Permission);


/*************************************************************************/

//-----------------------------------------------------------------------//
//  FUNZIONI PER I COMANDI AUTOCAD SENZA INTERFACCIA DCL (DAL PROMPT)    //
//-----------------------------------------------------------------------//

/*************************************************************************/
/*.doc gslogin()                                                         */
/*+
   Gestisce il comando login senza interfaccia DCL.
-*/
/*************************************************************************/
void gslogin(void)
{
   TCHAR    user[MAX_LEN_LOGIN] = GS_EMPTYSTR, passwd[MAX_LEN_PWD] = GS_EMPTYSTR, buff[133];
   size_t   usrlen;
   C_STRING msg;

   GEOsimAppl::CMDLIST.StartCmd();

   msg = GS_LFSTR;
   msg += gsc_msg(128); // "Nome Utente: "

   // leggo ultima login
   if (gsc_getLastLogin(user) == GS_GOOD && wcslen(user) > 0)
   {
      msg += _T("<");
      msg += user;
      msg += _T("> ");
   }
   if (ads_getstring(TRUE, msg.get_name(), buff) != RTNORM)
      return GEOsimAppl::CMDLIST.ErrorCmd();
   usrlen = wcslen(buff);
   // caso di stringa utente troppo lunga
   if (usrlen > MAX_LEN_LOGIN)
      { GS_ERR_COD = eGSInvalidLogin; return GEOsimAppl::CMDLIST.ErrorCmd(); }
   // se non è stata inserita una login
   if (usrlen == 0)
   {  // se non c'era una login di default 
      if (wcslen(user) == 0)
         // "stringa utente deve essere almeno un carattere valido"
         { GS_ERR_COD = eGSInvalidLogin; return GEOsimAppl::CMDLIST.ErrorCmd(); }
   }
   else
      gsc_strcpy(user, buff, MAX_LEN_LOGIN);

   acutPrintf(GS_LFSTR);
   if (gsc_getstringmask(TRUE, gsc_msg(129), buff, _T('*')) != GS_GOOD) // "Password: "
      return GEOsimAppl::CMDLIST.ErrorCmd();
   acutPrintf(GS_LFSTR);
   gsc_strcpy(passwd, buff, MAX_LEN_PWD);
   if (gsc_login(user, passwd) == GS_BAD)
      return GEOsimAppl::CMDLIST.ErrorCmd();

   return GEOsimAppl::CMDLIST.EndCmd();
}



/*************************************************************************/
/*.doc gscreausr()  */
/*+
   Comando che crea utente senza interfaccia DCL.
-*/
/*************************************************************************/
void gscreausr(void)
{
   TCHAR    user[MAX_LEN_LOGIN] = GS_EMPTYSTR, passwd[MAX_LEN_PWD] = GS_EMPTYSTR, confpwd[MAX_LEN_PWD] = GS_EMPTYSTR, buff[133];
   size_t   usrlen = 0;
   int      susr = 1, mval=0;
   C_STRING Prompt;
   	     
   GEOsimAppl::CMDLIST.StartCmd();

   if (gsc_superuser() == GS_BAD)
	   { GS_ERR_COD = eGSInvalidLogin; return GEOsimAppl::CMDLIST.ErrorCmd(); }  // "Check se superuser" 

   if (acedInitGet(2, gsc_msg(143) ) != RTNORM)
      return GEOsimAppl::CMDLIST.ErrorCmd();    //  "Utente Superutente"
   Prompt = gsc_msg(93); // "\nDigitare un'opzione "
   Prompt += gsc_msg(142); // [<Utente>/Superutente]: 
   if (acedGetKword(Prompt.get_name(), buff) != RTNORM)
      return GEOsimAppl::CMDLIST.ErrorCmd();
   if (gsc_strcmp(buff, gsc_msg(155)) == 0 || wcslen(buff) ==0) susr = 0;
   acutPrintf(GS_LFSTR);
   if (ads_getstring(TRUE, gsc_msg(128), buff) != RTNORM) // "Nome Utente: "
      return GEOsimAppl::CMDLIST.ErrorCmd();
   usrlen = wcslen(buff);
   if (usrlen > MAX_LEN_LOGIN || usrlen == 0 || buff[0] == _T('#'))
      { GS_ERR_COD = eGSInvalidLogin; return GEOsimAppl::CMDLIST.ErrorCmd(); } // "stringa utente troppo lunga"
                                                      // "stringa utente deve essere almeno un carattere valido"
   gsc_strcpy(user, buff, MAX_LEN_LOGIN);
   acutPrintf(GS_LFSTR);
   if (gsc_getstringmask(TRUE, gsc_msg(129), buff, _T('*')) != GS_GOOD) // "Password: "
      return GEOsimAppl::CMDLIST.ErrorCmd();
   if (wcslen(buff) > MAX_LEN_PWD)
      {GS_ERR_COD = eGSInvalidPwd; return GEOsimAppl::CMDLIST.ErrorCmd(); } // "stringa password troppo lunga"
   acutPrintf(GS_LFSTR);
   gsc_strcpy(passwd, buff, MAX_LEN_PWD);
   if (gsc_getstringmask(TRUE, gsc_msg(137), buff, _T('*')) != GS_GOOD)
      return GEOsimAppl::CMDLIST.ErrorCmd(); // "Conferma Password: "
   acutPrintf(GS_LFSTR);
   gsc_strcpy(confpwd, buff, MAX_LEN_PWD);
   if (gsc_strcmp(passwd, confpwd) != 0)
	   {GS_ERR_COD = eGSInvalidPwd; return GEOsimAppl::CMDLIST.ErrorCmd(); }           // "Conferma e passwd diverse"
   if (gsc_creausr(user, passwd, susr) == 0) return GEOsimAppl::CMDLIST.ErrorCmd();

   return GEOsimAppl::CMDLIST.EndCmd();
}

 
/*************************************************************************/
/*.doc gsmodpwd()  */																 
/*+
   Comando che modifica Password utente senza interfaccia DCL.
-*/
/*************************************************************************/
void gsmodpwd(void)
{
   TCHAR oldpwd[MAX_LEN_PWD] = GS_EMPTYSTR, newpwd[MAX_LEN_PWD] = GS_EMPTYSTR, confpwd[MAX_LEN_PWD] = GS_EMPTYSTR, buff[133];
   
   GEOsimAppl::CMDLIST.StartCmd();

   if (gsc_getstringmask(TRUE, gsc_msg(144), buff, _T('*')) != GS_GOOD) return GEOsimAppl::CMDLIST.ErrorCmd(); // "Vecchia pwd: "
   if (wcslen(buff) > MAX_LEN_PWD)
      {GS_ERR_COD=eGSInvalidPwd; return GEOsimAppl::CMDLIST.ErrorCmd(); }; // "stringa password troppo lunga"
   acutPrintf(GS_LFSTR);
   gsc_strcpy(oldpwd,buff, MAX_LEN_PWD);
   if (gsc_getstringmask(TRUE, gsc_msg(145), buff, _T('*')) != GS_GOOD) return GEOsimAppl::CMDLIST.ErrorCmd(); // "Nuova pwd "
   if (wcslen(buff) > MAX_LEN_PWD)
      {GS_ERR_COD=eGSInvalidPwd; return GEOsimAppl::CMDLIST.ErrorCmd(); }; // "stringa password troppo lunga"
   acutPrintf(GS_LFSTR);
   gsc_strcpy(newpwd, buff, MAX_LEN_PWD);
   if (gsc_getstringmask(TRUE, gsc_msg(137), buff, _T('*')) != GS_GOOD) return GEOsimAppl::CMDLIST.ErrorCmd(); // "Conferma Password: "
   acutPrintf(GS_LFSTR);
   gsc_strcpy(confpwd, buff, MAX_LEN_PWD);
   if (gsc_strcmp(newpwd, confpwd) != 0)
	  {GS_ERR_COD = eGSInvalidPwd; return GEOsimAppl::CMDLIST.ErrorCmd(); };           // "Conferma e passwd diverse"
   if (gsc_modpwd(oldpwd, newpwd) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();
      
   return GEOsimAppl::CMDLIST.EndCmd();
}

                 
/*************************************************************************/
/*.doc gsdelusr()  */
/*+
   Comando che cancella utente senza interfaccia DCL.
-*/
/*************************************************************************/			   
void gsdelusr(void)
{
   TCHAR         user[MAX_LEN_LOGIN] = GS_EMPTYSTR, passwd[MAX_LEN_PWD] = GS_EMPTYSTR, buff[133] = GS_EMPTYSTR;
   C_INT_INT_STR userinfo;
   size_t        usrlen;
   int           ris;
     	     
   GEOsimAppl::CMDLIST.StartCmd();

   if (gsc_superuser() == GS_BAD)
      { GS_ERR_COD = eGSInvalidLogin; return GEOsimAppl::CMDLIST.ErrorCmd(); }  // "Check se superuser"
   if (ads_getstring(TRUE, gsc_msg(149), buff) != RTNORM) //"\nNome utente <?>: "
      return GEOsimAppl::CMDLIST.ErrorCmd();
   do	
   {   				      
      usrlen = wcslen(buff);
      if ((TCHAR) buff[0] == _T('?') || usrlen == 0)
      {
         if (print_users() == GS_BAD) 
            {GS_ERR_COD=eGSListError; return GEOsimAppl::CMDLIST.ErrorCmd(); }
      }
      else break;
      // "\nNome utente <?>: "
      if (ads_getstring(TRUE, gsc_msg(149), buff) != RTNORM) return GEOsimAppl::CMDLIST.ErrorCmd(); 
   }
   while (TRUE);

   if (usrlen > MAX_LEN_LOGIN || usrlen == 0)
      { GS_ERR_COD = eGSInvalidLogin; return GEOsimAppl::CMDLIST.ErrorCmd(); } // "Stringa utente troppo lunga"
   gsc_strcpy(user, buff, MAX_LEN_LOGIN);
   if (gsc_chkusr(user, &userinfo) == GS_BAD)
      { GS_ERR_COD = eGSInvalidUser; return GEOsimAppl::CMDLIST.ErrorCmd(); }                                                                                      // "stringa utente deve essere almeno un carattere valido"
   acutPrintf(GS_LFSTR);
   if (gsc_getstringmask(TRUE, gsc_msg(129), buff, _T('*')) != GS_GOOD) // "Password: "
      return GEOsimAppl::CMDLIST.ErrorCmd();
   gsc_strcpy(passwd, buff, MAX_LEN_PWD);

   if (gsc_getconfirm(gsc_msg(163), &ris, GS_BAD) == GS_GOOD)   // "\nConfermare."
   {
      if (ris==GS_GOOD)
         if (gsc_delusr(userinfo.get_key(), passwd) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();
   }

   return GEOsimAppl::CMDLIST.EndCmd();
} 


/*********************************************************
/*.doc (new 2) printlistcommand <internal> */
/*+
  Questa funzione stampa la lista dei comandi di GEOsim e
  utente e il loro stato di abilitazione all'utente.
  Parametri:
  C_INT_LIST *commusr;  lista dei codici alle operazioni abilitate

  Restituisce GS_GOOD se tutto va bene.
-*/  
/*********************************************************/
int printlistcommand(C_INT_LIST *commusr)
{
   C_INT_STR_LIST punt;
   C_INT_STR      *p_comm;

   if (gsc_getgscomm(&punt) == GS_BAD) return GS_BAD;
   
   acutPrintf(_T("%s"), gsc_msg(177));        // \nLista comandi:
   p_comm = (C_INT_STR *) punt.get_head();
   while (p_comm)
   { 
      acutPrintf(_T("\n%.*s"), 25, p_comm->get_name());
      if (commusr->search_key(p_comm->get_key()) > 0)
         acutPrintf(_T("%s"), gsc_msg(202));  // "\tAbilitato"
      else 
         acutPrintf(_T("%s"), gsc_msg(203));  // "\tDisabilitato"
      p_comm = (C_INT_STR *) punt.get_next();
   }

   return GS_GOOD;      
}
/*********************************************************/
int printlistclasses(C_SINTH_CLASS_LIST *pclasses,C_INT_INT_LIST *classusr)
{
   C_SINTH_CLASS *p_comm;
   C_INT_INT     *p;
   
   acutPrintf(_T("%s"), gsc_msg(196));        // "\nLista classi"
   p_comm = (C_SINTH_CLASS *)pclasses->get_head();
   while (p_comm != NULL)
   {                                           
      acutPrintf(_T("\n%.*s"), 25, p_comm->get_name());
      if ((p = (C_INT_INT*) classusr->search_key(pclasses->get_cursor()->get_key())) != NULL)
      {
         switch (p->get_type())
         {
            case 1:
               acutPrintf(_T("%s"),gsc_msg(205));
               break;
            case 2:
               acutPrintf(_T("%s"),gsc_msg(206));
               break;
         }
      }
      else
         acutPrintf(_T("%s"), gsc_msg(204));

      p_comm = (C_SINTH_CLASS *) pclasses->get_next();
   }

   return GS_GOOD;      
}


/*********************************************************
/*.doc codabl_to_msg(int codice) <internal> */
/*+
  Questa funzione ritorna lo stato di abilitazione dei
  comandi GEOsim asseganti all'utente.

  Restituisce GS_GOOD se tutto va bene.
-*/  
/*********************************************************/
int codabl_to_msg(int codice)
{
   if (codice > 0)
      acutPrintf(_T("%s"), gsc_msg(183));   // (stato attuale abilitato)
   else 
      acutPrintf(_T("%s"), gsc_msg(184));  // (stato attuale disabilitato)
                                      
   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//  FUNZIONI PER LE CHIAMATE LISP DEL MODULO ARX                         //
//-----------------------------------------------------------------------//


/*************************************************************************/
/*.doc gs_login                                                          */
/*+
   Funzione lisp che riceve login e password di un utente ne controlla l'esattezza 
   nel file 'GEOpwd' in caso affermativo aggiorna tutte i campi della variabile
   globale GEOsimAppl::GS_USER (allocando le stringhe) con le informazioni di 'GEOpwd'
   Parametri:
   (<nome> <parola segreta> [<controllo sessioni>])

   La funzione ritorna T in caso di login corretta, nil altrimenti.
-*/
/*************************************************************************/
int gs_login(void)
{
   TCHAR  *login, *pwd;
   resbuf *arg;
   int    SessionCheck = GS_GOOD;

   acedRetNil();
   arg = acedGetArgs();

   // nome utente
   if (arg == NULL || arg->restype != RTSTR)  // Errore argomenti
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; } 
   login = arg->resval.rstring;

   // parola segreta
   if ((arg = arg->rbnext) == NULL || arg->restype != RTSTR) // Errore argomenti
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   pwd = arg->resval.rstring;

   // controllo connessioni (opzionale)
   if ((arg = arg->rbnext) != NULL && arg->restype == RTNIL) SessionCheck = GS_BAD;

   if (gsc_login(login, pwd, SessionCheck)==GS_BAD) return RTERROR;
   
   acedRetT();

   return RTNORM;
}


//-----------------------------------------------------------------------//
//  GS_SUPERUSER  //
//-----------------------------------------------------------------------//
int gs_superuser(void)
{
   // CHIAMATA FUNZIONE C++ RELATIVA
   presbuf arg = acedGetArgs();
   
   if (!arg)
   { // utente corrente
      if (gsc_superuser() == GS_GOOD) acedRetT();
      else acedRetNil();
   }
   else
   {
      if (arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
      if (gsc_superuser(arg->resval.rint) == GS_GOOD) acedRetT();
      else acedRetNil();
   }

   return RTNORM;
}   


//-----------------------------------------------------------------------//
//  GS_CHKUSR  //
//-----------------------------------------------------------------------//
int gs_chkusr(void)
{
   resbuf        *arg;
   C_RB_LIST     ret;
   int           su = 1;
   C_USER        temp;
   C_INT_INT_STR usr;

   acedRetNil();
   arg = acedGetArgs();

   if (arg == NULL || arg->restype != RTSTR) // Errore argomenti
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; } 

   // Chiamata a funzione C++ relativa.
   if (gsc_chkusr(arg->resval.rstring, &usr) == GS_BAD) return RTERROR;

   if ((ret << acutBuildList(RTSHORT, usr.get_key(),
                             RTSTR, usr.get_name(),
                             RTSHORT, usr.get_type(), 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }   
   ret.LspRetList();

   return RTNORM;
}   


/*********************************************************/
/*.doc gs_creausr                             <external> */
/*+
  Funzione LISP per la creazione di un utente GEOsim.
  Parametri:
  Lista di resbuf (<login><password><flag>)

  <flag> = 0 -> superutente, 1 -> utente normale

  Ritorna il codice del nuovo utente creato o nil in caso di errore
-*/  
/*********************************************************/
int gs_creausr(void)
{
   TCHAR  *log,*pwd;
   resbuf *arg;
   int    su, UserID;
   C_USER temp;

   acedRetNil();
   arg=acedGetArgs();
                          
   // alloca memoria usata solo per 'mode'
   if(temp.alloc_from_geopwd()==GS_BAD) return RTERROR;

   if (arg==NULL || arg->restype!=RTSTR) // Errore argomenti
      { GS_ERR_COD=eGSInvalidArg; return RTERROR; }
   log=arg->resval.rstring;

   if ((arg=arg->rbnext)==NULL || arg->restype!=RTSTR) // Errore argomenti
      { GS_ERR_COD=eGSInvalidArg; return RTERROR; }
   pwd=arg->resval.rstring;

   if ((arg=arg->rbnext)==NULL || arg->restype!=RTSHORT) //Errore argomenti
      { GS_ERR_COD=eGSInvalidArg; return RTERROR; }
   su=arg->resval.rint;

   // CHIAMATA FUNZIONE C++ RELATIVA 
   if ((UserID = gsc_creausr(log, pwd, su)) == 0) return RTERROR;

   acedRetInt(UserID);

   return RTNORM;
}


/*********************************************************/
/*.doc gs_delusr                              <external> */
/*+
  Questa funzione LISP cancella un utente di GEOsim.
  Parametri:
  Lista RESBUF: <user_code><password>[<Destroy>]

  <user_code> = codice utente da cancellare
  <password>  = password utente corrente
  <Destroy>   = Opzionale. Flag di modo, se = T distrugge l'utente senza fare controlli
      
  Restituisce TRUE in caso di successo altrimenti restituisce NIL.
-*/  
/*********************************************************/
int gs_delusr(void)
{
   resbuf *arg = acedGetArgs();
   int    UsrCode;
   bool   DestroyUsr = false;
   TCHAR  *Pwd;

   acedRetNil();

   // Codice utente
   if (gsc_rb2Int(arg, &UsrCode) == GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   // password utente corrente
   if (!(arg = arg->rbnext) || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   Pwd = arg->resval.rstring;
   // Flag di distruzione utente senza controlli (opzionale)
   if ((arg = (arg->rbnext)) != NULL && arg->restype == RTT) DestroyUsr = true;
   // CHIAMATA FUNZIONE C++ RELATIVA
   if (gsc_delusr(UsrCode, Pwd, DestroyUsr) == GS_BAD) return RTERROR;

   acedRetT();

   return RTNORM;
}         


/*********************************************************/
/*.doc gs_getusr                              <external> */
/*+
  Questa funzione LISP ritorna la lista degli utenti di GEOsim.
-*/  
/*********************************************************/
int gs_getusr(void)
{
   C_INT_INT_STR_LIST list;

   acedRetNil();
   // CHIAMATA FUNZIONE C++ RELATIVA
   if (gsc_getusrlist(&list) == GS_BAD) return RTERROR;

   if (list.get_count() !=0 )
   {
      C_RB_LIST ret;

      list.sort_name();
      ret << list.to_rb();
      ret.LspRetList();
   }

   return RTNORM;
}


//-----------------------------------------------------------------------//
//  GS_WHOAMI  //
//-----------------------------------------------------------------------//
int gs_whoami(void)
{
   C_RB_LIST     ret;
   C_INT_INT_STR usr;
   TCHAR         *string, str[] = GS_EMPTYSTR;

   acedRetNil(); 
   if (gsc_whoami(&usr) == GS_BAD)
      return (GS_ERR_COD == eGSInvalidLogin) ? RTNORM : RTERROR;

   if ((string = usr.get_name()) == NULL) string = str;
   if ((ret << acutBuildList(RTSHORT, usr.get_key() ,RTSTR, string, RTSHORT,
                             usr.get_type(), 0)) == NULL)
   	return RTERROR;
   ret.LspRetList();
       
   return RTNORM;
}


/*********************************************************/
/*.doc gs_getusrcomm                          <external> */
/*+
  Questa funzione LISP ritorna la lista degli codici dei comandi
  abilitati ad un utente di GEOsim.
-*/  
/*********************************************************/
int gs_getusrcomm(void)
{
   presbuf    arg;
   C_INT_LIST list;

   acedRetNil();
   arg = acedGetArgs();

   if (arg == NULL || arg->restype != RTSHORT)    // Errore argomenti //
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }  

   // Chiamata funzione C++ relativa //
   if (gsc_getusrcomm(arg->resval.rint, &list) == GS_BAD) return RTERROR;

   if (list.get_count() > 0)
   {
      C_RB_LIST ret(list.to_rb());
      ret.LspRetList();
   }

   return RTNORM;
}         


//-----------------------------------------------------------------------//
//  GS_SETUSRCOMM  //
//-----------------------------------------------------------------------//
int gs_setusrcomm(void)
{
resbuf *arg;
int code;
C_INT_LIST list;

   acedRetNil();
   arg=acedGetArgs();

   if (arg == NULL || arg->restype != RTSHORT)  // Errore argomenti
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; } 
   code = arg->resval.rint;

   if (list.from_rb(arg->rbnext) == GS_BAD) return RTERROR;

   // CHIAMATA FUNZIONE C++ RELATIVA
   if (gsc_setusrcomm(code,&list) == GS_BAD) return RTERROR;
   acedRetT();

   return RTNORM;
}         


//-----------------------------------------------------------------------//
//  GS_GETGSCOMM  //
//-----------------------------------------------------------------------//
int gs_getgscomm(void)
{
   C_INT_STR_LIST list;

   acedRetNil();

   // CHIAMATA FUNZIONE C++ RELATIVA
   if (gsc_getgscomm(&list)==GS_BAD) return RTERROR;

   if (list.get_count() == 0) acedRetNil();
   else 
   {
      C_RB_LIST ret(list.to_rb());
      ret.LspRetList();
   }

   return RTNORM;
}


/*********************************************************/
/*.doc gs_getusrprj                          <external> */
/*+
  Questa funzione LISP ritorna la lista degli codici dei progetti
  e relative abilitazioni personali (non ereditate) ad un utente di GEOsim.
-*/  
/*********************************************************/
int gs_getusrprj(void)
{
   presbuf        arg = acedGetArgs();
   C_INT_INT_LIST list;

   acedRetNil();
   if (!arg || arg->restype != RTSHORT)    // Errore argomenti
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; } 

   // CHIAMATA FUNZIONE C++ RELATIVA
   if (gsc_getPersonalPrjPermissions(arg->resval.rint, &list) == GS_BAD) return RTERROR;

   if (list.get_count() > 0)
   {
      C_RB_LIST ret(list.to_rb());
      ret.LspRetList();
   }

   return RTNORM;
}
//-----------------------------------------------------------------------//
//  GS_SETUSRPRJ  //
//-----------------------------------------------------------------------//
int gs_setusrprj(void)
{
   resbuf         *arg;
   C_INT_INT_LIST list;

   arg = acedGetArgs();

   if (arg == NULL || arg->restype != RTSHORT)    // Errore argomenti //
      { GS_ERR_COD = eGSInvalidArg; acedRetNil(); return RTERROR; } 

   if (list.from_rb(arg->rbnext) == GS_BAD)
      { acedRetNil(); return RTERROR; } 

   if (gsc_setPersonalPrjPermissions(arg->resval.rint, list) == GS_BAD)
      { acedRetNil(); return RTERROR; }

   acedRetT();
   return RTNORM;
}


/*********************************************************/
/*.doc gs_getPrjPermissions                   <external> */
/*+
  Questa funzione LISP ritorna la lista degli codici dei progetti
  e relative abilitazioni ad un utente di GEOsim.
-*/  
/*********************************************************/
int gs_getPrjPermissions(void)
{
   presbuf        arg = acedGetArgs();
   C_INT_INT_LIST list;

   acedRetNil();
   if (!arg || arg->restype != RTSHORT)    // Errore argomenti
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; } 

   // CHIAMATA FUNZIONE C++ RELATIVA
   if (gsc_getPrjPermissions(arg->resval.rint, list) == GS_BAD) return RTERROR;

   if (list.get_count() > 0)
   {
      C_RB_LIST ret(list.to_rb());
      ret.LspRetList();
   }

   return RTNORM;
}


/*********************************************************/
/*.doc gs_getusrclass                         <external> */
/*+
  Funzione LISP per lettura abilitazione personale
  (non ereditata) alla banca dati di un utente GEOsim.
  Parametri:
  Lista di resbuf (<user ID><prj>)

  Ritorna una lista così composta:
  ((<cls><abilit>)...)
  dove
  <cls>     codice classe
  <abilit>  1 -> sola lettura, 2 -> modificabile
-*/  
/*********************************************************/
int gs_getusrclass(void)
{
   C_INT_INT_LIST list;
   resbuf         *arg;
   int             usr;

   arg = acedGetArgs();

   if (arg == NULL || arg->restype != RTSHORT)                    // Errore argomenti
      { GS_ERR_COD = eGSInvalidArg; acedRetNil(); return RTERROR; } 
   usr = (int)arg->resval.rint;
   
   if ((arg = arg->rbnext) == NULL || arg->restype != RTSHORT)    // Errore argomenti
      { GS_ERR_COD = eGSInvalidArg; acedRetNil(); return RTERROR; } 

   // CHIAMATA FUNZIONE C++ RELATIVA
   if (gsc_getPersonalClassPermissions(usr, arg->resval.rint, &list) == GS_BAD)
      { acedRetNil(); return RTERROR; }

   if (list.get_count() == 0) acedRetNil();
   else 
   {
      C_RB_LIST ret(list.to_rb());
      ret.LspRetList();
   }

   return RTNORM;
}
//-----------------------------------------------------------------------//
//  GS_SETUSRCLASS  //
//-----------------------------------------------------------------------//
int gs_setusrclass(void)
{
   C_INT_INT_LIST list;
   resbuf         *arg = acedGetArgs();
   int            usr;

   acedRetNil();
   
   if (!arg || arg->restype != RTSHORT)    // Errore argomenti
      { GS_ERR_COD = eGSInvalidArg;  return RTERROR; } 
   usr = (int) arg->resval.rint;
   
   if ((arg = arg->rbnext) == NULL || arg->restype != RTSHORT) //Errore argomenti
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; } 

   if (list.from_rb(arg->rbnext) == GS_BAD) return RTERROR;

   // CHIAMATA FUNZIONE C++ RELATIVA
   if (gsc_setPersonalClassPermissions(usr,arg->resval.rint, list) == GS_BAD) return RTERROR;

   acedRetT();
   
   return RTNORM;
}


/*********************************************************/
/*.doc gs_getClassPermissions                 <external> */
/*+
  Funzione LISP per lettura abilitazione alla banca dati di un utente GEOsim.
  Parametri:
  Lista di resbuf (<user ID><prj>)

  Ritorna una lista così composta:
  ((<cls><abilit>)...)
  dove
  <cls>     codice classe
  <abilit>  1 -> sola lettura, 2 -> modificabile
-*/  
/*********************************************************/
int gs_getClassPermissions(void)
{
   C_INT_INT_LIST list;
   resbuf         *arg;
   int             usr;

   arg = acedGetArgs();

   if (arg == NULL || arg->restype != RTSHORT)                    // Errore argomenti //
      { GS_ERR_COD = eGSInvalidArg; acedRetNil(); return RTERROR; } 
   usr = (int)arg->resval.rint;
   
   if ((arg = arg->rbnext) == NULL || arg->restype != RTSHORT)    //Errore argomenti//
      { GS_ERR_COD = eGSInvalidArg; acedRetNil(); return RTERROR; } 

   // CHIAMATA FUNZIONE C++ RELATIVA //
   if (gsc_getClassPermissions(usr, arg->resval.rint, list) == GS_BAD)
      { acedRetNil(); return RTERROR; }

   if (list.get_count() == 0) acedRetNil();
   else 
   {
      C_RB_LIST ret(list.to_rb());
      ret.LspRetList();
   }

   return RTNORM;
}


//-----------------------------------------------------------------------//
//  GS_CHKCLSVIS  //
//-----------------------------------------------------------------------//
int gs_chkclsvis(void)
{
   C_INT_INT_LIST list;
   C_PROJECT      *pPrj = NULL;  
   resbuf         *arg;

   arg = acedGetArgs();

   if (arg == NULL || arg->restype != RTSHORT)    // Errore argomenti //
      { GS_ERR_COD = eGSInvalidArg; acedRetNil(); return RTERROR; } 

   if (list.from_rb(arg->rbnext) == GS_BAD)
      { acedRetNil(); return RTERROR; } 

   if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(arg->resval.rint)) == NULL) 
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; } 

   // CHIAMATA FUNZIONE C++ RELATIVA
   if (gsc_chkClsPermissions(pPrj, list, 1) == GS_BAD)
      { acedRetNil(); return RTERROR; }

   if (list.get_count() == 0) acedRetNil();
   else 
   {
      C_RB_LIST ret(list.to_rb());
      ret.LspRetList();
   }

   return RTNORM;
}
//-----------------------------------------------------------------------//
//  GS_GETFAMILY  //
//-----------------------------------------------------------------------//
int gs_getfamily(void)
{
   C_FAMILY_LIST  list;
   resbuf        *arg;
   int            prj;
   C_PROJECT     *pPrj;

   acedRetNil();
   arg = acedGetArgs();

   if (arg == NULL || arg->restype != RTSHORT)    // Errore argomenti
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; } 
   prj = (int) arg->resval.rint;
   if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL) 
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; } 

   // CHIAMATA FUNZIONE C++ RELATIVA
   if (gsc_getfamily(pPrj, &list) == GS_BAD) return RTERROR;

   if (list.get_count() == 0) acedRetNil();
   else
   {
      C_RB_LIST ret(list.to_rb());
      ret.LspRetList();
   }
   
   return RTNORM;
}
/*********************************************************/
/*.doc gs_modpwd <external> */
/*+
  Questa funzione modifica la propria password.
  Lista : (<vecchia password> <nuova password>)
    
  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR.
-*/  
/*********************************************************/
int gs_modpwd(void)
{
   presbuf arg;
   TCHAR   OldPassword[MAX_LEN_PWD], NewPassword[MAX_LEN_PWD];
   
   acedRetNil();
   arg = acedGetArgs();
   if (arg == NULL || arg->restype != RTSTR) // Errore argomenti
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   // vecchia password
   if (wcslen(arg->resval.rstring) >= MAX_LEN_PWD)
      { GS_ERR_COD = eGSStringTooLong; return RTERROR; }
   else wcscpy(OldPassword, arg->resval.rstring);

   if ((arg = arg->rbnext) == NULL || arg->restype != RTSTR) // Errore argomenti
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   // nuova password
   if (wcslen(arg->resval.rstring) >= MAX_LEN_PWD)
      { GS_ERR_COD = eGSStringTooLong; return RTERROR; }
   else wcscpy(NewPassword, arg->resval.rstring);

   if (gsc_modpwd(OldPassword, NewPassword) == GS_BAD) return RTERROR;
   acedRetT();

   return RTNORM;
}


/*************************************************************************/
/*.doc gsc_login                                                         */
/*+
   Riceve login e password di un utente ne controlla l'esattezza nel file
   'GEOpwd' in caso affermativo aggiorna tutte i campi della variabile
   globale GEOsimAppl::GS_USER (allocando le stringhe) con le informazioni di 'GEOpwd'
   Parametri:
   const TCHAR *log;    Login utente
   const TCHAR *pwd;    Password utente
   int CheckSessions;   Flag; Se = GS_GOOD dopo la login effettuata con successo
                        controlla che l'utente loggato abbia sessioni attive appese
                        e in quel caso chiede conferma per la loro cancellazione
                        (default = GS_GOOD).

   La funzione ritorna GS_GOOD in caso di login corretta, GS_BAD altrimenti.
-*/
/*************************************************************************/
int gsc_login(const TCHAR *log, const TCHAR *pwd, int CheckSessions)
{
   C_USER     temp;
   C_LOG_HEAD log_head;

   if (gsc_startAutorization() == GS_BAD)
   {
      acutPrintf(gsc_msg(388)); // "\nErrore di licenza."
      return GS_BAD;
   }

   if (GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSSessionsFound; return GS_BAD; }
   if (!pwd || !log) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   // i files GEOPWD e GEOCOM sono stati spostati in GEOsimAppl::GEODIR + _T('\\') + GEOUSRDIR nella versione 2015
   C_STRING dstPath, srcPath;
   dstPath = GEOsimAppl::GEODIR + _T('\\') + GEOUSRDIR + _T('\\') + GEOPWD;
   if (gsc_path_exist(dstPath) != GS_GOOD)
   {
      srcPath = GEOsimAppl::GEODIR + _T('\\') + GEOPWD;
      if (gsc_copyfile(srcPath, dstPath) == GS_BAD) return GS_BAD;
   }
   dstPath = GEOsimAppl::GEODIR + _T('\\') + GEOUSRDIR + _T('\\') + GEOCOM;
   if (gsc_path_exist(dstPath) != GS_GOOD)
   {
      srcPath = GEOsimAppl::GEODIR + _T('\\') + GEOCOM;
      if (gsc_copyfile(srcPath, dstPath) == GS_BAD) return GS_BAD;
   }

   // comando di GEOsim
   if (temp.alloc_from_geopwd()==GS_BAD) return GS_BAD;
   if (log_head.from_geopwd()==GS_BAD) return GS_BAD;
     
   // Trova codice utente
   if ((temp.code = gsc_get_usrcode(log)) == -1) return GS_BAD;

   // Controlla password
   if (gsc_checkpwd(temp.code, pwd) == GS_BAD) return GS_BAD;

   // Carica login
   if (gsc_get_user(temp.code,&temp) == GS_BAD) return GS_BAD;

   // Setta la variabile globale di GEOsimAppl::GS_USER
   temp.set_login(GEOsimAppl::GS_USER);
  
   // inizializza le strutture usate da GEOsim che dipendono
   // dall'utente corrente.
   if (GEOsimAppl::initByUser() == GS_BAD) return GS_BAD;

   // comando "login" di ade
   if (gsc_ade_userset(log, pwd) != RTNORM)
   { 
      // se non esisteva l'utente map provo a crearlo
      if (gsc_createMapSuperUsr(log, pwd) == GS_BAD ||
          gsc_ade_userset(log, pwd) != RTNORM)
         { GS_ERR_COD = eGSInvAdeUser; return GS_BAD; }
   }

   // Notifico in file log
   TCHAR Msg[MAX_LEN_MSG];
   swprintf(Msg, MAX_LEN_MSG, _T("User <%s> logged."), log);
   gsc_write_log(Msg);

   // memorizzo la login in GEOSIM.INI per riproporla nella prossima login
   gsc_setLastLogin(log);
   // memorizzo il controllo sulle sessioni appese in GEOSIM.INI per
   // riproporla nella prossima login
   gsc_setCheckSessionOnLogin(CheckSessions);

   // Se devo verificare le sessioni dell'utente corrente
   if (CheckSessions == GS_GOOD)
   {
      C_PROJECT   *pPrj;

      pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.get_head();
      while (pPrj)
      {
         pPrj->ddSessionCheck();
         pPrj = (C_PROJECT *) pPrj->get_next();
      }
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc resbuf *gsc_chkusr                                                */
/*+ 
   Riceve in ingresso una login ed una struttura C_INT_INT_STR.
   Questa viene riempita con le informazioni dell'utente corrispondente
   secondo lo schema : <codice utente> <login> <livello> ).
   NB. livello=(0 -> SUPERUSER) (1->user).
   Parametri:
   TCHAR* login;
   C_INT_INT_STR *out;

   Ritorna GS_BAD in caso di errore o di login non esistente.  
-*/
/*************************************************************************/
int gsc_chkusr(TCHAR *login, C_INT_INT_STR *out)
{
   int code, su;

   if (out == NULL || login == NULL)
      { GS_ERR_COD = eGSNotAllocVar; return GS_BAD; }
   if ((code = gsc_get_usrcode(login)) == -1) return GS_BAD; 

   switch (gsc_superuser(code))
   {
      case GS_GOOD : su = SUPERUSR; break;   // Super-User
      case GS_BAD  : su = NORMALUSR; break;   // Utente normale
      default      : return GS_BAD; // Utente non abilitato
   }

   out->set_key(code);
   if (out->set_name(login)==GS_BAD) return GS_BAD;
   out->set_type(su);

   return GS_GOOD;
}   


/*************************************************************************/
/*.doc gsc_creausr(struct gs_user_type)  */
/*+ 
    (Solo per SUPER-USER)
    Crea un nuovo utente con le caratteristiche (login, pwd, livello).
    Ad esso viene assegnato un codice (numero d'ordine di memorizzazione)
    in 'GEOpwd'.
    Parametri:
    TCHAR *login;
    TCHAR *passw;
    int   UserType;          se = 0 superuser altrimenti utente normale

    Ritorna 0 in caso di errore, codice utente in caso di successo.
-*/
/*************************************************************************/
int gsc_creausr(const TCHAR *login, const TCHAR *passw, int UserType)
{
   int        code, flag = 0;
   int        file, i;
   C_USER     temp;
   C_LOG_HEAD log_head;

   if (!passw || !login) { GS_ERR_COD = eGSInvalidArg; return 0; }

   // la login non può iniziare con un #
   if (login[0] == FREE_USER) { GS_ERR_COD = eGSInvalidLogin; return 0; }

   if (gsc_superuser() == GS_BAD) { GS_ERR_COD = eGSOpNotAble; return 0; }

   // comando di GEOsim
   if ((file = gsc_open_geopwd()) == -1)    return 0;
   if (temp.alloc_from_geopwd(file) == GS_BAD) { _close(file); return 0; }
   if (log_head.read(file) == GS_BAD) { _close(file); return 0; }

   if (UserType == 0) temp.mode[0] = SUPER_USER;
   else temp.mode[0] = 0;

   //  Controlla esistenza login
   for (i = 1; i <= log_head.max_usr; i++)                  
   {
      if (gsc_get_log(file, i, LOG_ID, temp.log) == GS_BAD)
         { _close(file); return 0; }
                
      if (gsc_strcmp((TCHAR *) temp.log, login, FALSE) == 0)  // Login uguale case insensitive
         { GS_ERR_COD = eGSInvalidLogin; _close(file); return 0; }

      if (flag == 0)   // Trova il primo codice libero
         if (temp.log[0]==FREE_USER) { flag=1; code=i; }
   }                                                 

   // Codice libero non trovato->aggiunge nuovo codice in coda
   if (flag == 0) { log_head.max_usr = code = log_head.max_usr + 1; }

   // Salva dati nuovo utente
   if (gsc_set_log(file, code, LOG_ID, login) == GS_BAD)
      { _close(file); return 0; }
   if (gsc_set_log(file, code, PWD_ID, passw) == GS_BAD)
      { _close(file); return 0; }
   if (gsc_set_log(file, code, MODE_ID, temp.mode) == GS_BAD)
      { _close(file); return 0; }
   if (gsc_set_log(file, code, FLAGS_ID, temp.flags) == GS_BAD)
      { _close(file); return 0; }
   log_head.num_usr++;                              

   // Salva nuovo numero utenti in testa a 'GEOpwd'
   if (log_head.write(file) == GS_BAD) { _close(file); return 0; }
   
   _close(file);

   return code;
}


/*************************************************************************/
/*.doc gsc_delusr                                                        */
/*+ 
  Cancella un utente e rende disponibile il suo codice (solo per superuser).
  Parametri:
  int code;                Codice utente da cancellare
  const TCHAR *Password;   password utente corrente
  bool Destroy;            Se = true continua la cancellazione nonostante  
                           i controlli sui dati lo dovrebbero impedire (default = false)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
int gsc_delusr(int code, const TCHAR *Password, bool Destroy)
{
   int            file;
   C_USER         Usr;
   C_LOG_HEAD     log_head;
   C_WRK_SESSION_LIST    AreasList;
   C_INT_INT_LIST PrjctList;
   C_INT_INT      *punt;
   TCHAR          *prj_dir;
   C_STRING       Path;

   // solo per super-utente
   if (gsc_check_op(opDelUsr) == GS_BAD) return GS_BAD;

   // verifico che la password sia corretta
   if (gsc_strcmp(Password, GEOsimAppl::GS_USER.pwd) != 0)
      { GS_ERR_COD = eGSInvalidPwd; return GS_BAD; }

   // non ci si può auto-cancellare
   if (code == GEOsimAppl::GS_USER.code) { GS_ERR_COD = eGSInvalidUser; return GS_BAD; }

   if (Usr.alloc_from_geopwd() == GS_BAD) return GS_BAD;
   // leggo dati utente
   if (gsc_get_user(code, &Usr) == GS_BAD) return GS_BAD;

   if (Usr.is_superuser() == false)
   {
      // Lista dei progetti abilitati all'utente
      if (gsc_getPrjPermissions(code, PrjctList) == GS_BAD) return GS_BAD;
      // Ricarico la lista dei progetti x verificare se i permessi si riferiscono a
      // progetti ancora esistenti
      C_LIST_PRJ CurrentPrjList;

      if (CurrentPrjList.restore_projects() == GS_BAD) return GS_BAD;
      punt = (C_INT_INT *) PrjctList.get_head();
      while (punt)
      {
         if (CurrentPrjList.search_key(punt->get_key()) == NULL)
         {
            PrjctList.remove_at();
            punt = (C_INT_INT *) PrjctList.get_cursor();
         }
         else
            punt = (C_INT_INT *) PrjctList.get_next();
      }
   }

   // apro il file degli utenti bloccandolo agli altri
   if ((file = gsc_open_geopwd()) == -1) return GS_BAD;
   if (log_head.from_geopwd(file) == GS_BAD)
   {
      _close(file);
      return GS_BAD;
   }

   // non posso cancellare se esiste solo un utente
   if (log_head.num_usr <= 1)
   {
      _close(file);
      GS_ERR_COD = eGSInvalidUser;
      return GS_BAD;
   }

   // se si sta cancellando un superutente
   if (gsc_superuser(code, file) == GS_GOOD) 
   {  // conto il numero di superutenti
      C_USER usr;
      int    count = 0;

      if (usr.alloc_from_geopwd(file) == GS_BAD) { _close(file); return GS_BAD; }
      
      for (int i = 1; i <= log_head.max_usr; i++)
      {
         if (gsc_get_log(file, i, MODE_ID, usr.mode)==GS_BAD)
            { _close(file); return GS_BAD; }

         if (usr.is_superuser()) count++;
      }
   
      // non si può cancellare l'ultimo superutente
      if (count == 1) { GS_ERR_COD = eGSOpNotAble; _close(file); return GS_BAD; }
   }

   if (!Destroy)
   {
      // per ogni progetto verifico che l'utente non abbia alcuna sessione di lavoro
      punt = (C_INT_INT*) PrjctList.get_head();
      while (punt)
      {  // Lista di tutte le aree (-1) solo dell'utente (TRUE)
         if (AreasList.ruserarea(code, punt->get_key(), -1, TRUE) == GS_BAD)
            // se il progetto non c'era più non è un problema
            if (GS_ERR_COD != eGSInvalidPrjCode) { _close(file); return GS_BAD; }
         if (!AreasList.is_empty()) 
            { _close(file); GS_ERR_COD = eGSSessionsFound; return GS_BAD; }
         punt = (C_INT_INT*) PrjctList.get_next();   
      }
   }

   // per ogni progetto cancello il file delle abilitazioni alle classi e alle tabelle secondarie
   punt = (C_INT_INT*) PrjctList.get_head();
   while (punt)
   {
      // Legge direttorio del progetto dalla lista GEOsimAppl::PROJECTS
      // Se utente corrente e' SUPERUSER vedra' tutti i progetti disponibili
      if ((prj_dir = GEOsimAppl::PROJECTS.get_dir(punt->get_key())) == NULL)
      {
         if (Destroy) { punt = (C_INT_INT*) PrjctList.get_next(); continue; }
         _close(file);
         return GS_BAD;
      }

      Path = prj_dir;
      Path += _T('\\');
      Path += GEOUSRDIR;
      Path += _T("\\usr_");
      Path += code;
      Path += _T(".cls");
      if (gsc_path_exist(Path) == GS_GOOD)
         if (gsc_delfile(Path) == GS_BAD) { _close(file); return GS_BAD; }

      Path = prj_dir;
      Path += _T('\\');
      Path += GEOUSRDIR;
      Path += _T("\\usr_");
      Path += code;
      Path += _T(".sec");
      if (gsc_path_exist(Path) == GS_GOOD)
         if (gsc_delfile(Path) == GS_BAD) { _close(file); return GS_BAD; }

      punt = (C_INT_INT*) PrjctList.get_next();   
   }

   // cancello il file delle abilitazioni ai progetti
   Path = GEOsimAppl::GEODIR;
   Path += _T('\\');
   Path += GEOUSRDIR;
   Path += _T("\\usr_");
   Path += code;
   Path += _T(".prj");
   if (gsc_path_exist(Path) == GS_GOOD)
      if (gsc_delfile(Path) == GS_BAD) { _close(file); return GS_BAD; }

   // cancello utente dai file di eredità dei permessi
   C_INT_INT_STR_LIST UsrList;

   if (gsc_getusrlist(&UsrList, file) == GS_GOOD)
   {
      C_INT_INT_STR *pUsr = (C_INT_INT_STR *) UsrList.get_head();
      C_STR_LIST    UserNames;

      while (pUsr)
      {  // per gli utenti normali eccetto quello da cancellare
         if (pUsr->get_key() != code && pUsr->get_type() == NORMALUSR &&
             gsc_loadInheritanceUsers(pUsr->get_key(), UserNames, file) == GS_GOOD &&
             UserNames.remove_name(Usr.log) == GS_GOOD)
            gsc_saveInheritanceUsers(code, UserNames);

         pUsr = (C_INT_INT_STR *) UsrList.get_next();
      }
   }

   // cancello il file di eredità dei permessi
   Path = GEOsimAppl::GEODIR;
   Path += _T('\\');
   Path += GEOUSRDIR;
   Path += _T("\\usr_");
   Path += code;
   Path += _T(".inh");
   if (gsc_path_exist(Path) == GS_GOOD)
      if (gsc_delfile(Path) == GS_BAD) { _close(file); return GS_BAD; }

   // cancello utente
   if (gsc_set_log(file, code, FREE_ID, (TCHAR *) "") == GS_BAD)
      { _close(file); return GS_BAD; }
   log_head.num_usr--;

   // Se cancello ultimo utente della lista di 'GEOpwd' riduco file
   if (code == log_head.max_usr)
   {
      do
      {
         log_head.max_usr--;
         if (log_head.max_usr==0) break;
         if (gsc_get_log(file, log_head.max_usr, LOG_ID, Usr.log) == GS_BAD)
            { _close(file); return GS_BAD; }
         // Riduce dimensione file
         if (_chsize(file, _filelength(file)-log_head.tot_len) == -1)
            { GS_ERR_COD=eGSWriteFile; _close(file); return GS_BAD; }
      }
      while (Usr.log[0]==FREE_USER);
   }

   if (log_head.write(file)==GS_BAD) {_close(file); return GS_BAD; }
   _close(file); // rilascio il file degli utenti agli altri

   return GS_GOOD;
}


/*************************************************************************/
/*.doc int gsc_whoami(C_INT_INT_STR*)  */
/*+ 
   Compila C_INT_INT_STR con : <codice-utente> <login> <livello>.
   dell'utente attualmente logato.
   Ritorna GS_BAD in caso di nessun utente logato, GS_GOOD altrimenti.
   NB: livello=(0->SUPERUSER) (1->user).  
-*/
/*************************************************************************/
int gsc_whoami(C_INT_INT_STR *out)
{
   if (GEOsimAppl::GS_USER.code == 0) { GS_ERR_COD = eGSInvalidLogin; return GS_BAD; }
   if (GEOsimAppl::GS_USER.log == NULL) { GS_ERR_COD = eGSInvalidLogin; return GS_BAD;	}

   if (out != NULL)
   {
      out->set_key(GEOsimAppl::GS_USER.code);
      if (out->set_name(GEOsimAppl::GS_USER.log) == GS_BAD) return GS_BAD;
      if (gsc_superuser()==GS_GOOD) out->set_type(0);
      else out->set_type(1);
   }
   return GS_GOOD;
}


/*************************************************************************/
/*.doc int gsc_getLastLogin                                              */
/*+ 
   Funzione che restituisce l'ultima login usata per entrare in GEOsim.
   Parametri:
   TCHAR login[MAX_LEN_LOGIN];    login precedente

   Ritorna GS_BAD in caso di errore o nessuna login precedente, GS_GOOD altrimenti.
-*/
/*************************************************************************/
int gsc_getLastLogin(TCHAR login[MAX_LEN_LOGIN])
{
   C_STRING pathfile;
   TCHAR    *str;

   // leggo la login in GEOSIM.INI
   pathfile = GEOsimAppl::CURRUSRDIR; // Directory locale dell'utente corrente
   pathfile += _T('\\');
   pathfile += GS_INI_FILE;
   if (gsc_path_exist(pathfile) == GS_BAD) return GS_BAD;
   str = login;
   if (gsc_get_profile(pathfile.get_name(), GS_INI_LABEL, _T("LastLogin"), &str,
                       MAX_LEN_LOGIN - 1, 0) == GS_BAD) return GS_BAD;
   gsc_alltrim(login);

   return GS_GOOD;
}


/*************************************************************************/
/*.doc int gsc_setLastLogin                                              */
/*+ 
   Funzione che setta l'ultima login usata per entrare in GEOsim.
   Parametri:
   const TCHAR *login;    login precedente

   Ritorna GS_BAD in caso di errore, GS_GOOD altrimenti.
-*/
/*************************************************************************/
static int gsc_setLastLogin(const TCHAR *login)
{
   C_STRING pathfile;

   // memorizzo la login in GEOSIM.INI per riproporla nella prossima login
   pathfile = GEOsimAppl::CURRUSRDIR; // Directory locale dell'utente corrente
   pathfile += _T('\\');
   pathfile += GS_INI_FILE;

   return gsc_set_profile(pathfile.get_name(), GS_INI_LABEL, _T("LastLogin"), login);
}


/*************************************************************************/
/*.doc int gsc_getCheckSessionOnLogin                                    */
/*+ 
   Funzione che restituisce se all'ultima login si era voluto il controllo 
   sulle sessioni di lavoro appese.
   Parametri:
   int *Value;

   Ritorna GS_BAD in caso di errore o nessuna valore precedente, GS_GOOD altrimenti.
-*/
/*************************************************************************/
int gsc_getCheckSessionOnLogin(int *Value)
{
   C_STRING pathfile, buff;

   // leggo flag di controllo sesssioni dal file GEOSIM.INI
   pathfile = GEOsimAppl::CURRUSRDIR; // Directory locale dell'utente corrente
   pathfile += _T('\\');
   pathfile += GS_INI_FILE;
   if (gsc_getInfoFromINI(_T("LastCheckSessionOnLogin"), buff, &pathfile) == GS_BAD)
      return GS_BAD;   
   *Value = buff.toi();

   return GS_GOOD;
}


/*************************************************************************/
/*.doc int gsc_setCheckSessionOnLogin                                    */
/*+ 
   Funzione che setta se all'ultima login si era voluto il controllo 
   sulle sessioni di lavoro appese.
   Parametri:
   int Value;    login precedente

   Ritorna GS_BAD in caso di errore, GS_GOOD altrimenti.
-*/
/*************************************************************************/
static int gsc_setCheckSessionOnLogin(int Value)
{
   C_STRING pathfile, Buff;

   // memorizzo il flag di controllo sesssioni nel file GEOSIM.INI 
   // per riproporla nella prossima login
   pathfile = GEOsimAppl::CURRUSRDIR; // Directory locale dell'utente corrente
   pathfile += _T('\\');
   pathfile += GS_INI_FILE;
   Buff = Value;

   return gsc_setInfoToINI(_T("LastCheckSessionOnLogin"), Buff, &pathfile);
}


/*************************************************************************/
/*.doc int gsc_getusrlist                                                */
/*+ 
  Compila la lista in ingresso con le informazioni degli utenti abilitati 
  secondo la struttura C_INT_INT_STR: <codice-utente> <login> <livello>.
  NB: livello=(0->SUPERUSER) (1->user). 
  Parametri:
  C_INT_INT_STR_LIST *out;
  int file;    Opzionale, il descrittore al file già aperto.
               Se = NULL la procedura provvede ad aprire il file
               delle password in sola lettura (default = NULL).

  Ritorna GS_BAD in caso di errore, GS_GOOD altrimenti.
-*/
/*************************************************************************/
int gsc_getusrlist(C_INT_INT_STR_LIST *out, int file)
{
   int           su, i, cont = 0;
   C_USER        temp;
   C_LOG_HEAD    log_head;
   C_INT_INT_STR *new_node;

   if (out == NULL) { GS_ERR_COD = eGSNotAllocVar; return GS_BAD; }

   if (temp.alloc_from_geopwd(file)==GS_BAD)  return GS_BAD; 
   if (log_head.from_geopwd(file)==GS_BAD)    return GS_BAD; 
   
   out->remove_all();
   
   for (i = log_head.max_usr; i > 0; i--)
   {
      if (gsc_get_user(i, &temp, file) == GS_BAD) 
         { out->remove_all(); return GS_BAD; }
      if (temp.code!=0)
      {
         cont++;
         su = (temp.is_superuser()) ? 0 : 1;

         new_node = new C_INT_INT_STR(log_head.log_len+1);
         out->add_tail(new_node);         

         new_node->set_key(temp.code);
         new_node->set_name(CW2A(temp.log)); // Conversione in ASCII
         new_node->set_type(su);
      }
   }

   if (log_head.num_usr != cont) // Errore su GEOPWD //
      { GS_ERR_COD = eGSErrGeopwd; out->remove_all(); return GS_BAD; }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc int gsc_getusrcomm(int,C_INT_LIST*)  */
/*+
   Riceve il codice di un utente ed il puntatore a lista di 'int'.
   Compila la lista con i codici (compatibile con 'gs_getgscomm')
   dei comandi a cui l'utente e' abilitato.   Se l'utente e'
   SUPERUSER o non esistente e in caso di errore ritorna GS_BAD.
-*/
/*************************************************************************/
int gsc_getusrcomm(int code, C_INT_LIST *out)
{
   div_t      punt;
   int        file, i;
   C_USER     temp;
   C_LOG_HEAD log_head;
   C_INT      *new_node;

   if (!out) { GS_ERR_COD = eGSNotAllocVar; return GS_BAD; }

   if ((file = gsc_open_geopwd(READONLY)) == -1) return GS_BAD;
   if (temp.alloc_from_geopwd(file) == GS_BAD) { _close(file); return GS_BAD; }
   if (log_head.read(file) == GS_BAD) { _close(file); return GS_BAD; }

   // Se utente e' SUPER-USER o non e' abilitato ritorno GS_BAD
   if (gsc_superuser(code, file) != GS_BAD) 
      { _close(file); GS_ERR_COD = eGSInvalidUser; return GS_BAD; }

   // Carica FLAGS operazioni
   if (gsc_get_log(file, code, FLAGS_ID, temp.flags) == GS_BAD)
      { _close(file); return GS_BAD; }
   _close(file);

   out->remove_all();
   
   // Controlla bit di abilitazione operazioni
   for (i = (log_head.flags_len * 8) - 1; i >= 0; i--)
   {
      punt = div(i, 8);
      if ((temp.flags[punt.quot] & gsc_bitmask(punt.rem)) != 0)
      {
         if ((new_node = new C_INT(i)) == NULL)
            { out->remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         out->add_tail(new_node);
      }
   }
  
   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_setusrcomm(int,C_INT_LIST*)  */
/*+
   (solo per SUPERUSER)
   Riceve un codice utente ed una lista di 'int' contenente i numeri 
   d'ordine (sempre secondo gs_getgscomm) dei comandi a cui si vuole 
   abilitare l'utente (tutti gli altri comandi saranno disabilitati).
   Ritorna GS_BAD se l'utente corrente non e' SUPERUSER, nel caso il codice
   dell'utente che si vuole abilitare non sia valido o corrisponda ad un
   SUPERUSER, oppure in caso di errore. GS_GOOD altrimenti.
-*/
/*************************************************************************/
int gsc_setusrcomm(int code, C_INT_LIST *in)
{
   div_t      flag;
   int        file, op;
   C_USER     temp;
   C_LOG_HEAD log_head;
   C_INT      *punt;

   if (!in) { GS_ERR_COD = eGSNotAllocVar; return GS_BAD; }

   if (log_head.from_geopwd() == GS_BAD)   { return GS_BAD; }
   if (temp.alloc_from_geopwd() == GS_BAD) { return GS_BAD; }

   // Se sono non sono SUPER-USER ritorno (nil)
   if (gsc_superuser() != GS_GOOD)
      { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }
   // Se l'utente da settare non esiste  o e' SUPER-USER
   if (gsc_superuser(code) != GS_BAD) 
      { GS_ERR_COD = eGSInvalidUser; return GS_BAD; }

   punt = (C_INT*)in->get_head();                       
   while (punt)
   {
      op = punt->get_key();
      if (op < 0 || op >= (log_head.flags_len * 8))
         { GS_ERR_COD = eGSInvalidOpCod; return GS_BAD; } 
      flag = div(op, 8);
      temp.flags[flag.quot] |= gsc_bitmask(flag.rem);

      punt = (C_INT*)in->get_next();
   }
            
   // Setta FLAGS operazioni
   if ((file = gsc_open_geopwd()) == -1) return GS_BAD;
   if (gsc_set_log(file, code, FLAGS_ID, temp.flags) == GS_BAD)
      { _close(file); return GS_BAD; }
   _close(file);

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_getgscomm(C_INT_STR_LIST*)  */
/*+
   Riceve un puntatore a lista di stringhe e la compila con i nomi di tutti
   i comandi abilitabili o disabilitabili agli utenti con i loro codici.
   I primi sono comandi di GEOsim gli altri vengono letti dal file GEOCOM 
   contenente i comandi definita dall'utente.
   
   Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
int gsc_getgscomm(C_INT_STR_LIST *out)
{
   C_STRING   path;
   TCHAR      comm[MAX_LEN_COMMANDNAME+1];
   FILE       *file;
   int        i;
   C_USER     temp;
   C_LOG_HEAD log_head;
   C_INT_STR  *new_node;

   if (!out) { GS_ERR_COD = eGSNotAllocVar; return GS_BAD; }

   if (log_head.from_geopwd() == GS_BAD)   return GS_BAD;
   if (temp.alloc_from_geopwd() == GS_BAD) return GS_BAD;
   
   out->remove_all();
   for (i = 0; i < ELEMENTS(GEOsimAppl::GS_COMMANDS); i++)
   {
      if ((new_node = new C_INT_STR(GEOsimAppl::GS_COMMANDS[i].code, GEOsimAppl::GS_COMMANDS[i].name)) == NULL)
         { out->remove_all(); return GS_BAD; }
      out->add_tail(new_node);
   }

   path = GEOsimAppl::GEODIR + _T('\\') + GEOUSRDIR + _T('\\') + GEOCOM;
   
   // SE NON TROVA IL FILE RITORNA LA LISTA COSI' COME E'
   if ((file = gsc_fopen(path, _T("r"), ONETEST)) != NULL)
   {  // i codici dei comandi personalizzati sono da 70 in poi
      i = 70;
      while (fwscanf(file, _T("%s"), comm) != EOF)
      {
         if (i < 0 || i >= (log_head.flags_len * 8))
            { GS_ERR_COD = eGSInvalidOpCod; return GS_BAD; } 

         if ((new_node = new C_INT_STR(i, comm)) == NULL)
            { out->remove_all(); gsc_fclose(file); return GS_BAD; }
         out->add_tail(new_node);

         i++;
      }
      gsc_fclose(file);
   }

   return GS_GOOD;
}      


/*************************************************************************/
/*.doc gsc_getPersonalPrjPermissions                                     */
/*+
   Riceve un codice utente ed un puntatore ad una lista di coppie di 'int'.
   Compila la lista con i codici dei progetti a cui l'utente
   puo'accedere e il livello di visibilita' corripondente.
   Tali abilitazioni sono lette dal file corrispondente all'utente nel 
   sottodirettirio USR di GEOSIM.
   Parametri:
   int usr;                codice utente
   C_INT_INT_LIST *out;    lista dei permessi

   Ritorna GS_BAD nel caso il codice non corrisponda ad alcun utente o 
   corrisponda ad un SUPERUSER (per cui non ha senso chiamare questa
   funzione) oppure in caso di errore. GS_GOOD altrimenti.
-*/
/*************************************************************************/
int gsc_getPersonalPrjPermissions(int usr, C_INT_INT_LIST *out)
{
   int  file;
   int  prj, vis, result;
   long cursor = 4; // Numero byte di intestazione file (codice utente)

   out->remove_all();

   if ((result = gsc_superuser(usr)) == -1) // Utente non esistente
      { GS_ERR_COD = eGSInvalidUser; return GS_BAD; }
   if (result == GS_GOOD) //  e' un superuser
   {
      C_LIST_PRJ PrjList;
      C_PROJECT  *pPrj;

      if (PrjList.restore_projects(usr) == GS_BAD) return GS_BAD;

      pPrj = (C_PROJECT *) PrjList.get_head();
      while (pPrj)
      {
         out->values_add_tail(pPrj->get_key(), pPrj->get_level());

         pPrj = (C_PROJECT *) PrjList.get_next();
      }
   }
   else  // utente normale
   {
      // Se ha problemi ad aprire il file ritorna nessuna abilitazione
      if ((file = gsc_open_usrprj(usr, READONLY)) == -1) return GS_GOOD;
                                    
      while((prj = gsc_byte2int(file,2,cursor)) != -1)  // Legge codice progetto
      {
         cursor+=2;
         // Legge visibilita' progetto
         if ((vis = gsc_byte2int(file,1,cursor)) == -1)   
         {            
            out->remove_all(); _close(file);
            return GS_BAD; 
         }
         if (prj<1)    
         {  // Codice progetto non valido
            cursor++;
            continue;
         }
         if (vis != GSInvisibleData && vis != GSReadOnlyData && vis != GSUpdateableData)    
            // Codice di abilitazione non valido
            vis = GSInvisibleData;

         cursor++;
      
         out->values_add_tail(prj, vis);
      }
      
      _close(file); 
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_setPersonalPrjPermissions                                     */
/*+
   (solo per SUPERUSER)
   Riceve un codice utente ed una lista di coppie di 'int' contenente i 
   codici dei progetti a cui si vuole abilitare l'accesso e il tipo di 
   visibilita'.
   Tali abilitazioni sono salvate nel file corispondente all'utente nel 
   sottodirettirio USR di GEOSIM.
   Ritorna GS_BAD se l'utente corrente non e' SUPERUSER, nel caso il codice
   dell'utente che si vuole abilitare non sia valido o corrisponda ad un
   SUPERUSER, oppure in caso di errore. GS_GOOD altrimenti.
   Parametri:
   int            usr;  codice utente
   C_INT_INT_LIST &in;  lista dei progetti con relativa abilitazione

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/
/*************************************************************************/
int gsc_setPersonalPrjPermissions(int usr, C_INT_INT_LIST &in)
{
   C_INT_INT_LIST prj_list, cls_list, new_prj_list;  // Liste di lavoro
   C_INT_INT      *punt, *oldprj, *oldcls;
   int            file;
   long           cursor = 4; // Numero byte di intestazione file (codice utente)
   int            prj;
   GSDataPermissionTypeEnum vis;
   C_WRK_SESSION_LIST AreasList;

   // Operazione solo per superuser
   if (gsc_check_op(opModUsr) == GS_BAD) return GS_BAD;

   if (in.copy(&new_prj_list) == GS_BAD) return GS_BAD;

   // Carica vecchia situazione abilitazioni
   // In questa funzione si controlla implicitamente validita' dell'utente
   if (gsc_getPersonalPrjPermissions(usr, &prj_list) == GS_BAD) return GS_BAD;

   // controllo i progetti resi invisibili all'utente
   punt = (C_INT_INT *) prj_list.get_head();
   while (punt)
   {
      prj = punt->get_key();
      if (new_prj_list.search_key(prj) == NULL) // progetto non più abilitato
      {
         // controllo che l'utente non abbia in uso una sessione di lavoro nel
         // progetto che si sta disabilitando
         if (AreasList.ruserarea(usr, prj, -1, TRUE) == GS_BAD)
            if (GS_ERR_COD == eGSInvalidPrjCode) // il progetto non c'è più
            {
               prj_list.remove_at();
               punt = (C_INT_INT *) prj_list.get_cursor();
               continue;
            }
            else
               return GS_BAD;

         if (!AreasList.is_empty()) { GS_ERR_COD = eGSSessionsFound; return GS_BAD; }
      }
      punt = (C_INT_INT *) prj_list.get_next();
   }
   
   // CONTROLLA CORRETTEZZA DEI PARAMETRI
   punt = (C_INT_INT*) new_prj_list.get_head();
   while (punt)
   {
      prj = punt->get_key();
      vis = (GSDataPermissionTypeEnum) punt->get_type();
      oldprj = (C_INT_INT *) prj_list.search_key(prj);

      // Cerca progetto nella lista GEOsimAppl::PROJECTS
      if (GEOsimAppl::PROJECTS.search_key(prj) == NULL)
      {
         new_prj_list.remove_at(); // lo elimino dalla lista
         punt = (C_INT_INT*)new_prj_list.get_cursor();
         continue;
      }

      switch (vis)
      {
         case GSUpdateableData:
            // Non deve modificare abilitazione classi
            prj_list.remove_key(prj);
            punt=(C_INT_INT*) new_prj_list.get_next();
            break;

         case GSReadOnlyData:
            // Se prima il progetto era modificabile ed ora è in sola lettura
            if (oldprj && oldprj->get_type() == (int) GSUpdateableData)
            {
               // controllo che l'utente non abbia in uso una sessione di lavoro in
               // modalità "modifica" nel progetto che si sta disabilitando
               if (AreasList.ruserarea(usr, prj, -1, TRUE) == GS_BAD) return GS_BAD;
               if (AreasList.search_level(GSUpdateableData))
                  { GS_ERR_COD = eGSSessionsFound; return GS_BAD; }
            }
            punt = (C_INT_INT *) new_prj_list.get_next();
            break;
         
         case GSInvisibleData:
            // Se prima il progetto era modificabile o in sola lettura ed ora è invisibile
            if (oldprj && 
                (oldprj->get_type() == (int) GSUpdateableData || oldprj->get_type() == (int) GSReadOnlyData))
            {
               // controllo che l'utente non abbia in uso una sessione di lavoro nel
               // progetto che si sta disabilitando
               if (AreasList.ruserarea(usr, prj, -1, TRUE) == GS_BAD) return GS_BAD;
               if (!AreasList.is_empty()) { GS_ERR_COD = eGSSessionsFound; return GS_BAD; }
            }
            punt=(C_INT_INT*) new_prj_list.get_next();
            break;
         
         case GSNonePermission:
            // Se ora il progetto ha i permessi ereditati

            // controllo che l'utente non abbia in uso una sessione di lavoro nel
            // progetto che si sta trattando
            if (AreasList.ruserarea(usr, prj, -1, TRUE) == GS_BAD) return GS_BAD;
            if (!AreasList.is_empty()) { GS_ERR_COD = eGSSessionsFound; return GS_BAD; }
            
            new_prj_list.remove_at(); // lo elimino dalla lista
            punt = (C_INT_INT*)new_prj_list.get_cursor();
      }
   }

   // SCRIVE DATI CORRETTI SUL FILE .PRJ
   // Apre file e cancella vecchia configurazione                       
   if ((file = gsc_open_usrprj(usr, UPDATEABLE)) == -1) return GS_BAD;
   punt = (C_INT_INT *) new_prj_list.get_head();

   while (punt)
   {
      prj = punt->get_key();
      vis = (GSDataPermissionTypeEnum) punt->get_type();

      if (gsc_int2byte(file, prj, 2, cursor) == GS_BAD) { _close(file); return GS_BAD; }
      cursor += 2;
      if (gsc_int2byte(file, (int) vis, 1, cursor) == GS_BAD) { _close(file); return GS_BAD; }
      cursor++;  
      punt = (C_INT_INT *) new_prj_list.get_next();   
   }
   _close(file); 


   // AGGIORNA ABILITAZIONI progetti NEI FILE .CLS DEI PROGETTI
   punt = (C_INT_INT *) new_prj_list.get_head();
   while (punt)
   {
      prj = punt->get_key();
      vis = (GSDataPermissionTypeEnum) punt->get_type();

      // Controlla che non vi siano classi del progetto di livello piu' alto
      if (vis == GSReadOnlyData) // Progetto in sola lettura
      {
         if ((oldprj = (C_INT_INT *) prj_list.search_key(prj)) != NULL)
            if (oldprj->get_type() == (int) GSUpdateableData)
            {  // Se prima il progetto era modificabile ed ora è in sola lettura
               // Modifica file abilitazione classi di quel progetto
               // Porta tutto il modificabile a sola lettura
               if (gsc_getPersonalClassPermissions(usr, prj, &cls_list) == GS_BAD) return GS_BAD;

               while ((oldcls = (C_INT_INT *) cls_list.search_type((int) GSUpdateableData)) != NULL)
                  oldcls->set_type((int) GSReadOnlyData);

               if (gsc_setPersonalClassPermissions(usr, prj, cls_list) == GS_BAD) return GS_BAD;
               prj_list.C_LIST::remove(oldprj);
            }
      }
      else
      if (vis == GSInvisibleData) // Progetto invisibile
      {
         if ((oldprj = (C_INT_INT *) prj_list.search_key(prj)) != NULL)
            if (oldprj->get_type() == (int) GSUpdateableData || 
                oldprj->get_type() == (int) GSReadOnlyData)
            {  // Se prima il progetto era modificabile o in sola lettura ed ora è invisibile
               // Modifica file abilitazione classi di quel progetto
               // Porta tutto il modificabile o sola lettura a invisibile
               if (gsc_getPersonalClassPermissions(usr, prj, &cls_list) == GS_BAD) return GS_BAD;

               while ((oldcls = (C_INT_INT *)cls_list.search_type((int) GSUpdateableData)))
                  oldcls->set_type((int) GSInvisibleData);

               while ((oldcls = (C_INT_INT *)cls_list.search_type((int) GSReadOnlyData)))
                  oldcls->set_type((int) GSInvisibleData);

               if (gsc_setPersonalClassPermissions(usr, prj, cls_list) == GS_BAD) return GS_BAD;
               prj_list.C_LIST::remove(oldprj);
            }
      }

      punt = (C_INT_INT*) new_prj_list.get_next();   
   }

   // Cancello abilitazioni alle classi dei progetti non settati
   cls_list.remove_all();
   oldprj = (C_INT_INT *) prj_list.get_head();
   while (oldprj)
   {
      if (gsc_setPersonalClassPermissions(usr, oldprj->get_key(), cls_list) == GS_BAD) return GS_BAD;
      oldprj = (C_INT_INT *) prj_list.get_next();
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_getPersonalClassPermissions                                   */
/*+
   Ritorna una lista contenente i codici delle classi a cui l'utente
   puo'accedere e il livello di visibilita corripondente secondo :
   ...(<codice-classe> <Permesso>) (<codice-classe> <Permesso>)..
   NB: Permesso = (0->invisibile) (1->sola lettura) (2->modificabile).
   Tali abilitazioni sono salvate nel file corrispondente all'utente nel 
   sottodirettirio USR del direttorio del progetto.
   Parametri:
   int usr;                codice utente        (input)
   int prj;                codice progetto      (input)
   C_INT_INT_LIST *out;    lista  ((classe visib)...)

   Ritorna GS_BAD in caso di errore o se <usr> corrisponde ad un
   SUPERUSER altrimenti GS_GOOD.
-*/
/*************************************************************************/
int gsc_getPersonalClassPermissions(int usr, int prj, C_INT_INT_LIST *out)
{
   int  file;
   int  cls, vis;
   long cursor = 8; // Numero byte di intestazione file (codice usr e prj)

   out->remove_all();

   if (gsc_superuser(usr) != GS_BAD)                  // Utente non esiste
      { GS_ERR_COD = eGSInvalidUser; return GS_BAD; } // o e' superuser

   // Se ha problemi ad aprire il file ritorna nessuna abilitazione
   if ((file = gsc_open_usrclass(usr, prj, READONLY)) == -1) return GS_GOOD;
                                        
   while ((cls = gsc_byte2int(file, 2, cursor)) != -1)  // Legge codice classe
   {
      cursor += 2;
      
      // Legge visibilita' classe
      if ((vis = gsc_byte2int(file, 1, cursor)) == -1)   
      {            
         out->remove_all(); _close(file);
         return GS_BAD; 
      }
      if (vis != GSInvisibleData && vis != GSReadOnlyData && vis != GSUpdateableData)    
      {  // Codice di abilitazione non valido
         out->remove_all(); _close(file);
         GS_ERR_COD=eGSInvalidLevel; return GS_BAD; 
      }
      cursor++;

      out->values_add_tail(cls, vis);
   }

   _close(file); 

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_setPersonalClassPermissions                                   */
/*+
   (solo per SUPERUSER)
   Riceve codice utente e progetto ed una lista contenente i codici 
   delle classi a cui si vuole abilitare l'accesso e il tipo di visibilita'.
   ...(<codice-classe> <Permesso>) (<codice-classe> <Permesso>)..
   NB: Permesso = (0->invisibile) (1->sola lettura) (2->modificabile).
   Legge direttorio del progetto dalla lista GEOsimAppl::PROJECTS.
   Se utente corrente e' SUPERUSER vedra' tutti i progetti disponibili.
   Se e' solo USER vedra solo i progetti a lui abilitati.
   Tali abilitazioni sono salvate nel file corispondente all'utente nel 
   sottodirettirio USR del direttorio del progetto.
   Parametri:
   int usr;
   int prj;
   C_INT_INT_LIST &in;

   Ritorna GS_BAD se l'utente corrente non e' SUPERUSER, nel caso il codice
   dell'utente che si vuole abilitare non sia valido o corrisponda ad un
   SUPERUSER, oppure in caso di errore. GS_GOOD altrimenti.
-*/
/*************************************************************************/
int gsc_setPersonalClassPermissions(int usr, int prj, C_INT_INT_LIST &in)
{
   C_INT_INT_LIST vis_list, new_class_list, old_class_list;
   C_INT_INT      *punt, *pOldCls;
   C_PROJECT     *pPrj = NULL;
   int            file, cls;
   long           SessionCode, cursor = 8; // Numero byte di intestazione file (codice usr e prj)
   GSDataPermissionTypeEnum PrjPermission, ClsPermission;

   // Operazione solo per superuser
   if (gsc_check_op(opModUsr) == GS_BAD) return GS_BAD;

   // Ricavo il puntatore al progetto
   if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL) 
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; } 

   if (in.copy(&new_class_list) == GS_BAD) return GS_BAD;

   // Legge visibilita' del progetto
   // Controlla anche la validita' dell'utente e l'esistenza del progetto
   if (gsc_getPersonalPrjPermissions(usr, &vis_list) == GS_BAD) return GS_BAD;
   if ((punt = (C_INT_INT *) vis_list.search_key(prj)))
      PrjPermission = (GSDataPermissionTypeEnum) punt->get_type();
   else
      PrjPermission = GSNonePermission;

   // Carica vecchia situazione abilitazioni classi
   if (gsc_getPersonalClassPermissions(usr, prj, &old_class_list) == GS_BAD) return GS_BAD;
   
   C_SINTH_CLASS_LIST SinthClassList;
   if (pPrj->getSinthClassList(SinthClassList) == GS_BAD) return GS_BAD;

   // CONTROLLA CORRETTEZZA DEI PARAMETRI
   punt = (C_INT_INT *) new_class_list.get_head();
   while (punt)
   {
      cls           = punt->get_key();
      ClsPermission = (GSDataPermissionTypeEnum) punt->get_type();
      pOldCls       = (C_INT_INT *) old_class_list.search_key(cls);

      // Cerca classe nella lista SinthClassList
      if (SinthClassList.search_key(cls) == NULL)
      {
         new_class_list.remove_at(); // la elimino dalla lista
         punt = (C_INT_INT *) new_class_list.get_cursor();
         continue;
      }

      if (PrjPermission == GSNonePermission) // nessun permesso esplicito per il progetto (ereditato da altro utente)
      {
         if (ClsPermission != GSNonePermission) { GS_ERR_COD = eGSInvalidLevel; return GS_BAD; }       
      }
      else
         // Livello di abilitazione superiore a quello del progetto
         if (ClsPermission > PrjPermission) { GS_ERR_COD = eGSInvalidLevel; return GS_BAD; }       

      switch (ClsPermission)
      {
         case GSUpdateableData:
            punt = (C_INT_INT *) new_class_list.get_next();
            break;

         case GSReadOnlyData:
            // Se prima la classe era modificabile ed ora è in sola lettura
            if (pOldCls && pOldCls->get_type() == (int) GSUpdateableData)
               // verifico se la classe fa parte di una sessione di lavoro dell 'utente
               if (gsc_is_inarea(prj, cls, &SessionCode, &usr) == GS_BAD || SessionCode > 0)
                  { GS_ERR_COD = eGSSessionsFound; return GS_BAD; }
            
            punt = (C_INT_INT *) new_class_list.get_next();
            break;

         case GSInvisibleData:
            // Se prima la classe era modificabile o in sola lettura ed ora è invisibile
            if (pOldCls && 
                (pOldCls->get_type() == (int) GSUpdateableData || pOldCls->get_type() == (int) GSReadOnlyData))
               // verifico se la classe fa parte di una sessione di lavoro dell 'utente
               if (gsc_is_inarea(prj, cls, &SessionCode, &usr) == GS_BAD || SessionCode > 0)
                  { GS_ERR_COD = eGSSessionsFound; return GS_BAD; }
            
            punt = (C_INT_INT *) new_class_list.get_next();
            break;

         case GSNonePermission:
            // Se ora la classe ha i permessi ereditati

            // verifico se la classe fa parte di una sessione di lavoro dell 'utente
            if (gsc_is_inarea(prj, cls, &SessionCode, &usr) == GS_BAD || SessionCode > 0)
               { GS_ERR_COD = eGSSessionsFound; return GS_BAD; }
            
            new_class_list.remove_at(); // la elimino dalla lista
            punt = (C_INT_INT *) new_class_list.get_cursor();
            break;
      }
   }

   // CONTROLLA COERENZA DELLE VISIBILITA' SU CLASSI PARENTI
   if (gsc_chkClsPermissions(pPrj, in, 0) == GS_BAD) return GS_BAD;

   // SCRIVE SU FILE PARAMETRI CORRETTI

   // Legge direttorio del progetto dalla lista GEOsimAppl::PROJECTS
   // Apre file e cancella vecchia configurazione
   if ((file = gsc_open_usrclass(usr, prj, UPDATEABLE)) == -1) return GS_BAD;

   punt = (C_INT_INT *) new_class_list.get_head();
   while (punt)
   {
      cls           = punt->get_key();
      ClsPermission = (GSDataPermissionTypeEnum) punt->get_type();
      if (gsc_int2byte(file, cls, 2, cursor) == GS_BAD)
         { _close(file); return GS_BAD; }
      cursor+=2;
      if (gsc_int2byte(file, (int) ClsPermission, 1, cursor) == GS_BAD)
         { _close(file); return GS_BAD; }
      cursor++;  
      punt=(C_INT_INT*) new_class_list.get_next();   
   }

   _close(file); 
   
   return GS_GOOD;
}


/*********************************************************/
/*  FINE FUNZIONI PER LE CLASSI DI ENTITA'               */
/*  INIZIO FUNZIONI PER LE TABELLE SECONDARIE            */
/*********************************************************/


/*************************************************************************/
/*.doc gsc_open_usrSec                                                   */
/*+
   Apre il file abilitazione utente alle tabelle secondarie contenuto nella sub-directory
   USR del direttorio del progetto, il cui codice e' passato come parametro.
   La modalità di apertura è in lettura o in scrittura a seconda di <mode>
   (READONLY o UPDATEABLE).
   Legge direttorio del progetto dalla lista GEOsimAppl::PROJECTS.
   Se l'utente corrente e' SUPERUSER vedra' tutti le tabelle secondarie disponibili.
   Se e' solo USER vedra solo le tabelle secondarie a lui abilitate.
   In lettura controlla che il codice utente corrisponda a quello scritto 
   sul file. In scrittura crea il file (se non esiste), lo cancella se
   esiste e vi ricopia il codice dell'utente.
   Parametri:
   int usr;    codice utente
   int prj;    codice progetto
   int mode;   modalità di apertura (READONLY o UPDATEABLE)

   Ritorna l'identificatore del file o -1 in caso di errore o di codice 
   utente non corrispondente.
-*/
/*************************************************************************/
int gsc_open_usrSec(int usr, int prj, int mode)
{
   C_STRING path;
   int      file;
   
   // Legge direttorio del progetto dalla lista GEOsimAppl::PROJECTS
   // Se utente corrente e' SUPERUSER vedra' tutti i progetti disponibili
   // Se e' solo USER vedra solo i progetti a lui abilitati
   path = GEOsimAppl::PROJECTS.get_dir(prj); 
   path += _T('\\');
   path += GEOUSRDIR;
   path += _T("\\usr_");
   path += usr;
   path += _T(".sec");

   if (gsc_nethost2drive(path) == GS_BAD) return -1;
   
   if (mode == READONLY) // APRE FILE IN LETTURA, SE NON ESISTE RITORNA -1
   {
      if ((file = gsc_sopen(path.get_name(), _O_RDONLY | _O_BINARY, _SH_DENYRW, -1, ONETEST)) != -1)
      {  // CONTROLLA CHE I CODICI UTENTE E PROGETTO SIANO CORRETTI
         if (gsc_byte2int(file, 4, 0) != usr)
            { _close(file); GS_ERR_COD = eGSReadFile; return -1; }
         if (gsc_byte2int(file, 4, 4) != prj)
            { _close(file); GS_ERR_COD = eGSReadFile; return -1; }
      }
   }
   else
   if (mode == UPDATEABLE) // APRE FILE IN SCRITTURA, SE NON ESISTE LO CREA
   {              // CANCELLA IL CONTENUTO E VI SCRIVE I CODICI UTENTE E PROGETTO
      if ((file = gsc_sopen(path.get_name(), _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _SH_DENYRW,
                            _S_IREAD | _S_IWRITE)) == -1)
         return -1;
      if (_chsize(file,0) != 0 || 
          gsc_int2byte(file, usr, 4, 0) == GS_BAD ||
          gsc_int2byte(file, prj, 4, 4) == GS_BAD )
         { _close(file); return -1; }
   }
   
   return file;
}


/*************************************************************************/
/*.doc gsc_getPersonalSecPermissions                                     */
/*+
   Ritorna una lista contenente i codici delle tabelle secondarie a cui l'utente
   puo'accedere e il livello di visibilita corripondente secondo:
   Permesso = (0->invisibile) (1->sola lettura) (2->modificabile).

   Uso una C_4INT_STR in cui:   
   set_key() e get_key()           gestiscono il codice della classe
   set_type() e get_type()         gestiscono il codice della sotto-classe
   set_category() e get_category() gestiscono il codice della tabella secondaria
   set_level() e get_level()       gestiscono l'abilitazione della tabella secondaria

   Tali abilitazioni sono salvate nel file corrispondente all'utente nel 
   sottodirettirio USR del direttorio del progetto.
   Parametri:
   int usr;                codice utente        (input)
   int prj;                codice progetto      (input)
   C_4INT_STR_LIST *out;   lista delle abilitazioni

   Ritorna GS_BAD in caso di errore o se <usr> corrisponde ad un
   SUPERUSER altrimenti GS_GOOD.
-*/
/*************************************************************************/
int gsc_getPersonalSecPermissions(int usr, int prj, C_4INT_STR_LIST *out)
{
   C_4INT_STR *punt;
   int        file;
   int        cls, sub, sec, vis;
   long       cursor = 8; // Numero byte di intestazione file (codice usr e prj)
   bool       err = false;

   out->remove_all();

   if (gsc_superuser(usr) != GS_BAD)                  // Utente non esiste
      { GS_ERR_COD = eGSInvalidUser; return GS_BAD; } // o e' superuser

   // Se ha problemi ad aprire il file ritorna nessuna abilitazione
   if ((file = gsc_open_usrSec(usr, prj, READONLY)) == -1) return GS_GOOD;
                                        
   while ((cls = gsc_byte2int(file, 2, cursor)) != -1)  // Legge codice classe
   {
      cursor += 2;
      // Legge codice sottoclasse classe
      if ((sub = gsc_byte2int(file, 2, cursor)) == -1)   
         { err = true; break; }           
      cursor += 2;
      // Legge codice tabella secondaria
      if ((sec = gsc_byte2int(file, 2, cursor)) == -1)   
         { err = true; break; }           
      cursor += 2;
      // Legge visibilita' tabella secondaria
      if ((vis = gsc_byte2int(file, 1, cursor)) == -1)   
         { err = true; break; }           
      cursor++;
      // Codice classe e sottoclasse non valido 
      if (cls < 1 || sub < 0 || sec < 1)
         { err = true; GS_ERR_COD = eGSInvClassCode; break; }           

      // Codice di abilitazione non valido
      if (vis != GSInvisibleData && vis != GSReadOnlyData && vis != GSUpdateableData)    
         { err = true; GS_ERR_COD = eGSInvalidLevel; break; }           

      punt = new C_4INT_STR;
      out->add_tail(punt);

      punt->set_key(cls);       // codice della classe
      punt->set_type(sub);      // codice della sotto-classe
      punt->set_category(sec);  // codice della tabella secondaria
      punt->set_level((GSDataPermissionTypeEnum) vis);     // abilitazione della tabella secondaria
   }

   _close(file); 

   if (err)
   {
      out->remove_all();
      return GS_BAD; 
   }

   return GS_GOOD;
}



/*************************************************************************/
/*.doc gsc_getInheritanceSecPermissions                                  */
/*+
   Ritorna una lista contenente i codici delle tabelle secondarie a cui l'utente
   puo'accedere e il livello di visibilita corripondente secondo :
   Permesso = (0->invisibile) (1->sola lettura) (2->modificabile).

   Uso una C_4INT_STR in cui:   
   set_key() e get_key()           gestiscono il codice della classe
   set_type() e get_type()         gestiscono il codice della sotto-classe
   set_category() e get_category() gestiscono il codice della tabella secondaria
   set_level() e get_level()       gestiscono l'abilitazione della tabella secondaria

   Tali abilitazioni sono quelle derivate da altri utenti.
   Se più utenti abiilitano la stessa tabella secondaria in modo differente verrà considerata
   l'abilitazione meno restrittiva (es. se utente A -> GSReadOnlyData, 
   utente B -> GSUpdateableData l'abilitazione considerata è GSUpdateableData)
   Parametri:
   int usr;                      codice utente        (input)
   int prj;                      codice progetto      (input)
   C_4INT_STR_LIST &out;         lista delle abilitazioni
   C_STR_LIST *pLoadedUserNames; Uso interno (default = NULL)

   Ritorna GS_BAD in caso di errore altrimenti GS_GOOD.
-*/
/*************************************************************************/
int gsc_getInheritanceSecPermissions(int usr, int prj, C_4INT_STR_LIST &out, C_STR_LIST *pLoadedUserNames)
{
   C_STR_LIST      UserNames;
   C_STR           *pUserName;
   C_INT_INT_STR   UsrInfo;
   C_4INT_STR_LIST Partial;
   C_4INT_STR      *pPermission, *pPartialPermission;

   out.remove_all();
   if (gsc_loadInheritanceUsers(usr, UserNames) == GS_BAD) return GS_GOOD;

   pUserName = (C_STR *) UserNames.get_head();
   while (pUserName)
   {  // utente già caricato (evita un ciclo infinito: A eredita da B che eredita da A)
      if (pLoadedUserNames && pLoadedUserNames->search_name(pUserName->get_name()))
         pUserName = (C_STR *) UserNames.get_next();

      if (gsc_chkusr(pUserName->get_name(), &UsrInfo) == GS_GOOD &&
          gsc_getSecPermissions(UsrInfo.get_key(), prj, Partial) == GS_GOOD)
      {
         if (!pLoadedUserNames) pLoadedUserNames = new C_STR_LIST();
         pLoadedUserNames->add_tail_str(pUserName->get_name());

         pPartialPermission = (C_4INT_STR *) Partial.get_head();
         while (pPartialPermission)
         {  // tabella secondaria già presente
            if ((pPermission = (C_4INT_STR *) out.search(pPartialPermission->get_key(),        // classe
                                                         pPartialPermission->get_type(),       // sottoclasse
                                                         pPartialPermission->get_category()))) // tabella secondaria
            {  // adotto il permesso meno restrittivo
               if (pPermission->get_level() < pPartialPermission->get_level())
                  pPermission->set_level(pPartialPermission->get_level());
            }
            else
            {
               C_4INT_STR *punt = new C_4INT_STR;

               out.add_tail(punt);

               punt->set_key(pPartialPermission->get_key());           // codice della classe
               punt->set_type(pPartialPermission->get_type());         // codice della sotto-classe
               punt->set_category(pPartialPermission->get_category()); // codice della tabella secondaria
               punt->set_level(pPartialPermission->get_level());              // abilitazione della tabella secondaria
            }

            pPartialPermission = (C_4INT_STR *) Partial.get_next();
         }
      }
      pUserName = (C_STR *) UserNames.get_next();
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_setPersonalSecPermissions                                     */
/*+
   (solo per SUPERUSER)
   Riceve codice utente e progetto ed una lista contenente i codici 
   delle tabelle secondariei a cui si vuole abilitare l'accesso e il tipo di visibilita'.
   Uso una C_4INT_STR in cui:   
   set_key() e get_key()           gestiscono il codice della classe
   set_type() e get_type()         gestiscono il codice della sotto-classe
   set_category() e get_category() gestiscono il codice della tabella secondaria
   set_level() e get_level()       gestiscono l'abilitazione della tabella secondaria

   Legge direttorio del progetto dalla lista GEOsimAppl::PROJECTS.
   Tali abilitazioni sono salvate nel file corispondente all'utente nel 
   sottodirettirio USR del direttorio del progetto.
   Parametri:
   int usr;                codice utente        (input)
   int prj;                codice progetto      (input)
   C_4INT_STR_LIST &in;    lista delle abilitazioni

   Ritorna GS_BAD se l'utente corrente non e' SUPERUSER, nel caso il codice
   dell'utente che si vuole abilitare non sia valido o corrisponda ad un
   SUPERUSER, oppure in caso di errore. GS_GOOD altrimenti.
-*/
/*************************************************************************/
int gsc_setPersonalSecPermissions(int usr, int prj, C_4INT_STR_LIST &in)
{
   C_INT_INT_LIST  prjPermissionList, clsPermissionList;
   C_INT_INT       *pClsPermission;
   C_4INT_STR_LIST new_sec_list, old_sec_list;
   C_4INT_STR      *punt, *pOldSec;
   int             file, cls, sub, sec;
   long            SessionCode, cursor = 8; // Numero byte di intestazione file (codice usr e prj)
   C_CLASS         *pCls;
   C_INT_INT_STR_LIST SinthTabSecList;
   GSDataPermissionTypeEnum clsPermission, secPermission;

   // Operazione solo per superuser
   if (gsc_check_op(opModUsr) == GS_BAD) return GS_BAD;

   if (in.copy(&new_sec_list) == GS_BAD) return GS_BAD;

   // Legge i permessi delle classi del progetto
   if (gsc_getPersonalClassPermissions(usr, prj, &clsPermissionList) == GS_BAD) return GS_BAD;

   // Legge la situazione delle tabelle secondarie attuale
   if (gsc_getPersonalSecPermissions(usr, prj, &old_sec_list) == GS_BAD) return GS_BAD;

   // controllo che le classi non siano in uso
   punt = (C_4INT_STR *) old_sec_list.get_head();
   while (punt)
   {
      cls = punt->get_key();
      sub = punt->get_type();
      sec = punt->get_category();
      if (new_sec_list.search(cls, sub, sec) == NULL) // tabella secondaria non più settata
         // verifico se la classe fa parte di una sessione di lavoro dell 'utente
         if (gsc_is_inarea(prj, cls, &SessionCode, &usr) == GS_BAD || SessionCode > 0)
            { GS_ERR_COD = eGSSessionsFound; return GS_BAD; }

      punt = (C_4INT_STR *) old_sec_list.get_next();
   }

   // CONTROLLA CORRETTEZZA DEI PARAMETRI
   punt = (C_4INT_STR *) new_sec_list.get_head();
   while (punt)
   {
      cls           = punt->get_key();
      sub           = punt->get_type();
      sec           = punt->get_category();
      secPermission = punt->get_level();
      pOldSec       = (C_4INT_STR *) old_sec_list.search(cls, sub, sec);

      // cerca la secondaria 
      if ((pCls = gsc_find_class(prj, cls, sub)) == NULL ||
          pCls->rtab_sec(SinthTabSecList) == GS_BAD || 
          SinthTabSecList.search_key(sec) == NULL)
      {
         new_sec_list.remove_at(); // lo elimino dalla lista
         punt = (C_4INT_STR *) new_sec_list.get_cursor();
         continue;
      }

      if ((pClsPermission = (C_INT_INT *) clsPermissionList.search_key(cls)))
         clsPermission = (GSDataPermissionTypeEnum) pClsPermission->get_type();
      else
         clsPermission = GSNonePermission;

      if (clsPermission == GSNonePermission) // nessun permesso esplicito per la classe (ereditato da altro utente)
      {
         if (secPermission != GSNonePermission) { GS_ERR_COD = eGSInvalidLevel; return GS_BAD; }       
      }
      else
         // Livello di abilitazione superiore a quello della classe
         if (secPermission > clsPermission) { GS_ERR_COD = eGSInvalidLevel; return GS_BAD; }       

      switch (secPermission)
      {
         case GSUpdateableData:
            punt = (C_4INT_STR *) new_sec_list.get_next();
            break;

         case GSReadOnlyData:
            // Se prima la tabella secondaria era modificabile ed ora è in sola lettura
            if (pOldSec && pOldSec->get_type() == (int) GSUpdateableData)
               // verifico se la classe fa parte di una sessione di lavoro dell'utente
               if (gsc_is_inarea(prj, cls, &SessionCode, &usr) == GS_BAD || SessionCode > 0)
                  { GS_ERR_COD = eGSSessionsFound; return GS_BAD; }
            
            punt = (C_4INT_STR *) new_sec_list.get_next();
            break;

         case GSInvisibleData:
            // Se prima la tabella secondaria era modificabile o in sola lettura ed ora è invisibile
            if (pOldSec && 
                (pOldSec->get_type() == (int) GSUpdateableData || pOldSec->get_type() == (int) GSReadOnlyData))
               // verifico se la classe fa parte di una sessione di lavoro dell'utente
               if (gsc_is_inarea(prj, cls, &SessionCode, &usr) == GS_BAD || SessionCode > 0)
                  { GS_ERR_COD = eGSSessionsFound; return GS_BAD; }
            
            punt = (C_4INT_STR *) new_sec_list.get_next();
            break;

         case GSNonePermission:
            // Se ora la tabella secondaria ha i permessi ereditati

            // verifico se la classe fa parte di una sessione di lavoro dell'utente
            if (gsc_is_inarea(prj, cls, &SessionCode, &usr) == GS_BAD || SessionCode > 0)
               { GS_ERR_COD = eGSSessionsFound; return GS_BAD; }

            // le tabelle secondarie non settate vengono eliminate dalla lista
            new_sec_list.remove_at();
            punt = (C_4INT_STR *) new_sec_list.get_cursor();
            break;
      }
   }

   // SCRIVE SU FILE PARAMETRI CORRETTI

   // Legge direttorio del progetto dalla lista GEOsimAppl::PROJECTS
   // Apre file e cancella vecchia configurazione
   if ((file = gsc_open_usrSec(usr, prj, UPDATEABLE)) == -1) return GS_BAD;

   punt = (C_4INT_STR *) new_sec_list.get_head();
   while (punt)
   {
      cls           = punt->get_key();
      sub           = punt->get_type();
      sec           = punt->get_category();
      secPermission = punt->get_level();

      if (gsc_int2byte(file, cls, 2, cursor) == GS_BAD)
         { _close(file); return GS_BAD; }
      cursor += 2;
      if (gsc_int2byte(file, sub, 2, cursor) == GS_BAD)
         { _close(file); return GS_BAD; }
      cursor += 2;
      if (gsc_int2byte(file, sec, 2, cursor) == GS_BAD)
         { _close(file); return GS_BAD; }
      cursor += 2;
      if (gsc_int2byte(file, (int) secPermission, 1, cursor) == GS_BAD)
         { _close(file); return GS_BAD; }
      cursor++;

      punt = (C_4INT_STR*) new_sec_list.get_next();   
   }

   _close(file); 
   
   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_getSecPermissions                                             */
/*+
   Ritorna una lista contenente i codici delle tabelle secondarie a cui l'utente
   puo'accedere e il livello di visibilita corripondente secondo :
   Permesso = (0->invisibile) (1->sola lettura) (2->modificabile).

   Uso una C_4INT_STR in cui:   
   set_key() e get_key()           gestiscono il codice della classe
   set_type() e get_type()         gestiscono il codice della sotto-classe
   set_category() e get_category() gestiscono il codice della tabella secondaria
   set_level() e get_level()       gestiscono l'abilitazione della tabella secondaria

   Tali abilitazioni sono la somma tra quelle impostate per l'utente e
   quelle derivate da altri utenti. Solo le classi non settate per l'utente
   ereditano i permessi di altri utenti.
   Parametri:
   int usr;                codice utente        (input)
   int prj;                codice progetto      (input)
   C_4INT_STR_LIST &out;   lista delle abilitazioni
   
   Ritorna GS_BAD in caso di errore o se <usr> corrisponde ad un
   SUPERUSER altrimenti GS_GOOD.
-*/
/*************************************************************************/
int gsc_getSecPermissions(int usr, int prj, C_4INT_STR_LIST &out)
{
   C_INT_INT_LIST  clsPermissionList;
   C_INT_INT       *pClsPermission;
   C_4INT_STR_LIST InheritancePermissionList;
   C_4INT_STR      *pInheritancePermission, *punt;
   int             cls, intPermission;

   // Carico le abilitazioni delle classi per l'utente
   if (gsc_getClassPermissions(usr, prj, clsPermissionList) == GS_BAD) return GS_BAD;

   if (gsc_getPersonalSecPermissions(usr, prj, &out) == GS_BAD) return GS_BAD;
   // Carico le abilitazioni ereditate da altri utenti
   if (gsc_getInheritanceSecPermissions(usr, prj, InheritancePermissionList) == GS_BAD) return GS_BAD;

   pInheritancePermission = (C_4INT_STR *) InheritancePermissionList.get_head();
   while (pInheritancePermission)
   {
      cls = pInheritancePermission->get_key(); // classe
      // Se il permesso di questa tabella secondaria non era stato settato
      if (out.search(cls,
                     pInheritancePermission->get_type(), // sottoclasse
                     pInheritancePermission->get_category()) == NULL) // tabella secondaria
      {
         if ((pClsPermission = (C_INT_INT *) clsPermissionList.search_key(cls)))
            // considero quella più restrittiva tra quella della classe e quella ereditata
            intPermission = min(pClsPermission->get_type(), pInheritancePermission->get_level());
         else
            intPermission = pInheritancePermission->get_level(); // considero quella ereditata

         punt = new C_4INT_STR;
         out.add_tail(punt);

         punt->set_key(cls);                                         // codice della classe
         punt->set_type(pInheritancePermission->get_type());         // codice della sotto-classe
         punt->set_category(pInheritancePermission->get_category()); // codice della tabella secondaria
         punt->set_level((GSDataPermissionTypeEnum) intPermission);  // abilitazione della tabella secondaria
      }

      pInheritancePermission = (C_4INT_STR *) InheritancePermissionList.get_next();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_delSecPermissions <internal> */
/*+
  Questa funzione cancella le abilitazioni alla tabella secondaria per tutti gli utenti.
  Parametri:
  int prj;        codice progetto
  int cls;        codice classe
  int sub;        codice sottoclasse
  int sec;        codice tabella secondaria

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_delSecPermissions(int prj, int cls, int sub, int sec)
{
   C_INT_INT_STR_LIST pusr_list;       // lista utenti
   C_INT_INT_STR      *pusr;
   C_4INT_STR_LIST    usr_listSec;  // lista abilitazioni a tabelle secondarie x utente

   // leggo lista degli utenti
   if (gsc_getusrlist(&pusr_list) == GS_GOOD) 
   {
      pusr = (C_INT_INT_STR *) pusr_list.get_head();  // inizio lista
      // per ciascun utente non SUPER-USER cancello la classe dal
      // suo file delle abilitazioni
      while (pusr)
      {
         if (pusr->get_type() != SUPERUSR) 
            // lista classi utente
            if (gsc_getSecPermissions(pusr->get_key(), prj , usr_listSec) == GS_GOOD)
            {
               if (usr_listSec.search(cls, sub, sec) != NULL)
                  if (usr_listSec.remove_at() == GS_GOOD)
                  {
                     if (gsc_setPersonalSecPermissions(pusr->get_key(), prj, usr_listSec) == GS_BAD)
                        return GS_BAD;
                  }
                  else
                     return GS_BAD;
            }
            else
               return GS_BAD;
         pusr = (C_INT_INT_STR *) pusr_list.get_next();  // elemento successivo  
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*  FINE FUNZIONI PER LE TABELLE SECONDARIE              */
/*********************************************************/


/*************************************************************************/
/*.doc gsc_saveInheritanceUsers                                          */
/*+
   Riceve codice utente e lista di nomi di utenti da cui ereditare le abilitazioni
   di classi e progetti che per l'utente <usr> non sono state settate.
   Parametri:
   int usr;                utente a cui settare l'eredità
   C_STR_LIST &UserNames;  Lista di nomi di utenti da cui ereditare le abilitazioni

   Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
*/
/*************************************************************************/
int gsc_saveInheritanceUsers(int usr, C_STR_LIST &UserNames)
{
   C_STRING path, Dir, DekriptFile;

   // Operazione solo per superuser
   if (gsc_check_op(opModUsr) == GS_BAD) return GS_BAD;
   
   path = GEOsimAppl::GEODIR;
   path += _T('\\');
   path += GEOUSRDIR;
   path += _T("\\usr_");
   path += usr;
   path += _T(".inh");

   Dir = GEOsimAppl::CURRUSRDIR;
   Dir += _T('\\');
   Dir += GEOTEMPDIR;
   if (gsc_get_tmp_filename(Dir.get_name(), _T("DEKRIPT"), _T(".inh"), DekriptFile) == GS_BAD)
      return GS_BAD;

   if (UserNames.save(DekriptFile.get_name()) == GS_BAD)
      { gsc_delfile(DekriptFile, ONETEST); return GS_BAD; }

   if (gsc_KriptFile(DekriptFile, path) == GS_BAD) return GS_BAD;
   
   gsc_delfile(DekriptFile, ONETEST);

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_loadInheritanceUsers                                          */
/*+
  Riceve codice utente e lista di nomi di utenti da cui ereditare le abilitazioni
  di classi e progetti che per l'utente <usr> non sono state settate.
  Parametri:
  int usr;                utente a cui settare l'eredità
  C_STR_LIST &UserNames;  Lista di nomi di utenti da cui ereditare le abilitazioni
  int file;               Opzionale, il descrittore al file già aperto.
                          Se = NULL la procedura provvede ad aprire il file
                          delle password in sola lettura (default = NULL).

   Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
*/
/*************************************************************************/
int gsc_loadInheritanceUsers(int usr, C_STR_LIST &UserNames, int file)
{
   C_STRING path, Dir, DekriptFile;

   if (gsc_superuser(usr, file) == GS_GOOD) //  e' superuser
      { UserNames.remove_all(); return GS_GOOD; }
  
   path = GEOsimAppl::GEODIR;
   path += _T('\\');
   path += GEOUSRDIR;
   path += _T("\\usr_");
   path += usr;
   path += _T(".inh");
   if (gsc_path_exist(path) != GS_GOOD)
      { UserNames.remove_all(); return GS_GOOD; }

   Dir = GEOsimAppl::CURRUSRDIR;
   Dir += _T('\\');
   Dir += GEOTEMPDIR;
   if (gsc_get_tmp_filename(Dir.get_name(), _T("DEKRIPT"), _T(".inh"), DekriptFile) == GS_BAD)
      return GS_BAD;
   if (gsc_DeKriptFile(path, DekriptFile) == GS_BAD) return GS_BAD;

   if (UserNames.load(DekriptFile.get_name()) == GS_BAD)
      { gsc_delfile(DekriptFile, ONETEST); return GS_BAD; }
   
   gsc_delfile(DekriptFile, ONETEST);

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_getInheritancePrjPermissions                                  */
/*+
   Ritorna una lista contenente i codici deli progetti a cui l'utente
   puo'accedere e il livello di visibilita corripondente secondo :
   ...(<codice-progetto> <permesso>) (<codice-progetto> <permesso>)..
   Tali abilitazioni sono quelle derivate da altri utenti.
   Se più utenti abiilitano lo stesso progetto in modo differente verrà considerata
   l'abilitazione meno restrittiva (es. se utente A -> GSReadOnlyData, 
   utente B -> GSUpdateableData l'abilitazione considerata è GSUpdateableData)
   Parametri:
   int usr;                      codice utente        (input)
   C_INT_INT_LIST *out;          lista  ((progetto permesso)...)
   C_STR_LIST *pLoadedUserNames; Uso interno (default = NULL)

   Ritorna GS_BAD in caso di errore altrimenti GS_GOOD.
-*/
/*************************************************************************/
int gsc_getInheritancePrjPermissions(int usr, C_INT_INT_LIST &out, C_STR_LIST *pLoadedUserNames)
{
   C_STR_LIST    UserNames;
   C_STR         *pUserName;
   C_INT_INT_STR UsrInfo;
   C_INT_INT_LIST Partial;
   C_INT_INT      *pPermission, *pPartialPermission;

   out.remove_all();
   if (gsc_loadInheritanceUsers(usr, UserNames) == GS_BAD) return GS_GOOD;

   pUserName = (C_STR *) UserNames.get_head();
   while (pUserName)
   {  // utente già caricato (evita un ciclo infinito: A eredita da B che eredita da A)
      if (pLoadedUserNames && pLoadedUserNames->search_name(pUserName->get_name()))
         pUserName = (C_STR *) UserNames.get_next();

      if (gsc_chkusr(pUserName->get_name(), &UsrInfo) == GS_GOOD &&
          gsc_getPrjPermissions(UsrInfo.get_key(), Partial) == GS_GOOD)
      {
         if (!pLoadedUserNames) pLoadedUserNames = new C_STR_LIST();
         pLoadedUserNames->add_tail_str(pUserName->get_name());

         pPartialPermission = (C_INT_INT *) Partial.get_head();
         while (pPartialPermission)
         {
            if ((pPermission = (C_INT_INT *) out.search_key(pPartialPermission->get_key()))) // progetto già presente
            {  // adotto il permesso meno restrittivo
               if (pPermission->get_type() < pPartialPermission->get_type())
                  pPermission->set_type(pPartialPermission->get_type());
            }
            else
               out.values_add_tail(pPartialPermission->get_key(), pPartialPermission->get_type());

            pPartialPermission = (C_INT_INT *) Partial.get_next();
         }
      }
      pUserName = (C_STR *) UserNames.get_next();
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_getInheritanceClsPermissions                                  */
/*+
   Ritorna una lista contenente i codici delle classi a cui l'utente
   puo'accedere e il livello di visibilita corripondente secondo :
   ...(<codice-classe> <permesso>) (<codice-classe> <permesso>)..
   Tali abilitazioni sono quelle derivate da altri utenti.
   Se più utenti abiilitano la stessa classe in modo differente verrà considerata
   l'abilitazione meno restrittiva (es. se utente A -> GSReadOnlyData, 
   utente B -> GSUpdateableData l'abilitazione considerata è GSUpdateableData)
   Parametri:
   int usr;                      codice utente        (input)
   int prj;                      codice progetto      (input)
   C_INT_INT_LIST *out;          lista  ((classe permesso)...)
   C_STR_LIST *pLoadedUserNames; Uso interno (default = NULL)

   Ritorna GS_BAD in caso di errore altrimenti GS_GOOD.
-*/
/*************************************************************************/
int gsc_getInheritanceClsPermissions(int usr, int prj, C_INT_INT_LIST &out, C_STR_LIST *pLoadedUserNames)
{
   C_STR_LIST    UserNames;
   C_STR         *pUserName;
   C_INT_INT_STR UsrInfo;
   C_INT_INT_LIST Partial;
   C_INT_INT      *pPermission, *pPartialPermission;

   out.remove_all();
   if (gsc_loadInheritanceUsers(usr, UserNames) == GS_BAD) return GS_GOOD;

   pUserName = (C_STR *) UserNames.get_head();
   while (pUserName)
   {  // utente già caricato (evita un ciclo infinito: A eredita da B che eredita da A)
      if (pLoadedUserNames && pLoadedUserNames->search_name(pUserName->get_name()))
         pUserName = (C_STR *) UserNames.get_next();

      if (gsc_chkusr(pUserName->get_name(), &UsrInfo) == GS_GOOD &&
          gsc_getClassPermissions(UsrInfo.get_key(), prj, Partial) == GS_GOOD)
      {
         if (!pLoadedUserNames) pLoadedUserNames = new C_STR_LIST();
         pLoadedUserNames->add_tail_str(pUserName->get_name());

         pPartialPermission = (C_INT_INT *) Partial.get_head();
         while (pPartialPermission)
         {
            if ((pPermission = (C_INT_INT *) out.search_key(pPartialPermission->get_key()))) // classe già presente
            {  // adotto il permesso meno restrittivo
               if (pPermission->get_type() < pPartialPermission->get_type())
                  pPermission->set_type(pPartialPermission->get_type());
            }
            else
               out.values_add_tail(pPartialPermission->get_key(), pPartialPermission->get_type());

            pPartialPermission = (C_INT_INT *) Partial.get_next();
         }
      }
      pUserName = (C_STR *) UserNames.get_next();
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_getPrjPermissions                                             */
/*+
   Ritorna una lista contenente i codici dei progetti a cui l'utente
   puo'accedere e il livello di visibilita corripondente secondo :
   ...(<codice-classe> <Permesso>) (<codice-classe> <Permesso>)..
   NB: Permesso = (0->invisibile) (1->sola lettura) (2->modificabile).
   Tali abilitazioni sono la somma tra quelle impostate per l'utente e
   quelle derivate da altri utenti. Solo i progetti non settati per l'utente
   ereditano i permessi di altri utenti.
   Parametri:
   int usr;                codice utente        (input)
   C_INT_INT_LIST *out;    lista  ((progetto Permesso)...)

   Ritorna GS_BAD in caso di errore o se <usr> corrisponde ad un
   SUPERUSER altrimenti GS_GOOD.
-*/
/*************************************************************************/
int gsc_getPrjPermissions(int usr, C_INT_INT_LIST &out)
{
   C_INT_INT_LIST InheritancePermissions;
   C_INT_INT      *pPermission, *pInheritancePermission;

   // Carico le abilitazioni personali dell'utente
   if (gsc_getPersonalPrjPermissions(usr, &out) == GS_BAD) return GS_BAD;
   // Carico le abilitazioni ereditate da altri utenti
   if (gsc_getInheritancePrjPermissions(usr, InheritancePermissions) == GS_BAD) return GS_BAD;

   pInheritancePermission = (C_INT_INT *) InheritancePermissions.get_head();
   while (pInheritancePermission)
   {
      // Se il permesso di questo progetto non era stato settato
      if ((pPermission = (C_INT_INT *) out.search_key(pInheritancePermission->get_key())) == NULL)
         out.values_add_tail(pInheritancePermission->get_key(), pInheritancePermission->get_type());

      pInheritancePermission = (C_INT_INT *) InheritancePermissions.get_next();
   }

   return GS_GOOD;
}

/*********************************************************/
/*.doc gsc_delPrjPermissions <internal> */
/*+
  Questa funzione cancella le abilitazioni al progetto per tutti gli utenti.
  Parametri:
  int prj;        codice progetto

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_delPrjPermissions(int prj)
{
   C_INT_INT_STR_LIST pusr_list;    // lista utenti
   C_INT_INT_STR      *pusr;
   C_INT_INT_LIST     usr_listPrj;  // lista progetti utente

   // leggo lista degli utenti
   if (gsc_getusrlist(&pusr_list) == GS_GOOD) 
   {
      pusr = (C_INT_INT_STR *) pusr_list.get_head();  // inizio lista
      // per ciascun utente non SUPER-USER cancello la classe dal
      // suo file delle abilitazioni
      while (pusr)
      {
         if (pusr->get_type() != SUPERUSR) 
            // lista progetti utente
            if (gsc_getPrjPermissions(pusr->get_key(), usr_listPrj) == GS_GOOD)
            {
               if (usr_listPrj.search_key(prj) != NULL)
                  if (usr_listPrj.remove_at() == GS_GOOD)
                  {
                     if (gsc_setPersonalPrjPermissions(pusr->get_key(), usr_listPrj) == GS_BAD)
                        return GS_BAD;
                  }
                  else
                     return GS_BAD;
            }
            else
               return GS_BAD;
         pusr = (C_INT_INT_STR *) pusr_list.get_next();  // elemento successivo  
      }
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_getClassPermissions                                           */
/*+
   Ritorna una lista contenente i codici delle classi a cui l'utente
   puo'accedere e il livello di visibilita corripondente secondo :
   ...(<codice-classe> <Permesso>) (<codice-classe> <Permesso>)..
   NB: Permesso = (0->invisibile) (1->sola lettura) (2->modificabile).
   Tali abilitazioni sono la somma tra quelle impostate per l'utente e
   quelle derivate da altri utenti. Solo le classi non settate per l'utente
   ereditano i permessi di altri utenti.
   Parametri:
   int usr;                codice utente        (input)
   int prj;                codice progetto      (input)
   C_INT_INT_LIST *out;    lista  ((classe Permesso)...)

   Ritorna GS_BAD in caso di errore o se <usr> corrisponde ad un
   SUPERUSER altrimenti GS_GOOD.
-*/
/*************************************************************************/
int gsc_getClassPermissions(int usr, int prj, C_INT_INT_LIST &out)
{
   C_INT_INT_LIST UsrPrjPermission, InheritancePermissions;
   C_INT_INT      *pPermission, *pInheritancePermission;
   GSDataPermissionTypeEnum PrjPermission = GSNonePermission; // non settato
   int                      intPermission;

   // Carico le abilitazioni personali dell'utente
   if (gsc_getPrjPermissions(usr, UsrPrjPermission) == GS_BAD) return GS_BAD;

   if ((pPermission = (C_INT_INT *) UsrPrjPermission.search_key(prj)))
      PrjPermission = (GSDataPermissionTypeEnum) pPermission->get_type();

   if (gsc_getPersonalClassPermissions(usr, prj, &out) == GS_BAD) return GS_BAD;
   // Carico le abilitazioni ereditate da altri utenti
   if (gsc_getInheritanceClsPermissions(usr, prj, InheritancePermissions) == GS_BAD) return GS_BAD;

   pInheritancePermission = (C_INT_INT *) InheritancePermissions.get_head();
   while (pInheritancePermission)
   {
      // Se il permesso di questa classe non era stato settato
      if ((pPermission = (C_INT_INT *) out.search_key(pInheritancePermission->get_key())) == NULL)
      {  // se la classe non aveva abilitazione settate       
         if (PrjPermission == GSNonePermission)
            intPermission = pInheritancePermission->get_type(); // considero quella ereditata
         else  // considero quella più restrittiva tra quella del progetto e quella ereditata
            intPermission = min((int) PrjPermission, pInheritancePermission->get_type());

         out.values_add_tail(pInheritancePermission->get_key(), intPermission);
      }

      pInheritancePermission = (C_INT_INT *) InheritancePermissions.get_next();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_delClassPermissions <internal> */
/*+
  Questa funzione cancella le abilitazioni alla classe per tutti gli utenti.
  Parametri:
  int prj;        codice progetto
  int cls;        codice classe

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_delClassPermissions(int prj, int cls)
{
   C_INT_INT_STR_LIST pusr_list;       // lista utenti
   C_INT_INT_STR      *pusr;
   C_INT_INT_LIST     usr_listclass;  // lista progetti utente

   // leggo lista degli utenti
   if (gsc_getusrlist(&pusr_list) == GS_GOOD) 
   {
      pusr = (C_INT_INT_STR *) pusr_list.get_head();  // inizio lista
      // per ciascun utente non SUPER-USER cancello la classe dal
      // suo file delle abilitazioni
      while (pusr)
      {
         if (pusr->get_type() != SUPERUSR) 
            // lista classi utente
            if (gsc_getClassPermissions(pusr->get_key(), prj , usr_listclass) == GS_GOOD)
            {
               if (usr_listclass.search_key(cls) != NULL)
                  if (usr_listclass.remove_at() == GS_GOOD)
                  {
                     if (gsc_setPersonalClassPermissions(pusr->get_key(), prj, usr_listclass) == GS_BAD)
                        return GS_BAD;
                  }
                  else
                     return GS_BAD;
            }
            else
               return GS_BAD;
         pusr = (C_INT_INT_STR *) pusr_list.get_next();  // elemento successivo  
      }
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_chkClsPermissions                                  <internal> */
/*+
  Controlla 
  Parametri:
  C_NODE *pPrj;         Puntatore a progetto
  C_INT_INT_LIST &vis;  Lista delle abilitazioni delle classi
  int flag;             Nel caso di abilitazioni errate, se = 1 lista viene corretta,
                        se = 0 ritorna errore

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/*************************************************************************/
int gsc_chkClsPermissions(C_NODE *pPrj, C_INT_INT_LIST &vis, int flag)
{
   C_FAMILY_LIST family;
   C_FAMILY      *list;
   C_INT_INT     *punt;
   C_INT         *son;
   int           cls, tot;
   GSDataPermissionTypeEnum level;
   
   if (gsc_getfamily(pPrj, &family) == GS_BAD) return GS_BAD;

   // TROVA LIVELLO PIU' BASSO NELLA FAMIGLIA
   list = ( C_FAMILY *) family.get_head();
   while (list)
   {
      tot = GSUpdateableData;
      son = (C_INT *) list->relation.get_head();
      while (son)
      {
         cls = son->get_key();

         if ((punt = (C_INT_INT *) vis.search_key(cls)) == NULL) level = GSNonePermission;
         else level = (GSDataPermissionTypeEnum) punt->get_type();

         if (level < tot) tot = level;
         son = (C_INT *) list->relation.get_next();
      }
      list->set_key(tot);
      list = (C_FAMILY*) family.get_next();
   }

   // SETTA ABILITAZIONE CORRETTA
   punt = (C_INT_INT *) vis.get_head();
   while (punt)
   {
      cls   = punt->get_key();
      level = (GSDataPermissionTypeEnum) punt->get_type();
      if ((list = family.search_list(cls)) != NULL)
      {
         tot = list->get_key();
         if (flag == 1)
         {  // CORREGGE ERRORE
            if (tot == GSNonePermission) 
            { 
               vis.remove(cls, level);        // Sono gia' su next
               if ((punt = (C_INT_INT *) vis.get_cursor()) != NULL) continue;
               else break;
            } 
            else 
               if (level != tot) { punt->set_type(tot); }
         }
         else  // SE SBAGLIATO RITORNA ERRORE
            if (level != tot) { GS_ERR_COD = eGSInvalidLevel; return GS_BAD; }
      }
      punt = (C_INT_INT *) vis.get_next();
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_getprjcls(int,C_INT_LIST *out)  */
/*+
-*/
/*************************************************************************/
int gsc_getprjcls(int prj, C_INT_LIST *out)
{
   C_CLASS_LIST *cls_list;
   C_CLASS      *cls;
   C_ID         *id;
   C_INT        *new_nod;

   if (out==NULL) { GS_ERR_COD=eGSNotAllocVar; return GS_BAD; }
   if ((cls_list=GEOsimAppl::PROJECTS.get_classlist(prj))==NULL) return GS_BAD;

   out->remove_all();

   cls=(C_CLASS*)cls_list->get_head();
   while(cls!=NULL)
   {
      id=cls->ptr_id();
      if ((new_nod = new C_INT(id->code)) == NULL)
         { out->remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      out->add_tail(new_nod); 
      cls=(C_CLASS*)cls_list->get_next();
   }
   return GS_GOOD;   
}


/*************************************************************************/
/*.doc gsc_getfamily(int,C_FAMILY_LIST *out)  */
/*+
   La funzione riceve in ingresso il codice progetto ed il puntatore ad
   una lista di liste e vengono restituite le liste dei codici delle classi
   legate da un legame di parentela (attraverso classi gruppo).
   Ogni lista corrisponderà ad una famiglia di classi indipendente
   dalle altre. NB: Un utente deve avere il medesimo livello di visibilita'
   su tutte le classi di una famiglia.
-*/
/*************************************************************************/
int gsc_getfamily(C_NODE *pPrj, C_FAMILY_LIST *out)
{
   C_BIRELATION_LIST compl_list;
   C_BIRELATION      *compl;
   C_FAMILY          *family;
 
   if (!pPrj) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (!out) { GS_ERR_COD = eGSNotAllocVar; return GS_BAD; }

   if (((C_PROJECT *)pPrj)->get_group_list(&compl_list) == GS_BAD ) return GS_BAD;

   out->remove_all();

   compl=(C_BIRELATION*)compl_list.get_head();
   while(compl!=NULL)
   {
      family=new C_FAMILY;
      out->add_tail(family);

      family->set_key(compl->get_key()); // codice classe madre
      gsc_getchildren(compl->get_key(),&compl_list,family,out,0);

      compl=(C_BIRELATION*)compl_list.get_head();
   }
      
   return GS_GOOD;      
}


//*************************************************************************/
/*.doc gsc_setfamily                                                      */
/*+
   Poichè un utente deve avere il medesimo livello di visibilita'
   su tutte le classi di una famiglia:
   la funzione setta le famiglie appartenenti alla classe cod_class
   con l'abilitazione desiderata nella lista class_list.
   Parametri:
   C_NODE     *pPrj
   int         cod_class;
   int         abilitaz;
   C_INT_INT  *class_list;

   NB: Questa funzione non va per gruppi di grado maggiore di uno (gruppi di gruppi)
-*/
/*************************************************************************/
int gsc_setfamily(C_NODE *pPrj, int cod_class, int abilitaz, C_INT_INT_LIST *class_list)
{
   C_FAMILY      *family;
   C_FAMILY_LIST  family_list;
   C_INT         *son;
   int            code;

   if (class_list==NULL) { GS_ERR_COD=eGSNotAllocVar; return GS_BAD; }

   // ricavo lista delle famiglie
   if (gsc_getfamily(pPrj, &family_list) == GS_BAD) return GS_BAD;

   // primo ciclo per variare tutte le famiglie in cui compare cod_class
   family = (C_FAMILY *) family_list.get_head();
   while (family)
   {
      if (family->relation.search_key(cod_class)) // è in famiglia
      {  // setto i figli e il capofamiglia con abilitaz
         son = (C_INT *) family->relation.get_head();
         while (son)
         {
            code = son->get_key();
            if (class_list->search_key(code) == NULL)
            {  // la classe era invisibile , la aggiungo
               C_INT_INT *p;

               if ((p = new C_INT_INT) == NULL)
                  { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
               p->set_key(code);
               p->set_type(abilitaz);
               class_list->add_tail(p);
            }
            else class_list->get_cursor()->set_type(abilitaz);
            
            son = (C_INT *) family->relation.get_next();
         }
      }
      family = (C_FAMILY *) family_list.get_next();
   }
      
   return GS_GOOD;      
}


/*************************************************************************/
/*.doc gsc_getchildren(int,C_BIRELATION_LIST *in,C_INT_LIST *out)  */
/*+
-*/
/*************************************************************************/
int gsc_getchildren(int parent,C_BIRELATION_LIST *in,
                    C_FAMILY *family,C_FAMILY_LIST *out, int cont)
{
   C_BIRELATION *compl;
   C_FAMILY     *group;
   C_INT        *new_node,*other;
   C_INT_INT    *punt;
   int           father;

   if (cont>100) { acutPrintf(_T("\n Mi sono inlooppato \n")); exit(1); }
   // AGGIUNGO GENITORE NELLA LISTA DELLA FAMIGLIA SE NON VI E' GIA' //
   if (family->relation.search_key(parent)==NULL)
   {                  
      if ((new_node = new C_INT(parent)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      family->relation.add_tail(new_node);
   }
      
   if ((compl=(C_BIRELATION*)in->cut_key(parent))==NULL) return 1;
      // E' una foglia, devo controllare la presenza in altri gruppi // 

   punt=(C_INT_INT*)(compl->relation).get_head();    // primo figlio //
   while(punt!=NULL)
   {
      father=punt->get_key();

//  CONTROLLO EVENTULI LEGAMI DI PARENTELA NON DIRETTI //
      if (gsc_getchildren(father,in,family,out,cont++)==1)
         {               // Devo controllare la presenza della foglia in //
         group=(C_FAMILY*)out->get_head();             // altri gruppi //
         while(group!=NULL)
            {
            if (group!=family)
               {
               other=(C_INT*)group->relation.cut_key(father); // Estrai //
               if (other!=NULL)  // Linka le due famiglie in un unica //
                  {              // Eliminando i doppioni //
                  delete other;
                  while((other=(C_INT*)group->relation.cut_head())!=NULL)
                     {
                     if (family->relation.search_key(other->get_key())!=NULL)
                        delete other;    // Se e' gia' in family delete //
                     else                         // aggiungi in family //
                        family->relation.add_tail(other); 
                     }
                  out->remove(group);
                  if ((group=(C_FAMILY*)out->get_cursor())!=NULL) continue;
                  else break;
                  }
               }
            group=(C_FAMILY*)out->get_next();
            }
         }
      punt=(C_INT_INT*)(compl->relation).get_next(); // next figlio //
   }
   delete compl;

   return 0;
}


//-----------------------------------------------------------------------//
// C_LOG_HEAD //
//-----------------------------------------------------------------------//


/*************************************************************************/
/*.doc class C_LOG_HEAD  */
/*+
   Classe per contenere le informazioni circa le lunghezze dei vari campi
   dei record del file utenti GEOPWD (ogni record corrisponde ad un utente)
   ed altre informazioni contenute intesta al file GEOPWD.
   
   Campi struttura :  log_len   -> lungezza in byte della login.
                      pwd_len   ->    "    "   "    "   password.
                      mode_len  ->    "    "   " delle informazioni circa
                                il tipo di utente : mode[0] (superuser-user)
                      flags_len ->    "    "   "  dei flags abilitazioni
                                comandi GEOSIM.
                      tot_len   -> Lunghezza totale del record (somma delle
                                lunghezze precedenti).
                                
                      num_usr   -> Mumero degli utenti attualmente abilitati.
                      max_usr   -> Codice piu' alto di un utente abilitato.

  NB: Il codice di un utente corrisponde alla posizione del record nel file
      GEOPWD.          
  Le macro SUPERUSER in mode[0] indica un superutente.
  La macro FREEUSER  in log[0] indica che il record non e' utilizzato
  da alcun utente.
-*/
/*************************************************************************/


/*************************************************************************/
/*.doc C_LOG_HEAD::read(int file)  */
/*+
   Carica le informazioni di testa del file 'GEOpwd'. Riceve in ingresso il
   codice del file aperto come 'GEOpwd'. Setta i campi della variabile
   con i valori letti da file.
   Ritorna GS_BAD in caso di errore, GS_GOOD altrimenti.
-*/
/*************************************************************************/
int C_LOG_HEAD::read(int file)
{
   long cursor=0;

   if (_lseek(file,0,SEEK_SET)==-1) { GS_ERR_COD=eGSReadFile; return GS_BAD; }

   if((log_len  =gsc_byte2int(file,LHL1,cursor))==-1) return GS_BAD;
   cursor+=LHL1;
   if((pwd_len  =gsc_byte2int(file,LHL2,cursor))==-1) return GS_BAD;
   cursor+=LHL2;
   if((mode_len =gsc_byte2int(file,LHL3,cursor))==-1) return GS_BAD;
   cursor+=LHL3;
   if((flags_len=gsc_byte2int(file,LHL4,cursor))==-1) return GS_BAD;
   cursor+=LHL4;
   if((max_usr  =gsc_byte2int(file,LHL5,cursor))==-1) return GS_BAD;
   cursor+=LHL5;
   if((num_usr  =gsc_byte2int(file,LHL6,cursor))==-1) return GS_BAD;

   tot_len= log_len+pwd_len+mode_len+flags_len;
   return GS_GOOD;
}
/*************************************************************************/
/*.doc C_LOG_HEAD::write(int file)  */
/*+
   Salva i campi della variabile globale GS_LOG_HEAD in testa al file
   'GEOpwd'. Riceve in ingresso il codice del file aperto come 'GEOpwd'.
   Ritorna GS_BAD in caso di errore, GS_GOOD altrimenti.
-*/
/*************************************************************************/
int C_LOG_HEAD::write(int file)
{
   long cursor=0;

   if (_lseek(file,0,SEEK_SET) == -1) { GS_ERR_COD=eGSWriteFile; return GS_BAD; }

   if(gsc_int2byte(file,log_len,LHL1,cursor)  ==GS_BAD) return GS_BAD;
   cursor+=LHL1;
   if(gsc_int2byte(file,pwd_len,LHL2,cursor)  ==GS_BAD) return GS_BAD;
   cursor+=LHL2;
   if(gsc_int2byte(file,mode_len,LHL3,cursor) ==GS_BAD) return GS_BAD;
   cursor+=LHL3;
   if(gsc_int2byte(file,flags_len,LHL4,cursor)==GS_BAD) return GS_BAD;
   cursor+=LHL4;
   if(gsc_int2byte(file,max_usr,LHL5,cursor)  ==GS_BAD) return GS_BAD;
   cursor+=LHL5;
   if(gsc_int2byte(file,num_usr,LHL6,cursor)  ==GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*************************************************************************/
/*.doc C_LOG_HEAD::from_geopwd(void)  */
/*+
  Legge la testa del file "GEOPWD" e setta i campi della variabile.
  Parametri:
  int file;    Opzionale, il descrittore al file già aperto.
               Se = NULL la procedura provvede ad aprire il file
               delle password in sola lettura (default = NULL).

  Ritorna GS_BAD in caso di errore, GS_GOOD altrimenti.
-*/
/*************************************************************************/
int C_LOG_HEAD::from_geopwd(int file)
{
   int _file;

   if (file)
      _file = file;
   else
      if ((_file = gsc_open_geopwd(READONLY)) == -1) return GS_BAD;

   if (read(_file) == GS_BAD)
      { if (!file) _close(_file); return GS_BAD; }

   if (!file) _close(_file);

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//  C_USER //
//-----------------------------------------------------------------------//


/*************************************************************************/
/*.doc class C_USER  */
/*+
   Classe per contenere le informazioni di un generico utente. 
   contenute nel file GEOPWD con la stessa struttura.
   Campi struttura :  code  -> codice utente.     (int)
                      log   -> nome login.        (BYTE*)
                      pwd   -> password utente.   (BYTE*)
                      mode  -> informazioni circa il tipo di utente :
                               mode[0] (superuser-user)  (BYTE*)
                      flags -> flags abilitazioni comandi GEOSIM.(BYTE*)
  BYTE=unsigned char.
  NB: Il codice di un utente corrisponde alla posizione del record nel file
      GEOPWD.           
  Le macro SUPERUSER in mode[0] indica un superutente.
  La macro FREEUSER  in log[0] indica che il record non e' utilizzato
  da alcun utente.
-*/
/*************************************************************************/


/*************************************************************************/
/*.doc C_USER::alloc(int,int,int,int)  */
/*+
   Alloca memoria di BYTE di dimensione secondo i parametri in ingresso
   per le variabili log,pwd,mode,flags.
   Il distruttore della classe provvedera' automaticamente al free di tale
   memoria.
-*/
/*************************************************************************/
int C_USER::alloc(int l_log,int l_pwd,int l_mode,int l_flags)
{
  if ((log = (TCHAR *) realloc(log, (l_log + 1) * sizeof(TCHAR))) == NULL)
     { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
  memset(log, 0, (l_log + 1) * sizeof(TCHAR));

  if ((pwd = (TCHAR *) realloc(pwd, (l_pwd + 1) * sizeof(TCHAR))) == NULL)
     { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
  memset(pwd, 0, (l_pwd + 1) * sizeof(TCHAR));

  if ((mode = (BYTE *) realloc(mode, (l_mode + 1) * sizeof(BYTE))) == NULL)
     { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
  memset(mode, 0, (l_mode + 1) * sizeof(BYTE));

  if ((flags = (BYTE *) realloc(flags, (l_flags + 1) * sizeof(BYTE))) == NULL)
     { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
  memset(flags, 0, (l_flags + 1) * sizeof(BYTE));

  return GS_GOOD;
}


/*************************************************************************/
/*.doc C_USER::alloc_from_geopwd(void)  */
/*+
   Alloca memoria di BYTE di dimensione secondo le dimensioni dei campi di
   GEOPWD (letti in testa al file) per le variabili log,pwd,mode,flags.
   Il distruttore della classe provvedera' automaticamente al free di tale
   memoria.
   Parametri:
   int file;    Opzionale, il descrittore al file già aperto.
                Se = NULL la procedura provvede ad aprire il file
                delle password in sola lettura (default = NULL).

   Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/
/*************************************************************************/
int C_USER::alloc_from_geopwd(int file)
{
   C_LOG_HEAD lh;

   if (file == NULL)
      { if (lh.from_geopwd() == GS_BAD) return GS_BAD; }
   else
      if (lh.read(file) == GS_BAD) return GS_BAD;

   return alloc(lh.log_len, lh.pwd_len, lh.mode_len, lh.flags_len);
}


/*************************************************************************/
/*.doc C_USER::set_login
/*+
   Copia i dati di un oggetto C_USER in un altro oggetto C_USER
   Il distruttore della classe provvedera' automaticamente al free di tale
   memoria.
-*/
/*************************************************************************/
int C_USER::set_login(C_USER &glob)
{
   C_LOG_HEAD lh;
   int        i;

   if (lh.from_geopwd() == GS_BAD) return GS_BAD;
   if (glob.alloc(lh.log_len, lh.pwd_len, lh.mode_len, lh.flags_len) == GS_BAD)
      return GS_BAD;
   
   glob.code = code;
   wcscpy((TCHAR *) glob.log, (TCHAR *) log);
   wcscpy((TCHAR *) glob.pwd, (TCHAR *) pwd);
   strcpy((char *) glob.mode, (char *) mode);
   for (i = 0; i < lh.flags_len; i++)
      glob.flags[i] = flags[i];

   return GS_GOOD;
}


/*************************************************************************/
/*.doc C_USER::is_superuser
/*+
   Ritorna true se l'utente è un superuser altrimenti false
-*/
/*************************************************************************/
bool C_USER::is_superuser(void)
{
   if (mode == NULL || code == 0) return false;
   return (mode[0] == SUPER_USER) ? true : false;
}


/*************************************************************************/
/*.doc gsc_bitmask(int)  */
/*+
  Riceve un codice da 0 a 7 e ritorna un BYTE (unsigned char) corrispondente
  alla maschera per il corrispondete bit (contato da sinistra a destra).
-*/
/*************************************************************************/
BYTE gsc_bitmask(int bit)
{
   switch(bit)
   {
      case 0 : return 128;
      case 1 : return 64;
      case 2 : return 32;
      case 3 : return 16;
      case 4 : return 8;
      case 5 : return 4;
      case 6 : return 2;
      case 7 : return 1;
   }
   if (bit > 7) return 1;
   
   return 128;    // bit < 0
}
   

/*************************************************************************/
/*.doc gsc_byte2int(int,int,long)  */
/*+
  Legge da file un certo numero di caratteri ASCII e li converte in un
  numero intero. Riceve in ingresso l'identificatore al file (gia' aperto),
  il numero di byte da leggere e la posizione nel file da cui iniziare la
  lettura.
  Parametri:
  int file;    puntatore al file
  int nn;      numero di byte da leggere
  long cursor; posizione nel cursore nel file

  Ritorna l'intero convertito oppure -1 in caso di errore.
-*/
/*************************************************************************/
int gsc_byte2int(int file, int nn, long cursor)
{
   long kript_pos;
   BYTE *buf, kript_val;
   int  i, tot = 0;

   if ((buf = (BYTE *) malloc(nn * sizeof(BYTE))) == NULL)
     { GS_ERR_COD = eGSOutOfMem; return -1; }

   for (i = 0; i < nn; i++)
   {                        
      if (gsc_kript_pos(cursor + i, &kript_pos) == GS_BAD || /* dekript.posiz.*/
          _lseek(file, kript_pos, SEEK_SET) == -1 ||
          _read(file, &kript_val, 1) != 1 ||               /* dekript.val.*/
          gsc_dekript_val(cursor + i, kript_pos, kript_val, buf + i) == GS_BAD)
          { free(buf); GS_ERR_COD = eGSReadFile ; return -1; }

      tot += (int) pow((double)256,(double)nn-1-i)*(int)buf[i];
   }
   free(buf);

   return tot;
}


/*************************************************************************/
/*.doc gsc_int2byte(int,int,int,long)  */
/*+
   Scrive un intero su file convertendolo in caratteri ASCII.
   Riceve in ingresso l'identificatore al file (gia' aperto), il numero da
   salvare ed il numero di byte da scrivere e la posizione nel file da cui
   iniziare la scrittura.
   Ritorna GS_BAD in caso di errore, GS_GOOD altrimenti.
-*/
/*************************************************************************/
int gsc_int2byte(int file,int number,int nn,long cursor)
{
   long kript_pos;
   BYTE *buf,kript_val;
   int i;
   div_t risu;

   if((buf=(BYTE*)malloc(nn*sizeof(BYTE)))==NULL)
     { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }

   for (i=0;i<nn;i++)
   {
      risu=div(number,(int)pow((double)256,(double)nn-i-1) );
      buf[i]=(BYTE)risu.quot;
      number=risu.rem;
   }

   for (i=0;i<nn;i++)
   {
      if (gsc_kript_pos(cursor+i,&kript_pos)==GS_BAD ||
          gsc_kript_val(cursor+i,kript_pos,buf[i],&kript_val)==GS_BAD || 
          _lseek(file,kript_pos,SEEK_SET)==-1 ||
          _write(file,&kript_val,1)!=1)
         { GS_ERR_COD = eGSWriteFile; free(buf); return GS_BAD; }
   }

   free(buf);
   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_open_geopwd(void)  */
/*+
   Apre il file 'GEOpwd' contenuto nella directory di GEOSIM.
   Parametri:
   int shflag;    se = READONLY il file è aperto in sola lettura
                  e gli altri utenti lo possono aprire solo in lettura;
                  se = UPDATEABLE il file è aperto in lettura/scrittura
                  e gli altri utenti non lo possono aprire (default = UPDATEABLE).

   Ritorna l'identificatore del file o -1 in caso di errore.
-*/
/*************************************************************************/
int gsc_open_geopwd(int shflag)
{
   C_STRING path;

   path = GEOsimAppl::GEODIR + _T('\\') + GEOUSRDIR + _T('\\') + GEOPWD;
   
   if (shflag == READONLY)
      // apro il file in modo che gli altri non lo possano aprire solo in lettura
      return gsc_sopen(path.get_name(), _O_RDONLY | O_BINARY, _SH_DENYWR);
   else
      // apro il file in modo che gli altri non lo possano aprire nè in lettura nè in scrittura
      return gsc_sopen(path.get_name(), O_RDWR | O_BINARY, _SH_DENYRW);
}


/*************************************************************************/
/*.doc gsc_open_usrprj(filename)  */
/*+
   Apre il file 'filename' contenuto nella sub-directory USR di GEOSIM
   o in lettura o in scrittura (a seconda di 'mode').
   In lettura controlla che il codice utente corrisponda a quello scritto 
   sul file. In scrittura crea il file (se non esiste), lo cancella se
   esiste e vi ricopia il codice dell'utente.
   Parametri:
   int usr;    codice utente
   int mode;   modalità di apertura (READONLY o UPDATEABLE)

   Ritorna l'identificatore del file o -1 in caso di errore o di codice 
   utente non corrispondente.
-*/
/*************************************************************************/
int gsc_open_usrprj(int usr, int mode)
{
   C_STRING path;
   int      file;
   
   path = GEOsimAppl::GEODIR;
   path += _T('\\');
   path += GEOUSRDIR;
   path += _T("\\usr_");
   path += usr;
   path += _T(".prj");

   if (mode == READONLY) // APRE FILE IN LETTURA, SE NON ESISTE RITORNA -1
      if ((file = gsc_sopen(path.get_name(), O_RDONLY | O_BINARY, _SH_DENYRW, -1, ONETEST)) != -1)
      {      // CONTROLLA CHE IL CODICE UTENTE SIA CORRETTO
         if (gsc_byte2int(file, 4, 0) != usr)
            { _close(file); GS_ERR_COD = eGSReadFile; return -1; }
      }

   if (mode == UPDATEABLE) // APRE FILE IN SCRITTURA, SE NON ESISTE LO CREA
   {                       // CANCELLA IL CONTENUTO E VI SCRIVE IL CODICE UTENTE
      if ((file = gsc_sopen(path.get_name(), O_RDWR | O_BINARY | O_CREAT | _O_TRUNC, _SH_DENYRW,
                            S_IREAD | S_IWRITE)) == -1)
         return -1;
      if (_chsize(file,0) != 0 || gsc_int2byte(file, usr, 4, 0) == GS_BAD)
         { _close(file); GS_ERR_COD = eGSWriteFile; return -1; }
   }
   
   return file;
}


/*************************************************************************/
/*.doc gsc_open_usrclass                                                 */
/*+
   Apre il file abilitazione utente alle classi contenuto nella sub-directory
   USR del direttorio del progetto, il cui codice e' passato come parametro.
   La modalità di apertura è in lettura o in scrittura a seconda di <mode>.
   Legge direttorio del progetto dalla lista GEOsimAppl::PROJECTS.
   Se l'utente corrente e' SUPERUSER vedra' tutti i progetti disponibili.
   Se e' solo USER vedra solo i progetti a lui abilitati.
   In lettura controlla che il codice utente corrisponda a quello scritto 
   sul file. In scrittura crea il file (se non esiste), lo cancella se
   esiste e vi ricopia il codice dell'utente.
   Parametri:
   int usr;    codice utente
   int prj;    codice progetto
   int mode;   modalità di apertura (READONLY o UPDATEABLE)

   Ritorna l'identificatore del file o -1 in caso di errore o di codice 
   utente non corrispondente.
-*/
/*************************************************************************/
int gsc_open_usrclass(int usr, int prj, int mode)
{
   C_STRING path;
   int      file;
   
   // Legge direttorio del progetto dalla lista GEOsimAppl::PROJECTS
   // Se utente corrente e' SUPERUSER vedra' tutti i progetti disponibili
   // Se e' solo USER vedra solo i progetti a lui abilitati
   path = GEOsimAppl::PROJECTS.get_dir(prj); 
   path += _T('\\');
   path += GEOUSRDIR;
   path += _T("\\usr_");
   path += usr;
   path += _T(".cls");

   if (gsc_nethost2drive(path) == GS_BAD) return -1;
   
   if (mode == READONLY) // APRE FILE IN LETTURA, SE NON ESISTE RITORNA -1
      if ((file = gsc_sopen(path.get_name(), _O_RDONLY | _O_BINARY, _SH_DENYRW, -1, ONETEST)) != -1)
      {  // CONTROLLA CHE I CODICI UTENTE E PROGETTO SIANO CORRETTI
         if (gsc_byte2int(file, 4, 0) != usr)
            { _close(file); GS_ERR_COD = eGSReadFile; return -1; }
         if (gsc_byte2int(file, 4, 4) != prj)
            { _close(file); GS_ERR_COD = eGSReadFile; return -1; }
      }

   if (mode == UPDATEABLE) // APRE FILE IN SCRITTURA, SE NON ESISTE LO CREA
   {                       // CANCELLA IL CONTENUTO E VI SCRIVE I CODICI UTENTE E PROGETTO
      if ((file = gsc_sopen(path.get_name(), _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _SH_DENYRW,
                            _S_IREAD | _S_IWRITE)) == -1)
         return -1;
      if (_chsize(file,0)!=0 || 
          gsc_int2byte(file, usr, 4, 0) == GS_BAD ||
          gsc_int2byte(file, prj, 4, 4) == GS_BAD )
         { _close(file); return -1; }
   }
   
   return file;
}


/*************************************************************************/
/*.doc gsInitGeopwd                                                      */
/*+
  Comando che inizializza il file 'GEOpwd' craendo come primo utente un super-user
  con login 'geo' password 'geo'. NB: Cancella tutte le informazioni
  circa gli utenti gia' creati perecedentemente.
-*/
/*************************************************************************/
void gsInitGeopwd()
{
   C_STRING   path;
   TCHAR      *UserInfo;
   BYTE       *UserSet;
   int        file, error = 0;
   C_LOG_HEAD log_head;
  
   GEOsimAppl::CMDLIST.StartCmd();

   log_head.log_len   = MAX_LEN_LOGIN;
   log_head.pwd_len   = MAX_LEN_PWD;
   log_head.mode_len  = LEN_MODE;
   log_head.flags_len = LEN_FLAGS;
   log_head.tot_len   = MAX_LEN_LOGIN+MAX_LEN_PWD+LEN_MODE+LEN_FLAGS;
   log_head.max_usr   = 1;
   log_head.num_usr   = 1;

   // APRE o CREA il FILE NELLA DIRECTORY di GEOSIM
   path = GEOsimAppl::GEODIR + _T('\\') + GEOUSRDIR + _T('\\') + GEOPWD;
   if (gsc_dir_exist(path) == GS_BAD)
      { GEOsimAppl::CMDLIST.ErrorCmd(); return; }

   if ((file = gsc_sopen(path.get_name(), O_RDWR | O_BINARY | O_CREAT, _SH_DENYRW,
                         S_IREAD | S_IWRITE)) == -1)
      { GEOsimAppl::CMDLIST.ErrorCmd(); return; }

   //  RIDUCE DIMENSIONE FILE
   if (_chsize(file, LOG_HEAD_LEN + log_head.tot_len) == -1)
      { GS_ERR_COD = eGSWriteFile; _close(file); GEOsimAppl::CMDLIST.ErrorCmd(); return; }  

   // SCRIVE INTESTAZIONE FILE
   if (log_head.write(file)==GS_BAD) { _close(file); GEOsimAppl::CMDLIST.ErrorCmd(); return; }

   // Alloca memoria di lavoro
   if ((UserInfo = (TCHAR *) calloc(log_head.tot_len, sizeof(TCHAR))) == NULL)
      { GS_ERR_COD=eGSOutOfMem ; _close(file); GEOsimAppl::CMDLIST.ErrorCmd(); return; }
   if ((UserSet = (BYTE *) calloc(log_head.tot_len, sizeof(BYTE))) == NULL)
      { GS_ERR_COD=eGSOutOfMem ; _close(file); GEOsimAppl::CMDLIST.ErrorCmd(); return; }

   // CREA PRIMO UTENTE SUPER-USER CON PASSWORD=LOGIN DI DEFAULT
   if (gsc_set_log(file, log_head.max_usr, FLAGS_ID, UserSet) == GS_BAD)
      { free(UserInfo); free(UserSet); _close(file); GEOsimAppl::CMDLIST.ErrorCmd(); return; }

   UserSet[0]=SUPER_USER;
   if (gsc_set_log(file, 1, MODE_ID, UserSet) == GS_BAD) 
      { free(UserInfo); free(UserSet); _close(file); GEOsimAppl::CMDLIST.ErrorCmd(); return; }

   wcscpy_s((TCHAR *) UserInfo, log_head.tot_len, (TCHAR *) INITIAL_PWD);
   if (gsc_set_log(file, 1, LOG_ID, UserInfo) == GS_BAD) 
      { free(UserInfo); free(UserSet); _close(file); GEOsimAppl::CMDLIST.ErrorCmd(); return; }

   if (gsc_set_log(file, 1, PWD_ID, UserInfo)==GS_BAD)
      { free(UserInfo); free(UserSet); _close(file); GEOsimAppl::CMDLIST.ErrorCmd(); return; }

   _close(file);
   free(UserInfo);
   free(UserSet);

   return GEOsimAppl::CMDLIST.EndCmd();
}


/*************************************************************************/
/*.doc gsc_get_log(int,int,int,BYTE*)  */
/*+
   Legge informazioni utenti in 'GEOpwd'.
   Riceve in ingresso quattro paramentri che identificano rispettivamente :
   codice del file aperto come 'GEOpwd', codice dell'utente, identificatore 
   dell'elemento da settare (LOG_ID, PWD_ID, MODE_ID, FLAGS_ID), puntatore 
   alla stringa che conterra' l'informazione corrispondente estratta 
   da 'GEOpwd' (NB: si tratta di una stringa di BYTE allocata con dimensione
   di uno superiore alla lunghezza corrispondente in GS_LOG_HEAD).
   Ritorna GS_BAD in caso di errore, GS_GOOD altrimenti.
-*/
/*************************************************************************/
int gsc_get_log(int file, int user, int tipo, TCHAR *out)
{
   BYTE       kript_val, dummy;
   int        i,len;
   long       cursor,kript_pos;
   C_LOG_HEAD log_head;

   if (log_head.read(file) == GS_BAD) return GS_BAD;

   if (user < 1 || user > log_head.max_usr)
     { GS_ERR_COD = eGSInvalidUser; return GS_BAD; }

   cursor = LOG_HEAD_LEN + (user - 1) * log_head.tot_len;

   switch (tipo)
   {
      case LOG_ID :
         len = log_head.log_len; break;

      case PWD_ID :
         len = log_head.pwd_len;
         cursor += log_head.log_len;
         break;

      default :
         return GS_BAD;
   }

   for (i = 0; i < len; i++)
   {                        
      if (gsc_kript_pos(cursor + i, &kript_pos) == GS_BAD || // dekript.posiz.
          _lseek(file, kript_pos, SEEK_SET) == -1 ||
          _read(file, &kript_val, 1) != 1 ||                 // dekript.val.
          gsc_dekript_val(cursor + i, kript_pos, kript_val, &dummy) == GS_BAD)
         { GS_ERR_COD = eGSReadFile; return GS_BAD; }

      out[i] = gsc_CharToUnicode(dummy); // Conversione in Unicode
   }
   out[i] = _T('\0');

   return GS_GOOD;
}
int gsc_get_log(int file, int user, int tipo, BYTE *out)
{
   BYTE       kript_val;
   int        i, len;
   long       cursor,kript_pos;
   C_LOG_HEAD log_head;

   if (log_head.read(file)==GS_BAD) return GS_BAD;

   if (user < 1 || user > log_head.max_usr)
      { GS_ERR_COD = eGSInvalidUser; return GS_BAD; }

   cursor = LOG_HEAD_LEN + (user - 1) * log_head.tot_len;

   switch (tipo)
   {
      case MODE_ID  :
         len=log_head.mode_len;
         cursor+=log_head.log_len+log_head.pwd_len; break;

      case FLAGS_ID :
         len=log_head.flags_len;
         cursor+=log_head.log_len+log_head.pwd_len+log_head.mode_len; break;

      default :
         return GS_BAD;
   }

   for (i = 0; i < len; i++)
   {                        
      if (gsc_kript_pos(cursor + i, &kript_pos) == GS_BAD || // dekript.posiz.
          _lseek(file,kript_pos,SEEK_SET) == -1 ||
          _read(file,&kript_val, 1) != 1 ||                  // dekript.val.
          gsc_dekript_val(cursor + i, kript_pos, kript_val, out + i) == GS_BAD)
         { GS_ERR_COD = eGSReadFile ; return GS_BAD; }
   }
   out[i]='\0';

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_set_log(int,int,int,BYTE*)  */
/*+
   Setta informazioni utenti in 'GEOpwd'.
   Riceve in ingresso quattro paramentri che identificano rispettivamente :
   codice del file aperto come 'GEOpwd', codice dell'utente, identificatore
   dell'elemento da settare (LOG_ID, PWD_ID, MODE_ID, FLAGS_ID), stringa
   contenete l'informazione da registrare in 'GEOpwd'.

   Ritorna GS_BAD in caso di errore, GS_GOOD altrimenti.
-*/
/*************************************************************************/
int gsc_set_log(int file, int user, int tipo, const TCHAR *in)
{
   size_t     i, len , len_in;
   BYTE       kript_val;                
   long       cursor, kript_pos;
   TCHAR      free = FREE_USER;
   C_LOG_HEAD log_head;
   char       CharValue;
   C_STRING   Login(in);

   if (log_head.read(file) == GS_BAD) return GS_BAD;

   if (user < 1) { GS_ERR_COD = eGSInvalidUser; return GS_BAD; }

   cursor = LOG_HEAD_LEN + (user - 1) * log_head.tot_len;
   switch (tipo)
   {
      case FREE_ID :
         len   = 1;
         Login = FREE_USER;
         break;   // Imposta cancellazione utente

      case LOG_ID :
         if ((int) (len_in = Login.len()) < log_head.log_len)
            len = len_in + 1;
         else 
            len = log_head.log_len;
         break;
            
      case PWD_ID :
         if ((int) (len_in = Login.len()) < log_head.pwd_len)
            len = len_in + 1;
         else 
            len = log_head.pwd_len;
         cursor += log_head.log_len;
         break;
            
      case MODE_ID :
         len = log_head.mode_len;
         cursor += log_head.log_len + log_head.pwd_len;
         break;

      case FLAGS_ID :
         len = log_head.flags_len;
         cursor += log_head.log_len + log_head.pwd_len + log_head.mode_len;
         break;

      default :
         return GS_BAD;
   }

   for (i = 0; i < len; i++)
   {
      CharValue = gsc_UnicodeToChar(Login.get_chr(i));
      if (gsc_kript_pos((long) (cursor + i), &kript_pos) == GS_BAD ||          
          gsc_kript_val((long) (cursor + i), kript_pos, CharValue, &kript_val) == GS_BAD || // Conversione in Unicode
          lseek(file, kript_pos, SEEK_SET) == -1 || _write(file, &kript_val, 1) != 1)
         { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   }

   return GS_GOOD;
}
int gsc_set_log(int file, int user, int tipo, BYTE *in)
{
   int        i, len;
   BYTE       kript_val;                
   long       cursor, kript_pos;
   BYTE       free = FREE_USER;
   C_LOG_HEAD log_head;

   if (log_head.read(file) == GS_BAD) return GS_BAD;

   if (user < 1) { GS_ERR_COD = eGSInvalidUser; return GS_BAD; }

   cursor = LOG_HEAD_LEN + (user - 1) * log_head.tot_len;
   switch (tipo)
   {
      case FREE_ID :
         len=1; in=&free; break;   // Imposta cancellazione utente

      case MODE_ID  :
         len=log_head.mode_len;
         cursor+=log_head.log_len+log_head.pwd_len; break;

      case FLAGS_ID :
         len=log_head.flags_len;
         cursor+=log_head.log_len+log_head.pwd_len+log_head.mode_len; break;
      default :
         return GS_BAD;
   }

   for (i = 0; i < len; i++)
   {
      if (gsc_kript_pos(cursor+i,&kript_pos) == GS_BAD ||
          gsc_kript_val(cursor+i,kript_pos,in[i],&kript_val) == GS_BAD || 
          lseek(file,kript_pos,SEEK_SET) == -1 || _write(file,&kript_val,1) != 1)
         { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_kript_pos(long,long*)  */
/*+
   Traduce posizione logica in posizione fisica relativa a 'GEOpwd'.
   Riceve in ingresso un indice di posizione (logica) nel file 'GEOpwd'
   oltre al puntatore alla relativa variabile di output in cui la funzione
   scrivera' posizione kriptata nel file (posizione fisica). 
   La funzione ritorna GS_BAD in caso di errore, GS_GOOD altrimenti.
-*/
/*************************************************************************/
int gsc_kript_pos(long pos_in,long *pos_out)
{  
   //  ATTENZIONE !! MODIFICARE QUESTA PROCEDURA PUO' ESSERE PERICOLOSO
   //  PER ACCESSO A FILE NON A DIMENSIONE FISSA COME 'GEOPWD' O 'USRPRJ'
   *pos_out=pos_in;
   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_kript_val(long,long,BYTE,BYTE*)  */
/*+
   Kripta valore da inserire in 'GEOpwd'.
   Riceve in ingresso una posizione logica, la relativa kriptata(fisica)
   ed un valore da kriptatre prima di inserirlo nel file 'GEOpwd',
   assieme al puntatore alla relativa variabile di output in cui la
   funzione scrive il valore kriptato. 
   La funzione ritorna GS_BAD in caso di errore, GS_GOOD altrimenti.
-*/
/*************************************************************************/
int gsc_kript_val(long pos,long pos_kri,BYTE val_in,BYTE *val_out)
{
   *val_out=val_in;
   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_dekript_val(long,long,BYTE,BYTE*)  */  
/*+
   Decripta valore letto da 'GEOpwd'.
   Riceve in ingresso una posizione logica, la relativa kriptata(fisica)
   ed un valore da dekriptatre letto dal file 'GEOpwd',
   assieme al puntatore alla relativa variabile di output in cui la
   funzione scrive il valore dekriptato.    
   La funzione ritorna GS_BAD in caso di errore, GS_GOOD altrimenti.
-*/
/*************************************************************************/
int gsc_dekript_val(long pos, long pos_kri, TCHAR val_in, TCHAR *val_out)
{
   *val_out = val_in;
   return GS_GOOD;
}
int gsc_dekript_val(long pos, long pos_kri, BYTE val_in, BYTE *val_out)
{
   *val_out = val_in;
   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_get_usrcode (char *)  */
/*+
  La funzione ritorna il codice utente corrispondente al suo nome (login,
  case insensitive), -1 altrimenti.
  Tali informazioni vengono lette dal file GEOPWD.
  Parametri:
  const TCHAR *log_name;   login di un utente

  Ritorna il codice utente in caso di successo altrimenti -1
-*/
/*************************************************************************/
int gsc_get_usrcode(const TCHAR *log_name)
{
   int        file, i, code = 0;
   C_USER     temp;
   C_LOG_HEAD log_head;

   if ((file = gsc_open_geopwd(READONLY)) == -1) return -1;
   if (temp.alloc_from_geopwd(file) == GS_BAD) { _close(file); return -1; }
   if (log_head.read(file) == GS_BAD) { _close(file); return -1; }

   for (i = 1; i <= log_head.max_usr; i++)
   {
      if (gsc_get_log(file, i, LOG_ID, temp.log) == GS_BAD)
         { _close(file); return -1; } 
      if (gsc_strcmp(log_name, temp.log, FALSE) == 0) // case insensitive
         { code = i; break; }
   }

   _close(file);
   if (code ==0 ) { GS_ERR_COD = eGSInvalidLogin; return -1; }

   return code;
}


/*************************************************************************/
/*.doc gsc_get_user (int,C_USER*)  */
/*+
  Riceve il codice di un utente e la funzione ritorna una classe C_USER
  contenente il codice utente stesso se l'utente esiste 0 se non esiste.
  La classe conterra' inoltre tutte le informazioni dell'utente.
  Parametri:
  int code;       Codice utente
  C_USER* user;   utente
  int file;       Opzionale, il descrittore al file già aperto.
                  Se = NULL la procedura provvede ad aprire il file
                  delle password in sola lettura (default = NULL).

  Ritorna GS_BAD in caso di errore, GS_GOOD altrimenti.
-*/
/*************************************************************************/
int gsc_get_user(int code, C_USER* user, int file)
{
   int _file;

   _file = (file) ? file : gsc_open_geopwd(READONLY);

   if (_file == -1) return GS_BAD;

   if (gsc_get_log(_file, code, LOG_ID, user->log) == GS_BAD)
      { if (!file) _close(_file); return GS_BAD; }

   if (user->log[0] == FREE_USER)    // Codice utente non utilizzato
      { if (!file) _close(_file); user->code = 0; return GS_GOOD; }
   else { user->code = code; }
   
   if (gsc_get_log(_file, code, PWD_ID, user->pwd) == GS_BAD)
      { if (!file) _close(_file); return GS_BAD; }
   if (gsc_get_log(_file, code, MODE_ID, user->mode) == GS_BAD)
      { if (!file) _close(_file); return GS_BAD; }
   if (gsc_get_log(_file, code, FLAGS_ID, user->flags) == GS_BAD)
      { if (!file) _close(_file); return GS_BAD; }

  if (!file) _close(_file);

  return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_checkpwd                                                      */
/*+
   Riceve codice e password di un utente.
   Viene verificata che la corrispondenza sia esatta nel file GEPWD :
   in caso affermativo la funzione ritorna GS_GOOD, GS_BAD altrimenti.
   Parametri:
   int code;
   const TCHAR *pwd_name;
-*/
/*************************************************************************/
int gsc_checkpwd(int code, const TCHAR *pwd_name)
{
   int    file;
   C_USER temp;

   if ((file = gsc_open_geopwd(READONLY)) == -1) return -1;
   if (temp.alloc_from_geopwd(file) == GS_BAD) { _close(file); return GS_BAD; } 
   if (gsc_get_log(file, code, PWD_ID, temp.pwd) == GS_BAD) 
      { _close(file); return GS_BAD; } 
   _close(file);

   if (gsc_strcmp(pwd_name,(TCHAR *)temp.pwd) == 0)
      return GS_GOOD; 
   else 
   {
      GS_ERR_COD = eGSInvalidPwd; 
      return GS_BAD;
   }
}


/*************************************************************************/
/*.doc gsc_superuser(void)  */
/*+
    Ritorna GS_GOOD se l'utente corrente e' 'SUPER-USER', testando il 
    primo byte della vaiabile GEOsimAppl::GS_USER.mode, GS_BAD atrimenti.
-*/
/*************************************************************************/
int gsc_superuser(void)
{
   return GEOsimAppl::GS_USER.is_superuser();
}


/*************************************************************************/
/*.doc gsc_superuser(int)  */
/*+
  Riceve un codice utente e ritorna GS_GOOD se questo e' 'SUPER-USER',
  GS_BAD se e' user normale -1 altrimenti.
  parametri:
  int code;    Codice utente
  int file;    Opzionale, il descrittore al file già aperto.
               Se = NULL la procedura provvede ad aprire il file
               delle password in sola lettura (default = NULL).
-*/
/*************************************************************************/
int gsc_superuser(int code, int file)
{
   C_USER temp;

   if (temp.alloc_from_geopwd(file) == GS_BAD) return -1;
   if (gsc_get_user(code, &temp, file) == GS_BAD) return -1;
      
   return temp.is_superuser();
}


/*************************************************************************/
/*.doc gs_check_op(int)  */
/*+
   Riceve codice di una operazione e verifica che l'utente corrente sia
   abilitato ad eseguirla.
   In caso affermativo ritorna TRUE altrimenti ritorna NIL.
-*/
/*************************************************************************/
int gs_checkop(void)
{
   presbuf arg;

   acedRetNil();
   arg = acedGetArgs();

   if (arg==NULL || arg->restype!=RTSHORT) // Errore argomenti
      { GS_ERR_COD=eGSInvalidArg; return RTERROR; } 
   if (gsc_check_op(arg->resval.rint) == GS_GOOD) acedRetT();

   return RTNORM;
}


/*************************************************************************/
/*.doc gsc_check_op(int)  */
/*+
   Riceve codice di una operazione e verifica che l'utente corrente sia
   abilitato ad eseguirla (Testando il bit corrispondente di GEOsimAppl::GS_USER.flags).
   In caso affermativo ritorna GS_GOOD altrimenti ritorna GS_BAD.
-*/
/*************************************************************************/
int gsc_check_op(int op)
{
   div_t      punt;
   C_LOG_HEAD log_head;

   if (GEOsimAppl::GS_USER.flags == NULL || GEOsimAppl::GS_USER.code == 0)
      { GS_ERR_COD = eGSInvalidOpCod; return GS_BAD; }

   // la possibilità di eseguire il salvataggio è negata se lo stato di 
   // GS_CHECK_DBREACTOR == GS_CAN cioè reattori disabilitati per tutta la
   // durata della sessione di lavoro corrente
   if (op == opSaveArea && GS_CHECK_DBREACTOR == GS_CAN)
      { GS_ERR_COD=eGSOpNotAble; return GS_BAD; }

   if (gsc_superuser() == GS_GOOD) return GS_GOOD;
   else // utente normale
      switch (op) // comandi solo per superutenti
      {
         case opCreatePrj:      case opDelPrj:
         case opModPrj:         case opBackupPrj:
         case opCopyPrj:        case opCreateClass:
         case opDelClass:       case opModClass:
         case opCopyClass:      case opDestroyClass:
         case opCreateSec:      case opDelSec:
         case opDestroySec:     case opModSec:
         case opCopySec:        case opCreateLinkSec:
         case opDelLinkSec:     case opCreateUsr:
         case opDelUsr:         case opModUsr:
         case opConv2ActualVer: case opSynchronizeClass:
         case opDWGOptimization:
            { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }
      }

   if (log_head.from_geopwd() == GS_BAD) return GS_BAD;
//   if (GEOsimAppl::GS_USER.alloc_from_geopwd() == GS_BAD) return GS_BAD;

   punt = div(op, 8);
   if (punt.quot >= log_head.flags_len)
      { GS_ERR_COD = eGSInvalidOpCod; return GS_BAD; }

   if ((GEOsimAppl::GS_USER.flags[punt.quot] & gsc_bitmask(punt.rem)) == 0)
      { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }
   
   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_set_op(int,int,int)  */
/*+
   (Solo per SUPER-USER)
   Riceve il codice di una operazione, il codice di un utente ed un flag:
   se il flag e' GS_GOOD abilita l'utente specificato ad esegure l'operazione
   specificata, se e' GS_BAD lo disabilita. L'aggiornamento avviene nel file
   'GEOpwd'.  Ritorna GS_BAD in caso di errore, GS_GOOD altrimenti.
-*/
/*************************************************************************/
int gsc_set_op(int op,int user,int flag)
{
   int        file, error = 0;
   BYTE       flags;
   div_t      punt;
   C_USER     temp;
   C_LOG_HEAD log_head;

   if (gsc_superuser() == GS_BAD) { GS_ERR_COD=eGSOpNotAble; return GS_BAD; }

   if ((file = gsc_open_geopwd()) == -1) return GS_BAD; 
   if (temp.alloc_from_geopwd(file) == GS_BAD) { _close(file); return GS_BAD; }
   if (log_head.read(file) == GS_BAD) { _close(file); return GS_BAD; }

   if (gsc_get_log(file, user, FLAGS_ID, &flags) == GS_BAD)
      { _close(file); return GS_BAD; }

   punt = div(op, 8);
   if (punt.quot >= log_head.flags_len)
      { GS_ERR_COD = eGSInvalidOpCod; _close(file); return GS_BAD; }

   if (flag==GS_GOOD)                 /*** Alza il bit corrispondente ***/
      temp.flags[punt.quot] |= gsc_bitmask(punt.rem) ;
   if (flag==GS_BAD)               /*** Abbassa il bit corrispondente ***/
      temp.flags[punt.quot] &=~( gsc_bitmask(punt.rem) );
  
   if (gsc_set_log(file, user, FLAGS_ID, &flags) == GS_BAD)
      { _close(file); return GS_BAD; }  
   _close(file);

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsPrintGeopwd                                                     */
/*+
   Comando che stampa le informazioni utenti come codificate in 'GEOpwd'.
-*/
/*************************************************************************/
void gsPrintGeopwd()
{
   int        i, j, file;
   TCHAR      buf[10], log[2] = {_T('\0'), _T('\0')};
   C_USER     temp;
   C_LOG_HEAD log_head;
   TCHAR      CurrPassword[132];

   GEOsimAppl::CMDLIST.StartCmd();

   if (gsc_superuser() == GS_BAD)
      { GS_ERR_COD = eGSInvalidOpCod; GEOsimAppl::CMDLIST.ErrorCmd(); return; }

   // Richiesta della password del superutente corrente
   acutPrintf(GS_LFSTR);
   if (acedGetString(1, gsc_msg(129), CurrPassword) != RTNORM) // "Password: "
      { GEOsimAppl::CMDLIST.ErrorCmd(); return; }
   // verifico che la password sia corretta
   if (gsc_strcmp(CurrPassword, GEOsimAppl::GS_USER.pwd) != 0)
      { GS_ERR_COD = eGSInvalidPwd; GEOsimAppl::CMDLIST.ErrorCmd(); return; }

   if ((file = gsc_open_geopwd(READONLY)) == -1)
      { GEOsimAppl::CMDLIST.ErrorCmd(); return; }
   if (temp.alloc_from_geopwd(file) == GS_BAD)
      { _close(file); GEOsimAppl::CMDLIST.ErrorCmd(); return; }
   if (log_head.read(file) == GS_BAD)
      { _close(file); GEOsimAppl::CMDLIST.ErrorCmd(); return; }

   acutPrintf(_T("\nUSR:  %d  %d  "), log_head.num_usr, log_head.max_usr);
   acutPrintf(_T("     LEN: %d %d %d %d\n"), log_head.log_len,
                                             log_head.pwd_len,
                                             log_head.mode_len,
                                             log_head.flags_len);

   for (i = 1; i <= log_head.max_usr; i++)
   {
      if (gsc_get_log(file, i, LOG_ID, temp.log) == GS_BAD)
         { _close(file); GEOsimAppl::CMDLIST.ErrorCmd(); return; }
      if (gsc_get_log(file, i, PWD_ID, temp.pwd) == GS_BAD)
         { _close(file); GEOsimAppl::CMDLIST.ErrorCmd(); return; }
      if (gsc_get_log(file, i, MODE_ID, temp.mode) == GS_BAD)
         { _close(file); GEOsimAppl::CMDLIST.ErrorCmd(); return; }
      if (gsc_get_log(file, i, FLAGS_ID, temp.flags) == GS_BAD)
         { _close(file); GEOsimAppl::CMDLIST.ErrorCmd(); return; }
      if (i == GEOsimAppl::GS_USER.code) log[0] = _T('*');
      else log[0] = _T('\0');

      acutPrintf(_T("\n%d%s\t%s\t%s\t%s\t"), i, log, temp.log, temp.pwd, temp.mode);
   
      for (j = 0; j < log_head.flags_len; j++)
      {
         gsc_itoa((int)temp.flags[j],(TCHAR *)&buf, 2);
         acutPrintf(_T("%s "), buf);
      }
   }
   _close(file);
   acutPrintf(_T("\n\n"));

   return GEOsimAppl::CMDLIST.EndCmd();
}


/*************************************************************************/
/*.doc print_users(void) <internal> */
/*+
   Stampa la lista degli utenti (se il parametro mode è omesso
   significa ALL)
   Parametri:
   int mode;		criteri do selezione degli utenti:
   					SUPERUSR  = solo super utenti
					   NORMALUSR = solo utenti normali
					   ALL 		 = tutti gli utenti

   La funzione ritorna GS_GOOD in caso di login corretta, GS_BAD altrimenti.
-*/
/*************************************************************************/
int print_users(int mode)
{
   C_INT_INT_STR_LIST list;
   C_INT_INT_STR      *puser;

   acutPrintf(_T("%s"), gsc_msg(146));
   if (gsc_getusrlist(&list) == GS_BAD) return GS_BAD;

   puser = (C_INT_INT_STR *) list.get_head();
   while (puser)
   {
      switch (mode)
      {
         case SUPERUSR:
            if (puser->get_type() == 0) // super-utente
               acutPrintf(_T("\n%s"), puser->get_name());
            break;
         case NORMALUSR:
            if (puser->get_type() == 1) // utente normale
               acutPrintf(_T("\n%s"), puser->get_name());
            break;
         case ALL:                      // tutti gli utenti
            acutPrintf(_T("\n%s"), puser->get_name());
            break;
         default: break;
      }
      puser = (C_INT_INT_STR *) list.get_next();
   }

   acutPrintf(GS_LFSTR);

   return GS_GOOD;
}


/*********************************************************/
/*.doc (new 2) gsc_modpwd <external> */
/*+                                                                       
int gsc_modpwd(void)

  Modifica la password dell'utente corrente.
  Parametri:
  const TCHAR *OldPassword;      Vecchia password di controllo
  const TCHAR *NewPassword;      Nuova password
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_modpwd(const TCHAR *OldPassword, const TCHAR *NewPassword)
{
   int file;
   
   // SENZA LOGIN NON PUOI FARE NULLA //
   if (gsc_whoami(NULL)==GS_BAD) return GS_BAD;

   if (gsc_check_op(opModPwd) == GS_BAD) return GS_BAD;

   if (!OldPassword || !NewPassword) { GS_ERR_COD=eGSInvalidArg; return GS_BAD; }

   // Controlla password
   if (gsc_checkpwd(GEOsimAppl::GS_USER.code, OldPassword)==GS_BAD) return GS_BAD;

   if ((file = gsc_open_geopwd())==-1) return GS_BAD;
   if (gsc_set_log(file, GEOsimAppl::GS_USER.code, PWD_ID, (TCHAR *)NewPassword) == GS_BAD)
      { _close(file); return GS_BAD; }
   _close(file);   

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_createMapSuperUsr                             */
/*+                                                                       
  Crea un nuovo utente di AutoCAD Map.
  Parametri:
  const TCHAR *UserName;    nome utente
  const TCHAR *Password;    password
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_createMapSuperUsr(const TCHAR *UserName, const TCHAR *Password)
{
   AcMapSession    *mapSessionApi;
   int             right, SuperRight;
   
   SuperRight = AcMap::kRightsSuperUser | AcMap::kRightsAltDwgSet |
                AcMap::kRightsEditDwg | AcMap::kRightsDrawQuery |
                AcMap::kRightsAltClass;

   if ((mapSessionApi = AcMapGetSession()) == NULL) return GS_BAD;
   if (mapSessionApi->GetUserRights(right, UserName) == AcMap::kOk)
   {
      // l'utente esisteva già ma non era un superutente
      if (right != SuperRight)
         if (mapSessionApi->SetUserRights(SuperRight, UserName) != AcMap::kOk) return GS_BAD;
      
      if (mapSessionApi->ChangePassword(UserName, Password) != AcMap::kOk) return GS_BAD;

      return GS_GOOD;
   }
      
   // Call Create User()
   if (mapSessionApi->CreateUser(UserName, Password, SuperRight) != AcMap::kOk)
   {
      if (!mapSessionApi->LogIn(_T("Superuser"), _T("SUPERUSER")))
         if (!mapSessionApi->LogIn(_T("geo"), _T("geo")))
            return GS_BAD;

      if (mapSessionApi->CreateUser(UserName, Password, SuperRight) != AcMap::kOk)
         return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_delMapUsr                                */
/*+                                                                       
  Cancella un utente di AutoCAD Map.
  Parametri:
  const TCHAR *UserName;    nome utente
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_delMapUsr(const TCHAR *UserName)
{
   AcMapSession *mapSessionApi;

   if ((mapSessionApi = AcMapGetSession()) == NULL) return GS_BAD;
   if (mapSessionApi->DeleteUser(UserName) != AcMap::kOk) return GS_BAD;
 
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_updMapUsrPwd                                  */
/*+                                                                       
  Modifica la password di un utente di AutoCAD Map.
  Parametri:
  const TCHAR *UserName;    nome utente
  const TCHAR *Password;    password
 
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_updMapUsrPwd(const TCHAR *UserName, const TCHAR *Password)
{
   AcMapSession *mapSessionApi;

   if ((mapSessionApi = AcMapGetSession()) == NULL) return GS_BAD;
   if (mapSessionApi->ChangePassword(UserName, Password) != AcMap::kOk) return GS_BAD;
 
   return GS_GOOD;
}