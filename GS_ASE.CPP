/**********************************************************
Name: GS_ASE

Module description: File contenente le funzioni per la gestione dei
                    Link tra oggetti grafici e righe di tabelle di DB.
            
Author: Poltini Roberto

(c) Copyright 1996-2014 by IREN ACQUA GAS  S.p.A.

Entry points: 

Modification history:
              
Notes and restrictions on use: 


**********************************************************/

/*********************************************************/
/* INCLUDES */
/*********************************************************/


#include "stdafx.h"         // MFC core and standard components

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")


#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>
#include <string.h>      /*  per strcat() strcmp()  */
#include <math.h>        /*  per sin, cos, atan2  */

#include "adslib.h"   

#include "..\gs_def.h" // definizioni globali
#include "gs_error.h"     // codici errori
#include "gs_resbf.h"
#include "gs_utily.h"  
#include "gs_list.h"      // gestione liste C++ 
#include "gs_init.h" 
#include "gs_dbref.h"
#include "gs_ase.h"       // gestione links
#include "gs_class.h"     // gestione classi
#include "gs_prjct.h"     // gestione progetti
#include "gs_graph.h"
#include "gs_area.h"      // gestione links
#include "gs_attbl.h"     // gestione blocchi DA
#include "gs_query.h"

#define BUFLEN           128           /* The local buffer length */


#if defined(GSDEBUG) // se versione per debugging
   #include <sys/timeb.h>
   #include <time.h>
#endif


/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/

#if defined(GSDEBUG) // se versione per debugging
   #include <sys/timeb.h>  // Solo per debug
   #include <time.h>       // Solo per debug
   double  ASEtempo1=0, ASEtempo2=0, ASEtempo3=0, ASEtempo4=0, ASEtempo5=0, ASEtempo6=0, ASEtempo7=0;
   double  ASEtempo8=0, ASEtempo9=0, ASEtempo10=0, ASEtempo11=0, ASEtempo12=0, ASEtempo13=0, ASEtempo14=0;
#endif


/*********************************************************/
/* PRIVATE FUNCTIONS */
/*********************************************************/


int gsc_SetASEKey(ads_name ent, C_STRING &ASELPName, long Key);
int gsc_SetASEKey(ads_name ent, C_STRING &ASELPName, CAsiRow &pKeyCols, long Key);


/*********************************************************/
/* FUNCTIONS */
/*********************************************************/


/*********************************************************/
/*.doc gsc_Table2MAPFormat                    <internal> */
/*+                                                                       
  Trasforma il riferimento di una tabella nel formato di MAP.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_Table2MAPFormat(C_DBCONNECTION *pConn, C_STRING &TableRef, C_STRING &MapTableRef)
{
   C_STRING Catalog, Schema, Table;

   MapTableRef.clear();
   if (pConn->split_FullRefTable(TableRef, Catalog, Schema, Table) == GS_BAD)
      return GS_BAD;
   if (Catalog.len() > 0)
   {
      MapTableRef = _T('"');
      MapTableRef += Catalog;
      MapTableRef += _T("\".");
   }
   if (Schema.len() > 0)
   {
      MapTableRef += _T('"');
      MapTableRef += Schema;
      MapTableRef += _T("\".");
   }
   if (Table.len() > 0)
   {
      MapTableRef += _T('"');
      MapTableRef += Table;
      MapTableRef += _T('"');
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_AdjSyntaxMAPFormat                 <internal> */
/*+                                                                       
  Trasforma il nome di un campo di una tabella nel formato di MAP.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
void gsc_AdjSyntaxMAPFormat(C_STRING &FieldName)
{
   C_STRING MapFieldName;

   // SQL2 Grammar
   static TCHAR *SqlReservedWord[] = 
{
_T("ABSOLUTE"), _T("ACTION"), _T("ADD"), _T("ALL"), _T("ALLOCATE"), _T("ALTER"),
_T("AND"), _T("ANY"), _T("ARE"), _T("AS"), _T("ASC"), _T("ASSERTION"), _T("AT"),
_T("AUTHORIZATION"), _T("AVG"),
_T("BEGIN"), _T("BETWEEN"), _T("BIT"), _T("BIT_LENGTH"), _T("BOTH"), _T("BY"),
_T("CASCADE"), _T("CASE"), _T("CAST"), _T("CATALOG"), _T("CHAR"), _T("CHARACTER"),
_T("CHAR_LENGTH"), _T("CHARACTER_LENGTH"), _T("CHECK"), _T("CLOSE"), _T("COALESCE"),
_T("COLLATE"), _T("COLLATION"), _T("COLUMN"), _T("COMMIT"), _T("CONNECT"), _T("CONNECTION"),
_T("CONSTRAINT"), _T("CONSTRAINTS"), _T("CONTINUE"), _T("CONVERT"), _T("CORRESPONDING"),
_T("COUNT"), _T("CREATE"), _T("CROSS"), _T("CURRENT_DATE"), _T("CURRENT_TIME"),
_T("CURRENT_TIMESTAMP"), _T("CURRENT_USER"), _T("CURSOR"),
_T("DATE"), _T("DAY"), _T("DEALLOCATE"), _T("DEC"), _T("DECIMAL"), _T("DECLARE"),
_T("DEFAULT"), _T("DEFERABLE"), _T("DEFERRED"), _T("DELETE"), _T("DESC"), _T("DESCRIBE"),
_T("DESCRIPTOR"), _T("DIAGNOSTICS"), _T("DISCONNECT"), _T("DISTINCT"), _T("DOMAIN"),
_T("DOUBLE"), _T("DROP"),
_T("ELSE"), _T("END"), _T("END-EXEC"), _T("ESCAPE"), _T("EXEPT"),
_T("EXCEPTION"), _T("EXEC"), _T("EXECUTE"), _T("EXISTS"), _T("EXTERNAL"), _T("EXTRACT"),
_T("FALSE"), _T("FETCH"), _T("FIRST"), _T("FLOAT"), _T("FOR"), _T("FOREIGN"),
_T("FOUND"), _T("FROM"), _T("FULL"),
_T("GET"), _T("GLOBAL"), _T("GO"), _T("GOTO"), _T("GRANT"), _T("GROUP"),
_T("HAVING"), _T("HOUR"),
_T("IDENTITY"), _T("IMMEDIATE"), _T("IN"), _T("INDICATOR"), _T("INITIALLY"), _T("INNER"),
_T("INPUT"), _T("INSENSITIVE"), _T("INSERT"), _T("INT"), _T("INTEGER"), _T("INTERSECT"),
_T("INTERVAL"), _T("INTO"), _T("IS"), _T("ISOLATION"),
_T("JOIN"),
_T("KEY"),
_T("LANGUAGE"), _T("LAST"), _T("LEADING"), _T("LEFT"), _T("LEVEL"), _T("LIKE"), 
_T("LOCAL"), _T("LOWER"),
_T("MATCH"), _T("MAX"), _T("MIN"), _T("MINUTE"), _T("MODULE"), _T("MONTH"),
_T("NAMES"), _T("NATIONAL"), _T("NATURAL"), _T("NCHAR"), _T("NEXT"), _T("NO"), _T("NOT"),
_T("NULL"), _T("NULLIF"), _T("NUMERIC"),
_T("OCTET_LENGTH"), _T("OF"), _T("ON"), _T("ONLY"), _T("OPEN"), _T("OPTION"), _T("OR"),
_T("ORDER"), _T("OUTER"), _T("OUTPUT"), _T("OVERLAPS"),
_T("PARTIAL"), _T("POSITION"), _T("PRECISION"), _T("PREPARE"), _T("PRESERVE"),
_T("PRIMARY"), _T("PRIOR"), _T("PRIVILEGES"), _T("PROCEDURE"), _T("PUBLIC"),
_T("READ"), _T("REAL"), _T("REFERENCES"), _T("RELATIVE"), _T("RESTRICT"),
_T("REVOKE"), _T("RIGHT"), _T("ROLLBACK"), _T("ROWS"),
_T("SCHEMA"), _T("SCROLL"), _T("SECOND"), _T("SECTION"), _T("SELECT"), _T("SESSION"),
_T("SESSION_USER"), _T("SET"), _T("SIZE"), _T("SMALLINT"), _T("SOME"), _T("SQL"),
_T("SQLCODE"), _T("SQLERROR"), _T("SQLSTATE"), _T("SUBSTRING"), _T("SUM"), _T("SYSTEM_USER"),
_T("TABLE"), _T("TEMPORARY"), _T("THEN"),  _T("TIME"), _T("TIMESTAMP"), _T("TIMEZONE_HOUR"),
_T("TIMEZONE_MINUTE"), _T("TO"), _T("TRAILING"), _T("TRANSACTION"), _T("TRANSLATE"),
_T("TRANSLATION"), _T("TRIM"), _T("TRUE"),
_T("UNION"), _T("UNIQUE"), _T("UNKNOWN"), _T("UPDATE"), _T("UPPER"), _T("USAGE"),
_T("USER"),  _T("USING"),
_T("VALUE"), _T("VALUES"), _T("VARCHAR"), _T("VARYING"), _T("VIEW"),
_T("WHEN"), _T("WHENEVER"), _T("WHERE"), _T("WITH"), _T("WORK"), _T("WRITE"),
_T("YEAR"),
_T("ZONE")
};

   // se contiene uno spazio o un apice
   if (FieldName.at(_T(" ")) || FieldName.at(_T("'")))
   {
      MapFieldName = _T('"');
      MapFieldName += FieldName;
      MapFieldName += _T('"');
      
      FieldName = MapFieldName;

      return;
   }

   for (int i = 0; i < ELEMENTS(SqlReservedWord); i++)
      // se uguale ad una parola riservata SQL
      if (FieldName.comp(SqlReservedWord[i], GS_BAD) == 0)
      {
         MapFieldName = _T('"');
         MapFieldName += FieldName;
         MapFieldName += _T('"');

         FieldName = MapFieldName;

         return;
      }

   return;
}


/*********************************************************/
/*.doc gsc_AdjSyntaxDateToTimestampMAPFormat  <internal> */
/*+                                                                       
  Purtroppo se la condizione di fltro ricade su campi di tipo date
  la compilazione x ACCESS non funziona perchè access non ha il tipo date
  ma solo timestamp (CAMPO = DATE '2013-03-06' non andrebbe).
  Allora dove trovo DATE '...' lo sostituisco con TIMESTAMP '... 00:00:00'
  Parametri:
  C_STRING &statement;  Istruzione SQL

  Restituisce la stringa corretta.
-*/  
/*********************************************************/
C_STRING gsc_AdjSyntaxDateToTimestampMAPFormat(C_STRING &statement)
{
   C_STRING CorrectedStm, dummy(statement), DateStm;
   int      pos;
   size_t   test_len = gsc_strlen(_T("DATE 'yyyy-mm-dd'")); // lunghezza di test

   while ((pos = dummy.find(_T("DATE '"))) > 0) // 1-indexed
   {
      CorrectedStm += dummy.left(pos - 1);
      DateStm = dummy.mid(pos - 1, test_len); // 0-indexed
      // se la lunghezza è giusta ha i caratteri "-" tra anno e mese e giorno e finisce per '
      if (DateStm.len() == test_len &&
          DateStm.mid(10, 1).comp(_T("-")) == 0 &&
          DateStm.mid(13, 1).comp(_T("-")) == 0 &&
          DateStm.right(1).comp(_T("'")) == 0)
      {
         DateStm.set_chr(_T(' '), DateStm.len() - 1);
         DateStm += _T("00:00:00'");
         DateStm.strtran(_T("DATE"), _T("TIMESTAMP"));                          
         dummy = dummy.right(dummy.len() - (pos + test_len) + 1);
      }
      else
         dummy = dummy.right(dummy.len() - DateStm.len());

      CorrectedStm += DateStm;
   }
   CorrectedStm += dummy;


   test_len = gsc_strlen(_T("#yyyy-mm-dd#")); // lunghezza di test
   dummy = CorrectedStm;
   CorrectedStm.clear();

   while ((pos = dummy.find(_T("#"))) > 0) // 1-indexed
   {
      CorrectedStm += dummy.left(pos - 1);
      DateStm = dummy.mid(pos - 1, test_len); // 0-indexed
      // se la lunghezza è giusta ha i caratteri "-" tra anno e mese e giorno e finisce per #
      if (DateStm.len() == test_len &&
          DateStm.mid(5, 1).comp(_T("-")) == 0 &&
          DateStm.mid(8, 1).comp(_T("-")) == 0 &&
          DateStm.right(1).comp(_T("#")) == 0)
      {
         DateStm.set_chr(_T('\''), 0);
         DateStm.set_chr(_T(' '), DateStm.len() - 1);
         DateStm += _T("00:00:00'");
         DateStm.addPrefixSuffix(_T("TIMESTAMP "));
         dummy = dummy.right(dummy.len() - (pos + test_len) + 1);
      }
      else
         dummy = dummy.right(dummy.len() - DateStm.len());

      CorrectedStm += DateStm;
   }
   CorrectedStm += dummy;

   return CorrectedStm;
}


/*********************************************************/
/*.doc gsc_AdjSyntaxTimeToTimestampMAPFormat  <internal> */
/*+                                                                       
  Purtroppo se la condizione di fltro ricade su campi di tipo time
  la compilazione x ACCESS non funziona perchè access non ha il tipo time
  ma solo timestamp (CAMPO = TIME '2013-03-06' non andrebbe).
  Allora dove trovo TIME '...' lo sostituisco con TIMESTAMP '1899-12-30 ...'
  Parametri:
  C_STRING &statement;  Istruzione SQL

  Restituisce la stringa corretta.
-*/  
/*********************************************************/
C_STRING gsc_AdjSyntaxTimeToTimestampMAPFormat(C_STRING &statement)
{
   C_STRING CorrectedStm, dummy(statement), TimeStm;
   int      pos;
   size_t   test_len = gsc_strlen(_T("TIME 'hh:mm:ss'")); // lunghezza di test

   while ((pos = dummy.find(_T("TIME '"))) > 0) // 1-indexed
   {
      CorrectedStm += dummy.left(pos - 1);
      TimeStm = dummy.mid(pos - 1, test_len); // 0-indexed
      // se la lunghezza è giusta ha i caratteri ":" tra anno e mese e giorno e finisce per '
      if (TimeStm.len() == test_len &&
          TimeStm.mid(8, 1).comp(_T(":")) == 0 &&
          TimeStm.mid(11, 1).comp(_T(":")) == 0 &&
          TimeStm.right(1).comp(_T("'")) == 0)
      {
         TimeStm.strtran(_T("TIME '"), _T("TIMESTAMP '1899-12-30 "));                          
         dummy = dummy.right(dummy.len() - (pos + test_len) + 1);
      }
      else
         dummy = dummy.right(dummy.len() - TimeStm.len());

      CorrectedStm += TimeStm;
   }
   CorrectedStm += dummy;

   return CorrectedStm;
}


///////////////////////////////////////////////////////////////////////////////
//  INIZIO  FUNZIONI  ASI
///////////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc gsc_initasi                            <external> /*
/*+
  Questa funzione inizializza l'ambiente ASI. 
  Parametri:
  CAsiAppl *ahandle;   puntatore all'ambiente ASE.
  
  Restituisce il puntatore all'ambiente ASE in caso di successo 
  altrimenti NULL.
-*/  
/*********************************************************/
CAsiAppl *gsc_initasi(void)
{
   CAsiAppl *pAppl;

   // Inizializzo interfaccia ASE
	if ((pAppl = new CAsiAppl()) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
	if (pAppl->Init() != kAsiGood)
   {
      gsc_printASIErr(pAppl);
      delete pAppl;
      GS_ERR_COD = eGSInitASI;
      return NULL;
   }

   return pAppl;
}


/*********************************************************/
/*.doc gsc_termasi                            <external> /*
/*+
  Questa funzione termina l'ambiente ASI e va chiamata dopo
  tutte le altre funzioni per la gestione dei link. 
  Parametri:
  CAsiAppl **pAppl;   puntatore all'ambiente ASE.
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce 
  GS_BAD.
-*/  
/*********************************************************/
int gsc_termasi(CAsiAppl **pAppl)
{
   try
   {
	   if (*pAppl)
      {
         (*pAppl)->Term();
         delete *pAppl;
         *pAppl = NULL;
      }
   }
   catch (...)
   {
      return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_ASITermSession                     <external> /*
/*+
  Questa funzione termina la sessione ASI.
  Parametri:
  CAsiSession **pSession;
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_ASITermSession(CAsiSession **pSession)
{
   try
   {
	   if (*pSession)
      {
         (*pSession)->Disconnect();
         delete *pSession;
         *pSession = NULL;
      }
   }
   catch (...)
   {
      return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_ASITermStm                         <external> /*
/*+
  Questa funzione termina l'istruzione ASI.
  Parametri:
  CAsiExecStm **pStm;
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_ASITermStm(CAsiExecStm **pStm)
{
   try
   {
	   if (*pStm)
      {
         (*pStm)->Deallocate();
         delete *pStm;
         *pStm = NULL;
      }
   }
   catch (...)
   {
      return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_ASICreateSession                   <external> /*
/*+
  Crea una sessione ASI.
  Parametri:
  const TCHAR *UDLName;        Nome file UDL
  
  Restituisce il puntatore ad una CAsiSession in caso di successo oppure NULL
  in caso di errore. 
-*/  
/*********************************************************/
CAsiSession *gsc_ASICreateSession(const TCHAR *UDLName)
{
   CAsiSession *pSession;
   CAsiUcStr   UcStr;
   CAsiUcStr   UcStrLog((TCHAR *) GEOsimAppl::GS_USER.log);
   CAsiUcStr   UcStrPwd((TCHAR *) GEOsimAppl::GS_USER.pwd);
   int         TryAgain = FALSE;

	if ((pSession = new CAsiSession(GEOsimAppl::ASI)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }

   UcStr = GEOsimAppl::ASE->getAcadDsPath(); // dir dei data source file
   UcStr.Append("\\");
   UcStr.Append(UDLName);
   UcStr.Append(".UDL");

   try // ASI_TRY
   {
      CAsiUcStr IniString;

      C_STRING  MyUDLFile(GEOsimAppl::ASE->getAcadDsPath()), ConnStr;
      MyUDLFile += _T("\\");
      MyUDLFile += UDLName;
      MyUDLFile += _T(".UDL");

      ConnStr.paste(gsc_LoadOleDBIniStrFromFile(MyUDLFile.get_name()));
      IniString = ConnStr.get_name();

      // uso la login e password evenrtualmente indicati in IniString
      if (pSession->ConnectIniString(IniString) != kAsiGood)
      {
         gsc_printASIErr(pSession);
         GS_ERR_COD = eGSInitASI;
         return NULL;
      }
   }
   catch (...) // ASI_CATCH(e)
   { TryAgain = TRUE; }
   // ASI_END_CATCH

   if (TryAgain)
   {
      try // ASI_TRY
      {
         if (pSession->Connect(UcStr, UcStrLog, UcStrPwd) != kAsiGood)
         {
            gsc_printASIErr(pSession);
            GS_ERR_COD = eGSInitASI;
            return NULL;
         }
      }
      catch (...) // ASI_CATCH(e)
      {
         gsc_printASIErr(pSession);
         GS_ERR_COD = eGSInitASI;
         return NULL;
      }
      // ASI_END_CATCH
   }
  
   return pSession;
}


/*********************************************************/
/*.doc gsc_ASIPrepareSql                      <external> /*
/*+
  Prepara una istruzione ASI creando anche il cursore e collegando fino
  a 3 eventuali parametri numerici interi.
  Parametri:
  CAsiSession *pSession;   input
  const TCHAR *statement;  input
  const TCHAR *CsrName;    input
  CAsiExecStm **pExecStm;  output (viene allocata dalla funzione)
  CAsiCsr     **pCsr;      output (viene allocato dalla funzione)
  CAsiData **pParam1;      (viene fatto il bind dalla funzione);
                           se usato deve già essere allocato (default = NULL)
  CAsiData **pParam2;      (viene fatto il bind dalla funzione);
                           se usato deve già essere allocato (default = NULL)
  CAsiData **pParam3;      (viene fatto il bind dalla funzione);
                           se usato deve già essere allocato (default = NULL)
  int PrintError;          Se il flag = GS_GOOD in caso di errore viene
                           stampato il messaggio relativo altrimenti non
                           viene stampato alcun messaggio (default = GS_GOOD)
  
  Restituisce GS_GOOD in caso di successo oppure GS_BAD in caso di errore. 
  N.B. per un baco di map il parametro (se esistente) deve essere 
       obbligatoriamente CAsiInt
-*/  
/*********************************************************/
int gsc_ASIPrepareSql(CAsiSession *pSession, const TCHAR *statement, const TCHAR *CsrName,
                      CAsiExecStm **pExecStm, CAsiCsr **pCsr,
                      CAsiData **pParam1, CAsiData **pParam2, CAsiData **pParam3,
                      int PrintError)
{
   CAsiIdent name(CsrName);
   CAsiUcStr UcStr(statement);

   if ((*pExecStm = new CAsiExecStm) == NULL) 
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   if ((*pExecStm)->Prepare(pSession, UcStr) != kAsiGood)
   {
      if (PrintError == GS_GOOD) gsc_printASIErr(*pExecStm);
      gsc_ASITermStm(pExecStm);
      GS_ERR_COD = eGSPrepareStm;
      return GS_BAD;
   }

   if ((*pExecStm)->ParamQty() > 3) // si accetta al max 3 parametri
   {
      gsc_ASITermStm(pExecStm);
      GS_ERR_COD = eGSInitPar;
      return GS_BAD;
   }

   if ((*pCsr = new CAsiCsr) == NULL) 
      { gsc_ASITermStm(pExecStm); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   if ((*pCsr)->Allocate(*pExecStm, name) != kAsiGood)
      { gsc_ASITermStm(pExecStm); delete *pCsr; return GS_BAD; }

   if ((*pExecStm)->ParamQty() > 0 && pParam1)
   {
      *pParam1 = new CAsiChar(256); // con il parametro carattere sembra funzionare bene
      ((CAsiChar *) *pParam1)->setVarying(); // se si tratta di postgresql è necessario impostare setvaring
                                             // altrimenti non funziona

      if ((*pCsr)->Bind(0, *pParam1) != kAsiGood)
      {
         gsc_ASITermStm(pExecStm);
         delete *pCsr;
         delete *pParam1;
         GS_ERR_COD = eGSInitPar;
         return GS_BAD;
      }

      if ((*pExecStm)->ParamQty() > 1 && pParam2)
      {
         *pParam2 = new CAsiChar(256);
         ((CAsiChar *) *pParam2)->setVarying(); // se si tratta di postgresql è necessario impostare setvaring
                                                // altrimenti non funziona

         if ((*pCsr)->Bind(0, *pParam2) != kAsiGood)
         {
            gsc_ASITermStm(pExecStm);
            delete *pCsr;
            delete *pParam2;
            GS_ERR_COD = eGSInitPar;
            return GS_BAD;
         }

         if ((*pExecStm)->ParamQty() > 2 && pParam3)
         {
            *pParam3 = new CAsiChar(256);
            ((CAsiChar *) *pParam3)->setVarying(); // se si tratta di postgresql è necessario impostare setvaring
                                                   // altrimenti non funziona

            if ((*pCsr)->Bind(0, *pParam3) != kAsiGood)
            {
               gsc_ASITermStm(pExecStm);
               delete *pCsr;
               delete *pParam3;
               GS_ERR_COD = eGSInitPar;
               return GS_BAD;
            }
         }
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_ASIReadCol                          <external> /*
/*+
  Apre un cursore ASI, legge il dato di una colonna e lo richiude.
  Parametri:
  CAsiCsr     *pCsr;          input
  presbuf     OutData;        output; se usato deve già essere allocato (default = NULL)
  const TCHAR *ColName;       se passato viene letto la colonna indicata, altrimenti
                              verrà letta la prima colonna (default = NULL)
  
  Restituisce GS_GOOD in caso di successo oppure GS_BAD in caso di errore. 
-*/  
/*********************************************************/
int gsc_ASIReadCol(CAsiCsr *pCsr, presbuf OutData, const TCHAR *ColName)
{
   CAsiData  *pData;
   CAsiRow   *pRow;
   CAsiUcStr AsiUcrStr;
   int       i = 0;

   if (pCsr->Open() != kAsiGood)
   {
      gsc_printASIErr(pCsr);
      return GS_BAD;
   }

   if (pCsr->Fetch() != kAsiGood)
      { pCsr->Close(); GS_ERR_COD = eGSInvalidKey; return GS_BAD; }

   if ((pRow = pCsr->getCurrentRow()) == NULL)
      { pCsr->Close(); GS_ERR_COD = eGSInvalidKey; return GS_BAD; }

   // se la funzione è stata chiamata solo per sapere se c'era il record e non per leggere i valori
   if (!OutData) { pCsr->Close(); return GS_GOOD; }

   if (ColName) // Cerco una colonna con un nome conosciuto
   {
      int nFields = pRow->ColNum();
      for (i = 0; i < nFields; i++)
      {
         AsiUcrStr = GS_EMPTYSTR;
         (*pRow)[i].Name().ToString(&AsiUcrStr); // fa strcat
         if (AsiUcrStr.Compi(ColName) == 0) break;
      }

      if (i >= nFields) // Non trovato
         { pCsr->Close(); GS_ERR_COD = eGSReadRow; return GS_BAD; }
   }

   if ((pData = (*pRow)[i].getData()) == NULL)
      { pCsr->Close(); GS_ERR_COD = eGSReadRow; return GS_BAD; }

   if (gsc_ASIData2Rb(pData, OutData) == GS_BAD)
      { pCsr->Close(); GS_ERR_COD = eGSReadRow; return GS_BAD; }

   if (pCsr->Close() != kAsiGood) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_InitASIReadRow                     <external> */
/*+
  Questa funzione inizializza una C_RB_LIST per la lettura di
  una riga ASI.
  Parametri: 
  CAsiRow *pRow;        Puntatore alla riga ASI
  C_RB_LIST &ColValues; Lista per lettura righe
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_InitASIReadRow(CAsiRow *pRow, C_RB_LIST &ColValues)
{
   int       nFlds = pRow->ColNum(), i;
   CAsiUcStr AsiUcrStr;

   ColValues.remove_all();
   ColValues << acutBuildList(RTLB, 0);

   for (i = 0; i < nFlds; i++)
   {
      AsiUcrStr = GS_EMPTYSTR;
      (*pRow)[i].Name().ToString(&AsiUcrStr); // fa strcat
      
      if ((ColValues += acutBuildList(RTLB, RTSTR, AsiUcrStr.UcStr(), RTNIL, RTLE, 0)) == NULL)
         return GS_BAD;
   }

   if ((ColValues += acutBuildList(RTLE, 0)) == NULL) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_ASIReadRow                         <external> /*
/*+
  Questa funzione legge la riga corrente puntata dal cursore ASI 
  che deve essere già aperto.
  Parametri: 
  CAsiCsr   *pCsr;      Punatore al cursore ASI (input)
  C_RB_LIST &ColValues; Lista dei valori da leggere (se non è già stata 
                        inizializzata viene inizializzata automaticamente
                        vedi "gsc_InitDBReadRow")
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_ASIReadRow(CAsiCsr *pCsr, C_RB_LIST &ColValues)
{
   CAsiData  *pData;
   CAsiRow   *pRow;
   presbuf   pValue;
   long      nFlds;

   if ((pRow = pCsr->getCurrentRow()) == NULL)
      { pCsr->Close(); GS_ERR_COD = eGSInvalidKey; return GS_BAD; }
   nFlds = pRow->ColNum();

   if (!ColValues.get_head()) // da inizializzare
      if (gsc_InitASIReadRow(pRow, ColValues) == GS_BAD) return GS_BAD;

   pValue = ColValues.get_head();

   for (long i = 0; i < nFlds; i++)
   {
      if (!(pValue = pValue->rbnext) || !(pValue = pValue->rbnext) || !(pValue = pValue->rbnext))
         return GS_BAD; // ColValues passato come parametro era sbagliato

      if ((pData = (*pRow)[i].getData()) == NULL)
         { GS_ERR_COD = eGSReadRow; return GS_BAD; }
      if (gsc_ASIData2Rb(pData, pValue) == GS_BAD)
         { GS_ERR_COD = eGSReadRow; return GS_BAD; }

      pValue = pValue->rbnext;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_ASIReadRows                        <external> */
/*+
  Questa funzione apre il cursore, legge tutte le righe fin in fondo e
  richiude il cursore ASI.
  Parametri: 
  CAsiCsr     *pCsr;    Punatore al cursore ASI (input)
  C_RB_LIST &ColValues; Lista dei valori da leggere
                        (((<attr><val>)...) ((<attr><val>)...) ...)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
*/  
/*********************************************************/
int gsc_ASIReadRows(CAsiCsr *pCsr, C_RB_LIST &ColValues)
{
   C_RB_LIST Buffer;

   if (!(ColValues << acutBuildList(RTLB, 0))) return GS_BAD;

   if (pCsr->Open() != kAsiGood)
      { gsc_printASIErr(pCsr); return GS_BAD; }

   while (pCsr->Fetch() == kAsiGood)
   {
      if (gsc_ASIReadRow(pCsr, Buffer) == GS_BAD) { pCsr->Close(); return GS_BAD; }
      if (!(ColValues += gsc_rblistcopy(Buffer.get_head()))) return GS_BAD;
   }

   if (pCsr->Close() != kAsiGood) return GS_BAD;

   if (ColValues.GetCount() == 1)
      ColValues.remove_all();
   else
      if (!(ColValues += acutBuildList(RTLE, 0))) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_ASICheckSql <external> /*
/*+
  Controlla la correttezza dell'istruzione SQL.
  Parametri:
  const TCHAR *UDLName;        Nome file UDL
  const TCHAR *Statement;      Istruzione SQL
  
  Restituisce NULL in caso di successo oppure una stringa con messaggio 
  di errore. 
  N.B. Alloca memoria
-*/  
/*********************************************************/
TCHAR *gsc_ASICheckSql(const TCHAR *UDLName, const TCHAR *statement)
{
   CAsiAppl    *pAppl;
   CAsiSession *pSession;
   CAsiExecStm AsiExecStm;
   CAsiUcStr   UcStr(statement);
   TCHAR       *Result = NULL;

   // Inizializzo interfaccia ASI
	if ((pAppl = gsc_initasi()) == NULL) return NULL;

   // Inizializzo sessione ASI
	if ((pSession = gsc_ASICreateSession(UDLName)) == NULL)
      { gsc_termasi(&pAppl); return NULL; }

   ASI_TRY
      if (AsiExecStm.Prepare(pSession, UcStr) != kAsiGood)
      {
         GS_ERR_COD = eGSPrepareStm;
         Result = gsc_tostring(gsc_err(eGSPrepareStm));
      }
      AsiExecStm.Deallocate();

   ASI_CATCH(e)
      TCHAR Msg[MAX_LEN_MSG];

      UcStr = e->ErrMsg();
      UcStr.getToChar(Msg, MAX_LEN_MSG);
      Result = gsc_tostring(Msg);
   ASI_END_CATCH

   gsc_ASITermSession(&pSession);
   // Termino interfaccia ASI
	gsc_termasi(&pAppl);
   
   return Result;
}


/*********************************************************/
/*.doc gsc_ASIData2Rb                         <external> /*
/*+
  Converte un dato CAsiData in un resbuf.
  Parametri:
  CAsiData *pAsiData;
  presbuf  p_rb;
  
  Restituisce GS_GOOD in caso di successo oppure una GS_BAD.
-*/  
/*********************************************************/
int gsc_ASIData2Rb(CAsiData *pAsiData, presbuf p_rb)
{
   if (pAsiData->isNull() == kAsiGood)
      gsc_RbSubstNIL(p_rb);
   else
   if (pAsiData->is_numeric() == kAsiGood)
   {
      real buff;
      pAsiData->getValue(&buff);
      gsc_RbSubst(p_rb, buff);
   }
   else
   if (pAsiData->is_character() == kAsiGood)
   {
      TCHAR buff[ACCESS_MAX_LEN_FIELDCHAR];
      pAsiData->getValue(buff, ACCESS_MAX_LEN_FIELDCHAR - 1);
      gsc_RbSubst(p_rb, buff);
   }
   else
   if (pAsiData->is_datetime() == kAsiGood || pAsiData->is_yminterval() == kAsiGood ||
       pAsiData->is_dtinterval() == kAsiGood)
   {
      TCHAR buff[ACCESS_MAX_LEN_FIELDCHAR];
      pAsiData->getValue(buff, ACCESS_MAX_LEN_FIELDCHAR - 1);
      gsc_RbSubst(p_rb, buff);
   }
   else
      return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_Rb2ASIData                         <external> /*
/*+
  Converte un dato resbuf in un CAsiData.
  Parametri:
  presbuf  p_rb;
  CAsiData *pAsiData;
  
  Restituisce GS_GOOD in caso di successo oppure una GS_BAD.
-*/  
/*********************************************************/
int gsc_Rb2ASIData(presbuf p_rb, CAsiData *pAsiData)
{
   switch (p_rb->restype)
   {
      case RTNIL: case RTNONE:
         pAsiData->setNull();
         break;
      case RTSHORT:
         pAsiData->storeValue(p_rb->resval.rint);
         break;
      case RTLONG:
         pAsiData->storeValue(p_rb->resval.rlong);
         break;
      case RTREAL:
         pAsiData->storeValue(p_rb->resval.rreal);
         break;
      case RTT:
         pAsiData->storeValue(1L);
         break;
      case RTSTR:
      {
         CAsiUcStr dummy(p_rb->resval.rstring);
         pAsiData->storeValue(dummy);
         break;
      }
      default:
         return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_ReadASIParams                      <external> /*
/*+
  Questa funzione legge i valori dei parametri di input e li sostituisce
  nella istruzione SQL.
  Parametri:
  const TCHAR *UDLName;    Nome file UDL che definisce il collegamento OLE-DB
  const TCHAR *TableRef;   Riferiemnto alla tabella
  C_STRING    &Where;      Condizione della istruzione SQL
  UserInteractionModeEnum WINDOW_mode; Flag, se TRUE la richiesta dei parametri avviene
                                       tramite finestra (default = GSTextInteraction)                         
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  La funziona utilizza la libreria ASI.
-*/  
/*********************************************************/
int gsc_ReadASIParams(const TCHAR *UDLName, const TCHAR *TableRef, C_STRING &Where,
                      UserInteractionModeEnum WINDOW_mode)
{
   int         i = 0, j, n, cont_apici;
   int         InStr = FALSE, InParam = FALSE, JustRead = FALSE, Skip, EndOfStr;
   TCHAR       buff[1024], buffer[128], *pCond, ch, *p, *res;
   C_STRING    result, value, statement, prefix, test;
   C_2STR_LIST ParamList;
   C_2STR      *pParam;

   if (Where.len() == 0) return GS_GOOD;

   prefix = _T("SELECT * FROM ");
   prefix += TableRef;
   prefix += _T(" WHERE ");

   pCond = Where.get_name();

   // cerco i parametri (: seguito da una serie di caratteri alfanumerici o '_')
   while ((ch = pCond[i]) != _T('\0'))
   {
      switch (ch)
      {
         case _T('\''):
            InStr = !InStr;
            break;
         case _T(':'):
            if (!InStr) // Se non sono in una stringa
            {
               InParam = TRUE;
               j       = 0;
            }
            break;
         default:
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
                     // test di correttezza
                     if ((res = gsc_ASICheckSql(UDLName, test.get_name())) == NULL)
                        Skip = TRUE;
                     else
                        free(res);
                  }
                  if (!Skip)
                  {
                     // devo anteporre il carattere di ''' al carattere '''
                     value = _T('\'');
                     p = buffer;
                     n = gsc_strsep(p, _T('\0'), _T('\''));
                     for (cont_apici = 0; cont_apici < n; cont_apici++)
                     {
                        value += p;
                        value += _T("\'\'");
                        while (*p != _T('\0')) p++;
                        p++;
                     }
                     value += p;
                     value += _T('\'');
                     // provo considerando il parametro come una stringa
                     test = prefix;
                     test += result;
                     // valore esplicito
                     test += value;
                     if (!EndOfStr)
                        test += (pCond + i);
                     // test di correttezza
                     if ((res = gsc_ASICheckSql(UDLName, test.get_name())) != NULL)
                     {
                        free(res);
                        GS_ERR_COD = eGSInvalidSqlStatement;
                        return GS_BAD;
                     }
                  }
                  result += value;
                  if (EndOfStr) JustRead = TRUE;
               }
            }
      }
      if (!InParam && !JustRead || InStr)
         result += ch;
      else
         if (JustRead) JustRead = FALSE;

      i++;
   }

   Where = result;

   return GS_GOOD;
}


/****************************************************************************/
/*.doc gsc_printASIErr                                           <internal> */
/*+
  Questa funzione stampa i messaggi di errore per un puntatore generico
  (da cui tutti gli altri oggetti sono derivati)
  Parametri:
  CAseApiObj *dsc;
-*/
/****************************************************************************/
void gsc_printASIErr(CAsiSQLObject *dsc) 
{
   if (dsc->CondQty() > 0)
   {
      CAsiUcStr *pUcStr;
      TCHAR buf[BUFLEN];  // The error message buffer

      acutPrintf(_T("\nASI Errors #%d"), dsc->CondQty());
       
      for (int index = 0; index < dsc->CondQty(); index++ )
      {
         acutPrintf(_T("\nError N%d\n----------"), index);
         acutPrintf(_T("\nSQLSTATE Code: %s"), dsc->SQLSTATE(index));
         acutPrintf(_T("\nCode: %d"), dsc->Err(index));

         if ((pUcStr = dsc->ErrMsg(index)))
         {
            pUcStr->getToChar(buf, BUFLEN);
            acutPrintf(_T("\nMessage: %s"), buf);
         }
      }
   }
}


///////////////////////////////////////////////////////////////////////////////
//  FINE  FUNZIONI  ASI
///////////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc gsc_initase <external> /*
/*+
  Questa funzione inizializza l'ambiente ASE e va chiamata prima di
  tutte le altre funzioni per la gestione dei link. 
  Parametri:
  CAseAppl *ahandle;   puntatore all'ambiente ASE.
  
  Restituisce il puntatore all'ambiente ASE in caso di successo 
  altrimenti NULL.
-*/  
/*********************************************************/
CAseAppl *gsc_initase(void)
{
   CAseAppl *pAppl;

   // Inizializzo interfaccia ASE
	if ((pAppl = new CAseAppl()) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
	if (pAppl->init() != kAsiGood)
   {
      gsc_printASEErr(pAppl);
      delete pAppl; 
      GS_ERR_COD = eGSInitASE;
      return NULL;
   }

   return pAppl;
}


/*********************************************************/
/*.doc gsc_termase <external> /*
/*+
  Questa funzione termina l'ambiente ASE e va chiamata dopo
  tutte le altre funzioni per la gestione dei link. 
  Parametri:
  CAseAppl *pAppl;   puntatore all'ambiente ASE.
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_termase(CAseAppl **pAppl)
{
	if (*pAppl)
   {
      (*pAppl)->term();
      delete *pAppl;
      *pAppl = NULL;
   }

   return GS_GOOD;
}


// fatta per le applicazioni DllExport, perchè la variabile GEOsimAppl::ASE non si vede nella libreria
CAseAppl *get_GS_ASE()
{
   return GEOsimAppl::ASE;
}


/****************************************************************************/
/*.doc gsc_printASEErr                                           <internal> */
/*+
  Questa funzione stampa i messaggi di errore per un puntatore generico
  (da cui tutti gli altri oggetti sono derivati)
  Parametri:
  CAseApiObj *dsc;
-*/
/****************************************************************************/
void gsc_printASEErr(CAseApiObj *dsc) 
{
   TCHAR buf[BUFLEN];   // The error message buffer
   TCHAR *ptr;          // The Diagnostic parameter name pointer
   int   parcode;       // The diagnostic parameter code
   int   isstr;         // The diagnostic parameter type
   int   intpar;        // Integer parameter value

   if (dsc->errQty() > 0)
   {
      acutPrintf(_T("\nASE Errors #%d"), dsc->errQty());
       
      for (int index = 0; index < dsc->errQty(); index++ )
      {
         acutPrintf(_T("\nError N%d\n----------"), index);
         acutPrintf(_T("\nDescription Code: %d"), dsc->errDsc(index));
         acutPrintf(_T("\nCode: %d"), dsc->errCode(index));

         dsc->errMsg(index, buf, BUFLEN);
         acutPrintf(_T("\nMessage: %s"), buf);
         // Scan all of the diagnostic parameters(ASE & ASI), that ASE supports
         acutPrintf(_T("\nDiagnostic Parameters are:"));
         for (int pindex = 0; 
              (ptr = (TCHAR *) dsc->errDiagParNameCode(pindex, 
                                                      &parcode, &isstr)) != NULL;
              pindex++)
         {
            // Get the parameter values
            if (isstr)
            {
               // Get the string parameter value
               if (dsc->errDiagPar(index, parcode, buf, BUFLEN) == kAsiGood)
                  acutPrintf(_T("\n%s: [%s]"), ptr, buf);
            }
            else
            {
               // Get the integer parameter value
               if (dsc->errDiagPar(index, parcode, &intpar) == kAsiGood)
                  acutPrintf(_T("\n%s: [%d]"), ptr, intpar);
            }
         }
      }
   }
}


/****************************************************************************/
/*.doc gsc_LPNConnectAse                                         (internal) */
/*+
  Connette ASE all'ambiente indicato da LPN.
  Parametri:
  CAseLinkPath &LPN;  LinkPathName
  TCHAR        *usr; UserName
  TCHAR        *pwd; Password

  Returns GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/****************************************************************************/
int gsc_LPNConnectAse(CAseLinkPath &LPN, const TCHAR *usr, const TCHAR *pwd) 
{
   if (!(LPN.getStatus() & ASE_DO_CON)) // The DO belongs to the connected environment
   {
      // Collego la sessione ASE
      LPN.errClear();

      try //ASI_TRY
      {
         long  lenNameLPT;
         TCHAR *LPTName;

         // Ricavo la dimensione del nome del link
         if ((lenNameLPT = LPN.getNameSize(kAseEnvCode)) < 0) 
            { GS_ERR_COD = eGSInvalidLink; return GS_BAD; }
         // Alloco lo spazio per il nome del link
         if ((LPTName = (TCHAR *) calloc(lenNameLPT, sizeof(TCHAR))) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         if (LPN.getName(LPTName, lenNameLPT, kAseEnvCode) != kAsiGood)
            { free(LPTName); GS_ERR_COD = eGSInvalidLink; return GS_BAD; }

         C_STRING MyUDLFile(GEOsimAppl::ASE->getAcadDsPath()), ConnStr;

         MyUDLFile += _T("\\");
         MyUDLFile += LPTName;
         MyUDLFile += _T(".UDL");
         free(LPTName);

         ConnStr.paste(gsc_LoadOleDBIniStrFromFile(MyUDLFile.get_name()));

         // uso la login e password eventualmente indicati in IniString
         if (LPN.connectAseIniString(ConnStr.get_name()) == kAsiTrue) return GS_GOOD;
      }
      catch (...) {}
      //ASI_CATCH(e) {}
      //ASI_END_CATCH

      try //ASI_TRY
      {
         // provo con login e password
         if (LPN.connectAse(usr, pwd, kAsiTrue) == kAsiTrue) return GS_GOOD;
      }
      catch (...) {}
      //ASI_CATCH(e) {}
      //ASI_END_CATCH

      gsc_printASEErr(&LPN);
      GS_ERR_COD = eGSLPNConnectASE;
      return GS_BAD;
   }
   
   return GS_GOOD;
}


/****************************************************************************/
/*.doc gsc_LPNDisconnectAse                                      (internal) */
/*+
  Disconnette ASE all'ambiente indicato da LPN.
  Parametri:
  const TCHAR *pName; nome LPT
  oppure
  CAseLinkPath &LPN;  LinkPathName

  Returns GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/****************************************************************************/
int gsc_LPNDisconnectAse(const TCHAR *pName)
{
   CAseLinkPath LPN(GEOsimAppl::ASE);

   if (LPN.initName(pName) == kAsiBad) return GS_GOOD; // non c'è più

   return gsc_LPNDisconnectAse(LPN);
}
int gsc_LPNDisconnectAse(CAseLinkPath &LPN)
{
   int Status;
 
   Status = LPN.getStatus();
   if (Status & ASE_DO_CON)     // The DO belongs to the connected environment
   {
      long  lenNameLPT;
      TCHAR *LPTName;

      // Ricavo la dimensione del nome del link
      if ((lenNameLPT = LPN.getNameSize()) < 0) 
         { GS_ERR_COD = eGSInvalidLink; return GS_BAD; }
      // Alloco lo spazio per il nome del link
      if ((LPTName = (TCHAR *) calloc(lenNameLPT, sizeof(TCHAR))) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if (LPN.getName(LPTName, lenNameLPT) != kAsiGood)
         { free(LPTName); GS_ERR_COD = eGSInvalidLink; return GS_BAD; }

      // Scollego la sessione ASE
      if (LPN.disconnectAse() == kAsiBad)
      {
         gsc_printASEErr(&LPN);
         #if defined(GSDEBUG) // se versione per debugging
            acutPrintf(_T("\nLPT Name: %s\n"), LPTName);
         #endif
         GS_ERR_COD = eGSDisconnect;
         free(LPTName);

         return GS_BAD;
      }
      free(LPTName);
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_disconnectUDL                      <internal> */
/*+
  Questa funzione ritorna la lista dei Link Path Name di GEOsim registrati
  nel disegno.
  Parametri:
  const TCHAR *UDL
-*/  
/*********************************************************/
int gsc_disconnectUDL(const TCHAR *UDL)
{
   CAseLinkPath envPath(GEOsimAppl::ASE);    // Link Path Descriptor

   if (envPath.init() != kAsiGood) return GS_BAD;
   envPath.setName(UDL, kAseEnvCode);
   if (envPath.getStatus() & ASE_DO_CON)
      if (envPath.disconnectAse() != kAsiGood) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getLinkNames                       <internal> */
/*+
  Questa funzione ritorna la lista dei Link Path Name usati 
  dall'entità grafica.
  Parametri:
  ads_name ent;         Entità grafica
  C_STR_LIST &LPNList;  Lista dei LPN

  Ritorna GS_GOOD in caso di succeso altrimenti ritorna GS_BAD.
-*/  
/*********************************************************/
int gsc_getLinkNames(ads_name ent, C_STR_LIST &LPNList)
{
   AcDbObject   *pObj;
   AcDbObjectId objId;
   C_RB_LIST    EntDescrList;
   presbuf      p; 
   C_STR        *pStr;

   LPNList.remove_all();

   // Lettura entità estese di ASE
   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvalidEED; return GS_BAD; }  
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvalidEED; return GS_BAD; }
   EntDescrList << pObj->xData(ASE_APP_ID);
   if (pObj->close() != Acad::eOk) return GS_BAD;

   // Cerco dall'inizio
   p = EntDescrList.SearchType(1000);

   while (p)
   {
      // Prima di inserire il nome nella lista verifico che non esista
      if (LPNList.search_name(p->resval.rstring) == NULL)
      {
         if ((pStr = new C_STR(p->resval.rstring)) == NULL) 
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         LPNList.add_tail(pStr);
      }
 
      // Cerco dalla posizione del cursore successiva a quella attuale
      p = EntDescrList.SearchNextType(1000);
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getLinkNames                       <internal> */
/*+
  Questa funzione ritorna la lista dei Link Path Name usati 
  dagli oggetti del gruppo di selezione.
  Parametri:
  C_SELSET &SelSet;     Gruppo di selezione
  C_STR_LIST &listLPN;  Lista dei LPN
  int CounterToVideo;   flag, se = GS_GOOD stampa a video il numero di entità che si 
                        stanno elaborando (default = GS_BAD)

  Ritorna GS_GOOD in caso di succeso altrimenti ritorna GS_BAD.
-*/  
/*********************************************************/
int gsc_getLinkNames(C_SELSET &SelSet, C_STR_LIST &listLPN, int CounterToVideo)
{
   long       i = 0;
   ads_name   ent;
   C_STR_LIST PartialLPNList;
   C_STR      *pPartialLPN, *pStr;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1086)); // "Lettura collegamenti a database"
   
   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.Init(SelSet.length());

   while (SelSet.entname(i++, ent) == GS_GOOD)
   {
      if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.Set(i);

      if (gsc_getLinkNames(ent, PartialLPNList) != GS_GOOD) continue;

      pPartialLPN = (C_STR *) PartialLPNList.get_head();
      while (pPartialLPN)
      {
         // Prima di inserire il nome nella lista verifico che non esista
         if (listLPN.search_name(pPartialLPN->get_name()) == NULL)
         {
            if ((pStr = new C_STR(pPartialLPN->get_name())) == NULL) 
               { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            listLPN.add_tail(pStr);
         }

         pPartialLPN = (C_STR *) PartialLPNList.get_next();
      }
   }
   StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."
   return GS_GOOD;
}


/*******************************************************/
/*.doc int gs_getLPNfromSS(void)                       */
/*+                                                                   
  Funzione lisp che riceve un gruppo di selezione di oggetti
  lo esamina e restituisce tutti i nomi dei possibili LPN
  trovati negli oggetti verificati.
  
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD. 
-*/  
/*********************************************************/
int gs_getLPNfromSS(void)
{
   presbuf    arg = NULL;
   C_SELSET   ss;
   C_STR_LIST lpnList;

   acedRetNil();
   arg = acedGetArgs();
   
   // Gruppo di selezione da esaminare 
   if (!arg || arg->restype != RTPICKS)  
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   ss << arg->resval.rlname;
   ss.ReleaseAllAtDistruction(GS_BAD);

   if (gsc_getLinkNames(ss, lpnList, GS_GOOD) == GS_BAD) return RTERROR;
   
   if (lpnList.get_count() > 0)
   {
      C_RB_LIST res;

      // Ordino la lista da restituire
      lpnList.sort_name(TRUE, TRUE); // sensitive e ascending
      // Restituisco la lista del nomi di LPN
      if ((res << lpnList.to_rb()) == NULL) return RTERROR;
      res.LspRetList();
   }

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_SetACADUDLFile                     <external> */
/*+
  Questa funzione crea se necesario il file UDL di ACAD per creare un LPN.
  Parametri:
  const TCHAR    *UDLName;       Nome file UDL
  C_DBCONNECTION *pConn;         Connessione OLE-DB
  const TCHAR    *TableRef;      Riferimento alla tabella
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_SetACADUDLFile(const TCHAR *UDLName, C_DBCONNECTION *pConn,
                       const TCHAR *TableRef)
{
   C_STRING ConnectionStr, CompConnStr, UDLFile;

   if (!ConnectionStr.paste(pConn->getConnectionStrSubstCat(TableRef))) return GS_BAD;

   UDLFile = GEOsimAppl::ASE->getAcadDsPath(); // dir dei data source file
   UDLFile += _T('\\');
   UDLFile += UDLName;
   UDLFile += _T(".UDL");

   if (gsc_path_exist(UDLFile) == GS_GOOD)
   {
      CompConnStr.paste(gsc_LoadOleDBIniStrFromFile(UDLFile.get_name()));
      // Se il file UDL non ha le proprietà indicate da <ConnectionStr>
      // compatibili allora lo cancello e lo ricreo
      if (CompConnStr.comp(ConnectionStr) != 0)
      {
         if (gsc_delfile(UDLFile) == GS_BAD) return GS_BAD;
         if (gsc_WriteOleDBIniStrToFile(ConnectionStr.get_name(), UDLFile.get_name()) == GS_BAD)
            return GS_BAD;
      }
   }
   else // creo il file
      if (gsc_WriteOleDBIniStrToFile(ConnectionStr.get_name(), UDLFile.get_name()) == GS_BAD)
         return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getDOR                             <external> */
/*+
  Questa funzione ritorna il nome completo del Database Object Reference per un LPN.
  Parametri:
  const TCHAR *UDLName;       Nome file UDL
  C_DBCONNECTION *pConn;      Connessione OLE-DB
  const TCHAR *TableRef;      Riferimento alla tabella
  C_STRING &DOR;              out
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B.: Per ogni classe di entità viene creato un file UDL senza riutilizzo di
        un eventuale file UDL già esistente contenente la medesima connessione.
-*/  
/*********************************************************/
int gsc_getDOR(const TCHAR *UDLName, C_DBCONNECTION *pConn, const TCHAR *TableRef,
               C_STRING &DOR)
{
   C_STRING Catalog, Schema, Table;
   int      Delimited = FALSE;

   if (pConn->split_FullRefTable(TableRef, Catalog, Schema, Table) == GS_BAD)
      return GS_BAD;

   DOR = UDLName;
   DOR += _T('.');
//   if (Catalog.get_name()) DOR += Catalog;
   DOR += _T('.');
   if (Schema.get_name()) DOR += Schema;
   DOR += _T('.');
   
   // MAP vuole il nome della tabella racchiuso tra " se:
   // 1) contiene spazi
   // 2) se inizia con un numero o con "_"
   // 3) se contiene caratteri che non sono lettere o numeri ad eccezione di "_"
   TCHAR First = Table.get_chr(0);

   if (Table.at(_T(' '))) Delimited = TRUE;
   else if (iswdigit(First) || First == _T('_')) Delimited = TRUE;
   else
   {
      C_STRING dummy(Table);

      // escludo dalla verifica il carattere alfanumerico anche "_" perche è ammesso
      if (dummy.isAlnum(_T('_')) == GS_BAD) Delimited = TRUE;
      else
      {
      TCHAR ReservedWords[] = _T(" ALTER AS BETWEEN CASE FROM INNER INSERT JOIN SELECT \
TABLE WHERE ");

         dummy = Table;
         dummy.toupper();
         if (gsc_strstr(ReservedWords, dummy.get_name()) != 0) Delimited = TRUE;
      }
   }

   if (Delimited)
   {
      DOR += _T('"');
      DOR += Table;
      DOR += _T('"');
   }
   else
      DOR += Table;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_CreateLinkName                     <external> */
/*+
  Questa funzione crea un LinkPathName se non esiste.
  Parametri:
  const TCHAR     *NameLPN;        Nome del LPN
  const TCHAR     *UDLName;        Nome del file UDL per connessione MAP ad OLE-DB
  C_DBCONNECTION  *pConn;          Connessione OLE-DB
  const TCHAR     *TableRef;       Riferimento alla tabella
  const TCHAR     *KeyAttrib;      Nome del campo chiave
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_CreateLinkName(const TCHAR *NameLPN, const TCHAR *UDLName, C_DBCONNECTION *pConn,
                       const TCHAR *TableRef, const TCHAR *KeyAttrib)
{
   int          esito = GS_BAD;
   CAseLinkPath *pInternalLPT;

   if (!NameLPN || wcslen(NameLPN) == 0 ||
       !UDLName || wcslen(UDLName) == 0 ||
       !TableRef || wcslen(TableRef) == 0 ||
       !KeyAttrib || wcslen(KeyAttrib) == 0)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
  
   if ((pInternalLPT = new CAseLinkPath(GEOsimAppl::ASE)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   do
   {
      if (pInternalLPT->initName(NameLPN) == kAsiGood) // c'è già ma non è pronto
      {
         int Status = pInternalLPT->getStatus();

         // se non è già pronto
         if (!(Status & ASE_DO_REG) ||   // The DO is registered (has at least one Link Path).
             !(Status & ASE_DO_EXIST) || // The DO exists in the SQL database (can be 
                                         // assigned if the Environment is connected in ASE).
             !(Status & ASE_DO_CON))     // The DO belongs to the connected environment
         {
            // Preparo se necessario il file UDL
            if (gsc_SetACADUDLFile(UDLName, pConn, TableRef) == GS_BAD) break;

            // collego il LPN con l'ambiente ASE se necessario
            if (gsc_LPNConnectAse(*pInternalLPT, (TCHAR *) GEOsimAppl::GS_USER.log,
                                  (TCHAR *) GEOsimAppl::GS_USER.pwd) == GS_BAD)
               break;
         }
      }
      else
      {
         CAsiRow  *pRowDsc = NULL;       // The table description
         C_STRING DOR;

         // Preparo se necessario il file UDL
         if (gsc_SetACADUDLFile(UDLName, pConn, TableRef) == GS_BAD) break;

         // Setto l'ambiente (inizializzare il file UDL solo con il nome del file senza path
         // nè estensione, altrimenti disconnectAse non funziona)
         if (pInternalLPT->setName(UDLName, kAseEnvCode) != kAsiGood)
            { GS_ERR_COD = eGSInvalidLPN; break; }

         // collego il LPN con l'ambiente ASE se necessario
         if (gsc_LPNConnectAse(*pInternalLPT, (TCHAR *) GEOsimAppl::GS_USER.log,
                               (TCHAR *) GEOsimAppl::GS_USER.pwd) == GS_BAD)
            break;

         if (gsc_getDOR(UDLName, pConn, TableRef, DOR) == GS_BAD) break;
         if (pInternalLPT->initPath(DOR.get_name()) != kAsiGood)
         {
            #if defined(GSDEBUG) // se versione per debugging
               gsc_printASEErr(pInternalLPT);
            #endif
            GS_ERR_COD = eGSInvalidLPN;
            break;
         }

         // Get the table columns description
         if (pInternalLPT->getTableDsc(&pRowDsc) != kAsiGood)
         {
            #if defined(GSDEBUG) // se versione per debugging
               gsc_printASEErr(pInternalLPT);
            #endif
            gsc_LPNDisconnectAse(*pInternalLPT);
            GS_ERR_COD = eGSInvalidLPN;
            break;
         }
   
         // Cerco il campo chiave
         CAsiIdent AttrName(KeyAttrib);
         int Pos = pRowDsc->findCol(AttrName);  
         if (Pos == -1) 
         {
            delete pRowDsc;
            gsc_LPNDisconnectAse(*pInternalLPT);
            GS_ERR_COD = eGSInvalidKey;
            break;
         }

         const CAsiColumn *pCols[]={&(*pRowDsc)[Pos]};  

         if (pInternalLPT->setName(NameLPN) != kAsiGood)
         {
            #if defined(GSDEBUG) // se versione per debugging
               gsc_printASEErr(pInternalLPT);
            #endif
            delete pRowDsc;
            gsc_LPNDisconnectAse(*pInternalLPT);
            GS_ERR_COD = eGSInvalidLPN;
            break;
         }

         // Do the LPN creating
         if (pInternalLPT->create(pCols, 1) != kAsiGood)
         {
            #if defined(GSDEBUG) // se versione per debugging
               gsc_printASEErr(pInternalLPT);
            #endif
            delete pRowDsc;
            gsc_LPNDisconnectAse(*pInternalLPT);
            GS_ERR_COD = eGSInvalidLPN;
            break;
         }

         // Deallocate the columns description
         delete pRowDsc;
      }

      esito = GS_GOOD;
   }
   while (0);
   
   if (esito == GS_BAD) delete pInternalLPT;

   return esito;
}


/*********************************************************/
/*.doc gsc_EraseASELinkName                   <external> */
/*+
  Questa funzione cancella un LinkPathName.
  Parametri:
  const TCHAR        *NameLPN;        Nome del LPN
  int delLinkOnEnt;  Flag, se = GS_GOOD cancella i link su tutti gli
                     oggetti che usano il LPN (default = GS_BAD)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_EraseASELinkName(const TCHAR *NameLPN, int delLinkOnEnt)
{
   CAseLinkPath LPN(GEOsimAppl::ASE);

   if (LPN.initName(NameLPN) == kAsiBad) return GS_BAD;

   if (delLinkOnEnt == GS_GOOD)
   {
      LinkID      ID;
      long        i = 0;
      CAseLink    Link(GEOsimAppl::ASE);
      CAseLinkSel LS(GEOsimAppl::ASE);
      int         result = GS_GOOD;

      if (LS.initLp(LPN) == kAsiGood)
      {
         // inizio transazione ASE
         CAseAppl::ETransactionMode AseMode;
         AseMode = GEOsimAppl::ASE->getAseTransactionMode();
         GEOsimAppl::ASE->setAseTransactionMode(CAseAppl::kTransUserManaged);
         GEOsimAppl::ASE->beginAseTransaction();

         while ((ID = LS.getId(i++)) != LINKID_NULL)
            // inizializzo il descrittore di link e lo cancello
            if (Link.initId(ID) != kAsiGood || Link.remove() != kAsiGood)
               { GS_ERR_COD = eGSInvalidLink; result = GS_BAD; continue; }

         // fine transazione ASE
         GEOsimAppl::ASE->commitAseTransaction();
         // riconfiguro le transazioni ASE come in precedenza
         GEOsimAppl::ASE->setAseTransactionMode(AseMode);
      }
      if (result == GS_BAD) return GS_BAD;
   }

   if (LPN.isUpdatable() == kAsiGood)
      if (LPN.erase() != kAsiGood) return GS_BAD;
   if (gsc_LPNDisconnectAse(LPN) != GS_GOOD) return GS_BAD;  

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_EraseASELink                       <external> */
/*+
  Questa funzione cancella i link riferiti a un LPT indicato 
  da un gruppo di oggetti grafici.
  Parametri:
  C_STRING &NameLPN;       Nome del LPN
  C_SELSET &SelSet;
  int      CounterToVideo; flag, se = GS_GOOD stampa a video il numero di entità
                           che si stanno elaborando (default = GS_BAD)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_EraseASELink(C_STRING &NameLPN, C_SELSET &SelSet, int CounterToVideo)
{
   CAseLinkPath LPN(GEOsimAppl::ASE);
   CAseLinkSel LS(GEOsimAppl::ASE);
   int         result = GS_GOOD;
   ads_name    dummy;

   SelSet.get_selection(dummy);

   if (LPN.initName(NameLPN.get_name()) == kAsiBad) return GS_BAD;

   if (LS.initLpSel(LPN, dummy) == kAsiGood)
   {
      long     i = 0;
      LinkID   ID;
      CAseLink Link(GEOsimAppl::ASE);
      C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(385)); // "Cancellazione collegamenti ASE..."

      if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.Init(LS.getQty());

      // inizio transazione ASE
      CAseAppl::ETransactionMode AseMode;
      AseMode = GEOsimAppl::ASE->getAseTransactionMode();
      GEOsimAppl::ASE->setAseTransactionMode(CAseAppl::kTransUserManaged);
      GEOsimAppl::ASE->beginAseTransaction();

      while ((ID = LS.getId(i++)) != LINKID_NULL)
      {
         if (CounterToVideo == GS_GOOD)
            StatusBarProgressMeter.Set(i);

         // inizializzo il descrittore di link e lo cancello
         if (Link.initId(ID) != kAsiGood || Link.remove() != kAsiGood)
            { GS_ERR_COD = eGSInvalidLink; result = GS_BAD; continue; }
      }

      // fine transazione ASE
      GEOsimAppl::ASE->commitAseTransaction();
      // riconfiguro le transazioni ASE come in precedenza
      GEOsimAppl::ASE->setAseTransactionMode(AseMode);

      if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."
   }
   if (result == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getLPNameInfo                      <external> */
/*+
  Questa funzione ricava il codice del progetto, della classe e sottoclasse
  dal nome di un LPN di GEOsim.
  nella classe.
  Parametri:
  TCHAR *LPName;  nome del LPN
  int  *prj;      codice progetto      (default = NULL)
  int  *cls;      codice classe        (default = NULL)
  int  *sub;      codice sottoclasse   (default = NULL)

-*/  
/*********************************************************/
int gsc_getLPNameInfo(TCHAR *LPName, int *prj, int *cls, int *sub)
{
   TCHAR *p = LPName;
   int  code;

   if (!p) return GS_BAD;

   code = _wtoi(p += 3);        // salto il prefisso "PRJ"
   if (prj) *prj = code;

   // grezzo ma veloce
   if (code < 10) p += 1;      // "PRJ1"
   else
      if (code < 100) p += 2;  // "PRJ11"
      else
      if (code < 1000) p += 3; // "PRJ111"
      else p += 4;             // "PRJ1111"

   code = _wtoi(p += 3);        // salto il prefisso "CLS"
   if (cls) *cls = code;

   // grezzo ma veloce
   if (*cls < 10) p += 1;      // "PRJ1CLS1SUB"
   else
      if (*cls < 100) p += 2;  // "PRJ11CLS11SUB"
      else
      if (*cls < 1000) p += 3; // "PRJ111CLS111SUB"
      else p += 4;             // "PRJ1111CLS1111SUB"

   code = _wtoi(p += 3);        // salto il prefisso "SUB"
   if (sub) *sub = code;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getASEKey                          <external> */
/*+
  Questa funzione restituisce il codice chiave per un link ASE.
  Parametri:
  ads_name ent;               oggetto grafico
  const TCHAR *LPNName;        Nome di un LPN
  long *Key;                  Codice chiave
  oppure
  CAseLink &Link;             Link ASE
  long *Key;                  Codice chiave
  const TCHAR *LPName;         Nome del Link Path Name (default = NULL)
  oppure
  CAseLink &Link;
  CAsiRow *pKeyCols;
  resbuf *rbKey;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_getASEKey(CAseLink &Link, long *Key, const TCHAR *LPName)
{
   resbuf rbKey;

   if (gsc_getASEKey(Link, &rbKey, LPName) == GS_BAD) return GS_BAD;

   return gsc_rb2Lng(&rbKey, Key);
}
int gsc_getASEKey(CAseLink &Link, resbuf *rbKey, const TCHAR *LPName)
{
   TCHAR        LPNName[MAX_LPN_LEN];
   CAsiRow      *pKeyCols;
   CAseLinkPath LPN(GEOsimAppl::ASE);

   if (LPName) gsc_strcpy(LPNName, LPName, MAX_LPN_LEN);
   else
      // Leggo LPN
      if (Link.getName(LPNName, MAX_LPN_LEN) != kAsiGood)
         { GS_ERR_COD = eGSInvalidLink; return GS_BAD; }

   // Leggo il valore chiave
   if (LPN.initName(LPNName) != kAsiGood)
      { GS_ERR_COD = eGSInvalidLink; return GS_BAD; }
   if (LPN.getKeyDsc(&pKeyCols) != kAsiGood || pKeyCols->ColNum() > 1)
      { GS_ERR_COD = eGSInvalidLPN; return GS_BAD; }

   return gsc_getASEKey(Link, pKeyCols, rbKey);
}
int gsc_getASEKey(CAseLink &Link, CAsiRow *pKeyCols, resbuf *rbKey)
{
   // Leggo il valore chiave
   if (Link.getKey(pKeyCols) != kAsiGood)
      { GS_ERR_COD = eGSInvalidLink; return GS_BAD; }

   switch ((*pKeyCols)[0].getData()->Type())
   {
      case kAsiChar:
      case kAsiCharVar:
      case kAsiDateTime:
      case kAsiInterval:
      case kAsiBit:
      case kAsiBitVar:
      {
         CAsiUcStr Buffer;

         if ((*pKeyCols)[0].getData()->getValue(&Buffer) == kAsiBad)
            { GS_ERR_COD = eGSInvalidLink; return GS_BAD; }
         gsc_RbSubst(rbKey, Buffer.UcStr());
         break;
      }
      case kAsiNumeric:
      case kAsiDecimal:
      case kAsiFloat:
      case kAsiReal:
      case kAsiDouble:
      {
         real Key;
         if ((*pKeyCols)[0].getData()->getValue(&Key) == kAsiBad)
            { GS_ERR_COD = eGSInvalidLink; return GS_BAD; }
         gsc_RbSubst(rbKey, Key);
         break;
      }
      case kAsiInteger:
      {
         integer Key;
         if ((*pKeyCols)[0].getData()->getValue(&Key) == kAsiBad)
            { GS_ERR_COD = eGSInvalidLink; return GS_BAD; }
         gsc_RbSubst(rbKey, Key);
         break;
      } 
      case kAsiSmallInt:
      {
         smallint Key;
         if ((*pKeyCols)[0].getData()->getValue(&Key) == kAsiBad)
            { GS_ERR_COD = eGSInvalidLink; return GS_BAD; }
         gsc_RbSubst(rbKey, Key);
         break;
      } 
      default:
         gsc_RbSubstNIL(rbKey);
   }

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
//  INIZIO  FUNZIONI    PER TABELLA GS_LINK
///////////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc gsc_get_gs_link_struct                 <internal> */
/*+
  Questa funzione restituisce la struttura della tabella GS_LINK di GEOsim.
  N.B. Utilizzando il Provider per ACCESS 97 non è possibile creare tabelle
       con strutture che superano una certa dimensione perchè si genera un 
       errore "record too large". Si utilizza perciò un database
       già preparato contenente la tabella in questione.
       La funzione ha, quindi, solo uno scopo documentativo.
-*/  
/*********************************************************/
void gsc_get_gs_link_struct(C_STRING &Stru)
{  
   Stru =  _T(" (CLASS_ID SMALLINT NOT NULL, "); // Codice della classe
   Stru += _T("SUB_CL_ID BYTE NOT NULL, ");      // Codice della sottoclasse
   Stru += _T("KEY_ATTRIB LONG NOT NULL, ");     // Codice dell'entità
   Stru += _T("HANDLE CHAR(");                   // Handle dell'oggetto grafico
   Stru += MAX_LEN_HANDLE - 1;
   Stru += _T(')');
}


/*********************************************************/
/*.doc gsc_PrepareSelLinkWhereCls             <external> */
/*+	   
  Questa funzione restituisce il comando SQL per la ricerca di link
  di una classe.
  Parametri:
  C_DBCONNECTION *pConn;                  Puntatore connessione OLE-DB
  C_STRING       &TableRef;               Riferimento completo alla tabella
  _CommandPtr    &pCmd;
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_PrepareSelLinkWhereCls(C_DBCONNECTION *pConn, C_STRING &TableRef,
                               _CommandPtr &pCmd)
{
   C_STRING      statement;
   _ParameterPtr pParam;

   // interrogo il database con la select where "codice classe e sottoclasse"
   statement = _T("SELECT * FROM ");
   statement += TableRef;
   statement += _T(" WHERE CLASS_ID=? AND SUB_CL_ID=?");

   if (gsc_PrepareCmd(pConn->get_Connection(), statement, pCmd) == GS_BAD)
      return GS_BAD;
   // adSmallInt = 2 byte
   if ((pParam = pCmd->CreateParameter(_T("CLASS_ID"), adSmallInt, adParamInput, 2)) == NULL)
      return GS_BAD;
   pCmd->Parameters->Append(pParam);
   // adSmallInt = 2 byte
   if ((pParam = pCmd->CreateParameter(_T("SUB_CL_ID"), adSmallInt, adParamInput, 2)) == NULL)
      return GS_BAD;
   pCmd->Parameters->Append(pParam);

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_PrepareSelLinkWhereKey             <external> */
/*+	   
  Questa funzione restituisce il comando SQL per la ricerca di link
  di un record di una classe.
  Parametri:
  C_DBCONNECTION *pConn;                  Puntatore connessione OLE-DB
  C_STRING       &TableRef;               Riferimento completo alla tabella
  _CommandPtr    &pCmd;
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_PrepareSelLinkWhereKey(C_DBCONNECTION *pConn, C_STRING &TableRef,
                               _CommandPtr &pCmd)
{
   C_STRING      statement;
   _ParameterPtr pParam;

   // interrogo il database con la select where "codice database"
   statement = _T("SELECT * FROM ");
   statement += TableRef;
   statement += _T(" WHERE CLASS_ID=? AND SUB_CL_ID=? AND KEY_ATTRIB=?");
   if (gsc_PrepareCmd(pConn->get_Connection(), statement, pCmd) == GS_BAD)
      return GS_BAD;
   // adSmallInt = 2 byte
   if ((pParam = pCmd->CreateParameter(_T("CLASS_ID"), adSmallInt, adParamInput, 2)) == NULL)
      return GS_BAD;
   pCmd->Parameters->Append(pParam);
   // adSmallInt = 2 byte
   if ((pParam = pCmd->CreateParameter(_T("SUB_CL_ID"), adSmallInt, adParamInput, 2)) == NULL)
      return GS_BAD;
   pCmd->Parameters->Append(pParam);
   // adInteger = 4 byte
   if ((pParam = pCmd->CreateParameter(_T("KEY_ATTRIB"), adInteger, adParamInput, 4)) == NULL)
      return GS_BAD;
   pCmd->Parameters->Append(pParam);

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_PrepareSelLinkWhereHandle          <external> */
/*+	   
  Questa funzione restituisce il comando SQL per la ricerca di link
  di una entità grafica di una classe.
  Parametri:
  C_DBCONNECTION *pConn;                  Puntatore connessione OLE-DB
  C_STRING       &TableRef;               Riferimento completo alla tabella
  _CommandPtr    &pCmd;
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_PrepareSelLinkWhereHandle(C_DBCONNECTION *pConn, C_STRING &TableRef,
                                  _CommandPtr &pCmd)
{
   C_STRING statement;
   _ParameterPtr pParam;

   // interrogo il database con la select where "handle grafico"
   statement = _T("SELECT * FROM ");
   statement += TableRef;
   statement += _T(" WHERE HANDLE=?");

   if (gsc_PrepareCmd(pConn->get_Connection(), statement, pCmd) == GS_BAD)
      return GS_BAD;
   if ((pParam = pCmd->CreateParameter(_T("HANDLE"), adWChar, adParamInput, MAX_LEN_HANDLE)) == NULL)
      return GS_BAD;
   pCmd->Parameters->Append(pParam);

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI PER TABELLA GS_LINK
///////////////////////////////////////////////////////////////////////////////


//-----------------------------------------------------------------------//
//////////////////  C_LINK  INIZIO  ///////////////////////////////////////
//-----------------------------------------------------------------------//


C_LINK::C_LINK()
{
   initialize();
}
C_LINK::C_LINK(ads_name _ent)
{
   initialize();
   SetEnt(_ent);
}


/*********************************************************/
/*.doc C_LINK::~C_LINK <internal> */
/*+
  Questa funzione è il distruttore di C_LINK
-*/  
/*********************************************************/
C_LINK::~C_LINK()
{
   ads_name_clear(ent);
   cls = sub = key = 0;
   wcscpy(handle, GS_EMPTYSTR);
}

void C_LINK::initialize(void)
{
   cls = sub = key = 0;
   ads_name_clear(ent);
   wcscpy(handle, GS_EMPTYSTR);

   // inizializzazione lista resbuf di lettura/scrittura
   ColValues <<acutBuildList(RTLB,
                             RTLB, RTSTR, _T("CLASS_ID"),   RTSHORT, 0,      RTLE,
                             RTLB, RTSTR, _T("SUB_CL_ID"),  RTSHORT, 0,      RTLE,
                             RTLB, RTSTR, _T("KEY_ATTRIB"), RTLONG,  0,      RTLE,
                             RTLB, RTSTR, _T("HANDLE"),     RTSTR,   _T(""), RTLE,
                             RTLE, 0);

   // inizializzazione puntatori lista resbuf di lettura
   prb_Cls    = ColValues.CdrAssoc(_T("CLASS_ID"));
   prb_Sub    = ColValues.CdrAssoc(_T("SUB_CL_ID"));
   prb_Key    = ColValues.CdrAssoc(_T("KEY_ATTRIB"));
   prb_Handle = ColValues.CdrAssoc(_T("HANDLE"));
}

int C_LINK::GetEnt(ads_name _ent)
{
   if (ads_name_nil(ent))
   {
      if (wcslen(handle) == 0) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      if (acdbHandEnt(handle, ent) != RTNORM) { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   }
   ads_name_set(ent, _ent);

   return GS_GOOD;
}

int C_LINK::GetHandle(TCHAR _handle[MAX_LEN_HANDLE])
{
   if (wcslen(handle) == 0)
   {
      if (ads_name_nil(ent))
         { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      if (gsc_enthand(ent, handle) == GS_BAD)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   }
   wcscpy(_handle, handle);

   return GS_GOOD;
}

int C_LINK::SetEnt(ads_name _ent)
{ ads_name_set(_ent, ent); wcscpy(handle, GS_EMPTYSTR); return GS_GOOD; }
int C_LINK::SetEnt(const TCHAR *_handle)
{
   ads_name_clear(ent);
   gsc_strcpy(handle, _handle, MAX_LEN_HANDLE);
   return GS_GOOD;
}


/*********************************************************/
/*.doc C_LINK::GetKey                         <external> */
/*+
  Questa funzione restituisce informazioni (lette dalla tabella GS_LINK)
  riguardanti l'oggetto grafico precedentemente settato con SetEnt().
  Parametri:
  long *key_val;     valore chiave
  int  *cls_code;    codice classe        (default = NULL)
  int  *sub_code;    codice sottoclasse   (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_LINK::GetKey(long *_key, int *_cls, int *_sub)
{
   _CommandPtr   pCmd;
   _RecordsetPtr pRs;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if (wcslen(handle) == 0)
      if (gsc_enthand(ent, handle) == GS_BAD) return GS_BAD;
   
   if ((pRs = GS_CURRENT_WRK_SESSION->get_RsSelectLink()) != NULL) 
   {  
      _variant_t KeyForSeek(handle);

      // Uso tecnica dei recordset molto più veloce
      if (gsc_DBSetIndexRs(pRs, _T("HANDLE")) == GS_BAD ||
          gsc_DBSeekRs(pRs, KeyForSeek, adSeekFirstEQ) == GS_BAD)
          return GS_BAD;

      if (gsc_isEOF(pRs) == GS_GOOD) { GS_ERR_COD = eGSInvalidKey; return GS_BAD; }

      // lettura record
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) return GS_BAD;
   }
   else
   {
      // Ricavo il comando che esegue la selezione di link per handle
      if ((pCmd = GS_CURRENT_WRK_SESSION->get_CmdSelectLinkWhereHandle()) == NULL)
         return GS_BAD;

      // imposto come parametro l'handle dell'oggetto grafico
      if (gsc_SetDBParam(pCmd, 0, handle) == GS_BAD) return GS_BAD;

      // Esegue l'istruzione SQL
      if (gsc_ExeCmd(pCmd, pRs) == GS_BAD) return GS_BAD;

      if (gsc_isEOF(pRs) == GS_GOOD)
         { gsc_DBCloseRs(pRs); GS_ERR_COD = eGSInvalidKey; return GS_BAD; }
   
      // lettura record
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
         { gsc_DBCloseRs(pRs); return GS_BAD; }
      gsc_DBCloseRs(pRs);
   }

   // se key = 0 allora link cancellato (record invalidato)
   if (gsc_rb2Lng(prb_Key, &key) == GS_BAD || key == 0) return GS_BAD;
   gsc_rb2Int(prb_Cls, &cls);
   gsc_rb2Int(prb_Sub, &sub);
   
   *_key = key;
   if (_cls) *_cls = cls;
   if (_sub) *_sub = sub;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_LINK::Set                            <external> */
/*+
  Questa funzione aggiorna la tabella GS_LINK con il codice GEOsim del
  corrispondente handle dell'oggetto grafico. Qualora esistesse già un link
  per l'oggetto grafico questo verrà aggiornato altrimenti verrà
  inserito un nuovo link.
  Parametri:
  ads_name _ent;  o  const TCHAR *Handle; Handle o entità grafica
  int _cls;                              Codice classe
  int _sub;                              Codice sottoclasse
  long _key;                             Valore chiave
  int mode;                 Modalità per ottimizzare la funzione. Se mode = INSERT si 
                            presuppone di inserire un link nuovo quindi la funzione 
                            proverà prima l'inserimento e in caso esistesse già lo modifica.
                            Se mode = MODIFY si presuppone che il link esista già quindi
                            la funzione proverà prima la modifica e in caso non esistesse
                            lo inserisce (default = INSERT)
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_LINK::Set(ads_name _ent, int _cls, int _sub, long _key, int mode)
{
   if (SetEnt(_ent) == GS_BAD) return GS_BAD;
   if (gsc_enthand(_ent, handle) == GS_BAD) return GS_BAD;
   return Set(handle, _cls, _sub, _key, mode);
}
int C_LINK::Set(const TCHAR *_handle, int _cls, int _sub, long _key, int mode)
{
   C_STRING      ODTableName;
   _RecordsetPtr pRs, pInsRs;
   _CommandPtr   pCmd;
   static int    isRsSelectLinkSupported = -1;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   gsc_strcpy(handle, _handle, MAX_LEN_HANDLE);
   SetEnt(_handle);
   cls = _cls;
   sub = _sub;
   key = _key;

   gsc_getODTableName(GS_CURRENT_WRK_SESSION->get_PrjId(), cls, sub, ODTableName);
   if (gsc_setID2ODTable(_handle, ODTableName, key) == GS_BAD) return GS_BAD;
   if (gsc_setID2EED(_handle, key) == GS_BAD) return GS_BAD;

   gsc_RbSubst(prb_Cls, _cls);
   gsc_RbSubst(prb_Sub, _sub);
   gsc_RbSubst(prb_Key, _key);
   gsc_RbSubst(prb_Handle, _handle);

   // Poichè l'inserimento di record aperto per applicare il metodo seek 
   // è circa 8 volte più lento che su un recordset standard
   // uso un recordset diverso per inserire
   if ((pInsRs = GS_CURRENT_WRK_SESSION->get_RsInsertLink()) == NULL) return GS_BAD;

   // modalità per ottimizzare la funzione
   if (mode == INSERT) // prima provo ad inserire e, se esiste già, lo modifico
   {
      if (isRsSelectLinkSupported == -1) // Non ancora inizializzato
         isRsSelectLinkSupported = (GS_CURRENT_WRK_SESSION->get_RsSelectLink() == NULL) ? GS_BAD : GS_GOOD;

      if (isRsSelectLinkSupported == GS_GOOD) pRs = GS_CURRENT_WRK_SESSION->get_RsSelectLink();

      if (gsc_DBInsRow(pInsRs, ColValues, ONETEST, GS_BAD) == GS_BAD)
      {
         // se l'errore non era dovuto al fatto che il record esisteva già
         if (GS_ERR_COD != eGSIntConstr) return GS_BAD;

         if (isRsSelectLinkSupported == GS_GOOD) 
         {  
            _variant_t KeyForSeek(_handle);

            // Uso tecnica dei recordset molto più veloce
            if (gsc_DBSetIndexRs(pRs, _T("HANDLE")) == GS_BAD ||
                gsc_DBSeekRs(pRs, KeyForSeek, adSeekFirstEQ) == GS_BAD)
                return GS_BAD;

            if (gsc_isEOF(pRs) == GS_BAD)
               if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD) return GS_BAD;
         }
         else
         {
            // Ricavo il comando che esegue la selezione di link per handle
            if ((pCmd = GS_CURRENT_WRK_SESSION->get_CmdSelectLinkWhereHandle()) == NULL)
               return GS_BAD;

            // imposto come parametro l'handle dell'oggetto grafico
            if (gsc_SetDBParam(pCmd, 0, _handle) == GS_BAD) return GS_BAD;

            // Esegue l'istruzione SQL
            // prima era adOpenKeyset ma postgresql in una transazione fa casino
            if (gsc_ExeCmd(pCmd, pRs, adOpenDynamic, adLockOptimistic) == GS_BAD)
               return GS_BAD;

            if (gsc_isEOF(pRs) == GS_BAD)
               if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD)
                  { gsc_DBCloseRs(pRs); return GS_BAD; }

            gsc_DBCloseRs(pRs);
         }
      }
   }
   else // prima provo a modificare e, se non esistente, lo inserisco
   {
      if ((pRs = GS_CURRENT_WRK_SESSION->get_RsSelectLink()) != NULL) 
      {  
         _variant_t KeyForSeek(_handle);

         // Uso tecnica dei recordset molto più veloce
         if (gsc_DBSetIndexRs(pRs, _T("HANDLE")) == GS_BAD ||
             gsc_DBSeekRs(pRs, KeyForSeek, adSeekFirstEQ) == GS_BAD)
             return GS_BAD;

         if (gsc_isEOF(pRs) == GS_BAD)
         {
            if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD) return GS_BAD;
         }
         else
            if (gsc_DBInsRow(pInsRs, ColValues, ONETEST, GS_BAD) == GS_BAD) return GS_BAD;
      }
      else
      {
         // Ricavo il comando che esegue la selezione di link per handle
         if ((pCmd = GS_CURRENT_WRK_SESSION->get_CmdSelectLinkWhereHandle()) == NULL)
            return GS_BAD;

         // imposto come parametro l'handle dell'oggetto grafico
         if (gsc_SetDBParam(pCmd, 0, _handle) == GS_BAD) return GS_BAD;

         // Esegue l'istruzione SQL
         // prima era adOpenKeyset ma postgresql in una transazione fa casino
         if (gsc_ExeCmd(pCmd, pRs, adOpenDynamic, adLockOptimistic) == GS_BAD)
            return GS_BAD;
   
         if (gsc_isEOF(pRs) == GS_BAD)
         {
            if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD)
               { gsc_DBCloseRs(pRs); return GS_BAD; }
         }
         else
            if (gsc_DBInsRow(pInsRs, ColValues, ONETEST, GS_BAD) == GS_BAD)
               return GS_BAD;

         gsc_DBCloseRs(pRs);
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_SetASEKey                          <internal> */
/*+
  Questa funzione associa un link ASE a ciascun oggetto 
  del gruppo di selezione.
  Parametri:
  C_SELSET &SelSet;        gruppo di selezione
  int      CounterToVideo; flag, se = GS_GOOD stampa a video il numero di entità
                           che si stanno elaborando (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_SetASEKey(C_SELSET &SelSet, int CounterToVideo)
{
   ads_name     ent;
   long         i = 0, Key;
   CAseLink     ASELink(GEOsimAppl::ASE);
   CAsiRow      *pKeyCols = NULL;
   CAseLinkPath LPT(GEOsimAppl::ASE);
   C_CLASS      *pCls, *pPrevCls = NULL;
   C_STRING     LPTName;
   int          res = GS_GOOD;
   C_LINK       Link;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(387)); // "Inserimento collegamenti ASE..."

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.Init(SelSet.length());

   // inizio transazione ASE
   CAseAppl::ETransactionMode AseMode;
   AseMode = GEOsimAppl::ASE->getAseTransactionMode();
   GEOsimAppl::ASE->setAseTransactionMode(CAseAppl::kTransUserManaged);
   GEOsimAppl::ASE->beginAseTransaction();

   while (SelSet.entname(i++, ent) == GS_GOOD)
   {
      if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.Set(i);

      if ((pCls = GS_CURRENT_WRK_SESSION->find_class(ent)) == NULL) 
         { res = GS_BAD; continue; }

      if (pPrevCls != pCls)
      {
         if (pPrevCls) // non era la prima volta
            gsc_LPNDisconnectAse(LPT);

         // ottengo il nome del LPT
         if (pCls->GetLPN4OLD(LPTName) == GS_BAD || 
             LPT.initName(LPTName.get_name()) == kAsiBad ||
             LPT.getKeyDsc(&pKeyCols) != kAsiGood || pKeyCols->ColNum() > 1)
            { res = GS_BAD; continue; }
         pPrevCls = pCls;
      }

      if (Link.SetEnt(ent) == GS_BAD || Link.GetKey(&Key) == GS_BAD)
         { res = GS_BAD; continue; }

      // le dimensioni sono controllate automaticamente
      if ((*pKeyCols)[0].getData()->storeValue(Key) == kAsiBad)
         { res = GS_BAD; continue; }

      // Inizializzo dati del link
      if (ASELink.initType(kAseEntityLink) != kAsiGood ||
          ASELink.initEnt(LPTName.get_name(), *pKeyCols, ent) != kAsiGood)
         { res = GS_BAD; continue; }

      // creo il link di AutoCAD
      if (ASELink.create() != kAsiGood) { res = GS_BAD; continue; }   
   }

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

   if (pPrevCls) // inizializzato il LPT roby test
      gsc_LPNDisconnectAse(LPT);

   // fine transazione ASE
   GEOsimAppl::ASE->commitAseTransaction();
   // riconfiguro le transazioni ASE come in precedenza
   GEOsimAppl::ASE->setAseTransactionMode(AseMode);

   return res;
}


/*********************************************************/
/*.doc gsc_SetASEKey                          <internal> */
/*+
  Questa funzione associa un link ASE ad un oggetto grafico.
  Parametri:
  ads_name ent;            Oggetto grafico
  C_STRING *ASELPName;     Nome di un LPN 
  long     Key;            Valore chiave

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_SetASEKey(ads_name ent, C_STRING &ASELPName, long Key)
{
   CAseLink Link(GEOsimAppl::ASE);
   CAsiRow        *pKeyCols;
   CAseLinkPath   LPT(GEOsimAppl::ASE);

   // inizializzo il LPT
   if (LPT.initName(ASELPName.get_name()) == kAsiBad)
      { GS_ERR_COD = eGSInvalidLPN; return GS_BAD; }
   if (LPT.getKeyDsc(&pKeyCols) != kAsiGood || pKeyCols->ColNum() > 1)
      { GS_ERR_COD = eGSInvalidLPN; return GS_BAD; }

   return gsc_SetASEKey(ent, ASELPName, *pKeyCols, Key);
}
int gsc_SetASEKey(ads_name ent, C_STRING &ASELPName, CAsiRow &pKeyCols, long Key)
{
   CAseLink Link(GEOsimAppl::ASE);

   // le dimensioni sono controllate automaticamente
   if (pKeyCols[0].getData()->storeValue(Key) == kAsiBad)
      { GS_ERR_COD = eGSInvalidLink; return GS_BAD; }

   // Inizializzo dati del link
   if (Link.initType(kAseEntityLink) != kAsiGood ||
       Link.initEnt(ASELPName.get_name(), pKeyCols, ent) != kAsiGood)
      { GS_ERR_COD = eGSInvalidLink; return GS_BAD; }

   // creo il link di AutoCAD
   if (Link.create() != kAsiGood)
      if (Link.errCode(0) != 946) // Link duplicato
      {
         #if defined(GSDEBUG) // se versione per debugging
            gsc_printASEErr(&Link);
         #endif
         GS_ERR_COD = eGSInvalidLink;
         return GS_BAD;
      }

   return GS_GOOD;
}


/******************************************************************/
/*.doc C_LINK::erase                                              */
/*+                                                            
  Funzione che cancella tutti i Link di GEOsim di una entità 
  Parametri:
  ads_name _ent;  entità da cui rimuovere i link ai DB (opzionale, default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/******************************************************************/
int C_LINK::erase(ads_name _ent)
{
   C_EED         eed;
   C_STRING      ODTableName;
   _CommandPtr   pCmd;
   _RecordsetPtr pRs;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if (_ent && !ads_name_nil(_ent))
      if (SetEnt(_ent) == GS_BAD) return GS_BAD;

   if (GetEnt(ent) == GS_BAD || eed.load(ent) == GS_BAD) return GS_GOOD;

   // ricavo il collegamento con il DB e lo cancello da OD e da EED
   gsc_getODTableName(GS_CURRENT_WRK_SESSION->get_PrjId(), eed.cls, eed.sub, ODTableName);
   if (gsc_delID2ODTable(ent, ODTableName) == GS_BAD) return GS_BAD;
   eed.gs_id = 0;
   if (eed.save() == GS_BAD) return GS_BAD;

   if (wcslen(handle) == 0)
      if (gsc_enthand(ent, handle) == GS_BAD) return GS_BAD;

   gsc_RbSubst(prb_Cls, 0);
   gsc_RbSubst(prb_Sub, 0);
   gsc_RbSubst(prb_Key, 0);
   gsc_RbSubst(prb_Handle, handle);

   if ((pRs = GS_CURRENT_WRK_SESSION->get_RsSelectLink()) != NULL) 
   {  
      _variant_t KeyForSeek(handle);

      // Uso tecnica dei recordset molto più veloce
      if (gsc_DBSetIndexRs(pRs, _T("HANDLE")) == GS_BAD ||
          gsc_DBSeekRs(pRs, KeyForSeek, adSeekFirstEQ) == GS_BAD)
          return GS_BAD;
      if (gsc_isEOF(pRs) == GS_BAD)
         if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD) return GS_BAD;
   }
   else
   {
      // Ricavo il comando che esegue la selezione di link per handle
      if ((pCmd = GS_CURRENT_WRK_SESSION->get_CmdSelectLinkWhereHandle()) == NULL) return GS_BAD;

      // imposto come parametro l'handle dell'oggetto grafico
      if (gsc_SetDBParam(pCmd, 0, handle) == GS_BAD) return GS_BAD;

      // Esegue l'istruzione SQL
      // prima era adOpenKeyset ma postgresql in una transazione fa casino
      if (gsc_ExeCmd(pCmd, pRs, adOpenDynamic, adLockOptimistic) == GS_BAD) return GS_BAD;

      if (gsc_isEOF(pRs) == GS_BAD)
         if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD)
            { gsc_DBCloseRs(pRs); return GS_BAD; }

      if (gsc_DBCloseRs(pRs) == GS_BAD) return GS_BAD;
   }
   
   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//////////////////  C_LINK      FINE    ///////////////////////////////////
//////////////////  C_LINK_SET  INIZIO  ///////////////////////////////////
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc C_LINK_SET::C_LINK_SET                 <internal> */
/*+
  Questa funzione è il costruttore di C_LINK_SET
-*/  
/*********************************************************/
C_LINK_SET::C_LINK_SET()
{
   initialize();
}


/*********************************************************/
/*.doc C_LINK_SET::~C_LINK_SET                <internal> */
/*+
  Questa funzione è il distruttore di C_LINK_SET
-*/  
/*********************************************************/
C_LINK_SET::~C_LINK_SET() {}


void C_LINK_SET::initialize(void)
{
   cls = 0;
   sub = 0;

   // inizializzazione lista resbuf di lettura/scrittura
   ColValues <<acutBuildList(RTLB,
                             RTLB, RTSTR, _T("CLASS_ID"),   RTSHORT, 0,      RTLE,
                             RTLB, RTSTR, _T("SUB_CL_ID"),  RTSHORT, 0,      RTLE,
                             RTLB, RTSTR, _T("KEY_ATTRIB"), RTLONG,  0,      RTLE,
                             RTLB, RTSTR, _T("HANDLE"),     RTSTR,   _T(""), RTLE,
                             RTLE, 0);

   // inizializzazione puntatori lista resbuf di lettura
   prb_Cls    = ColValues.CdrAssoc(_T("CLASS_ID"));
   prb_Sub    = ColValues.CdrAssoc(_T("SUB_CL_ID"));
   prb_Key    = ColValues.CdrAssoc(_T("KEY_ATTRIB"));
   prb_Handle = ColValues.CdrAssoc(_T("HANDLE"));
}


void C_LINK_SET::terminate(void)
{
   ColValues.remove_all();
}


/*********************************************************/
/*.doc C_LINK_SET::clear                      <external> */
/*+
  Esegue pulisce svuotando il link set.

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_LINK_SET::clear(void)
{
   cls = 0;
   sub = 0;
   KeyList.remove_all();
   SelSet.clear();

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_LINK_SET::save                       <external> */
/*+
  Salva il Link-Set su file.
  Parametri:
  const TCHAR *Path;  path del file

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_LINK_SET::save(const TCHAR *Path)
{
   FILE     *file;
   TCHAR    hand[MAX_LEN_HANDLE];
   long     i;
   ads_name ent;
   C_BLONG  *pKey;

   if ((file = gsc_fopen(Path, _T("w"))) == NULL) return GS_BAD; 

   // codice classe e sottoclasse
   if (fwprintf(file, _T("%d\n"), cls) < 0 || 
       fwprintf(file, _T("%d\n"), sub) < 0)
      { gsc_fclose(file); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // Scrivo gli handle degli oggetti grafici
   i = 0;
   while (SelSet.entname(i++, ent) == GS_GOOD)
   {
      if (gsc_enthand(ent, hand) == GS_GOOD)
         if (fwprintf(file, _T("%s\n"), hand) < 0)
            { gsc_fclose(file); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   }

   // Scrivo i codici delle entità
   if (fwprintf(file, _T("KEYLIST\n")) < 0)
      { gsc_fclose(file); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   pKey = (C_BLONG * ) KeyList.go_top();
   while (pKey)
   {
      if (fwprintf(file, _T("%ld\n"), pKey->get_key()) < 0)
         { gsc_fclose(file); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
      pKey = (C_BLONG *) KeyList.go_next();
   }

   gsc_fclose(file);

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_LINK_SET::load                       <external> */
/*+
  Carica il Link-Set da file.
  Parametri:
  const TCHAR *Path;  path del file

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_LINK_SET::load(const TCHAR *Path)
{
   FILE     *file;
   TCHAR    hand[MAX_LEN_HANDLE];
   long     Key;
   ads_name ent;

   SelSet.clear();
   KeyList.remove_all();

   if (gsc_path_exist(Path) == GS_BAD) return GS_GOOD;

   if ((file = gsc_fopen(Path, _T("r"))) == NULL) return GS_BAD; 

   if (fwscanf(file, _T("%d"), &cls) == EOF) // codice classe
      { gsc_fclose(file); GS_ERR_COD = eGSReadFile; return GS_BAD; }
   if (fwscanf(file, _T("%d"), &sub) == EOF) // codice sottoclasse
      { gsc_fclose(file); GS_ERR_COD = eGSReadFile; return GS_BAD; }

   // Leggo gli handle degli oggetti grafici
   while (fwscanf(file, _T("%s"), hand) != EOF && gsc_strcmp(hand, _T("KEYLIST")) != 0)
      if (acdbHandEnt(hand, ent) == RTNORM) SelSet.add(ent);

   // Leggo i codici delle entità
   while (fwscanf(file, _T("%ld"), &Key) != EOF)
      KeyList.add(&Key);

   gsc_fclose(file);

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_LINK_SET::copy                      <internal> */
/*+
  Esegue la copia del link set in un altro link set di destinazione.
  Parametri:
  C_LINK_SET &Dest;       linkset destinazione della copia

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_LINK_SET::copy(C_LINK_SET &Dest) 
{
   C_BLONG *pKey;
   long    Key;

   // copio lista di codici chiave
   Dest.KeyList.remove_all();
   pKey = (C_BLONG * ) KeyList.go_top();
   while (pKey)
   {
      Key = pKey->get_key();
      Dest.KeyList.add(&Key);
      pKey = (C_BLONG *) KeyList.go_next();
   }
   // copio il gruppo di selezione
   SelSet.copy(Dest.SelSet);

   Dest.cls = cls;
   Dest.sub = sub;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_LINK_SET::erase                      <external> */
/*+
  Questa funzione azzera nella tabella GS_LINK i codici dei
  corrispondenti handle degli oggetti grafici.
  Parametri:
  ads_name ss;               gruppo di selezione
  int      CounterToVideo;   flag, se = GS_GOOD stampa a video il numero di entità
                             che si stanno elaborando (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_LINK_SET::erase(C_SELSET &ss, int CounterToVideo)
{
   ads_name ss1;
   ss.get_selection(ss1);
   return erase(ss1, CounterToVideo);
}
int C_LINK_SET::erase(ads_name ss, int CounterToVideo)
{
   C_LINK   Link;
   long     i = 0, TotSS;
   ads_name entity;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(760)); // "Aggiornamento collegamenti grafica\\database..."

   if (ads_name_nil(ss) || ads_sslength(ss, &TotSS) != RTNORM || TotSS == 0) return GS_GOOD;

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.Init(TotSS);

   while (acedSSName(ss, i++, entity) == RTNORM)
   {
      if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.Set(i);
      if (Link.erase(entity) == GS_BAD) 
         { StatusBarProgressMeter.End(); return GS_BAD; }
   }

   if (CounterToVideo == GS_GOOD && --i > 0)
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

   return GS_GOOD;
}


/************************************************************************/
/*.doc C_LINK_SET::getLinkQty                                <internal> */
/*+                                                                       
  Questa funzione ritorna il numero di link del link set.
-*/  
/************************************************************************/
long C_LINK_SET::getLinkQty(void)
{
   return KeyList.get_count();
}


/*********************************************************/
/*.doc C_LINK_SET::GetSS                      <internal> */
/*+
  Questa funzione cerca il gruppo di selezione degli oggetti
  collegati ad un record nel database.
  Parametri:
  ads_name ent;   Oggetto grafico collegato ad un record
  C_SELSET &ss;   gruppo di selezione

  oppure:

  int      cls;   codice classe
  int      sub;   codice sottoclasse
  long     key;   valore chiave
  C_SELSET &ss;   gruppo di selezione

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_LINK_SET::GetSS(ads_name ent, C_SELSET &ss)
{
   C_LINK Link;
   int    cls, sub;
   long   key;
   
   Link.SetEnt(ent);
   if (Link.GetKey(&key, &cls, &sub) == GS_BAD) return GS_BAD;
   
   return GetSS(cls, sub, key, ss);
}
int C_LINK_SET::GetSS(int cls, int sub, long key, C_SELSET &ss)
{
   ads_name      ent;
   _RecordsetPtr pRs;

   ss.clear();

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if ((pRs = GS_CURRENT_WRK_SESSION->get_RsSelectLink()) != NULL) 
   {  // Uso metodo seek sui recordset che è molto più veloce
      SAFEARRAY FAR* psa = NULL; // Creo un safearray che prende 3 elementi
      SAFEARRAYBOUND rgsabound;
      _variant_t     var, KeyForSeek;
      int            val_Cls, val_Sub;
      long           ix, val_Key;

      rgsabound.lLbound   = 0;
      rgsabound.cElements = 3;
      psa = SafeArrayCreate(VT_VARIANT, 1, &rgsabound);

      // CODICE CLASSE
      ix  = 0;
      gsc_set_variant_t(var, (short) cls);
      SafeArrayPutElement(psa, &ix, &var);

      // CODICE SOTTOCLASSE
      ix  = 1;
      gsc_set_variant_t(var, (short) sub);
      SafeArrayPutElement(psa, &ix, &var);

      // CODICE CHIAVE
      ix  = 2;
      gsc_set_variant_t(var, key);
      SafeArrayPutElement(psa, &ix, &var);

      KeyForSeek.vt = VT_ARRAY|VT_VARIANT;
      KeyForSeek.parray = psa;  
      
      if (gsc_DBSetIndexRs(pRs, _T("ENTITYCODE")) == GS_BAD ||
          gsc_DBSeekRs(pRs, KeyForSeek, adSeekFirstEQ) == GS_BAD)
         return GS_BAD;

      while (gsc_isEOF(pRs) == GS_BAD)
      {
         // lettura record
         if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) return GS_BAD;

         if ((gsc_rb2Int(prb_Cls, &val_Cls) == GS_BAD || val_Cls != cls) ||
             (gsc_rb2Int(prb_Sub, &val_Sub) == GS_BAD || val_Sub != sub) ||
             (gsc_rb2Lng(prb_Key, &val_Key) == GS_BAD || val_Key != key))
            break;

         if (acdbHandEnt(prb_Handle->resval.rstring, ent) == RTNORM &&
             gsc_IsErasedEnt(ent) == GS_BAD)
            if (ss.add(ent) == GS_BAD) return GS_BAD;

         gsc_Skip(pRs);
      }
   }
   else
   {
      _CommandPtr pCmd;

      if ((pCmd = GS_CURRENT_WRK_SESSION->get_CmdSelectLinkWhereKey()) == NULL) return GS_BAD;

      // imposto come parametro il codice classe
      if (gsc_SetDBParam(pCmd, 0, cls) == GS_BAD) return GS_BAD;
      // imposto come parametro il codice sottoclasse
      if (gsc_SetDBParam(pCmd, 1, sub) == GS_BAD) return GS_BAD;
      // imposto come parametro il valore chiave
      if (gsc_SetDBParam(pCmd, 2, key) == GS_BAD) return GS_BAD;

      // Esegue l'istruzione SQL
      if (gsc_ExeCmd(pCmd, pRs) == GS_BAD) return GS_BAD;

      while (gsc_isEOF(pRs) == GS_BAD)
      {  // lettura record
         if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
            { gsc_DBCloseRs(pRs); return GS_BAD; }

         if (acdbHandEnt(prb_Handle->resval.rstring, ent) == RTNORM &&
             gsc_IsErasedEnt(ent) == GS_BAD)
            if (ss.add(ent) == GS_BAD) 
               { gsc_DBCloseRs(pRs); return GS_BAD; }

         gsc_Skip(pRs);
      }

      gsc_DBCloseRs(pRs);
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_LINK_SET::GetHandleList              <internal> */
/*+
  Questa funzione restituisce una lista di handle degli oggetti grafici.
  Parametri:
  int      cls;            codice classe
  int      sub;            codice sottoclasse
  long     key;            valore chiave
  C_STR_LIST &HandleList;  Lista di handle
  int      What;           Tipi di oggetti da cercare (default ALL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_LINK_SET::GetHandleList(int cls, int sub, long key, C_STR_LIST &HandleList,
                              int What)
{
   _RecordsetPtr pRs;
   C_STR         *pHandle;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   HandleList.remove_all();

   if ((pRs = GS_CURRENT_WRK_SESSION->get_RsSelectLink()) != NULL) 
   {
      SAFEARRAY FAR* psa = NULL; // Creo un safearray che prende 3 elementi
      SAFEARRAYBOUND rgsabound;
      _variant_t     var, KeyForSeek;
      int            val_Cls, val_Sub;
      long           ix, val_Key;

      rgsabound.lLbound   = 0;
      rgsabound.cElements = 3;
      psa = SafeArrayCreate(VT_VARIANT, 1, &rgsabound);

      // CODICE CLASSE
      ix  = 0;
      gsc_set_variant_t(var, (short) cls);
      SafeArrayPutElement(psa, &ix, &var);

      // CODICE SOTTOCLASSE
      ix  = 1;
      gsc_set_variant_t(var, (short) sub);
      SafeArrayPutElement(psa, &ix, &var);

      // CODICE CHIAVE
      ix  = 2;
      gsc_set_variant_t(var, key);
      SafeArrayPutElement(psa, &ix, &var);

      KeyForSeek.vt = VT_ARRAY|VT_VARIANT;
      KeyForSeek.parray = psa;  
      
      // Uso tecnica dei recordset molto più veloce
      if (gsc_DBSetIndexRs(pRs, _T("ENTITYCODE")) == GS_BAD ||
          gsc_DBSeekRs(pRs, KeyForSeek, adSeekFirstEQ) == GS_BAD)
         return GS_BAD;

      while (gsc_isEOF(pRs) == GS_BAD)
      {
         // lettura record
         if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) return GS_BAD;

         if ((gsc_rb2Int(prb_Cls, &val_Cls) == GS_BAD || val_Cls != cls) ||
             (gsc_rb2Int(prb_Sub, &val_Sub) == GS_BAD || val_Sub != sub) ||
             (gsc_rb2Lng(prb_Key, &val_Key) == GS_BAD || val_Key != key))
            break;

         if ((pHandle = new C_STR(prb_Handle->resval.rstring)) == NULL) return GS_BAD;
         HandleList.add_tail(pHandle);

         gsc_Skip(pRs);
      }
   }
   else
   {
      _CommandPtr pCmd;

      if ((pCmd = GS_CURRENT_WRK_SESSION->get_CmdSelectLinkWhereKey()) == NULL) return GS_BAD;

      // imposto come parametro il codice classe
      if (gsc_SetDBParam(pCmd, 0, cls) == GS_BAD) return GS_BAD;
      // imposto come parametro il codice sottoclasse
      if (gsc_SetDBParam(pCmd, 1, sub) == GS_BAD) return GS_BAD;
      // imposto come parametro il valore chiave
      if (gsc_SetDBParam(pCmd, 2, key) == GS_BAD) return GS_BAD;

      // Esegue l'istruzione SQL
      if (gsc_ExeCmd(pCmd, pRs) == GS_BAD) return GS_BAD;

      while (gsc_isEOF(pRs) == GS_BAD)
      {  // lettura record
         if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
            { gsc_DBCloseRs(pRs); return GS_BAD; }

         if ((pHandle = new C_STR(prb_Handle->resval.rstring)) == NULL)
            { gsc_DBCloseRs(pRs); return GS_BAD; }
         HandleList.add_tail(pHandle);

         gsc_Skip(pRs);
      }

      gsc_DBCloseRs(pRs);
   }

   if (What != ALL)
   {
      ads_name ent;
      bool     ToErase;

      pHandle = (C_STR *) HandleList.get_head();
      while (pHandle)
      {
         if (acdbHandEnt(pHandle->get_name(), ent) == RTNORM)
         {
            if (gsc_IsErasedEnt(ent) == GS_GOOD)
            {
               gsc_UnEraseEnt(ent);
               ToErase = true;
            }
            else
               ToErase = false;

            if (What == GRAPHICAL) // solo oggetti principali
            {
               if (gsc_is_DABlock(ent) == GS_GOOD) HandleList.remove_at();
               else HandleList.get_next();
            }
            else // solo blocchi di attributi visibili
               if (gsc_is_DABlock(ent) != GS_GOOD) HandleList.remove_at();
               else HandleList.get_next();

            if (ToErase) gsc_EraseEnt(ent);
         }
         else
            HandleList.remove_at(); // se non so cos'è la cancello

         pHandle = (C_STR *) HandleList.get_cursor();
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_LINK_SET::GetGSClassSS               <internal> */
/*+
  Questa funzione cerca il gruppo di selezione degli oggetti
  collegati ad una classe GEOsim.
  Parametri:
  int      cls;           codice classe
  int      sub;           codice sottoclasse
  C_SELSET &ss;           gruppo di selezione
  C_LONG_BTREE *pKeyList; opzionale, lista dei codici delle entità
  int      ObjType;       Tipi di oggetti da cercare (default GRAPHICAL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_LINK_SET::GetGSClassSS(int cls, int sub, C_SELSET &ss, C_LONG_BTREE *pKeyList,
                             int ObjType)
{
   ads_name      ent;
   _RecordsetPtr pRs;
   _CommandPtr   pCmd;
   long          Key;
   C_CLASS       *pCls;

   ss.clear();
   if (pKeyList) pKeyList->remove_all();

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(cls, sub)) == NULL) return GS_BAD;
   
   // Per le classi griglia
   if (pCls->get_category() == CAT_GRID)
   {
      if (pKeyList)
      {
         C_GRID *pGrid = ((C_CGRID *) pCls)->ptr_grid();
         C_RECT Rect;

         // Estensioni della griglia sottraendo un piccolo offset all'angolo alto-destro
         pGrid->getExtension(Rect, true);
         if (pGrid->getKeyListInWindow(Rect, CROSSING, *pKeyList) == GS_BAD)
            return GS_BAD;
      }

      return GS_GOOD;
   }

   if ((pCmd = GS_CURRENT_WRK_SESSION->get_CmdSelectLinkWhereCls()) == NULL) return GS_BAD;  

   // imposto come parametro il codice classe
   if (gsc_SetDBParam(pCmd, 0, cls) == GS_BAD) return GS_BAD;
   // imposto come parametro il codice sottoclasse
   if (gsc_SetDBParam(pCmd, 1, sub) == GS_BAD) return GS_BAD;

   // Esegue l'istruzione SQL
   if (gsc_ExeCmd(pCmd, pRs) == GS_BAD) return GS_BAD;

   while (gsc_isEOF(pRs) == GS_BAD)
   {  // lettura record
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
         { gsc_DBCloseRs(pRs); return GS_BAD; }

      if (acdbHandEnt(prb_Handle->resval.rstring, ent) == RTNORM)
         // solo se oggetto grafico esistente
         if (gsc_IsErasedEnt(ent) == GS_BAD)
         {
            if (ObjType == GRAPHICAL) // solo oggetti principali
            {
               if (gsc_is_DABlock(ent) == GS_GOOD) { gsc_Skip(pRs); continue; }
            }
            else if (ObjType == DA_BLOCK) // solo blocchi di attributi visibili
               if (gsc_is_DABlock(ent) != GS_GOOD) { gsc_Skip(pRs); continue; }

            if (ss.add(ent) == GS_BAD) 
               { gsc_DBCloseRs(pRs); return GS_BAD; }
            if (pKeyList)
               if (gsc_rb2Lng(prb_Key, &Key) == GS_GOOD) KeyList.add(&Key);
         }

      gsc_Skip(pRs);
   }

   return gsc_DBCloseRs(pRs);
}


/*********************************************************/
/*.doc C_LINK_SET::GetCountClassObjs          <internal> */
/*+
  Questa funzione conta quanti sono gli oggtti grafici collegati 
  ad una classe GEOsim.
  Parametri:
  int      cls;  codice classe
  int      sub;  codice sottoclasse

  Restituisce il conteggio in caso di successo altrimenti restituisce -1.
-*/  
/*********************************************************/
long C_LINK_SET::GetCountClassObjs(int cls, int sub)
{
   C_DBCONNECTION *pConn;
   C_STRING       TableRef;
   _RecordsetPtr  pRs;
   C_RB_LIST      CountColValues;
   long           n = 0;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return -1; }

   C_STRING      statement;
   _ParameterPtr pParam;

   if (GS_CURRENT_WRK_SESSION->getLinksTabInfo(&pConn, &TableRef) == GS_BAD) return -1;

   statement = _T("SELECT COUNT(HANDLE) FROM");
   statement += TableRef;
   statement += _T(" WHERE CLASS_ID=");
   statement += cls;
   statement += _T("AND SUB_CL_ID=");
   statement += sub;

   if (pConn->ExeCmd(statement, pRs) == GS_BAD) return -1;
   if (gsc_isEOF(pRs) == GS_GOOD)
      { gsc_DBCloseRs(pRs); return -1; }
   // lettura record
   if (gsc_DBReadRow(pRs, CountColValues) == GS_BAD)
      { gsc_DBCloseRs(pRs); return -1; }
   if (gsc_rb2Lng(gsc_nth(1, CountColValues.nth(0)), &n) == GS_BAD)
      { gsc_DBCloseRs(pRs); return -1; }

   gsc_DBCloseRs(pRs);

   return n;
}


/*********************************************************/
/*.doc C_LINK_SET::AddEntFromSQL              <internal> */
/*+
  Questa funzione di appoggio a "initSelSQLCond" per la ricerca di
  codici di entità e relativi oggetti grafici attraverso una 
  istruzione SQL. La funzione aggiunge al linkset i risultati trovati.
  Parametri:
  C_NODE      *pCls;       Puntatore alla classe di GEOsim
  CAsiSession *pSession;   Sessione ASI
  const TCHAR  *statement;  Istrizione per la ricerca SQL

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_LINK_SET::AddEntFromSQL(C_NODE *pCls, CAsiSession *pSession, const TCHAR *statement)
{
   CAsiExecStm *pStm;
   CAsiCsr     *pCsr;
   CAsiRow     *pRow;
   CAsiData    *pData;
   resbuf      rbKey;
   long        Key;
   C_SELSET    dummy;

   if (gsc_ASIPrepareSql(pSession, statement, _T("CsrName"), &pStm, &pCsr,
                         NULL, NULL, NULL, GS_BAD) != GS_GOOD) // senza visualizzare msg di errore
   {
      // Purtroppo se la condizione di fltro ricade su campi di tipo date
      // la compilazione x ACCESS non funziona perchè access non ha il tipo date
      // ma solo timestamp (CAMPO = DATE '2013-03-06' non andrebbe).
      // Allora dove trovo DATE '...' lo sostituisco con TIMESTAMP '... 00:00:00'
      C_STRING AccessStm, dummy(statement);

      AccessStm = gsc_AdjSyntaxDateToTimestampMAPFormat(dummy);
      AccessStm = gsc_AdjSyntaxTimeToTimestampMAPFormat(AccessStm);       

      if (gsc_ASIPrepareSql(pSession, AccessStm.get_name(), _T("CsrName"), &pStm, &pCsr) != GS_GOOD)
         return GS_BAD;
   }

   if (pCsr->Open() == kAsiGood)
   {
      while (pCsr->Fetch() == kAsiGood)
         if ((pRow = pCsr->getCurrentRow()) &&
             (pData = (*pRow)[0].getData()) &&
             gsc_ASIData2Rb(pData, &rbKey) == GS_GOOD &&
             gsc_rb2Lng(&rbKey, &Key) == GS_GOOD)
         {
            // Ottengo gruppo di selezione
            if (((C_CLASS *) pCls)->get_SelSet(Key, dummy) == GS_BAD) continue;
            // Aggiungo codice chiave ala lista
            KeyList.add(&Key);
            SelSet.add_selset(dummy);
         }

      pCsr->Close();
   }

   delete pCsr;
   gsc_ASITermStm(&pStm);

   return GS_GOOD;
}


/*************************************************************/
/*.doc C_LINK_SET::getApproxTimeForInitSelSQLCond <internal> */
/*+
  Questa funzione stima il tempo che impiegherà la initSelSQLCond.
  Da usare insieme alla "getApproxTimeForInitSQLCond" per confrontare
  i tempi di ricerca tra e tecniche possibili.
  Parametri:
  long Qty;    n di oggetti grafici

  Ritorna il numero di secondi in caso di successo altrimenti -1.
-*/  
/*************************************************************/
double C_LINK_SET::getApproxTimeForInitSelSQLCond(long Qty)
{
   // Con 70000 oggetti grafici si impiegano 473 sec.
   // quindi il tempo per la tecnica 2 è = n.ogg. * 473 / 70000
   return Qty * 473 / 70000;
}


/*********************************************************/
/*.doc C_LINK_SET::initSelSQLCond             <internal> */
/*+
  Questa funzione inizializza un linkset tramite una condizione SQL
  da applicare ad un gruppo di oggetti di una classe nota. Viene inizializzato un 
  gruppo di selezione e una lista di codici chiave.
  Parametri:
  C_NODE     *pCls;     Puntatore alla classe di GEOsim
  C_SELSET   &_SelSet;  Gruppo di oggetti grafici
  const TCHAR *Cond     Condizione SQL 
  int      ObjType;     Tipi di oggetti da cercare (default GRAPHICAL)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_LINK_SET::initSelSQLCond(C_NODE *pCls, C_SELSET &_SelSet, const TCHAR *Cond,
                               int ObjType)
{
   C_DBCONNECTION *pConn;
   C_ID           *pID = ((C_CLASS *) pCls)->ptr_id();  
   C_INFO         *pInfo = ((C_CLASS *) pCls)->ptr_info();  
   C_STRING       TmpStatement, OldStatemet, TmpTableRef, UDL, MapTableRef;
   C_STRING       PrefixTmpStatement, PrefixOldStatemet, KeyAttribCorrected;
   bool           CheckTempTable = FALSE;
   int            IsRsCloseable, nMaxCond = 20, nTmpCond = 0, nOldCond = 0;
   long           i = 0, Key;
   ads_name       ent;
   C_PREPARED_CMD pTmpCmd;
   CAsiSession    *pTmpSession, *pOldSession;
   _RecordsetPtr  pTmpRs;
   C_EED          eed;

#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2;
   double tempo1=0, tempo2=0, tempo3=0, tempo4=0, tempo5=0;
   _ftime(&t1);
#endif

   SelSet.clear();
   KeyList.remove_all();

   if (gsc_strlen(Cond) == 0)
   {
      C_LINK Link;
      
      _SelSet.copyIntersectClsCode(SelSet, pID->code, pID->sub_code);
      if (ObjType != ALL) SelSet.intersectType(ObjType);

      while (SelSet.entname(i++, ent) == GS_GOOD)
      {
         Link.SetEnt(ent);
         if (Link.GetKey(&Key) == GS_GOOD) KeyList.add(&Key);
      }

      return GS_GOOD;
   }

   // Correggo la sintassi del nome del campo per SQL MAP
   KeyAttribCorrected = pInfo->key_attrib;
   gsc_AdjSyntaxMAPFormat(KeyAttribCorrected);

   // se esiste già la tabella temporanea
   if ((pConn = pInfo->getDBConnection(TEMP)) == NULL) return GS_BAD;
   if (((C_CLASS *) pCls)->getTempTableRef(TmpTableRef, GS_BAD) == GS_GOOD &&
       pConn->ExistTable(TmpTableRef) == GS_GOOD)
   {

      // preparo la struttura per la ricerca di un record nel temporaneo
      if (((C_CLASS *) pCls)->prepare_data(pTmpCmd, TEMP) == GS_BAD)
         return GS_BAD;

      // preparo la ricerca dei codici dei record che soddisfano la condizione SQL
      // per la tabella TEMP
      if (gsc_Table2MAPFormat(pConn, TmpTableRef, MapTableRef) == GS_BAD) return GS_BAD;

      PrefixTmpStatement = _T("SELECT ");
      PrefixTmpStatement += KeyAttribCorrected;
      PrefixTmpStatement += _T(" FROM ");
      PrefixTmpStatement += MapTableRef;
      PrefixTmpStatement += _T(" WHERE (");
      PrefixTmpStatement += Cond;
      PrefixTmpStatement += _T(") AND ");
      PrefixTmpStatement += KeyAttribCorrected;
      PrefixTmpStatement += _T(" IN (");

      CheckTempTable = TRUE;
      // Preparo se necessario il file UDL
      if (((C_CLASS *) pCls)->getLPNameTemp(UDL) == GS_BAD) return GS_BAD;
      if (gsc_SetACADUDLFile(UDL.get_name(), pConn, TmpTableRef.get_name()) == GS_BAD)
         return GS_BAD;
      // Creo una sessione ASI utilizzando il file UDL
      if ((pTmpSession = gsc_ASICreateSession(UDL.get_name())) == NULL) return GS_BAD;
   }

   // preparo la ricerca dei codici dei record che soddisfano la condizione SQL
   // per la tabella OLD
   if ((pConn = pInfo->getDBConnection(OLD)) == NULL)
      { gsc_ASITermSession(&pTmpSession); return GS_BAD; }

   if (gsc_Table2MAPFormat(pConn, pInfo->OldTableRef, MapTableRef) == GS_BAD)
      { gsc_ASITermSession(&pTmpSession); return GS_BAD; }

   PrefixOldStatemet = _T("SELECT ");
   PrefixOldStatemet += KeyAttribCorrected;
   PrefixOldStatemet += _T(" FROM ");
   PrefixOldStatemet += MapTableRef;
   PrefixOldStatemet += _T(" WHERE (");
   PrefixOldStatemet += Cond;
   PrefixOldStatemet += _T(") AND ");
   PrefixOldStatemet += KeyAttribCorrected;
   PrefixOldStatemet += _T(" IN (");

   if ((pConn = pInfo->getDBConnection(OLD)) == NULL)
      { gsc_ASITermSession(&pTmpSession); return GS_BAD; }
   // Preparo se necessario il file UDL
   if (((C_CLASS *) pCls)->getLPNameOld(UDL) == GS_BAD)
      { gsc_ASITermSession(&pTmpSession); return GS_BAD; }
   if (gsc_SetACADUDLFile(UDL.get_name(), pConn, pInfo->OldTableRef.get_name()) == GS_BAD)
      { gsc_ASITermSession(&pTmpSession); return GS_BAD; }
   // Creo una sessione ASI utilizzando il file UDL
   if ((pOldSession = gsc_ASICreateSession(UDL.get_name())) == NULL)
      { gsc_ASITermSession(&pTmpSession); return GS_BAD; }

   // Ciclo sugli oggetti grafici
   while (_SelSet.entname(i++, ent) == GS_GOOD)
   {
      // solo della classe di interesse
      if (eed.load(ent) == GS_BAD || eed.cls != pID->code || eed.sub != pID->sub_code)
         continue;

      if (ObjType == GRAPHICAL) // solo oggetti principali
      {
         if (gsc_is_DABlock(ent) == GS_GOOD) continue;
      }
      else if (ObjType == DA_BLOCK) // solo blocchi di attributi visibili
         if (gsc_is_DABlock(ent) != GS_GOOD) continue;

      // Ottengo codice chiave
      if (((C_CLASS *) pCls)->getKeyValue(ent, &Key) == GS_BAD) continue;
      
      // Verifico se la scheda è nel Temp
      if (CheckTempTable && gsc_get_data(pTmpCmd, Key, pTmpRs, &IsRsCloseable) == GS_GOOD)
      {
         if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pTmpRs);

         if (nTmpCond == 0) TmpStatement = PrefixTmpStatement;         
         else TmpStatement += _T(",");
         TmpStatement += Key;
         nTmpCond++;

         // Se è stato raggiunto il limite di codici da verificare
         if (nTmpCond == nMaxCond)
         {
            TmpStatement += _T(")");
            nTmpCond = 0;
            // applico SQL
            AddEntFromSQL(pCls, pTmpSession, TmpStatement.get_name());
         }
      }
      else // è nell'Old
      {
         if (nOldCond == 0) OldStatemet = PrefixOldStatemet;         
         else OldStatemet += _T(",");
         OldStatemet += Key;
         nOldCond++;

         // Se è stato raggiunto il limite di codici da verificare
         if (nOldCond == nMaxCond)
         {
            OldStatemet += _T(")");
            nOldCond = 0;
            // applico SQL
            AddEntFromSQL(pCls, pOldSession, OldStatemet.get_name());
         }
      }
   }

   // Se ci sono ancora dei codici da verificare nel Temp
   if (nTmpCond > 0)
   {
      TmpStatement += _T(")");
      // applico SQL
      AddEntFromSQL(pCls, pTmpSession, TmpStatement.get_name());
   }

   // Se ci sono ancora dei codici da verificare nell'Old
   if (nOldCond > 0)
   {
      OldStatemet += _T(")");
      // applico SQL
      AddEntFromSQL(pCls, pOldSession, OldStatemet.get_name());
   }

   if (CheckTempTable) gsc_ASITermSession(&pTmpSession);
   gsc_ASITermSession(&pOldSession);     

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
   acutPrintf(_T("initSelSQLCond impiega %6.2f secondi.\n"), (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000) );
#endif

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_LINK_SET::initSelSQLCond             <internal> */
/*+
  Questa funzione inizializza un linkset tramite una condizione SQL
  da applicare ad una lista di codici di entità di una classe nota.
  Viene inizializzato un gruppo di selezione e una lista di codici chiave.
  Parametri:
  C_NODE       *pCls;     Puntatore alla classe di GEOsim
  C_LONG_BTREE &_KeyList; Lista di codici di entità
  const TCHAR  *Cond      Condizione SQL 
  int          ObjType;   Tipi di oggetti da cercare (default GRAPHICAL)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_LINK_SET::initSelSQLCond(C_NODE *pCls, C_LONG_BTREE &_KeyList, const TCHAR *Cond,
                               int ObjType)
{
   C_DBCONNECTION *pConn;
   C_ID           *pID = ((C_CLASS *) pCls)->ptr_id();  
   C_INFO         *pInfo = ((C_CLASS *) pCls)->ptr_info();
   C_FAS          *pFAS = ((C_CLASS *) pCls)->ptr_fas();
   C_STRING       TmpStatement, OldStatemet, TmpTableRef, UDL, MapTableRef;
   C_STRING       PrefixTmpStatement, PrefixOldStatemet, KeyAttribCorrected;
   bool           CheckTempTable = FALSE;
   int            IsRsCloseable, nMaxCond = 20, nTmpCond = 0, nOldCond = 0;
   long           i = 0;
   C_PREPARED_CMD pTmpCmd;
   CAsiSession    *pTmpSession, *pOldSession;
   _RecordsetPtr  pTmpRs;
   C_SELSET       _SelSet;
   C_BLONG        *pKey;

   SelSet.clear();
   KeyList.remove_all();

   if (gsc_strlen(Cond) == 0)
   {     
      KeyList.add_list(_KeyList);

      // se la classe ha grafica
      if (pFAS)
      {
         pKey = (C_BLONG *) _KeyList.go_top();
         while (pKey)
         {
            // se la classe ha grafica
            if (GetSS(pID->code, pID->sub_code, pKey->get_key(), _SelSet) == GS_GOOD)
               SelSet.add_selset(_SelSet);

            pKey = (C_BLONG *) _KeyList.go_next();
         }

         if (ObjType != ALL) SelSet.intersectType(ObjType);
      }

      return GS_GOOD;
   }

   // Correggo la sintassi del nome del campo per SQL MAP
   KeyAttribCorrected = pInfo->key_attrib;
   gsc_AdjSyntaxMAPFormat(KeyAttribCorrected);

   // se esiste già la tabella temporanea
   if ((pConn = pInfo->getDBConnection(TEMP)) == NULL) return GS_BAD;
   if (((C_CLASS *) pCls)->getTempTableRef(TmpTableRef, GS_BAD) == GS_GOOD &&
       pConn->ExistTable(TmpTableRef) == GS_GOOD)
   {

      // preparo la struttura per la ricerca di un record nel temporaneo
      if (((C_CLASS *) pCls)->prepare_data(pTmpCmd, TEMP) == GS_BAD)
         return GS_BAD;

      // preparo la ricerca dei codici dei record che soddisfano la condizione SQL
      // per la tabella TEMP
      if (gsc_Table2MAPFormat(pConn, TmpTableRef, MapTableRef) == GS_BAD) return GS_BAD;

      PrefixTmpStatement = _T("SELECT ");
      PrefixTmpStatement += KeyAttribCorrected;
      PrefixTmpStatement += _T(" FROM ");
      PrefixTmpStatement += MapTableRef;
      PrefixTmpStatement += _T(" WHERE (");
      PrefixTmpStatement += Cond;
      PrefixTmpStatement += _T(") AND ");
      PrefixTmpStatement += KeyAttribCorrected;
      PrefixTmpStatement += _T(" IN (");

      CheckTempTable = TRUE;
      // Preparo se necessario il file UDL
      if (((C_CLASS *) pCls)->getLPNameTemp(UDL) == GS_BAD) return GS_BAD;
      if (gsc_SetACADUDLFile(UDL.get_name(), pConn, TmpTableRef.get_name()) == GS_BAD)
         return GS_BAD;
      // Creo una sessione ASI utilizzando il file UDL
      if ((pTmpSession = gsc_ASICreateSession(UDL.get_name())) == NULL) return GS_BAD;
   }

   // preparo la ricerca dei codici dei record che soddisfano la condizione SQL
   // per la tabella OLD
   if ((pConn = pInfo->getDBConnection(OLD)) == NULL)
      { gsc_ASITermSession(&pTmpSession); return GS_BAD; }

   if (gsc_Table2MAPFormat(pConn, pInfo->OldTableRef, MapTableRef) == GS_BAD)
      { gsc_ASITermSession(&pTmpSession); return GS_BAD; }

   PrefixOldStatemet = _T("SELECT ");
   PrefixOldStatemet += KeyAttribCorrected;
   PrefixOldStatemet += _T(" FROM ");
   PrefixOldStatemet += MapTableRef;
   PrefixOldStatemet += _T(" WHERE (");
   PrefixOldStatemet += Cond;
   PrefixOldStatemet += _T(") AND ");
   PrefixOldStatemet += KeyAttribCorrected;
   PrefixOldStatemet += _T(" IN (");

   if ((pConn = pInfo->getDBConnection(OLD)) == NULL)
      { gsc_ASITermSession(&pTmpSession); return GS_BAD; }
   // Preparo se necessario il file UDL
   if (((C_CLASS *) pCls)->getLPNameOld(UDL) == GS_BAD)
      { gsc_ASITermSession(&pTmpSession); return GS_BAD; }
   if (gsc_SetACADUDLFile(UDL.get_name(), pConn, pInfo->OldTableRef.get_name()) == GS_BAD)
      { gsc_ASITermSession(&pTmpSession); return GS_BAD; }
   // Creo una sessione ASI utilizzando il file UDL
   if ((pOldSession = gsc_ASICreateSession(UDL.get_name())) == NULL)
      { gsc_ASITermSession(&pTmpSession); return GS_BAD; }

   // Ciclo sulla lista dei codici
   pKey = (C_BLONG *) _KeyList.go_top();
   while (pKey)
   {     
      // Verifico se la scheda è nel Temp
      if (CheckTempTable && gsc_get_data(pTmpCmd, pKey->get_key(), pTmpRs, &IsRsCloseable) == GS_GOOD)
      {
         if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pTmpRs);

         if (nTmpCond == 0) TmpStatement = PrefixTmpStatement;         
         else TmpStatement += _T(",");
         TmpStatement += pKey->get_key();
         nTmpCond++;

         // Se è stato raggiunto il limite di codici da verificare
         if (nTmpCond == nMaxCond)
         {
            TmpStatement += _T(")");
            nTmpCond = 0;
            // applico SQL
            AddEntFromSQL(pCls, pTmpSession, TmpStatement.get_name());
         }
      }
      else // è nell'Old
      {
         if (nOldCond == 0) OldStatemet = PrefixOldStatemet;         
         else OldStatemet += _T(",");
         OldStatemet += pKey->get_key();
         nOldCond++;

         // Se è stato raggiunto il limite di codici da verificare
         if (nOldCond == nMaxCond)
         {
            OldStatemet += _T(")");
            nOldCond = 0;
            // applico SQL
            AddEntFromSQL(pCls, pOldSession, OldStatemet.get_name());
         }
      }

      pKey = (C_BLONG *) _KeyList.go_next();
   }

   // Se ci sono ancora dei codici da verificare nel Temp
   if (nTmpCond > 0)
   {
      TmpStatement += _T(")");
      // applico SQL
      AddEntFromSQL(pCls, pTmpSession, TmpStatement.get_name());
   }

   // Se ci sono ancora dei codici da verificare nell'Old
   if (nOldCond > 0)
   {
      OldStatemet += _T(")");
      // applico SQL
      AddEntFromSQL(pCls, pOldSession, OldStatemet.get_name());
   }

   if (CheckTempTable) gsc_ASITermSession(&pTmpSession);
   gsc_ASITermSession(&pOldSession);     

   if (ObjType != ALL) SelSet.intersectType(ObjType);

   return GS_GOOD;
}


/**********************************************************/
/*.doc C_LINK_SET::getApproxTimeForInitSQLCond <internal> */
/*+
  Questa funzione stima il tempo che impiegherà la initSQLCond.
  Da usare insieme alla "getApproxTimeForInitSelSQLCond" per confrontare
  i tempi di ricerca tra e tecniche possibili.

  Parametri:
  C_NODE     *pCls;     Puntatore alla classe di GEOsim
  const TCHAR *Cond      Condizione SQL 

  Ritorna il numero di secondi in caso di successo altrimenti -1.
-*/  
/**********************************************************/
double C_LINK_SET::getApproxTimeForInitSQLCond(C_NODE *pCls, const TCHAR *Cond)
{
   C_DBCONNECTION *pConn;
   C_INFO         *pInfo = ((C_CLASS *) pCls)->ptr_info();  
   C_STRING       statement, TmpTableRef, UDL, MapTableRef, KeyAttribCorrected;
   resbuf         rbRec;
   int            Result = GS_GOOD;
   CAsiExecStm    *pExecStm;
   CAsiSession    *pSession;
   CAsiCsr        *pCsr;
   CAsiData       *pData;
   CAsiRow        *pRow;
   long           nRecTmp = 0, nRecOld = 0;

#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2;
   double tempo1=0;
   _ftime(&t1);
#endif

   // Correggo la sintassi del nome del campo per SQL MAP
   KeyAttribCorrected = pInfo->key_attrib;
   gsc_AdjSyntaxMAPFormat(KeyAttribCorrected);

   // Si conta quanti record sodisfano la cond SQL (su TEMP e OLD)
   // Con 70000 record che soddisfano la condizione SQL si impiegano 137 sec.
   // quindi il tempo per la tecnica 1 è = n.record * 137 / 70000
   
   // se esiste già la tabella temporanea
   if ((pConn = pInfo->getDBConnection(TEMP)) == NULL) return GS_BAD;
   if (((C_CLASS *) pCls)->getTempTableRef(TmpTableRef, GS_BAD) == GS_GOOD &&
       pConn->ExistTable(TmpTableRef) == GS_GOOD)
   {
      // preparo la ricerca dei codici dei record che soddisfano la condizione SQL
      // per la tabella TEMP
      if (gsc_Table2MAPFormat(pConn, TmpTableRef, MapTableRef) == GS_BAD) return GS_BAD;

      statement = _T("SELECT COUNT(");
      statement += KeyAttribCorrected;
      statement += _T(") FROM ");
      statement += MapTableRef;
      statement += _T(" WHERE ");
      statement += Cond;

      // Preparo se necessario il file UDL
      if (((C_CLASS *) pCls)->getLPNameTemp(UDL) == GS_BAD) return GS_BAD;
      if (gsc_SetACADUDLFile(UDL.get_name(), pConn, TmpTableRef.get_name()) == GS_BAD)
         return GS_BAD;
      // Creo una sessione ASI utilizzando il file UDL
      if ((pSession = gsc_ASICreateSession(UDL.get_name())) == NULL) return GS_BAD;
      if (gsc_ASIPrepareSql(pSession, statement.get_name(), UDL.get_name(),
                            &pExecStm, &pCsr,
                            NULL, NULL, NULL, GS_BAD) == GS_BAD) // senza visualizzare msg di errore
      {
         // Purtroppo se la condizione di fltro ricade su campi di tipo date
         // la compilazione x ACCESS non funziona perchè access non ha il tipo date
         // ma solo timestamp (CAMPO = DATE '2013-03-06' non andrebbe).
         // Allora dove trovo DATE '...' lo sostituisco con TIMESTAMP '... 00:00:00'
         C_STRING AccessStm;

         AccessStm = gsc_AdjSyntaxDateToTimestampMAPFormat(statement);
         AccessStm = gsc_AdjSyntaxTimeToTimestampMAPFormat(AccessStm);       

         if (gsc_ASIPrepareSql(pSession, AccessStm.get_name(), UDL.get_name(),
                               &pExecStm, &pCsr) == GS_BAD)
            { gsc_ASITermSession(&pSession); return GS_BAD; }
      }

      // Apro cursore
      if (pCsr->Open() != kAsiGood)
      { 
         delete pCsr;
         gsc_ASITermStm(&pExecStm);
         gsc_ASITermSession(&pSession);
         return GS_BAD;
      }
      // leggo la riga      
      if (pCsr->Fetch() != kAsiGood ||
          (pRow = pCsr->getCurrentRow()) == NULL ||
          (pData = (*pRow)[0].getData()) == NULL ||
          gsc_ASIData2Rb(pData, &rbRec) == GS_BAD ||
          gsc_rb2Lng(&rbRec, &nRecTmp) == GS_BAD)
      { 
         delete pCsr;
         gsc_ASITermStm(&pExecStm);
         gsc_ASITermSession(&pSession);
         GS_ERR_COD = eGSReadRow;
         return GS_BAD;
      }

      pCsr->Close();
      delete pCsr;
      gsc_ASITermStm(&pExecStm);
      gsc_ASITermSession(&pSession);
   }

   // preparo la ricerca dei codici dei record che soddisfano la condizione SQL
   // per la tabella OLD
   if ((pConn = pInfo->getDBConnection(OLD)) == NULL) return GS_BAD;

   if (gsc_Table2MAPFormat(pConn, pInfo->OldTableRef, MapTableRef) == GS_BAD) return GS_BAD;

   // Correggo la sintassi del nome del campo per SQL MAP
   KeyAttribCorrected = pInfo->key_attrib;
   gsc_AdjSyntaxMAPFormat(KeyAttribCorrected);

   statement = _T("SELECT COUNT(");
   statement += KeyAttribCorrected;
   statement += _T(") FROM ");
   statement += MapTableRef;
   statement += _T(" WHERE ");
   statement += Cond;

   // Preparo se necessario il file UDL
   if (((C_CLASS *) pCls)->getLPNameOld(UDL) == GS_BAD) return GS_BAD;
   if (gsc_SetACADUDLFile(UDL.get_name(), pConn, pInfo->OldTableRef.get_name()) == GS_BAD)
      return GS_BAD;
   // Creo una sessione ASI utilizzando il file UDL
   if ((pSession = gsc_ASICreateSession(UDL.get_name())) == NULL) return GS_BAD;
   if (gsc_ASIPrepareSql(pSession, statement.get_name(), UDL.get_name(),
                         &pExecStm, &pCsr) == GS_BAD)
      { gsc_ASITermSession(&pSession); return GS_BAD; }
   // Apro cursore
   if (pCsr->Open() != kAsiGood)
   {
      delete pCsr;
      gsc_ASITermStm(&pExecStm);
      gsc_ASITermSession(&pSession);
      return GS_BAD;
   }

   // leggo la riga      
   if (pCsr->Fetch() != kAsiGood ||
       (pRow = pCsr->getCurrentRow()) == NULL ||
       (pData = (*pRow)[0].getData()) == NULL ||
       gsc_ASIData2Rb(pData, &rbRec) == GS_BAD ||
       gsc_rb2Lng(&rbRec, &nRecOld) == GS_BAD)
   { 
      delete pCsr;
      gsc_ASITermStm(&pExecStm);
      gsc_ASITermSession(&pSession);
      GS_ERR_COD = eGSReadRow;
      return GS_BAD;
   }

   pCsr->Close();
   delete pCsr;
   gsc_ASITermStm(&pExecStm);
   gsc_ASITermSession(&pSession);

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
   acutPrintf(_T("initSQLCond impiega %6.2f secondi.\n"), (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000) );
#endif

   return (nRecTmp + nRecOld) * 137 / 70000;
}


/*********************************************************/
/*.doc C_LINK_SET::initSQLCond                 <internal> */
/*+
  Questa funzione inizializza un linkset tramite una condizione SQL
  da applicare agli oggetti di una classe nota. Viene inizializzato un 
  gruppo di selezione e una lista di codici chiave.
  Parametri:
  C_NODE     *pCls;     Puntatore alla classe di GEOsim
  const TCHAR *Cond     Condizione SQL 
  int      ObjType;     Tipi di oggetti da cercare (default GRAPHICAL)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_LINK_SET::initSQLCond(C_NODE *pCls, const TCHAR *Cond, int ObjType)
{
   C_DBCONNECTION *pConn;
   C_ID           *pID = ((C_CLASS *) pCls)->ptr_id();  
   C_INFO         *pInfo = ((C_CLASS *) pCls)->ptr_info();
   C_STRING       statement, TmpTableRef, UDL, MapTableRef, KeyAttribCorrected;
   resbuf         rbKey;
   long           Key;
   int            Result = GS_GOOD, IsRsCloseable, Category;
   bool           CheckTempTable = FALSE;
   C_SELSET       dummy;
   CAsiExecStm    *pExecStm;
   CAsiSession    *pSession;
   CAsiCsr        *pCsr;
   CAsiData       *pData;
   CAsiRow        *pRow;
   _RecordsetPtr  pTmpRs;
   C_PREPARED_CMD pTmpCmd;

#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2;
   double tempo1=0;
   _ftime(&t1);
#endif

   if (gsc_strlen(Cond) == 0)
      // considero tutte le entità della classe
      return GetGSClassSS(pID->code, pID->sub_code, SelSet, ptr_KeyList(), ObjType);

   SelSet.clear();
   KeyList.remove_all();

   Category = ((C_CLASS *) pCls)->get_category();

   // Correggo la sintassi del nome del campo per SQL MAP
   KeyAttribCorrected = pInfo->key_attrib;
   gsc_AdjSyntaxMAPFormat(KeyAttribCorrected);

   // se esiste già la tabella temporanea
   if ((pConn = pInfo->getDBConnection(TEMP)) == NULL) return GS_BAD;
   if (((C_CLASS *) pCls)->getTempTableRef(TmpTableRef, GS_BAD) == GS_GOOD &&
       pConn->ExistTable(TmpTableRef) == GS_GOOD)
   {
      // preparo la ricerca dei codici dei record che soddisfano la condizione SQL
      // per la tabella TEMP
      if (gsc_Table2MAPFormat(pConn, TmpTableRef, MapTableRef) == GS_BAD) return GS_BAD;

      statement = _T("SELECT ");
      statement += KeyAttribCorrected;
      statement += _T(" FROM ");
      statement += MapTableRef;
      statement += _T(" WHERE ");
      statement += Cond;

      CheckTempTable = TRUE;
      // Preparo se necessario il file UDL
      if (((C_CLASS *) pCls)->getLPNameTemp(UDL) == GS_BAD) return GS_BAD;
      if (gsc_SetACADUDLFile(UDL.get_name(), pConn, TmpTableRef.get_name()) == GS_BAD)
         return GS_BAD;
      // Creo una sessione ASI utilizzando il file UDL
      if ((pSession = gsc_ASICreateSession(UDL.get_name())) == NULL) return GS_BAD;
      if (gsc_ASIPrepareSql(pSession, statement.get_name(), UDL.get_name(),
                            &pExecStm, &pCsr,
                            NULL, NULL, NULL, GS_BAD) == GS_BAD) // senza visualizzare msg di errore
      {
         // Purtroppo se la condizione di fltro ricade su campi di tipo date
         // la compilazione x ACCESS non funziona perchè access non ha il tipo date
         // ma solo timestamp (CAMPO = DATE '2013-03-06' non andrebbe).
         // Allora dove trovo DATE '...' lo sostituisco con TIMESTAMP '... 00:00:00'
         C_STRING AccessStm;

         AccessStm = gsc_AdjSyntaxDateToTimestampMAPFormat(statement);
         AccessStm = gsc_AdjSyntaxTimeToTimestampMAPFormat(AccessStm);

         if (gsc_ASIPrepareSql(pSession, AccessStm.get_name(), UDL.get_name(),
                               &pExecStm, &pCsr) == GS_BAD)
            { gsc_ASITermSession(&pSession); return GS_BAD; }
      }

      // Apro cursore
      if (pCsr->Open() != kAsiGood)
      { 
         delete pCsr;
         gsc_ASITermStm(&pExecStm);
         gsc_ASITermSession(&pSession);
         return GS_BAD;
      }

      // Per le classi griglia
      if (Category == CAT_GRID)
         // Ciclo di lettura dei codici
         while (pCsr->Fetch() == kAsiGood)
         {
            if ((pRow = pCsr->getCurrentRow()) == NULL ||
                (pData = (*pRow)[0].getData()) == NULL ||
                gsc_ASIData2Rb(pData, &rbKey) == GS_BAD ||
                gsc_rb2Lng(&rbKey, &Key) == GS_BAD)
               { Result = GS_BAD; GS_ERR_COD = eGSReadRow; break; }

            // Aggiungo codice chiave alla lista
            KeyList.add(&Key);
         }
      else
         // Ciclo di lettura dei codici
         while (pCsr->Fetch() == kAsiGood)
         {
            if ((pRow = pCsr->getCurrentRow()) == NULL ||
                (pData = (*pRow)[0].getData()) == NULL ||
                gsc_ASIData2Rb(pData, &rbKey) == GS_BAD ||
                gsc_rb2Lng(&rbKey, &Key) == GS_BAD)
               { Result = GS_BAD; GS_ERR_COD = eGSReadRow; break; }

            // Ricavo gli oggetti grafici collegati a questo codice chiave
            if (GetSS(pID->code, pID->sub_code, Key, dummy) == GS_GOOD)
            {
               if (ObjType != ALL) dummy.intersectType(ObjType);

               if (dummy.length() > 0)
               {
                  // Aggiungo codice chiave alla lista
                  KeyList.add(&Key);
                  SelSet.add_selset(dummy);
               }
            }
         }
      pCsr->Close();
      delete pCsr;
      gsc_ASITermStm(&pExecStm);
      gsc_ASITermSession(&pSession);

      // Per le classi griglia e le simulazioni
      if (Category == CAT_GRID || Category == CAT_SUBCLASS) return Result; // tutti i dati sono nel temp

      // preparo la struttura per la ricerca di un record nel temporaneo
      if (((C_CLASS *) pCls)->prepare_data(pTmpCmd, TEMP) == GS_BAD) return GS_BAD;
   }

   // preparo la ricerca dei codici dei record che soddisfano la condizione SQL
   // per la tabella OLD
   if ((pConn = pInfo->getDBConnection(OLD)) == NULL) return GS_BAD;

   if (gsc_Table2MAPFormat(pConn, pInfo->OldTableRef, MapTableRef) == GS_BAD) return GS_BAD;

   statement = _T("SELECT ");
   statement += KeyAttribCorrected;
   statement += _T(" FROM ");
   statement += MapTableRef;
   statement += _T(" WHERE ");
   statement += Cond;

   // Preparo se necessario il file UDL
   if (((C_CLASS *) pCls)->getLPNameOld(UDL) == GS_BAD) return GS_BAD;
   if (gsc_SetACADUDLFile(UDL.get_name(), pConn, pInfo->OldTableRef.get_name()) == GS_BAD)
      return GS_BAD;
   // Creo una sessione ASI utilizzando il file UDL
   if ((pSession = gsc_ASICreateSession(UDL.get_name())) == NULL) return GS_BAD;
   if (gsc_ASIPrepareSql(pSession, statement.get_name(), UDL.get_name(),
                         &pExecStm, &pCsr) == GS_BAD)
      { gsc_ASITermSession(&pSession); return GS_BAD; }
   // Apro cursore
   if (pCsr->Open() != kAsiGood)
   {
      delete pCsr;
      gsc_ASITermStm(&pExecStm);
      gsc_ASITermSession(&pSession);
      return GS_BAD;
   }
   // Ciclo di lettura dei codici
   while (pCsr->Fetch() == kAsiGood)
   {
      if ((pRow = pCsr->getCurrentRow()) == NULL ||
          (pData = (*pRow)[0].getData()) == NULL ||
          gsc_ASIData2Rb(pData, &rbKey) == GS_BAD ||
          gsc_rb2Lng(&rbKey, &Key) == GS_BAD)
         { Result = GS_BAD; GS_ERR_COD = eGSReadRow; break; }
      
      // Se esisteva la tabella temp
      if (CheckTempTable)
      {  // devo scartare tutti i record già presenti nel temporaneo
         if (gsc_get_data(pTmpCmd, Key, pTmpRs, &IsRsCloseable) == GS_GOOD)
         {
            if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pTmpRs);
         }
         else
            // Ricavo gli oggetti grafici collegati a questo codice chiave
            if (GetSS(pID->code, pID->sub_code, Key, dummy) == GS_GOOD)
            {
               if (ObjType != ALL) dummy.intersectType(ObjType);

               if (dummy.length() > 0)
               {
                  // Aggiungo codice chiave alla lista
                  KeyList.add(&Key);
                  SelSet.add_selset(dummy);
               }
            }
      }
      else
         // Ricavo gli oggetti grafici collegati a questo codice chiave
         if (GetSS(pID->code, pID->sub_code, Key, dummy) == GS_GOOD)
         {
            if (ObjType != ALL) dummy.intersectType(ObjType);

            if (dummy.length() > 0)
            {
               // Aggiungo codice chiave alla lista
               KeyList.add(&Key);
               SelSet.add_selset(dummy);
            }
         }
   }

   pCsr->Close();
   delete pCsr;
   gsc_ASITermStm(&pExecStm);
   gsc_ASITermSession(&pSession);

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
   acutPrintf(_T("initSQLCond impiega %6.2f secondi.\n"), (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000) );
#endif

   return Result;
}


C_SELSET* C_LINK_SET::ptr_SelSet(void) { return &SelSet; }
C_LONG_BTREE* C_LINK_SET::ptr_KeyList(void) { return &KeyList; }


/*********************************************************/
/*.doc C_LINK_SET::RefreshSS                  <external> */
/*+
  Questa funzione aggiorna nella tabella GS_LINK i codici GEOsim dei
  corrispondenti handle degli oggetti grafici.
  Parametri:
  ads_name ss;               gruppo di selezione
  int      CounterToVideo;   flag, se = GS_GOOD stampa a video il numero di entità
                             che si stanno elaborando (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_LINK_SET::RefreshSS(C_SELSET &ss, int CounterToVideo)
{
   ads_name ss1;
   ss.get_selection(ss1);
   return RefreshSS(ss1, CounterToVideo);
}
int C_LINK_SET::RefreshSS(ads_name ss, int CounterToVideo)
{
   TCHAR         Handle[MAX_LEN_HANDLE];
   presbuf       prb_before_Handle;
   long          ItemNum = 0, Tot, Key, val_Key;
   long          Inserted = 0, Updated = 0, Erased = 0;
   ads_name      entity;
   C_EED         eed;
   C_CLASS       *pClass;
   C_CLASS_LIST  *pClassList;
   C_LIST        *pSubList;
   C_ATTRIB_LIST *p_attrib_list;
   int           result, prj, val_Cls, val_Sub;
   int           isRsSelectLinkSupported = -1;
   _RecordsetPtr pRsFromSession, pRs, pInsRs;
   _CommandPtr   pCmd;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(760)); // "Aggiornamento collegamenti grafica\\database..."

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (ads_name_nil(ss) || acedSSLength(ss, &Tot) != RTNORM || Tot == 0) return GS_GOOD;

   pClassList = GS_CURRENT_WRK_SESSION->get_pPrj()->ptr_classlist();
   prj        = GS_CURRENT_WRK_SESSION->get_PrjId();

   isRsSelectLinkSupported = (GS_CURRENT_WRK_SESSION->get_RsSelectLink() == NULL) ? GS_BAD : GS_GOOD;

   if (isRsSelectLinkSupported == GS_GOOD) pRsFromSession = GS_CURRENT_WRK_SESSION->get_RsSelectLink();
   else
      // Ricavo il comando che esegue la selezione di link per handle
      if ((pCmd = GS_CURRENT_WRK_SESSION->get_CmdSelectLinkWhereHandle()) == NULL)
         return GS_BAD;

   // Poichè l'inserimento di record aperto per applicare il metodo seek 
   // è circa 8 volte più lento che su un recordset standard
   // uso un recordset diverso per inserire
   if ((pInsRs = GS_CURRENT_WRK_SESSION->get_RsInsertLink()) == NULL) return GS_BAD;

   // perchè la lista nella ColValues dopo KEY_ATTRIB viene HANDLE
   prb_before_Handle = prb_Key->rbnext->rbnext;

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.Init(Tot);

   result = GS_GOOD;
   
   while (acedSSName(ss, ItemNum++, entity) == RTNORM)
   {
      if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.Set(ItemNum);

      // leggo Handle
      gsc_enthand(entity, Handle);

      // leggo informazioni di GEOsim
      if (eed.load(entity) == GS_BAD)
      {  // Link non valido (non aveva la EED di GEOsim) lo cancello
         if (isRsSelectLinkSupported)
         {  
            _variant_t KeyForSeek(Handle);

            // Uso tecnica dei recordset molto più veloce
            if (gsc_DBSetIndexRs(pRsFromSession, _T("HANDLE")) == GS_BAD ||
                gsc_DBSeekRs(pRsFromSession, KeyForSeek, adSeekFirstEQ) == GS_BAD)
               { result = GS_BAD; break; }
            if (gsc_isEOF(pRsFromSession) == GS_BAD)
            {
               // lettura record
               if (gsc_DBReadRow(pRsFromSession, ColValues) == GS_BAD)
                  { result = GS_BAD; break; }

               // se il record non è già stato invalidato 
               if ((gsc_rb2Int(prb_Cls, &val_Cls) == GS_GOOD && val_Cls != 0) ||
                   (gsc_rb2Int(prb_Sub, &val_Sub) == GS_GOOD && val_Sub != 0) ||
                   (gsc_rb2Lng(prb_Key, &val_Key) == GS_GOOD && val_Key != 0))
               {
                  prb_Cls->restype = RTSHORT; prb_Cls->resval.rint = 0;
                  prb_Sub->restype = RTSHORT; prb_Sub->resval.rint = 0;
                  prb_Key->restype = RTLONG;  prb_Key->resval.rlong = 0;
                  prb_before_Handle->restype = RTLE; // per non considerare handle

                  if (gsc_DBUpdRow(pRsFromSession, ColValues) == GS_BAD)
                     { prb_before_Handle->restype = RTLB; result = GS_BAD; break; }
                  prb_before_Handle->restype = RTLB;

                  Erased++;
               }
            }
         }
         else
         {
            // imposto come parametro l'handle dell'oggetto grafico
            if (gsc_SetDBParam(pCmd, 0, Handle) == GS_BAD)
               { result = GS_BAD; break; }

            // Esegue l'istruzione SQL
            // prima era adOpenKeyset ma postgresql in una transazione fa casino
            if (gsc_ExeCmd(pCmd, pRs, adOpenDynamic, adLockOptimistic) == GS_BAD)
               { result = GS_BAD; break; }

            if (gsc_isEOF(pRs) == GS_BAD)
            {
               // lettura record
               if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
                  { gsc_DBCloseRs(pRs); result = GS_BAD; break; }

               // se il record non è già stato invalidato 
               if ((gsc_rb2Int(prb_Cls, &val_Cls) == GS_GOOD && val_Cls != 0) ||
                   (gsc_rb2Int(prb_Sub, &val_Sub) == GS_GOOD && val_Sub != 0) ||
                   (gsc_rb2Lng(prb_Key, &val_Key) == GS_GOOD && val_Key != 0))
               {
                  prb_Cls->restype = RTSHORT; prb_Cls->resval.rint = 0;
                  prb_Sub->restype = RTSHORT; prb_Sub->resval.rint = 0;
                  prb_Key->restype = RTLONG;  prb_Key->resval.rlong = 0;
                  prb_before_Handle->restype = RTLE; // per non considerare handle

                  if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD)
                     { gsc_DBCloseRs(pRs); prb_before_Handle->restype = RTLB; result = GS_BAD; break; }

                  prb_before_Handle->restype = RTLB;

                  Erased++;
               }
            }
            gsc_DBCloseRs(pRs);
         }

         continue;
      }

      // non utilizzo la gsc_find_class perchè la search_key della C_CLASS_LIST utilizza
      // la posizione attuale del cursore e questo velocizza la ricerca
      if ((pClass = (C_CLASS *) pClassList->search_key(eed.cls)) == NULL)
         continue;

      if (eed.sub > 0)
         if ((pSubList = pClass->ptr_sub_list()) != NULL)
            // Cerca la sotto-classe
            if ((pClass = (C_CLASS *)pSubList->search_key(eed.sub)) == NULL)
               continue;

      if ((p_attrib_list = pClass->ptr_attrib_list()) == NULL) continue;

      // leggo il valore chiave
      Key = eed.gs_id;

      // setto il codice chiave, codice della classe e sottoclasse
      gsc_RbSubst(prb_Key, Key);
      gsc_RbSubst(prb_Cls, eed.cls);
      gsc_RbSubst(prb_Sub, eed.sub);
      gsc_RbSubst(prb_Handle, Handle);
      
      // inserisco i dati in tabella
      if (isRsSelectLinkSupported)
      {
         if (gsc_DBInsRow(pInsRs, ColValues, ONETEST, GS_BAD) == GS_BAD)
         {
            // se l'errore non era dovuto al fatto che il record esisteva già
            if (GS_ERR_COD != eGSIntConstr) { result = GS_BAD; break; }

            _variant_t KeyForSeek(Handle);

            // Uso tecnica dei recordset molto più veloce
            if (gsc_DBSetIndexRs(pRsFromSession, _T("HANDLE")) == GS_BAD ||
                gsc_DBSeekRs(pRsFromSession, KeyForSeek, adSeekFirstEQ) == GS_BAD)
               { result = GS_BAD; break; }
            // lettura record
            if (gsc_DBReadRow(pRsFromSession, ColValues) == GS_BAD)
               { result = GS_BAD; break; }
            // se il record non è da aggiornare 
            if ((gsc_rb2Int(prb_Cls, &val_Cls) == GS_GOOD && val_Cls != eed.cls) ||
                (gsc_rb2Int(prb_Sub, &val_Sub) == GS_GOOD && val_Sub != eed.sub) ||
                (gsc_rb2Lng(prb_Key, &val_Key) == GS_GOOD && val_Key != Key))
            {
               prb_Cls->restype = RTSHORT; prb_Cls->resval.rint = eed.cls;
               prb_Sub->restype = RTSHORT; prb_Sub->resval.rint = eed.sub;
               prb_Key->restype = RTLONG;  prb_Key->resval.rlong = Key;
               prb_before_Handle->restype = RTLE; // per non considerare handle
               if (gsc_DBUpdRow(pRsFromSession, ColValues) == GS_BAD)
                  { prb_before_Handle->restype = RTLB; result = GS_BAD; break; }
               prb_before_Handle->restype = RTLB;

               Updated++;
            }
         }
      }
      else
      if (gsc_DBInsRow(pInsRs, ColValues, ONETEST, GS_BAD) == GS_BAD)
      {
         // se l'errore non era dovuto al fatto che il record esisteva già
         if (GS_ERR_COD != eGSIntConstr) { result = GS_BAD; break; }

         // imposto come parametro l'handle dell'oggetto grafico
         if (gsc_SetDBParam(pCmd, 0, Handle) == GS_BAD)
            { result = GS_BAD; break; }

         // Esegue l'istruzione SQL
         // prima era adOpenKeyset ma postgresql in una transazione fa casino
         if (gsc_ExeCmd(pCmd, pRs, adOpenDynamic, adLockOptimistic) == GS_BAD)
            { result = GS_BAD; break; }

         // lettura record
         if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
            { gsc_DBCloseRs(pRs); result = GS_BAD; break; }

         // se il record non è da aggiornare 
         if ((gsc_rb2Int(prb_Cls, &val_Cls) == GS_GOOD && val_Cls != eed.cls) ||
             (gsc_rb2Int(prb_Sub, &val_Sub) == GS_GOOD && val_Sub != eed.sub) ||
             (gsc_rb2Lng(prb_Key, &val_Key) == GS_GOOD && val_Key != Key))
         {
            prb_Cls->restype = RTSHORT; prb_Cls->resval.rint = eed.cls;
            prb_Sub->restype = RTSHORT; prb_Sub->resval.rint = eed.sub;
            prb_Key->restype = RTLONG;  prb_Key->resval.rlong = Key;
            prb_before_Handle->restype = RTLE; // per non considerare handle
            if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD)
               { gsc_DBCloseRs(pRs); prb_before_Handle->restype = RTLB; result = GS_BAD; break; }
            prb_before_Handle->restype = RTLB;

            Updated++;
         }
         gsc_DBCloseRs(pRs);
      }
      else Inserted++;
   }

   if (CounterToVideo == GS_GOOD && --ItemNum > 0)
   {
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

      acutPrintf(GS_LFSTR);
      if (Inserted > 0)
         acutPrintf(gsc_msg(761), Inserted); // "\n%ld collegamenti inseriti."
      if (Updated > 0)
         acutPrintf(gsc_msg(762), Updated); // "\n%ld collegamenti modificati."
      if (Erased > 0)
         acutPrintf(gsc_msg(763), Erased); // "\n%ld collegamenti cancellati."
   }

   return result;
}


/*********************************************************/
/*.doc C_LINK_SET::add_ent                    <internal> */
/*+
  Aggiunge un entità al link set.
  Parametri:
  ads_name ent;

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_LINK_SET::add_ent(ads_name ent)
{
   C_LINK Link(ent);
   long   Key;

   SelSet.add(ent);
   if (Link.GetKey(&Key) == GS_GOOD) KeyList.add(&Key);
   return GS_GOOD;
}
int C_LINK_SET::add_ent(long EntId, C_SELSET &EntSelSet)
{
   SelSet.add_selset(EntSelSet);
   KeyList.add(&EntId);
   return GS_GOOD;
}


/*********************************************************/
/*.doc C_LINK_SET::add                        <internal> */
/*+
  Aggiunge un gruppo di selezione al link set.
  Parametri:
  C_SELSET &_SelSet;    gruppo di oggetti grafici

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_LINK_SET::add(C_SELSET &_SelSet)
{
   C_LINK   Link;
   long     Key, i = 0;
   ads_name ent;

   SelSet.add_selset(_SelSet);
   while (_SelSet.entname(i++, ent) == GS_GOOD)
   {
      Link.SetEnt(ent);
      if (Link.GetKey(&Key) == GS_GOOD) KeyList.add(&Key);
   }
   return GS_GOOD;
}


/*********************************************************/
/*.doc C_LINK_SET::unite                      <internal> */
/*+
  Esegue l'unione del linkset con una entità di cui si conosce
  codice chiave e gruppo di oggetti grafici. Il risultato è
  nel linkset della classe (this).
  Parametri:
  long _Key;            codice chiave
  C_SELSET &_SelSet;    gruppo di oggetti grafici

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_LINK_SET::unite(long _Key, C_SELSET &_SelSet) 
{
   KeyList.add(&_Key);
   return SelSet.add_selset(_SelSet);
}
int C_LINK_SET::unite(C_LINK_SET &_LinkSet) 
{
   C_BLONG *pKey;
   long    Key;

   SelSet.add_selset(_LinkSet.SelSet);
   
   pKey = (C_BLONG *) _LinkSet.KeyList.go_top();
   while (pKey)
   {
      Key = pKey->get_key();
      KeyList.add(&Key);
      pKey = (C_BLONG *) _LinkSet.KeyList.go_next();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_LINK_SET::subtract                   <internal> */
/*+
  Cancello dal linkset gli oggetti grafici e i codici chiave
  passati come parametro.
  Il risultato è nel linkset della classe (this).
  Parametri:
  long _Key;            codice chiave
  C_SELSET &_SelSet;    gruppo di oggetti grafici
  oppure
  C_LINK_SET &_LinkSet
  oppure
  C_SELSET &_SelSet;    gruppo di oggetti grafici

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_LINK_SET::subtract(long _Key, C_SELSET &_SelSet) 
{
   SelSet.subtract(_SelSet);
   if (KeyList.search(&_Key) != NULL) KeyList.remove_at();

   return GS_GOOD;
}
int C_LINK_SET::subtract(C_LINK_SET &_LinkSet) 
{
   if (subtract(_LinkSet.SelSet) == GS_BAD) return GS_BAD;
   if (subtract(_LinkSet.KeyList) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}
int C_LINK_SET::subtract(C_SELSET &_SelSet) 
{
   // Sottrae gli oggetti grafici e i rispettivi codici chiave se non ci sono altri
   // oggetti grafici collegati con quell'entità
   long     i = 0;
   ads_name ent;

   while (_SelSet.entname(i++, ent) == GS_GOOD)
      subtract_ent(ent);

   return GS_GOOD;
}
int C_LINK_SET::subtract_ent(ads_name ent)
{
   // Sottrae l'oggetto grafico e il rispettivo codici chiave se non ci sono altri
   // oggetti grafici collegati con quell'entità
   long     Key;
   C_LINK   Link(ent);
   int     _cls, _sub;

   SelSet.subtract_ent(ent);
   if (Link.GetKey(&Key, &_cls, &_sub) == GS_GOOD &&
       _cls == cls && _sub == sub)
   {
      C_LINK_SET entLS;
      C_SELSET   entSS;

      // verifico se ci sono ancora oggetti grafici della stessa entità in SelSet
      if (entLS.GetSS(cls, sub, Key, entSS) == GS_GOOD)
      {
         entSS.intersect(SelSet);
         if (entSS.length() == 0)
            if (KeyList.search(&Key) != NULL) KeyList.remove_at();
      }
   }

   return GS_GOOD;
}
int C_LINK_SET::subtract_key(long Key)
{
   // Sottrae il codice chiave e i rispettivi oggetti grafici collegati con quell'entità
   C_LINK_SET entLS;
   C_SELSET entSS;

   if (KeyList.search(&Key) != NULL) KeyList.remove_at();
   if (entLS.GetSS(cls, sub, Key, entSS) == GS_GOOD)
      SelSet.subtract(entSS);

   return GS_GOOD;
}
int C_LINK_SET::subtract(C_LONG_BTREE &_KeyList)
{
   // Sottrae i codici chiave e i rispettivi oggetti grafici collegati con quell'entità
   C_BLONG *pKey;
   
   pKey = (C_BLONG *) _KeyList.go_top();
   while (pKey)
   {
      subtract_key(pKey->get_key());
      pKey = (C_BLONG *) _KeyList.go_next();
   }

   return GS_GOOD;
}

/*********************************************************/
/*.doc C_LINK_SET::intersect                   <internal> */
/*+
  Questa funzione filtra dal linkset solo le entità presenti
  nel gruppo di selezione indicato dal parametro <_SelSet>.
  Parametri:
  int selset;   gruppo di selezione
  Parametri:
  C_SELSET &_SelSet;    gruppo di oggetti grafici

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_LINK_SET::intersect(C_SELSET &_SelSet) 
{
   if (SelSet.intersect(_SelSet) == GS_BAD)
      return GS_BAD;

   KeyList.remove_all();
   
   C_LINK   Link;
   long     Key, i = 0;
   ads_name ent;

   while (SelSet.entname(i++, ent) == GS_GOOD)
   {
      Link.SetEnt(ent);
      if (Link.GetKey(&Key) == GS_GOOD) KeyList.add(&Key);
   }

   return GS_GOOD;
}
int C_LINK_SET::intersect(C_LINK_SET &_LinkSet) 
{
   SelSet.intersect(*(_LinkSet.ptr_SelSet()));
   KeyList.intersect(*(_LinkSet.ptr_KeyList()));

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_LINK_SET::ClosestTo                  <internal> */
/*+
  Questa funzione mantiene nel linkset solo le Qty entità più vicine a Ent
  cancellando le più distanti.
  Parametri:
  ads_name Ent;   Entità a cui bisogna calcolare la distanza
  int Qty;        Quante entità mantenere tra le più vicine (default = 1)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_LINK_SET::ClosestTo(C_LINK_SET &_LinkSet, int Qty)
{
   C_CLASS *pCls;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(_LinkSet.cls, _LinkSet.sub)) == NULL) return GS_BAD;
   if (pCls->get_category() != CAT_GRID)
      return ClosestTo(*(_LinkSet.ptr_SelSet()), Qty);

   int                i;
   AcDbEntityPtrArray ObjEntitySet, ReferenceEntitySet, ResultEntitySet;
   C_SELSET           ReferenceSS;

   // Ottengo i rettangoli che descrivono le celle
   if (((C_CGRID *) pCls)->get2DCells(_LinkSet.ptr_KeyList(), ReferenceEntitySet) == GS_BAD) return GS_BAD;
   ReferenceSS.set_AcDbEntityPtrArray(ReferenceEntitySet);
   for (i = 0; i < ReferenceEntitySet.length(); i++) ((AcDbEntity *) ReferenceEntitySet[i])->close();
   
   return ClosestTo(ReferenceSS, Qty);
}
int C_LINK_SET::ClosestTo(C_SELSET &_SelSet, int Qty) 
{
   C_CLASS *pCls;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(cls, sub)) == NULL) return GS_BAD;

   if (pCls->get_category() != CAT_GRID)
   {
      C_SELSET mySS;

      SelSet.copy(mySS);
      if (mySS.ClosestTo(_SelSet, Qty) == GS_BAD)
         return GS_BAD;
      intersect(mySS);
   }
   else // se griglia
   {
      int                i;
      AcDbEntityPtrArray ObjEntitySet, ReferenceEntitySet, ResultEntitySet;
      ads_point          pt;
      long               Key;

      // Ottengo i rettangoli che descrivono le celle
      if (((C_CGRID *) pCls)->get2DCells(&KeyList, ObjEntitySet) == GS_BAD) return GS_BAD;

      if (_SelSet.get_AcDbEntityPtrArray(ReferenceEntitySet) == GS_BAD) return GS_BAD;
      if (gsc_getClosestTo(ObjEntitySet, ReferenceEntitySet, ResultEntitySet, Qty) == GS_BAD)
      {
         gsc_close_AcDbEntities(ReferenceEntitySet);
         return GS_BAD;
      }
      if (gsc_close_AcDbEntities(ReferenceEntitySet) == GS_BAD) return GS_BAD;

      // ricavo i codici delle celle relative ai rettangoli
      KeyList.remove_all();
      for (i = 0; i < ResultEntitySet.length(); i++)
      {
         gsc_get_firstPoint(((AcDbEntity *) ResultEntitySet[i]), pt);
         ((C_CGRID *) pCls)->ptr_grid()->pt2key(pt, &Key);
         KeyList.add(&Key);
      }
   }

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//////////////////  C_LINK_SET   FINE  ///////////////////////////////////
//-----------------------------------------------------------------------//


/****************************************************************************/
/*.doc gsc_getBlockDef <external> */
/*+
  Questa funzione restituisce la definizione di un blocco DA di ASE.
  Parametri:
  ads_name entity;            oggetto blocco DA
  C_RB_LIST *BlockDefinition; definizione del blocco (lista di resbuf)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************************/
int gsc_getBlockDef(ads_name entity, C_RB_LIST *BlockDefinition)
{
   TCHAR     BlockName[MAX_LEN_BLOCKNAME];
   C_RB_LIST EntList;
   presbuf   prb;

   if ((EntList << acdbEntGet(entity)) == NULL)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   if ((prb = EntList.SearchType(kDxfStart)) == NULL ||
       gsc_strcmp(prb->resval.rstring, _T("INSERT"))!=0)    // tipo di entità
      { GS_ERR_COD=eGSInvEntityOp; return GS_BAD; }
   if ((prb = EntList.SearchType(2)) == NULL)       // nome dell'entità
      { GS_ERR_COD=eGSInvEntityOp; return GS_BAD; }
   gsc_strcpy(BlockName, prb->resval.rstring, MAX_LEN_BLOCKNAME);

   // leggo la definizione del blocco
   if (((*BlockDefinition) << acdbTblSearch(_T("BLOCK"), BlockName, 0)) == NULL)
      { GS_ERR_COD = eGSInvalidBlock; return GS_BAD; }

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
///// INIZIO      FUNZIONI   LISP
///////////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc gs_PurgeLPTs                           <external> */
/*+
  Questa funzione LISP cancella i LPT che non sono usati.
  Parametri:
  ([dwg])   opzionale, path completa di un file dwg
  
  Restituisce T in caso di successo altrimenti restituisce NIL.
-*/  
/*********************************************************/
int gs_PurgeLPTs(void)
{
   presbuf arg = acedGetArgs();
   int     result;

   if (arg && arg->restype == RTSTR)
      result = gsc_PurgeLPTs(arg->resval.rstring);
   else
      result = gsc_PurgeLPTs();

   if (result == GS_GOOD)
   {
      acedRetT();
      return RTNORM;
   }
   else
   {
      acedRetNil();
      return RTERROR;
   }
}

/*********************************************************/
/*.doc gsc_PurgeLPTs                           <external> */
/*+
  Questa funzione LISP cancella i LPT che non sono usati.
  Parametri:
  const TCHAR *DwgPath; Opzionale, Nome del file DWG da elaborare
                        Se = NULL si usa il disegno corrente (default = NULL)
  
  Restituisce T in caso di successo altrimenti restituisce NIL.
-*/  
/*********************************************************/
int gsc_PurgeLPTs(const TCHAR *DwgPath)
{
   CAseLinkPath LPT(GEOsimAppl::ASE);    // Link Path Descriptor
   C_STRING     tmp_path, ext;
   AcDbDatabase extDatabase(Adesk::kFalse);
   presbuf      p;
   C_RB_LIST    LPTNameList;
   CAseLinkSel  ls(GEOsimAppl::ASE);
   int          result = GS_GOOD;
   TCHAR        Msg[MAX_LEN_MSG];
   long         nPurged = 0;

   if (gsc_strlen(DwgPath) > 0)
   {
      tmp_path = DwgPath;
      // Controlla Correttezza Path
      if (gsc_nethost2drive(tmp_path) == GS_BAD) return GS_BAD; 
      gsc_splitpath(tmp_path, NULL, NULL, NULL, &ext);
      if (ext.len() == 0) // se non ha estensione la aggiungo
         tmp_path += _T(".DWG");

      // leggo il dwg
      // _SH_DENYWR = open for read/write and allow no sharing
      if (extDatabase.readDwgFile(tmp_path.get_name(), _SH_DENYWR) != Acad::eOk)
         { GS_ERR_COD = eGSInvalidPath; return GS_BAD; }

      if (LPT.setDwg(&extDatabase) != kAsiGood) 
         { GS_ERR_COD = eGSInvalidPath; return GS_BAD; }
      if (ls.setDwg(&extDatabase) != kAsiGood) 
         { GS_ERR_COD = eGSInvalidPath; return GS_BAD; }
   }

   // leggo la lista di tutti i LPN registrati nel disegno
   p = LPT.getLinkNames();
   if (p) LPTNameList << p; // in questo modo se non ci sono LPN non
                            // viene settato errore dall'operatore << della C_RB_LIST
   while (p) // verifico le connessioni dei LPN
   {
      ls.errClear();
      // ricavo il gruppo di selezione di tutti gli oggetti della classe
      if (ls.initLp(p->resval.rstring) == kAsiGood) 
      {
         if (ls.getQty() == 0)
            // cancello il LPT
            if (LPT.erase() != kAsiGood)
            {
               swprintf(Msg, MAX_LEN_MSG, _T("LPT named <%s> not erased."), p->resval.rstring);
               gsc_write_log(Msg);
               result = GS_BAD;
            }
            else
               nPurged++;
      }
      else if (ls.errQty() == 0) // A volte se initLp non restituisce kAsiGood e non ci sono stati errori 
                                 // è perchè non ha trovato links
      {
         // cancello il LPT
         if (LPT.erase() != kAsiGood)
         {
            swprintf(Msg, MAX_LEN_MSG, _T("LPT named <%s> not erased."), p->resval.rstring);
            gsc_write_log(Msg);
            result = GS_BAD;
         }
         else
            nPurged++;
      }
      else
      {
         gsc_printASEErr(&ls);
         swprintf(Msg, MAX_LEN_MSG, _T("LPT named <%s> not initialized."), p->resval.rstring);
         gsc_write_log(Msg);
         result = GS_BAD;
      }

      p = p->rbnext;
   }

   if (gsc_strlen(DwgPath) > 0 && nPurged > 0)
      if (extDatabase.saveAs(tmp_path.get_name()) != Acad::eOk)
         return GS_BAD;

   acutPrintf(gsc_msg(446), nPurged); // "\nEliminati %ld Link Path Template dal disegno."
   
   return result;
}
