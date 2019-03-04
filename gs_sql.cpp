/**********************************************************
Name: GS_SQL.CPP

Module description: File contenente le funzioni per la gestione dei database
                    in Geosim
            
Author: Poltini Roberto

(c) Copyright 1999-2015 by IREN ACQUA GAS

Modification history:
              
Notes and restrictions on use: 

**********************************************************/


/*********************************************************/
/* INCLUDES */
/*********************************************************/


#include "stdafx.h"         // MFC core and standard components
#include <atlconv.h>

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")
#include <oledb.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "..\gs_def.h" // definizioni globali
#include "gs_error.h"      // codici errori
#include "gs_list.h"       // gestione liste C++ 
#include "gs_utily.h"      // funzioni di utilità
#include "gs_resbf.h"      // gestione delle liste di resbuf
#include "gs_init.h"       // variabili di ambiente GEOsim
#include "gs_user.h"       // gestione utenti     
#include "gs_netw.h"       // gestione nodi rete     
#include "gs_sql.h"        // gestione dei database
#include "gs_sec.h"        // gestione delle tabelle secondarie


#define CHUNKSIZE	2000     // dimensione (in byte) di un chunk per gestione dati binari


/*********************************************************/
/* GLOBAL VARIABILES */
/*********************************************************/


/*********************************************************/
/* PRIVATE FUNCTIONS */
/*********************************************************/


int gsc_getTablePartInfo(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *TablePart, TCHAR *Separator,
                         C_STRING &UDLProperty, C_STRING &ResourceType, C_STRING &Description);
TCHAR *gsc_getFullRefTabComponent(TCHAR **FullRefTab, const TCHAR InitQuotedId,
                                  const TCHAR FinalQuotedId, const TCHAR Separator = _T('\0'),
                                  StrCaseTypeEnum StrCase = Upper, 
                                  ResourceTypeEnum ResourceType = EmptyRes);
TCHAR *gsc_getRudeTabComponent(TCHAR **FullRefTab, const TCHAR Separator = _T('\0'));

int gsc_CheckPropList(C_2STR_LIST &PropList, C_2STR_LIST &VerifPropList,
                      const TCHAR *PropNameNoCheck = NULL);
int gsc_internal_UDLProperties_nethost2drive(TCHAR *UDLPropertyName,
                                             C_2STR_LIST &PropList);
int gsc_internal_UDLProperties_drive2nethost(TCHAR *UDLPropertyName,
                                             C_2STR_LIST &PropList);

int gsc_Rb2Variant(struct resbuf *rb, _variant_t &Val, DataTypeEnum *pDataType = NULL);
int gsc_Variant2Rb(_variant_t &Val, struct resbuf *pRB);
int gsc_DBReadField(FieldPtr &pFld, presbuf pValue);
int gsc_RbSubst(struct resbuf *pRB, TCHAR *Value, long Size);
int gsc_DBSetRowValues(_RecordsetPtr &pRs, presbuf ColVal);
int gsc_DBSetRowValues(_RecordsetPtr &pDestRs, _RecordsetPtr &pSourceRs);

int gsc_BlobToVariant(void *pValue, long Size, VARIANT &varArray);
int gsc_SetBLOB(void *pValue, long Size, _ParameterPtr &pParam);

C_PROVIDER_TYPE *gsc_SearchNumericBySizePrec(C_PROVIDER_TYPE_LIST *p_DataTypeList,
                                             long Size, int Prec, int IsAutoUniqueValue);

int gsc_ReadDBSample(C_PROFILE_SECTION_BTREE &ProfileSections, C_STRING &DBSample);
int gsc_ReadDBSample(const TCHAR *RequiredFile, C_STRING &DBSample);
bool gsc_is_IntegrityConstraint_DBErr(_com_error &e, _ConnectionPtr &Conn);


/*****************************************************************************/
/*  GESTIONE CONNESSIONE SQL     -     INIZIO
/*****************************************************************************/


/****************************************************************/
/*.doc gsc_DBOpenConnFromUDL                         <external> */
/*+
  Questa funzione apre una connessione con un provider di database.
  Parametri: 
  const TCHAR *UdlFile;    File di connessione al database
  _ConnectionPtr &pConn;   Puntatore alla connessione database
  TCHAR *username;         Nome utente
  TCHAR *password;         Password
  int PrintError;          Se il flag = GS_GOOD in caso di errore viene
                           stampato il messaggio relativo altrimenti non
                           viene stampato alcun messaggio (default = GS_GOOD)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. ricordarsi di inizializzare le COM (vedi "::CoInitialize(NULL);")
-*/  
/****************************************************************/
int gsc_DBOpenConnFromUDL(const TCHAR *UdlFile, _ConnectionPtr &pConn, 
                          TCHAR *username, TCHAR *password, int PrintError)
{
   C_STRING InitStr, FullPath(UdlFile);

   if (gsc_getUDLFullPath(FullPath) == GS_BAD) return GS_BAD;
   InitStr.paste(gsc_LoadOleDBIniStrFromFile(FullPath.get_name()));

   return gsc_DBOpenConnFromStr(InitStr.get_name(), pConn, username, password, true, PrintError);
}     


/****************************************************************/
/*.doc gsc_DBOpenConnFromStr                         <external> */
/*+
  Questa funzione apre una connessione con un provider di database.
  Parametri: 
  const TCHAR *ConnStr;     Stringa di connessione al database
  _ConnectionPtr &pConn;   Puntatore alla connessione database
  TCHAR *username;         Nome utente
  TCHAR *password;         Password
  bool Prompt;             Flag, se = true nel caso la connessione non sia possibile
                           chiede all'utente una login e password (default = true)
  int PrintError;          Se il flag = GS_GOOD in caso di errore viene
                           stampato il messaggio relativo altrimenti non
                           viene stampato alcun messaggio (default = GS_GOOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. ricordarsi di inizializzare le COM (vedi "::CoInitialize(NULL);")
-*/  
/****************************************************************/
int gsc_DBOpenConnFromStr(const TCHAR *ConnStr, _ConnectionPtr &pConn, 
                          TCHAR *username, TCHAR *password, 
                          bool Prompt, int PrintError)
{
	HRESULT hr = E_FAIL;

   if (!ConnStr || wcslen(ConnStr) == 0)
      { GS_ERR_COD = eGSConnect; return GS_BAD; }

   // Creo una nuova connessione
   if (FAILED(pConn.CreateInstance(__uuidof(Connection))))
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

try
	{  // Apro connessione
	   if (FAILED(hr = pConn->Open(ConnStr, username, password, adConnectUnspecified)))
         _com_issue_error(hr);    
   }

	catch (_com_error)
	{
      // provo senza login e password 
	   try
      {  // Apro connessione
         if (FAILED(hr = pConn->Open(ConnStr, GS_EMPTYSTR, GS_EMPTYSTR, adConnectUnspecified)))
            _com_issue_error(hr);    
      }

      catch (_com_error &e)
	   {
         if (Prompt)
         {
            // provo a chiedere login e password
            C_STRING UserID, Password;

            gsc_ddinput(gsc_msg(128), UserID, NULL, FALSE, FALSE); // "Nome Utente: "
            gsc_ddinput(gsc_msg(129), Password, NULL, FALSE, FALSE); // "Password: "

	         try
            {  // Apro connessione
               if (FAILED(hr = pConn->Open(ConnStr, UserID.get_name(), Password.get_name(), adConnectUnspecified)))
                  _com_issue_error(hr);    
            }

            catch (_com_error &e)
	         {
               if (PrintError == GS_GOOD) printDBErrs(e);
               pConn.Release();
               GS_ERR_COD = eGSConnect;
               return GS_BAD;
            }
         }
         else // non chiedo login e password
         {
            if (PrintError == GS_GOOD) printDBErrs(e);
            pConn.Release();
            GS_ERR_COD = eGSConnect;
            return GS_BAD;
         }
      }
   }

   // roby test
   //for (int i = 0; i < pConn->Properties->Count; i++)
   //   acutPrintf(_T("\n%s=%s"), (TCHAR*)(pConn->Properties->GetItem((long)i)->GetName()),
   //                             (TCHAR*)_bstr_t(pConn->Properties->GetItem((long)i)->GetValue()));

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_DBCloseConn                        <external> */
/*+
  Questa funzione chiude una connessione con un provider di database.
  Parametri: 
  _ConnectionPtr &pConn;   Puntatore alla connessione database
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_DBCloseConn(_ConnectionPtr &pConn)
{
	HRESULT hr = E_FAIL;

   if (pConn.GetInterfacePtr() == NULL) return GS_GOOD;

	try
	{
      if (pConn->State == adStateOpen)
         if (FAILED(hr = pConn->Close())) _com_issue_error(hr);
      pConn.Release();
   }

	catch (_com_error &e)
	{
      printDBErrs(e);
      GS_ERR_COD = eGSDisconnect;
      return GS_BAD;
   }
   catch (...) 
   {
      GS_ERR_COD = eGSDisconnect;
      return GS_BAD;
   }

   return GS_GOOD;
}


/*****************************************************************************/
/*  GESTIONE CONNESSIONE SQL     -     FINE
/*  GESTIONE COMANDI SQL         -     INIZIO
/*****************************************************************************/


/*************************************************************/
/*.doc gsc_PrepareCmd                             <external> */
/*+
  Questa funzione prepara un comando SQL.
  Parametri: 
  _ConnectionPtr &pConn;   Puntatore alla connessione database
  C_STRING &statement;     Istruzione SQL
  _CommandPtr &pCmd;       Puntatore al comando

  oppure

  _ConnectionPtr &pConn;   Puntatore alla connessione database
  const TCHAR *statement;  Istruzione SQL
  _CommandPtr &pCmd;       Puntatore al comando

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************/
int gsc_PrepareCmd(_ConnectionPtr &pConn, C_STRING &statement, _CommandPtr &pCmd)
{
   return gsc_PrepareCmd(pConn, statement.get_name(), pCmd);
}
int gsc_PrepareCmd(_ConnectionPtr &pConn, const TCHAR *statement, _CommandPtr &pCmd)
{
	HRESULT hr = E_FAIL;

   // istanzio un comando
   if (FAILED(pCmd.CreateInstance(__uuidof(Command))))
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

	try
	{
      pCmd->ActiveConnection = pConn;
      if (FAILED(hr = pCmd->put_CommandType(adCmdText))) _com_issue_error(hr);
      if (FAILED(hr = pCmd->put_CommandText((_bstr_t)statement))) _com_issue_error(hr);
      // la mantengo compilata per usi successivi 
      if (FAILED(hr = pCmd->put_Prepared(VARIANT_TRUE))) _com_issue_error(hr);
      pCmd->PutCommandTimeout(GEOsimAppl::GLOBALVARS.get_WaitTime());
   }

	catch (_com_error &e)
	{
      printDBErrs(e, pConn);
      pCmd.Release();
      GS_ERR_COD = eGSConstrStm;
      return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_ExeCmd                             <external> */
/*+
  Questa funzione esegue un comando SQL.
  Parametri: 
  _ConnectionPtr &pConn;   Puntatore alla connessione database
  C_STRING &statement;     Istruzione SQL
  long *RecordsAffected;   Numero di record aggiornati dal comando (default = NULL)
  int retest;              Se MORETESTS -> in caso di errore riprova n volte 
                           con i tempi di attesa impostati poi ritorna GS_BAD,
                           ONETEST -> in caso di errore ritorna GS_BAD senza riprovare

  oppure

  _ConnectionPtr &pConn;   Puntatore alla connessione database
  const TCHAR *statement;  Istruzione SQL
  long *RecordsAffected;   Numero di record aggiornati dal comando (default = NULL)
  int retest;              Se MORETESTS -> in caso di errore riprova n volte 
                           con i tempi di attesa impostati poi ritorna GS_BAD,
                           ONETEST -> in caso di errore ritorna GS_BAD senza riprovare

  oppure

  _CommandPtr &pCmd;       Puntatore al comando già preparato (vedi gsc_PrepareCmd)
  long *RecordsAffected;   Numero di record aggiornati dal comando (default = NULL)
  int retest;              Se MORETESTS -> in caso di errore riprova n volte 
                           con i tempi di attesa impostati poi ritorna GS_BAD,
                           ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
  int PrintError;          Utilizzato solo se <retest> = ONETEST. 
                           Se il flag = GS_GOOD in caso di errore viene
                           stampato il messaggio relativo altrimenti non
                           viene stampato alcun messaggio (default = GS_GOOD)
                           (vedi "C_DBCONNECTION::CopyTable")

  oppure

  _CommandPtr &pCmd;       Puntatore al comando già preparato (vedi gsc_PrepareCmd)
  _RecordsetPtr &pRs;
  enum CursorTypeEnum CursorType; Tipo di cursore (default = adOpenForwardOnly)
  enum LockTypeEnum LockType;     Tipo di Lock (default = adLockReadOnly)
  int retest;                     Se MORETESTS -> in caso di errore riprova n volte 
                                  con i tempi di attesa impostati poi ritorna GS_BAD,
                                  ONETEST -> in caso di errore ritorna GS_BAD senza
                                  riprovare (default = MORETESTS)
  int PrintError;                 Utilizzato solo se <retest> = ONETEST. 
                                  Se il flag = GS_GOOD in caso di errore viene
                                  stampato il messaggio relativo altrimenti non
                                  viene stampato alcun messaggio (default = GS_GOOD)
                                  (vedi "C_DBCONNECTION::CopyTable")

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_ExeCmd(_ConnectionPtr &pConn, C_STRING &statement, long *RecordsAffected,
               int retest, int PrintError)
{
   return gsc_ExeCmd(pConn, statement.get_name(), RecordsAffected, retest, PrintError);
}
int gsc_ExeCmd(_ConnectionPtr &pConn, const TCHAR *statement, long *RecordsAffected,
               int retest, int PrintError)
{                              
   _CommandPtr pCmd;

   // preparo il comando
   if (gsc_PrepareCmd(pConn, statement, pCmd) == GS_BAD) return GS_BAD;
   
   return gsc_ExeCmd(pCmd, RecordsAffected, retest, PrintError);
}
int gsc_ExeCmd(_CommandPtr &pCmd, long *RecordsAffected, int retest, int PrintError)
{
   VARIANT _RecordsAffected;
   int     i = 1;

   _RecordsAffected.vt = VT_I4;

   do 
   {
      i++;
	   try
	   {
         // eseguo il comando
         // Attenzione in Postgres se il comando fallisce e ci si trova in una transazione
         // questa viene annullata con un rollback
         pCmd->Execute(&_RecordsAffected, NULL, adCmdText);
         if (RecordsAffected) *RecordsAffected = _RecordsAffected.lVal;
      }

	   catch (_com_error &e)
	   {
         C_RB_LIST ErrList;

         ErrList << gsc_get_DBErrs(e);

         if (retest != MORETESTS)
         {
            if (PrintError == GS_GOOD) gsc_PrintError(ErrList);

            if (gsc_is_IntegrityConstraint_DBErr(e, (_ConnectionPtr) pCmd->GetActiveConnection()))
               GS_ERR_COD = eGSIntConstr;
            else
               GS_ERR_COD = eGSExe;

            return GS_BAD;
         }
         else
         {
            if (i > GEOsimAppl::GLOBALVARS.get_NumTest())
            {
               int ReTry;

               if (PrintError == GS_GOOD)
                  ReTry = gsc_dderroradmin(ErrList, pCmd->CommandText);
               else
                  ReTry = GS_BAD;

               if (ReTry != GS_GOOD)
               {
                  if (gsc_is_IntegrityConstraint_DBErr(e, (_ConnectionPtr) pCmd->GetActiveConnection()))
                     GS_ERR_COD = eGSIntConstr;
                  else
                     GS_ERR_COD = eGSExe;

                  return GS_BAD;
               } // si abbandona

               i = 1;
            }
            else
               gsc_wait(GEOsimAppl::GLOBALVARS.get_WaitTime()); // attesa tra un tentativo e l'altro

            continue;
         }
      }
      break;
   }
   while (1);
   
   return GS_GOOD;
}
int gsc_ExeCmd(_ConnectionPtr &pConn, C_STRING &statement, _RecordsetPtr &pRs,
               enum CursorTypeEnum CursorType, enum LockTypeEnum LockType, int retest)
{
   return gsc_ExeCmd(pConn, statement.get_name(), pRs, CursorType, LockType, retest);
}
int gsc_ExeCmd(_ConnectionPtr &pConn, const TCHAR *statement, _RecordsetPtr &pRs,
               enum CursorTypeEnum CursorType, enum LockTypeEnum LockType, int retest)
{
   _CommandPtr pCmd;

   // preparo il comando
   if (gsc_PrepareCmd(pConn, statement, pCmd) == GS_BAD) return GS_BAD;
   
   return gsc_ExeCmd(pCmd, pRs, CursorType, LockType, retest);
}
int gsc_ExeCmd(_CommandPtr &pCmd, _RecordsetPtr &pRs, enum CursorTypeEnum CursorType,
               enum LockTypeEnum LockType, int retest)
{
	HRESULT    hr = E_FAIL;
   _variant_t vNull;
   int        i = 1;

   vNull.vt    = VT_ERROR;
   vNull.scode = DISP_E_PARAMNOTFOUND;

   // istanzio un recordset
   if (FAILED(pRs.CreateInstance(__uuidof(Recordset))))
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   do 
   {
      i++;
	   try
	   {
         pRs->PutRefSource(pCmd);
         // eseguo il comando
         if (FAILED(hr = pRs->Open(vNull, vNull, CursorType, LockType, adCmdUnknown)))
            _com_issue_error(hr);
      }

	   catch (_com_error &e)
	   {
         C_RB_LIST ErrList;

         ErrList << gsc_get_DBErrs(e);

         if (retest != MORETESTS)
         {
            pRs.Release();
            gsc_PrintError(ErrList);
            GS_ERR_COD = eGSExe;
            return GS_BAD;
         }
         else
         {
            if (i > GEOsimAppl::GLOBALVARS.get_NumTest())
            {
               if (gsc_dderroradmin(ErrList, pCmd->CommandText) != GS_GOOD)
               {  // si abbandona
                  pRs.Release();
                  GS_ERR_COD = eGSExe;
                  return GS_BAD;
               }

               i = 1;
            }
            else
               gsc_wait(GEOsimAppl::GLOBALVARS.get_WaitTime()); // attesa tra un tentativo e l'altro

            continue;
         }
      }
      break;
   }
   while (1);
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_ExeCmdFromFile                     <external> */
/*+
  Questa funzione esegue i comando SQL contenuti in un file testo che deve 
  essere preparato come segue:
  CONNSTRUDLFILE = stringa di connessione o file UDL
  UDLPROPERTIES = lista di proprietà di connessione da settare
  comandi SQL = se dopo un comando SQL c'è un altro comando SQL, il primo deve 
                terminare con il carattere ';' seguito da fine riga.
                Ogni comando SQL può essere diviso su più righe.
                (le stringhe non vanno divise es. "1234" non va come "12 <a capo> 34")
  Parametri:
  C_STRING &SQLFile;   Path del file con i comandi SQL da eseguire
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_ExeCmdFromFile(C_STRING &SQLFile)
{
   FILE           *f;
   C_STRING       Buffer, propName, propValue, ConnStrUDLFile, UdlPropertiesStr, statement;
   bool           Unicode;
   TCHAR          *str;
   C_2STR_LIST    UDLProperties;
   C_DBCONNECTION *pConn = NULL;

   // Carico la lista delle istruzioni SQL
   if ((f = gsc_open_profile(SQLFile, READONLY, MORETESTS, &Unicode)) == NULL)
      return GS_BAD;

   while (gsc_readline(f, Buffer, Unicode) == GS_GOOD)
   {
      Buffer.alltrim();
      if (Buffer.len() == 0) continue;
      if (Buffer.startWith(_T("--"))) continue; // riga di commento

      if (!pConn)
      {
         if ((Buffer.startWith(_T("CONNSTRUDLFILE"), FALSE) || Buffer.startWith(_T("CONNSTRUDLFILE"), FALSE)) && 
              ((str = Buffer.at(_T("="))) != NULL))
         {
            *str = _T('\0');
            str++;
            propName = Buffer; propName.alltrim();
            propValue = str; propValue.alltrim();
            if (propName.comp(_T("CONNSTRUDLFILE"), FALSE) == 0)
               ConnStrUDLFile = propValue;
            else
               UdlPropertiesStr = propValue;
         }
         else // si tratta di un istruzione SQL
         {
            if (gsc_path_exist(ConnStrUDLFile) == GS_GOOD)
               // se si tratta di un file e NON di una stringa di connessione
               if (gsc_nethost2drive(ConnStrUDLFile) == GS_BAD) // lo converto
                  { gsc_fclose(f); return GS_BAD; }

            if (UdlPropertiesStr.len() > 0)
            {
               // Conversione path UDLProperties da assoluto in dir relativo
               if (gsc_UDLProperties_nethost2drive(ConnStrUDLFile.get_name(),
                                                   UdlPropertiesStr) == GS_BAD)
                  { gsc_fclose(f); return GS_BAD; }
               if (gsc_PropListFromConnStr(UdlPropertiesStr.get_name(), UDLProperties) == GS_BAD)
                  { gsc_fclose(f); return GS_BAD; }
            }

            if ((pConn = GEOsimAppl::DBCONNECTION_LIST.get_Connection(ConnStrUDLFile.get_name(), 
                                                                      &UDLProperties)) == NULL)
               { gsc_fclose(f); return GS_BAD; }

            statement = Buffer;
         }
      }
      else
      {
         if (statement.get_name() != NULL) statement += _T(" ");
         statement += Buffer;
         if (Buffer.get_chr(Buffer.len() - 1) == _T(';')) // se ultima lettera della riga = ';'
         {
            if (pConn->ExeCmd(statement) != GS_GOOD)
               { gsc_fclose(f); return GS_BAD; }
            statement.clear();
         }
      }
   }

   gsc_fclose(f);

   if (statement.get_name() != NULL)
      if (pConn->ExeCmd(statement) != GS_GOOD) return GS_BAD;

   return GS_GOOD;
}


/*****************************************************************************/
/*  GESTIONE COMANDI SQL         -     FINE
/*  GESTIONE RECORDSET SQL       -     INIZIO
/*****************************************************************************/


/*********************************************************/
/*.doc gsc_DBSetIndexRs                           <external> */
/*+
  Questa funzione setta un indice per un recordset aperto in modalità
  adCmdTableDirect con cursore di tipo adUseServer.
  Parametri: 
  _RecordsetPtr &pRs;         Puntatore al recordset database
  const TCHAR* IndexName;     Nome dell'indice
  int PrintError;             Se il flag = GS_GOOD in caso di errore viene
                              stampato il messaggio relativo altrimenti non
                              viene stampato alcun messaggio (default = GS_GOOD)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_DBSetIndexRs(_RecordsetPtr &pRs, const TCHAR* IndexName, int PrintError)
{
if (gsc_DBIsClosedRs(pRs))
acutPrintf(_T("\nSi è chiuso il recordset!"));

   if (pRs.GetInterfacePtr() == NULL) return GS_GOOD;

   try
   {  // Se l'indice attule non è quello da impostare
      if (gsc_strcmp(pRs->Index, IndexName) != 0)
      {
         // Per baco di Microsoft sono costretto a fare una requery prima di settare
         // l'indice da usare.
         pRs->Requery(adOptionUnspecified);
         pRs->Index = (BSTR) NULL;
if (gsc_DBIsClosedRs(pRs))
acutPrintf(_T("\nSi è chiuso il recordset!"));
         pRs->Requery(adOptionUnspecified);
if (gsc_DBIsClosedRs(pRs))
acutPrintf(_T("\nSi è chiuso il recordset!"));
         pRs->Index = IndexName;
if (gsc_DBIsClosedRs(pRs))
acutPrintf(_T("\nSi è chiuso il recordset!"));
      }
   }

	catch (_com_error &e)
	{
      if (PrintError == GS_GOOD) printDBErrs(e);
      GS_ERR_COD = eGSExe;
if (gsc_DBIsClosedRs(pRs))
acutPrintf(_T("\nSi è chiuso il recordset!"));
      return GS_BAD;
   }
if (gsc_DBIsClosedRs(pRs))
acutPrintf(_T("\nSi è chiuso il recordset!"));

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_DBSeekRs                           <external> */
/*+
  Questa funzione ricerca con il metodo seek in un recordset.
  Parametri: 
  _RecordsetPtr &pRs;          Puntatore al recordset database
  const _variant_t &KeyValues; Valori chiave da ricercare
  enum SeekEnum SeekOption;    Modalità di ricerca (default = adSeekFirstEQ)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_DBSeekRs(_RecordsetPtr &pRs, const _variant_t &KeyValues, enum SeekEnum SeekOption)
{
   if (pRs.GetInterfacePtr() == NULL) return GS_BAD;

   try
	{
      pRs->Seek(KeyValues, adSeekFirstEQ);
   }

	catch (_com_error &e)
	{
      printDBErrs(e);
      GS_ERR_COD = eGSExe;
      return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_DBCloseRs                          <external> */
/*+
  Questa funzione chiude un recordset.
  Parametri: 
  _RecordsetPtr &pRs;   Puntatore al recordset database
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_DBCloseRs(_RecordsetPtr &pRs)
{
   if (pRs.GetInterfacePtr() == NULL) return GS_GOOD;

   try
	{
      pRs->Close();
      pRs.Release();
   }

   #if defined(GSDEBUG) // se versione per debugging
	   catch (_com_error &e)
      {
      printDBErrs(e);
   #else
	   catch (_com_error)
	   {
   #endif
         GS_ERR_COD = eGSTermCsr;
         return GS_BAD;
      }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_DBIsClosedRs                       <external> */
/*+
  Questa funzione verifica se il recordset è chiuso o aperto.
  Parametri: 
  _RecordsetPtr &pRs;   Puntatore al recordset database
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_DBIsClosedRs(_RecordsetPtr &pRs)
{
   ObjectStateEnum mState;

   if (pRs.GetInterfacePtr() == NULL) return GS_GOOD;

   try
	{
      mState = (ObjectStateEnum) pRs->State;
      return (mState == adStateClosed) ? GS_GOOD : GS_BAD;
   }

   #if defined(GSDEBUG) // se versione per debugging
   	catch (_com_error &e)
	   {
         printDBErrs(e);
         return GS_GOOD;
      }
   #else
   	catch (_com_error)
	   {
         return GS_GOOD;
      }
   #endif

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_DBOpenRs                           <external> */
/*+
  Questa funzione apre un recordset.
  Parametri: 
  _ConnectionPtr &pConn;          Puntatore alla connessione database
  const TCHAR *statement;         Istruzione
  _RecordsetPtr &pRs;
  enum CursorTypeEnum CursorType; Tipo di cursore (default = adOpenForwardOnly)
  enum LockTypeEnum LockType;     Tipo di Lock (default = adLockReadOnly)
  long Options;                   Opzioni varie es. adCmdTable, adCmdText 
                                  (default = adCmdText)
  int retest;                     Se MORETESTS -> in caso di errore riprova n volte 
                                  con i tempi di attesa impostati poi ritorna GS_BAD,
                                  ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                                  (default = MORETESTS)
  int PrintError;                 Utilizzato solo se <retest> = ONETEST. 
                                  Se il flag = GS_GOOD in caso di errore viene
                                  stampato il messaggio relativo altrimenti non
                                  viene stampato alcun messaggio (default = GS_GOOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_DBOpenRs(_ConnectionPtr &pConn, const TCHAR *statement, _RecordsetPtr &pRs,
                 enum CursorTypeEnum CursorType, enum LockTypeEnum LockType,
                 long Options, int retest, int PrintError)
{
   int i = 1;    

   // istanzio un recordset
   if (FAILED(pRs.CreateInstance(__uuidof(Recordset))))
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   do 
   {
      i++;
	   try
      {  // apro il recordset
         pRs->Open(statement, pConn.GetInterfacePtr(), CursorType, LockType, Options);
      }

	   catch (_com_error &e)
	   {
         C_RB_LIST ErrList;

         ErrList << gsc_get_DBErrs(e, pConn);

         if (retest != MORETESTS)
         {
            if (PrintError == GS_GOOD) gsc_PrintError(ErrList);
            pRs.Release();
            GS_ERR_COD = eGSConstrCsr;
            return GS_BAD;
         }
         else
         {
            if (i > GEOsimAppl::GLOBALVARS.get_NumTest())
            {
               if (gsc_dderroradmin(ErrList) != GS_GOOD)
                  { pRs.Release(); GS_ERR_COD = eGSConstrCsr; return GS_BAD; } // si abbandona

               i = 1;
            }
            else
               gsc_wait(GEOsimAppl::GLOBALVARS.get_WaitTime()); // attesa tra un tentativo e l'altro

            continue;
         }
      }
      break;
   }
   while (1);

   return GS_GOOD;
}
// Il recordset deve essere aperto in modalità adLockPessimistic
int gsc_DBLockRs(_RecordsetPtr &Rs)
{
   _ConnectionPtr pConn = Rs->GetActiveConnection();
   FieldPtr       pFld;
   long           nField = 0, Attributes;

   try
   {
      // Cerco la prima colonna modificabile che non sia di tipo BLOB
      while (nField < Rs->Fields->Count)
      {
         pFld = Rs->Fields->GetItem(nField);
         pFld->get_Attributes(&Attributes);

         // se il campo è aggiornabile
         if (Attributes & adFldUpdatable) break;
         nField++;
      }
      if (nField >= Rs->Fields->Count) // Non trovato
         { gsc_DBUnLockRs(Rs, READONLY, true); return GS_BAD; }
      pFld->Value = pFld->Value;
   }

   #if defined(GSDEBUG) // se versione per debugging
	   catch (_com_error &e)
	   {
         printDBErrs(e);
   #else
	   catch (_com_error)
	   {
   #endif
         GS_ERR_COD = eGSLocked;
         gsc_DBUnLockRs(Rs, READONLY);
         return GS_BAD;
      }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_DBUnLockRs                         <external> */
/*+
  Questa funzione sblocca tutti il record corrente del recordset
  (vedi funzione "gsc_DBLockRs").
  Parametri: 
  _RecordsetPtr &pRs;      recordset
  int            mode;     Flag se = UPDATEABLE viene aggiornato ogni 
                           recordset altrimenti vengono abbandonate
                           le modifiche (default = UPDATEABLE)
  bool           CloseRs;  Flag di chiusura del Rs (default = false)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_DBUnLockRs(_RecordsetPtr &pRs, int mode, bool CloseRs)
{
   try
   {
      if (pRs->EditMode != adEditNone)
      {
         if (mode != UPDATEABLE) pRs->CancelUpdate();
         else pRs->Update();
      }
      if (CloseRs) gsc_DBCloseRs(pRs);
   }

	catch (_com_error &e)
	{
      printDBErrs(e);
      return GS_BAD;
   }

   return GS_GOOD;
}


/****************************************************************/
/*.doc gsc_OpenSchema                                <external> */
/*+
  Questa funzione apre un recordset relativo allo schema indicato.
  I Providers non sono abbligati a supportare tutti le query di schema standard OLE DB.
  N.B.
  In particolare solo adSchemaTables, adSchemaColumns e adSchemaProviderTypes sono 
  obbligatoriamente supportati. Se si è in una transazione e viene richiamata questa
  funzione usando un tipo di schema non supprtato, la tranazione verrà interrotta
  da un rollback.

  Parametri: 
  _ConnectionPtr &pConn;          Puntatore alla connessione database
  SchemaEnum  Schema;             Nome schema
  _RecordsetPtr &pRs;             Risultato
  const _variant_t &Restrictions; Condizioni di filtro (default = vtMissing)
  int retest;                     se MORETEST -> in caso di errore riprova n volte 
                                  con i tempi di attesa impostati poi ritorna GS_BAD,
                                  ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                                  (default = MORETESTS)
  int PrintError;                 Utilizzato solo se <retest> = ONETEST. 
                                  Se il flag = GS_GOOD in caso di errore viene
                                  stampato il messaggio relativo altrimenti non
                                  viene stampato alcun messaggio (default = GS_GOOD)

  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int gsc_OpenSchema(_ConnectionPtr &pConn, SchemaEnum Schema, _RecordsetPtr &pRs,
                   const _variant_t &Restrictions, int retest, int PrintError)
{
   int i = 1;    

   // istanzio un recordset
   if (FAILED(pRs.CreateInstance(__uuidof(Recordset))))
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   do 
   {
      i++;
      try
	   {
         pRs = pConn->OpenSchema(Schema, Restrictions);
      }

	   catch (_com_error &e)
      {
         C_RB_LIST ErrList;

         ErrList << gsc_get_DBErrs(e, pConn);

         if (retest == ONETEST)
         {
            if (PrintError == GS_GOOD) gsc_PrintError(ErrList);
            GS_ERR_COD = eGSOpenSchema;
            return GS_BAD;
         }
         else
         {
            if (i > GEOsimAppl::GLOBALVARS.get_NumTest())
            {
               if (gsc_dderroradmin(ErrList) != GS_GOOD)
                  { GS_ERR_COD = eGSOpenSchema; return NULL; } // si abbandona

               i = 1;
            }
            else
               gsc_wait(GEOsimAppl::GLOBALVARS.get_WaitTime()); // attesa tra un tentativo e l'altro

            continue;
         }
      }
	   catch (...)
      {
         GS_ERR_COD = eGSOpenSchema;
         return GS_BAD;
      }

      break;
   }
   while (1);

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_InitDBReadRow                      <external> */
/*+
  Questa funzione inizializza una C_RB_LIST per la lettura di righe
  del recordset.
  Parametri: 
  _RecordsetPtr &pRs;   Puntatore al recordset database
  C_RB_LIST &ColValues; Lista per lettura righe
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_InitDBReadRow(_RecordsetPtr &pRs, C_RB_LIST &ColValues)
{
   FieldPtr pFld;
   BSTR     pbstr;
   long     nFlds = pRs->Fields->Count, i;

   ColValues.remove_all();
   if (nFlds == 0) { GS_ERR_COD = eGSReadRow; return GS_BAD; }

   ColValues << acutBuildList(RTLB, 0);

 	DataTypeEnum DataType; // test

	try
	{
      for (i = 0; i < nFlds; i++)
      {
         pFld = pRs->Fields->GetItem(i);
         pFld->get_Name(&pbstr);

         pFld->get_Type(&DataType); // test

         if ((ColValues += acutBuildList(RTLB, RTSTR, pbstr, RTNIL, RTLE, 0)) == NULL)
            return GS_BAD;

         SysFreeString(pbstr);
      }
   }

   catch (_com_error &e)
	{
      printDBErrs(e);
      GS_ERR_COD = eGSReadStruct;
      return GS_BAD;
   }

   if ((ColValues += acutBuildList(RTLE, 0)) == NULL) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_DBReadRows                         <external> */
/*+
  Questa funzione legge tutte le righe fin in fondo al recordset.
  Parametri: 
  _RecordsetPtr &pRs;   Puntatore al recordset database
  C_RB_LIST &ColValues; Lista dei valori da leggere
                        (((<attr><val>)...) ((<attr><val>)...) ...)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
*/  
/*********************************************************/
int gsc_DBReadRows(_RecordsetPtr &pRs, C_RB_LIST &ColValues)
{
   C_RB_LIST Buffer;

   if (!(ColValues << acutBuildList(RTLB, 0))) return GS_BAD;

   while (gsc_isEOF(pRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pRs, Buffer) == GS_BAD) return GS_BAD; 
      if (!(ColValues += gsc_rblistcopy(Buffer.get_head()))) return GS_BAD;
      gsc_Skip(pRs);
   }

   if (ColValues.GetCount() == 1)
      ColValues.remove_all();
   else
      if (!(ColValues += acutBuildList(RTLE, 0))) return GS_BAD;

   return GS_GOOD;
}

/*********************************************************/
/*.doc gsc_DBReadRow                          <external> */
/*+
  Questa funzione legge una riga del recordset.
  Parametri: 
  _RecordsetPtr &pRs;   Puntatore al recordset database
  C_RB_LIST &Values;    Lista dei valori da leggere (se non è già stata 
                        inizializzata viene inizializzata automaticamente
                        vedi "gsc_InitDBReadRow")
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
*/  
/*********************************************************/
int gsc_DBReadRow(_RecordsetPtr &pRs, C_RB_LIST &Values)
{
   FieldPtr pFld;
   presbuf  pValue;
   long     nFlds = pRs->Fields->Count;

   if (gsc_isEOF(pRs) == GS_GOOD || gsc_isBOF(pRs) == GS_GOOD)
      { GS_ERR_COD = eGSReadRow; return GS_BAD; }

   if (!Values.get_head()) // da inizializzare
      if (gsc_InitDBReadRow(pRs, Values) == GS_BAD) return GS_BAD;

   pValue = Values.get_head();

   for (long i = 0; i < nFlds; i++)
   {
      pFld = pRs->Fields->GetItem(i);

      if (!(pValue = pValue->rbnext) || !(pValue = pValue->rbnext) || !(pValue = pValue->rbnext))
         // Values passato come parametro era sbagliato quindi lo inizializzo
         if (gsc_InitDBReadRow(pRs, Values) == GS_BAD) return GS_BAD;
         else // ricomincio il ciclo
         {
            pValue = Values.get_head();
            i = -1;
            continue;
         }
      if (gsc_DBReadField(pFld, pValue) == GS_BAD) return GS_BAD;
      pValue = pValue->rbnext;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_DBReadField                        <external> */
/*+
  Questa funzione legge il valore di un campo e lo memorizza in un resbuf
  già allocato.
  Parametri: 
  FieldPtr &pFld;       Puntatore al campo database
  presbuf  pValue;      Puntatore al valore letto (resbuf già allocato in C_RB_LIST
                        che è l'unica classe in grado di rilasciare dati binari)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_DBReadField(FieldPtr &pFld, presbuf pValue)
{
	DataTypeEnum DataType;
   
   try
	{
      if (pFld->Value.vt == VT_EMPTY || pFld->Value.vt == VT_NULL)
         return gsc_RbSubstNONE(pValue);

      pFld->get_Type(&DataType);

      switch (DataType)
      {
         case adTinyInt:
         case adSmallInt:
         case adInteger:
         case adBigInt:
         case adUnsignedTinyInt:
         case adUnsignedSmallInt:
         case adUnsignedInt:
         case adUnsignedBigInt:
         case adSingle:
         case adDouble:
         case adCurrency:
         case adDecimal:
         case adNumeric:
         case adBoolean:
         case adDate:
         case adDBDate:
         case adDBTimeStamp:
         case adBSTR:
         case adChar:
         case adWChar:
         case adBinary:
         case adVarBinary:
         case adFileTime:
            if (gsc_Variant2Rb(pFld->Value, pValue) == GS_BAD) return GS_BAD;
            break;
         case adDBTime:
            // poichè pFld->Value contiene la data odierna, depuro il dato e lascio solo l'ora
            if (gsc_Variant2Rb(pFld->Value, pValue) == GS_BAD) return GS_BAD;
            if (pValue->restype == RTSTR)
            {
               C_STRING TimeValue;
               gsc_getSQLTime(pValue->resval.rstring, TimeValue);
               gsc_RbSubst(pValue, TimeValue.get_name());
            }
            break;
         case adEmpty:
         case adError:
         case adVariant:
         case adIDispatch:
         case adIUnknown:
         case adGUID:
         case adChapter:
         case adPropVariant:
            if (gsc_RbSubstNONE(pValue) == GS_BAD) return GS_BAD;
            break;
         case adVarChar:
         case adLongVarChar:
         case adVarWChar:
         case adLongVarWChar:
         case adLongVarBinary:
         {
            long Attributes;

            pFld->get_Attributes(&Attributes);
            if (Attributes & adFldLong) // si può usare GetChunk
            {
               TCHAR *Value = NULL;
               long Size;
   
               if (gsc_GetBLOB(pFld, &Value, &Size) == GS_BAD) return GS_BAD;

               if (DataType == adLongVarBinary)
               {  // dato binario
                  // I valori BLOB non vengono trattati perchè il resbuf non è adatto a questo scopo.
                  // if (gsc_RbSubst(pValue, Value, Size) == GS_BAD) return GS_BAD;
                  if (gsc_RbSubstNONE(pValue) == GS_BAD) return GS_BAD;
               }
               else // stringa (memo)
                  if (gsc_RbSubst(pValue, Value) == GS_BAD) return GS_BAD;
               
               if (Value) free(Value);
            }
            else
            {
               if (DataType == adLongVarBinary)
               {  // dato binario
                  // I valori BLOB non vengono trattati perchè il resbuf non è adatto a questo scopo.
                  // if (gsc_RbSubst(pValue, (TCHAR*)(_bstr_t) pFld->Value, pFld->GetActualSize()) == GS_BAD) return GS_BAD;
                  if (gsc_RbSubstNONE(pValue) == GS_BAD) return GS_BAD;
               }
               else // stringa (memo)
                  if (gsc_RbSubst(pValue, (TCHAR*)(_bstr_t) pFld->Value) == GS_BAD) return GS_BAD;
            }     
            break;
         }
         case adVarNumeric:
            if (gsc_RbSubst(pValue, (double) pFld->Value) == GS_BAD) return GS_BAD;
            break;
         default:
            if (gsc_RbSubstNONE(pValue) == GS_BAD) return GS_BAD;
      }
   }

   catch (_com_error &e)
	{
      if (e.Error() != -2146825267) // No current row
         printDBErrs(e);
      GS_ERR_COD = eGSReadRow;
      return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_isUdpateableField                  <external> */
/*+
  Questa funzione valuta se un campo contenuto nel recordset è aggiornabile.
  Parametri:
  _RecordsetPtr &pRs;         RecordSet
  const TCHAR   *FieldName;   Nome del campo da analizzare

  Restituisce GS_GOOD se attributo sicuramente modificabile, GS_CAN se non si può sapere
  e GS_BAD se campo non modificabile.
-*/  
/*********************************************************/
int gsc_isUdpateableField(_RecordsetPtr &pRs, const TCHAR *FieldName)
{ 
   FieldPtr   pField;
   _variant_t Val;
   long       AttributesFld;

   try
	{
      gsc_set_variant_t(Val, FieldName); // di comodo
      pField        = pRs->Fields->GetItem(Val);
      AttributesFld = pField->GetAttributes();

      // se è aggiornabile
      if (AttributesFld & adFldUpdatable) return GS_GOOD;
      // se non si riesce a determinare se è aggiornabile
      if (AttributesFld & adFldUnknownUpdatable)
         // se il campo può essere nullo lo considero aggiornabile
         if ((AttributesFld & adFldIsNullable) && (AttributesFld & adFldMayBeNull))
            return GS_GOOD;
         else
            return GS_CAN;
   }

   catch (_com_error)
	{
      GS_ERR_COD = eGSInvAttribName;
   }

   // se arriva qui non è aggiornabile
   return GS_BAD;
}


/*********************************************************/
/*.doc gsc_Skip                               <external> */
/*+
  Questa funzione posiziona il cursore di un'istruzione SQL n posizioni
  avanti (se n>0) o indietro (se n<0) rispetto la posizione attuale nel
  recordset di dati.
  Parametri:
  _RecordsetPtr &pRs;   Puntatore al recordset database
  long          OffSet; Posizione relativa della riga (default = 1)
  
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
  N.B.: Non setta GS_ERR_COD !
-*/  
/*********************************************************/
int gsc_Skip(_RecordsetPtr &pRs, long posiz)
{
   HRESULT hr = E_FAIL;

   try
	{  // Il Move di -1 su driver a 64 bit in versione release dà errore
      if (FAILED(hr = pRs->Move(posiz))) _com_issue_error(hr);
   }

#if defined(GSDEBUG) // se versione per debugging
	catch (_com_error &e)
	{
      printDBErrs(e);
#else
   catch (_com_error)
	{
#endif
      GS_ERR_COD = eGSSkip;
      return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_Skip                               <external> */
/*+
  Questa funzione posiziona il cursore di un'istruzione SQL all'inizio
  del recordset di dati.
  Parametri:
  _RecordsetPtr &pRs;   Puntatore al recordset database
  
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
  N.B.: Non setta GS_ERR_COD !
-*/  
/*********************************************************/
int gsc_MoveFirst(_RecordsetPtr &pRs)
{
   HRESULT hr = E_FAIL;

   try
	{
      if (FAILED(hr = pRs->MoveFirst())) _com_issue_error(hr);
   }

   catch (_com_error)
	{
      GS_ERR_COD = eGSSkip;
      return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_isEOF                              <external> */
/*+
  Questa funzione ritorna TRUE se ci si trova alla fine del recordset.
  Parametri:
  _RecordsetPtr &pRs;   Puntatore al recordset database
-*/  
/*********************************************************/
int gsc_isEOF(_RecordsetPtr &pRs)
{
   int result;

   try
	   { result = (pRs->EndOfFile == VARIANT_TRUE) ? TRUE : FALSE; }

   catch (_com_error &e)
	{
      printDBErrs(e);
      return GS_BAD;
   }

   return result;
}


/*********************************************************/
/*.doc gsc_isBOF                              <external> */
/*+
  Questa funzione ritorna TRUE se ci si trova all'inizio del recordset.
  Parametri:
  _RecordsetPtr &pRs;   Puntatore al recordset database
-*/  
/*********************************************************/
int gsc_isBOF(_RecordsetPtr &pRs)
{
   int result;

   try
	   { result = (pRs->BOF == VARIANT_TRUE) ? TRUE : FALSE; }

   catch (_com_error &e)
	{
      printDBErrs(e);
      return GS_BAD;
   }

   return result;
}


/*********************************************************/
/*.doc gsc_RowsQty                            <external> */
/*+
  Questa funzione ritorna il numero delle righe del recordset.
  Parametri:
  _RecordsetPtr &pRs;   Puntatore al recordset database
  long          *qty;   n. di record
  
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
  N.B. il recordset NON DEVE essere stato aperto con cursore forward-only e
  DEVE essere di tipo static o keyset.
  qty = -1 se la funzione non è in grado di trovare i records
-*/  
/*********************************************************/
int gsc_RowsQty(_RecordsetPtr &pRs, long *qty)
{
	HRESULT hr = E_FAIL;

   try
	{
      if (FAILED(hr = pRs->get_RecordCount(qty)))
         _com_issue_error(hr);
   }

   catch (_com_error &e)
	{
      printDBErrs(e);
      return GS_BAD;
   }

   if (*qty == -1) // li conto con un ciclo
   {
      try
	   {
         *qty = 0;
         pRs->MoveFirst();
         while (gsc_isEOF(pRs) == GS_BAD) 
         {
            (*qty)++;
            gsc_Skip(pRs);
         }
         pRs->MoveFirst();
      }

      catch (_com_error)
	   {
         *qty = 0;
         return GS_GOOD;
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_DBDelRow                           <external> */
/*+
  Questa funzione cancella la riga attuale in un set di dati.
  Parametri:
  _RecordsetPtr &pRs; Puntatore al recordset database
  int retest;         se MORETEST -> in caso di errore riprova n volte 
                      con i tempi di attesa impostati poi ritorna GS_BAD,
                      ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                      (default = MORETESTS)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_DBDelRow(_RecordsetPtr &pRs, int retest)
{
	HRESULT hr = E_FAIL;
   int     i = 1;    

   do 
   {
      i++;
	   try
	   {
         // eseguo il comando
         if (FAILED(hr = pRs->Delete(adAffectCurrent))) _com_issue_error(hr);
      }

	   catch (_com_error &e)
	   {
         C_RB_LIST ErrList;

         ErrList << gsc_get_DBErrs(e);

         if (retest != MORETESTS)
         {
            gsc_PrintError(ErrList);
            GS_ERR_COD = eGSExe;
            return GS_BAD;
         }
         else
         {
            if (i > GEOsimAppl::GLOBALVARS.get_NumTest())
            {
               if (gsc_dderroradmin(ErrList) != GS_GOOD)
                  { GS_ERR_COD = eGSDelRow; return GS_BAD; } // si abbandona

               i = 1;
            }
            else
               gsc_wait(GEOsimAppl::GLOBALVARS.get_WaitTime()); // attesa tra un tentativo e l'altro

            continue;
         }
      }
      break;
   }
   while (1);

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_DBDelRows                          <external> */
/*+
  Questa funzione cancella tutte le righe del recordset.
  Parametri:
  _RecordsetPtr &pRs; Puntatore al recordset database
  int retest;         se MORETEST -> in caso di errore riprova n volte 
                      con i tempi di attesa impostati poi ritorna GS_BAD,
                      ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                      (default = MORETESTS)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_DBDelRows(_RecordsetPtr &pRs, int retest)
{
	HRESULT hr = E_FAIL;
   int     i = 1;    

   do 
   {
      i++;
	   try
	   {
         while (gsc_isEOF(pRs) == GS_BAD)
         {  // eseguo il comando
            if (FAILED(hr = pRs->Delete(adAffectCurrent))) _com_issue_error(hr);
            gsc_Skip(pRs);
         }
      }

	   catch (_com_error &e)
	   {
         C_RB_LIST ErrList;

         ErrList << gsc_get_DBErrs(e);

         if (retest != MORETESTS)
         {
            gsc_PrintError(ErrList);
            GS_ERR_COD = eGSExe;
            return GS_BAD;
         }
         else
         {
            if (i > GEOsimAppl::GLOBALVARS.get_NumTest())
            {
               if (gsc_dderroradmin(ErrList) != GS_GOOD)
                  { GS_ERR_COD = eGSDelRow; return GS_BAD; } // si abbandona

               i = 1;
            }
            else
               gsc_wait(GEOsimAppl::GLOBALVARS.get_WaitTime()); // attesa tra un tentativo e l'altro

            continue;
         }
      }
      break;
   }
   while (1);

   return GS_GOOD;
}


/*****************************************************************************/
/*  GESTIONE RECORDSET SQL       -     FINE
/*  ISTRUZIONI DI UTILITA'   -     INIZIO
/*****************************************************************************/


/****************************************************************/
/*.doc gsc_CreateACCDB                              <external> */
/*+
  Funzione che crea un nuovo database ACCESS con estensione ACCDB.
  Se la path del database non esiste viene creata.
  Parametri: 
  const TCHAR *ACCDBFile;   Path completa file ACCDB
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int gs_CreateACCDB(void)
{
   presbuf arg = acedGetArgs();

   acedRetNil();
   if (!arg || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (gsc_CreateACCDB(arg->resval.rstring) == GS_GOOD) 
      { acedRetT(); return RTNORM; }

   return RTERROR;
}
int gsc_CreateACCDB(const TCHAR *ACCDBFile)
{
   C_STRING ACCDBSample;

   if (gsc_path_exist(ACCDBFile) == GS_GOOD) return GS_GOOD;

   if (gsc_mkdir_from_path(ACCDBFile) == GS_BAD) return GS_BAD;

   ACCDBSample = GEOsimAppl::GEODIR;
   ACCDBSample += _T("\\Data Links\\");
   ACCDBSample += ACCESSSAMPLEDB;
   // creo database copiando un file "campione"
   if (gsc_copyfile(ACCDBSample.get_name(), ACCDBFile) == GS_BAD)
      { gsc_rmdir_from_path(ACCDBFile); return GS_BAD; }

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_ConvMDBToACCDB                                                 */
/*+
  Questa funzione converte un file MDB in ACCDB copiando solo le tabelle da un
  database all'altro.
  Parametri:
  C_STRING &MDBFile;    Path completa del file MDB

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
int gsc_ConvMDBToACCDB(C_STRING &MDBFile)
{
   C_STRING       MyFile(MDBFile), drive, dir, name, FileACCDB;
   C_DBCONNECTION *pSourceConn, *pDestConn;
   C_STR_LIST     ItemList;
   C_STR          *pItem;
   C_2STR_LIST    UDLProps;

   // traduco dir da assoluto in dir relativo
   if (gsc_nethost2drive(MyFile) == GS_BAD) return GS_BAD;

   // Mi collego al nuovo ACCDB
   gsc_splitpath(MyFile, &drive, &dir, &name, NULL);
   FileACCDB = drive;
   FileACCDB += dir;
   FileACCDB += name;
   FileACCDB += _T(".ACCDB");
   // Se il file esiste già
   if (gsc_path_exist(FileACCDB) == GS_GOOD)
      { GS_ERR_COD = eGSPathAlreadyExisting; return GS_BAD; }

   UDLProps.add_tail_2str(_T("Data Source"), FileACCDB.get_name());
   if ((pDestConn = GEOsimAppl::DBCONNECTION_LIST.get_Connection(NULL, &UDLProps)) == NULL)
      return GS_BAD;

   // Mi collego al vecchio MDB
   UDLProps.remove_all();
   UDLProps.add_tail_2str(_T("Data Source"), MyFile.get_name());
   if ((pSourceConn = GEOsimAppl::DBCONNECTION_LIST.get_Connection(NULL, &UDLProps)) == NULL)
   {
      // termino tutte le connessioni OLE-DB
      GEOsimAppl::DBCONNECTION_LIST.remove_all();
      return GS_BAD; 
   }

   acutPrintf(_T("%s %s..."), gsc_msg(594), MyFile.get_name());  // "\nConversione"

   // Copio tutte le tabelle
   if (pSourceConn->getTableAndViewList(ItemList, NULL, NULL, NULL,
                                        true, _T("TABLE"), false) == GS_BAD)
   {
      // termino tutte le connessioni OLE-DB
      GEOsimAppl::DBCONNECTION_LIST.remove_all();
      return GS_BAD; 
   }
   pItem = (C_STR *) ItemList.get_head();
   while (pItem)
   {
      if (pSourceConn->CopyTable(pItem->get_name(), 
                                 *pDestConn, pItem->get_name()) == GS_BAD)
      {
         // termino tutte le connessioni OLE-DB
         GEOsimAppl::DBCONNECTION_LIST.remove_all();
         return GS_BAD; 
      }

      pItem = (C_STR *) ItemList.get_next();
   }

   return GS_GOOD;
}


/****************************************************************/
/*.doc gsc_BlobToVariant                             <internal> */
/*+
  Funzione che crea un safe array ed inizializza il dato in VARIANT
  Parametri: 
  void *pValue;      Dato
  long Size;         dimensione del dato
  VARIANT &varArray; out
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int gsc_BlobToVariant(void *pValue, long Size, VARIANT &varArray)
{
	BYTE *pByte;
	
	try
	{
		SAFEARRAY FAR* psa;
		SAFEARRAYBOUND rgsabound[1];
		rgsabound[0].lLbound = 0;	
		rgsabound[0].cElements = Size * sizeof(TCHAR);

		// crea un array ad una dimensione di byte
		psa = SafeArrayCreate(VT_I1, 1, rgsabound);
			
		// setto il dato dell'array con il dato
		if (SafeArrayAccessData(psa, (void **)&pByte) == NOERROR)
			memcpy((LPVOID) pByte, pValue, Size);
	   SafeArrayUnaccessData(psa);

		varArray.vt = VT_ARRAY | VT_UI1;
		varArray.parray = psa;
	}
	
   catch(_com_error &e)
	{
      printDBErrs(e);
      GS_ERR_COD = eGSUpdRow;
      return GS_BAD;
	}

   return GS_GOOD;
}


/****************************************************************/
/*.doc gsc_GetBLOB                                   <external> */
/*+
  Lettura di un dato BLOB da tabella.
  Parametri: 
  FieldPtr &pFld;     Campo della tabella
  CString &Value;     Stringa risultato
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int gsc_GetBLOB(FieldPtr &pFld, TCHAR **pValue, long *Size)
{
	_variant_t varBLOB;
	size_t     nDataLenRetrieved = 0;
   TCHAR      *pBuf = NULL;

   *pValue = NULL;

	try
	{
		// lunghezza attuale del dato
		*Size = pFld->ActualSize;
		if (*Size > 0)
      {
         VariantInit(&varBLOB);
			// leggo il chunk del dato 
			varBLOB = pFld->GetChunk(*Size);
			
			// Se il dato letto è un array di bytes allora leggo il dato dall'array
			if (varBLOB.vt == (VT_ARRAY | VT_UI1))
			{
				SafeArrayAccessData(varBLOB.parray,(void **) &pBuf);

            // alloco buffer
            if ((*pValue = (TCHAR *) calloc(*Size, sizeof(TCHAR))) == NULL)
               { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            wmemcpy_s(*pValue, *Size * sizeof(TCHAR), pBuf, *Size * sizeof(TCHAR));
				SafeArrayUnaccessData(varBLOB.parray);
			}
         else
            if (varBLOB.vt == VT_BSTR)
            {
               CString Buffer(varBLOB);
               // alloco buffer len + 1 per carattere fine stringa
               if ((*pValue = (TCHAR *) calloc(Buffer.GetLength() + 1, sizeof(TCHAR))) == NULL)
                  { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

               wcscpy_s(*pValue, Buffer.GetLength() * sizeof(TCHAR), Buffer);
            }
		}
		// alcuni providers ritornano -1 come lunghezza del dato 
		// in quel caso ciclo per leggere il dato in chunks
		else
		if (*Size < 0)
		{
			do
			{
				VariantInit(&varBLOB);
				varBLOB = pFld->GetChunk((long)CHUNKSIZE);
				
			   // Se il dato letto è un array di bytes allora leggo il dato dall'array
				if (varBLOB.vt == (VT_ARRAY | VT_UI1))
				{
               SafeArrayAccessData(varBLOB.parray,(void **)&pBuf);
					// leggo la lunghezza del chunk ottenuto
					nDataLenRetrieved = varBLOB.parray->rgsabound[0].cElements;

					if(!(*pValue))
               {  
                  // alloco buffer
                  if ((*pValue = (TCHAR *) calloc(nDataLenRetrieved, sizeof(TCHAR))) == NULL)
                     { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         			wmemcpy_s(*pValue, nDataLenRetrieved, pBuf, nDataLenRetrieved);
                  *Size = (long) nDataLenRetrieved;
               }
					else
               {
                  // rialloco buffer
                  if ((*pValue = (TCHAR *) realloc(*pValue, (*Size + nDataLenRetrieved) * sizeof(TCHAR))) == NULL)
                     { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         			wmemcpy_s(*pValue + *Size, nDataLenRetrieved, pBuf, nDataLenRetrieved);
                  *Size =+ (long) nDataLenRetrieved;
               }
					SafeArrayUnaccessData(varBLOB.parray);
				}
            else
               if (varBLOB.vt == VT_BSTR)
               {
                  pBuf = (TCHAR *) (_bstr_t) varBLOB;
                  nDataLenRetrieved = wcslen(pBuf);
					   if(!(*pValue))
                  {  
                     // alloco buffer
                     if ((*pValue = (TCHAR *) calloc(nDataLenRetrieved + 1, sizeof(TCHAR))) == NULL)
                        { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         			   wmemcpy_s(*pValue, nDataLenRetrieved, pBuf, nDataLenRetrieved);
                     *Size = (long) nDataLenRetrieved;
                  }
                  else
                  {
                     // rialloco buffer
                     if ((*pValue = (TCHAR *) realloc(*pValue, (*Size + nDataLenRetrieved) * sizeof(TCHAR))) == NULL)
                        { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         			   wmemcpy_s(*pValue + *Size, nDataLenRetrieved, pBuf, nDataLenRetrieved);
                     *Size =+ (long) nDataLenRetrieved;
                  }
               }
			}
			while (nDataLenRetrieved == CHUNKSIZE); // ciclo per leggere il dato in chunks (dimensione CHUNKSIZE)
		}
	}
   
   catch (_com_error &e)
	{
      printDBErrs(e);
      if (*pValue) free(*pValue);
      GS_ERR_COD = eGSReadRow;
      return GS_BAD;
   }

   return GS_GOOD;
}


/****************************************************************/
/*.doc gsc_SetBLOB                                   <external> */
/*+
  Scrittura di un dato BLOB in tabella.
  Parametri: 
  void *pValue             Stringa BLOB
  long Size;               Lunghezza BLOB
  FieldPtr &pFld;          Campo della tabella
  oppure:
  void *pValue;            Stringa BLOB
  long Size;               Lunghezza BLOB
  _ParameterPtr &pParam;   Parametro di un comando
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int gsc_SetBLOB(void *pValue, long Size, FieldPtr &pFld)
{
	VARIANT varBLOB;
   long    OffSet = 0, UsedSizeBuff;
   TCHAR   Buff[CHUNKSIZE];

   try
	{
      while (OffSet < Size)
      {
         UsedSizeBuff = ((OffSet + CHUNKSIZE) <= Size) ? CHUNKSIZE : Size - OffSet;
         // roby probabile errore di cast: buf non dovrebbe essere char*
         wmemcpy_s(Buff, CHUNKSIZE, ((TCHAR *) pValue) + OffSet, CHUNKSIZE);

		   // ottengo un dato VARIANT da un dato BLOB
		   if (gsc_BlobToVariant(Buff, UsedSizeBuff, varBLOB) == GS_BAD) return GS_BAD;
		   // setto il valore del campo
		   pFld->AppendChunk(varBLOB);

   		VariantClear(&varBLOB);
         OffSet += CHUNKSIZE;
      }
	}

   catch(_com_error e)
	{
      printDBErrs(e);
      GS_ERR_COD = eGSUpdRow;
      return GS_BAD;
   }

   return GS_GOOD;
}
// questa funzione non può scrivere un dato BLOB molto grande (dalle
// prove fatte superando i 38 KByte la funzione che esegue l'inserimento
// dà un errore con il seguente il messaggio: "errore irreparabile")
int gsc_SetBLOB(void *pValue, long Size, _ParameterPtr &pParam)
{
	VARIANT varBLOB;

   try
	{
      pParam->put_Size(Size);
		   // ottengo un dato VARIANT da un dato BLOB
		if (gsc_BlobToVariant(pValue, Size, varBLOB) == GS_BAD) return GS_BAD;
      // setto il valore del parametro
		pParam->AppendChunk(varBLOB);

		VariantClear(&varBLOB);
	}

   catch(_com_error e)
	{
      printDBErrs(e);
      GS_ERR_COD = eGSUpdRow;
      return GS_BAD;
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_Rb2Variant                                         (internal) */  
/*+
  La funzione riceve un puntatore ad un elemento 'resbuf' ed opera la 
  conversione del suo contenuto in _variant_t.
  Parametri:
  struct resbuf *rb;       Valore in forma di resbuf
  _variant_t    &Val;      Valore in forma di _variant_t
  DataTypeEnum *pDataType; Tipo di campo DB destinazione
-*/
/*************************************************************************/
int gsc_Rb2Variant(struct resbuf *rb, _variant_t &Val, DataTypeEnum *pDataType)
{
   if (!rb) return GS_BAD;

   switch (rb->restype)
   {
      case RTLONG : // Long integer
         gsc_set_variant_t(Val, (long) rb->resval.rlong);
         break;
      case RTNIL :  // nil
      case RTNONE : // No result
      case RTVOID : // Blank symbol
         if (pDataType && gsc_DBIsBoolean(*pDataType) == GS_GOOD)
            gsc_set_variant_t(Val, (bool) false);
         else
         {
            Val.Clear();
            Val.ChangeType(VT_NULL);
         }
         break;      
      case RTANG :   // Angle
      case RTORINT : // Orientation
      case RTREAL :  // Real number
         gsc_set_variant_t(Val, rb->resval.rreal);
         break;
      case RTSHORT : // Short integer
         gsc_set_variant_t(Val, rb->resval.rint);
         break;
      case RTSTR : // String
         if (gsc_strlen(rb->resval.rstring) == 0)
         {
            Val.Clear();
            Val.ChangeType(VT_NULL);
         }
         else
            if (pDataType &&
                (gsc_DBIsDate(*pDataType) == GS_GOOD ||
                 gsc_DBIsTimestamp(*pDataType) == GS_GOOD ||
                 gsc_DBIsTime(*pDataType) == GS_GOOD))
            {
               COleDateTime dummy;

               if (dummy.ParseDateTime(rb->resval.rstring) == 0)
                  { GS_ERR_COD = eGSInvalidDateType; return GS_BAD; }
               gsc_set_variant_t(Val, (DATE) dummy);
            }
            else
               gsc_set_variant_t(Val, rb->resval.rstring);
         break;      
      case RTT : // T atom
         gsc_set_variant_t(Val, (bool) true);
         break;
      case 62 : // Short integer = dxf code x colore autocad
         gsc_set_variant_t(Val, rb->resval.rint);
         break;
      case 420 : // long = dxf code x colore rgb
         gsc_set_variant_t(Val, (long) rb->resval.rlong);
         break;

      default :
         return GS_BAD;
   }                       

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_Variant2Rb                                         (internal) */  
/*+
  La funzione riceve un _variant_t ed opera la conversione del suo contenuto in
  resbuf.
  Parametri:
  _variant_t    &Val;   valore in forma di _variant_t
  struct resbuf *pRB;    valore in forma di resbuf

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
  N.B. I valori BLOB non vengono trattati perchè il resbuf non è adatto a questo scopo.
-*/
/*************************************************************************/
int gsc_Variant2Rb(_variant_t &Val, struct resbuf *pRB)
{
   if (!pRB) return GS_BAD;

   switch (Val.vt)
   {
      case VT_DISPATCH:
      case VT_ERROR:
      case VT_VARIANT:
      case VT_UNKNOWN:
      case VT_EMPTY:
      case VT_NULL:
      case VT_VOID:
      case VT_HRESULT:
      case VT_PTR:
      case VT_SAFEARRAY:
      case VT_CARRAY:
      case VT_USERDEFINED:
      case VT_RECORD:
      case VT_CLSID:
      case VT_VECTOR:
      case VT_ARRAY:
      case VT_BYREF:
      case VT_RESERVED:
      case VT_ILLEGAL:
         if (gsc_RbSubstNONE(pRB) == GS_BAD) return GS_BAD;
         break;
      case VT_I2:
      case VT_UI2:
      case VT_UI4:
         if (gsc_RbSubst(pRB, (short) Val) == GS_BAD) return GS_BAD;
         break;
      case VT_BOOL:
         if (Val.boolVal) 
            { if (gsc_RbSubstT(pRB) == GS_BAD) return GS_BAD; }
         else
            { if (gsc_RbSubstNIL(pRB) == GS_BAD) return GS_BAD; } 
         break;
      case VT_I4:
      case VT_INT:
      case VT_UINT:
         if (gsc_RbSubst(pRB, (long) Val) == GS_BAD) return GS_BAD;
         break;
      case VT_R4:
      case VT_R8:
      case VT_I8:
      case VT_UI8:
      case VT_CY:
      case VT_DECIMAL:
         if (gsc_RbSubst(pRB, (double) Val) == GS_BAD) return GS_BAD;
         break;
      case VT_BSTR:
      case VT_LPSTR:
      case VT_LPWSTR:
      case VT_FILETIME:
      case VT_I1:
      case VT_UI1:
      case VT_STREAM:
      case VT_STORAGE:
      case VT_DATE:
         if (gsc_RbSubst(pRB, (TCHAR *)(_bstr_t) Val) == GS_BAD) return GS_BAD;
         gsc_rtrim(pRB->resval.rstring);
         break;
      case VT_BSTR_BLOB:
         // I valori BLOB non vengono trattati perchè il resbuf non è adatto a questo scopo.
         // if (gsc_RbSubst(pRB, (TCHAR*)(_bstr_t) Val) == GS_BAD) return GS_BAD;
         if (gsc_RbSubstNONE(pRB) == GS_BAD) return GS_BAD;
         break;
      case VT_BLOB:
      case VT_STREAMED_OBJECT:
      case VT_STORED_OBJECT:
      case VT_BLOB_OBJECT:
         if (gsc_RbSubstNONE(pRB) == GS_BAD) return GS_BAD;
         break;
      case VT_CF:
         if (gsc_RbSubstNONE(pRB) == GS_BAD) return GS_BAD;
         break;
      default:
         if (gsc_RbSubstNONE(pRB) == GS_BAD) return GS_BAD;
         break;
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_RbSubst                                            <internal> */  
/*+
  La funzione inserisce un buffer binario (in forma vettore char) in un resbuf.
  Il dato non viene duplicato ma il resbuf punta direttamente al buffer originale.
  La definzione di rbinary è stata variata e il campo <clen> è diventato
  da <short> a <long>. Attenzione perchè la funzione acutRelRb() dà errore se 
  trova un valore binario. Il resbuf DEVE appartenere ad una C_RB_LIST perchè il
  suo distruttore provvede a rilasciare correttamente il buffer binario.
  resbuf.
  Parametri:  
  struct resbuf *pRB;    valore in forma di resbuf
  TCHAR *Value;          valore in forma di TCHAR
  long Size
-*/
/*************************************************************************/
int gsc_RbSubst(struct resbuf *pRB, TCHAR *Value, long Size)
{
   switch (pRB->restype)
   {
      case RTPICKS: // gruppo di selezione
         ads_ssfree(pRB->resval.rlname);
         pRB->restype = 1004;
         break;
      case RTSTR: // stringa
         if (pRB->resval.rstring) free(pRB->resval.rstring);
         pRB->restype = 1004;
         break;
      case 1004: // dato binario
         if (pRB->resval.rbinary.buf) free(pRB->resval.rbinary.buf);
         break;
      default:
         pRB->restype = 1004;
   }                  
   // roby probabile errore di cast: buf non dovrebbe essere char*
   pRB->resval.rbinary.buf = (char *) Value;
   pRB->resval.rbinary.clen = (short) Size;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getConnectionFromLisp              <internal> */
/*+
  Questa funzione legge i parametri da LISP e restituisce 
  la connessione OLE-DB specificata.
  Parametri:
  (("UDL_FILE" <Connection>) ("UDL_PROP" <Properties>) ("TABLE_REF" <TableRef>))
  dove
  <Connection> = <file UDL> | <stringa di connessione>
  <Properties> = stringa delle proprietà | ((<prop1><value>)(<prop1><value>)...)
  <TableRef>   = riferimento completo tabella | (<cat><schema><tabella>)

  C_STRING *FullRefTable;   Riferimento alla tabella (default = NULL) out;

  Restituisce la connessione in caso di successo altrimenti NULL.
-*/  
/*********************************************************/
C_DBCONNECTION *gsc_getConnectionFromLisp(presbuf arg, C_STRING *FullRefTable)
{
   C_STRING       ConnStrUDLFile;
   presbuf        p;
   C_DBCONNECTION *pConn;
   C_2STR_LIST    UDLProperties;

   if ((p = gsc_CdrAssoc(_T("UDL_FILE"), arg, FALSE)))
      if (p->restype == RTSTR)
      {
         ConnStrUDLFile = p->resval.rstring;
         ConnStrUDLFile.alltrim();
         if (gsc_path_exist(ConnStrUDLFile, GS_BAD) == GS_GOOD)
            // traduco dir assoluto in dir relativo
            if (gsc_nethost2drive(ConnStrUDLFile) == GS_BAD) return GS_BAD;
      }
      else
         ConnStrUDLFile.clear();

   // UDL Props
   if ((p = gsc_CdrAssoc(_T("UDL_PROP"), arg, FALSE)))
      if (p->restype == RTSTR)
      {
         if (gsc_PropListFromConnStr(p->resval.rstring, UDLProperties) == GS_BAD)
            return GS_BAD;
      }
      else
      if (p->restype == RTLB)
      {
         if (gsc_getUDLProperties(&p, UDLProperties) == GS_BAD) return GS_BAD;
      }
      else
         UDLProperties.remove_all();

   // Conversione path UDLProperties da assoluto in dir relativo
   if (gsc_UDLProperties_nethost2drive(ConnStrUDLFile.get_name(), UDLProperties) == GS_BAD)
      return GS_BAD;

   if ((pConn = GEOsimAppl::DBCONNECTION_LIST.get_Connection(ConnStrUDLFile.get_name(),
																			    &UDLProperties)) == NULL)
      return NULL;

   // Table reference (opzionale)
   if (FullRefTable)
      if ((p = gsc_CdrAssoc(_T("TABLE_REF"), arg, FALSE)))
         if (p->restype == RTSTR)
         {
            // Conversione riferimento alla tabella tabella
            if (FullRefTable->paste(pConn->FullRefTable_nethost2drive(p->resval.rstring)) == NULL)
               return NULL;
         }
         else
         if (p->restype == RTLB)
         {  // (<cat><sch><tab>)
            C_STRING Catalog, Schema, Table;

            if (!(p = p->rbnext)) { GS_ERR_COD = eGSInvalidArg; return NULL; }
            if (p->restype == RTSTR) Catalog = p->resval.rstring;
            if (!(p = p->rbnext)) { GS_ERR_COD = eGSInvalidArg; return NULL; }
            if (p->restype == RTSTR) Schema = p->resval.rstring;
            if (!(p = p->rbnext)) { GS_ERR_COD = eGSInvalidArg; return NULL; }
            if (p->restype == RTSTR) Table = p->resval.rstring;
            if (FullRefTable->paste(pConn->get_FullRefTable(Catalog, Schema, Table)) == NULL)
               return NULL;
         }
         else
            FullRefTable->clear();

   return pConn;
}


/*********************************************************/
/*.doc gsc_getConnectionFromRb                <external> */
/*+
  Questa funzione legge restituisce una connessione OLE-DB usando
  i parametri specificati in un a lista di resbuf strutturata come segue:
  (("UDL_FILE" <UDLConnStr>)[("UDL_PROP" <property list>) 
   [("TABLE_REF" <riferimento a tabella>|<FullTabRef>)]])
  Parametri:
  C_RB_LIST &ColValues;     Lista di resbuf;
  C_STRING *FullRefTable;   Riferimento alla tabella (default = NULL) out;

  dove:
  <UDLConnStr>          = <file UDL> || <stringa di connessione>
  <property list>       = nil || stringa con le proprietà separate da ";"
  <riferimento tabella> = (<cat><sch><tab>)
  <FullTabRef>          = stringa di riferimento completo alla tabella

  Restituisce la connessione in caso di successo altrimenti NULL.
  Nota: il puntatore <arg> viene avanzato all'elemento successivo la lista di parametri
        relativi la connessione OLE-DB.
-*/  
/*********************************************************/
C_DBCONNECTION *gsc_getConnectionFromRb(C_RB_LIST &ColValues, C_STRING *FullRefTable)
{
   C_STRING       ConnStrUDLFile;
   presbuf        pRB;
   C_DBCONNECTION *pConn;
   C_2STR_LIST    UDLPropList;

   if ((pRB = ColValues.CdrAssoc(_T("UDL_FILE"))) == NULL || pRB->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return NULL; }

   ConnStrUDLFile = gsc_alltrim(pRB->resval.rstring);
   if (gsc_path_exist(ConnStrUDLFile) == GS_GOOD)
      // traduco dir da assoluto in dir relativo
      if (gsc_nethost2drive(ConnStrUDLFile) == GS_BAD) return NULL;

   if ((pRB = ColValues.CdrAssoc(_T("UDL_PROP"))) != NULL && pRB->restype == RTSTR)
   {
      C_STRING StrUDLProperties(gsc_alltrim(pRB->resval.rstring));

      // traduco dir da assoluto in dir relativo
      if (gsc_UDLProperties_nethost2drive(ConnStrUDLFile.get_name(), StrUDLProperties) == GS_BAD)
         return NULL;
      if (gsc_PropListFromConnStr(StrUDLProperties.get_name(), UDLPropList) == GS_BAD)
         return NULL;
   }

   if ((pConn = GEOsimAppl::DBCONNECTION_LIST.get_Connection(ConnStrUDLFile.get_name(), 
                                                             &UDLPropList)) == NULL)
      return NULL;

   if (FullRefTable && 
       (pRB = ColValues.CdrAssoc(_T("TABLE_REF"))) != NULL && pRB->restype == RTSTR)
      // Conversione riferimento tabella da dir assoluto in relativo
      if (FullRefTable->paste(pConn->FullRefTable_nethost2drive(gsc_alltrim(pRB->resval.rstring))) == NULL)
         return NULL;

   return pConn;
}


/*********************************************************/
/*.doc gsc_CreateTable                        <external> /*
/*+
  Questa funzione crea una nuova tabella. Riceve un handle, un nome di
  tabella e una lista resbuf (con <nome colonna> <tipo colonna> 
  <lunghezza> <decimali>).
  Parametri:
  _ConnectionPtr &pConn;  Puntatore alla connessione database
  const TCHAR *TableRef;  Riferimento completo della tabella
                          (catalogo.schema.tabella)
  presbuf DefTab;         lista di definizione tabella 
                          ((<nome campo><tipo campo>[<param1>[<param n>...]])...)
  int retest;             se MORETESTS -> in caso di errore riprova n volte 
                          con i tempi di attesa impostati poi ritorna GS_BAD,
                          ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                          (default = MORETESTS)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B.
  Se la creazione della tabella avviene tramite odbc per PostgreSQL, il nome
  della tabella NON deve incominciare per "pg_" e nemmeno per i prefissi indicati 
  nelle opzioni del driver alla voce "SysTable Prefixes" altrimenti la struttura viene
  alterata e aggiunta una colonna "OID".
-*/  
/*********************************************************/
int gsc_CreateTable(_ConnectionPtr &pConn, const TCHAR *TableRef, presbuf DefTab, int retest)
{
   presbuf  pField, pParam;
   C_STRING statement;          // istruzione SQL
   int      j, i = 0;
   C_STRING Buff;

   statement = _T("CREATE TABLE ");
   statement += TableRef;

   pField = gsc_nth(i++, DefTab);
   if (pField)
   {
      statement += _T(" (");
      // leggo definizioni colonne
      while (pField)
      {
         statement += gsc_nth(0, pField)->resval.rstring; // nome campo
         statement += _T(' ');
         statement += gsc_nth(1, pField)->resval.rstring; // tipo campo
         j = 2;
         // lettura parametri opzionali
         pParam = gsc_nth(j++, pField);
         if (pParam)
         {
            while (pParam)
            {  // traformo parametro in stringa
               if (Buff.paste(gsc_rb2str(pParam)) == NULL) return GS_BAD;

               // Non ci sono altri parametri da inserire
               if (Buff.comp(_T("-1")) == 0) break;

               pParam = gsc_nth(j++, pField); // parametro successivo

               if (j == 4) // per sicurezza se si tratta del primo parametro
               {           // e questo era uguale a 0 non inserisco ulteriori parametri
                  if (Buff.comp(_T("0")) == 0) break;
                  statement += _T('(');
               }
               else
                  statement += _T(',');  // secondo parametro e successivi...

               statement += Buff; // aggiungo valore parametro
            }

            // se si è inserito almeno un parametro
            if (j > 3) statement += _T(')');
         }
         if ((pField = gsc_nth(i++, DefTab)) != NULL) statement += _T(',');
      }
      statement += _T(')');
   }

   if (gsc_ExeCmd(pConn, statement, NULL, retest) == GS_BAD)
      { GS_ERR_COD = eGSCreateTable; return GS_BAD; }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getTableIndexes                    <external> */
/*+
  Questa funzione restituisce una lista degli indici di una tabella nella
  seguente forma di lista di resbuf:
  ((<nome indice><tipo di indice>(<campo1><campo2>...)...)

  Parametri:
  _ConnectionPtr &pConn;   Puntatore alla connessione database
  const TCHAR *Table;      Nome Tabella
  const TCHAR *Catalog;    Catalogo
  const TCHAR *Schema;     Schema
  presbuf *pIndexList;     Lista degli indici (i nomi non sono con catalogo e schema)
  int retest;              se MORETESTS -> in caso di errore riprova n volte 
                           con i tempi di attesa impostati poi ritorna GS_BAD,
                           ONETEST -> in caso di errore ritorna GS_BAD senza riprovare

  Restituisce GS_GOOD in caso di successo, GS_CAN se non è possibile avere 
  queste informazioni altrimenti GS_BAD in caso di errore.
  N.B.: Alloca memoria.
-*/  
/*********************************************************/
int gsc_getTableIndexes(_ConnectionPtr &pConn, const TCHAR *Table, const TCHAR *Catalog,
                        const TCHAR *Schema, presbuf *pIndexList, int retest)
{
   SAFEARRAY FAR* psa = NULL; // Creo un safearray che prende 5 elementi
   SAFEARRAYBOUND rgsabound;
   _variant_t     var, varCriteria;
   long           ix;
   _RecordsetPtr  pRs;
   C_RB_LIST	   ColValues, Out;
   presbuf        pCatalog, pSchema, pTable, pIndexName, pUnique, pColumn, pPriKey, pOrdinal;
   int            res = GS_BAD, isUnique, IndexType, Ordinal, IndexPos;
   IndexTypeEnum  IndexMode;
   C_INT_STR_LIST IndexList;
   C_2STR_LIST    **FieldNameListArray = NULL;
   C_STRING       OrdinalStr;

   rgsabound.lLbound   = 0;
   rgsabound.cElements = 5;
   psa = SafeArrayCreate(VT_VARIANT, 1, &rgsabound);

   // CATALOGO
   ix  = 0;
   if (Catalog)
   {
      gsc_set_variant_t(var, Catalog);
      SafeArrayPutElement(psa, &ix, &var);
   }
   else
      { var.vt = VT_EMPTY; SafeArrayPutElement(psa, &ix, &var); }

   // SCHEMA
   ix= 1;
   if (Schema)
   {
      gsc_set_variant_t(var, Schema);
      SafeArrayPutElement(psa, &ix, &var);
   }
   else
      { var.vt = VT_EMPTY; SafeArrayPutElement(psa, &ix, &var); }
   
   // INDEX NAME
   ix = 2;
   var.vt = VT_EMPTY;
   SafeArrayPutElement(psa, &ix, &var);
   
   // INDEX TYPE
   ix = 3;
   var.vt = VT_EMPTY;
   SafeArrayPutElement(psa, &ix, &var);

   // TABLE
   ix  = 4;
   gsc_set_variant_t(var, Table);
   SafeArrayPutElement(psa, &ix, &var);

   varCriteria.vt = VT_ARRAY|VT_VARIANT;
   varCriteria.parray = psa;  

   // memorizzo il codice di errore precedente perchè se gli indici non sono 
   // supportate si genera un errore
   int Prev_GS_ERR_COD = GS_ERR_COD;
   if (gsc_OpenSchema(pConn, adSchemaIndexes, pRs, &varCriteria, ONETEST, GS_BAD) != GS_GOOD)
      { GS_ERR_COD = Prev_GS_ERR_COD; return GS_CAN; }

   if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD)
      { gsc_DBCloseRs(pRs); return GS_BAD; }

   pCatalog   = gsc_nth(1, gsc_assoc(_T("TABLE_CATALOG"), ColValues.get_head()));
   pSchema    = gsc_nth(1, gsc_assoc(_T("TABLE_SCHEMA"), ColValues.get_head()));
   pTable     = gsc_nth(1, gsc_assoc(_T("TABLE_NAME"), ColValues.get_head()));
   pIndexName = gsc_nth(1, gsc_assoc(_T("INDEX_NAME"), ColValues.get_head()));
   pUnique    = gsc_nth(1, gsc_assoc(_T("UNIQUE"), ColValues.get_head()));
   pColumn    = gsc_nth(1, gsc_assoc(_T("COLUMN_NAME"), ColValues.get_head()));
   pPriKey    = gsc_nth(1, gsc_assoc(_T("PRIMARY_KEY"), ColValues.get_head()));
   pOrdinal   = gsc_nth(1, gsc_assoc(_T("ORDINAL_POSITION"), ColValues.get_head()));

   res = GS_GOOD;

   while (gsc_isEOF(pRs) == GS_BAD)
   {
      gsc_DBReadRow(pRs, ColValues);
      
      // se catalogo, schema (quando specificati) e tabella corrispondono
      if ((!Catalog || gsc_strcmp(Catalog, (pCatalog->restype == RTSTR) ? pCatalog->resval.rstring : NULL, FALSE) == 0) &&
          (!Schema || gsc_strcmp(Schema, (pSchema->restype == RTSTR) ? pSchema->resval.rstring : NULL, FALSE) == 0) &&
          gsc_strcmp(pTable->resval.rstring, Table, FALSE) == 0 && pIndexName->restype == RTSTR)
      {
         if ((IndexPos = IndexList.getpos_name(pIndexName->resval.rstring, FALSE)) == 0) // nuovo indice
         {  // lo aggiungo alla lista
            if (gsc_rb2Int(pUnique, &isUnique) == GS_BAD) isUnique = 0;
            
            // se pPriKey = RTNIL non si può sapere se si tratta di chiave primaria
            if (pPriKey->restype != RTNIL && gsc_rb2Int(pPriKey, &IndexType) == GS_GOOD && IndexType != 0)
               IndexMode = PRIMARY_KEY;
            else
               IndexMode = (isUnique == 0) ? INDEX_NOUNIQUE : INDEX_UNIQUE;

            IndexList.add_tail_int_str(IndexMode, pIndexName->resval.rstring);
            FieldNameListArray = (C_2STR_LIST **) realloc(FieldNameListArray, 
                                                          IndexList.get_count() * sizeof(C_2STR_LIST *));
            IndexPos = IndexList.get_count();
            FieldNameListArray[IndexPos - 1] = new C_2STR_LIST();
         }

         gsc_rb2Int(pOrdinal, &Ordinal);
         OrdinalStr = Ordinal;
         // aggiungo il campo di indicizzazione
         FieldNameListArray[IndexPos - 1]->add_tail_2str(OrdinalStr.get_name(), pColumn->resval.rstring);
      }
      gsc_Skip(pRs);
   }
   gsc_DBCloseRs(pRs);
   
   if (IndexList.get_count() > 0)
   {
      C_STRING PrimaryKey;
      presbuf  p;

      // Siccome adSchemaIndexes con PostgreSQL non supporta le chiavi primarie
      // allora devo leggere esplicitamente quale è la chiave primaria
      if (gsc_getTablePrimaryKey(pConn, Table, Catalog, Schema, &p) == GS_GOOD)
      {
         C_RB_LIST List;

         List << p;
         if ((p = List.nth(0)) && p->restype == RTSTR) PrimaryKey = p->resval.rstring;
      }

      C_INT_STR *pIndex = (C_INT_STR *) IndexList.get_head();
      C_2STR    *pFieldName;

      Out << acutBuildList(RTLB, 0);
      IndexPos = 0;
      while (pIndex)
      {
         Out += acutBuildList(RTLB,
                                 RTSTR, pIndex->get_name(),  // nome indice
                                 RTSHORT, (PrimaryKey.comp(pIndex->get_name()) == 0) ? PRIMARY_KEY : pIndex->get_key(), // tipo di indice
                                 0);

         FieldNameListArray[IndexPos]->sort_nameByNum();
         pFieldName = (C_2STR *) FieldNameListArray[IndexPos]->get_head();
         Out += acutBuildList(RTLB, 0);
         while (pFieldName)
         {
            Out += acutBuildList(RTSTR, pFieldName->get_name2(), 0);

            pFieldName = (C_2STR *) FieldNameListArray[IndexPos]->get_next();
         }
         Out += acutBuildList(RTLE, RTLE, 0);

         pIndex = (C_INT_STR *) IndexList.get_next();
         IndexPos++;
      }

      Out += acutBuildList(RTLE, 0);
      Out.ReleaseAllAtDistruction(GS_BAD);
      *pIndexList = Out.get_head();
   }
   else
      *pIndexList = NULL;

   for (int i = 0; i <= IndexList.get_count() - 1; i++) delete FieldNameListArray[i];

   return GS_GOOD;
}
//int gsc_getTableIndexes(_ConnectionPtr &pConn, const TCHAR *Table, const TCHAR *Catalog,
//                        const TCHAR *Schema, presbuf *pIndexList, int retest)
//{
//   SAFEARRAY FAR* psa = NULL; // Creo un safearray che prende 5 elementi
//   SAFEARRAYBOUND rgsabound;
//   _variant_t     var, varCriteria;
//   long           ix;
//   _RecordsetPtr  pRs;
//   C_RB_LIST	   ColValues, Out;
//   presbuf        pCatalog, pSchema, pTable, pIndex, pUnique, pColumn, pType, pPriKey, pOrdinal, p;
//   int            res = GS_BAD, isUnique, IndexType;
//   IndexTypeEnum  IndexMode;
//
//   rgsabound.lLbound   = 0;
//   rgsabound.cElements = 5;
//   psa = SafeArrayCreate(VT_VARIANT, 1, &rgsabound);
//
//   // CATALOGO
//   ix  = 0;
//   if (Catalog)
//   {
//      gsc_set_variant_t(var, Catalog);
//      SafeArrayPutElement(psa, &ix, &var);
//   }
//   else
//      { var.vt = VT_EMPTY; SafeArrayPutElement(psa, &ix, &var); }
//
//   // SCHEMA
//   ix= 1;
//   if (Schema)
//   {
//      gsc_set_variant_t(var, Schema);
//      SafeArrayPutElement(psa, &ix, &var);
//   }
//   else
//      { var.vt = VT_EMPTY; SafeArrayPutElement(psa, &ix, &var); }
//   
//   // INDEX NAME
//   ix = 2;
//   var.vt = VT_EMPTY;
//   SafeArrayPutElement(psa, &ix, &var);
//   
//   // INDEX TYPE
//   ix = 3;
//   var.vt = VT_EMPTY;
//   SafeArrayPutElement(psa, &ix, &var);
//
//   // TABLE
//   ix  = 4;
//   gsc_set_variant_t(var, Table);
//   SafeArrayPutElement(psa, &ix, &var);
//
//   varCriteria.vt = VT_ARRAY|VT_VARIANT;
//   varCriteria.parray = psa;  
//
//   // memorizzo il codice di errore precedente perchè se gli indici non sono 
//   // supportate si genera un errore
//   int Prev_GS_ERR_COD = GS_ERR_COD;
//   if (gsc_OpenSchema(pConn, adSchemaIndexes, pRs, &varCriteria) != GS_GOOD)
//      { GS_ERR_COD = Prev_GS_ERR_COD; return GS_CAN; }
//
//   if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD)
//      { gsc_DBCloseRs(pRs); return GS_BAD; }
//
//   pCatalog = gsc_nth(1, gsc_assoc(_T("TABLE_CATALOG"), ColValues.get_head()));
//   pSchema  = gsc_nth(1, gsc_assoc(_T("TABLE_SCHEMA"), ColValues.get_head()));
//   pTable   = gsc_nth(1, gsc_assoc(_T("TABLE_NAME"), ColValues.get_head()));
//   pIndex   = gsc_nth(1, gsc_assoc(_T("INDEX_NAME"), ColValues.get_head()));
//   pUnique  = gsc_nth(1, gsc_assoc(_T("UNIQUE"), ColValues.get_head()));
//   pColumn  = gsc_nth(1, gsc_assoc(_T("COLUMN_NAME"), ColValues.get_head()));
//   pType    = gsc_nth(1, gsc_assoc(_T("TYPE"), ColValues.get_head()));
//   pPriKey  = gsc_nth(1, gsc_assoc(_T("PRIMARY_KEY"), ColValues.get_head()));
//   pOrdinal = gsc_nth(1, gsc_assoc(_T("ORDINAL_POSITION"), ColValues.get_head()));
//
//   res = GS_GOOD;
//   if ((Out << acutBuildList(RTLB, 0)) == NULL) { gsc_DBCloseRs(pRs); return GS_BAD; }
//
//   while (gsc_isEOF(pRs) == GS_BAD)
//   {
//      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { gsc_DBCloseRs(pRs); return GS_BAD; }
//      
//      // se catalogo, schema (quando specificati) e tabella corrispondono
//      if ((!Catalog || gsc_strcmp(Catalog, (pCatalog->restype == RTSTR) ? pCatalog->resval.rstring : NULL, FALSE) == 0) &&
//          (!Schema || gsc_strcmp(Schema, (pSchema->restype == RTSTR) ? pSchema->resval.rstring : NULL, FALSE) == 0) &&
//          gsc_strcmp(pTable->resval.rstring, Table, FALSE) == 0 && pIndex->restype == RTSTR)
//      {
//         if ((p = gsc_assoc(pIndex->resval.rstring, Out.get_head())) != NULL)
//         { // esiste già questo indice nella lista (aggiungo questa chiave)
//            presbuf pKey;
//            int     LenList;
//
//            if ((pKey = acutBuildList(RTSTR, pColumn->resval.rstring, 0)) == NULL)
//               { gsc_DBCloseRs(pRs); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
//            p            = gsc_nth(2, p); // inizio lista dei campi
//            LenList      = gsc_length(p);
//            p            = gsc_nth(LenList - 1, p);
//            pKey->rbnext = p->rbnext;
//            p->rbnext    = pKey;
//         }
//         else
//         {
//            if (gsc_rb2Int(pUnique, &isUnique) == GS_BAD) isUnique = 0;
//            
//            // se pPriKey = RTNIL non si può sapere se si tratta di chiave primaria
//            if (pPriKey->restype != RTNIL && gsc_rb2Int(pPriKey, &IndexType) == GS_GOOD && IndexType != 0)
//               IndexMode = PRIMARY_KEY;
//            else
//               IndexMode = (isUnique == 0) ? INDEX_NOUNIQUE : INDEX_UNIQUE;
//            
//            // aggiungo questo indice alla lista
//            if ((Out += acutBuildList(RTLB,
//                                         RTSTR, pIndex->resval.rstring, // nome indice
//                                         RTSHORT, (int) IndexMode,      // tipo di indice
//                                         RTLB,
//                                            RTSTR, pColumn->resval.rstring, // chiave
//                                         RTLE,
//                                      RTLE, 0)) == NULL)
//               { gsc_DBCloseRs(pRs); return GS_BAD; }
//         }
//      }
//      gsc_Skip(pRs);
//   }
//   gsc_DBCloseRs(pRs);
//   
//   if (Out.GetCount() > 1)
//   {
//      if ((Out += acutBuildList(RTLE, 0)) == NULL) return GS_BAD;
//      Out.ReleaseAllAtDistruction(GS_BAD);
//      *pIndexList = Out.get_head();
//   }
//   else
//      *pIndexList = NULL;
//
//   return GS_GOOD;
//}


/*********************************************************/
/*.doc gsc_getTablePrimaryKey                 <external> */
/*+
  Questa funzione restituisce una descrizione della chiave primaria 
  di una tabella nella seguente forma di lista di resbuf:
  (<nome chiave primaria>(<campo1><campo2>...))

  Parametri:
  _ConnectionPtr &pConn;   Puntatore alla connessione database
  const TCHAR *Table;      Nome Tabella
  const TCHAR *Catalog;    Catalogo
  const TCHAR *Schema;     Schema
  presbuf *pPKDescr;       Descrizione chiave primaria (il nome non è con catalogo e schema)
  int retest;              se MORETESTS -> in caso di errore riprova n volte 
                           con i tempi di attesa impostati poi ritorna GS_BAD,
                           ONETEST -> in caso di errore ritorna GS_BAD senza riprovare

  Restituisce GS_GOOD in caso di successo, GS_CAN se non è possibile avere 
  queste informazioni altrimenti GS_BAD in caso di errore.
  N.B.: Alloca memoria.
-*/  
/*********************************************************/
int gsc_getTablePrimaryKey(_ConnectionPtr &pConn, const TCHAR *Table, const TCHAR *Catalog,
                           const TCHAR *Schema, presbuf *pPKDescr, int retest)
{
   SAFEARRAY FAR* psa = NULL; // Creo un safearray che prende 5 elementi
   SAFEARRAYBOUND rgsabound;
   _variant_t     var, varCriteria;
   long           ix;
   _RecordsetPtr  pRs;
   C_RB_LIST	   ColValues, Out;
   presbuf        pCatalog, pSchema, pTable, pColumn, pPKName, pOrdinal;
   int            res = GS_BAD, Ordinal;
   C_2STR_LIST    FieldNameList;
   C_STRING       OrdinalStr;

   rgsabound.lLbound   = 0;
   rgsabound.cElements = 3;
   psa = SafeArrayCreate(VT_VARIANT, 1, &rgsabound);

   // CATALOGO
   ix  = 0;
   if (Catalog)
   {
      gsc_set_variant_t(var, Catalog);
      SafeArrayPutElement(psa, &ix, &var);
   }
   else
      { var.vt = VT_EMPTY; SafeArrayPutElement(psa, &ix, &var); }

   // SCHEMA
   ix= 1;
   if (Schema)
   {
      gsc_set_variant_t(var, Schema);
      SafeArrayPutElement(psa, &ix, &var);
   }
   else
      { var.vt = VT_EMPTY; SafeArrayPutElement(psa, &ix, &var); }
   
   // TABLE
   ix = 2;
   gsc_set_variant_t(var, Table);
   SafeArrayPutElement(psa, &ix, &var);

   varCriteria.vt = VT_ARRAY|VT_VARIANT;
   varCriteria.parray = psa;  

   // memorizzo il codice di errore precedente perchè se le chiavi primaria non sono 
   // supportate si genera un errore
   int Prev_GS_ERR_COD = GS_ERR_COD;
   if (gsc_OpenSchema(pConn, adSchemaPrimaryKeys, pRs, &varCriteria, ONETEST, GS_BAD) != GS_GOOD)
      { GS_ERR_COD = Prev_GS_ERR_COD; return GS_CAN; }

   if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD)
      { gsc_DBCloseRs(pRs); return GS_BAD; }

   pCatalog = gsc_nth(1, gsc_assoc(_T("TABLE_CATALOG"), ColValues.get_head()));
   pSchema  = gsc_nth(1, gsc_assoc(_T("TABLE_SCHEMA"), ColValues.get_head()));
   pTable   = gsc_nth(1, gsc_assoc(_T("TABLE_NAME"), ColValues.get_head()));
   pColumn  = gsc_nth(1, gsc_assoc(_T("COLUMN_NAME"), ColValues.get_head()));
   pPKName  = gsc_nth(1, gsc_assoc(_T("PK_NAME"), ColValues.get_head()));
   pOrdinal = gsc_nth(1, gsc_assoc(_T("ORDINAL"), ColValues.get_head()));

   res = GS_GOOD;

   while (gsc_isEOF(pRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { gsc_DBCloseRs(pRs); return GS_BAD; }
      
      // se catalogo, schema (quando specificati) e tabella corrispondono
      if ((!Catalog || gsc_strcmp(Catalog, (pCatalog->restype == RTSTR) ? pCatalog->resval.rstring : NULL, FALSE) == 0) &&
          (!Schema || gsc_strcmp(Schema, (pSchema->restype == RTSTR) ? pSchema->resval.rstring : NULL, FALSE) == 0) &&
          gsc_strcmp(pTable->resval.rstring, Table, FALSE) == 0 && pPKName->restype == RTSTR)
      {
         if (Out.GetCount() == 0)
            if ((Out += acutBuildList(RTLB,
                                         RTSTR, pPKName->resval.rstring, // nome chiave primaria
                                      0)) == NULL)
               { gsc_DBCloseRs(pRs); return GS_BAD; }

         if (gsc_rb2Int(pOrdinal, &Ordinal) != GS_GOOD)
            { gsc_DBCloseRs(pRs); return GS_BAD; }
         OrdinalStr = Ordinal;
         // aggiungo il campo di indicizzazione
         FieldNameList.add_tail_2str(OrdinalStr.get_name(), pColumn->resval.rstring);
      }
      gsc_Skip(pRs);
   }
   gsc_DBCloseRs(pRs);
   
   if (FieldNameList.get_count() > 0)
   {
      C_2STR *pFieldName;
      
      FieldNameList.sort_nameByNum();
      pFieldName = (C_2STR *) FieldNameList.get_head();
      if ((Out += acutBuildList(RTLB, 0)) == NULL) return GS_BAD;
      while (pFieldName)
      {
         if ((Out += acutBuildList(RTSTR, pFieldName->get_name2(), 0)) == NULL)
            return GS_BAD;

         pFieldName = (C_2STR *) FieldNameList.get_next();
      }
      if ((Out += acutBuildList(RTLE, 0)) == NULL) return GS_BAD;
      if ((Out += acutBuildList(RTLE, 0)) == NULL) return GS_BAD;
      Out.ReleaseAllAtDistruction(GS_BAD);
      *pPKDescr = Out.get_head();
   }
   else
      *pPKDescr = NULL;

   return GS_GOOD;
}


/*********************************************************/
/*.doc (new 2) gsc_DBDelRows                  <external> */
/*+
  Questa funzione cancella tutte le righe della tabella.
  Parametri:
  _ConnectionPtr &pConn;      Puntatore alla connessione database
  const TCHAR    *TableRef;
  const TCHAR    *WhereSql;   Condizione di filtro (default = NULL)
  int retest;                 se MORETESTS -> in caso di errore riprova n volte 
                              con i tempi di attesa impostati poi ritorna GS_BAD,
                              ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                              (default = MORETESTS)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_DBDelRows(_ConnectionPtr &pConn, const TCHAR *TableRef, 
                  const TCHAR *WhereSql, int retest)
{
   C_STRING statement;

   if (!TableRef) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   statement = _T("DELETE FROM ");
   statement += TableRef;

   if (WhereSql && wcslen(WhereSql) > 0)
   {
      statement += _T(" WHERE ");
      statement += WhereSql;
   }

   if (gsc_ExeCmd(pConn, statement, NULL, retest) == GS_BAD)
      { GS_ERR_COD = eGSDelRow; return GS_BAD; }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_DelTable                           <external> */
/*+
  Questa funzione cancella una tabella.
  Parametri:
  _ConnectionPtr &pConn;      Puntatore alla connessione database
  const TCHAR    *TableRef;   Riferimento completo tabella
  int            retest;      Se MORETESTS -> in caso di errore riprova n volte 
                              con i tempi di attesa impostati poi ritorna GS_BAD,
                              ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                              (default = MORETESTS)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_DelTable(_ConnectionPtr &pConn, const TCHAR *TableRef, int retest)
{
   C_STRING statement;  // istruzione SQL

   statement = _T("DROP TABLE ");
   statement += TableRef;
   // In alcuni database non è possibile cancellare la tabella se ci sono 
   // altri oggetti dipendenti da essa (es. viste) quindi aggiungo "CASCADE"
   statement += _T(" CASCADE");
                
   if (gsc_ExeCmd(pConn, statement, NULL, retest, GS_BAD) == GS_BAD)
   { 
      // Provo senza l'opzione "CASCADE" nel caso alcuni DBMS non la supportassero
      statement = _T("DROP TABLE ");
      statement += TableRef;

      if (gsc_ExeCmd(pConn, statement, NULL, retest) == GS_BAD)
      { 
         GS_ERR_COD = eGSTableNotDel; 
         return GS_BAD; 
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_existtable                         <external> */
/*+
  Questa funzione controlla l'esistenza di una tabella.
  Parametri:
  Lista RESBUF 
  (("UDL_FILE" <file UDL> || <stringa di connessione>) ("UDL_PROP" <value>) ("TABLE_REF" <value>))
-*/  
/*********************************************************/
int gs_existtable(void)
{                        
   presbuf        arg = acedGetArgs();
   C_DBCONNECTION *pConn;
   C_STRING       Table;

   acedRetNil();

   // Legge nella lista dei parametri i riferimenti alla tabella
   if ((pConn = gsc_getConnectionFromLisp(arg, &Table)) == NULL) return RTERROR;

   if (pConn->ExistTable(Table) == GS_GOOD) acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_getProviderTypeList                 <external> */
/*+
  Questa funzione legge i tipi supportati da un provider OLE-DB e li restituisce 
  ordinati in modo crescente per nome.
  Parametri:
  Lista RESBUF
  (<OLE_DB conn> <noBLOBType>)
  dove
  <OLE_DB conn> = (("UDL_FILE" <Connection>) ("UDL_PROP" <Properties>))
  <Connection>  = <file UDL> | <stringa di connessione>
  <Properties>  = stringa delle proprietà | ((<prop1><value>)(<prop1><value>)...)
  <noBLOBType>  = un flag che se TRUE indica un filtro per tipi non BLOB (default nil)

  restituisce una lista:
  ((<TypeName><Size><Prec><IsFixedPrecScale>(<Param1><Param2>...)) ...)
  dove:
  <TypeName> è una descrizione del tipo di dato del provider
  <Size>     è il numero di caratteri o il numero di cifre (-1 non usato)
  <Prec>     n. max di decimali (-1 = non usato)
  <IsFixedPrecScale> RTT se lunghezza fissa altrimenti RTNIL (RTVOID = non si sa)
  (<Param1><Param2>...) è una lista di parametri necessari per creare una colonna di quel
                        tipo (es. per ORACLE 7.3, tipo NUMBER ("precision" "scale"))

-*/  
/*********************************************************/
int gs_getProviderTypeList(void)
{
   C_STRING             Params;
   TCHAR                *p;
   int                  Qty, i, noBLOB = FALSE;
   presbuf              arg = acedGetArgs();
   C_DBCONNECTION       *pConn;
   C_PROVIDER_TYPE_LIST SortedDataTypeList;
   C_PROVIDER_TYPE      *pDataType;
   C_RB_LIST            Result;

   acedRetNil();
 
   // Legge nella lista dei parametri i riferimenti alla tabella
   if ((pConn = gsc_getConnectionFromLisp(gsc_nth(0, arg))) == NULL) return RTERROR;
   if ((arg = gsc_nth(1, arg)) && arg->restype == RTT) noBLOB = TRUE;

   pConn->ptr_DataTypeList()->copy(SortedDataTypeList);
   SortedDataTypeList.sort_name();

   pDataType = (C_PROVIDER_TYPE *) SortedDataTypeList.get_head();
   while (pDataType)
   {
      // se non è impostato il filtro per BLOB
      // oppure se impostato il filtro e non si tratta di tipo BLOB
      if (!noBLOB || (noBLOB && !pDataType->get_IsLong()))
      {
         if ((Result += acutBuildList(RTLB,
                                      RTSTR, pDataType->get_name(),
                                      RTREAL, (double) pDataType->get_Size(),
                                      RTSHORT, pDataType->get_Dec(),
                                      (pDataType->get_IsFixedPrecScale() == TRUE) ? RTT : RTNIL,
                                      0)) == NULL)
            return RTERROR;
         Params = pDataType->get_CreateParams();
         if (Params.len() > 0)
         {
            p = Params.get_name();
            Qty = gsc_strsep(p, _T('\0'));
            if ((Result += acutBuildList(RTLB, 0)) == NULL) return RTERROR;
            for (i = 0; i < Qty; i++)
            {
               if ((Result += acutBuildList(RTSTR, p, 0)) == NULL) return RTERROR;
               while (*p != _T('\0')) p++; p++;
            }
            if ((Result += acutBuildList(RTSTR, p, RTLE, RTLE, 0)) == NULL) return RTERROR;
         }
         else
            if ((Result += acutBuildList(RTNIL, RTLE, 0)) == NULL) return RTERROR;
      }

      pDataType = (C_PROVIDER_TYPE *) pDataType->get_next();
   }

   if (Result.GetCount() == 0) return RTERROR;
   Result.LspRetList();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_Descr2ProviderDescr                 <external> */
/*+
  Questa funzione trasforma la descrizione di un tipo di dato e le sue dimensioni,
  convertendole per un'altra connessione OLE-DB.
  Parametri:
  Lista RESBUF 
  (<OLE_DB conn src> (<type><dim><dec>) <OLE_DB conn dst>)
  dove:
  <OLE_DB conn src> = <OLE_DB conn>
  <OLE_DB conn dst> = <OLE_DB conn>
  <OLE_DB conn> = (("UDL_FILE" <Connection>) ("UDL_PROP" <Properties>))
  <Connection>  = <file UDL> | <stringa di connessione>
  <Properties>  = stringa delle proprietà | ((<prop1><value>)(<prop1><value>)...)

  restituisce una lista:
  (("TYPE"<type>)("LEN"<dim>)("DECIM"<dec>))
  dove <type> è una descrizione del provider
-*/  
/*********************************************************/
int gs_Descr2ProviderDescr(void)
{
   C_DBCONNECTION *pSrcConn, *pDstConn;
   presbuf        arg = acedGetArgs(), p1, p;
   TCHAR          *SrcProviderDescr;
   C_STRING       DestProviderDescr;
   long           SrcSize, DestSize;
   int            SrcPrec, DestPrec;
   C_RB_LIST      Result;

   // Legge nella lista dei parametri i riferimenti al DB
   if ((pSrcConn = gsc_getConnectionFromLisp(gsc_nth(0, arg))) == NULL) return RTERROR;

   p1 = gsc_nth(1, arg);
   if (!p1 || !(p = gsc_nth(0, p1)) || p->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   SrcProviderDescr = p->resval.rstring;  // descrizione tipo

   if (!(p = gsc_nth(1, p1)) || gsc_rb2Lng(p, &SrcSize) == GS_BAD) // dimensione
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (!(p = gsc_nth(2, p1)) || gsc_rb2Int(p, &SrcPrec) == GS_BAD) // precisione
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   // Legge nella lista dei parametri i riferimenti al DB
   if ((pDstConn = gsc_getConnectionFromLisp(gsc_nth(2, arg))) == NULL) return RTERROR;

   if (pSrcConn->Descr2ProviderDescr(SrcProviderDescr, SrcSize, SrcPrec, *pDstConn,
                                     DestProviderDescr, &DestSize, &DestPrec) == GS_BAD)
      return RTNORM;

   if ((Result << acutBuildList(RTLB,
                                      RTSTR, _T("TYPE"),
                                      RTSTR, DestProviderDescr.get_name(),
                                RTLE,
                                RTLB,
                                      RTSTR, _T("LEN"),
                                      RTREAL, (double) DestSize,
                                RTLE,
                                RTLB,
                                      RTSTR, _T("DECIM"),
                                      RTSHORT, DestPrec,
                                RTLE, 0)) == NULL)
       return RTERROR;

   Result.LspRetList();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_ReadStruct                         <external> */
/*+
  Questa funzione legge la struttura di una tabella.
  Parametri:
  Lista RESBUF 
  (("UDL_FILE" <file UDL> || <stringa di connessione>) ("UDL_PROP" <value>) ("TABLE_REF" <value>))

  restituisce una lista:
  (("ATTRIB"<name>)("TYPE"<type>)("LEN"<dim>)("DECIM"<dec>))
  dove <tipo> è una descrizione del provider
-*/  
/*********************************************************/
int gs_readstruct(void)
{                        
   presbuf        arg = acedGetArgs(), pRB;
   C_RB_LIST      Stru, Result;
   C_DBCONNECTION *pConn;
   int            i = 0;
   C_STRING       TypeDescr, Table;

   acedRetNil();

   // Legge nella lista dei parametri i riferimenti alla tabella
   if ((pConn = gsc_getConnectionFromLisp(arg, &Table)) == NULL) return RTERROR;

   // leggo struttura
   if ((pRB = pConn->ReadStruct(Table.get_name())) == NULL) return RTERROR;
   if ((Stru << pRB) == NULL) return RTERROR;

   while ((pRB = gsc_nth(i++, Stru.get_head())) != NULL)
   {  // nome
      if ((Result += acutBuildList(RTLB, 
                                   RTLB,
                                      RTSTR, _T("ATTRIB"),
                                      RTSTR, gsc_nth(0, pRB)->resval.rstring,
                                   RTLE, 0)) == NULL)
         return RTERROR;
      // tipo (descrizione)
      TypeDescr.paste(pConn->Type2ProviderDescr((DataTypeEnum) gsc_nth(1, pRB)->resval.rlong,     // tipo
                                                (gsc_nth(4, pRB)->restype == RTT) ? TRUE : FALSE, // is long
                                                (gsc_nth(5, pRB)->restype == RTT) ? TRUE : FALSE, // is FixedPrecScale
                                                gsc_nth(6, pRB)->restype,                         // is FixedLength
                                                (gsc_nth(7, pRB)->restype == RTT) ? TRUE : FALSE, // is Write
                                                gsc_nth(2, pRB)->resval.rlong,                    // dimensione
                                                FALSE,                                            // Exact mode
                                                gsc_nth(3, pRB)->resval.rint));                  // precisione
      if (TypeDescr.len() == 0) return RTERROR;
      if ((Result += acutBuildList(RTLB,
                                      RTSTR, _T("TYPE"),
                                      RTSTR, TypeDescr.get_name(),
                                   RTLE, 0)) == NULL)
         return RTERROR;
      // dim
      if ((Result += acutBuildList(RTLB,
                                      RTSTR, _T("LEN"),
                                      RTREAL, (double)gsc_nth(2, pRB)->resval.rlong,
                                   RTLE, 0)) == NULL)
         return RTERROR;
      // dec
      if ((Result += acutBuildList(RTLB,
                                      RTSTR, _T("DECIM"),
                                      RTSHORT, gsc_nth(3, pRB)->resval.rint,
                                   RTLE,
                                   RTLE, 0)) == NULL)
         return RTERROR;
   }

   Result.LspRetList();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_exesql                              <external> */
/*+
  Questa funzione esegue un comando sql.
  Parametri:
  Lista RESBUF 
  ((("UDL_FILE" <file UDL> || <stringa di connessione>) ("UDL_PROP" <value>))
   <sql_statement>
  )

  restituisce una T in caso di successo:
-*/  
/*********************************************************/
int gs_exesql(void)
{                        
   presbuf        arg = acedGetArgs(), pRB;
   C_DBCONNECTION *pConn;
   C_STRING       statement;

   acedRetNil();

   // Legge nella lista dei parametri i riferimenti alla prima connessione OLE-DSB
   if (!(pRB = gsc_nth(0, arg))) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if ((pConn = gsc_getConnectionFromLisp(pRB)) == NULL) return RTERROR;

   if (!(pRB = gsc_nth(1, arg))) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (!pRB || pRB->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   statement = pRB->resval.rstring;

   // eseguo istruzione SQL
   if (pConn->ExeCmd(statement) == GS_BAD) return RTERROR;
   acedRetT();

   return GS_GOOD;

}


/*********************************************************/
/*.doc gsc_ReadStruct                         <external> */
/*+
  Questa funzione legge la struttura di una tabella usando gli schemi di ADO.
  Parametri:
  _ConnectionPtr &pConn;   Puntatore alla connessione database
  const TCHAR *Table;      Nome Tabella
  const TCHAR *Catalog;    Catalogo
  const TCHAR *Schema;     Schema
  int retest;              se MORETESTS -> in caso di errore riprova n volte 
                           con i tempi di attesa impostati poi ritorna GS_BAD,
                           ONETEST -> in caso di errore ritorna GS_BAD senza riprovare

  Restituisce, in caso di successo, una lista di resbuf composta come segue:
  presbuf *defstru;        puntatore resbuf 
                           ((<Name><Type><Dim><Dec><IsLong><IsFixedPrecScale>
                             <IsFixedLength><Write>)...)
                           dove <Type> è un codice numerico ADO che esprime 
                           un tipo di dato (DataTypeEnum). 
  altrimenti restituisce NULL.
  N.B.: Alloca memoria.
-*/  
/*********************************************************/
presbuf gsc_ReadStruct(_ConnectionPtr &pConn, const TCHAR *Table, const TCHAR *Catalog,
                       const TCHAR *Schema, int retest)
{
   SAFEARRAY FAR* psa = NULL; // Creo un safearray che prende 4 elementi
   SAFEARRAYBOUND rgsabound;
   _variant_t     var, varCriteria;
   long           ix, dummyLong, ColumnFlag;
   _RecordsetPtr  pRs;
   C_RB_LIST	   ColValues, Stru, OrderedStru;
   presbuf        pCatalog, pSchema, pTable, pOrdinalPos;
   presbuf        pRBInfoType, pName, pType, pNumPrec, pDec, pDateTimePrec, pCharLen, pFlag;
   int            res = GS_BAD, i , pos, dummyInt;
   C_INT_LIST     OrderList;
   C_INT          *pOrder; 

   if (!Table) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   
   rgsabound.lLbound   = 0;
   rgsabound.cElements = 4;
   psa = SafeArrayCreate(VT_VARIANT, 1, &rgsabound);

   // CATALOGO
   ix  = 0;
   if (Catalog)
   {
      gsc_set_variant_t(var, Catalog);
      SafeArrayPutElement(psa, &ix, &var);
   }
   else
      { var.vt = VT_EMPTY; SafeArrayPutElement(psa, &ix, &var); }

   // SCHEMA
   ix= 1;
   if (Schema)
   {
      gsc_set_variant_t(var, Schema);
      SafeArrayPutElement(psa, &ix, &var);
   }
   else
      { var.vt = VT_EMPTY; SafeArrayPutElement(psa, &ix, &var); }

   // TABLE
   ix  = 2;
   gsc_set_variant_t(var, Table);
   SafeArrayPutElement(psa, &ix, &var);

   // COLUMN
   ix = 3;
   var.vt = VT_EMPTY;
   SafeArrayPutElement(psa, &ix, &var);

   varCriteria.vt = VT_ARRAY|VT_VARIANT;
   varCriteria.parray = psa;  

   // un tentativo senza stampa di errore nel caso il provider non supporti open schema (vedi dbase)
   if (gsc_OpenSchema(pConn, adSchemaColumns, pRs, &varCriteria, ONETEST, GS_BAD) == GS_BAD)
      return GS_BAD;

   if (gsc_InitDBReadRow(pRs, ColValues) == NULL)
      { gsc_DBCloseRs(pRs); return NULL; }
   
   pCatalog = gsc_nth(1, gsc_assoc(_T("TABLE_CATALOG"), ColValues.get_head()));
   pSchema  = gsc_nth(1, gsc_assoc(_T("TABLE_SCHEMA"), ColValues.get_head()));
   pTable   = gsc_nth(1, gsc_assoc(_T("TABLE_NAME"), ColValues.get_head()));
   pOrdinalPos = gsc_nth(1, gsc_assoc(_T("ORDINAL_POSITION"), ColValues.get_head()));
   pName    = gsc_nth(1, gsc_assoc(_T("COLUMN_NAME"), ColValues.get_head()));
   pType    = gsc_nth(1, gsc_assoc(_T("DATA_TYPE"), ColValues.get_head()));
   // "NUMERIC_PRECISION" Se il dato è numerico, questo è il limite superiore della
   //                     massima precisione del tipo di dato ovvero il numero delle
   //                     cifre più rappresentative memorizzabili (senza contare il segno e il
   //                     seoaratore decimale).
   pNumPrec = gsc_nth(1, gsc_assoc(_T("NUMERIC_PRECISION"), ColValues.get_head()));
   // "NUMERIC_SCALE" = Se type è adDecimal o adNumeric rappresenta il numero massimo
   //                   di cifre decimali ammesse a destra del separatore decimale
   pDec     = gsc_nth(1, gsc_assoc(_T("NUMERIC_SCALE"), ColValues.get_head()));
   pDateTimePrec = gsc_nth(1, gsc_assoc(_T("DATETIME_PRECISION"), ColValues.get_head()));
   // "CHARACTER_MAXIMUM_LENGTH" = lunghezza massima per tipo carattere
   pCharLen = gsc_nth(1, gsc_assoc(_T("CHARACTER_MAXIMUM_LENGTH"), ColValues.get_head()));
   pFlag    = gsc_nth(1, gsc_assoc(_T("COLUMN_FLAGS"), ColValues.get_head()));

   if ((Stru << acutBuildList(RTLB, 0)) == NULL)
      { gsc_DBCloseRs(pRs); return NULL; }

   i = 1;
   // OpenSchema restituisce gli attributi in ordine alfabetico
   while (gsc_isEOF(pRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
         { gsc_DBCloseRs(pRs); return NULL; }
   
      // se catalogo, schema (quando specificati) e tabella corrispondono
      if ((!Catalog || gsc_strcmp(Catalog, (pCatalog->restype == RTSTR) ? pCatalog->resval.rstring : NULL, FALSE) == 0) &&
          (!Schema || gsc_strcmp(Schema, (pSchema->restype == RTSTR) ? pSchema->resval.rstring : NULL, FALSE) == 0) &&
          gsc_strcmp(Table, pTable->resval.rstring, FALSE) == 0)
      {
         // nome, tipo
         if (gsc_rb2Lng(pType, &dummyLong) == GS_BAD) dummyLong = 0;

         if ((Stru += acutBuildList(RTLB, RTSTR, pName->resval.rstring,
                                    RTLONG, dummyLong, 0)) == NULL)
            { gsc_DBCloseRs(pRs); return NULL; }

         // dimensione
         switch (dummyLong)
         {
            case adBinary:
               // "Oggetto OLE" dim=0 in ACCESS
               // "Long Raw" , "Raw" 0<dim<=255, "RowID" in ORACLE
               if (gsc_rb2Lng(pCharLen, &dummyLong) == GS_GOOD)
                  pRBInfoType = acutBuildList(RTLONG, dummyLong, 0);
               else
                  pRBInfoType = acutBuildList(RTLONG, -1, 0);
               break;

            case adBoolean:
               // "Logico" dim=2 in ACCESS
               // "Logic" dim=nil in DBase
               if (gsc_rb2Lng(pCharLen, &dummyLong) == GS_GOOD)
                  pRBInfoType = acutBuildList(RTLONG, dummyLong, 0);
               else
                  pRBInfoType = acutBuildList(RTLONG, -1, 0);
               break;

            case adChapter:
               pRBInfoType = NULL;
               break;

            case adChar:
            case adWChar:
            case adBSTR:
               // "Collegamento ipertestuale" dim=0, "Memo" dim=0, "Testo" 0<dim<=255 in ACCESS
               // "Character" dim<=254, "Memo" dim=4998 in DBase
               // "Char" 0<dim<=255, "VarChar2" 0<dim<=2000 in ORACLE
               if (gsc_rb2Lng(pCharLen, &dummyLong) == GS_GOOD)
                  pRBInfoType = acutBuildList(RTLONG, dummyLong, 0);
               else
                  pRBInfoType = acutBuildList(RTLONG, -1, 0);
               break;

            case adCurrency:
               // "Valuta" dim=19 in ACCESS
               if (gsc_rb2Lng(pNumPrec, &dummyLong) == GS_GOOD)
                  pRBInfoType = acutBuildList(RTLONG, dummyLong, 0);
               else
                  pRBInfoType = acutBuildList(RTLONG, -1, 0);
               break;

            case adDate:
               // "Data ora" 0<dim<=19 in ACCESS
               if (gsc_rb2Lng(pDateTimePrec, &dummyLong) == GS_GOOD)
                  pRBInfoType = acutBuildList(RTLONG, dummyLong, 0);
               else
                  pRBInfoType = acutBuildList(RTLONG, -1, 0);
               break;

            case adDBDate:
               // "Data" dim=0 in DBase
               if (gsc_rb2Lng(pDateTimePrec, &dummyLong) == GS_GOOD)
                  pRBInfoType = acutBuildList(RTLONG, dummyLong, 0);
               else
                  pRBInfoType = acutBuildList(RTLONG, -1, 0);
               break;

            case adDBTime:
               pRBInfoType = NULL;
               break;

            case adDBTimeStamp:
               // "Data" dim=0 in ORACLE
               if (gsc_rb2Lng(pDateTimePrec, &dummyLong) == GS_GOOD)
                  pRBInfoType = acutBuildList(RTLONG, dummyLong, 0);
               else
                  pRBInfoType = acutBuildList(RTLONG, -1, 0);
               break;

            case adDouble:
               // "Numerico (precisione doppia)" dim=16 in ACCESS
               // "Numeric" dim=15 in DBase
               if (gsc_rb2Lng(pNumPrec, &dummyLong) == GS_GOOD)
                  pRBInfoType = acutBuildList(RTLONG, dummyLong, 0);
               else
                  pRBInfoType = acutBuildList(RTLONG, -1, 0);
               break;

            case adEmpty:
            case adError:
            case adFileTime:
               pRBInfoType = NULL;
               break;

            case adGUID:
               // "ID replica" dim=16 in ACCESS
               if (gsc_rb2Lng(pNumPrec, &dummyLong) == GS_GOOD)
                  pRBInfoType = acutBuildList(RTLONG, dummyLong, 0);
               else
                  pRBInfoType = acutBuildList(RTLONG, -1, 0);
               break;

            case adIDispatch:
               pRBInfoType = NULL;
               break;

            case adBigInt:
            case adInteger:
               // "Numerico (intero lungo)" dim=10 in ACCESS
               if (gsc_rb2Lng(pNumPrec, &dummyLong) == GS_GOOD)
                  pRBInfoType = acutBuildList(RTLONG, dummyLong, 0);
               else
                  pRBInfoType = acutBuildList(RTLONG, -1, 0);
               break;

            case adIUnknown:
               // "MLSLabel" o "GEOM" in ORACLE
               pRBInfoType = acutBuildList(RTLONG, -1, 0);
               break;

            case adLongVarBinary:
            case adLongVarChar:
            case adLongVarWChar:
               pRBInfoType = NULL;
               break;

            case adDecimal:
            case adNumeric:
               // "Numeric" 0<dim<=38 in ORACLE
               if (gsc_rb2Lng(pNumPrec, &dummyLong) == GS_GOOD)
                  pRBInfoType = acutBuildList(RTLONG, dummyLong, 0);
               else
                  pRBInfoType = acutBuildList(RTLONG, -1, 0);
               break;

            case adPropVariant:
               pRBInfoType = NULL;
               break;

            case adSingle:
               // "Numerico (precisione singola)" dim=7 in ACCESS
               if (gsc_rb2Lng(pNumPrec, &dummyLong) == GS_GOOD)
                  pRBInfoType = acutBuildList(RTLONG, dummyLong, 0);
               else
                  pRBInfoType = acutBuildList(RTLONG, -1, 0);
               break;

            case adSmallInt:
               // "Numerico (intero)" dim=5 in ACCESS
               if (gsc_rb2Lng(pNumPrec, &dummyLong) == GS_GOOD)
                  pRBInfoType = acutBuildList(RTLONG, dummyLong, 0);
               else
                  pRBInfoType = acutBuildList(RTLONG, -1, 0);
               break;

            case adUnsignedBigInt:
            case adUnsignedInt:
            case adUnsignedSmallInt:
               pRBInfoType = NULL;
               break;

            case adTinyInt:         // "Numerico (byte)" per ACCESS 97 via ODBC
            case adUnsignedTinyInt: // "Numerico (byte)" dim=3 in ACCESS
               if (gsc_rb2Lng(pNumPrec, &dummyLong) == GS_GOOD)
                  pRBInfoType = acutBuildList(RTLONG, dummyLong, 0);
               else
                  pRBInfoType = acutBuildList(RTLONG, -1, 0);
               break;

            case adUserDefined:
            case adVarBinary:
            case adVarChar:
            case adVariant:
            case adVarNumeric:
            case adVarWChar:
            default:
               pRBInfoType = NULL;
               break;
         }
      
         if (!pRBInfoType) { GS_ERR_COD = eGSUnkwnDrvFieldType; gsc_DBCloseRs(pRs); return NULL; }

         Stru += pRBInfoType;

         // Ado type
         if (gsc_rb2Lng(pType, &dummyLong) == GS_BAD) dummyLong = 0;
         // Decimals
         if (gsc_rb2Int(pDec, &dummyInt) == GS_GOOD &&
             (dummyLong == adDecimal || dummyLong == adNumeric))
         {
            if ((Stru += acutBuildList(RTSHORT, dummyInt, 0)) == NULL)
               { gsc_DBCloseRs(pRs); return NULL; }
         }
         else
            if ((Stru += acutBuildList(RTSHORT, -1, 0)) == NULL)
               { gsc_DBCloseRs(pRs); return NULL; }

         // Blob o Memo (long value)
         if (gsc_rb2Lng(pFlag, &ColumnFlag) == GS_BAD) ColumnFlag = 0;
         if ((Stru += acutBuildList((ColumnFlag & DBCOLUMNFLAGS_ISLONG) ? RTT : RTNIL, 0)) == NULL)
            { gsc_DBCloseRs(pRs); return NULL; }

         // Fixed PrecScale
         // Cerco nella lista delle connessioni di GEOsim la connessione
         C_DBCONNECTION *pDBConn;
         if ((pDBConn = GEOsimAppl::DBCONNECTION_LIST.search_connection(pConn)))
         {
            C_PROVIDER_TYPE *pProviderType;

            if (gsc_rb2Lng(pType, &dummyLong) == GS_BAD) dummyLong = 0;
            // Cerco il tipo di dato e verifico se IsFixedPrecScale = TRUE
            if ((pProviderType = (C_PROVIDER_TYPE *) pDBConn->ptr_DataTypeList()->search_Type((DataTypeEnum) dummyLong, FALSE)) != NULL)
            {
               if ((Stru += acutBuildList((pProviderType->get_IsFixedPrecScale()) ? RTT : RTNIL, 0)) == NULL)
                  { gsc_DBCloseRs(pRs); return NULL; }
            }
            else // tipo sconosciuto allora impongo IsFixedPrecScale = FALSE
               if ((Stru += acutBuildList(RTNIL, 0)) == NULL)
                  { gsc_DBCloseRs(pRs); return NULL; }
         }
         else // altrimenti lo impongo TRUE
            if ((Stru += acutBuildList(RTT, 0)) == NULL)
               { gsc_DBCloseRs(pRs); return NULL; }

         // Fixed Length
         if ((Stru += acutBuildList((ColumnFlag & DBCOLUMNFLAGS_ISFIXEDLENGTH) ? RTT : RTNIL, 0)) == NULL)
            { gsc_DBCloseRs(pRs); return NULL; }

         // IsWrite
         if (ColumnFlag & DBCOLUMNFLAGS_WRITE)
         {
            if ((Stru += acutBuildList(RTT, 0)) == NULL) { gsc_DBCloseRs(pRs); return NULL; }
         }
         else
            if (ColumnFlag & DBCOLUMNFLAGS_WRITEUNKNOWN)
            {
               if ((Stru += acutBuildList(RTT, 0)) == NULL) { gsc_DBCloseRs(pRs); return NULL; }
            }
            else
               if ((Stru += acutBuildList(RTNIL, 0)) == NULL)
                  { gsc_DBCloseRs(pRs); return NULL; }
         
         if ((Stru += acutBuildList(RTLE, 0)) == NULL) { gsc_DBCloseRs(pRs); return NULL; }

         // posizione della colonna
         gsc_rb2Int(pOrdinalPos, &dummyInt);
         if (gsc_rb2Int(pOrdinalPos, &dummyInt) == GS_BAD)
            pOrder = new C_INT(i++);
         else
            pOrder = new C_INT(dummyInt);

         if (!pOrder) { gsc_DBCloseRs(pRs); GS_ERR_COD = eGSOutOfMem; return NULL; }

         OrderList.add_tail(pOrder);
      }

      gsc_Skip(pRs);
      i++;
   }
   gsc_DBCloseRs(pRs);

   if (Stru.GetCount() == 1) { GS_ERR_COD = eGSTableNotExisting; return NULL; }
   if ((Stru += acutBuildList(RTLE, 0)) == NULL) return NULL;

   // Ordino gli attributi per ordine di creazione
   if ((OrderedStru << acutBuildList(RTLB, 0)) == NULL) return NULL;

   i = 1;
   pos = OrderList.getpos_key(i++);
   while (pos != 0)
   {
      if ((OrderedStru += gsc_nthcopy(pos - 1, Stru.get_head())) == NULL) return NULL;
      pos = OrderList.getpos_key(i++);
   }

   if ((OrderedStru += acutBuildList(RTLE, 0)) == NULL) return NULL;

   OrderedStru.ReleaseAllAtDistruction(GS_BAD);

   return OrderedStru.get_head();
}


/*********************************************************/
/*.doc gsc_InitDBInsRow                       <external> */
/*+
  Questa funzione genera un recordset per l'inserimento di righe in una tabella.
  Parametri:
  _ConnectionPtr &pConn;      Puntatore alla connessione database
  const TCHAR    *TableRef;   Riferimento completo tabella
  _RecordsetPtr  &pRs;        Recordset   

  Restituisce GS_GOOD in caso di successo, altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_InitDBInsRow(_ConnectionPtr &pConn, const TCHAR *TableRef, _RecordsetPtr &pRs)
{
   return gsc_DBOpenRs(pConn, TableRef, pRs, adOpenDynamic, adLockOptimistic, adCmdTable);
}


/*********************************************************/
/*.doc gsc_DBInsRow                           <external> */
/*+
  Questa funzione inserisce una riga in una tabella.
  Parametri:
  _CommandPtr    &pCmd;       Comando con i parametri allocati (out)
  C_RB_LIST      &ColVal;     Lista ((<nome colonna> <valore>)...) dei campi da inserire
  int retest;                 Se MORETESTS -> in caso di errore riprova n volte 
                              con i tempi di attesa impostati poi ritorna GS_BAD,
                              ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                              (default = MORETESTS)
  int PrintError;             Utilizzato solo se <retest> = ONETEST. 
                              Se il flag = GS_GOOD in caso di errore viene
                              stampato il messaggio relativo altrimenti non
                              viene stampato alcun messaggio (default = GS_GOOD)

  oppure:

  _RecordsetPtr  &pRs;        RecordSet
  C_RB_LIST      &ColVal;     Lista ((<nome colonna> <valore>)...) dei campi da inserire
  int retest;                 Se MORETESTS -> in caso di errore riprova n volte 
                              con i tempi di attesa impostati poi ritorna GS_BAD,
                              ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                              (default = MORETESTS)
  int PrintError;             Utilizzato solo se <retest> = ONETEST. 
                              Se il flag = GS_GOOD in caso di errore viene
                              stampato il messaggio relativo altrimenti non
                              viene stampato alcun messaggio (default = GS_GOOD)

  opppure:

  _RecordsetPtr &pDestRs;     RecordSet destinazione
  _RecordsetPtr &pSourceRs;   RecordSet sorgente
  int retest;                 Se MORETESTS -> in caso di errore riprova n volte 
                              con i tempi di attesa impostati poi ritorna GS_BAD,
                              ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                              (default = MORETESTS)
  int PrintError;             Utilizzato solo se <retest> = ONETEST. 
                              Se il flag = GS_GOOD in caso di errore viene
                              stampato il messaggio relativo altrimenti non
                              viene stampato alcun messaggio (default = GS_GOOD)

  Restituisce GS_GOOD in caso di successo, altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_DBInsRow(_CommandPtr &pCmd, C_RB_LIST &ColVal, int retest, int PrintError)
{ 
   // setto i valori dei parametri
   if (gsc_SetDBParam(pCmd, ColVal.get_head()) == GS_BAD) return GS_BAD;
   // eseguo inserimento
   return gsc_ExeCmd(pCmd, NULL, retest, PrintError);
}
int gsc_DBInsRow(_RecordsetPtr &pRs, C_RB_LIST &ColVal, int retest, int PrintError)
{
   return gsc_DBInsRow(pRs, ColVal.get_head(), retest, PrintError);
}
int gsc_DBInsRow(_RecordsetPtr &pRs, presbuf ColVal, int retest, int PrintError)
{ 
   C_RB_LIST ErrList;
   int       i = 1, Res = GS_GOOD;

   do 
   {
      i++;
      ErrList.remove_all();

      try
	   {
	      pRs->AddNew();
         Res = gsc_DBSetRowValues(pRs, ColVal);
      }

	   catch (_com_error &e)
	   {
         ErrList << gsc_get_DBErrs(e);

         if (gsc_is_IntegrityConstraint_DBErr(e, (_ConnectionPtr) pRs->GetActiveConnection()))
            GS_ERR_COD = eGSIntConstr;
         else
            GS_ERR_COD = eGSAddRow;
         
         Res = GS_BAD;
	   }

      if (Res == GS_BAD)
      {         
         pRs->CancelUpdate(); 
         
         if (retest != MORETESTS)
         {
            if (PrintError == GS_GOOD) gsc_PrintError(ErrList);
            return GS_BAD;
         }
         else
         {
            if (i > GEOsimAppl::GLOBALVARS.get_NumTest())
            {
               if (gsc_dderroradmin(ErrList) != GS_GOOD)
                  return GS_BAD; // si abbandona

               i = 1;
            }
            else
               gsc_wait(GEOsimAppl::GLOBALVARS.get_WaitTime()); // attesa tra un tentativo e l'altro

            continue;
         }
      }
      break;
   }
   while (1);

   return GS_GOOD;
}
int gsc_DBInsRow(_RecordsetPtr &pDestRs, _RecordsetPtr &pSourceRs, int retest, int PrintError)
{
   C_RB_LIST      ErrList;
   int            i = 1, Res = GS_GOOD;
   
   if (gsc_isEOF(pSourceRs) == GS_GOOD) return GS_GOOD;

   do 
   {
      i++;
      ErrList.remove_all();

      try
	   {
	      pDestRs->AddNew();
         Res = gsc_DBSetRowValues(pDestRs, pSourceRs);
      }

	   catch (_com_error &e)
	   {
         ErrList << gsc_get_DBErrs(e);

         if (gsc_is_IntegrityConstraint_DBErr(e, (_ConnectionPtr) pDestRs->GetActiveConnection()))
            GS_ERR_COD = eGSIntConstr;
         else
            GS_ERR_COD = eGSAddRow;

         Res = GS_BAD;
	   }

      if (Res == GS_BAD)
      {     
         pDestRs->CancelUpdate(); 
      
         if (retest != MORETESTS)
         {
            if (PrintError == GS_GOOD) gsc_PrintError(ErrList);
            return GS_BAD;
         }
         else
         {
            if (i > GEOsimAppl::GLOBALVARS.get_NumTest())
            {
               if (gsc_dderroradmin(ErrList) != GS_GOOD)
                  return GS_BAD; // si abbandona

               i = 1;
            }
            else
               gsc_wait(GEOsimAppl::GLOBALVARS.get_WaitTime()); // attesa tra un tentativo e l'altro

            continue;
         }
      }
      break;
   }
   while (1);
      
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_DBInsRowSet                        <external> */
/*+
  Questa funzione inserisce un set di righe in una tabella.
  Ci sono 2 versioni di questa funzione perchè usando il recordset
  di destinazione in una transazione con PostgreSQL dopo 40.000 record si pianta.
  Usando un comando precompilato di destinazione è più lento ma non si pianta...
  Parametri:
  _RecordsetPtr &pDestRs;     RecordSet destinazione
  _RecordsetPtr &pSourceRs;   RecordSet sorgente
  int retest;                 Se MORETESTS -> in caso di errore riprova n volte 
                              con i tempi di attesa impostati poi ritorna GS_BAD,
                              ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                              (default = MORETESTS)
  int PrintError;             Utilizzato solo se <retest> = ONETEST. 
                              Se il flag = GS_GOOD in caso di errore viene
                              stampato il messaggio relativo altrimenti non
                              viene stampato alcun messaggio (default = GS_GOOD)
  int CounterToVideo;         Flag, se = GS_GOOD stampa a video il numero di record
                              che si stanno elaborando (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo, altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_DBInsRowSet(_RecordsetPtr &pDestRs, _RecordsetPtr &pSourceRs, int retest,
                    int PrintError, int CounterToVideo)
{
   C_RB_LIST      ErrList;
   int            Res = GS_GOOD, TransSupported = FALSE, ConnOK = FALSE;
   long           i = 0;
   _ConnectionPtr pConn;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(1073)); // "Copia valori di database"
   StatusLineMsg.Init(gsc_msg(404), LARGE_STEP); // ogni 1000 "%ld record copiati."

   try
	{
      pConn = pDestRs->GetActiveConnection();
      ConnOK = TRUE;
      pConn->BeginTrans();
      TransSupported = TRUE;
   }

	catch (_com_error &e)
	{
      if (!ConnOK)
         // codice errore per "L'operazione richiesta dall'applicazione non è supportata dal provider"
         if (e.Error() != -2146825037)
         {
            ErrList << gsc_get_DBErrs(e);

            if (PrintError == GS_GOOD) gsc_PrintError(ErrList);
            GS_ERR_COD = eGSAddRow;
            return GS_BAD;
         }
   }
   
   while (gsc_isEOF(pSourceRs) == GS_BAD)
   {
      if (gsc_DBInsRow(pDestRs, pSourceRs, retest, PrintError) == GS_BAD)     
         { Res = GS_BAD; break; }
      if (CounterToVideo)
         StatusLineMsg.Set(++i); // "%ld record copiati."

      gsc_Skip(pSourceRs);
   }

   if (TransSupported)
   {
      try
	   {
         if (Res == GS_BAD) pConn->RollbackTrans();
         else pConn->CommitTrans();
      }

      // per sicurezza
	   catch (_com_error &e)
	   {
         // codice errore per "L'operazione richiesta dall'applicazione non è supportata dal provider"
         if (e.Error() != -2146825037)
         {
            ErrList << gsc_get_DBErrs(e);

            if (PrintError == GS_GOOD) gsc_PrintError(ErrList);
            GS_ERR_COD = eGSAddRow;
            return GS_BAD;
         }
      }
   }

   if (CounterToVideo == GS_GOOD)
      StatusLineMsg.End(gsc_msg(404), i); // "%ld record copiati."

   return Res;
}
int gsc_DBInsRowSet(_CommandPtr &pDestCmd, _RecordsetPtr &pSourceRs, int retest,
                    int PrintError, int CounterToVideo)
{ 
   C_RB_LIST      ErrList, ColVal;
   int            Res = GS_GOOD, TransSupported = FALSE, ConnOK = FALSE;
   long           i = 0;
   _ConnectionPtr pConn;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(1073)); // "Copia valori di database"
   StatusLineMsg.Init(gsc_msg(404), LARGE_STEP); // ogni 1000 "%ld record copiati."

   try
	{
      pConn = pDestCmd->GetActiveConnection();
      ConnOK = TRUE;
      pConn->BeginTrans();
      TransSupported = TRUE;
   }

	catch (_com_error &e)
	{
      if (!ConnOK)
         // codice errore per "L'operazione richiesta dall'applicazione non è supportata dal provider"
         if (e.Error() != -2146825037)
         {
            ErrList << gsc_get_DBErrs(e);

            if (PrintError == GS_GOOD) gsc_PrintError(ErrList);
            GS_ERR_COD = eGSAddRow;
            return GS_BAD;
         }
   }
   
   while (gsc_isEOF(pSourceRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pSourceRs, ColVal) != GS_GOOD) { Res = GS_BAD; break; }
      if (gsc_DBInsRow(pDestCmd, ColVal, retest, PrintError) == GS_BAD)     
         { Res = GS_BAD; break; }
      if (CounterToVideo)
         StatusLineMsg.Set(++i); // "%ld record copiati."

      gsc_Skip(pSourceRs);
   }

   if (TransSupported)
   {
      try
	   {
         if (Res == GS_BAD) pConn->RollbackTrans();
         else pConn->CommitTrans();
      }

      // per sicurezza
	   catch (_com_error &e)
	   {
         // codice errore per "L'operazione richiesta dall'applicazione non è supportata dal provider"
         if (e.Error() != -2146825037)
         {
            ErrList << gsc_get_DBErrs(e);

            if (PrintError == GS_GOOD) gsc_PrintError(ErrList);
            GS_ERR_COD = eGSAddRow;
            return GS_BAD;
         }
      }
   }

   if (CounterToVideo == GS_GOOD) 
      StatusLineMsg.End(gsc_msg(404), i); // "%ld record copiati."

   return Res;
}


/*********************************************************/
/*.doc gsc_DBUpdRow                           <external> */
/*+
  Questa funzione modifica i valori della riga corrente di un recordset.
  Parametri:
  _RecordsetPtr  &pRs;        RecordSet
  C_RB_LIST      &ColVal;     Lista ((<nome colonna> <valore>)...) dei campi da inserire
  int retest;                 Se MORETESTS -> in caso di errore riprova n volte 
                              con i tempi di attesa impostati poi ritorna GS_BAD,
                              ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                              (default = MORETESTS)
  int PrintError;             Utilizzato solo se <retest> = ONETEST. 
                              Se il flag = GS_GOOD in caso di errore viene
                              stampato il messaggio relativo altrimenti non
                              viene stampato alcun messaggio (default = GS_GOOD)

  Restituisce GS_GOOD in caso di successo, altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_DBUpdRow(_RecordsetPtr &pRs, C_RB_LIST &ColVal, int retest, int PrintError)
{ 
   C_RB_LIST ErrList;
   int       i = 1, Res = GS_GOOD;

   do 
   {
      i++;
      ErrList.remove_all();

      try
	   {
         Res = gsc_DBSetRowValues(pRs, ColVal.get_head());
      }

	   catch (_com_error &e)
	   {
         ErrList << gsc_get_DBErrs(e);

         if (gsc_is_IntegrityConstraint_DBErr(e, (_ConnectionPtr) pRs->GetActiveConnection()))
            GS_ERR_COD = eGSIntConstr;
         else
            GS_ERR_COD = eGSUpdRow;

         Res = GS_BAD;
	   }

      if (Res == GS_BAD)
      {         
         try { pRs->CancelUpdate(); }
	      catch (_com_error) { }
         
         if (retest != MORETESTS)
         {
            if (PrintError == GS_GOOD) gsc_PrintError(ErrList);
            return GS_BAD;
         }
         else
         {
            if (i > GEOsimAppl::GLOBALVARS.get_NumTest())
            {
               if (gsc_dderroradmin(ErrList) != GS_GOOD)
                  return GS_BAD; // si abbandona

               i = 1;
            }
            else
               gsc_wait(GEOsimAppl::GLOBALVARS.get_WaitTime()); // attesa tra un tentativo e l'altro

            continue;
         }
      }
      break;
   }
   while (1);

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_DBSetRowValues                     <internal> */
/*+
  Questa funzione opera in congiunzione con "gsc_DBInsRow" e "gsc_DBUpdRow".
  Parametri:
  _RecordsetPtr  &pRs;        RecordSet
  presbuf        ColVal;      Lista ((<nome colonna> <valore>)...) dei campi da inserire

  oppure
  _RecordsetPtr  &pDestRs;    RecordSet destinazione
  _RecordsetPtr  &pSourceRs;  RecordSet sorgente

  Restituisce GS_GOOD in caso di successo, altrimenti restituisce GS_BAD.
  N.B. non setto GS_ERR_COD perchè lo lascio alla funzione chiamante.
-*/  
/*********************************************************/
int gsc_DBSetRowValues(_RecordsetPtr &pRs, presbuf ColVal)
{ 
   presbuf      pFieldName, pFieldValue;
   FieldPtr     pField;
   DataTypeEnum DataType;
   _variant_t   Val;
   long         AttributesFld;

   pFieldName = ColVal->rbnext->rbnext; // mi fido che sia passato correttamente (per velocità)
   while (pFieldName)
   {
      gsc_set_variant_t(Val, pFieldName->resval.rstring); // di comodo
      pField   = pRs->Fields->GetItem(Val);
      DataType = pField->GetType();

      AttributesFld = pField->GetAttributes();
      pFieldValue   = pFieldName->rbnext;
      // se è scrivibile oppure non si riesce a determinare se è scrivibile
      if (AttributesFld & adFldUpdatable || AttributesFld & adFldUnknownUpdatable) 
         if ((AttributesFld & adFldLong) && // BLOB (il valore è stringa)
             (pFieldValue->restype == 1004))          // dato binario
         {
            if (gsc_SetBLOB(pFieldValue->resval.rbinary.buf,
                            pFieldValue->resval.rbinary.clen, pField) == GS_BAD)
               return GS_BAD;
         }
         else
         {
            if (gsc_Rb2Variant(pFieldValue, Val, &DataType) == GS_BAD)
               return GS_BAD;
            pField->PutValue(Val);
         }
      pFieldValue = pFieldValue->rbnext->rbnext; // parentesi tonda successiva
      pFieldName = (pFieldValue->restype == RTLB) ? pFieldValue->rbnext : NULL; 
   }

	pRs->Update();

   return GS_GOOD;
}
int gsc_DBSetRowValues(_RecordsetPtr &pDestRs, _RecordsetPtr &pSourceRs)
{ 
   FieldPtr   pSourceField, pDestField;
   long       FldCount, ndx, FldAttributes;
   _variant_t Value;

   try
   {
      FldCount = pSourceRs->Fields->Count;
      for (ndx = 0; ndx < FldCount; ndx++)
      {
         pSourceField  = pSourceRs->Fields->GetItem(ndx);
         pDestField    = pDestRs->Fields->GetItem(ndx);
         FldAttributes = pDestField->GetAttributes();

         // se è scrivibile oppure non si riesce a determinare se è scrivibile
         if (FldAttributes & adFldUpdatable || FldAttributes & adFldUnknownUpdatable) 
         {
            Value = pSourceField->GetValue();

            if (pDestField->GetType() == adBoolean &&
                (Value.vt == VT_EMPTY || Value.vt == VT_NULL))
               // Poichè ACCESS non accetta i boolean vuoti allora setto il valore a false
               gsc_set_variant_t(Value, (bool) false);

            pDestField->PutValue(Value);
         }
      }
	   pDestRs->Update();
   }

   #if defined(GSDEBUG) // se versione per debugging
	   catch (_com_error &e)
	   {
         printDBErrs(e);
   #else
	   catch (_com_error)
	   {
   #endif
      GS_ERR_COD = eGSInvAttribValue;
      return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_RefTable2Path                      <internal> */
/*+
  Questa funzione trasforma il riferimento completo di una tabella in path del
  file relativo alla stessa. Questo può essere fatto solamente per quelle
  connessioni OLE-DB che implementano le tabelle come singoli file.
  Parametri:
  const TCHAR *DBMSName;   Nome DBMS
  C_STRING &Catalog;       Catalogo (es. "c:\\aa")
  C_STRING &Schema;        Schema
  C_STRING &Table;         Tabella (es. "bb")
  C_STRING &Path;          Path del file (out) (es. "c:\\aa\\bb.dbf")

  Restituisce GS_GOOD in caso di successo, altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_RefTable2Path(const TCHAR *DBMSName, C_STRING &Catalog, C_STRING &Schema,
                      C_STRING &Table, C_STRING &Path)
{
   Path = Catalog;
   Path += _T('\\');
   if (Schema.len() > 0)
   {
      Path += Schema;
      Path += _T('\\');
   }
   Path += Table;

   if (gsc_strcmp(DBMSName, _T("DBASE")) == 0 ||
       gsc_strcmp(DBMSName, _T("FOXPRO")) == 0 ||
       gsc_strcmp(DBMSName, _T("VISUAL FOXPRO")) == 0 ||
       gsc_strcmp(DBMSName, _T("DBASE V")) == 0)            // MERANT
      Path += _T(".DBF");
   else
   if (gsc_strcmp(DBMSName, _T("EXCEL")) == 0)
      Path += _T(".XLS");
   else
   if (gsc_strcmp(DBMSName, _T("TEXT")) == 0)
      Path += _T(".TXT");
   else
   if (gsc_strcmp(DBMSName, _T("PARADOX")) == 0)
      Path += _T(".DB");
   else
      { GS_ERR_COD = eGSUnkwnDBMS; return GS_BAD; }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_GetNumAggrValue                    <external> */
/*+
  Questa funzione è legge un numero risultato di una funzione di aggregazione
  (SUM, COUNT, MIN, MAX, ...) da una tabella.
  Parametri:
  _ConnectionPtr &pConn;  Puntatore alla connessione database
  const TCHAR *TableRef;  Riferimento completo della tabella
  const TCHAR *FieldName; Nome del campo su cui effettuare la funz. di aggr.
  const TCHAR *AggrFun;   Funzione di Aggregazione
  ads_real *Value;        Risultato

  Restituisce GS_GOOD in caso di successo, altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_GetNumAggrValue(_ConnectionPtr &pConn, const TCHAR *TableRef, const TCHAR *FieldName,
                        const TCHAR *AggrFun, ads_real *Value)
{
   C_STRING      statement;
   presbuf       p;
   C_RB_LIST     ColValues;
   _RecordsetPtr pRs;

   statement = _T("SELECT ");
   statement += AggrFun;
   statement += _T('(');
   statement += FieldName;
   statement += _T(") FROM ");
   statement += TableRef;

   if (gsc_DBOpenRs(pConn, statement.get_name(), pRs) == GS_BAD) return GS_BAD;
   if (gsc_isEOF(pRs) == GS_GOOD) // tabella senza righe
      { gsc_DBCloseRs(pRs); return GS_CAN; }
   if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { gsc_DBCloseRs(pRs); return GS_BAD; }
   gsc_DBCloseRs(pRs);

   p = gsc_nth(1, gsc_nth(0, ColValues.get_head()));
   switch (p->restype)
   {
      case RTSTR:
         if (ads_distof(p->resval.rstring, 2, Value) != RTNORM) return GS_BAD;
         break;
      case RTREAL:
         *Value = p->resval.rreal;
         break;
      case RTLONG:
         *Value = p->resval.rlong;
         break;
      case RTSHORT:
         *Value = (ads_real) p->resval.rint;
         break;
      case RTNONE:
         return GS_CAN; // tabella senza righe
      default:
         GS_ERR_COD = eGSReadRow;
         return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_DBCheckSql                         <external> */
/*+                                                                       
  Controlla la correttezza dell'istruzione SQL.
  Parametri:
  _ConnectionPtr &pConn;      Connessione OLE-DB
  const TCHAR *Statement;      Istruzione SQL
  
  Restituisce NULL in caso di successo oppure una stringa con messaggio 
  di errore. 
  N.B. Alloca memoria
-*/  
/*********************************************************/
TCHAR *gsc_DBCheckSql(_ConnectionPtr &pConn, const TCHAR *Statement)
{
   _CommandPtr pCmd;
   
   if (gsc_PrepareCmd(pConn, Statement, pCmd) == GS_GOOD) return NULL;
   
   if (pConn->Errors->Count > 0)
   {
      _bstr_t dummy;
      
      // messaggio di errore
      dummy = pConn->Errors->GetItem((long) 0)->Description;
      return gsc_tostring((wchar_t *) dummy);
   }
   return NULL;
}


/*********************************************************/
/*.doc gsc_DBGetFormats                       <external> */
/*+
  Questa funzione prepara i formati testo per l'import\export dei dati. 
  Parametri:
  _RecordsetPtr &pRs;  RecordSet
  int modo;            Modo di esportazione dati:
                       DELIMITED  colonne delimitate con <delimiter>; '\n' a 
                                  fine riga
                       SDF        colonne spaziate per le loro dimensioni 
                                  (senza intestazione <nomi colonne>) '\n' a 
                                  fine riga
                       SDF_HEADER colonne spaziate per le loro dimensioni e per
                                  le dimensioni delle intestazioni (prima riga
                                  intestazione <nomi colonne>); '\n' a fine 
                                  riga (fra una colonna e l'altra ' ')
  TCHAR *delimiter;    Stringa delimitatrice di fine colonna
  C_STR_LIST &Formats; Lista dei formati
  
  Restituisce i formati in caso di successo altrimenti NULL.
  N.B. la funzione NON tratta campi MEMO o di tipi incompabili a TXT
-*/  
/*********************************************************/
int gsc_DBGetFormats(_RecordsetPtr &pRs, int modo, TCHAR *delimiter, C_STR_LIST &Formats)
{
   long      i, tot;
   FieldsPtr pFields;
   C_STR     *pFormat;
   TCHAR     Buffer[32];     // Formato colonna 
   size_t    Len, NameLen;

   try
	{
      Formats.remove_all();
      pFields = pRs->GetFields();
      tot     = pFields->Count; 

      for (i = 0; i < tot; i++) 
      {
         Buffer[0] = _T('\0');
         if (gsc_DBIsNumeric(pFields->GetItem(i)->GetType()) == GS_GOOD)
         {
            switch (modo)     // testo giustificato a destra
            {
               case DELIMITED:  // con delimitatore
                  if (i + 1 == tot) // elimino delimitatore per ultima colonna
                     swprintf(Buffer, 32, _T("%%s"));
                  else
                     swprintf(Buffer, 32, _T("%%s%s"), delimiter);
                  break;
               case SDF:        // SDF semplice
                  Len = (int) pFields->GetItem(i)->GetPrecision();
                  swprintf(Buffer, 32, _T("%%%ds"), Len);
                  break;
               case SDF_HEADER: // SDF con intestazione
                  NameLen = wcslen(pFields->GetItem(i)->GetName());
                  Len     = (int) pFields->GetItem(i)->GetPrecision();
                  swprintf(Buffer, 32, _T("%%%ds"), max(NameLen, Len));
                  break;
            }
         }
         else
         if (gsc_DBIsDate(pFields->GetItem(i)->GetType()) == GS_GOOD ||
             gsc_DBIsTimestamp(pFields->GetItem(i)->GetType()) == GS_GOOD)
         {
            switch (modo)     // testo giustificato a destra
            {
               case DELIMITED:  // con delimitatore
                  if (i + 1 == tot) // elimino delimitatore per ultima colonna
                     swprintf(Buffer, 32, _T("%%s"));
                  else
                     swprintf(Buffer, 32, _T("%%s%s"), delimiter);
                  break;
               case SDF:        // SDF semplice
                  Len = 30; // per data estesa
                  swprintf(Buffer, 32, _T("%%-%ds"), Len);
                  break;
               case SDF_HEADER: // SDF con intestazione
                  NameLen = wcslen(pFields->GetItem(i)->GetName());
                  Len = 30; // per data estesa
                  swprintf(Buffer, 32, _T("%%-%ds"), max(NameLen, Len));
                  break;
            }
         }
         else
         if (gsc_DBIsTime(pFields->GetItem(i)->GetType()) == GS_GOOD)
         {
            switch (modo)     // testo giustificato a destra
            {
               case DELIMITED:  // con delimitatore
                  if (i + 1 == tot) // elimino delimitatore per ultima colonna
                     swprintf(Buffer, 32, _T("%%s"));
                  else
                     swprintf(Buffer, 32, _T("%%s%s"), delimiter);
                  break;
               case SDF:        // SDF semplice
                  Len = 8; // per ora (hh:mm:ss)
                  swprintf(Buffer, 32, _T("%%-%ds"), Len);
                  break;
               case SDF_HEADER: // SDF con intestazione
                  NameLen = wcslen(pFields->GetItem(i)->GetName());
                  Len = 8; // per ora (hh:mm:ss)
                  swprintf(Buffer, 32, _T("%%-%ds"), max(NameLen, Len));
                  break;
            }
         }
         else
         {
            switch (modo)     // testo giustificato a sinistra
            {
               case DELIMITED:  // con delimitatore
                  if (i + 1 == tot) // elimino delimitatore per ultima colonna
                     swprintf(Buffer, 32, _T("\"%%s\""));
                  else
                     swprintf(Buffer, 32, _T("\"%%s\"%s"), delimiter);
                  break;
               case SDF:        // SDF semplice
                  Len = (int) pFields->GetItem(i)->GetDefinedSize();
                  swprintf(Buffer, 32, _T("%%-%ds"), Len);
                  break;
               case SDF_HEADER: // SDF con intestazione
                  NameLen = wcslen(pFields->GetItem(i)->GetName());
                  Len     = (int) pFields->GetItem(i)->GetDefinedSize();
                  swprintf(Buffer, 32, _T("%%-%ds"), max(NameLen, Len));
                  break;
            }
         }

         if ((pFormat = new C_STR(Buffer)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

         Formats.add_tail(pFormat);
      }
   }

   catch(_com_error &e)
	{
      printDBErrs(e);
      return GS_BAD;
	}
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_DBsprintf                          <external> */
/*+
  Questa funzione stampa una riga di dati in modo formattato. 
  Parametri:
  FILE *fhandle;        Handle del file in cui scrivere
  C_RB_LIST &ColValues; Lista colonna-valore
  C_STR_LIST &Formats;  Lista dei formati
  
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_DBsprintf(FILE *fhandle, C_RB_LIST &ColValues, C_STR_LIST &Formats)
{
   return gsc_DBsprintf(fhandle, ColValues.get_head(), Formats);
}
int gsc_DBsprintf(FILE *fhandle, presbuf ColValues, C_STR_LIST &Formats)
{
   C_STR    *pFormat;
   C_STRING Buffer;
   presbuf  p;
   
   pFormat = (C_STR *) Formats.get_head();
   p       = ColValues->rbnext->rbnext; // mi fido che sia passato correttamente (per velocità)
   while (pFormat && p)
   {
      p = p->rbnext;
      if (pFormat->len() > 0)
         if (Buffer.paste(gsc_rb2str(p)))
         {
            if (fwprintf(fhandle, pFormat->get_name(), Buffer.get_name()) < 0)
               { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
         }
         else
            if (fwprintf(fhandle, pFormat->get_name(), GS_EMPTYSTR) < 0)
               { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

      pFormat = (C_STR *) Formats.get_next();      
      p       = p->rbnext->rbnext; // parentesi tonda successiva
      p = (p->restype != RTLB) ? NULL: p->rbnext;
   }

   if (fwprintf(fhandle, GS_LFSTR) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_DBsprintfHeader                    <external> */
/*+
  Questa funzione stampa l'intestazione di una tabella in modo formattato. 
  Parametri:
  FILE *fhandle;       Handle del file in cui scrivere
  _RecordsetPtr &pRs;  RecordSet
  C_STR_LIST &Formats; Lista dei formati
  
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_DBsprintfHeader(FILE *fhandle, _RecordsetPtr &pRs, C_STR_LIST &Formats)
{
   long      i, tot;
   FieldsPtr pFields;
   C_STR     *pFormat;

   try
	{
      pFields = pRs->GetFields();
      tot     = pFields->Count; 

      pFormat = (C_STR *) Formats.get_head();
      for (i = 0; i < tot && pFormat; i++) 
      {
         pFields->GetItem(i)->GetName();

         if (pFormat->len() > 0)
            if (fwprintf(fhandle, pFormat->get_name(), (TCHAR *)_bstr_t(pFields->GetItem(i)->GetName())) < 0)
               { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

         pFormat = (C_STR *) Formats.get_next();
      }
   }

   catch(_com_error &e)
	{
      printDBErrs(e);
      return GS_BAD;
	}

   if (fwprintf(fhandle, GS_LFSTR) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return GS_GOOD;
}


/****************************************************************************/
/*.doc gs_readrows                                               (external) */
/*+
  Funzione lisp per leggere un set di righe da una o più tabelle.
  Parametri:
  Lista RESBUF 
  (("UDL_FILE" <Connection>) ("UDL_PROP" <Properties>) ("TABLE_REF" <TableRef>)
   ("SQL_STATEMENT" <sql statement>))
  dove:
  <Connection> = <file UDL> | <stringa di connessione>
  <Properties> = stringa delle proprietà | ((<prop1><value>)(<prop1><value>)...)
  <TableRef>   = riferimento completo tabella | (<cat><schema><tabella>)
  <sql statement> = istruzione sql da eseguire

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/
/****************************************************************************/
int gs_readrows(void)
{
   presbuf        arg = acedGetArgs();
   C_DBCONNECTION *pConn;
   C_STRING       Statement;
   C_RB_LIST      ColValues;

   acedRetNil();

   // Legge nella lista dei parametri i riferimenti alla tabella
   if ((pConn = gsc_getConnectionFromLisp(arg)) == NULL) return RTERROR;

   if ((arg = gsc_CdrAssoc(_T("SQL_STATEMENT"), arg, FALSE)) == NULL || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   Statement = arg->resval.rstring; // istruzione sql

   if (pConn->ReadRows(Statement, ColValues) == GS_BAD) return RTERROR;
   ColValues.remove_head(); // elimino prima parentesi aperta
   ColValues.remove_tail(); // elimino ultima parentesi chiusa
   ColValues.LspRetList();

   return RTNORM;
}


/****************************************************************************/
/*.doc gs_SplitFullRefTable                                      (external) */
/*+
  Funzione che divide un riferimento completo a tabella in catalogo, schema
  e tabella.
  Parametri:
  (("UDL_FILE" <file UDL> || <stringa di connessione>) ("UDL_PROP" <value>) ("TABLE_REF" <value>))
  Ritorna una lista (<cat><sch><tab>)
-*/
/****************************************************************************/
int gs_SplitFullRefTable(void)
{
   presbuf        arg = acedGetArgs();
   C_DBCONNECTION *pConn;
   C_STRING       FullRefTable, Catalog, Schema, Table;
   C_RB_LIST      ret;

   acedRetNil();

   // Legge nella lista dei parametri i riferimenti alla tabella
   if ((pConn = gsc_getConnectionFromLisp(arg, &FullRefTable)) == NULL) return RTERROR;

   if (pConn->split_FullRefTable(FullRefTable, Catalog, Schema, Table) == GS_BAD)
      return RTERROR;

   if (Catalog.len() > 0)
      { if ((ret << acutBuildList(RTSTR, Catalog.get_name(), 0)) == NULL) return RTERROR; }
   else
      if ((ret << acutBuildList(RTNIL, 0)) == NULL) return RTERROR;

   if (Schema.len() > 0)
      { if ((ret += acutBuildList(RTSTR, Schema.get_name(), 0)) == NULL) return RTERROR; }
   else
      if ((ret += acutBuildList(RTNIL, 0)) == NULL) return RTERROR;

   if (Table.len() > 0)
      { if ((ret += acutBuildList(RTSTR, Table.get_name(), 0)) == NULL) return RTERROR; }
   else
      if ((ret += acutBuildList(RTNIL, 0)) == NULL) return RTERROR;
   
   ret.LspRetList();

   return RTNORM;
}


/****************************************************************************/
/*.doc gs_getFullRefTable                                        (external) */
/*+
  Funzione LISP che ottiene un riferimento completo a tabella in 
  catalogo, schema e tabella.
  Parametri:
  Lista RESBUF 
  (("UDL_FILE" <file UDL> || <stringa di connessione>) ("UDL_PROP" <value>) ("TABLE_REF" <value>))

  Ritorna una stringa in caso di successo altrimenti nil.
-*/
/****************************************************************************/
int gs_getFullRefTable(void)
{
   presbuf        arg = acedGetArgs();
   C_DBCONNECTION *pConn;
   C_STRING       FullRefTable;

   acedRetNil();

   // Legge nella lista dei parametri i riferimenti alla tabella
   if ((pConn = gsc_getConnectionFromLisp(arg, &FullRefTable)) == NULL) return RTERROR;
   acedRetStr(FullRefTable.get_name());

   return RTNORM;
}


/*****************************************************************************/
/*  ISTRUZIONI DI UTILITA'   -    FINE
/*  GESTIONE PARAMETRI SQL       -    INIZIO
/*****************************************************************************/


/*********************************************************/
/*.doc gsc_CreateDBParameter                  <external> */
/*+
  Questa funzione crea un parametro con le caratteristiche date.
  Parametri:
  _ParameterPtr &pParam;                  Oggetto parametro (out)
  const TCHAR *Name;                      Nome
  enum DataTypeEnum Type;                 Tipo
  long Size = 0;                          Dimensione in byte o in numero caratteri
                                          (default = 0; usare solo per tipo carattere)
  enum ParameterDirectionEnum Direction;  Direzione (default = adParamInput)

  Restituisce GS_GOOD in caso di successo, altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_CreateDBParameter(_ParameterPtr &pParam, const TCHAR *Name, enum DataTypeEnum Type,
                          long Size, enum ParameterDirectionEnum Direction)
{
   // Creo una nuovo parametro
   if (FAILED(pParam.CreateInstance(__uuidof(Parameter))))
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   try
	{
      pParam->PutName(Name);
      pParam->PutType(Type);
      pParam->PutDirection(Direction);
      if (Size > 0) pParam->PutSize(Size);
   }

	catch (_com_error &e)
	{
      printDBErrs(e);
      GS_ERR_COD = eGSInitPar;
      return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_GetDBParam                         <external> */
/*+
  Questa funzione è di test per i parametri dei comandi SQL.
  Parametri:
  _ConnectionPtr &pConn;  Puntatore alla connessione database
  _CommandPtr &pCmd;

  Restituisce GS_GOOD in caso di successo, ltrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_GetDBParam(_ConnectionPtr &pConn, _CommandPtr &pCmd)
{
	HRESULT hr = E_FAIL;

   try
	{
      if (FAILED(hr = pCmd->Parameters->Refresh())) _com_issue_error(hr);
   }

	catch (_com_error &e)
	{
      printDBErrs(e, pConn);
      GS_ERR_COD = eGSConnect;
      return GS_BAD;
   }

   return GS_BAD;
}


/*********************************************************/
/*.doc gsc_SetDBParam                         <external> */
/*+
  Questa funzione setta i valori dei parametri.
  Parametri:
  _CommandPtr    &pCmd;       Comando con i parametri allocati (out)
  presbuf        ColVal;      Lista ((<nome param> <valore>)...) dei parametri da inserire

  Restituisce GS_GOOD in caso di successo, altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_SetDBParam(_CommandPtr &pCmd, presbuf ColVal)
{
   presbuf       p, pCol, pName;
   _variant_t    Val;
   DataTypeEnum  DataType;
   _ParameterPtr Parameter;

   try
	{
      pCol = gsc_nth(0, ColVal);
      while (pCol && pCol->restype == RTLB)
      {
         pName = pCol->rbnext;
         p     = pName->rbnext;
         Parameter = pCmd->Parameters->GetItem(pName->resval.rstring);
         // converto il valore da resbuf a _variant_t
         DataType = Parameter->GetType();

         if ((DataType == adLongVarBinary || DataType == adLongVarWChar ||
              DataType == adLongVarChar) && p->restype == 1004) // dato binario
         {
            if (p->resval.rbinary.buf == NULL || p->resval.rbinary.clen == 0)
            {
               if (gsc_SetBLOB(GS_EMPTYSTR, 0, Parameter) == GS_BAD)
                  return GS_BAD;
            }
            else
               if (gsc_SetBLOB(p->resval.rbinary.buf, p->resval.rbinary.clen, 
                               Parameter) == GS_BAD)
                  return GS_BAD;
         }
         else
         {
            if (gsc_Rb2Variant(p, Val, &DataType) == GS_BAD) return GS_BAD;
            Parameter->PutValue(Val);
         }

         pCol = p->rbnext;
         if (pCol) pCol = pCol->rbnext;
      }
   }

	catch (_com_error &e)
	{
      printDBErrs(e);
      GS_ERR_COD = eGSInitPar;
      return GS_BAD;
   }

   return GS_GOOD;
}
int gsc_SetDBParam(_CommandPtr &pCmd, int Index, presbuf Value)
{
   _variant_t    Val;
   DataTypeEnum  DataType;
   _ParameterPtr Parameter;

   try
	{
      Parameter = pCmd->Parameters->GetItem((long) Index);
      // converto il valore da resbuf a _variant_t
      DataType = Parameter->GetType();

      if ((DataType == adLongVarBinary || DataType == adLongVarWChar ||
           DataType == adLongVarChar) && Value->restype == 1004) // dato binario
      {
         if (Value->resval.rbinary.buf == NULL || Value->resval.rbinary.clen == 0)
         {
            if (gsc_SetBLOB(GS_EMPTYSTR, 0, Parameter) == GS_BAD)
               return GS_BAD;
         }
         else
            if (gsc_SetBLOB(Value->resval.rbinary.buf, Value->resval.rbinary.clen, 
                            Parameter) == GS_BAD)
               return GS_BAD;
      }
      else
      {
         if (gsc_Rb2Variant(Value, Val, &DataType) == GS_BAD) return GS_BAD;
         Parameter->PutValue(Val);
      }
   }

	catch (_com_error &e)
	{
      printDBErrs(e);
      GS_ERR_COD = eGSInitPar;
      return GS_BAD;
   }

   return GS_GOOD;
}
int gsc_SetDBParam(_CommandPtr &pCmd, int Index, const TCHAR *Str)
{
   DataTypeEnum  DataType;
   _ParameterPtr Parameter;

   try
	{
      Parameter = pCmd->Parameters->GetItem((long) Index);

      // converto il valore da resbuf a _variant_t
      DataType = Parameter->GetType();

      if ((DataType == adLongVarBinary || DataType == adLongVarWChar ||
           DataType == adLongVarChar) && Str) // dato binario
      {
         if (gsc_SetBLOB((LPVOID) Str, (long) wcslen(Str) + 1, Parameter) == GS_BAD) return GS_BAD;
      }
      else
      {
         _variant_t Val(Str);
         Parameter->PutValue(Val);
      }
   }

	catch (_com_error &e)
	{
      printDBErrs(e);
      GS_ERR_COD = eGSInitPar;
      return GS_BAD;
   }

   return GS_GOOD;
}
int gsc_SetDBParam(_CommandPtr &pCmd, int Index, long Num)
{
   try
	{
      _variant_t Val(Num);
      pCmd->Parameters->GetItem((long) Index)->PutValue(Val);
   }

	catch (_com_error &e)
	{
      printDBErrs(e);
      GS_ERR_COD = eGSInitPar;
      return GS_BAD;
   }

   return GS_GOOD;
}
int gsc_SetDBParam(_CommandPtr &pCmd, int Index, int Num)
{
   return gsc_SetDBParam(pCmd, Index, (long) Num);
}


/*********************************************************/
/*.doc gsc_ReadDBParams                       <external> /*
/*+
  Questa funzione legge i valori dei parametri di input
  legati al comando.
  Parametri:
  _CommandPtr &pCmd;                   Puntatore al comando
  UserInteractionModeEnum WINDOW_mode; Flag, se TRUE la richiesta dei parametri avviene
                                       tramite finestra (default = GSTextInteraction)                         

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_ReadDBParams(_CommandPtr &pCmd, UserInteractionModeEnum WINDOW_mode)
{
   long          Tot, i;
   _ParameterPtr Parameter;
   TCHAR         buffer[128];

   try
	{
      Tot = pCmd->Parameters->GetCount();
      for (i = 0; i < Tot; i++)
      { 
         Parameter = pCmd->Parameters->GetItem(i);
      
         // se il parametro e non è stato ancora inizializzato
         if (Parameter->GetValue().vt == VT_EMPTY)
         {
            if (WINDOW_mode == GSTextInteraction)
            {
               acutPrintf(GS_LFSTR);
               // "Inserire valore parametro <%s>: "
               acutPrintf(gsc_msg(651), Parameter->Name);
               acedGetString(1, GS_EMPTYSTR, buffer);
            }
            else
            {
               TCHAR    Msg[TILE_STR_LIMIT];
               C_STRING Out;
               int      Result;

               // "Inserire valore parametro <%s>: "
               swprintf(Msg, TILE_STR_LIMIT, gsc_msg(651), Parameter->Name);
               if ((Result = gsc_ddinput(Msg, Out)) != GS_GOOD) return Result;
               gsc_strcpy(buffer, Out.get_name(), 128);
            }

            if (gsc_SetDBParam(pCmd, i, buffer) == GS_BAD) return GS_BAD;
         }
      }
   }

	catch (_com_error &e)
	{
      printDBErrs(e);
      GS_ERR_COD = eGSInitPar;
      return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_GetParamCount                      <external> /*
/*+
  Questa funzione legge il numero di parametri per un comando.
  Parametri:
  _CommandPtr &pCmd;       Puntatore al comando

  Restituisce il numero di parametri in caso di successo altrimenti restituisce -1.
-*/  
/*********************************************************/
int gsc_GetParamCount(_CommandPtr &pCmd, bool Refresh)
{
   try
	{
      if (Refresh)
         pCmd->Parameters->Refresh();

      return pCmd->Parameters->GetCount();
   }

	catch (_com_error &e)
	{
      printDBErrs(e);
      GS_ERR_COD = eGSInitPar;
      return -1;
   }

   return -1;
}


/*****************************************************************************/
/*  GESTIONE PARAMETRI SQL          -    FINE
/*  GESTIONE MESSAGGI DI ERRORE     -    INIZIO
/*****************************************************************************/


/**********************************************************************/
/*.doc gsc_get_DBErrs                                                 */
/*+	   
   Restituisce una lista di resbuf contenente le informazioni diagnostiche
   di ADO o del provider di database nel seguente formato:
   ((<message 1> (<param 1><param 2>...))(<message 2> (<param 1><param 2>...)) ...)

   Parametri:
   _com_error &e;          per ottenere le informazioni di ADO
   oppure
   _ConnectionPtr pConn;   per ottenere le informazioni del provider di database

   Restituisce la lista in caso di successo altrimenti restituisce NULL.
-*/  
/*********************************************************/
presbuf gsc_get_DBErrs(_com_error &e)
{
   TCHAR     Fmt[10];
   C_STRING  message;
   C_RB_LIST list;
   _bstr_t   dummy;

   if (list << acutBuildList(RTLB, 0) == NULL) return NULL;

   // Condizione di errore fornite da ADO
   // messaggio sintetico
   message = _T("ADO Error: ");
   message += e.ErrorMessage();
   if ((list += acutBuildList(RTLB, RTSTR, message.get_name(), RTLB, 0)) == NULL)
      return NULL;

   // dettaglio errore
   message = _T("ADO Error");
   if ((list += acutBuildList(RTSTR, message.get_name(), 0)) == NULL) return NULL;
   message = _T("Description: ");
   dummy   = e.Description();
   message += (wchar_t *) dummy;
   if ((list += acutBuildList(RTSTR, message.get_name(), 0)) == NULL) return NULL;
   message = _T("Source: ");
   dummy   = e.Source();
   message += (LPCSTR) dummy;
   if ((list += acutBuildList(RTSTR, message.get_name(), 0)) == NULL) return NULL;
   message = _T("Code: ");
   swprintf(Fmt, 10, _T("%08lx"), e.Error());
   message += Fmt;
   if ((list += acutBuildList(RTSTR, message.get_name(), RTLE, RTLE, RTLE, 0)) == NULL)
      return NULL;

   list.ReleaseAllAtDistruction(GS_BAD);
   
   return list.get_head();
}
presbuf gsc_get_DBErrs(_ConnectionPtr &pConn)
{
   long      iCount, i;
   C_STRING  message;
   C_RB_LIST list;
   ErrorPtr  pErr;
   _bstr_t   dummy;

   if (pConn.GetInterfacePtr() == NULL) return NULL;

   // Condizioni di errore fornite dal provider
   if ((iCount = pConn->Errors->Count) > 0)
   {
      if ((list << acutBuildList(RTLB, 0)) == NULL) return NULL;

      for (i = 0; i < iCount; i++)
      {
         pErr = pConn->Errors->GetItem(i);

         // messaggio sintetico
         message = pErr->Number;
         message += _T(" ");
         dummy   = pErr->Description;
         message += (wchar_t *) dummy;
         if ((list += acutBuildList(RTLB, RTSTR, message.get_name(), RTLB, 0)) == NULL)
            return NULL;

         // dettaglio errore
         message = _T("Error #: ");
         message += pErr->Number;
         if ((list += acutBuildList(RTSTR, message.get_name(), 0)) == NULL) return NULL;
         message = _T("Description: ");
         dummy   = pErr->Description;
         message += (LPCSTR) dummy;
         if ((list += acutBuildList(RTSTR, message.get_name(), 0)) == NULL) return NULL;
         message = _T("Source: ");
         dummy   = pErr->Description;
         message += (LPCSTR) dummy;
         if ((list += acutBuildList(RTSTR, message.get_name(), 0)) == NULL) return NULL;
         message = _T("SQL State: ");
         dummy   = pErr->SQLState;
         message += (LPCSTR) dummy;
         if ((list += acutBuildList(RTSTR, message.get_name(), 0)) == NULL) return NULL;
         message = _T("Native Error: ");
         message += pErr->NativeError;
         if ((list += acutBuildList(RTSTR, message.get_name(), 0)) == NULL) return NULL;
         if (pErr->HelpFile.length() > 0)
         {
            message = _T("Help File: ");
            dummy   = pErr->HelpFile;
            message += (LPCSTR) dummy;
            if ((list += acutBuildList(RTSTR, message.get_name(), 0)) == NULL) return NULL;
            message = _T("Help Context: ");
            message += pErr->HelpContext;
            if ((list += acutBuildList(RTSTR, message.get_name(), 0)) == NULL) return NULL;
         }

         if ((list += acutBuildList(RTLE, RTLE, 0)) == NULL) return NULL;
      }
      
      if ((list += acutBuildList(RTLE, 0)) == NULL) return NULL;
   }

   list.ReleaseAllAtDistruction(GS_BAD);
   
   return list.get_head();
}
presbuf gsc_get_DBErrs(_com_error &e, _ConnectionPtr &pConn)
{
   C_RB_LIST ErrList1, ErrList2;

   ErrList1 << gsc_get_DBErrs(e);
   ErrList1.remove_tail();
   ErrList2 << gsc_get_DBErrs(pConn);
   ErrList2.remove_head();
   ErrList2.ReleaseAllAtDistruction(GS_BAD);
   ErrList1 += &ErrList2;
   ErrList1.ReleaseAllAtDistruction(GS_BAD);

   return ErrList1.get_head();
}


/**********************************************************************/
/*.doc printDBErrs                                         <internal> */
/*+	   
  Stampa gli errori ADO e del provider database.
  Parametri:
  _com_error &e;
  _ConnectionPtr *pConn;
-*/  
/*********************************************************/
void printDBErrs(_com_error &e, _ConnectionPtr &pConn)
{
   C_RB_LIST ErrList;

   if ((ErrList << gsc_get_DBErrs(e, pConn)) == NULL) return;
   gsc_PrintError(ErrList);
}
void printDBErrs(_com_error &e)
{
   C_RB_LIST ErrList;

   if ((ErrList << gsc_get_DBErrs(e)) == NULL) return;
   gsc_PrintError(ErrList);
}
void printDBErrs(_ConnectionPtr &pConn)
{
   C_RB_LIST ErrList;

   if ((ErrList << gsc_get_DBErrs(pConn)) == NULL) return;
   gsc_PrintError(ErrList);
}


/**********************************************************************/
/*.doc gsc_is_IntegrityConstraint_DBErr                               */
/*+	   
   Verifica se l'errore è determinato da un eccezzione di tipo
   "integrity contraint" (es. chiave duplicata su indice univoco).
   Parametri:
   _com_error &e;          per ottenere le informazioni di ADO
   _ConnectionPtr pConn;   per ottenere le informazioni del provider di database

   Restituisce true se si tratta di erroe di tipo "integrity contraint"
   altrimenti restituisce false.
-*/  
/*********************************************************/
bool gsc_is_IntegrityConstraint_DBErr(_com_error &e, _ConnectionPtr &Conn)
{
   switch (e.Error())
   {
      case DB_E_INTEGRITYVIOLATION: // "A specified value violated the integrity constraints 
                                     // for a column or table"
         return true;
      case -2147217900: // "The command contained one or more errors" (JET 4 e 3.51)
      case -2147217887: // "Errors occurred"
      case -2147467259: // "OLE-DB e il provider DB non sono in grado
                        // di identificare l'errore" (Oracle 8 e postgresql)
      {
         ErrorsPtr pErrors;
         long      nCount, err;
         C_STRING  Descr;

         pErrors = Conn->GetErrors();
         nCount = pErrors->GetCount();
         for (long i = 0; i < nCount; i++)
         {
            err = pErrors->GetItem(i)->GetNativeError();
            // codice errore per "Couldn't update; duplicate values in the index, primary key, or relationship ..."
            if (err == -105121349) // JET 4 e 3.51
               return true;
            // per PostgreSQL
            Descr = (LPCSTR) pErrors->GetItem(i)->Description;
            if (Descr.at(_T("ERROR: duplicate key value violates unique constraint")) ||
                Descr.at(_T("ERRORE: un valore chiave duplicato viola il vincolo univoco")))
               return true;
         }
         break;
      }
   }

   return false;
}


/*****************************************************************************/
/*  GESTIONE MESSAGGI DI ERRORE     -     FINE
/*  GESTIONE RIFERIMENTI A DATABASE -     INIZIO
/*****************************************************************************/


/****************************************************************/
/*.doc gsc_DBstr2CatResType                          <internal> */
/*+
  Questa funzione converte una stringa in un tipo <ResourceTypeEnum>.
  Parametri: 
  TCHAR *str;           stringa
  
  Restituisce il valore corrispondente in caso di successo altrimenti EmptyRes.
-*/  
/****************************************************************/
ResourceTypeEnum gsc_DBstr2CatResType(TCHAR *str)
{
   if (gsc_strcmp(str, _T("DIR"), FALSE) == 0) return DirectoryRes;
   if (gsc_strstr(str, _T("FILE"), FALSE) == str) return FileRes; // incomincia per "FILE"
   if (gsc_strcmp(str, _T("SERVER"), FALSE) == 0) return ServerRes;
   if (gsc_strcmp(str, _T("DATABASE"), FALSE) == 0) return Database;
  
   return EmptyRes;
}


/****************************************************************/
/*.doc gsc_PropListFromConnStr                       <internal> */
/*+
  Questa funzione restituisce una lista di valori delle proprietà UDL nella
  stringa di connessione specificata.
  Nella stringa di connessione, se la proprietà si chiama 
  "Extended Properties" allora viene letta la stringa contenente i
  valori di tutte le proprietà estese. Se invece la proprietà si chiama
  [Extended Properties]<nome proprietà> allora viene letta solo la 
  proprietà estesa indicata.

  Parametri: 
  const TCHAR *ConnStr;    Stringa di connessione OLE-DB
  C_2STR_LIST &PropList;   Lista delle proprietà e relativi valori
  
  Restituisce il valore corrispondente in caso di successo altrimenti NULL.
-*/  
/****************************************************************/
int gs_PropListFromConnStr(void)
{
   presbuf     arg = acedGetArgs();
   C_2STR_LIST PropList;
   C_RB_LIST   Result;

   acedRetNil(); 

   if (!arg || arg->restype == RTNIL) return RTNORM;
  
   if (arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (gsc_PropListFromConnStr(arg->resval.rstring, PropList) != GS_GOOD) return RTERROR;
   
   Result = PropList.to_rb();
   Result.LspRetList();

   return RTNORM;
}
int gsc_PropListFromConnStr(const TCHAR *ConnStr, C_2STR_LIST &PropList)
{
   int      ReadValue = FALSE, InStr = FALSE;
   size_t   i = 0, ConnStrLen;
   C_STRING ItemName, ItemValue;

   PropList.remove_all();
   if (!ConnStr || (ConnStrLen = wcslen(ConnStr)) == 0) return GS_GOOD;

   while (i <= ConnStrLen)
   {
      if (ConnStr[i] == _T('"'))
         if (!InStr)
            InStr = TRUE; // se non eravamo in una stringa ora lo siamo
         else
            if (!(i < ConnStrLen && ConnStr[i + 1] == _T('"')))
               InStr = FALSE; // se eravamo in una stringa ora non lo siamo più

      if (ReadValue) // se si sta leggendo un valore di proprietà
      {
         // se non siamo in una stringa e fine del valore della proprietà
         // si tratta della proprietà cercata
         if (!InStr && (ConnStr[i] == _T(';') || i == ConnStrLen))
         {
            C_2STR *pProp;
            
            ItemName.alltrim();
            ItemValue.alltrim();
            if ((pProp = new C_2STR(ItemName.get_name(), ItemValue.get_name())) == NULL)
               { PropList.remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            PropList.add_tail(pProp);

            ReadValue = FALSE;
            ItemName.clear();
            ItemValue.clear();
         }
         else ItemValue += ConnStr[i];
      }
      else // si sta leggendo il nome di una proprietà
      {
         // se non siamo in una stringa fine del nome della proprietà,
         // inizio del suo valore
         if (!InStr && ConnStr[i] == _T('='))
            ReadValue = TRUE;
         else
            ItemName += ConnStr[i];
      }

      i++;
   }

   // Se esiste la proprietà "Extended Properties" la esplodo in tante proprietà
   // del tipo [Extended Properties]PropName=PropValue
   C_2STR *pExtendedProp;

   if ((pExtendedProp = (C_2STR *) PropList.search_name(_T("Extended Properties"), GS_BAD)))
   {
      C_STRING    dummy;
      C_2STR_LIST ExtendedPropList;

      if (gsc_PropListFromExtendedPropConnStr(pExtendedProp->get_name2(), ExtendedPropList) != GS_GOOD)
         return RTERROR;
      
      if (ExtendedPropList.get_count() > 0)
      {
         PropList.remove(pExtendedProp);
         pExtendedProp = (C_2STR *) ExtendedPropList.get_head();
         while (pExtendedProp)
         {
            dummy = _T("[Extended Properties]");
            dummy += pExtendedProp->get_name();
            pExtendedProp->set_name(dummy.get_name());

            pExtendedProp = (C_2STR *) pExtendedProp->get_next();
         }
         PropList.paste_tail(ExtendedPropList);
      }
   }

   return GS_GOOD;
}


/****************************************************************/
/*.doc gsc_PropListFromExtendedPropConnStr           <internal> */
/*+
  Questa funzione restituisce una lista di valori delle proprietà UDL dichiarate
  nella proprietà "Extended Properties" specificata.
  Parametri: 
  const TCHAR *ExtendedPropConnStr; Valore della proprietà "Extended Properties"
  C_2STR_LIST &ExtendedPropList;    Lista delle proprietà e relativi valori
  
  Restituisce il valore corrispondente in caso di successo altrimenti NULL.
-*/  
/****************************************************************/
int gsc_PropListFromExtendedPropConnStr(const TCHAR *ExtendedPropConnStr,
                                        C_2STR_LIST &ExtendedPropList)
{
   C_STRING ExtendStr;

   if (!ExtendedPropConnStr) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      
   // Elimino il carattere " a inizio e a fine stringa
   ExtendStr.set_name(ExtendedPropConnStr, 1, (int) wcslen(ExtendedPropConnStr) - 2);
   return gsc_PropListFromConnStr(ExtendStr.get_name(), ExtendedPropList);
}


/****************************************************************/
/*.doc gsc_ExtendedPropListToConnStr                 <internal> */
/*+
  Questa funzione restituisce una stringa di connessione risultante
  dalla lista di valori delle proprietà "Extended Properties" UDL.
  Parametri: 
  C_2STR_LIST &ExtendedPropList;   Lista delle proprietà e relativi valori
  
  Restituisce il valore corrispondente in caso di successo altrimenti NULL.
  N.B. Alloca memoria
-*/  
/****************************************************************/
TCHAR *gsc_ExtendedPropListToConnStr(C_2STR_LIST &ExtendedPropList)
{
   C_STRING ExtendedConnStr, dummy;
   
   // Aggiungo il carattere " a inizio e a fine stringa
   ExtendedConnStr = _T('"');
   dummy.paste(gsc_PropListToConnStr(ExtendedPropList));
   ExtendedConnStr += dummy;
   ExtendedConnStr += _T('"');

   return gsc_tostring(ExtendedConnStr.get_name());
}


/****************************************************************/
/*.doc gsc_PropListToConnStr                         <external> */
/*+
  Questa funzione restituisce una stringa di connessione risultante
  dalla lista di valori delle proprietà UDL.
  Parametri: 
  C_2STR_LIST &PropList;   Lista delle proprietà e relativi valori
                           Le proprietà estese vanno tutte raggruppate 
                           in un unica proprietà.
  
  Restituisce il valore corrispondente in caso di successo altrimenti NULL.
  N.B. Alloca memoria
-*/  
/****************************************************************/
int gs_PropListToConnStr(void)
{
   presbuf     arg = acedGetArgs();
   C_2STR_LIST PropList;
   C_STRING    Buffer;

   acedRetNil(); 

   if (gsc_getUDLProperties(&arg, PropList) == GS_BAD) return RTERROR;
   if (PropList.get_count() == 0)
      Buffer = GS_EMPTYSTR;
   else
      if (Buffer.paste(gsc_PropListToConnStr(PropList)) == NULL) return RTERROR;

   acedRetStr(Buffer.get_name());

   return RTNORM;
}
TCHAR *gsc_PropListToConnStr(C_2STR_LIST &PropList)
{
   C_2STR   *pProp;
   C_STRING ConnStr;
   C_STRING ExtendedProperties;

   pProp = (C_2STR *) PropList.get_head();
   while (pProp)
   {
      // Se si tratta di una proprietà estesa indicata con 
      // [Extended Properties]<nome proprietà>
      if (gsc_strstr(pProp->get_name(), _T("[Extended Properties]")) == pProp->get_name())
      {
         ExtendedProperties += pProp->get_name() + wcslen(_T("[Extended Properties]"));
         ExtendedProperties += _T('=');
         ExtendedProperties += pProp->get_name2();
         ExtendedProperties += _T(';');
      }
      else
      {
         ConnStr += pProp->get_name();
         ConnStr += _T('=');
         ConnStr += pProp->get_name2();
         ConnStr += _T(';');
      }  

      pProp = (C_2STR *) PropList.get_next();
   }

   if (ExtendedProperties.len() > 0)
   {
      ConnStr += _T("Extended Properties=\"");
      ConnStr += ExtendedProperties;
      ConnStr += _T('"');
   }

   return gsc_tostring(ConnStr.get_name());
}


/****************************************************************/
/*.doc gsc_UpdProp                                   <internal> */
/*+
  Questa funzione aggiorna la lista delle proprietà UDL. Se la proprietà
  esiste già la modifica altrimenti la aggiunge in fondo alla lista.
  Parametri: 
  C_2STR_LIST &PropList;   Lista delle proprietà e relativi valori
  const TCHAR *PropName;   Nome proprietà da aggiornare
  const TCHAR *NewValue;   Nuovo valore
  
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/****************************************************************/
int gsc_UpdProp(C_2STR_LIST &PropList, const TCHAR *PropName, const TCHAR *NewValue)
{
   C_2STR *pProp;

   if ((pProp = (C_2STR *) PropList.search_name(PropName)) != NULL)
   {  // modifico valore proprietà
      if (pProp->set_name2(NewValue) == GS_BAD) return GS_BAD;
   }
   else
   {  // aggiungo proprietà
      if ((pProp = new C_2STR(PropName, NewValue)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      PropList.add_tail(pProp);
   }

   return GS_GOOD;
}


/****************************************************************/
/*.doc gsc_CheckPropList                             <internal> */
/*+
  Questa funzione verifica se tutte le proprietà (esclusa quella indicata)
  che sono presenti in entrambe le liste sono uguali.
  Parametri: 
  C_2STR_LIST &PropList;         Lista delle proprietà e relativi valori
  C_2STR_LIST &VerifPropList;    Lista delle proprietà e relativi valori da verificare
  const TCHAR  *PropNameNoCheck; Nome della proprietà da non controllare (default = NULL)
  
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/****************************************************************/
int gsc_CheckPropList(C_2STR_LIST &PropList, C_2STR_LIST &VerifPropList,
                      const TCHAR *PropNameNoCheck)
{
   C_2STR *pProp, *pVerifProp;

   pProp = (C_2STR *) PropList.get_head();
   while (pProp)
   {  // se non si tratta della proprietà da scartare
      if (gsc_strcmp(PropNameNoCheck, pProp->get_name(), FALSE) != 0)
      {
         pVerifProp = (C_2STR *) VerifPropList.search_name(pProp->get_name(), FALSE);
         // se non esiste o nel caso esista questa è diversa ritorno GS_BAD
         if (!pVerifProp || (pVerifProp &&
             gsc_strcmp(pProp->get_name2(), pVerifProp->get_name2(), FALSE) != 0))
            return GS_BAD;
      }
      
      pProp = (C_2STR *) PropList.get_next();
   }

   return GS_GOOD;
}


/****************************************************************/
/*.doc gsc_ModUDLProperties                          <internal> */
/*+
  Questa funzione aggiorna una lista di proprietà UDL con i nuovi valori
  passati dal parametro <NewUDLProperties>. Se una proprietà non esisteva
  nella lista originale verrà aggiunta.
  Parametri: 
  C_2STR_LIST &UDLProperties;       Lista originale delle proprietà UDL
  C_2STR_LIST &NewUDLProperties;    Lista delle nuove proprietà UDL

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/****************************************************************/
int gsc_ModUDLProperties(C_2STR_LIST &UDLProperties, C_2STR_LIST &NewUDLProperties)
{
   C_2STR      *pNewProp = (C_2STR *) NewUDLProperties.get_head(), *pOldExtendedProp;
   C_2STR_LIST OldExtendedPropList;
   bool        ExtendedPropListToUpdate = false;
   TCHAR       *dummy;
   C_STRING    NewExtendedStr;

   // Se le vecchie proprietà "Extended Properties" sono memorizzate tutte insieme
   if ((pOldExtendedProp = (C_2STR *) UDLProperties.search_name(_T("Extended Properties"), FALSE)) != NULL)
   {
      C_STRING ExtendedStr;

      // Elimino il carattere <"> a inizio e fine stringa
      dummy = pOldExtendedProp->get_name2();
      ExtendedStr.set_name(dummy, 1, (int) wcslen(dummy) - 2);
      // Carico la lista delle proprietà in ExtendedPropList
      if (gsc_PropListFromConnStr(ExtendedStr.get_name(), OldExtendedPropList) == GS_BAD)
         return GS_BAD;
   }

   while (pNewProp)
   {
      // Se si tratta di una nuova proprietà estesa indicata con 
      // [Extended Properties]<nome proprietà>
      if (gsc_strstr(pNewProp->get_name(), _T("[Extended Properties]")) == pNewProp->get_name())
      {
         // Se le vecchie proprietà "Extended Properties" sono memorizzate tutte insieme
         if (OldExtendedPropList.get_count() > 0)
         {
            C_2STR_LIST NewExtendedPropList;
            C_2STR *p = new C_2STR(pNewProp->get_name() + wcslen(_T("[Extended Properties]")), pNewProp->get_name2());
            NewExtendedPropList.add_tail(p);

            if (gsc_ModUDLProperties(OldExtendedPropList, NewExtendedPropList) == GS_BAD)
               return GS_BAD;
            ExtendedPropListToUpdate = true;
         }
         else
            // Se si tratta di una vecchia proprietà estesa indicata con 
            // [Extended Properties]<nome proprietà>
            // modifico la lista UDLProperties
            if (gsc_UpdProp(UDLProperties, pNewProp->get_name(), pNewProp->get_name2()) == GS_BAD)
               return GS_BAD;
      }
      else
      // Se le nuove proprietà "Extended Properties" sono memorizzate tutte insieme
      if (gsc_strstr(pNewProp->get_name(), _T("Extended Properties")) == pNewProp->get_name())
      {
         C_2STR_LIST NewExtendedPropList;

         // Elimino il carattere <"> a inizio e fine stringa
         dummy = pNewProp->get_name2();
         NewExtendedStr.set_name(dummy, 1, (int) wcslen(dummy) - 2);
         if (gsc_PropListFromConnStr(NewExtendedStr.get_name(), NewExtendedPropList) == GS_BAD)
            return GS_BAD;

         // Se le vecchie proprietà "Extended Properties" sono memorizzate tutte insieme
         if (OldExtendedPropList.get_count() > 0)
         {
            if (gsc_ModUDLProperties(OldExtendedPropList, NewExtendedPropList) == GS_BAD)
               return GS_BAD;
            ExtendedPropListToUpdate = true;
         }
         else
         // Se si tratta di una vecchia proprietà estesa indicata con 
         // [Extended Properties]<nome proprietà>
         {
            C_2STR *pNewExtendedProp = (C_2STR *) NewExtendedPropList.get_head();

            while (pNewExtendedProp)
            {
               NewExtendedStr = _T("[Extended Properties]");
               NewExtendedStr += pNewExtendedProp->get_name();
            
               // modifico la lista UDLProperties
               if (gsc_UpdProp(UDLProperties, NewExtendedStr.get_name(), pNewExtendedProp->get_name2()) == GS_BAD)
                  return GS_BAD;

               pNewExtendedProp = (C_2STR *) pNewExtendedProp->get_next();
            }
         }
      }
      else
         // modifico la lista UDLProperties
         if (gsc_UpdProp(UDLProperties, pNewProp->get_name(), pNewProp->get_name2()) == GS_BAD)

      if (ExtendedPropListToUpdate)
      {
         dummy = gsc_PropListToConnStr(OldExtendedPropList);
         NewExtendedStr = _T('"');
         NewExtendedStr += dummy;
         NewExtendedStr += _T('"');
         free(dummy);

         pOldExtendedProp->set_name2(NewExtendedStr.get_name());
      }

      pNewProp = (C_2STR *) NewUDLProperties.get_next();
   }

   return GS_GOOD;
}


/****************************************************************/
/*.doc gsc_AdjSyntax                                 <internal> */
/*+
  Questa funzione "aggiusta" il nome dell'oggetto SQL (catalogo, schema, tabella,
  campo di tabella) o una lista di nomi di oggetti SQL.
  In particolare se esistono spazi o " il nome dell'oggetto sarà
  racchiuso in caratteri delimitatori ed aggiungendo ad ogni carattere 
  viroletta " già esistente nella stringa un'altra virgoletta secondo lo 
  standard SQL 92.
  Parametri: 
  C_STRING &Item;             Oggetto SQL
  const TCHAR InitQuotedId;   Delimitatore iniziale
  const TCHAR FinalQuotedId;  Delimitatore finale
  StrCaseTypeEnum StrCase;    se = Upper il riferimento deve essere maiuscolo
                              se = Lower il riferimento deve essere minuscolo
                              se NoSensitive non interessa (default = Upper)
  oppure
  C_STR_LIST &ItemList;       Oggetti SQL
  const TCHAR InitQuotedId;   Delimitatore iniziale
  const TCHAR FinalQuotedId;  Delimitatore finale
  StrCaseTypeEnum StrCase;    se = Upper il riferimento deve essere maiuscolo
                              se = Lower il riferimento deve essere minuscolo
                              se NoSensitive non interessa (default = Upper)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/****************************************************************/
int gsc_AdjSyntax(C_STRING &Item, const TCHAR InitQuotedId, const TCHAR FinalQuotedId,
                  StrCaseTypeEnum StrCase)
{
   C_STRING NewItem;
   int      i = 0;
   TCHAR     ch;

   // commentato perchè la funzione può essere usata per valori di campi composti da soli spazi (es. " ")
   // Item.alltrim();
   if (Item.len() == 0) return GS_GOOD;

   switch (StrCase)
   {
      case Upper: Item.toupper(); break;
      case Lower: Item.tolower(); break;
   }

   // parte commentata perchè se esiste un Item che è uguale ad una
   // parola chiave si avrebbe un errore
   //// se non ci sono caratteri delimitatori iniziali o finali o
   //// caratteri che non siano alfanumerici (escluso "_" perchè è ammesso)
   //if (!Item.at(InitQuotedId) && !Item.at(FinalQuotedId) && 
   //    Item.isAlnum(_T('_')) == GS_GOOD)
   //   // non va fatta alcuna modifica
   //   return GS_GOOD;

   NewItem = InitQuotedId;
   while ((ch = Item.get_chr(i++)) != _T('\0'))
      // se il carattere delimitatore finale = al carattere letto allora lo raddoppio
      // (es. aaa"aaa  diventa  aaa""aaa  se delimitatore finale = ") 
      if (ch == FinalQuotedId)
      {  // un carattere " allora lo raddoppio
         NewItem += ch;
         NewItem += ch;
      }
      else NewItem += ch;

   NewItem += FinalQuotedId;
   Item = NewItem;

   return GS_GOOD;
}
int gsc_AdjSyntax(C_STR_LIST &ItemList, C_DBCONNECTION *pConn)
{
   C_STR    *pItem = (C_STR *) ItemList.get_head();
   C_STRING dummy;

   while (pItem)
   {
      dummy = pItem->get_name();
      if (gsc_AdjSyntax(dummy, pConn->get_InitQuotedIdentifier(),
                        pConn->get_FinalQuotedIdentifier(),
                        pConn->get_StrCaseFullTableRef()) == GS_BAD)
         return GS_BAD;
      pItem->set_name(dummy.get_name());

      pItem = (C_STR *) ItemList.get_next();
   }

   return GS_GOOD;
}


/****************************************************************/
/*.doc gsc_getFullRefTabComponent                    <internal> */
/*+
  Questa funzione legge un componente SQL del riferimento completo a tabella.
  Parametri: 
  TCHAR **FullRefTab;            Puntatore al primo carattere del componente SQL
                                 (il puntatore viene spostato al carattere successivo
                                 la fine del componente SQL)
  const TCHAR InitQuotedId;      Delimitatore iniziale
  const TCHAR FinalQuotedId;     Delimitatore finale
  const TCHAR Separator;         Separatore (di catalogo o di schema) (default = '\0')
  StrCaseTypeEnum StrCase;       se = Upper il riferimento deve essere maiuscolo
                                 se = Lower il riferimento deve essere minuscolo
                                 se Nocase non interessa (default = Upper)
  ResourceTypeEnum ResourceType; Tipo di risorsa rappresentata dal componente SQL
                                 (default = EmptyRes)

  Restituisce il componente SQL in caso di successo altrimenti NULL.
-*/  
/****************************************************************/
TCHAR *gsc_getFullRefTabComponent(TCHAR **FullRefTab, const TCHAR InitQuotedId, 
                                  const TCHAR FinalQuotedId, const TCHAR Separator,
                                  StrCaseTypeEnum StrCase, ResourceTypeEnum ResourceType)
{
   C_STRING Item;
   int      i = 0;
   TCHAR    *p, *pStr;

   if (!FullRefTab) return NULL;

   pStr = *FullRefTab;
   while (pStr && *pStr == _T(' ')) pStr++; // salto spazi iniziali
   if (!pStr || *pStr == _T('\0')) return NULL;

   if (*pStr != InitQuotedId) // non sono usati i delimitatori
   {
      if (Separator == _T('\\') && ResourceType == DirectoryRes)
      {  // il riferimento completo è una path di un file, il catalogo è il direttorio
         TCHAR *pFinal = pStr;
         
         p = pStr;
         while (*p != _T('\0'))
            { if (*p == _T('\\')) pFinal = p; p++; } // cerco ultimo '\\'

         if (pFinal == pStr) pFinal = p; // non esistono caratteri '\\'
         Item.set_name(pStr, 0, (int) (pFinal - pStr - 1));
         pStr = pFinal;
      }
      else
         while (*pStr != _T('\0') && *pStr != Separator)
            { Item += *pStr; pStr++; }
   }
   else // deve terminare con <FinalQuotedId>
   {
      pStr++; // salto delimitatore iniziale
      while (*pStr != _T('\0'))
      {
         if (*pStr == FinalQuotedId) // se = a carattere delimitatore finale
         { 
            // se il carattere delimitatore finale = "
            if (FinalQuotedId == _T('"'))
            {
               p = pStr + 1;
               // se il carattere successivo NON è un altro '"' allora è terminato
               if (*p != _T('"')) break;
               Item += *pStr;
            }
            else break;
         }
         Item += *pStr;
         pStr++;
      }
      pStr++; // salto delimitatore finale
   }

   *FullRefTab = pStr;
   
   if (Item.len() == 0) return NULL;

   switch (StrCase)
   {
      case Upper: Item.toupper(); break;
      case Lower: Item.tolower(); break;
   }

   return gsc_tostring(Item.get_name());     
}


/****************************************************************/
/*.doc gsc_getRudeTabComponent                       <internal> */
/*+
  Questa funzione legge un componente SQL senza alcuna pulizia dello stesso
  (lasciando i delimitatori, senza correzione maiuscolo/minuscolo).
  Parametri: 
  TCHAR **FullRefTab;            Puntatore al primo carattere del componente SQL
                                 (il puntatore viene spostato al carattere successivo
                                 la fine del componente SQL)
  const TCHAR Separator;         Separatore (di catalogo o di schema) (default = '\0')

  Restituisce il componente SQL in caso di successo altrimenti NULL.
-*/  
/****************************************************************/
TCHAR *gsc_getRudeTabComponent(TCHAR **FullRefTab, const TCHAR Separator)
{
   C_STRING Item;
   TCHAR    *pStr, *pFinal;

   if (!FullRefTab) return NULL;

   pStr = *FullRefTab;

   if ((pFinal = wcschr(pStr, Separator)))
   {
      Item.set_name(pStr, 0, (int) (pFinal - pStr));
      *FullRefTab = pFinal;
   }
   else 
   {
      Item = pStr;
      while (*pStr != _T('\0')) pStr++; // vado in fondo alla stringa
      *FullRefTab = pStr;
   }

   return Item.cut();
}

/****************************************************************/
/*.doc gsc_getUDLPropertiesDescrFromFile             <external> */
/*+
  Questa funzione legge la descrizione delle proprietà necessarie per
  personalizzare una stringa di connessione OLE-DB da un file testo.
  Restituisce una lista di resbuf così composta:
  ((<Name><Descr.><Resource Type>[CATALOG || SCHEMA]) ...)
  Parametri: 
  const TCHAR *File;           File UDL (o solo nome del file)
  
  Restituisce una lista di resbuf in caso di successo altrimenti NULL.
-*/  
/****************************************************************/
int gs_getUDLPropertiesDescrFromFile(void)
{
   presbuf   arg = acedGetArgs();
   C_RB_LIST ret;

   acedRetNil(); 

   if (!arg || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if ((ret << gsc_getUDLPropertiesDescrFromFile(arg->resval.rstring)) != NULL)
      ret.LspRetList();

   return RTNORM;
}
presbuf gsc_getUDLPropertiesDescrFromFile(const TCHAR *File, bool UdlFile)
{
   C_PROFILE_SECTION_BTREE ProfileSections;
   C_BPROFILE_SECTION      *ProfileSection;
   C_2STR_BTREE            *pProfileEntries;
   C_B2STR                 *pProfileEntry, *pPrevProfileEntry;
   C_RB_LIST               Result;
   C_STRING                ItemName, PropName, full_path;

   if (UdlFile) // se il file si riferisce ad un file UDL
   {  // Cerco il corrispondente file Required
      C_STRING drive, dir, name, ext, extComp;

      full_path = File;
      if (gsc_getUDLFullPath(full_path) == GS_BAD) return NULL;
      gsc_splitpath(full_path, &drive, &dir, &name, &ext);
      extComp = _T('.');
      extComp += UDL_FILE_REQUIRED_EXT;

      if (ext.comp(extComp, FALSE) != 0)
      {  // verifico l'esistenza di uno stesso file con estensione .REQUIRED
         full_path = drive;
         full_path += dir;
         full_path += name;
         full_path += _T('.');
         full_path += UDL_FILE_REQUIRED_EXT;
      }
   }
   else
      full_path = File;

   if (gsc_read_profile(full_path, ProfileSections) == GS_BAD) return GS_BAD;

   if ((Result << acutBuildList(RTLB, 0)) == NULL) return NULL;

   // Leggo le entry nella sezione "Extended Properties"
   if ((ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(_T("Extended Properties"))))
   {
      pProfileEntries = (C_2STR_BTREE *) ProfileSection->get_ptr_EntryList();
      pProfileEntry = (C_B2STR *) pProfileEntries->go_top();
      while (pProfileEntry)
      {
         if (gsc_strcmp(pProfileEntry->get_name2(), GS_EMPTYSTR) == 0) // cerco entry senza valore
         {
            pPrevProfileEntry = (C_B2STR *) pProfileEntries->get_cursor();

            PropName  = _T("[Extended Properties]");
            PropName += pProfileEntry->get_name();
            // nome parametro UDL
            if ((Result += acutBuildList(RTLB, RTSTR, PropName.get_name(), 0)) == NULL)
               return NULL;

            PropName = pProfileEntry->get_name();
            ItemName = PropName;
            ItemName += _T("_GEOsimDescription");
            if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(ItemName.get_name())))
            {
               if ((Result += acutBuildList(RTSTR, pProfileEntry->get_name2(), 0)) == NULL)
                  return NULL;
            }
            else
               if ((Result += acutBuildList(RTNIL, 0)) == NULL) return NULL;

            ItemName = PropName;
            ItemName += _T("_GEOsimResourceType");
            if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(ItemName.get_name())))
            {
               if ((Result += acutBuildList(RTSTR, pProfileEntry->get_name2(), 0)) == NULL)
                  return NULL;
            }
            else
               if ((Result += acutBuildList(RTNIL, 0)) == NULL) return NULL;

            ItemName  = PropName;
            ItemName += _T("_GEOsimSQLPart");
            if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(ItemName.get_name())))
            {
               if ((Result += acutBuildList(RTSTR, pProfileEntry->get_name2(), RTLE, 0)) == NULL)
                  return NULL;
            }
            else
               if ((Result += acutBuildList(RTLE, 0)) == NULL) return NULL;

            pProfileEntries->set_cursor(pPrevProfileEntry);
         }

         pProfileEntry = (C_B2STR *) pProfileEntries->go_next();
      }
   }

   // Leggo le entry nella sezione "General"
   if ((ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(_T("General"))))
   {
      pProfileEntries = (C_2STR_BTREE *) ProfileSection->get_ptr_EntryList();
      pProfileEntry = (C_B2STR *) pProfileEntries->go_top();
      while (pProfileEntry)
      {
         if (gsc_strcmp(pProfileEntry->get_name2(), GS_EMPTYSTR) == 0) // cerco entry senza valore
         {
            pPrevProfileEntry = (C_B2STR *) pProfileEntries->get_cursor();

            // nome parametro UDL
            if ((Result += acutBuildList(RTLB, RTSTR, pProfileEntry->get_name(), 0)) == NULL)
               return NULL;

            PropName = pProfileEntry->get_name();
            ItemName = PropName;
            ItemName += _T("_GEOsimDescription");
            if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(ItemName.get_name())))
            {
               if ((Result += acutBuildList(RTSTR, pProfileEntry->get_name2(), 0)) == NULL)
                  return NULL;
            }
            else
               if ((Result += acutBuildList(RTNIL, 0)) == NULL) return NULL;

            ItemName = PropName;
            ItemName += _T("_GEOsimResourceType");
            if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(ItemName.get_name())))
            {
               if ((Result += acutBuildList(RTSTR, pProfileEntry->get_name2(), 0)) == NULL)
                  return NULL;
            }
            else
               if ((Result += acutBuildList(RTNIL, 0)) == NULL) return NULL;

            ItemName  = PropName;
            ItemName += _T("_GEOsimSQLPart");
            if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(ItemName.get_name())))
            {
               if ((Result += acutBuildList(RTSTR, pProfileEntry->get_name2(), RTLE, 0)) == NULL)
                  return NULL;
            }
            else
               if ((Result += acutBuildList(RTLE, 0)) == NULL) return NULL;

            pProfileEntries->set_cursor(pPrevProfileEntry);
         }

         pProfileEntry = (C_B2STR *) pProfileEntries->go_next();
      }
   }
   
   if (Result.GetCount() == 1) return NULL;
   if ((Result += acutBuildList(RTLE, 0)) == NULL) return NULL;

   Result.ReleaseAllAtDistruction(GS_BAD);
   
   return Result.get_head();
}


/****************************************************************/
/*.doc gs_getCatSchInfoFromFile                      <external> */
/*+
  Questa funzione legge alcune informazioni riguardanti il catalogo e
  lo schema da un file testo.
  Restituisce una lista di resbuf composta da 2 sottoliste, la prima
  relativa al catalogo, la seconda allo schema.
  ((<Cat Description><Cat ResourceType><Cat Separator><Cat UDLProperty>)
   (<Sch Description><Sch ResourceType><Sch Separator><Sch UDLProperty>))
  
  Restituisce una lista di resbuf in caso di successo altrimenti NULL.
-*/  
/****************************************************************/
int gs_getCatSchInfoFromFile(void)
{
   presbuf   arg = acedGetArgs();
   C_STRING  full_path, drive, dir, name, ext;
   C_PROFILE_SECTION_BTREE ProfileSections;
   TCHAR     Separator;
   C_STRING  UDLProperty, ResourceType, Description;
   C_RB_LIST Res;

   acedRetNil(); 

   if (!arg || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   full_path = arg->resval.rstring;
   if (gsc_getUDLFullPath(full_path) == GS_BAD) return RTERROR;
   gsc_splitpath(full_path, &drive, &dir, &name, &ext);

   if (ext.comp(_T(".UDL"), FALSE) == 0) // se si tratta di un file UDL
   {  // verifico l'esistenza di uno stesso file con estensione .REQUIRED
      full_path = drive;
      full_path += dir;
      full_path += name;
      full_path += _T('.');
      full_path += UDL_FILE_REQUIRED_EXT;
   }

   if (gsc_path_exist(full_path) == GS_BAD) return RTNORM;
   if (gsc_read_profile(full_path, ProfileSections) == GS_BAD)
      return RTERROR;

   // Leggo le caratteristiche del catalogo (importante: "Catalog" è case sensitive)
   if (gsc_getTablePartInfo(ProfileSections, _T("Catalog"), &Separator, UDLProperty, 
                            ResourceType, Description) == GS_BAD)
      return RTERROR;

   if (Description.len() == 0 && ResourceType.len() == 0 && Separator == _T('\0') &&
       UDLProperty.len() == 0) 
      return RTNORM;

   if (Description.len() > 0)
   {
      if ((Res += acutBuildList(RTLB, RTSTR, Description.get_name(), 0)) == NULL) return RTERROR;
   }
   else
      if ((Res += acutBuildList(RTLB, RTNIL, 0)) == NULL) return RTERROR;

   if (ResourceType.len() > 0)
   {
      if ((Res += acutBuildList(RTSTR, ResourceType.get_name(), 0)) == NULL) return RTERROR;
   }
   else
      if ((Res += acutBuildList(RTNIL, 0)) == NULL) return RTERROR;

   // Riutilizzo la variabile <name>
   name = Separator;
   if (name.len() > 0)
   {
      if ((Res += acutBuildList(RTSTR, name.get_name(), 0)) == NULL) return RTERROR;
   }
   else
      if ((Res += acutBuildList(RTNIL, 0)) == NULL) return RTERROR;

   if (UDLProperty.len() > 0)
   {
      if ((Res += acutBuildList(RTSTR, UDLProperty.get_name(), RTLE, 0)) == NULL) return RTERROR;
   }
   else
      if ((Res += acutBuildList(RTNIL, RTLE, 0)) == NULL) return RTERROR;

   // Leggo le caratteristiche dello schema (importante: "Schema" è case sensitive)
   if (gsc_getTablePartInfo(ProfileSections, _T("Schema"), &Separator, UDLProperty, 
                            ResourceType, Description) == GS_BAD) return RTERROR;

   if (Description.len() > 0 || ResourceType.len() > 0 || Separator != _T('\0') ||
       UDLProperty.len() > 0)
   {
      if (Description.len() > 0)
      {
         if ((Res += acutBuildList(RTLB, RTSTR, Description.get_name(), 0)) == NULL) return RTERROR;
      }
      else
         if ((Res += acutBuildList(RTLB, RTNIL, 0)) == NULL) return RTERROR;

      if (ResourceType.len() > 0)
      {
         if ((Res += acutBuildList(RTSTR, ResourceType.get_name(), 0)) == NULL) return RTERROR;
      }
      else
         if ((Res += acutBuildList(RTNIL, 0)) == NULL) return RTERROR;

      // Riutilizzo la variabile <name>
      name = Separator;
      if (name.len() > 0)
      {
         if ((Res += acutBuildList(RTSTR, name.get_name(), 0)) == NULL) return RTERROR;
      }
      else
         if ((Res += acutBuildList(RTNIL, 0)) == NULL) return RTERROR;

      if (UDLProperty.len() > 0)
      {
         if ((Res += acutBuildList(RTSTR, UDLProperty.get_name(), RTLE, 0)) == NULL) return RTERROR;
      }
      else
         if ((Res += acutBuildList(RTNIL, RTLE, 0)) == NULL) return RTERROR;
   }

   Res.LspRetList();

   return RTNORM;
}


/****************************************************************/
/*.doc gsc_getUDLProperties                          <external> */
/*+
  Questa funzione legge i valori delle proprietà UDL da LISP.
  La lista di resbuf è così composta:
  ((<Name><Value>) ...)
  se <Name> ha un prefisso incluso tra parentesi quadre 
  (es. [Extended Properties]DBQ) si intende che la proprietà UDL che segue 
  (es. DBQ) si riferisce alla proprietà indicata tra le parentesi quadre.
  Parametri:
  presbuf *arg;         Lista dei valori letti da LISP.
  C_2STR_LIST &list;    Lista delle proprietà UDL.

  Restituisce una lista di resbuf in caso di successo altrimenti NULL.
  Nota: il puntatore <arg> viene avanzato nella lista.
-*/  
/****************************************************************/
int gsc_getUDLProperties(presbuf *arg, C_2STR_LIST &PropList)
{
   presbuf  p_rb, pPropName, pPropVal;
   C_2STR   *pProp;
   C_STRING Value, Name;
   int      i = 0; 

   PropList.remove_all();
   while ((p_rb = gsc_nth(i++, *arg)) != NULL)
   {
      if (!(pPropName = gsc_nth(0, p_rb)) || pPropName->restype != RTSTR ||
          !(pPropVal = gsc_nth(1, p_rb)) || pPropVal->restype != RTSTR)
         continue;

      if ((pProp = new C_2STR(pPropName->resval.rstring,
                              pPropVal->resval.rstring)) == NULL)
            { PropList.remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      PropList.add_tail(pProp);
   }

   if ((*arg = gsc_scorri(*arg)) != NULL) *arg = (*arg)->rbnext; // elemento successivo

   return GS_GOOD;
}
//int gsc_getUDLProperties(presbuf *arg, C_2STR_LIST &PropList)
//{
//   presbuf  p_rb, pPropName, pPropVal;
//   TCHAR    *p_ch;
//   C_2STR   *pProp;
//   C_STRING Value, Name;
//   int      i = 0; 
//
//   PropList.remove_all();
//   while ((p_rb = gsc_nth(i++, *arg)) != NULL)
//   {
//      if (!(pPropName = gsc_nth(0, p_rb)) || pPropName->restype != RTSTR ||
//          !(pPropVal = gsc_nth(1, p_rb)) || pPropVal->restype != RTSTR)
//         continue;
//
//      if (pPropName->resval.rstring[0] == _T('['))
//      {
//         if (!(p_ch = wcschr(pPropName->resval.rstring, _T(']')))) continue;
//         Name.set_name(pPropName->resval.rstring, 1, p_ch - pPropName->resval.rstring - 1);
//
//         if (!(pProp = (C_2STR *) PropList.search_name(Name.get_name(), FALSE)))
//         {  // se non c'era aggiungo
//            Value = _T('"');
//            Value += p_ch + 1;
//            Value += _T('=');
//            Value += pPropVal->resval.rstring;
//            Value += _T('"');
//            if ((pProp = new C_2STR(Name.get_name(), Value.get_name())) == NULL)
//               { PropList.remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
//            PropList.add_tail(pProp);
//         }
//         else // se c'era modifico
//         {
//            Value = pProp->get_name2();
//            Value.set_chr(*(p_ch + 1), Value.len());
//            Value += p_ch + 2;
//            Value += _T('"');
//         }
//      }
//      else
//      {  // per sicurezza
//         if (!(pProp = (C_2STR *) PropList.search_name(pPropName->resval.rstring, FALSE)))
//         {   // se non c'era aggiungo
//            if ((pProp = new C_2STR) == NULL)
//               { PropList.remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
//            if (pProp->set_name(pPropName->resval.rstring) == GS_BAD)
//               { PropList.remove_all(); return GS_BAD; }
//         }
//         if (pProp->set_name2(pPropVal->resval.rstring) == GS_BAD)
//            { PropList.remove_all(); return GS_BAD; }
//         PropList.add_tail(pProp);
//      }
//   }
//
//   if ((*arg = gsc_scorri(*arg)) != NULL) *arg = (*arg)->rbnext; // elemento successivo
//
//   return GS_GOOD;
//}


/****************************************************************/
/*.doc gsc_getTablePartInfo                          <internal> */
/*+
  Questa funzione legge, da un file testo, alcune caratteristiche 
  riguardanti l'uso di una parte del riferimento a tabelle (catalogo
  oppure schema) nella connessione OLE-DB.
  In particolare viene letto:
  - Carattere separatore di catalogo o schema
  - Tipo di risorsa rappresentata dal catalogo o dallo schema
  - Proprietà UDL che rappresenta il catalogo o lo schema
  - Descrizione per interfaccia utente
  
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  const    TCHAR *TablePart;     Parte SQL della tabella ("CATALOG" o "SCHEMA")
  TCHAR    *Separator;           Output
  C_STRING &UDLProperty;         Output
  C_STRING &ResourceType;        Output
  C_STRING &Description;         Output

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int gsc_getTablePartInfo(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *TablePart, TCHAR *Separator,
                         C_STRING &UDLProperty, C_STRING &ResourceType, C_STRING &Description)
{
   C_BPROFILE_SECTION *ProfileSection;
   C_2STR_BTREE       *pProfileEntries;
   C_B2STR            *pProfileEntry = NULL;
   TCHAR              buf[255], *str;
   C_2STR_LIST        EntryList;
   C_2STR             *pEntry = NULL;
   C_STRING           Item, Prefix;
   int                prev_GS_ERR_COD = GS_ERR_COD;

   *Separator = _T('\0');
   UDLProperty.clear();
   ResourceType.clear();
   Description.clear();

   str = buf;
   Item = TablePart; // "CATALOG" oppure "SCHEMA"
   Item += _T(" Separator");
   // Catalog Separator
   if ((pProfileEntry = ProfileSections.get_entry(_T("GEOsim"), Item.get_name())))
      *Separator = *gsc_alltrim(pProfileEntry->get_name2());

   // Leggo le entry nella sezione "Extended Properties"
   if ((ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(_T("Extended Properties"))))
   {
      pProfileEntries = (C_2STR_BTREE *) ProfileSection->get_ptr_EntryList();
      pProfileEntry = (C_B2STR *) pProfileEntries->go_top();
      while (pProfileEntry)
      {
         if (gsc_strcmp(pProfileEntry->get_name2(), TablePart, FALSE) == 0) // cerco entry
         {
            if ((str = gsc_strstr(pProfileEntry->get_name(), _T("_GEOsimSQLPart"), FALSE)) != NULL &&
                *(str + wcslen(_T("_GEOsimSQLPart"))) == _T('\0'))
            {
               Prefix.set_name(pProfileEntry->get_name(), 0, (int) (str - pProfileEntry->get_name() - 1));
               UDLProperty = _T("[Extended Properties]");
               UDLProperty += Prefix;

               Item = Prefix;
               Item += _T("_GEOsimResourceType");
               if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(Item.get_name())))
                  ResourceType = pProfileEntry->get_name2();
               else 
                  ResourceType.clear();

               Item = Prefix;
               Item += _T("_GEOsimDescription");
               if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(Item.get_name())))
                  Description = pProfileEntry->get_name2();
               else 
                  Description.clear();

               break;
            }
         }

         pProfileEntry = (C_B2STR *) pProfileEntries->go_next();
      }
   }

   // se non trovato nella sezione "Extended Properties"
   if (!pProfileEntry)
   {
      // Leggo le entry nella sezione "General"
      if ((ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(_T("General"))))
      {
         pProfileEntries = (C_2STR_BTREE *) ProfileSection->get_ptr_EntryList();
         pProfileEntry = (C_B2STR *) pProfileEntries->go_top();
         while (pProfileEntry)
         {
            if (gsc_strcmp(pProfileEntry->get_name2(), TablePart, FALSE) == 0) // cerco entry
            {
               if ((str = gsc_strstr(pProfileEntry->get_name(), _T("_GEOsimSQLPart"), FALSE)) != NULL &&
                   *(str + wcslen(_T("_GEOsimSQLPart"))) == _T('\0'))
               {
                  Prefix.set_name(pProfileEntry->get_name(), 0, (int) (str - pProfileEntry->get_name() - 1));
                  UDLProperty = Prefix;
               
                  Item = Prefix;
                  Item += _T("_GEOsimResourceType");
                  if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(Item.get_name())))
                     ResourceType = pProfileEntry->get_name2();
                  else 
                     ResourceType.clear();

                  Item = Prefix;
                  Item += _T("_GEOsimDescription");
                  if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(Item.get_name())))
                     Description = pProfileEntry->get_name2();
                  else 
                     Description.clear();

                  break;
               }
            }

            pProfileEntry = (C_B2STR *) pProfileEntries->go_next();
         }
      }      
   }

   GS_ERR_COD = prev_GS_ERR_COD;

   return GS_GOOD;
}


/****************************************************************/
/*.doc gs_getDBConnection                            <external> */
/*+
  Questa funzione LISP ritorna un puntatore all'oggetto C_DBCONNECTION
  di GEOsim.
  Riceve una lista di resbuf così composta:
  (("UDL_FILE" <file UDL> || <stringa di connessione>) ("UDL_PROP" <value>))

  Restituisce una lista di resbuf in caso di successo altrimenti NULL.
-*/  
/****************************************************************/
int gs_getDBConnection(void)
{
   presbuf        arg = acedGetArgs();
   C_DBCONNECTION *pDBConn;

   acedRetNil(); 

   if ((pDBConn = gsc_getConnectionFromLisp(arg)) == NULL) return RTERROR;

   #if !defined(_WIN64) && !defined(__LP64__) && !defined(_AC64)
      acedRetReal((ads_real) (__w64 long) pDBConn);
   #else
      acedRetReal((ads_real) (__int64) pDBConn);
   #endif

   return RTNORM;
}


/****************************************************************/
/*.doc gsc_getUDLFullPath                            <internal> */
/*+
  Questa funzione ritorna una path completa di un file .UDL con il
  seguente criterio.
  1) Se viene specificato la path completa diversa
     dal sottodirettorio di GEOsimAppl::GEODIR indicato dalla macro GEODATALINKDIR
     (es. X:\GS4\DATA LINKS\DBASE 3.UDL) questa rimarrà inalterata
     (ad eccezione dell'unità di rete che eventualmente verrà convertita).
     Se il file viene indicato senza estensione verrà aggiunto .UDL.
  2) Se viene specificato solo il nome di un file (con o senza estensione)
     questo verrà cercato nel sottodirettorio di GEOsimAppl::GEODIR indicato dalla 
     macro GEODATALINKDIR (es. X:\GS4\DATA LINKS\DBASE 3.UDL).
     Se i file .UDL non hanno il corrispondente file *.REQUIRED (senza possibilità 
     di personalizzazione) oppure sono presenti anche nel sottodirettorio di 
     GEOsimAppl::WORKDIR indicato dalla macro GEODATALINKDIR 
     (es. C:\GS4\DATA LINKS\DBASE 3.UDL) con il file REQUIRED
     identico a quello specificato nel sottodirettorio di GEOsimAppl::GEODIR
     indicato dalla macro GEODATALINKDIR allora si potranno usare i file del 
     sottodirettorio di GEOsimAppl::WORKDIR (quindi in locale).
     Se il file viene indicato senza estensione verrà aggiunto .UDL.

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
  N.B. Il parametro UDLFile può essere anche una stringa di connessione
-*/  
/****************************************************************/
int gsc_getUDLFullPath(C_STRING &UDLFile)
{
   C_STRING Buffer, GeoPrefix, WrkPrefix;
   C_STRING Drive, Dir, Name, Ext;

	if (UDLFile.len() == 0)
      { GS_ERR_COD = eGSUDLFileNotFound; return GS_BAD; }
	Buffer = UDLFile;

   GeoPrefix = GEOsimAppl::GEODIR;
   GeoPrefix += _T('\\');
   GeoPrefix += GEODATALINKDIR;
   GeoPrefix += _T('\\');

   WrkPrefix = GEOsimAppl::WORKDIR;
   WrkPrefix += _T('\\');
   WrkPrefix += GEODATALINKDIR;
   WrkPrefix += _T('\\');

   gsc_splitpath(Buffer, &Drive, &Dir, &Name, &Ext);
   if (Name.len() == 0) { GS_ERR_COD = eGSUDLFileNotFound; return GS_BAD; }
   if (Ext.len() == 0) Ext = _T(".UDL");

   if (UDLFile.at(_T('\\')) != NULL || UDLFile.at(_T('/')) != NULL) // Se è stata specificata la path
   {
      C_STRING Prefix(Drive);
      Prefix += Dir;

      if (Prefix.comp(GeoPrefix, FALSE) != 0) // se questa path non è di GEOsim
      {
         UDLFile = Drive;
         UDLFile += Dir;
         UDLFile += Name;
         UDLFile += Ext;

         if (gsc_path_exist(UDLFile, GS_BAD) == GS_GOOD) return GS_GOOD;
         
         // se il nome del file conteneva un .
         // aggiungo l'estensione ".UDL" per fare un'ulteriore prova
         UDLFile += _T(".UDL");

         if (gsc_path_exist(UDLFile, GS_BAD) != GS_GOOD)
         {
            GS_ERR_COD = eGSUDLFileNotFound;
            return GS_BAD;
         }
         else return GS_GOOD;
      }
   }

   UDLFile = GeoPrefix;
   UDLFile += Name;
   UDLFile += Ext;

   if (gsc_path_exist(UDLFile, GS_BAD) == GS_BAD)
   {  // se il nome del file conteneva un .
      // aggiungo l'estensione ".UDL" per fare un'ulteriore prova
      UDLFile += _T(".UDL");
      if (gsc_path_exist(UDLFile, GS_BAD) == GS_BAD)
         { GS_ERR_COD = eGSUDLFileNotFound; return GS_BAD; }
   }

   Buffer = GeoPrefix;
   Buffer += Name;
   Buffer += _T('.');
   Buffer += UDL_FILE_REQUIRED_EXT;
   
   if (gsc_path_exist(Buffer) == GS_BAD)
   {  // manca il file .REQUIRED in GEODATALINKDIR sotto GEOsimAppl::GEODIR
      Buffer = WrkPrefix;
      Buffer += Name;
      Buffer += Ext;
      if (gsc_path_exist(Buffer) == GS_GOOD)
      {  // esiste il file .UDL in GEODATALINKDIR sotto GEOsimAppl::WORKDIR
         Buffer = WrkPrefix;
         Buffer += Name;
         Buffer += _T('.');
         Buffer += UDL_FILE_REQUIRED_EXT;
         if (gsc_path_exist(Buffer) == GS_BAD)
         {  // manca il file .REQUIRED in GEODATALINKDIR sotto GEOsimAppl::WORKDIR
            UDLFile = WrkPrefix;
            UDLFile += Name;
            UDLFile += Ext;
         }
      }
   }
   else if (GeoPrefix.comp(WrkPrefix) != 0) // direttorio server e locale devono essere diversi
   {  // esiste il file .REQUIRED in GEODATALINKDIR sotto GEOsimAppl::GEODIR
      C_STRING GEODIRRequired(Buffer);

      Buffer = WrkPrefix;
      Buffer += Name;
      Buffer += Ext;
      if (gsc_path_exist(Buffer) == GS_GOOD)
      {  // esiste il file .UDL in GEODATALINKDIR sotto GEOsimAppl::WORKDIR
         Buffer = WrkPrefix;
         Buffer += Name;
         Buffer += _T('.');
         Buffer += UDL_FILE_REQUIRED_EXT;

         if (gsc_path_exist(Buffer) == GS_GOOD && 
             gsc_identicalFiles(Buffer, GEODIRRequired) == GS_GOOD)
         {  // esiste il file .REQUIRED in GEODATALINKDIR sotto GEOsimAppl::WORKDIR
            // e i due file .REQUIRED sono uguali
            UDLFile = WrkPrefix;
            UDLFile += Name;
            UDLFile += Ext;
         }
      }
   }

   return gsc_nethost2drive(UDLFile);
}


/****************************************************************/
/*.doc gs_getUDLList                                 <external> */
/*+
  Questa funzione LISP ritorna una lista dei nomi dei file *.UDL nel 
  sottodirettorio di GEOsimAppl::GEODIR indicato dalla macro GEODATALINKDIR.
  Per i file .UDL che non hanno il corrispondente file *.REQUIRED
  (senza possibilità di personalizzazione) e che sono presenti anche
  nel sottodirettorio di GEOsimAppl::WORKDIR indicato dalla macro GEODATALINKDIR,
  questi ultimi avranno la precedenza sui primi.

  Restituisce una lista di resbuf (ordinata per nome file)
  in caso di successo altrimenti NULL.
-*/  
/****************************************************************/
int gs_getUDLList(void)
{
   C_STR_LIST ResList;
   C_RB_LIST  rbList;

   acedRetNil(); 

   if (gsc_getUDLList(ResList) == GS_BAD) return RTERROR;
   if (ResList.get_head() != NULL) rbList << ResList.to_rb();
   rbList.LspRetList();

   return RTNORM;
}
int gsc_getUDLList(C_STR_LIST &ResList)
{
   C_STR_LIST FileNameList;
   C_STR      *pFileName, *pRes;
   C_STRING   Mask, Name;
   presbuf    rbList;

   Mask = GEOsimAppl::GEODIR;
   Mask += _T('\\');
   Mask += GEODATALINKDIR;
   Mask += _T("\\*.UDL");

   // cerco i file *.UDL in GEODATALINKDIR sotto GEOsimAppl::GEODIR
   if (gsc_adir(Mask.get_name(), &rbList) == 0) return RTNORM;
   if (FileNameList.from_rb(rbList) == GS_BAD) 
      { acutRelRb(rbList); return GS_BAD; }
   acutRelRb(rbList);

   pFileName = (C_STR *) FileNameList.get_head();
   while (pFileName)
   {
      gsc_splitpath(pFileName->get_name(), NULL, NULL, &Name);

      if ((pRes = new C_STR(Name.get_name())) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      ResList.add_tail(pRes);

      pFileName = (C_STR *) FileNameList.get_next();
   }

   ResList.sort_name(TRUE, TRUE); // sensitive e ascending

   return GS_GOOD;
}


/****************************************************************/
/*.doc gs_IsCompatibleType                           <external> */
/*+
  Questa funzione riceve una connessione OLE-DB con le 
  caratteristiche ADO di un tipo di dato e una seconda connessione OLE-DB con le 
  caratteristiche ADO di un tipo di dato.
  La funzione restituisce GS_GOOD se il secondo tipo è compatibile
  ad ospitare dati del primo tipo.
  Parametri:
  (<OLE_DB conn src><src_type><src_len><src_dec>
   <OLE_DB conn dst><dst_type><dst_len><dst_dec>)
  dove:
  <OLE_DB conn src> = <OLE_DB conn>
  <OLE_DB conn dst> = <OLE_DB conn>
  <OLE_DB conn> = (("UDL_FILE" <Connection>) ("UDL_PROP" <Properties>))
  <Connection>  = <file UDL> | <stringa di connessione>
  <Properties>  = stringa delle proprietà | ((<prop1><value>)(<prop1><value>)...)
-*/  
/****************************************************************/
int gs_IsCompatibleType(void)
{
   presbuf        arg = acedGetArgs(), p;
   TCHAR          *pSrcType, *pDstType;
   long           LenSrc, LenDst;
   int            DecSrc, DecDst, res;
   C_DBCONNECTION *pSrcConn, *pDstConn;

   acedRetNil();
 
   // Legge nella lista dei parametri i riferimenti alla prima connessione OLE-DSB
   if (!(p = gsc_nth(0, arg))) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if ((pSrcConn = gsc_getConnectionFromLisp(p)) == NULL) return RTERROR;

   if (!(p = gsc_nth(1, arg))) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (!p || p->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   pSrcType = p->resval.rstring;
   if (!(p = p->rbnext) || gsc_rb2Lng(p, &LenSrc) == GS_BAD) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (!(p = p->rbnext) || gsc_rb2Int(p, &DecSrc) == GS_BAD) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (!(p = p->rbnext)) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   // Legge nella lista dei parametri i riferimenti alla seconda connessione OLE-DSB
   if ((pDstConn = gsc_getConnectionFromLisp(p)) == NULL) return RTERROR;
   if (!(p = gsc_nth(5, arg))) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (!p || p->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   pDstType = p->resval.rstring;
   if (!(p = p->rbnext) || gsc_rb2Lng(p, &LenDst) == GS_BAD) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (!(p = p->rbnext) || gsc_rb2Int(p, &DecDst) == GS_BAD) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   // Verifico la compatibilità dei tipi   
   res = pSrcConn->IsCompatibleType(pSrcType, LenSrc, DecSrc, *pDstConn, pDstType, LenDst, DecDst);
   
   acedRetInt(res);

   return RTNORM;
}


/****************************************************************/
/*.doc gs_getSRIDList                                <external> */
/*+
  Questa funzione riceve una connessione OLE-DB.
  La funzione restituisce una lista dei sistemi di coordinate supportati dalla
  connessione OLD-DB.
  Parametri:
  Lista RESBUF 
  (("UDL_FILE" <file UDL> || <stringa di connessione>) ("UDL_PROP" <value>))
-*/
/****************************************************************/
int gs_getSRIDList(void)
{
   presbuf        arg = acedGetArgs();
   C_RB_LIST      ret;
   C_2STR_LIST    SRID_descr_list;
   C_DBCONNECTION *pConn;

   acedRetNil();
 
   // Legge nella lista dei parametri i riferimenti alla prima connessione OLE-DSB
   if ((pConn = gsc_getConnectionFromLisp(arg)) == NULL) return GS_BAD;
   // Leggo la lista degli SRID
   if (pConn->get_SRIDList(SRID_descr_list) == GS_BAD) return GS_BAD;
   ret << SRID_descr_list.to_rb();
   ret.LspRetList();

   return GS_GOOD;
}

/****************************************************************/
/*.doc gsc_UDLProperties_nethost2drive               <external> */
/*+
  Questa funzione converte le proprietà (per ora solo il catalogo) che indicano
  una path assoluta in una path relativa.
  Parametri: 
  const TCHAR *UDLFile;       File di connessione UDL
  C_STRING &UDLProperties;    Stringa delle proprietà UDL per la personalizzazione
                              della connessione OLE-DB
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int gsc_UDLProperties_nethost2drive(const TCHAR *UDLFile, C_STRING &UDLProperties)
{
   C_2STR_LIST PropList;

   if (UDLProperties.len() == 0) return GS_GOOD;
   if (gsc_PropListFromConnStr(UDLProperties.get_name(), PropList) == GS_GOOD)
   {
      if (gsc_UDLProperties_nethost2drive(UDLFile, PropList) == GS_BAD) return GS_BAD;
      // modifico stringa di proprietà UDL
      UDLProperties.paste(gsc_PropListToConnStr(PropList));
   }

   return GS_GOOD;
}
int gsc_UDLProperties_nethost2drive(const TCHAR *UDLFile, C_2STR_LIST &PropList)
{
   C_RB_LIST DescrUDL;
   int       i = 0;
   presbuf   pDescrUDL, pTypeDescrUDL, pNameDescrUDL;

   // ((<Name>[<Descr.>][Resource Type][CATALOG || SCHEMA])
   //  (<Name>[<Descr.>][Resource Type][CATALOG || SCHEMA])...)
   DescrUDL << gsc_getUDLPropertiesDescrFromFile(UDLFile);

   while ((pDescrUDL = DescrUDL.nth(i++)))
      // se si riferisce ad una risorsa di tipo file o direttorio
      if ((pTypeDescrUDL = gsc_nth(2, pDescrUDL)) && pTypeDescrUDL->restype == RTSTR &&
          (gsc_DBstr2CatResType(pTypeDescrUDL->resval.rstring) == DirectoryRes ||
           gsc_DBstr2CatResType(pTypeDescrUDL->resval.rstring) == FileRes))
      {
         if ((pNameDescrUDL = gsc_nth(0, pDescrUDL)) && pNameDescrUDL->restype == RTSTR)
            if (gsc_internal_UDLProperties_nethost2drive(pNameDescrUDL->resval.rstring,
                                                         PropList) == GS_BAD)
               return GS_BAD;
      }

   return GS_GOOD;
}

int gsc_internal_UDLProperties_nethost2drive(TCHAR *UDLPropertyName,
                                             C_2STR_LIST &PropList)
{
   TCHAR    *PropName;
   C_2STR   *pProp;
   C_STRING NewProp;

   PropName = gsc_strstr(UDLPropertyName, _T("[Extended Properties]"), FALSE);
   if (PropName && PropName == UDLPropertyName)
   {  // la proprietà UDL è rappresentata da una proprietà in "Extended Properties"
      C_2STR_LIST ExtendedPropList;
      C_2STR      *pExtendedProp;

      PropName += wcslen(_T("[Extended Properties]"));
      if (!(pProp = (C_2STR *) PropList.search_name(_T("Extended Properties"), FALSE)))
         { GS_ERR_COD = eGSUDLPropertyNotFound; return GS_BAD; }

      if (gsc_PropListFromExtendedPropConnStr(pProp->get_name2(), ExtendedPropList) == GS_BAD)
         { GS_ERR_COD = eGSUDLPropertyNotFound; return GS_BAD; }

      if (!(pExtendedProp = (C_2STR *) ExtendedPropList.search_name(PropName, FALSE)))
         { GS_ERR_COD = eGSUDLPropertyNotFound; return GS_BAD; }

      NewProp = pExtendedProp->get_name2();
      if (gsc_nethost2drive(NewProp) == GS_BAD) return GS_BAD; // Converto

      pExtendedProp->set_name2(NewProp.get_name()); // setto nuova proprietà Extended
      NewProp.paste(gsc_ExtendedPropListToConnStr(ExtendedPropList));
   }
   else
   {
      if ((pProp = (C_2STR *) PropList.search_name(UDLPropertyName, FALSE)))
      {
         NewProp = pProp->get_name2();
         if (gsc_nethost2drive(NewProp) == GS_BAD) return GS_BAD;
      }
   }

   if (pProp) pProp->set_name2(NewProp.get_name()); // setto nuova proprietà

   return GS_GOOD;
}


/****************************************************************/
/*.doc gsc_UDLProperties_drive2nethost               <external> */
/*+
  Questa funzione converte le proprietà (per ora solo il catalogo) che indicano
  una path relativa in una path assoluta.
  Parametri: 
  const TCHAR *UDLFile;       File di connessione UDL
  C_STRING &UDLProperties;    Stringa delle proprietà UDL per la personalizzazione
                              della connessione OLE-DB
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int gsc_UDLProperties_drive2nethost(const TCHAR *UDLFile, C_STRING &UDLProperties)
{
   C_2STR_LIST PropList;

   if (UDLProperties.len() == 0) return GS_GOOD;
   if (gsc_PropListFromConnStr(UDLProperties.get_name(), PropList) == GS_GOOD)
   {
      if (gsc_UDLProperties_drive2nethost(UDLFile, PropList) == GS_BAD) return GS_BAD;
      // modifico stringa di proprietà UDL
      UDLProperties.paste(gsc_PropListToConnStr(PropList));
   }

   return GS_GOOD;
}
int gsc_UDLProperties_drive2nethost(const TCHAR *UDLFile, C_2STR_LIST &PropList)
{
   C_RB_LIST DescrUDL;
   int       i = 0;
   presbuf   pDescrUDL, pTypeDescrUDL, pNameDescrUDL;

   // ((<Name>[<Descr.>][Resource Type][CATALOG || SCHEMA])
   //  (<Name>[<Descr.>][Resource Type][CATALOG || SCHEMA])...)
   DescrUDL << gsc_getUDLPropertiesDescrFromFile(UDLFile);

   while ((pDescrUDL = DescrUDL.nth(i++)))
      // se si riferisce ad una risorsa di tipo file o direttorio
      if ((pTypeDescrUDL = gsc_nth(2, pDescrUDL)) && pTypeDescrUDL->restype == RTSTR &&
          (gsc_DBstr2CatResType(pTypeDescrUDL->resval.rstring) == DirectoryRes ||
           gsc_DBstr2CatResType(pTypeDescrUDL->resval.rstring) == FileRes))
      {
         if ((pNameDescrUDL = gsc_nth(0, pDescrUDL)) && pNameDescrUDL->restype == RTSTR)
            if (gsc_internal_UDLProperties_drive2nethost(pNameDescrUDL->resval.rstring,
                                                         PropList) == GS_BAD)
               return GS_BAD;
      }

   return GS_GOOD;
}

int gsc_internal_UDLProperties_drive2nethost(TCHAR *UDLPropertyName,
                                             C_2STR_LIST &PropList)
{
   TCHAR    *PropName;
   C_2STR   *pProp;
   C_STRING NewProp;

   PropName = gsc_strstr(UDLPropertyName, _T("[Extended Properties]"), FALSE);
   if (PropName && PropName == UDLPropertyName)
   {  // la proprietà UDL è rappresentata da una proprietà in "Extended Properties"
      C_2STR_LIST ExtendedPropList;
      C_2STR      *pExtendedProp;

      PropName += wcslen(_T("[Extended Properties]"));
      if (!(pProp = (C_2STR *) PropList.search_name(_T("Extended Properties"), FALSE)))
         { GS_ERR_COD = eGSUDLPropertyNotFound; return GS_BAD; }

      if (gsc_PropListFromExtendedPropConnStr(pProp->get_name2(), ExtendedPropList) == GS_BAD)
         { GS_ERR_COD = eGSUDLPropertyNotFound; return GS_BAD; }

      if (!(pExtendedProp = (C_2STR *) ExtendedPropList.search_name(PropName, FALSE)))
         { GS_ERR_COD = eGSUDLPropertyNotFound; return GS_BAD; }

      NewProp = pExtendedProp->get_name2();
      if (gsc_drive2nethost(NewProp) == GS_BAD) return GS_BAD; // Converto

      pExtendedProp->set_name2(NewProp.get_name()); // setto nuova proprietà Extended
      NewProp.paste(gsc_ExtendedPropListToConnStr(ExtendedPropList));
   }
   else
   {
      if ((pProp = (C_2STR *) PropList.search_name(UDLPropertyName, FALSE)))
      {
         NewProp = pProp->get_name2();
         if (gsc_drive2nethost(NewProp) == GS_BAD) return GS_BAD;
      }
   }

   if (pProp) pProp->set_name2(NewProp.get_name()); // setto nuova proprietà

   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_IsValidFieldName                    <external> */
/*+
  Questa funzione riceve un nome di campo e verifica se è valido
  eventualmente correggendolo (se possibile). Ritorna al lisp una stringa
  conetnente il nome del campo validato oppure nil in  caso di errore.
  Parametri:
  (<OLE_DB conn><field_name>)
  dove
  <OLE_DB conn> = (("UDL_FILE" <Connection>) ("UDL_PROP" <Properties>))
  <Connection>  = <file UDL> | <stringa di connessione>
  <Properties>  = stringa delle proprietà | ((<prop1><value>)(<prop1><value>)...)

  Restituisce GS_GOOD se il tipo rappresenta un Blob altrimenti ritorna GS_BAD.
-*/  
/*********************************************************/
int gs_IsValidFieldName(void)
{
   presbuf        arg = acedGetArgs();
   C_DBCONNECTION *pConn;
   C_STRING       FieldName;

   acedRetNil();
 
   // Legge nella lista dei parametri i riferimenti alla tabella
   if ((pConn = gsc_getConnectionFromLisp(gsc_nth(0, arg))) == NULL) return RTERROR;
   if (!(arg = gsc_nth(1, arg)) || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   FieldName = arg->resval.rstring;
   if (pConn->IsValidFieldName(FieldName) == GS_GOOD) acedRetStr(FieldName.get_name());

   return RTNORM;
}


//-----------------------------------------------------------------------//
//////////////////  FUNZIONI PER TIPI ADO    INIZIO    ////////////////////
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc gsc_DBIsBlob                           <external> */
/*+
  Questa funzione riceve un tipo ADO e ne verifica la tipologia.
  Parametri:
  (<OLE_DB conn><field_descr_type>)
  dove
  <OLE_DB conn> = (("UDL_FILE" <Connection>) ("UDL_PROP" <Properties>))
  <Connection>  = <file UDL> | <stringa di connessione>
  <Properties>  = stringa delle proprietà | ((<prop1><value>)(<prop1><value>)...)

  oppure

  DataTypeEnum DataType;  Tipo ADO

  Restituisce GS_GOOD se il tipo rappresenta un Blob altrimenti ritorna GS_BAD.
-*/  
/*********************************************************/
int gs_DBIsBlob(void)
{
   presbuf        arg = acedGetArgs();
   C_DBCONNECTION *pConn;

   acedRetNil();
 
   // Legge nella lista dei parametri i riferimenti alla tabella
   if ((pConn = gsc_getConnectionFromLisp(gsc_nth(0, arg))) == NULL) return RTERROR;
   if (!(arg = gsc_nth(1, arg))|| arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (pConn->IsBlob(arg->resval.rstring) == GS_GOOD) acedRetT();

   return RTNORM;
}
int gsc_DBIsBlob(DataTypeEnum DataType)
{       
   return (DataType == adBinary ||
           DataType == adLongVarBinary ||
           DataType == adLongVarChar ||
           DataType == adLongVarWChar ||
           DataType == adVarBinary ||
           DataType == adBinary) ? GS_GOOD : GS_BAD;
}


/*********************************************************/
/*.doc gsc_DBIsBoolean                        <external> */
/*+
  Questa funzione riceve un tipo ADO e ne verifica la tipologia.
  Parametri:
  (<OLE_DB conn><field_descr_type>)
  dove
  <OLE_DB conn> = (("UDL_FILE" <Connection>) ("UDL_PROP" <Properties>))
  <Connection>  = <file UDL> | <stringa di connessione>
  <Properties>  = stringa delle proprietà | ((<prop1><value>)(<prop1><value>)...)

  oppure

  DataTypeEnum DataType;  Tipo ADO

  Restituisce GS_GOOD se il tipo rappresenta un boolean altrimenti ritorna GS_BAD.
-*/  
/*********************************************************/
int gs_DBIsBoolean(void)
{
   presbuf        arg = acedGetArgs();
   C_DBCONNECTION *pConn;

   acedRetNil();
 
   // Legge nella lista dei parametri i riferimenti alla tabella
   if ((pConn = gsc_getConnectionFromLisp(gsc_nth(0, arg))) == NULL) return RTERROR;
   if (!(arg = gsc_nth(1, arg))|| arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (pConn->IsBoolean(arg->resval.rstring) == GS_GOOD) acedRetT();

   return RTNORM;
}
inline int gsc_DBIsBoolean(DataTypeEnum DataType)
{       
   return (DataType == adBoolean) ? GS_GOOD : GS_BAD;
}


/*********************************************************/
/*.doc gsc_DBIsNumeric                        <external> */
/*+
  Questa funzione riceve un tipo ADO e ne verifica la tipologia.
  Parametri:
  (<OLE_DB conn><field_descr_type>)
  dove
  <OLE_DB conn> = (("UDL_FILE" <Connection>) ("UDL_PROP" <Properties>))
  <Connection>  = <file UDL> | <stringa di connessione>
  <Properties>  = stringa delle proprietà | ((<prop1><value>)(<prop1><value>)...)

  oppure

  DataTypeEnum DataType;  Tipo ADO

  Restituisce GS_GOOD se il tipo rappresenta un numero altrimenti ritorna GS_BAD.
-*/  
/*********************************************************/
int gs_DBIsNumeric(void)
{
   presbuf        arg = acedGetArgs();
   C_DBCONNECTION *pConn;

   acedRetNil();
 
   // Legge nella lista dei parametri i riferimenti alla tabella
   if ((pConn = gsc_getConnectionFromLisp(gsc_nth(0, arg))) == NULL) return RTERROR;
   if (!(arg = gsc_nth(1, arg))|| arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (pConn->IsNumeric(arg->resval.rstring) == GS_GOOD) acedRetT();

   return RTNORM;
}
int gsc_DBIsNumeric(DataTypeEnum DataType)
{       
   switch (DataType)
   {
      case adTinyInt:
      case adSmallInt:
      case adInteger:
      case adBigInt:
      case adUnsignedTinyInt:
      case adUnsignedSmallInt:
      case adUnsignedInt:
      case adUnsignedBigInt:
      case adSingle:
      case adDouble:
      case adCurrency:
      case adDecimal:
      case adNumeric:
         return GS_GOOD;
   }
   return GS_BAD;
}


/*********************************************************/
/*.doc gsc_DBIsNumericWithDecimals            <external> */
/*+
  Questa funzione riceve un tipo ADO e ne verifica la tipologia.
  Parametri:
  (<OLE_DB conn><field_descr_type>)
  dove
  <OLE_DB conn> = (("UDL_FILE" <Connection>) ("UDL_PROP" <Properties>))
  <Connection>  = <file UDL> | <stringa di connessione>
  <Properties>  = stringa delle proprietà | ((<prop1><value>)(<prop1><value>)...)

  oppure

  DataTypeEnum DataType;  Tipo ADO

  Restituisce GS_GOOD se il tipo rappresenta un numero altrimenti ritorna GS_BAD.
-*/  
/*********************************************************/
int gs_DBIsNumericWithDecimals(void)
{
   presbuf        arg = acedGetArgs();
   C_DBCONNECTION *pConn;

   acedRetNil();
 
   // Legge nella lista dei parametri i riferimenti alla tabella
   if ((pConn = gsc_getConnectionFromLisp(gsc_nth(0, arg))) == NULL) return RTERROR;
   if (!(arg = gsc_nth(1, arg))|| arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (pConn->IsNumericWithDecimals(arg->resval.rstring) == GS_GOOD) acedRetT();

   return RTNORM;
}
int gsc_DBIsNumericWithDecimals(DataTypeEnum DataType)
{       
   switch (DataType)
   {
      case adSingle:
      case adDouble:
      case adCurrency:
      case adDecimal:
      case adNumeric:
         return GS_GOOD;
   }
   return GS_BAD;
}


/*********************************************************/
/*.doc gsc_DBIsChar                           <external> */
/*+
  Questa funzione riceve un tipo ADO e ne verifica la tipologia.
  Parametri:
  (<OLE_DB conn><field_descr_type>)
  dove
  <OLE_DB conn> = (("UDL_FILE" <Connection>) ("UDL_PROP" <Properties>))
  <Connection>  = <file UDL> | <stringa di connessione>
  <Properties>  = stringa delle proprietà | ((<prop1><value>)(<prop1><value>)...)

  oppure

  DataTypeEnum DataType;  Tipo ADO

  Restituisce GS_GOOD se il tipo rappresenta una stringa altrimenti ritorna GS_BAD.
-*/  
/*********************************************************/
int gs_DBIsChar(void)
{
   presbuf        arg = acedGetArgs();
   C_DBCONNECTION *pConn;

   acedRetNil();
 
   // Legge nella lista dei parametri i riferimenti alla tabella
   if ((pConn = gsc_getConnectionFromLisp(gsc_nth(0, arg))) == NULL) return RTERROR;
   if (!(arg = gsc_nth(1, arg))|| arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (pConn->IsChar(arg->resval.rstring) == GS_GOOD) acedRetT();

   return RTNORM;
}
int gsc_DBIsChar(DataTypeEnum DataType)
{       
   switch (DataType)
   {
      case adBSTR:
      case adChar:
      case adVarChar:
      case adLongVarChar:
      case adWChar:
      case adVarWChar:
      case adLongVarWChar:
         return GS_GOOD;
   }
   return GS_BAD;
}


/*********************************************************/
/*.doc gsc_DBIsDate                           <external> */
/*+
  Questa funzione riceve un tipo ADO e ne verifica la tipologia.
  Parametri:
  (<OLE_DB conn><field_descr_type>)
  dove
  <OLE_DB conn> = (("UDL_FILE" <Connection>) ("UDL_PROP" <Properties>))
  <Connection>  = <file UDL> | <stringa di connessione>
  <Properties>  = stringa delle proprietà | ((<prop1><value>)(<prop1><value>)...)

  oppure

  DataTypeEnum DataType;  Tipo ADO

  Restituisce GS_GOOD se il tipo rappresenta una data altrimenti ritorna GS_BAD.
-*/  
/*********************************************************/
int gs_DBIsDate(void)
{
   presbuf        arg = acedGetArgs();
   C_DBCONNECTION *pConn;

   acedRetNil();
 
   // Legge nella lista dei parametri i riferimenti alla tabella
   if ((pConn = gsc_getConnectionFromLisp(gsc_nth(0, arg))) == NULL) return RTERROR;
   if (!(arg = gsc_nth(1, arg))|| arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (pConn->IsDate(arg->resval.rstring) == GS_GOOD) acedRetT();

   return RTNORM;
}
int gsc_DBIsDate(DataTypeEnum DataType)
{       
   switch (DataType)
   {
      case adDate:
      case adDBDate:
         return GS_GOOD;
   }
   return GS_BAD;
}


/*********************************************************/
/*.doc gsc_DBIsTimestamp                      <external> */
/*+
  Questa funzione riceve un tipo ADO e ne verifica la tipologia.
  Parametri:
  (<OLE_DB conn><field_descr_type>)
  dove
  <OLE_DB conn> = (("UDL_FILE" <Connection>) ("UDL_PROP" <Properties>))
  <Connection>  = <file UDL> | <stringa di connessione>
  <Properties>  = stringa delle proprietà | ((<prop1><value>)(<prop1><value>)...)

  oppure

  DataTypeEnum DataType;  Tipo ADO

  Restituisce GS_GOOD se il tipo rappresenta una data altrimenti ritorna GS_BAD.
-*/  
/*********************************************************/
int gs_DBIsTimestamp(void)
{
   presbuf        arg = acedGetArgs();
   C_DBCONNECTION *pConn;

   acedRetNil();
 
   // Legge nella lista dei parametri i riferimenti alla tabella
   if ((pConn = gsc_getConnectionFromLisp(gsc_nth(0, arg))) == NULL) return RTERROR;
   if (!(arg = gsc_nth(1, arg))|| arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (pConn->IsTimestamp(arg->resval.rstring) == GS_GOOD) acedRetT();

   return RTNORM;
}
int gsc_DBIsTimestamp(DataTypeEnum DataType)
{       
   switch (DataType)
   {
      case adDBTimeStamp:
         return GS_GOOD;
   }
   return GS_BAD;
}


/*********************************************************/
/*.doc gsc_DBIsTime                          <external> */
/*+
  Questa funzione riceve un tipo ADO e ne verifica la tipologia.
  Parametri:
  (<OLE_DB conn><field_descr_type>)
  dove
  <OLE_DB conn> = (("UDL_FILE" <Connection>) ("UDL_PROP" <Properties>))
  <Connection>  = <file UDL> | <stringa di connessione>
  <Properties>  = stringa delle proprietà | ((<prop1><value>)(<prop1><value>)...)

  oppure

  DataTypeEnum DataType;  Tipo ADO

  Restituisce GS_GOOD se il tipo rappresenta una data altrimenti ritorna GS_BAD.
-*/  
/*********************************************************/
int gs_DBIsTime(void)
{
   presbuf        arg = acedGetArgs();
   C_DBCONNECTION *pConn;

   acedRetNil();
 
   // Legge nella lista dei parametri i riferimenti alla tabella
   if ((pConn = gsc_getConnectionFromLisp(gsc_nth(0, arg))) == NULL) return RTERROR;
   if (!(arg = gsc_nth(1, arg))|| arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (pConn->IsTime(arg->resval.rstring) == GS_GOOD) acedRetT();

   return RTNORM;
}
int gsc_DBIsTime(DataTypeEnum DataType)
{       
   switch (DataType)
   {
      case adDBTime:
         return GS_GOOD;
   }
   return GS_BAD;
}


/*********************************************************/
/*.doc gsc_DBIsCurrency                           <external> */
/*+
  Questa funzione riceve un tipo ADO e ne verifica la tipologia.
  Parametri:
  (<OLE_DB conn><field_descr_type>)
  dove
  <OLE_DB conn> = (("UDL_FILE" <Connection>) ("UDL_PROP" <Properties>))
  <Connection>  = <file UDL> | <stringa di connessione>
  <Properties>  = stringa delle proprietà | ((<prop1><value>)(<prop1><value>)...)

  oppure

  DataTypeEnum DataType;  Tipo ADO

  Restituisce GS_GOOD se il tipo rappresenta una data altrimenti ritorna GS_BAD.
-*/  
/*********************************************************/
int gs_DBIsCurrency(void)
{
   presbuf        arg = acedGetArgs();
   C_DBCONNECTION *pConn;

   acedRetNil();
 
   // Legge nella lista dei parametri i riferimenti alla tabella
   if ((pConn = gsc_getConnectionFromLisp(gsc_nth(0, arg))) == NULL) return RTERROR;
   if (!(arg = gsc_nth(1, arg))|| arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (pConn->IsCurrency(arg->resval.rstring) == GS_GOOD) acedRetT();

   return RTNORM;
}
int gsc_DBIsCurrency(DataTypeEnum DataType)
{       
   switch (DataType)
   {
      case adCurrency:
         return GS_GOOD;
   }
   return GS_BAD;
}


//-----------------------------------------------------------------------//
//////////////////  FUNZIONI PER TIPI ADO    FINE      ////////////////////
//////////////////  C_PROVIDER_TYPE      INIZIO    ////////////////////////
//-----------------------------------------------------------------------//


C_PROVIDER_TYPE::C_PROVIDER_TYPE() : C_STR()
{
   Type   = adEmpty;
   Size   = 0;
   Dec    = 0;
   IsLong = FALSE;          
   IsFixedLength     = RTVOID;
   IsFixedPrecScale  = FALSE;
   IsAutoUniqueValue = FALSE;
}

C_PROVIDER_TYPE::~C_PROVIDER_TYPE() {}

DataTypeEnum C_PROVIDER_TYPE::get_Type() { return Type; }
int          C_PROVIDER_TYPE::set_Type(DataTypeEnum in)
   { Type = in; return GS_GOOD; }

long C_PROVIDER_TYPE::get_Size() { return Size; }
int    C_PROVIDER_TYPE::set_Size(long in)
   { Size = in; return GS_GOOD; }

int C_PROVIDER_TYPE::get_Dec() { return Dec; }
int C_PROVIDER_TYPE::set_Dec(int in)
   { Dec = in; return GS_GOOD; }

TCHAR *C_PROVIDER_TYPE::get_LiteralPrefix() { return LiteralPrefix.get_name(); }
int  C_PROVIDER_TYPE::set_LiteralPrefix(const TCHAR *in)
   { LiteralPrefix = in; return GS_GOOD; }

TCHAR *C_PROVIDER_TYPE::get_LiteralSuffix() { return LiteralSuffix.get_name(); }
int  C_PROVIDER_TYPE::set_LiteralSuffix(const TCHAR *in)
   { LiteralSuffix = in; return GS_GOOD; }

TCHAR *C_PROVIDER_TYPE::get_CreateParams() { return CreateParams.get_name(); }
int  C_PROVIDER_TYPE::set_CreateParams(const TCHAR *in)
   { CreateParams = in; return GS_GOOD; }

short C_PROVIDER_TYPE::get_IsLong() { return IsLong; }
int  C_PROVIDER_TYPE::set_IsLong(short in)
   { IsLong = in; return GS_GOOD; }

short C_PROVIDER_TYPE::get_IsFixedLength() { return IsFixedLength; }
int  C_PROVIDER_TYPE::set_IsFixedLength(short in)
   { IsFixedLength = in; return GS_GOOD; }

short C_PROVIDER_TYPE::get_IsFixedPrecScale() { return IsFixedPrecScale; }
int  C_PROVIDER_TYPE::set_IsFixedPrecScale(short in)
   { IsFixedPrecScale = in; return GS_GOOD; }

short C_PROVIDER_TYPE::get_IsAutoUniqueValue() { return IsAutoUniqueValue; }
int  C_PROVIDER_TYPE::set_IsAutoUniqueValue(short in)
   { IsAutoUniqueValue = in; return GS_GOOD; }


/****************************************************************/
/*.doc C_PROVIDER_TYPE::DataValueStr2Rb              <external> */
/*+
  Questa funzione trasforma un valore stringa in un resbuf del tipo corretto.
  Parametri: 
  const TCHAR *Val;     Valore in formato stringa
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B.: Alloca memoria
-*/  
/****************************************************************/
presbuf C_PROVIDER_TYPE::DataValueStr2Rb(TCHAR *Val)
{
   presbuf p;

   if (!Val || wcslen(Val) == 0)
   {
      p = acutBuildList(RTNIL, 0);
   }
   else
   {
      switch (Type)
      {
         case adEmpty:
         case adError:
         case adUserDefined:
         case adVariant:
         case adIDispatch:
         case adIUnknown:
         case adGUID:
         case adChapter:
         case adFileTime:
         case adPropVariant:
         case adVarNumeric:
            p = acutBuildList(RTNIL, 0);
            break;

         case adTinyInt:
         case adUnsignedTinyInt:
         case adSmallInt:
         case adUnsignedSmallInt:
         {
            double Num;

            if (gsc_conv_Number(Val, &Num) == GS_BAD)
               { GS_ERR_COD = eGSInvAttribType; return NULL; }
            if (Num > 32767 || Num < -32768)
               { GS_ERR_COD = eGSInvAttribLen; return NULL; }
            p = acutBuildList(RTSHORT, int(Num), 0);
            break;
         }

         case adInteger:
         case adBigInt:
         case adUnsignedInt:
         case adUnsignedBigInt:
         {
            double Num;

            if (gsc_conv_Number(Val, &Num) == GS_BAD)
               { GS_ERR_COD = eGSInvAttribType; return NULL; }
            if (Num > 2147483647 || Num < -2147483647)
               { GS_ERR_COD = eGSInvAttribLen; return NULL; }
            p = acutBuildList(RTLONG, long(Num), 0);
            break;
         }

         case adSingle:
         case adDouble:
         case adDecimal:
         case adNumeric:
         {
            double Num;

            if (gsc_conv_Number(Val, &Num) == GS_BAD)
               { GS_ERR_COD = eGSInvAttribType; return NULL; }
            p = acutBuildList(RTREAL, Num, 0);
            break;
         }

         case adCurrency:
         {
            double Num;

            if (gsc_conv_Currency(Val, &Num) == GS_BAD)
               { GS_ERR_COD = eGSInvAttribType; return NULL; }
            p = acutBuildList(RTREAL, Num, 0);
            break;
         }

         case adBoolean:
         {
            int Bool;

            if (gsc_conv_Bool(Val, &Bool) == GS_BAD)
               { GS_ERR_COD = eGSInvAttribType; return NULL; }
            p = acutBuildList((Bool == FALSE) ? RTNIL : RTT, 0);
            break;
         }
         
         case adDate:
         case adDBDate:
         case adDBTime:
         case adDBTimeStamp:
         case adBSTR:
         case adChar:
         case adVarChar:
         case adLongVarChar:
         case adWChar:
         case adVarWChar:
         case adLongVarWChar:
            p = acutBuildList(RTSTR, Val, 0);
            break;

         case adBinary:
         case adVarBinary:
         case adLongVarBinary:
            p = acutBuildList(RTNIL, 0);
            break;

         default:
            p = acutBuildList(RTNIL, 0);
      }
   }
   if (!p) GS_ERR_COD = eGSOutOfMem;
   
   return p;
}


int C_PROVIDER_TYPE::copy(C_PROVIDER_TYPE &out)
{
   out.name              = name;
   out.Type              = Type;
   out.Size              = Size;
   out.Dec               = Dec;
   out.LiteralPrefix     = LiteralPrefix;
   out.LiteralSuffix     = LiteralSuffix;
   out.CreateParams      = CreateParams;
   out.IsLong            = IsLong;
   out.IsFixedLength     = IsFixedLength;
   out.IsFixedPrecScale  = IsFixedPrecScale;
   out.IsAutoUniqueValue = IsAutoUniqueValue;

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//////////////////  C_PROVIDER_TYPE      FINE      ////////////////////////
//////////////////  C_PROVIDER_TYPE_LIST   INIZIO  ////////////////////////
//-----------------------------------------------------------------------//


C_PROVIDER_TYPE_LIST::C_PROVIDER_TYPE_LIST() : C_LIST() {}

C_PROVIDER_TYPE_LIST::~C_PROVIDER_TYPE_LIST() {}


/****************************************************************/
/*.doc C_PROVIDER_TYPE::search_Type                  <external> */
/*+
  Questa funzione cerca un tipo di dato nella lista che, possibilmente soddisfi
  la caratteristica di auto-incremento.
  Parametri:
  DataTypeEnum in;         Tipo di dato
  int IsAutoUniqueValue;   Caratteristica di auto-incremento

  Restituisce il puntatore al tipo di dato in caso di successo 
  altrimenti restituisce NULL.
-*/  
/****************************************************************/
C_PROVIDER_TYPE *C_PROVIDER_TYPE_LIST::search_Type(DataTypeEnum in, int IsAutoUniqueValue)
{
   C_PROVIDER_TYPE *pProviderType = NULL;

   // in questo ciclo cerco i tipi verificando la caratteristica di AutoUniqueValue
   cursor = get_head();
   while (cursor)
   {
      if (((C_PROVIDER_TYPE *) cursor)->get_Type() == in)
      {  // i tipi coincidono
         pProviderType = (C_PROVIDER_TYPE *) cursor;
         if (IsAutoUniqueValue == pProviderType->get_IsAutoUniqueValue()) break;
      }
      cursor = get_next();
   }

   // se la ricerca ha dato esito negativo ed esisteva un tipo coincidente che però
   // non soddisfava la caratteristica di AutoUniqueValue lo accetto comunque
   // perchè dò per scontato che per un tipo ad autoincremento ne esiste
   // uno anche normale.
   if (!cursor && pProviderType) cursor = pProviderType;

   return (C_PROVIDER_TYPE *) cursor;
}   
// begin from cursor
C_PROVIDER_TYPE *C_PROVIDER_TYPE_LIST::search_Next_Type(DataTypeEnum in, int IsAutoUniqueValue)
{
   C_PROVIDER_TYPE *pProviderType = NULL;

   // in questo ciclo cerco i tipi verificando la caratteristica di AutoUniqueValue
   get_next();
   while (cursor)
   {
      if (((C_PROVIDER_TYPE *) cursor)->get_Type() == in)
      {  // i tipi coincidono
         pProviderType = (C_PROVIDER_TYPE *) cursor;
         if (IsAutoUniqueValue == pProviderType->get_IsAutoUniqueValue()) break;
      }
      cursor = get_next();
   }

   // se la ricerca ha dato esito negativo ed esisteva un tipo coincidente che però
   // non soddisfava la caratteristica di AutoUniqueValue lo accetto comunque
   // perchè dò per scontato che per un tipo ad autoincremento ne esiste
   // uno anche normale.
   if (!cursor && pProviderType) cursor = pProviderType;

   return (C_PROVIDER_TYPE *) cursor;
}   


/****************************************************************/
/*.doc C_PROVIDER_TYPE_LIST::load                    <external> */
/*+
  Questa funzione carica tutti i tipi di dato (escluso quelli binari) 
  supportati dalla connessione con un provider di database.
  La funzione non ammette la duplicazione dei tipi quindi, nel caso ci siano due
  tipi con nome identici, carica quello con maggiore capacità di memorizzazione.
  Parametri: 
  _ConnectionPtr &pConn;     Connessione OLE-DB
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_PROVIDER_TYPE_LIST::load(_ConnectionPtr &pConn)
{
   _RecordsetPtr   pRs;
   C_RB_LIST	    ColValues;
   presbuf         pName, pType, pSize, pDec, pPrefix, pSuffix, pCreatePar;
   presbuf         pIsLong, pIsFixedLength, pIsFixedPrecScale, pIsAutoUniqueValue;
   C_PROVIDER_TYPE *pProvType;
   long            _Long;
   int             _Int, Result = GS_GOOD;

   remove_all();
   
   if (gsc_OpenSchema(pConn, adSchemaProviderTypes, pRs) == GS_BAD) return GS_BAD;
   
   if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD)
      { gsc_DBCloseRs(pRs); return GS_BAD; }
   pName      = ColValues.CdrAssoc(_T("TYPE_NAME"));
   pType      = ColValues.CdrAssoc(_T("DATA_TYPE"));
   // "COLUMN_SIZE" = La lunghezza di una colonna non-numerica o di un parametro
   //                 si riferisce sia alla lunghezza massima che a quella definita
   //                 per questo tipo dal provider. Per i dati caratteri,
   //                 questo valore rappresenta la lunghezza massima o quella
   //                 definita in caratteri. Per i tipi datetime, questa è la
   //                 lunghezza della rappresentazione stringa (considerando
   //                 la precisione massima ammessa per la componente frazionaria dei secondi)
   //                 Se il dato è numerico, questo è il limite superiore della
   //                 massima precisione del tipo di dato ovvero il numero delle
   //                 cifre più rappresentative memorizzabili (senza contare il segno e il
   //                 seoaratore decimale).
   pSize      = ColValues.CdrAssoc(_T("COLUMN_SIZE"));
   // "MAXIMUM_SCALE" = Se type è adDecimal o adNumeric rappresenta il numero massimo
   //                   di cifre decimali ammesse a destra del separatore decimale
   pDec       = ColValues.CdrAssoc(_T("MAXIMUM_SCALE"));
   pPrefix    = ColValues.CdrAssoc(_T("LITERAL_PREFIX"));
   pSuffix    = ColValues.CdrAssoc(_T("LITERAL_SUFFIX"));
   pCreatePar = ColValues.CdrAssoc(_T("CREATE_PARAMS"));
   pIsLong    = ColValues.CdrAssoc(_T("IS_LONG"));
   pIsFixedLength     = ColValues.CdrAssoc(_T("IS_FIXEDLENGTH"));
   pIsFixedPrecScale  = ColValues.CdrAssoc(_T("FIXED_PREC_SCALE"));
   pIsAutoUniqueValue = ColValues.CdrAssoc(_T("AUTO_UNIQUE_VALUE"));

   while (gsc_isEOF(pRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { Result = GS_BAD; break; }

//#if defined(GSDEBUG) // se versione per debugging
//      gsc_printlist(ColValues.get_head()); // roby test stampa tipi di dato di un provider
//#endif

      // leggo la dimensione supportata dalla tipologia di dato
      if (gsc_rb2Lng(pSize, &_Long) == GS_BAD) { Result = GS_BAD; break; }

      // leggo il codice ADO del tipo di dato
      if (gsc_rb2Int(pType, &_Int) == GS_BAD) { Result = GS_BAD; break; }

      // verifico che il nome del tipo non sia ancora presente
      if ((pProvType = (C_PROVIDER_TYPE *) search_name(pName->resval.rstring)) != NULL)
         // Se il tipo era già presente in lista
         // Se il tipo a livello ADO è uguale
         if (pProvType->Type == _Int)
            // Se il tipo già presente in lista poteva contenere meno dati
            if (pProvType->get_Size() < _Long)
               remove(pProvType); // lo rimuovo
            else
               { gsc_Skip(pRs); continue; } // lo salto

      // scarto il tipo binario 
      if ((DataTypeEnum) _Int == adBinary || 
          (DataTypeEnum) _Int == adVarBinary ||
          (DataTypeEnum) _Int == adLongVarBinary)
         { gsc_Skip(pRs); continue; }

      if ((pProvType = new C_PROVIDER_TYPE) == NULL)
         { GS_ERR_COD = eGSOutOfMem; Result = GS_BAD; break; }

      if (pProvType->set_name(pName->resval.rstring) == GS_BAD)
         { delete pProvType; Result = GS_BAD; break; }

      if (pProvType->set_Type((DataTypeEnum) _Int) == GS_BAD)
         { delete pProvType; Result = GS_BAD; break; }

      if (pProvType->set_Size(_Long) == GS_BAD)
         { delete pProvType; Result = GS_BAD; break; }

      if (gsc_rb2Lng(pIsLong, &_Long) == GS_GOOD)
         pProvType->set_IsLong((_Long == 0) ? FALSE : TRUE);

      if (gsc_rb2Lng(pIsFixedPrecScale, &_Long) == GS_GOOD)
          pProvType->set_IsFixedPrecScale((_Long == 0) ? FALSE : TRUE);
      
      if (gsc_rb2Lng(pIsAutoUniqueValue, &_Long) == GS_GOOD)
          pProvType->set_IsAutoUniqueValue((_Long == 0) ? FALSE : TRUE);

      // se il tipo è numerico adNumeric o adNumeric leggo il n. max di decimali
      if (gsc_rb2Int(pDec, &_Int) == GS_GOOD && 
          (pProvType->get_Type() == adNumeric || pProvType->get_Type() == adNumeric))
         pProvType->set_Dec(_Int);
      else
         pProvType->set_Dec(-1);

      if (pPrefix->restype == RTSTR)
         if (pProvType->set_LiteralPrefix(pPrefix->resval.rstring) == GS_BAD)
            { delete pProvType; Result = GS_BAD; break; }

      if (pSuffix->restype == RTSTR)
         if (pProvType->set_LiteralSuffix(pSuffix->resval.rstring) == GS_BAD)
            { delete pProvType; Result = GS_BAD; break; }

      if (pCreatePar->restype == RTSTR)
         if (pProvType->set_CreateParams(pCreatePar->resval.rstring) == GS_BAD)
            { delete pProvType; Result = GS_BAD; break; }

      if (gsc_rb2Lng(pIsFixedLength, &_Long) == GS_GOOD)
         pProvType->set_IsFixedLength((_Long == 0) ? RTNIL : RTT);
      else
         pProvType->set_IsFixedLength(RTVOID); // non si sa

      add_tail(pProvType);
      gsc_Skip(pRs);
   }
   gsc_DBCloseRs(pRs);

   return Result;
}


resbuf *C_PROVIDER_TYPE_LIST::to_rb()
{
   C_RB_LIST result;

   result.ReleaseAllAtDistruction(GS_BAD);

   return result.get_head();
}


int C_PROVIDER_TYPE_LIST::copy(C_PROVIDER_TYPE_LIST &out)
{
   C_PROVIDER_TYPE *pItem, *pOutItem;

   out.remove_all();
   pItem = (C_PROVIDER_TYPE *) get_head();
   while (pItem)
   {
      if ((pOutItem = new C_PROVIDER_TYPE) == NULL)
         { out.remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if (pItem->copy(*pOutItem) == GS_BAD)
         { out.remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      out.add_tail(pOutItem);

      pItem = (C_PROVIDER_TYPE *) pItem->get_next();
   }

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_PROVIDER_TYPE::DataValueStr2Rb              <external> */
/*+
  Questa funzione trasforma un valore stringa di un tipo di dati noto
  in un resbuf del tipo corretto.
  Parametri:
  const TCHAR *Type;    Tipo di dato
  TCHAR *Val;           Valore in formato stringa
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B.: Alloca memoria
-*/  
/****************************************************************/
presbuf C_PROVIDER_TYPE_LIST::DataValueStr2Rb(const TCHAR *Type, TCHAR *Val)
{
   C_PROVIDER_TYPE *pProviderType = (C_PROVIDER_TYPE *) search_name(Type, FALSE);
   
   if (!pProviderType) { GS_ERR_COD = eGSUnkwnDrvFieldType; return GS_BAD; }
   
   return pProviderType->DataValueStr2Rb(Val);
}


//-----------------------------------------------------------------------//
//////////////////  C_PROVIDER_TYPE_LIST     FINE  ////////////////////////
//////////////////  C_DBCONNECTION       INIZIO    ////////////////////////
//-----------------------------------------------------------------------//


C_DBCONNECTION::C_DBCONNECTION(const TCHAR *FileRequired)
{
   CatalogSeparator      = DEFAULT_CATALOG_SEP;
   CatalogResourceType   = EmptyRes;
   InitQuotedIdentifier  = DEFAULT_INIT_QUOTED_ID;
   FinalQuotedIdentifier = DEFAULT_FINAL_QUOTED_ID;
   MultiCharWildcard     = _T("%"); // standard SQL
   SingleCharWildcard    = _T("_"); // standard SQL
   MaxTableNameLen       = MAX_TABLE_NAME_LEN;
   MaxFieldNameLen       = MAX_FIELD_NAME_LEN;
   StrCaseFullTableRef   = Upper;
   pCharProviderType     = NULL;
   EscapeStringSupported = false;
   IsMultiUser           = TRUE;
   IsUniqueIndexName     = FALSE;
   SchemaViewsSupported  = false;

   MaxTolerance2ApproxCurve  = 1.0;
   MaxSegmentNum2ApproxCurve = 50;
   SQLMaxVertexNum           = 32000;

   // verifico l'esistenza del file con estensione .REQUIRED
   if (gsc_path_exist(FileRequired) == GS_GOOD)
   {
      C_PROFILE_SECTION_BTREE ProfileSections;

      if (gsc_read_profile(FileRequired, ProfileSections) == GS_GOOD)
      {
         ReadCatProperty(ProfileSections);         // leggo proprietà del Catalogo
         ReadQuotedIdentifiers(ProfileSections);   // leggo proprietà dei QuotedIdentifiers
         ReadMaxTableNameLen(ProfileSections);     // leggo lunghezza massima nome tabella
         ReadWildcards(ProfileSections);           // leggo proprietà di wildcards
         ReadMaxFieldNameLen(ProfileSections);     // leggo lunghezza massima nome campo
         ReadStrCaseFullTableRef(ProfileSections); // leggo se il riferimento alla tabella deve essere maiuscolo
         ReadInvalidFieldNameCharacters(ProfileSections); // leggo i caratteri non validi per i nomi dei campi
         ReadEscapeStringSupported(ProfileSections); // leggo se i carattere di esacape sono supportati nei valori dei campi carattere
         ReadRowLockSuffix_SQLKeyWord(ProfileSections); // leggo se il suffisso SQL per bloccare le righe di una tabella
         ReadIsMultiUser(ProfileSections);         // leggo se il DB è gestito in multiutenza
         ReadIsUniqueIndexName(ProfileSections);   // leggo se il nome dell'indice è univoco 
         gsc_ReadDBSample(ProfileSections, DBSample);
         // Spatial
         ReadGeomTypeDescrList(ProfileSections);   // leggo la lista dei tipi geometrici
         ReadGeomToWKT_SQLKeyWord(ProfileSections); // leggo parola chiave SQL per trasformare geometria in testo
         ReadWKTToGeom_SQLKeyWord(ProfileSections); // leggo parola chiave SQL per trasformare testo in geometria
         ReadInsideSpatialOperator_SQLKeyWord(ProfileSections); // leggo parola chiave SQL per operatore spaziale
         ReadCrossingSpatialOperator_SQLKeyWord(ProfileSections); // leggo parola chiave SQL per operatore spaziale
         ReadSRIDList_SQLKeyWord(ProfileSections); // leggo istruzione SQL per avere la lista dei SRID
         ReadSRIDFromField_SQLKeyWord(ProfileSections); // leggo istruzione SQL per avere lo SRID di un campo
         ReadSpatialIndexCreation_SQLKeyWord(ProfileSections); // leggo parola chiave SQL per la creazione di un indice spaziale
         ReadMaxTolerance2ApproxCurve(ProfileSections);  // leggo la distanza massima ammessa tra segmenti retti e la curva
         ReadMaxSegmentNum2ApproxCurve(ProfileSections); // leggo numero massimo di segmenti retti
         ReadIsValidGeom_SQLKeyWord(ProfileSections);       // leggo parola chiave SQL per validazione geometria 
         ReadIsValidReasonGeom_SQLKeyWord(ProfileSections); // leggo parola chiave SQL per sapere il motivo per cui la geometria non è valida
         ReadCoordTransform_SQLKeyWord(ProfileSections); // leggo parola chiave SQL per convertire le coordinate di una geometria in SRID
      }
   }
}


C_DBCONNECTION::~C_DBCONNECTION()
{
   gsc_DBCloseConn(pConn);
}


/****************************************************************/
/*.doc C_DBCONNECTION::Open                          <external> */
/*+
  Questa funzione apre una connessione con un provider di database.
  Parametri: 
  const TCHAR *ConnStr;    Stringa di connessione al database
  bool Prompt;             Flag, se = true nel caso la connessione non sia possibile
                           chiede all'utente una login e password (default = true)
  int PrintError;          Se il flag = GS_GOOD in caso di errore viene
                           stampato il messaggio relativo altrimenti non
                           viene stampato alcun messaggio (default = GS_GOOD)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::Open(const TCHAR *ConnStr, bool Prompt, int PrintError)
{
   if (gsc_DBOpenConnFromStr(ConnStr, pConn,
                             (TCHAR *) GEOsimAppl::GS_USER.log,
                             (TCHAR *) GEOsimAppl::GS_USER.pwd,
                             Prompt, PrintError) == GS_BAD)
      return GS_BAD;

   OrigConnectionStr = ConnStr;
   if (DataTypeList.load(pConn) == GS_BAD) return GS_BAD;

   try
   {
      DBMSName = (TCHAR*)_bstr_t(pConn->Properties->GetItem(_T("DBMS Name"))->GetValue());
      DBMSName.toupper();      

      DBMSVersion = (TCHAR*)_bstr_t(pConn->Properties->GetItem(_T("DBMS Version"))->GetValue());
      DBMSVersion.toupper();

      if (CatalogResourceType == EmptyRes) // se non è ancora stato inizializzato
      {                                    // (es. mancanza del file *.REQUIRED)
         C_STRING CatalogTerm;
      
         CatalogTerm = (TCHAR *)_bstr_t(pConn->Properties->GetItem(_T("Catalog Term"))->GetValue());
         if (CatalogTerm.comp(_T("DIRECTORY"), FALSE) == 0) CatalogResourceType = DirectoryRes;
      }

      // Verifico se viene supportata l'opzione adSchemaViews
      SAFEARRAY FAR* psa = NULL; // Creo un safearray che prende 4 elementi
      SAFEARRAYBOUND rgsabound;
      _variant_t     var, varCriteria;
      long           ix;
   	_RecordsetPtr  pRs;

      rgsabound.lLbound   = 0;
      rgsabound.cElements = 4;
      psa = SafeArrayCreate(VT_VARIANT, 1, &rgsabound);

      // CATALOGO
      ix = 0; var.vt = VT_EMPTY; SafeArrayPutElement(psa, &ix, &var);
      // SCHEMA
      ix = 1; var.vt = VT_EMPTY; SafeArrayPutElement(psa, &ix, &var);
      // NOME VISTA DA CERCARE
      ix = 2; var.vt = VT_EMPTY; SafeArrayPutElement(psa, &ix, &var);
      // TIPO DI OGGETTO DA CERCARE
      ix = 3; var.vt = VT_EMPTY; SafeArrayPutElement(psa, &ix, &var);

      varCriteria.vt = VT_ARRAY|VT_VARIANT;
      varCriteria.parray = psa;  

      // memorizzo il codice di errore precedente perchè se le viste non sono 
      // supportate si genera un errore
      int Prev_GS_ERR_COD = GS_ERR_COD;

      // Leggo la lista delle tabelle
      if (gsc_OpenSchema(pConn, adSchemaViews, pRs, &varCriteria, ONETEST, GS_BAD) == GS_GOOD)
      {
         SchemaViewsSupported = true;
         gsc_DBCloseRs(pRs);
      }
      else
         GS_ERR_COD = Prev_GS_ERR_COD;
   }

   catch (_com_error &e)
	{
      if (PrintError == GS_GOOD)
         printDBErrs(e, pConn);
   }

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::UDLProperties_nethost2drive    <external> */
/*+
  Questa funzione converte le proprietà (per ora solo il catalogo) che indicano
  una path assoluta in una path relativa.
  Parametri: 
  C_STRING &UDLProperties;     Stringa delle proprietà UDL per la personalizzazione
                               della connessione OLE-DB
  oppure
  C_2STR_LIST &PropList;
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::UDLProperties_nethost2drive(C_STRING &UDLProperties)
{
   C_2STR_LIST PropList;

   if (UDLProperties.len() == 0) return GS_GOOD;

   if (gsc_PropListFromConnStr(UDLProperties.get_name(), PropList) == GS_GOOD)
   {
      if (UDLProperties_nethost2drive(PropList) == GS_BAD) return GS_BAD;
      // modifico stringa di proprietà UDL
      UDLProperties.paste(gsc_PropListToConnStr(PropList));
   }
   return GS_GOOD;
}
int C_DBCONNECTION::UDLProperties_nethost2drive(C_2STR_LIST &PropList)
{
   if (PropList.get_count() > 0)
   {
      TCHAR    *PropName;
      C_2STR   *pProp;
      C_STRING NewProp;

      // Converto eventuale path del catalogo contenuto nella stringa di connessione
      PropName = gsc_strstr(get_CatalogUDLProperty(), _T("[Extended Properties]"), FALSE);
      if (PropName && PropName == get_CatalogUDLProperty())
      {  // il catalogo è rappresentato da una proprietà in "Extended Properties"
         if ((pProp = (C_2STR *) PropList.search_name(PropName, FALSE)))
         {
            NewProp = pProp->get_name2();
            if (gsc_nethost2drive(NewProp) == GS_BAD) return GS_BAD; // Converto
            pProp->set_name2(NewProp.get_name()); // aggiorno la proprietà Extended
         }
         else
            if ((pProp = (C_2STR *) PropList.search_name(_T("Extended Properties"), FALSE)))
            {
               C_2STR_LIST ExtendedPropList;
               C_2STR      *pExtendedProp;

               if (gsc_PropListFromExtendedPropConnStr(pProp->get_name2(), ExtendedPropList) == GS_BAD)
                  { GS_ERR_COD = eGSUDLPropertyNotFound; return GS_BAD; }

               PropName += wcslen(_T("[Extended Properties]"));
               if ((pExtendedProp = (C_2STR *) ExtendedPropList.search_name(PropName, FALSE)))
                  { GS_ERR_COD = eGSUDLPropertyNotFound; return GS_BAD; }

               NewProp = pExtendedProp->get_name2();

               if (gsc_nethost2drive(NewProp) == GS_BAD) return GS_BAD; // Converto

               pExtendedProp->set_name2(NewProp.get_name()); // aggiorno la proprietà Extended
               NewProp.paste(gsc_ExtendedPropListToConnStr(ExtendedPropList));
            }
      }
      else
      {
         if ((pProp = (C_2STR *) PropList.search_name(_T("Data Source"), FALSE)))
         {
            C_STRING Drive, Dir;

            NewProp = pProp->get_name2();

            // se si tratta di una path, la converto
            gsc_splitpath(NewProp, &Drive, &Dir);
            if (Drive.len() >0 && Dir.len() > 0)
               if (gsc_nethost2drive(NewProp) == GS_BAD) return GS_BAD;
         }
      }

      if (pProp) pProp->set_name2(NewProp.get_name()); // setto nuova proprietà
   }

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::UDLProperties_drive2nethost   <external> */
/*+
  Questa funzione converte le proprietà (per ora solo il catalogo) che indicano
  una path assoluta in una path relativa.
  Parametri: 
  C_STRING &UDLProperties;     Stringa delle proprietà UDL per la personalizzazione
                               della connessione OLE-DB
  oppure
  C_2STR_LIST &PropList;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::UDLProperties_drive2nethost(C_STRING &UDLProperties)
{
   C_2STR_LIST PropList;

   if (UDLProperties.len() == 0) return GS_GOOD;

   if (gsc_PropListFromConnStr(UDLProperties.get_name(), PropList) == GS_GOOD)
   {
      if (UDLProperties_drive2nethost(PropList) == GS_BAD) return GS_BAD;
      // modifico stringa di proprietà UDL
      UDLProperties.paste(gsc_PropListToConnStr(PropList));
   }
   return GS_GOOD;
}
int C_DBCONNECTION::UDLProperties_drive2nethost(C_2STR_LIST &PropList)
{
   if (PropList.get_count() > 0)
   {
      TCHAR    *PropName;
      C_2STR   *pProp;
      C_STRING NewProp;

      // Converto eventuale path del catalogo contenuto nella stringa di connessione
      PropName = gsc_strstr(get_CatalogUDLProperty(), _T("[Extended Properties]"), FALSE);
      if (PropName && PropName == get_CatalogUDLProperty())
      {  // il catalogo è rappresentato da una proprietà in "Extended Properties"
         if ((pProp = (C_2STR *) PropList.search_name(_T("Extended Properties"), FALSE)))
         {
            C_2STR_LIST ExtendedPropList;
            C_2STR      *pExtendedProp;

            if (gsc_PropListFromExtendedPropConnStr(pProp->get_name2(), ExtendedPropList) == GS_BAD)
               { GS_ERR_COD = eGSUDLPropertyNotFound; return GS_BAD; }

            PropName += wcslen(_T("[Extended Properties]"));
            if (!(pExtendedProp = (C_2STR *) ExtendedPropList.search_name(PropName, FALSE)))
               { GS_ERR_COD = eGSUDLPropertyNotFound; return GS_BAD; }

            NewProp = pExtendedProp->get_name2();
            if (gsc_drive2nethost(NewProp) == GS_BAD) return GS_BAD; // Converto

            pExtendedProp->set_name2(NewProp.get_name()); // setto nuova proprietà Extended
            NewProp.paste(gsc_ExtendedPropListToConnStr(ExtendedPropList));
         }
         else  // forse è scritta nella sintassi [Extended Properties]propname
            if ((pProp = (C_2STR *) PropList.search_name(PropName, FALSE)))
            {
               NewProp = pProp->get_name2();
               if (gsc_drive2nethost(NewProp) == GS_BAD) return GS_BAD; // Converto

               pProp->set_name2(NewProp.get_name()); // setto nuova proprietà Extended
            }
      }
      else
      {
         if ((pProp = (C_2STR *) PropList.search_name(_T("Data Source"), FALSE)))
         {
            C_STRING Drive, Dir;

            NewProp = pProp->get_name2();

            // se si tratta di una path, la converto
            gsc_splitpath(NewProp, &Drive, &Dir);
            if (Drive.len() >0 && Dir.len() > 0)
               if (gsc_drive2nethost(NewProp) == GS_BAD) return GS_BAD;
         }
      }

      if (pProp) pProp->set_name2(NewProp.get_name()); // setto nuova proprietà
   }

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::WriteToUDL                    <external> */
/*+
  Questa funzione scrive la stringa di connessione in un file .UDL.
  Parametri: 
  const TCHAR *UDLFile;     File di connessione al database
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::WriteToUDL(const TCHAR *UDLFile)
{
   return gsc_WriteOleDBIniStrToFile(pConn->ConnectionString, UDLFile);
}


/****************************************************************/
/*.doc C_DBCONNECTION::get_Connection                <external> */
/*+
  Questa restituisce la connessione con un provider di database.
-*/  
/****************************************************************/
_ConnectionPtr C_DBCONNECTION::get_Connection()
{
   return pConn;
}


/****************************************************************/
/*.doc C_DBCONNECTION::get_OrigConnectionStr         <external> */
/*+
  Questa restituisce la stringa di connessione originale che era stata
  proposta per l'apertura del collegamento con un provider di database.
  Questa stringa può differire da quella letta dalla pConn->ConnectionString
  che invece ricava la stringa dalla proprietà
  "ConnectionString" dell'oggetto ADO Connection.
-*/  
/****************************************************************/
const TCHAR *C_DBCONNECTION::get_OrigConnectionStr()
   { return OrigConnectionStr.get_name(); }


/****************************************************************/
/*.doc C_DBCONNECTION::get_DBMSName                  <external> */
/*+
  Questa restituisce il nome del DataBaseManagemenetSystem.
-*/
/****************************************************************/
const TCHAR *C_DBCONNECTION::get_DBMSName()
   { return DBMSName.get_name(); }


/****************************************************************/
/*.doc C_DBCONNECTION::getConnectionStrSubstCat      <external> */
/*+
  Questa restituisce una stringa di connessione modificata per gestione
  tabelle nel catalogo specificato (se supportato).
  Parametri:
  const TCHAR *TableRef;    Riferimento completo alla tabella

  N.B. Alloca memoria.
-*/
/****************************************************************/
TCHAR *C_DBCONNECTION::getConnectionStrSubstCat(const TCHAR *TableRef)
{
   C_STRING    Catalog, Schema, Table;
   C_2STR_LIST PropList;

   if (gsc_PropListFromConnStr(get_OrigConnectionStr(), PropList) == GS_BAD) return NULL;

   if (split_FullRefTable(TableRef, Catalog, Schema, Table) == GS_BAD) return NULL;

   if (Catalog.get_name() && gsc_strlen(get_CatalogUDLProperty()) > 0) // catalogo supportato
   {
      C_STRING    CatProp;
      C_2STR_LIST NewUDLProperties;

      CatProp = get_CatalogUDLProperty();
      CatProp += _T('=');
      CatProp += Catalog;
      if (gsc_PropListFromConnStr(CatProp.get_name(), NewUDLProperties) == GS_BAD)
         return NULL;

      // aggiorno la lista di proprietà UDL
      if (gsc_ModUDLProperties(PropList, NewUDLProperties) == GS_BAD) return NULL;
   }
      
   // modifico stringa di inizializzazione connessione OLE-DB
   return gsc_PropListToConnStr(PropList);
}


/****************************************************************/
/*.doc C_DBCONNECTION::get_CatalogUsage              <external> */
/*+
  Questa funzione restituisce la modalità di uso del catalogo 
  implementata dalla connessione con il provider di database.
  Composizione a bit:
  - se = DBPROPVAL_CU_DML_STATEMENTS il catalogo è
         supportato in tutte le istruzioni di manipolazione dati
  - se = DBPROPVAL_CU_TABLE_DEFINITION il catalogo è
         supportato in tutte le istruzioni di definizione delle tabelle
  - se = DBPROPVAL_CU_INDEX_DEFINITION il catalogo è
         supportato in tutte le istruzioni di definizione degli indici
  - se = DBPROPVAL_CU_PRIVILEGE_DEFINITION il catalogo è
         supportato in tutte le istruzioni di definizione dei privilegi
-*/  
/****************************************************************/
short C_DBCONNECTION::get_CatalogUsage()
{
   Property   *pResult;
   _variant_t PropName(_T("Catalog Usage"));

   if (pConn.GetInterfacePtr() == NULL) return 0;

   if (pConn->Properties->get_Item(PropName, &pResult) != S_OK) return 0; 

   return (short) pResult->GetValue();
}


/****************************************************************/
/*.doc C_DBCONNECTION::get_CatalogLocation           <external> */
/*+
  Questa funzione restituisce la posizione del catalogo 
  implementata dalla connessione con il provider di database.
  Composizione a bit:
  - se = DBPROPVAL_CL_START indica che il catalogo è
         all'inizio del nome completo della tabella
  - se = DBPROPVAL_CL_END indica che il catalogo è
         alla fine del nome completo della tabella
-*/  
/****************************************************************/
short C_DBCONNECTION::get_CatalogLocation()
{
   Property   *pResult;
   _variant_t PropName(_T("Catalog Location"));

   if (pConn.GetInterfacePtr() == NULL) return 0;

   if (pConn->Properties->get_Item(PropName, &pResult) != S_OK) return 0; 

   return (short) pResult->GetValue();
}


/****************************************************************/
/*.doc C_DBCONNECTION::get_CatalogSeparator          <external> */
/*+
  Questa funzione restituisce il carattere separatore del catalogo.
  (per dBASE, FOX = "\", per ACCESS = ".", per ORACLE = "@")
-*/  
/****************************************************************/
TCHAR C_DBCONNECTION::get_CatalogSeparator()
   { return CatalogSeparator; }


/****************************************************************/
/*.doc C_DBCONNECTION::get_CatalogResourceType       <external> */
/*+
  Questa funzione restituisce il tipo di risorsa rappresentata dal catalogo.
  (EmptyRes, DirectoryRes, FileRes, ServerRes, WorkbookRes, Database)
-*/  
/****************************************************************/
ResourceTypeEnum C_DBCONNECTION::get_CatalogResourceType()
   { return CatalogResourceType; }


/****************************************************************/
/*.doc C_DBCONNECTION::CatalogUDLProperty            <external> */
/*+
  Questa funzione restituisce la Proprietà UDL che rappresenta il catalogo.
  (es. "[Extended Properties]DBQ")
-*/  
/****************************************************************/
TCHAR *C_DBCONNECTION::get_CatalogUDLProperty()
   { return CatalogUDLProperty.get_name(); }


/****************************************************************/
/*.doc C_DBCONNECTION::CatalogUDLProperty            <external> */
/*+
  Questa funzione restituisce il puntatore alla lista dei tipi di dato
  della connessione OLE-DB.
-*/  
/****************************************************************/
C_PROVIDER_TYPE_LIST* C_DBCONNECTION::ptr_DataTypeList()
   { return &DataTypeList; }


/****************************************************************/
/*.doc C_DBCONNECTION::get_SchemaUsage               <external> */
/*+
  Questa funzione restituisce la modalità di uso dello schema 
  implementata dalla connessione con il provider di database.
  Composizione a bit:
  - se = DBPROPVAL_SU_DML_STATEMENTS lo schema è
         supportato in tutte le istruzioni di manipolazione dati
  - se = DBPROPVAL_SU_TABLE_DEFINITION lo schema è
         supportato in tutte le istruzioni di definizione delle tabelle
  - se = DBPROPVAL_SU_INDEX_DEFINITION lo schema è
         supportato in tutte le istruzioni di definizione degli indici
  - se = DBPROPVAL_SU_PRIVILEGE_DEFINITION lo schema è
         supportato in tutte le istruzioni di definizione dei privilegi
-*/  
/****************************************************************/
short C_DBCONNECTION::get_SchemaUsage()
{
   Property   *pResult;
   _variant_t PropName(_T("Schema Usage"));

   if (pConn.GetInterfacePtr() == NULL) return 0;

   if (pConn->Properties->get_Item(PropName, &pResult) != S_OK) return 0; 

   return (short) pResult->GetValue();
}
   
 
/****************************************************************/
/*.doc C_DBCONNECTION::get_SchemaSeparator           <external> */
/*+
  Questa funzione restituisce il carattere separatore dello schema.
  E' sempre "." ?
-*/  
/****************************************************************/
TCHAR C_DBCONNECTION::get_SchemaSeparator()
   { return _T('.'); }
   

/****************************************************************/
/*.doc C_DBCONNECTION::get_InitQuotedIdentifier      <external> */
/*+
  Questa funzione restituisce il carattere iniziale per delimitare
  il nome del catalogo, dello schema e della tabella.
  (per ODBC = ", per ACCESS 97 = [)
-*/  
/****************************************************************/
TCHAR C_DBCONNECTION::get_InitQuotedIdentifier()
   { return InitQuotedIdentifier; }
   

/****************************************************************/
/*.doc C_DBCONNECTION::get_FinalQuotedIdentifier     <external> */
/*+
  Questa funzione restituisce il carattere finale per delimitare
  il nome del catalogo, dello schema e della tabella.
  (per ODBC = ", per ACCESS 97 = ])
-*/  
/****************************************************************/
TCHAR C_DBCONNECTION::get_FinalQuotedIdentifier()
   { return FinalQuotedIdentifier; }
   

/****************************************************************/
/*.doc C_DBCONNECTION::get_MultiCharWildcard         <external> */
/*+
  Questa funzione restituisce il carattere jolly wildcard
  per più caratteri
-*/  
/****************************************************************/
const TCHAR *C_DBCONNECTION::get_MultiCharWildcard()
   { return MultiCharWildcard.get_name(); }


/****************************************************************/
/*.doc C_DBCONNECTION::get_SingleCharWildcard         <external> */
/*+
  Questa funzione restituisce il carattere jolly wildcard
  per più caratteri
-*/  
/****************************************************************/
const TCHAR *C_DBCONNECTION::get_SingleCharWildcard()
   { return SingleCharWildcard.get_name(); }


/****************************************************************/
/*.doc C_DBCONNECTION::get_MaxTableNameLen           <external> */
/*+
  Questa funzione restituisce la lunghezza massima del nome della tabella.
  (per dBASE 3,dBASE 4, dBASE 5, FOX 2, FOX 2.5, FOX 2.6 = 8)
-*/  
/****************************************************************/
int C_DBCONNECTION::get_MaxTableNameLen()
   { return MaxTableNameLen; }


/****************************************************************/
/*.doc C_DBCONNECTION::get_MaxFieldNameLen           <external> */
/*+
  Questa funzione restituisce la lunghezza massima del nome dell'attributo.
  (per dBASE 3,dBASE 4, dBASE 5, FOX 2, FOX 2.5, FOX 2.6 = 10)
-*/  
/****************************************************************/
int C_DBCONNECTION::get_MaxFieldNameLen()
   { return MaxFieldNameLen; }


/****************************************************************/
/*.doc C_DBCONNECTION::get_StrCaseFullTableRef       <external> */
/*+
  Questa funzione restituisce se il riferimento alla tabella deve 
  essere indicato in maiuscolo, minuscolo o non ha importanza.
-*/  
/****************************************************************/
StrCaseTypeEnum C_DBCONNECTION::get_StrCaseFullTableRef()
   { return StrCaseFullTableRef; }


/****************************************************************/
/*.doc C_DBCONNECTION::get_InvalidFieldNameCharacters <external> */
/*+
  Questa funzione restituisce i caratteri non validi per i nomi 
  dei campi di una tabella.
-*/  
/****************************************************************/
const TCHAR* C_DBCONNECTION::get_InvalidFieldNameCharacters()
   { return InvalidFieldNameCharacters.get_name(); }


/****************************************************************/
/*.doc C_DBCONNECTION::get_EscapeStringSupported     <external> */
/*+
  Questa funzione restituisce se i carattere di escape (\n, \t ...)
  sono supportati nei valori dei campi carattere. In caso affermativo
  i valori stringa che contengono lo '\' devono essere raddoppiati in '\\'
-*/  
/****************************************************************/
bool C_DBCONNECTION::get_EscapeStringSupported()
   { return EscapeStringSupported; }


/****************************************************************/
/*.doc C_DBCONNECTION::get_RowLockSuffix_SQLKeyWord <external> */
/*+
  Questa funzione restituisce la parola chiave SQL da aggiungere in fondo
  all'istruzione SELECT...per bloccare le righe
  (es. per PostgreSQL "FOR UPDATE NOWAIT" in una transazione).
-*/  
/****************************************************************/
const TCHAR* C_DBCONNECTION::get_RowLockSuffix_SQLKeyWord()
   { return RowLockSuffix_SQLKeyWord.get_name(); }


/****************************************************************/
/*.doc C_DBCONNECTION::get_IsMultiUser               <external> */
/*+
  Questa funzione restituisce se il DB è gestito in multiutenza o 
  in monoutenza (Es. EXCEL è monoutente).
-*/  
/****************************************************************/
short C_DBCONNECTION::get_IsMultiUser()
   { return IsMultiUser; }


/****************************************************************/
/*.doc C_DBCONNECTION::get_IsUniqueIndexName         <external> */
/*+
  Questa funzione restituisce se il nome dell'indice è univoco 
  nello stesso schema (nome tabella diverso da nome indice es. ORACLE e
  PostgreSQL).
-*/  
/****************************************************************/
short C_DBCONNECTION::get_IsUniqueIndexName()
   { return IsUniqueIndexName; }


/****************************************************************/
/*.doc C_DBCONNECTION::get_GeomTypeDescrListPtr      <external> */
/*+
  Questa funzione restituisce la lista di tipi 
  geometrici GEOsim associati alla descrizione SQL usata dal provider
  (es. (TYPE_POLYLINE "LINESTRING") in PostGIS)
-*/  
/****************************************************************/
C_INT_STR_LIST *C_DBCONNECTION::get_GeomTypeDescrListPtr()
   { return &GeomTypeDescrList; }


/****************************************************************/
/*.doc C_DBCONNECTION::get_GeomToWKT_SQLKeyWord      <external> */
/*+
  Questa funzione restituisce la parola chiave SQL 
  per trasformare geometria in testo (es. "st_asewkt(alias.campo)" in PostGIS 
  o "alias.campo.get_wkt()" in ORACLE).
  La parola chiave sarà quindi composta da un parte fissa e una variabile
  es. "st_asewkt(%s)" o "%s.get_wkt()" dove %s verrà sostituito run-time.
-*/  
/****************************************************************/
const TCHAR *C_DBCONNECTION::get_GeomToWKT_SQLKeyWord()
   { return GeomToWKT_SQLKeyWord.get_name(); }


/****************************************************************/
/*.doc C_DBCONNECTION::get_GeomToWKT_SQLSyntax       <external> */
/*+
  Questa funzione restituisce la sintassi SQL per leggere da una tabella
  (di cui è noto l'alias) la geometria in formato testo di un
  campo (es. "st_asewkt(alias.campo)" in PostGIS o
             "alias.campo.get_wkt()" in ORACLE).
  La parola chiave sarà quindi composta da un parte fissa e una variabile
  es. "st_asewkt(%s)" o "%s.get_wkt()" dove %s verrà sostituito run-time.
  Parametri:
  C_STRING &AliasTabName;        Alias della tabella
  C_STRING &GeomAttribName;      nome del campo geometrico
  C_STRING &DestSRID;            Codice del SRID di destinazione (vuoto = non usato)
  C_STRING &Result;

-*/  
/****************************************************************/
int C_DBCONNECTION::get_GeomToWKT_SQLSyntax(C_STRING &AliasTabName,
                                            C_STRING &GeomAttribName,
                                            C_STRING &DestSRID,
                                            C_STRING &Result)
{
   C_STRING CompletedFieldName, AliasFieldName;
   TCHAR dummy[1024];
   
   AliasFieldName = AliasTabName;
   AliasFieldName += _T(".");
   AliasFieldName += get_InitQuotedIdentifier();
   AliasFieldName += GeomAttribName;
   AliasFieldName += get_FinalQuotedIdentifier();

   if (DestSRID.len() == 0) // Se NON si deve convertire il sistema di coordinate
      CompletedFieldName = AliasFieldName;
   else
      if (get_CoordTransform_SQLSyntax(AliasFieldName, DestSRID, CompletedFieldName) != GS_GOOD)
         { GS_ERR_COD = eGSInvalidCoord; return GS_BAD; }

   swprintf(dummy, 1024, get_GeomToWKT_SQLKeyWord(), CompletedFieldName.get_name());

   Result = dummy;

   return GS_GOOD; 
}


/****************************************************************/
/*.doc C_DBCONNECTION::get_WKTToGeom_SQLKeyWord      <external> */
/*+
  Questa funzione restituisce la parola chiave SQL 
  per trasformare testo in geometria (es. "st_geomfromtext('%s',%s)" in PostGIS o 
  "SDO_GEOMETRY('%s',%s)" in ORACLE).
  La parola chiave sarà quindi composta da un parte fissa e una variabile
  es. "st_geomfromtext('%s',%s)" o "SDO_GEOMETRY('%s',%s)"
  dove %s verrà sostituito run-time.
  -*/  
/****************************************************************/
const TCHAR *C_DBCONNECTION::get_WKTToGeom_SQLKeyWord()
   { return WKTToGeom_SQLKeyWord.get_name(); }


/****************************************************************/
/*.doc C_DBCONNECTION::get_WKTToGeom_SQLSyntax       <external> */
/*+
  Questa funzione restituisce la sintassi SQL per convertire una
  geometria in formato testo in campo geometrico 
  (es. "st_geomfromtext('%s',%s)" in PostGIS o "SDO_GEOMETRY('%s',%s)" in ORACLE).
  La parola chiave sarà quindi composta da un parte fissa e una variabile
  es. "st_geomfromtext('%s',%s)" o "SDO_GEOMETRY('%s',%s)"
  dove %s verrà sostituito run-time.
  Parametri:
  C_STRING &WKT;      Stringa in formato WKT
  C_STRING &SrcSRID;  Codice del sistema di coordinate sorgente (vuoto = non usato)
  C_STRING &DestSRID; Codice del SRID di destinazione (vuoto = non usato)
  C_STRING &Result;   Stringa con la sintassi corretta (output)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::get_WKTToGeom_SQLSyntax(C_STRING &WKT, C_STRING &SrcSRID,
                                            C_STRING &DestSRID, C_STRING &Result)
{
   TCHAR  *pStr;
   size_t Len;

   if (SrcSRID.len() == 0)
   {
      Len = WKTToGeom_SQLKeyWord.len() + WKT.len() + DestSRID.len() + 1;
      if ((pStr = (TCHAR *) malloc(sizeof (TCHAR) * Len)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      swprintf(pStr, Len, get_WKTToGeom_SQLKeyWord(), WKT.get_name(), DestSRID.get_name());
      Result.paste(pStr);
   }
   else
   {
      C_STRING GeomSQL;

      Len = WKTToGeom_SQLKeyWord.len() + WKT.len() + SrcSRID.len() + 1;
      if ((pStr = (TCHAR *) malloc(sizeof (TCHAR) * Len)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      swprintf(pStr, Len, get_WKTToGeom_SQLKeyWord(), WKT.get_name(), SrcSRID.get_name());

      GeomSQL.paste(pStr);
      if (get_CoordTransform_SQLSyntax(GeomSQL, DestSRID, Result) != GS_GOOD)
      {
         GeomSQL.clear();
         get_WKTToGeom_SQLSyntax(WKT, GeomSQL, DestSRID, Result);
      }
   }
   
   return GS_GOOD; 
}


/****************************************************************/
/*.doc C_DBCONNECTION::get_InsideSpatialOperator_SQLKeyWord <external> */
/*+
  Questa funzione restituisce la parola chiave SQL usata come operatore
  spaziale per cercare gli oggetti totalmente interni ad una area
  (es. "within" in PostGIS o "SDO_INSIDE" in ORACLE).
  La parola chiave sarà quindi composta da un parte fissa e una variabile
  es. "within(%s, %s)" in PostGIS o "SDO_INSIDE(%s, %s) = 'TRUE'" in Oracle
  dove %s verrà sostituito run-time.
  -*/  
/****************************************************************/
const TCHAR *C_DBCONNECTION::get_InsideSpatialOperator_SQLKeyWord()
   { return InsideSpatialOperator_SQLKeyWord.get_name(); }


/****************************************************************/
/*.doc C_DBCONNECTION::get_InsideSpatialOperator_SQLSyntax <external> */
/*+
  Questa funzione restituisce la sintassi SQL per cercare gli oggetti 
  totalmente interni (Geom1)ad una area (Geom2)
  (es. "within" in PostGIS o "SDO_INSIDE" in ORACLE).
  La parola chiave sarà quindi composta da un parte fissa e una variabile
  es. "within(%s, %s)" in PostGIS o "SDO_INSIDE(%s, %s) = 'TRUE'" in Oracle
  dove %s verrà sostituito run-time.
-*/  
/****************************************************************/
int C_DBCONNECTION::get_InsideSpatialOperator_SQLSyntax(C_STRING &Geom1,
                                                        C_STRING &Geom2,
                                                        C_STRING &Result)
{
   TCHAR    *pStr;
   size_t   Len = InsideSpatialOperator_SQLKeyWord.len() + Geom1.len() + Geom2.len() + 1;
   C_STRING Prefix;

   if (DBMSName.at(PG_DBMSNAME, FALSE) != NULL) // è in PostgreSQL
   {
      Prefix = Geom1;
      Prefix += _T(" && st_envelope(");
      Prefix += Geom2;
      Prefix += _T(") AND ");
   }

   if ((pStr = (TCHAR *) malloc(sizeof (TCHAR) * Len)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   swprintf(pStr, Len, get_InsideSpatialOperator_SQLKeyWord(), 
            Geom1.get_name(), Geom2.get_name());

   Result.paste(pStr);

   return GS_GOOD; 
}


/****************************************************************/
/*.doc C_DBCONNECTION::get_CrossingSpatialOperator_SQLKeyWord <external> */
/*+
  Questa funzione restituisce la parola chiave SQL usata come operatore
  spaziale per cercare gli oggetti totalmente interni e anche parzialmente
  esterni ad una area
  (es. "st_intersects" in PostGIS o "SDO_ANYINTERACT" in ORACLE).
  La parola chiave sarà quindi composta da un parte fissa e una variabile
  es. "st_intersects(%s, %s") in PostGIS o "SDO_ANYINTERACT(%s, %s) = 'TRUE'" in Oracle
  dove %s verrà sostituito run-time.
  -*/  
/****************************************************************/
const TCHAR *C_DBCONNECTION::get_CrossingSpatialOperator_SQLKeyWord()
   { return CrossingSpatialOperator_SQLKeyWord.get_name(); }


/****************************************************************/
/*.doc C_DBCONNECTION::get_CrossingSpatialOperator_SQLSyntax <external> */
/*+
  Questa funzione restituisce la sintassi SQL per cercare gli oggetti 
  totalmente interni e anche parzialmente esterni ad una area
  (es. "&&" in PostGIS o "SDO_ANYINTERACT" in ORACLE).
  La parola chiave sarà quindi composta da un parte fissa e una variabile
  es. "%s && %s" in PostGIS o "SDO_ANYINTERACT(%s, %s) = 'TRUE'" in Oracle
  dove %s verrà sostituito run-time.
-*/  
/****************************************************************/
int C_DBCONNECTION::get_CrossingSpatialOperator_SQLSyntax(C_STRING &Geom1,
                                                          C_STRING &Geom2,
                                                          C_STRING &Result)
{
   TCHAR    *pStr;
   size_t   Len = CrossingSpatialOperator_SQLKeyWord.len() + Geom1.len() + Geom2.len() + 1;
   C_STRING Prefix;

   if (DBMSName.at(PG_DBMSNAME, FALSE) != NULL) // è in PostgreSQL
   {
      Prefix = Geom1;
      Prefix += _T(" && st_envelope(");
      Prefix += Geom2;
      Prefix += _T(") AND ");
   }

   if ((pStr = (TCHAR *) malloc(sizeof (TCHAR) * Len)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   swprintf(pStr, Len, get_CrossingSpatialOperator_SQLKeyWord(), 
            Geom1.get_name(), Geom2.get_name());

   Result.paste(pStr);
   Result.addPrefixSuffix(Prefix.get_name());

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::get_SRIDList_SQLKeyWord       <external> */
/*+
  Questa funzione restituisce l'istruzione SQL per ottenere la 
  lista dei sistemi di coordinate con loro descrizione.
-*/  
/****************************************************************/
const TCHAR *C_DBCONNECTION::get_SRIDList_SQLKeyWord(void)
   { return SRIDList_SQLKeyWord.get_name(); }


/****************************************************************/
/*.doc C_DBCONNECTION::get_SRIDFromField_SQLKeyWord       <external> */
/*+
  Questa funzione restituisce l'istruzione SQL per ottenere il 
  sistema di coordinate di un campo di una tabella.
-*/  
/****************************************************************/
const TCHAR *C_DBCONNECTION::get_SRIDFromField_SQLKeyWord(void)
   { return SRIDFromField_SQLKeyWord.get_name(); }


/****************************************************************/
/*.doc C_DBCONNECTION::get_SRIDFromField_SQLSyntax <external> */
/*+
  Questa funzione restituisce la sintassi SQL per cercare il 
  sistema di coordinate di un campo di una tabella.
  (es. "select srid,coord_dimension from public.geometry_columns where 
  f_table_schema='%s' and f_table_name='%s' and f_geometry_column='%s'" in PostGIS
  o "SELECT SDO_SRID FROM MDSYS.SDO_GEOM_METADATA_TABLE WHERE 
  SDO_OWNER='%s' AND SDO_TABLE_NAME='%s' AND SDO_COLUMN_NAME='%s'" in ORACLE).
  La parola chiave sarà quindi composta da un parte fissa e una variabile
  %s (dove %s verrà sostituito run-time).
-*/  
/****************************************************************/
int C_DBCONNECTION::get_SRIDFromField_SQLSyntax(C_STRING &CatalogSchema, C_STRING &TableName, 
                                                C_STRING &FieldName, C_STRING &Result)
{
   if (SRIDFromField_SQLKeyWord.len() == 0)
      { GS_ERR_COD = eGSInvalidSqlStatement; return GS_BAD; }

   TCHAR  *pStr;
   size_t Len = SRIDFromField_SQLKeyWord.len() + CatalogSchema.len() + TableName.len() + FieldName.len() + 1;

   if ((pStr = (TCHAR *) malloc(sizeof (TCHAR) * Len)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   swprintf(pStr, Len, get_SRIDFromField_SQLKeyWord(), 
            CatalogSchema.get_name(), TableName.get_name(), FieldName.get_name());

   Result.paste(pStr);

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::get_SRIDFromField             <external> */
/*+
  Questa funzione restituisce lo SRID di un campo di una tabella e, se possibile,
  il numero di dimensioni della geometria (2 o 3 o -1 se sconosciuto).
  Parametri:
  C_STRING &CatalogSchema;    Catalogo o schema della tabella
  C_STRING &TableName;        Nome della tabella
  C_STRING &FieldName;        Nome del campo
  C_STRING &SRID;             SRID (out)
  int *NDim;                  NDim (out), opzionale (default = NULL)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::get_SRIDFromField(C_STRING &CatalogSchema, C_STRING &TableName, C_STRING &FieldName,
                                      C_STRING &SRID, int *NDim)
{
   _RecordsetPtr pRs;
   C_RB_LIST     ColValues;
   presbuf       pSRID, pNDim;
   C_STRING      statement, Buffer;

   if (get_SRIDFromField_SQLSyntax(CatalogSchema, TableName, FieldName, 
                                   statement)  == GS_BAD) return GS_BAD;

   if (ExeCmd(statement, pRs) != GS_GOOD) return GS_BAD;

   if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD) { gsc_DBCloseRs(pRs); return GS_BAD; }
   pSRID = gsc_nth(1, ColValues.nth(0));
   pNDim = gsc_nth(1, ColValues.nth(1));

   if (gsc_isEOF(pRs) == GS_GOOD) { gsc_DBCloseRs(pRs); return GS_BAD; }
   if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { gsc_DBCloseRs(pRs); return GS_BAD; }
   gsc_DBCloseRs(pRs);

   SRID.paste(gsc_rb2str(pSRID)); // Converto SRID in stringa
   if (pNDim && NDim) gsc_rb2Int(pNDim, NDim);

   return GS_GOOD; 
}


/****************************************************************/
/*.doc C_DBCONNECTION::get_SRIDList <external> */
/*+
  Questa funzione restituisce la lista dei sistemi di coordinate..
  Parametri:
  C_2STR_LIST &SRID_descr_list;  Lista degli SRID e delle descrizioni

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::get_SRIDList(C_2STR_LIST &SRID_descr_list)
{
   _RecordsetPtr pRs;
   C_RB_LIST     ColValues;
   presbuf       pSRID, pDescr;
   C_2STR        *pSRID_descr;
   C_STRING      Buffer;

   SRID_descr_list.remove_all();
   if (gsc_strlen(get_SRIDList_SQLKeyWord()) == 0) return GS_GOOD;

   if (ExeCmd(SRIDList_SQLKeyWord, pRs) != GS_GOOD) return GS_BAD;

   if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD) { gsc_DBCloseRs(pRs); return GS_BAD; }
   pSRID = gsc_nth(1, ColValues.nth(0));
   pDescr = gsc_nth(1, ColValues.nth(1));

   while (gsc_isEOF(pRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { gsc_DBCloseRs(pRs); return GS_BAD; }
      Buffer.paste(gsc_rb2str(pSRID)); // Converto SRID in stringa
      if (Buffer.len() > 0)
      {
         if ((pSRID_descr = new (C_2STR)) == NULL)
            { gsc_DBCloseRs(pRs); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         
         pSRID_descr->set_name(Buffer.get_name());
         
         if (pDescr->restype == RTSTR) pSRID_descr->set_name2(pDescr->resval.rstring);
         else pSRID_descr->set_name2(_T(""));
         
         SRID_descr_list.add_tail(pSRID_descr);
      }

      gsc_Skip(pRs);
   }

   gsc_DBCloseRs(pRs);

   return GS_GOOD; 
}


/******************************************************************************/
/*.doc C_DBCONNECTION::get_SRIDType                                <external> */
/*+
  Questa funzione restituisce tipo di codifica del sistema di coordinate 
  supportato dal DMBS (GSSRID_Autodesk, GSSRID_EPSG, GSSRID_Oracle, GSSRID_ESRI)
  Parametri:
  GSSRIDTypeEnum &SRIDType;     Tipo di SRID

  Ritorna GS_GOOD in caso di successo, GS_CAN se non è possibile avere questa 
  informazione altrimenti GS_BAD.
-*/
/******************************************************************************/
int C_DBCONNECTION::get_SRIDType(GSSRIDTypeEnum &SRIDType)
{
   if (DBMSName.at(PG_DBMSNAME, FALSE) != NULL) // è in PostgreSQL
      SRIDType = GSSRID_EPSG;
   else if (DBMSName.at(ORACLE_DBMSNAME, FALSE) != NULL) // è in ORACLE
      SRIDType = GSSRID_Oracle;
   else
      return GS_BAD;

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::get_SpatialIndexCreation_SQLKeyWord <external> */
/*+
  Questa funzione restituisce la parola chiave SQL per la creazione
  di un indice spaziale da aggiungere in fondo alla "CREATE INDEX ..."
-*/  
/****************************************************************/
const TCHAR *C_DBCONNECTION::get_SpatialIndexCreation_SQLKeyWord(void)
   { return SpatialIndexCreation_SQLKeyWord.get_name(); }


/****************************************************************/
/*.doc C_DBCONNECTION::get_MaxTolerance2ApproxCurve  <external> */
/*+
  Questa funzione restituisce la distanza massima ammessa tra 
  segmenti retti e la curva quando si approssima una curva (unita autocad).
-*/  
/****************************************************************/
double C_DBCONNECTION::get_MaxTolerance2ApproxCurve(void)
   { return MaxTolerance2ApproxCurve; }


/****************************************************************/
/*.doc C_DBCONNECTION::get_MaxSegmentNum2ApproxCurve <external> */
/*+
  Questa funzione restituisce lil numero massimo 
  dei segmenti retti usati per approssimare una curva.
-*/  
/****************************************************************/
int C_DBCONNECTION::get_MaxSegmentNum2ApproxCurve(void)
   { return MaxSegmentNum2ApproxCurve; }


/****************************************************************/
/*.doc C_DBCONNECTION::get_SQLMaxVertexNum              <external> */
/*+
  Questa funzione restituisce il numero massimo di vertici 
  ammesso per LINESTRING e POLYGON.
-*/  
/****************************************************************/
int C_DBCONNECTION::get_SQLMaxVertexNum(void)
   { return SQLMaxVertexNum; }


/****************************************************************/
/*.doc C_DBCONNECTION::get_IsValidGeom_SQLKeyWord    <external> */
/*+
  Questa funzione restituisce la parola chiave SQL per validare la geometria
  (es. "st_isvalid(%s)" in PostGIS o "SDO_GEOM.VALIDATE_GEOMETRY(%s,0.0001)" in Oracle).
  La parola chiave sarà quindi composta da un parte variabile "%s" che  verrà 
  sostituita run-time da un nome di campo o da una stringa WKT convertita in geometria.
  -*/  
/****************************************************************/
const TCHAR *C_DBCONNECTION::get_IsValidGeom_SQLKeyWord()
   { return IsValidGeom_SQLKeyWord.get_name(); }


/******************************************************************************/
/*.doc C_DBCONNECTION::get_IsValidGeom_SQLSyntax     <external> */
/*+
  Questa funzione restituisce la sintassi SQL per validare una geometria
  (es. "st_isvalid(%s)" in PostGIS o "SDO_GEOM.VALIDATE_GEOMETRY(%s,0.0001)" in ORACLE).
  Parametri:
  C_STRING &Geom;    Stringa rappresentante una geometria
  C_STRING &Result;  Stringa con la sintassi corretta (output)

  Ritorna GS_GOOD in caso di successo, GS_CAN se non è possibile avere questa 
  informazione altrimenti GS_BAD.
-*/
/******************************************************************************/
int C_DBCONNECTION::get_IsValidGeom_SQLSyntax(C_STRING &Geom, C_STRING &Result)
{
   TCHAR  *pStr;
   size_t Len = IsValidGeom_SQLKeyWord.len() + Geom.len() + 1;

   // Non posso sapere se la geometria è valida
   if (IsValidGeom_SQLKeyWord.len() == 0) return GS_CAN;

   if ((pStr = (TCHAR *) malloc(sizeof (TCHAR) * Len)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   swprintf(pStr, Len, get_IsValidGeom_SQLKeyWord(), Geom.get_name());

   Result.paste(pStr);

   return GS_GOOD; 
}


/******************************************************************************/
/*.doc C_DBCONNECTION::get_IsValidWKTGeom_SQLSyntax                <external> */
/*+
  Questa funzione restituisce la sintassi SQL per validare una stringa WKT 
  (es. "st_isvalid(st_geomfromtext('%s',%s))" in PostGIS o 
  "SDO_GEOM.VALIDATE_GEOMETRY(SDO_GEOMETRY('%s',%s),0.0001)" in ORACLE).
  Parametri:
  C_STRING &WKT;        Stringa in formato WKT
  C_STRING &SrcSRID;    Codice del sistema di coordinate sorgente (vuoto = non usato)
  C_STRING &DestSRID;   Codice del SRID di destinazione (vuoto = non usato)
  C_STRING &Result;     Stringa con la sintassi corretta (output)

  Ritorna GS_GOOD in caso di successo, GS_CAN se non è possibile avere questa 
  informazione altrimenti GS_BAD.
-*/
/******************************************************************************/
int C_DBCONNECTION::get_IsValidWKTGeom_SQLSyntax(C_STRING &WKT, C_STRING &SrcSRID, 
                                                 C_STRING &DestSRID, C_STRING &Result)
{
   TCHAR    *pStr;
   C_STRING Geom;

   // Non posso sapere se la geometria è valida
   if (IsValidGeom_SQLKeyWord.len() == 0) return GS_CAN;

   if (get_WKTToGeom_SQLSyntax(WKT, SrcSRID, DestSRID, Geom) == GS_BAD) return GS_BAD;

   size_t Len = IsValidGeom_SQLKeyWord.len() + Geom.len() + 1;

   if ((pStr = (TCHAR *) malloc(sizeof (TCHAR) * Len)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   swprintf(pStr, Len, get_IsValidGeom_SQLKeyWord(), Geom.get_name());

   Result = _T("SELECT ");
   Result += pStr;

   if (DBMSName.at(ORACLE_DBMSNAME, FALSE) != NULL) // è in ORACLE
      Result += _T(" FROM DUAL");

   free(pStr);

   return GS_GOOD; 
}


/******************************************************************************/
/*.doc C_DBCONNECTION::get_IsValidReasonGeom_SQLKeyWord            <external> */
/*+
  Questa funzione restituisce la parola chiave SQL per conoscere il motivo per cui
  la geometria non è valida (es. "st_isvalidreason(%s)" in PostGIS).
  La parola chiave sarà quindi composta da un parte variabile "%s" che  verrà 
  sostituita run-time da un nome di campo o da una stringa WKT convertita in geometria.
  -*/  
/****************************************************************/
const TCHAR *C_DBCONNECTION::get_IsValidReasonGeom_SQLKeyWord()
   { return IsValidReasonGeom_SQLKeyWord.get_name(); }


/****************************************************************/
/*.doc C_DBCONNECTION::get_IsValidReasonGeom_SQLSyntax     <external> */
/*+
  Questa funzione restituisce la sintassi SQL per validare una geometria
  (es. "st_isvalidreason(%s)" in PostGIS o "SDO_GEOM.VALIDATE_GEOMETRY(%s,0.0001)" in ORACLE).
  Parametri:
  C_STRING &Geom;    Stringa rappresentante una geometria
  C_STRING &Result;  Stringa con la sintassi corretta (output)

  Ritorna GS_GOOD in caso di successo, GS_CAN se non è possibile avere questa 
  informazione altrimenti GS_BAD.
-*/
/****************************************************************/
int C_DBCONNECTION::get_IsValidReasonGeom_SQLSyntax(C_STRING &Geom, C_STRING &Result)
{
   TCHAR  *pStr;
   size_t Len = IsValidReasonGeom_SQLKeyWord.len() + Geom.len() + 1;

   // Non posso sapere la ragione della non validità
   if (IsValidReasonGeom_SQLKeyWord.len() == 0) return GS_CAN;

   if ((pStr = (TCHAR *) malloc(sizeof (TCHAR) * Len)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   swprintf(pStr, Len, get_IsValidReasonGeom_SQLKeyWord(), Geom.get_name());

   Result.paste(pStr);

   return GS_GOOD; 
}


/******************************************************************************/
/*.doc C_DBCONNECTION::get_IsValidReasonWKTGeom_SQLSyntax          <external> */
/*+
  Questa funzione restituisce la sintassi SQL per validare una geometria
  (es. "st_isvalidreason(st_geomfromtext('%s',%s))" in PostGIS o
  "SDO_GEOM.VALIDATE_GEOMETRY(SDO_GEOMETRY('%s',%s),0.0001)" in ORACLE).
  Parametri:
  C_STRING &WKT;        Stringa in formato WKT
  C_STRING &SrcSRID;    Codice del sistema di coordinate sorgente (vuoto = non usato)
  C_STRING &DestSRID;   Codice del SRID di destinazione (vuoto = non usato)
  C_STRING &Result;     Stringa con la sintassi corretta (output)

  Ritorna GS_GOOD in caso di successo, GS_CAN se non è possibile avere questa 
  informazione altrimenti GS_BAD.
-*/
/******************************************************************************/
int C_DBCONNECTION::get_IsValidReasonWKTGeom_SQLSyntax(C_STRING &WKT, C_STRING &SrcSRID, 
                                                       C_STRING &DestSRID, C_STRING &Result)
{
   TCHAR *pStr;
   C_STRING Geom;

   // Non posso sapere la ragione della non validità
   if (IsValidReasonGeom_SQLKeyWord.len() == 0) return GS_CAN;

   if (get_WKTToGeom_SQLSyntax(WKT, SrcSRID, DestSRID, Geom) == GS_BAD) return GS_BAD;

   size_t Len = IsValidReasonGeom_SQLKeyWord.len() + Geom.len() + 1;

   if ((pStr = (TCHAR *) malloc(sizeof (TCHAR) * Len)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   swprintf(pStr, Len, get_IsValidReasonGeom_SQLKeyWord(), Geom.get_name());

   Result = _T("select ");
   Result += pStr;

   if (DBMSName.at(ORACLE_DBMSNAME, FALSE) != NULL) // è in ORACLE
      Result += _T(" from dual");

   free(pStr);

   return GS_GOOD; 
}


/******************************************************************************/
/*.doc C_DBCONNECTION::get_CoordTransform_SQLKeyWord               <external> */
/*+
  Questa funzione restituisce la parola chiave SQL per trasformare le 
  coordinate della geometria in un SRID (es. "st_transform(%s, %s)" in PostGIS).
  La parola chiave sarà quindi composta da due parti variabili "%s" che  verranno
  sostituite run-time da un nome di campo o da una stringa WKT convertita in geometria
  e dal codice SRID di destinazione.
  -*/  
/****************************************************************/
const TCHAR *C_DBCONNECTION::get_CoordTransform_SQLKeyWord()
   { return CoordTransform_SQLKeyWord.get_name(); }


/****************************************************************/
/*.doc C_DBCONNECTION::get_CoordTransform_SQLSyntax  <external> */
/*+
  Questa funzione restituisce la sintassi SQL per trasformare le coordinate di una geometria
  (es. "st_transform(%s, %s)" in PostGIS o "SDO_CS.TRANSFORM(%s, %s)" in ORACLE).
  Parametri:
  C_STRING &Geom;    Stringa rappresentante una geometria
  C_STRING &SRID;    Stringa rappresentante il codice SRID di destinazione
  C_STRING &Result;  Stringa con la sintassi corretta (output)

  Ritorna GS_GOOD in caso di successo, GS_CAN se non è possibile avere questa 
  informazione altrimenti GS_BAD.
-*/
/****************************************************************/
int C_DBCONNECTION::get_CoordTransform_SQLSyntax(C_STRING &Geom, C_STRING &SRID, C_STRING &Result)
{
   TCHAR  *pStr;
   size_t Len = IsValidReasonGeom_SQLKeyWord.len() + Geom.len() + SRID.len() + 1;

   // Non posso convertire le coordinate
   if (CoordTransform_SQLKeyWord.len() == 0) return GS_CAN;

   if ((pStr = (TCHAR *) malloc(sizeof (TCHAR) * Len)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   swprintf(pStr, Len, get_CoordTransform_SQLKeyWord(), Geom.get_name(), SRID.get_name());

   Result.paste(pStr);

   return GS_GOOD; 
}


/****************************************************************/
/*.doc C_DBCONNECTION::IsUsable                      <external> */
/*+
  Questa funzione restituisce GS_GOOD se la connessione è compatibile
  con la stringa di connessione specificata. In caso positivo sarà 
  possibile utilizzare la connessione, altrimenti si dovrà creare 
  una nuova connessione.
  Parametri:
  const TCHAR *ConnStr;     Stringa di connessione al database

  Ritorna GS_GOOD se la connessione è utilizzabile.
-*/  
/****************************************************************/
int C_DBCONNECTION::IsUsable(const TCHAR *ConnStr)
{
   C_2STR_LIST PropList, VerifPropList;
   C_2STR      *pProp, *pVerifProp;
   TCHAR       *PropName;

   if (gsc_PropListFromConnStr(get_OrigConnectionStr(), PropList) == GS_BAD) return GS_BAD;
   if (gsc_PropListFromConnStr(ConnStr, VerifPropList) == GS_BAD) return GS_BAD;

   // Se non supporta l'uso del catalogo
   if (get_CatalogUsage() == 0)
   {  // Tutte le proprietà devono coincidere
      if (PropList.get_count() != VerifPropList.get_count() ||
          gsc_CheckPropList(PropList, VerifPropList) == GS_BAD) return GS_BAD;
      else return GS_GOOD;
   }

   PropName = gsc_strstr(get_CatalogUDLProperty(), _T("[Extended Properties]"), FALSE);
   if (PropName && PropName == get_CatalogUDLProperty())
   {  // il catalogo è rappresentato da una proprietà in "Extended Properties"
      PropName += wcslen(_T("[Extended Properties]"));
      // se le liste sono uguali (esclusa "Extended Properties")
      if (gsc_CheckPropList(PropList, VerifPropList, _T("Extended Properties")) == GS_GOOD)
      {
         if ((pProp = (C_2STR *) PropList.search_name(_T("Extended Properties"), FALSE)) &&
             (pVerifProp = (C_2STR *) VerifPropList.search_name(_T("Extended Properties"), FALSE)))
         {
            if (gsc_PropListFromExtendedPropConnStr(pProp->get_name2(), PropList) == GS_BAD ||
                gsc_PropListFromExtendedPropConnStr(pVerifProp->get_name2(), VerifPropList) == GS_BAD)
               return GS_BAD;
            // se le liste differiscono per almeno una delle proprietà (esclusa PropName)
            if (gsc_CheckPropList(PropList, VerifPropList, PropName) == GS_BAD) return GS_BAD;
         }
      }
      else return GS_BAD;
   }
   else
      // se le liste differiscono per almeno una delle proprietà
      // (esclusa get_CatalogUDLProperty())
      if (gsc_CheckPropList(PropList, VerifPropList, get_CatalogUDLProperty()) == GS_BAD)
         return GS_BAD;

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::BeginTrans                    <external> */
/*+
  Questa funzione inizia una nuova transazione.
  Parametri:
  long *pTransLevel;  Livello di nidificazione della transazione (default = NULL)
 
  Ritorna GS_GOOD in caso di successo, GS_CAN se la connessione non supporta
  questa funzionalità altrimenti GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::BeginTrans(long *pTransLevel)
{
   //return GS_CAN; // roby test

   // Tutti i driver che utilizzano tabelle i forma di file
   // non supportano l'uso delle transazioni (vedi MERANT che comunque non dà errore)
   if (get_CatalogResourceType() == DirectoryRes) return GS_CAN;

   try
   {
      if (pTransLevel) *pTransLevel = pConn->BeginTrans();
      else pConn->BeginTrans();
   }

   catch (_com_error &e)
	{
      HRESULT err = e.Error();

      // codice errore per "L'operazione richiesta dall'applicazione non è supportata dal provider"
      if (err == -2146825037) return GS_CAN;
      // codice errore per "Un'integrazione in una transazione è già presente" (PostgreSQL)
      if (err == -2147168237) return GS_CAN;
      printDBErrs(e, pConn);
      return GS_BAD;
   }

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::CommitTrans                   <external> */
/*+
  Questa funzione salva tutte le modifiche effettuate alla banca dati
  e termina la transazione corrente.
 
  Ritorna GS_GOOD in caso di successo, GS_CAN se la connessione non supporta
  questa funzionalità altrimenti GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::CommitTrans(void)
{
   HRESULT hr = E_FAIL;

   //return GS_CAN; // roby test

   // Tutti i driver che utilizzano tabelle i forma di file
   // non supportano l'uso delle transazioni (vedi MERANT che comunque non dà errore)
   if (get_CatalogResourceType() == DirectoryRes) return GS_CAN;

   try
   {
      if (FAILED(hr = pConn->CommitTrans())) _com_issue_error(hr);    
   }

   catch (_com_error &e)
	{
      // codice errore per "L'operazione richiesta dall'applicazione non è supportata dal provider"
      if (e.Error() == -2146825037) return GS_CAN;
      printDBErrs(e, pConn);
      return GS_BAD;
   }

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::RollbackTrans                 <external> */
/*+
  Questa funzione cancella tutte le modifiche effettuate alla banca dati
  e termina la transazione corrente.
 
  Ritorna GS_GOOD in caso di successo, GS_CAN se la connessione non supporta
  questa funzionalità altrimenti GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::RollbackTrans(void)
{
   HRESULT hr = E_FAIL;

   //return GS_CAN; // roby test

   // Tutti i driver che utilizzano tabelle i forma di file
   // non supportano l'uso delle transazioni (vedi MERANT che comunque non dà errore)
   if (get_CatalogResourceType() == DirectoryRes) return GS_CAN;

   try
   {
      if (FAILED(hr = pConn->RollbackTrans())) _com_issue_error(hr);    
   }

   catch (_com_error &e)
	{
      // codice errore per "L'operazione richiesta dall'applicazione non è supportata dal provider"
      if (e.Error() == -2146825037) return GS_CAN;
      printDBErrs(e, pConn);
      return GS_BAD;
   }

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadCatProperty               <internal> */
/*+
  Questa funzione legge, da un file testo, alcune caratteristiche 
  riguardanti l'uso del catalogo nella connessione OLE-DB.
  In particolare viene letto:
  - Carattere separatore di catalogo
  - Tipo di risorsa rappresentata dal catalogo
  - Proprietà UDL che rappresenta il catalogo
  
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ReadCatProperty(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   C_STRING ResourceType, Description;

   // Leggo le caratteristiche del catalogo (importante: "Catalog" è case sensitive)
   if (gsc_getTablePartInfo(ProfileSections, _T("Catalog"), &CatalogSeparator,
                            CatalogUDLProperty, ResourceType, Description) == GS_BAD)
      return GS_BAD;

   if (ResourceType.len() > 0)
      CatalogResourceType = gsc_DBstr2CatResType(ResourceType.get_name());
   else 
      CatalogResourceType = EmptyRes;

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadQuotedIdentifiers         <internal> */
/*+
  Questa funzione legge, da un file testo, alcune caratteristiche 
  riguardanti l'uso dei QuotedIdentifiers nella connessione OLE-DB.
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ReadQuotedIdentifiers(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   C_BPROFILE_SECTION *ProfileSection;
   C_2STR_BTREE       *pProfileEntries;
   C_B2STR            *pProfileEntry;
   
   if (!(ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(_T("GEOsim"))))
      return GS_GOOD;
   pProfileEntries = (C_2STR_BTREE *) ProfileSection->get_ptr_EntryList();

   // leggo proprietà "InitQuotedIdentifier"
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("Init Quoted Identifiers"))))
      InitQuotedIdentifier = *(gsc_alltrim(pProfileEntry->get_name2()));
   // leggo proprietà "FinalQuotedIdentifier"
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("Final Quoted Identifiers"))))
      FinalQuotedIdentifier = *(gsc_alltrim(pProfileEntry->get_name2()));

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadWildcards                 <internal> */
/*+
  Questa funzione legge, da un file testo, alcune caratteristiche 
  riguardanti l'uso dei wildcards (caratteri jolly) nella connessione OLE-DB.
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ReadWildcards(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   C_BPROFILE_SECTION *ProfileSection;
   C_2STR_BTREE       *pProfileEntries;
   C_B2STR            *pProfileEntry;
   
   if (!(ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(_T("GEOsim"))))
      return GS_GOOD;
   pProfileEntries = (C_2STR_BTREE *) ProfileSection->get_ptr_EntryList();

   // leggo proprietà "MultiCharWildcard_SQLKeyWord"
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("MultiCharWildcard_SQLKeyWord"))))
      MultiCharWildcard = *(gsc_alltrim(pProfileEntry->get_name2()));
   // leggo proprietà "SingleCharWildcard_SQLKeyWord"
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("SingleCharWildcard_SQLKeyWord"))))
      SingleCharWildcard = *(gsc_alltrim(pProfileEntry->get_name2()));

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadMaxTableNameLen           <internal> */
/*+
  Questa funzione legge, da un file testo, la lunghezza massima del
  nome della tabella nella connessione OLE-DB.
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ReadMaxTableNameLen(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   C_B2STR *pProfileEntry;

   // leggo proprietà "MaxTableNameLen"
   if ((pProfileEntry = ProfileSections.get_entry(_T("GEOsim"), _T("MaxTableNameLen"))))
      MaxTableNameLen = _wtoi(pProfileEntry->get_name2());

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadMaxFieldNameLen           <internal> */
/*+
  Questa funzione legge, da un file testo, la lunghezza massima del
  campo di una tabella nella connessione OLE-DB.
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ReadMaxFieldNameLen(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   C_B2STR *pProfileEntry;

   // leggo proprietà "MaxFieldNameLen"
   if ((pProfileEntry = ProfileSections.get_entry(_T("GEOsim"), _T("MaxFieldNameLen"))))
      MaxFieldNameLen = _wtoi(pProfileEntry->get_name2());

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadStrCaseFullTableRef       <internal> */
/*+
  Questa funzione legge, da un file testo, se il riferimento completo
  di una tabella deve essere indicato in maiuscolo.
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ReadStrCaseFullTableRef(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   C_B2STR *pProfileEntry;

   // leggo proprietà "StrCase_SQLFullTableRef"
   if ((pProfileEntry = ProfileSections.get_entry(_T("GEOsim"), _T("StrCase_SQLFullTableRef"))))
      if (gsc_strcmp(pProfileEntry->get_name2(), _T("UPPER"), FALSE) == 0)
         StrCaseFullTableRef = Upper;
      else if (gsc_strcmp(pProfileEntry->get_name2(), _T("LOWER"), FALSE) == 0)
         StrCaseFullTableRef = Lower;
      else
         StrCaseFullTableRef = NoSensitive;

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadInvalidFieldNameCharacters <internal> */
/*+
  Questa funzione legge, da un file testo, i caratteri non validi
  per i nomi dei campi di una tabella.
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ReadInvalidFieldNameCharacters(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   C_B2STR *pProfileEntry;

   // leggo proprietà "StrCase_SQLFullTableRef"
   if ((pProfileEntry = ProfileSections.get_entry(_T("GEOsim"), _T("InvalidFieldNameCharacters"))))
      InvalidFieldNameCharacters = pProfileEntry->get_name2();
   else
      InvalidFieldNameCharacters.clear();

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadEscapeStringSupported     <internal> */
/*+
  Questa funzione legge, da un file testo, se i carattere di
  escape sono supportati nei valori carattere dei campi.
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ReadEscapeStringSupported(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   C_B2STR *pProfileEntry;

   // leggo proprietà "EscapeStringSupported"
   if (!(pProfileEntry = ProfileSections.get_entry(_T("GEOsim"), _T("EscapeStringSupported"))))
      EscapeStringSupported = false;
   else
      EscapeStringSupported = (_wtoi(pProfileEntry->get_name2()) == 1) ? true : false;

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadRowLockSuffix_SQLKeyWord  <internal> */
/*+
  Questa funzione legge, da un file testo, la parola chiave SQL
  da aggiungere in fondo all'istruzione SELECT...per bloccare le righe
  (es. per PostgreSQL "FOR UPDATE NOWAIT" in una transazione).
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ReadRowLockSuffix_SQLKeyWord(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   C_B2STR *pProfileEntry;

   // leggo proprietà "RowLockSuffix_SQLKeyWord"
   if ((pProfileEntry = ProfileSections.get_entry(_T("GEOsim"), _T("RowLockSuffix_SQLKeyWord"))))
      RowLockSuffix_SQLKeyWord = pProfileEntry->get_name2();
   else
      RowLockSuffix_SQLKeyWord.clear();

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadIsMultiUser               <internal> */
/*+
  Questa funzione legge, da un file testo, se il DB è gestito in
  multiutenza o monoutenza (es. EXCEL).
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ReadIsMultiUser(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   C_B2STR *pProfileEntry;

   // leggo proprietà "IsMultiUser"
   if ((pProfileEntry = ProfileSections.get_entry(_T("GEOsim"), _T("IsMultiUser"))))
      if (gsc_strcmp(pProfileEntry->get_name2(), _T("1")) == 0  ||
          gsc_strcmp(pProfileEntry->get_name2(), _T("TRUE"), FALSE) == 0)
         IsMultiUser = TRUE;
      else
         IsMultiUser = FALSE;

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadIsUniqueIndexName         <internal> */
/*+
  Questa funzione legge, da un file testo, se il nome degli indici
  sono univoci nello stesso schema (nome tabella diverso da nome 
  indice es. ORACLE e PostgreSQL).
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ReadIsUniqueIndexName(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   C_B2STR *pProfileEntry;

   // leggo proprietà "IsMultiUser"
   if ((pProfileEntry = ProfileSections.get_entry(_T("GEOsim"), _T("IsUniqueIndexName"))))
      if (gsc_strcmp(pProfileEntry->get_name2(), _T("1")) == 0  ||
          gsc_strcmp(pProfileEntry->get_name2(), _T("TRUE"), FALSE) == 0)
         IsUniqueIndexName = TRUE;
      else
         IsUniqueIndexName = FALSE;

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadGeomTypeDescrList         <internal> */
/*+
  Questa funzione legge, da un file testo, la lista di tipi 
  geometrici GEOsim associati alla descrizione SQL usata dal provider
  (es. (TYPE_POLYLINE "LINESTRING") in PostGIS)
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ReadGeomTypeDescrList(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   C_B2STR *pProfileEntry;

   // leggo proprietà "GeomTypeDescrList"
   if ((pProfileEntry = ProfileSections.get_entry(_T("GEOsim"), _T("GeomTypeDescrList"))))
      if (GeomTypeDescrList.from_str(pProfileEntry->get_name2()) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadGeomToWKT_SQLKeyWord      <internal> */
/*+
  Questa funzione legge, da un file testo, la parola chiave SQL 
  per trasformare testo in geometria (es. "st_geomfromtext" in PostGIS)
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ReadGeomToWKT_SQLKeyWord(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   C_B2STR *pProfileEntry;

   // leggo proprietà "GeomToWKT_SQLKeyWord"
   if ((pProfileEntry = ProfileSections.get_entry(_T("GEOsim"), _T("GeomToWKT_SQLKeyWord"))))
      GeomToWKT_SQLKeyWord = pProfileEntry->get_name2();

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadWKTToGeom_SQLKeyWord      <internal> */
/*+
  Questa funzione legge, da un file testo, la parola chiave SQL 
  per trasformare geometria in testo (es. "st_asewkt" in PostGIS)
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ReadWKTToGeom_SQLKeyWord(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   C_B2STR *pProfileEntry;

   // leggo proprietà "WKTToGeom_SQLKeyWord"
   if ((pProfileEntry = ProfileSections.get_entry(_T("GEOsim"), _T("WKTToGeom_SQLKeyWord"))))
      WKTToGeom_SQLKeyWord = gsc_alltrim(pProfileEntry->get_name2());

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadInsideSpatialOperator_SQLKeyWord <internal> */
/*+
  Questa funzione legge, da un file testo, la parola chiave SQL 
  usata come operatore spaziale per cercare gli oggetti totalmente interni
  ad una area (es. "within" in PostGIS e "SDO_INSIDE" in Oracle)
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ReadInsideSpatialOperator_SQLKeyWord(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   C_B2STR *pProfileEntry;

   // leggo proprietà "InsideSpatialOperator_SQLKeyWord"
   if ((pProfileEntry = ProfileSections.get_entry(_T("GEOsim"), _T("InsideSpatialOperator_SQLKeyWord"))))
      InsideSpatialOperator_SQLKeyWord = pProfileEntry->get_name2();

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadCrossingSpatialOperator_SQLKeyWord <internal> */
/*+
  Questa funzione legge, da un file testo, la parola chiave SQL 
  usata come operatore spaziale per cercare gli oggetti totalmente interni
  e anche parzialmente esterni ad una area (es. " st_intersects " in PostGIS e 
  "SDO_ANYINTERACT" in Oracle)
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ReadCrossingSpatialOperator_SQLKeyWord(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   C_B2STR *pProfileEntry;

   // leggo proprietà "CrossingSpatialOperator_SQLKeyWord"
   if ((pProfileEntry = ProfileSections.get_entry(_T("GEOsim"), _T("CrossingSpatialOperator_SQLKeyWord"))))
      CrossingSpatialOperator_SQLKeyWord = pProfileEntry->get_name2();

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadSRIDList_SQLKeyWord       <internal> */
/*+
  Questa funzione legge, da un file testo, l'istruzione SQL 
  per ottenere la lista dei sistemi di coordinate con loro
  descrizione.
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ReadSRIDList_SQLKeyWord(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   C_B2STR *pProfileEntry;

   // leggo proprietà "SRIDList_SQLKeyWord"
   if ((pProfileEntry = ProfileSections.get_entry(_T("GEOsim"), _T("SRIDList_SQLKeyWord"))))
      SRIDList_SQLKeyWord = pProfileEntry->get_name2();

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadSRIDFromField_SQLKeyWord  <internal> */
/*+
  Questa funzione legge, da un file testo, l'istruzione SQL 
  per ottenere il sistema di coordinate di un campo di una tabella.
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ReadSRIDFromField_SQLKeyWord(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   C_B2STR *pProfileEntry;

   // leggo proprietà "SRIDFromField_SQLKeyWord"
   if ((pProfileEntry = ProfileSections.get_entry(_T("GEOsim"), _T("SRIDFromField_SQLKeyWord"))))
      SRIDFromField_SQLKeyWord = pProfileEntry->get_name2();

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadSpatialIndexCreation_SQLKeyWord <internal> */
/*+
  Questa funzione legge, da un file testo, l'istruzione SQL 
  per ottenere la lista dei sistemi di coordinate con loro
  descrizione.
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ReadSpatialIndexCreation_SQLKeyWord(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   C_B2STR *pProfileEntry;

   // leggo proprietà "SpatialIndexCreation_SQLKeyWord"
   if ((pProfileEntry = ProfileSections.get_entry(_T("GEOsim"), _T("SpatialIndexCreation_SQLKeyWord"))))
      SpatialIndexCreation_SQLKeyWord = pProfileEntry->get_name2();

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadMaxTolerance2ApproxCurve  <internal> */
/*+
  Questa funzione legge, da un file testo, la distanza massima 
  ammessa tra segmenti retti e la curva (unita autocad).
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ReadMaxTolerance2ApproxCurve(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   C_B2STR *pProfileEntry;

   // leggo proprietà "MaxTolerance2ApproxCurve"
   if ((pProfileEntry = ProfileSections.get_entry(_T("GEOsim"), _T("MaxTolerance2ApproxCurve"))))
      if (_wtof(pProfileEntry->get_name2()) > 0 )
         MaxTolerance2ApproxCurve = _wtof(pProfileEntry->get_name2());

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadMaxSegmentNum2ApproxCurve <internal> */
/*+
  Questa funzione legge, da un file testo, il numero massimo 
  dei segmenti retti usati per approssimare una curva.
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ReadMaxSegmentNum2ApproxCurve(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   C_B2STR *pProfileEntry;

   // leggo proprietà "MaxSegmentNum2ApproxCurve"
   if ((pProfileEntry = ProfileSections.get_entry(_T("GEOsim"), _T("MaxSegmentNum2ApproxCurve"))))
      if (_wtoi(pProfileEntry->get_name2()) > 0 )
         MaxSegmentNum2ApproxCurve = _wtoi(pProfileEntry->get_name2());

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadSQLMaxVertexNum              <internal> */
/*+
  Questa funzione legge, da un file testo, il numero massimo 
  di vertici ammesso per LINESTRING e POLYGON.
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ReadSQLMaxVertexNum(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   C_B2STR *pProfileEntry;

   // leggo proprietà "SQLMaxVertexNum"
   if ((pProfileEntry = ProfileSections.get_entry(_T("GEOsim"), _T("SQLMaxVertexNum"))))
      if (_wtoi(pProfileEntry->get_name2()) > 0 )
         SQLMaxVertexNum = _wtoi(pProfileEntry->get_name2());

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadIsValidGeom_SQLKeyWord    <internal> */
/*+
  Questa funzione legge, da un file testo, la parola chiave SQL 
  per verificare la validità della geometria (es. "st_isvalid" in PostGIS,
  "SDO_GEOM.VALIDATE_GEOMETRY" in Oracle)
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ReadIsValidGeom_SQLKeyWord(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   C_B2STR *pProfileEntry;

   // leggo proprietà "IsValidGeom_SQLKeyWord"
   if ((pProfileEntry = ProfileSections.get_entry(_T("GEOsim"), _T("IsValidGeom_SQLKeyWord"))))
      IsValidGeom_SQLKeyWord = gsc_alltrim(pProfileEntry->get_name2());

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadIsValidReasonGeom_SQLKeyWord <internal> */
/*+
  Questa funzione legge, da un file testo, la parola chiave SQL 
  per sapere il motivo per cui la geometria non è valida 
  (es. "st_isvalidreason" in PostGIS).
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ReadIsValidReasonGeom_SQLKeyWord(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   C_B2STR *pProfileEntry;

   // leggo proprietà "IsValidReasonGeom_SQLKeyWord"
   if ((pProfileEntry = ProfileSections.get_entry(_T("GEOsim"), _T("IsValidReasonGeom_SQLKeyWord"))))
      IsValidReasonGeom_SQLKeyWord = gsc_alltrim(pProfileEntry->get_name2());

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadCoordTransform_SQLKeyWord <internal> */
/*+
  Questa funzione legge, da un file testo, la parola chiave SQL 
  per sapere convertire le coordinate di una geometria in SRID noto
  (es. "st_transform" in PostGIS).
  Parametri: 
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ReadCoordTransform_SQLKeyWord(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   C_B2STR *pProfileEntry;

   // leggo proprietà "IsValidReasonGeom_SQLKeyWord"
   if ((pProfileEntry = ProfileSections.get_entry(_T("GEOsim"), _T("CoordTransform_SQLKeyWord"))))
      CoordTransform_SQLKeyWord = gsc_alltrim(pProfileEntry->get_name2());

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::get_FullRefTable               <internal> */
/*+
  Questa funzione dato un catalogo, uno schema ed una tabella restituisce
  il riferimento completo alla tabella.
  Parametri: 
  const TCHAR *Catalog;            Catalogo (se = NULL e la connessione lo
                                   permette viene usato il catalogo corrente)
  const TCHAR *Schema;             Schema (se = NULL e la connessione lo
                                   permette viene usato lo schema corrente)
  const TCHAR *Table;              Nome della tabella
  CatSchUsageTypeEnum CatSchUsage; Uso che si vuole fare della tabella (default = DML)
  const TCHAR *TableParams;        Opzionale, Parametri della tabella (es. history con funzione parametrica)
                                   (default = NULL)
  
  Restituisce una stringa in caso di successo altrimenti restituisce NULL.
  N.B. Alloca memoria
-*/  
/****************************************************************/
TCHAR *C_DBCONNECTION::get_FullRefTable(C_STRING &Catalog, C_STRING &Schema,
                                        C_STRING &Table, CatSchUsageTypeEnum CatSchUsage,
                                        C_STRING *pTableParams)
   { return get_FullRefTable(Catalog.get_name(), Schema.get_name(), Table.get_name(), CatSchUsage,
                             (pTableParams) ? pTableParams->get_name() : NULL); }
TCHAR *C_DBCONNECTION::get_FullRefTable(const TCHAR *Catalog, const TCHAR *Schema,
                                       const TCHAR *Table, CatSchUsageTypeEnum CatSchUsage,
                                       const TCHAR *TableParams)
{
   switch (CatSchUsage)
   {
      case DML:
         return get_FullRefSQL(Catalog, DBPROPVAL_CU_DML_STATEMENTS,
                               Schema, DBPROPVAL_SU_DML_STATEMENTS, Table, TableParams);
      case Definition:
         return get_FullRefSQL(Catalog, DBPROPVAL_CU_TABLE_DEFINITION,
                               Schema, DBPROPVAL_SU_TABLE_DEFINITION, Table, TableParams);
      case Privilege:
         return get_FullRefSQL(Catalog, DBPROPVAL_CU_PRIVILEGE_DEFINITION,
                               Schema, DBPROPVAL_SU_PRIVILEGE_DEFINITION, Table, TableParams);
      default:
         return NULL;
   }
}


/****************************************************************/
/*.doc C_DBCONNECTION::get_FullRefIndex              <internal> */
/*+
  Questa funzione dato un catalogo, uno schema ed un indice restituisce
  il riferimento completo all'indice.
  Parametri: 
  const TCHAR *Catalog;            Catalogo (se = NULL e la connessione lo
                                   permette viene usato il catalogo corrente)
  const TCHAR *Schema;             Schema (se = NULL e la connessione lo
                                   permette viene usato lo schema corrente)
  const TCHAR *Index;              Nome dell'indice
  CatSchUsageTypeEnum CatSchUsage; Uso che si vuole fare dell'indice (default = DML)
  short Reason;                    Flag per sapere quale operazione si intende
                                   fare sugli indici. INSERT se si vuole creare 
                                   un indice, ERASE se si vuole cancellare un indice
                                   NONE negli altri casi (default NONE). Il falg è stato
                                   creato per questi DBMS (es. PostgreSQL) che in 
                                   creazione vogliono solo il nome dell'indice mentre
                                   in cancellazione vogliono anche lo schema.
  
  Restituisce una stringa in caso di successo altrimenti restituisce NULL.
  N.B. Alloca memoria
-*/  
/****************************************************************/
TCHAR *C_DBCONNECTION::get_FullRefIndex(const TCHAR *Catalog, const TCHAR *Schema,
                                       const TCHAR *Index, CatSchUsageTypeEnum CatSchUsage,
                                       short Reason)
{
   C_STRING ProviderName;
   bool     NoCatalogSchema;

   // Chi NON usa il DBMS di ORACLE o POSTGRESQL non utilizza catalogo e 
   // schema per gli indici

   if (DBMSName.at(ORACLE_DBMSNAME, FALSE) != NULL) // è in ORACLE
      NoCatalogSchema = false;
   else if (DBMSName.at(PG_DBMSNAME, FALSE) != NULL)
      // Se si tratta di PostgreSQL si usa lo schema solo 
      // se si vuole cancellare l'indice
      NoCatalogSchema = (Reason == ERASE) ? false : true;
   else
      NoCatalogSchema = true;

   if (NoCatalogSchema)
   {
      ProviderName = Index;
      if (gsc_AdjSyntax(ProviderName, get_InitQuotedIdentifier(), get_FinalQuotedIdentifier(),
                        get_StrCaseFullTableRef()) == GS_BAD)
         return GS_BAD;
      return gsc_tostring(ProviderName.get_name());
   }

   switch (CatSchUsage)
   {
      case DML:
         return get_FullRefSQL(Catalog, DBPROPVAL_CU_DML_STATEMENTS,
                               Schema, DBPROPVAL_SU_DML_STATEMENTS, Index);
      case Definition:
         return get_FullRefSQL(Catalog, DBPROPVAL_CU_INDEX_DEFINITION,
                               Schema, DBPROPVAL_SU_INDEX_DEFINITION, Index);
      case Privilege:
         return get_FullRefSQL(Catalog, DBPROPVAL_CU_PRIVILEGE_DEFINITION,
                               Schema, DBPROPVAL_SU_PRIVILEGE_DEFINITION, Index);
      default:
         return NULL;
   }
}


/****************************************************************/
/*.doc C_DBCONNECTION::get_FullRefSQL                <internal> */
/*+
  Questa funzione dato un catalogo, uno schema e un componente SQL 
  (<tabella>\<indice>\<definizione di privilegi>), restituisce
  il riferimento completo al componente SQL.
  Parametri: 
  const TCHAR *Catalog;   Catalogo (se = NULL e la connessione lo
                          permette viene usato il catalogo corrente)
  int        CatCheck;    Flag di riferimento per uso o meno del catalogo
  const TCHAR *Schema;    Schema (se = NULL e la connessione lo
                          permette viene usato lo schema corrente)
  int        SchCheck;    Flag di riferimento per uso o meno dello schema
  const TCHAR *Component; Nome del componente SQL (<tabella>\<indice>\
                          <definizione di privilegi>)
  const TCHAR *ComponentParams; Opzionale, Parametri del componente (es. history con funzione parametrica)
                                (default = NULL)
  
  Restituisce una stringa in caso di successo altrimenti restituisce NULL.
  N.B. Alloca memoria
-*/  
/****************************************************************/
TCHAR *C_DBCONNECTION::get_FullRefSQL(const TCHAR *Catalog, int CatCheck,
                                      const TCHAR *Schema, int SchCheck,
                                      const TCHAR *Component, const TCHAR *ComponentParams)
{
   Property   *pResult;
   _variant_t PropName;
   C_STRING   FullRefTable, Cat, Sch, Tab;

   if (pConn.GetInterfacePtr() == NULL) return NULL;

   if (!Component) { GS_ERR_COD = eGSInvalidArg; return NULL; }
   Tab = Component;
   if (gsc_AdjSyntax(Tab, get_InitQuotedIdentifier(), get_FinalQuotedIdentifier(),
                     get_StrCaseFullTableRef()) == GS_BAD)
      return NULL;
   Tab += ComponentParams;

   if (get_CatalogUsage() & CatCheck) // catalogo supportato
   {
      TCHAR *pStr;

      if (!Catalog) // se non è stato passato uso quello corrente
      {
         gsc_set_variant_t(PropName, _T("Current Catalog"));
         if (pConn->Properties->get_Item(PropName, &pResult) == S_OK) // se esiste la proprietà
            Cat = (TCHAR *) _bstr_t (pResult->GetValue());
      }
      else Cat = Catalog;

      switch (get_CatalogResourceType())
      {
         case DirectoryRes:
         {  
            // Il riferimento completo è una path di un file quindi il catalogo è 
            // il direttorio, la tabella e l'indice sono il nome di un file
            if ((pStr = Cat.rat(_T('.'))) != NULL) // Elimino eventuale estensione
               Cat.set_chr(_T('\0'), (int) (pStr - Cat.get_name()));

            break;
         }
         case FileRes:
         {
            // Il catalogo è il nome di un file
            if ((pStr = Cat.rat(_T('.'))) != NULL) // Elimino eventuale estensione
               Cat.set_chr(_T('\0'), (int) (pStr - Cat.get_name()));
            break;
         }
      }

      if (gsc_AdjSyntax(Cat, get_InitQuotedIdentifier(), get_FinalQuotedIdentifier(),
                        get_StrCaseFullTableRef()) == GS_BAD)
         return NULL;

      if (get_SchemaUsage() & SchCheck) // schema supportato
      {
         if (!Schema) // se non è stato passato uso quello corrente
         {
            gsc_set_variant_t(PropName, _T("Current Schema"));
            if (pConn->Properties->get_Item(PropName, &pResult) == S_OK) // se esiste la proprietà
               Sch = (TCHAR *) _bstr_t (pResult->GetValue());
            else
            {
               gsc_set_variant_t(PropName, _T("Schema Term"));
               if (pConn->Properties->get_Item(PropName, &pResult) == S_OK && // se esiste la proprietà
                   (gsc_strcmp(_T("Owner"), (TCHAR *) _bstr_t (pResult->GetValue()), FALSE) == 0 ||
                    gsc_strcmp(_T("User Name"), (TCHAR *) _bstr_t (pResult->GetValue()), FALSE) == 0))
               {
                  // Lo schema rappresenta l'utente collegato al db (non l'utente GEOsim)
                  gsc_set_variant_t(PropName, _T("User ID"));
                  if (pConn->Properties->get_Item(PropName, &pResult) == S_OK) // se esiste la proprietà
                     Sch = (TCHAR *) _bstr_t (pResult->GetValue());
               }
            }
         }
         else Sch = Schema;

         if (gsc_AdjSyntax(Sch, get_InitQuotedIdentifier(), get_FinalQuotedIdentifier(), 
                           get_StrCaseFullTableRef()) == GS_BAD) return NULL;

         if (Sch.len() > 0) // Schema specificato
         {
            Sch += get_SchemaSeparator();
            Sch += Tab;
         }
         else Sch = Tab;
      }
      else Sch = Tab;

      if (get_CatalogLocation() == DBPROPVAL_CL_START) // il catalogo va all'inizio
      {
         if (Cat.len() > 0) // Catalogo specificato
         {
            FullRefTable = Cat;
            FullRefTable += get_CatalogSeparator();
            FullRefTable += Sch;
         }
         else
            FullRefTable = Sch;
      }
      else // il catalogo va in fondo
      {
         FullRefTable = Sch;
         if (Cat.len() > 0) // Catalogo specificato
         {
            FullRefTable += get_CatalogSeparator();
            FullRefTable += Cat;
         }
      }
   }
   else // non supporta l'uso del catalogo
   {
      if (get_SchemaUsage() & SchCheck) // schema supportato
      {
         if (!Schema) // se non è stato passato uso quello corrente
         {
            gsc_set_variant_t(PropName, _T("Current Schema"));
            if (pConn->Properties->get_Item(PropName, &pResult) == S_OK) // se esiste la proprietà
               Sch = (TCHAR *) _bstr_t (pResult->GetValue());
            else
            {
               gsc_set_variant_t(PropName, _T("Schema Term"));
               if (pConn->Properties->get_Item(PropName, &pResult) == S_OK && // se esiste la proprietà
                   (gsc_strcmp(_T("Owner"), (TCHAR *) _bstr_t (pResult->GetValue()), FALSE) == 0 ||
                    gsc_strcmp(_T("User Name"), (TCHAR *) _bstr_t (pResult->GetValue()), FALSE) == 0))
               {
                  // Lo schema rappresenta l'utente collegato al db (non l'utente GEOsim)
                  gsc_set_variant_t(PropName, _T("User ID"));
                  if (pConn->Properties->get_Item(PropName, &pResult) == S_OK) // se esiste la proprietà
                     Sch = (TCHAR *) _bstr_t (pResult->GetValue());
               }
            }
         }
         else Sch = Schema;

         if (gsc_AdjSyntax(Sch, get_InitQuotedIdentifier(), get_FinalQuotedIdentifier(), 
                           get_StrCaseFullTableRef()) == GS_BAD) return NULL;

         if (Sch.len() > 0) // Schema specificato
         {
            Sch += get_SchemaSeparator();
            Sch += Tab;
         }
         else Sch = Tab;

         FullRefTable = Sch;
      }
      else // non supporta l'uso dello schema
         FullRefTable = Tab;  
   }
   return gsc_tostring(FullRefTable.get_name());
}


/****************************************************************/
/*.doc C_DBCONNECTION::split_FullRefTable            <internal> */
/*+
  Questa funzione scompone un riferimento completo a tabella nei componenti
  catalogo, schema e tabella.
  Parametri: 
  const TCHAR *FullRefTable;       Riferimento completo alla tabella
  C_STRING &Catalog;               Catalogo
  C_STRING &Schema;                Schema
  C_STRING &Table;                 Nome della tabella
  CatSchUsageTypeEnum CatSchUsage; Uso che si vuole fare della tabella (default = DML)
  bool RudeComponent;    Opzionale, se = true il componente non viene pulito 
                         (caratteri di delimitazione, maiuscolo/minuscolo...)
                         ma lasciato anche con eventuali parametri (vedi history)
                         Es. se il riferimento completo = "catalogo"."schema"."mia_fun"(NULL::timestamp)"
                         il nome della tabella con RudeComponent=true sarà "mia_fun"(NULL::timestamp)
                         mentre con RudeComponent=false sarà mia_fun.
                         (default = false)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::split_FullRefTable(C_STRING &FullRefTable, C_STRING &Catalog,
                                       C_STRING &Schema, C_STRING &Table, 
                                       CatSchUsageTypeEnum CatSchUsage,
                                       bool RudeComponent)
   { return split_FullRefTable(FullRefTable.get_name(), Catalog, Schema, Table, CatSchUsage, RudeComponent); }
int C_DBCONNECTION::split_FullRefTable(const TCHAR *FullRefTable, C_STRING &Catalog,
                                       C_STRING &Schema, C_STRING &Table, 
                                       CatSchUsageTypeEnum CatSchUsage,
                                       bool RudeComponent)
{
   switch (CatSchUsage)
   {
      case DML:
         return split_FullRefSQL(FullRefTable, Catalog, DBPROPVAL_CU_DML_STATEMENTS,
                                 Schema, DBPROPVAL_SU_DML_STATEMENTS, Table, RudeComponent);
      case Definition:
         return split_FullRefSQL(FullRefTable, Catalog, DBPROPVAL_CU_TABLE_DEFINITION,
                                 Schema, DBPROPVAL_SU_TABLE_DEFINITION, Table, RudeComponent);
      case Privilege:
         return split_FullRefSQL(FullRefTable, Catalog, DBPROPVAL_CU_PRIVILEGE_DEFINITION,
                                 Schema, DBPROPVAL_SU_PRIVILEGE_DEFINITION, Table, RudeComponent);
      default:
         return GS_BAD;
   }
}


/****************************************************************/
/*.doc C_DBCONNECTION::split_FullRefSQL              <internal> */
/*+
  Questa funzione scompone un riferimento completo a 
  <tabella>\<indice>\<definizione di privilegi> nei componenti
  catalogo, schema e <tabella>\<indice>\<definizione di privilegi>.
  Parametri: 
  const TCHAR *FullRef;  Riferimento completo al componente SQL (<tabella>\<indice>\
                         <definizione di privilegi>)
  C_STRING &Catalog;     Catalogo
  int       CatCheck;    Flag di riferimento per uso o meno del catalogo
  C_STRING &Schema;      Schema
  int       SchCheck;    Flag di riferimento per uso o meno dello schema
  C_STRING &Component;   Nome del componente SQL (<tabella>\<indice>\
                         <definizione di privilegi>)
  bool RudeComponent;    Opzionale, se = true il componente non viene pulito 
                         (caratteri di delimitazione, maiuscolo/minuscolo...)
                         ma lasciato anche con eventuali parametri (vedi history)
                         Es. se il riferimento completo = "catalogo"."schema"."mia_fun"(NULL::timestamp)"
                         il nome della tabella con RudeComponent=true sarà "mia_fun"(NULL::timestamp)
                         mentre con RudeComponent=false sarà mia_fun.
                         (default = false)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::split_FullRefSQL(const TCHAR *FullRef, C_STRING &Catalog, int CatCheck,
                                     C_STRING &Schema, int SchCheck, C_STRING &Component,
                                     bool RudeComponent)
{
   C_STRING RefTable(FullRef);
   TCHAR    *pStr;

   RefTable.alltrim();
   if (RefTable.len() == 0) return GS_BAD;
   pStr = RefTable.get_name();

   if (get_CatalogUsage() & CatCheck) // catalogo supportato
   {  // Catalogo in posizione iniziale (es. "CATALOG.SCHEMA.TABLE")
      if (get_CatalogLocation() == DBPROPVAL_CL_START)
      {
         if (RudeComponent)
            Catalog.paste(gsc_getRudeTabComponent(&pStr, get_CatalogSeparator()));
         else
            Catalog.paste(gsc_getFullRefTabComponent(&pStr, get_InitQuotedIdentifier(), 
                                                     get_FinalQuotedIdentifier(),
                                                     get_CatalogSeparator(),
                                                     get_StrCaseFullTableRef(),
                                                     get_CatalogResourceType()));
         if (*pStr != _T('\0')) pStr++; // salto il carattere separatore
         if (get_SchemaUsage() & SchCheck) // schema supportato
         {
            if (RudeComponent)
               Schema.paste(gsc_getRudeTabComponent(&pStr, get_SchemaSeparator()));
            else
               Schema.paste(gsc_getFullRefTabComponent(&pStr, get_InitQuotedIdentifier(), 
                                                       get_FinalQuotedIdentifier(),
                                                       get_SchemaSeparator(),
                                                       get_StrCaseFullTableRef()));
            if (*pStr != _T('\0')) pStr++; // salto il carattere separatore
         }

         if (RudeComponent)
            Component.paste(gsc_getRudeTabComponent(&pStr, _T('\0')));
         else
            Component.paste(gsc_getFullRefTabComponent(&pStr, get_InitQuotedIdentifier(), 
                                                       get_FinalQuotedIdentifier(),
                                                       _T('\0'),
                                                       get_StrCaseFullTableRef()));

         if (Component.len() == 0) // se è specificata solo la tabella
         {
            if (Catalog.len() > 0)
            {
               Component = Schema;
               Schema = Catalog;
               Catalog.clear();
            }
            if (Component.len() == 0)
            {
               Component = Schema;
               Schema.clear();
            }
         }
      }
      else // Catalogo in posizione finale (es. "SCHEMA.TABLE@CATALOG")
      {
         if (get_SchemaUsage() & SchCheck) // schema supportato
         {
            if (RudeComponent)
               Schema.paste(gsc_getRudeTabComponent(&pStr, get_SchemaSeparator()));
            else
               Schema.paste(gsc_getFullRefTabComponent(&pStr, get_InitQuotedIdentifier(), 
                                                       get_FinalQuotedIdentifier(),
                                                       get_SchemaSeparator(),
                                                       get_StrCaseFullTableRef()));
            if (*pStr != _T('\0')) pStr++; // salto il carattere separatore
         }

         if (RudeComponent)
            Component.paste(gsc_getRudeTabComponent(&pStr, get_CatalogSeparator()));
         else
            Component.paste(gsc_getFullRefTabComponent(&pStr, get_InitQuotedIdentifier(), 
                                                       get_FinalQuotedIdentifier(),
                                                       get_CatalogSeparator(),
                                                       get_StrCaseFullTableRef()));
         if (*pStr != _T('\0')) pStr++; // salto il carattere separatore

         if (RudeComponent)
            Component.paste(gsc_getRudeTabComponent(&pStr, _T('\0')));
         else
            Catalog.paste(gsc_getFullRefTabComponent(&pStr, get_InitQuotedIdentifier(), 
                                                     get_FinalQuotedIdentifier(),
                                                     _T('\0'),
                                                     get_StrCaseFullTableRef()));

         if (Component.len() == 0) // se è specificata solo la tabella
         {
            Component = Schema;
            Schema.clear();
         }
      }
         
      switch (get_CatalogResourceType())
      {
         case DirectoryRes:
         {
            // Il riferimento completo è una path di un file quindi il catalogo è 
            // il direttorio, la tabella e l'indice sono il nome di un file
            if ((pStr = Component.rat(_T('.'))) != NULL) // Elimino eventuale estensione
               Component.set_chr(_T('\0'), (int) (pStr - Component.get_name()));
            break;
         }
         case FileRes:
         {
            // Il catalogo è il nome di un file
            if ((pStr = Catalog.rat(_T('.'))) != NULL) // Elimino eventuale estensione
               Catalog.set_chr(_T('\0'), (int) (pStr - Catalog.get_name()));
            break;
         }
      }
   }
   else // catalogo non supportato
   {
      Catalog.clear();
      if (get_SchemaUsage() & SchCheck) // schema supportato
      {
         if (get_CatalogLocation() == DBPROPVAL_CL_START) // lo schema in posizione finale
         {
            if (RudeComponent)
               Component.paste(gsc_getRudeTabComponent(&pStr, get_SchemaSeparator()));
            else
               Component.paste(gsc_getFullRefTabComponent(&pStr, get_InitQuotedIdentifier(), 
                                                          get_FinalQuotedIdentifier(),
                                                          get_SchemaSeparator(),
                                                          get_StrCaseFullTableRef()));
            if (*pStr != _T('\0')) pStr++; // salto il carattere separatore
            if (get_SchemaUsage() & DBPROPVAL_SU_TABLE_DEFINITION) // schema supportato
            {
               if (RudeComponent)
                  Schema.paste(gsc_getRudeTabComponent(&pStr, _T('\0')));
               else
                  Schema.paste(gsc_getFullRefTabComponent(&pStr, get_InitQuotedIdentifier(), 
                                                          get_FinalQuotedIdentifier(),
                                                          _T('\0'),
                                                          get_StrCaseFullTableRef()));
            }
         }
         else  // lo schema in posizione iniziale
         {
            if (RudeComponent)
               Schema.paste(gsc_getRudeTabComponent(&pStr, get_SchemaSeparator()));
            else
               Schema.paste(gsc_getFullRefTabComponent(&pStr, get_InitQuotedIdentifier(), 
                                                       get_FinalQuotedIdentifier(),
                                                       get_SchemaSeparator(),
                                                       get_StrCaseFullTableRef()));
            if (*pStr != _T('\0')) // se esiste un carattere separatore
            {  // leggo il nome della tabella
               pStr++; // salto il carattere separatore

               if (RudeComponent)
                  Component.paste(gsc_getRudeTabComponent(&pStr, _T('\0')));
               else
                  Component.paste(gsc_getFullRefTabComponent(&pStr, get_InitQuotedIdentifier(), 
                                                             get_FinalQuotedIdentifier(),
                                                             _T('\0'),
                                                             get_StrCaseFullTableRef()));
            }
            else // se non esiste un carattere separatore
            {  // la variabile Schema contiene il nome della tabella e lo schema
               // è quello di default del db in uso
               Component = Schema;
               Schema.clear();
            }
         }
      }
      else
      {
         if (RudeComponent)
            Component.paste(gsc_getRudeTabComponent(&pStr, _T('\0')));
         else
            Component.paste(gsc_getFullRefTabComponent(&pStr, get_InitQuotedIdentifier(), 
                                                       get_FinalQuotedIdentifier(),
                                                       _T('\0'),
                                                       get_StrCaseFullTableRef()));
      }
   }

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::FullRefTable_drive2nethost     <external> */
/*+
  Questa funzione converte il riferimento completo (per ora solo il catalogo)
  modificando una path assoluta in una path relativa.
  Parametri: 
  const TCHAR *FullRefTable;   riferimento completo tabella
  
  Restituisce la stringa convertita in caso di successo altrimenti NULL.
  N.B. Alloca memoria
-*/  
/****************************************************************/
TCHAR *C_DBCONNECTION::FullRefTable_drive2nethost(const TCHAR *FullRefTable)
{
   if (!FullRefTable) return NULL;

   if (get_CatalogResourceType() == DirectoryRes ||
       get_CatalogResourceType() == FileRes)
   {
      C_STRING Catalog, Schema, Table;

      if (split_FullRefTable(FullRefTable, Catalog, Schema, Table) == GS_BAD) return NULL;
      if (gsc_drive2nethost(Catalog) == GS_BAD) return GS_BAD; // Converto
      return get_FullRefTable(Catalog, Schema, Table);
   }
   else
      return gsc_tostring(FullRefTable);
}


/****************************************************************/
/*.doc C_DBCONNECTION::AdjTableParams                <external> */
/*+
  Questa funzione corregge la tabella nel caso sia una funzione
  con parametri come nel caso della tabella storica in cui il parametro è
  una data ("select * from myfunction(NULL::timestamp)"). Il parametro è
  espresso fra parentesi tonde ed è seguito da :: e dal tipo SQL
  (es. timestamp, numeric, varchar...).
  Il valore del parametro è letto dalla sessione di lavoro (fino a 3 parametri)
  o, in mancanza di questa, richiesto all'utente.
  Parametri:
  C_STRING &FullRefTable; Riferimento completo tabella (input-output)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/****************************************************************/
int C_DBCONNECTION::AdjTableParams(C_STRING &FullRefTable)
{
   // Se la tabella geometrica è una funzione con parametri
   DataTypeEnum ParamType;
   int          i = 0;
   C_STRING     Value;

   while (GetTable_iParamType(FullRefTable, 1, &ParamType) == GS_GOOD)
   {
      // Se la sessione di lavoro ha già inizializzato il parametro
      if (GS_CURRENT_WRK_SESSION && i < MAX_WRK_SESSION_PARAMNUMBER &&
          GS_CURRENT_WRK_SESSION->get_SQLParameters()[i].len() > 0)
         SetTable_iParam(FullRefTable, 1,
                         GS_CURRENT_WRK_SESSION->get_SQLParameters()[i],
                         ParamType);
      else
      {
         Value.clear();
         if (ParamType == adDBDate || ParamType == adDBTimeStamp)
         {
            C_STRING dummy;

            if (gsc_ddSuggestDBValue(dummy, adDBTimeStamp) == GS_BAD) return GS_BAD;
            if (gsc_getSQLDateTime(dummy.get_name(), Value) == GS_BAD) return GS_BAD;
            Value.strtran(_T("00:00:00"), _T("23:59:59"));
         }
         else
         {
            TCHAR Msg[TILE_STR_LIMIT];

            swprintf(Msg, TILE_STR_LIMIT, gsc_msg(651), _T("")); // "Inserire valore parametro <%s>: "
            if (gsc_ddinput(Msg, Value) != GS_GOOD) return GS_BAD;
         }

         if (SetTable_iParam(FullRefTable, 1, Value, ParamType) == GS_BAD) return GS_BAD;

         if (GS_CURRENT_WRK_SESSION && i < MAX_WRK_SESSION_PARAMNUMBER)
            GS_CURRENT_WRK_SESSION->get_SQLParameters()[i] = Value;
      }
   }

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::FullRefTable_nethost2drive     <external> */
/*+
  Questa funzione converte il riferimento completo (per ora solo il catalogo)
  modificando una path relativa in una path assoluta.
  Parametri: 
  const TCHAR *FullRefTable;   riferimento completo tabella
  
  Restituisce la stringa convertita in caso di successo altrimenti NULL.
  N.B. Alloca memoria
-*/  
/****************************************************************/
TCHAR *C_DBCONNECTION::FullRefTable_nethost2drive(const TCHAR *FullRefTable)
{
   if (get_CatalogResourceType() == DirectoryRes ||
       get_CatalogResourceType() == FileRes)
   {
      C_STRING Catalog, Schema, Table;

      if (split_FullRefTable(FullRefTable, Catalog, Schema, Table) == GS_BAD) return NULL;
      if (gsc_nethost2drive(Catalog) == GS_BAD) return GS_BAD; // Converto
      return get_FullRefTable(Catalog, Schema, Table);
   }
   else
      return gsc_tostring(FullRefTable);
}


/****************************************************************/
/*.doc C_DBCONNECTION::IsValidTabName                <external> */
/*+
  Questa funzione valuta la correttezza del nome della tabella.
  Parametri: 
  C_STRING &TabName;   Nome della tabella (senza catalogo e schema)
  
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::IsValidTabName(C_STRING &TabName)
{
   TabName.alltrim();
   if (TabName.len() == 0 || (int) TabName.len() > get_MaxTableNameLen())
      { GS_ERR_COD = eGSInvTabName; return GS_BAD; }

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::IsValidFieldName              <external> */
/*+
  Questa funzione valuta la correttezza del nome dell'attributo di una tabella.
  Parametri: 
  C_STRING &FieldName;   Nome dell'attributo di una tabella
  
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::IsValidFieldName(C_STRING &FieldName)
{
   FieldName.alltrim();
   if (FieldName.len() == 0 || (int) FieldName.len() > get_MaxFieldNameLen()) return GS_BAD;
   
   // Non può essere = ? perchè usato nelle query parametriche
   if (FieldName.comp(_T("?")) == 0) return GS_BAD;

   switch (get_StrCaseFullTableRef())
   {
      case Upper: FieldName.toupper(); break; // converto in maiuscolo
      case Lower: FieldName.tolower(); break; // converto in minuscolo
   }

   // Sostituisco i caratteri non validi con _
   for (int i = 0; i < (int) FieldName.len(); i++)
      if (InvalidFieldNameCharacters.at(FieldName.get_chr(i)) != NULL)
         FieldName.set_chr(_T('_'), i);

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::OpenRs                        <external> */
/*+
  Questa funzione apre un recordset relativoi all'istruzione data.
  Parametri: 
  C_STRING &statement;            Istruzione
  _RecordsetPtr &pRs;
  enum CursorTypeEnum CursorType; Tipo di cursore (default = adOpenForwardOnly)
  enum LockTypeEnum LockType;     Tipo di Lock (default = adLockReadOnly)
  int retest;                     Se MORETESTS -> in caso di errore riprova n volte 
                                  con i tempi di attesa impostati poi ritorna GS_BAD,
                                  ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                                  (default = MORETESTS)
  int PrintError;                 Utilizzato solo se <retest> = ONETEST. 
                                  Se il flag = GS_GOOD in caso di errore viene
                                  stampato il messaggio relativo altrimenti non
                                  viene stampato alcun messaggio (default = GS_GOOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::OpenRecSet(C_STRING &statement, _RecordsetPtr &pRs,
                               enum CursorTypeEnum CursorType, enum LockTypeEnum LockType,
                               int retest, int PrintError)
{
   return gsc_DBOpenRs(pConn, statement.get_name(), pRs,
                       CursorType, LockType, adCmdText, retest, PrintError);
}


///////////////////////////////////////////////////////////////////////////////
// FUNZIONI PER TABELLE - INIZIO                                             //
///////////////////////////////////////////////////////////////////////////////


/****************************************************************/
/*.doc gsc_ReadDBSample                              <internal> */
/*+
  Questa funzione legge, da un file testo, il nome del database di esempio
  da cui creare altri DB, la path è intesa GEODATALINKDIR sotto GEOsimAppl::GEODIR.
  Parametri:
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  C_STRING &DBSample;         Out

  oppure

  const TCHAR *RequiredFile;  File Required
  C_STRING &DBSample;         Out
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int gsc_ReadDBSample(C_PROFILE_SECTION_BTREE &ProfileSections, C_STRING &DBSample)
{
   C_B2STR *pProfileEntry;
   int res;
   
   // leggo proprietà "DB Sample"
   if ((pProfileEntry = ProfileSections.get_entry(_T("GEOsim"), _T("DB Sample"))))
   {
      DBSample = pProfileEntry->get_name2();
      res = GS_GOOD;
   }
   else
   {
      DBSample.clear();
      res = GS_BAD;
   }

   return res;
}
int gsc_ReadDBSample(const TCHAR *RequiredFile, C_STRING &DBSample)
{
   C_PROFILE_SECTION_BTREE ProfileSections;

   if (gsc_read_profile(RequiredFile, ProfileSections) == GS_BAD) return GS_BAD;
   return gsc_ReadDBSample(ProfileSections, DBSample);
}


/****************************************************************/
/*.doc C_DBCONNECTION::CreateDB                      <external> */
/*+
  Questa funzione crea un database se questo è un riferimento ad
  una cartella o ad un file che viene utilizzato come catalogo.
  Parametri: 
  const TCHAR *Ref;     Riferimento completo del db
  int retest;           Se MORETESTS -> in caso di errore riprova n volte 
                        con i tempi di attesa impostati poi ritorna GS_BAD,
                        ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::CreateDB(const TCHAR *Ref, int retest)
{
   switch (CatalogResourceType)
   {
      case DirectoryRes: // se il riferimento è una cartella
            return gsc_mkdir(Ref);
      case FileRes: // se il riferimento è un file
      {
         C_STRING _Ref(Ref), ext;

         gsc_splitpath(_Ref, NULL, NULL, NULL, &ext);
         // Se non c'è l'estensione aggiungo quella usata dal file campione
         if (ext.len() == 0)
         {
            gsc_splitpath(DBSample, NULL, NULL, NULL, &ext);
            _Ref += ext;
         }

         if (DBSample.len() == 0) return GS_BAD;
         // se il riferimento è un file
         if (gsc_mkdir_from_path(_Ref) == GS_BAD) return GS_BAD;
         // creo database copiando un file "campione"
         if (gsc_path_exist(_Ref) == GS_GOOD) return GS_GOOD;
         if (gsc_copyfile(DBSample, _Ref, retest) == GS_BAD)
            { gsc_rmdir_from_path(_Ref); return GS_BAD; }
         
         return GS_GOOD;
      }
   }

   return GS_BAD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::CreateSchema                  <external> */
/*+
  Questa funzione crea uno schema.
  Parametri: 
  const TCHAR *Schema;  Nome dello schema
  int retest;           Se MORETESTS -> in caso di errore riprova n volte 
                        con i tempi di attesa impostati poi ritorna GS_BAD,
                        ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::CreateSchema(const TCHAR *Schema, int retest)
{
   C_STRING Stm;

   if (DBMSName.at(PG_DBMSNAME, FALSE) != NULL) // è in PostgreSQL
   {
      // CREATE SCHEMA schema [AUTHORIZATION usr];
      Stm = _T("CREATE SCHEMA ");
      Stm += Schema;
   }
   else if (DBMSName.at(ORACLE_DBMSNAME, FALSE) != NULL) // è in ORACLE
   {
      // CREATE SCHEMA AUTHORIZATION schema
      Stm = _T("CREATE SCHEMA AUTHORIZATION ");
      Stm += Schema;
   }
   else
      return GS_BAD;

   return ExeCmd(Stm, NULL, retest);
}


/****************************************************************/
/*.doc C_DBCONNECTION::CreateSequence                <external> */
/*+
  Questa funzione crea una sequenza.
  Parametri: 
  const TCHAR *FullRefSequence;  Riferimento completo sequenza
  int retest;                    Se MORETESTS -> in caso di errore riprova n volte 
                                 con i tempi di attesa impostati poi ritorna GS_BAD,
                                 ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::CreateSequence(const TCHAR *FullRefSequence, int retest)
{
   C_STRING Stm;

   if (DBMSName.at(PG_DBMSNAME, FALSE) != NULL) // è in PostgreSQL
   {
      // CREATE SEQUENCE sequence
      //  INCREMENT 1
      //  MINVALUE 1
      //  MAXVALUE 9223372036854775807
      //  START 1
      //  CACHE 1;
      Stm = _T("CREATE SEQUENCE ");
      Stm += FullRefSequence;
      Stm += _T(" INCREMENT 1 MINVALUE 1 MAXVALUE 9223372036854775807 START 1 CACHE 1");
   }
   else if (DBMSName.at(ORACLE_DBMSNAME, FALSE) != NULL) // è in ORACLE
   {
      // CREATE SEQUENCE sequence
      //  INCREMENT BY 1
      //  MINVALUE 1
      //  MAXVALUE 9223372036854775807
      //  START WITH 1;
      Stm = _T("CREATE SEQUENCE ");
      Stm += FullRefSequence;
      Stm += _T(" INCREMENT BY 1 MINVALUE 1 MAXVALUE 9223372036854775807 START WITH 1");
   }
   else
      return GS_BAD;

   return ExeCmd(Stm, NULL, retest);
}


/****************************************************************/
/*.doc C_DBCONNECTION::DelSequence                   <external> */
/*+
  Questa funzione cancella una sequenza.
  Parametri: 
  const TCHAR *FullRefSequence;  Riferimento completo sequenza
  int retest;                    Se MORETESTS -> in caso di errore riprova n volte 
                                 con i tempi di attesa impostati poi ritorna GS_BAD,
                                 ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::DelSequence(const TCHAR *FullRefSequence, int retest)
{
   C_STRING Stm;

   if (DBMSName.at(PG_DBMSNAME, FALSE) != NULL) // è in PostgreSQL
   {
      // DROP SEQUENCE IF EXISTS name
      Stm = _T("DROP SEQUENCE IF EXISTS ");
      Stm += FullRefSequence;
   }
   else if (DBMSName.at(ORACLE_DBMSNAME, FALSE) != NULL) // è in ORACLE
   {
      // DROP SEQUENCE name
      Stm = _T("DROP SEQUENCE ");
      Stm += FullRefSequence;
   }
   else
      return GS_BAD;

   return ExeCmd(Stm, NULL, retest);
}


/****************************************************************/
/*.doc C_DBCONNECTION::DelView                       <external> */
/*+
  Questa funzione cancella una vista.
  Parametri: 
  const TCHAR *FullRefView;  Riferimento completo della vista
  int retest;                Se MORETESTS -> in caso di errore riprova n volte 
                             con i tempi di attesa impostati poi ritorna GS_BAD,
                             ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::DelView(const TCHAR *FullRefView, int retest)
{
   C_STRING Stm;

   if (DBMSName.at(PG_DBMSNAME, FALSE) != NULL) // è in PostgreSQL
   {
      // DROP VIEW IF EXISTS name CASCADE
      Stm = _T("DROP VIEW IF EXISTS ");
      Stm += FullRefView;
      Stm += _T(" CASCADE");   
   }
   else if (DBMSName.at(ORACLE_DBMSNAME, FALSE) != NULL) // è in ORACLE
   {
      // DROP VIEW name CASCADE
      Stm = _T("DROP VIEW ");
      Stm += FullRefView;
      Stm += _T(" CASCADE");   
   }
   else
      return GS_BAD;

   return ExeCmd(Stm, NULL, retest);
}


/****************************************************************/
/*.doc C_DBCONNECTION::DelTrigger                    <external> */
/*+
  Questa funzione cancella una vista.
  Parametri: 
  const TCHAR *TriggerName;  Nome del trigger
  const TCHAR *TableRef;     Riferimento completo della tabella
  int retest;                Se MORETESTS -> in caso di errore riprova n volte 
                             con i tempi di attesa impostati poi ritorna GS_BAD,
                             ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::DelTrigger(const TCHAR *TriggerName, const TCHAR *TableRef, int retest)
{
   C_STRING Stm;

   if (DBMSName.at(PG_DBMSNAME, FALSE) != NULL) // è in PostgreSQL
   {
      // DROP TRIGGER IF EXISTS trigger_name ON table
      Stm = _T("DROP TRIGGER IF EXISTS ");
      Stm += TriggerName;
      Stm += _T(" ON ");
      Stm += TableRef;
   }
   else if (DBMSName.at(ORACLE_DBMSNAME, FALSE) != NULL) // è in ORACLE
   {
      // DROP TRIGGER trigger_name
      Stm = _T("DROP TRIGGER ");
      Stm += TriggerName;
   }
   else
      return GS_BAD;

   return ExeCmd(Stm, NULL, retest);
}


/****************************************************************/
/*.doc C_DBCONNECTION::DelFunction                   <external> */
/*+
  Questa funzione cancella una funzione.
  Parametri: 
  const TCHAR *FullRefFunction;  Riferimento completo della funzione
  int retest;                Se MORETESTS -> in caso di errore riprova n volte 
                             con i tempi di attesa impostati poi ritorna GS_BAD,
                             ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::DelFunction(const TCHAR *FullRefFunction, int retest)
{
   C_STRING Stm;

   if (DBMSName.at(PG_DBMSNAME, FALSE) != NULL) // è in PostgreSQL
   {
      // DROP FUNCTION IF EXISTS base.bc_edifici_v_del(integer) CASCADE
      Stm = _T("DROP FUNCTION IF EXISTS ");
      Stm += FullRefFunction;
      Stm += _T(" CASCADE");   
   }
   else if (DBMSName.at(ORACLE_DBMSNAME, FALSE) != NULL) // è in ORACLE
   {
      // DROP FUNCTION name CASCADE
      Stm = _T("DROP FUNCTION ");
      Stm += FullRefFunction;
      Stm += _T(" CASCADE");   
   }
   else
      return GS_BAD;

   return ExeCmd(Stm, NULL, retest);
}


/****************************************************************/
/*.doc C_DBCONNECTION::getConcatenationStrSQLKeyWord <external> */
/*+
  Questa funzione restituisce la parola chiave SQl per la concatenazione tra stringhe.
  Parametri: 
  C_STRING &SQLKeyWord;
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::getConcatenationStrSQLKeyWord(C_STRING &SQLKeyWord)
{
   if (DBMSName.at(ACCESS_DBMSNAME, FALSE) != NULL) // è in SQL Jet (Microsoft Access)
       SQLKeyWord  =_T("&");
   else if (DBMSName.at(PG_DBMSNAME, FALSE) != NULL) // è in PostgreSQL
       SQLKeyWord  =_T("||");
   else if (DBMSName.at(ORACLE_DBMSNAME, FALSE) != NULL) // è in ORACLE
       SQLKeyWord  =_T("||");
   else if (DBMSName.at(SQLSERVER_DBMSNAME, FALSE) != NULL) // è in SQL Server
       SQLKeyWord  =_T("+");
   else if (DBMSName.at(MYSQL_DBMSNAME, FALSE) != NULL) // è in MySQL
      SQLKeyWord  =_T(" ");
   else
      return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBCONNECTION::getTableAndViewList    <external> */
/*+
  Questa funzione restituisce la lista delle tabelle e delle viste esistenti
  con il riferimento completo (catalogo, schema, tabella con i delimitatori).
  Parametri:
  _ConnectionPtr &pConn;   Puntatore alla connessione database
  C_STR_LIST &ItemList;    Lista delle tabelle e/o viste
  const TCHAR *Catalog;    Catalogo
  const TCHAR *Schema;     Schema
  const TCHAR *ItemName;   Opzionale; Nome della tabella o della vista da cercare
                           (default = NULL)
  bool Tables;             Se = true include la lista delle tabelle (default = true)
  const TCHAR *TableTypeFilter;  Opzionale; Se Tables = true, TableType è un filtro per 
                           leggere solo le tabelle di un certo tipo come ad es.
                           "ACCESS TABLE", "SYSTEM TABLE", "SYSTEM VIEW",
                           "TABLE", "LINK", "VIEW" (default = NULL)
  bool Views;              Se = true include la lista delle viste (default = true)
  int retest;              se MORETESTS -> in caso di errore riprova n volte 
                           con i tempi di attesa impostati poi ritorna GS_BAD,
                           ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                           (default = MORETESTS)
  int PrintError;          Utilizzato solo se <retest> = ONETEST. 
                           Se il flag = GS_GOOD in caso di errore viene
                           stampato il messaggio relativo altrimenti non
                           viene stampato alcun messaggio (default = GS_GOOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::getTableAndViewList(C_STR_LIST &ItemList,
                                        const TCHAR *Catalog, const TCHAR *Schema,
                                        const TCHAR *ItemName,
                                        bool Tables, const TCHAR *TableTypeFilter,
                                        bool Views, 
                                        int retest, int PrintError)
{
   SAFEARRAY FAR* psa = NULL; // Creo un safearray che prende 4 elementi
   SAFEARRAYBOUND rgsabound;
   _variant_t     var, varCriteria;
   long           ix;
	_RecordsetPtr  pRs;
   C_RB_LIST	   ColValues;
   presbuf        pCatalog, pSchema, pTable, pTableType;
   C_STR          *pItem;
   int            res = GS_GOOD;

   ItemList.remove_all();

   rgsabound.lLbound   = 0;
   rgsabound.cElements = 4;
   psa = SafeArrayCreate(VT_VARIANT, 1, &rgsabound);

   // CATALOGO
   ix  = 0;
   if (Catalog)
   {
      gsc_set_variant_t(var, Catalog);
      SafeArrayPutElement(psa, &ix, &var);
   }
   else
      { var.vt = VT_EMPTY; SafeArrayPutElement(psa, &ix, &var); }

   // SCHEMA
   ix= 1;
   if (Schema)
   {
      gsc_set_variant_t(var, Schema);
      SafeArrayPutElement(psa, &ix, &var);
   }
   else
      { var.vt = VT_EMPTY; SafeArrayPutElement(psa, &ix, &var); }

   // NOME TABELLA O VISTA DA CERCARE
   ix = 2;
   if (ItemName)
   {
      gsc_set_variant_t(var, ItemName);
      SafeArrayPutElement(psa, &ix, &var);
   }
   else
      { var.vt = VT_EMPTY; SafeArrayPutElement(psa, &ix, &var); }

   // TIPO DI OGGETTO DA CERCARE
   ix = 3;
   var.vt = VT_EMPTY;
   SafeArrayPutElement(psa, &ix, &var);

   varCriteria.vt = VT_ARRAY|VT_VARIANT;
   varCriteria.parray = psa;  

   // Leggo la lista delle tabelle
   if (gsc_OpenSchema(pConn, adSchemaTables, pRs, &varCriteria, 
                      retest, PrintError) == GS_BAD)
      return GS_BAD;

   if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD)
      { gsc_DBCloseRs(pRs); return GS_CAN; }

   pCatalog   = ColValues.CdrAssoc(_T("TABLE_CATALOG"));
   pSchema    = ColValues.CdrAssoc(_T("TABLE_SCHEMA"));
   pTable     = ColValues.CdrAssoc(_T("TABLE_NAME"));
   pTableType = ColValues.CdrAssoc(_T("TABLE_TYPE"));

   if (Tables)
   {
      while (gsc_isEOF(pRs) == GS_BAD)
      {
         if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { res = GS_CAN; break; }
         // Se esiste un filtro sul tipo di tabella
         if (TableTypeFilter)
            if (gsc_strcmp(TableTypeFilter, pTableType->resval.rstring, GS_BAD) != 0)
               { gsc_Skip(pRs); continue; }
            
         if ((pItem = new C_STR()) == NULL)
            { GS_ERR_COD = eGSOutOfMem; res = GS_BAD; break; }
         pItem->paste(get_FullRefTable((pCatalog->restype == RTSTR) ? pCatalog->resval.rstring : NULL,
                                       (pSchema->restype == RTSTR) ? pSchema->resval.rstring : NULL,
                                       pTable->resval.rstring));
         ItemList.add_tail(pItem);
         gsc_Skip(pRs);
      }
   }
   gsc_DBCloseRs(pRs);

   // Se devo leggere la lista delle viste 
   // provo una sola voltà perchè può non essere supportato
   // Attenzione percè in questo caso se si è in una transazione, questa viene annullata
   // dalla funzione OpenSchema con parametro adSchemaViews non supportato
   if (Views && SchemaViewsSupported)
   {
      // memorizzo il codice di errore precedente perchè se le viste non sono 
      // supportate si genera un errore
      int Prev_GS_ERR_COD = GS_ERR_COD;

      if (gsc_OpenSchema(pConn, adSchemaViews, pRs, &varCriteria, ONETEST, GS_BAD) == GS_GOOD)
      {
         while (gsc_isEOF(pRs) == GS_BAD)
         {
            if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { res = GS_BAD; break; }
            if ((pItem = new C_STR()) == NULL)
               { GS_ERR_COD = eGSOutOfMem; res = GS_BAD; break; }

            pItem->paste(get_FullRefTable((pCatalog->restype == RTSTR) ? pCatalog->resval.rstring : NULL,
                                          (pSchema->restype == RTSTR) ? pSchema->resval.rstring : NULL,
                                          pTable->resval.rstring));
            ItemList.add_tail(pItem);
            gsc_Skip(pRs);
         }
         gsc_DBCloseRs(pRs);
      }

      GS_ERR_COD = Prev_GS_ERR_COD;
   }

   return res;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ExistTable                    <external> */
/*+
  Questa verifica l'esistenza di una tabella.
  Parametri: 
  const TCHAR *FullRefTable;     Riferimento completo tabella
  int retest;                    Se MORETESTS -> in caso di errore riprova n volte 
                                 con i tempi di attesa impostati poi ritorna GS_BAD,
                                 ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ExistTable(C_STRING &FullRefTable, int retest)
   { return ExistTable(FullRefTable.get_name(), retest); }
int C_DBCONNECTION::ExistTable(const TCHAR *FullRefTable, int retest)
{
   C_STRING   Catalog, Schema, Table, TempFullTabName;
   C_STR_LIST ItemList;
   
   if (split_FullRefTable(FullRefTable, Catalog, Schema, Table) == GS_BAD)
      return GS_BAD;

   if (getTableAndViewList(ItemList, Catalog.get_name(), Schema.get_name(), 
                           Table.get_name(), true, NULL, true, retest) == GS_BAD)
      return GS_BAD;

   // se non è stato specificato nè catalogo nè schema
   if (Catalog.get_name() == NULL && Schema.get_name() == NULL)
   {
      //C_STR *pItem;
      
      // La lista delle tabelle è letta dal catalogo o dallo schema di default
   }

   TempFullTabName.paste(get_FullRefTable(Catalog, Schema, Table));
   // Se il nome della tabella non importa sia scritto in maiuscolo o minuscolo
   if (get_StrCaseFullTableRef() == NoSensitive)
      return (ItemList.search_name(TempFullTabName.get_name(), GS_BAD)) ? GS_GOOD : GS_BAD;
   else
      return (ItemList.search_name(TempFullTabName.get_name(), GS_GOOD)) ? GS_GOOD : GS_BAD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ReadStruct                    <external> */
/*+
  Questa funzione legge la struttura di una tabella.
  Parametri: 
  const TCHAR *TableRef;     Riferimento completo tabella
  int retest;                Se MORETESTS -> in caso di errore riprova n volte 
                             con i tempi di attesa impostati poi ritorna GS_BAD,
                             ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
presbuf C_DBCONNECTION::ReadStruct(const TCHAR *TableRef, int retest)
{
   C_STRING Catalog, Schema, Table;
   presbuf  p;
  
   if (split_FullRefTable(TableRef, Catalog, Schema, Table) == GS_BAD) return NULL;

   // provo la lettura della struttura usando gli schemi ADO
   p = gsc_ReadStruct(pConn, Table.get_name(), Catalog.get_name(), Schema.get_name(), retest);

   if (!p)
   {
      // alcuni provider OLE-DB (vedi ODBC per EXCEL) non supportano questa tecnica
      // provo a eseguire una istruzione tipo "SELECT * FROM ..."
      C_STRING      statement;
      C_RB_LIST     Structure;
      _RecordsetPtr pRs;
      FieldPtr      pFld;
      BSTR          pbstr;
      long          nFlds, i;

      statement = _T("SELECT * FROM ");
      statement += TableRef;
      if (OpenRecSet(statement, pRs, adOpenForwardOnly, adLockReadOnly, ONETEST) == GS_BAD)
         { GS_ERR_COD = eGSTableNotExisting; return NULL; }
      if ((nFlds = pRs->Fields->Count) == 0)
         { GS_ERR_COD = eGSTableNotExisting; return NULL; }

      if ((Structure << acutBuildList(RTLB, 0)) == NULL) return GS_BAD;

   	try
	   {
         for (i = 0; i < nFlds; i++)
         {
            pFld = pRs->Fields->GetItem(i);

            // Nome
            pFld->get_Name(&pbstr);
            if ((Structure += acutBuildList(RTLB, RTSTR, pbstr, 0)) == NULL)
               { return GS_BAD; }
            SysFreeString(pbstr);

            // Tipo
            if ((Structure += acutBuildList(RTLONG, (long) pFld->GetType(), 0)) == NULL)
               return GS_BAD;
            // Size
            if ((Structure += acutBuildList(RTLONG, pFld->GetDefinedSize(), 0)) == NULL)
               return GS_BAD;
            // Dec
            if ((Structure += acutBuildList(RTSHORT, (int) pFld->GetNumericScale(), 0)) == NULL)
               return GS_BAD;
            // isLong, isFixedPrecScale, isFixedLength e isWriteable
            if ((Structure += acutBuildList(RTSHORT, 0,
                                            RTSHORT, 0,
                                            RTVOID, 
                                            RTT, RTLE, 0)) == NULL) return GS_BAD;
         }
      }

	   catch (_com_error)
         { GS_ERR_COD = eGSTableNotExisting; return NULL; }
      
      if ((Structure += acutBuildList(RTLE, 0)) == NULL) return GS_BAD;
      Structure.ReleaseAllAtDistruction(GS_BAD);
      p = Structure.get_head();
   }

   return p;
}


/****************************************************************/
/*.doc C_DBCONNECTION::ExeCmd                        <external> */
/*+
  Questa funzione esegue un comando SQL.
  Parametri: 
  C_STRING &statement;        Istruzione SQL
  long     *RecordsAffected;  Numero di record aggiornati dal comando (default = NULL)
  int      retest;            Se MORETESTS -> in caso di errore riprova n volte 
                              con i tempi di attesa impostati poi ritorna GS_BAD,
                              ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
  int PrintError;             Utilizzato solo se <retest> = ONETEST. 
                              Se il flag = GS_GOOD in caso di errore viene
                              stampato il messaggio relativo altrimenti non
                              viene stampato alcun messaggio (default = GS_GOOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ExeCmd(C_STRING &statement, long *RecordsAffected, int retest,
                           int PrintError)
{
   return gsc_ExeCmd(pConn, statement.get_name(), RecordsAffected, retest, PrintError);
}


/****************************************************************/
/*.doc C_DBCONNECTION::ExeCmd                        <external> */
/*+
  Questa funzione esegue un comando SQL.
  Parametri: 
  C_STRING &statement;            Istruzione SQL
  _RecordsetPtr &pRs;             Recorset
  enum CursorTypeEnum CursorType; Tipo di cursore (default = adOpenForwardOnly)
  enum LockTypeEnum LockType;     Tipo di Lock (default = adLockReadOnly)
  int retest;                     Se MORETESTS -> in caso di errore riprova n volte 
                                  con i tempi di attesa impostati poi ritorna GS_BAD,
                                  ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                                  (default = MORETESTS)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::ExeCmd(C_STRING &statement, _RecordsetPtr &pRs,
                           enum CursorTypeEnum CursorType, enum LockTypeEnum LockType,
                           int retest)
{
   return gsc_ExeCmd(pConn, statement, pRs, CursorType, LockType, retest);
}


/****************************************************************/
/*.doc C_DBCONNECTION::PrepareCmd                        <external> */
/*+
  Questa funzione prepara un comando SQL.
  Parametri: 
  C_STRING &statement;           Istruzione SQL
  _CommandPtr &pCmd;             Comando preparato e compilato

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
int C_DBCONNECTION::PrepareCmd(C_STRING &statement, _CommandPtr &pCmd)
{
   return gsc_PrepareCmd(pConn, statement, pCmd);
}


/*********************************************************/
/*.doc C_DBCONNECTION::CreateTable            <external> /*
/*+
  Questa funzione crea una nuova tabella.
  Parametri:
  const TCHAR *TableRef;  Riferimento completo della tabella
                          (catalogo.schema.tabella)
  presbuf DefTab;         lista di definizione tabella 
                          ((<nome campo><tipo campo>[<param1>[<param n>...]])...)
  int retest;             se MORETESTS -> in caso di errore riprova n volte 
                          con i tempi di attesa impostati poi ritorna GS_BAD,
                          ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                          (default = MORETESTS)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::CreateTable(const TCHAR *TableRef, presbuf DefTab, int retest)
{
   C_RB_LIST       Structure;
   presbuf         p;
   C_STRING        Name, Params;
   int             i = 0, Qty;
   C_PROVIDER_TYPE *pProviderType;

   // il catalogo rappresenta una path di un file o un direttorio
   if (get_CatalogResourceType() == DirectoryRes || 
       get_CatalogResourceType() == FileRes ||
       get_CatalogResourceType() == WorkbookRes)
   {
      C_STRING Catalog, Schema, Table;

      if (split_FullRefTable(TableRef, Catalog, Schema, Table) == GS_BAD) return GS_BAD;
      if (CreateDB(Catalog.get_name()) == GS_BAD) return GS_BAD;
   }

   // inserisco i delimitatori ai nomi dei campi
   Structure << gsc_rblistcopy(DefTab);
   while ((p = Structure.nth(i++)))
   {
      // nome
      Name = p->rbnext->resval.rstring;
      if (gsc_AdjSyntax(Name, get_InitQuotedIdentifier(), get_FinalQuotedIdentifier(),
                        get_StrCaseFullTableRef()) == GS_BAD) return GS_BAD;
      gsc_RbSubst(p->rbnext, Name.get_name());

      // parametri
      p = p->rbnext->rbnext; // tipo
      if (p->rbnext->restype != RTLE) // se c'è la dimensione
      {
         if (p->restype != RTSTR) // descrizione del tipo
            { GS_ERR_COD = eGSUnkwnDrvFieldType; return GS_BAD; }

         if ((pProviderType = (C_PROVIDER_TYPE *) ptr_DataTypeList()->search_name(p->resval.rstring, FALSE)) == NULL)
            { GS_ERR_COD = eGSUnkwnDrvFieldType; return GS_BAD; }

         Params = pProviderType->get_CreateParams();
         Qty = (Params.len() > 0) ? gsc_strsep(Params.get_name(), _T('\0')) + 1 : 0;
         switch (Qty)
         {
            case 0:
               gsc_RbSubst(p->rbnext, (long) -1); // dimensione
            case 1:
               if (p->rbnext->rbnext->restype != RTLE) // se ci sono i decimali
                  gsc_RbSubst(p->rbnext->rbnext, (int) -1);
         }
      }
   }

   return gsc_CreateTable(pConn, TableRef, Structure.get_head(), retest);
}


/*********************************************************/
/*.doc C_DBCONNECTION::DelTable               <external> */
/*+
  Questa funzione cancella un indice di una tabella.
  Parametri:
  const TCHAR *TableRef;  Riferimento completo della tabella
  int retest;             Se MORETESTS -> in caso di errore riprova n volte 
                          con i tempi di attesa impostati poi ritorna GS_BAD,
                          ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                          (default = MORETESTS)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::DelTable(const TCHAR *TableRef, int retest)
{
   return gsc_DelTable(pConn, TableRef, retest);
}


/*********************************************************/
/*.doc C_DBCONNECTION::CopyTable              <external> */
/*+
  Questa funzione copia una tabella usando la stessa connessione OLE-DB.
  Parametri:
  const TCHAR *SourceTableRef; Riferimento completo tabella sorgente con eventuali
                               delimitatori di nome già inclusi (es [] per access)
  const TCHAR *DestTableRef;   Riferimento completo tabella destinazione con eventuali
                               delimitatori di nome già inclusi (es [] per access)
  DataCopyTypeEnum Mode;       Tipo di copia (GSStructureOnlyCopyType = solo struttura,
                                              GSStructureAndDataCopyType = tutto)
  int        Index;            Se = GS_GOOD copia anche gli indici
                               (default = GS_GOOD)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B.
  Se la creazione della tabella avviene tramite odbc per PostgreSQL, il nome
  della tabella NON deve incominciare per "pg_" e nemmeno per i prefissi indicati 
  nelle opzioni del driver alla voce "SysTable Prefixes" altrimenti la struttura viene
  alterata e aggiunta una colonna "OID".
-*/  
/*********************************************************/
int C_DBCONNECTION::CopyTable(const TCHAR *SourceTableRef, const TCHAR *DestTableRef,
                              DataCopyTypeEnum Mode, int Index)
{
   C_STRING  Stm;
   presbuf   p;
   C_RB_LIST IndexList;

   if (Index == GS_GOOD)
   {
      // memorizzo gli indici utilizzati dalla tabella originale
      if (getTableIndexes(SourceTableRef, &p) == GS_BAD) return GS_BAD;
      IndexList << p;
   }

   if (Mode == GSStructureOnlyCopyType)
   {
      if (DBMSName.at(ACCESS_DBMSNAME, FALSE) != NULL) // è in SQL Jet (Microsoft Access)
      {
         Stm = _T("SELECT * INTO ");
         Stm += DestTableRef;
         Stm += _T(" FROM ");
         Stm += SourceTableRef;
         Stm += _T(" WHERE 1<0"); // con condizione impossibile per fare solo struttura
      }
      else if (DBMSName.at(PG_DBMSNAME, FALSE) != NULL || // è ORACLE o PostgreSQL
               DBMSName.at(ORACLE_DBMSNAME, FALSE) != NULL)
      {
         // provo con la sintassi ORACLE, POSTGRESQL
         // 
         Stm = _T("CREATE TABLE ");
         Stm += DestTableRef;
         Stm += _T(" AS SELECT * FROM ");
         Stm += SourceTableRef;
         Stm += _T(" WHERE 1<0"); // con condizione impossibile per fare solo struttura
      }
      else
         return GS_BAD;

      return gsc_ExeCmd(pConn, Stm, NULL, ONETEST, GS_BAD);
   }
   else
   {
      if (DBMSName.at(ACCESS_DBMSNAME, FALSE) != NULL) // è in SQL Jet (Microsoft Access)
      {
         Stm = _T("SELECT * INTO ");
         Stm += DestTableRef;
         Stm += _T(" FROM ");
         Stm += SourceTableRef;
      }
      else if (DBMSName.at(PG_DBMSNAME, FALSE) != NULL || // è in ORACLE e PostgreSQL
               DBMSName.at(ORACLE_DBMSNAME, FALSE) != NULL)
      {
         Stm = _T("CREATE TABLE ");
         Stm += DestTableRef;
         Stm += _T(" AS SELECT * FROM ");
         Stm += SourceTableRef;
      }
                
      if (Stm.get_name())
         return gsc_ExeCmd(pConn, Stm, NULL, ONETEST, GS_BAD);

      // Merant non supporta queste sintassi
      if (get_CatalogResourceType() == DirectoryRes)
      {  // se la tabella è un file copio il file
         C_STRING Catalog, Schema, Table, Src, Dst;

         if (split_FullRefTable(SourceTableRef, Catalog, Schema, Table) == GS_BAD)
            return GS_BAD;
         if (gsc_RefTable2Path(get_DBMSName(), Catalog, Schema, Table, Src) == GS_BAD)
            return GS_BAD;

         if (split_FullRefTable(DestTableRef, Catalog, Schema, Table) == GS_BAD)
            return GS_BAD;
         if (gsc_RefTable2Path(get_DBMSName(), Catalog, Schema, Table, Dst) == GS_BAD)
            return GS_BAD;

         // Copio file
         if (gsc_copyfile(Src, Dst) == GS_BAD) return GS_BAD;
      }
      else
         return GS_BAD;
   }

   if (Index == GS_GOOD)
      if (CreateIndex(IndexList.get_head(), DestTableRef) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBCONNECTION::CopyTable              <external> */
/*+
  Questa funzione copia una tabella usando diverse connessioni OLE-DB.
  Parametri:
  const TCHAR    *SourceTableRef; Riferimento completo tabella sorgente
  C_DBCONNECTION &DestConn;       Connessione OLE-DB destinazione
  const TCHAR    *DestTableRef;   Riferimento completo tabella destinazione
  DataCopyTypeEnum Mode;          Tipo di copia (GSStructureOnlyCopyType = solo struttura,
                                                 GSStructureAndDataCopyType = tutto)
  int            Index;           Se = GS_GOOD copia anche gli indici e la chiave primaria
                                  (default = GS_GOOD)
  int CounterToVideo;             Flag, se = GS_GOOD stampa a video il numero di record
                                  che si stanno elaborando (default = GS_BAD)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::CopyTable(const TCHAR *SourceTableRef,
                              C_DBCONNECTION &DestConn, const TCHAR *DestTableRef,
                              DataCopyTypeEnum Mode, int Index, int CounterToVideo)
{
   C_RB_LIST     SourceStru, DestStru;
   presbuf       pRB;
   int           i, DestProviderPrec, Result = GS_GOOD;
   C_STRING      DestProviderDescr, statement;
   long          DestProviderSize;
   _RecordsetPtr pReadRs, pInsRs;
   presbuf       p;
   C_RB_LIST     IndexList;

   if (Index == GS_GOOD)
   {
      // Indici
      C_STRING Index, Catalog, Schema, IndexRef;
      C_STRING DstCatalog, DstSchema;

      if (DestConn.split_FullRefTable(DestTableRef, DstCatalog, DstSchema, Index) == GS_BAD)
         return GS_BAD;

      // memorizzo gli indici utilizzati dalla tabella originale 
      // (con eventuale catalogo e schema, modo ERASE)
      if (getTableIndexes(SourceTableRef, &p, ERASE) == GS_BAD) return GS_BAD;
      IndexList << p;
      // ciclo sui nomi degli indici da cambiare secondo la sintassi
      // del database di destinazione
      i = 0;
	   while ((p = IndexList.nth(i++)) != NULL)
	   {
         p = gsc_nth(0, p); // nome indice

         if (split_FullRefTable(p->resval.rstring, Catalog, Schema, Index) == GS_BAD)
            return GS_BAD;

		   if (IndexRef.paste(DestConn.get_FullRefIndex(DstCatalog.get_name(),
                                                      DstSchema.get_name(),
												                  Index.get_name(),
												                  Definition)) == NULL)
			   return GS_BAD;
         gsc_RbSubst(p, IndexRef.get_name());
	   }
   }

   // leggo la struttura tabella sorgente
   // ((<Name><Type><Dim><Dec><IsLong><IsFixedPrecScale><Write>)...)
   if ((SourceStru << ReadStruct(SourceTableRef, ONETEST)) == NULL) return GS_BAD;

   if ((DestStru << acutBuildList(RTLB, 0)) == NULL) return GS_BAD;

   i = 0;
   while ((pRB = SourceStru.nth(i++)) != NULL)
   {
      // converto tipo nel formato del provider di destinazione
      if (DestConn.Type2ProviderType((DataTypeEnum) gsc_nth(1, pRB)->resval.rlong,     // tipo
                                     (gsc_nth(4, pRB)->restype == RTT) ? TRUE : FALSE, // is long
                                     (gsc_nth(5, pRB)->restype == RTT) ? TRUE : FALSE, // is PrecScale
                                     gsc_nth(6, pRB)->restype,                         // IsFixedLength
                                     (gsc_nth(7, pRB)->restype == RTT) ? TRUE : FALSE, // IsAutoUniqueValue
                                     gsc_nth(2, pRB)->resval.rlong,                    // dimensione
                                     gsc_nth(3, pRB)->resval.rint,                     // precisione
                                     DestProviderDescr,
                                     &DestProviderSize,
                                     &DestProviderPrec) == GS_BAD) return GS_BAD;

      if ((DestStru += acutBuildList(RTLB, 
                                           RTSTR, gsc_nth(0, pRB)->resval.rstring,
                                           RTSTR, DestProviderDescr.get_name(),
                                           RTLONG, DestProviderSize,
                                           RTSHORT, DestProviderPrec,
                                     RTLE, 0)) == NULL) return GS_BAD;
   }

   if ((DestStru += acutBuildList(RTLE, 0)) == NULL) return GS_BAD;

   int IsTransactionSupported = GS_BAD;
   //int IsTransactionSupported = DestConn.BeginTrans(); // roby prova
   if (DestConn.CreateTable(DestTableRef, DestStru.get_head(), ONETEST) == GS_BAD)
      { if (IsTransactionSupported == GS_GOOD) DestConn.RollbackTrans(); return GS_BAD; }

   if (Mode == GSStructureAndDataCopyType)
   {
      if (DestConn.InitInsRow(DestTableRef, pInsRs) == GS_BAD)
         { if (IsTransactionSupported == GS_GOOD) DestConn.RollbackTrans(); return GS_BAD; }

      statement = _T("SELECT * FROM ");
      statement += SourceTableRef;
      if (OpenRecSet(statement, pReadRs, adOpenForwardOnly, adLockReadOnly, ONETEST) == GS_BAD)
         { if (IsTransactionSupported == GS_GOOD) DestConn.RollbackTrans(); return GS_BAD; }

      Result = gsc_DBInsRowSet(pInsRs, pReadRs, ONETEST, GS_GOOD, CounterToVideo);

      gsc_DBCloseRs(pReadRs);
      gsc_DBCloseRs(pInsRs);
   }

   // solo alla fine faccio gli indici per velocizzare 
   // l'eventuale operazione di copia records
   if (Index == GS_GOOD)
      Result = DestConn.CreateIndex(IndexList.get_head(), DestTableRef);

   if (Result == GS_BAD)
   {
      if (IsTransactionSupported == GS_GOOD) DestConn.RollbackTrans();
      return GS_BAD;
   }
   else
   {
      if (IsTransactionSupported == GS_GOOD) DestConn.CommitTrans();
      return GS_GOOD;
   }
}


/*********************************************************/
/*.doc C_DBCONNECTION::getNewFullRefIndex     <external> */
/*+
  Questa funzione restituisce un nome di indice univoco in modo che 
  non vi siano altre tabelle o indici con lo stesso nome. Alcuni DBMS
  non supportano il nome dell'indice uguale a quello della tabella 
  (PostgreSQL).
  Parametri:
  const TCHAR *Catalog; Catalogo
  const TCHAR *Schema;  Schema
  const TCHAR *Name;    Nome base dell'indice
  const TCHAR *Suffix;  Suffisso del nome indice
  short Reason;         Flag per sapere quale operazione si intende
                        fare sugli indici. INSERT se si vuole creare 
                        un indice, ERASE se si vuole cancellare un indice
                        NONE negli altri casi (default NONE). Il flag è stato
                        creato per questi DBMS (es. PostgreSQL) che in 
                        creazione vogliono solo il nome dell'indice mentre
                        in cancellazione vogliono anche lo schema.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
DllExport TCHAR *C_DBCONNECTION::getNewFullRefIndex(const TCHAR *Catalog,
                                                    const TCHAR *Schema,
                                                    const TCHAR *Name, 
                                                    const TCHAR *Suffix,
                                                    short Reason)
{
   C_STRING IndexName(Name);

   if (get_IsUniqueIndexName() && Suffix)
   {
      C_STRING AdjSuffix(Suffix);
      size_t   i, Len = AdjSuffix.len();
   	TCHAR    ch;

      // Sostituisco tutti i caratteri che non sono alfanumerici o '_'
      // con il carattere '_'
      for (i = 0; i < Len; i++)
      {
         ch = AdjSuffix.get_chr(i);
         if (iswalnum(ch) == 0 && ch != _T('_')) AdjSuffix.set_chr(_T('_'), i);
      }

      IndexName = Name;
      IndexName += AdjSuffix;
      if ((int) IndexName.len() > get_MaxTableNameLen()) // nome indice troppo lungo
         IndexName.set_chr(_T('\0'), get_MaxTableNameLen()); // tronco il nome
   }

   return get_FullRefIndex(Catalog, Schema, IndexName.get_name(), Definition, Reason);
}


/*********************************************************/
/*.doc C_DBCONNECTION::CreateIndex            <external> */
/*+
  Questa funzione crea un indice di una tabella. Riceve il nome dell'indice,
  il nome della tabella e la chiave di indicizzazione.
  Parametri:
  const TCHAR *indexname;  nome dell'indice
  const TCHAR *TableRef;   Riferimento completo della tabella
  const TCHAR *keyespr;    chiave di indicizzazione
  IndexTypeEnum IndexMode; tipo di indice da creare (default = INDEX_NOUNIQUE)
  int retest;              se MORETESTS -> in caso di errore riprova n volte 
                           con i tempi di attesa impostati poi ritorna GS_BAD,
                           ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                           (default = MORETESTS)
  oppure
  presbuf IndexList;       Lista degli indici da creare (vedi "gsc_getTableIndexes")
  const TCHAR *TableRef;   Nome della tabella
  int retest;              se MORETESTS -> in caso di errore riprova n volte 
                           con i tempi di attesa impostati poi ritorna GS_BAD,
                           ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                           (default = MORETESTS)
  int PrintError;          Utilizzato solo se <retest> = ONETEST. 
                           Se il flag = GS_GOOD in caso di errore viene
                           stampato il messaggio relativo altrimenti non
                           viene stampato alcun messaggio (default = GS_GOOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::CreateIndex(const TCHAR *indexname, const TCHAR *TableRef,
                                const TCHAR *KeyEspr, IndexTypeEnum IndexMode,
                                int retest, int PrintError)
{
   // Se si vuole creare una chiave primaria
   if (IndexMode == PRIMARY_KEY)
   {
      C_STRING Catalog, Schema, ConstraintName;

      if (split_FullRefTable(indexname, Catalog, Schema, ConstraintName) == GS_BAD) return GS_BAD;
      return CreatePrimaryKey(TableRef, ConstraintName.get_name(),
                              KeyEspr, retest, PrintError);
   }

   C_STRING dummy(KeyEspr), statement;

   if (!wcschr(KeyEspr, _T(','))) // KeyEspr è un nome di attributo
      // Poichè KeyEspr è un nome di attributo senza delimitatori devo aggiungerli
      // per non avere errori di sintassi (es. nome attributo con uno spazio)
      if (gsc_AdjSyntax(dummy, get_InitQuotedIdentifier(), get_FinalQuotedIdentifier(),
                        get_StrCaseFullTableRef()) == GS_BAD) return GS_BAD;

   statement = _T("CREATE ");
   if (IndexMode == INDEX_UNIQUE || IndexMode == SPATIAL_INDEX_UNIQUE)
      statement += _T("UNIQUE ");
   statement += _T("INDEX ");
   statement += indexname;
   statement += _T(" ON ");
   statement += TableRef;

   // Se si vuole creare un indice spaziale ed esiste una parola chiave SQL
   // specifica del provider (non SQL standard)
   if ((IndexMode == SPATIAL_INDEX_UNIQUE || IndexMode == SPATIAL_INDEX_NOUNIQUE) &&
       SpatialIndexCreation_SQLKeyWord.len() > 0)
   {
      TCHAR  *pStr;
      size_t Len = SpatialIndexCreation_SQLKeyWord.len();

      Len += (long) dummy.len();
      Len += 1;
      if ((pStr = (TCHAR *) malloc(sizeof (TCHAR) * Len)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

      swprintf(pStr, Len, get_SpatialIndexCreation_SQLKeyWord(), dummy.get_name());
      statement += _T(' ');
      statement += pStr;
   }
   else
   {
      statement += _T(" (");
      statement += dummy;
      statement += _T(")");
   }

   if (gsc_ExeCmd(pConn, statement, NULL, retest, PrintError) == GS_BAD)
      { GS_ERR_COD = eGSCreateIndex; return GS_BAD; }

   return GS_GOOD;
}
int C_DBCONNECTION::CreateIndex(presbuf IndexList, const TCHAR *TableRef, int retest)
{
   C_RB_LIST     _IndexList;
   presbuf       p, pKey;
   int           i = 0, j;
   IndexTypeEnum IndexMode;
   C_STRING      Index, IndexKey;  

   _IndexList << gsc_rblistcopy(IndexList);
   while ((p = _IndexList.nth(i++)))
   {
      Index = p->rbnext->resval.rstring;

      // riutilizzo <First> come flag per indice univoco
      gsc_rb2Int(gsc_nth(1, p), (int *) &IndexMode);
      
      // riutilizzo <statement> come chiave di indicizzazione
      p = gsc_nth(2, p);
      j = 0;
      IndexKey.clear();
      while ((pKey = gsc_nth(j++, p)) != NULL) // riutilizzo <pField>
      {
         if (j > 1) IndexKey += _T(',');
         IndexKey += pKey->resval.rstring;
      }
      if (CreateIndex(Index.get_name(), TableRef, IndexKey.get_name(), 
                      IndexMode, retest) == GS_BAD) return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBCONNECTION::DelIndex               <external> */
/*+
  Questa funzione cancella un indice di una tabella.
  Parametri:
  const TCHAR *TableRef;   Riferimento della tabella

  const TCHAR *IndexRef;   Riferimento completo indice
  oppure
  presbuf IndexList;      Lista degli indici da creare (vedi "gsc_getTableIndexes")

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::DelIndex(C_STRING &TableRef, C_STRING &IndexRef)
{
   return DelIndex(TableRef.get_name(), IndexRef.get_name());
}
int C_DBCONNECTION::DelIndex(const TCHAR *TableRef, const TCHAR *IndexRef)
{
   C_STRING Stm;

   if (DBMSName.at(ACCESS_DBMSNAME, FALSE) != NULL) // è in SQL Jet (Microsoft Access)
   {
      // DROP INDEX index_name ON table_name
      Stm = _T("DROP INDEX ");
      Stm += IndexRef;
      Stm += _T(" ON ");
      Stm += TableRef;
   }
   else if (DBMSName.at(PG_DBMSNAME, FALSE) != NULL) // è in PostgreSQL
   {
      // DROP INDEX index_name
      Stm = _T("DROP INDEX IF EXISTS ");
      Stm += IndexRef;
   }
   else if (DBMSName.at(ORACLE_DBMSNAME, FALSE) != NULL) // è in ORACLE
   {
      // DROP INDEX index_name
      Stm = _T("DROP INDEX ");
      Stm += IndexRef;
   }
   //else if (DBMSName.at( , FALSE) != NULL) // è in MS SQL Server
   //{
   //   // DROP INDEX table_name.index_name
   //   Stm = _T("DROP INDEX ");
   //   Stm += TableRef;
   //   Stm += _T('.');
   //   Stm += IndexRef;
   //}
   //else if (DBMSName.at( , FALSE) != NULL) // è in MySQL
   //{
   //   // ALTER TABLE table_name DROP INDEX index_name
   //   Stm = _T("ALTER TABLE ");
   //   Stm += TableRef;
   //   Stm += _T(" DROP INDEX ");
   //   Stm += IndexRef;
   //}

   if (Stm.get_name())
      return gsc_ExeCmd(pConn, Stm, NULL, ONETEST, GS_BAD);

   Property   *pResult;
   _variant_t PropName(_T("Catalog Term"));

   if (pConn->Properties->get_Item(PropName, &pResult) != S_OK) return GS_BAD;

   if (gsc_strcmp(((TCHAR *)_bstr_t(pResult->GetValue())), _T("DIRECTORY"), FALSE) == 0)
   {  // se il catalogo è un direttorio allora provo con la sintassi di MERANT
      // <path file indice (.MDX")>.<nome indice>
      C_STRING Catalog, Schema, Table, dummy, Index;

      if (split_FullRefTable(IndexRef, Catalog, Schema, Index) == GS_BAD)
         return GS_BAD;
      
      if (split_FullRefTable(TableRef, Catalog, Schema, Table) == GS_BAD)
         return GS_BAD;
      dummy = _T('`');
      dummy += Catalog;
      dummy += _T('\\');
      dummy += Table;
      dummy += _T('`');
      dummy += _T('.');
      dummy += Index;

      // DROP INDEX index_name ON table_name
      Stm = _T("DROP INDEX ");
      Stm += dummy;
      Stm += _T(" ON ");
      Stm += TableRef;
   
      return gsc_ExeCmd(pConn, Stm, NULL, ONETEST, GS_BAD);
   }
   else
      return GS_BAD;

   return GS_GOOD;
}
int C_DBCONNECTION::DelIndex(const TCHAR *TableRef, presbuf IndexList)
{
   C_RB_LIST _IndexList;
   presbuf   p;
   int       i = 0;

   _IndexList << gsc_rblistcopy(IndexList);
   while ((p = _IndexList.nth(i++)))
      if (DelIndex(TableRef, p->rbnext->resval.rstring) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::getTableIndexes               <external> */
/*+
  Questa funzione restituisce una lista degli indici di una tabella nella
  seguente forma di lista di resbuf:
  ((<nome indice><tipo di indice>(<campo1><campo2>...)...)

  Parametri: 
  const TCHAR *FullRefTable;  Riferimento completo tabella
  presbuf *pIndexList;        Lista degli indici
  short Reason;               Flag per sapere quale operazione si intende
                              fare sugli indici. INSERT se si vuole creare 
                              un indice, ERASE se si vuole cancellare un indice
                              NONE negli altri casi (default NONE). Il flag è stato
                              creato per questi DBMS (es. PostgreSQL) che in 
                              creazione vogliono solo il nome dell'indice mentre
                              in cancellazione vogliono anche lo schema.
  int retest;                 Se MORETESTS -> in caso di errore riprova n volte 
                              con i tempi di attesa impostati poi ritorna GS_BAD,
                              ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
  
  Restituisce GS_GOOD in caso di successo, GS_CAN se non è possibile avere 
  queste informazioni altrimenti GS_BAD in caso di errore.
-*/  
/****************************************************************/
int C_DBCONNECTION::getTableIndexes(const TCHAR *FullRefTable, presbuf *pIndexList,
                                    short Reason, int retest)
{
   C_STRING Catalog, Schema, Table, dummy;
	presbuf  p;
	int		i = 0, res;
   
   if (split_FullRefTable(FullRefTable, Catalog, Schema, Table) == GS_BAD)
      return GS_BAD;

   if ((res = gsc_getTableIndexes(pConn, Table.get_name(), Catalog.get_name(), 
                                  Schema.get_name(), pIndexList, retest)) != GS_GOOD)
      return res;

	while ((p = gsc_nth(i++, *pIndexList)) != NULL)
	{
      p = gsc_nth(0, p); // nome indice
		if (dummy.paste(get_FullRefIndex(Catalog.get_name(),
                                       Schema.get_name(),
												   p->resval.rstring,
												   Definition,
                                       Reason)) == NULL)
			return GS_BAD;
      gsc_RbSubst(p, dummy.get_name());
	}

	return GS_GOOD;
}


/****************************************************************/
/*.doc C_DBCONNECTION::getTablePrimaryKey            <external> */
/*+
  Questa funzione restituisce una descrizione della chiave primaria 
  di una tabella nella seguente forma di lista di resbuf:
  (<nome chiave primaria>(<campo1><campo2>...))

  Parametri: 
  const TCHAR *FullRefTable;  Riferimento completo tabella
  presbuf *pPKDescr;          Descrizione chiave primaria (il nome non è con catalogo e schema)
  int retest;                 Se MORETESTS -> in caso di errore riprova n volte 
                              con i tempi di attesa impostati poi ritorna GS_BAD,
                              ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
  
  Restituisce GS_GOOD in caso di successo, GS_CAN se non è possibile avere 
  queste informazioni altrimenti GS_BAD in caso di errore.
  N.B.: Alloca memoria.
-*/
/****************************************************************/
int C_DBCONNECTION::getTablePrimaryKey(const TCHAR *FullRefTable, presbuf *pPKDescr,
                                       int retest)
{
   C_STRING Catalog, Schema, Table;
   
   if (split_FullRefTable(FullRefTable, Catalog, Schema, Table) == GS_BAD)
      return GS_BAD;

   return gsc_getTablePrimaryKey(pConn, Table.get_name(), Catalog.get_name(), 
                                 Schema.get_name(), pPKDescr, retest);
}


/*********************************************************/
/*.doc C_DBCONNECTION::Reindex                <external> */
/*+
  Questa funzione cancella e ricostruisce gli indici di una tabella.
  Parametri:
  const TCHAR *TableRef;   Riferimento alla tabella
  int retest;              se MORETESTS -> in caso di errore riprova n volte 
                           con i tempi di attesa impostati poi ritorna GS_BAD,
                           ONETEST -> in caso di errore ritorna GS_BAD senza riprovare

  Restituisce GS_GOOD in caso di successo, altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::Reindex(const TCHAR *TableRef, int retest)
{
   // i nomi degli indici possono avere una sintassi diversa 
   // a seconda che li si voglia creare o cancellare
   presbuf IndexList;
   presbuf IndexListToDrop;

   // memorizzo gli indici utilizzati dalla tabella originale
   if (getTableIndexes(TableRef, &IndexList) == GS_GOOD && IndexList)
   {
      getTableIndexes(TableRef, &IndexListToDrop, ERASE);
      if (DelIndex(TableRef, IndexListToDrop) == GS_BAD)
         { acutRelRb(IndexList); acutRelRb(IndexListToDrop); return GS_BAD; }
      if (CreateIndex(IndexList, TableRef) == GS_BAD)
         { acutRelRb(IndexList); acutRelRb(IndexListToDrop); return GS_BAD; }
      acutRelRb(IndexList);
      acutRelRb(IndexListToDrop);
   }
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBCONNECTION::CreatePrimaryKey       <external> */
/*+
  Questa funzione crea una indice di chiave primaria di una tabella.
  Riceve il nome della tabella e la chiave di indicizzazione.
  Parametri:
  const TCHAR *TableRef; Riferimento completo della tabella
  const TCHAR *ConstraintName; Nome dell'indice di chiave primaria
  const TCHAR *keyespr;  chiave di indicizzazione
  int retest;            se MORETESTS -> in caso di errore riprova n volte 
                         con i tempi di attesa impostati poi ritorna GS_BAD,
                         ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                         (default = MORETESTS)
  int PrintError;        Utilizzato solo se <retest> = ONETEST. 
                         Se il flag = GS_GOOD in caso di errore viene
                         stampato il messaggio relativo altrimenti non
                         viene stampato alcun messaggio (default = GS_GOOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::CreatePrimaryKey(const TCHAR *TableRef, const TCHAR *ConstraintName,
                                     const TCHAR *KeyEspr, int retest, int PrintError)
{
   C_STRING dummy(ConstraintName), statement;

   // Creo chiave primaria (MySQL / SQL Server / Oracle / MS Access / PostgreSQL)
   statement = _T("ALTER TABLE ");
   statement += TableRef;
   statement += _T(" ADD CONSTRAINT ");

   // Poichè ConstraintName è senza delimitatori devo aggiungerli
   // per non avere errori di sintassi (es. ConstraintName con uno spazio)
   if (gsc_AdjSyntax(dummy, get_InitQuotedIdentifier(), get_FinalQuotedIdentifier(),
                     get_StrCaseFullTableRef()) == GS_BAD) return GS_BAD;

   statement += dummy;
   statement += _T(" PRIMARY KEY");
   statement += _T(" (");

   dummy = KeyEspr;
   if (!wcschr(KeyEspr, _T(','))) // KeyEspr è un nome di attributo
      // Poichè KeyEspr è un nome di attributo senza delimitatori devo aggiungerli
      // per non avere errori di sintassi (es. nome attributo con uno spazio)
      if (gsc_AdjSyntax(dummy, get_InitQuotedIdentifier(), get_FinalQuotedIdentifier(),
                        get_StrCaseFullTableRef()) == GS_BAD) return GS_BAD;

   statement += dummy;
   statement += _T(")");

   if (gsc_ExeCmd(pConn, statement, NULL, retest, PrintError) == GS_BAD)
      { GS_ERR_COD = eGSCreateIndex; return GS_BAD; }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBCONNECTION::DelPrimaryKey          <external> */
/*+
  Questa funzione cancella un indice di chiave primaria di una tabella.
  Riceve il nome della tabella.
  Parametri:
  const TCHAR *TableRef; Riferimento completo della tabella
  const TCHAR *ConstraintName; Nome dell'indice di chiave primaria
  int retest;            se MORETESTS -> in caso di errore riprova n volte 
                         con i tempi di attesa impostati poi ritorna GS_BAD,
                         ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                         (default = MORETESTS)
  int PrintError;        Utilizzato solo se <retest> = ONETEST. 
                         Se il flag = GS_GOOD in caso di errore viene
                         stampato il messaggio relativo altrimenti non
                         viene stampato alcun messaggio (default = GS_GOOD)

  Restituisce GS_GOOD in caso di successo, GS_CAN se non è possibile avere 
  queste informazioni altrimenti GS_BAD in caso di errore.
-*/  
/*********************************************************/
int C_DBCONNECTION::DelPrimaryKey(const TCHAR *TableRef, const TCHAR *ConstraintName,
                                  int retest, int PrintError)
{
   C_STRING statement, dummy(ConstraintName);  // istruzione SQL
   presbuf   p;

   // verifica esistenza della chiave primaria
   if (getTablePrimaryKey(TableRef, &p, ERASE) == GS_GOOD)
   {
      C_RB_LIST List;

      List << p;
      if ((p = List.nth(0)) && p->restype == RTSTR)
      {
         if (gsc_strcmp(p->resval.rstring, ConstraintName, FALSE) != 0) return GS_BAD;
      }
      else
         return GS_GOOD; // Non ci sono chiavi primarie
   }

   // Della serie: l'SQL è uno standard internazionale... (di sto cazzo)

   // Correggo la sintassi del nome del campo
   if (gsc_AdjSyntax(dummy, get_InitQuotedIdentifier(), get_FinalQuotedIdentifier(),
                     get_StrCaseFullTableRef()) == GS_BAD) return GS_BAD;  

   if (DBMSName.at(PG_DBMSNAME, FALSE) != NULL) // è in PostgreSQL
   {
      // ALTER TABLE table_name DROP CONSTRAINT primary_key
      statement = _T("ALTER TABLE ");
      statement += TableRef;
      statement += _T(" DROP CONSTRAINT ");
      if (DBMSVersion.toi() >= 9)
         statement += _T("IF EXISTS ");
      statement += dummy;
   }
   else if (DBMSName.at(ACCESS_DBMSNAME, FALSE) != NULL ||  // è in SQL Jet (Microsoft Access)
            DBMSName.at(ORACLE_DBMSNAME, FALSE) != NULL ||  // è in ORACLE
            DBMSName.at(SQLSERVER_DBMSNAME, FALSE) != NULL) // è in SQL Server
   {
      // ALTER TABLE table_name DROP CONSTRAINT primary_key
      statement = _T("ALTER TABLE ");
      statement += TableRef;
      statement += _T(" DROP CONSTRAINT ");
      statement += dummy;
   }
   else if (DBMSName.at(MYSQL_DBMSNAME, FALSE) != NULL) // è in MySQL
   {
      // ALTER TABLE table_name DROP PRIMARY KEY
      statement = _T("ALTER TABLE ");
      statement += TableRef;
      statement += _T(" DROP PRIMARY KEY");
   }      

   return gsc_ExeCmd(pConn, statement, NULL, retest, PrintError);
}


/*********************************************************/
/*.doc C_DBCONNECTION::Type2ProviderDescr     <external> */
/*+
  Questa funzione riceve il codice di un tipo di dato e restituisce
  la decrizione data dal provider.
  Parametri:
  DataTypeEnum DataType;          Tipo di dato
  short        IsLong;            TRUE o FALSE dato lungo
  short        IsFixedPrecScale;  TRUE è di precisione e scala a lunghezza fissa 
                                  altrimenti non è di lunghezza fissa
  short        IsFixedLength;     RTT se di lunghezza fissa a livello di DDL
                                  (Data Definition Language es. "CREATE TABLE ..."
                                  RTNIL non è di lunghezza fissa, RTVOID non si sa.
  short        IsWrite;           TRUE o FALSE scrivibile
  long         Size;              Dimensione
  int          ExactMode;         Flag di ricerca. Se = TRUE la ricerca avviene
                                  cercando rigidamente il tipo <DataType>, se = FALSE
                                  qualora la ricerca del tipo fosse fallita, si procede
                                  alla ricerca del tipo più simile (default = TRUE).
  int          Prec;              Precisione. Parametro usato solo se <ExactMode> = FALSE
                                  (default = 0).

  Restituisce la descrizione in caso di successo altrimenti restituisce NULL.
  N.B.: Alloca memoria.
-*/  
/*********************************************************/
TCHAR *C_DBCONNECTION::Type2ProviderDescr(DataTypeEnum DataType, short IsLong,
                                          short IsFixedPrecScale, short IsFixedLength,
                                          short IsWrite, long Size, 
                                          int ExactMode, int Prec)
{
   short           isAutoUniqueValue = !IsWrite; // se non è scrivibile è autoincrementato
   C_PROVIDER_TYPE *pProviderType = ptr_DataTypeList()->search_Type(DataType, isAutoUniqueValue), *pBestMatched = NULL;
   C_PROVIDER_TYPE *pWrongSizeProviderType = NULL;
   int             nBestMatched = 0, nMatched, TryNumericBySizePrec = TRUE;
   long            BestOffSetSize = -1, OffSetSize;

   while (pProviderType)
   {
      nMatched = 0;

      if (gsc_DBIsNumeric(DataType) == GS_GOOD)
         // Se entrambi di precisione e scala a lunghezza fissa
         if (IsFixedPrecScale == pProviderType->get_IsFixedPrecScale()) nMatched++;

      // Se entrambi a lunghezza fissa a livello di DDL
      // (Data Definition Language es. "CREATE TABLE ..."
      if (IsFixedLength == pProviderType->get_IsFixedLength()) nMatched++;

      // Se entrambi BLOB
      if (IsLong == pProviderType->get_IsLong()) nMatched++;
      // Se entrambi "contatori" di ACCESS
      if (isAutoUniqueValue == pProviderType->get_IsAutoUniqueValue()) nMatched++;
      // differenza tra la dimensione del campo e quella massima ammessa
      OffSetSize = pProviderType->get_Size() - Size;
      
      // Se il tipo trovato non ha dimensioni fisse (virgola mobile)
      if (gsc_DBIsNumeric(DataType) == GS_GOOD && !pProviderType->get_IsFixedPrecScale())
         // allora il tipo trovato può ospitare i valori del tipo originale
         // seppur con una possibile perdita di precisione dovuta all'arrotondamento
         // in questo caso (OffSetSize < 0) pongo un offset molto alto per 
         // privilegiare altri tipi compatibili con un offset più basso
         if (OffSetSize < 0) OffSetSize = 999;

      // questo campo è grande a sufficienza per contenere le informazioni
      if (OffSetSize >= 0)
      {
         // questo campo è quello che si avvicina di più alle caratteristiche del tipo
         if (nBestMatched < nMatched)
         {
            nBestMatched   = nMatched;
            pBestMatched   = pProviderType;
            BestOffSetSize = OffSetSize;
         }
         else
         if (nBestMatched == nMatched)
         {
            bool setNewProviderType = false;
            
            if (BestOffSetSize == -1) // se non è stato ancora inizializzato BestOffSetSize (= -1)
               setNewProviderType = true;
            else
               // nota: per un baco su ACCESS il tipo "datetime" è dichiarato
               // con dimensione = 8 ma leggendo la struttura della tabella ritorna 19
               if (Size == 0) // se size = 0 significa che il provider non sa la dimensione
               {
                  // scelgo il tipo con dimensione maggiore
                  if (BestOffSetSize < OffSetSize) setNewProviderType = true;
               }
               else
               if (BestOffSetSize > OffSetSize) setNewProviderType = true;
            
            if (setNewProviderType)
            {
               nBestMatched   = nMatched;
               pBestMatched   = pProviderType;
               BestOffSetSize = OffSetSize;
            }
         }
      }
      else // questo campo non è grande a sufficienza per contenere le informazioni
         pWrongSizeProviderType = pProviderType;
      
      pProviderType = ptr_DataTypeList()->search_Next_Type(DataType, isAutoUniqueValue);
   }

   if (!pBestMatched && ExactMode == FALSE)
      // Procedo alla ricerca del tipo più simile
      switch (DataType)
      {
         case adEmpty:
            break;

         case adUnsignedTinyInt: // intero senza segno a 1 byte
            if (TryNumericBySizePrec)
            {  // provo con i tipi numerici esatti con precisione e scala fissi
               if ((pBestMatched = gsc_SearchNumericBySizePrec(ptr_DataTypeList(), Size, Prec, isAutoUniqueValue))) break;
               TryNumericBySizePrec = FALSE;
            }
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adUnsignedSmallInt, isAutoUniqueValue))) break;
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adSmallInt, isAutoUniqueValue))) break;
         case adUnsignedSmallInt: // intero senza segno a 2 byte
            if (TryNumericBySizePrec)
            {  // provo con i tipi numerici esatti con precisione e scala fissi
               if ((pBestMatched = gsc_SearchNumericBySizePrec(ptr_DataTypeList(), Size, Prec, isAutoUniqueValue))) break;
               TryNumericBySizePrec = FALSE;
            }
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adUnsignedInt, isAutoUniqueValue))) break;
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adInteger, isAutoUniqueValue))) break;
         case adUnsignedInt: // intero senza segno a 4 byte
            if (TryNumericBySizePrec)
            {  // provo con i tipi numerici esatti con precisione e scala fissi
               if ((pBestMatched = gsc_SearchNumericBySizePrec(ptr_DataTypeList(), Size, Prec, isAutoUniqueValue))) break;
               TryNumericBySizePrec = FALSE;
            }
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adUnsignedBigInt, isAutoUniqueValue))) break;
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adBigInt, isAutoUniqueValue))) break;
         case adUnsignedBigInt: // intero senza segno a 8 byte
            if (TryNumericBySizePrec)
            {  // provo con i tipi numerici esatti con precisione e scala fissi
               if ((pBestMatched = gsc_SearchNumericBySizePrec(ptr_DataTypeList(), Size, Prec, isAutoUniqueValue))) break;
               TryNumericBySizePrec = FALSE;
            }
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adSingle, isAutoUniqueValue))) break;
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adDouble, isAutoUniqueValue))) break;
            break;

         case adBoolean:
            // purtroppo ACCESS 97 Direct considera il dato adBoolean con Size = 2
            // Con la riga seguente si accetta la conversione da un adBoolean 
            // a un adBoolean indipendentemente dal Size 
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adBoolean, isAutoUniqueValue))) break;

            // provo con i tipi numerici esatti con precisione e scala fissi
            if ((pBestMatched = gsc_SearchNumericBySizePrec(ptr_DataTypeList(), Size, Prec, isAutoUniqueValue))) break;
            TryNumericBySizePrec = FALSE;
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adTinyInt, isAutoUniqueValue))) break;
         case adTinyInt: // intero con segno a 1 byte
            if (TryNumericBySizePrec)
            {  // provo con i tipi numerici esatti con precisione e scala fissi
               if ((pBestMatched = gsc_SearchNumericBySizePrec(ptr_DataTypeList(), Size, Prec, isAutoUniqueValue))) break;
               TryNumericBySizePrec = FALSE;
            }
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adSmallInt, isAutoUniqueValue))) break;
         case adSmallInt: // intero con segno a 2 byte
            if (TryNumericBySizePrec)
            {  // provo con i tipi numerici esatti con precisione e scala fissi
               if ((pBestMatched = gsc_SearchNumericBySizePrec(ptr_DataTypeList(), Size, Prec, isAutoUniqueValue))) break;
               TryNumericBySizePrec = FALSE;
            }
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adInteger, isAutoUniqueValue))) break;
         case adInteger: // intero con segno a 4 byte
            if (TryNumericBySizePrec)
            {  // provo con i tipi numerici esatti con precisione e scala fissi
               if ((pBestMatched = gsc_SearchNumericBySizePrec(ptr_DataTypeList(), Size, Prec, isAutoUniqueValue))) break;
               TryNumericBySizePrec = FALSE;
            }
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adBigInt, isAutoUniqueValue))) break;
         case adBigInt: // intero con segno a 8 byte
            if (TryNumericBySizePrec)
            {  // provo con i tipi numerici esatti con precisione e scala fissi
               if ((pBestMatched = gsc_SearchNumericBySizePrec(ptr_DataTypeList(), Size, Prec, isAutoUniqueValue))) break;
               TryNumericBySizePrec = FALSE;
            }
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adSingle, isAutoUniqueValue))) break;
         case adSingle: // valore a precisione singola floating point
            if (TryNumericBySizePrec)
            {  // provo con i tipi numerici esatti con precisione e scala fissi
               if ((pBestMatched = gsc_SearchNumericBySizePrec(ptr_DataTypeList(), Size, Prec, isAutoUniqueValue))) break;
               TryNumericBySizePrec = FALSE;
            }
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adDouble, isAutoUniqueValue))) break;
         case adDouble: // valore a precisione doppia floating point
            if (TryNumericBySizePrec)
            {  // provo con i tipi numerici esatti con precisione e scala fissi
               if ((pBestMatched = gsc_SearchNumericBySizePrec(ptr_DataTypeList(), Size, Prec, isAutoUniqueValue))) break;
               TryNumericBySizePrec = FALSE;
            }
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adCurrency, isAutoUniqueValue))) break;
         case adCurrency:
            if (TryNumericBySizePrec)
            {  // provo con i tipi numerici esatti con precisione e scala fissi
               if ((pBestMatched = gsc_SearchNumericBySizePrec(ptr_DataTypeList(), Size, Prec, isAutoUniqueValue))) break;
               TryNumericBySizePrec = FALSE;
            }
            break;

         case adDecimal: // valore numerico esatto con precisione e scala fissi
         case adNumeric:
            if (TryNumericBySizePrec)
               // provo con i tipi numerici esatti con precisione e scala fissi
               if ((pBestMatched = gsc_SearchNumericBySizePrec(ptr_DataTypeList(), Size, Prec, isAutoUniqueValue))) break;

            // NON provo con adBoolean perchè il provider di ACCESS 97 direct da errore
            // se si tenta di inserire un numero in un booleano

            // provo con adTinyInt
            if ((pProviderType = ptr_DataTypeList()->search_Type(adTinyInt, isAutoUniqueValue)) &&
                Size <= pProviderType->get_Size())
               if (Prec <= 0) { pBestMatched = pProviderType; break; } // se non sono richiesti decimali
            // provo con adSmallInt
            if ((pProviderType = ptr_DataTypeList()->search_Type(adSmallInt, isAutoUniqueValue)) &&
                Size <= pProviderType->get_Size())
               if (Prec <= 0) { pBestMatched = pProviderType; break; } // se non sono richiesti decimali
            // provo con adInteger
            if ((pProviderType = ptr_DataTypeList()->search_Type(adInteger, isAutoUniqueValue)) &&
                Size <= pProviderType->get_Size())
               if (Prec <= 0) { pBestMatched = pProviderType; break; } // se non sono richiesti decimali
            // provo con adBigInt
            if ((pProviderType = ptr_DataTypeList()->search_Type(adBigInt, isAutoUniqueValue)) &&
                Size <= pProviderType->get_Size())
               if (Prec <= 0) { pBestMatched = pProviderType; break; } // se non sono richiesti decimali
            // provo con adSingle senza verificare le dimensioni
            if ((pProviderType = ptr_DataTypeList()->search_Type(adSingle, isAutoUniqueValue)))
                  { pBestMatched = pProviderType; break; }
            // provo con adDouble senza verificare le dimensioni
            if ((pProviderType = ptr_DataTypeList()->search_Type(adDouble, isAutoUniqueValue)))
               { pBestMatched = pProviderType; break; }
            // provo con adCurrency
            if ((pProviderType = ptr_DataTypeList()->search_Type(adCurrency, isAutoUniqueValue)) &&
                Size <= pProviderType->get_Size())
               if (Prec <= 0)  // se non sono richiesti decimali
                  { pBestMatched = pProviderType; break; }
               else 
                  if (gsc_DBIsNumericWithDecimals(pProviderType->get_Type()) && Prec <= pProviderType->get_Dec())
                  { pBestMatched = pProviderType; break; }              
            break;

         case adError:
         case adUserDefined:
         case adVariant:
         case adIDispatch:
         case adIUnknown:
         case adGUID:
            break;

         ////////////////////////
         // fine tipi
         // inizio tipi data-tempo
         ////////////////////////
         case adDate: // data con tempo
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adDate, isAutoUniqueValue))) break;
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adDBTimeStamp, isAutoUniqueValue))) break;
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adDBDate, isAutoUniqueValue))) break;
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adDBTime, isAutoUniqueValue))) break;
            break;

         case adDBDate: // solo data
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adDBDate, isAutoUniqueValue))) break;
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adDate, isAutoUniqueValue))) break;
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adDBTimeStamp, isAutoUniqueValue))) break;
            break;

         case adDBTime: // solo tempo
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adDBTime, isAutoUniqueValue))) break;
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adDate, isAutoUniqueValue))) break;
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adDBTimeStamp, isAutoUniqueValue))) break;
            break;

         case adDBTimeStamp: // data con tempo
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adDBTimeStamp, isAutoUniqueValue))) break;
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adDate, isAutoUniqueValue))) break;
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adDBDate, isAutoUniqueValue))) break;
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adDBTime, isAutoUniqueValue))) break;
            break;

         ////////////////////////
         // fine tipi data-tempo
         // inizio tipi carattere
         ////////////////////////
         case adVarChar: case adVarWChar: case adChar: case adWChar: case adBSTR:
         case adLongVarChar: case adLongVarWChar:
         {  // cerco nell'ordine i tipi : adVarChar, adVarWChar, adChar, adWChar, adBSTR,
            //                            adLongVarChar, adLongVarWChar
            C_STRING dummy;

            // cerco i tipi valutando le dimensioni del campo
            // provo con adVarChar
            if (DataType != adVarChar)
               if (dummy.paste(Type2ProviderDescr(adVarChar, IsLong, IsFixedPrecScale, 
                                                  IsFixedLength, IsWrite, Size)) != NULL)
                  return gsc_tostring(dummy.get_name());
            // provo con adVarWChar
            if (DataType != adVarWChar)
               if (dummy.paste(Type2ProviderDescr(adVarWChar, IsLong, IsFixedPrecScale, 
                                                  IsFixedLength, IsWrite, Size)) != NULL)
                  return gsc_tostring(dummy.get_name());
            // provo con adChar
            if (DataType != adChar)
               if (dummy.paste(Type2ProviderDescr(adChar, IsLong, IsFixedPrecScale,
                                                  IsFixedLength, IsWrite, Size)) != NULL)
                  return gsc_tostring(dummy.get_name());
            // provo con adWChar
            if (DataType != adWChar)
               if (dummy.paste(Type2ProviderDescr(adWChar, IsLong, IsFixedPrecScale,
                                                  IsFixedLength, IsWrite, Size)) != NULL)
                  return gsc_tostring(dummy.get_name());
            // provo con adBSTR
            if (DataType != adBSTR)
               if (dummy.paste(Type2ProviderDescr(adBSTR, IsLong, IsFixedPrecScale,
                                                  IsFixedLength, IsWrite, Size)) != NULL)
                  return gsc_tostring(dummy.get_name());
            // provo con adLongVarChar
            if (DataType != adLongVarChar)
               if (dummy.paste(Type2ProviderDescr(adLongVarChar, IsLong, IsFixedPrecScale,
                                                  IsFixedLength, IsWrite, Size)) != NULL)
                  return gsc_tostring(dummy.get_name());
            // provo con adLongVarWChar
            if (DataType != adLongVarWChar)
               if (dummy.paste(Type2ProviderDescr(adLongVarWChar, IsLong, IsFixedPrecScale,
                                                  IsFixedLength, IsWrite, Size)) != NULL)
                  return gsc_tostring(dummy.get_name());

            break;
         }

         ////////////////////////
         // fine tipi carattere
         ////////////////////////

         case adBinary:
            if ((pBestMatched = ptr_DataTypeList()->search_Type(adVarBinary, isAutoUniqueValue))) break;
         case adChapter:
            break;

         case adFileTime:
            break;

         case adPropVariant:
            break;

         case adVarNumeric:
            break;

         // questi tipi sono utilizzati solo per oggetti "Parameter" non per definire
         // campi di tabelle
         case adVarBinary: case adLongVarBinary:

         default:
            break;
      }

   if (!pBestMatched) // non è stato trovato un tipo adatto
      // non è stato trovato un tipo grande a sufficienza per contenere le informazioni 
      if (pWrongSizeProviderType) // però in mancanza d'altro mi accontento
         pBestMatched = pWrongSizeProviderType; // possibile perdita di dati
      else
         { GS_ERR_COD = eGSUnkwnDrvFieldType; return NULL; }
   
   return gsc_tostring(pBestMatched->get_name());
}

   
/**********************************************************/
/*.doc C_DBCONNECTION::boolToSQL               <external> */
/*+
  Questa funzione restituisce il valore in formato SQL.
  Parametri:
  bool Value;
-*/  
/*********************************************************/
TCHAR *C_DBCONNECTION::boolToSQL(bool Value)
{
   if (ptr_DataTypeList()->search_Type(adBoolean, FALSE))
      return (Value) ? _T("TRUE") : _T("FALSE");
   else
      return (Value) ? _T("1") : _T("0");
}

/**********************************************************/
/*.doc C_DBCONNECTION::getStruForCreatingTable <external> */
/*+
  Questa funzione ottiene da una lista che descrive la struttura di una 
  tabella (ottenuta dalla funzione "ReadStruct") un'altra lista che descrive
  la stessa tabella utilizzando la descrizione dei tipi delle colonne anzichè
  i codici ADO.
  Parametri:
  C_RB_LIST &Stru;   Lista descrivente la struttura di una tabella

  Restituisce la nuova lista in caso di successo altrimenti restituisce NULL.
  N.B.: Alloca memoria.
-*/  
/*********************************************************/
presbuf C_DBCONNECTION::getStruForCreatingTable(presbuf Stru)
{
   C_RB_LIST DestStru;
   presbuf   pRB;
   int       i = 0, Prec;
   C_STRING  Descr;
   long      Size;

   if ((DestStru << acutBuildList(RTLB, 0)) == NULL) return GS_BAD;

   while ((pRB = gsc_nth(i++, Stru)) != NULL)
   {
      Size = gsc_nth(2, pRB)->resval.rlong;
      Prec = gsc_nth(3, pRB)->resval.rint;
      if (Descr.paste(Type2ProviderDescr((DataTypeEnum) gsc_nth(1, pRB)->resval.rlong,     // tipo
                                         (gsc_nth(4, pRB)->restype == RTT) ? TRUE : FALSE, // is long
                                         (gsc_nth(5, pRB)->restype == RTT) ? TRUE : FALSE, // is FixedPrecScale
                                         gsc_nth(6, pRB)->restype,                         // is FixedLength
                                         (gsc_nth(7, pRB)->restype == RTT) ? TRUE : FALSE, // IsAutoUniqueValue
                                         Size,
                                         FALSE,                                            // Exact mode
                                         Prec)) == NULL)
         return GS_BAD;

      if ((DestStru += acutBuildList(RTLB, 
                                           RTSTR, gsc_nth(0, pRB)->resval.rstring,
                                           RTSTR, Descr.get_name(),
                                           RTLONG, Size,
                                           RTSHORT, Prec,
                                     RTLE, 0)) == NULL) return GS_BAD;
   }

   if ((DestStru += acutBuildList(RTLE, 0)) == NULL) return GS_BAD;
   DestStru.ReleaseAllAtDistruction(GS_BAD);

   return DestStru.get_head();
}

/**************************************************************/
/*.doc gsc_SearchNumericBySizePrec                 <internal> */
/*+
  Questa funzione cerca tra i tipi numerici con dimensione e precisione indicati
  dall'utente (es. "NUMERIC(Size, Prec)") quale tipo puo ospitare un tipo 
  numerico con dimensione e precisione noti.
  Parametri:
  C_PROVIDER_TYPE_LIST *p_DataTypeList;   Lista dei tipi di dato di un provider
  long                 Size;              Dimensione
  int                  Prec;              Precisione
  int                  IsAutoUniqueValue; TRUE se è un tipo ad autoincremeneto

  Restituisce il puntatore al tipo in caso di successo altrimenti restituisce NULL.
  N.B. Funzione di ausilio a "C_DBCONNECTION::Type2ProviderDescr"
-*/  
/**************************************************************/
C_PROVIDER_TYPE *gsc_SearchNumericBySizePrec(C_PROVIDER_TYPE_LIST *p_DataTypeList,
                                             long Size, int Prec, int IsAutoUniqueValue)
{
   C_PROVIDER_TYPE *pProviderType;

   // provo prima con i tipi numerici esatti con precisione e scala fissi
   if ((pProviderType = p_DataTypeList->search_Type(adDecimal, IsAutoUniqueValue)) &&
       Size <= pProviderType->get_Size() && Prec <= pProviderType->get_Dec())
      return pProviderType;
   if ((pProviderType = p_DataTypeList->search_Type(adNumeric, IsAutoUniqueValue)) &&
       Size <= pProviderType->get_Size() && Prec <= pProviderType->get_Dec())
      return pProviderType;

   return NULL;
}


/*************************************************************/
/*.doc C_DBCONNECTION::ProviderDescr2InfoType     <external> */
/*+
  Questa funzione riceve la descrizione di un tipo (del provider) e 
  restituisce una serie di informazioni sul quel tipo.
  Parametri:
  const TCHAR *DataDescr;
  DataTypeEnum *pDataType;     (default = NULL)
  long *pDim;                  (default = NULL) dimensioni in byte (-1 se non usato)
  int *pDec;                   (default = NULL) dimensioni in byte (-1 se non usato)
  short *pIsLong;              (default = NULL)
  short *pIsFixedPrecScale;    (default = NULL)
  short *pIsAutoUniqueValue;   (default = NULL)
  C_STRING *pCreateParam;      (default = NULL)
  short *pIsFixedLength;       (default = NULL)

  Restituisce la descrizione in caso di successo altrimenti restituisce NULL.
-*/  
/*********************************************************/
int C_DBCONNECTION::ProviderDescr2InfoType(const TCHAR *DataDescr, DataTypeEnum *pDataType,
                                           long *pDim, int *pDec, short *pIsLong,
                                           short *pIsFixedPrecScale, short *pIsAutoUniqueValue,
                                           C_STRING *pCreateParam, short *pIsFixedLength)
{
   C_PROVIDER_TYPE *pProviderType = (C_PROVIDER_TYPE *) ptr_DataTypeList()->search_name(DataDescr, FALSE);

   if (pProviderType)
   {
      if (pDataType)          *pDataType          = pProviderType->get_Type();
      if (pDim)               *pDim               = pProviderType->get_Size();
      if (pDec)               *pDec               = pProviderType->get_Dec();
      if (pIsLong)            *pIsLong            = pProviderType->get_IsLong();
      if (pIsFixedPrecScale)  *pIsFixedPrecScale  = pProviderType->get_IsFixedPrecScale();
      if (pIsAutoUniqueValue) *pIsAutoUniqueValue = pProviderType->get_IsAutoUniqueValue();
      if (pCreateParam)       pCreateParam->set_name(pProviderType->get_CreateParams());
      if (pIsFixedLength)     *pIsFixedLength     = pProviderType->get_IsFixedLength();
      
      return GS_GOOD;
   }
   else
      { GS_ERR_COD = eGSUnkwnDrvFieldType; return GS_BAD; }
}


/*********************************************************/
/*.doc C_DBCONNECTION::Type2ProviderType      <external> */
/*+
  Questa funzione riceve le caratteristiche ADO di un tipo e restituisce
  il tipo più simile del Provider.
  Parametri:
  DataTypeEnum DataType;          Tipo di dato
  short        IsLong;            TRUE o FALSE dato lungo
  short        IsFixedPrecScale;  TRUE se di precisione e scala a lunghezza fissa 
                                  altrimenti non è di lunghezza fissa
  short        IsFixedLength;     RTT se di lunghezza fissa a livello di DDL
                                  (Data Definition Language es. "CREATE TABLE ..."
                                  altrimenti non è di lunghezza fissa.
  short        IsWrite;           TRUE o FALSE scrivibile
  long         Size;              Dimensione
  int          Prec;              Precisione
  C_STRING     ProviderDescr;     Descrizione tipo del provider (out)
  long         *ProviderSize;     Dimensione del tipo del provider (-1 se non usato) (out)
  int          *ProviderPrec;     Precisione del tipo del provider (-1 se non usato) (out)

  Restituisce la descrizione in caso di successo altrimenti restituisce NULL.
-*/  
/*********************************************************/
int C_DBCONNECTION::Type2ProviderType(DataTypeEnum DataType, short IsLong,
                                      short IsFixedPrecScale, short IsFixedLength,
                                      short IsWrite, long Size,
                                      int Prec, C_STRING &ProviderDescr,
                                      long *ProviderSize, int *ProviderPrec)
{
   C_PROVIDER_TYPE *pProviderType;
   C_STRING        Params;

   // converto campo da codice ADO in Provider dipendente
   ProviderDescr.paste(Type2ProviderDescr(DataType, IsLong, 
                                          IsFixedPrecScale, IsFixedLength,
                                          IsWrite, Size, FALSE, Prec)); // prec era messo a 0 (roby)
   if (ProviderDescr.len() == 0) return GS_BAD;

   pProviderType = (C_PROVIDER_TYPE *) ptr_DataTypeList()->search_name(ProviderDescr.get_name(), FALSE);
   if (!pProviderType) return GS_BAD; // per sicurezza

   Params = pProviderType->get_CreateParams();
   if (Params.len() > 0) // Controllo sulle dimensioni del campo
   {
      int   Qty;
      TCHAR *p;

      p = Params.get_name();
      Qty = gsc_strsep(p, _T('\0'));
      
      if (Size == 0) *ProviderSize = pProviderType->get_Size();
      else *ProviderSize = min(Size, pProviderType->get_Size());

      if (Qty > 0) *ProviderPrec = min(Prec, pProviderType->get_Dec());
      else *ProviderPrec = -1;
   }
   else
   {
      *ProviderSize = pProviderType->get_Size();
      *ProviderPrec = pProviderType->get_Dec();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBCONNECTION::Descr2ProviderDescr    <external> */
/*+
  Questa funzione trasforma la descrizione di un tipo di dato e le sue dimensioni,
  convertendole per un'altra connessione OLE-DB.
  Parametri:
  const TCHAR    *SrcProviderDescr;  Descrizione tipo del provider sorgente
  long           SrcSize;            Dimensione tipo sorgente
  int            SrcPrec;            Precisione tipo sorgente
  C_DBCONNECTION &DestConn;          Connessione OLE-DB destinazione
  C_STRING       &DestProviderDescr; Descrizione tipo del provider destinazione (out)
  long           *DestSize;          Dimensione tipo destinazione (out)
  int            *DestPrec;          Precisione tipo destinazione (out)

  Restituisce GS_GOOD in caso di piena compatibilità, GS_CAN se i tipi sono
  compatibili ma potrebbe esserci una perdita di dati, GS_BAD se i tipi non 
  sono compatibili.
-*/  
/*********************************************************/
int C_DBCONNECTION::Descr2ProviderDescr(const TCHAR *SrcProviderDescr, long SrcSize, int SrcPrec,
                                        C_DBCONNECTION &DestConn, C_STRING &DestProviderDescr,
                                        long *DestSize, int *DestPrec)
{
   DataTypeEnum DataType;
   short        IsLong, IsAutoUniqueValue, IsFixedPrecScale, IsFixedLength;
   int          Prec;
   long         Size;
   C_STRING     CreateParam;

   // leggo caratteristiche del tipo sorgente
   if (ProviderDescr2InfoType(SrcProviderDescr, &DataType, &Size, &Prec, &IsLong,
                              &IsFixedPrecScale, &IsAutoUniqueValue, &CreateParam,
                              &IsFixedLength) == GS_BAD)
      return GS_BAD;

   if (SrcSize > 0) Size = SrcSize; // se si deve usare la dimensione dal parametro
   if (SrcPrec >= 0) Prec = SrcPrec; // se si deve usare la precisione dal parametro
   // converto tipo nel formato del provider di destinazione
   return DestConn.Type2ProviderType(DataType,           // tipo
                                     IsLong,             // is long
                                     IsFixedPrecScale,   // is IsFixedPrecScale
                                     IsFixedLength,      // is IsFixedLength
                                     !IsAutoUniqueValue, // IsWrite
                                     Size,               // dimensione
                                     Prec,               // precisione
                                     DestProviderDescr,
                                     DestSize,
                                     DestPrec);
}


/*********************************************************/
/*.doc C_DBCONNECTION::IsCompatibleType       <external> */
/*+
  Questa funzione riceve le caratteristiche ADO di un tipo di dato,
  una connessione OLE-DB e le caratteristiche ADO di un tipo di dato.
  La funzione restituisce GS_GOOD se il secondo tipo è compatibile
  ad ospitare dati del primo tipo.
  Parametri:
  const TCHAR    *SrcProviderDescr;  Descrizione tipo del provider sorgente
  long           SrcSize;            Dimensione tipo sorgente
  int            SrcPrec;            Precisione tipo sorgente
  C_DBCONNECTION &DestConn;          Connessione OLE-DB destinazione
  const TCHAR    *DestProviderDescr; Descrizione tipo del provider destinazione
  long           DestSize;           Dimensione tipo destinazione
  int            DestPrec;           Precisione tipo destinazione
  int            ExactMode;

  Restituisce GS_GOOD in caso di piena compatibilità, GS_CAN se i tipi sono
  compatibili ma potrebbe esserci una perdita di dati, GS_BAD se i tipi non 
  sono compatibili.
-*/  
/*********************************************************/
int C_DBCONNECTION::IsCompatibleType(const TCHAR *SrcProviderDescr,
                                     long SrcSize, int SrcPrec,
                                     C_DBCONNECTION &DestConn, const TCHAR *DestProviderDescr,
                                     long DestSize, int DestPrec)
{
   DataTypeEnum SrcDataType, DestDataType;
   long         SrcDim, DestDim;
   int          SrcDec, DestDec;
   short        SrcIsFixedPrecScale, DestIsFixedPrecScale;
   short        SrcIsLong, SrcIsAutoUniqueValue;
   short        DestIsLong, DestIsAutoUniqueValue;

   // leggo caratteristiche del tipo sorgente
   if (ProviderDescr2InfoType(SrcProviderDescr, &SrcDataType, &SrcDim,
                              &SrcDec, &SrcIsLong, &SrcIsFixedPrecScale,
                              &SrcIsAutoUniqueValue) == GS_BAD)
      return GS_BAD;

   // leggo caratteristiche del tipo destinazione
   if (DestConn.ProviderDescr2InfoType(DestProviderDescr, &DestDataType, &DestDim,
                                       &DestDec, &DestIsLong, &DestIsFixedPrecScale,
                                       &DestIsAutoUniqueValue) == GS_BAD)
      return GS_BAD;

   if (SrcIsFixedPrecScale != RTT) SrcDim = SrcSize; // Se la dimensione sorgente non è fissa
   if (DestIsFixedPrecScale != RTT) DestDim = DestSize; // Se la dimensione di destinazione non è fissa

   switch (SrcDataType)
   {
      case adTinyInt: // intero con segno a 1 byte
         switch (DestDataType)
         {
            // lista dei tipi compatibili
            case adUnsignedTinyInt:
               return GS_CAN;
            case adTinyInt:
            case adSmallInt:
            case adUnsignedSmallInt:
            case adInteger:
            case adUnsignedInt:
            case adBigInt:
            case adUnsignedBigInt:
            case adCurrency:
            case adSingle:
            case adDouble:
            case adNumeric:
            case adDecimal:
               if (SrcDim <= DestDim) return GS_GOOD; // la dimensione è compatibile
               return GS_CAN;
            default:
               return GS_BAD;
         }
         break;

      case adUnsignedTinyInt: // intero senza segno a 1 byte
         switch (DestDataType)
         {
            // lista dei tipi compatibili
            case adTinyInt:
               return GS_CAN;
            case adUnsignedTinyInt:
            case adSmallInt:
            case adUnsignedSmallInt:
            case adInteger:
            case adUnsignedInt:
            case adBigInt:
            case adUnsignedBigInt:
            case adCurrency:
            case adSingle:
            case adDouble:
            case adNumeric:
            case adDecimal:
               if (SrcDim <= DestDim) return GS_GOOD; // la dimensione è compatibile
               return GS_CAN;
            default:
               return GS_BAD;
         }
         break;

      case adSmallInt: // intero con segno a 2 byte
         switch (DestDataType)
         {
            // lista dei tipi compatibili
            case adTinyInt:
            case adUnsignedTinyInt:
            case adUnsignedSmallInt:
               return GS_CAN;
            case adSmallInt:
            case adInteger:
            case adUnsignedInt:
            case adBigInt:
            case adUnsignedBigInt:
            case adCurrency:
            case adSingle:
            case adDouble:
            case adNumeric:
            case adDecimal:
               if (SrcDim <= DestDim) return GS_GOOD; // la dimensione è compatibile
               return GS_CAN;
            default:
               return GS_BAD;
         }
         break;

      case adUnsignedSmallInt: // intero senza segno a 2 byte
         switch (DestDataType)
         {
            // lista dei tipi compatibili
            case adTinyInt:
            case adUnsignedTinyInt:
            case adSmallInt:
               return GS_CAN;
            case adUnsignedSmallInt:
            case adInteger:
            case adUnsignedInt:
            case adBigInt:
            case adUnsignedBigInt:
            case adCurrency:
            case adSingle:
            case adDouble:
            case adNumeric:
            case adDecimal:
               if (SrcDim <= DestDim) return GS_GOOD; // la dimensione è compatibile
               return GS_CAN;
            default:
               return GS_BAD;
         }
         break;

      case adInteger: // intero con segno a 4 byte
         switch (DestDataType)
         {
            // lista dei tipi compatibili
            case adTinyInt:
            case adUnsignedTinyInt:
            case adSmallInt:
            case adUnsignedSmallInt:
            case adUnsignedInt:
               return GS_CAN;
            case adInteger:
            case adBigInt:
            case adUnsignedBigInt:
            case adCurrency:
            case adSingle:
            case adDouble:
            case adNumeric:
            case adDecimal:
               if (SrcDim <= DestDim) return GS_GOOD; // la dimensione è compatibile
               return GS_CAN;
            default:
               return GS_BAD;
         }
         break;

      case adUnsignedInt: // intero senza segno a 4 byte
         switch (DestDataType)
         {
            // lista dei tipi compatibili
            case adTinyInt:
            case adUnsignedTinyInt:
            case adSmallInt:
            case adUnsignedSmallInt:
            case adInteger:
               return GS_CAN;
            case adUnsignedInt:
            case adBigInt:
            case adUnsignedBigInt:
            case adCurrency:
            case adSingle:
            case adDouble:
            case adNumeric:
            case adDecimal:
               if (SrcDim <= DestDim) return GS_GOOD; // la dimensione è compatibile
               return GS_CAN;
            default:
               return GS_BAD;
         }
         break;

      case adBigInt: // intero con segno a 8 byte
         switch (DestDataType)
         {
            // lista dei tipi compatibili
            case adTinyInt:
            case adUnsignedTinyInt:
            case adSmallInt:
            case adUnsignedSmallInt:
            case adInteger:
            case adUnsignedInt:
               return GS_CAN;
            case adUnsignedBigInt:
            case adBigInt:
            case adCurrency:
            case adSingle:
            case adDouble:
            case adNumeric:
            case adDecimal:
               if (SrcDim <= DestDim) return GS_GOOD; // la dimensione è compatibile
               return GS_CAN;
            default:
               return GS_BAD;
         }
         break;

      case adUnsignedBigInt: // intero senza segno a 8 byte
         switch (DestDataType)
         {
            // lista dei tipi compatibili
            case adTinyInt:
            case adUnsignedTinyInt:
            case adSmallInt:
            case adUnsignedSmallInt:
            case adInteger:
            case adUnsignedInt:
               return GS_CAN;
            case adBigInt:
            case adUnsignedBigInt:
            case adCurrency:
            case adSingle:
            case adDouble:
            case adNumeric:
            case adDecimal:
               if (SrcDim <= DestDim) return GS_GOOD; // la dimensione è compatibile
               return GS_CAN;
            default:
               return GS_BAD;
         }
         break;

      case adCurrency: // intero con segno a 8 byte scalato di 10,000
         switch (DestDataType)
         {
            // lista dei tipi compatibili
            case adTinyInt:
            case adUnsignedTinyInt:
            case adSmallInt:
            case adUnsignedSmallInt:
            case adInteger:
            case adUnsignedInt:
            case adBigInt:
            case adUnsignedBigInt:
               return GS_CAN;
            case adCurrency:
            case adSingle:
            case adDouble:
            case adNumeric:
            case adDecimal:
               if (SrcDim <= DestDim) // la dimensione è compatibile
                  if (SrcPrec >= 0 && DestPrec >= 0) // i decimali sono dimensionabili
                     if (SrcPrec <= DestPrec) return GS_GOOD; // i decimali sono compatibili
               return GS_CAN;
            default:
               return GS_BAD;
         }
         break;

      case adSingle: // valore a precisione singola floating point
         switch (DestDataType)
         {
            // lista dei tipi compatibili
            case adTinyInt:
            case adUnsignedTinyInt:
            case adSmallInt:
            case adUnsignedSmallInt:
            case adInteger:
            case adUnsignedInt:
            case adBigInt:
            case adUnsignedBigInt:
               return GS_CAN;
            case adCurrency:
            case adSingle:
            case adDouble:
            case adNumeric:
            case adDecimal:
               if (SrcDim <= DestDim) // la dimensione è compatibile
                  if (SrcPrec >= 0 && DestPrec >= 0) // i decimali sono dimensionabili
                     if (SrcPrec <= DestPrec) return GS_GOOD; // i decimali sono compatibili
               return GS_CAN;
            default:
               return GS_BAD;
         }
         break;

      case adDouble: // valore a precisione doppia floating point
         switch (DestDataType)
         {
            // lista dei tipi compatibili
            case adTinyInt:
            case adUnsignedTinyInt:
            case adSmallInt:
            case adUnsignedSmallInt:
            case adInteger:
            case adUnsignedInt:
            case adBigInt:
            case adUnsignedBigInt:
               return GS_CAN;
            case adCurrency:
            case adSingle:
            case adDouble:
            case adNumeric:
            case adDecimal:
               if (SrcDim <= DestDim) // la dimensione è compatibile
                  if (SrcPrec >= 0 && DestPrec >= 0) // i decimali sono dimensionabili
                     if (SrcPrec <= DestPrec) return GS_GOOD; // i decimali sono compatibili
               return GS_CAN;
            default:
               return GS_BAD;
         }
         break;

      case adDecimal: // valore numerico esatto con precisione e scala fissi
         switch (DestDataType)
         {
            // lista dei tipi compatibili
            case adUnsignedTinyInt:
            case adUnsignedSmallInt:
            case adUnsignedInt:
            case adUnsignedBigInt:
            case adTinyInt:
            case adSmallInt:
            case adInteger:
            case adBigInt:
            case adCurrency:
            case adSingle:
            case adDouble:
            case adNumeric:
            case adDecimal:
               if (SrcDim <= DestDim) // la dimensione è compatibile
                  if (SrcPrec >= 0 && DestPrec >= 0) // i decimali sono dimensionabili
                     if (SrcPrec <= DestPrec) return GS_GOOD; // i decimali sono compatibili
               return GS_CAN;
            default:
               return GS_BAD;
         }
         break;

      case adNumeric: // valore numerico esatto con precisione e scala fissi
         switch (DestDataType)
         {
            // lista dei tipi compatibili
            case adUnsignedTinyInt:
            case adUnsignedSmallInt:
            case adUnsignedInt:
            case adUnsignedBigInt:
            case adTinyInt:
            case adSmallInt:
            case adInteger:
            case adBigInt:
            case adCurrency:
            case adSingle:
            case adDouble:
            case adNumeric:
            case adDecimal:
               if (SrcDim <= DestDim) // la dimensione è compatibile
                  if (SrcPrec >= 0 && DestPrec >= 0) // i decimali sono dimensionabili
                     if (SrcPrec <= DestPrec) return GS_GOOD; // i decimali sono compatibili
               return GS_CAN;
            default:
               return GS_BAD;
         }
         break;

      case adBoolean:
         if (DestDataType == adBoolean) return GS_GOOD;
         break;

      case adError:
      case adUserDefined:
      case adVariant:
      case adIDispatch:
      case adIUnknown:
      case adGUID:
         break;

      case adDate: // data con tempo
         switch (DestDataType)
         {
            // lista dei tipi compatibili
            case adDBDate:
            case adDBTime:
               return GS_CAN;
            case adDate:
            case adDBTimeStamp:
               return GS_GOOD;
         }
         break;

      case adDBDate: // solo data
         switch (DestDataType)
         {
            // lista dei tipi compatibili
            case adDBDate:
            case adDate:
            case adDBTimeStamp:
               return GS_GOOD;
         }
         break;

      case adDBTime: // solo tempo
         switch (DestDataType)
         {
            // lista dei tipi compatibili
            case adDBTime:
            case adDBTimeStamp:
            case adDate:
               return GS_GOOD;
         }
         break;

      case adDBTimeStamp: // data con tempo
         switch (DestDataType)
         {
            // lista dei tipi compatibili
            case adDBDate:
            case adDBTime:
               return GS_CAN;
            case adDBTimeStamp:
            case adDate:
               return GS_GOOD;
         }
         break;

      case adWChar: // stringa unicode
      case adBSTR: // stringa unicode
         switch (DestDataType)
         {
            // lista dei tipi compatibili
            case adWChar:
            case adBSTR:
            case adChar:
               if (SrcDim <= DestDim) return GS_GOOD; // la dimensione è compatibile
               return GS_CAN;
            default:
               return GS_BAD;
         }
         break;

      case adChar:
         switch (DestDataType)
         {
            // lista dei tipi compatibili
            case adChar:
            case adVarChar:
            case adWChar:
               if (SrcDim <= DestDim) return GS_GOOD; // la dimensione è compatibile
               return GS_CAN;
            default:
               return GS_BAD;
         }
         break;

      case adBinary:
         if (DestDataType != adBinary) return GS_BAD;
         break;

      case adVarBinary: // usati solo come parametri ADO
      case adLongVarBinary:
      case adVarChar:
      case adVarWChar:
      case adLongVarChar:
         return GS_BAD;

      case adEmpty:     // tipi sconosciuti
      case adChapter:
      case adFileTime:
      case adPropVariant:
      case adVarNumeric:
         return GS_BAD;

      default:
         return GS_BAD;
   }

   return GS_BAD;
}


/*********************************************************/
/*.doc C_DBCONNECTION::IsBlob                 <external> */
/*+
  Questa funzione riceve la descrizione ADO e ne verifica la tipologia.
  Parametri:
  const TCHAR *ProviderDescr;  Descrizione tipo del provider sorgente

  Restituisce GS_GOOD se la descrizione ADO rappresenta una stringa altrimenti
  ritorna GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::IsBlob(const TCHAR *ProviderDescr)
{
   DataTypeEnum DataType;

   if (ProviderDescr2InfoType(ProviderDescr, &DataType) == GS_BAD) return GS_BAD;
   
   return gsc_DBIsBlob(DataType);
}


/*********************************************************/
/*.doc C_DBCONNECTION::IsBoolean              <external> */
/*+
  Questa funzione riceve la descrizione ADO e ne verifica la tipologia.
  Parametri:
  const TCHAR *ProviderDescr;  Descrizione tipo del provider sorgente

  Restituisce GS_GOOD se la descrizione ADO rappresenta una stringa altrimenti
  ritorna GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::IsBoolean(const TCHAR *ProviderDescr)
{
   DataTypeEnum DataType;

   if (ProviderDescr2InfoType(ProviderDescr, &DataType) == GS_BAD) return GS_BAD;
   
   return gsc_DBIsBoolean(DataType);
}


/*********************************************************/
/*.doc C_DBCONNECTION::IsNumeric              <external> */
/*+
  Questa funzione riceve la descrizione ADO e ne verifica la tipologia.
  Parametri:
  const TCHAR *ProviderDescr;  Descrizione tipo del provider sorgente

  Restituisce GS_GOOD se la descrizione ADO rappresenta un numero altrimenti
  ritorna GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::IsNumeric(const TCHAR *ProviderDescr)
{
   DataTypeEnum DataType;

   if (ProviderDescr2InfoType(ProviderDescr, &DataType) == GS_BAD) return GS_BAD;
   
   return gsc_DBIsNumeric(DataType);
}


/*********************************************************/
/*.doc C_DBCONNECTION::IsNumericWithDecimals  <external> */
/*+
  Questa funzione riceve la descrizione ADO e ne verifica la tipologia.
  Parametri:
  const TCHAR * ProviderDescr;  Descrizione tipo del provider sorgente

  Restituisce GS_GOOD se la descrizione ADO rappresenta un numero altrimenti
  ritorna GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::IsNumericWithDecimals(const TCHAR *ProviderDescr)
{
   DataTypeEnum DataType;

   if (ProviderDescr2InfoType(ProviderDescr, &DataType) == GS_BAD) return GS_BAD;
   
   return gsc_DBIsNumericWithDecimals(DataType);
}


/*********************************************************/
/*.doc C_DBCONNECTION::IsChar                 <external> */
/*+
  Questa funzione riceve la descrizione ADO e ne verifica la tipologia.
  Parametri:
  const TCHAR *ProviderDescr;  Descrizione tipo del provider sorgente

  Restituisce GS_GOOD se la descrizione ADO rappresenta una stringa altrimenti
  ritorna GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::IsChar(const TCHAR *ProviderDescr)
{
   DataTypeEnum DataType;

   if (ProviderDescr2InfoType(ProviderDescr, &DataType) == GS_BAD) return GS_BAD;
   
   return gsc_DBIsChar(DataType);
}


/*********************************************************/
/*.doc C_DBCONNECTION::IsCurrency             <external> */
/*+
  Questa funzione riceve la descrizione ADO e ne verifica la tipologia.
  Parametri:
  const TCHAR *ProviderDescr;  Descrizione tipo del provider sorgente

  Restituisce GS_GOOD se la descrizione ADO rappresenta una stringa altrimenti
  ritorna GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::IsCurrency(const TCHAR *ProviderDescr)
{
   DataTypeEnum DataType;

   if (ProviderDescr2InfoType(ProviderDescr, &DataType) == GS_BAD) return GS_BAD;
   
   return gsc_DBIsCurrency(DataType);
}


/*********************************************************/
/*.doc C_DBCONNECTION::IsDate                 <external> */
/*+
  Questa funzione riceve la descrizione ADO e ne verifica la tipologia.
  Parametri:
  const TCHAR *ProviderDescr;  Descrizione tipo del provider sorgente

  Restituisce GS_GOOD se la descrizione ADO rappresenta una data altrimenti
  ritorna GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::IsDate(const TCHAR *ProviderDescr)
{
   DataTypeEnum DataType;

   if (ProviderDescr2InfoType(ProviderDescr, &DataType) == GS_BAD) return GS_BAD;
   
   return gsc_DBIsDate(DataType);
}


/*********************************************************/
/*.doc C_DBCONNECTION::IsTimestamp            <external> */
/*+
  Questa funzione riceve la descrizione ADO e ne verifica la tipologia.
  Parametri:
  const TCHAR *ProviderDescr;  Descrizione tipo del provider sorgente

  Restituisce GS_GOOD se la descrizione ADO rappresenta una data altrimenti
  ritorna GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::IsTimestamp(const TCHAR *ProviderDescr)
{
   DataTypeEnum DataType;

   if (ProviderDescr2InfoType(ProviderDescr, &DataType) == GS_BAD) return GS_BAD;
   
   return gsc_DBIsTimestamp(DataType);
}


/*********************************************************/
/*.doc C_DBCONNECTION::IsTime                 <external> */
/*+
  Questa funzione riceve la descrizione ADO e ne verifica la tipologia.
  Parametri:
  const TCHAR *ProviderDescr;  Descrizione tipo del provider sorgente

  Restituisce GS_GOOD se la descrizione ADO rappresenta una data altrimenti
  ritorna GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::IsTime(const TCHAR *ProviderDescr)
{
   DataTypeEnum DataType;

   if (ProviderDescr2InfoType(ProviderDescr, &DataType) == GS_BAD) return GS_BAD;
   
   return gsc_DBIsTime(DataType);
}


/*********************************************************/
/*.doc C_DBCONNECTION::InitInsRow             <external> */
/*+
  Questa funzione genera un comando con i parametri già allocati per
  l'inserimento di righe in una tabella.
  Parametri:
  const TCHAR   *TableRef;   Riferimento completo tabella
  _RecordsetPtr &pRs;        RecordSet usato per inserimento di righe (out)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::InitInsRow(const TCHAR *TableRef, _RecordsetPtr &pRs)
{
   return gsc_InitDBInsRow(pConn, TableRef, pRs);
}
int C_DBCONNECTION::InitInsRow(const TCHAR *TableRef, _CommandPtr &pCmd)
{
   C_RB_LIST    Stru;
   int          i = 0;
   C_STRING     FieldName, statement, ValueList;
   presbuf      pRB, pField, pSize, pDec, pLong; 
   TCHAR        *pFieldName;
   DataTypeEnum DataType;

   // leggo struttura
   if ((pRB = ReadStruct(TableRef)) == NULL) return GS_BAD;
   Stru << pRB;

   statement = _T("INSERT INTO ");
   statement += TableRef;
   statement += _T(" (");

   while ((pRB = Stru.nth(i++)))
   {
      FieldName = gsc_nth(0, pRB)->resval.rstring;
      // Correggo la sintassi del nome del campo
      if (gsc_AdjSyntax(FieldName, get_InitQuotedIdentifier(), get_FinalQuotedIdentifier(),
                        get_StrCaseFullTableRef()) == GS_BAD) return GS_BAD;
      statement += FieldName;
      statement += _T(',');

      ValueList += _T("?,");
   }
   statement.set_chr(_T('\0'), statement.len() - 1); // elimino ultima ','
   ValueList.set_chr(_T('\0'), ValueList.len() - 1); // elimino ultima ','

   statement += _T(") VALUES (");
   statement += ValueList;
   statement += _T(')');

   if (gsc_PrepareCmd(pConn, statement.get_name(), pCmd) == GS_BAD) return GS_BAD;

   try
   {
      // Alloca i parametri
      i = 0;
      while ((pField = Stru.nth(i++)) != NULL)
      {
         if (!(pFieldName = gsc_nth(0, pField)->resval.rstring) ||
             !(pField = Stru.Assoc(pFieldName)) ||
             !(pSize = gsc_nth(2, pField)) || !(pDec = gsc_nth(3, pField)) ||
             !(pLong = gsc_nth(4, pField)) || !(pField = gsc_nth(1, pField)))
            { GS_ERR_COD = eGSAddRow; pCmd.Release(); return GS_BAD; }
         DataType = (DataTypeEnum) pField->resval.rlong;

         // Se si tenta di usare un parametro di tipo Numeric in cui ci sono 
         // 0 decimali PostgreSQL dà un msg di errore quindi ricorro al tipo integer
         if (gsc_DBIsNumeric(DataType) == GS_GOOD && pDec->resval.rlong == 0)
            DataType = adBigInt;
         else
            // Alcuni provider non ritornano il tipo giusto di una colonna
            // (es. colonna tipo "adLongVarBinary" viene interpretata come "adVarBinary"
            // con il flag is_long = TRUE in ACCESS)
            if (pLong->restype == RTT) // se il dato ha la proprietà is_long = TRUE
               switch (DataType)
               {
                  case adBinary:
                  case adVarBinary:
                     DataType = adLongVarBinary; break;
                  case adVarWChar:
                     DataType = adLongVarWChar; break;
                  case adChar:
                  case adVarChar:
                     DataType = adLongVarChar; break;
               }

         statement = pFieldName;

         _ParameterPtr pParameter = pCmd->CreateParameter(statement.get_name(), // nome
                                                          DataType, // tipo
                                                          adParamInput, // direction
                                                          (pSize->resval.rlong == 0) ? 1 : pSize->resval.rlong); // size
         // If you specify adDecimal or adNumeric data type, 
         // you must also set the NumericScale and the Precision properties of the Parameter object
         if (DataType == adDecimal || DataType == adNumeric)
         {
            pParameter->Precision    = (unsigned char) pSize->resval.rlong;
            pParameter->NumericScale = (unsigned char) pDec->resval.rlong;
         }
         pCmd->Parameters->Append(pParameter);

         //pCmd->Parameters->Append(pCmd->CreateParameter(statement.get_name(), // nome
         //                                               DataType, // tipo
         //                                               adParamInput, // direction
         //                                               (pSize->resval.rlong == 0) ? 1 : pSize->resval.rlong)); // size
      }
   }

   catch (_com_error &e)
	{
      printDBErrs(e, pConn);
      pCmd.Release();
      GS_ERR_COD = eGSAddRow;
      return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBCONNECTION::InsRow               <external> */
/*+
  Questa funzione inserisce una riga in una tabella.
  Parametri:
  const TCHAR *TableRef;   Riferimento completo tabella
  presbuf     ColVal;      Lista ((<nome colonna> <valore>)...) dei campi da inserire
  int retest;              Se MORETESTS -> in caso di errore riprova n volte 
                           con i tempi di attesa impostati poi ritorna GS_BAD,
                           ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                           (default = MORETESTS)
  int PrintError;          Utilizzato solo se <retest> = ONETEST. 
                           Se il flag = GS_GOOD in caso di errore viene
                           stampato il messaggio relativo altrimenti non
                           viene stampato alcun messaggio (default = GS_GOOD)

  Restituisce GS_GOOD in caso di successo, altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::InsRow(const TCHAR *TableRef, C_RB_LIST &ColVal, int retest,
                           int PrintError)
{ 
   _RecordsetPtr pInsRs;

   // preparo il recordset per l'inserimento di record nella tabella
   if (InitInsRow(TableRef, pInsRs) == GS_BAD) return GS_BAD;
   
   return gsc_DBInsRow(pInsRs, ColVal, retest, PrintError);
}


/*********************************************************/
/*.doc C_DBCONNECTION::GetTempTabName         <external> */
/*+
  Questa funzione restituisce un nome di una tabella temporanea.
  Parametri:
  const TCHAR *TableRef;    riferimento completo tabella esistente
  C_STRING &TempTabName;    riferimento completo tabella temporanea
        
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::GetTempTabName(const TCHAR *TableRef, C_STRING &TempTabName)
{
   C_STRING Catalog, Schema, Table, TmpRef;
   int      i = 0, res;

   if (split_FullRefTable(TableRef, Catalog, Schema, Table) == GS_BAD)
      return GS_BAD;

   do
   {
      Table = _T("GSTMP");
      Table += i++;
      TempTabName.paste(get_FullRefTable(Catalog, Schema, Table));
      if (TempTabName.len() == 0) return GS_BAD;
      res = ExistTable(TempTabName);
      if (res == GS_CAN) return GS_BAD;
   }
   while (res == GS_GOOD);

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBCONNECTION::UpdStruct              <external> /*
/*+
  Questa funzione modifica la struttura di una tabella.
  Riceve una lista di resbuf contenente la definizione della nuova struttura,
  una lista resbuf <setvalues> contenente coppie di valori:
  ((<nome colonna destinazione> <nome colonna sorgente> | <valore di default>)...)
                                
  <nome colonna destinazione> = stringa senza delimitatori (RTSTR)
  <nome colonna sorgente> = stringa senza delimitatori (RTSTR)

  <valore di default> = <valore numerico> | <valore alfanumerico>

  <valore numerico> = numero (RTREAL, RTLONG, RTSHORT)                       
  <valore alfanumerico> = stringa delimitata da apici <'> (RTSTR)
  
  La lista <setvalues> stabilisce quali valori devono essere associati alle
  colonne della tabella destinazione. Per ciascuna coppia, nella colonna <nome
  colonna destinazione>, se il secondo valore è un <nome colonna sorgente>
  verranno inseriti per ogni riga il valore di quella colonna, se è un <valore
  di default> verrà inserito lo stesso valore per tutte le righe.

  Parametri:
  const TCHAR    *TableRef;   Riferimento completo tabella
  presbuf DefStru;            puntatore resbuf (colonna, tipo, lunghezza,
                              decimali)
  presbuf SetValues;          puntatore resbuf ai valori da inserire nelle colonne
  int retest;                 se MORETESTS -> in caso di errore riprova n volte 
                              con i tempi di attesa impostati poi ritorna GS_BAD,
                              ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                              (default = MORETESTS)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  
  N.B. ATTENZIONE la funzione non tratta campi geometrici che verranno 
       convertiti in campi carattere !
-*/  
/*********************************************************/
int C_DBCONNECTION::UpdStruct(const TCHAR *TableRef, presbuf DefStru,
                              presbuf SetValues, int retest)
{  
   C_STRING  statement, DestFields, SourceFields, FieldValue, TempTabName, FieldName;
   int       i, First = TRUE, res = GS_BAD;
   presbuf   pField, pSource, p;
   C_RB_LIST IndexList;
   C_PROVIDER_TYPE *pCharProviderType;
      
   if ((pCharProviderType = getCharProviderType()) == NULL) return GS_BAD;

   i = 0;
   while ((pField = gsc_nth(i++, DefStru)) != NULL)
   {
      FieldName = gsc_nth(0, pField)->resval.rstring;

      // se colonna i cui valori sono da assegnare
      if ((pSource = gsc_nth(1, gsc_assoc(FieldName.get_name(), SetValues, FALSE))) != NULL &&
          pSource->restype != RTNIL)
      {
         // Correggo la sintassi del nome del campo
         if (gsc_AdjSyntax(FieldName, get_InitQuotedIdentifier(), get_FinalQuotedIdentifier(),
                           get_StrCaseFullTableRef()) == GS_BAD) return GS_BAD;

         if (First) First = FALSE;
         else
         {
            DestFields   += _T(',');
            SourceFields += _T(',');
         }

         DestFields += FieldName;
         // Se il valore non incomincia con il carattere apice (') si tratta di un nome di campo
         if (pSource->restype == RTSTR && pSource->resval.rstring[0] != pCharProviderType->get_LiteralPrefix()[0])
         {  // Correggo la sintassi del nome del campo
            FieldValue = pSource->resval.rstring;
            if (gsc_AdjSyntax(FieldValue, get_InitQuotedIdentifier(), get_FinalQuotedIdentifier(),
                              get_StrCaseFullTableRef()) == GS_BAD) return GS_BAD;
         }
         else
            if (IsBoolean(gsc_nth(1, pField)->resval.rstring) == GS_GOOD)
               FieldValue = (pSource->restype == RTT) ? _T("TRUE" ): _T("FALSE");
            else
               if (IsDate(gsc_nth(1, pField)->resval.rstring) == GS_GOOD)
               {
                  FieldValue.paste(gsc_rb2str(pSource));
                  FieldValue.removePrefixSuffix(pCharProviderType->get_LiteralPrefix(), pCharProviderType->get_LiteralSuffix());
                  // Correggo la stringa "time-stamp" secondo la sintassi SQL  
                  if (Str2SqlSyntax(FieldValue, ptr_DataTypeList()->search_Type(adDBTimeStamp, FALSE)) == GS_BAD)
                     return GS_BAD;
               }
               else
                  FieldValue.paste(gsc_rb2str(pSource));

         // Se il valore è un 
         SourceFields += FieldValue;
      }
   }

   int IsTransactionSupported = BeginTrans();
   if (IsTransactionSupported == GS_BAD) return GS_BAD;

   i = 1;
   do
   {
      if (GetTempTabName(TableRef, TempTabName) == GS_BAD)
         { if (IsTransactionSupported == GS_GOOD) RollbackTrans(); return GS_BAD; }
   } // faccio al max 10 tentivi
   while (CreateTable(TempTabName.get_name(), DefStru, ONETEST) == GS_BAD && i++ < 10);
   if (i > 10) { if (IsTransactionSupported == GS_GOOD) RollbackTrans(); return GS_BAD; }

   if (DestFields.get_name() && SourceFields.get_name())
   {
      statement = _T("INSERT INTO ");
      statement += TempTabName;
      statement += _T(" (");
      statement += DestFields;
      statement += _T(") SELECT ");
      statement += SourceFields;
      statement += _T(" FROM ");
      statement += TableRef;

      if (gsc_ExeCmd(pConn, statement, NULL, retest) == GS_BAD)
      {
         // devo cancellare la tabella temporanea (il Rollback non la rimuove)
         DelTable(TempTabName.get_name(), retest);
         if (IsTransactionSupported == GS_GOOD) RollbackTrans();
         GS_ERR_COD = eGSUpdStruct;
         return GS_BAD;
      }
   }

   // memorizzo gli indici utilizzati dalla tabella originale
   if (getTableIndexes(TableRef, &p, retest) == GS_BAD)
   {
      // devo cancellare la tabella temporanea (il Rollback non la rimuove)
      DelTable(TempTabName.get_name(), retest);
      if (IsTransactionSupported == GS_GOOD) RollbackTrans();
      GS_ERR_COD = eGSUpdStruct;
      return GS_BAD;
   }
   IndexList << p;

   // se non è supportato il Rollback e
   // il riferimento completo è una path di un file (il catalogo è il direttorio)
   if (IsTransactionSupported != GS_GOOD && get_CatalogResourceType() == DirectoryRes)
   {
      C_STRING Catalog, Schema, Table, SourcePath, DestPath;

      do
      {
         if (split_FullRefTable(TableRef, Catalog, Schema, Table) == GS_BAD) break;
         if (gsc_RefTable2Path(DBMSName.get_name(), Catalog, Schema, Table, DestPath) == GS_BAD)
            break;

         if (split_FullRefTable(TempTabName, Catalog, Schema, Table) == GS_BAD) break;
         if (gsc_RefTable2Path(DBMSName.get_name(), Catalog, Schema, Table, SourcePath) == GS_BAD)
            break;

         // è sufficiente cancellare la tabella (che perderà i suoi indici) e
         // successivamente rinominare la tabella temporanea come la tabella originale
         if (DelTable(TableRef, retest) == GS_BAD) break;

         if (DBMSName.comp(_T("DBASE")) == 0)
         {  // caso DBASE (controllo file MEMO)
            C_STRING SourceMemo;

            gsc_splitpath(SourcePath, &Catalog, &Schema, &Table);
            SourceMemo = Catalog;
            SourceMemo += Schema;
            SourceMemo += Table;
            SourceMemo += _T(".DBT");

            if (gsc_path_exist(SourceMemo) == GS_GOOD)
            {
               C_STRING DestMemo;

               gsc_splitpath(DestPath, &Catalog, &Schema, &Table);
               DestMemo = Catalog;
               DestMemo += Schema;
               DestMemo += Table;
               DestMemo += _T(".DBT");

               if (gsc_renfile(SourceMemo.get_name(), DestMemo.get_name()) == GS_BAD) break;
            }
         }
         else
         if (DBMSName.comp(_T("FOXPRO")) == 0 ||
             DBMSName.comp(_T("VISUAL FOXPRO")) == 0)
         {  // caso FOXPRO e VISUAL FOXPRO (controllo file MEMO, GENERAL e OLE)
            C_STRING SourceMemo;

            gsc_splitpath(SourcePath, &Catalog, &Schema, &Table);
            SourceMemo = Catalog;
            SourceMemo += Schema;
            SourceMemo += Table;
            SourceMemo += _T(".FPT");

            if (gsc_path_exist(SourceMemo) == GS_GOOD)
            {
               C_STRING DestMemo;

               gsc_splitpath(DestPath, &Catalog, &Schema, &Table);
               DestMemo = Catalog;
               DestMemo += Schema;
               DestMemo += Table;
               DestMemo += _T(".FPT");

               if (gsc_renfile(SourceMemo.get_name(), DestMemo.get_name()) == GS_BAD) break;
            }
         }

         // rinomino la tabella temporanea come la tabella originale
         if (gsc_renfile(SourcePath.get_name(), DestPath.get_name()) == GS_BAD) break;

         res = GS_GOOD;
      }
      while (0);

      if (res == GS_BAD)
      {  // devo cancellare la tabella temporanea
         DelTable(TempTabName.get_name(), retest);
         GS_ERR_COD = eGSUpdStruct;
         return GS_BAD;
      }
   }
   else // se è supportato il Rollback
   {
      do
      {
         // cancello la tabella (che perderà i suoi indici)
         if (DelTable(TableRef, retest) == GS_BAD) break;
         
         // copio la tabella temporanea creando la tabella originale (senza indici)
         if (CopyTable(TempTabName.get_name(), TableRef, GSStructureAndDataCopyType, GS_BAD) == GS_BAD) break;

         // cancello la tabella temporanea
         if (DelTable(TempTabName.get_name(), retest) == GS_BAD) break;

         res = GS_GOOD;
      }
      while (0);

      if (res == GS_BAD)
      {
         // devo cancellare la tabella temporanea (il Rollback non la rimuove)
         DelTable(TempTabName.get_name(), retest);
         if (IsTransactionSupported == GS_GOOD) RollbackTrans();
         GS_ERR_COD = eGSUpdStruct;
         return GS_BAD;
      }
   }

   // anche se la funzione fallisce non fermo l'esecuzione e non segnalo errore
   CreateIndex(IndexList.get_head(), TableRef, ONETEST);

   if (IsTransactionSupported == GS_GOOD)
      if (CommitTrans() == GS_BAD) return GS_BAD;
         
   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBCONNECTION::DelRows               <external> */
/*+
  Questa funzione cancella tutte le righe della tabella.
  Parametri:
  const TCHAR *TableRef;
  const TCHAR *WhereSql;  Condizione di filtro (default = NULL)
  int retest;             se MORETESTS -> in caso di errore riprova n volte 
                          con i tempi di attesa impostati poi ritorna GS_BAD,
                          ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                          (default = MORETESTS)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::DelRows(const TCHAR *TableRef, const TCHAR *WhereSql, int retest)
{
   return gsc_DBDelRows(pConn, TableRef, WhereSql, retest);
}


/*********************************************************/
/*.doc C_DBCONNECTION::RowsQty               <external> */
/*+
  Questa funzione conta tutte le righe della tabella.
  Parametri:
  const TCHAR *TableRef;
  const TCHAR *WhereSql;  Condizione di filtro (default = NULL)

  Restituisce il numero di righe in caso di successo altrimenti -1.
-*/  
/*********************************************************/
long C_DBCONNECTION::RowsQty(const TCHAR *TableRef, const TCHAR *WhereSql)
{
   C_STRING      statement;
   _RecordsetPtr pRs;
   long          n;
   C_RB_LIST     ColValues;
   presbuf       p;

   // preparo la statement
   statement = _T("SELECT COUNT(*) FROM ");
   statement += TableRef;
   if (WhereSql)
   {
      statement += _T(" WHERE ");
      statement += WhereSql;
   }

   // leggo le righe della tabella
   if (OpenRecSet(statement, pRs, adOpenForwardOnly, adLockReadOnly) == GS_BAD) return -1;
 
   if (gsc_isEOF(pRs) == GS_GOOD) { gsc_DBCloseRs(pRs); return -1; }

   if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { gsc_DBCloseRs(pRs); return -1; }
   gsc_DBCloseRs(pRs);

   if ((p = ColValues.nth(0)) && (p = gsc_nth(1, p))) 
   {
      if (gsc_rb2Lng(p, &n) == GS_BAD) n = 0;
   }
   else return -1;

   return n;
}


/*********************************************************/
/*.doc C_DBCONNECTION::get_DefValue           <external> */
/*+	   
   Restituisce il valore di default per un campo di tipo conosciuto.
   Riceve come parametri:
   const TCHAR *DataDescr; Descrizione del tipo di dato
   presbuf *Result;        Resbuf di output

   Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::get_DefValue(const TCHAR *DataDescr, presbuf *Result)
{
   DataTypeEnum DataType;

   if (!DataDescr || !Result) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if (ProviderDescr2InfoType(DataDescr, &DataType) == GS_BAD) return GS_BAD;
   
   switch (DataType)
   {
      case adEmpty:
      case adBoolean:
      case adError:
      case adUserDefined:
      case adIDispatch:
      case adIUnknown:
      case adVariant:
      case adDate:
      case adDBDate:
      case adDBTime:
      case adDBTimeStamp:
      case adBSTR:
      case adChar:
      case adVarChar:
      case adLongVarChar:
      case adWChar:
      case adVarWChar:
      case adLongVarWChar:
      case adBinary:
      case adVarBinary:
      case adLongVarBinary:
      case adChapter:
      case adFileTime:
      case adPropVariant:
         if ((*Result = acutBuildList(RTNIL, 0)) == NULL)
	         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         break;
      case adTinyInt:
      case adSmallInt:
      case adInteger:
      case adBigInt:
      case adUnsignedTinyInt:
      case adUnsignedSmallInt:
      case adUnsignedInt:
      case adUnsignedBigInt:
      case adSingle:
      case adDouble:
      case adCurrency:
      case adDecimal:
      case adNumeric:
      case adVarNumeric:
         if ((*Result = acutBuildList(RTSHORT, 0, 0)) == NULL)
	         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         break;
      default :
         { GS_ERR_COD = eGSUnkwnDrvFieldType; return GS_BAD; }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBCONNECTION::CheckSql               <external> */
/*+	   
  Controlla la correttezza dell'istruzione SQL.
  Parametri:
  const TCHAR *Statement;      Istruzione SQL
  
  Restituisce NULL in caso di successo oppure una stringa con messaggio 
  di errore. 
  N.B. Alloca memoria
-*/  
/*********************************************************/
TCHAR *C_DBCONNECTION::CheckSql(const TCHAR *Statement)
{
   return gsc_DBCheckSql(pConn, Statement);
}


/*********************************************************/
/*.doc C_DBCONNECTION::GetNumAggrValue        <external> */
/*+	   
  Questa funzione è legge un numero risultato di una funzione di aggregazione
  (SUM, COUNT, MIN, MAX, ...) da una tabella.
  Parametri:
  _ConnectionPtr &pConn;  Puntatore alla connessione database
  const TCHAR *TableRef;  Riferimento completo della tabella
  const TCHAR *FieldName; Nome del campo su cui effettuare la funz. di aggr.
  const TCHAR *AggrFun;   Funzione di Aggregazione
  ads_real *Value;        Risultato

  Restituisce GS_GOOD in caso di successo, altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::GetNumAggrValue(const TCHAR *TableRef, const TCHAR *FieldName,
                                    const TCHAR *AggrFun, ads_real *Value)
{
   C_STRING Name(FieldName);

   if (gsc_AdjSyntax(Name, get_InitQuotedIdentifier(), get_FinalQuotedIdentifier(),
                     get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;

   return gsc_GetNumAggrValue(pConn, TableRef, Name.get_name(), AggrFun, Value);
}


/*********************************************************/
/*.doc C_DBCONNECTION::ReadRows               <external> */
/*+
  Questa funzione legge tutte le righe fino in fondo al recordset.
  Parametri: 
  C_STRING  &statement; Istruzione SQL
  C_RB_LIST &ColValues; Lista dei valori da leggere
                        (((<attr><val>)...) ((<attr><val>)...) ...)
  int retest;           Se MORETESTS -> in caso di errore riprova n volte 
                        con i tempi di attesa impostati poi ritorna GS_BAD,
                        ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
*/  
/*********************************************************/
int C_DBCONNECTION::ReadRows(C_STRING &statement, C_RB_LIST &ColValues, int retest)
{
   _RecordsetPtr pRs;

   if (OpenRecSet(statement, pRs, adOpenForwardOnly, adLockReadOnly, retest) == GS_BAD)
      return GS_BAD;
   if (gsc_DBReadRows(pRs, ColValues) == GS_BAD)
      { gsc_DBCloseRs(pRs); return GS_BAD; }
   
   return gsc_DBCloseRs(pRs);
}



/*********************************************************/
/*.doc GetParamNames                          <external> /*
/*+
  Questa funzione restituisce la lista dei nomi dei parametri 
  dell'istruzione SQL.
  I parametri sono identificati da ":" seguito dal nome del parametro
  es. "SELECT CAMPO1 FROM TAB WHERE CAMPO2=:PARAM1").
  Parametri:
  C_STRING  &Stm;          Istruzione SQL
  C_STR_LIST &ParamList;   Lista dei nomi dei parametri

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::GetParamNames(C_STRING &Stm, C_STR_LIST &ParamList)
{
   C_STRING        Where;
   int             i = 0, j;
   int             InStr = FALSE, InParam = FALSE, Skip, EndOfStr;
   TCHAR           buff[1024], *pCond, ch;
   TCHAR           *LiteralPrefix, *LiteralSuffix;
   C_PROVIDER_TYPE *pProviderType;

   ParamList.remove_all();

   // Cerco il tipo carattere nella connessione OLE-DB
   if (!(pProviderType = getCharProviderType())) return GS_BAD;

   LiteralPrefix = pProviderType->get_LiteralPrefix();
   LiteralSuffix = pProviderType->get_LiteralSuffix();

   if (Stm.len() == 0) return GS_GOOD;
   // se non esiste la condizione WHERE
   if ((pCond = Stm.at(_T(" WHERE "), GS_BAD)) == NULL) return GS_GOOD;
   Where = pCond + gsc_strlen(_T(" WHERE "));

   pCond = Where.get_name();

   // cerco i parametri (: seguito da una serie di caratteri alfanumerici o '_')
   while ((ch = pCond[i]) != _T('\0'))
   {
      if (ch == LiteralPrefix[0]) // inizio-fine stringa
         InStr = !InStr;
      else
      if (ch == _T(':')) // Se non sono in una stringa
      {
         InParam = TRUE;
         j       = 0;
      }
      else
         if (!InStr && InParam)
         {
            Skip = FALSE;
            if (iswalnum(ch) || ch == _T('_'))
            {
               buff[j++] = ch;
               buff[j]   = _T('\0');
               Skip = TRUE;
            }

            EndOfStr = (pCond[i + 1] == _T('\0')) ? TRUE : FALSE;

            if (!Skip || EndOfStr)
            {
               InParam = FALSE;
               ParamList.add_tail_unique(buff, FALSE);                       
            }
         }

      i++;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc SetParamValues                     <external> /*
/*+
  Questa funzione sostituisce i parametri con i valori letti da una lista di resbuf
  ((param_name param_val) ...). 
  I parametri sono identificati da ":" seguito dal nome del parametro
  es. "SELECT CAMPO1 FROM TAB WHERE CAMPO2=:PARAM1").
  Parametri:
  C_STRING  &Stm;          Istruzione SQL
  C_RB_LIST &ColValues;    Lista di resbuf con i nomi dei parametri e i valori
  UserInteractionModeEnum WINDOW_mode; Flag, se TRUE la richiesta dei parametri sconosciuti
                                             avviene tramite finestra (default = GSTextInteraction) 

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::SetParamValues(C_STRING &Stm, C_RB_LIST *pColValues, 
                                   UserInteractionModeEnum WINDOW_mode)
{
   return SetParamValues(Stm, (pColValues) ? pColValues->get_head() : NULL, WINDOW_mode);
}
int C_DBCONNECTION::SetParamValues(C_STRING &Stm, presbuf pColValues, 
                                   UserInteractionModeEnum WINDOW_mode)
{
   C_STRING        Where;
   int             i = 0, j, n, cont_apici;
   int             InStr = FALSE, InParam = FALSE, JustRead = FALSE, Skip, EndOfStr;
   TCHAR           buff[1024], buffer[128], *pCond, ch, *p;
   TCHAR           *LiteralPrefix, *LiteralSuffix;
   C_STRING        result, value, prefix, test;
   presbuf         pColValue;
   C_2STR_LIST     ParamList;
   C_2STR          *pParam;
   C_PROVIDER_TYPE *pProviderType;

   // Cerco il tipo carattere nella connessione OLE-DB
   if (!(pProviderType = getCharProviderType())) return GS_BAD;

   LiteralPrefix = pProviderType->get_LiteralPrefix();
   LiteralSuffix = pProviderType->get_LiteralSuffix();

   if (Stm.len() == 0) return GS_GOOD;
   // se non esiste la condizione WHERE
   if ((pCond = Stm.at(_T(" WHERE "), GS_BAD)) == NULL) return GS_GOOD;
   Where = pCond + gsc_strlen(_T(" WHERE "));

   prefix.set_name(Stm.get_name(), 0, (int) (pCond - Stm.get_name()));
   prefix += _T("WHERE ");

   pCond = Where.get_name();

   // cerco i parametri (: seguito da una serie di caratteri alfanumerici o '_')
   while ((ch = pCond[i]) != _T('\0'))
   {
      if (ch == LiteralPrefix[0]) // inizio-fine stringa
         InStr = !InStr;
      else
      if (ch == _T(':')) // Se non sono in una stringa
      {
         InParam = TRUE;
         j       = 0;
      }
      else
         if (!InStr && InParam)
         {
            Skip = FALSE;
            if (iswalnum(ch) || ch == _T('_'))
            {
               buff[j++] = ch;
               buff[j]   = _T('\0');
               Skip = TRUE;
            }

            EndOfStr = (pCond[i + 1] == _T('\0')) ? TRUE : FALSE;

            if (!Skip || EndOfStr)
            {
               InParam = FALSE;

               // Verifico se il parametro è già stato chiesto (non sensitive)
               if (!(pParam = (C_2STR *) ParamList.search_name(buff, FALSE)))
               {
                  // Cerco il valore del parametro dalla lista pColValues (non sensitive)
                  if (pColValues && (pColValue = gsc_nth(1, gsc_assoc(buff, pColValues, FALSE))))
                  {
                     C_STRING Out;

                     Out.paste(gsc_rb2str(pColValue));
                     gsc_strcpy(buffer, Out.get_name(), 128);
                  }
                  else
                  {
                     if (WINDOW_mode == GSTextInteraction)
                     {
                        acutPrintf(GS_LFSTR);
                        acutPrintf(gsc_msg(651), buff); // "Inserire valore parametro <%s>: "
                        acedGetString(1, GS_EMPTYSTR, buffer);
                     }
                     else
                     {
                        TCHAR    Msg[TILE_STR_LIMIT];
                        C_STRING Out;
                        int      Result;

                        swprintf(Msg, TILE_STR_LIMIT, gsc_msg(651), buff); // "Inserire valore parametro <%s>: "
                        if ((Result = gsc_ddinput(Msg, Out)) != GS_GOOD) return Result;
                        gsc_strcpy(buffer, Out.get_name(), 128);
                     }
                  }

                  // aggiungo il parametro alla lista dei parametri già valorizzati
                  if ((pParam = new C_2STR(buff, buffer)) == NULL)
                     { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
                  ParamList.add_tail(pParam);
               }
               else // Se il parametro erà già stato chiesto                    
                  gsc_strcpy(buffer, pParam->get_name2(), 128);
                        
               Skip = FALSE;
               if (gsc_is_numeric(buffer) == GS_GOOD)
               {
                  value = buffer;
                  // provo considerando il parametro come un numero
                  test = prefix;
                  test += result;
                  // valore esplicito
                  test += value;
                  if (!EndOfStr)
                     test += (pCond + i);
                  // test di correttezza sintassi SQL (un test senza stampa errori)
                  if (ExeCmd(test, NULL, ONETEST, GS_BAD) == GS_GOOD)
                     Skip = TRUE;
               }
               if (!Skip)
               {
                  // devo anteporre il carattere di LiteralPrefix al carattere LiteralPrefix
                  // es. "DELL'ORTO" -> "DELL''ORTO"
                  value = LiteralPrefix;
                  p = buffer;
                  n = gsc_strsep(p, _T('\0'), LiteralPrefix[0]);
                  for (cont_apici = 0; cont_apici < n; cont_apici++)
                  {
                     value += p;
                     value += LiteralPrefix;
                     value += LiteralPrefix; // raddoppio il carattere
                     while (*p != _T('\0')) p++;
                     p++;
                  }
                  value += p;
                  value += LiteralSuffix;
                  // provo considerando il parametro come una stringa
                  test = prefix;
                  test += result;
                  // valore esplicito
                  test += value;
                  if (!EndOfStr)
                     test += (pCond + i);
                  // test di correttezza sintassi SQL
                  if (ExeCmd(test, NULL, ONETEST, GS_BAD) != GS_GOOD)
                  {
                     GS_ERR_COD = eGSInvalidSqlStatement;
                     return GS_BAD;
                  }
               }

               result += value;
               if (EndOfStr) JustRead = TRUE;
            }
         }

      if (!InParam && !JustRead || InStr)
         result += ch;
      else
         if (JustRead) JustRead = FALSE;

      i++;
   }

   Stm = prefix;
   Stm += result;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBCONNECTION::GetTable_iParamType    <external> /*
/*+
  Questa funzione cerca, se esistente, il tipo del parametro i-esimo
  della tabella. Si usa quando la tabella in realtà è una funzione SQL
  (es. "SELECT * FROM mia_fun(NULL::timestamp)") in cui il parametro non
  ha un valore ed è quindi dichiarato "NULL" seguito da il cast sul tipo
  (es. "::timestamp").
  Parametri:
  C_STRING &TableRef;      Riferimento alla tabella (es. "catalogo.schema.tab(NULL::timestamp)")
  int i;                   i-esimo parametro (1-indexed)
  DataTypeEnum *ParamType; Tipo del parametro

  Restituisce GS_GOOD se esiste il parametro i-esimo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::GetTable_iParamType(C_STRING &TableRef, int i, DataTypeEnum *ParamType)
{
   C_STRING Catalog, Schema, Table;
   int      n = 1;
   TCHAR    *p, *pType;

   *ParamType = adEmpty;
   // faccio lo split ottenendo i componenti grezzi (Catalog, Schema, Table)
   if (split_FullRefTable(TableRef, Catalog, Schema, Table, DML, true) == GS_BAD)
      return GS_BAD;
   Table.alltrim();
   // Se inizia con un delimitatore vado al delimitatore finale
   if (Table.get_chr(0) == get_InitQuotedIdentifier())
      while (Table.get_chr(n) != _T('\0'))
      {
         // se il carattere è il delimitatore finale
         if (Table.get_chr(n) == get_FinalQuotedIdentifier())
         {
            // se il carattere successivo NON è un altro delimitatore finale allora è terminato
            if (Table.get_chr(n + 1) != get_FinalQuotedIdentifier()) break;
            n++;
         }
         n++;
      }

   // se non ci sono parametri
   if ((p = gsc_strstr(Table.get_name() + n, _T("("))) == NULL)
      return GS_BAD;

   n = 0;
   while (n < i)
   {
      // Se esiste la stringa "NULL::" preceduta da "(" o " " o ","
      if ((p = gsc_strstr(p, _T("NULL::"), FALSE)) && 
            wcschr(_T("( ,"), *(p-1)))
      { // è un parametro
         n++;
         p = p + gsc_strlen(_T("NULL::"));
         if (i == n) break;
      }
      else
         return GS_BAD;
   }
   
   while (p && *p == _T(' ')) p++; // salto spazi iniziali

   if ((pType = gsc_strstr(p, _T("TIMESTAMP"), FALSE)) && pType == p)
      *ParamType = adDBTimeStamp;
   else
   if ((pType = gsc_strstr(p, _T("VARCHAR"), FALSE)) && pType == p)
      *ParamType = adVarChar;
   else
   if ((pType = gsc_strstr(p, _T("INTEGER"), FALSE)) && pType == p)
      *ParamType = adInteger;
   else
      return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBCONNECTION::SetTable_iParam    <external> /*
/*+
  Questa funzione sostituisce al parametro i-esimo della tabella, il suo valore.
  Si usa quando la tabella in realtà è una funzione SQL
  (es. "SELECT * FROM mia_fun(NULL::timestamp)") in cui il parametro non
  ha un valore ed è quindi dichiarato "NULL" seguito da il cast sul tipo
  (es. "::timestamp"). Il valore NULL viene sostituito dal valore del parametro
  (es. "SELECT * FROM mia_fun('2011-10-27 23:59:59'::timestamp)")
  Parametri:
  C_STRING &TableRef;      Riferimento alla tabella (es. "catalogo.schema.mia_fun(NULL::timestamp)")
                           che viene cambiato (es. "SELECT * FROM mia_fun('2011-10-27 23:59:59'::timestamp)")
  int i;                   i-esimo parametro
  C_STRING &ParamValue;    Valore del parametro in formato stringa
  DataTypeEnum ParamType;  Tipo di valore del valore

  Restituisce GS_GOOD se esiste il parametro i-esimo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::SetTable_iParam(C_STRING &TableRef, int i, C_STRING &ParamValue, DataTypeEnum ParamType)
{
   C_STRING Catalog, Schema, Table, Value(ParamValue), Parameters;
   int      n = 1;
   TCHAR    *p, *pStart;

   switch (ParamType)
   {
      case adDBDate: case adDBTimeStamp: case adDate: case adDBTime:
      case adBSTR: case adChar: case adVarChar: case adLongVarChar: case adWChar: case adVarWChar: case adLongVarWChar:    
         if (Str2SqlSyntax(Value) == GS_BAD) return GS_BAD;
   }

   // faccio lo split ottenendo i componenti grezzi (Catalog, Schema, Table)
   if (split_FullRefTable(TableRef, Catalog, Schema, Table, DML, true) == GS_BAD)
      return GS_BAD;
   Table.alltrim();
   // Se inizia con un delimitatore vado al delimitatore finale
   if (Table.get_chr(0) == get_InitQuotedIdentifier())
      while (Table.get_chr(n) != _T('\0'))
      {
         // se il carattere è il delimitatore finale
         if (Table.get_chr(n) == get_FinalQuotedIdentifier())
         {
            // se il carattere successivo NON è un altro delimitatore finale allora è terminato
            if (Table.get_chr(n + 1) != get_FinalQuotedIdentifier()) break;
            n++;
         }
         n++;
      }

   // se ci sono parametri
   if ((pStart = gsc_strstr(Table.get_name() + n, _T("("))))
   {
      p = pStart;
      n = 0;
      while (n < i)
      {
         // Se esiste la stringa "NULL::" preceduta da "(" o " " o ","
         if ((p = gsc_strstr(p, _T("NULL::"), FALSE)) && 
             wcschr(_T("( ,"), *(p-1)))
         { // è un parametro
            n++;
            if (i == n) break;
         }
         else
            if (*p == _T('\0')) return GS_BAD;
      }
   
      Parameters.set_name(pStart, 0, (int) (p - pStart - 1));
      Parameters += Value;
      Parameters += p + gsc_strlen(_T("NULL"));

      if (split_FullRefTable(TableRef, Catalog, Schema, Table) == GS_BAD)
         return GS_BAD;
      if (TableRef.paste(get_FullRefTable(Catalog, Schema, Table, DML, &Parameters)) == NULL)
         return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBCONNECTION::getCharProviderType    <internal> /*
/*+
  Questa funzione cerca tra i tipi supportati dalla conessione OLE-DB
  un tipo carattere.

  Restituisce il puntatore al tipo in caso di successo altrimenti restituisce NULL.
-*/  
/*********************************************************/
C_PROVIDER_TYPE *C_DBCONNECTION::getCharProviderType(void)
{
   if (!pCharProviderType)
      // Cerco un tipo carattere nella connessione OLE-DB
      if (!(pCharProviderType = ptr_DataTypeList()->search_Type(adChar, FALSE)))
         if (!(pCharProviderType = ptr_DataTypeList()->search_Type(adWChar, FALSE)))
            if (!(pCharProviderType = ptr_DataTypeList()->search_Type(adBSTR, FALSE)))      
               if (!(pCharProviderType = ptr_DataTypeList()->search_Type(adVarChar, FALSE)))
                  pCharProviderType = ptr_DataTypeList()->search_Type(adVarWChar, FALSE);
   
   return pCharProviderType;
}


/*********************************************************/
/*.doc Str2SqlSyntax                          <external> /*
/*+
  Questa funzione restituisce un valore carattere corretto con la sintassi SQL
  per essere usato in una istruzione SQL (es. "dell'orto" -> "'dell''orto'" 
  oppure "vasca\serbatoio" -> "'vasca\\serbatoio'" se supportati i carattere di escape)
  Parametri:
  C_STRING        &Value;           Valore stringa da trasformare
  C_PROVIDER_TYPE *pProviderType;   Tipo di campo se conosciuto
                                    altrimenti la funzione cercherà un tipo 
                                    carattere tra quelli supportati (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::Str2SqlSyntax(C_STRING &Value, C_PROVIDER_TYPE *pProviderType)
{
   C_STRING NewValue, DoubleLiteralSuffix;

   if (Value.get_name() == NULL)
   {
      NewValue = _T("NULL");
      return GS_GOOD;
   }

   if (!pProviderType) // se non specificato
      if ((pProviderType = getCharProviderType()) == NULL) return GS_BAD;

   if (gsc_DBIsDate(pProviderType->get_Type()) == GS_GOOD)
   {
      if (gsc_getSQLDate(Value.get_name(), NewValue) == GS_BAD) return GS_BAD;
      Value.clear();
      if (DBMSName.at(ACCESS_DBMSNAME, FALSE) == NULL) // se NON è in Microsoft Access roby 2016
		   Value += _T("DATE "); 
      Value += pProviderType->get_LiteralPrefix();
      Value += NewValue;
      Value += pProviderType->get_LiteralSuffix();

      return GS_GOOD;
   }
   else
   if (gsc_DBIsTimestamp(pProviderType->get_Type()) == GS_GOOD)
   {
      if (gsc_getSQLDateTime(Value.get_name(), NewValue) == GS_BAD) return GS_BAD;
		Value = _T("TIMESTAMP "); 
      Value += pProviderType->get_LiteralPrefix();
      Value += NewValue;
      Value += pProviderType->get_LiteralSuffix();

      return GS_GOOD;
   }
   else
   if (gsc_DBIsTime(pProviderType->get_Type()) == GS_GOOD)
   {
      if (gsc_getSQLTime(Value.get_name(), NewValue) == GS_BAD) return GS_BAD;
		Value = _T("TIME "); 
      Value += pProviderType->get_LiteralPrefix();
      Value += NewValue;
      Value += pProviderType->get_LiteralSuffix();

      return GS_GOOD;
   }

   DoubleLiteralSuffix = pProviderType->get_LiteralSuffix();
   DoubleLiteralSuffix += pProviderType->get_LiteralSuffix();

   NewValue = Value;

   // raddoppio eventuali caratteri di fine stringa (es. " o ')
   NewValue.strtran(pProviderType->get_LiteralSuffix(), DoubleLiteralSuffix.get_name());

   if (get_EscapeStringSupported())
      // raddoppio eventuali caratteri di escape "\"
      NewValue.strtran(_T("\\"), _T("\\\\"));

   Value = pProviderType->get_LiteralPrefix();
   Value += NewValue;
   Value += pProviderType->get_LiteralSuffix();

   return GS_GOOD;
}
int C_DBCONNECTION::Rb2SqlSyntax(presbuf pRb, C_STRING &Value, C_PROVIDER_TYPE *pProviderType)
{
   switch (pRb->restype)
   {
      case RTSTR:
      {
         Value = pRb->resval.rstring;
         // Correggo la stringa secondo la sintassi SQL
         if (Str2SqlSyntax(Value, pProviderType) == GS_BAD) return GS_BAD;
         break;
      }
      case RTNIL: case RTNONE: case RTVOID:
         Value = _T("NULL");
         break;
      default:
         Value.paste(gsc_rb2str(pRb));
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBCONNECTION::getPostGIS_Version     <external> */
/*+
  Questa funzione legge la verisone di postgis caricata in postgresql.
  Funzione valida solo per connessioni postgresql.
  Parametri:
  C_STRING &Version:   verisone di postgis
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::getPostGIS_Version(C_STRING &Version)
{
   _RecordsetPtr pRs;
   C_RB_LIST     ColValues;
   presbuf       p;
   C_STRING      stm(_T("SELECT PostGIS_Version()"));

   if (DBMSName.at(PG_DBMSNAME, FALSE) == NULL) // non è in PostgreSQL
      return GS_BAD;
   if (OpenRecSet(stm, pRs) == GS_BAD)
      return GS_BAD;
   if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
      { gsc_DBCloseRs(pRs); return GS_BAD; }
   gsc_DBCloseRs(pRs);
   if ((p = ColValues.nth(0)) && (p = gsc_nth(1, p)))
   {
      Version = p;
      return GS_GOOD;
   }
   else
      return GS_BAD;
}


/*********************************************************/
/*.doc C_DBCONNECTION::AddGeomField           <external> */
/*+
  Questa funzione aggiunge un campo geometrico ad una tabella.
  Parametri:
  const TCHAR *TableRef:   Riferimento completo alla tabella
  const TCHAR *FieldName;  Nome del campo geometrico da aggiungere
  const TCHAR *SRID;       Codice sistema di coordinate
  int         GeomType;    Tipo di dato geometrico (TYPE_POLYLINE, TYPE_TEXT,
                           TYPE_NODE, TYPE_SURFACE, TYPE_SPAGHETTI)
  GeomDimensionEnum n_dim; Numero di dimensioni (2D, 3D)
  int         retest;      Se MORETESTS -> in caso di errore riprova n volte 
                           con i tempi di attesa impostati poi ritorna GS_BAD,
                           ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                           (default = MORETESTS)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::AddGeomField(const TCHAR *TableRef, const TCHAR *FieldName, const TCHAR *SRID,
                                 int GeomType, GeomDimensionEnum n_dim, int retest)
{
   C_STRING  Stm;  // istruzione SQL
   C_STRING  Catalog, Schema, Table, AdjFieldName(FieldName);
   C_INT_STR *pGeomDescr;

   // Della serie: l'SQL è uno standard internazionale... (di sto cazzo) ... è bravo POLTI !!!!
  
   if (split_FullRefTable(TableRef, Catalog, Schema, Table) == GS_BAD)
      return GS_BAD;
   // non aggiungo i separatori perchè per postgis sono = " mentre in questa
   // sintassi ci vuole il carattere '
   if (gsc_AdjSyntax(AdjFieldName, _T(''), _T(''), get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;

   // Cerco la descrizione SQL del tipo geometrico
   if ((pGeomDescr = (C_INT_STR *) GeomTypeDescrList.search_key(GeomType)) == NULL)
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if (DBMSName.at(PG_DBMSNAME, FALSE) != NULL) // è in PostgreSQL
   {
      C_STRING ValidityConstraintName;

      // SELECT AddGeometryColumn('schema','table','column',srid,type,'dimension')
      Stm = _T("SELECT AddGeometryColumn('");
      Stm += Schema;
      Stm += _T("','");
      Stm += Table;
      Stm += _T("','");
      Stm += AdjFieldName;
      Stm += _T("',");
      Stm += SRID;
      Stm += _T(",'"); 
      Stm += pGeomDescr->get_name();
      Stm += _T("',");
      Stm += (int) n_dim;
      Stm += _T(")");
      if (gsc_ExeCmd(pConn, Stm, NULL, ONETEST, GS_BAD) == GS_BAD) return GS_BAD;

      // Aggiungo anche il controllo sulla validità della geometria
      // ALTER TABLE mytable ADD CONSTRAINT enforce_valid_geom CHECK (st_isvalid(the_geom));
      ValidityConstraintName = get_InitQuotedIdentifier();
      ValidityConstraintName += _T("geometry_valid_");
      ValidityConstraintName += AdjFieldName;
      ValidityConstraintName += get_FinalQuotedIdentifier();
      AdjFieldName = FieldName;
      if (gsc_AdjSyntax(AdjFieldName, get_InitQuotedIdentifier(), get_FinalQuotedIdentifier(),
                        get_StrCaseFullTableRef()) == GS_BAD)
         return GS_BAD;
      Stm = _T("ALTER TABLE ");
      Stm += TableRef;
      Stm += _T(" ADD CONSTRAINT ");
      Stm += ValidityConstraintName;
      Stm += _T(" CHECK (st_isvalid(");
      Stm += AdjFieldName;
      Stm += _T("));");
      if (gsc_ExeCmd(pConn, Stm, NULL, ONETEST, GS_BAD) == GS_BAD) return GS_BAD;
   }
   else if (DBMSName.at(ORACLE_DBMSNAME, FALSE) != NULL) // è in ORACLE
   {
      // ALTER TABLE 'tabella' ADD ('colonna', MDSYS.SDO_GEOMETRY)
      Stm = _T("ALTER TABLE ");
      Stm += TableRef;
      Stm += _T(" ADD (");
      Stm += AdjFieldName;
      Stm += _T(" MDSYS.SDO_GEOMETRY)");                  
      if (gsc_ExeCmd(pConn, Stm, NULL, ONETEST, GS_BAD) == GS_BAD)
         return GS_BAD;

      // Per fare gli indici spaziali Oracle ha bisogno anche di questa istruzione
      // INSERT INTO USER_SDO_GEOM_METADATA(TABLE_NAME, COLUMN_NAME, DIMINFO, SRID) 
      // VALUES ('tabella', 'colonna', MDSYS.SDO_DIM_ARRAY(
      // MDSYS.SDO_DIM_ELEMENT('X',0,1000,0.005),
      // MDSYS.SDO_DIM_ELEMENT('Y',0,1000,0.005) ), SRID oppure NULL)

      // Prima provo a cancellare la riga per sicurezza
      Stm = _T("DELETE FROM USER_SDO_GEOM_METADATA WHERE TABLE_NAME='");
      Stm += Table;
      Stm += _T("' AND COLUMN_NAME='");
      Stm += AdjFieldName;
      Stm += _T("'");
      if (gsc_ExeCmd(pConn, Stm, NULL, ONETEST, GS_BAD) == GS_BAD)
         return GS_BAD;

      Stm = _T("INSERT INTO USER_SDO_GEOM_METADATA(TABLE_NAME, COLUMN_NAME, DIMINFO, SRID) VALUES ('");
      Stm += Table;
      Stm += _T("', '");
      Stm += AdjFieldName;
      Stm += _T("', MDSYS.SDO_DIM_ARRAY(");
      Stm += _T("MDSYS.SDO_DIM_ELEMENT('X',-10000000,10000000,0.005), ");
      Stm += _T("MDSYS.SDO_DIM_ELEMENT('Y',-10000000,10000000,0.005)), ");
      // Se esiste il sistema di coordinate è ed diverso da nessuno (-1)
      if (gsc_strlen(SRID) > 0 && gsc_strcmp(SRID, _T("-1")) != 0)
          Stm += SRID;
      else
          Stm += _T("NULL");
      Stm += _T(")");
      if (gsc_ExeCmd(pConn, Stm, NULL, ONETEST, GS_BAD) == GS_BAD) return GS_BAD;
   }
   else
      return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBCONNECTION::AddField           <external> */
/*+
  Questa funzione aggiunge un campo alfanumerico ad una tabella.
  Parametri:
  const TCHAR *TableRef:      Riferimento completo alla tabella
  const TCHAR *FieldName;     Nome del campo da aggiungere
  const TCHAR *ProviderDescr; Descrizione tipo del provider
  long        DestSize;       Dimensione tipo
  int         DestPrec;       Precisione tipo
  int         retest;         Se MORETESTS -> in caso di errore riprova n volte 
                              con i tempi di attesa impostati poi ritorna GS_BAD,
                              ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                              (default = MORETESTS)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::AddField(const TCHAR *TableRef, const TCHAR *FieldName, const TCHAR *ProviderDescr,
                             long ProviderLen, int ProviderDec, int retest)
{
   C_STRING        AdjFieldName(FieldName), Params, Stm;
   C_PROVIDER_TYPE *pProviderType;
   int             Qty;
 
   if (gsc_AdjSyntax(AdjFieldName, get_InitQuotedIdentifier(), get_FinalQuotedIdentifier(),
                     get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;

   AdjFieldName += _T(" ");
   AdjFieldName += ProviderDescr;

   if ((pProviderType = (C_PROVIDER_TYPE *) ptr_DataTypeList()->search_name(ProviderDescr, FALSE)) == NULL)
      { GS_ERR_COD = eGSUnkwnDrvFieldType; return GS_BAD; }

   Params = pProviderType->get_CreateParams();
   Qty = (Params.len() > 0) ? gsc_strsep(Params.get_name(), _T('\0')) + 1 : 0;

   if (Qty > 0)
   {
      AdjFieldName += _T("(");
      AdjFieldName += ProviderLen;
      if (Qty > 1)
      {
         AdjFieldName += _T(",");
         AdjFieldName += ProviderDec;
      }
      AdjFieldName += _T(")");
   }

   // ALTER TABLE tabella ADD nome_campo definizione;
   Stm = _T("ALTER TABLE ");
   Stm += TableRef;
   Stm += _T(" ADD ");
   Stm += AdjFieldName;

   return gsc_ExeCmd(pConn, Stm, NULL, retest, GS_BAD);
}


/*********************************************************/
/*.doc C_DBCONNECTION::DropField              <external> */
/*+
  Questa funzione cancella un campo da una tabella.
  Parametri:
  const TCHAR *TableRef:      Riferimento completo alla tabella
  const TCHAR *FieldName;     Nome del campo da cancellare
  int         retest;         Se MORETESTS -> in caso di errore riprova n volte 
                              con i tempi di attesa impostati poi ritorna GS_BAD,
                              ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                              (default = MORETESTS)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::DropField(const TCHAR *TableRef, const TCHAR *FieldName, int retest)
{
   C_STRING AdjFieldName(FieldName), Stm;
 
   if (gsc_AdjSyntax(AdjFieldName, get_InitQuotedIdentifier(), get_FinalQuotedIdentifier(),
                     get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;

   // ALTER TABLE tabella DROP nome_campo;
   Stm = _T("ALTER TABLE ");
   Stm += TableRef;
   Stm += _T(" DROP ");
   Stm += AdjFieldName;
   Stm += _T(" CASCADE ");

   return gsc_ExeCmd(pConn, Stm, NULL, retest, GS_BAD);
}


/*********************************************************/
/*.doc C_DBCONNECTION::RenameField              <external> */
/*+
  Questa funzione rinomina un campo da una tabella.
  Parametri:
  const TCHAR *TableRef:      Riferimento completo alla tabella
  const TCHAR *OldFieldName;  Nome vecchio del campo da rinominare
  const TCHAR *NewFieldName;  Nome nuovo del campo da rinominare
  int         retest;         Se MORETESTS -> in caso di errore riprova n volte 
                              con i tempi di attesa impostati poi ritorna GS_BAD,
                              ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                              (default = MORETESTS)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBCONNECTION::RenameField(const TCHAR *TableRef,
                                const TCHAR *OldFieldName, const TCHAR *NewFieldName,
                                int retest)
{
   C_STRING AdjOldFieldName(OldFieldName), AdjNewFieldName(NewFieldName), Stm;
  
   if (gsc_AdjSyntax(AdjOldFieldName, get_InitQuotedIdentifier(), get_FinalQuotedIdentifier(),
                     get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;
   if (gsc_AdjSyntax(AdjNewFieldName, get_InitQuotedIdentifier(), get_FinalQuotedIdentifier(),
                     get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;

   // ALTER TABLE tabella RENAME COLUMN nome_vecchio_campo TO nome_nuovo_campo;
   Stm = _T("ALTER TABLE ");
   Stm += TableRef;
   Stm += _T(" RENAME COLUMN ");
   Stm += AdjOldFieldName;
   Stm += _T(" TO ");
   Stm += AdjNewFieldName;

   return gsc_ExeCmd(pConn, Stm, NULL, retest, GS_BAD);
}


/******************************************************************************/
/*.doc C_DBCONNECTION::ConvFieldToChar                                        */
/*+
  Questa funzione converte il campo indicato di una tabella a carattere.
  ACCESS non consente di modificare o eliminare più di un campo alla volta.
  ACCESS non consente di rinominare un campo.
  Quindi la funzione aggiunge un nuovo campo temporaneo, copia i dati in questo campo temporaneo,
  cancella il vecchio campo, rinomina il nuovo campo con il vecchio nome.
  ACCESS non consente di fare tutto questo in una transaction quindi devo eseguire un SQL
  specializzato apposta per questo DBMS.
  Parametri:
  C_STRING &TableRef;      Tabella
  const TCHAR *pField;     Campo da convertire
  int   FieldLen;          Lunghezza campo carattere

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
int C_DBCONNECTION::ConvFieldToChar(C_STRING &TableRef, const TCHAR *pField, int FieldLen)
{
   C_STRING  CharType_ProviderDescr, TempField;
   C_RB_LIST Stru;
   presbuf   p;
   int       DataType, CharType_Dec, Result = GS_BAD;
   long      CharType_Len;

   if ((Stru << ReadStruct(TableRef.get_name())) == NULL) return GS_BAD;

   // Se la tabella ha il campo pField di tipo numerico
   if (!(p = Stru.Assoc(pField)) || !(p = p->rbnext) || !(p = p->rbnext)) return GS_BAD;
   if (gsc_rb2Int(p, &DataType) == GS_BAD) return GS_BAD;
   if (gsc_DBIsNumeric((DataTypeEnum) DataType) == GS_GOOD)
   {
      TempField = pField;
      TempField += _T("_TEMP");
   }

   if (TempField.len() == 0) return GS_GOOD;

   C_PROVIDER_TYPE *providerType; // ROBY 2015
   if ((providerType = getCharProviderType()) == NULL) return GS_BAD;

   //((<Name><Type><Dim><Dec><IsLong><IsFixedPrecScale><IsFixedLength><Write>)...)
   // converto campo da codice ADO in Provider dipendente (testo)
   if (Type2ProviderType(providerType->get_Type(), // DataType per campo testo ROBY 2015
                         FALSE,                    // IsLong
                         FALSE,                    // IsFixedPrecScale
                         RTNIL,                    // IsFixedLength
                         TRUE,                     // IsWrite
                         FieldLen,                 // Size
                         0,                        // Prec
                         CharType_ProviderDescr,   // ProviderDescr
                         &CharType_Len, &CharType_Dec) == GS_BAD)
      return GS_BAD;
    
   C_STRING statement;

   int IsTransactionSupported = pConn->BeginTrans();

   do
   {
      C_STRING statement;
      C_STRING AdjFieldName(pField);

      if (gsc_AdjSyntax(AdjFieldName, get_InitQuotedIdentifier(), get_FinalQuotedIdentifier(),
                        get_StrCaseFullTableRef()) == GS_BAD)
         break;

      if (DBMSName.at(ACCESS_DBMSNAME, FALSE) != NULL) // è in SQL Jet (Microsoft Access)
      {
         statement = _T("ALTER TABLE ");
         statement += TableRef;
         statement += _T(" ALTER COLUMN ");
         statement += AdjFieldName;
         statement += _T(" ");
         statement += CharType_ProviderDescr;
         statement += _T("(");
         statement += CharType_Len;
         statement += _T(")");
         if (ExeCmd(statement, NULL, ONETEST) == GS_BAD)
            break;
         Result = GS_GOOD;
         break;
      }

      // modifico struttura tabella x aggiungere il nuovo campo carattere temporaneo
      if (AddField(TableRef.get_name(), TempField.get_name(), CharType_ProviderDescr.get_name(),
                   CharType_Len, 0) == GS_BAD)
         break;

      // copio i valori dalla vecchio campo al nuovo campo
      C_STRING AdjTempFieldName(TempField);

      if (gsc_AdjSyntax(AdjTempFieldName, get_InitQuotedIdentifier(), get_FinalQuotedIdentifier(),
                        get_StrCaseFullTableRef()) == GS_BAD)
         break;

      statement = _T("UPDATE ");
      statement += TableRef;
      statement += _T(" SET ");
      statement += AdjTempFieldName;
      statement += _T(" = CAST(");
      statement += AdjFieldName;
      statement += _T(" AS ");
      statement += CharType_ProviderDescr;
      statement += _T("(");
      statement += CharType_Len;
      statement += _T("))");
      if (ExeCmd(statement, NULL, ONETEST) == GS_BAD)
         break;

      // modifico struttura tabella x cancellare il vecchio campo pField
      if (DropField(TableRef.get_name(), pField) == GS_BAD)
         break;

      // modifico struttura tabella x rinominare il nuovo campo
      if (RenameField(TableRef.get_name(), TempField.get_name(), pField) == GS_BAD)
         break;

      Result = GS_GOOD;
   }
   while (0);

   if (Result == GS_BAD)
      { if (IsTransactionSupported == GS_GOOD) pConn->RollbackTrans(); return GS_BAD; }

   if (IsTransactionSupported == GS_GOOD) pConn->CommitTrans();

   return GS_GOOD;
}


/******************************************************************************/
/*.doc C_DBCONNECTION::ConvFieldToBool                                        */
/*+
  Questa funzione converte il campo indicato di una tabella a boleano.
  ACCESS non consente di modificare o eliminare più di un campo alla volta.
  ACCESS non consente di rinominare un campo.
  Quindi la funzione aggiunge un nuovo campo temporaneo, copia i dati in questo campo temporaneo,
  cancella il vecchio campo, rinomina il nuovo campo con il vecchio nome.
  ACCESS non consente di fare tutto questo in una transaction quindi devo eseguire un SQL
  specializzato apposta per questo DBMS.
  Parametri:
  C_STRING &TableRef;      Tabella
  const TCHAR *pField;     Campo da convertire

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
int C_DBCONNECTION::ConvFieldToBool(C_STRING &TableRef, const TCHAR *pField)
{
   C_STRING  BoolType_ProviderDescr, TempField;
   C_RB_LIST Stru;
   presbuf   p;
   int       DataType, Type_Dec, Result = GS_BAD;
   long      Type_Len;

   if ((Stru << ReadStruct(TableRef.get_name())) == NULL) return GS_BAD;

   // Se la tabella ha il campo pField di tipo numerico
   if (!(p = Stru.Assoc(pField)) || !(p = p->rbnext) || !(p = p->rbnext)) return GS_BAD;
   if (gsc_rb2Int(p, &DataType) == GS_BAD) return GS_BAD;
   if (gsc_DBIsNumeric((DataTypeEnum) DataType) == GS_GOOD)
   {
      TempField = pField;
      TempField += _T("_TEMP");
   }

   if (TempField.len() == 0) return GS_GOOD;

   C_PROVIDER_TYPE *providerType;
   // Cerco un tipo booleano nella connessione OLE-DB
   if ((providerType = ptr_DataTypeList()->search_Type(adBoolean, FALSE)) == NULL) return GS_BAD;

   //((<Name><Type><Dim><Dec><IsLong><IsFixedPrecScale><IsFixedLength><Write>)...)
   // converto campo da codice ADO in Provider dipendente (testo)
   if (Type2ProviderType(providerType->get_Type(), // DataType per campo booleano
                         FALSE,                    // IsLong
                         FALSE,                    // IsFixedPrecScale
                         RTNIL,                    // IsFixedLength
                         TRUE,                     // IsWrite
                         1,                        // Size
                         0,                        // Prec
                         BoolType_ProviderDescr,   // ProviderDescr
                         &Type_Len, &Type_Dec) == GS_BAD)
      return GS_BAD;
    
   C_STRING statement;

   int IsTransactionSupported = pConn->BeginTrans();

   do
   {
      C_STRING statement;
      C_STRING AdjFieldName(pField);

      if (gsc_AdjSyntax(AdjFieldName, get_InitQuotedIdentifier(), get_FinalQuotedIdentifier(),
                        get_StrCaseFullTableRef()) == GS_BAD)
         break;

      if (DBMSName.at(ACCESS_DBMSNAME, FALSE) != NULL) // è in SQL Jet (Microsoft Access)
      {
         statement = _T("ALTER TABLE ");
         statement += TableRef;
         statement += _T(" ALTER COLUMN ");
         statement += AdjFieldName;
         statement += _T(" ");
         statement += BoolType_ProviderDescr;
         if (ExeCmd(statement, NULL, ONETEST) == GS_BAD)
            break;
         Result = GS_GOOD;
         break;
      }
      else
      if (DBMSName.at(PG_DBMSNAME, FALSE) != NULL) // è in PostgreSQL
      {
         // ALTER TABLE <table> ALTER hidden TYPE bool USING (hidden::int::bool);
         statement = _T("ALTER TABLE ");
         statement += TableRef;
         statement += _T(" ALTER ");
         statement += AdjFieldName;
         statement += _T(" TYPE ");
         statement += BoolType_ProviderDescr;
         statement += _T(" USING (");
         statement += AdjFieldName;
         statement += _T("::int::");
         statement += BoolType_ProviderDescr;
         statement += _T(")");
         if (ExeCmd(statement, NULL, ONETEST) == GS_BAD)
            break;
         Result = GS_GOOD;
         break;
      }

      // modifico struttura tabella x aggiungere il nuovo campo booleano temporaneo
      if (AddField(TableRef.get_name(), TempField.get_name(), BoolType_ProviderDescr.get_name(),
                   0, 0) == GS_BAD)
         break;

      // copio i valori dal vecchio campo al nuovo campo
      C_STRING AdjTempFieldName(TempField);

      if (gsc_AdjSyntax(AdjTempFieldName, get_InitQuotedIdentifier(), get_FinalQuotedIdentifier(),
                        get_StrCaseFullTableRef()) == GS_BAD)
         break;

      statement = _T("UPDATE ");
      statement += TableRef;
      statement += _T(" SET ");
      statement += AdjTempFieldName;
      statement += _T(" = CAST(");
      statement += AdjFieldName;
      statement += _T(" AS ");
      statement += BoolType_ProviderDescr;
      if (ExeCmd(statement, NULL, ONETEST) == GS_BAD)
         break;

      // modifico struttura tabella x cancellare il vecchio campo pField
      if (DropField(TableRef.get_name(), pField) == GS_BAD)
         break;

      // modifico struttura tabella x rinominare il nuovo campo
      if (RenameField(TableRef.get_name(), TempField.get_name(), pField) == GS_BAD)
         break;

      Result = GS_GOOD;
   }
   while (0);

   if (Result == GS_BAD)
      { if (IsTransactionSupported == GS_GOOD) pConn->RollbackTrans(); return GS_BAD; }

   if (IsTransactionSupported == GS_GOOD) pConn->CommitTrans();

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//////////////////  C_DBCONNECTION       FINE    //////////////////////////
//////////////////  C_DBCONNECTION_LIST  INIZIO  //////////////////////////
//-----------------------------------------------------------------------//


C_DBCONNECTION_LIST::C_DBCONNECTION_LIST() : C_LIST() {}


C_DBCONNECTION_LIST::~C_DBCONNECTION_LIST() {}


/****************************************************************/
/*.doc C_DBCONNECTION_LIST::get_Connection           <external> */
/*+
  Se il parametro <ConnStrUDLFile> è un file valido di tipo .UDL allora:
  Questa funzione restituisce il puntatore ad una connessione verificando
  tra le connessioni già esistenti se ne esiste una compatibile con il 
  file .UDL personalizzato dalla stringa di proprietà UDL specificata (opzionale).

  Se il parametro <ConnStrUDLFile> non è un file valido di tipo .UDL allora verrà 
  interpretato come una stringa di connessione:
  Questa funzione restituisce il puntatore ad una connessione verificando
  tra le connessioni già esistenti se ne esiste una compatibile con
  la stringa di connessione.

  Se non sarà possibile utilizzare una connessione già esistente verrà 
  creata una nuova connessione (aggiunta in coda alla lista).

  Parametri: 
  const TCHAR *ConnStrUDLFile;   Stringa di connessione o File di tipo .UDL
                                 (se nella stringa di connessione esistono delle
                                 <path>, queste devono essere indicate senza alias).
                                 Se = NULL si utilizza il file UDL di default indicato
                                 dalla macro GEOSIM_DATASOURCE
  C_2STR_LIST *UDLProperties;    Puntatore a lista di proprietà UDL (default = NULL)
  bool Prompt;                   Flag, se = true nel caso la connessione non sia possibile
                                 chiede all'utente una login e password (default = true)
  int PrintError;                Se il flag = GS_GOOD in caso di errore viene
                                 stampato il messaggio relativo altrimenti non
                                 viene stampato alcun messaggio (default = GS_GOOD)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************/
C_DBCONNECTION* C_DBCONNECTION_LIST::get_Connection(const TCHAR *ConnStrUDLFile,
                                                    C_2STR_LIST *UDLProperties,
                                                    bool Prompt,
                                                    int PrintError)
{
   C_DBCONNECTION *pDBConn;
   C_STRING       InitStr, UDLFileName;
   short          UDLFile;

   if (ConnStrUDLFile)
      UDLFileName = ConnStrUDLFile;
   else
   {  // file UDL di default
      UDLFileName = GEOsimAppl::GEODIR;
      UDLFileName += _T('\\');
      UDLFileName += GEODATALINKDIR;
      UDLFileName += _T('\\');
      UDLFileName += GEOSIM_DATASOURCE;
   }

   gsc_getUDLFullPath(UDLFileName);
   // provo a leggere la stringa di connessione
   InitStr.paste(gsc_LoadOleDBIniStrFromFile(UDLFileName.get_name()));
   
   if (InitStr.len() == 0) // se il parametro <ConnStrUDLFile> non è un file valido
   {                       // allora è una stringa di connessione
      UDLFile = FALSE;
      InitStr = ConnStrUDLFile;
   }
   else
      UDLFile = TRUE;

   // personalizzazione stringa di connessione
   if (UDLProperties && UDLProperties->get_count() > 0)
   {
      C_2STR_LIST PropList;

      if (gsc_PropListFromConnStr(InitStr.get_name(), PropList) == GS_BAD) return NULL;
      
      // aggiorno la lista di proprietà UDL
      if (gsc_ModUDLProperties(PropList, *UDLProperties) == GS_BAD) return NULL;
      
      // modifico stringa di inizializzazione connessione OLE-DB
      InitStr.paste(gsc_PropListToConnStr(PropList));
   }
   else
      if (!ConnStrUDLFile) // se si utilizza la connessione di default (Access diretto)
      {                    // e non è stata specificata alcuna proprietà UDL
         C_STRING Catalog; // utilizzo "Data Source = ACCESSGEOMAINDB" (file .ACCDB contenente la lista dei progetti)

         Catalog = GEOsimAppl::GEODIR;
         Catalog += _T('\\');
         Catalog += ACCESSGEOMAINDB;

         C_2STR_LIST PropList, dummy(_T("Data Source"), Catalog.get_name());
   
         if (gsc_PropListFromConnStr(InitStr.get_name(), PropList) == GS_BAD) return NULL;
        
         // aggiorno la lista di proprietà UDL
         if (gsc_ModUDLProperties(PropList, dummy) == GS_BAD) return NULL;
      
         // modifico stringa di inizializzazione connessione OLE-DB
         InitStr.paste(gsc_PropListToConnStr(PropList));
      }

   pDBConn = (C_DBCONNECTION *) get_head();
   while (pDBConn)
   {
      if (pDBConn->IsUsable(InitStr.get_name()) == GS_GOOD) break;
      pDBConn = (C_DBCONNECTION *) get_next();
   }

   // se non esisteva una connessione utilizzabile ne creo una nuova
   if (!pDBConn)
   {
      if (UDLFile) // se esiste il file .UDL
      {
         C_STRING full_path(UDLFileName), drive, dir, name, RequiredFile;

         if (gsc_getUDLFullPath(full_path) == GS_BAD) return NULL;
         gsc_splitpath(full_path, &drive, &dir, &name);
         RequiredFile = drive;
         RequiredFile += dir;
         RequiredFile += name;
         RequiredFile += _T('.');
         RequiredFile += UDL_FILE_REQUIRED_EXT;

         if ((pDBConn = new C_DBCONNECTION(RequiredFile.get_name())) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return NULL; }

         // Se la connessione non supporta il catalogo ed esiste un file campione di DB
         // allora provo a creare il DB prima di aprire la connessione
         if (pDBConn->CatalogResourceType == EmptyRes && pDBConn->DBSample.len() > 0)
         {
            C_2STR *pUDLProp;
   
            if (UDLProperties &&
                (pUDLProp = (C_2STR *) UDLProperties->search_name(_T("Data Source"), FALSE)) != NULL &&
                pUDLProp->get_name2())
            {
               // se il riferimento è un file
               if (gsc_mkdir_from_path(pUDLProp->get_name2()) == GS_GOOD)
                  // creo database copiando un file "campione"
                  if (gsc_path_exist(pUDLProp->get_name2()) == GS_BAD)
                     if (gsc_copyfile(pDBConn->DBSample.get_name(), pUDLProp->get_name2()) == GS_BAD)
                        gsc_rmdir_from_path(pUDLProp->get_name2());
            }
         }
      }
      else
         if ((pDBConn = new C_DBCONNECTION()) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return NULL; }

      if (pDBConn->Open(InitStr.get_name(), Prompt, PrintError) == GS_BAD)
         { delete pDBConn; return NULL; }
      add_tail(pDBConn);
   }

   return pDBConn; 
}
C_DBCONNECTION* C_DBCONNECTION_LIST::search_connection(_ConnectionPtr &pConn)
{
   C_DBCONNECTION *p;

   p = (C_DBCONNECTION *) get_head();
   while (p)
   {
      if (p->get_Connection() == pConn) break;
      p = (C_DBCONNECTION *) get_next();
   }

   return p;
}
C_DBCONNECTION* C_DBCONNECTION_LIST::search_connection(const TCHAR *OrigConnectionStr)
{
   C_DBCONNECTION *p;

   p = (C_DBCONNECTION *) get_head();
   while (p)
   {
      if (gsc_strcmp(p->get_OrigConnectionStr(), OrigConnectionStr) == 0) break;
      p = (C_DBCONNECTION *) get_next();
   }

   return p;
}


//-----------------------------------------------------------------------//
//////////////////  C_DBCONNECTION_LIST  FINE    //////////////////////////
//////////////////  C_PREPARED_CMD       INIZIO    ////////////////////////
//-----------------------------------------------------------------------//


C_PREPARED_CMD::C_PREPARED_CMD() : C_INT() {}
C_PREPARED_CMD::C_PREPARED_CMD(_ConnectionPtr &pConn, C_STRING &statement) : C_INT()
   { gsc_PrepareCmd(pConn, statement, pCmd); }
C_PREPARED_CMD::C_PREPARED_CMD(_CommandPtr &_pCmd) : C_INT()
   { pCmd = _pCmd; }
C_PREPARED_CMD::C_PREPARED_CMD(_RecordsetPtr &_pRs) : C_INT()
   { pRs = _pRs; }
C_PREPARED_CMD::~C_PREPARED_CMD()
{
   Terminate();
}
void C_PREPARED_CMD::Terminate(void)
{
   if (pCmd.GetInterfacePtr()) pCmd.Release();
   gsc_DBCloseRs(pRs);
}


//-----------------------------------------------------------------------//
//////////////////  C_PREPARED_CMD       FINE      ////////////////////////
//////////////////  C_PREPARED_CMD_LIST  INIZIO    ////////////////////////
//-----------------------------------------------------------------------//


C_PREPARED_CMD_LIST::C_PREPARED_CMD_LIST() : C_INT_LIST() {}
C_PREPARED_CMD_LIST::~C_PREPARED_CMD_LIST() {}


//-----------------------------------------------------------------------//
//////////////////  C_PREPARED_CMD_LIST  FINE      ////////////////////////
// FUNZIONI PER SELEZIONE VALORE DA LISTA VALORI UNIVOCI VIA DCL  INIZIO //
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc gsc_dd_sel_uniqueVal <external> */
/*+                                                                       
  Permette la selezione di un valore di un attributo da una lista di
  valori esistenti nella tabella.
  Parametri:
  C_DBCONNECTION *pConn;   Connessione OLE-DB
  C_STRING &TableRef;      Riferimento tabella
  C_STRING &AttrName;      Nome attributo
  C_STRING &result;

  oppure:
  C_STRING &Title;
  _RecordsetPtr &pRs;
  C_STRING &result

  Restituisce NULL in caso di successo oppure una stringa con messaggio 
  di errore. 
-*/  
/*********************************************************/
int gsc_dd_sel_uniqueVal(int prj, int cls, int sub, int sec, C_STRING &AttribName,
                         C_STRING &result, C_STRING &Title)
{
   C_CLASS        *pCls;
   C_SECONDARY    *pSec;
   C_ATTRIB_LIST  *pAttriblist;
   C_ATTRIB       *pAttrib;
   C_DBCONNECTION *pConn = NULL;
   C_STRING       statement, TableRef, AdjAttribName, AttribDescr, SupportFile;
   ValuesListTypeEnum SupportFileType;
   _RecordsetPtr  pRs;
   TCHAR          *msg = NULL;
   C_RB_LIST      ColValues;
   presbuf        p;
   int            res = GS_GOOD, n_col = 1;
   C_2STR_LIST    StrValList;
   C_2STR         *pDescrFrom_TAB_REF;
   C_CACHE_CLS_ATTRIB_VALUES_LIST *pCacheClsAttribValuesList;
   bool                           Release_pCacheClsAttribValuesList = false;

   if (!(pCls = gsc_find_class(prj, cls, sub))) return GS_BAD;
   if (sec > 0)
   {
      if (!(pSec = (C_SECONDARY *) pCls->find_sec(sec))) return GS_BAD;
      pAttriblist = pSec->ptr_attrib_list();
      if (pSec->ptr_info()) pConn = pSec->ptr_info()->getDBConnection(OLD);
      TableRef = pSec->ptr_info()->OldTableRef;
   }
   else
   {
      pAttriblist = pCls->ptr_attrib_list();
      if (pCls->ptr_info()) pConn = pCls->ptr_info()->getDBConnection(OLD);
      TableRef = pCls->ptr_info()->OldTableRef;
   }

   if (!pAttriblist || !pConn) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
   if (!(pAttrib = (C_ATTRIB *) pAttriblist->search_name(AttribName.get_name(), FALSE)))
      { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }

   // Cerco il file di supporto
   if (gsc_FindSupportFiles(AttribName.get_name(), prj, cls, sub, sec,
                            SupportFile, &SupportFileType) == GS_GOOD)
      // Lista a 2 colonne
      if (SupportFileType == TAB || SupportFileType == REF)
      {
         n_col = 2;

         if (GS_CURRENT_WRK_SESSION)
            pCacheClsAttribValuesList = GS_CURRENT_WRK_SESSION->get_pCacheClsAttribValuesList();
         else
         {
            pCacheClsAttribValuesList         = new C_CACHE_CLS_ATTRIB_VALUES_LIST();
            Release_pCacheClsAttribValuesList = true;
         }

         if (pCacheClsAttribValuesList->has_parameters(AttribName, prj, cls, sub, sec)) // roby 2018
            n_col = 1;
      }

   AdjAttribName = AttribName;
   if (gsc_AdjSyntax(AdjAttribName, pConn->get_InitQuotedIdentifier(), pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD)
   {
      if (Release_pCacheClsAttribValuesList) delete pCacheClsAttribValuesList; 
      return GS_BAD;
   }

   statement = _T("SELECT DISTINCT ");
   statement += AdjAttribName;
   statement += _T(" FROM ");
   statement += TableRef;
   statement += _T(" ORDER BY ");
   statement += AdjAttribName;

   if (pConn->OpenRecSet(statement, pRs) == GS_BAD)
   {
      if (Release_pCacheClsAttribValuesList) delete pCacheClsAttribValuesList; 
      return GS_BAD;
   }

   if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD)
   {
      if (Release_pCacheClsAttribValuesList) delete pCacheClsAttribValuesList; 
      return GS_BAD;
   }

   p = gsc_nth(1, ColValues.nth(0));

   while (gsc_isEOF(pRs) == GS_BAD)
   {
      // leggo riga
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { res = GS_BAD; break; }
      if (p->restype != RTNONE && p->restype != RTVOID && p->restype != RTNIL)
      {
         if ((msg = gsc_rb2str(p)) == NULL) { res = GS_BAD; break; }

         if (n_col == 2) // cerco la descrizione
         {
            pDescrFrom_TAB_REF = pCacheClsAttribValuesList->get_Descr(AttribName,
                                                                      ColValues,
                                                                      prj, cls, sub, sec);
            if (pDescrFrom_TAB_REF) AttribDescr = pDescrFrom_TAB_REF->get_name2();
            else AttribDescr.clear();
         }

	      if (StrValList.add_tail_2str(msg, AttribDescr.get_name()) == GS_BAD)
            { free(msg); res = GS_BAD; break; }
         free(msg);
      }
      gsc_Skip(pRs);
   }
   gsc_DBCloseRs(pRs);

   if (Release_pCacheClsAttribValuesList) delete pCacheClsAttribValuesList; 

   if (res == GS_BAD) return GS_BAD;
   
   return gsc_dd_sel_uniqueVal(Title, StrValList, n_col, result);
}
int gsc_dd_sel_uniqueVal(C_DBCONNECTION *pConn, C_STRING &TableRef, C_STRING &AttribName,
                         C_STRING &result, C_STRING &Title)
{
   C_STRING      statement, AdjAttribName;
   _RecordsetPtr pRs;

   AdjAttribName = AttribName;
   if (gsc_AdjSyntax(AdjAttribName, pConn->get_InitQuotedIdentifier(), pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;

   statement = _T("SELECT DISTINCT ");
   statement += AdjAttribName;
   statement += _T(" FROM ");
   statement += TableRef;
   statement += _T(" ORDER BY ");
   statement += AdjAttribName;

   if (pConn->OpenRecSet(statement, pRs) == GS_BAD) return GS_BAD;
 
   return gsc_dd_sel_uniqueVal(Title, pRs, result);
}

int dcl_AttrUniqueValues_Redraw_ValuesList(ads_hdlg dcl_id, C_2STR_LIST *pStrValList, int n_col)
{
   C_2STR   *pStrVal;
   int      i = 0, n1, n2, PrevPos;
   TCHAR    offset[10];
   C_STRING Buffer;

   // posizione della riga corrente
   ads_get_tile(dcl_id, _T("ValuesList"), offset, 9);
   PrevPos = _wtoi(offset);
   
   // offset di spostamento dello slider1
   ads_get_tile(dcl_id, _T("horiz_slider1"), offset, 9);
   n1 = _wtoi(offset);
   if (n_col == 2)
   {  // offset di spostamento dello slider2
      ads_get_tile(dcl_id, _T("horiz_slider2"), offset, 9);
      n2 = _wtoi(offset);
   }

   ads_start_list(dcl_id, _T("ValuesList"), LIST_NEW, 0);
   pStrVal = (C_2STR *) pStrValList->get_head();
   while (pStrVal)
   {
      if (wcslen(pStrVal->get_name()) > (unsigned int) n1) Buffer = pStrVal->get_name() + n1;
      else Buffer = GS_EMPTYSTR;

      if (n_col == 2)
      {
         Buffer += _T("\t");
         if (wcslen(pStrVal->get_name2()) > (unsigned int) n2) Buffer += pStrVal->get_name2() + n2;
      }
      gsc_add_list(Buffer.get_name());
      pStrVal = (C_2STR *) pStrValList->get_next();
   }
   ads_end_list();

   // riposiziono la riga che era corrente
   ads_set_tile(dcl_id, _T("ValuesList"), _itow(PrevPos, offset, 10));
   
   return GS_GOOD;
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "AttrUniqueValues" su controllo "ValuesLis"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_AttrUniqueValues_ValuesList(ads_callback_packet *dcl)
{
   if (dcl->reason == CBR_DOUBLE_CLICK)
   {
      ((C_LIST *)(dcl->client_data))->getptr_at(_wtoi(dcl->value) + 1);
      ads_done_dialog(dcl->dialog, DLGOK);
   }
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "AttrUniqueValues" su controllo "horiz_slider"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_AttrUniqueValues_horiz_slider1(ads_callback_packet *dcl)
{
   dcl_AttrUniqueValues_Redraw_ValuesList(dcl->dialog, (C_2STR_LIST *) (dcl->client_data), 1);
} 
static void CALLB dcl_AttrUniqueValues_horiz_slider2(ads_callback_packet *dcl)
{
   dcl_AttrUniqueValues_Redraw_ValuesList(dcl->dialog, (C_2STR_LIST *) (dcl->client_data), 2);
} 

///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "AttrUniqueValues" su controllo "accept"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_AttrUniqueValues_accept(ads_callback_packet *dcl)
{
   TCHAR val[50];

   if (ads_get_tile(dcl->dialog, _T("ValuesList"), val, 50-1) == RTNORM)
   {
      if (wcslen(val) > 0)
      {
         ((C_LIST *)(dcl->client_data))->getptr_at(_wtoi(val) + 1);
         ads_done_dialog(dcl->dialog, DLGOK);
      }
   }
}
int gsc_dd_sel_uniqueVal(C_STRING &Title, _RecordsetPtr &pRs, C_STRING &result)
{
   TCHAR       *msg = NULL;
   C_RB_LIST   ColValues;
   presbuf     p;
   int         res = GS_GOOD;
   C_2STR_LIST StrValList;

   result.clear();

   if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD) return GS_BAD;
   p = gsc_nth(1, ColValues.nth(0));

   while (gsc_isEOF(pRs) == GS_BAD)
   {
      // leggo riga
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { res = GS_BAD; break; }
      if (p->restype != RTNONE && p->restype != RTVOID && p->restype != RTNIL)
      {
         if ((msg = gsc_rb2str(p)) == NULL) { res = GS_BAD; break; }
	      if (StrValList.add_tail_2str(msg, NULL) == GS_BAD) { free(msg); res = GS_BAD; break; }
         free(msg);
      }
      gsc_Skip(pRs);
   }
   if (res == GS_BAD) return GS_BAD;

   return gsc_dd_sel_uniqueVal(Title, StrValList, 1, result); // 1 colonna
}
int gsc_dd_sel_uniqueVal(C_STRING &Title, C_2STR_LIST &StrValList, int n_col, C_STRING &result)
{
   C_STRING   path;
   ads_hdlg   dcl_id;
   int        res = GS_GOOD, status, dcl_file;
   C_STRING   Buffer;
   C_2STR     *StrVal;

   result.clear();

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_GRAPH.DCL");
   if (gsc_load_dialog(path, &dcl_file) == RTERROR) return GS_BAD;

   if (n_col == 1)
      ads_new_dialog(_T("AttrUniqueValues"), dcl_file, (CLIENTFUNC)NULLCB, &dcl_id);
   else if (n_col == 2)
      ads_new_dialog(_T("AttrUniqueValues2Col"), dcl_file, (CLIENTFUNC)NULLCB, &dcl_id);
   else
   {
      ads_unload_dialog(dcl_file);
      GS_ERR_COD = eGSAbortDCL; 
      return GS_BAD;
   }

   if (!dcl_id)
   {
      ads_unload_dialog(dcl_file);
      GS_ERR_COD = eGSAbortDCL; 
      return GS_BAD;
   }

   res = GS_GOOD;
   ads_start_list(dcl_id, _T("ValuesList"), LIST_NEW, 0);
   StrVal = (C_2STR *) StrValList.get_head();
   while (StrVal)
   {
      Buffer = StrVal->get_name();
      if (n_col == 2)
      {
         Buffer += _T("\t");
         Buffer += StrVal->get_name2();
      }
      gsc_add_list(Buffer.get_name());
      StrVal = (C_2STR *) StrValList.get_next();
   }
   ads_end_list();

   StrValList.get_head(); // posiziono il cursore sul primo elemento
   ads_set_tile(dcl_id, _T("title"), Title.get_name());
   ads_set_tile(dcl_id, _T("ValuesList"), _T("0"));
   ads_mode_tile(dcl_id, _T("ValuesList"), MODE_SETFOCUS); 
   ads_action_tile(dcl_id, _T("ValuesList"), (CLIENTFUNC) dcl_AttrUniqueValues_ValuesList);
   ads_client_data_tile(dcl_id, _T("ValuesList"), &StrValList);
   if (n_col == 1)
   {
      ads_action_tile(dcl_id, _T("horiz_slider1"), dcl_AttrUniqueValues_horiz_slider1); // slider orizzontale
      ads_client_data_tile(dcl_id, _T("horiz_slider1"), &StrValList);
   }
   else
   {
      ads_action_tile(dcl_id, _T("horiz_slider1"), dcl_AttrUniqueValues_horiz_slider2); // slider orizzontale
      ads_client_data_tile(dcl_id, _T("horiz_slider1"), &StrValList);
      ads_action_tile(dcl_id, _T("horiz_slider2"), dcl_AttrUniqueValues_horiz_slider2); // slider orizzontale
      ads_client_data_tile(dcl_id, _T("horiz_slider2"), &StrValList);
   }
   ads_action_tile(dcl_id, _T("accept"), (CLIENTFUNC) dcl_AttrUniqueValues_accept);
   ads_client_data_tile(dcl_id, _T("accept"), &StrValList);

   ads_start_dialog(dcl_id, &status);
   ads_unload_dialog(dcl_file);

   if (status == DLGCANCEL) res = GS_CAN;
   else result.set_name(StrValList.get_cursor()->get_name());

   ads_unload_dialog(dcl_file);

   return res;
}


//-----------------------------------------------------------------------//
// FUNZIONI PER SELEZIONE VALORE DA LISTA VALORI UNIVOCI VIA DCL   FINE  //
//-----------------------------------------------------------------------//



/*
   for (int i = 0; i < pConn->Properties->Count; i++)
      acutPrintf("\n%s=%s", (TCHAR*)(pConn->Properties->GetItem((long)i)->GetName()),
                            (TCHAR*)_bstr_t(pConn->Properties->GetItem((long)i)->GetValue()));

         switch (pType->resval.rlong)
         {
            case adEmpty:
            case adTinyInt:
            case adSmallInt:
            case adInteger:
            case adBigInt:
            case adUnsignedTinyInt:
            case adUnsignedSmallInt:
            case adUnsignedInt:
            case adUnsignedBigInt:
            case adSingle:
            case adDouble:
            case adCurrency:
            case adDecimal:
            case adNumeric:
            case adBoolean:
            case adError:
            case adUserDefined:
            case adVariant:
            case adIDispatch:
            case adIUnknown:
            case adGUID:
            case adDate:
            case adDBDate:
            case adDBTime:
            case adDBTimeStamp:
            case adBSTR:
            case adChar:
            case adVarChar:
            case adLongVarChar:
            case adWChar:
            case adVarWChar:
            case adLongVarWChar:
            case adBinary:
            case adVarBinary:
            case adLongVarBinary:
            case adChapter:
            case adFileTime:
            case adDBFileTime:
            case adPropVariant:
            case adVarNumeric:

*/




