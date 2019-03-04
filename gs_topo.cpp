/**********************************************************
Name: GS_TOPO.CPP

Module description: File contenente le funzioni per la gestione
                    della topologia dinamica in Geosim

Author: Roberto Poltini

(c) Copyright 1998-2015 by IREN ACQUA GAS S.p.A.

Modification history:
              
Notes and restrictions on use:
La topologia si può applicare solo ad entità GEOsim di categoria
simulazione.
**********************************************************/


#include "stdafx.h"         // MFC core and standard components

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")

#define NODETOPOTABLENAME _T("TN") // Topologia Node
#define LINKTOPOTABLENAME _T("TL") // Topologia Link
#define AREATOPOTABLENAME _T("TA") // Topologia Area


/**********************************************************/
/*   INCLUDE                                              */
/**********************************************************/


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits>

#include "rxdefs.h"   
#include "adslib.h"   

#include "..\gs_def.h" // definizioni globali
#include "gs_opcod.h"     // codici delle operazioni
#include "gs_error.h"   
#include "gs_list.h"   
#include "gs_utily.h"
#include "gs_resbf.h"
#include "gs_sql.h"
#include "gs_selst.h"
#include "gs_user.h"
#include "gs_dbref.h" 
#include "gs_init.h"
#include "gs_prjct.h"     // gestione progetti
#include "gs_area.h"      // gestione sessioni di lavoro
#include "gs_class.h"     // prototipi funzioni gestione classi
#include "gs_graph.h"
#include "gs_query.h"
#include "gs_attbl.h"
#include "gs_filtr.h"
#include "gs_topo.h"      // gestione topologia GEOsim


#if defined(GSDEBUG) // se versione per debugging
   #include <sys/timeb.h>  // Solo per debug
   #include <time.h>       // Solo per debug
   double  topotempo=0;
   struct _timeb topot1, topot2;
#endif


/*************************************************************************/
/* PRIVATE FUNCTIONS                                                     */
/*************************************************************************/


static int gsc_isExistingEnt(int cls, int sub, long Key);

//////////////////////////////////////////////////////////////////
/// INIZIO FUNZIONI C_BPOINT_OBJS                              ///
//////////////////////////////////////////////////////////////////


// costruttore
C_BPOINT_OBJS::C_BPOINT_OBJS() : C_BNODE() {}

C_BPOINT_OBJS::C_BPOINT_OBJS(ads_namep ent) : C_BNODE() 
{
   gsc_get_firstPoint(ent, pt);
   add_ent2EntList(ent);
}

C_BPOINT_OBJS::C_BPOINT_OBJS(ads_pointp in) : C_BNODE() 
{
   ads_point_set(in, pt);
}

// distruttore
C_BPOINT_OBJS::~C_BPOINT_OBJS() {}

int C_BPOINT_OBJS::get_point(ads_point out)
   { ads_name_set(pt, out); return GS_GOOD; }

int C_BPOINT_OBJS::set_point(ads_point in)
   { ads_name_set(in, pt); return GS_GOOD; }

C_CLS_PUNT_LIST* C_BPOINT_OBJS::ptr_EntList(void)
   { return &EntList; }

int C_BPOINT_OBJS::add_ent2EntList(ads_name ent)
{
   C_CLS_PUNT *p;
   C_EED      eed;
   C_CLASS    *pCls;
   
   if (EntList.search_ent(ent)) return GS_GOOD; // c'è già

   if ((p = new C_CLS_PUNT(ent)) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   if (eed.load(ent) == GS_GOOD)
      if ((pCls = GS_CURRENT_WRK_SESSION->find_class(eed.cls, eed.sub)) != NULL)
         p->cls = pCls;

   return EntList.add_tail(p);
}

int C_BPOINT_OBJS::del_ent2EntList(ads_name ent)
{
   if (EntList.search_ent(ent)) return EntList.remove_at();
   
   return GS_BAD;
}

int C_BPOINT_OBJS::add_pClsPunt2EntList(C_CLS_PUNT *pClsPunt)
{
   C_CLS_PUNT *p;
   
   if (EntList.search_ent(pClsPunt->ent)) return GS_GOOD; // c'è già

   if ((p = new C_CLS_PUNT(pClsPunt)) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   return EntList.add_tail(p);
}

// Utilizzo un valore di offset di errore (vedi definizione di MAX_COORDINATE_OFFSET).
int C_BPOINT_OBJS::compare(const void *in) // <in> deve essere un puntatore a C_BPOINT_OBJS
{
   double diff;

   diff = pt[X] - ((ads_pointp) in)[X];
   if (diff > GEOsimAppl::TOLERANCE) return 1;
   if (diff < -GEOsimAppl::TOLERANCE) return -1;

   diff = pt[Y] - ((ads_pointp) in)[Y];
   if (diff > GEOsimAppl::TOLERANCE) return 1;
   if (diff < -GEOsimAppl::TOLERANCE) return -1;

   // per ora considero solo topologia 2D perchè Stefano varia l'elevazione
   // dei nodi della simulazione per usare quicksurf
   //diff = pt[Z] - ((ads_pointp) in)[Z];
   //if (diff > GEOsimAppl::TOLERANCE) return 1;
   //if (diff < -GEOsimAppl::TOLERANCE) return -1;

   return 0;
}

void C_BPOINT_OBJS::print(int *level)
{
   acutPrintf(GS_LFSTR);
   if (level)
      for (int i = 1; i < *level; i++) acutPrintf(_T("  "));
   acutPrintf(_T("%f,%f,%f"), pt[X], pt[Y], pt[Z]);
}


/////////////////////////////////////////////////////////////
///    FINE    FUNZIONI    C_BPOINT_OBJS                  ///
///    INIZIO  FUNZIONI    C_POINT_OBJS_BTREE             ///
/////////////////////////////////////////////////////////////


C_POINT_OBJS_BTREE::C_POINT_OBJS_BTREE():C_BTREE() {}

C_POINT_OBJS_BTREE::~C_POINT_OBJS_BTREE() {}

C_BNODE* C_POINT_OBJS_BTREE::alloc_item(const void *in)
{
   C_BPOINT_OBJS *p = new C_BPOINT_OBJS((ads_pointp) in);

   if (!p) GS_ERR_COD = eGSOutOfMem;
   return (C_BPOINT_OBJS *) p;
}

int C_POINT_OBJS_BTREE::add_ent(ads_name ent)
{
   ads_point pt;

   gsc_get_firstPoint(ent, pt);

   if (add(pt) == GS_BAD) return GS_GOOD;
   
   return ((C_BPOINT_OBJS *) cursor)->add_ent2EntList(ent);
}

int C_POINT_OBJS_BTREE::add_pClsPunt(C_CLS_PUNT *pClsPunt)
{
   ads_point pt;

   gsc_get_firstPoint(pClsPunt->ent, pt);

   if (add(pt) == GS_BAD) return GS_GOOD;
   
   return ((C_BPOINT_OBJS *) cursor)->add_pClsPunt2EntList(pClsPunt);
}

int C_POINT_OBJS_BTREE::add_SelSet(C_SELSET &SelSet)
{
   long     i = 0;
   ads_name ent;

   while (SelSet.entname(i++, ent) == GS_GOOD)
      if (add_ent(ent) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/////////////////////////////////////////////////////////////
//    FINE    FUNZIONI    C_POINT_OBJS_BTREE               //
//    INIZIO  FUNZIONI    C_TOPOLOGY                       //
/////////////////////////////////////////////////////////////


// costruttore
C_TOPOLOGY::C_TOPOLOGY() : C_INT()
{ 
   type = TYPE_POLYLINE;
   Temp = GS_GOOD;
   pCls = NULL;

   NetCost    = (std::numeric_limits<double>::max)();
   NodesVett  = NULL;
   nNodesVett = 0;
   LinksVett  = NULL;
   nLinksVett = 0;
}

// distruttore
C_TOPOLOGY::~C_TOPOLOGY()
   { Terminate(); }

void C_TOPOLOGY::Terminate(void)
{
   if (RsInsertTopo.GetInterfacePtr()) 
      { RsInsertTopo->Close(); RsInsertTopo.Release(); }
   CmdSelectWhereLinkTopo.Terminate();
   if (CmdSelectWhereNodeTopo.GetInterfacePtr()) CmdSelectWhereNodeTopo.Release();
   if (CmdSelectWhereNodesTopo.GetInterfacePtr()) CmdSelectWhereNodesTopo.Release();
   CmdSelectWhereKeySubList.remove_all();

   ClearNetStructure();
}

void C_TOPOLOGY::ClearNetStructure(void)
{
   InitForNetAnalysis();

   if (NodesVett)
      gsc_freeTopoNetNode(&NodesVett, &nNodesVett);

   if (LinksVett)
      gsc_freeTopoNetLink(&LinksVett, &nLinksVett);
}

int C_TOPOLOGY::get_type()                
   { return type; }

int C_TOPOLOGY::set_type(int in)          
{
   if (in != TYPE_NODE && in != TYPE_POLYLINE && in != TYPE_SURFACE)
      { GS_ERR_COD = eGSInvalidTopoType; return GS_BAD; }
   Terminate();
   type = in; 
   return GS_GOOD; 
}

int C_TOPOLOGY::is_temp() { return Temp; }

int C_TOPOLOGY::set_temp(int in)
{
   if (in != GS_GOOD && in != GS_BAD)
      { GS_ERR_COD = eGSInvalidTopoType; return GS_BAD; }
   Terminate();
   Temp = in; 
   return GS_GOOD; 
}

C_EXTERN *C_TOPOLOGY::get_cls()
   { return pCls; }

int C_TOPOLOGY::set_cls(C_CLASS *pMotherCls)
{
   if (!pMotherCls) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
   
   if (pMotherCls->get_category() != CAT_EXTERN)
   {
      if (pMotherCls->is_subclass() == GS_BAD)
         { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
      Terminate();
      pCls = (C_EXTERN *) ((C_PROJECT *) pMotherCls->ptr_id()->pPrj)->find_class(pMotherCls->ptr_id()->code);
   }
   else
   {
      Terminate();
      pCls = (C_EXTERN *) pMotherCls;
   }

   return GS_GOOD;
}

C_INT_LONG_LIST *C_TOPOLOGY::ptr_NetNodes()
   { return &NetNodes; }

C_INT_LONG_LIST *C_TOPOLOGY::ptr_NetLinks()
   { return &NetLinks; }


/*********************************************************
/*.doc C_TOPOLOGY::getTopoTabName            <internal> */
/*+
  Questa funzione restituisce il nome della tabella della topologia.
  Parametri:
  C_DBCONNECTION *pConn;     Connessione OLE-DB
  int prj;
  int cls;
  const TCHAR *Catalog;
  const TCHAR *Schema;
  C_STRING       &TableRef;  Riferimento completo tabella della 
                             topologia (default = NULL)
     
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_TOPOLOGY::getTopoTabName(C_DBCONNECTION *pConn, int prj, int cls,
                               const TCHAR *Catalog, const TCHAR *Schema, C_STRING &TableRef)
{
   C_STRING Table;
   TCHAR    *cod36prj, *cod36cls;

   switch (type)
   {
      case TYPE_POLYLINE:
         Table = LINKTOPOTABLENAME;
         break;
      case TYPE_NODE:
      case TYPE_SURFACE:
      default:
         GS_ERR_COD = eGSInvalidTopoType;
         return GS_BAD;
   }

   if (gsc_long2base36(&cod36prj, prj, 2) == GS_BAD) return GS_BAD;
   if (gsc_long2base36(&cod36cls, cls, 3) == GS_BAD) { free(cod36prj); return GS_BAD; }
   Table += cod36prj;
   Table += cod36cls;
   
   free(cod36prj); free(cod36cls);
   
   if (TableRef.paste(pConn->get_FullRefTable(Catalog, Schema, Table.get_name())) == NULL)
      return GS_BAD;
   
   return GS_GOOD;
}


/*********************************************************
/*.doc C_TOPOLOGY::getTopoTabInfo            <internal> */
/*+
  Questa funzione restituisce il nome della tabella della topologia.
  Parametri:
  C_DBCONNECTION **pConn;     Puntatore a connessione OLE-DB
  C_STRING       *pTableRef;  Riferimento completo tabella della 
                              topologia (default = NULL)
  int Create;                 Flag di creazione tabella (default = GS_BAD)
     
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_TOPOLOGY::getTopoTabInfo(C_DBCONNECTION **pConn, C_STRING *TableRef, int Create)
{
   C_SUB      *pSub = NULL;
   C_STRING   Catalog, Schema, Table, Tref, Buff;

   // prima sottoclasse
   if ((pSub = (C_SUB *) pCls->ptr_sub_list()->get_head()) == NULL) return GS_BAD;
   if (is_temp() == GS_GOOD)
   {
      if ((*pConn = pSub->ptr_info()->getDBConnection(TEMP)) == GS_BAD) return GS_BAD;
      if (pSub->getTempTableRef(Tref, GS_BAD) == GS_BAD) return GS_BAD;
   }
   else
   {
      if ((*pConn = pSub->ptr_info()->getDBConnection(OLD)) == GS_BAD) return GS_BAD;
      Tref = pSub->ptr_info()->OldTableRef;
   }

   if ((*pConn)->split_FullRefTable(Tref, Catalog, Schema, Table) == GS_BAD)
      return GS_BAD;

   if (getTopoTabName(*pConn, pCls->ptr_id()->pPrj->get_key(), pCls->ptr_id()->code,
                      Catalog.get_name(), Schema.get_name(), Buff) == GS_BAD)
      return GS_BAD;

   if (TableRef) TableRef->set_name(Buff.get_name());

   if (Create == GS_GOOD)
   {
      C_SUB    *pSub = NULL;
      C_STRING ProviderDescrByte, ProviderDescrLong, KeyAttr_ProviderDescr;
      long     Sub_len, KeyAttr_len;
      int      Sub_dec, KeyAttr_dec;

      switch (type)
      {
         case TYPE_NODE:
            break;
         case TYPE_POLYLINE:
            // se NON c'è la tabella la creo
            if ((*pConn)->ExistTable(Buff) == GS_BAD)
            {
               C_RB_LIST stru;
            
               // Converto campo da codice ADO in Provider dipendente
               if ((*pConn)->Type2ProviderType(adUnsignedTinyInt,   // DataType per campo sottoclasse
                                               FALSE,               // IsLong
                                               FALSE,               // IsFixedPrecScale
                                               RTT,                 // IsFixedLength
                                               TRUE,                // IsWrite
                                               3,                   // Size
                                               0,                   // Prec
                                               ProviderDescrByte,   // ProviderDescr
                                               &Sub_len, &Sub_dec) == GS_BAD) return GS_BAD;

               // converto campo da codice ADO in Provider dipendente (numero intero)
               if ((*pConn)->Type2ProviderType(CLASS_KEY_TYPE_ENUM, // DataType per campo chiave tab. classi
                                               FALSE,               // IsLong
                                               FALSE,               // IsFixedPrecScale
                                               RTT,                 // IsFixedLength
                                               TRUE,                // IsWrite
                                               CLASS_LEN_KEY_ATTR,  // Size
                                               0,                   // Prec
                                               KeyAttr_ProviderDescr, // ProviderDescr
                                               &KeyAttr_len, &KeyAttr_dec) == GS_BAD)
                  return GS_BAD;
            
               if ((stru << acutBuildList(RTLB,
                                          RTLB,
                                          RTSTR, _T("LINK_SUB"),                 // LINK_SUB
                                          RTSTR, ProviderDescrByte.get_name(),   // <tipo>
                                          RTSHORT, Sub_len,                      // <len>
                                          RTSHORT, Sub_dec,                      // <dec>
                                          RTLE,

                                          RTLB,
                                          RTSTR, _T("LINK_ID"),                    // LINK_ID
                                          RTSTR, KeyAttr_ProviderDescr.get_name(), // <tipo>
                                          RTSHORT, KeyAttr_len,                    // <len>
                                          RTSHORT, KeyAttr_dec,                    // <dec>
                                          RTLE, 
         
                                          RTLB,
                                          RTSTR, _T("INIT_SUB"),                 // INIT_SUB
                                          RTSTR, ProviderDescrByte.get_name(),   // <tipo>
                                          RTSHORT, Sub_len,                      // <len>
                                          RTSHORT, Sub_dec,                      // <dec>
                                          RTLE,  
            
                                          RTLB,
                                          RTSTR, _T("INIT_ID"),                    // INIT_ID
                                          RTSTR, KeyAttr_ProviderDescr.get_name(), // <tipo>
                                          RTSHORT, KeyAttr_len,                    // <len>
                                          RTSHORT, KeyAttr_dec,                    // <dec>
                                          RTLE, 
      
                                          RTLB,
                                          RTSTR, _T("FINAL_SUB"),                // FINAL_SUB
                                          RTSTR, ProviderDescrByte.get_name(),   // <tipo>
                                          RTSHORT, Sub_len,                      // <len>
                                          RTSHORT, Sub_dec,                      // <dec>
                                          RTLE,
            
                                          RTLB,
                                          RTSTR, _T("FINAL_ID"),                   // FINAL_ID
                                          RTSTR, KeyAttr_ProviderDescr.get_name(), // <tipo>
                                          RTSHORT, KeyAttr_len,                    // <len>
                                          RTSHORT, KeyAttr_dec,                    // <dec>
                                          RTLE, 
                                          RTLE, 0)) == NULL) return GS_BAD;
            
               // Creo la tabella   
               if ((*pConn)->CreateTable(Buff.get_name(), stru.get_head()) == GS_BAD) return GS_BAD;

               // Creo gli indici
               if (reindex() == GS_BAD) return GS_BAD;
            }
            break;
         case TYPE_SURFACE:
            break;
         default:
            GS_ERR_COD = eGSInvalidTopoType;
            return GS_BAD;
      }
   }

   return GS_GOOD;
}


/*********************************************************
/*.doc C_TOPOLOGY::OLD2TEMP                  <internal> */
/*+
  Questa funzione copia la tabella della topologia OLD in TEMP
  se non già esistente.
     
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_TOPOLOGY::OLD2TEMP(void)
{          
   C_STRING       OldTableRef, TempTableRef;
   C_DBCONNECTION *pOldConn, *pTempConn;

   // Topo TEMP
   set_temp(GS_GOOD);
   if (getTopoTabInfo(&pTempConn, &TempTableRef, GS_BAD) == GS_BAD) return GS_BAD;
   // Se esiste già la tabella
   if (pTempConn->ExistTable(TempTableRef) == GS_GOOD) return GS_GOOD;

   // Topo OLD
   set_temp(GS_BAD);
   if (getTopoTabInfo(&pOldConn, &OldTableRef, GS_GOOD) == GS_BAD) return GS_BAD;

   // Creo e copio intera tabella topologia da OLD in TEMP
   return pOldConn->CopyTable(OldTableRef.get_name(),
                              *pTempConn, TempTableRef.get_name());
}


/*********************************************************
/*.doc C_TOPOLOGY::synchronize               <external> */
/*+
  Funzione per la sincronizzazione della topologia.
  Verifica che esistano gli elementi indicati nella topologia.
  La funzione non blocca la classe quindi il controllo è demandato 
  alla funzione chiamante.
  Parametri:
  int CounterToVideo;        flag, se = GS_GOOD stampa a video il numero di entità che si 
                             stanno elaborando (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_TOPOLOGY::synchronize(int CounterToVideo)
{
   C_DBCONNECTION *pConn;
   C_STRING       statement, TableRef;
   _RecordsetPtr  pRs, pSubRs;
   C_RB_LIST      Row;
   presbuf        pLinkSub, pLinkID;
   presbuf        pInitSub, pInitID , pFinalSub, pFinalID;
   int            LinkSub, InitSub, FinalSub;
   long           LinkID, InitID, FinalID, i = 0, deleted = 0;
   C_SUB          *pSub;
   C_PREPARED_CMD_LIST OldCmdList;
   C_PREPARED_CMD      *pOldCmd;
   int                  IsOldRsCloseable;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(413)); // "Sincronizzazione topologia..." 

   if (!pCls) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if (getTopoTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;
   statement = _T("SELECT * FROM ");
   statement += TableRef;

   if (pConn->OpenRecSet(statement, pRs, adOpenForwardOnly, adLockPessimistic, ONETEST) == GS_BAD)
      return GS_BAD;
   // Preparo i vari puntatori per la lettura delle righe 
   if (gsc_InitDBReadRow(pRs, Row) == GS_BAD)
      { gsc_DBCloseRs(pRs); return GS_BAD; }
   pLinkSub  = Row.CdrAssoc(_T("LINK_SUB"));
   pLinkID   = Row.CdrAssoc(_T("LINK_ID"));
   pInitSub  = Row.CdrAssoc(_T("INIT_SUB"));
   pInitID   = Row.CdrAssoc(_T("INIT_ID"));
   pFinalSub = Row.CdrAssoc(_T("FINAL_SUB"));
   pFinalID  = Row.CdrAssoc(_T("FINAL_ID"));
   if (!pLinkSub || !pLinkID || !pInitSub || !pInitID || !pFinalSub || !pFinalID)
      { GS_ERR_COD = eGSInvalidSqlStatement; gsc_DBCloseRs(pRs); return GS_BAD; }

   // Preparo le select di interrogazione del database delle sottoclassi
   pSub = (C_SUB *) pCls->ptr_sub_list()->get_head();
   while (pSub)
   {
      if ((pOldCmd = new C_PREPARED_CMD()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      pOldCmd->set_key(pSub->ptr_id()->sub_code);
      if (pSub->prepare_data(*pOldCmd, OLD) == GS_BAD) return GS_BAD;
      OldCmdList.add_tail(pOldCmd);
	
      pSub = (C_SUB *) pSub->get_next();
   }

   if (CounterToVideo == GS_GOOD)
      StatusLineMsg.Init(gsc_msg(416), MEDIUM_STEP); // ogni 100 "%ld righe elaborate."

   // ciclo di lettura della topologia
   while (gsc_isEOF(pRs) == GS_BAD)
   {  // lettura record
      if (CounterToVideo == GS_GOOD)
         StatusLineMsg.Set(++i); // "%ld righe elaborate."

      if (gsc_DBReadRow(pRs, Row) == GS_BAD)
         { gsc_DBCloseRs(pRs); return GS_BAD; }
      // Codice della sottoclasse del link
      if (gsc_rb2Int(pLinkSub, &LinkSub) == GS_BAD) 
         { gsc_DBCloseRs(pRs); return GS_BAD; }
      // ID del link
      if (gsc_rb2Lng(pLinkID, &LinkID) == GS_BAD) 
         { gsc_DBCloseRs(pRs); return GS_BAD; }
      // Verifico che il link esista
      if (!(pOldCmd = (C_PREPARED_CMD *) OldCmdList.goto_pos(LinkSub)))
         { gsc_DBDelRow(pRs); gsc_Skip(pRs); deleted++; continue; } // cancello la riga e continuo
      if (gsc_get_data(*pOldCmd, LinkID, pSubRs, &IsOldRsCloseable) != GS_GOOD)
         { gsc_DBDelRow(pRs); gsc_Skip(pRs); deleted++; continue; } // cancello la riga e continuo
      if (IsOldRsCloseable == GS_GOOD) gsc_DBCloseRs(pSubRs);

      // Codice della sottoclasse del nodo iniziale
      if (gsc_rb2Int(pInitSub, &InitSub) == GS_BAD) 
         { gsc_DBCloseRs(pRs); return GS_BAD; }
      // ID del nodo iniziale
      if (gsc_rb2Lng(pInitID, &InitID) == GS_BAD) 
         { gsc_DBCloseRs(pRs); return GS_BAD; }
      // Verifico che il nodo iniziale esista
      if (!(pOldCmd = (C_PREPARED_CMD *) OldCmdList.goto_pos(InitSub)))
         { gsc_DBDelRow(pRs); gsc_Skip(pRs); deleted++; continue; } // cancello la riga e continuo
      if (gsc_get_data(*pOldCmd, InitID, pSubRs, &IsOldRsCloseable) != GS_GOOD)
         { gsc_DBDelRow(pRs); gsc_Skip(pRs); deleted++; continue; } // cancello la riga e continuo
      if (IsOldRsCloseable == GS_GOOD) gsc_DBCloseRs(pSubRs);

      // Codice della sottoclasse del nodo finale
      if (gsc_rb2Int(pFinalSub, &FinalSub) == GS_BAD) 
         { gsc_DBCloseRs(pRs); return GS_BAD; }
      // ID del nodo finale
      if (gsc_rb2Lng(pFinalID, &FinalID) == GS_BAD) 
         { gsc_DBCloseRs(pRs); return GS_BAD; }
      // Verifico che il nodo finale esista
      if (!(pOldCmd = (C_PREPARED_CMD *) OldCmdList.goto_pos(FinalSub)))
         { gsc_DBDelRow(pRs); gsc_Skip(pRs); deleted++; continue; } // cancello la riga e continuo
      if (gsc_get_data(*pOldCmd, FinalID, pSubRs, &IsOldRsCloseable) != GS_GOOD)
         { gsc_DBDelRow(pRs); gsc_Skip(pRs); deleted++; continue; } // cancello la riga e continuo
      if (IsOldRsCloseable == GS_GOOD) gsc_DBCloseRs(pSubRs);

      gsc_Skip(pRs);
   }
   gsc_DBCloseRs(pRs);

   if (CounterToVideo == GS_GOOD)
   {
      StatusLineMsg.End(gsc_msg(416), i); // "%ld righe elaborate."
      acutPrintf(GS_LFSTR);
      acutPrintf(gsc_msg(417), i, deleted); // "\nRighe elaborate %ld, scartate %ld."
      acutPrintf(GS_LFSTR);
   }

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gs_topocreate                                                          */
/*+
  Questa funzione crea una topologia GEOsim associata ad una classe simulazione.
  Per default viene creata una topologia di tipo rete.
  Lista : (<codice classe simulazione>[<tipo topologia>])

  Restituisce RTNORM in caso di successo altrimenti RTERROR.
-*/  
/******************************************************************************/
int gs_topocreate(void)
{
   presbuf    arg = acedGetArgs();
   C_TOPOLOGY topo;
   int        tipo = TYPE_POLYLINE;
   C_CLASS    *pClass;

   acedRetNil();

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return RTERROR; }

   // codice della classe simulazione
   if (arg == NULL || arg->restype != RTSHORT) // Errore argomenti
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if ((pClass = GS_CURRENT_WRK_SESSION->find_class(arg->resval.rint)) == NULL)
      return RTERROR;

   if ((arg = arg->rbnext) != NULL)
      if (arg->restype != RTSHORT) // Errore argomenti
         { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
      else tipo = arg->resval.rint;
   
   if (topo.set_type(tipo) == GS_BAD) return RTERROR;
   if (topo.set_cls(pClass) == GS_BAD) return RTERROR;
   // CounterToVideo
   if (topo.create(GS_GOOD) == GS_BAD) return RTERROR;

   acedRetT();

   return RTNORM;
}


/******************************************************************************/
/*.doc C_TOPOLOGY::create                                                     */
/*+
  Questa funzione crea una topologia GEOsim associata ad una classe simulazione.
  L'oggetto deve essere già inizializzato tramite le chiamate a <set_cls> (per
  stabilire su quale classe simulazione sarà elaborata la topologia) e <set_type>
  (per stabilire se la topologia è di tipo nodale, polilinea o superficie,
  per default si tratta di tipo polilinea).
  Per quanto riguarda la topologia di tipo polilinea, sarà creata una tabella
  (LINKTOPOTABLENAME) con 3 campi : LINK, INIT_NODE, FINAL_NODE.
  La tabella (con un indice per ogni campo) risiederà nello stesso direttorio
  delle tabelle delle sottoclassi e conterrà i codici delle entità GEOsim dei
  lati con i relativi nodi iniziali e finali.
  E' indispensabile che la classe simulazione sia interamente estratta altrimenti la
  topologia sarà parziale e potrà creare dei malfunzionamenti.
  Se ci si trova in una sessione di lavoro con classe estratta si considera
  la topologia temporanea altrimenti quella OLD.
  Paramteri:
  int CreateNewNodes;      Flag se = TRUE devono essere creati nuovi nodi dove
                           mancano (default = FALSE)
  int SubNewNodes;         Codice sottoclasse per i nuovi nodi (usato solo se
                           <CreateNewNodes> = TRUE). Se = 0 o se <SubNewNodes>
                           non indicasse una sottoclasse valida, la funzione prenderà
                           in considerazione la prima sottoclasse nodale a cui si
                           può connettere il link (default = 0).
  bool Interactive;        Opzionale, se true il programma chiede conferma all'utente
                           altrimenti non chiede nulla (default = true)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/******************************************************************************/
int C_TOPOLOGY::create(int CreateNewNodes, int SubNewNodes, bool Interactive)
{
   C_DBCONNECTION *pConn, *pConnSub;
   _RecordsetPtr  pRsSub;
   C_SUB          *pSub;
   C_ID           *pID;
   C_RB_LIST      default_values;
   C_SELSET       GraphObjs, ToAlign;
   ads_name       ent;
   presbuf        p = NULL;
   int            prj;
   long           i, Tot;
   C_STRING       TableRef;

   // verifico l'abilitazione dell' utente;
   if (gsc_check_op(opCreateTopo) == GS_BAD) return GS_BAD;

   if (!pCls) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   // se NON esiste una sessione di lavoro o la classe NON è estratta
   if (!GS_CURRENT_WRK_SESSION || pCls->is_extracted() == GS_BAD)
   {
      // Creo topologia OLD
      set_temp(GS_BAD);
      return getTopoTabInfo(&pConn, &TableRef, GS_GOOD);
   }

   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD) return GS_BAD;

   prj = GS_CURRENT_WRK_SESSION->get_PrjId();
   
   if (pCls->ptr_id()->abilit != GSUpdateableData)
   {
      GS_ERR_COD = (pCls->ptr_id()->abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
      return GS_BAD;
   }

   if (pCls->is_extracted() == GS_BAD) 
      { GS_ERR_COD = eGSNotClassExtr; return GS_BAD; } // se classe NON estratta

   if (Interactive)
   {
      // "\n*Attenzione* La classe deve essere interamente estratta altrimenti la topologia
      // sarà parziale e potrà creare dei malfunzionamenti."
      acutPrintf(gsc_msg(693));
      int result = GS_BAD;

      // "\nConfermare l'operazione."
      if (gsc_getconfirm(gsc_msg(529), &result, GS_BAD) != GS_GOOD || result != GS_GOOD)
         return GS_GOOD;
   }

   switch (type)
   {
      case TYPE_NODE:
         break;
      case TYPE_POLYLINE:
      {
         C_INT_LIST    Connected_to_List;
         C_INT         *pConnected_to;
         C_SELSET      LinearSS, PunctualSS;
         int           InitNodeSub, FinalNodeSub, IsDefCalc;
         long          LinkID, InitNodeID, FinalNodeID;
         long          LinksQty = 0, NewNodesQty = 0;
         ads_point     point;
         ads_name      Last;
         C_SUB         *pNode, *pNewNode = NULL;
         C_POINT_LIST  PtErrorList;
         C_BPOINT_OBJS *pPtObjs;
         C_POINT_OBJS_BTREE PtObjsBTree;
         C_CLS_PUNT    *pNodeCls;
         C_RB_LIST     param_list;
         C_LINK        Link;
         C_EED         eed;
         C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(692), pCls->get_name()); // "Creazione topologia rete per classe <%s>"
         long                      nSub = 0;

         // svuoto la topologia precedente
         if (getTopoTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;
         if (pConn->DelRows(TableRef.get_name()) == GS_BAD) return GS_BAD;
         
         pSub = (C_SUB *) pCls->ptr_sub_list()->get_head();
         StatusBarProgressMeter.Init(pCls->ptr_sub_list()->get_count());

         // ricavo tabella GS_DELETE 
         if (GS_CURRENT_WRK_SESSION->getDeletedTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;
         
         while (pSub)
         {
            StatusBarProgressMeter.Set(++nSub);

            pID = pSub->ptr_id();
            // scarto le sottoclassi che non sono lineari
            if (pID->type != TYPE_POLYLINE)
               { pSub = (C_SUB *) pSub->get_next(); continue; }

            C_STATUSBAR_PROGRESSMETER SubStatusBarProgressMeter(gsc_msg(324), pID->name); // "Analisi oggetti grafici sottoclasse <%s>"

            // ricavo le sottoclassi di tipo polilinea con connessioni topologiche
            Connected_to_List.remove_all();
            if (pCls->add_connected_2_sub(pID->sub_code, Connected_to_List) == GS_BAD)
               return GS_BAD;
            // scarto le sottoclassi che non hanno connessioni
            if (Connected_to_List.get_count() == 0)
               { pSub = (C_SUB *) pSub->get_next(); continue; }

            if (CreateNewNodes) // se è stato indicato di creare i nodi dove non esistono
            {
               if (SubNewNodes > 0) // se è stata indicata la sottoclasse
               {
                  C_CONNECT_LIST *pConnList = pSub->ptr_connect_list();
                  C_INT_INT      *pConn;

                  // verifico correttezza
                  if (pConnList &&
                      (pConn = (C_INT_INT *) pConnList->search_key(SubNewNodes)) &&
                      pConn->get_type() & CONCT_POINT)
                     pNewNode = (C_SUB *) pCls->ptr_sub_list()->C_LIST::search_key(SubNewNodes);
                  else
                     pNewNode = pCls->getJollyNodalSub(pSub->ptr_id()->sub_code);
               }
               else
                  pNewNode = pCls->getJollyNodalSub(pSub->ptr_id()->sub_code);
            }

            if (pNewNode)
            {
               // ricavo tabella riferimento temporanea
               if (pNewNode->getTempTableRef(TableRef) == GS_BAD) return GS_BAD;
               if ((pConnSub = pNewNode->ptr_info()->getDBConnection(TEMP)) == GS_BAD) return GS_BAD;
               if (pConnSub->InitInsRow(TableRef.get_name(), pRsSub) == GS_BAD) return GS_BAD;

               IsDefCalc = pNewNode->ptr_attrib_list()->is_DefCalculated();

               // leggo i valori di default
               if (pNewNode->get_default_values(default_values) == GS_BAD)
                  { gsc_DBCloseRs(pRsSub); return GS_BAD; } 
            }

            // filtro su tutti gli oggetti grafici della sottoclasse
            if (pSub->get_SelSet(LinearSS, GRAPHICAL) != GS_GOOD) 
            { 
               if (pNewNode) gsc_DBCloseRs(pRsSub); 
               return GS_BAD;
            }
            if (LinearSS.length() == 0)
               { pSub = (C_SUB *) pSub->get_next(); continue; }

            // ricavo un gruppo di selezione dei nodes
            pConnected_to = (C_INT *) Connected_to_List.get_head();
            while (pConnected_to)
            {
               pNode = (C_SUB *) pCls->ptr_sub_list()->C_LIST::search_key(pConnected_to->get_key());
               
               // filtro su tutti gli oggetti grafici della sottoclasse
               if (pNode->get_SelSet(GraphObjs, GRAPHICAL) != GS_GOOD)
               { 
                  if (pNewNode) gsc_DBCloseRs(pRsSub); 
                  return GS_BAD;
               }

               if (GraphObjs.length() > 0)
                  if (PunctualSS.add_selset(GraphObjs) == GS_BAD)
                  { 
                     if (pNewNode) gsc_DBCloseRs(pRsSub); 
                     return GS_BAD;
                  }

               pConnected_to = (C_INT *) pConnected_to->get_next();
            }
            
            // Memorizzo le entità e i punti di inserimento in un BTree
            if (PtObjsBTree.add_SelSet(PunctualSS) == GS_BAD)
            { 
               if (pNewNode) gsc_DBCloseRs(pRsSub); 
               return GS_BAD;
            }
            PunctualSS.clear(); // libero memoria

            Tot       = LinearSS.length();
            i         = 0;

            if (Tot > 0)
            {
               SubStatusBarProgressMeter.Init(Tot);
               // per ogni link leggo se esiste un nodo iniziale e finale
               while (LinearSS.entname(i++, ent) == GS_GOOD)
               {
                  SubStatusBarProgressMeter.Set(i);
                  
                  // nodo iniziale
                  if (gsc_get_firstPoint(ent, point) == GS_BAD)
                  { 
                     if (pNewNode) gsc_DBCloseRs(pRsSub); 
                     return GS_BAD;
                  }
                  // ottengo oggetti sul nodo iniziale del link
                  if ((pPtObjs = (C_BPOINT_OBJS *) PtObjsBTree.search(point)) == NULL)
                  { // non esiste un nodo iniziale
                     if (pNewNode)
                     {  
                        if (gsc_insert_new_node(pNewNode, point, Last, &InitNodeID,
                                                default_values, IsDefCalc, pRsSub) == GS_BAD)
                        {
                           // il nodo non può essere inserito per le regole di sovrapp.
                           if (GS_ERR_COD == eGSOverlapValidation)
                           {
                              PtErrorList.add_point(point);
                              LinksQty++;
                              continue;
                           }
                           if (pNewNode) gsc_DBCloseRs(pRsSub); 
                           return GS_BAD;
                        }
                        if (PtObjsBTree.add_ent(Last) == GS_BAD)
                        { 
                           if (pNewNode) gsc_DBCloseRs(pRsSub); 
                           return GS_BAD;
                        }
                        NewNodesQty++;
                        InitNodeSub = pNewNode->ptr_id()->sub_code;
                     }
                     else // link senza nodo e senza la possibilità di inserirne uno
                     {
                        PtErrorList.add_point(point);
                        LinksQty++;
                        continue;
                     }
                  }
                  else // esiste un nodo iniziale
                  {  // leggo il codice identificativo del nodo
                     pNodeCls = (C_CLS_PUNT *) pPtObjs->ptr_EntList()->get_head();
                     if ((InitNodeID = pNodeCls->get_gs_id()) == 0)
                     { 
                        if (pNewNode) gsc_DBCloseRs(pRsSub); 
                        return GS_BAD;
                     }

                     InitNodeSub = ((C_CLASS *) pNodeCls->get_class())->ptr_id()->sub_code;
                  }

                  // nodo finale
                  if (gsc_get_lastPoint(ent, point) == GS_BAD)
                  { 
                     if (pNewNode) gsc_DBCloseRs(pRsSub); 
                     return GS_BAD;
                  }
                  // ottengo oggetti sul nodo finale del link
                  if ((pPtObjs = (C_BPOINT_OBJS *) PtObjsBTree.search(point)) == NULL)
                  { // non esiste un nodo finale
                     if (pNewNode)
                     {
                        if (gsc_insert_new_node(pNewNode, point, Last, &FinalNodeID,
                                                default_values, IsDefCalc, pRsSub) == GS_BAD)
                        { 
                           // il nodo non può essere inserito per le regole di sovrapp.
                           if (GS_ERR_COD == eGSOverlapValidation)
                           {
                              PtErrorList.add_point(point);
                              LinksQty++;
                              continue;
                           }
                           if (pNewNode) gsc_DBCloseRs(pRsSub);
                           return GS_BAD;
                        }
                        if (PtObjsBTree.add_ent(Last) == GS_BAD)
                        { 
                           gsc_DBCloseRs(pRsSub);  
                           return GS_BAD;
                        }

                        NewNodesQty++;
                        FinalNodeSub = pNewNode->ptr_id()->sub_code;
                     }
                     else // link senza nodo e senza la possibilità di inserirne uno
                     {
                        PtErrorList.add_point(point);
                        LinksQty++;
                        continue;
                     }
                  }
                  else // esiste un nodo finale
                  {  // leggo il codice identificativo del nodo
                     pNodeCls = (C_CLS_PUNT *) pPtObjs->ptr_EntList()->get_head();
                     if ((FinalNodeID = pNodeCls->get_gs_id()) == 0) 
                     { 
                        if (pNewNode) gsc_DBCloseRs(pRsSub); 
                        return GS_BAD;
                     }
                     FinalNodeSub = ((C_CLASS *) pNodeCls->get_class())->ptr_id()->sub_code;
                  }

                  // leggo codice identificativo del link
                  if (pSub->getKeyValue(ent, &LinkID) == GS_BAD)
                  { 
                     if (pNewNode) gsc_DBCloseRs(pRsSub); 
                     return GS_BAD;
                  }

                  // Inserisco in tabella topologia
                  if (editlink(pID->sub_code, LinkID, ent, InitNodeSub, InitNodeID,
                               FinalNodeSub, FinalNodeID) == GS_BAD) 
                  { 
                     if (pNewNode) gsc_DBCloseRs(pRsSub); 
                     return GS_BAD;
                  }
                  ToAlign.add(ent);
                  LinksQty++;
               }
            }

            // Chiudo i record set
            gsc_DBCloseRs(pRsSub);

            pSub = (C_SUB *) pSub->get_next();
         }
         StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

         acutPrintf(gsc_msg(698), LinksQty); // "\n%ld collegamenti elaborati."
         if (pNewNode)
            acutPrintf(gsc_msg(699), NewNodesQty); // "\n%ld nuovi nodi inseriti."

         if (PtErrorList.get_count() > 0)
         {
            C_POINT *pPt = (C_POINT *) PtErrorList.get_head();

            acutPrintf(gsc_msg(700)); // "\nTopologia non valida nei seguenti punti:"
            while (pPt)
            {
               acutPrintf(_T("\nX=%f, Y=%f, Z=%f"), pPt->point[X], pPt->point[Y], pPt->point[Z]);
               pPt = (C_POINT *) pPt->get_next();
            }
         }

         GEOsimAppl::REFUSED_SS.clear();
         //              selset,change_fas,AttribValuesFromVideo,CounterToVideo,tipo modifica
         gsc_class_align(ToAlign, GS_BAD, GS_BAD, &(GEOsimAppl::REFUSED_SS), GS_GOOD, RECORD_MOD);
      
         // Verifica che i nodi con connessione non siano isolati
         EraseIsolatedNodes(PtObjsBTree, GS_GOOD);

         break;
      }
      case TYPE_SURFACE:
         break;
      default:
         GS_ERR_COD = eGSInvalidTopoType;
         return GS_BAD;
   }
   
   return GS_GOOD;
}


/******************************************************************************/
/*.doc C_TOPOLOGY::EraseIsolatedNodes                                         */
/*+
  Questa funzione cancella i nodi che per esistere necessitano di essere
  collegati a un'altro oggetto e che invece risultano isolati.
  Parametri:
  C_SELSET &SelSet;
  int CounterToVideo;        flag, se = GS_GOOD stampa a video il numero di entità che si 
                             stanno elaborando (default = GS_BAD)
  oppure
  C_POINT_OBJS_BTREE &PtObjsBTree;
  int CounterToVideo;        flag, se = GS_GOOD stampa a video il numero di entità che si 
                             stanno elaborando (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/******************************************************************************/
int C_TOPOLOGY::EraseIsolatedNodes(C_SELSET &SelSet, int CounterToVideo)
{
   C_POINT_OBJS_BTREE PtObjsBTree;

   PtObjsBTree.add_SelSet(SelSet);

   return EraseIsolatedNodes(PtObjsBTree, CounterToVideo);
}
int C_TOPOLOGY::EraseIsolatedNodes(C_POINT_OBJS_BTREE &PtObjsBTree, int CounterToVideo)
{
   int           TopoClsCode = pCls->ptr_id()->code;
   long          i = 0;
   C_BPOINT_OBJS *pPtObjs;
   C_POINT_LIST  PtErrorList;
   C_CLS_PUNT    *pNodeCls;
   C_LINK        Link;
   C_EED         eed;
   presbuf       p;
   ads_point     point;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(122)); // "Controllo topologico nodi isolati"

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.Init(PtObjsBTree.get_count());

   // Ciclo sui nodi per verificare che i nodi con connessione non siano isolati
   pPtObjs = (C_BPOINT_OBJS *) PtObjsBTree.go_top();
   while (pPtObjs)
   {  // leggo il codice identificativo del nodo
      if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.Set(++i);

      pNodeCls = (C_CLS_PUNT *) pPtObjs->ptr_EntList()->get_head();
      while (pNodeCls)
      {
         // non è un blocco attributi visibili
         if (gsc_is_DABlock(pNodeCls->ent) == GS_BAD &&
             // stessa simulazione di quella della topologia corrente
             ((C_SUB *) pNodeCls->get_class())->ptr_id()->code == TopoClsCode && 
             // la sottoclasse nodale necessita di collegamenti topologici
             ((C_SUB *) pNodeCls->get_class())->ptr_connect_list()->is_to_be_connected() == GS_GOOD)
            if ((p = elemadj(pNodeCls->get_gs_id(), TYPE_POLYLINE)) == NULL)
            {
               // leggo coordinate del nodo isolato
               if (gsc_get_firstPoint(pNodeCls->ent, point) == GS_GOOD)
                  PtErrorList.add_point(point);

               if (((C_SUB *) pNodeCls->get_class())->erase_data(pNodeCls->ent) == GS_BAD)
               {
                  Link.erase(pNodeCls->ent);
                  eed.clear(pNodeCls->ent);
                  gsc_EraseEnt(pNodeCls->ent);
               }
            }
            else
               acutRelRb(p);

         pNodeCls = (C_CLS_PUNT *) pNodeCls->get_next();
      }

      pPtObjs = (C_BPOINT_OBJS *) PtObjsBTree.go_next();
   }
   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

   if (PtErrorList.get_count() > 0)
   {
      // Notifico in file log
      TCHAR Msg[MAX_LEN_MSG];

      C_POINT *pPt = (C_POINT *) PtErrorList.get_head();

      if (CounterToVideo == GS_GOOD)
         acutPrintf(gsc_msg(700)); // "\nTopologia non valida nei seguenti punti:"

      while (pPt)
      {
         swprintf(Msg, MAX_LEN_MSG, _T("Isolated object located on point %f,%f,%f."),
                  pPt->point[X], pPt->point[Y], pPt->point[Z]);
         gsc_write_log(Msg);

         if (CounterToVideo == GS_GOOD)
            acutPrintf(_T("\nX=%f, Y=%f, Z=%f"), pPt->point[X], pPt->point[Y], pPt->point[Z]);

         pPt = (C_POINT *) pPt->get_next();
      }
   }

   return GS_GOOD;
}


/******************************************************************************/
/*.doc C_TOPOLOGY::remove                                                     */
/*+
  Questa funzione rimuove una topologia GEOsim associata ad una classe simulazione.
  L'oggetto deve essere già inizializzato tramite le chiamate a <set_cls> (per
  stabilire su quale classe simulazione sarà elaborata la topologia) e <set_type>
  (per stabilire se la topologia è di tipo nodale, polilinea o superficie,
  per default si tratta di tipo polilinea).
  Se ci si trova in una sessione di lavoro con classe estratta si considera
  la topologia temporanea altrimenti quella OLD.

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/******************************************************************************/
int C_TOPOLOGY::remove(void)
{
   C_DBCONNECTION *pConn;
   C_STRING       TableRef;

   if (getTopoTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;
   if (pConn->ExistTable(TableRef) == GS_GOOD)
      if (pConn->DelTable(TableRef.get_name()) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_TOPOLOGY::PrepareSelectWhereLinkTopo            */
/*+
  Questa funzione prepara il comando SQL per la ricerca 
  di un record nella tabella della topologia cercando un link

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_TOPOLOGY::PrepareSelectWhereLinkTopo(void)
{
   if (CmdSelectWhereLinkTopo.pCmd.GetInterfacePtr() == NULL &&
       CmdSelectWhereLinkTopo.pRs.GetInterfacePtr() == NULL)
   {
      C_DBCONNECTION *pConn;
      C_STRING       statement, TableRef;
      C_STRING       Catalog, Schema, IndexRef;
      _ParameterPtr  pParam;
      bool           RsOpened = false;
      HRESULT        hr = E_FAIL;

      if (getTopoTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

      // 1) il metodo seek è attualmente supportato solo da JET
      // 2) se si prova ad aprire un recordset con la modalità adCmdTableDirect
      //    mentre si è in una transazione questa viene automaticamente abortita
      //    con PostgreSQL
      if (gsc_strcmp(pConn->get_DBMSName(), ACCESS_DBMSNAME, FALSE) == 0)
      {
         // verifico la possibilità di usare il metodo seek

         // nome indice per campo chiave di ricerca
         if (pConn->split_FullRefTable(TableRef, Catalog, Schema, IndexRef) == GS_BAD)
            return GS_BAD;

         if (pConn->get_StrCaseFullTableRef() == Lower) IndexRef += _T("l");
         else IndexRef += _T("L");

         if (FAILED(CmdSelectWhereLinkTopo.pRs.CreateInstance(__uuidof(Recordset))))
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

	      try
         {
            C_STRING NoPrefSufTableRef(TableRef), InitQuotedId, FinalQuotedId;

            // Se la connessione ole-db non supporta l'uso del catalogo e dello schema
            if (pConn->get_CatalogUsage() == 0 && pConn->get_SchemaUsage() == 0)
            {
               // rimuove se esiste il prefisso e il suffisso (es. per ACCESS = [ ])
               InitQuotedId = pConn->get_InitQuotedIdentifier();
               FinalQuotedId = pConn->get_FinalQuotedIdentifier();
               NoPrefSufTableRef.removePrefixSuffix(InitQuotedId.get_name(), FinalQuotedId.get_name());
            }

            CmdSelectWhereLinkTopo.pRs->CursorLocation = adUseServer;
            // apro il recordset
            if (FAILED(hr = CmdSelectWhereLinkTopo.pRs->Open(NoPrefSufTableRef.get_name(),
                                                             pConn->get_Connection().GetInterfacePtr(), 
                                                             adOpenDynamic, 
                                                             adLockOptimistic, 
                                                             adCmdTableDirect)))
               return GS_BAD;
            RsOpened = true;
         
            if (CmdSelectWhereLinkTopo.pRs->Supports(adIndex) &&
                CmdSelectWhereLinkTopo.pRs->Supports(adSeek))
            {
               // Setto l'indice di ricerca
               if (gsc_DBSetIndexRs(CmdSelectWhereLinkTopo.pRs, IndexRef.get_name()) == GS_GOOD)
                  return GS_GOOD;
               else
                  gsc_DBCloseRs(CmdSelectWhereLinkTopo.pRs);
            }
            else
               // non supportato
               gsc_DBCloseRs(CmdSelectWhereLinkTopo.pRs);
         }

	      catch (_com_error)
         {
            if (RsOpened) gsc_DBCloseRs(CmdSelectWhereLinkTopo.pRs);
            else CmdSelectWhereLinkTopo.pRs.Release();
         }
      }

      // se non è possibile usare il metodo seek 
      // Preparo la select di interrogazione del database della topologia
      statement = _T("SELECT * FROM ");
      statement += TableRef;

      if (pConn->get_StrCaseFullTableRef() == Lower) statement += _T(" WHERE link_id=?");
      else statement += _T(" WHERE LINK_ID=?");

      if (gsc_PrepareCmd(pConn->get_Connection(), statement, CmdSelectWhereLinkTopo.pCmd) == GS_BAD)
         return GS_BAD;

      if (gsc_CreateDBParameter(pParam, _T("?"), adDouble) == GS_BAD) return GS_BAD;
      CmdSelectWhereLinkTopo.pCmd->Parameters->Append(pParam);
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_TOPOLOGY::SelectWhereLinkTopo            */
/*+
  Questa funzione cerca un link SQL nella tabella della topologia.
  Parametri:
  long Key;             codice link
  _RecordsetPtr &pRs;   recordset risultato della ricerca
  int *IsRsCloseable;   Flag; GS_GOOD se il recordset è richiudibile

  Ritorna GS_GOOD in caso in caso di successo altrimenti GS_BAD;
-*/  
/*********************************************************/
int C_TOPOLOGY::SelectWhereLinkTopo(long LinkID, _RecordsetPtr &pRs, int *IsRsCloseable)
{
   // Verifico se posso usare il metodo seek
   if (CmdSelectWhereLinkTopo.pRs.GetInterfacePtr())
   {
      _variant_t KeyForSeek(LinkID);

      *IsRsCloseable = GS_BAD;
      if (gsc_DBSeekRs(CmdSelectWhereLinkTopo.pRs, KeyForSeek, adSeekFirstEQ) == GS_BAD)
         return GS_BAD;
      pRs = CmdSelectWhereLinkTopo.pRs;
   }
   else
   if (CmdSelectWhereLinkTopo.pCmd.GetInterfacePtr())
   {
      *IsRsCloseable = GS_GOOD;
      // Imposto i vari parametri 
      if (gsc_SetDBParam(CmdSelectWhereLinkTopo.pCmd, 0, LinkID) == GS_BAD)
         return GS_BAD;
      // Esegue l'istruzione SQL
      // prima era adOpenKeyset poi adOpenDynamic ma PostgreSQL
      // in una transazione fa casino (al secondo recordset che viene aperto)
      if (gsc_ExeCmd(CmdSelectWhereLinkTopo.pCmd, pRs, adOpenForwardOnly, adLockOptimistic) == GS_BAD)
         return GS_BAD;
   }
   else
      return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_TOPOLOGY::PrepareSelectWhereNodeTopo            */
/*+
  Questa funzione prepara il comando SQL per la ricerca 
  di un record nella tabella della topologia cercando un nodo
-*/  
/*********************************************************/
int C_TOPOLOGY::PrepareSelectWhereNodeTopo(void)
{
   if (CmdSelectWhereNodeTopo.GetInterfacePtr() == NULL)
   {
      C_DBCONNECTION *pConn;
      C_STRING       statement, TableRef;
      _ParameterPtr  pParam;

      if (getTopoTabInfo(&pConn, &TableRef) == GS_BAD) return NULL;

      // Preparo la select di interrogazione del database della topologia
      statement = _T("SELECT * FROM ");
      statement += TableRef;
      if (pConn->get_StrCaseFullTableRef() == Lower) statement += _T(" WHERE init_id=? OR final_id=?");
      else statement += _T(" WHERE INIT_ID=? OR FINAL_ID=?");

      if (gsc_PrepareCmd(pConn->get_Connection(), statement, CmdSelectWhereNodeTopo) == GS_BAD) return NULL;

      if (gsc_CreateDBParameter(pParam, _T("INIT_ID"), adDouble) == GS_BAD) return GS_BAD;
      CmdSelectWhereNodeTopo->Parameters->Append(pParam);
      if (gsc_CreateDBParameter(pParam, _T("FINAL_ID"), adDouble) == GS_BAD) return GS_BAD;
      CmdSelectWhereNodeTopo->Parameters->Append(pParam);
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_TOPOLOGY::PrepareSelectWhereNodeSTopo           */
/*+
  Questa funzione prepara il comando SQL per la ricerca 
  di un record nella tabella della topologia cercando links
  con 2 nodi noti.
-*/  
/*********************************************************/
int C_TOPOLOGY::PrepareSelectWhereNodesTopo(void)
{
   if (CmdSelectWhereNodesTopo.GetInterfacePtr() == NULL)
   {
      C_DBCONNECTION *pConn;
      C_STRING       statement, TableRef;
      _ParameterPtr  pParam;

      if (getTopoTabInfo(&pConn, &TableRef) == GS_BAD) return NULL;

      // Preparo la select di interrogazione del database della topologia
      statement = _T("SELECT * FROM ");
      statement += TableRef;
      if (pConn->get_StrCaseFullTableRef() == Lower)
         statement += _T(" WHERE (init_id=? AND final_id=?) OR (final_id=? AND init_id=?)");
      else 
         statement += _T(" WHERE (INIT_ID=? AND FINAL_ID=?) OR (FINAL_ID=? AND INIT_ID=?)");

      if (gsc_PrepareCmd(pConn->get_Connection(), statement, CmdSelectWhereNodesTopo) == GS_BAD) return NULL;

      if (gsc_CreateDBParameter(pParam, _T("INIT_ID1"), adDouble) == GS_BAD) return GS_BAD;
      CmdSelectWhereNodesTopo->Parameters->Append(pParam);
      if (gsc_CreateDBParameter(pParam, _T("FINAL_ID1"), adDouble) == GS_BAD) return GS_BAD;
      CmdSelectWhereNodesTopo->Parameters->Append(pParam);
      if (gsc_CreateDBParameter(pParam, _T("FINAL_ID2"), adDouble) == GS_BAD) return GS_BAD;
      CmdSelectWhereNodesTopo->Parameters->Append(pParam);
      if (gsc_CreateDBParameter(pParam, _T("INIT_ID2"), adDouble) == GS_BAD) return GS_BAD;
      CmdSelectWhereNodesTopo->Parameters->Append(pParam);
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc  C_TOPOLOGY::PrepareInsertTopo                    */
/*+
  Questa funzione restituisce un RecordSer per l'inserimento 
  in tabella topologia.
-*/  
/*********************************************************/
int C_TOPOLOGY::PrepareInsertTopo(void)
{
   if (RsInsertTopo.GetInterfacePtr() == NULL)
   {
      C_DBCONNECTION *pConn;
      C_STRING       TableRef;
      
      if (getTopoTabInfo(&pConn, &TableRef) == GS_BAD) return NULL;
      return pConn->InitInsRow(TableRef.get_name(), RsInsertTopo);
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_TOPOLOGY::PrepareSelectWhereKeyNodalSubList     */
/*+
  Questa funzione prepara il comando SQL per la ricerca 
  di un record nella tabella della topologia cercando un nodo
-*/  
/*********************************************************/
int C_TOPOLOGY::PrepareSelectWhereKeyNodalSubList(void)
{
   if (CmdSelectWhereKeySubList.get_count() == 0)
   {
      C_PREPARED_CMD *pCmd;
      C_SUB          *pSub;

      // Preparo le select di interrogazione del database delle sottoclassi
      pSub = (C_SUB *) pCls->ptr_sub_list()->get_head();
      while (pSub)
      {
			if (pSub->get_type() == TYPE_NODE)
			{
				if ((pCmd = new C_PREPARED_CMD()) == NULL)
					{ GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
				pCmd->set_key(pSub->ptr_id()->sub_code);
				if (pSub->prepare_data(*pCmd, TEMP) == GS_BAD) return GS_BAD;
				CmdSelectWhereKeySubList.add_tail(pCmd);
			}
         pSub = (C_SUB *) pSub->get_next();
      }
   }

   return GS_GOOD;
}


/******************************************************************************/
/*.doc C_TOPOLOGY::reindex                                                    */
/*+
  Questa funzione reindicizza una topologia GEOsim.
  L'oggetto deve essere già inizializzato tramite le chiamate a <set_cls> (per
  stabilire su quale classe simulazione sarà elaborata la topologia) e <set_type>
  (per stabilire se la topologia è di tipo nodale, polilinea o superficie,
  per default si tratta di tipo polilinea).

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/******************************************************************************/
int C_TOPOLOGY::reindex(void)
{
   C_DBCONNECTION *pConn;
   C_STRING       TableRef, Catalog, Schema, Table, TempRif;
   presbuf        IndexList;

   // Ricavo la tabella della topologia
   if (getTopoTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

   // memorizzo gli indici utilizzati dalla tabella originale
   if (pConn->getTableIndexes(TableRef.get_name(), &IndexList, ERASE) == GS_GOOD && IndexList) 
   {
      if (pConn->DelIndex(TableRef.get_name(), IndexList) == GS_BAD) 
         { acutRelRb(IndexList); return GS_BAD; }
      acutRelRb(IndexList);
   }

   // Effettuo la reindicizzazione 
   if (pConn->split_FullRefTable(TableRef, Catalog, Schema, Table) == GS_BAD)
      return GS_BAD;

   // creo indice per LINK_ID
   TempRif = Table;
   TempRif += _T("L");
   // Creo l' indice
   if (TempRif.paste(pConn->getNewFullRefIndex(Catalog.get_name(),
                                               Schema.get_name(),
                                               TempRif.get_name(),
                                               NULL)) == NULL)
      return GS_BAD;
   if (pConn->CreateIndex(TempRif.get_name(), TableRef.get_name(), _T("LINK_ID"), INDEX_UNIQUE) == GS_BAD) 
      return GS_BAD;

   // creo indice per INIT_ID
   TempRif = Table;
   TempRif += _T("I");
   // Creo l' indice
   if (TempRif.paste(pConn->getNewFullRefIndex(Catalog.get_name(),
                                               Schema.get_name(),
                                               TempRif.get_name(),
                                               NULL)) == NULL)
      return GS_BAD;
   if (pConn->CreateIndex(TempRif.get_name(), TableRef.get_name(), _T("INIT_ID"), INDEX_NOUNIQUE) == GS_BAD) 
      return GS_BAD;

   // creo indice per FINAL_ID
   TempRif = Table;
   TempRif += _T("F");
   // Creo l' indice
   if (TempRif.paste(pConn->getNewFullRefIndex(Catalog.get_name(),
                                               Schema.get_name(),
                                               TempRif.get_name(),
                                               NULL)) == NULL)
      return GS_BAD;
   if (pConn->CreateIndex(TempRif.get_name(), TableRef.get_name(), _T("FINAL_ID"), INDEX_NOUNIQUE) == GS_BAD) 
      return GS_BAD;

   return GS_GOOD;
}


/******************************************************************************/
/*.doc C_TOPOLOGY::elemadj                                                    */
/*+
  Questa funzione ricava una lista di coppie di valori (contenenti
  l'identificatore e il codice della sottoclasse a cui si riferisce;
  es. ((<sub1> <id1>)(<sub2> <id2>))) delle entità adiacenti ad un oggetto
  specificato.
  La topologia deve essere già aperta in modalità QUERY.
  Parametri:
  long elem_id;   codice elemento
  int adj_type;   tipo di elementi adiacenti; i valori possibili sono:
                  TYPE_NODE, TYPE_POLYLINE, TYPE_SURFACE

  Restituisce una lista di resbuf in caso di successo altrimenti NULL.
-*/  
/******************************************************************************/
presbuf C_TOPOLOGY::elemadj(long elem_id, int adj_type)
{
   _RecordsetPtr pRs;
   C_RB_LIST     result, Row;
   presbuf       pLinkSub, pLinkID;
   presbuf       pInitSub, pInitID , pFinalSub, pFinalID;
   int           LinkSub, InitSub, FinalSub;
   long          LinkID, InitID, FinalID;
     
   if ((result << acutBuildList(RTLB, 0)) == NULL) return NULL;

   switch (type)
   {
      case TYPE_NODE:
         break;
      case TYPE_POLYLINE:
         if (adj_type == TYPE_POLYLINE) // ricerca i lati adiacenti il nodo
         {
            // Ricavo il comando che esegue la select sul database della topologia per nodo
            if (PrepareSelectWhereNodeTopo() == GS_BAD) return NULL;
            // Imposto i vari parametri 
            if (gsc_SetDBParam(CmdSelectWhereNodeTopo, 0, elem_id) == GS_BAD || // ID del nodo iniziale
                gsc_SetDBParam(CmdSelectWhereNodeTopo, 1, elem_id) == GS_BAD)   // ID del nodo finale
               return NULL;
            // Esegue l'istruzione SQL
            // prima era adOpenKeyset poi adOpenDynamic ma PostgreSQL
            // in una transazione fa casino (al secondo recordset che viene aperto)
            if (gsc_ExeCmd(CmdSelectWhereNodeTopo, pRs, adOpenForwardOnly, adLockOptimistic) == GS_BAD)
               return NULL;

            // Preparo i vari puntatori per la lettura delle righe 
            if (gsc_InitDBReadRow(pRs, Row) == GS_BAD)
               { gsc_DBCloseRs(pRs); return NULL; }
            pLinkSub = Row.get_head()->rbnext->rbnext->rbnext;
            pLinkID  = pLinkSub->rbnext->rbnext->rbnext->rbnext;

            // Scorro il record set risultato della select
            while (gsc_isEOF(pRs) == GS_BAD)
            {  // lettura record
               if (gsc_DBReadRow(pRs, Row) == GS_BAD)
                  { gsc_DBCloseRs(pRs); return NULL; }
               // Codice della sottoclasse del link
               if (gsc_rb2Int(pLinkSub, &LinkSub) == GS_BAD) 
                  { gsc_DBCloseRs(pRs); return NULL; }
               // ID del link
               if (gsc_rb2Lng(pLinkID, &LinkID) == GS_BAD) 
                  { gsc_DBCloseRs(pRs); return NULL; }
               // Verifico che il link esista
               if (gsc_isExistingEnt(pCls->ptr_id()->code, LinkSub, LinkID) == GS_GOOD)
                  if ((result += acutBuildList(RTLB, 
                                                     RTSHORT, LinkSub, RTLONG, LinkID,
                                               RTLE, 0)) == NULL)
                     { GS_ERR_COD = eGSOutOfMem; gsc_DBCloseRs(pRs); return NULL; }

               gsc_Skip(pRs);
            }
            gsc_DBCloseRs(pRs);
         }
         else 
         if (adj_type == TYPE_NODE) // ricerca i nodi adiacenti il lato
         {
            int  IsRsCloseable;

            // se VAL2 = 1 ricerca per LINK_ID (quindi va usato VAL1)
            // se VAL4 = 1 ricerca per INIT_ID (quindi va usato VAL3)
            // se VAL6 = 1 ricerca per FINAL_ID  (quindi va usato VAL5)
            // Ricavo il comando che esegue la select sul database della topologia per link
            if (PrepareSelectWhereLinkTopo() == GS_BAD) return NULL;
            // Eseguo la ricerca
            if (SelectWhereLinkTopo(elem_id, pRs, &IsRsCloseable) == GS_BAD) return NULL;

            // Preparo i vari puntatori per la lettura delle righe 
            if (gsc_InitDBReadRow(pRs, Row) == GS_BAD)
               { if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs); return NULL; }
            // Codice della sottoclasse del nodo iniziale
            pInitSub  = Row.CdrAssoc(_T("INIT_SUB"));
            // ID del nodo iniziale
            pInitID   = Row.CdrAssoc(_T("INIT_ID"));
            // Codice della sottoclasse del nodo finale
            pFinalSub = Row.CdrAssoc(_T("FINAL_SUB"));
            // ID del nodo finale
            pFinalID  = Row.CdrAssoc(_T("FINAL_ID"));

            // Verifico il record set risultato della select
            if (gsc_isEOF(pRs) == GS_BAD)
            {  // lettura record
               if (gsc_DBReadRow(pRs, Row) == GS_BAD)
                  { if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs); return NULL; }

               // Codice sottoclasse nodo iniziale
               if (gsc_rb2Int(pInitSub, &InitSub) == GS_BAD) InitSub = 0;
               // ID del nodo iniziale
               if (gsc_rb2Lng(pInitID, &InitID) == GS_BAD) InitID = 0;
               // Verifico che il nodo iniziale esista
               if (gsc_isExistingEnt(pCls->ptr_id()->code, InitSub, InitID) == GS_GOOD)
                   if ((result += acutBuildList(RTLB, 
                                                     RTSHORT, InitSub, RTLONG, InitID,
                                                RTLE, 0)) == NULL)
                   {
                      if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
                      return NULL;
                   }

               // Codice sottoclasse nodo finale
               if (gsc_rb2Int(pFinalSub, &FinalSub) == GS_BAD) FinalSub = 0;
               // ID del nodo finale
               if (gsc_rb2Lng(pFinalID, &FinalID) == GS_BAD) FinalID = 0;
               // Verifico che il nodo finale esista
               if (gsc_isExistingEnt(pCls->ptr_id()->code, FinalSub, FinalID) == GS_GOOD)
                   if ((result += acutBuildList(RTLB, 
                                                     RTSHORT, FinalSub, RTLONG, FinalID,
                                                RTLE, 0)) == NULL)
                   {
                      if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
                      return NULL;
                   }

               gsc_Skip(pRs);
            }
            if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
         }
         else
            { GS_ERR_COD = eGSInvalidTopoType; return NULL; }

         break;
      case TYPE_SURFACE:
         break;
      default:
         GS_ERR_COD = eGSInvalidTopoType;
         return NULL;
   }

   if (result.GetCount() <= 1) return NULL;
      
   if ((result += acutBuildList(RTLE, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }

   result.ReleaseAllAtDistruction(GS_BAD);

   return result.get_head();
}


/******************************************************************************/
/*.doc C_TOPOLOGY::LinksBetween                                               */
/*+
  Questa funzione ricava una lista di coppie di valori (contenenti
  l'identificatore e il codice della sottoclasse a cui si riferisce;
  es. ((<sub1> <id1>)(<sub2> <id2>))) dei link tra 2 nodi specificati.
  La topologia deve essere già aperta in modalità QUERY.
  Parametri:
  long id1;   codice nodo
  long id2;   codice nodo

  Restituisce una lista di resbuf in caso di successo altrimenti NULL.
-*/  
/******************************************************************************/
presbuf C_TOPOLOGY::LinksBetween(long id1, long id2)
{
   _RecordsetPtr pRs;
   C_RB_LIST     result, Row;
   presbuf       pLinkSub, pLinkID;
   int           LinkSub;
   long          LinkID;
     
   if ((result << acutBuildList(RTLB, 0)) == NULL) return NULL;

   switch (type)
   {
      case TYPE_NODE:
         break;
      case TYPE_POLYLINE:
         // Ricavo il comando che esegue la select sul database della topologia per nodo
         if (PrepareSelectWhereNodesTopo() == GS_BAD) return NULL;
         // Imposto i vari parametri 
         if (gsc_SetDBParam(CmdSelectWhereNodesTopo, 0, id1) == GS_BAD || // ID del nodo iniziale
             gsc_SetDBParam(CmdSelectWhereNodesTopo, 1, id2) == GS_BAD || // ID del nodo finale
             gsc_SetDBParam(CmdSelectWhereNodesTopo, 2, id1) == GS_BAD || // ID del nodo iniziale
             gsc_SetDBParam(CmdSelectWhereNodesTopo, 3, id2) == GS_BAD)   // ID del nodo finale
            return NULL;
         // Esegue l'istruzione SQL
         // prima era adOpenKeyset poi adOpenDynamic ma PostgreSQL
         // in una transazione fa casino (al secondo recordset che viene aperto)
         if (gsc_ExeCmd(CmdSelectWhereNodesTopo, pRs, adOpenForwardOnly, adLockOptimistic) == GS_BAD)
            return NULL;

         // Preparo i vari puntatori per la lettura delle righe 
         if (gsc_InitDBReadRow(pRs, Row) == GS_BAD)
            { gsc_DBCloseRs(pRs); return NULL; }
         pLinkSub = Row.get_head()->rbnext->rbnext->rbnext;
         pLinkID  = pLinkSub->rbnext->rbnext->rbnext->rbnext;

         // Scorro il record set risultato della select
         while (gsc_isEOF(pRs) == GS_BAD)
         {  // lettura record
            if (gsc_DBReadRow(pRs, Row) == GS_BAD)
               { gsc_DBCloseRs(pRs); return NULL; }
            // Codice della sottoclasse del link
            if (gsc_rb2Int(pLinkSub, &LinkSub) == GS_BAD) 
               { gsc_DBCloseRs(pRs); return NULL; }
            // ID del link
            if (gsc_rb2Lng(pLinkID, &LinkID) == GS_BAD) 
               { gsc_DBCloseRs(pRs); return NULL; }
            if ((result += acutBuildList(RTLB, 
                                            RTSHORT, LinkSub, RTLONG, LinkID,
                                         RTLE, 0)) == NULL)
               { GS_ERR_COD = eGSOutOfMem; gsc_DBCloseRs(pRs); return NULL; }

            gsc_Skip(pRs);
         }
         gsc_DBCloseRs(pRs);
         break;
      case TYPE_SURFACE:
         break;
      default:
         GS_ERR_COD = eGSInvalidTopoType;
         return NULL;
   }

   if (result.GetCount() <= 1) return NULL;
      
   if ((result += acutBuildList(RTLE, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }

   result.ReleaseAllAtDistruction(GS_BAD);

   return result.get_head();
}


/******************************************************************************/
/*.doc C_TOPOLOGY::elemqty                                                    */
/*+
  Questa funzione ricava il numero di entità di un tipo specificato presenti
  nella topologia.
  La topologia deve essere già aperta in modalità QUERY.
  Parametri:
  int elem_type;   tipo di elementi; i valori possibili sono:
                  TYPE_NODE, TYPE_POLYLINE, TYPE_SURFACE

  Restituisce una numero >= 0 in caso di successo altrimenti -1.
-*/  
/******************************************************************************/
long C_TOPOLOGY::elemqty(int elem_type)
{
   C_DBCONNECTION   *pConn;
   _RecordsetPtr     pRs;
   C_STRING          TableRef, statement;
   long              result = -1;

   // Ricavo la tabella della topologia
   if (getTopoTabInfo(&pConn, &TableRef) == GS_BAD) return -1;

   switch (elem_type)
   {
      case TYPE_NODE:
      {
         // Costruisco lo statement di interrogazione tabella topologia
         statement = _T("SELECT DISTINCT ");
         if (pConn->get_StrCaseFullTableRef() == Lower)
            statement += _T("init_id AS ID FROM ");
         else
            statement += _T("INIT_ID AS ID FROM ");
         statement += TableRef.get_name();
         statement += _T(" UNION SELECT DISTINCT ");
         if (pConn->get_StrCaseFullTableRef() == Lower)
            statement += _T("final_id AS ID FROM ");
         else
            statement += _T("FINAL_ID AS ID FROM ");
         statement += TableRef.get_name();

         // Eseguo lo statement
         // prima era adOpenKeyset poi adOpenDynamic ma PostgreSQL
         // in una transazione fa casino (al secondo recordset che viene aperto)
         if (gsc_ExeCmd(pConn->get_Connection(), statement, pRs, adOpenForwardOnly, adLockOptimistic, ONETEST) == GS_BAD)
            return -1;
         // Conto le righe che soddisfano lo statement
         if (gsc_RowsQty(pRs, &result) == GS_BAD)
            { gsc_DBCloseRs(pRs); return -1; }
         gsc_DBCloseRs(pRs);
         break;
      }
      case TYPE_POLYLINE:
         if (pConn->ExistTable(TableRef) == GS_BAD)
	         return -1;
         break;
      case TYPE_SURFACE:
         break;
      default:
         GS_ERR_COD = eGSInvalidTopoType;
         return -1;
   }

   return result;
}


/******************************************************************************/
/*.doc gsc_freeTopoNetNode                                                    */
/*+
  Questa funzione rilascia il vettore dei nodi usato dalla funzione LoadInMemory.
  Parametri:
  TOPONET_NODE **NodesVett;   Vettore dei nodi
  long *nNodes;               Lunghezza vettore dei nodi

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/******************************************************************************/
void gsc_freeTopoNetNode(TOPONET_NODE **NodesVett, long *nNodes)
{
   if (!NodesVett || (*NodesVett)) return;

   gsc_freeTopoNetNodeAuxInfo(*NodesVett, *nNodes);

   for (long i = 0; i < *nNodes; i++)
      if (((*NodesVett)[i]).pLinkList) delete ((*NodesVett)[i]).pLinkList;

   free(*NodesVett); *NodesVett = NULL;
   *nNodes = 0;

   return;
}


/******************************************************************************/
/*.doc gsc_freeTopoNetNodeAuxInfo                                             */
/*+
  Questa funzione rilascia le informazioni ausiliarie memorizzate in ciascun
  elemento del vettore dei nodi attraverso il puntatore punt.
  Parametri:
  TOPONET_NODE *NodesVett;   Vettore dei nodi
  long nNodes;               Lunghezza vettore dei nodi

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/******************************************************************************/
void gsc_freeTopoNetNodeAuxInfo(TOPONET_NODE *NodesVett, long nNodes)
{
   if (!NodesVett) return;

   for (long i = 0; i < nNodes; i++)
   {
      if (NodesVett[i].Punt) { free(NodesVett[i].Punt); NodesVett[i].Punt = NULL; }
      NodesVett[i].visited = false;
   }
}


/******************************************************************************/
/*.doc gsc_freeTopoNetLink                                                    */
/*+
  Questa funzione rilascia il vettore dei links usato dalla funzione LoadInMemory.
  Parametri:
  TOPONET_LINK **LinksVett;   Vettore dei links
  long *nLinks;               Lunghezza vettore dei links

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/******************************************************************************/
void gsc_freeTopoNetLink(TOPONET_LINK **LinksVett, long *nLinks)
{
   if (!LinksVett || (*LinksVett)) return;

   gsc_freeTopoNetLinkAuxInfo(*LinksVett, *nLinks);

   free(*LinksVett); *LinksVett = NULL;
   *nLinks = 0;

   return;
}


/******************************************************************************/
/*.doc gsc_freeTopoNetLinkAuxInfo                                             */
/*+
  Questa funzione rilascia le informazioni ausiliarie memorizzate in ciascun
  elemento del vettore dei link attraverso il puntatore punt.
  Parametri:
  TOPONET_LINK *LinksVett;   Vettore dei link
  long nLinks;               Lunghezza vettore dei link

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/******************************************************************************/
void gsc_freeTopoNetLinkAuxInfo(TOPONET_LINK *LinksVett, long nLinks)
{
   if (!LinksVett) return;

   for (long i = 0; i < nLinks; i++)
   {
      if (LinksVett[i].Punt) { free(LinksVett[i].Punt); LinksVett[i].Punt = NULL; }
      LinksVett[i].visited = false;
   }
}


/******************************************************************************/
/*.doc gsc_searchTopoNetNode e gsc_searchTopoNetLink                          */
/*+
  Queste funzioni cercano nel vettore dei nodi e dei link per gs_id.
  Parametri:
  TOPONET_NODE *NodesVett;   Vettore dei nodi da allocare
  long nNodes;               Lunghezza vettore dei nodi
  long gs_id;                gs_id del nodo da cercare
  
  oppure
  
  TOPONET_LINK *LinksVett;   Vettore dei link da allocare
  long nLinks;               Lunghezza vettore dei link
  long gs_id;                gs_id del link da cercare

  Restituisce la posizione nel vettore in caso di successo altrimenti -1.
-*/  
/******************************************************************************/
long gsc_searchTopoNetNode(TOPONET_NODE *NodesVett, long nNodes, long gs_id)
{
   if (!NodesVett) return -1;
   for (long i = 0; i < nNodes; i++)
      if (NodesVett[i].gs_id == gs_id) return i;

   return -1;
}
long gsc_searchTopoNetLink(TOPONET_LINK *LinksVett, long nLinks, long gs_id)
{
   if (!LinksVett) return -1;
   for (long i = 0; i < nLinks; i++)
      if (LinksVett[i].gs_id == gs_id) return i;

   return -1;
}


/******************************************************************************/
/*.doc gsc_prepareTopoCostCmdList                                             */
/*+
  Questa funzione compila le istruzioni contenute nella lista pCostSQLList.
  Parametri:
  C_EXTERN *pCls;                  Puntatore alla classe simulazione
  C_2STR_INT_LIST *pCostSQLList;   Lista delle istruzioni SQL
                                   La prima stringa indica quale campo 
                                   (o quale espressione) deve essere valutato 
                                   e la seconda la condizione SQL
  C_PREPARED_CMD_LIST &CostCmdList; Lista delle istruzioni precompilate

  Restituisce la posizione nel vettore in caso di successo altrimenti -1.
-*/  
/******************************************************************************/
int gsc_prepareTopoCostCmdList(C_EXTERN *pCls,
                               C_2STR_INT_LIST *pCostSQLList,
                               C_PREPARED_CMD_LIST &CostCmdList)
{
   C_CLASS        *pSub;
   C_2STR_INT     *pCostSQL = (C_2STR_INT *) pCostSQLList->get_head();
   C_PREPARED_CMD *pCmd;
   _ParameterPtr  pParam;
     
   CostCmdList.remove_all();
   while (pCostSQL)
   {
      if (!(pSub = (C_CLASS *) pCls->ptr_sub_list()->search_key(pCostSQL->get_type())))
         return GS_BAD;

      if (!(pCmd = new C_PREPARED_CMD()))
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      CostCmdList.add_tail(pCmd);
      pCmd->set_key(pCostSQL->get_type()); // codice sottoclasse
      if (pSub->prepare_data(pCmd->pCmd, TEMP, pCostSQL->get_name(), pCostSQL->get_name2()) == GS_BAD)
         return GS_BAD;

      int nParams = gsc_GetParamCount(pCmd->pCmd, TRUE);

      // pSub->prepare_data crea già un parametro (gs_id)
      for (int i = 1; i < nParams; i++)
      {
         // Creo e aggiungo un nuovo parametro numerico
         if (gsc_CreateDBParameter(pParam, _T("?"), adDouble) == GS_BAD) return GS_BAD;
         // Aggiungo parametro
         pCmd->pCmd->Parameters->Append(pParam);
      }

      pCostSQL = (C_2STR_INT *) pCostSQL->get_next();
   }

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_getTopoCost                                                        */
/*+
  Questa funzione compila le istruzioni contenute nella lista pCostSQLList.
  Se l'istruzione SQL non genera alcuna riga il costo sarà considerato il 
  limite massimo dei double (non percorribile). Se viene generata una o più righe
  il costo sarà contenuto nella prima colonna della prima riga.
  Parametri:
  C_PREPARED_CMD_LIST &CostCmdList; Lista delle istruzioni precompilate
  int Sub;                          Codice sottoclasse
  long gs_id;                       gs_id dell'elemento da trovare (nodo o link)
  long Param1;                      Se si tratta di un nodo,
                                    qualora venga richiesto dal comando precompilato,
                                    indica il gs_id del link che insiste su di esso
                                    Se si tratta di un link,
                                    qualora venga richiesto dal comando precompilato,
                                    indica il gs_id del nodo iniziale
  long Param2;                      Se si tratta di un link,
                                    qualora venga richiesto dal comando precompilato,
                                    indica il gs_id del nodo finale
  bool SecondItem;                  Se falso verrà utilizzata la prima istruzione
                                    trovata, se vero la seconda. Da utilizzare quando 
                                    si cerca il costo di un link da nodo finale a nodo
                                    iniziale (default = FALSE).

  Restituisce la posizione nel vettore in caso di successo altrimenti -1.
-*/  
/******************************************************************************/
double gsc_getTopoCost(C_PREPARED_CMD_LIST &CostCmdList, int Sub,
                       long gs_id, long Param1, long Param2, bool SecondItem = FALSE)
{
   C_PREPARED_CMD *pCostCmd;
   int            nParam;
   C_RB_LIST      ColValues;
   double         Cost;

   if (!(pCostCmd = (C_PREPARED_CMD *) CostCmdList.search_key(Sub))) return 0.0;
   
   if (SecondItem)
      // Cerco la seconda istruzione precompilata per la stessa sottoclasse
      if (!(pCostCmd = (C_PREPARED_CMD *) CostCmdList.search_next_key(Sub))) return 0.0;

   nParam = gsc_GetParamCount(pCostCmd->pCmd);

   if (nParam  == 0 || nParam  > 3) { GS_ERR_COD = eGSInitPar; return 0.0; }
     
   if (nParam  > 1)
   {
      if (gsc_SetDBParam(pCostCmd->pCmd, 1, Param1) == GS_BAD) return 0.0;

      if (nParam  > 2)
         if (gsc_SetDBParam(pCostCmd->pCmd, 2, Param2) == GS_BAD) return 0.0;
   }

   if (gsc_get_data(pCostCmd->pCmd, gs_id, ColValues) == GS_BAD)
      return (std::numeric_limits<double>::max)();

   if (gsc_rb2Dbl(gsc_nth(1, ColValues.nth(0)), &Cost) == GS_BAD) return 0.0;

   return Cost;
}


/******************************************************************************/
/*.doc C_TOPOLOGY::LoadInMemory                                               */
/*+
  Questa funzione carica la topologia di tipo rete in memoria attraverso
  un vettore di nodi (<NodesVett>) e uno di links (LinksVett) tra loro collegati.
  Parametri:
  C_2STR_INT_LIST *pCostSQLList; Parametro opzionale, se <> NULL rappresenta
                                 una lista di condizioni SQL (formato ACCESS)
                                 per calcolare, per ciascuna sottoclasse,
                                 il costo per il passaggio. 
                                 La prima stringa indica quale campo 
                                 (o quale espressione) deve essere valutato 
                                 e la seconda la condizione SQL.
                                 Per le sottoclassi di tipo polilinea 
                                 ci saranno 2 elementi consecutivi
                                 nella lista: il primo si riferirà al senso
                                 nodo iniziale -> nodo finale e il successivo
                                 si riferirà alla direzione inversa.
                                 Ciascuna istruzione SQL potrà avere fino a 3 parametri
                                 opzionali:

                                 - il primo parametro (obbligatorio) sarà considerato 
                                   il gs_id
                                 - se si tratta di un nodo il secondo parametro (opzionale)
                                   sarà considerato il gs_id del link che insiste su di esso
                                 - se si tratta di un link il secondo parametro (opzionale)
                                   sarà considerato il gs_id del nodo iniziale e il 
                                   terzo parametro (opzionale) il gs_id del nodo finale

                                 Se l'istruzione SQL non genera alcuna riga il costo
                                 sarà considerato il limite massimo dei double 
                                 (non percorribile). Se viene generata una o più righe
                                 il costo sarà contenuto nella prima colonna 
                                 della prima riga.

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/******************************************************************************/
int C_TOPOLOGY::LoadInMemory(C_2STR_INT_LIST *pCostSQLList)
{
   long                iLink, iNode;
   double              Cost;
   C_DBCONNECTION      *pConn;
   C_STRING            statement, TableRef;
   _RecordsetPtr       pRs;
   C_RB_LIST           ColValues;
   presbuf             pRbLinkSub, pRbLinkID, pRbInitID, pRbFinalID, pRbSubID, pRbNodeID;
   C_PREPARED_CMD_LIST CostCmdList;
   int                 res = GS_GOOD;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(441)); // "Caricamento topologia"

   StatusBarProgressMeter.Init(3);

   // Se esiste una lista di condizioni SQL da soddisfare
   if (pCostSQLList) // precompilo i comandi
      if (gsc_prepareTopoCostCmdList(pCls, pCostSQLList, CostCmdList) == GS_BAD)
         return GS_BAD;

   // Pulisco le strutture topologiche
   ClearNetStructure();

   do
   {
      if (getTopoTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

      // Carico vettore dei link
      statement = _T("SELECT * FROM ");
      statement += TableRef;
      
      // leggo le righe della tabella
      if (pConn->OpenRecSet(statement, pRs) == GS_BAD) { res = GS_BAD; break; }
      if (gsc_RowsQty(pRs, &nLinksVett) == GS_BAD) { res = GS_BAD; break; }
      if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD) { res = GS_BAD; break; }
      if ((LinksVett = (TOPONET_LINK *) malloc(sizeof(TOPONET_LINK) * nLinksVett)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; res = GS_BAD; break; }

      if (!(pRbLinkSub = ColValues.CdrAssoc(_T("LINK_SUB"))) ||
         !(pRbLinkID = ColValues.CdrAssoc(_T("LINK_ID"))) ||
         !(pRbInitID = ColValues.CdrAssoc(_T("INIT_ID"))) ||
         !(pRbFinalID = ColValues.CdrAssoc(_T("FINAL_ID"))))
         { res = GS_BAD; break; }

      iLink = 0;
      while (gsc_isEOF(pRs) == GS_BAD)
      {
         // leggo riga
         if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { res = GS_BAD; break; }
         if (gsc_rb2Int(pRbLinkSub, &((LinksVett[iLink]).sub)) == GS_BAD) (LinksVett[iLink]).sub = 0;
         if (gsc_rb2Lng(pRbLinkID, &((LinksVett[iLink]).gs_id)) == GS_BAD) (LinksVett[iLink]).gs_id = 0;
         // Inizializzo i seguenti valori con delle costanti
         LinksVett[iLink].visited = FALSE;
         LinksVett[iLink].Punt = NULL;

         // Memorizzo temporaneamente gli ID dei nodi iniziale e finale
         if (gsc_rb2Lng(pRbInitID, &(LinksVett[iLink].InitialNode)) == GS_BAD) LinksVett[iLink].InitialNode = 0;
         if (gsc_rb2Lng(pRbFinalID, &(LinksVett[iLink].FinalNode)) == GS_BAD) LinksVett[iLink].FinalNode = 0;
         
         // Se esistono condizioni sul link verifico se da questo
         // si può passare dal nodo iniziale a quello finale 
         // e con che costo (es. senso unico)
         Cost = gsc_getTopoCost(CostCmdList,
                                LinksVett[iLink].sub,   // codice sottoclasse
                                LinksVett[iLink].gs_id, // gs_id del link
                                LinksVett[iLink].InitialNode, // gs_id nodo iniziale
                                LinksVett[iLink].FinalNode); // gs_id nodo finale
         LinksVett[iLink].In2Fi_Cost = Cost;

         // Se esistono condizioni sul link verifico se da questo
         // si può passare dal nodo finale a quello iniziale
         // e con che costo (es. senso unico)
         Cost = gsc_getTopoCost(CostCmdList,
                                LinksVett[iLink].sub,   // codice sottoclasse
                                LinksVett[iLink].gs_id, // gs_id del link
                                LinksVett[iLink].FinalNode, // gs_id nodo finale
                                LinksVett[iLink].InitialNode, // gs_id nodo iniziale
                                TRUE); // cerco la seconda istruzione precompilata
                                       //  per la sottoclasse (indica costo da fin->init)
         LinksVett[iLink].Fi2In_Cost = Cost;

         iLink++;

         gsc_Skip(pRs);
      }
      if (res == GS_BAD) break;
      gsc_DBCloseRs(pRs);

      StatusBarProgressMeter.Set(1);

      // Carico vettore dei nodi
      if (pConn->get_StrCaseFullTableRef() == Lower)
      {
         statement = _T("SELECT init_sub AS SUB_ID, init_id AS NODE_ID FROM ");
         statement += TableRef;
         statement += _T(" UNION SELECT final_sub AS SUB_ID, final_id AS NODE_ID FROM ");
      }
      else
      {
         statement = _T("SELECT INIT_SUB AS SUB_ID, INIT_ID AS NODE_ID FROM ");
         statement += TableRef;
         statement += _T(" UNION SELECT FINAL_SUB AS SUB_ID, FINAL_ID AS NODE_ID FROM ");
      }
      statement += TableRef;

      // leggo le righe della tabella
      if (pConn->OpenRecSet(statement, pRs) == GS_BAD) { res = GS_BAD; break; }
      if (gsc_RowsQty(pRs, &nNodesVett) == GS_BAD){ res = GS_BAD; break; }
      if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD) { res = GS_BAD; break; }
      if ((NodesVett = (TOPONET_NODE *) malloc(sizeof(TOPONET_NODE) * nNodesVett)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; res = GS_BAD; break; }

      if (!(pRbSubID = ColValues.CdrAssoc(_T("SUB_ID"))) ||
         !(pRbNodeID = ColValues.CdrAssoc(_T("NODE_ID"))))
         { res = GS_BAD; break; }

      iNode = 0;
      while (gsc_isEOF(pRs) == GS_BAD)
      {
         // leggo riga
         if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { res = GS_BAD; break; }
         if (gsc_rb2Int(pRbSubID, &(NodesVett[iNode].sub)) == GS_BAD) NodesVett[iNode].sub = 0;
         if (gsc_rb2Lng(pRbNodeID, &(NodesVett[iNode].gs_id)) == GS_BAD) NodesVett[iNode].gs_id = 0;
         // Inizializzo i seguenti valori con delle costanti
         NodesVett[iNode].visited = FALSE;
         NodesVett[iNode].pLinkList = NULL;
         NodesVett[iNode].Punt = NULL;

         iNode++;

         gsc_Skip(pRs);
      }
      if (res == GS_BAD) break;
      gsc_DBCloseRs(pRs);
      StatusBarProgressMeter.Set(2);

      // Allineo i due vettori
      C_LONG_REAL *p;

      // Ciclo nel vettore dei link
      for (iLink = 0; iLink < nLinksVett; iLink++)
      {
         // Cerco il nodo iniziale nel vettore dei nodi
         for (iNode = 0; iNode < nNodesVett; iNode++)
            if (NodesVett[iNode].gs_id == LinksVett[iLink].InitialNode)
            {
               // Se esistono condizioni sul nodo verifico se da questo
               // si può passare al link in questione e con che costo (es. valvola chiusa)
               Cost = gsc_getTopoCost(CostCmdList,
                                      NodesVett[iNode].sub,   // codice sottoclasse
                                      NodesVett[iNode].gs_id, // gs_id del nodo
                                      LinksVett[iLink].gs_id, // gs_id link collegato al nodo
                                      0); // Non usato

               // Se non esiste la lista la alloco
               if (NodesVett[iNode].pLinkList == NULL)
                  if ((NodesVett[iNode].pLinkList = new C_LONG_REAL_LIST()) == NULL)
                     { GS_ERR_COD = eGSOutOfMem; res = GS_BAD; break; }
               // Aggiungo un link nella lista di collegamento
               if ((p = new C_LONG_REAL(iLink, Cost)) == NULL)
                  { GS_ERR_COD = eGSOutOfMem; res = GS_BAD; break; }
               NodesVett[iNode].pLinkList->add_tail(p);
               LinksVett[iLink].InitialNode = iNode;
               break;
            }

         if (res == GS_BAD) break;

         // Cerco il nodo finale nel vettore dei nodi
         for (iNode = 0; iNode < nNodesVett; iNode++)
            if (NodesVett[iNode].gs_id == LinksVett[iLink].FinalNode)
            {  
               // Se esistono condizioni sul nodo verifico se da questo
               // si può passare al link in questione e con che costo (es. valvola chiusa)
               Cost = gsc_getTopoCost(CostCmdList,
                                      NodesVett[iNode].sub,   // codice sottoclasse
                                      NodesVett[iNode].gs_id, // gs_id del nodo
                                      LinksVett[iLink].gs_id, // gs_id link collegato al nodo
                                      0); // Non usato
               
               // Se non esiste la lista la alloco
               if (NodesVett[iNode].pLinkList == NULL)
                  if ((NodesVett[iNode].pLinkList = new C_LONG_REAL_LIST()) == NULL)
                     { GS_ERR_COD = eGSOutOfMem; res = GS_BAD; break; }
               // Aggiungo un link nella lista di collegamento
               if ((p = new C_LONG_REAL(iLink, Cost)) == NULL)
                  { GS_ERR_COD = eGSOutOfMem; res = GS_BAD; break; }
               NodesVett[iNode].pLinkList->add_tail(p);
               LinksVett[iLink].FinalNode = iNode;
               break;
            }
      }
      if (res == GS_BAD) break;
      StatusBarProgressMeter.Set(3);
   }
   while (0);

   gsc_DBCloseRs(pRs);

   if (res == GS_BAD)
      // Pulisco le strutture topologiche
      ClearNetStructure();

   StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

   return res;
}


/******************************************************************************/
/*.doc C_TOPOLOGY::InitForNetAnalysis                                         */
/*+
  Questa funzione pulisce ed inizializza alcune strutture interne usate 
  dalle funzioni di anlisi di rete per poter analizzare ciclicamente la rete.
-*/  
/******************************************************************************/
void C_TOPOLOGY::InitForNetAnalysis()
{
   m_VisitedNodes.remove_all();
   m_VisitedLinks.remove_all();

   NetNodes.remove_all();
   NetLinks.remove_all();

   gsc_freeTopoNetNodeAuxInfo(NodesVett, nNodesVett);
   gsc_freeTopoNetLinkAuxInfo(LinksVett, nLinksVett);
}


/******************************************************************************/
/*.doc C_TOPOLOGY::GetNetPropagation                                          */
/*+
  Questa funzione ritorna una lista dei elementi collegati ad un nodo.
  Deve essere precedentemente settato:
  il membro <NetCost> che verrà utilizzato come costo limite da non superare
                      (usare il numero double massimo per trovare tutta la
                       rete connessa)

  I Risultati saranno letti dai membri della classe:
  <NetNodes> lista dei nodi coinvolti nella propagazione
  <NetLinks> lista dei links coinvolti nella propagazione

  Parametri:
  long iNode;                 Nodo da cui partire
  double ActualCost;          Opzionale; costo attuale (default = 0.0)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.

  N.B: Poichè la funzione alloca un costo per ogni nodo. Se si intende usare
  più volte questa funzione è necessario pulire i vettori <LinksVett> e 
  <NodesVett> dalle informazioni ausiliarie usate da questa funzione e 
  svuotare le liste <NetNodes> e <NetLinks> (usare la funzione InitForNetAnalysis()).
-*/  
/******************************************************************************/
int C_TOPOLOGY::GetNetPropagation(long iNode, double ActualCost)
{
   C_LONG_REAL *pLink;
   C_INT_LONG  *p;
   long        iAdjNode;
   double      LinkCost; // costo del passaggio su un link

   // Se non esiste il costo per arrivare fino a questo nodo
   if (NodesVett[iNode].Punt == NULL)
   {
      // Alloco un numero double per memorizzare il costo
      if ((NodesVett[iNode].Punt = (double *) malloc(sizeof(double))) == NULL)
         return GS_BAD;
   }
   else // se esiste già il costo
      // Se il costo attuale è maggiore o uguale non vado avanti
      // (c'è un altro percorso che da prestazioni migliori o uguali)
      if (ActualCost >= *((double *) NodesVett[iNode].Punt)) return GS_GOOD;

   // Memorizzo sul nodo il costo attuale
   *((double *) NodesVett[iNode].Punt) = ActualCost;

   // Se il nodo NON era già stato memorizzato nella lista (non voglio doppioni)
   if (!NodesVett[iNode].visited)
   {
      // Lo segno come già memorizzato nella lista
      NodesVett[iNode].visited = true;
      // Inserisco nella lista degli ID dei nodi visitati
      if ((p = new C_INT_LONG(NodesVett[iNode].sub, NodesVett[iNode].gs_id)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      NetNodes.add_tail(p);
   }

   // Lista dei link collegati
   if (NodesVett[iNode].pLinkList)
      pLink = (C_LONG_REAL*) NodesVett[iNode].pLinkList->get_head();
   else
      pLink = NULL;

   while (pLink)
   {
      // Se il costo per passare dal nodo al link è il limite massimo di un double
      // significa che non si può passare da quel link
      if (pLink->get_real() == (std::numeric_limits<double>::max)())
         { pLink = (C_LONG_REAL*) pLink->get_next(); continue; }

      // Costo per passare dal nodo al link
      ActualCost += pLink->get_real();

      // Se il costo attuale è maggiore o uguale al limite massimo
      // significa che non si può passare da quel link
      if (ActualCost > NetCost)
         { pLink = (C_LONG_REAL*) pLink->get_next(); continue; }

      // Se si sta attraversando il link dal nodo iniziale a quello finale
      if (LinksVett[pLink->get_id()].InitialNode == iNode)
      {
         // Se il costo per passare sul link è il limite massimo di un double
         // significa che non si può passare su quel link
         if (LinksVett[pLink->get_id()].In2Fi_Cost == (std::numeric_limits<double>::max)())
            { pLink = (C_LONG_REAL*) pLink->get_next(); continue; }
         // Aggiungo costo passare dal link
         LinkCost = LinksVett[pLink->get_id()].In2Fi_Cost;

         // Leggo nodo all'altro capo del link
         iAdjNode = LinksVett[pLink->get_id()].FinalNode;
      }
      else // altrimenti si sta attraversando il link dal nodo finale a quello iniziale
      {
         // Se il costo per passare sul link è il limite massimo di un double
         // significa che non si può passare su quel link
         if (LinksVett[pLink->get_id()].Fi2In_Cost == (std::numeric_limits<double>::max)())
            { pLink = (C_LONG_REAL*) pLink->get_next(); continue; }
         // Aggiungo costo passare dal link
         LinkCost = LinksVett[pLink->get_id()].Fi2In_Cost;

         // Leggo nodo all'altro capo del link
         iAdjNode = LinksVett[pLink->get_id()].InitialNode;
      }

      // Se il costo attuale è maggiore o uguale al limite massimo
      // significa che non si può passare da quel link
      if (ActualCost + LinkCost > NetCost)
         { pLink = (C_LONG_REAL*) pLink->get_next(); continue; }

      // Se il link NON era già stato memorizzato nella lista (non voglio doppioni)
      if (!LinksVett[pLink->get_id()].visited)
      {
         // Lo segno come già memorizzato nella lista
         LinksVett[pLink->get_id()].visited = true;
         // Inserisco nella lista dei link visitati
         if ((p = new C_INT_LONG(LinksVett[pLink->get_id()].sub, 
                                 LinksVett[pLink->get_id()].gs_id)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         NetLinks.add_tail(p);
      }

      // continuo la visita della rete
      if (GetNetPropagation(iAdjNode, ActualCost + LinkCost) == GS_BAD)
         break;               

      pLink = (C_LONG_REAL*) pLink->get_next();
   }

   return GS_GOOD;
}


/******************************************************************************/
/*.doc C_TOPOLOGY::GetShortestNetPath                                         */
/*+
  Questa funzione ritorna una lista degli elementi che determinano il percorso
  più breve tra due nodi. Deve essere precedentemente settato:
  il membro <NetFinalNode> con il codice del nodo finale
  il membro <NetCost> che verrà utilizzato come costo limite da non superare
                      (normalmente usare il numero double massimo)
  
  I Risultati saranno letti dai membri della classe:
  <NetNodes> lista dei nodi che compongono il percorso più breve
  <NetLinks> lista dei links che compongono il percorso più breve
  <NetCost> costo del percorso più breve (senza quello del nodo finale 
            perchè il costo di un nodo esiste solo in relazione
            al passaggio su di un link).

  Parametri:
  long iNode;                 nodo da cui partire (cambia di volta in volta)
  double ActualCost;             Opzionale; costo attuale (default = 0.0)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.

  N.B: Poichè la funzione alloca un costo per ogni nodo. Se si intende usare
  più volte questa funzione è necessario pulire i vettori <LinksVett> e 
  <NodesVett> dalle informazioni ausiliarie usate da questa funzione e 
  svuotare le liste <NetNodes> e <NetLinks> (usare la funzione InitForNetAnalysis()).
-*/  
/******************************************************************************/
int C_TOPOLOGY::GetShortestNetPath(long iNode, double ActualCost)
{
   C_LONG_REAL *pLink;
   C_INT_LONG  *p;
   long        iAdjNode;
   double      NodeLinkCost; // somma del costo per passare da nodo a link + costo link

   // Se non esiste il costo per arrivare fino a questo nodo
   if (NodesVett[iNode].Punt == NULL)
   {
      // Alloco un numero double per memorizzare il costo
      if ((NodesVett[iNode].Punt = (double *) malloc(sizeof(double))) == NULL)
         return GS_BAD;
   }
   else // se esiste già il costo
      // Se il costo attuale è maggiore o uguale non vado avanti
      // (c'è un altro percorso che da prestazioni migliori o uguali)
      if (ActualCost >= *((double *) NodesVett[iNode].Punt)) return GS_GOOD;
   
   // Memorizzo sul nodo il costo attuale
   *((double *) NodesVett[iNode].Punt) = ActualCost;

   // Inserisco nella lista degli ID dei nodi del percorso
   if ((p = new C_INT_LONG(NodesVett[iNode].sub, NodesVett[iNode].gs_id)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   m_VisitedNodes.add_tail(p);

   // Se il nodo è quello finale
   if (NodesVett[iNode].gs_id == NetFinalNode)
   {
      // Se il costo non supera quello massimo consentito
      if (ActualCost < NetCost)
      {
         // Copio il percorso attuale in quello definitivo
         m_VisitedNodes.copy(&NetNodes);
         m_VisitedLinks.copy(&NetLinks);
         NetCost = ActualCost;
         // Rimuovo dalla lista dei nodi del percorso il nodo finale
         m_VisitedNodes.remove_tail();
      }

      return GS_GOOD;
   }

   // Lista dei link collegati
   if (NodesVett[iNode].pLinkList)
      pLink = (C_LONG_REAL*) NodesVett[iNode].pLinkList->get_head();
   else
      pLink = NULL;

   while (pLink)
   {
      // Se il costo per passare dal nodo al link è il limite massimo di un double
      // significa che non si può passare da quel link
      if (pLink->get_real() == (std::numeric_limits<double>::max)())
         { pLink = (C_LONG_REAL*) pLink->get_next(); continue; }

      // Costo per passare dal nodo al link
      NodeLinkCost = pLink->get_real();

      // Se si sta attraversando il link dal nodo iniziale a quello finale
      if (LinksVett[pLink->get_id()].InitialNode == iNode)
      {
         // Se il costo per passare sul link è il limite massimo di un double
         // significa che non si può passare su quel link
         if (LinksVett[pLink->get_id()].In2Fi_Cost == (std::numeric_limits<double>::max)())
            { pLink = (C_LONG_REAL*) pLink->get_next(); continue; }
         // Aggiungo costo passare dal link
         NodeLinkCost += LinksVett[pLink->get_id()].In2Fi_Cost;

         // Leggo nodo all'altro capo del link
         iAdjNode = LinksVett[pLink->get_id()].FinalNode;
      }
      else // altrimenti si sta attraversando il link dal nodo finale a quello iniziale
      {
         // Se il costo per passare sul link è il limite massimo di un double
         // significa che non si può passare su quel link
         if (LinksVett[pLink->get_id()].Fi2In_Cost == (std::numeric_limits<double>::max)())
            { pLink = (C_LONG_REAL*) pLink->get_next(); continue; }
         // Aggiungo costo passare dal link
         NodeLinkCost += LinksVett[pLink->get_id()].Fi2In_Cost;

         // Leggo nodo all'altro capo del link
         iAdjNode = LinksVett[pLink->get_id()].InitialNode;
      }

      // Inserisco nella lista dei link del percorso
      if ((p = new C_INT_LONG(LinksVett[pLink->get_id()].sub, 
                              LinksVett[pLink->get_id()].gs_id)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      m_VisitedLinks.add_tail(p);
      
      // continuo la visita della rete
      if (GetShortestNetPath(iAdjNode, ActualCost + NodeLinkCost) == GS_BAD)
         break;

      // Rimuovo dalla lista dei link del percorso
      m_VisitedLinks.remove_tail();

      pLink = (C_LONG_REAL*) pLink->get_next();
   }

   // Rimuovo dalla lista dei nodi del percorso
   m_VisitedNodes.remove_tail();

   return GS_GOOD;
}


/******************************************************************************/
/*.doc C_TOPOLOGY::GetOptimizedNetPath                                        */
/*+
  Questa funzione ritorna una lista degli elementi che determinano il percorso
  più breve passando tra n nodi nell'ordine dato.
  il membro <NetCost> che verrà utilizzato come costo limite da non superare
                      (normalmente usare il numero double massimo)
  
  I Risultati saranno letti dai membri della classe:
  <NetNodes> lista dei nodi che compongono il percorso più breve
  <NetLinks> lista dei links che compongono il percorso più breve
  <NetCost> costo del percorso più breve (senza quello del nodo finale 
            perchè il costo di un nodo esiste solo in relazione
            al passaggio su di un link).

  Parametri:
  C_LONG_LIST &Nodes;   Lista di nodi da quello di partenza a quello di arrivo
                        da visitare nell'ordine indicato nella lista
  bool Ordered;         Flag di ricerca. Se = true la lista dei nodi va visitata
                        nello stesso ordine proposto dalla lista stessa.
                        Se = false si deve cercare il percorso ottimale per visitare
                        tutti i nodi qualsiasi sia il loro ordine (tranne il primo 
                        e l'ultimo della lista che determinano la partenza e l'arrivo)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/******************************************************************************/
int C_TOPOLOGY::GetOptimizedNetPath(C_LONG_LIST &Nodes, bool Ordered)
{
   if (Ordered)
      return GetOptimizedNetPathWithOrderedNodes(Nodes);
   else
      return GetOptimizedNetPathWithCasualNodes(Nodes);
}


/******************************************************************************/
/*.doc C_TOPOLOGY::GetOptimizedNetPathWithOrderedNodes             <internal> */
/*+
  Funzione interna per ricercare una lista degli elementi che determinano il percorso
  più breve passando tra n nodi nell'ordine dato dalla lista in input.
  il membro <NetCost> che verrà utilizzato come costo limite da non superare
                      (normalmente usare il numero double massimo)
  
  I Risultati saranno letti dai membri della classe:
  <NetNodes> lista dei nodi che compongono il percorso più breve
  <NetLinks> lista dei links che compongono il percorso più breve
  <NetCost> costo del percorso più breve (senza quello del nodo finale 
            perchè il costo di un nodo esiste solo in relazione
            al passaggio su di un link).

  Parametri:
  C_LONG_LIST &OrderedNodes;   Lista di nodi da quello di partenza a quello di arrivo
                               da visitare nell'ordine indicato nella lista

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/******************************************************************************/
int C_TOPOLOGY::GetOptimizedNetPathWithOrderedNodes(C_LONG_LIST &OrderedNodes)
{
   C_LONG          *pNode;
   long            StartNode;
   double          _TotalCost = 0;
   C_INT_LONG_LIST _TotalNetNodes;
   C_INT_LONG_LIST _TotalNetLinks;

   pNode = (C_LONG *) OrderedNodes.get_head();
   if (!pNode) return GS_BAD;
   StartNode = pNode->get_id();

   // la somma dei percorsi minimi tra i punti da il percorso ottimale
   // per raggiungere un punto partendo da un punto e passando attraverso una serie
   // di tappe ordinate
   pNode = (C_LONG *) pNode->get_next();
   while (pNode)
   {
      InitForNetAnalysis(); // re-inizializzo per analizzare il percorso più breve
      NetFinalNode = pNode->get_id(); // assegno il punto finale
      StartNode    = gsc_searchTopoNetNode(NodesVett, nNodesVett, StartNode);

      if (GetShortestNetPath(StartNode) == GS_BAD) return GS_BAD;
      
      if (NetNodes.get_count() > 0) // se esiste un percorso
      {
         // Aggiungo il percorso attuale a quello definitivo
         _TotalNetNodes.paste_tail(NetNodes);
         _TotalNetLinks.paste_tail(NetLinks);
         _TotalCost += NetCost;
      }

      // Il nodo finale diventa quello iniziale per il prossimo tratto
      StartNode = NetFinalNode;

      pNode = (C_LONG *) pNode->get_next();
   }

   // riassegno il risultato ai membri della classe
   _TotalNetNodes.copy(&NetNodes);
   _TotalNetLinks.copy(&NetLinks);
   NetCost = _TotalCost;

   return GS_GOOD;
}


/******************************************************************************/
/*.doc C_TOPOLOGY::GetOptimizedNetPathWithCasualNodes              <internal> */
/*+
  Funzione interna per ricercare una lista degli elementi che determinano il percorso
  più breve passando tra n nodi indipendentemente dall'ordine dato dalla lista in input.
  il membro <NetCost> che verrà utilizzato come costo limite da non superare
                      (normalmente usare il numero double massimo)
  
  I Risultati saranno letti dai membri della classe:
  <NetNodes> lista dei nodi che compongono il percorso più breve
  <NetLinks> lista dei links che compongono il percorso più breve
  <NetCost> costo del percorso più breve (senza quello del nodo finale 
            perchè il costo di un nodo esiste solo in relazione
            al passaggio su di un link).

  Parametri:
  C_LONG_LIST &CasualNodes;   Lista di nodi da quello da visitare
                              da visitare indipendentemente dall'ordine 
                              indicato nella lista (tranne il primo e l'ultimo
                              della lista che determinano la partenza e l'arrivo)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/******************************************************************************/
int C_TOPOLOGY::GetOptimizedNetPathWithCasualNodes(C_LONG_LIST &CasualNodes)
{
   return GS_GOOD;
}


/******************************************************************************/
/*.doc C_TOPOLOGY::editlink                                                   */
/*+
  Questa funzione aggiunge un link tra 2 nodi o, se già esistente lo aggiorna
  (per topologia di tipo polilinea).
  Parametri:
  Questa funzione aggiorna la tabella topologica leggendo il nodo iniziale e finale 
  dalla grafica (usata nel riallineamento generato dai reattori)
  int  elem_sub;        codice sottoclasse del link
  long elem_id;         id del link
  ads_name elem_ent;    entità grafica link

  oppure:

  Questa funzione aggiorna la tabella topologica allineando la grafica (se indicato dal
  parametro "elem_ent")
  int  elem_sub;        codice sottoclasse del link
  long elem_id;         id del link
  ads_name elem_ent;    entità grafica link
  C_RB_LIST &AdjNodes;  Lista di resbuf come ottenuta da "::elemadj"

  oppure:

  Questa funzione aggiorna la tabella topologica allineando la grafica (se indicato dal
  parametro "elem_ent")
  int  elem_sub;        codice sottoclasse del link
  long elem_id;         id del link
  ads_name elem_ent;    entità grafica link
  int  init_node_sub;   codice sottoclasse del nodo iniziale (se = 0 verrà trascurato
                        questo parametro e il successivo)
  long init_node_id;    id del nodo iniziale
  int  final_node_sub;  codice sottoclasse del nodo finale (se = 0 verrà trascurato
                        questo parametro e il successivo)
  long final_node_id;   id del nodo finale

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/******************************************************************************/
int C_TOPOLOGY::editlink(int elem_sub, long elem_id, ads_name elem_ent)
{
   C_EED          eed;
   int            init_node_sub = 0, final_node_sub = 0;
   long           init_node_id, final_node_id;
   C_PREPARED_CMD *pCmd;
   _RecordsetPtr  pRs;
   int            IsRsCloseable;

   if (!elem_ent && ads_name_nil(elem_ent))
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   // leggo il nodo iniziale e finale dalla grafica
   if (eed.load(elem_ent) == GS_BAD) return GS_BAD;
   // Ricavo la lista di comandi per interrogare le sottoclassi
   if (PrepareSelectWhereKeyNodalSubList() == GS_BAD) return GS_BAD;

   // Cerco la sottoclasse relativa al nodo iniziale
   init_node_id = eed.initial_node;
   pCmd = (C_PREPARED_CMD *) CmdSelectWhereKeySubList.get_head();
   while (pCmd)
   {
      if (gsc_get_data(*pCmd, init_node_id, pRs, &IsRsCloseable) == GS_GOOD)
      {
			init_node_sub = pCmd->get_key();
			if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
			break;
		}

      pCmd = (C_PREPARED_CMD *) pCmd->get_next();
   }
   if (!pCmd) { GS_ERR_COD = eGSInvalidKey; return GS_BAD; }

   // Cerco la sottoclasse relativa al nodo finale
   final_node_id = eed.final_node;
   pCmd = (C_PREPARED_CMD *) CmdSelectWhereKeySubList.get_head();
   while (pCmd)
   {
      if (gsc_get_data(*pCmd, final_node_id, pRs, &IsRsCloseable) == GS_GOOD)
      {
			final_node_sub = pCmd->get_key();
			if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
			break;
		}

      pCmd = (C_PREPARED_CMD *) pCmd->get_next();
   }
   if (!pCmd) { GS_ERR_COD = eGSInvalidKey; return GS_BAD; }

   return editlink(elem_sub, elem_id, elem_ent, init_node_sub, init_node_id,
                   final_node_sub, final_node_id);
}
int C_TOPOLOGY::editlink(int elem_sub, long elem_id, ads_name elem_ent, C_RB_LIST &AdjNodes)
{
   int     init_node_sub, final_node_sub;
   long    init_node_id, final_node_id;
   presbuf pNode, p;

   // Nodo iniziale
   if ((pNode = AdjNodes.nth(0)) == NULL)
      { GS_ERR_COD = eGSNoNodeToConnect; return GS_BAD; }
   if ((p = gsc_nth(0, pNode)) == NULL || gsc_rb2Int(p, &init_node_sub) == GS_BAD)
      { GS_ERR_COD = eGSNoNodeToConnect; return GS_BAD; }
   if ((p = gsc_nth(1, pNode)) == NULL || gsc_rb2Lng(p, &init_node_id) == GS_BAD)
      { GS_ERR_COD = eGSNoNodeToConnect; return GS_BAD; }
   // Nodo finale
   if ((pNode = AdjNodes.nth(1)) == NULL)
      { GS_ERR_COD = eGSNoNodeToConnect; return GS_BAD; }
   if ((p = gsc_nth(0, pNode)) == NULL || gsc_rb2Int(p, &final_node_sub) == GS_BAD)
      { GS_ERR_COD = eGSNoNodeToConnect; return GS_BAD; }
   if ((p = gsc_nth(1, pNode)) == NULL || gsc_rb2Lng(p, &final_node_id) == GS_BAD)
      { GS_ERR_COD = eGSNoNodeToConnect; return GS_BAD; }

   return editlink(elem_sub, elem_id, elem_ent, init_node_sub, init_node_id,
                   final_node_sub, final_node_id);
}
int C_TOPOLOGY::editlink(int elem_sub, long elem_id, ads_name elem_ent,
                         int init_node_sub, long init_node_id,
                         int final_node_sub, long final_node_id)
{
   C_RB_LIST      param_list;
   _RecordsetPtr  pRs;
   int            IsRsCloseable;
   C_DBCONNECTION *pConn;

   if (type != TYPE_POLYLINE)
      { GS_ERR_COD = eGSInvalidTopoType; return GS_BAD; }

   if (getTopoTabInfo(&pConn) == GS_BAD) return GS_BAD;

   if ((param_list << acutBuildList(RTLB,
                                       RTLB, RTSTR, _T("LINK_SUB"), RTSHORT, elem_sub, RTLE,
                                       RTLB, RTSTR, _T("LINK_ID"), RTLONG, elem_id, RTLE, 0)) == NULL)
      return GS_BAD;
   
   if (init_node_sub != 0)
      if ((param_list += acutBuildList(RTLB, RTSTR, _T("INIT_SUB"), RTSHORT, init_node_sub, RTLE,
                                       RTLB, RTSTR, _T("INIT_ID"), RTLONG, init_node_id, RTLE, 0)) == NULL)
         return GS_BAD;

   if (final_node_sub != 0)
      if ((param_list += acutBuildList(RTLB, RTSTR, _T("FINAL_SUB"), RTSHORT, final_node_sub, RTLE,
                                       RTLB, RTSTR, _T("FINAL_ID"), RTLONG, final_node_id, RTLE, 0)) == NULL)
         return GS_BAD;

   if ((param_list += acutBuildList(RTLE, 0)) == NULL)
      return GS_BAD;

   // Ricavo il comando che esegue la select sul database della topologia per link
   if (PrepareSelectWhereLinkTopo() == GS_BAD) return GS_BAD;

   // Eseguo la ricerca
   if (SelectWhereLinkTopo(elem_id, pRs, &IsRsCloseable) == GS_BAD) return GS_BAD;

   if (gsc_isEOF(pRs) == GS_BAD) // Link già esistente: lo aggiorno
   {
      // Aggiorno la riga risultante
      if (gsc_DBUpdRow(pRs, param_list) == GS_BAD)
         { if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs); return GS_BAD; }
      // Chiudo il record set
      if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
   }
   else // Link da inserire
   {         
      // Chiudo il record set
      if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);

      if (PrepareInsertTopo() == GS_BAD) return GS_BAD;
      // Aggiungo il nuovo link
      if (gsc_DBInsRow(RsInsertTopo, param_list, ONETEST, GS_BAD) == GS_BAD)
         return GS_BAD;
   }

   if (elem_ent && !ads_name_nil(elem_ent))
   {  // sostituisco nella eed nodo iniziale e finale
      C_EED eed;

      if (init_node_sub != 0 && final_node_sub != 0)
      {
         if (eed.set_initialfinal_node(elem_ent, init_node_id, final_node_id) == GS_BAD)
            return GS_BAD;
      }
      else if (init_node_sub != 0)
      {
         if (eed.set_initial_node(elem_ent, init_node_id) == GS_BAD)
            return GS_BAD;
      }
      else
         if (eed.set_final_node(elem_ent, final_node_id) == GS_BAD)
            return GS_BAD;

   }
   
   return GS_GOOD;
}


/******************************************************************************/
/*.doc C_TOPOLOGY::editdelelem                                                */
/*+
  Questa funzione cancella un elemento dalla topologia.
  La topologia deve essere già aperta in modalità ERASE.
  Parametri:
  long elem_id;      ID del link
  int  type;         Tipo di elemento; i valori possibili sono:
                     TYPE_NODE, TYPE_POLYLINE, TYPE_SURFACE
  ads_name elem_ent; Entità grafica link (solo per type = TYPE_POLYLINE)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/******************************************************************************/
int C_TOPOLOGY::editdelelem(long elem_id, int elem_type, ads_name elem_ent)
{
   _RecordsetPtr pRs;

   if (type != TYPE_POLYLINE)
      { GS_ERR_COD = eGSInvalidTopoType; return GS_BAD; }
   
   switch (elem_type)
   {
      case TYPE_NODE:
         // Ricavo il comando che esegue la select sul database della topologia per nodo
         if (PrepareSelectWhereNodeTopo() == GS_BAD) return GS_BAD;
         // Imposto i vari parametri 
         if (gsc_SetDBParam(CmdSelectWhereNodeTopo, 0, elem_id) == GS_BAD || // ID del nodo iniziale
             gsc_SetDBParam(CmdSelectWhereNodeTopo, 1, elem_id) == GS_BAD)   // ID del nodo finale
            return GS_BAD;
         // Esegue l'istruzione SQL
         // prima era adOpenKeyset poi adOpenDynamic ma PostgreSQL
         // in una transazione fa casino (al secondo recordset che viene aperto)
         if (gsc_ExeCmd(CmdSelectWhereNodeTopo, pRs, adOpenForwardOnly, adLockPessimistic) == GS_BAD)
            return GS_BAD;

         if (gsc_DBDelRows(pRs) == GS_BAD) { gsc_DBCloseRs(pRs); return GS_BAD; }
         // Chiudo il record set
         gsc_DBCloseRs(pRs);

         break;
      case TYPE_POLYLINE:
      {
         C_RB_LIST Row;
         presbuf   pLinkID;
         long      LinkID;
         int       IsRsCloseable;

         // Ricavo il comando che esegue la select sul database della topologia per link
         if (PrepareSelectWhereLinkTopo() == GS_BAD) return GS_BAD;
         // Eseguo la ricerca
         if (SelectWhereLinkTopo(elem_id, pRs, &IsRsCloseable) == GS_BAD) return GS_BAD;

         if (elem_ent && !ads_name_nil(elem_ent))
         {  // sostituisco nella eed nodo iniziale e finale
            C_EED eed;
            if (eed.set_initialfinal_node(elem_ent, 0, 0) == GS_BAD) return GS_BAD;
         }

         // Preparo il puntatore per la lettura delle righe 
         if (gsc_InitDBReadRow(pRs, Row) == GS_BAD)
            { if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs); return GS_BAD; }
         // Codice del link
         pLinkID = Row.CdrAssoc(_T("LINK_ID"));

         // Cancello le righe risultanti
         while (gsc_isEOF(pRs) == GS_BAD)
         {
            // lettura record
            if (gsc_DBReadRow(pRs, Row) == GS_BAD)
               { if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs); return GS_BAD; }
            // Codice sottoclasse nodo iniziale
            if (gsc_rb2Lng(pLinkID, &LinkID) == GS_BAD || LinkID != elem_id) break;

            if (gsc_DBDelRow(pRs) == GS_BAD)
               { if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs); return GS_BAD; }
            gsc_Skip(pRs);
         }
         if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);

         break;
      }
      case TYPE_SURFACE:
         break;
      default:
         GS_ERR_COD = eGSInvalidTopoType;
         return GS_BAD;
   }
 
   return GS_GOOD;
}


/////////////////////////////////////////////////////////////
///////////      FINE  FUNZIONI  C_TOPOLOGY       ///////////
/////////////////////////////////////////////////////////////


/****************************************************************************/
/*.doc gsc_insert_new_node                                                      */
/*+
  Inserimento di un nodo nuovo di una sottoclasse nota in posizione nota.
  Parametri:
  C_SUB     *pSub;           puntatore a sottoclasse nodale da inserire
  ads_point pt;              posizione
  ads_name  entity;          nuova entità GEOsim
  long      *key_attrib;     codice identificativo della nuova entità di GEosim
  C_RB_LIST &default_values; valori da inserire
  int       IsDefCalc;       Flag; se = GS_GOOD i valori di default sono calcolati
  _RecordsetPtr *pRsSub;     Recordset per inserimento dati

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/******************************************************************************/
int gsc_insert_new_node(C_SUB *pSub, ads_point pt, ads_name entity, long *key_attrib,
                        C_RB_LIST &default_values, int IsDefCalc,
                        _RecordsetPtr &pRsSub)
{
   C_FAS *p_fas= pSub->ptr_fas(), OldGraphEnv;
   C_ID  *p_id = pSub->ptr_id();

   // Effettua settaggi FAS di default
   if (gsc_setenv_graph(p_id->category, p_id->type, *p_fas, &OldGraphEnv) == GS_BAD)
      return GS_BAD;

   if (gsc_insert_block(p_fas->block, pt, p_fas->block_scale, p_fas->block_scale,
                        p_fas->rotation) != GS_GOOD)
      { gsc_setenv_graph(p_id->category, p_id->type, OldGraphEnv); return GS_BAD; }

   // Ripristina situazione FAS precedente
   gsc_setenv_graph(p_id->category, p_id->type, OldGraphEnv);

   acdbEntLast(entity); // nuova entità

   if (IsDefCalc == GS_GOOD) // calcolo dei valori di default
      if (pSub->get_default_values(default_values, entity) == GS_BAD)
         { gsc_EraseEnt(entity); return GS_BAD; }

   // Inserisco entità GEOsim in tabella temp
   C_CLS_PUNT_LIST ent_list;
   C_CLS_PUNT      *punt;

	if ((punt = new C_CLS_PUNT(pSub, entity)) == NULL)
      { gsc_EraseEnt(entity); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   ent_list.add_tail(punt);

   if (pSub->ins_data(&ent_list, default_values, key_attrib,
                      (GEOsimAppl::GLOBALVARS.get_InsPos() == AUTO) ? VISIBLE : INVISIBLE,
                      UNKNOWN_MOD, pRsSub) == GS_BAD)
      { gsc_EraseEnt(entity); return GS_BAD; }
   ads_name_set(((C_CLS_PUNT *) ent_list.get_head())->ent, entity);

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_get_PunctualSS                     <external> */
/*+
  Questa funzione ricava un gruppo di selezione di tutti gli
  oggetti puntuali (testi e blocchi) in grafica.
  Parametri:
  C_SELSET &PunctualSS;  gruppo di selezione (out)
  bool      OnlyOnVisibleLayers; Flag, se = true la funzione ritorna solo oggetti
                                 che risiedono su layer non spenti o congelati (default true)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_get_PunctualSS(C_SELSET &PunctualSS, bool OnlyOnVisibleLayers)
{
   C_RB_LIST EntMask;
   ads_name  dummy;

   // maschera per filtrare testi e blocchi esclusi i "$T"
   if ((EntMask << acutBuildList(-4, _T("<or"),
                                    RTDXF0, _T("TEXT"),
                                    RTDXF0, _T("MTEXT"),
                                    -4, _T("<and"),
                                       RTDXF0, _T("INSERT"),
                                       -4, _T("<not"),
                                          2, _T("$T"),
                                       -4, _T("not>"),
                                    -4, _T("and>"),
                                 -4, _T("or>"), 0)) == NULL)
      return GS_BAD;
   if (acedSSGet(_T("_X"), NULL, NULL, EntMask.get_head(), dummy) != RTNORM) PunctualSS.clear();
   else PunctualSS << dummy;

   if (OnlyOnVisibleLayers)
      PunctualSS.intersectVisibile();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getNodesInWindow                   <external> */
/*+
  Questa funzione ritorna un gruppo di selezione di oggetti puntuali
  (testi e blocchi) il cui punto di inserimento è interno ad una finestra.
  Parametri:
  ads_point pt1;          Primo angolo della finestra
  ads_point pt2;          Secondo angolo della finestra
  C_SELSET  &SelSet;      Gruppo di selezione
  int       CheckOn2D;    Controllo oggetti senza usare la Z (default = GS_GOOD) 
  bool      OnlyOnVisibleLayers; Flag, se = true la funzione ritorna solo oggetti
                                 che risiedono su layer non spenti o congelati (default true)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_getNodesInWindow(ads_point pt1, ads_point pt2, C_SELSET &SelSet, int CheckOn2D,
                         bool OnlyOnVisibleLayers)
{
   C_RB_LIST EntMask;
   ads_name  dummy;
   ads_point _pt1, _pt2;

   SelSet.clear();
   
   _pt1[X] = gsc_min(pt1[X], pt2[X]);
   _pt1[Y] = gsc_min(pt1[Y], pt2[Y]);
   _pt1[Z] = gsc_min(pt1[Z], pt2[Z]);

   _pt2[X] = gsc_max(pt1[X], pt2[X]);
   _pt2[Y] = gsc_max(pt1[Y], pt2[Y]);
   _pt2[Z] = gsc_max(pt1[Z], pt2[Z]);

   if (CheckOn2D == GS_GOOD)
   {
      if ((EntMask << acutBuildList(-4, _T("<and"),
                                       -4, _T("<or"),
                                          RTDXF0, _T("TEXT"),
                                          RTDXF0, _T("MTEXT"),
                                          RTDXF0, _T("INSERT"),
                                       -4, _T("or>"),
                                       -4, _T("<and"),
                                          -4, _T(">=,>="),
                                             10, _pt1,
                                          -4, _T("<=,<="),
                                             10, _pt2,
                                       -4, _T("and>"),
                                    -4, _T("and>"), 0)) == NULL)
         return GS_BAD;
   }
   else
      if ((EntMask << acutBuildList(-4, _T("<and"),
                                       -4, _T("<or"),
                                          RTDXF0, _T("TEXT"),
                                          RTDXF0, _T("MTEXT"),
                                          RTDXF0, _T("INSERT"),
                                       -4, _T("or>"),
                                       -4, _T("<and"),
                                          -4, _T(">=,>=,>="),
                                             10, _pt1,
                                          -4, _T("<=,<=,<="),
                                             10, _pt2,
                                       -4, _T("and>"),
                                    -4, _T("and>"), 0)) == NULL)
         return GS_BAD;

   if (acedSSGet(_T("_X"), NULL, NULL, EntMask.get_head(), dummy) != RTNORM) SelSet.clear();
   else SelSet << dummy;

   if (OnlyOnVisibleLayers)
      SelSet.intersectVisibile();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getNodesOnPt                       <external> */
/*+
  Questa funzione ritorna un gruppo di selezione di oggetti puntuali
  (testi e blocchi ad esclusione del $T)
  che insistono su un punto noto.
  Parametri:
  ads_point point;        Punto
  C_SELSET  &SelSet;      Gruppo di selezione
  int       CheckOn2D;    Controllo oggetti senza usare la Z (default = GS_GOOD) 
  C_SELSET  *PunctualSS;  Gruppo di selezione di oggetti puntuali in cui effettuare la
                          ricerca; default = NULL
  bool      OnlyOnVisibleLayers; Flag, se = true la funzione ritorna solo oggetti
                                 che risiedono su layer non spenti o congelati (default true)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_getNodesOnPt(ads_point point, C_SELSET &SelSet, int CheckOn2D, C_SELSET *PunctualSS,
                     bool OnlyOnVisibleLayers)
{
   SelSet.clear();
   
   if (!PunctualSS)
   {
      C_RB_LIST EntMask;
      ads_name  dummy;
      ads_point pt1, pt2;

      ads_point_set(point, pt1);
      pt1[X] -= GEOsimAppl::TOLERANCE; pt1[Y] -= GEOsimAppl::TOLERANCE; pt1[Z] -= GEOsimAppl::TOLERANCE;
      ads_point_set(point, pt2);
      pt2[X] += GEOsimAppl::TOLERANCE; pt2[Y] += GEOsimAppl::TOLERANCE; pt2[Z] += GEOsimAppl::TOLERANCE;

      // maschera per filtrare testi e blocchi esclusi i "$T" con un punto di ins. 
      // all'interno di una finestra di tolleranza con controllo o meno sulla Z
      if (CheckOn2D == GS_GOOD)
      {
         if ((EntMask << acutBuildList(-4, _T("<and"),
                                          -4, _T("<or"),
                                             RTDXF0, _T("TEXT"),
                                             RTDXF0, _T("MTEXT"),
                                             -4, _T("<and"),
                                                RTDXF0, _T("INSERT"),
                                                -4, _T("<not"),
                                                   2, _T("$T"),
                                                -4, _T("not>"),
                                             -4, _T("and>"),
                                          -4, _T("or>"),
                                          -4, _T("<and"),
                                             -4, _T(">=,>="),
                                                10, pt1,
                                             -4, _T("<=,<="),
                                                10, pt2,
                                          -4, _T("and>"),
                                       -4, _T("and>"), 0)) == NULL)
            return GS_BAD;
      }
      else
         if ((EntMask << acutBuildList(-4, _T("<and"),
                                          -4, _T("<or"),
                                             RTDXF0, _T("TEXT"),
                                             RTDXF0, _T("MTEXT"),
                                             -4, _T("<and"),
                                                RTDXF0, _T("INSERT"),
                                                -4, _T("<not"),
                                                   2, _T("$T"),
                                                -4, _T("not>"),
                                             -4, _T("and>"),
                                          -4, _T("or>"),
                                          -4, _T("<and"),
                                             -4, _T(">=,>=,>="),
                                                10, pt1,
                                             -4, _T("<=,<=,<="),
                                                10, pt2,
                                          -4, _T("and>"),
                                       -4, _T("and>"), 0)) == NULL)
            return GS_BAD;

      if (acedSSGet(_T("_X"), NULL, NULL, EntMask.get_head(), dummy) == RTNORM)
      {
         // Filtro ulteriormente il gruppo di selezione utilizzando la tolleranza di GEOsim
         long      i = 0;
         ads_name  ent;
         C_RB_LIST Descr;

         while (ads_ssname(dummy, i++, ent) == RTNORM)
         {
            if ((Descr << acdbEntGet(ent)) == NULL)
               { SelSet.clear(); GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

            if (CheckOn2D == GS_GOOD)
            {
               if (gsc_2Dpoint_equal(Descr.SearchType(10)->resval.rpoint, point))
                  if (SelSet.add(ent) != GS_GOOD)
                     { acedSSFree(dummy); SelSet.clear(); return GS_BAD; }
            }
            else
               if (gsc_point_equal(Descr.SearchType(10)->resval.rpoint, point))
                  if (SelSet.add(ent) != GS_GOOD)
                     { acedSSFree(dummy); SelSet.clear(); return GS_BAD; }
         }
      }
      if (!ads_name_nil(dummy)) acedSSFree(dummy);
   }
   else
   {
      long      i = 0;
      ads_name  ent;
      C_RB_LIST Descr;

      while (PunctualSS->entname(i++, ent) == GS_GOOD)
      {
         if ((Descr << acdbEntGet(ent)) == NULL)
            { SelSet.clear(); GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

         if (CheckOn2D == GS_GOOD)
         {
            if (gsc_2Dpoint_equal(Descr.SearchType(10)->resval.rpoint, point))
               if (SelSet.add(ent) != GS_GOOD)
                  { SelSet.clear(); return GS_BAD; }
         }
         else
            if (gsc_point_equal(Descr.SearchType(10)->resval.rpoint, point))
               if (SelSet.add(ent) != GS_GOOD)
                  { SelSet.clear(); return GS_BAD; }
      }
   }

   if (OnlyOnVisibleLayers)
      SelSet.intersectVisibile();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_OverlapValidation <internal> */
/*+
  Questa funzione verifica che non ci siano altri oggetti puntuali
  (nodi e testi) con lo stesso punto di inserimento i quali non
  possono essere sovrapposti.
  Parametri:
  ads_point      point;       Punto di inserimento dell'oggetto puntuale
  C_CLASS        *p_class;    Struttura identificativa della classe
  const TCHAR    *Handle;     Identificativo dell'oggetto (qualora questo sia già di GEOsim)
                              per saltare il controllo su se stesso (default = NULL)
  oppure

  ads_point      point;       Punto di inserimento dell'oggetto puntuale
  C_CLASS        *p_class;    Struttura identificativa della classe
  C_POINT_OBJS_BTREE &PtObjsBTree; lista degli oggetti puntuali esistenti in grafica
  const TCHAR    *Handle;     Identificativo dell'oggetto (qualora questo sia già di GEOsim)
                              per saltare il controllo su se stesso (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD 
  se esisteva un'altro oggetto non sovrapponibile.
-*/  
/*********************************************************/
int gsc_OverlapValidation(ads_point point, C_CLASS *p_class, const TCHAR *Handle)
{
   long           i = 0;
   int            code;
   C_SELSET       SelSet;
   ads_name       ent;
   C_EED          eed;
   C_INT_INT      *pConct;
   C_CONNECT_LIST *pConctList;
   C_ID           *p_id;
   TCHAR          hand[MAX_LEN_HANDLE];

   if (!p_class || !point) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (!(p_id = p_class->ptr_id()) || !(pConctList = p_class->ptr_connect_list()))
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if (!pConctList || !pConctList->is_to_be_unique()) return GS_GOOD;

   // filtra testi e blocchi esclusi i "$T" con un punto di ins. noto (2D)
   if (gsc_getNodesOnPt(point, SelSet) == GS_BAD) return GS_BAD;

   while (SelSet.entname(i++, ent) == GS_GOOD)
      // controllo se è della classe indicata
      if (eed.load(ent) == GS_GOOD) // è di GEOsim
      {  
         if (Handle)
            if (gsc_enthand(ent, hand) == GS_BAD || gsc_strcmp(Handle, hand) == 0)
               // da saltare
               continue;

         code = -1;
         if (p_class->is_subclass())   // se sottoclasse
         {
            if (p_id->code == eed.cls) // verifico appartenenza stessa classe
               code = eed.sub;         // il codice della sottoclasse
         }
         else code = eed.cls;          // se classe normale

         // se questa classe (o sottoclasse) non può essere sovrapposta
         if ((pConct = (C_INT_INT *) pConctList->search_key(code)) && 
             (pConct->get_type() & NO_OVERLAP))
            return GS_BAD;
      }

   return GS_GOOD;
}
int gsc_OverlapValidation(ads_point point, C_CLASS *p_class,
                          C_POINT_OBJS_BTREE &PtObjsBTree, const TCHAR *Handle)
{
   int            code;
   C_INT_INT      *pConct;
   C_CONNECT_LIST *pConctList;
   C_ID           *p_id = p_class->ptr_id(), *p_id_NodeCls;
   C_BPOINT_OBJS  *pPtObjs;
   C_CLS_PUNT     *pNodeCls;
   ads_pointp     dummy = point;
   TCHAR          hand[MAX_LEN_HANDLE];

   // se non ci sono regole di sovrapposizione
   if (!(pConctList = p_class->ptr_connect_list()) ||
       pConctList->is_to_be_unique() == GS_BAD) return GS_GOOD;

   // se non ci oggetti nel punto
   if ((pPtObjs = (C_BPOINT_OBJS *) PtObjsBTree.search(dummy)) == NULL)
      return GS_GOOD;

   pNodeCls = (C_CLS_PUNT *) pPtObjs->ptr_EntList()->get_head();
   while (pNodeCls)
   {
      if (Handle)
         if (gsc_enthand(pNodeCls->ent, hand) == GS_BAD || gsc_strcmp(Handle, hand) == 0)
         {  // da saltare
            pNodeCls = (C_CLS_PUNT *) pNodeCls->get_next();
            continue;
         }

      p_id_NodeCls = ((C_CLASS *) pNodeCls->get_class())->ptr_id();
      // controllo se è della classe indicata
      code = -1;
      if (p_class->is_subclass())   // se sottoclasse
      {
         if (p_id->code == p_id_NodeCls->code) // verifico appartenenza stessa classe
            code = p_id_NodeCls->sub_code; // il codice della sottoclasse
      }
      else code = p_id_NodeCls->code; // se classe normale

      // se questa classe (o sottoclasse) non può essere sovrapposta
      if ((pConct = (C_INT_INT *) pConctList->search_key(code)) && 
          (pConct->get_type() & NO_OVERLAP))
         return GS_BAD;
      
      pNodeCls = (C_CLS_PUNT *) pNodeCls->get_next();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_get_AdjNode                        <external> */
/*+
  Questa funzione verifica che il lato abbia un nodo compatibile 
  coincidente al punto dato.
  Le informazioni sono lette direttamente dalla grafica.
  Parametri:
  C_SUB     *pLinkSub;     puntatore a sottoclasse lato (input)
  ads_point pt;            punto iniziale o finale del lato (input)
  C_SUB     *pNodeSub;     puntatore a sottoclasse nodo (output)
  ads_name  ent;           entità puntuale della sottoclasse nodo

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_get_AdjNode(C_SUB *pLinkSub, ads_point pt, C_SUB **pNodeSub, ads_name ent)
{
   C_SELSET  PunctualSS;
   C_INT_INT *pConnCls;
   long      i = 0;

   *pNodeSub = NULL;

   // filtra testi e blocchi esclusi i "$T" con un punto di ins. noto (2D)
   if (gsc_getNodesOnPt(pt, PunctualSS) == GS_BAD) return GS_BAD;

   while (PunctualSS.entname(i++, ent) == GS_GOOD)
   {               
      // Ritorna il puntatore alla classe cercata
      if ((*pNodeSub = (C_SUB *) GS_CURRENT_WRK_SESSION->find_class(ent)) &&
          (*pNodeSub)->ptr_id()->code == pLinkSub->ptr_id()->code)
		   // verifico se il lato necessita di collegarsi al nodo
			if ((pConnCls = (C_INT_INT *) pLinkSub->ptr_connect_list()->search_key((*pNodeSub)->ptr_id()->sub_code)) != NULL &&
				 pConnCls->get_type() & CONCT_POINT)
	         break;
			else
				// se il lato non necessita il collegamento (vedi caso valvola)
				// verifico che il nodo abbia la necessità di collegarsi al lato 
				// (in qualsiasi punto o a inizio-fine)
				if ((pConnCls = (C_INT_INT *) (*pNodeSub)->ptr_connect_list()->search_key(pLinkSub->ptr_id()->sub_code)) != NULL &&
					 (pConnCls->get_type() & CONCT_ANY_POINT || pConnCls->get_type() & CONCT_START_END))
					break;
      else (*pNodeSub) = NULL;
   }

   return (*pNodeSub) ? GS_GOOD : GS_BAD;
}


/*********************************************************/
/*.doc gsc_isExistingEnt                      <external> */
/*+
  Questa funzione verifica che l'entità indicata non sia stata cancellata.
  Parametri:
  int cls;     codice classe
  int sub;     codice sottoclasse
  long Key;    codice chiave

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
static int gsc_isExistingEnt(int cls, int sub, long Key)
{
   C_LINK_SET GraphDBLinkSet;
   C_STR_LIST HandleList;

   if (GraphDBLinkSet.GetHandleList(cls, sub, Key, HandleList) == GS_GOOD)
   {
      C_STR    *pHandle;
      ads_name ent;

      // l'oggetto non era stato caricato in grafica
      if (HandleList.get_count() == 0) return GS_GOOD;

      pHandle = (C_STR *) HandleList.get_head();
      while (pHandle)
      {  // Verifico esistenza di almeno un oggetto grafico
         if (acdbHandEnt(pHandle->get_name(), ent) == RTNORM && 
             gsc_IsErasedEnt(ent) == GS_BAD)
            return GS_GOOD;
         pHandle = (C_STR *) HandleList.get_next();
      }
   }
   
   return GS_BAD;
}