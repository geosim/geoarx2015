/**********************************************************
Name: GS_DWG

Module description: File contenente le funzioni per 
                    la gestione dei dati grafici in formato DWG
            
Author: Roberto Poltini

(c) Copyright 1996-2014 by IREN ACQUA GAS S.p.a. Genova

              
Modification history:
              
Notes and restrictions on use: 


**********************************************************/


/*********************************************************/
/* INCLUDES */
/*********************************************************/


#include "stdafx.h"         // MFC core and standard components

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")


#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <string.h>
#include <math.h>

#include "asiappl.h"
#include "rxdefs.h"
#include "adslib.h"
#include <errno.h>
#include <adeads.h>
#include "adsdlg.h"   
#include "dbapserv.h"
#include <dbpl.h>          // lwpolyline
#include "afxwin.h"
#include "rxmfcapi.h"
#include "mapstringarray.h"

#include "..\gs_def.h" // definizioni globali
#include "gs_error.h"     // codici errori
#include "gs_utily.h"     // (gsc_strcat, gsc_tostring)
#include "gs_resbf.h"     // (gsc_nth)
#include "gs_ade.h"
#include "gs_dbref.h" 
#include "gs_class.h"     // definizioni classi
#include "gs_dwg.h"       // gestione disegni
#include "gs_prjct.h"
#include "gs_init.h"
#include "gs_user.h"
#include "gs_area.h"      // gestione aree di lavoro
#include "gs_graph.h"
#include "gs_query.h"
#include "gs_netw.h"      // gestione nodi di rete 
#include "gs_cmd.h"
#include "gs_setv.h"
#include "gs_attbl.h"
#include "gs_gphdata.h" // Gestione sorgenti dati grafiche


/*********************************************************/
/* TYPEDEFS */
/*********************************************************/

#define DWG_FILE_INFORMATION_EXT  "INF" 


/****************************************************************************/
/* GLOBALS */
/****************************************************************************/


/*********************************************************/
/* FUNCTIONS */
/*********************************************************/


//-----------------------------------------------------------------------//
//////////////////         C_DWG   INIZIO    //////////////////////////////
//-----------------------------------------------------------------------//


C_DWG::C_DWG() : C_STR()
{
   size = 0;
   Xmin = Ymin = Xmax = Ymax = 0.0;
   pMapAttachedDwg = NULL;
}

C_DWG::C_DWG(C_STRING &_Path, long _Size, bool DateSensitive)
{
   set_size(_Size);
   set_name(_Path.get_name());
   pMapAttachedDwg = NULL;
   load_extents(DateSensitive);
}

C_DWG::~C_DWG()
{
   if (pMapAttachedDwg) delete pMapAttachedDwg;
}

long C_DWG::get_size() // dimensione del file DWG in byte
{
   return size;
}

int C_DWG::set_size(long in) 
{
   size = in;
   return GS_GOOD;
}

int C_DWG::copy(C_DWG &out)
{
   out.set_name(get_name());
   out.pMapAttachedDwg = pMapAttachedDwg;
   out.size = size;
   out.Xmin = Xmin;
   out.Ymin = Ymin;
   out.Xmax = Xmax;
   out.Ymax = Ymax;

   out.Ids.removeAll();
   for (int i = 0; i < Ids.length(); i++)
      out.Ids.append(Ids[i]);

   return GS_GOOD;
}

int C_DWG::set_extents(double _Xmin, double _Ymin, double _Xmax, double _Ymax)
{
   if (_Xmin <= _Xmax && _Ymin <= _Ymax)
   {
      Xmin = _Xmin;
      Ymin = _Ymin;
      Xmax = _Xmax;
      Ymax = _Ymax;

      return GS_GOOD;
   }
   else
      return GS_BAD;
}


void C_DWG::get_extents(double *_Xmin, double *_Ymin, double *_Xmax, double *_Ymax)
{
   *_Xmin = Xmin;
   *_Ymin = Ymin;
   *_Xmax = Xmax;
   *_Ymax = Ymax;
}

/*********************************************************/
/*.doc int C_DWG::regen_extents_from_dwg      <internal> /*
/*+
  Effettua il calcolo delle estensioni del disegno leggendo 
  l'estensione di ogni singolo oggetto contenuto del disegno.
  Parametri:
  int MessageToVideo;    Flag, se = GS_GOOD stampa a video eventuali messaggi
                         (default = GS_GOOD)

  Restituisce il numero di oggetti estratti se tutto OK altrimenti -1.
-*/  
/*********************************************************/
int C_DWG::regen_extents_from_dwg(int MessageToVideo)
{
   AcDbBlockTableRecordIterator *pIterator;
   ads_point corner1, corner2;
   int       res = GS_BAD, WasAlreadyActivated = GS_BAD, WasAlreadyAttached = GS_BAD;

   // "\nCalcolo estensioni disegno <%s>..."
   if (MessageToVideo == GS_GOOD) acutPrintf(gsc_msg(92), get_name());

   if ((WasAlreadyAttached = is_attached()) == GS_GOOD) // verifico se il dwg era già attaccato
      WasAlreadyActivated = is_activated(); // verifico se il dwg era già attivato
   
   if (WasAlreadyActivated == GS_BAD)           // se non già attivato
      if (activate() == GS_BAD) return GS_BAD;  // lo attivo        

   if (pMapAttachedDwg->LockForRead() != AcMap::kOk)
   {
      if (WasAlreadyAttached == GS_BAD) detach();
      return GS_BAD;
   }
      
   if ((pIterator = pMapAttachedDwg->GetObjectIterator()))
   {
      if (gsc_get_window(pIterator, corner1, corner2) == GS_GOOD)
      {
         Xmin = corner1[X];
         Ymin = corner1[Y];
         Xmax = corner2[X];
         Ymax = corner2[Y];
      }
      res  = GS_GOOD;
      delete pIterator;
   }

   pMapAttachedDwg->UnlockForRead();
   
   // se il dwg non era attivato lo disattivo
   if (WasAlreadyActivated == GS_BAD) deactivate();
   
   // se il dwg non era attaccato lo stacco
   if (WasAlreadyAttached == GS_BAD) detach();

   return res;
}


/*********************************************************/
/*.doc int C_DWG::ptr_ObjectIdArray           <external> /*
/*+
  Restituisce il puntatore al vettore degli oggetti letti dal disegno.
-*/  
/*********************************************************/
AcDbObjectIdArray* C_DWG::ptr_ObjectIdArray(void)
{
   return &Ids;
}


/*********************************************************/
/*.doc int C_DWG::clear_ObjectIdArray           <external> /*
/*+
  Rimuove il vettore degli oggetti letti dal disegno.

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
void C_DWG::clear_ObjectIdArray(void)
{
   Ids.removeAll();
   return;
}


/*********************************************************/
/*.doc int C_DWG::is_empty                    <external> /*
/*+
  Verifica se ci il disegno contiene oggetti grafici.
  Parametri:
  bool *_Empty;      resituisce TRUE se il disegno è vuoto

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DWG::is_empty(bool *_Empty)
{
   AcDbBlockTableRecordIterator *pIterator;
   int WasAlreadyActivated = GS_BAD, WasAlreadyAttached = GS_BAD;

   *_Empty = TRUE;

   if ((WasAlreadyAttached = is_attached()) == GS_GOOD) // verifico se il dwg era già attaccato
      WasAlreadyActivated = is_activated(); // verifico se il dwg era già attivato
   
   if (WasAlreadyActivated == GS_BAD)           // se non già attivato
      if (activate() == GS_BAD) return GS_BAD;  // lo attivo        

   if (pMapAttachedDwg->LockForRead() != AcMap::kOk)
   {
      if (WasAlreadyAttached == GS_BAD) detach();
      return GS_BAD;
   }
     
   if ((pIterator = pMapAttachedDwg->GetObjectIterator()))
   {
      // Cerco primo oggetto valido
      if (!pIterator->done()) *_Empty = FALSE;
      delete pIterator;
   }

   pMapAttachedDwg->UnlockForRead();

   // se il dwg non era attivato lo disattivo
   if (WasAlreadyActivated == GS_BAD) deactivate();
   
   // se il dwg non era attaccato lo stacco
   if (WasAlreadyAttached == GS_BAD) detach();

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DWG::get_extents_file_path           <internal> /*
/*+
  Restituisce la path del file INF con le estensioni del disegno.
  Parametri:
  C_STRING &ExtFilePath;
-*/  
/*********************************************************/
void C_DWG::get_extents_file_path(C_STRING &ExtFilePath)
{
   C_STRING drive, dir, name;

   gsc_splitpath(get_name(), &drive, &dir, &name);
   ExtFilePath = drive;
   ExtFilePath += dir;
   ExtFilePath += name;
   ExtFilePath += _T('.');
   ExtFilePath += DWG_FILE_INFORMATION_EXT;
}


/*********************************************************/
/*.doc int C_DWG::load_extents()             <internal> /*
/*+
  Carica da file INF le estensioni del disegno.
  Parametri:
  bool DateSensitive; Flag se = true carica le estensioni verificando che la 
                      data del file INF sia >= del relativo file DWG altrimenti
                      ignora questo controllo (default = TRUE)

  Restituisce GS_GOOD se tutto OK altrimenti GS_BAD
-*/  
/*********************************************************/
int C_DWG::load_extents(bool DateSensitive)
{
   C_STRING ExtFilePath;

   Xmin = Ymin = Xmax = Ymax = 0.0;

   get_extents_file_path(ExtFilePath);

   if (gsc_path_exist(get_name()) == GS_GOOD && gsc_path_exist(ExtFilePath) == GS_GOOD)
   {
      CTime               DWGDateTime, INFDateTime;
      intptr_t            hfile;
      struct _wfinddata_t descrfile;

      // Leggo data file DWG
      if ((hfile = _wfindfirst(get_name(), &descrfile)) == -1L) return GS_BAD;
      DWGDateTime = descrfile.time_write;
      _findclose(hfile);

      // Leggo data file INF
      if ((hfile = _wfindfirst(ExtFilePath.get_name(), &descrfile)) == -1L) return GS_BAD;
      INFDateTime = descrfile.time_write;
      _findclose(hfile);

      // Se si deve ignorare la data oppure se
      // la data del file INF deve essere successiva o uguale a quella del DWG
      if (!DateSensitive || INFDateTime >= DWGDateTime)
      {
         C_PROFILE_SECTION_BTREE ProfileSections;
         C_BPROFILE_SECTION      *ProfileSection;
         C_2STR_BTREE            *pProfileEntries;
         C_B2STR                 *pProfileEntry;
         double                  _Xmin, _Ymin, _Xmax, _Ymax;

         if (gsc_read_profile(ExtFilePath, ProfileSections) == GS_BAD) return GS_BAD;
         if (!(ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(_T("EXTENTS"))))
            return GS_BAD;
         pProfileEntries = (C_2STR_BTREE *) ProfileSection->get_ptr_EntryList();
         if (!(pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("XMIN")))) return GS_BAD;
         _Xmin = _wtof(pProfileEntry->get_name2());
         if (!(pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("YMIN")))) return GS_BAD;
         _Ymin = _wtof(pProfileEntry->get_name2());
         if (!(pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("XMAX")))) return GS_BAD;
         _Xmax = _wtof(pProfileEntry->get_name2());
         if (!(pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("YMAX")))) return GS_BAD;
         _Ymax = _wtof(pProfileEntry->get_name2());

         if (set_extents(_Xmin, _Ymin, _Xmax, _Ymax) == GS_BAD) return GS_BAD;
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc int C_DWG::save_extents()             <internal> /*
/*+
  Salva nel file INF le estensioni del disegno.

  Restituisce GS_GOOD se tutto OK altrimenti GS_BAD
-*/  
/*********************************************************/
int C_DWG::save_extents(void)
{
   C_STRING                ExtFilePath;
   C_PROFILE_SECTION_BTREE ProfileSections;
   C_BPROFILE_SECTION      *ProfileSection;
   bool                    Unicode = false;
   
   if (gsc_path_exist(get_name()) == GS_BAD) return GS_BAD;

   get_extents_file_path(ExtFilePath);

   if (gsc_path_exist(ExtFilePath) == GS_GOOD)
      if (gsc_read_profile(ExtFilePath, ProfileSections, &Unicode) == GS_BAD) return GS_BAD;

   if (!(ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(_T("EXTENTS"))))
   {
      if (ProfileSections.add(_T("EXTENTS")) == GS_BAD) return GS_BAD;
      ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.get_cursor();
   }
   ProfileSection->set_entry(_T("XMIN"), Xmin);
   ProfileSection->set_entry(_T("YMIN"), Ymin);
   ProfileSection->set_entry(_T("XMAX"), Xmax);
   ProfileSection->set_entry(_T("YMAX"), Ymax);

   return gsc_write_profile(ExtFilePath, ProfileSections, Unicode);
}


/*********************************************************/
/*.doc int C_DWG::intersect                   <internal> /*
/*+
  Verifica se la zona passata come parametro interseca l'estensione del DWG.

  Restituisce GS_GOOD se tutto OK altrimenti GS_BAD
-*/  
/*********************************************************/
int C_DWG::intersect(double _Xmin, double _Ymin, double _Xmax, double _Ymax)
{
   // Se l'estensione non è stata determinata
   if (Xmin == 0 && Ymin == 0 && Xmax == 0 && Ymax == 0) return GS_GOOD;

   if (_Xmin > Xmax || _Xmax < Xmin || _Ymin > Ymax || _Ymax < Ymin)
      return GS_BAD;
   else
      return GS_GOOD;
}

/*********************************************************/
/*.doc int C_DWG::resize()                    <internal> /*
/*+
  Aggiorna la dimensione del DWG.

  Restituisce GS_GOOD se tutto OK altrimenti GS_BAD
-*/  
/*********************************************************/
int C_DWG::resize()
{
   presbuf filesize;
   long n_adir;

   n_adir = gsc_adir(get_name(), NULL, &filesize, NULL, NULL);
   if (n_adir <= 0) { GS_ERR_COD = eGSCannotResizeDwgList; return GS_BAD; }
   set_size(filesize->rbnext->resval.rlong);
   acutRelRb(filesize);

   return GS_GOOD;
}


/*********************************************************/
/*.doc int C_DWG::is_attached()               <external> /*
/*+
  Verifica se il disegno è attaccato.
  Parametri:
  
  Restituisce GS_GOOD se tutto OK altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DWG::is_attached(void)
{
   return (pMapAttachedDwg) ? GS_GOOD : GS_BAD;
}


/*********************************************************/
/*.doc int C_DWG::is_activated()              <external> /*
/*+
  Verifica se il disegno è attaccato e attivato.
  Parametri:
  
  Restituisce GS_GOOD se tutto OK altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DWG::is_activated(void)
{
   if (!pMapAttachedDwg) return GS_BAD;

   return (pMapAttachedDwg->ActiveStatus() == AcMap::kDwgActive) ? GS_GOOD : GS_BAD;
}


/*********************************************************/
/*.doc int C_DWG::activate()                    <internal> /*
/*+
  Effettua il collegamento "attach" e l'attivazione al disegno.
  Parametri:
  
  Restituisce GS_GOOD se tutto OK altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DWG::activate(void)
{
   if (is_attached() == GS_BAD)
      if (attach() != GS_GOOD) return GS_BAD;

   if (is_activated() == GS_BAD) // da attivare
      if (pMapAttachedDwg->Activate() != AcMap::kOk) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc int C_DWG::deactivate()                <internal> /*
/*+
  Effettua il la disattivazione "deactivate" del disegno.
  Parametri:
  
  Restituisce GS_GOOD se tutto OK altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DWG::deactivate(void)
{
   if (is_activated() == GS_BAD) return GS_GOOD; // già disattivato

   if (pMapAttachedDwg->Deactivate() != AcMap::kOk)  return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc int C_DWG::attach()                    <internal> /*
/*+
  Effettua il collegamento "attach" al disegno.
  Parametri:
  
  Restituisce GS_GOOD se tutto OK altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DWG::attach(void)
{
   AcMapSession         *mapApi;
   AcMapProject         *pProj;
   AcMapDrawingSet      *pDSet;
   C_STRING             Path;

   if (!(mapApi = AcMapGetSession())) return GS_BAD; // sessione di MAP
   if (mapApi->GetProject(pProj) == Adesk::kFalse) return GS_BAD; // progetto MAP
   if (pProj->GetDrawingSet(pDSet) == Adesk::kFalse) return GS_BAD; // disegni attaccati

   if (Path.paste(get_ADEAliasPath()) == NULL) return GS_BAD;
   
   if (pDSet->GetDrawing(pMapAttachedDwg, Path.get_name()) != AcMap::kOk)
   {
      if (pDSet->AttachDrawing(pMapAttachedDwg, Path.get_name()) != AcMap::kOk)
      {
         if (pMapAttachedDwg) { delete pMapAttachedDwg; pMapAttachedDwg = NULL; }
         
         #if defined(GSDEBUG) // se versione per debugging
            gsc_printAdeErr();
         #endif

         return GS_BAD;
      }

      // Leggo le tabelle dati oggetti definite del disegno
      AcMapStringArray tAr;
      
      // Aggiungo le tabelle definite in GEOsimAppl::OD_TABLENAME_LIST
      if (pMapAttachedDwg->GetTableList(tAr, AcMap::kODD) == AcMap::kOk)
         for (int i = 0; i < tAr.Length(); i++)
            GEOsimAppl::OD_TABLENAME_LIST.add_tail_unique(tAr[i], FALSE);
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc int C_DWG::detach()                    <internal> /*
/*+
  Effettua lo scollegamento del disegno.
  
  Restituisce GS_GOOD se tutto OK altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DWG::detach(void)
{
   AcMapSession    *mapApi;
   AcMapProject    *pProj;
   AcMapDrawingSet *pDSet;
   C_STRING        Path;
   int             OldCmdDia;

   if (is_attached() == GS_BAD) return GS_GOOD; // già staccato

   if (!(mapApi = AcMapGetSession())) return GS_BAD; // sessione di MAP
   if (mapApi->GetProject(pProj) == Adesk::kFalse) return GS_BAD; // progetto MAP
   if (pProj->GetDrawingSet(pDSet) == Adesk::kFalse) return GS_BAD; // disegni attaccati

   gsc_set_cmddia(0, &OldCmdDia);

   if (deactivate() == GS_BAD)
      { gsc_set_cmddia(OldCmdDia); return GS_BAD; }

   if (pDSet->DetachDrawing(pMapAttachedDwg->ObjectId()) != AcMap::kOk)
      { gsc_set_cmddia(OldCmdDia); return GS_BAD; }

   delete pMapAttachedDwg;
   pMapAttachedDwg = NULL;

   gsc_set_cmddia(OldCmdDia); // Reset CMDDIA

   return GS_GOOD;
}


/*********************************************************/
/*.doc int C_DWG::GetTableList                <internal> /*
/*+
  Gets the object names from one of the drawing's symbol tables.
  Parametri:
  C_STR_LIST &TableList;
  AcMap::ETableType kType;    0 kBlockTable Block names. 
                              1 kLayerTable Layer names. 
                              2 kLinetypeTable Line types. 
                              3 kStyleTable Text styles. 
                              4 kRegappTable AutoCAD regapps. 
                              5 kGroupTable Group names. 
                              6 kLinkTemplate Link template names. 
                              7 kODD Object data table names. 
                              8 kMLinestyleTable Line styles. 
                              9 kFeatureTable Feature names. 
                              10 kLineweightTable Line weights. 
                              11 kPlotstyleTable Plot styles. 

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DWG::GetTableList(C_STR_LIST &TableList, AcMap::ETableType kType)
{
   C_RB_LIST        xData;
   int              ToDetach = FALSE;
   C_STRING         ExtFilePath;
   AcMapStringArray tArray;

   TableList.remove_all();
   // Se il disegno non era attaccato verrà attaccato, attivato
   // e alla fine staccato, se esisteva il file di etensione viene ridatato
   // alla data-ora corrente per non invalidarlo
   if (is_attached() == GS_BAD)
   {
      get_extents_file_path(ExtFilePath);
      if (gsc_path_exist(ExtFilePath) == GS_BAD) ExtFilePath.clear();
      ToDetach = TRUE;
   }
   
   if (activate() == GS_BAD)
   {
      if (ToDetach)
      {
         detach(); 
         // Aggiorna la data del file a quella corrente
         if (ExtFilePath.len() > 0) gsc_SetFileTime(ExtFilePath);
      }
      return GS_BAD;
   }

   if (pMapAttachedDwg->LockForRead() != AcMap::kOk)
   {
      if (ToDetach)
      {
         detach(); 
         if (ExtFilePath.len() > 0) gsc_SetFileTime(ExtFilePath);
      }
      return GS_BAD;
   }
     
   // get the list of layers defined in the source drawing
   if (pMapAttachedDwg->GetTableList(tArray, kType) == AcMap::kOk)
      for (int i = 0; i < tArray.Length(); i++)
         TableList.add_tail_str(tArray[i]); 

   pMapAttachedDwg->UnlockForRead();
   
   if (ToDetach)
   {
      detach(); 
      if (ExtFilePath.len() > 0) gsc_SetFileTime(ExtFilePath);
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc int C_DWG::HasLocks                    <internal> /*
/*+
  Questa funzione legge dal disegno se ci sono oggetti bloccati.
-*/  
/*********************************************************/
bool C_DWG::HasLocks(void)
{
   C_RB_LIST                    xData;
   bool                         Result = false, ToDetach = false;
   C_STRING                     ExtFilePath;
   AcDbBlockTableRecordIterator *pIterator;
   AcDbEntity                   *pEntity;

   // Se il disegno non era attaccato verrà attaccato, attivato
   // e alla fine staccato, se esisteva il file di etensione viene ridatato
   // alla data-ora corrente per non invalidarlo
   if (is_attached() == GS_BAD)
   {
      get_extents_file_path(ExtFilePath);
      if (gsc_path_exist(ExtFilePath) == GS_BAD) ExtFilePath.clear();
      ToDetach = true;
   }
   
   if (activate() == GS_BAD)
   {
      if (ToDetach)
      {
         detach(); 
         // Aggiorna la data del file a quella corrente
         if (ExtFilePath.len() > 0) gsc_SetFileTime(ExtFilePath);
      }
      return false;
   }

   if (pMapAttachedDwg->LockForRead() != AcMap::kOk)
   {
      if (ToDetach)
      {
         detach(); 
         if (ExtFilePath.len() > 0) gsc_SetFileTime(ExtFilePath);
      }
      return false;
   }

   if ((pIterator = pMapAttachedDwg->GetObjectIterator()))
   {
      for (; !pIterator->done(); pIterator->step())
      {
         if (pIterator->getEntity(pEntity, AcDb::kForRead) != AcMap::kOk) break;
         xData << pEntity->xData(_T("ADE_ENTITY_LOCK"));
         pEntity->close();
         if (xData.get_head())
         {
            Result = true;
            break;
         }
      }
      delete pIterator;
   }

   pMapAttachedDwg->UnlockForRead();
   
   if (ToDetach)
   {
      detach(); 
      if (ExtFilePath.len() > 0) gsc_SetFileTime(ExtFilePath);
   }

   return Result;
}


/*********************************************************/
/*.doc int C_DWG::unlockObjs()                    <internal> /*
/*+
  Questa funzione sblocca tutti gli oggetti rimasti bloccati per errore.
  
  Restituisce GS_GOOD se tutto OK altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DWG::unlockObjs(void)
{
   int      Result = GS_GOOD, ToDetach = FALSE, OldCmdDia;
   C_STRING ExtFilePath;

   // Se il disegno non era attaccato verrà attaccato, attivato
   // e alla fine staccato, se esisteva il file di etensione viene ridatato
   // alla data-ora corrente per non invalidarlo
   if (is_attached() == GS_BAD)
   {
      get_extents_file_path(ExtFilePath);
      if (gsc_path_exist(ExtFilePath) == GS_BAD) ExtFilePath.clear();
      ToDetach = TRUE;
   }

   if (activate() == GS_BAD)
   {
      if (ToDetach)
      {
         detach(); 
         // Aggiorna la data del file a quella corrente
         if (ExtFilePath.len() > 0) gsc_SetFileTime(ExtFilePath);
      }
      return GS_BAD;
   }

   gsc_set_cmddia(0, &OldCmdDia);

   if (pMapAttachedDwg->LockForWrite(Adesk::kTrue) == AcMap::kOk) // set an exclusive lock
   {
      GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = true; // usato nell'evento beginSave
      pMapAttachedDwg->RemoveLocks(_T("*")); // rimuovo lock di tutti gli utenti
      GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = false;
      pMapAttachedDwg->UnlockForWrite();
   }
   else
   {
      GS_ERR_COD = eGSFileLocked;
      Result     = GS_BAD;
   }

   if (ToDetach)
   {
      detach(); 
      if (ExtFilePath.len() > 0) gsc_SetFileTime(ExtFilePath);
   }

   gsc_set_cmddia(OldCmdDia); // Reset CMDDIA

   return Result;
}


/*********************************************************/
/*.doc int C_DWG::index()                     <internal> /*
/*+
  Questa funzione indicizza tutti gli oggetti per location e object-data.
  Per quanto riguarda l'indice per object-data, se indicata la classe 
  verrà generato l'indice solo per la tabella OD della classe per il campo ID.
  Paramerti:
  int prj;              codice del progetto; opzionale (default = 0)
  int cls;              codice della classe; opzionale (default = 0)
  
  Restituisce GS_GOOD se tutto OK altrimenti GS_BAD.
-*/  
/*********************************************************/
ade_id C_DWG::GetAdeId(void)
   { return gsc_getAdeId(pMapAttachedDwg); }


/*********************************************************/
/*.doc int C_DWG::index()                     <internal> /*
/*+
  Questa funzione indicizza tutti gli oggetti per location e object-data.
  Per quanto riguarda l'indice per object-data, se indicata la classe 
  verrà generato l'indice solo per la tabella OD della classe per il campo ID.
  Paramerti:
  int prj;              codice del progetto; opzionale (default = 0)
  int cls;              codice della classe; opzionale (default = 0)
  
  Restituisce GS_GOOD se tutto OK altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DWG::index(int prj, int cls)
{
   int       Result = GS_GOOD, ToDetach = FALSE, OldCmdDia;
   C_STRING  ExtFilePath;
   C_RB_LIST OdParam;

   // Se il disegno non era attaccato verrà attaccato, attivato
   // e alla fine staccato, se esisteva il file di etensione viene ridatato
   // alla data-ora corrente per non invalidarlo
   if (is_attached() == GS_BAD)
   {
      get_extents_file_path(ExtFilePath);
      if (gsc_path_exist(ExtFilePath) == GS_BAD) ExtFilePath.clear();
      ToDetach = TRUE;
   }

   if (activate() == GS_BAD)
   {
      if (ToDetach)
      {
         detach(); 
         // Aggiorna la data del file a quella corrente
         if (ExtFilePath.len() > 0) gsc_SetFileTime(ExtFilePath);
      }
      return GS_BAD;
   }

   gsc_set_cmddia(0, &OldCmdDia);

   Result = GS_BAD;
   do
   {
      // pulisce la definizione degli indici
      if (ade_dwgindexdef(GS_EMPTYSTR, 1, NULL) != RTNORM)
         { GS_ERR_COD = eGSOpNotAble; break; }
      // indice per Location
      if (ade_dwgindexdef(_T("Location"), 1, NULL) != RTNORM)
         { GS_ERR_COD = eGSOpNotAble; break; }

      if (prj > 0 && cls > 0)
      {
         C_CLASS  *pCls;
         C_STRING ODTableName;

         if ((pCls = gsc_find_class(prj, cls, 0)) != NULL)
         {
            OdParam << acutBuildList(RTLB, 0);

            // se simulazione leggo dalla prima sottoclasse (le altre sono uguali)
            if (pCls->get_category() == CAT_EXTERN)
            {
               C_SUB *pSub = (C_SUB *) pCls->ptr_sub_list()->get_head();

               while (pSub)
               {
                  gsc_getODTableName(prj, cls, pSub->ptr_id()->sub_code, ODTableName);
                  // verifico se la tabella è già esistente
                  if (GEOsimAppl::OD_TABLENAME_LIST.search_name(ODTableName.get_name(), FALSE) == NULL)
                     { pSub = (C_SUB *) pSub->get_next(); continue; }

                  OdParam += acutBuildList(RTLB, 
                                              RTSTR, ODTableName.get_name(),
                                              RTSTR, _T("ID"),
                                           RTLE, 0);
                  pSub = (C_SUB *) pSub->get_next();
               }   
            }
            else
            {
               gsc_getODTableName(prj, cls, 0, ODTableName);
               // verifico se la tabella è già esistente
               if (GEOsimAppl::OD_TABLENAME_LIST.search_name(ODTableName.get_name(), FALSE) != NULL)
                  OdParam += acutBuildList(RTLB, 
                                              RTSTR, ODTableName.get_name(),
                                              RTSTR, _T("ID"),
                                           RTLE, 0);
            }

            OdParam += acutBuildList(RTLE, 0); 
         }

         // indice per Object Data
         if (ade_dwgindexdef(_T("ObjData"), 1, OdParam.get_head()) != RTNORM)
            { GS_ERR_COD = eGSOpNotAble; break; }
      }

      // creazione indici
      GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = true; // usato nell'evento beginSave
      if (ade_dwgindex(GetAdeId()) != RTNORM)
      {
         GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = false;
         GS_ERR_COD = eGSOpNotAble;
         #if defined(GSDEBUG) // se versione per debugging
            gsc_printAdeErr();
         #endif
         break;
      }
      GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = false;

      Result = GS_GOOD;
   }
   while (0);

   if (ToDetach)
   {
      detach(); 
      if (ExtFilePath.len() > 0) gsc_SetFileTime(ExtFilePath);
   }

   gsc_set_cmddia(OldCmdDia);

   return Result;
}


/*********************************************************/
/*.doc long C_DWG::ApplyQuery                 <internal> /*
/*+
  Effettua la lettura degli oggetti che soddisfano la query impostata.
  Parametri:
  int retest; se MORETESTS -> in caso di errore riprova n volte 
              con i tempi di attesa impostati poi ritorna GS_BAD,
              ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
              (default = MORETESTS)

  Restituisce GS_GOOD in caso di successo, GS_BAD in caso di errore e GS_CAN
  in caso di interruzione voluta dall'utente con CTRL-C.
-*/  
/*********************************************************/
int C_DWG::ApplyQuery(int retest)
{
   AcMapSession    *mapApi;
   int             Result = GS_BAD, tentativi = 1;
   AcMapErrorStack *pErrorStack;
   AcMap::EErrCode MapError;

   if (!(mapApi = AcMapGetSession())) return GS_BAD; // sessione di MAP

   // get MAP error stack
	if (mapApi->GetErrorStack(pErrorStack) == Adesk::kFalse)
      { GS_ERR_COD = eGSCannotExtract; return GS_BAD; }
   
   // Pulisco lo stack degli errori
   pErrorStack->Clear();

   do
   {
      do
      {
         // se non è il primo tentativo
         if (tentativi > 1) gsc_wait(GEOsimAppl::GLOBALVARS.get_WaitTime()); // attesa tra un tentativo e l'altro

         if (activate() == GS_GOOD)
         {
            if (pMapAttachedDwg->LockForRead() == AcMap::kOk)
               if ((MapError = pMapAttachedDwg->ApplyQuery(Ids)) == AcMap::kOk)
                  Result = GS_GOOD;
               else
                  if (MapError == AcMap::kErrUsrBreak) 
                     Result = GS_CAN;

            pMapAttachedDwg->UnlockForRead();
            if (Result == GS_GOOD || Result == GS_CAN) break;
         }

         tentativi++;
      }
      while (tentativi <= GEOsimAppl::GLOBALVARS.get_NumTest() && retest == MORETESTS);

      // Se l'operazione ha avuto successo o se l'utente ha voluto interrompere
      if (Result == GS_GOOD || Result == GS_CAN) break;

      if (Result == GS_BAD &&                      // lettura fallita
          (retest != MORETESTS ||                  // solo un tentativo oppure
          gsc_dderroradmin(_T("ADE")) != GS_GOOD)) // l'utente vuole abbandonare
      {
         GS_ERR_COD = eGSCannotExtract; 
         break;
      }
   }
   while (1);
   
   return Result;
}


/*********************************************************/
/*.doc long C_DWG::QueryIn                    <internal> /*
/*+
  Questa funzione copia gli oggetti letti dalla query ADE corrente 
  nel disegno corrente.
  Parametri:
  C_SELSET *pSelSet;       Puntatore a gruppo di selezione in cui aggiungere 
                           gli oggetti estratti (default = NULL)
  long flag_fas;           Flag che identifica che alterazioni grafiche apportare agli
                           oggetti grafici (default = GSNoneSetting)
  C_FAS                    Caratteristiche grafiche (default = NULL)
  int retest;              se MORETESTS -> in caso di errore riprova n volte 
                           con i tempi di attesa impostati poi ritorna GS_BAD,
                           ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                           (default = MORETESTS)

  Restituisce GS_GOOD in caso di successo, GS_BAD in caso di errore e GS_CAN
  in caso di interruzione voluta dall'utente con CTRL-C.
-*/  
/*********************************************************/
int C_DWG::QueryIn(C_SELSET *pSelSet, long flag_set, C_FAS *pFAS, int retest)
{
   AcMapSession                 *mapApi;
   int                          Result = GS_BAD, tentativi = 1;
   ads_name                     Prev;
   AcMapErrorStack              *pErrorStack;
   AcMap::EErrCode              MapError;
   AcDbBlockTable               *pBlockTable;
   AcDbBlockTableRecord         *pBlockTableRecord;
   AcDbBlockTableRecordIterator *pBlockIterator;
   AcDbObjectId                 LastObjectId;

   // Imposto le alterazioni nella query ADE corrente senza alterazione blocco
   if (ade_altpclear() != RTNORM) return GS_BAD;
   if (pFAS && flag_set != GSNoneSetting)
   {
      // abilito le proprietà grafiche impostate se esistono
      if (ade_qrysetaltprop(ADE_TRUE) != RTNORM) return GS_BAD;
      if (gsc_altpdefine(*pFAS, flag_set) == GS_BAD) return GS_BAD;
   }
   else
      // disabilito le proprietà grafiche impostate se esistono
      if (ade_qrysetaltprop(ADE_FALSE) != RTNORM) return GS_BAD;

   if (!(mapApi = AcMapGetSession())) return GS_BAD; // sessione di MAP

   if (acdbEntLast(Prev) == RTERROR) ads_name_clear(Prev);

   // get MAP error stack
	if (mapApi->GetErrorStack(pErrorStack) == Adesk::kFalse)
      { GS_ERR_COD = eGSCannotExtract; return GS_BAD; }
   
   // Pulisco lo stack degli errori
   pErrorStack->Clear();

   if (pSelSet) // se devo aggiungere gli oggetti estratti al gruppo di selezione
   {
      if (acdbHostApplicationServices()->workingDatabase()->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk)
         { GS_ERR_COD = eGSCannotExtract; return GS_BAD; }
      if (pBlockTable->getAt(ACDB_MODEL_SPACE, pBlockTableRecord, AcDb::kForRead) != Acad::eOk)
         { pBlockTable->close(); GS_ERR_COD = eGSCannotExtract; return GS_BAD; }
      pBlockTable->close();
      // si posiziona in fondo
      pBlockTableRecord->newIterator(pBlockIterator, Adesk::kFalse);
      if (!pBlockIterator->done()) // se esiste un'entità
         pBlockIterator->getEntityId(LastObjectId);
      else
         LastObjectId.setNull();

      delete pBlockIterator;
      pBlockTableRecord->close();
   }

   do
   {
      do
      {
         // se non è il primo tentativo
         if (tentativi > 1) gsc_wait(GEOsimAppl::GLOBALVARS.get_WaitTime()); // attesa tra un tentativo e l'altro

         if (activate() == GS_GOOD)
         {
            if ((MapError = pMapAttachedDwg->QueryIn(Ids)) == AcMap::kOk)
               Result = GS_GOOD;
            else
               if (MapError == AcMap::kErrUsrBreak) 
                  Result = GS_CAN;

            if (Result == GS_GOOD || Result == GS_CAN) break;
         }

         tentativi++;
      }
      while (tentativi <= GEOsimAppl::GLOBALVARS.get_NumTest() && retest == MORETESTS);

      // Se l'operazione ha avuto successo o se l'utente ha voluto interrompere
      if (Result == GS_GOOD || Result == GS_CAN) break;

      if (Result == GS_BAD &&                      // lettura fallita
          (retest != MORETESTS ||                  // solo un tentativo oppure
          gsc_dderroradmin(_T("ADE")) != GS_GOOD)) // l'utente vuole abbandonare
      {
         GS_ERR_COD = eGSCannotExtract; 
         break;
      }
   }
   while (1);

   if (Result == GS_GOOD) // Se non ci sono stati problemi
   {
      long n;

      if ((n = Ids.length()) == 0)
      {
         ads_name Last;

         if (acdbEntLast(Last) == RTERROR) ads_name_clear(Last);

         // se n = 0 ma i due elast sono diversi vuol dire che ha estratto qualcosa
         // ma ApplyQuery o QueryIn hanno generato un errore non intrappolato da ade_errqty
         if (n == 0 && !ads_name_equal(Prev, Last))
         {
            // "Si è verificato un errore in fase di estrazione; possibile DWG rovinato."
            gsc_ddalert(gsc_msg(99));
            delete pMapAttachedDwg;
            GS_ERR_COD = eGSCannotExtract;
            return GS_BAD;
         }
      }
      else if (pSelSet) // se devo aggiungere gli oggetti estratti al gruppo di selezione
      {
         AcDbObjectId entityId;
         ads_name     ent;

         if (acdbHostApplicationServices()->workingDatabase()->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk)
            { GS_ERR_COD = eGSCannotExtract; return GS_BAD; }

         if (pBlockTable->getAt(ACDB_MODEL_SPACE, pBlockTableRecord, AcDb::kForRead) != Acad::eOk)
            { pBlockTable->close(); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
         pBlockTable->close();

         pBlockTableRecord->newIterator(pBlockIterator);
         if (!LastObjectId.isNull())
         {
            pBlockIterator->seek(LastObjectId);
            pBlockIterator->step(); // vado al successivo
         }
         for (; !pBlockIterator->done(); pBlockIterator->step())
         {
            pBlockIterator->getEntityId(entityId);
            acdbGetAdsName(ent, entityId);
            pSelSet->add(ent);
         }
         delete pBlockIterator;
         pBlockTableRecord->close();
      }
   }

   return Result;
}


/*********************************************************/
/*.doc int C_DWG::Report                      <internal> /*
/*+
  Questa funzione scrive informazioni degli oggetti grafici letti 
  dalla query ADE corrente su file testo.
  Parametri:
  C_STRING &Path;         File testo in cui scrivere il report
  const TCHAR *Mode;      Flag di apertura file
                          "w" Opens an empty file for writing.
                              If the given file exists, its contents are destroyed.
                          "a" Opens for writing at the end of the file (appending) without removing 
                              the EOF marker before writing new data to the file; 
                              creates the file first if it doesn’t exist.
  int retest;             se MORETESTS -> in caso di errore riprova n volte 
                          con i tempi di attesa impostati poi ritorna GS_BAD,
                          ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                          (default = MORETESTS)

  Restituisce GS_GOOD in caso di successo, GS_BAD in caso di errore e GS_CAN
  in caso di interruzione voluta dall'utente con CTRL-C.
-*/  
/*********************************************************/
int C_DWG::Report(C_STRING &Path, const TCHAR *Mode, int retest)
{
   AcMapSession    *mapApi;
   int             Result = GS_BAD, tentativi = 1;
   AcMapErrorStack *pErrorStack;
   AcMap::EErrCode MapError;
   C_STRING        RptFile;

   if (gsc_strcmp(Mode, _T("w")) == 0) // crea nuovo file
   {
      if (gsc_path_exist(Path) == GS_GOOD)
         if (gsc_delfile(Path) == GS_BAD) return GS_BAD;
      RptFile = Path;
   }
   else // aggiunge a file esistente
   {
      if (gsc_path_exist(Path) == GS_GOOD)
      {
         C_STRING dir, fname;

         gsc_splitpath(Path.get_name(), &RptFile, &dir, &fname);
         RptFile += dir;
         RptFile.set_chr(_T('\0'), RptFile.len() - 1); // elimino ultimo slash
         if (gsc_get_tmp_filename(RptFile.get_name(), fname.get_name(),
                                 _T(".TXT"), RptFile) == GS_BAD)
            return GS_BAD;
      }
      else
         RptFile = Path;
   }

   if (gsc_setADECurrRptFile(Path) == GS_BAD) return GS_BAD;

   if (!(mapApi = AcMapGetSession())) return GS_BAD; // sessione di MAP

   // get MAP error stack
	if (mapApi->GetErrorStack(pErrorStack) == Adesk::kFalse)
      { GS_ERR_COD = eGSCannotExtract; return GS_BAD; }
   
   // Pulisco lo stack degli errori
   pErrorStack->Clear();

   do
   {
      do
      {
         // se non è il primo tentativo
         if (tentativi > 1) gsc_wait(GEOsimAppl::GLOBALVARS.get_WaitTime()); // attesa tra un tentativo e l'altro

         if (activate() == GS_GOOD)
         {
            if (pMapAttachedDwg->LockForRead() == AcMap::kOk)
               if ((MapError = pMapAttachedDwg->Report(Ids)) == AcMap::kOk ||
                    MapError == AcMap::kErrObjectNotFound) // OD table non trovata
               {
                  // aggiungo il file di report temporaneo generato a quello originale
                  if (RptFile.comp(Path) != 0 && gsc_path_exist(RptFile) == GS_GOOD)
                     Result = gsc_appendfile(RptFile, Path);
                  else
                     Result = GS_GOOD;
               }
               else
                  if (MapError == AcMap::kErrUsrBreak) 
                     Result = GS_CAN;

            pMapAttachedDwg->UnlockForRead();
            if (Result == GS_GOOD || Result == GS_CAN) break;
         }

         tentativi++;
      }
      while (tentativi <= GEOsimAppl::GLOBALVARS.get_NumTest() && retest == MORETESTS);

      // Se l'operazione ha avuto successo o se l'utente ha voluto interrompere
      if (Result == GS_GOOD || Result == GS_CAN) break;

      if (Result == GS_BAD &&                      // lettura fallita
          (retest != MORETESTS ||                  // solo un tentativo oppure
          gsc_dderroradmin(_T("ADE")) != GS_GOOD)) // l'utente vuole abbandonare
      {
         GS_ERR_COD = eGSCannotExtract; 
         break;
      }
   }
   while (1);
 
   return Result;
}


/*********************************************************/
/*.doc long C_DWG::Preview                    <internal> /*
/*+
  Questa funzione crea un blocco di anteprima degli oggetti letti 
  dalla query ADE corrente nel disegno corrente.
  Parametri:
  long flag_fas;           Flag che identifica che alterazioni grafiche apportare agli
                           oggetti grafici (default = GSNoneSetting)
  C_FAS                    Caratteristiche grafiche (default = NULL)
  int retest;              se MORETESTS -> in caso di errore riprova n volte 
                           con i tempi di attesa impostati poi ritorna GS_BAD,
                           ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                           (default = MORETESTS)

  Restituisce GS_GOOD in caso di successo, GS_BAD in caso di errore e GS_CAN
  in caso di interruzione voluta dall'utente con CTRL-C.
-*/  
/*********************************************************/
int C_DWG::Preview(long flag_set, C_FAS *pFAS, int retest)
{
   AcMapSession    *mapApi;
   int             Result = GS_BAD, tentativi = 1;
   AcMapErrorStack *pErrorStack;
   AcMap::EErrCode MapError;

   // Imposto le alterazioni nella query ADE corrente senza alterazione blocco
   if (ade_altpclear() != RTNORM) return GS_BAD;
   if (pFAS && flag_set != GSNoneSetting)
   {
      // abilito le proprietà grafiche impostate se esistono
      if (ade_qrysetaltprop(ADE_TRUE) != RTNORM) return GS_BAD;
      if (gsc_altpdefine(*pFAS, flag_set) == GS_BAD) return GS_BAD;
   }
   else
      // disabilito le proprietà grafiche impostate se esistono
      if (ade_qrysetaltprop(ADE_FALSE) != RTNORM) return GS_BAD;

   if (!(mapApi = AcMapGetSession())) return GS_BAD; // sessione di MAP

   // get MAP error stack
	if (mapApi->GetErrorStack(pErrorStack) == Adesk::kFalse)
      { GS_ERR_COD = eGSCannotExtract; return GS_BAD; }
   
   // Pulisco lo stack degli errori
   pErrorStack->Clear();

   do
   {
      do
      {
         // se non è il primo tentativo
         if (tentativi > 1) gsc_wait(GEOsimAppl::GLOBALVARS.get_WaitTime()); // attesa tra un tentativo e l'altro

         if (activate() == GS_GOOD)
         {
            if (pMapAttachedDwg->LockForRead() == AcMap::kOk)
               if ((MapError = pMapAttachedDwg->Preview(Ids)) == AcMap::kOk)
                  Result = GS_GOOD;
               else
                  if (MapError == AcMap::kErrUsrBreak) 
                     Result = GS_CAN;

            pMapAttachedDwg->UnlockForRead();
            if (Result == GS_GOOD || Result == GS_CAN) break;
         }

         tentativi++;
      }
      while (tentativi <= GEOsimAppl::GLOBALVARS.get_NumTest() && retest == MORETESTS);

      // Se l'operazione ha avuto successo o se l'utente ha voluto interrompere
      if (Result == GS_GOOD || Result == GS_CAN) break;

      if (Result == GS_BAD &&                      // lettura fallita
          (retest != MORETESTS ||                  // solo un tentativo oppure
          gsc_dderroradmin(_T("ADE")) != GS_GOOD)) // l'utente vuole abbandonare
      {
         GS_ERR_COD = eGSCannotExtract; 
         break;
      }
   }
   while (1);
 
   return Result;
}


/*********************************************************/
/*.doc int C_DWG::get_ADEAliasPath()          <external> /*
/*+
  Effettua ritorna il path del disegno in formato ADE.
  Parametri:
-*/  
/*********************************************************/
TCHAR *C_DWG::get_ADEAliasPath(void)
{
   C_STRING ADEAliasPath;
   if (gsc_ADEdrv2nethost(get_name(), ADEAliasPath) == GS_BAD) return NULL;
   return ADEAliasPath.cut();
}


/*********************************************************/
/*.doc int C_DWG::setCrdSys()                 <internal> /*
/*+
  Questa funzione setta il sistema di coordinate ad un disegno.
  Parametri:
  const TCHAR *CrdSys;   Nome del sistema di coordinate

  Restituisce GS_GOOD se tutto OK altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DWG::setCrdSys(const TCHAR *CrdSys)
{
   int      Result = GS_GOOD, ToDetach = FALSE;
   C_STRING ExtFilePath;
   TCHAR    *PrevCrdSys = NULL;

   get_extents_file_path(ExtFilePath);
   // Se il disegno non era attaccato verrà attaccato, attivato
   // e alla fine staccato, se esisteva il file di etensione viene cancellato
   if (is_attached() == GS_BAD) ToDetach = TRUE;

   if (activate() == GS_BAD)
   {
      if (ToDetach)
      {
         detach(); 
         // Aggiorna la data del file a quella corrente
         if (ExtFilePath.len() > 0) gsc_SetFileTime(ExtFilePath);
      }
      return GS_BAD;
   }

   if (pMapAttachedDwg->GetProjection(PrevCrdSys) == AcMap::kOk)
   {
      if (gsc_strcmp(PrevCrdSys, CrdSys) == 0) // se sono uguali
      {
         if (ToDetach)
         {
            detach(); 
            // Aggiorna la data del file a quella corrente
            if (ExtFilePath.len() > 0) gsc_SetFileTime(ExtFilePath);
         }
         if (PrevCrdSys) delete PrevCrdSys;

         return GS_GOOD;      
      }
      if (PrevCrdSys) delete PrevCrdSys;
   }

   if (pMapAttachedDwg->LockForWrite(Adesk::kTrue) == AcMap::kOk) // set an exclusive lock
   {
      GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = true; // usato nell'evento beginSave
      pMapAttachedDwg->SetProjection(CrdSys); // setto il sistema di coordinate
      GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = false;
      pMapAttachedDwg->UnlockForWrite();
   }
   else
   {
      GS_ERR_COD = eGSFileLocked;
      Result     = GS_BAD;
   }

   if (ToDetach) detach();
   if (gsc_path_exist(ExtFilePath) == GS_GOOD) gsc_delfile(ExtFilePath);

   return Result;
}


/*********************************************************/
/*.doc int C_DWG::Save()                 <internal> /*
/*+
  Salva gli oggetti nel disegno.
  Parametri:
  C_SELSET &SelSet;  Gruppo di selezione
  int retest;        se MORETESTS -> in caso di errore riprova n volte 
                     con i tempi di attesa impostati poi ritorna GS_BAD,
                     ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                     (default = MORETESTS)

  oppure:

  ads_name selSet;   gruppo di selezione
  int retest;        se MORETESTS -> in caso di errore riprova n volte 
                     con i tempi di attesa impostati poi ritorna GS_BAD,
                     ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                     (default = MORETESTS)

  Restituisce GS_GOOD se tutto OK altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DWG::Save(C_SELSET &SelSet, int retest)
{
   ads_name ss;
   SelSet.get_selection(ss);
   
   return Save(ss, retest);
}
int C_DWG::Save(ads_name SelSet, int retest)
{
   if (activate() != GS_GOOD)return GS_BAD;
      
   return gsc_savetodwg(SelSet, pMapAttachedDwg, retest);
}


//-----------------------------------------------------------------------//
//////////////////      C_DWG       FINE      /////////////////////////////
//////////////////      C_DWG_LIST  INIZIO    /////////////////////////////
//-----------------------------------------------------------------------//


int C_DWG_LIST::copy(C_DWG_LIST &out)
{
   C_DWG *pDwg, *pNewDwg;

   pDwg = (C_DWG *) get_head();
   out.remove_all();
   
   while (pDwg)
   {
      if ((pNewDwg = new C_DWG()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }      
      pDwg->copy(*pNewDwg);
      out.add_tail(pNewDwg);

      pDwg = (C_DWG *) get_next();
   }

   return GS_GOOD;
}

int C_DWG_LIST::activate(void)
{
   C_DWG *pDwg;

   pDwg = (C_DWG *) get_head(); 
   while (pDwg)
   {
      if (pDwg->activate() == GS_BAD) return GS_BAD;
      pDwg = (C_DWG *) get_next();
   }

   return GS_GOOD;
}

int C_DWG_LIST::deactivate(void)
{
   C_DWG *pDwg;

   pDwg = (C_DWG *) get_head(); 
   while (pDwg)
   {
      if (pDwg->deactivate() == GS_BAD) return GS_BAD;
      pDwg = (C_DWG *) get_next();
   }

   return GS_GOOD;
}

int C_DWG_LIST::attach(void)
{
   C_DWG *pDwg;

   pDwg = (C_DWG *) get_head(); 
   while (pDwg)
   {
      if (pDwg->attach() == GS_BAD) return GS_BAD;
      pDwg = (C_DWG *) get_next();
   }

   return GS_GOOD;
}

int C_DWG_LIST::detach(void)
{
   C_DWG *pDwg;

   pDwg = (C_DWG *) get_head(); 
   while (pDwg)
   {
      if (pDwg->detach() == GS_BAD) return GS_BAD;
      pDwg = (C_DWG *) get_next();
   }

   return GS_GOOD;
}

int C_DWG_LIST::unlockObjs(void)
{
   C_DWG *pDwg;

   pDwg = (C_DWG *) get_head(); 
   while (pDwg)
   {
      if (pDwg->HasLocks())
         if (pDwg->unlockObjs() == GS_BAD) return GS_BAD;

      pDwg = (C_DWG *) get_next();
   }

   return GS_GOOD;
}

int C_DWG_LIST::ApplyQuery(int retest, int CounterToVideo)
{
   C_DWG *pDwg;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(482)); // "Ricerca oggetti grafici"
   long i = 0;

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.Init(get_count());

   pDwg = (C_DWG *) get_head(); 
   while (pDwg)
   {
      if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.Set(++i);
      if (pDwg->ApplyQuery(retest) == GS_BAD) return GS_BAD;
      
      pDwg = (C_DWG *) get_next();
   }

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

   return GS_GOOD;
}

int C_DWG_LIST::QueryIn(C_SELSET *pSelSet, long flag_set, C_FAS *pFAS, int retest, int CounterToVideo)
{
   C_DWG *pDwg;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(481)); // "Visualizzazione oggetti"
   long i = 0;

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.Init(get_count());

   pDwg = (C_DWG *) get_head(); 
   while (pDwg)
   {
      if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.Set(++i);
      if (pDwg->QueryIn(pSelSet, flag_set, pFAS, retest) == GS_BAD) return GS_BAD;
      
      pDwg = (C_DWG *) get_next();
   }

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

   return GS_GOOD;
}

int C_DWG_LIST::Report(C_STRING &Path, const TCHAR *Mode, int retest)
{
   C_DWG *pDwg;

   pDwg = (C_DWG *) get_head(); 
   while (pDwg)
   {
      if (pDwg->Report(Path, Mode, retest) == GS_BAD) return GS_BAD;
      
      pDwg = (C_DWG *) get_next();
   }

   return GS_GOOD;
}

int C_DWG_LIST::Preview(long flag_set, C_FAS *pFAS, int retest)
{
   C_DWG *pDwg;

   pDwg = (C_DWG *) get_head(); 
   while (pDwg)
   {
      if (pDwg->Preview(flag_set, pFAS, retest) == GS_BAD) return GS_BAD;
      
      pDwg = (C_DWG *) get_next();
   }

   return GS_GOOD;
}

int C_DWG_LIST::index(int prj, int cls)
{
   C_DWG *pDwg;

   pDwg = (C_DWG *) get_head(); 
   while (pDwg)
   {
      if (pDwg->index(prj, cls) == GS_BAD) return GS_BAD;
      pDwg = (C_DWG *) get_next();
   }

   return GS_GOOD;
}

int C_DWG_LIST::setCrdSys(const TCHAR *CrdSys)
{
   C_DWG *pDwg;

   pDwg = (C_DWG *) get_head(); 
   while (pDwg)
   {
      if (pDwg->setCrdSys(CrdSys) == GS_BAD) return GS_BAD;
      pDwg = (C_DWG *) get_next();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DWG_LIST::to_rb <internal> /*
/*+
-*/  
/*********************************************************/
resbuf *C_DWG_LIST::to_rb(void)
{
   C_RB_LIST List;
   C_DWG     *punt;
   TCHAR     str[1] = GS_EMPTYSTR, *string;
            
   punt = (C_DWG *) get_tail();
   if (!punt) 
      if ((List << acutBuildList(RTNIL, 0)) == NULL) return NULL;
   
   while (punt)
   {
      if ((string = punt->get_name()) == NULL) string = str;
                                         
      if ((List += acutBuildList(RTLB, RTSTR, string, 
                                 RTLONG, punt->get_size(),
                                 RTLE, 0)) == NULL)
         return NULL;

      punt = (C_DWG *) get_prev();
   }                             
   
   List.ReleaseAllAtDistruction(GS_BAD);
   
   return List.get_head();      
}


/*********************************************************/
/*.doc C_DWG_LIST::search_min <internal> /*
/*+
  Cerca il dwg con dimensione minore (se esiste) altrimenti ritorna NULL
   
  Restituisce il puntatore all'elemento
  altrimenti NULL se per esempio la lista è vuota
-*/  
/*********************************************************/
C_DWG *C_DWG_LIST::search_min(void)
{
   C_DWG *p_elem, *p_result = NULL;
   long  dimens = 0;

   p_elem = (C_DWG*) get_head();
   if (p_elem) dimens = p_elem->get_size();
   while (p_elem)
   {
      if (p_elem->get_size() <= dimens)
      {
         dimens = p_elem->get_size();
         p_result = p_elem;
      }
      
      p_elem = (C_DWG *) get_next();
   }

   return p_result;
}


/*********************************************************/
/*.doc int C_DWG_LIST::load                   <internal> /*
/*+
  Carica la lista dei dwg di una classe cls di un progetto prj
  
  Parametri:
  int prj;              codice del progetto
  int cls;              codice della classe
  bool DateSensitive;   Flag se = TRUE carica le estensioni verificando che le 
                        date dei file INF sia >= del relativo file DWG altrimenti
                        ignora questo controllo (default = TRUE)

  Restituisce GS_GOOD se tutto OK altrimenti GS_BAD
  N.B.: usando questa funzione la classe deve già esistere
-*/  
/*********************************************************/
int C_DWG_LIST::load(int prj, int cls, bool DateSensitive)
{
   C_CLASS    *p_cls;
   C_GPH_INFO *pGraphInfo;

   if ((p_cls = gsc_find_class(prj, cls, 0)) == NULL) return GS_BAD;

   // se simulazione leggo dalla prima sottoclasse (le altre sono uguali)
   if (p_cls->get_category() == CAT_EXTERN)
   {
      C_SUB  *pSub;
      C_NODE *ptr = p_cls->ptr_sub_list()->get_cursor(); // posiz. precedente

      if ((pSub = (C_SUB *) p_cls->ptr_sub_list()->get_head()) == NULL)
         { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      pGraphInfo = pSub->ptr_GphInfo();

      p_cls->ptr_sub_list()->getpos(ptr); // riposiziono il cursore
   }
   else 
      pGraphInfo = p_cls->ptr_GphInfo();

   if (pGraphInfo == NULL)
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   return load(prj, cls, 
               ((C_DWG_INFO *) pGraphInfo)->dir_dwg.get_name(),
               DateSensitive);
}
// N.B. usando questa funzione la classe può anche non essere ancora creata
int C_DWG_LIST::load(int prj, int cls, TCHAR *dir_dwg, bool DateSensitive)
{
   C_STRING path_cls, tmp_path_cls, Prefix;
   presbuf  filename, filesize, startfilename, startfilesize;
   int      index=0;
   C_DWG    *p_elem;
   long     n_adir;

   remove_all();
   
   if (!dir_dwg) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if (gsc_getClsPrefixDWGName(prj, cls, Prefix) == GS_BAD) return GS_BAD;

   path_cls = dir_dwg;
   path_cls += _T('\\');
   path_cls += Prefix;
   path_cls += _T("*.dwg");

   n_adir = gsc_adir(path_cls.get_name(), &filename, &filesize, NULL, NULL);

   if (n_adir == -1) { GS_ERR_COD = eGSCannotLoadDwgList; return GS_BAD; }
   if (n_adir == 0) return GS_GOOD;     // se non ci sono file ritorno GS_GOOD

   startfilename = filename;
   startfilesize = filesize;
   filename = filename->rbnext;
   filesize = filesize->rbnext;
   while (filename && filename->restype == RTSTR)
   {
      if (wcslen(filename->resval.rstring) == 11) // nome (7) + ".dwg" (4)
      {
         tmp_path_cls = dir_dwg;
         tmp_path_cls += _T('\\');
         tmp_path_cls += filename->resval.rstring;

         if ((p_elem = new C_DWG(tmp_path_cls, filesize->resval.rlong, DateSensitive)) == NULL)
         {
            acutRelRb(startfilename);
            acutRelRb(startfilesize);
            GS_ERR_COD=eGSOutOfMem;
            return GS_BAD;
         }

         add_tail(p_elem);
      }

      filename = filename->rbnext;
      filesize = filesize->rbnext;
   }
   acutRelRb(startfilename);
   acutRelRb(startfilesize);

   return GS_GOOD;
}


/*********************************************************/
/*.doc int C_DWG_LIST::update_extents         <internal> /*
/*+
  Dato un gruppo di oggetti grafici si ricavano le estensioni dei
  singoli oggetti e si aggiornano le estensioni memorizzate nei 
  relativi files INF dei disegni di provenienza se esistente.
  Parametri:
  C_SELSET &SS;      Gruppo di selezione
  int OnlyUpdate;    Flag; Se = GS_GOOD il programma aggiorna i file INF
                     già esistenti senza crearli se questi non esistono.
                     (default = GS_GOOD)

  Restituisce GS_GOOD se tutto OK altrimenti GS_BAD
-*/  
/*********************************************************/
int C_DWG_LIST::update_extents(C_SELSET &SS, int OnlyUpdate)
{
   int        result = GS_GOOD;
   C_DWG      *pDwg;
   double     Xmin, Ymin, Xmax, Ymax;
   C_RB_LIST  RbList;
   ade_id     dwg_id;
   long       i = 0;
   ads_name   ent;
   ads_point  corner1, corner2;
   C_INT_LIST ModifiedList;
   C_INT      *pModified;
   C_STRING   ExtFilePath, AdePath;

   // Creo una lista per sapere quali disegni hanno subito modifiche di estensione
   pDwg = (C_DWG *) get_head();
   while (pDwg)
   {
      if ((pModified = new C_INT) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      ModifiedList.add_tail(pModified);
      pDwg = (C_DWG *) get_next();
   }

   // Se non c'erano oggetti nel gruppo di selezione significa che si deve aggiornare
   // la data di tutti gli INF a seconda del parametro OnlyUpdate
   if (SS.length() == 0)
   {
      pDwg = (C_DWG *) get_head();
      while (pDwg)
      {
         if (OnlyUpdate == GS_GOOD) // Se si deve solo aggiornare file INF esistenti
         {
            pDwg->get_extents_file_path(ExtFilePath);
            // se non esisteva lo salto per non crearlo
            if (gsc_path_exist(ExtFilePath) == GS_BAD) 
            {
               pDwg = (C_DWG *) get_next();
               continue;
            }
         }

         if (pDwg->save_extents() == GS_BAD) result = GS_BAD;

         pDwg = (C_DWG *) get_next();
      }

      return result;
   }

   while (SS.entname(i++, ent) == GS_GOOD)
   {
		if (gsc_IsErasedEnt(ent) == GS_GOOD) continue; // se l'oggetto non esiste più
      // ricavo ID del DWG di provenienza dell'entità 
      if ((RbList << gsc_ade_qrygetdwgandhandle(ent)) == NULL) continue;
      dwg_id = (ade_id) RbList.get_head()->resval.rreal;
      if ((RbList << ade_dwggetsetting(dwg_id, _T("DWGNAME"))) == NULL)
         { result = GS_BAD; GS_ERR_COD = eGSCannotLoadDwgList; break; }

      // Converto path con alias
      AdePath = RbList.get_head()->resval.rstring;
      if (gsc_nethost2drive(AdePath) == GS_BAD) return GS_BAD;

      pDwg      = (C_DWG *) get_head();
      pModified = (C_INT *) ModifiedList.get_head();
      while (pDwg)
      {
         if (AdePath.comp(pDwg->get_name(), FALSE) == 0)
         {   
            if (OnlyUpdate == GS_GOOD) // Se si deve solo aggiornare file INF esistenti
            {
               pDwg->get_extents_file_path(ExtFilePath);
               // se non esisteva lo salto per non crearlo
               if (gsc_path_exist(ExtFilePath) == GS_BAD) break;
            }

            // calcolo estensioni dell'oggetto
            if (gsc_get_ent_window(ent, corner1, corner2) == GS_GOOD)
            {
               pDwg->get_extents(&Xmin, &Ymin, &Xmax, &Ymax);
               if (Xmin > corner1[X])
                  { Xmin = corner1[X]; pModified->set_key(1); }
               if (Ymin > corner1[Y]) 
                  { Ymin = corner1[Y]; pModified->set_key(1); }
               if (Xmax < corner2[X])
                  { Xmax = corner2[X]; pModified->set_key(1); } 
               if (Ymax < corner2[Y])
                  { Ymax = corner2[Y]; pModified->set_key(1); } 
            }

            if (pDwg->set_extents(Xmin, Ymin, Xmax, Ymax) == GS_BAD)
               { result = GS_BAD; break; }
         }

         pDwg      = (C_DWG *) get_next();
         pModified = (C_INT *) ModifiedList.get_next();
      }
      
      if (result == GS_BAD) return GS_BAD;
   }

   result    = GS_GOOD;
   pDwg      = (C_DWG *) get_head();
   pModified = (C_INT *) ModifiedList.get_head();
   while (pModified)
   {
      if (pModified->get_key() == 1) // se è stato modificato
         if (pDwg->save_extents() == GS_BAD)
            { result = GS_BAD; break; }

      pDwg = (C_DWG *) get_next();
      pModified = (C_INT *) ModifiedList.get_next();
   }

   return result;
}


/*********************************************************/
/*.doc int C_DWG_LIST::regen_extents_from_dwg <internal> /*
/*+
  Rigenera e scrive le nuove estensioni dei disegni della classe
  leggendo direttamente gli oggetti dai disegni.
  Parametri:
  int OnlyUpdate;    Flag; Se = GS_GOOD il programma aggiorna i file INF
                     già esistenti senza crearli se questi non esistono.
                     (default = GS_GOOD)

  Restituisce GS_GOOD se tutto OK altrimenti GS_BAD.

  N.B.: se i disegni risultano già attivi allora i files INF generati
  potrebbero non avere la data >= ai files DWG (e quindi non verrebbero usati).
-*/  
/*********************************************************/
int C_DWG_LIST::regen_extents_from_dwg(int OnlyUpdate)
{
   int    result = GS_GOOD;
   C_DWG  *pDwg;

   pDwg = (C_DWG *) get_head();
   while (pDwg)
   {
      if (OnlyUpdate == GS_GOOD) // Se si deve solo aggiornare file INF esistenti
      {
         C_STRING  ExtFilePath;

         pDwg->get_extents_file_path(ExtFilePath);
         if (gsc_path_exist(ExtFilePath) == GS_BAD)
         {  // se non esisteva lo salto per non crearlo
            pDwg = (C_DWG *) get_next();
            continue;
         }
      }

      if (pDwg->regen_extents_from_dwg() == GS_BAD) { result = GS_BAD; break; }
      if (pDwg->save_extents() == GS_BAD) { result = GS_BAD; break; }

      pDwg = (C_DWG *) get_next();
   }

   return result;
}


/*********************************************************/
/*.doc int C_DWG_LIST::intersect              <internal> /*
/*+
  Elimina i dwg della lista non intersecano la zona.
  Parametri:
  double Xmin;      Xmin zona da estrarre
  double Ymin;      Ymin zona da estrarre
  double Xmax;      Xmax zona da estrarre
  double Ymax;      Ymax zona da estrarre
  

  Restituisce GS_GOOD se tutto OK altrimenti GS_BAD
-*/  
/*********************************************************/
int C_DWG_LIST::intersect(double Xmin, double Ymin, double Xmax, double Ymax)
{
   C_DWG *pDwg;

   // effettuo l'attach di tutti i dwg
   pDwg = (C_DWG *) get_head();
   while (pDwg)
   {
      // se il disegno NON contiene oggetti nella zona
      if (pDwg->intersect(Xmin, Ymin, Xmax, Ymax) == GS_BAD)
      {
         remove_at(); // cancella e va al successivo
         pDwg = (C_DWG *) get_cursor();
      }
      else
         pDwg = (C_DWG *) get_next();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DWG_LIST::SelectDwg                  <internal> /*
/*+
  Ritorna il codice del dwg su cui salvare un oggetto. 
  
  Parametri: 
  int cls;               codice della classe (usato per creare nuovi DWG)

  Restituisce il puntatore al disegno se tutto OK altrimenti restituisce NULL.
-*/  
/*********************************************************/
C_DWG* C_DWG_LIST::SelectDwg(int cls)
{
   C_STRING Source;
   long     soglia = DEFAULT_DWG_DIM * 1024 * 1024;
   C_STRING path_cls;
   C_CLASS  *p_cls;
   C_DWG    *pDwg;
   
   // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
   // BISOGNA CONFIGURARE L'AMBIENTE DI SALVATAGGIO ade_prefsetval

   if (GS_CURRENT_WRK_SESSION->get_pPrj())
      soglia = GS_CURRENT_WRK_SESSION->get_pPrj()->get_dwg_dim() * 1024 * 1024; // dim in byte

   // cerco dalla lista dei dwg se ne esiste uno disponibile altrimenti lo creo
   pDwg = search_min();
   
   if (pDwg == NULL || pDwg->get_size() > soglia)
   {
      C_GPH_INFO *pGraphInfo;

      // imposto il nome di un disegno nuovo su cui salvare
      if ((p_cls = GS_CURRENT_WRK_SESSION->find_class(cls)) == NULL) return NULL;

      if (p_cls->get_category() == CAT_EXTERN)
      {
         C_SUB *pSub;
         
         if ((pSub = (C_SUB *) p_cls->ptr_sub_list()->get_head()) == NULL)
            { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
         pGraphInfo = pSub->ptr_GphInfo();
      }
      else
         pGraphInfo = p_cls->ptr_GphInfo();

      if (pGraphInfo == NULL)
         { GS_ERR_COD = eGSInvClassType; return NULL; }

      if (pDwg == NULL)
      {
         C_STRING Prefix;

         if (gsc_getClsPrefixDWGName(GS_CURRENT_WRK_SESSION->get_PrjId(), cls, Prefix) == GS_BAD)
            return NULL;
         // in path_cls la path dei dwg della classe
         path_cls = ((C_DWG_INFO *) pGraphInfo)->dir_dwg.get_name();
         path_cls += _T('\\');
         path_cls += Prefix;
         path_cls += _T("01.dwg");
      }
      else
      {
         TCHAR *dummy;

         if (gsc_setfilenamecode(GS_CURRENT_WRK_SESSION->get_PrjId(), cls, 
                                 ((C_DWG_INFO *) pGraphInfo)->dir_dwg.get_name(),
                                 &dummy) == GS_BAD)
            return GS_BAD;
         if ((path_cls.paste(dummy)) == GS_BAD) return NULL;
      }

      // copio il disegno prototipo di GEOsim (GEOPROTDWG = sample.dwg) nel direttorio
      // della classe
      Source  = GEOsimAppl::GEODIR + _T('\\') + GEOSAMPLEDIR + _T('\\') + GEOPROTDWG;
      if (gsc_copyfile(Source.get_name(), path_cls.get_name())==GS_BAD) return NULL;

      if ((pDwg = new C_DWG) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
      if (pDwg->set_name(path_cls.get_name()) == GS_BAD)
         { delete pDwg; return NULL; }

      // lo indicizzo
      if (pDwg->index(GS_CURRENT_WRK_SESSION->get_PrjId(), cls) == GS_BAD)
         return NULL;

      add_tail(pDwg);
   }
   
   // attivo il disegno
   if (pDwg->activate() == GS_BAD) return NULL;
   
   return pDwg;
}


long C_DWG_LIST::get_size() // somma delle dimensioni dei files DWG in byte
{
   C_DWG *pDwg;
   long  TotSize = 0;

   // effettuo l'attach di tutti i dwg
   pDwg = (C_DWG *) get_head();
   while (pDwg)
   {
      TotSize += pDwg->get_size();
      pDwg = (C_DWG *) get_next();
   }

   return TotSize;
}


/*********************************************************/
/*.doc C_DWG_LIST::SaveObjs                   <external> /*
/*+
  Salva gli oggetti editati nei disegni di origine passati come parametro.
  Il salvataggio comprende solo la parte grafica.
  Parametri:

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DWG_LIST::SaveObjs(void)
{
   AcMapSession         *mapApi;
   AcMapErrorStack      *pErrStack;
   AcMapProject         *pProj;
   AcMapDrawingSet      *pDSet;
   AcMapAttachedDrawing *pDwg = NULL;
   long                 tot;
   int                  Result = GS_GOOD;
   const TCHAR          *DwgName;
   C_DWG                *pSaveDwg;
   C_STR_LIST           ConvertedSaveDwgList;
   C_STR                *pConvertedSaveDwg;
   C_STRING             ConvertedDwgName;

   // converto i nomi in formato ADE
   pSaveDwg = (C_DWG *) get_head();
   while (pSaveDwg)
   {
      if (ConvertedDwgName.paste(pSaveDwg->get_ADEAliasPath()) == NULL) return GS_BAD;
      if ((pConvertedSaveDwg = new C_STR(ConvertedDwgName.get_name())) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      ConvertedSaveDwgList.add_tail(pConvertedSaveDwg);
      pSaveDwg = (C_DWG *) get_next();
   }

   if (!(mapApi = AcMapGetSession()))
      { GS_ERR_COD = eGSCannotSaveEntIntoDwg; return GS_BAD; } // sessione di MAP
   if (mapApi->GetErrorStack(pErrStack) == Adesk::kFalse)
      { GS_ERR_COD = eGSCannotSaveEntIntoDwg; return GS_BAD; } // stack degli errori
   pErrStack->Clear(); // pulisco lo stack degli errori
   if (mapApi->GetProject(pProj) == Adesk::kFalse)
      { GS_ERR_COD = eGSCannotSaveEntIntoDwg; return GS_BAD; } // progetto MAP
   if (pProj->GetDrawingSet(pDSet) == Adesk::kFalse)
      { GS_ERR_COD = eGSCannotSaveEntIntoDwg; return GS_BAD; } // disegni attaccati
   tot = pDSet->CountDrawings();

   for (int i = 0; i < tot; i++)
   {
      if (pDSet->GetDrawing(pDwg, i) == AcMap::kOk)
      {
         if (pDwg->GetAliasPath(DwgName) == AcMap::kOk && DwgName)
         {
            // cerca nome (case insensitive) tra i disegni da salvare
            if (ConvertedSaveDwgList.search_name(DwgName, FALSE) != NULL)
               if (pDwg->ActiveStatus() == AcMap::kDwgActive)
                  // Se ci sono oggetti da salvare provenienti dal disegno
                  if (pDwg->NumInSaveSet(AcMap::kQueriedExisted + AcMap::kQueriedErased) > 0)
                  {
                     if (pDwg->LockForWrite(Adesk::kFalse) == AcMap::kOk) // set no exclusive lock
                     {
                        if (pDwg->CloneBack() == AcMap::kOk)
                        {
                           AcMap::EErrCode Err;

                           GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = true; // usato nell'evento beginSave
                           if ((Err = pDwg->Save()) != AcMap::kOk)
                           {
                              GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = false;
                              #if defined(GSDEBUG) // se versione per debugging
                                 gsc_printAdeErr();
                              #endif
                              Result = GS_BAD;
                           }
                           GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = false;
                        }
                        else Result = GS_BAD;

                        pDwg->UnlockForWrite();

                        if (Result == GS_GOOD)
                           // rimuove il lock dai disegni
                           // pulendo il gruppo di selezione del salvataggio
                           pDwg->RemoveAllFromSaveSet();
                     }
                     else Result = GS_BAD;
                  }
         }
         delete pDwg;
         pDwg = NULL;
      }
      else Result = GS_BAD;
   }

   // Con Map 2011 succede che il salvataggio va a buon fine ma lo stack degli errori
   // viene comunque riempito... (no comment)
   // se ci sono stati degli errori
   //if (pErrStack->CountEntries() > 0) Result = GS_BAD;

   if (Result == GS_BAD) GS_ERR_COD = eGSCannotSaveEntIntoDwg;

   return Result;
}


//-----------------------------------------------------------------------//
//////////////////    C_DWG_LIST   FINE      //////////////////////////////
//////////////////    C_INT_DWGS   INIZIO    //////////////////////////////
//-----------------------------------------------------------------------//


C_INT_DWGS::C_INT_DWGS() : C_INT()
{}

C_INT_DWGS::~C_INT_DWGS()
{}

int C_INT_DWGS::LoadFromClsCode(int cls)
{
   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//////////////////    C_INT_DWGS     FINE    //////////////////////////////
//-----------------------------------------------------------------------//



/*********************************************************/
/*.doc gsc_SaveNewObjs                           <internal>
/*+
  Salvataggio dei nuovi oggetti di un disegno.
  Si esegue un ciclo su tutti i disegni della classe, per tutti
  quelli aventi dimensioni in MB inferiori o uguali alla soglia viene letta
  l'estensione (dove presente). Se esistono oggetti nuovi da salvare 
  che intersecano la zona allora vengono salvati in quel disegno.
  Per tutti gli oggetti che non sono inclusi in questa logica di salvataggio
  si applica la logica precedente (solo in base alla dimensione in MB dei disegni)
  Parametri:
  int        cls;
  C_SELSET   &NewObjs;
  C_DWG_LIST &DwgList;

  Ritorna GS_GOOD in caso di successo altrimenti ritorna GS_BAD.
-*/  
/*********************************************************/
int gsc_SaveNewObjs(int cls, C_SELSET &NewObjs, C_DWG_LIST &DwgList)
{
   C_DWG     *pDwg;
   double    Xmin, Ymin, Xmax, Ymax;
   long      soglia = DEFAULT_DWG_DIM * 1024 * 1024;
   C_SELSET  PartialSS;
   ads_point p1, p2;

   if (NewObjs.length() == 0)
   {
      acutPrintf(gsc_msg(621));  // "\nNessun nuovo oggetto."
      return GS_GOOD;
   }
   
   if (GS_CURRENT_WRK_SESSION->get_pPrj())
      soglia = GS_CURRENT_WRK_SESSION->get_pPrj()->get_dwg_dim() * 1024 * 1024; // dim in byte

   p1[Z] = p2[Z] = 0.0;
   // per ciascun DWG sotto la soglia dei MB stabiliti
   // determino gli oggetti che vi devono essere salvati 
   // finchè ci sono oggetti da salvare
   pDwg = (C_DWG *) DwgList.get_head();
   while (pDwg && NewObjs.length() > 0)
   {
      if (pDwg->get_size() <= soglia) // il DWG può ancora memorizzare dati
      {
         pDwg->get_extents(&Xmin, &Ymin, &Xmax, &Ymax);
         if (Xmin != 0 || Ymin != 0 || Xmax != 0 || Ymax != 0)
         {  // estensione DWG valida
            PartialSS.clear();
            NewObjs.copy(PartialSS);
            p1[X] = Xmin; p1[Y] = Ymin;
            p2[X] = Xmax; p2[Y] = Ymax;
            PartialSS.SpatialIntersectWindow(p1, p2);

            if (PartialSS.length() > 0)
            {
               // salvo nuovi oggetti
               if (pDwg->Save(PartialSS) == GS_BAD) break;
               // elimino gli oggetti salvati da GEOsimAppl::SAVE_SS
               GEOsimAppl::SAVE_SS.subtract(PartialSS);
               // elimino gli oggetti salvati da NewObjs
               NewObjs.subtract(PartialSS);
            }
         }         
      }

      pDwg = (C_DWG *) DwgList.get_next();
   }

   // Per tutti gli oggetti rimasti, applico la logica tradizionale
   if (NewObjs.length() > 0)
   {
      // determino il dwg su cui salvare
      if ((pDwg = DwgList.SelectDwg(cls)) == NULL) return GS_BAD;
      // salvo nuovi oggetti
      if (pDwg->Save(NewObjs) == GS_BAD) return GS_BAD;
      // elimino gli oggetti salvati da GEOsimAppl::SAVE_SS
      GEOsimAppl::SAVE_SS.subtract(NewObjs);
   }

   acutPrintf(gsc_msg(622)); // "\nSalvataggio nuovi oggetti terminato."

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_PrepareObjects4AdeSave                <internal>
/*+
  Carica il gruppo di selezione di ADE per il salvataggio:
  dato che non se ne conosce il nome e gli oggetti da salvare sono memorizzati
  in un gruppo di selezione di GEOsim, gli oggetti realmente cancellati vengono
  ripristinati e cancellati e gli oggetti modificati-nuovi vengono spostati a vuoto.
  Parametri:
  C_SELSET &SelSet; Gruppo di selezione da salvare

  N.B.: Se l'entget di un oggetto va a vuoto allora lo ripristina e lo cancella
        altrimenti lo sposta.
-*/  
/*********************************************************/
int gsc_PrepareObjects4AdeSave(int Cls, int Sub, C_SELSET &SelSet)
{
   ads_name    ent;
   C_MAP_ENV   AdeEnv;
   long        i;
   int         Result = GS_GOOD;
   C_SELSET    SSDel;
   C_CLASS     *pCls;
   bool        VerifyBlockDef = false, VerifyLineTypeDef = false, VerifyHatchDef = false;
   C_STR_LIST  GEOsimBlockNameList, NewBlockNameList, NewLineTypeNameList;
   C_2STR_LIST GEOsimLineTypeNameList, GEOsimHatchNameList;
   C_2STR_LIST NewHatchDescrList; // Nome riempimento e sua descrizione ASCII
   C_STRING    BlockName, LineTypeName;
   TCHAR       HatchName[MAX_LEN_HATCHNAME];
   
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) != GS_GOOD) return GS_BAD;

   // nessun oggetto nel gruppo di selezione
	if (SelSet.length() <= 0) return GS_GOOD;

   // Ritorna il puntatore alla classe cercata
   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(Cls, Sub)) == NULL) return GS_BAD;

   // inizializzo le variabili d'ambiente ADE per GEOsim in fase di salvataggio
   AdeEnv.SetEnv4GEOsimSave();

   // pulisco lo stack degli errori ADE
   ade_errclear();

   // se il disegno non ha ancora subito un congelamento 
   // posso saltare la "gsc_extract_objs"
   if (GS_CURRENT_WRK_SESSION->HasBeenFrozen() == GS_GOOD)
   {
      C_SELSET ss_from_dwg;

      // estraggo le entità dai dwg di provenienza (se esistono)
      if (gsc_ExtrOldGraphObjsFrom_SS(Cls, Sub, SelSet, ss_from_dwg) == GS_BAD)
         { AdeEnv.SetEnv4GEOsim(); return GS_BAD; }

	   if (ss_from_dwg.length() > 0)
      {        
         // cancello realmente intanto blocco e inserisco l'oggetto nel gruppo di sel. di ADE
         if (gsc_adeSelObjs(ss_from_dwg) == GS_BAD)
            { AdeEnv.SetEnv4GEOsim(); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }           

         if (ss_from_dwg.Erase() != GS_GOOD) return GS_BAD;
      }
   }

   // Se la classe è spaghetti o nodo
   if (pCls->get_category() == CAT_SPAGHETTI || pCls->get_type() == TYPE_NODE)
   {
      VerifyBlockDef = true;
      // Leggo la lista dei blocchi di GEOsim
      gsc_getGEOsimBlockList(GEOsimBlockNameList);
   }

   // Se la classe è spaghetti o polilinea o superficie
   if (pCls->get_category() == CAT_SPAGHETTI || pCls->get_type() == TYPE_POLYLINE || pCls->get_type() == TYPE_SURFACE)
   {
      VerifyLineTypeDef = true;
      // Leggo la lista dei tipi linea di GEOsim
      gsc_getGEOsimLType(GEOsimLineTypeNameList);
   }

   // Se la classe è spaghetti o superficie
   if (pCls->get_category() == CAT_SPAGHETTI || pCls->get_type() == TYPE_SURFACE)
   {
      VerifyHatchDef = true;
      // Leggo la lista dei riempimenti di GEOsim
      gsc_getGEOsimHatch(GEOsimHatchNameList);
   }

   // ripristino gli oggetti cancellati
   // e verifico 
   i = 0;
   while (SelSet.entname(i++, ent) == GS_GOOD)
		if (gsc_IsErasedEnt(ent) == GS_GOOD)  	// se l'oggetto non esiste
      {
         if (acdbEntDel(ent) == RTNORM) // lo rispristino
            if (SSDel.add(ent) == GS_BAD) { AdeEnv.SetEnv4GEOsim(); return GS_BAD; }
      }
      else
      {
         if (VerifyBlockDef)
            // Verifico se il blocco non è definito in GEOsim (scartando i blocchi $T e GEOSIM)
            if (gsc_get_blockName(ent, BlockName) == GS_GOOD && 
                BlockName.comp(_T("$T")) != 0 && BlockName.comp(_T("GEOSIM")) != 0)
               if (GEOsimBlockNameList.search_name(BlockName.get_name(), FALSE) == NULL)
                  NewBlockNameList.add_tail_unique(BlockName.get_name());

         if (VerifyLineTypeDef)
            // Verifico se il tipolinea non è definito in GEOsim (scartando i tipilinea "CONTINUOUS" e "DALAYER")
            if (gsc_get_lineType(ent, LineTypeName) == GS_GOOD && 
                LineTypeName.comp(_T("CONTINUOUS"), FALSE) != 0 && LineTypeName.comp(_T("BYLAYER"), FALSE) != 0)
               if (GEOsimLineTypeNameList.search_name(LineTypeName.get_name(), FALSE) == NULL)
                  NewLineTypeNameList.add_tail_unique(LineTypeName.get_name());

         if (VerifyHatchDef)
            // Verifico se il riempimento non è definito in GEOsim (scartando il riempineto "SOLID")
            if (gsc_getInfoHatch(ent, HatchName) == GS_GOOD && 
                  gsc_strcmp(HatchName, _T("SOLID"), FALSE) != 0)
               if (GEOsimHatchNameList.search_name(HatchName, FALSE) == NULL)
                  if (NewHatchDescrList.search_name(HatchName) == NULL)
                  {
                     C_STRING ASCII;

                     if (gsc_get_ASCIIFromHatch(ent, ASCII) == GS_GOOD)
                        NewHatchDescrList.add_tail_2str(HatchName, ASCII.get_name());
                  }
      }

   // Salvo le definizioni dei blocchi che non sono definiti in GEOsim
   // in USRTHM.DWG
   if (NewBlockNameList.get_count() > 0)
   {
      C_STRING DwgPath;

      DwgPath = GEOsimAppl::GEODIR;
      DwgPath += _T('\\');
      DwgPath += GEOTHMDIR;
      DwgPath += _T("\\USRTHM.DWG");

      gsc_save_blocks(NewBlockNameList, DwgPath, GS_GOOD);
   }

   // Salvo le definizioni dei tipilinea che non sono definiti in GEOsim
   if (NewLineTypeNameList.get_count() > 0)
   {
      C_STRING LineTypesPath;

      if (gsc_getCurrentGEOsimLTypeFilePath(LineTypesPath) == GS_GOOD)
         gsc_save_lineTypes(NewLineTypeNameList, LineTypesPath);
   }

   // Salvo le definizioni dei riempimenti che non sono definiti in GEOsim
   if (NewHatchDescrList.get_count() > 0)
   {
      C_STRING HatchesPath;

      // Salvo i riempienti sul file dei riempimenti sul server (poi lo copia sul client)
      if (gsc_getCurrentGEOsimHatchFilePath(HatchesPath, false) == GS_GOOD)
      {
         gsc_save_hatches(NewHatchDescrList, HatchesPath);
         // copio il file dei riempimenti anche sul client perchè serve ad AutoCAD
         gsc_refresh_thm();
      }
   }

   // inserisco gli oggetti nuovi nel gruppo di selezione ADE
   if (gsc_adeSelObjs(SelSet) == GS_BAD)
   {
      gsc_printAdeErr();
      AdeEnv.SetEnv4GEOsim();
      GS_ERR_COD = eGSAdsCommandErr;
      return GS_BAD;
   }           

   if (SSDel.length() > 0)
      if (SSDel.Erase() != GS_GOOD)
         { AdeEnv.SetEnv4GEOsim(); return GS_BAD; }

   // se ci sono stati degli errori
   if (ade_errqty() > 0) { AdeEnv.SetEnv4GEOsim(); return GS_BAD; }

   // Ripristino l'ambiente ADE per GEOsim
   AdeEnv.SetEnv4GEOsim();

   return Result;
}


/******************************************************************************/
/*.doc gsc_getClsPrefixDWGName                                    <internal>  */
/*+
  Questa funzione ritorna il prefisso da utilizzare nella composizione
  del nome dei disegni di una classe di entità.
  Parametri:
  int prj;
  int cls;
  C_STRING &Prefix;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
int gsc_getClsPrefixDWGName(int prj, int cls, C_STRING &Prefix)
{
   TCHAR *cod36prj = NULL, *cod36cls = NULL;

   if (gsc_long2base36(&cod36prj, prj, 2) == GS_BAD || !cod36prj) return GS_BAD;
   if (gsc_long2base36(&cod36cls, cls, 3) == GS_BAD || !cod36cls)
      { free(cod36prj); return GS_BAD; }
   Prefix = cod36prj;
   Prefix += cod36cls;
   free(cod36prj);
   free(cod36cls);

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_CreaFirstDWG <external>
/*+
  Dato il codice di una classe, crea il primo file DWG per una classe.
  Il DWG è vuoto, il suo nome è codificato in modo appropriato (con codice 
  in base 36) ed è copiato da un DWG campione di GEOsim.
  Parametri:
  int prj;            codice progetto
  int cls;            codice classe
  TCHAR *dir_dwg;     Direttorio del file
  const TCHAR *Coord; sistema di coordinate (default = NULL)
  TCHAR **path_dwg;   Path completa del file dwg (output)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. alloca memoria.
-*/  
/*********************************************************/
int gsc_CreaFirstDWG(int prj, int cls, TCHAR *dir_dwg,
                     const TCHAR *coord, TCHAR **path_dwg)
{
   C_STRING PathNewDwg, PathSampleDwg, path_new, Prefix;
   C_DWG    Dwg;

   if (gsc_getClsPrefixDWGName(prj, cls, Prefix) == GS_BAD) return GS_BAD;
   
   // in path_cls la path del dwg della classe
   PathNewDwg = dir_dwg;
   PathNewDwg += _T('\\');
   PathNewDwg += Prefix;
   PathNewDwg += _T("01.dwg");

   // file dwg prototipo per la creazione di un disegno nuovo
   PathSampleDwg  = GEOsimAppl::GEODIR + _T('\\') + GEOSAMPLEDIR + _T('\\') + GEOPROTDWG;

   // se esisteva PathNewDwg
   if (gsc_path_exist(PathNewDwg) == GS_GOOD) return GS_GOOD;

   // copio il disegno prototipo di GEOsim nel direttorio della classe
   if (gsc_copyfile(PathSampleDwg, PathNewDwg)==GS_BAD) return GS_BAD;

   // attivo il dwg su cui creare gli indici
   Dwg.set_name(PathNewDwg.get_name());
   if (Dwg.activate() == GS_BAD) return GS_BAD;

   // creo indici al disegno senza passare le coordinate della classe
   // perchè questa ancora non esiste
   if (Dwg.index() == GS_BAD) 
      { Dwg.detach(); return GS_BAD; }

   // setto il sistema di coordinate
   if (Dwg.setCrdSys(coord) == GS_BAD) return GS_BAD;

   // detach del dwg nuovo
   if (Dwg.detach() == GS_BAD) return GS_BAD;

   if (path_dwg)
      if ((*path_dwg = gsc_tostring(PathNewDwg.get_name())) == NULL) return GS_BAD;

   return GS_GOOD;
}


/////////////////////////////////////////////////////////////////////////
//         INIZIO FUNZIONI DELLA CATEGORIA C_SPAGHETTI                 //
/////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc gsc_optimizeDWG                        <internal> */
/*+                                                                       
  La funzione deve creare n disegni suddividendo il territorio
  occupato dagli oggetti della classe in rettangoli contenenti
  un numero di oggetti il più simile possibile. I disegni sono 
  scritti nel direttorio temporaneo di GEOSim.
  E' stimato che per ogni Mbyte di disegno ci siano :
  4000 oggetti per classi senza collegamento a DB
  4000 oggetti per classi con collegamento a DB senza blocchi DA
  1500 oggetti per classi con collegamento a DB con blocchi DA
  Parametri:
  const TCHAR *PrefixDWGName; Se <> NULL rappresenta il prefisso da usare
                              per nominare i files DWG generati dall'ottimizzazione
                              altrimenti viene usata la stringa "Optimized"
 C_STRING &ODTableName;       Se conyiene una stringa non  vuota indica se si tratta di oggetti appartenenti
                              a classe spaghetti. In questo caso vengono riallineati
                              tutti gli OD.
  ads_point pMin;          Punto inferiore sx del rettangolo da dividere
  ads_point pMax;          Punto superiore dx del rettangolo da dividere
  double    nObjs;         N. di oggetti grafici nel rettangolo indicato
  long      ObjsMaxLimit;  N. max. di oggetti che può stare in ogni disegno
  bool      HorizSplit;    se = TRUE la divisione avviene in modo orizzonatale
                           altrimenti avviene in modo verticale (default = FALSE)
  C_HANDLERECT_BTREE       *pHandleRectTree; uso interno per memorizzare le estensioni degli oggetti
                                             e quindi evitare il ricalcolo (defaul = NULL)
                                             Purtoppo da prove fatte dopo aver memorizzato
                                             circa 60.000 oggetti AutoCAD dà errore di
                                             STACK OVERFLOW. Meglio non usarlo, per ora.
  int       DepthLevel;    uso interno; Livello di profondità della ricorsione (poichè il numero
                           max di gruppi di selezione = 128 si è deciso che
                           non si può scendere oltre la profondità 100 di divisione)
  int       nUnusefulDivision; uso interno; N. di divisioni effettuate in cui il numero di 
                               oggetti è sempre lo stesso (per fermare la ricorsione)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_optimizeDWG(const TCHAR *PrefixDWGName, C_STRING &ODTableName,
                    ads_point pMin, ads_point pMax, C_SELSET &SS,
                    long ObjsMaxLimit, bool HorizSplit, 
                    C_HANDLERECT_BTREE *pHandleRectTree, int DepthLevel, int nUnusefulDivision)
{
   double    Medium, MaxDiffLimit, CoordMin, CoordMax;
   long      nObjs, Diff, PrevDiff, nPartial1, nPartial2, nPrevPartial1, nPrevPartial2;
   ads_point p1Rect1, p2Rect1, p1Rect2, p2Rect2;
   ads_point Prev_p1Rect1, Prev_p2Rect1, Prev_p1Rect2, Prev_p2Rect2;
   int       Step = 1;

   // Conto gli oggetti contenuti e che intersecano il rettangolo
   nObjs = SS.get_SpatialIntersectWindowNumObjs(pMin, pMax, GS_GOOD, pHandleRectTree);
   if (nObjs == 0) return GS_GOOD;

   if (ObjsMaxLimit == 0) ObjsMaxLimit = DEFAULT_DWG_DIM * 4500;

   // Se il rettangolo contiene un n. di oggetti minore o uguale al limite
   // oppure se si è diviso il disegno per 10 volte avendo sempre lo stesso
   // numero di oggetti
   // oppure se il livello di profondità della ricorsione ha superato 100
   if (nObjs <= ObjsMaxLimit || nUnusefulDivision > 10 || DepthLevel > 100)
   {
      C_STRING TempDir, Source;
      TCHAR    *Path;
      C_DWG    Dwg;

      acutPrintf(gsc_msg(83)); // "\nSalvataggio cella..."

      TempDir = GEOsimAppl::CURRUSRDIR;
      TempDir += _T('\\');
      TempDir += GEOTEMPDIR;

      if (PrefixDWGName) // se esiste uso il prefisso
      {
         if (gsc_tempfilebase36(TempDir.get_name(), PrefixDWGName, _T(".DWG"), &Path) == GS_BAD)
            return GS_BAD;
      }
      else // altrimenti ne uso uno di default
         if (gsc_get_tmp_filename(TempDir.get_name(), _T("Optimized"), _T(".DWG"), &Path) == GS_BAD)
            return GS_BAD;

      // copio il disegno prototipo di GEOsim (GEOPROTDWG = sample.dwg)
      Source  = GEOsimAppl::GEODIR + _T('\\') + GEOSAMPLEDIR + _T('\\') + GEOPROTDWG;
      if (gsc_copyfile(Source.get_name(), Path) == GS_BAD) { free(Path); return GS_BAD; }
      // attach del dwg nuovo
      Dwg.set_name(Path);
      free(Path);
      if (Dwg.activate() == GS_BAD) return GS_BAD;

      // Salvo nel disegno in questo modo per ottenere un corretto 
      // settaggio delle estensioni
      C_MAP_ENV   AdeEnv;
      EAsiBoolean PrevEraseSavedBackObjects, PrevCreateBackupFileOfSourceDwg, PrevRemoveLockAfterSave;
      
      AdeEnv.FreezeCurrentAdeEnvironment();

      if (ODTableName.get_name())
      {  // Non cancello gli oggetti salvati
         PrevEraseSavedBackObjects = (!AdeEnv.EraseSavedBackObjects || AdeEnv.EraseSavedBackObjects->restype == RTNIL) ? kAsiFalse : kAsiTrue;
         AdeEnv.SetAdeEnvVariable(_T("EraseSavedBackObjects"), kAsiFalse);
         // Non rimuovo il lock dopo aver salvato   
         PrevRemoveLockAfterSave = (!AdeEnv.RemoveLockAfterSave || AdeEnv.RemoveLockAfterSave->restype == RTNIL) ? kAsiFalse : kAsiTrue;
         AdeEnv.SetAdeEnvVariable(_T("RemoveLockAfterSave"), kAsiFalse);
      }
      else
      {  // Cancello gli oggetti salvati
         PrevEraseSavedBackObjects = (!AdeEnv.EraseSavedBackObjects || AdeEnv.EraseSavedBackObjects->restype == RTNIL) ? kAsiFalse : kAsiTrue;
         AdeEnv.SetAdeEnvVariable(_T("EraseSavedBackObjects"), kAsiTrue);
         // Rimuovo il lock dopo aver salvato   
         PrevRemoveLockAfterSave = (!AdeEnv.RemoveLockAfterSave || AdeEnv.RemoveLockAfterSave->restype == RTNIL) ? kAsiFalse : kAsiTrue;
         AdeEnv.SetAdeEnvVariable(_T("RemoveLockAfterSave"), kAsiTrue);
      }

      // Non faccio il Backup
      PrevCreateBackupFileOfSourceDwg = (!AdeEnv.CreateBackupFileOfSourceDwg || AdeEnv.CreateBackupFileOfSourceDwg->restype == RTNIL) ? kAsiFalse : kAsiTrue;
      AdeEnv.SetAdeEnvVariable(_T("CreateBackupFileOfSourceDwg"), kAsiFalse);
      // Rimuovo il lock dopo aver salvato   
      PrevRemoveLockAfterSave = (!AdeEnv.RemoveLockAfterSave || AdeEnv.RemoveLockAfterSave->restype == RTNIL) ? kAsiFalse : kAsiTrue;
      AdeEnv.SetAdeEnvVariable(_T("RemoveLockAfterSave"), kAsiTrue);

      // Poichè il gruppo di oggetti da salvare può contenere degli oggetti che
      // sono associati ad altri oggetti non inclusi nel gruppo stesso
      // utilizzo la funzione "ade_editlockobjs"
      C_SELSET PartialSS;
      ads_name LockedSS;
      int      LockedSSLen, Result = GS_BAD;

      do
      {     
         if (SS.get_SpatialIntersectWindowSS(pMin, pMax, PartialSS, GS_GOOD,
                                             pHandleRectTree, GS_GOOD) == GS_BAD)
            break;

         PartialSS.get_selection(LockedSS);
         if ((LockedSSLen = (int) gsc_editlockobjs(LockedSS)) < nObjs)
         {
            ads_name_clear(LockedSS);
            // ricavo gli oggetti nel gruppo di selez. ADE
            if (ade_editlocked(LockedSS) == RTNORM)
            {
               long      i = 0;
               ads_name  ent;
               ads_point pt;
               TCHAR     Msg[MAX_LEN_MSG], handle[MAX_LEN_HANDLE];

               PartialSS.subtract(LockedSS);
               while (PartialSS.entname(i++, ent) == GS_GOOD)
               {
                  if (gsc_get_firstPoint(ent, pt) == GS_GOOD)
                  {
                     swprintf(Msg, MAX_LEN_MSG, _T("\nObject not saved; Object located on point %f,%f,%f"),
                             pt[X], pt[Y], pt[Z]);
                     acutPrintf(Msg);
                     gsc_write_log(Msg);

                  }
                  if (gsc_enthand(ent, handle) == GS_GOOD)
                  {
                     swprintf(Msg, MAX_LEN_MSG, _T("\nObject not saved; Object Handle %s"),
                              handle);
                     acutPrintf(Msg);
                     gsc_write_log(Msg);
                  }
               }
            }

            break;
         }

         ads_name_clear(LockedSS);
         // ricavo gli oggetti nel gruppo di selez. ADE
         if (ade_editlocked(LockedSS) != RTNORM) break;
         // Li salvo nel disegno
         if (Dwg.Save(LockedSS) == GS_BAD)
            { ads_ssfree(LockedSS); break; }

         // Se gli oggetti appartengono ad una classe spaghetti allora devono essere
         // riallineati gli OD con handle e nome disegno di provenienza.
         if (ODTableName.get_name())
         {
            long     i = 0;
            ads_name ent;

            // Cancello gli oggetti salvati
            PrevEraseSavedBackObjects = (!AdeEnv.EraseSavedBackObjects || AdeEnv.EraseSavedBackObjects->restype == RTNIL) ? kAsiFalse : kAsiTrue;
            AdeEnv.SetAdeEnvVariable(_T("EraseSavedBackObjects"), kAsiTrue);
            // Rimuovo il lock dopo aver salvato   
            PrevRemoveLockAfterSave = (!AdeEnv.RemoveLockAfterSave || AdeEnv.RemoveLockAfterSave->restype == RTNIL) ? kAsiFalse : kAsiTrue;
            AdeEnv.SetAdeEnvVariable(_T("RemoveLockAfterSave"), kAsiTrue);

            i = 0;
            Result = GS_GOOD;
            while (acedSSName(LockedSS, i++, ent) == RTNORM)
               // memorizzo identificatore nell'entità
               if (gsc_setID2ODTable(ent, ODTableName) == GS_BAD) { Result = GS_BAD; break; }

            if (Result == GS_BAD) { ads_ssfree(LockedSS); break; }
            Result = GS_BAD;

            // salvo le entità modificate nel disegno di provenienza
            if (gsc_editlockobjs(LockedSS) == ADE_REALFAIL) { ads_ssfree(LockedSS); break; }
            if (gsc_saveobjs() != GS_GOOD){ ads_ssfree(LockedSS); break; }
         }

         ads_ssfree(LockedSS);
         Result = GS_GOOD;
      }
      while (0);
      
      Dwg.detach();
 
      AdeEnv.SetAdeEnvVariable(_T("EraseSavedBackObjects"), PrevEraseSavedBackObjects);
      AdeEnv.SetAdeEnvVariable(_T("CreateBackupFileOfSourceDwg"), PrevCreateBackupFileOfSourceDwg);
      AdeEnv.SetAdeEnvVariable(_T("RemoveLockAfterSave"), PrevRemoveLockAfterSave);

      //acutPrintf("\nrettangolo p1=%f,%f p2=%f,%f", pMin[X], pMin[Y], pMax[X], pMax[Y]);

      ads_point pt1, pt2, pt3, pt4;
      
      pt1[Z] = pt2[Z] = pt3[Z] = pt4[Z] = 0.0;
      pt1[X] = pMin[X]; pt1[Y] = pMin[Y];
      pt2[X] = pMin[X]; pt2[Y] = pMax[Y];
      pt3[X] = pMax[X]; pt3[Y] = pMax[Y];
      pt4[X] = pMax[X]; pt4[Y] = pMin[Y];
      acedGrDraw(pt1, pt2, 1, 1);
      acedGrDraw(pt2, pt3, 1, 1);
      acedGrDraw(pt3, pt4, 1, 1);
      acedGrDraw(pt4, pt1, 1, 1);
      
      return Result;
   }

   ads_point_set(pMin, p1Rect1);
   ads_point_set(pMax, p2Rect2);

   // la differenza massima tra il n. di oggetti contenuti nei 2 rettangoli
   // deve essere minore o uguale al 10 percento del globale
   MaxDiffLimit = nObjs * (10.0 / 100.0); // 10 %
   PrevDiff     = nObjs + 1;

   if (HorizSplit) // bisogna dividere il rettangolo in due tagliando orizzontalmente
   {
      // Il rettangolo 1 è quello inferiore
      // Il rettangolo 2 è quello superiore

      CoordMin = pMin[Y];
      CoordMax = pMax[Y];

      p2Rect1[X] = pMax[X]; p2Rect1[Z] = pMax[Z];
      p1Rect2[X] = pMin[X]; p1Rect2[Z] = pMin[Z];

      do
      {
         Medium = (CoordMax + CoordMin) / 2; // coordinata Y media
         p1Rect2[Y] = p2Rect1[Y] = Medium;

         acutPrintf(gsc_msg(79), DepthLevel, Step++); // "\nRicerca cella (livello %ld - passo %ld)..."

         // Conto gli oggetti contenuti e che intersecano il rettangolo 1
         nPartial1 = SS.get_SpatialIntersectWindowNumObjs(p1Rect1, p2Rect1, GS_GOOD, 
                                                          pHandleRectTree, GS_GOOD);
         // Conto gli oggetti contenuti nel rettangolo 2
         nPartial2 = nObjs - nPartial1;
         // Differenza tra n di oggetti dei 2 rettangoli
         Diff = labs(nPartial1 - nPartial2);

         // Se la differenza è entro il limite accettabile
         if (Diff <= MaxDiffLimit) break;

         // se la differenza è maggiore o uguale alla precedente
         if (Diff > PrevDiff)
         {
            nPartial1 = nPrevPartial1;
            nPartial2 = nPrevPartial2;
            ads_point_set(Prev_p1Rect1, p1Rect1);
            ads_point_set(Prev_p2Rect1, p2Rect1);
            ads_point_set(Prev_p1Rect2, p1Rect2);
            ads_point_set(Prev_p2Rect2, p2Rect2);
            break;
         }

         PrevDiff = Diff;
         nPrevPartial1 = nPartial1;
         nPrevPartial2 = nPartial2;
         ads_point_set(p1Rect1, Prev_p1Rect1);
         ads_point_set(p2Rect1, Prev_p2Rect1);
         ads_point_set(p1Rect2, Prev_p1Rect2);
         ads_point_set(p2Rect2, Prev_p2Rect2);

         // calcolo la coordinata Y in modo dicotomico
         if (nPartial1 > nPartial2)
            // la sposto in basso
            CoordMax = Medium; 
         else
            // la sposto in in alto
            CoordMin = Medium; 
      }
      while (1);

      acutPrintf(gsc_msg(79), DepthLevel, Step++); // "\nRicerca cella (livello %ld - passo %ld)..."
      // se la divisione non è servita aumento il livello di uno
      if (nPartial1 == nObjs) nUnusefulDivision++;
      // Elaboro rettangolo 1 dividendolo in verticale
      if (gsc_optimizeDWG(PrefixDWGName, ODTableName,
                          p1Rect1, p2Rect1, SS, ObjsMaxLimit, FALSE,
                          pHandleRectTree, DepthLevel + 1, nUnusefulDivision) == GS_BAD)
         return GS_BAD;
 
      acutPrintf(gsc_msg(79), DepthLevel, Step++); // "\nRicerca cella (livello %ld - passo %ld)..."
      // se la divisione non è servita aumento il livello di uno
      if (nPartial2 == nObjs) nUnusefulDivision++;
      // Elaboro rettangolo 2 dividendolo in verticale
      if (gsc_optimizeDWG(PrefixDWGName, ODTableName,
                          p1Rect2, p2Rect2, SS, ObjsMaxLimit, FALSE,
                          pHandleRectTree, DepthLevel + 1, nUnusefulDivision) == GS_BAD)
         return GS_BAD;
   }
   else // bisogna dividere il rettangolo in due tagliando verticalmente
   {
      // Il rettangolo 1 è quello a sinistra
      // Il rettangolo 2 è quello a destra

      CoordMin = pMin[X];
      CoordMax = pMax[X];
      p2Rect1[Y] = pMax[Y]; p2Rect1[Z] = pMax[Z];
      p1Rect2[Y] = pMin[Y]; p1Rect2[Z] = pMin[Z];

      do
      {
         Medium = (CoordMax + CoordMin) / 2; // coordinata X media
         p1Rect2[X] = p2Rect1[X] = Medium;

         acutPrintf(gsc_msg(79), DepthLevel, Step++); // "\nRicerca cella (livello %ld - passo %ld)..."

         // Conto gli oggetti contenuti e che intersecano il rettangolo 1
         nPartial1 = SS.get_SpatialIntersectWindowNumObjs(p1Rect1, p2Rect1, GS_GOOD,
                                                          pHandleRectTree, GS_GOOD);
         // Conto gli oggetti contenuti nel rettangolo 2
         nPartial2 = nObjs - nPartial1;
         // Differenza tra n di oggetti dei 2 rettangoli
         Diff = labs(nPartial1 - nPartial2);

         // Se la differenza è entro il limite accettabile
         if (Diff <= MaxDiffLimit) break;

         // se la differenza è maggiore alla precedente
         if (Diff > PrevDiff)
         {
            nPartial1 = nPrevPartial1;
            nPartial2 = nPrevPartial2;
            ads_point_set(Prev_p1Rect1, p1Rect1);
            ads_point_set(Prev_p2Rect1, p2Rect1);
            ads_point_set(Prev_p1Rect2, p1Rect2);
            ads_point_set(Prev_p2Rect2, p2Rect2);
            break;
         }

         PrevDiff = Diff;
         nPrevPartial1 = nPartial1;
         nPrevPartial2 = nPartial2;
         ads_point_set(p1Rect1, Prev_p1Rect1);
         ads_point_set(p2Rect1, Prev_p2Rect1);
         ads_point_set(p1Rect2, Prev_p1Rect2);
         ads_point_set(p2Rect2, Prev_p2Rect2);

         // calcolo la coordinata X in modo dicotomico
         if (nPartial1 > nPartial2)
            // la sposto a sinistra
            CoordMax = Medium; 
         else
            // la sposto a destra
            CoordMin = Medium; 
      }
      while (1);

      acutPrintf(gsc_msg(79), DepthLevel, Step++); // "\nRicerca cella (livello %ld - passo %ld)..."
      // se la divisione non è servita aumento il livello di uno
      if (nPartial1 == nObjs) nUnusefulDivision++;
      // Elaboro rettangolo 1 dividendolo in orizzontale
      if (gsc_optimizeDWG(PrefixDWGName, ODTableName,
                          p1Rect1, p2Rect1, SS, ObjsMaxLimit, TRUE,
                          pHandleRectTree, DepthLevel + 1, nUnusefulDivision) == GS_BAD)
         return GS_BAD;
      
      acutPrintf(gsc_msg(79), DepthLevel, Step++); // "\nRicerca cella (livello %ld - passo %ld)..."
      // se la divisione non è servita aumento il livello di uno
      if (nPartial2 == nObjs) nUnusefulDivision++;
      // Elaboro rettangolo 2 dividendolo in orizzontale
      if (gsc_optimizeDWG(PrefixDWGName, ODTableName,
                          p1Rect2, p2Rect2, SS, ObjsMaxLimit, TRUE,
                          pHandleRectTree, DepthLevel + 1, nUnusefulDivision) == GS_BAD)
         return GS_BAD;
   }
 
   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_DisplayDWGExt                      <external> */
/*+                                                                       
  La funzione LISP disegna dei rettangoli che individuano le
  estensioni dei disegni che compongono la banca dati grafica di
  una classe. Al centro di ciascun rettangolo verrà scritto il 
  nome del disegno relativo a quell'estensione.
  Parametri:
  (<Prj><Cls>)

  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR. 
-*/  
/*********************************************************/
int gs_DisplayDWGExt(void)
{
   presbuf arg;
   int     Prj, Cls;

   arg = acedGetArgs();
   acedRetNil();
   // Legge codice progetto
   if (!arg || gsc_rb2Int(arg, &Prj) == GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   // Legge codice classe
   if (!(arg = arg->rbnext) || gsc_rb2Int(arg, &Cls) == GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (gsc_DisplayDWGExt(Prj, Cls) == GS_BAD) return RTERROR;

   acedRetT();
   return RTNORM;
}
int gsc_DisplayDWGExt(int Prj, int Cls)
{
   C_DWG_LIST ClassDwgList;
   C_DWG      *pDwg;
   double     Xmin, Ymin, Xmax, Ymax;

   // inizializzo la lista dei dwg della classe
   if (ClassDwgList.load(Prj, Cls) == GS_BAD) return GS_BAD;

   // ciclo per ogni disegno
   pDwg = (C_DWG *) ClassDwgList.get_head();
   while (pDwg)
   {
      if (pDwg->regen_extents_from_dwg() != GS_GOOD) return GS_BAD;
      pDwg->get_extents(&Xmin, &Ymin, &Xmax, &Ymax);

      AcDbBlockTable       *pBlockTable;
      AcDbBlockTableRecord *pBlockTableRecord;
      AcDbObjectId         Id;
      AcGePoint2d          pt;

      if (acdbHostApplicationServices()->workingDatabase()->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk)
         { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
      if (pBlockTable->getAt(ACDB_MODEL_SPACE, pBlockTableRecord, AcDb::kForWrite) != Acad::eOk)
         { pBlockTable->close(); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
      pBlockTable->close();

      // disegno il rettangolo rappresentante le estensioni
      AcDbPolyline *pRect = new AcDbPolyline(4); // 4 vertici
      pt.set(Xmin, Ymin);
      pRect->addVertexAt(0, pt);
      pt.set(Xmax, Ymin);
      pRect->addVertexAt(0, pt);
      pt.set(Xmax, Ymax);
      pRect->addVertexAt(0, pt);
      pt.set(Xmin, Ymax);
      pRect->addVertexAt(0, pt);
      pRect->setClosed(Adesk::kTrue);
      pBlockTableRecord->appendAcDbEntity(Id, pRect);

      // disegno l'etichetta
      C_STRING FileName;
      pDwg->get_name();
      gsc_splitpath(pDwg->get_name(), NULL, NULL, &FileName);

      AcDbMText  *pLbl = new AcDbMText();
      AcGePoint3d Location((Xmax + Xmin) / 2, (Ymax + Ymin) / 2, 0.0);
      pLbl->setLocation(Location);
      pLbl->setTextHeight(gsc_min((Ymax - Ymin), (Xmax - Xmin)) / (FileName.len()));
      pLbl->setWidth(Xmax - Xmin);
      pLbl->setContents(FileName.get_name());
      pLbl->setAttachment(AcDbMText::kMiddleCenter);
      pBlockTableRecord->appendAcDbEntity(Id, pLbl);

      pBlockTableRecord->close();
      pRect->close();
      pLbl->close();

      pDwg = (C_DWG *) ClassDwgList.get_next();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_get_dwg_list                        <external> */
/*+                                                                       
  La funzione LISP restituisce la lista dei disegni di una classe
  nel seguente formato:
  ((nome dwg><dimensione>)(...))
  Parametri:
  (<Prj><Cls>[<dir_dwg>])

  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR. 
-*/  
/*********************************************************/
int gs_get_dwg_list(void)
{
   presbuf            arg;
   C_RB_LIST          ret;
   int                Prj, Cls;
   C_PROJECT          *pPrj;
   C_SINTH_CLASS_LIST SinthClassList;
   C_SINTH_CLASS      *pSinthClass;
   C_DWG_LIST         DwgList;

   arg = acedGetArgs();
   acedRetNil();
   // Legge codice progetto
   if (!arg || gsc_rb2Int(arg, &Prj) == GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   // Legge codice classe
   if (!(arg = arg->rbnext) || gsc_rb2Int(arg, &Cls) == GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   // Legge dir_dwg della classe (opzionale)
   if ((arg = arg->rbnext) && arg->restype == RTSTR)
   {
      if (gsc_strlen(arg->resval.rstring) == 0) return RTNORM;
      if (DwgList.load(Prj, Cls, arg->resval.rstring) == GS_BAD)
         return RTERROR;
   }
   else
   {
      // Cerca progetto nella lista GEOsimAppl::PROJECTS
      if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(Prj)) == NULL)
         { GS_ERR_COD = eGSInvalidPrjCode; return RTERROR; }

      if (pPrj->getSinthClassList(SinthClassList) == GS_BAD) return RTERROR;

      if (!(pSinthClass = (C_SINTH_CLASS *) SinthClassList.search_key(Cls))) return RTERROR;
      if (pSinthClass->get_category() == CAT_EXTERN)
         pSinthClass = (C_SINTH_CLASS *) pSinthClass->ptr_sub_list()->get_head();
      
      if (gsc_strlen(pSinthClass->get_dir_dwg()) == 0) return RTNORM;
      if (DwgList.load(Prj, Cls, pSinthClass->get_dir_dwg()) == GS_BAD)
         return RTERROR;
   }

   if (DwgList.get_count() == 0) return RTNORM;

   ret << DwgList.to_rb();
   ret.LspRetList();

   return RTNORM;
}