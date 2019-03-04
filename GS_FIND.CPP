/**********************************************************
Name: 

Module description: Funzioni di filtro per estrazione oggetti
                    e altre funzioni varie
            
Author:  Roberto Poltini & Paolo Carlio

(c) Copyright 1995-2015 by IREN ACQUA GAS S.p.A.

              
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
#include <math.h>
#include <ctype.h>       /*  per isdigit() */
#include <fcntl.h>
#include <string.h>      /*  per strcat() strcmp()  */

#include "rxdefs.h"   
#include "adslib.h"   
#include <adeads.h>
#include "adsdlg.h"   

#include "GSresource.h"

#include "..\gs_def.h" // definizioni globali
#include "gs_error.h"     // codici errori

#include "gs_resbf.h"     // gestione resbuf
#include "gs_utily.h" 
#include "gs_list.h"      // gestione liste C++ 
#include "gs_ase.h" 
#include "gs_dbref.h"
#include "gs_class.h" 
#include "gs_lisp.h"
#include "gs_init.h" 
#include "gs_area.h" 
#include "gs_query.h" 
#include "gs_thm.h"       // gestione tematismi e sistemi di coordinate
#include "gs_filtr.h" 
#include "gs_evid.h" 
#include "gs_find.h" 
#include "gs_ade.h" 

#include "d2hMap.h" // doc to help


/*************************************************************************/
/* STRUTTURE PER IL PASSAGGIO VALORI FRA DCL                             */                                    
/*************************************************************************/

struct Common_Dcl_main_Sql_Addr_Struct
{
   int             all_sel;
   int             all_sel_der;
   int             sql_punt;
   int             sql_ind;
   int             num_cls;
   int             n_pag;
   int             cat;
   int             tp_like;
   int             query_flag;
   TCHAR           sql_statement[MAX_LEN_SQL_STM];
   TCHAR           sql_field[MAX_LEN_SQL_STM];
   TCHAR           sql_order[MAX_LEN_SQL_STM];
   TCHAR           sql_via_text[MAX_LEN_SQL_STM];
   TCHAR           sql_civ_text[MAX_LEN_SQL_STM];
   C_STRING        sql_via_cod;
   C_STRING        sql_civ_cod;
   C_CLASS         *filter_cls;
   C_CLASS         *sql_cls;
   C_CLASS         *sql_cplx;
   C_ATTRIB_LIST   *attr_list;
   C_2LONG_STR_LIST list_class_to_visual;
   C_INT_INT_STR_LIST sql_list;
   C_STR_LIST       cls_name; 
   C_STR_LIST       list_field_sql;
   C_STR_LIST       list_field_sql_addr;
   C_RB_LIST        last_point;
   C_RB_LIST        sql_risult;
   C_RB_LIST        list_field;
   C_RB_LIST        n_classi;
   C_RB_LIST        ris_sql_civ;
};

/*************************************************************************/
/* PRIVATE FUNCTIONS                                                     */
/*************************************************************************/
int gsc_filter_fas(C_CLASS *pCls, int md);
int gsc_filter_sql(C_CLASS *pCls, C_INT_INT_STR_LIST *sql_list, int tipo,
                   Common_Dcl_main_Sql_Addr_Struct *CommonStruct);
TCHAR *gsc_sql_bldqry(C_CLASS *pCls, TCHAR *in, int mod);
TCHAR *gsc_adr_bldqry(Common_Dcl_main_Sql_Addr_Struct *CommonStruct,
                      C_CLASS *pCls, TCHAR *in, TCHAR *via, TCHAR *civ);
int gsc_find_addr(Common_Dcl_main_Sql_Addr_Struct *CommonStruct);
int gsc_setGraphSettingsToCurrentADEQry(long flag_set, C_CLASS *pCls, C_FAS &FAS);
int gsc_get_oldusrfas(C_CLASS *pCls, C_FAS_LIST *fasList, C_FAS &FAS);

int gsc_impost_estrqry(int cls, int sub, int mod);
int gsc_find_elements(C_CLASS *pCls, C_STR_LIST *fields, TCHAR *condsql, TCHAR *condorder,
							 Common_Dcl_main_Sql_Addr_Struct *CommonStruct, C_RB_LIST &result_list);
int gsc_lista_val(C_RB_LIST *lista, int pos, int passo,
                  Common_Dcl_main_Sql_Addr_Struct *CommonStruct);
int gsc_lista_civici(C_RB_LIST *lista, int pos, ads_callback_packet *dcl,
                     Common_Dcl_main_Sql_Addr_Struct *CommonStruct);
int gsc_do_sql_addr(int cls, int sub, TCHAR *condition);
int gsc_extract_coord(C_RB_LIST *lista, int all_sel_der);
int gsc_calc_min_dist(C_RB_LIST lista_coord[2], ads_point p1, ads_point p2);
int gsc_calc_pnt_min_max(C_RB_LIST lista_coord[2], ads_point p1, ads_point p2);
int gsc_impost_win(ads_point p1, ads_point p2, int clsp, int clsd);
int gsc_zoom_win(TCHAR *filep, ads_point p1, ads_point p2);
int gsc_lista_classi(int prj, C_SINTH_CLASS_LIST *list_class,
                     Common_Dcl_main_Sql_Addr_Struct *CommonStruct);
int gsc_string_arrange(int *pos, int cntf, C_STRING *strng, C_RB_LIST *lista_val, 
                       int spost);
int gsc_string_confirm(C_STRING *strng, C_RB_LIST *lista_val, 
                       int posi = GS_BAD , int passo = GS_BAD);
int gsc_find_cls_env(int prj, Common_Dcl_main_Sql_Addr_Struct *CommonStruct);
int gsc_visual_list(ads_hdlg dcl_id, TCHAR *namelist, C_2LONG_STR_LIST &ListClassToVisual);


///////////////////////////////////////////////////////////////////////////
/* 
Questa funzione imposta una condizione di SQL su una classe del progetto al
fine di estrarre dal disegno gli oggetti che la soddisfano. E' invocata dal
LISP con parametri di passaggio <cod.prj> <cod.cls>.
Restituisce una lista ad autocad nel seguente formato
((<cls><sql><sub>)...)
*/                                                    
///////////////////////////////////////////////////////////////////////////
int gs_filter_sql(void)
{
   int                prj, cls, sub;
   C_INT_INT_STR_LIST sql_list;
   C_CLASS            *pCls;
   C_STRING           SQL;
   presbuf            arg;
   C_RB_LIST          ret;
   Common_Dcl_main_Sql_Addr_Struct CommonStruct;

   acedRetNil();

   // Legge nella lista dei parametri: progetto classe e sub //
   arg = acedGetArgs();
   if (arg == NULL) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_BAD) return RTERROR;

   // Ritorna il puntatore alla classe cercata //
   if ((pCls = gsc_find_class(prj, cls, sub)) == NULL) return RTERROR;

   // Compongo l'istruzione SQL
   CommonStruct.sql_cls = pCls;
   if (gsc_filter_sql(pCls, &sql_list, 1, &CommonStruct) == GS_BAD) return RTERROR;

	// Verifico se esiste una condizione di SQL non nulla
	if (sql_list.get_head() && gsc_strlen(sql_list.get_head()->get_name()) > 0)
   {
      SQL = sql_list.get_head()->get_name();

      // Salvo la condizione SQL
      if (gsc_ClsSQLQrySave(cls, 0, SQL) == GS_BAD) return RTERROR;

      ret << sql_list.to_rb();
      ret.LspRetList();
	}
   // verifico se esiste già una query di SQL per la classe corrente e la cancello
   else 
      if (gsc_ClsSQLQryDel(cls, 0) == GS_BAD) return RTERROR;
   
   return RTNORM;
}

///////////////////////////////////////////////////////////////////////////
/* 
Questa funzione permette di variare le caratteristiche grafiche di una
classe al momento della selezione della stessa prima che venga estratta.
E' invocata dal LISP con parametri di passaggio (modo prj cls [sub])
*/                                                    
///////////////////////////////////////////////////////////////////////////
int gs_filter_fas(void)
{
   int     ris, prj, cls, sub, modo = 0;
   C_CLASS *pCls;
   resbuf  *arg;

   acedRetNil();

   arg = acedGetArgs();
   if (arg == NULL || arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   else modo = arg->resval.rint;
   arg = arg->rbnext;
   if (arg == NULL) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub)==GS_BAD) return RTERROR;

   // Ritorna il puntatore alla classe cercata
   if ((pCls = gsc_find_class(prj, cls, sub))==NULL) return RTERROR;

   // Compongo la condizione FAS
   ris = gsc_filter_fas(pCls, modo);

   if (ris == GS_CAN) { acedRetNil(); return RTNORM; }
   if (ris != GS_GOOD) return RTERROR;

   acedRetT();
     
   return RTNORM;
}


///////////////////////////////////////////////////////////////////////////
/* 
Questa funzione imposta la defintiva query per ogni classe del progetto selezionata
al fine di estrarre dal disegno gli oggetti che la soddisfano. E' invocata dal
LISP con parametri di passaggio <tipo_fas> <cod.prj> <cod.cls>.
*/                                                    
///////////////////////////////////////////////////////////////////////////
int gs_impost_estrqry(void)
{
   int     ris, prj, cls, sub, modo = 0;
   C_CLASS *pCls;
   resbuf  *arg;

   acedRetNil();

   arg = acedGetArgs();
   if (arg == NULL || arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   else modo = arg->resval.rint;
   arg = arg->rbnext;
   if (arg == NULL) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (arg_to_prj_cls_sub(&arg,&prj,&cls,&sub)==GS_BAD) return RTERROR;

   // Ritorna il puntatore alla classe cercata //
   if ((pCls = gsc_find_class(prj, cls, sub))==NULL) return RTERROR;

   // Compongo la condizione di estrazione 
   if ((ris = gsc_impost_estrqry(cls, sub, modo))!=GS_GOOD) return RTERROR;

   acedRetT();
     
   return RTNORM;
}


/*********************************************************************/
/*.doc gsc_impost_estrqry <internal> */
/*+
  Questa funzione riceve num_class,sub_class,modalità e imposta la query
  definitiva di estrazione FAS-SQL per ogni classe selezionata.
  Parametri:  
  int num_class : codice class
  int sub_class : codice sottoclasse
  int modo      : modalità di impostazione sulla classe
                0 = FAS del disegno
                1 = FAS dell'utente
                2 = FAS di default
                3 = Condizione di SQL 
  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_impost_estrqry(int num_class, int sub_class, int md)
{
   C_RB_LIST list_cat;
   TCHAR     qry_cat[] = _T("fasqlestr"), *str=NULL;
   C_STRING  query_name;
	C_CLASS   *pCls;

	// ricavo il puntatore alla classe
   pCls  = gsc_find_class(GS_CURRENT_WRK_SESSION->get_PrjId(), num_class, sub_class);

   switch (md)
   {
      case 0:
         query_name = _T("fasw");
         break;
      case 1:
         query_name = _T("fasu");
         break;
      case 2:
         query_name = _T("fasd");
         break;
      case 3:
         query_name = _T("sqlu");
         break;
   }         
   
   query_name += num_class;
   query_name += _T("-");
   query_name += sub_class;
   
   if ((list_cat << ade_qllistctgy()) != NULL)
      gsc_find_qry(pCls, list_cat.get_head(), qry_cat, query_name.get_name(), 1);

   return GS_GOOD;
}


/*********************************************************************/
/*.doc gsc_filter_fas <external> */
/*+
  Questa funzione permette di variare le caratteristiche grafiche di una
  classe al momento della selezione della stessa prima che venga estratta.
  Parametri:
  C_CLASS *pCls;  puntatore a classe
  int md: flag sulle modalità della FAS; i valori di md sono:
               2 = fas di default della classe;
               1 = fas definita dall'utente;
               0 = fas del disegno;
              -1 = fas del disegno della classe senza attivazione della dcl.

  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_filter_fas(C_CLASS *pCls, int md)
{
	int      result;
	int      flag, categ, tipo;
   long     flag_set = GSNoneSetting;
	C_ID     *p_id;
	resbuf   *list_cat;
	TCHAR    qry_cat[] = _T("fasqlestr");
	C_STRING query_name, path;
   C_FAS    FAS;
   double   dummyDbl = 0;
   int      dummyInt = ALL;
  
	// verifico se esiste una query spaziale attualmente in memoria e la memorizzo
	if (gsc_ExistCurrentAdeQry() == GS_GOOD) 
	{
		gsc_ade_qldelquery(ADE_SPATIAL_QRY_NAME);
		if (gsc_save_qry() == GS_BAD) 
			return GS_BAD; 
	}

	switch (md)
	{
		case 0: 
		case -1:
			query_name = _T("fasw");
			break;
		case 1:
			query_name = _T("fasu");
			break;
		case 2:
			query_name = _T("fasd");
			break;
	}      
   
	query_name += pCls->ptr_id()->code;
	query_name += _T("-");
	query_name += pCls->ptr_id()->sub_code;

	p_id  = pCls->ptr_id();
	categ = p_id->category;
	tipo  = p_id->type;

	if (md <= 0)
	{	// FAS disegno (DWG) (pulisco tutto ed esco)
		if (ade_altpclear() != RTNORM) return GS_BAD;
		if (ade_qryclear() != RTNORM) return GS_BAD;
		gsc_save_qry(qry_cat, query_name.get_name());
		return GS_GOOD; 
	} 

	// carico la fas di default
	if (md > 0)
      pCls->ptr_fas()->copy(&FAS);

   // se modo = 2 (caratteristiche di default)
   if (md == 2)
   {
      // leggo quali caratteristiche grafiche si possono modificare
      flag_set = pCls->what_is_graph_updateable();

      // imposto la classe a NULL per evitare che si possano modificare
      // le opzioni di evidenziazione
      result = gsc_ddChooseGraphSettings(NULL, FALSE, &flag_set, FAS, 
                                         &dummyDbl, &dummyInt, FALSE);

      // rileggo quali caratteristiche grafiche si possono modificare
      flag_set = pCls->what_is_graph_updateable();
   }
   else
   {
	   // per scelta user vedo se esiste già una query impostata
	   if (md == 1) // fas definita dall'utente al momento
	   {
		   if ((list_cat = ade_qllistctgy()) != NULL)
		   {
            // Carico la query
			   flag = gsc_find_qry(pCls, list_cat, qry_cat, query_name.get_name(), 2); 
			   if (flag == 1)
				   // recupero i nuovi valori impostati        
				   if (gsc_getGraphSettingsFromCurrentADEQry(FAS, &flag_set) == NULL) 
                  return GS_BAD;
		   }
	   }

      result = gsc_ddChooseGraphSettings(pCls, FALSE, &flag_set, FAS, 
                                         &dummyDbl, &dummyInt, FALSE);
   }   

   if (result == GS_GOOD)
	{ 
		result = gsc_setGraphSettingsToCurrentADEQry(flag_set, pCls, FAS);
		gsc_save_qry(qry_cat, query_name.get_name());
	}

	return result;
}


/***************************************************************************/
/*.doc gsc_get_oldusrfas <external> */
/*+
	Questa funzione recupera la fas di default di una classe.
	Parametri:
	C_CLASS	  *pCls		: puntatore a classe;
	C_FAS_LIST *fasList	: puntatore alla lista fas della classe;
	C_FAS      &FAS;     : caratteristiche grafiche delle classe (out);

  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/***************************************************************************/
int gsc_get_oldusrfas(C_CLASS *pCls, C_FAS_LIST *fasList, C_FAS &FAS)
{
	C_CLASS  *mother_cls = NULL;
	C_STRING name_layer;

	if (!pCls) return GS_BAD;

	if (!(pCls->ptr_fas())) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

	if (fasList->get_count() == 0) // fasList vuota: ricavo la fas di default  
	{	// inserisco la fas della classe nella lista per non perdere dati successivamente
      C_FAS *p_FAS = new C_FAS();
	
		pCls->ptr_fas()->copy(p_FAS);
		fasList->add_tail(p_FAS);
	}

	((C_FAS *) fasList->getptr_at(1))->copy(&FAS);
   
	// se la classe che sto trattando è una sottoclasse ricavo il puntatore alla classe madre
	if (pCls->ptr_id()->sub_code != 0)
		if ((mother_cls = gsc_find_class(((C_PROJECT *) pCls->ptr_id()->pPrj)->get_key(),
			 pCls->ptr_id()->code)) == NULL) return GS_BAD;
	   
   // se sto usando la funzione durante la copia delle classi inizializzo il nome
   // del piano della sottoclasse come 'nome_classe + nome_sottoclasse'

	if (mother_cls != NULL) 
	{
		name_layer.clear();
		name_layer += mother_cls->get_name();
		if (mother_cls->get_category() == CAT_EXTERN)
		{
			name_layer += _T("_");
			name_layer += pCls->get_name();
		}
      gsc_strcpy(FAS.layer, name_layer.get_name(), MAX_LEN_LAYERNAME);
	}

	return GS_GOOD;
}


///////////////////////////////////////////////
// ACTION TILE : click su slider per address //
///////////////////////////////////////////////
static void CALLB dcl_address_slidqry(ads_callback_packet *dcl)
{
   ads_hdlg       hdlg = dcl->dialog;
   C_STR          *node1;
   C_INT_INT_STR  *node2;
   TCHAR          str[MAX_LEN_SQL_STM + MAX_LEN_CLASSNAME + 10], val[30];
   TCHAR          Buffer[MAX_LEN_SQL_STM + MAX_LEN_CLASSNAME + 10];
   size_t         len = MAX_LEN_SQL_STM + MAX_LEN_CLASSNAME + 10, offset;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);

   ads_get_tile(hdlg, _T("slidqry"), val, 30);
   offset = _wtoi(val);

   ads_start_list(hdlg, _T("listqry"), LIST_NEW, 0);
   node1 = (C_STR*)CommonStruct->cls_name.get_head();
   node2 = (C_INT_INT_STR*)CommonStruct->sql_list.get_head();
   while(node1 != NULL && node2 != NULL)
   {
   	if (len > node1->len() + node2->len())
      {
   		swprintf(str, MAX_LEN_SQL_STM + MAX_LEN_CLASSNAME + 10, _T("%s\t:\t%s"),
                  node1->get_name(), node2->get_name());

         if (((int) wcslen(str)) > offset)
            wcscpy(Buffer, str + offset);
         else
            wcscpy(Buffer, GS_EMPTYSTR);
         
         gsc_add_list(Buffer);
      }
      node1 = (C_STR*)node1->get_next();
      node2 = (C_INT_INT_STR*)node2->get_next();
   }
   ads_end_list();

   swprintf(val, 30, _T("%d"), CommonStruct->sql_punt);
   ads_set_tile(hdlg, _T("listqry"), val);
}
///////////////////////////////////////////////
// ACTION TILE : click su slider lista campi //
///////////////////////////////////////////////
static void CALLB dcl_filter_slidfield(ads_callback_packet *dcl)
{
   ads_hdlg hdlg = dcl->dialog;
   int      i, k, cnte, cnti, passo, pos;
   TCHAR    val[30];
   C_STRING stringa;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   pos = _wtoi(dcl->value);
   cnte = CommonStruct->list_field.GetCount() - 2;
   cnti = CommonStruct->list_field_sql.get_count();
   passo = (cnti * 4) + 2;
   i = CommonStruct->n_pag * 50 * passo + 5;
   
   // costruisco la lista di valori shiftati e riempio la list box
   ads_start_list(hdlg, _T("liststreet"), LIST_NEW, 0);
   CommonStruct->list_field.getptr_at(i);

   do 
	{
      if (gsc_string_arrange(&i, cnti, &stringa, &CommonStruct->list_field, pos) == GS_BAD) return;
      gsc_add_list(stringa);
      for (k = 0; k < 6; k++) { CommonStruct->list_field.get_next(); i++; }
   }
	while (i < (CommonStruct->n_pag + 1) * 50 * passo + 5 && i <= cnte); 

   ads_end_list();
   swprintf(val, 30, _T("%d"), 0);
   ads_set_tile(hdlg, _T("liststreet"), val);
   ads_mode_tile(hdlg, _T("liststreet"), MODE_SETFOCUS);

}
///////////////////////////////////////////////
// ACTION TILE : click su pagina su          //
///////////////////////////////////////////////
static void CALLB dcl_filter_up(ads_callback_packet *dcl)
{
   ads_hdlg hdlg = dcl->dialog;
   int      k, i, cnte, cnti, passo;
   TCHAR    val[30];
   C_STRING stringa;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   cnte  = CommonStruct->list_field.GetCount()-2;
   cnti  = CommonStruct->list_field_sql.get_count();
   passo = (cnti * 4) + 2;

   if (CommonStruct->n_pag > 0) CommonStruct->n_pag--;
   i = CommonStruct->n_pag * 50 * passo + 5;

   // costruisco la lista di valori shiftati indietro di 50 riempio la list box
   ads_start_list(hdlg, _T("liststreet"), LIST_NEW, 0);
   CommonStruct->list_field.getptr_at(i);

   do
	{
      if (gsc_string_arrange(&i, cnti, &stringa, &CommonStruct->list_field, 0) == GS_BAD) return;
      gsc_add_list(stringa);
      for (k = 0; k < 6; k++) { CommonStruct->list_field.get_next(); i++; }
   }
	while (i < (CommonStruct->n_pag + 1) * 50 * passo + 5 && i <= cnte); 

   ads_end_list();
   swprintf(val, 30, _T("%d"), 0);
   ads_set_tile(hdlg, _T("liststreet"), val);
   ads_mode_tile(hdlg, _T("liststreet"), MODE_SETFOCUS);

}
///////////////////////////////////////////////
// ACTION TILE : click su pagina giù         //
///////////////////////////////////////////////
static void CALLB dcl_filter_down(ads_callback_packet *dcl)
{
   ads_hdlg       hdlg = dcl->dialog;
   int            i, k, cnte, cnti, passo, npag;
   TCHAR          val[30];
   C_STRING       stringa;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   cnte  = CommonStruct->list_field.GetCount() - 2;
   cnti  = CommonStruct->list_field_sql.get_count();
   passo = (cnti * 4) + 2;
   npag  = (int) floor((double) (cnte / (50 * passo)));
   
   // costruisco la lista di valori shiftati avanti di 50 e riempio la list box
   ads_start_list(hdlg, _T("liststreet"), LIST_NEW, 0);
   if (npag == 1) i = 5;
   else
   {
      if (CommonStruct->n_pag < npag) CommonStruct->n_pag++;
      i = CommonStruct->n_pag * 50 * passo + 5;
   }

   CommonStruct->list_field.getptr_at(i);

   do 
	{
      if (gsc_string_arrange(&i, cnti, &stringa, &CommonStruct->list_field, 0) == GS_BAD) return;
      gsc_add_list(stringa);
      for (k = 0; k < 6; k++) { CommonStruct->list_field.get_next(); i++; }
   }
	while (i < (CommonStruct->n_pag + 1) * 50 * passo + 5 && i <= cnte); 

   ads_end_list();
   swprintf(val, 30, _T("%d"), 0);
   ads_set_tile(hdlg, _T("liststreet"), val);
   ads_mode_tile(hdlg, _T("liststreet"), MODE_SETFOCUS);

}   
///////////////////////////////////////////////
// ACTION TILE : click su fine lista         //
///////////////////////////////////////////////
static void CALLB dcl_filter_end(ads_callback_packet *dcl)
{
   ads_hdlg       hdlg = dcl->dialog;
   int            i, k, cnte, cnti, passo, npag;
   TCHAR          val[30];
   C_STRING       stringa;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   cnte  = CommonStruct->list_field.GetCount()-2;
   cnti  = CommonStruct->list_field_sql.get_count();
   passo = (cnti * 4) + 2;
   npag  = (int) floor((double) (cnte / (50 * passo)));
   
   // costruisco la lista degli ultimi 50 valori e riempio la list box
   ads_start_list(hdlg, _T("liststreet"), LIST_NEW, 0);

   // controllo se la lista ha meno di 50 elementi 
   if (npag == 1) CommonStruct->n_pag = 0;
   else CommonStruct->n_pag = npag;
   i = CommonStruct->n_pag * 50 * passo + 5;

   CommonStruct->list_field.getptr_at(i);

   do
	{
      if (gsc_string_arrange(&i, cnti, &stringa, &CommonStruct->list_field, 0) == GS_BAD) return;
      gsc_add_list(stringa);
      for (k = 0; k < 6; k++) { CommonStruct->list_field.get_next(); i++; }
   }
	while (i < (CommonStruct->n_pag + 1) * 50 * passo + 5 && i <= cnte); 


   ads_end_list();
   swprintf(val, 30, _T("%d"), 0);
   ads_set_tile(hdlg, _T("liststreet"), val);
   ads_mode_tile(hdlg, _T("liststreet"), MODE_SETFOCUS);

}
///////////////////////////////////////////////
// ACTION TILE : click su selezione globale  //
///////////////////////////////////////////////
static void CALLB dcl_filter_allsel(ads_callback_packet *dcl)
{   
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;
   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   CommonStruct->all_sel = 1;
}
///////////////////////////////////////////////
// ACTION TILE : click su seleziona singola  //
///////////////////////////////////////////////
static void CALLB dcl_filter_multsel(ads_callback_packet *dcl)
{
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;
   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   CommonStruct->all_sel = 0;
}
/////////////////////////////////////////////////////////////////////
// ACTION TILE : click su selezione globale per la classe derivata //
/////////////////////////////////////////////////////////////////////
static void CALLB dcl_filter_globsel(ads_callback_packet *dcl)
{
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;
   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   CommonStruct->all_sel_der = 1;
}
/////////////////////////////////////////////////////////////////////
// ACTION TILE : click su selezione singola per la classe derivata //
/////////////////////////////////////////////////////////////////////
static void CALLB dcl_filter_singsel(ads_callback_packet *dcl)
{
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;
   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   CommonStruct->all_sel_der = 0;
}
///////////////////////////////////////////////
// ACTION TILE : click su inizio lista       //
///////////////////////////////////////////////
static void CALLB dcl_filter_start(ads_callback_packet *dcl)
{
   ads_hdlg       hdlg = dcl->dialog;
   int            i, k, cnte, cnti, passo;
   TCHAR          val[30];
   C_STRING       stringa;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   cnte = CommonStruct->list_field.GetCount() - 2;
   cnti = CommonStruct->list_field_sql.get_count();
   passo = (cnti * 4) + 2;
   
   // costruisco la lista di valori shiftati avanti di 50 e riempio la list box
   ads_start_list(hdlg, _T("liststreet"), LIST_NEW, 0);
   CommonStruct->n_pag = 0;
   i = 5;
   CommonStruct->list_field.getptr_at(i);

   do
	{
      if (gsc_string_arrange(&i, cnti, &stringa, &CommonStruct->list_field, 0) == GS_BAD) return;
      gsc_add_list(stringa);
      for (k = 0; k < 6; k++) { CommonStruct->list_field.get_next(); i++; }

   }
	while (i < 5 + 50 * passo && i <= cnte); 

   ads_end_list();
   swprintf(val, 30, _T("%d"), 0);
   ads_set_tile(hdlg, _T("liststreet"), val);
   ads_mode_tile(hdlg, _T("liststreet"), MODE_SETFOCUS);
}   
//////////////////////////////////////////////////////////
// ACTION TILE : click su lista attributi per indirizzo //
//////////////////////////////////////////////////////////
static void CALLB dcl_address_listqry(ads_callback_packet *dcl)
{
   ads_hdlg       hdlg = dcl->dialog;
   TCHAR          *str, val[30];
   TCHAR          ind[MAX_LEN_CLASSNAME] = GS_EMPTYSTR, vie[MAX_LEN_CLASSNAME] = GS_EMPTYSTR;
   TCHAR          civ[MAX_LEN_CLASSNAME]= GS_EMPTYSTR;
   C_STRING       filename;
   C_CLASS_LIST   *clslist;
   C_INT_STR_LIST lista_classe; 
   C_CLASS        *cls;                  
   C_STR          *punt1, *nod;
   C_NODE         *punt, *cpunt;
   C_INT_INT_STR  *punt2, *punt3;
   FILE           *f;
   ads_callback_packet temp;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   CommonStruct->sql_ind = 0;
   CommonStruct->sql_punt = _wtoi(dcl->value);
   if (dcl->reason == CBR_DOUBLE_CLICK)
   {
      punt1 = (C_STR*)CommonStruct->cls_name.getptr_at(CommonStruct->sql_punt+1);
      punt2 = (C_INT_INT_STR*)CommonStruct->sql_list.getptr_at(CommonStruct->sql_punt+1);
      clslist = GS_CURRENT_WRK_SESSION->get_pPrj()->ptr_classlist();
      CommonStruct->num_cls = punt2->get_key();     // righe da verificare

      punt3 = (C_INT_INT_STR*)CommonStruct->sql_list.get_head();
      punt3 = (C_INT_INT_STR*)punt3->get_next();
      while(punt3 != NULL)
      {
	      if (punt3->get_key() != CommonStruct->num_cls)
	      {
            CommonStruct->num_cls = punt3->get_key();
            break;
         }
         punt3 = (C_INT_INT_STR*)punt3->get_next();
      }

      if ((cls = (C_CLASS*)clslist->search_name(punt1->get_name())) == NULL) 
      {
         gsc_toupper(punt1->get_name());
         if ((cls = (C_CLASS*)clslist->search_name(punt1->get_name())) == NULL) 
            return;  // $$$ aggiungere segnalazione di errore
      }
      // verifico se la classe scelta è del tipo 'indirizzi' da trattarsi 
      // in modo particolare
      filename = GS_CURRENT_WRK_SESSION->get_pPrj()->get_dir();
      filename += _T("\\ADDRESS.TXT");
      
      // verifico se esiste il file address.txt che contiene tutte le terne di gruppi 
      // di tipo 'indirizzo'
      if ((gsc_path_exist(filename)) == GS_GOOD)
      {
         bool     Unicode;
         C_STRING Row;

         if ((f = gsc_fopen(filename, _T("r"), MORETESTS, &Unicode)) == NULL) return;

         // costruisco la lista delle classi elencate nel file
			while (gsc_readline(f, Row, Unicode) != WEOF)
         {  
            if ((nod = new C_INT_STR) == NULL) { GS_ERR_COD=eGSOutOfMem; return; }
            lista_classe.add_tail(nod);
            if ((cpunt = (C_NODE*) clslist->getptr_at(clslist->getpos_name(Row.get_name(), FALSE))) != NULL)
               nod->set_key(cpunt->get_key());
            else
               nod->set_key(0);

            Row.toupper();
            if (nod->set_name(Row.get_name()) == GS_BAD) return;
         }
         gsc_fclose(f);

         // cerco nella lista la terna dei valori corrispondenti
         punt = lista_classe.get_head();
         wcscpy(ind, CommonStruct->sql_cplx->get_name());
         gsc_toupper(ind);
         while (punt)
         {
            if ((gsc_strcmp(punt->get_name(), ind)) == 0)
            {  // ho trovato il gruppo indirizzo che cercavo
               if((punt = punt->get_next()) == NULL) return;  // errore da segnalare
               gsc_toupper(cls->get_name());
               if ((gsc_strcmp(punt->get_name(), cls->get_name())) != 0) return; // errore da segn.
               wcscpy(vie, cls->get_name());
               if((punt = punt->get_next()) == NULL) return; // errore da segnalare
               wcscpy(civ, punt->get_name());
               CommonStruct->num_cls = punt->get_key();
               CommonStruct->sql_ind = 1;
               break;
            }
            punt = punt->get_next();
         }             
      }

      TCHAR dummy[MAX_LEN_SQL_STM];
      wcscpy(dummy, CommonStruct->sql_statement);
      if ((str = gsc_adr_bldqry(CommonStruct, cls, dummy, vie, civ)) != NULL) 
      {
         if ((CommonStruct->sql_ind != 1) && (CommonStruct->cat != CAT_GROUP))
         { punt2->set_name(str); free(str); }
      }
      else
         if (gsc_strcmp(cls->get_name(), civ) == 0) 
            ads_set_tile(hdlg, _T("error"), gsc_msg(366)); //  "Impostare prima la Via ..."
      swprintf(val, 30, _T("%d"), 1);
      temp.value = val;
      temp.dialog = hdlg;
      temp.client_data = CommonStruct;
      ads_set_tile(hdlg, _T("slidqry"), _T("1"));
      dcl_address_slidqry(&temp);
   }
}
///////////////////////////////////////
// ACTION TILE : click su lista vie  //
///////////////////////////////////////
static void CALLB dcl_address_liststreet(ads_callback_packet *dcl)
{
   int            cnte, cnti, pos, posi, posiz, passo, poscod, postxt, rispdef = 1, ris;
	long           codice;
   ads_hdlg       hdlg = dcl->dialog;
   C_STRING       prompt, stringa;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   posiz = _wtoi(dcl->value);

   if ((dcl->reason == CBR_DOUBLE_CLICK) && (CommonStruct->all_sel == 0))
   {
      // cerco il campo chiave della classe
      C_INFO  *pInfo = CommonStruct->sql_cls->ptr_info();

      cnti = CommonStruct->list_field_sql.get_count();
      cnte = CommonStruct->list_field.GetCount() - 2;
      passo =  (cnti * 4) + 2,
      poscod = (CommonStruct->list_field_sql.getpos_name(pInfo->key_attrib.get_name())-1) * 4;

      posi = pos = posiz * passo + CommonStruct->n_pag * 50 * passo + 5;
      
      if (CommonStruct->sql_ind == 1)  // la classe è indirizzo
      {
         postxt = (CommonStruct->list_field_sql.getpos_name(_T("TEXT")) - 1) * 4;
         prompt += gsc_msg(358);              // "Confermare la scelta"
         prompt += _T(' ');
         prompt += gsc_rtrim(CommonStruct->sql_risult.getptr_at(pos + postxt)->resval.rstring);  
         if (gsc_ddgetconfirm(prompt.get_name(), &ris, rispdef) == GS_BAD) return;
      }
      else  // è classe diversa da indirizzo
      // compongo la stringa con i valori di tutti i campi selezionati
      {
 
	      if (gsc_string_confirm(&stringa, &CommonStruct->list_field, 
                                posi - 1, passo) == GS_BAD) return;           
         prompt += gsc_msg(358);              // "Confermare la scelta"
         prompt += _T('\n');
         prompt += gsc_rtrim(stringa.get_name());  
         if (gsc_ddgetconfirm(prompt.get_name(), &ris, rispdef,
				                  GS_BAD, GS_BAD, GS_GOOD) == GS_BAD) return;
      }
      if (ris) 
      {  
         C_INT_INT_STR *node;

         CommonStruct->sql_via_cod.clear();
         CommonStruct->sql_via_cod += pInfo->key_attrib.get_name();
         CommonStruct->sql_via_cod += _T(" = ");
         CommonStruct->sql_via_cod += CommonStruct->sql_risult.getptr_at(pos + poscod);

         if (CommonStruct->sql_ind == 1)  // la classe gruppo è un indirizzo
         {
            wcscpy(CommonStruct->sql_via_text,
                   CommonStruct->sql_risult.getptr_at(pos + postxt)->resval.rstring);
            node=(C_INT_INT_STR*)CommonStruct->sql_list.get_head();
            while(node != NULL)
            {
      	      if (node->get_key() == CommonStruct->sql_cls->get_key())
      	      {
                  node->set_name(CommonStruct->sql_via_text);
                  break;
               }
               node=(C_INT_INT_STR*)node->get_next();
            }
         }
         else    // la classe è gruppo, ma non indirizzo
         {
            node = (C_INT_INT_STR*)CommonStruct->sql_list.get_head();
            while(node != NULL)
            {
      	      if (node->get_key() == CommonStruct->sql_cls->get_key())
      	      {
                  node->set_name(CommonStruct->sql_via_cod.get_name());
                  break;
               }
               node = (C_INT_INT_STR*)node->get_next();
            }
         }
         codice = CommonStruct->sql_risult.getptr_at(pos + poscod)->resval.rlong;
         CommonStruct->sql_risult << acutBuildList(RTLB, RTLB, RTLB,
                                                   RTSTR, pInfo->key_attrib.get_name(),
                                                   RTLONG, codice,
                                                   RTLE, RTLE, RTLE, 0); 
         ads_done_dialog(hdlg, DLGOK);
      }
      else ads_done_dialog(hdlg, DLGCANCEL);
   }
}
///////////////////////////////////////////////////////////////////
// ACTION TILE : click su tasto OK per accettazione via da lista //
///////////////////////////////////////////////////////////////////
static void CALLB dcl_liststreet_accept_ok(ads_callback_packet *dcl)
{
   int      posi, postxt, poscod, posiz, passo, cnti, pos, ris, rispdef = 1;
	long     codice;
   ads_hdlg hdlg = dcl->dialog;
   TCHAR    valore[10];
   C_STRING prompt, stringa;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   if (ads_get_tile(hdlg, _T("liststreet"), valore, 10) != RTNORM) return;
   else posiz = _wtoi(valore);

   // cerco il campo chiave della classe 
   C_INFO   *pInfo = CommonStruct->sql_cls->ptr_info();

   cnti = CommonStruct->list_field_sql.get_count();
   passo =  (cnti * 4) + 2;
   poscod = (CommonStruct->list_field_sql.getpos_name(pInfo->key_attrib.get_name()) - 1) * 4;

   posi = pos = posiz * passo + CommonStruct->n_pag * 50 * passo + 5;
   
   if (CommonStruct->all_sel == 0)  //  ho fatto una scelta univoca (Singola)
   {
      if (CommonStruct->sql_ind == 1) // è classe indirizzo: chiedo conferma della via
      {
         postxt = (CommonStruct->list_field_sql.getpos_name("TEXT") - 1) * 4; 
         prompt += gsc_msg(358);              // "Confermare la scelta"
         prompt += _T(' ');
         prompt += gsc_rtrim(CommonStruct->sql_risult.getptr_at(pos + postxt)->resval.rstring);  
         if (gsc_ddgetconfirm(prompt.get_name(), &ris, rispdef) == GS_BAD) return;
      }
      else  // è classe diversa da indirizzo
      // compongo la stringa con i valori di tutti i campi selezionati
      {
         if (gsc_string_confirm(&stringa, &CommonStruct->list_field,
                                posi - 1, passo) == GS_BAD) return;             
         prompt += gsc_msg(358);              // "Confermare la scelta"
         prompt += _T('\n');
         prompt += gsc_rtrim(stringa.get_name());  
         if (gsc_ddgetconfirm(prompt.get_name(), &ris, rispdef,
				                  GS_BAD, GS_BAD, GS_GOOD) == GS_BAD) return;
      }
      if (ris) 
      {  
         C_INT_INT_STR *node;

         CommonStruct->sql_via_cod.clear();
         CommonStruct->sql_via_cod += pInfo->key_attrib.get_name();
         CommonStruct->sql_via_cod += _T(" = ");
         CommonStruct->sql_via_cod += CommonStruct->sql_risult.getptr_at(pos + poscod);

         if (CommonStruct->sql_ind == 1)  // la classe è gruppo - indirizzo
         {
            wcscpy(CommonStruct->sql_via_text, 
                   CommonStruct->sql_risult.getptr_at(pos + postxt)->resval.rstring);
            node = (C_INT_INT_STR*)CommonStruct->sql_list.get_head();
            while(node != NULL)
            {
      	      if (node->get_key() == CommonStruct->sql_cls->get_key())
      	      {
                  node->set_name(CommonStruct->sql_via_text);
                  break;
               }
               node = (C_INT_INT_STR*)node->get_next();
            }
         }
         else    // la classe è gruppo, ma non indirizzo
         {
            node = (C_INT_INT_STR*)CommonStruct->sql_list.get_head();
            while(node != NULL)
            {
      	      if (node->get_key() == CommonStruct->sql_cls->get_key())
      	      {
                  node->set_name(CommonStruct->sql_via_cod.get_name());
                  break;
               }
               node=(C_INT_INT_STR*)node->get_next();
            }
         }
         codice = CommonStruct->sql_risult.getptr_at(pos+poscod)->resval.rlong;
         CommonStruct->sql_risult << acutBuildList(RTLB, RTLB, RTLB,
                                                   RTSTR, pInfo->key_attrib.get_name(),
                                                   RTLONG, codice,
                                                   RTLE, RTLE, RTLE, 0); 
         ads_done_dialog(hdlg, DLGOK);
      }
      else ads_done_dialog(hdlg, DLGCANCEL);
   }
   else   // ho fatto la scelta globale (Tutte)
   {
      CommonStruct->sql_via_cod.clear();
      int cnt = (CommonStruct->sql_risult.GetCount() - 2) / passo;
      poscod = (CommonStruct->list_field_sql.getpos_name(pInfo->key_attrib.get_name())) * 4;

      CommonStruct->sql_risult.getptr_at(1 + poscod);

      CommonStruct->sql_via_cod += pInfo->key_attrib.get_name();
      CommonStruct->sql_via_cod += _T(" IN (");

      for (int i=0; i<cnt; i++)
      {
         if (i > 0) { for (int j = 0; j<passo; j++) CommonStruct->sql_risult.get_next(); }

         CommonStruct->sql_via_cod += CommonStruct->sql_risult.get_cursor();
         if (i != cnt-1) CommonStruct->sql_via_cod += _T(",");
      }
      
      CommonStruct->sql_via_cod += _T(")");

      ads_done_dialog(hdlg, DLGOK);
   }
}
///////////////////////////////////////
// ACTION TILE : click su tasto HELP //
///////////////////////////////////////
static void CALLB dcl_liststreets_help(ads_callback_packet *dcl)
{
   //gsc_help();
} 
/////////////////////////////////////////////////////////////////////
// ACTION TILE : click su lista civici (classe derivata in genere) //
/////////////////////////////////////////////////////////////////////
static void CALLB dcl_address_listcivici(ads_callback_packet *dcl)
{
   int           posiz, ris, rispdef = 1, cnti = 0, cnte = 0, passo = 0, posi = 0;
   C_STRING      stringa;
   C_CLASS       *pCls;
   C_ATTRIB_LIST *pattr;
   ads_hdlg      hdlg = dcl->dialog;
   C_STRING      prompt;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   pCls  = gsc_find_class(GS_CURRENT_WRK_SESSION->get_PrjId(), CommonStruct->num_cls, 0);
   pattr = pCls->ptr_attrib_list();
   posiz = _wtoi(dcl->value);

   if ((dcl->reason == CBR_DOUBLE_CLICK) && 
       (CommonStruct->all_sel_der == 0)) // scelta univoca
   {
      if (CommonStruct->sql_ind == 1)      // la classe è indirizzo
      {
         passo = (pattr->get_count()) * 4 + 2;
         if (posiz == 0)
         {
            prompt += gsc_msg(358);              // "Confermare la scelta"
            prompt += _T(' ');
            prompt += gsc_rtrim(CommonStruct->sql_via_text);
         }
         else
         {
            prompt += gsc_msg(359);              // " Confermare Civico "
            prompt += gsc_rtrim(CommonStruct->ris_sql_civ.getptr_at(9 + (posiz - 1) * passo)->resval.rstring);
         } 
         prompt += _T('.');  
         if (gsc_ddgetconfirm(prompt.get_name(), &ris, rispdef) == GS_BAD) return;
      }
      else  // è classe diversa da indirizzo
      // compongo la stringa con i valori di tutti i campi selezionati
      {
         cnti = CommonStruct->list_field_sql_addr.get_count();
         cnte = CommonStruct->ris_sql_civ.GetCount() - 2;
         passo = (cnti * 4) + 2,
         posi = posiz * passo + 5;

         //CommonStruct->ris_sql_civ.getptr_at(posi);
			if (gsc_string_confirm(&stringa, &CommonStruct->ris_sql_civ,
                                posi - 1, passo) == GS_BAD) return;             
         prompt += gsc_msg(358);              // "Confermare la scelta"
         prompt += _T('\n');
         prompt += gsc_rtrim(stringa.get_name());  
         if (gsc_ddgetconfirm(prompt.get_name(), &ris, rispdef,
				                  GS_BAD, GS_BAD, GS_GOOD) == GS_BAD) return;
      }
    
      if (ris)
      {
         if (CommonStruct->sql_ind == 1)  // la classe è gruppo - indirizzo
         {
            if (posiz !=0) 
            {
               CommonStruct->sql_civ_cod.clear();
               CommonStruct->sql_civ_cod += 
               gsc_rtrim(CommonStruct->ris_sql_civ.getptr_at(4)->resval.rstring);
               CommonStruct->sql_civ_cod += _T(" = ");
               CommonStruct->sql_civ_cod += CommonStruct->ris_sql_civ.getptr_at(5 + (posiz - 1) * passo);

               wcscpy(CommonStruct->sql_civ_text, _T("TEXT = '"));
               wcscat(CommonStruct->sql_civ_text,
               gsc_rtrim(CommonStruct->ris_sql_civ.getptr_at(9 + (posiz - 1) * passo)->resval.rstring));
               wcscat(CommonStruct->sql_civ_text, _T("'"));

               C_INT_INT_STR *node;

               node = (C_INT_INT_STR *) CommonStruct->sql_list.get_head();
               while(node != NULL)
               {
      	         if (node->get_key() == CommonStruct->num_cls)
      	         {
                     node->set_name(gsc_rtrim(CommonStruct->ris_sql_civ.getptr_at(9 + (posiz - 1) * passo)->resval.rstring));
                     break;
                  }
                  node=(C_INT_INT_STR*)node->get_next();
               }
            }
            else
            {
               CommonStruct->sql_civ_cod.clear();
               wcscpy(CommonStruct->sql_civ_text, GS_EMPTYSTR);
            }
            ads_done_dialog(hdlg, DLGOK);
         }                                                 
         else // la classe è gruppo ma diversa da indirizzo
         {
            CommonStruct->sql_civ_cod.clear();
            CommonStruct->sql_civ_cod +=
            gsc_rtrim(CommonStruct->ris_sql_civ.getptr_at(4)->resval.rstring);
            CommonStruct->sql_civ_cod += _T(" = ");
            CommonStruct->sql_civ_cod += CommonStruct->ris_sql_civ.getptr_at(5 + posiz * passo);

            C_INT_INT_STR *node;
                                                      
            node = (C_INT_INT_STR*)CommonStruct->sql_list.get_head();
            while(node != NULL)
            {
      	      if (node->get_key() == CommonStruct->num_cls)
      	      {
                  node->set_name(gsc_rtrim(CommonStruct->sql_civ_cod.get_name()));
                  break;
               }
               node=(C_INT_INT_STR*)node->get_next();
            }
         }
         ads_done_dialog(hdlg, DLGOK);
      }                                                 
      else ads_done_dialog(hdlg, DLGCANCEL);
   }
}
///////////////////////////////////////////////////////////////////////////////////////////////////
// ACTION TILE : click su tasto OK per accettazione CIVICO (classe derivata in generale da lista //
///////////////////////////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_listcivico_accept_ok(ads_callback_packet *dcl)
{
   int           passo, posiz = 0, cnti = 0, cnte = 0, posi = 0, ris, rispdef = 1;
   ads_hdlg      hdlg = dcl->dialog;
   C_STRING      prompt, stringa;
   C_CLASS       *pCls; 
   C_ATTRIB_LIST *pattr;
   TCHAR         valore[10];
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

	CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   pCls  = gsc_find_class(GS_CURRENT_WRK_SESSION->get_PrjId(), CommonStruct->num_cls);
   pattr = pCls->ptr_attrib_list();

   if (ads_get_tile(hdlg, _T("listcivici"), valore, 10) != RTNORM) return;
   else posiz = _wtoi(valore);
   
   passo = (pattr->get_count()) * 4 + 2;

   if (CommonStruct->all_sel_der == 0)    // ho fatto la scelta univoca 
   {
      if (CommonStruct->sql_ind == 1)     // la classe è gruppo - indirizzo
      {
         if (posiz == 0)
         {
            prompt += gsc_msg(358);                // "Confermare la scelta"
            prompt += _T(' ');
            prompt += gsc_rtrim(CommonStruct->sql_via_text);
         }
         else
         {
            prompt += gsc_msg(359);                // "Confermare Civico "
            prompt += gsc_rtrim(CommonStruct->ris_sql_civ.getptr_at(9 + (posiz - 1) * passo)->resval.rstring);
         }
         prompt += _T('.');  
         if (gsc_ddgetconfirm(prompt.get_name(), &ris, rispdef) == GS_BAD) return; 
      }
      else  // è classe diversa da indirizzo
      // compongo la stringa con i valori di tutti i campi selezionati
      {
         cnti = CommonStruct->list_field_sql_addr.get_count();
         cnte = CommonStruct->ris_sql_civ.GetCount() - 2;
         passo = (cnti * 4) + 2,
         posi = posiz * passo + 5;

         CommonStruct->ris_sql_civ.getptr_at(posi);
			if (gsc_string_confirm(&stringa, &CommonStruct->ris_sql_civ) == GS_BAD) return;             
         prompt += gsc_msg(358);              // "Confermare la scelta"
         prompt += _T('\n');
         prompt += gsc_rtrim(stringa.get_name());  
         if (gsc_ddgetconfirm(prompt.get_name(), &ris, rispdef,
				                  GS_BAD, GS_BAD, GS_GOOD) == GS_BAD) return;
      }

      if (ris)        // se risposta affermativa
      {
         if (CommonStruct->sql_ind == 1)  // la classe è gruppo - indirizzo
         {
            if (posiz > 0) 
            {
               CommonStruct->sql_civ_cod.clear();
               CommonStruct->sql_civ_cod += 
               gsc_rtrim(CommonStruct->ris_sql_civ.getptr_at(4)->resval.rstring);
               CommonStruct->sql_civ_cod += _T(" = ");
               CommonStruct->sql_civ_cod += CommonStruct->ris_sql_civ.getptr_at(5 + (posiz - 1) * passo);
               wcscpy(CommonStruct->sql_civ_text, _T("TEXT = '"));
               wcscat(CommonStruct->sql_civ_text, 
               gsc_rtrim(CommonStruct->ris_sql_civ.getptr_at(9 + (posiz - 1) * passo)->resval.rstring));
               wcscat(CommonStruct->sql_civ_text, _T("'"));

               C_INT_INT_STR *node;

               node = (C_INT_INT_STR*)CommonStruct->sql_list.get_head();
               while(node != NULL)
               {
      	         if (node->get_key() == CommonStruct->num_cls)
      	         {
                     node->set_name(gsc_rtrim(CommonStruct->ris_sql_civ.getptr_at(9 + (posiz - 1) * passo)->resval.rstring));
                     break;
                  }
                  node=(C_INT_INT_STR*)node->get_next();
               }
            }
            else 
            { 
               wcscpy(CommonStruct->sql_civ_text, GS_EMPTYSTR); 
               CommonStruct->sql_civ_cod.clear();
            }
            ads_done_dialog(hdlg, DLGOK);
         }
         else // la classe è gruppo ma diversa da indirizzo
         {
            CommonStruct->sql_civ_cod.clear();
            CommonStruct->sql_civ_cod +=
            gsc_rtrim(CommonStruct->ris_sql_civ.getptr_at(4)->resval.rstring);
            CommonStruct->sql_civ_cod += _T(" = ");
            CommonStruct->sql_civ_cod += CommonStruct->ris_sql_civ.getptr_at(5 + posiz * passo);

            C_INT_INT_STR *node;
                                                         
            node = (C_INT_INT_STR*)CommonStruct->sql_list.get_head();
            while(node != NULL)
            {
      	      if (node->get_key() == CommonStruct->num_cls)
      	      {
                  node->set_name(gsc_rtrim(CommonStruct->sql_civ_cod.get_name()));
                  break;
               }
               node=(C_INT_INT_STR*)node->get_next();
            }
         }
         ads_done_dialog(hdlg, DLGOK);
      }
      else ads_done_dialog(hdlg, DLGCANCEL);
   }
   else   // ho fatto la scelta globale
   {
      int cnt = (CommonStruct->ris_sql_civ.GetCount() - 2) / passo;
      CommonStruct->sql_civ_cod.clear();
      // se la condizione con 'OR' crea problemi di SQL sostituirla con la condizione IN(..)
      // come segue
      /*CommonStruct->sql_civ_cod.getptr_at(1 + poscod);

      CommonStruct->sql_civ_cod += pInfo->key_attrib;
      CommonStruct->sql_civ_cod += _T(" IN (");

      for (int i = 0; i < cnt; i++)
      {
         if (i > 0) { for (int j = 0; j < passo; j++) CommonStruct->ris_sql_civ.get_next(); }

         CommonStruct->sql_civ_cod += CommonStruct->ris_sql_civ.get_cursor();
         if (i != cnt - 1) CommonStruct->sql_civ_cod += _T(",");
      }
      
      CommonStruct->sql_civ_cod += _T(")");*/

      for (int i = 0; i < cnt; i++)
      {
         CommonStruct->sql_civ_cod += CommonStruct->ris_sql_civ.getptr_at(4);
         CommonStruct->sql_civ_cod += _T(" = ");
         CommonStruct->sql_civ_cod += CommonStruct->ris_sql_civ.getptr_at(5 + passo * i);
         if (i != cnt - 1) CommonStruct->sql_civ_cod += _T(" OR ");
      }

      if (CommonStruct->sql_ind != 1)  // la classe NON è gruppo - indirizzo
      {
         C_INT_INT_STR *node;
                                                      
         node = (C_INT_INT_STR*)CommonStruct->sql_list.get_head();
         while(node != NULL)
         {
      	   if (node->get_key() == CommonStruct->num_cls)
      	   {
               node->set_name(gsc_rtrim(CommonStruct->sql_civ_cod.get_name()), 0, 128);
               break;
            }
            node=(C_INT_INT_STR*)node->get_next();
         }
      }

      ads_done_dialog(hdlg, DLGOK);
   }
}
// ACTION TILE : click su tasto HELP //
static void CALLB dcl_listcivico_help(ads_callback_packet *dcl)
{
   //gsc_help();
} 
/////////////////////////////////////////
// ACTION TILE : click su lista classi //
/////////////////////////////////////////
static void CALLB dcl_address_listclass(ads_callback_packet *dcl)
{
   int         prj, cls, sub, posiz = 0;
   ads_hdlg    hdlg = dcl->dialog;
   C_2LONG_STR *punt;
   TCHAR       valore[10];
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   posiz = _wtoi(dcl->value);

   if (dcl->reason == CBR_DOUBLE_CLICK)
   {
      if (ads_get_tile(hdlg, _T("listclass"), valore, 10) != RTNORM) return;
      else posiz = _wtoi(valore) + 1;

      punt = (C_2LONG_STR *) CommonStruct->list_class_to_visual.getptr_at(posiz);
      prj = GS_CURRENT_WRK_SESSION->get_PrjId();
      cls = (int) punt->get_id();
      sub = (int) punt->get_id_2();
   
      ads_set_tile(hdlg, _T("error"), gsc_msg(360)); // "Attendere..."

      if ((CommonStruct->sql_cls = gsc_find_class(prj, cls, sub)) == NULL) return;
      else if (CommonStruct->sql_cls->get_category() == CAT_SPAGHETTI)
         { ads_set_tile(hdlg, _T("error"), gsc_msg(367)); return; }
              //"SQL non definibile per questa classe !"

      ads_done_dialog(hdlg, DLGOK);
   }                     
}
//////////////////////////////////////////////////////////////////////
// ACTION TILE : click su tasto OK per accettazione CLASSE da lista //
//////////////////////////////////////////////////////////////////////
static void CALLB dcl_listclass_accept_ok(ads_callback_packet *dcl)
{
   int         prj, cls, sub, posiz = 0;
   ads_hdlg    hdlg = dcl->dialog;
   C_2LONG_STR *punt;
   TCHAR       valore[10];
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   if (ads_get_tile(hdlg, _T("listclass"), valore, 10) != RTNORM) return;
   else posiz = _wtoi(valore) + 1;

   punt = (C_2LONG_STR *) CommonStruct->list_class_to_visual.getptr_at(posiz);
   prj = GS_CURRENT_WRK_SESSION->get_PrjId();
   cls = (int) punt->get_id();
   sub = (int) punt->get_id_2();

   ads_set_tile(hdlg, _T("error"), gsc_msg(360)); //  "Attendere..."

   if ((CommonStruct->sql_cls = gsc_find_class(prj, cls, sub)) == NULL) return;
   else if (CommonStruct->sql_cls->get_category() == CAT_SPAGHETTI)
   {
      ads_set_tile(hdlg, _T("error"), gsc_msg(367));//"SQL non definibile per questa classe !"
      return;
   }

   ads_done_dialog(hdlg, DLGOK);
} 
////////////////////////////////////////////////////////////////
// ACTION TILE : click su tasto OK per impostazione indirizzo //
////////////////////////////////////////////////////////////////
static void CALLB dcl_address_accept_ok(ads_callback_packet *dcl)
{
   ads_hdlg      hdlg = dcl->dialog;
   int           cls, ret = 1;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   ads_set_tile(hdlg, _T("error"), gsc_msg(360));      //   "Attendere..."
   cls = CommonStruct->sql_cls->get_key();
    
   if (CommonStruct->sql_via_cod.len() != 0) 
   {
      ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR);
      if (gsc_do_sql_addr(cls, 0, CommonStruct->sql_via_cod.get_name()) == GS_BAD)
         return;
      CommonStruct->n_classi << acutBuildList(RTLB, RTSHORT, cls , 0);
      if (CommonStruct->sql_civ_cod.len() != 0) 
      {
         if (gsc_do_sql_addr(CommonStruct->num_cls, 0,
                             CommonStruct->sql_civ_cod.get_name()) == GS_BAD) return;
         CommonStruct->n_classi += acutBuildList(RTSHORT, CommonStruct->num_cls, 0); 
      }
      CommonStruct->n_classi += acutBuildList(RTLE, 0);
   }

   ads_done_dialog(hdlg, DLGOK);
}
// ACTION TILE : click su tasto HELP //
static void CALLB dcl_listclass_help(ads_callback_packet *dcl)
{
   //gsc_help();
} 

/////////////////////////////////////////
// ACTION TILE : click su tasto CANCEL //
/////////////////////////////////////////
static void CALLB dcl_filter_quitter(ads_callback_packet *dcl)
{
   ads_hdlg hdlg=dcl->dialog;
   Common_Dcl_main_Sql_Addr_Struct *CommonFasStruct;

   CommonFasStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   if (CommonFasStruct->sql_risult.get_head()) CommonFasStruct->sql_risult.remove_all();

   ads_done_dialog(hdlg, DLGCANCEL);
}

// ACTION TILE : click su tasto HELP //
static void CALLB dcl_address_help(ads_callback_packet *dcl)
   { gsc_help(IDH_Selezionezonaperentit); } 


//////////////////////////////////////////////////////////////////
// ACTION TILE : click su tasto EDIT per impostazione indirizzo //
//////////////////////////////////////////////////////////////////
static void CALLB dcl_address_edit(ads_callback_packet *dcl)
{
   TCHAR valore[10];

   if (ads_get_tile(dcl->dialog, _T("listqry"), valore, 10) != RTNORM) return;

   //if (dcl->value) wcscpy(dcl->value, GS_EMPTYSTR);
   //gsc_strcat(dcl->value, valore);
   //dcl->reason = CBR_DOUBLE_CLICK;
   ads_callback_packet new_dcl;

   new_dcl.dialog = dcl->dialog;
   new_dcl.tile = dcl->tile;
   new_dcl.value = gsc_tostring(valore);
   new_dcl.client_data = dcl->client_data;
   new_dcl.reason = CBR_DOUBLE_CLICK;
   new_dcl.x = dcl->x;
   new_dcl.y = dcl->y;

   return dcl_address_listqry(&new_dcl);
}


/*********************************************************************/
/*.doc gsc_filter_sql <external> */
/*+
  Questa funzione imposta il filtro SQL da usarsi nei filtri, nell'impo-
  stazione FAS/SQL delle classi e nella ricerca territoriale mediante 
  condizione SQL.
  Parametri:
  C_CLASS            *pCls : puntatore alla classe oggetto di analisi;
  C_INT_INT_STR_LIST *sql_list: 
  int                tipo     : parametro che discerne il tipo di chiamata
                     1 = chiamata operata dalla impostazione classi(FAS/SQL);
                     2 = chiamata operata dalla ricerca territoriale mediante 
                         condizione SQL sulla classe oggetto di analisi.
  Common_Dcl_main_Sql_Addr_Struct *CommonStruct: struttura con le variabili di
                     passaggio fra DCL.

  La funzione restituisce GS_BAD in caso di errore GS_GOOD oppure GS_CAN.
-*/  
/*********************************************************************/
int gsc_filter_sql(C_CLASS *pCls, C_INT_INT_STR_LIST *sql_list, int tipo,
                   Common_Dcl_main_Sql_Addr_Struct *CommonStruct)
{
   ads_hdlg      dcl_id;
   int           dcl_file, status, ret, cls = 0, flag = 1, subcls =0;
   size_t        len;
   C_ID          *pid, *sub;
   C_INT_LIST    *group_list = NULL;
   C_STR         *node1;
   C_INT_INT_STR *node2;
   C_CLASS       *pMemberCls;
   C_INT         *punt;
   TCHAR         str[MAX_LEN_SQL_STM+MAX_LEN_CLASSNAME+10], *strg, val[30], *string;
	C_STRING      path;
                                                            
   len = MAX_LEN_SQL_STM + MAX_LEN_CLASSNAME + 10;

   if ((CommonStruct->filter_cls = pCls) == NULL || sql_list == NULL) 
      { GS_ERR_COD=eGSInvalidArg; return GS_BAD; }
   
   CommonStruct->cls_name.remove_all();
   CommonStruct->sql_list.remove_all();
   wcscpy(CommonStruct->sql_statement, GS_EMPTYSTR);

   if (tipo == 1)
   {
      C_STRING SQL;

      if (gsc_ClsSQLQryLoad(pCls->ptr_id()->code, 0, SQL) == GS_GOOD)
			gsc_strcpy(CommonStruct->sql_statement, SQL.get_name(), MAX_LEN_SQL_STM);
   }

   pid = CommonStruct->filter_cls->ptr_id();
   if (pid->category == CAT_GROUP)
      group_list = (C_INT_LIST*)CommonStruct->filter_cls->ptr_group_list();

   if ((tipo == 2) && group_list != NULL)
   {
	   if ((node1 = new C_STR(pCls->get_name())) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
	   CommonStruct->cls_name.add_tail(node1);

	   if ((node2 = new C_INT_INT_STR) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
	   CommonStruct->sql_list.add_tail(node2);
	   if ((node2->set_name(GS_EMPTYSTR)) == GS_BAD) return GS_BAD;
	   node2->set_key(pCls->get_key());
	   node2->set_type(0);

 	   punt = (C_INT*)group_list->get_head();
	   while(punt!=NULL)
	   {
		   if ((pMemberCls = GS_CURRENT_WRK_SESSION->find_class(punt->get_key())) == NULL) return GS_BAD;

		   if ((node1 = new C_STR(pMemberCls->get_name())) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
		   CommonStruct->cls_name.add_tail(node1);

		   if ((node2 =new C_INT_INT_STR) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
		   CommonStruct->sql_list.add_tail(node2);
		   if ((node2->set_name(GS_EMPTYSTR)) == GS_BAD) return GS_BAD;
		   node2->set_key(pMemberCls->get_key());
         node2->set_type(0);
 
	      punt = (C_INT*)punt->get_next();
		}

      path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");

      do
      { 
         ret = GS_GOOD;
         CommonStruct->n_classi.remove_all();
         // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
         if (gsc_load_dialog(path, &dcl_file) == RTERROR) return GS_BAD;
 
         ads_new_dialog(_T("sql_address_list"), dcl_file, (CLIENTFUNC)NULLCB, &dcl_id);
         if (dcl_id == NULL)
            { ads_unload_dialog(dcl_file); GS_ERR_COD = eGSAbortDCL; return GS_BAD; }
      
         if (flag == 0)
				if (GS_ERR_COD == 221)
               ads_set_tile(dcl_id, _T("error"), gsc_err(eGSNoValidListSep));
				else // "*Errore* Impostazione gruppo fallita ..."
					ads_set_tile(dcl_id, _T("error"), gsc_msg(370));        

         // riempio la list box delle condizioni
         ads_start_list(dcl_id, _T("listqry"), LIST_NEW, 0);
         node1 = (C_STR*)CommonStruct->cls_name.get_head();
         node2 = (C_INT_INT_STR*)CommonStruct->sql_list.get_head();
         while(node1 != NULL && node2 != NULL)
         {
   	      if (len > node1->len() + node2->len())
            {
   		      swprintf(str, MAX_LEN_SQL_STM+MAX_LEN_CLASSNAME+10, 
                        _T("%s\t:\t%s"), node1->get_name(), node2->get_name()); 
               gsc_add_list(str); 
            }
            node1 = (C_STR*)node1->get_next();
            node2 = (C_INT_INT_STR*)node2->get_next();
         }
         ads_end_list();
    
         wcscpy(CommonStruct->sql_statement, GS_EMPTYSTR);
         wcscpy(CommonStruct->sql_field, GS_EMPTYSTR);
         cls = CommonStruct->sql_cls->get_key();
         ads_action_tile(dcl_id, _T("slidqry"), (CLIENTFUNC) dcl_address_slidqry);
         ads_client_data_tile(dcl_id, _T("slidqry"), CommonStruct);
         ads_action_tile(dcl_id, _T("listqry"), (CLIENTFUNC) dcl_address_listqry);
         ads_client_data_tile(dcl_id, _T("listqry"), CommonStruct);
         ads_action_tile(dcl_id, _T("edit"), (CLIENTFUNC) dcl_address_edit); 
         ads_client_data_tile(dcl_id, _T("edit"), CommonStruct);
         ads_mode_tile(dcl_id, _T("accept"), MODE_SETFOCUS);
         ads_action_tile(dcl_id, _T("accept"), (CLIENTFUNC) dcl_address_accept_ok);
         ads_client_data_tile(dcl_id, _T("accept"), CommonStruct);
         ads_action_tile(dcl_id, _T("cancel"), (CLIENTFUNC) dcl_filter_quitter);
         ads_client_data_tile(dcl_id, _T("cancel"), CommonStruct);
         ads_action_tile(dcl_id, _T("help"), (CLIENTFUNC) dcl_address_help);

         swprintf(val, 30, _T("%d"), 0);
         ads_set_tile(dcl_id, _T("listqry"), val);
         ads_mode_tile(dcl_id, _T("edit"), MODE_SETFOCUS);

         // LANCIA LA DIALOG-BOX
         ads_start_dialog(dcl_id, &status);
         ads_unload_dialog(dcl_file);

         // chiamo la funzione che estrae le coordinate dai file relativi
			if (status == DLGOK)
				if (CommonStruct->n_classi.get_head() != NULL)
				{
					if ((ret = gsc_extract_coord(&CommonStruct->n_classi, 
						                           CommonStruct->all_sel_der)) == GS_BAD) flag = 0;
				}
		}
      while (ret == GS_BAD);

      if (tipo != 2)
      { if (status == DLGOK) CommonStruct->sql_list.move_all(sql_list); else return GS_CAN; }
	}
   else
   {
      if ((tipo == 2) && (pCls->get_category() != CAT_GROUP))
      {
         if ((strg = gsc_adr_bldqry(CommonStruct, CommonStruct->sql_cls, 
                                    GS_EMPTYSTR, GS_EMPTYSTR, GS_EMPTYSTR)) != NULL)
            { free(strg); }
         if (CommonStruct->sql_via_cod.len() != 0)
         {
            sub = CommonStruct->sql_cls->ptr_id();
            cls = sub->code;
            subcls = sub->sub_code;

	         if (gsc_do_sql_addr(cls, subcls, CommonStruct->sql_via_cod.get_name()) == GS_BAD)
               return GS_BAD;
            CommonStruct->n_classi << acutBuildList(RTLB, RTSHORT, cls, RTLE,0);
         }
         // chiamo la funzione che estrae le coordinate dai file relativi
         if (CommonStruct->n_classi.get_head() != NULL)
            if ((ret = gsc_extract_coord(&CommonStruct->n_classi, 
                                          CommonStruct->all_sel_der)) == GS_BAD) return GS_BAD;
      }
      else
      {
         if ((string = gsc_sql_bldqry(pCls, CommonStruct->sql_statement, tipo)) != NULL)
         {
      	   if ((node2 = new C_INT_INT_STR) == NULL) 
               { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   	      sql_list->add_tail(node2);
   	      if ((node2->set_name(string)) == GS_BAD) return GS_BAD;
   	      node2->set_key(pid->code);
   	      node2->set_type(pid->sub_code);
   	      free(string);
   		}
   	   else return GS_CAN;
      }
	}

   return GS_GOOD;
}
//////////////////////////////////////////////////////////////
// ACTION TILE : click su lista attributi per opzione WHERE //
//////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_listattr(ads_callback_packet *dcl)
{
   int            pos;
   size_t         len;
   ads_hdlg       hdlg = dcl->dialog;
   C_ATTRIB       *attr;
	C_STRING       FieldName;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);

   pos = _wtoi(dcl->value);

   if (dcl->reason == CBR_DOUBLE_CLICK)
   {
      if ((attr = (C_ATTRIB*)CommonStruct->attr_list->getptr_at(pos + 1)) == NULL) return;
      if ((len = 1 + wcslen(CommonStruct->sql_statement)
                 + wcslen(attr->get_name())) >= MAX_LEN_SQL_STM) return;
      wcscat(CommonStruct->sql_statement, _T(" "));
      FieldName = attr->get_name();
		C_DBCONNECTION *pConn = CommonStruct->filter_cls->ptr_info()->getDBConnection(OLD);
      // modifico il nome del campo in funzione del tipo di connessione
		if (gsc_AdjSyntax(FieldName, pConn->get_InitQuotedIdentifier(), pConn->get_FinalQuotedIdentifier(),
								pConn->get_StrCaseFullTableRef()) == GS_BAD) return;
      wcscat(CommonStruct->sql_statement, FieldName.get_name());
      ads_set_tile(hdlg, _T("editsql"), CommonStruct->sql_statement); 
      ads_mode_tile(hdlg, _T("editsql"), MODE_SETFOCUS); 
   }
}
///////////////////////////////////////////////////////////////
// ACTION TILE : click su lista attributi per opzione SELECT //
///////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_attrlist(ads_callback_packet *dcl)
{
   int            pos;
   size_t         len;
   ads_hdlg       hdlg = dcl->dialog;
   C_ATTRIB       *attr;
   C_STR          *nod;
	C_STRING       FieldName; 
	C_DBCONNECTION *pConn;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   pConn = CommonStruct->filter_cls->ptr_info()->getDBConnection(OLD);
   ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR);

   pos = _wtoi(dcl->value);

   if (dcl->reason == CBR_DOUBLE_CLICK)
   {
      if (pos != 0)
      { 
         if ((gsc_strstr(CommonStruct->sql_field, _T("*")) == NULL))
         {
            if ((attr = (C_ATTRIB*)CommonStruct->attr_list->getptr_at(pos)) == NULL) return;
            FieldName = attr->get_name();
            if ((len = 1 + wcslen(CommonStruct->sql_field)
                       + wcslen(attr->get_name())) >= MAX_LEN_SQL_STM) return;
            if (gsc_strcmp(CommonStruct->sql_field, GS_EMPTYSTR) != 0)
                wcscat(CommonStruct->sql_field, _T(" , "));
				// modifico il nome del campo in funzione del tipo di connessione
            if (gsc_AdjSyntax(FieldName, pConn->get_InitQuotedIdentifier(),
					               pConn->get_FinalQuotedIdentifier(),
										pConn->get_StrCaseFullTableRef()) == GS_BAD) return;
            wcscat(CommonStruct->sql_field, FieldName.get_name()); 

            if ((nod = new C_STR(attr->get_name())) == NULL)
               { GS_ERR_COD = eGSOutOfMem; CommonStruct->list_field_sql.remove_all(); return; }
            CommonStruct->list_field_sql.add_tail(nod);
         }
         else ads_set_tile(hdlg, _T("error"), gsc_msg(371));
         // "Impossibile la scelta * con altri campi..."
      }
      else
      {
         if (gsc_strcmp(CommonStruct->sql_field, GS_EMPTYSTR) == 0) 
            wcscat(CommonStruct->sql_field, _T("*"));
         else ads_set_tile(hdlg, _T("error"), gsc_msg(371));
         // "Impossibile la scelta * con altri campi..."
      }
      ads_set_tile(hdlg, _T("editfield"), CommonStruct->sql_field); 
      ads_mode_tile(hdlg, _T("editfield"), MODE_DISABLE); 
   }
}
/////////////////////////////////////////////////////////////////
// ACTION TILE : click su lista attributi per opzione ORDER_BY //
/////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_listorder(ads_callback_packet *dcl)
{
   int            pos;
   size_t         len;
   TCHAR          *p1 = NULL, *p2 = NULL;
   ads_hdlg       hdlg = dcl->dialog;
   C_ATTRIB       *attr;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);

   ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR);

   pos = _wtoi(dcl->value);

   if (dcl->reason == CBR_DOUBLE_CLICK)
   {
      if ((attr = (C_ATTRIB*)CommonStruct->attr_list->getptr_at(pos+1)) == NULL) return;
      if ((len = 1 + wcslen(CommonStruct->sql_order)
                + wcslen(attr->get_name())) >= MAX_LEN_SQL_STM) return;

      if (((p1 = gsc_strstr(CommonStruct->sql_field, attr->get_name())) == NULL) &&
          ((p2 = gsc_strstr(CommonStruct->sql_field, _T("*"))) == NULL))
      {
         C_STRING dummy;

         dummy = gsc_msg(372); // "Impossibile ordinare su un campo non selezionato..."
         dummy += attr->get_name();
         ads_set_tile(hdlg, _T("error"), dummy.get_name()); 
      }
      else
      {  
      	C_STRING FieldName;
			
			FieldName = attr->get_name();
			C_DBCONNECTION *pConn = CommonStruct->filter_cls->ptr_info()->getDBConnection(OLD);
			// modifico il nome del campo in funzione del tipo di connessione
			if (gsc_AdjSyntax(FieldName, pConn->get_InitQuotedIdentifier(), 
                           pConn->get_FinalQuotedIdentifier(),
									pConn->get_StrCaseFullTableRef()) == GS_BAD) return;

         if (gsc_strcmp(CommonStruct->sql_order, GS_EMPTYSTR) == 0)
            wcscat (CommonStruct->sql_order, FieldName.get_name());
         else
         {   
            if ((gsc_strcmp(CommonStruct->sql_order, GS_EMPTYSTR) != 0) &&
                (gsc_strstr(CommonStruct->sql_order, attr->get_name()) == NULL))
            {
               wcscat(CommonStruct->sql_order, _T(" , "));
               wcscat(CommonStruct->sql_order, FieldName.get_name());
            }
         }
      }
      
      ads_set_tile(hdlg, _T("editorder"), CommonStruct->sql_order); 
      ads_mode_tile(hdlg, _T("editorder") ,MODE_DISABLE); 
   }
}
////////////////////////////////
// ACTION TILE : click su AND //
////////////////////////////////
static void CALLB dcl_bldqry_and(ads_callback_packet *dcl)
{
   size_t    len;
   ads_hdlg  hdlg = dcl->dialog;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);

   if ((len = 1 + wcslen(CommonStruct->sql_statement)
             + wcslen(_T("and"))) >= MAX_LEN_SQL_STM) return;
   wcscat(CommonStruct->sql_statement, _T(" AND "));

   ads_set_tile(hdlg, _T("editsql"), CommonStruct->sql_statement); 
   ads_mode_tile(hdlg, _T("editsql"), MODE_SETFOCUS); 
}
///////////////////////////////
// ACTION TILE : click su OR //
///////////////////////////////
static void CALLB dcl_bldqry_or(ads_callback_packet *dcl)
{
   size_t    len;
   ads_hdlg  hdlg = dcl->dialog;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);

   if ((len = 1 + wcslen(CommonStruct->sql_statement)
              + wcslen(_T("OR"))) >= MAX_LEN_SQL_STM) return;
   wcscat(CommonStruct->sql_statement, _T(" OR "));

   ads_set_tile(hdlg, _T("editsql"), CommonStruct->sql_statement); 
   ads_mode_tile(hdlg, _T("editsql"), MODE_SETFOCUS); 
}
////////////////////////////////
// ACTION TILE : click su NOT //
////////////////////////////////
static void CALLB dcl_bldqry_not(ads_callback_packet *dcl)
{
   size_t    len;
   ads_hdlg  hdlg = dcl->dialog;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);

   if ((len = 1 + wcslen(CommonStruct->sql_statement) 
              + wcslen(_T("NOT"))) >= MAX_LEN_SQL_STM) return;
   wcscat(CommonStruct->sql_statement, _T(" NOT "));

   ads_set_tile(hdlg, _T("editsql"), CommonStruct->sql_statement); 
   ads_mode_tile(hdlg, _T("editsql"), MODE_SETFOCUS); 
}
// operatori generici
////////////////////////////////////
// ACTION TILE : click su BETWEEN //
////////////////////////////////////
static void CALLB dcl_bldqry_between(ads_callback_packet *dcl)
{
   size_t     len;
   ads_hdlg   hdlg = dcl->dialog;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);

   if ((len = 1 + wcslen(CommonStruct->sql_statement)
            + wcslen(_T("BETWEEN"))) >= MAX_LEN_SQL_STM) return;
   wcscat(CommonStruct->sql_statement, _T(" BETWEEN "));

   ads_set_tile(hdlg, _T("editsql"), CommonStruct->sql_statement); 
   ads_mode_tile(hdlg, _T("editsql"), MODE_SETFOCUS); 
}
///////////////////////////////
// ACTION TILE : click su IN //
///////////////////////////////
static void CALLB dcl_bldqry_in(ads_callback_packet *dcl)
{
   size_t     len;
   ads_hdlg   hdlg=dcl->dialog;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);

   if ((len = 1 + wcslen(CommonStruct->sql_statement)
             + wcslen(_T(" IN(<valore1>, <valore2> ...)"))) >= MAX_LEN_SQL_STM) return;
   wcscat(CommonStruct->sql_statement, _T(" IN(<valore1>, <valore2> ...)"));

   ads_set_tile(hdlg, _T("editsql"), CommonStruct->sql_statement); 
   ads_mode_tile(hdlg, _T("editsql"), MODE_SETFOCUS); 
}
////////////////////////////////////
// ACTION TILE : click su IS NULL //
////////////////////////////////////
static void CALLB dcl_bldqry_is_null(ads_callback_packet *dcl)
{
   size_t     len;
   ads_hdlg   hdlg=dcl->dialog;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);

   if ((len = 1 + wcslen(CommonStruct->sql_statement)
             + wcslen(_T(" IS NULL"))) >= MAX_LEN_SQL_STM) return;
   wcscat(CommonStruct->sql_statement, _T(" IS NULL"));

   ads_set_tile(hdlg, _T("editsql"), CommonStruct->sql_statement); 
   ads_mode_tile(hdlg, _T("editsql"), MODE_SETFOCUS); 
}
////////////////////////////////////////
// ACTION TILE : click su IS NOT NULL //
////////////////////////////////////////
static void CALLB dcl_bldqry_is_not_null(ads_callback_packet *dcl)
{
   size_t     len;
   ads_hdlg   hdlg=dcl->dialog;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);

   if ((len = 1 + wcslen(CommonStruct->sql_statement)
             + wcslen(_T(" IS NOT NULL"))) >= MAX_LEN_SQL_STM) return;
   wcscat(CommonStruct->sql_statement, _T(" IS NOT NULL"));

   ads_set_tile(hdlg, _T("editsql"), CommonStruct->sql_statement); 
   ads_mode_tile(hdlg, _T("editsql"), MODE_SETFOCUS); 
}
////////////////////////////////////////
// ACTION TILE : click su '(' open_op //
////////////////////////////////////////
static void CALLB dcl_bldqry_open_op(ads_callback_packet *dcl)
{
   size_t     len;
   ads_hdlg   hdlg=dcl->dialog;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);

   if ((len = 1 + wcslen(CommonStruct->sql_statement)
             + wcslen(_T("("))) >= MAX_LEN_SQL_STM) return;
   wcscat(CommonStruct->sql_statement, _T(" ("));

   ads_set_tile(hdlg, _T("editsql"), CommonStruct->sql_statement); 
   ads_mode_tile(hdlg, _T("editsql"), MODE_SETFOCUS); 
}
/////////////////////////////////////////
// ACTION TILE : click su ')' close_op //
/////////////////////////////////////////
static void CALLB dcl_bldqry_close_op(ads_callback_packet *dcl)
{
   size_t     len;
   ads_hdlg   hdlg=dcl->dialog;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);

   if ((len = 1 + wcslen(CommonStruct->sql_statement)
             + wcslen(_T(")"))) >= MAX_LEN_SQL_STM) return;
   wcscat(CommonStruct->sql_statement, _T(")"));

   ads_set_tile(hdlg, _T("editsql"), CommonStruct->sql_statement); 
   ads_mode_tile(hdlg, _T("editsql"), MODE_SETFOCUS); 
}
/////////////////////////////////////
// ACTION TILE : click su operator //
/////////////////////////////////////
static void CALLB dcl_bldqry_operator(ads_callback_packet *dcl)
{
   int            val;
   size_t         len;
   ads_hdlg       hdlg = dcl->dialog;
   TCHAR          op[5] = GS_EMPTYSTR;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   ads_get_tile(hdlg, _T("Operator"), op, 4); 

   if ((len = 3 + wcslen(CommonStruct->sql_statement)) >= MAX_LEN_SQL_STM) return;
   val = _wtoi(op);
   switch(val)
      {
         case 0 :
            wcscat(CommonStruct->sql_statement, _T(" = ")); break;
         case 1 :
            wcscat(CommonStruct->sql_statement, _T(" > ")); break;
         case 2 :
            wcscat(CommonStruct->sql_statement, _T(" < ")); break;
         case 3 :
            wcscat(CommonStruct->sql_statement, _T(" <> ")); break;
         case 4 :                  
            wcscat(CommonStruct->sql_statement, _T(" >= ")); break;
         case 5 :
            wcscat(CommonStruct->sql_statement, _T(" <= ")); break;
         default : break;
      }   
   ads_set_tile(hdlg, _T("editsql"), CommonStruct->sql_statement); 
   ads_mode_tile(hdlg, _T("editsql"), MODE_SETFOCUS); 
}
// operatori stringa
////////////////////////////////////////
// ACTION TILE : click su TERMINA CON //
////////////////////////////////////////
static void CALLB dcl_bldqry_EndWith(ads_callback_packet *dcl)
{
   size_t     len;
   ads_hdlg   hdlg=dcl->dialog;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);

   if ((len = 1 + wcslen(CommonStruct->sql_statement)
             + wcslen(_T(" LIKE '%<valore>'"))) >= MAX_LEN_SQL_STM) return;

   CommonStruct->tp_like = 2;
   wcscat(CommonStruct->sql_statement, _T(" LIKE '%<valore>'"));

   ads_set_tile(hdlg, _T("editsql"), CommonStruct->sql_statement); 
   ads_mode_tile(hdlg, _T("editsql"), MODE_SETFOCUS); 
}
///////////////////////////////////////
// ACTION TILE : click su INIZIA CON //
///////////////////////////////////////
static void CALLB dcl_bldqry_StartWith(ads_callback_packet *dcl)
{
   size_t     len;
   ads_hdlg   hdlg=dcl->dialog;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);

   if ((len = 1 + wcslen(CommonStruct->sql_statement) 
            + wcslen(_T(" LIKE '<valore>%'"))) >= MAX_LEN_SQL_STM) return;

   CommonStruct->tp_like = 1;
   wcscat(CommonStruct->sql_statement, _T(" LIKE '<valore>%'"));

   ads_set_tile(hdlg, _T("editsql"), CommonStruct->sql_statement); 
   ads_mode_tile(hdlg, _T("editsql"), MODE_SETFOCUS); 
} 
///////////////////////////////////////
// ACTION TILE : click su CONTENENTE //
///////////////////////////////////////
static void CALLB dcl_bldqry_Containing(ads_callback_packet *dcl)
{
   size_t     len;
   ads_hdlg   hdlg=dcl->dialog;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);

   if ((len = 1 + wcslen(CommonStruct->sql_statement)
             + wcslen(_T(" LIKE '%<valore>%'"))) >= MAX_LEN_SQL_STM) return;

   CommonStruct->tp_like = 2;
   wcscat(CommonStruct->sql_statement, _T(" LIKE '%<valore>%'"));

   ads_set_tile(hdlg, _T("editsql"), CommonStruct->sql_statement); 
   ads_mode_tile(hdlg, _T("editsql"), MODE_SETFOCUS); 
}
/////////////////////////////////
// ACTION TILE : click su LIKE //
/////////////////////////////////
static void CALLB dcl_bldqry_like(ads_callback_packet *dcl)
{
   size_t     len;
   ads_hdlg   hdlg=dcl->dialog;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);

   if ((len = 1 + wcslen(CommonStruct->sql_statement) 
            + wcslen(_T(" LIKE '"))) >= MAX_LEN_SQL_STM) return;

   CommonStruct->tp_like = 1;
   wcscat(CommonStruct->sql_statement, _T(" LIKE '"));

   ads_set_tile(hdlg, _T("editsql"), CommonStruct->sql_statement); 
   ads_mode_tile(hdlg, _T("editsql"), MODE_SETFOCUS); 
} 
// operatori date
///////////////////////////////
// ACTION TILE click su DATE //
///////////////////////////////
static void CALLB dcl_bldqry_date(ads_callback_packet *dcl)
{
   C_STRING   Msg(" TIMESTAMP'<");
   size_t     len;
   ads_hdlg   hdlg = dcl->dialog;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   Msg += gsc_msg(296); // "aaaa-mm-gg"
   Msg += _T("> 00:00:00'");
   if ((len = 1 + wcslen(CommonStruct->sql_statement) + Msg.len()) >= MAX_LEN_SQL_STM) return;
   wcscat(CommonStruct->sql_statement, Msg.get_name()); 

	ads_set_tile(hdlg, _T("editsql"), CommonStruct->sql_statement); 
   ads_mode_tile(hdlg, _T("editsql"), MODE_SETFOCUS); 
}
///////////////////////////////////
// ACTION TILE click su DATETIME //
///////////////////////////////////
static void CALLB dcl_bldqry_datetime(ads_callback_packet *dcl)
{
   C_STRING   Msg(" TIMESTAMP'<");
	size_t     len;
   ads_hdlg   hdlg = dcl->dialog;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   Msg += gsc_msg(296); // "aaaa-mm-gg"
   Msg += _T(" ");
   Msg += gsc_msg(300); // "hh:mm:ss"
   Msg += _T(">'");
   if ((len = 1 + wcslen(CommonStruct->sql_statement) + Msg.len()) >= MAX_LEN_SQL_STM) return;
   wcscat(CommonStruct->sql_statement, Msg.get_name()); 

	ads_set_tile(hdlg, _T("editsql"), CommonStruct->sql_statement); 
   ads_mode_tile(hdlg, _T("editsql"), MODE_SETFOCUS); 
}
///////////////////////////////
// ACTION TILE click su YEAR //
///////////////////////////////
static void CALLB dcl_bldqry_year(ads_callback_packet *dcl)
{
   size_t     len;
   ads_hdlg   hdlg = dcl->dialog;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);

   if ((len = 1 + wcslen(CommonStruct->sql_statement)
            + wcslen(_T("YEAR"))) >= MAX_LEN_SQL_STM) return;
   wcscat(CommonStruct->sql_statement, _T(" YEAR("));

   ads_set_tile(hdlg, _T("editsql"), CommonStruct->sql_statement); 
   ads_mode_tile(hdlg, _T("editsql"), MODE_SETFOCUS); 
}
///////////////////////////////////////
// ACTION TILE click su lista valori //
///////////////////////////////////////
static void CALLB dcl_bldqry_AttrValues(ads_callback_packet *dcl)
{
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;
	ads_hdlg       hdlg = dcl->dialog;
   TCHAR          strPos[10];
   C_RB_LIST      Structure;
   C_STRING       AttrName, result, Title;
	C_CLASS        *pCls;
	C_DBCONNECTION *pConn;
	C_ATTRIB       *pAttrib;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   ads_get_tile(hdlg, _T("listattr"), strPos, 10); 
	pCls = CommonStruct->sql_cls;
   pConn = pCls->ptr_info()->getDBConnection(OLD);
	// ricavo la lista degli attributi della classe
	pAttrib = (C_ATTRIB*)CommonStruct->attr_list->get_head();
   Structure << acutBuildList (RTLB, 0);
   while (pAttrib != NULL)
   {
      Structure += acutBuildList(RTSTR, pAttrib->get_name(), 0);
      pAttrib = (C_ATTRIB*)pAttrib->get_next();
   }
	Structure += acutBuildList(RTLE, 0);
   AttrName = gsc_nth(_wtoi(strPos), Structure.get_head())->resval.rstring;

   if (pCls)
   {
      C_STRING dummy;

      if (pCls->ptr_secondary_list->get_head() == NULL)
      {
         if (!pCls || !pCls->ptr_info()) return;
			Title = gsc_msg(230); //  "Classe: "
         pCls->get_CompleteName(dummy);
	      Title += dummy;

			Title += _T(" - ");
			Title += AttrName.get_name();
         if (gsc_dd_sel_uniqueVal(pCls->get_PrjId(), pCls->ptr_id()->code, pCls->ptr_id()->sub_code, 0,
                                  AttrName, result, Title) != GS_GOOD) return;

         if ((pAttrib = (C_ATTRIB *) pCls->ptr_attrib_list()->search_name(AttrName.get_name(), FALSE)) == NULL) 
            return;
         pAttrib->init_ADOType(pCls->ptr_info()->getDBConnection(OLD));
      }
      if (gsc_DBIsChar(pAttrib->ADOType) == GS_GOOD)
      {  // Aggiungo apici perchè è di tipo carattere
         dummy = _T("'");
         dummy += result;
         dummy += _T("'");
         result = dummy;
      }
   }
   else
      if (gsc_dd_sel_uniqueVal(pConn, pCls->ptr_info()->OldTableRef, AttrName,
                               result, Title) != GS_GOOD) return;

   if (result.len() > 0)
	{
	   wcscat(CommonStruct->sql_statement, result.get_name());
		ads_set_tile(hdlg, _T("editsql"), CommonStruct->sql_statement); 
		ads_mode_tile(hdlg, _T("editsql"), MODE_SETFOCUS);
	}
}
////////////////////////////////////
// ACTION TILE : click su editsql //
////////////////////////////////////
static void CALLB dcl_bldqry_editsql(ads_callback_packet *dcl)
{
   ads_hdlg   hdlg = dcl->dialog;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   ads_get_tile(hdlg, _T("editsql"), CommonStruct->sql_statement, MAX_LEN_SQL_STM); 
}
//////////////////////////////////////
// ACTION TILE : click su editfield //
//////////////////////////////////////
static void CALLB dcl_bldqry_editfield(ads_callback_packet *dcl)
{
   ads_hdlg   hdlg=dcl->dialog;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   ads_get_tile(hdlg, _T("editfield"), CommonStruct->sql_field, MAX_LEN_SQL_STM); 
}
//////////////////////////////////////
// ACTION TILE : click su editorder //
//////////////////////////////////////
static void CALLB dcl_bldqry_editorder(ads_callback_packet *dcl)
{
   ads_hdlg   hdlg=dcl->dialog;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   ads_get_tile(hdlg, _T("editorder"), CommonStruct->sql_order, MAX_LEN_SQL_STM); 
}
///////////////////////////////////////////////
// ACTION TILE : click su CLEARSQL o su CLEAR//
///////////////////////////////////////////////
static void CALLB dcl_bldqry_clear(ads_callback_packet *dcl)
{
   ads_hdlg       hdlg = dcl->dialog;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   wcscpy(CommonStruct->sql_statement, GS_EMPTYSTR);

   ads_set_tile(hdlg, _T("editsql"), CommonStruct->sql_statement); 
   ads_mode_tile(hdlg, _T("editsql"), MODE_SETFOCUS); 
   ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR);
}
///////////////////////////////////////
// ACTION TILE : click su CLEARORDER //
///////////////////////////////////////
static void CALLB dcl_bldqry_clearorder(ads_callback_packet *dcl)
{
   ads_hdlg       hdlg = dcl->dialog;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   wcscpy(CommonStruct->sql_order, GS_EMPTYSTR);

   ads_set_tile(hdlg, _T("editorder"), CommonStruct->sql_order); 
   ads_mode_tile(hdlg, _T("accept"), MODE_SETFOCUS); 
   ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR);

}
///////////////////////////////////////
// ACTION TILE : click su CLEARFIELD //
///////////////////////////////////////
static void CALLB dcl_bldqry_clearfield(ads_callback_packet *dcl)
{
   ads_hdlg       hdlg = dcl->dialog;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   wcscpy(CommonStruct->sql_field, GS_EMPTYSTR);
   CommonStruct->list_field_sql.remove_all();

   ads_set_tile(hdlg, _T("editfield"), CommonStruct->sql_field); 
   ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR);
}
/////////////////////////////////////////////////////////////////
// ACTION TILE : click su tasto OK se sto impostando indirizzo //
/////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_accept_ok(ads_callback_packet *dcl)
{
   ads_hdlg      hdlg = dcl->dialog;
   TCHAR         *err, *pl = NULL, *via_text = NULL;
   int           cnti, ris, rispdef = 1, numpos = 0, posiz = 0, ripetiz = 0, poscod;
   long          codice;
   C_STR         *nod ,str;
   C_STRING      prompt, stringa;
	C_ATTRIB      *attr;
   C_RB_LIST     fields;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);
   ads_set_tile(hdlg, _T("error"), gsc_msg(360));               //  "Attendere..."

   if ((err = CommonStruct->sql_cls->check_sql(CommonStruct->sql_statement)) == NULL)
   {
      // verifico se in SQL_STATEMENT esiste la clausola ORDER BY che è trattata a parte
      if ((pl = gsc_strstr(CommonStruct->sql_statement, _T(" ORDER "))) != NULL)
         ads_set_tile(hdlg, _T("error"), gsc_msg(364));
         //  "Cancellare la clausola ORDER BY ed impostarla a parte..."
      else
      {
         // cerco il valore del campo chiave
         C_INFO   *pInfo;
         pInfo = CommonStruct->sql_cls->ptr_info();

         // controllo se e come ho impostato i campi da visualizzare alla prima entrata nella
         // maschera di condizioni SQL
         if (CommonStruct->list_field_sql.get_count() == 0)
         {
            if (gsc_strcmp(CommonStruct->sql_field, _T("*")) == 0) // ho selezionato tutti i campi
            {
               attr = (C_ATTRIB*)CommonStruct->attr_list->get_head();
               while(attr != NULL)
               {
                  if ((nod = new C_STR((int) wcslen(attr->get_name())+1)) == NULL)
                     { GS_ERR_COD = eGSOutOfMem; CommonStruct->list_field_sql.remove_all(); return; }
                  CommonStruct->list_field_sql.add_tail(nod);
                  if (nod->set_name(attr->get_name()) == GS_BAD)
                     { CommonStruct->list_field_sql.remove_all(); return; }
                  attr = (C_ATTRIB*)attr->get_next();
               }
            }
            if (gsc_strcmp(CommonStruct->sql_field, GS_EMPTYSTR) == 0) // non ho impostato alcun campo
            {  
               if (CommonStruct->sql_ind == 1)   // la classe è gruppo-indirizzo
               {
                  // imposto i campi che mi servono nella ricerca SQL solo se via
                  str += pInfo->key_attrib.get_name();
                  if ((nod = new C_STR((int)wcslen(str.get_name())+1)) == NULL)
                     { GS_ERR_COD=eGSOutOfMem; CommonStruct->list_field_sql.remove_all(); return; }
                  CommonStruct->list_field_sql.add_tail(nod);
                  if (nod->set_name(str.get_name()) == GS_BAD)
                     { CommonStruct->list_field_sql.remove_all(); return; }

                  str.clear();
                  str += CommonStruct->sql_cls->ptr_attrib_list()->getFirstVisibleAttrib()->get_name();
                  if ((nod = new C_STR((int)wcslen(str.get_name()) + 1)) == NULL)
                     { GS_ERR_COD = eGSOutOfMem; CommonStruct->list_field_sql.remove_all(); return; }
                  CommonStruct->list_field_sql.add_tail(nod);
                  if (nod->set_name(str.get_name()) == GS_BAD)
                     { CommonStruct->list_field_sql.remove_all(); return; }
               }
               else 
               {  // se è una generica classe gruppo aggiungo il campo chiave della classe
                  if (CommonStruct->cat == CAT_GROUP)
                  {
                     str.clear();
                     str += pInfo->key_attrib.get_name(); 
                     if ((nod = new C_STR((int)wcslen(str.get_name()) + 1)) == NULL)
                        { GS_ERR_COD=eGSOutOfMem; CommonStruct->list_field_sql.remove_all(); return; }
                     CommonStruct->list_field_sql.add_tail(nod);
                     if (nod->set_name(str.get_name())==GS_BAD)
                        { CommonStruct->list_field_sql.remove_all(); return; }
                  }
                  else  // segnalo che non è stato impostato alcun campo
                  {
                     ads_set_tile(hdlg, _T("error"), gsc_msg(374));  //"Nessun campo selezionato"
                     return;
                  }
               }
            }
         }
         else
         {  // non è presente il campo chiave
            if ((gsc_strstr(CommonStruct->sql_field, pInfo->key_attrib.get_name()) == NULL) &&
                (gsc_strcmp(CommonStruct->sql_field, _T("*")) != 0))
            {
               if (CommonStruct->list_field_sql.getpos_name(pInfo->key_attrib.get_name()) == 0)
               {
                  if ((nod = new C_STR((int)wcslen(pInfo->key_attrib.get_name()) + 1)) == NULL)
                     { GS_ERR_COD=eGSOutOfMem; CommonStruct->list_field_sql.remove_all(); return; }
                  CommonStruct->list_field_sql.add_tail(nod);
                  if (nod->set_name(pInfo->key_attrib.get_name()) == GS_BAD)
                     { CommonStruct->list_field_sql.remove_all(); return; }
               }
            }
            if (CommonStruct->sql_ind == 1) 
            {  // se non è presente l'attributo visibile lo aggiungo
               if (CommonStruct->list_field_sql.getpos_name(CommonStruct->sql_cls->ptr_attrib_list()->getFirstVisibleAttrib()->get_name()) == 0)
               {
                  if ((nod = new C_STR())==NULL)
                     { GS_ERR_COD = eGSOutOfMem; CommonStruct->list_field_sql.remove_all(); return; }
                  CommonStruct->list_field_sql.add_tail(nod);
                  if (nod->set_name(CommonStruct->sql_cls->ptr_attrib_list()->getFirstVisibleAttrib()->get_name()) == GS_BAD)
                     { CommonStruct->list_field_sql.remove_all(); return; }
               }
            }
         }
         // cerco gli elementi che soddisfano la condizione impostata:
         // se non ho impostato la condizione SQL scelgo tutto l'archivio
         if (gsc_find_elements(CommonStruct->sql_cls, &CommonStruct->list_field_sql,
                               CommonStruct->sql_statement, CommonStruct->sql_order,
                               CommonStruct,CommonStruct->sql_risult) == GS_GOOD)
         {
            if (CommonStruct->sql_risult.GetCount() <=2) 
            { 
               ads_set_tile(hdlg, _T("error"), gsc_msg(365));
               //  "Nessun elemento soddisfa la condizione..."
               return; 
            }
         }
         else 
            { ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR); return; }

         posiz = 5;                           // posizione del primo valore che serve
         ripetiz = (CommonStruct->list_field_sql.get_count() * 4) + 2; // passo ripetitivo della lista risultante

         if (CommonStruct->sql_ind == 1)  // sto operando sugli indirizzi
         {
            via_text = CommonStruct->sql_cls->ptr_attrib_list()->getFirstVisibleAttrib()->get_name();
            if (CommonStruct->sql_risult.GetCount() > ripetiz + 2)
            {  // lancio la lista vie che soddisfano alla condizione impostata
               if (gsc_lista_val(&CommonStruct->sql_risult, posiz, ripetiz, CommonStruct) == GS_GOOD)
               {  // lancio la lista civici aggregati alla via 
                  if (CommonStruct->sql_risult.get_head())
                  { 
                     if (gsc_lista_civici(&CommonStruct->sql_risult,
                                 CommonStruct->list_field_sql.getpos_name(pInfo->key_attrib.get_name()),
								         dcl, CommonStruct) == GS_GOOD)
                     {
                        ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR);
                        ads_mode_tile(hdlg, _T("goSQL"), MODE_ENABLE);
                        ads_mode_tile(hdlg, _T("accept"), MODE_ENABLE);
                        ads_mode_tile(hdlg, _T("accept"), MODE_SETFOCUS);
                     }
                  }
               }
            }
            else
            {  // ho imposto una condizione di tipo univoco
               poscod = (CommonStruct->list_field_sql.getpos_name(pInfo->key_attrib.get_name())) * 4 + 1;
               posiz = (CommonStruct->list_field_sql.getpos_name(via_text)) * 4 + 1;
               prompt += gsc_msg(358);                 // "Confermare la scelta"
               prompt += _T('\n');
               prompt += gsc_rtrim(CommonStruct->sql_risult.getptr_at(posiz)->resval.rstring);  
               ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR);
               if (gsc_ddgetconfirm(prompt.get_name(), &ris, rispdef,
						                  GS_BAD, GS_BAD, GS_GOOD) == GS_BAD) return;
               if (ris)
               {
                  wcscpy(CommonStruct->sql_via_text, GS_EMPTYSTR);
                  wcscpy(CommonStruct->sql_statement, GS_EMPTYSTR);
                  wcscat(CommonStruct->sql_statement, via_text);
                  wcscat(CommonStruct->sql_statement, _T(" = '"));
                  wcscat(CommonStruct->sql_statement,
                         gsc_rtrim(CommonStruct->sql_risult.getptr_at(posiz)->resval.rstring)); 
                  wcscat(CommonStruct->sql_statement, _T("'"));
                  wcscat(CommonStruct->sql_via_text,
                         gsc_rtrim(CommonStruct->sql_risult.getptr_at(posiz)->resval.rstring)); 
                  CommonStruct->sql_via_cod.clear();
                  CommonStruct->sql_via_cod += pInfo->key_attrib.get_name();
                  CommonStruct->sql_via_cod += _T(" = ");
                  CommonStruct->sql_via_cod += CommonStruct->sql_risult.getptr_at(poscod);

                  // lancio la lista civici aggregati alla via  
                  if (gsc_lista_civici(&CommonStruct->sql_risult, 
							                  posiz, dcl,CommonStruct) == GS_GOOD)
                  {
                     ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR);
                     ads_mode_tile(hdlg, _T("goSQL"), MODE_ENABLE);
                     ads_mode_tile(hdlg, _T("accept"), MODE_ENABLE);
                     ads_mode_tile(hdlg, _T("accept"), MODE_SETFOCUS);
                  }
               }
               // aggiorno la visualizzazione della via scelta nella prima tendina

               C_INT_INT_STR *node;

               node=(C_INT_INT_STR*)CommonStruct->sql_list.get_head();
               while(node != NULL)
               {
      	         if (node->get_key() == CommonStruct->sql_cls->get_key())
      	         {
                     node->set_name(CommonStruct->sql_via_text);
                     break;
                  }
                  node = (C_INT_INT_STR*)node->get_next();
               }
            }
         }
         else   // classe non indirizzo
         {  
            // ho ottenuto una lista di valori che soddisfano alla condizione impostata
            if (CommonStruct->sql_risult.GetCount() > ripetiz + 2)
            {
               if ((ris = gsc_lista_val(&CommonStruct->sql_risult, posiz, ripetiz, 
                                        CommonStruct)) == GS_GOOD)
               {
                  ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR);
                  ads_mode_tile(hdlg, _T("goSQL"), MODE_ENABLE);
                  ads_mode_tile(hdlg, _T("accept"), MODE_ENABLE);
                  ads_mode_tile(hdlg, _T("accept"), MODE_SETFOCUS);
                  /*if (CommonStruct->cat == CAT_GROUP)
                  {
                     ads_mode_tile(hdlg, _T("close"), MODE_ENABLE);
                     ads_mode_tile(hdlg, _T("close"), MODE_SETFOCUS);
                  }
                  else
                  {
                     ads_mode_tile(hdlg, _T("goSQL"), MODE_ENABLE);
                     ads_mode_tile(hdlg, _T("goSQL"), MODE_SETFOCUS);
                  }*/
               }
            }
            // ho ottenuto un valore univoco creo la stringa con i valori di tutti i campi selezionati
            else
            {
               cnti = CommonStruct->list_field_sql.get_count();
               CommonStruct->sql_risult.copy(CommonStruct->list_field);
               CommonStruct->list_field.getptr_at(posiz);
               if (gsc_string_confirm(&stringa, &CommonStruct->list_field) == GS_BAD) return;
               prompt += gsc_msg(358);                // "Confermare la scelta"
               prompt += _T('\n');
               prompt += gsc_rtrim(stringa.get_name());  
               if (gsc_ddgetconfirm(prompt.get_name(), &ris, rispdef, 
						                  GS_BAD, GS_BAD, GS_GOOD) == GS_BAD) return;
               poscod = (CommonStruct->list_field_sql.getpos_name(pInfo->key_attrib.get_name()) - 1) * 4; 
               if (ris) 
               {
						posiz = 5;
						codice = (CommonStruct->sql_risult.getptr_at(posiz + poscod))->resval.rlong;
                  CommonStruct->sql_via_cod.clear();
                  CommonStruct->sql_via_cod += pInfo->key_attrib.get_name();
                  CommonStruct->sql_via_cod += _T(" = ");
                  CommonStruct->sql_via_cod += codice;
						
                  CommonStruct->sql_risult << acutBuildList(RTLB, RTLB, RTLB,
							                                       RTSTR, pInfo->key_attrib.get_name(),
							                                       RTLONG, codice, 
																				RTLE, RTLE, RTLE, 0); 
               }
               ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR);
               ads_mode_tile(hdlg, _T("accept"), MODE_ENABLE);
               ads_mode_tile(hdlg, _T("accept"), MODE_SETFOCUS);
               ads_mode_tile(hdlg, _T("goSQL"), MODE_ENABLE);
               
            }
            if (CommonStruct->cat == CAT_GROUP  && ris == GS_GOOD)
            {
               C_INT_INT_STR  *node; 
               node=(C_INT_INT_STR*)CommonStruct->sql_list.get_head();
               while(node != NULL)
               {
         	      if (node->get_key() == CommonStruct->sql_cls->get_key())
         	      {                       
                     node->set_name(CommonStruct->sql_via_cod.get_name());
                     break;
                  }
                  node=(C_INT_INT_STR*)node->get_next();
               }
 
               if (gsc_lista_civici(&CommonStruct->sql_risult,
                          CommonStruct->list_field_sql.getpos_name(pInfo->key_attrib.get_name()),
                          dcl, CommonStruct) == GS_GOOD)
                  ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR);

               ads_mode_tile(hdlg, _T("accept"), MODE_ENABLE); 
               ads_mode_tile(hdlg, _T("accept"), MODE_SETFOCUS);
               ads_mode_tile(hdlg, _T("goSQL"), MODE_ENABLE);

               /*if (CommonStruct->cat == CAT_GROUP)
               {
                  ads_mode_tile(hdlg, _T("close"), MODE_ENABLE);
                  ads_mode_tile(hdlg, _T("close"), MODE_SETFOCUS);
               }
               else
               {
                  ads_mode_tile(hdlg, _T("confirm"), MODE_ENABLE);
                  ads_mode_tile(hdlg, _T("confirm"), MODE_SETFOCUS);
               }*/
            }
         }
      }
   }
   else
      { ads_set_tile(hdlg, _T("error"), err); free(err); }   
}
/////////////////////////////////////////
// ACTION TILE : click su tasto CANCEL //
/////////////////////////////////////////
static void CALLB dcl_bldqry_quitter(ads_callback_packet *dcl)
{
   ads_hdlg hdlg = dcl->dialog;
   Common_Dcl_main_Sql_Addr_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_main_Sql_Addr_Struct*)(dcl->client_data);

   if (CommonStruct->sql_ind != 1)
   {
      CommonStruct->sql_via_cod.clear();
      CommonStruct->sql_civ_cod.clear();
   }
   wcscpy(CommonStruct->sql_civ_text, GS_EMPTYSTR);
   wcscpy(CommonStruct->sql_via_text, GS_EMPTYSTR);
   wcscpy(CommonStruct->sql_statement, GS_EMPTYSTR);
   wcscpy(CommonStruct->sql_order, GS_EMPTYSTR);
   wcscpy(CommonStruct->sql_field, GS_EMPTYSTR);

   ads_mode_tile(hdlg, _T("accept"), MODE_DISABLE);
   ads_mode_tile(hdlg, _T("goSQL"), MODE_ENABLE);

   ads_done_dialog(hdlg, DLGCANCEL);         
}
///////////////////////////////////////////////////
// ACTION TILE : click su tasto CONFIRM  E CLOSE //
///////////////////////////////////////////////////
static void CALLB dcl_bldqry_close_addr(ads_callback_packet *dcl)
{
   ads_done_dialog(dcl->dialog, DLGOK);      
}
// ACTION TILE : click su tasto HELP //
static void CALLB dcl_bldqry_help(ads_callback_packet *dcl)
   { gsc_help(IDH_Selezionezonaperentit); } 

//////////////////////////////////////
// FUNZIONE PER CREAZIONE QUERY SQL //
//////////////////////////////////////
TCHAR *gsc_sql_bldqry(C_CLASS *pCls, TCHAR *in, int mod)
{
	TCHAR 	str[MAX_LEN_CLASSNAME + 15];   
	C_STRING	SQLCond;
	C_INFO	*pInfo;
   
	if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return NULL; }

   // "Classe :"
	swprintf(str, MAX_LEN_CLASSNAME + 15, _T("%s   %s"), gsc_msg(230), pCls->get_name());

	if (in)
		SQLCond = in;

	if ((pInfo = pCls->ptr_info()) == NULL) return NULL;

	if (gsc_ddBuildQry(str, pInfo->getDBConnection(OLD), pInfo->OldTableRef, SQLCond,
					       GS_CURRENT_WRK_SESSION->get_PrjId(), pCls->ptr_id()->code, 
							 pCls->ptr_id()->sub_code) == GS_BAD)
		return NULL;

	return gsc_tostring(SQLCond.get_name());
}
/////////////////////////////////////////////
// FUNZIONE PER CREAZIONE QUERY ADDRESS    //
/////////////////////////////////////////////
/*********************************************************************/
/*.doc gsc_adr_bldqry <external> */
/*+
  Questa funzione imposta la condizione di SQL per la ricerca da indirizzo.
  Parametri:
  Common_Dcl_main_Sql_Addr_Struct *CommonStruct: strutttura dei dati di 
                                                       passaggio fra DCL;
  C_CLASS *pCls: puntatore alla classescelta;
  TCHAR *in:         stringa della condizione impostata;
  TCHAR *via :       testo della via;
  TCHAR *civ :       testo del civico.

  La funzione restituisce NULL in caso di errore altrimenti la stringa-
  istruzione SQl impostata.
-*/  
/*********************************************************************/
TCHAR *gsc_adr_bldqry(Common_Dcl_main_Sql_Addr_Struct *CommonStruct,
                      C_CLASS *pCls, TCHAR *in, TCHAR *via, TCHAR *civ)
{
   C_STRING path;
   TCHAR    str[MAX_LEN_CLASSNAME + 15], val[30];
   ads_hdlg dcl_id;
   int      status, dcl_file;
   C_ATTRIB *attr;
   
   // ripulisco le variabili globali 
   wcscpy(CommonStruct->sql_field, _T("*"));
   wcscpy(CommonStruct->sql_order, GS_EMPTYSTR);
   wcscpy(CommonStruct->sql_statement, GS_EMPTYSTR);
   CommonStruct->list_field_sql.remove_all();
   CommonStruct->list_field.remove_all();

   if (CommonStruct->sql_cplx) CommonStruct->cat = CommonStruct->sql_cplx->get_category();
   else CommonStruct->cat = CommonStruct->sql_cls->get_category();

   // verifico se ho selezionato la classe gruppo
   if (CommonStruct->cat == CAT_GROUP)
      if (gsc_strcmp(pCls->get_name(), CommonStruct->sql_cplx->get_name()) == 0) return NULL;

   // verifico se ho già impostato la via oppure no
   if ((gsc_strcmp(pCls->get_name(), civ) == 0) &&
       (gsc_strcmp(CommonStruct->sql_statement, GS_EMPTYSTR) == 0)) return NULL;

   if ((CommonStruct->sql_cls = pCls) == NULL) { GS_ERR_COD = eGSInvalidArg; return NULL; }

   if ((CommonStruct->attr_list = pCls->ptr_attrib_list()) == NULL)
      { GS_ERR_COD = eGSInvClassType; return NULL; }

   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX //
   if (gsc_load_dialog(path, &dcl_file) == RTERROR) return NULL;

   ads_new_dialog(_T("entity_bldqry"), dcl_file, (CLIENTFUNC)NULLCB, &dcl_id);
   ads_action_tile(dcl_id, _T("help"), (CLIENTFUNC) dcl_bldqry_help);

   if (dcl_id == NULL) { ads_unload_dialog(dcl_file); GS_ERR_COD=eGSAbortDCL; return NULL; }
   
   swprintf(str, MAX_LEN_CLASSNAME + 15, _T("%s   %s"), gsc_msg(230), pCls->get_name()); // "Classe :"
   ads_set_tile(dcl_id, _T("TITOLO"), str);

   ads_set_tile(dcl_id, _T("editfield"), CommonStruct->sql_field);
  
   // riempio la list box con gli attributi che posso scegliere come visualizzabili 
   // nella SELECT
   ads_start_list(dcl_id, _T("attrlist"), LIST_NEW, 0);
   swprintf(str, MAX_LEN_CLASSNAME + 15, _T("%s"), _T("*"));
   gsc_add_list(str);
   attr = (C_ATTRIB*)CommonStruct->attr_list->get_head();
   while(attr != NULL)
   {
      gsc_add_list(attr->get_name());
      attr = (C_ATTRIB*)attr->get_next();
   }
   ads_end_list();
   swprintf(val, 30, _T("%d"), 0);
   ads_set_tile(dcl_id, _T("attrlist"), val);

   // riempio la list box con gli attributi che posso scegliere come impostabili con 
   // la clausola WHERE 
   ads_start_list(dcl_id, _T("listattr"), LIST_NEW, 0);
   attr = (C_ATTRIB*)CommonStruct->attr_list->get_head();
   while(attr != NULL)
   {
      gsc_add_list(attr->get_name());
      attr = (C_ATTRIB*)attr->get_next();
   }
   ads_end_list();
   swprintf(val, 30, _T("%d"), 0);
   ads_set_tile(dcl_id, _T("listattr"), val);

   // riempio la list box con gli attributi che posso scegliere come impostabili con 
   // la clausola ORDER BY 
   ads_start_list(dcl_id, _T("listorder"), LIST_NEW, 0);
   attr = (C_ATTRIB*)CommonStruct->attr_list->get_head();
   while(attr != NULL)
   {
      gsc_add_list(attr->get_name());
      attr = (C_ATTRIB*)attr->get_next();
   }
   ads_end_list();
   swprintf(val, 30, _T("%d"), 0);
   ads_set_tile(dcl_id, _T("listorder"), val);

   // attivo tutti gli action_tile delle possibili scelte
   ads_action_tile(dcl_id, _T("attrlist"), (CLIENTFUNC)dcl_bldqry_attrlist);
   ads_client_data_tile(dcl_id, _T("attrlist"), CommonStruct);
   ads_action_tile(dcl_id, _T("AttrValues"), (CLIENTFUNC)dcl_bldqry_AttrValues);
   ads_client_data_tile(dcl_id, _T("AttrValues"), CommonStruct);
   ads_mode_tile(dcl_id, _T("editfield"), MODE_DISABLE); 
   ads_action_tile(dcl_id, _T("listattr"), (CLIENTFUNC)dcl_bldqry_listattr);
   ads_client_data_tile(dcl_id, _T("listattr"), CommonStruct);
   ads_action_tile(dcl_id, _T("listorder"), (CLIENTFUNC)dcl_bldqry_listorder);
   ads_client_data_tile(dcl_id, _T("listorder"), CommonStruct);
   ads_mode_tile(dcl_id, _T("editorder"), MODE_DISABLE); 
	// operatori generici 
   ads_action_tile(dcl_id, _T("is_null"), (CLIENTFUNC)dcl_bldqry_is_null);
   ads_client_data_tile(dcl_id, _T("is_null"), CommonStruct);
   ads_action_tile(dcl_id , _T("is_not_null"), (CLIENTFUNC) dcl_bldqry_is_not_null);
   ads_client_data_tile(dcl_id, _T("is_not_null"), CommonStruct);
   ads_action_tile(dcl_id, _T("in"), (CLIENTFUNC)dcl_bldqry_in);
   ads_client_data_tile(dcl_id, _T("in"), CommonStruct);
   ads_action_tile(dcl_id, _T("between"), (CLIENTFUNC)dcl_bldqry_between);
   ads_client_data_tile(dcl_id, _T("between"), CommonStruct);
   ads_action_tile(dcl_id, _T("open_op"), (CLIENTFUNC)dcl_bldqry_open_op);
   ads_client_data_tile(dcl_id, _T("open_op"), CommonStruct);
   ads_action_tile(dcl_id, _T("close_op"), (CLIENTFUNC)dcl_bldqry_close_op);
   ads_client_data_tile(dcl_id, _T("close_op"), CommonStruct);
   ads_action_tile(dcl_id, _T("Operator"), (CLIENTFUNC)dcl_bldqry_operator);
   ads_client_data_tile(dcl_id, _T("Operator"), CommonStruct);
   // operatori logici
   ads_action_tile(dcl_id, _T("or"), (CLIENTFUNC)dcl_bldqry_or);
   ads_client_data_tile(dcl_id, _T("or"), CommonStruct);
   ads_action_tile(dcl_id, _T("and"), (CLIENTFUNC)dcl_bldqry_and);
   ads_client_data_tile(dcl_id, _T("and"), CommonStruct);
   ads_action_tile(dcl_id, _T("not"), (CLIENTFUNC)dcl_bldqry_not);
   ads_client_data_tile(dcl_id, _T("not"), CommonStruct);
	// operatori stringa
   ads_action_tile(dcl_id, _T("like"), (CLIENTFUNC)dcl_bldqry_like);
   ads_client_data_tile(dcl_id, _T("like"), CommonStruct);
   ads_action_tile(dcl_id, _T("EndWith"), (CLIENTFUNC)dcl_bldqry_EndWith);
   ads_client_data_tile(dcl_id, _T("EndWith"), CommonStruct);
   ads_action_tile(dcl_id, _T("StartWith"), (CLIENTFUNC)dcl_bldqry_StartWith);
   ads_client_data_tile(dcl_id, _T("StartWith"), CommonStruct);
   ads_action_tile(dcl_id, _T("Containing"), (CLIENTFUNC)dcl_bldqry_Containing);
   ads_client_data_tile(dcl_id, _T("Containing"), CommonStruct);
	// operatori date
   ads_action_tile(dcl_id, _T("date"), (CLIENTFUNC)dcl_bldqry_date);
   ads_client_data_tile(dcl_id, _T("date"), CommonStruct);
	ads_action_tile(dcl_id, _T("datetime"), (CLIENTFUNC)dcl_bldqry_datetime);
   ads_client_data_tile(dcl_id, _T("datetime"), CommonStruct);
   ads_action_tile(dcl_id, _T("year"), (CLIENTFUNC)dcl_bldqry_year);
   ads_client_data_tile(dcl_id, _T("year"), CommonStruct);
   // gestione istruzione sql
   ads_action_tile(dcl_id, _T("editsql"), (CLIENTFUNC)dcl_bldqry_editsql);
   ads_client_data_tile(dcl_id, _T("editsql"), CommonStruct);
   ads_action_tile(dcl_id, _T("clearfield"), (CLIENTFUNC)dcl_bldqry_clearfield);
   ads_client_data_tile(dcl_id, _T("clearfield"), CommonStruct);
   ads_action_tile(dcl_id, _T("clearorder"), (CLIENTFUNC)dcl_bldqry_clearorder);
   ads_client_data_tile(dcl_id,_T( "clearorder"), CommonStruct);
   ads_action_tile(dcl_id, _T("clearsql"), (CLIENTFUNC)dcl_bldqry_clear);
   ads_client_data_tile(dcl_id, _T("clearsql"), CommonStruct);
   /*if (CommonStruct->cat == CAT_GROUP)
   {
      ads_action_tile(dcl_id, _T("close"), (CLIENTFUNC)dcl_bldqry_close_addr);
      ads_mode_tile(dcl_id, _T("close"), MODE_DISABLE); 
   }
   else
   {*/
      ads_action_tile(dcl_id, _T("goSQL"), (CLIENTFUNC)dcl_bldqry_accept_ok);
      ads_client_data_tile(dcl_id, _T("goSQL"), CommonStruct);
      //ads_mode_tile(dcl_id, _T("goSQL"), MODE_DISABLE);
   //}
   ads_action_tile(dcl_id, _T("accept"), (CLIENTFUNC)dcl_bldqry_close_addr);
   ads_mode_tile(dcl_id, _T("accept"), MODE_DISABLE);

   ads_action_tile(dcl_id, _T("cancel"), (CLIENTFUNC)dcl_bldqry_quitter);
   ads_client_data_tile(dcl_id, _T("cancel"), CommonStruct);

   ads_set_tile(dcl_id, _T("error"), GS_EMPTYSTR);

   if (in == NULL) wcscpy(CommonStruct->sql_statement, GS_EMPTYSTR);
   else wcscpy(CommonStruct->sql_statement, in);

   ads_set_tile(dcl_id, _T("editsql"), CommonStruct->sql_statement); 

   // LANCIA LA DIALOG-BOX //
   ads_start_dialog(dcl_id, &status);
   ads_unload_dialog(dcl_file);

   if (status == DLGOK) return gsc_tostring(CommonStruct->sql_statement);
   
   return NULL;
}


/*********************************************************************/
/*.doc gsc_setGraphSettingsToCurrentADEQry <external> */
/*+
  Questa funzione imposta la fas di estrazione per la classe in oggetto.
  Parametri: 
  long    flag_set:  Valore del flag_set con i valori impostati per la classe
  C_CLASS *pCls;     Puntatore alla classe
  C_FAS   &FAS;      Caratteristiche grafiche.

  Restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_setGraphSettingsToCurrentADEQry(long flag_set, C_CLASS *pCls, C_FAS &FAS)
{
   if (ade_altpclear() != RTNORM) return GS_BAD;

   if (gsc_altpdefine(FAS, flag_set) == GS_BAD) return GS_BAD;

   if (flag_set != GSNoneSetting)
      if (gsc_ClsAlterPropQrySave(pCls->ptr_id()->code, pCls->ptr_id()->sub_code,
                                  FAS, flag_set) == GS_BAD)
         return GS_BAD;

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////
/* 
Cerca una zona contenente le entità che soddisfano un criterio SQL.
Questa funzione imposta la scelta da entità di qualunque tipo, verifica se 
esiste, ed imposta la finestra di estrazione minima del disegno .E' invocata dal
LISP senza parametri di passaggio.
*/                                                    
///////////////////////////////////////////////////////////////////////////
int gs_find_addr(void)
{
   int                prjcode, ris = 0;
   long               nfile = 0;
   C_PROJECT          *pproject;
   C_SINTH_CLASS_LIST lista;
   presbuf            arg, filename, pname;
   C_STRING           filedir, namefile, delfile;
   Common_Dcl_main_Sql_Addr_Struct CommonStruct;
   
   // azzero le variabili globali che servono 
   CommonStruct.all_sel = 0;
   CommonStruct.all_sel_der = 0;
   CommonStruct.sql_via_cod.clear();
   CommonStruct.sql_civ_cod.clear();
   wcscpy(CommonStruct.sql_civ_text, GS_EMPTYSTR);
   wcscpy(CommonStruct.sql_via_text, GS_EMPTYSTR);
   wcscpy(CommonStruct.sql_statement, GS_EMPTYSTR);
   wcscpy(CommonStruct.sql_order, GS_EMPTYSTR);
   wcscpy(CommonStruct.sql_field, GS_EMPTYSTR);

   // verifico se nel direttorio della sessione di lavoro esistono dei files .TXT con memorizzate
   // le coordinate di precedenti estrazioni e li cancello.
   filedir = GS_CURRENT_WRK_SESSION->get_dir();
   filedir += _T("\\*.*");
   if (gsc_path_conv(filedir) == GS_BAD) return RTERROR;
   if ((nfile = gsc_adir(filedir.get_name(), &filename, NULL, NULL, NULL)) != 0)
   {
      pname = filename;
      while (pname->rbnext != NULL) 
      {
         if (pname->restype == RTSTR)  // cerco i nomi di files da cancellare
         {
            namefile = pname->resval.rstring;
            // per i valori diversi dal direttorio di lavoro 
            if (namefile.comp(GS_CURRENT_WRK_SESSION->get_name(), FALSE) != 0) // insensitive
            { 
               if ((namefile.at(_T("cord")) != NULL || namefile.at(_T("sele")) != NULL) &&
                   namefile.at(_T(".txt")) != NULL)
               {
                  delfile = GS_CURRENT_WRK_SESSION->get_dir();
                  delfile += _T('\\');
                  delfile += namefile;
                  if (gsc_path_conv(delfile) == GS_BAD) return RTERROR;
                  if (gsc_delfile(delfile.get_name()) == GS_BAD) return RTERROR;
               }
            }
         }
         pname = pname->rbnext;
      }
   } 
   if (filename != NULL) acutRelRb(filename);

   acedRetNil();

   arg = acedGetArgs();

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return RTERROR; }

   prjcode = GS_CURRENT_WRK_SESSION->get_PrjId(); 

   // carico il progetto
   if ((pproject = (C_PROJECT*)GEOsimAppl::PROJECTS.search_key(prjcode)) == NULL)
      return RTERROR;

   // leggo lista (<codice classe> <livello> <nome classe>)
   if (pproject->getSinthClassList(lista) == GS_BAD) return RTERROR;

   // compongo la lista delle classi su cui applicare una condizione SQL
   if ((ris = gsc_lista_classi(prjcode, &lista, &CommonStruct)) == GS_BAD) return RTERROR;

   // Compongo la condizione di ricerca dell'entità 
   if (CommonStruct.sql_cls != NULL) 
   {
      if (gsc_find_cls_env(prjcode, &CommonStruct) == GS_BAD) return RTERROR;
      ris = gsc_find_addr(&CommonStruct);
   }
   else ris = GS_CAN;

   if (ris == GS_CAN) { acedRetNil(); return RTNORM; }
   if (ris != GS_GOOD) return RTERROR;

   acedRetT();
                                       
   return RTNORM;
}


/*********************************************************************/
/*.doc gsc_find_cls_env <external> */
/*+
  Questa ricava il LinkPathName della/e tabella/e OLD delle classi di 
  un progetto passate come parametro.
  Parametri:
  int prj:                        codice progetto;
  Common_Dcl_main_Sql_Addr_Struct *CommonStruct: struttura dei 
                                                 parametri comuni fra DCL.

  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_find_cls_env(int prj, Common_Dcl_main_Sql_Addr_Struct *CommonStruct)
{
   C_INT_INT_STR_LIST sql_list;
   C_GROUP_LIST     *pGroupList;
   C_INT_INT        *pGroup;
   C_CLASS          *child_class;

   wcscpy(CommonStruct->sql_field, GS_EMPTYSTR);
   wcscpy(CommonStruct->sql_statement, GS_EMPTYSTR);
   wcscpy(CommonStruct->sql_order, GS_EMPTYSTR);
   CommonStruct->list_field_sql.remove_all();
   CommonStruct->list_field.remove_all();
   CommonStruct->sql_ind = 0;
   CommonStruct->sql_cplx = CommonStruct->sql_cls;
   
   // se la classe non è gruppo
   if (CommonStruct->sql_cls->get_category() != CAT_GROUP)
   {
      C_STRING LPNName;

      // Ricavo il LinkPathName della tabella OLD
      if (CommonStruct->sql_cls->GetLPN4OLD(LPNName) == GS_BAD) return GS_BAD;
   }

   if (CommonStruct->sql_cls->get_category() == CAT_GROUP)
   { 
      pGroupList = CommonStruct->sql_cls->ptr_group_list();
      if (pGroupList->get_count() == 2)
      {
         C_STRING LPNName;

         pGroup = (C_INT_INT*) pGroupList->get_head();
         while (pGroup)
         {
            child_class = gsc_find_class(GS_CURRENT_WRK_SESSION->get_PrjId(), 
                                         pGroup->get_key());
            if (child_class->GetLPN4OLD(LPNName) == GS_BAD) return GS_BAD;

            pGroup = (C_INT_INT*) pGroup->get_next();
         }
      }
   }

   return GS_GOOD;   
} 


/*********************************************************************/
/*.doc gsc_find_addr <external> */
/*+
  Questa funzione lancia l'impostazione della clausola SQL sulla classe
  scelta.
  Parametri:
  Common_Dcl_main_Sql_Addr_Struct *CommonStruct: struttura con le
                 variabili comuni di passaggio fra una DCL e un'altra.

  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_find_addr(Common_Dcl_main_Sql_Addr_Struct *CommonStruct)
{
   C_INT_INT_STR_LIST sql_list;

   // imposto la sql per la classe scelta
   if ((gsc_filter_sql(CommonStruct->sql_cls, &sql_list, 2, CommonStruct)) != GS_GOOD) return GS_BAD;

   return GS_GOOD;   
} 


/*********************************************************************/
/*.doc gsc_find_elements <external> */
/*+
  Questa funzione ricerca un valore o una lista di valori che soddisfano
  ad una condizione SQL.

  Parametri:
  C_CLASS *pCls       puntatore alla classe in oggetto
  C_STR_LIST *fields  puntatore a lista campi di interesse nell'interrogazione SQL
  TCHAR *condsql      condizione SQL impostata
  TCHAR *condorder    eventuale clausola ORDER per SQL impostata
  Common_Dcl_main_Sql_Addr_Struct *CommonStruct struttura comune delle variabili 'globali'
  C_RB_LIST	&result_list	risultato della ricerca

  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_find_elements(C_CLASS *pCls, C_STR_LIST *fields, TCHAR *condsql, TCHAR *condorder, 
							 Common_Dcl_main_Sql_Addr_Struct *CommonStruct, C_RB_LIST &result_list)
{
	C_INFO	 *pInfo = pCls->ptr_info();
	C_STRING	 sql_condition, table, statement, via_text, FieldName, KeyFldName;
	C_STR	 	 *field = NULL;
	C_RB_LIST result_row, incr_result;
	int       res = GS_GOOD;
	C_DBCONNECTION	*pConn;
	C_ATTRIB_LIST *pAttrib = pCls->ptr_attrib_list();
	_RecordsetPtr	pRs;
	
	if (!pInfo) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
	
	if ((pConn = pInfo->getDBConnection(OLD)) == NULL) return GS_BAD;

   KeyFldName = pInfo->key_attrib;
   if (gsc_AdjSyntax(KeyFldName, pConn->get_InitQuotedIdentifier(), pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD) return GS_BAD;

	// se non presente la condizione GS_ID = 0 (scheda di default) l'agggiungo 
	sql_condition = condsql;
   if (gsc_strcmp(sql_condition.get_name(), GS_EMPTYSTR) == 0)
	{
		sql_condition += KeyFldName;
		sql_condition += _T("<>0");
	}
	else
	{
		sql_condition += _T(" AND ");
		sql_condition += KeyFldName;
		sql_condition += _T("<>0");
	}
   
	table = pInfo->OldTableRef.get_name();

	if (pCls->ptr_group_list() != NULL)
		table += _T("L");

	// costruisco l'istruzione sql con i campi specificati in fields
	field = (C_STR*) fields->get_head();
	//if (gsc_strstr((field->get_name()), _T("*")) != NULL)
	if (fields->get_count() == pAttrib->get_count())
		statement = _T("SELECT * FROM ");
	else
	{
		statement = _T("SELECT ");
		int      i = 1;
		
		while (i <= fields->get_count())
		{  
         FieldName = field->get_name();
			// modifico il nome del campo in funzione del tipo di connessione
			if (gsc_AdjSyntax(FieldName, pConn->get_InitQuotedIdentifier(), pConn->get_FinalQuotedIdentifier(),
									pConn->get_StrCaseFullTableRef()) == GS_BAD) return GS_BAD;
			statement += FieldName.get_name();
			if (i != fields->get_count())
				statement += _T(", ");
			field = (C_STR*) fields->get_next();
			i++;
		}
		statement += _T(" FROM ");
	}

   statement += table;

   if (sql_condition.len() > 0)
	{  // ho impostato una condizione SQL
		statement += _T(" WHERE ");
		statement += sql_condition.get_name();

		if (condorder && wcslen(condorder) > 0)
		{
			statement += _T(" ORDER BY ");
			statement += condorder;
		}
		else
		{  
			if (CommonStruct->sql_ind == 1)
			{ 
				via_text = CommonStruct->sql_cls->ptr_attrib_list()->getFirstVisibleAttrib()->get_name();
				if ((gsc_strstr(CommonStruct->sql_statement, _T(" LIKE ")) != NULL) && 
					(gsc_strstr(CommonStruct->sql_statement, via_text.get_name()) != NULL)) 
				{
					statement += _T(" ORDER BY ");
					statement += via_text;
				}
			}
		}
	}

	// leggo le righe della tabella senza bloccarla
	if (pConn->OpenRecSet(statement, pRs) == GS_BAD) return GS_BAD;
  
   result_list << acutBuildList(RTLB, 0); 
   while (gsc_isEOF(pRs) == GS_BAD)
	{
		if (gsc_DBReadRow(pRs, result_row) == GS_BAD)
			{ gsc_DBCloseRs(pRs); res = GS_BAD; break; }
      gsc_Skip(pRs);
		result_list += gsc_rblistcopy(result_row.get_head());
      result_row.remove_all();
   }
	result_list += acutBuildList(RTLE, 0);

	return GS_GOOD;

}
/*********************************************************************/
/*.doc gsc_lista_val <external> */
/*+
  Questa funzione visualizza in una DCL la lista di valori che soddisfano 
  ad una condizione SQL.
  Parametri:
  C_RB_LIST  lista :lista valori cle soddisfano l'interrogazione SQL
  int        pos   :posizione del resbuf che contiene il valore di interesse
  int        passo :passo ripetitivo del valore di sopra
  Common_Dcl_main_Sql_Addr_Struct *CommonStruct: struttura comune delle
                                                 variabili di passaggio fra DCL.

  La funzione restituisce GS_GOOD se va a buon fine altrimenti GS_BAD.
-*/  
/*********************************************************************/
int gsc_lista_val(C_RB_LIST *lista, int pos, int passo,
                  Common_Dcl_main_Sql_Addr_Struct *CommonStruct)
{
   TCHAR    str[MAX_LEN_SQL_STM + 10], val[30];
   int      i, len, cnte, cnti, status, dcl_file;
   ads_hdlg dcl_id;
   C_STRING stringa, path;
   
   CommonStruct->n_pag = 0;
   len = MAX_LEN_SQL_STM + 10;
   cnte = lista->GetCount() - 2;
   cnti = CommonStruct->list_field_sql.get_count();
   lista->copy(CommonStruct->list_field);

   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   if (gsc_load_dialog(path, &dcl_file) == RTERROR) return NULL;

   // carico la DCL per listare le strade
   ads_new_dialog(_T("sql_street_list"), dcl_file, (CLIENTFUNC)NULLCB, &dcl_id);
   if (dcl_id == NULL)
   {
      ads_unload_dialog(dcl_file);
      GS_ERR_COD=eGSAbortDCL; 
      return GS_BAD;
   }

   swprintf(str, MAX_LEN_SQL_STM + 10, _T("%s   %s"), gsc_msg(230), CommonStruct->sql_cls->get_name()); // "Classe :"
   ads_set_tile(dcl_id, _T("TITOLO1"), str);
   swprintf(str, MAX_LEN_SQL_STM + 10, _T("%s   %s"), gsc_msg(361), CommonStruct->sql_statement); // "Condizione :"
   ads_set_tile(dcl_id, _T("TITOLO2"), str);
   
   // costruisco la lista di valori e riempio la list box
   ads_start_list(dcl_id, _T("liststreet"), LIST_NEW, 0);
                                         
   i = pos;
   lista->getptr_at(i);
   do
	{
      if (gsc_string_arrange(&i, cnti, &stringa, lista, 0) == GS_BAD) return GS_BAD;
      gsc_add_list(stringa);
      for (int k = 0; k < 6; k++) { lista->get_next(); i++; }
   }
	while (i < 5 + 50 * passo && i<= cnte); 

   ads_end_list();

   if (CommonStruct->cat != CAT_GROUP)
   {
      ads_mode_tile(dcl_id, _T("allsel"), MODE_ENABLE);
      ads_mode_tile(dcl_id, _T("multsel"), MODE_ENABLE);
      ads_action_tile(dcl_id, _T("allsel"), (CLIENTFUNC)dcl_filter_allsel);
      ads_client_data_tile(dcl_id, _T("allsel"), CommonStruct);
      ads_action_tile(dcl_id, _T("multsel"), (CLIENTFUNC)dcl_filter_multsel);
      ads_client_data_tile(dcl_id, _T("multsel"), CommonStruct);
   }
   else 
   {
      ads_mode_tile(dcl_id, _T("allsel"), MODE_DISABLE);
      ads_mode_tile(dcl_id, _T("multsel"), MODE_DISABLE);
   }
   ads_action_tile(dcl_id, _T("start"), (CLIENTFUNC) dcl_filter_start);
   ads_client_data_tile(dcl_id, _T("start"), CommonStruct);
   ads_action_tile(dcl_id, _T("end"), (CLIENTFUNC) dcl_filter_end);
   ads_client_data_tile(dcl_id, _T("end"), CommonStruct);
   ads_action_tile(dcl_id, _T("up"), (CLIENTFUNC) dcl_filter_up);
   ads_client_data_tile(dcl_id, _T("up"), CommonStruct);
   ads_action_tile(dcl_id, _T("down"), (CLIENTFUNC) dcl_filter_down);
   ads_client_data_tile(dcl_id, _T("down"), CommonStruct);
   ads_action_tile(dcl_id, _T("slidqry"), (CLIENTFUNC) dcl_filter_slidfield);
   ads_client_data_tile(dcl_id, _T("slidqry"), CommonStruct);
   ads_action_tile(dcl_id, _T("liststreet"),(CLIENTFUNC) dcl_address_liststreet);
   ads_client_data_tile(dcl_id, _T("liststreet"), CommonStruct);
   ads_action_tile(dcl_id, _T("accept"), (CLIENTFUNC) dcl_liststreet_accept_ok);
   ads_client_data_tile(dcl_id, _T("accept"), CommonStruct);
   ads_action_tile(dcl_id, _T("cancel"), (CLIENTFUNC) dcl_filter_quitter);
   ads_client_data_tile(dcl_id, _T("cancel"), CommonStruct);
   ads_action_tile(dcl_id, _T("help"), (CLIENTFUNC) dcl_liststreets_help);

   swprintf(val, 30, _T("%d"), 0);
   ads_set_tile(dcl_id, _T("liststreet"), val);
   ads_mode_tile(dcl_id, _T("liststreet"), MODE_SETFOCUS);
   ads_start_dialog(dcl_id, &status); // LANCIA LA DIALOG-BOX 
   ads_unload_dialog(dcl_file);

   if (status == DLGOK) return GS_GOOD;
   else return GS_CAN;

}
/*********************************************************************/
/*.doc gsc_lista_civici <external> */
/*+	
  Questa funzione lista in una DCL la lista di valori che soddisfano 
  ad una condizione SQL.
  Parametri:
  C_RB_LIST *lista: lista valori che soddisfano l'interrogazione SQL;
  int pos: posizione del resbuf che contiene il valore di interesse;
  ads_callback_packet *dcl: puntatore alla struttura che contiene le 
                            informazioni sulla DCL aperta;
  Common_Dcl_main_Sql_Addr_Struct *CommonStruct: struttura comune delle
                            variabili di passaggio far DCL.

  La funzione restituisce GS_GOOD se va a buon fine altrimenti GS_BAD.
-*/  
/*********************************************************************/
int gsc_lista_civici(C_RB_LIST *lista, int pos, ads_callback_packet *dcl, 
                     Common_Dcl_main_Sql_Addr_Struct *CommonStruct)
{
   TCHAR          strg[MAX_LEN_SQL_STM + 10], val[30];
   int            len, cnt, cls_sel, cls_der, status, i, k, dcl_file;
   int            cnte, cnti, passo, num_pass, num_fields;
   long           gs_id;
   ads_hdlg       dcl_id = dcl->dialog;
   C_RB_LIST      linkStreetValues, linkCivicValues, tempValues;
   C_CLASS        *civici, *indir;
   C_STR_LIST     list_field_sql_street, list_field_sql_civ;
   C_STR          *nod, sql_order;
   C_STRING       stringa, path, sql_statement;
   C_GROUP_LIST *Group;
   C_NODE         *psempl;
   C_ATTRIB_LIST  *attrib;
   C_ATTRIB       *field = NULL;
   C_PREPARED_CMD	pGroupCmd, pSimplCmd, pOldCmd;

   CommonStruct->all_sel_der = 0;
   CommonStruct->list_field_sql_addr.remove_all();
   gs_id = lista->getptr_at(5)->resval.rlong;
   //gs_id = (long)lista->getptr_at(4)->resval.rlong;
   indir = CommonStruct->sql_cplx;
   cls_sel = CommonStruct->sql_cls->get_key();	

   // testo se la classe è gruppo
   if (CommonStruct->sql_cplx->get_category() == CAT_GROUP)
   {
      // Ritorna il puntatore a GROUP_LIST della classe //
      if ((Group = CommonStruct->sql_cplx->ptr_group_list()) == NULL)
         { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

      psempl = Group->get_head();
      while(psempl)
      {  
         if ((psempl->get_key() != cls_sel) && 
             (psempl->get_key() != CommonStruct->sql_cplx->get_key()))
         {
            cls_der = psempl->get_key();
            break;
         }
         psempl = Group->get_next();
      }
   }
   civici = gsc_find_class(GS_CURRENT_WRK_SESSION->get_PrjId(), cls_der, 0);
   CommonStruct->ris_sql_civ.remove_all();
   sql_order.clear();
   
   // compila istruzione per trovare elenco gruppi avente un certa sotto-entità
   if (indir->prepare_reldata_where_member(pGroupCmd, OLD) == GS_BAD) return GS_BAD;

   // compila istruzione per trovare elenco delle sotto-entità collegate a una certa gruppo
   if (indir->prepare_reldata_where_key(pSimplCmd, OLD) == GS_BAD) return GS_BAD;

   // compilo le istruzioni di lettura dei dati della classe "civici" da old
   if (civici->prepare_data(pOldCmd, OLD) == GS_BAD) return GS_BAD;

   // restituisce la lista del gruppo 
   // ((<cod.gruppo><cod.classe><cod entità><status=UNMODIFIED>) (...) (...) ...)
   if (gsc_get_reldata(pGroupCmd, cls_sel, gs_id, linkStreetValues) == GS_BAD)
   {
      gsc_ddalert(gsc_msg(85)); // "L'entità non ha collegamenti all'interno della classe gruppo"
      return GS_BAD;
   }
 
   cnt = (linkStreetValues.GetCount() - 2) / 6;
   for (i = 0; i < cnt; i++)
   {  // cerco i gruppi civico collegate alla via
     gs_id = linkStreetValues.getptr_at(i * 6 + 3)->resval.rlong;
	  gsc_get_reldata(pSimplCmd, gs_id, linkCivicValues);

      // estraggo il codice del civico
      k = (linkCivicValues.getptr_at(4)->resval.rint == cls_der) ? 5 : 11; 
                
      if (CommonStruct->ris_sql_civ.get_head() == NULL)
      { 
		  if (gsc_get_data(pOldCmd, linkCivicValues.getptr_at(k)->resval.rlong, CommonStruct->ris_sql_civ) == GS_BAD)         
		  
            { ads_set_tile(dcl_id, _T("error"), gsc_msg(438));  return GS_BAD; }
            // "Link non presente.. "
      }
      else
      {
		  if (gsc_get_data(pOldCmd, linkCivicValues.getptr_at(k)->resval.rlong, tempValues) == GS_BAD)         
		  
            { ads_set_tile(dcl_id, _T("error"), gsc_msg(438));  return GS_BAD; }
            // "Link non presente.. "

		  CommonStruct->ris_sql_civ += gsc_rblistcopy(tempValues.get_head());
      }
   }

	// se ho trovato civici collegati alla via li ordino
   if (CommonStruct->ris_sql_civ.get_head() != NULL) 
   {
      // imposto i campi per la ricerca SQL nell'archivio della classe derivata
      // Ritorna il puntatore alla ATTRIBLIST della classe //
      if ((attrib = civici->ptr_attrib_list()) == NULL)
         { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
      num_fields = attrib->get_count();
      field = (C_ATTRIB*)attrib->get_head();
      while (field)
      {
         wcscpy(strg, field->get_name());                                                 
         if ((nod = new C_STR((int)wcslen(strg) + 1)) == NULL)
             { GS_ERR_COD=eGSOutOfMem; CommonStruct->list_field_sql_addr.remove_all(); return GS_BAD; }
         if (nod->set_name(strg) == GS_BAD)
         { CommonStruct->list_field_sql_addr.remove_all(); return GS_BAD; }
         CommonStruct->list_field_sql_addr.add_tail(nod); 
   
         field = (C_ATTRIB*)attrib->get_next();
      }
      // ricavo il campo chiave della classe derivata
      C_INFO  *pInfo;
      pInfo = civici->ptr_info();
      // ordino la lista in fuzione del campo TEXT
      sql_statement.clear();
      sql_statement += pInfo->key_attrib.get_name();
      sql_statement += _T(" IN (");
      num_pass = (num_fields * 4) + 2;
      cnt = (CommonStruct->ris_sql_civ.GetCount()) / num_pass;
      for (i=0; i<cnt; i++)
      {
         sql_statement += CommonStruct->ris_sql_civ.getptr_at(i * num_pass + 4);
         if (i < cnt - 1)
            sql_statement += _T(",");
      }
      sql_statement += _T(")"); 
      if (CommonStruct->sql_ind == 1)
         sql_order += _T(" TEXT");
      else
      {
         sql_order += _T(" ");
         sql_order += pInfo->key_attrib.get_name();
      }

      if (gsc_find_elements(civici, &CommonStruct->list_field_sql_addr,
                            sql_statement.get_name(), sql_order.get_name(),
         						 CommonStruct, CommonStruct->ris_sql_civ) == GS_BAD)
			return GS_BAD;
   }
   // se ottengo valori della classe derivata dal gruppo principale li visualizzo per 
   // scelte succesive
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   if (gsc_load_dialog(path, &dcl_file) == RTERROR) return GS_BAD;

   // carico la DCL per listare i civici collegati alla strada scelta
   ads_new_dialog(_T("sql_civici_list"), dcl_file, (CLIENTFUNC)NULLCB, &dcl_id);
   if (dcl_id == NULL)
   {
      ads_unload_dialog(dcl_file);
      GS_ERR_COD=eGSAbortDCL; 
      return GS_BAD;
   }

   swprintf(strg, MAX_LEN_SQL_STM + 10, _T("%s   %s"), gsc_msg(362), 
            CommonStruct->sql_cls->get_name()); // "Classe Principale:"
   ads_set_tile(dcl_id, _T("TITOLO1"), strg);

   if (CommonStruct->sql_ind == 1)
   {
      swprintf(strg, MAX_LEN_SQL_STM + 10, _T("%s   %s"), gsc_msg(363),
               CommonStruct->sql_via_text); // "Via scelta:"
      ads_set_tile(dcl_id, _T("TITOLO2"), strg);
   }

   // costruisco la lista di valori e riempio la list box
   ads_start_list(dcl_id, _T("listcivici"), LIST_NEW, 0);
   if (CommonStruct->sql_ind == 1)
   {
      swprintf(strg, MAX_LEN_SQL_STM + 10, _T("%s"), gsc_msg(373));                           // " Solo Via "
      gsc_add_list(strg);
   }

   if (CommonStruct->ris_sql_civ.get_head() != NULL)
   {
      len = MAX_LEN_SQL_STM + 10;
      cnte = CommonStruct->ris_sql_civ.GetCount() - 2;
      cnti = CommonStruct->list_field_sql_addr.get_count();
      passo = (CommonStruct->list_field_sql_addr.get_count() * 4) + 2;
      i = 5;
      CommonStruct->ris_sql_civ.getptr_at(i);

      do
		{
         if (gsc_string_arrange(&i, cnti, &stringa, &CommonStruct->ris_sql_civ, 0) == GS_BAD) return GS_BAD;
         gsc_add_list(stringa);
         for (int k = 0; k < 6; k++) { CommonStruct->ris_sql_civ.get_next(); i++; }
      }
		while (i <= cnte); 
      ads_end_list();

      ads_action_tile(dcl_id, _T("globsel"), (CLIENTFUNC)dcl_filter_globsel);
      ads_client_data_tile(dcl_id, _T("globsel"), CommonStruct);
      ads_action_tile(dcl_id, _T("singsel"), (CLIENTFUNC)dcl_filter_singsel);
      ads_client_data_tile(dcl_id, _T("singsel"), CommonStruct);
      ads_action_tile(dcl_id, _T("listcivici"), (CLIENTFUNC)dcl_address_listcivici);
      ads_client_data_tile(dcl_id, _T("listcivici"), CommonStruct);
      ads_action_tile(dcl_id, _T("accept"), (CLIENTFUNC)dcl_listcivico_accept_ok);
      ads_client_data_tile(dcl_id, _T("accept"), CommonStruct);
      ads_action_tile(dcl_id, _T("cancel"), (CLIENTFUNC)dcl_filter_quitter);
      ads_client_data_tile(dcl_id, _T("cancel"), CommonStruct);
      swprintf(val, 30, _T("%d"), 0);
      ads_set_tile(dcl_id, _T("listcivici"), val);
      ads_mode_tile(dcl_id, _T("listcivici"), MODE_SETFOCUS);
   }
   else 
   {
      ads_end_list();
      ads_set_tile(dcl_id, _T("error"), gsc_msg(616)); // "Nessuna derivata presente"
		ads_action_tile(dcl_id, _T("accept"), (CLIENTFUNC)dcl_listcivico_accept_ok);
      ads_client_data_tile(dcl_id, _T("accept"), CommonStruct);
      ads_action_tile(dcl_id, _T("cancel"), (CLIENTFUNC)dcl_filter_quitter);
      ads_client_data_tile(dcl_id, _T("cancel"), CommonStruct);
      //ads_mode_tile(dcl_id, _T("cancel"), MODE_SETFOCUS);
      //ads_mode_tile(dcl_id, _T("accept"), MODE_DISABLE);
   }
   ads_action_tile(dcl_id, _T("help"), (CLIENTFUNC)dcl_listcivico_help);

   ads_start_dialog(dcl_id, &status); // LANCIA LA DIALOG-BOX 
   ads_unload_dialog(dcl_file); 

   if (status == DLGOK) return GS_GOOD;
   else return GS_CAN;
}


/*********************************************************************/
/*.doc gsc_do_sql_addr <external> */
/*+
  Questa funzione imposta le condizioni spaziali per la ricerca del 
  territorio da indirizzo.
  Parametri:
  int cls  :          classe di interesse
  int sub  :          sottoclasse relativa
  TCHAR *condition  : condizione di sql impostata

  La funzione restituisce GS_GOOD se va a buon fine altrimenti GS_BAD.
-*/  
/*********************************************************************/
int gsc_do_sql_addr(int cls, int sub, TCHAR *cond)
{
   C_STRING   ReportFile;
   C_STR_LIST KeyList;

   ReportFile = GS_CURRENT_WRK_SESSION->get_dir();
   ReportFile += _T("\\CORD");
   ReportFile += cls;
   ReportFile += _T(".TXT");

   // leggo i codici delle entità che soddisfano la query SQL sulla tabella OLD
   if (gsc_getKeyListFromASISQLonOld(cls, sub, cond, KeyList) == GS_BAD) return GS_BAD;

   if (gsc_extractFromKeyList(cls, sub, KeyList, REPORT + PREVIEW, &ReportFile) == GS_BAD)
      return GS_BAD;

   return GS_GOOD;
}


/****************************************************************************/
/*.doc gsc_calc_min_dist <external> */
/*+
  Questa funzione calcola la minima distanza fra due punti contenuti in due
  liste e restituisce i valori dei due punti.
  Parametri:
  C_RB_LIST  lista_coord[2]:liste valori da ordinare come punti 3D.
  ads_point  p1:1°punto trovato
  ads_point  p2:2°punto trovato

  La funzione restituisce GS_GOOD se va a buon fine altrimenti GS_BAD.
-*/  
/*********************************************************************/
int gsc_calc_min_dist(C_RB_LIST lista_coord[2], ads_point p1, ads_point p2)
{
   double      distmax = 1.0e+25;
   ads_real    dist = 0.0;
   int         i, j, cnt[2];
   
   cnt[0] = ((lista_coord[0].GetCount()-2)); 
   cnt[1] = ((lista_coord[1].GetCount()-2));

   for (i=0; i<cnt[0]; i++)
   {
      for (j=0; j<cnt[1]; j++)
      {
         dist = gsc_dist(lista_coord[0].getptr_at(i+2)->resval.rpoint,
                             lista_coord[1].getptr_at(j+2)->resval.rpoint);
         if (dist < distmax) { 
            distmax = dist;
            ads_point_set(lista_coord[0].getptr_at(i+2)->resval.rpoint, p1);
            ads_point_set(lista_coord[1].getptr_at(j+2)->resval.rpoint, p2); }
      }
   }
   return GS_GOOD;
} 
/****************************************************************************/
/*.doc gsc_calc_pnt_min_max <external> */
/*+
  Questa funzione calcola la minima distanza fra due punti contenuti in due
  liste e restituisce i valori dei due punti.
  Parametri:
  C_RB_LIST  lista_coord[2]:liste valori da ordinare come punti 3D.
  ads_point  p1:1°punto trovato
  ads_point  p2:2°punto trovato

  La funzione restituisce GS_GOOD se va a buon fine altrimenti GS_BAD.
-*/  
/*********************************************************************/
int gsc_calc_pnt_min_max(C_RB_LIST lista_coord[2], ads_point p1, ads_point p2)
{
   int    i, cnt;
   double x, y, xmin, ymin, xmax, ymax; 

   cnt = ((lista_coord[0].GetCount()-2)); 
   xmax = xmin = lista_coord[0].getptr_at(2)->resval.rpoint[0]; 
   ymax = ymin = lista_coord[0].getptr_at(2)->resval.rpoint[1];  

   for (i=0; i<cnt; i++)
   {
      x = lista_coord[0].getptr_at(i+2)->resval.rpoint[0]; 
      y = lista_coord[0].getptr_at(i+2)->resval.rpoint[1];  
      if (x > xmax) xmax = x;
      if (x < xmin) xmin = x;
      if (y > ymax) ymax = y;
      if (y < ymin) ymin = y;
   }

   p1[X] = xmin;
   p1[Y] = ymin;
   p1[Z] = 0.0;
   p2[X] = xmax;
   p2[Y] = ymax;
   p2[Z] = 0.0;

   return GS_GOOD;
} 

/****************************************************************************/
/*.doc gsc_zoom_win <external> */
/*+
  Questa funzione effettua uno zoom finestra in base alle coordinate dei punti
  trovati p1,p2 che soddisfacevano all'impostazione di indirizzo.
  Parametri:
  TCHAR     *filep: nome del file che contiene i punti min e max della finestra 
                    ovvero le occorrenze grafiche in caso di selezione sola via.
  ads_point p1,p2: punti della finestra di interesse.

  La funzione restituisce GS_GOOD se va a buon fine altrimenti GS_BAD.
-*/  
/****************************************************************************/
int gsc_zoom_win(TCHAR *filep, ads_point p1, ads_point p2)
{
   FILE      *f1;
   ads_point punto, pi, pf, pn1, pn2, pn3, pn4;
   double    xmin, ymin, xmax, ymax, offset, fattore = 150.00;
   C_RB_LIST punti[2];
   resbuf    res;

   if (p1[X] == 0.0 && p2[X] == 0.0 && p1[Y] == 0.0 && p2[Y] == 0.0) 
   {
      acedGetPoint(NULL, gsc_msg(349), pn1);      // "\nPrimo angolo: "
      ads_getcorner(pn1, gsc_msg(350), pn2);      // "\nSecondo angolo: "
      if(pn1[X] < pn2[X]) { xmin = pn1[X]; xmax = pn2[X]; }
      else { xmax = pn1[X]; xmin = pn2[X]; }
      if(pn1[Y] < pn2[Y]) { ymin = pn1[Y]; ymax = pn2[Y]; }
      else { ymax = pn1[Y]; ymin = pn2[Y]; }
      pn1[X] = xmin;
      pn1[Y] = ymin;
      pn1[Z] = 0.0;
      pn2[X] = xmax;
      pn2[Y] = ymax;             
      pn2[Z] = 0.0; 
   }
   else 
   {
      pn1[X] = p1[X];
      pn1[Y] = p1[Y];
      pn1[Z] = 0.0;
      pn2[X] = p2[X];
      pn2[Y] = p2[Y];
      pn2[Z] = 0.0; 
   }

   pn3[X] = pn2[X];
   pn3[Y] = pn1[Y];
   pn3[Z] = 0.0;
   pn4[X] = pn1[X];
   pn4[Y] = pn2[Y];

   gsc_zoom(pn1, pn2);

   // leggo il valore di VIEWSIZE e parametrizzo il valore delle croci di puntamento
   if (acedGetVar(_T("VIEWSIZE"), &res) != RTNORM)
      { GS_ERR_COD=eGSVarNotDef; return GS_BAD; }
   if (res.restype != RTREAL) 
      { GS_ERR_COD=eGSVarNotDef; return GS_BAD; }

   offset = res.resval.rreal/fattore;

   if ((f1 = gsc_fopen(filep, _T("r"))) == NULL) return GS_BAD;

   while (fwscanf(f1, _T("%lf,%lf"), &punto[X], &punto[Y]) != EOF)
      if ((punto[X] >= pn1[X] && punto[X] <= pn2[X]) &&
          (punto[Y] >= pn1[Y] && punto[Y] <= pn2[Y]))
      {
         pi[X] = punto[X] - offset;
         pi[Y] = punto[Y] + offset;
         pf[X] = punto[X] + offset;
         pf[Y] = punto[Y] - offset;
         // disegno un ramo della croce 
         if (acedGrDraw(pi, pf, 1, 0) != RTNORM) return GS_BAD;
         pi[X] = punto[X] - offset;
         pi[Y] = punto[Y] - offset;
         pf[X] = punto[X] + offset;
         pf[Y] = punto[Y] + offset;
         // disegno l'altro ramo della croce 
         if (acedGrDraw(pi, pf, 1, 0) != RTNORM) return GS_BAD;
      }
   gsc_fclose(f1);

   // disegno il rettangolo sulla finestra su cui ho impostato lo zoom
   if (acedGrDraw(pn1, pn3, 1, 0) != RTNORM) return GS_BAD;
   if (acedGrDraw(pn3, pn2, 1, 0) != RTNORM) return GS_BAD;
   if (acedGrDraw(pn2, pn4, 1, 0) != RTNORM) return GS_BAD;
   if (acedGrDraw(pn4, pn1, 1, 0) != RTNORM) return GS_BAD;

   return GS_GOOD;
}
///////////////////////////////////////////////////////////////////////////
/* 
Questa funzione effettua uno ZOOM finestra ed evidenzia gli elementi trovati
in una precedente selezione mediante croci.
E' invocata senza parametri di passaggio.
*/                                                    
///////////////////////////////////////////////////////////////////////////
int gs_zoom_in(void)
{
   int       ris;
   ads_point p1 = {0.0,0.0,0.0}, p2 = {0.0,0.0,0.0};
   C_STRING  filename;
                                      
   acedRetNil();

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return RTERROR; }

   // imposto il nome del file su cui sono scritte coordinate trovate e lo apro in lettura 
   filename = GS_CURRENT_WRK_SESSION->get_dir();
   filename += _T("\\SELECPNT.TXT");
   if (gsc_path_conv(filename) == GS_BAD) return GS_BAD;
   
   // Invoco la funzione che effettua lo 'zoom'
   ris = gsc_zoom_win(filename.get_name(), p1, p2);

   if (ris == GS_CAN) { acedRetNil(); return RTNORM; }
   if (ris != GS_GOOD) { GS_ERR_COD = eGSErrorExecutingCommand; return RTERROR; }

   acedRetT();
     
   return RTNORM;
}
/*********************************************************************/
/*.doc gsc_lista_classi <external> */
/*+
  Questa funzione riceve la lista delle classi di un progetto e 
  la visualizza in una DCL permettendo all'utente di sceglierne una. 
  Parametri:
  int prj;                          codice progetto;
  C_SINTH_CLASS_LIST *list_class;   lista delle classi;
  Common_Dcl_main_Sql_Addr_Struct *CommonStruct: struttura delle variabili
                                                 'comuni' passate fra moduli.
  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_lista_classi(int prj, C_SINTH_CLASS_LIST *list_class,
                     Common_Dcl_main_Sql_Addr_Struct *CommonStruct)
{
   int            cat, dcl_file, status, flg;
   C_STRING       path, ClsCategoryTypeDescr;
   TCHAR          val[30];   
   ads_hdlg       dcl_id;
   C_STRING       name;
   C_SINTH_CLASS  *pelement, *psubelement;
   C_2LONG_STR    *newelement;
   C_GROUP_LIST   *pGroupList;
   C_CLASS        *GroupClass;  
   C_INT_INT      *pGroup;

   CommonStruct->sql_cls = NULL;
   // costruisco la lista delle classi
   CommonStruct->list_class_to_visual.remove_all();                        
   pelement = (C_SINTH_CLASS *)list_class->get_head();
   while (pelement)
   {
      if ((cat = pelement->get_category()) == CAT_EXTERN) 
      {
         psubelement = (C_SINTH_CLASS *) pelement->ptr_sub_list()->get_head();
         while (psubelement)
         {
            if ((newelement = new (C_2LONG_STR)) != NULL) 
            {
               newelement->set_id(pelement->get_key());      // setto il codice della classe
               newelement->set_id_2(psubelement->get_key()); // setto il codice della sottoclasse
               name = pelement->get_name();                  
               name += _T(" - ");
               name += psubelement->get_name();
               
               if (gsc_getClsCategoryTypeDescr(CAT_SUBCLASS, psubelement->get_type(), ClsCategoryTypeDescr) != GS_GOOD)
                  return GS_BAD;
               name += ClsCategoryTypeDescr;

               newelement->set_name(name.get_name());         // setto il nome della classe
               CommonStruct->list_class_to_visual.add_tail(newelement);
            }
            psubelement = (C_SINTH_CLASS *) pelement->ptr_sub_list()->get_next();
         }
      }
      else
      {
         if ((cat = pelement->get_category()) == CAT_GROUP) 
         {
            if ((GroupClass = GS_CURRENT_WRK_SESSION->find_class(pelement->get_key())) == NULL) 
               return GS_BAD;

            pGroupList = GroupClass->ptr_group_list();
            if (pGroupList->get_count() == 2)
            {
               flg = 0;
               pGroup = (C_INT_INT*) pGroupList->get_head();
               while (pGroup)
               {
                  if (pGroup->get_type() != 1) { flg = 1; break; }
                  pGroup = (C_INT_INT*) pGroup->get_next();
               }

               if ((flg == 0) && ((newelement = new (C_2LONG_STR)) != NULL))
               {
                  newelement->set_id(pelement->get_key());
                  newelement->set_id_2(0);

                  name = pelement->get_name();
                  if (gsc_getClsCategoryTypeDescr(cat, pelement->get_type(), ClsCategoryTypeDescr) != GS_GOOD)
                     return GS_BAD;
                  name += ClsCategoryTypeDescr;

                  newelement->set_name(name.get_name());
                  CommonStruct->list_class_to_visual.add_tail(newelement);
               }
            }   
         }
         else
         {
            if ((newelement = new (C_2LONG_STR)) != NULL)
            {
               newelement->set_id(pelement->get_key());
               newelement->set_id_2(0);

               name = pelement->get_name();
               if (gsc_getClsCategoryTypeDescr(cat, pelement->get_type(), ClsCategoryTypeDescr) != GS_GOOD)
                  return GS_BAD;
               name += _T("\t");
               name += ClsCategoryTypeDescr;

               newelement->set_name(name.get_name());
               CommonStruct->list_class_to_visual.add_tail(newelement);
            }
         }
      }

      pelement = (C_SINTH_CLASS *) list_class->get_next();
   }
    
   // ordino la lista degli elementi per nome classe
   CommonStruct->list_class_to_visual.sort_name();

   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   if (gsc_load_dialog(path, &dcl_file) != RTNORM) return GS_BAD;

   // carico la DCL per listare le classi
   ads_new_dialog(_T("sql_class_list"), dcl_file, (CLIENTFUNC)NULLCB, &dcl_id);
   if (dcl_id == NULL)
   { ads_unload_dialog(dcl_file); GS_ERR_COD=eGSAbortDCL; return GS_BAD; }
                       
   // visualizzo la lista sulla DCL aperta
   if (gsc_visual_list(dcl_id, _T("listclass"), CommonStruct->list_class_to_visual) == NULL) return GS_BAD;

   // cambiarla con quella della lista civici
   ads_action_tile(dcl_id, _T("listclass"), (CLIENTFUNC) dcl_address_listclass);
   ads_client_data_tile(dcl_id, _T("listclass"), CommonStruct);
   ads_action_tile(dcl_id, _T("accept"),    (CLIENTFUNC) dcl_listclass_accept_ok);
   ads_client_data_tile(dcl_id, _T("accept"), CommonStruct);
   ads_action_tile(dcl_id, _T("cancel"),    (CLIENTFUNC) dcl_filter_quitter);
   ads_client_data_tile(dcl_id, _T("cancel"), CommonStruct);
   ads_action_tile(dcl_id, _T("help"),      (CLIENTFUNC) dcl_listclass_help);

   swprintf(val, 30, _T("%d"), 0);
   ads_set_tile(dcl_id, _T("listclass"), val);
   ads_mode_tile(dcl_id, _T("listclass"), MODE_SETFOCUS);

   ads_start_dialog(dcl_id, &status); // LANCIA LA DIALOG-BOX 
   ads_unload_dialog(dcl_file);
    
   if (status == DLGOK) return GS_GOOD;
   else return GS_CAN;
} 


/*********************************************************************/
/*.doc gsc_visual_list <internal>                                    */
/*+
  Questa funzione riceve una lista C_2LONG_STR_LIST e la visualizza in una
  DCL. 
  Parametri:
  ads_hdlg dcl_id : identificatore della dcl dove visualizzare la lista
  TCHAR *namelist :  puntatore a stringa che contiene il nome del controllo DCL di tipo
                     list-box
  C_2LONG_STR_LIST &list_class_to_visual : lista da visualizzare

  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_visual_list(ads_hdlg dcl_id, TCHAR *namelist, C_2LONG_STR_LIST &list_class_to_visual)
{
   C_2LONG_STR *punt;

   ads_start_list(dcl_id, namelist, LIST_NEW, 0);

   punt = (C_2LONG_STR *) list_class_to_visual.get_head();
   while(punt)
   {
      gsc_add_list(punt->get_name());
      punt = (C_2LONG_STR *) punt->get_next();
   }

   ads_end_list();

   return GS_GOOD;   
}

/*********************************************************************/
/*.doc gsc_string_arrange <external> */
/*+
  Questa funzione riceve una la posizione di partenza di un elemento in 
  una lista e la visualizza in una unica stringa da visualizzare poi in
  una DCL. 
  Parametri:
  int       *pos  : posizione del valore del primo elemento nella lista;
  int       cntf  : numero di campi scelti con SELECT;
  C_STRING  *strng : puntatore alla stinga risultato;
  C_RB_LIST *lista_val : lista dei valori risultato;
  int       spost : valore di scorrimento orrizzontale;
 
  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_string_arrange(int *pos, int cntf, C_STRING *strng, C_RB_LIST *lista_val, int spost)
{
   int        temp ;
   C_STRING   stringa;

   temp = *pos;
   stringa.clear();

   for (int j=0; j<cntf; j++)
   {
      switch(lista_val->get_cursor()->restype)
      {
         case RTREAL:
            stringa += _T("\t");
            stringa += lista_val->get_cursor()->resval.rreal;
            break;
         case RTSHORT:
            stringa += _T("\t");
            stringa += lista_val->get_cursor()->resval.rint;
            break;
         case RTSTR:
            stringa += _T("\t");
            stringa += lista_val->get_cursor()->resval.rstring;
            break;
         case RTLONG:
            stringa += _T("\t");
            stringa += lista_val->get_cursor()->resval.rlong;
            break;
         case RTNONE: case RTNIL:
            stringa += _T("\t ");
            break;
      }
	   if (j < (cntf-1)) for (int k=0; k<4; k++) { lista_val->get_next(); temp++; }
   }

   strng->set_name(stringa.get_name() + spost);
   *pos = temp;

   return GS_GOOD;
}
/*********************************************************************/
/*.doc gsc_string_confirm <external> */
/*+
  Questa funzione riceve una la posizione di partenza di un elemento in 
  una lista di valori, una lista dei campi di una tabella a cui i valori
  si riferiscono e crea una stringa da visualizzare poi in una DCL. 
  Parametri:
  C_STRING  *strng : puntatore alla stinga risultato;
  C_RB_LIST *lista_val : lista dei valori risultato;

  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_string_confirm(C_STRING *strng, C_RB_LIST *lista_val, int posi, int passo)
{
   presbuf  p = lista_val->getptr_at(posi);
   C_STRING stringa;
   int cont = 1;

   stringa.clear();

   while (p)
   {
      switch(p->restype)
      {
			case RTLB: case RTLE: case RTNONE: case RTNIL:
				break;
         case RTREAL:
				stringa += p->resval.rreal;
            if ((p->rbnext)->restype == RTLE) stringa += GS_LFSTR;
				else stringa += _T(": ");
            break;
         case RTSHORT:
				stringa += p->resval.rint;
            if ((p->rbnext)->restype == RTLE) stringa += GS_LFSTR;
				else stringa += _T(": ");
            break;
         case RTSTR:
				stringa += p->resval.rstring;
            if ((p->rbnext)->restype == RTLE) stringa += GS_LFSTR;
				else stringa += _T(": ");
            break;
         case RTLONG:
				stringa += p->resval.rlong;
            if ((p->rbnext)->restype == RTLE) stringa += GS_LFSTR;
				else stringa += _T(": ");
            break;
      }
      p = p->rbnext;
      if (passo > 0) 
         if (cont < passo) cont++;
         else break;
   }

   strng->set_name(stringa.get_name());

   return GS_GOOD;
}

// ACTION TILE : click su tasto HELP //
static void CALLB dcl_change_fas_help(ads_callback_packet *dcl)
   { gsc_help(IDH_Evidenziazionerisultatifiltro); } 


/****************************************************************************/
/*.doc gsc_impost_win <external> */
/*+
  Questa funzione imposta la finestra individuata dai punti passati p1,p2.
  Parametri:
  ads_point  p1,p2: punti min e max della finestra.
  int        clsp : codice della classe primaria di un gruppo
  int        clsd : codice della classe derivata di un gruppo
                    (0 se classe non gruppo).

  La funzione restituisce GS_GOOD se va a buon fine altrimenti GS_BAD.
-*/  
/*********************************************************************/
int gsc_impost_win(ads_point p1, ads_point p2, int clsp, int clsd)
{
   FILE      *f1 = NULL, *f2 = NULL;
   C_STRING  filename, filepunti;
   C_RB_LIST cond;
   ads_point punto, punto1, pn1, pn2;
   double    offx = 200.00, offy = 150.00, deltax = 0.0, deltay = 0.0;

   pn1[X] = p1[X];
   pn1[Y] = p1[Y];
   pn1[Z] = 0.0;
   pn2[X] = p2[X];
   pn2[Y] = p2[Y];
   pn2[Z] = 0.0;

   // imposto il nome del file su cui scrivere le coordinate trovate e lo apro in scrittura 
   filename = GS_CURRENT_WRK_SESSION->get_dir();
   filename += _T("\\SELECPNT.TXT");
   if (gsc_path_conv(filename) == GS_BAD) return GS_BAD;
   if ((f1 = gsc_fopen(filename, _T("w"))) == NULL) return GS_BAD;
   filepunti = filename;

   if (clsp == 0)
   {  // ho trovato due punti che hanno distanza minima (relazione di gruppo 1:1)
      // scrivo le coordinate dei punti trovati   
      fwprintf(f1, _T("%lf,%lf\n"), pn1[X], pn1[Y]);
      fwprintf(f1, _T("%lf,%lf\n"), pn2[X], pn2[Y]);
      gsc_fclose(f1);
   }                                                              
   else
   {  // ho fatto in generale una selezione globale e pertanto salvo tutte le coordinate
      // delle classi in questione in funzione dei parametri clsp e clsd (se # 0)
      // se da indirizzo ho scelto solo via salvo tutte le coordinate della classe 'VIA'

      filename = GS_CURRENT_WRK_SESSION->get_dir();
      filename += _T("\\CORD");
      filename += clsp;
      filename += _T(".TXT");
		ads_point_clear(punto);
      ads_point_clear(punto1);

      if (gsc_GetAreaFromReport(filename.get_name(), punto, punto1) == GS_BAD) return GS_BAD;

      // in base al tipo di classe che sto trattando ricopio le coordinate nel file 
		// "selecpnt.txt" le coordinate minime e massime per usarle in tempi successivi
      fwprintf(f1, _T("%lf,%lf\n"), punto[X], punto[Y]);
		if ((punto1[X] != 0.0) && (punto[Y] != 0.0))
			fwprintf(f1, _T("%lf,%lf\n"), punto1[X], punto1[Y]);

      // se sto trattando un gruppo ed ho operato la scelta globale sulle
      // sue derivate - collegate salvo la coordinate di tutte le derivate.

      if (clsd != 0)
      {
         filename = GS_CURRENT_WRK_SESSION->get_dir();
         filename += _T("\\CORD");
         filename += clsd;
         filename += _T(".TXT");
			ads_point_clear(punto);
			ads_point_clear(punto1);

			if (gsc_GetAreaFromReport(filename.get_name(), punto, punto1) == GS_BAD) return GS_BAD;

         fwprintf(f1, _T("%lf,%lf\n"), punto[X], punto[Y]);
			if ((punto1[X] != 0.0) && (punto[Y] != 0.0))
				fwprintf(f1, _T("%lf,%lf\n"), punto1[X], punto1[Y]);

		}
      gsc_fclose(f1);
   }

   // controllo che p1 e p2 non coincidano (una sola occorrenza delle via)
   if (pn1 == pn2) 
	{
      pn1[X] = pn1[X] - offx;
      pn1[Y] = pn1[Y] - offy;
      pn2[X] = pn2[X] + 2*offx;
      pn2[Y] = pn2[Y] + 2*offy;
	}
   else
	{ // incremento la finestra minima di un offset su x e y
      deltax = fabs(offx - fabs(pn2[X]-pn1[X])/2.0);
      deltay = fabs(offy - fabs(pn2[Y]-pn1[Y])/2.0);
      pn1[X] = pn1[X] - deltax;

      pn1[Y] = pn1[Y] - deltay;
      pn2[X] = pn2[X] + deltax;
      pn2[Y] = pn2[Y] + deltay;
	}

   // imposto la condizione di query spaziale 
   cond << acutBuildList(RTLB, RTSTR, WINDOW_SPATIAL_COND, RTSTR, _T("crossing"), // window
                         RTPOINT, pn1, RTPOINT, pn2, RTLE, 0); 
   if (ade_qryclear() != RTNORM) return GS_BAD;
   if (ade_qrydefine(GS_EMPTYSTR, GS_EMPTYSTR, GS_EMPTYSTR, _T("Location"), cond.get_head(), GS_EMPTYSTR) == ADE_NULLID)
      { GS_ERR_COD = eGSQryCondNotDef; return GS_BAD; }

    // verifico se esiste la query spaziale in memoria e la memorizzo
	if (gsc_ExistCurrentAdeQry() == GS_GOOD) 
   {
		gsc_ade_qldelquery(ADE_SPATIAL_QRY_NAME);
      if (gsc_save_qry() == GS_BAD) return GS_BAD;
   }

   // chiamo la funzione che effettua lo zoom sulla zona scelta ed evidenzia con croci
   // quanto selezionato

   if (gsc_zoom_win(filepunti.get_name(), pn1, pn2) != GS_GOOD) return GS_BAD;

   return GS_GOOD;
}


/****************************************************************************/
/*.doc gsc_extract_coord <external> */ 
/*+
  Questa funzione imposta il rettangolo di massima per l'estrazione di una
  porzione di territorio da indirizzo.
  Parametri:
  C_RB_LIST  lista :lista codici delle classi di cui leggere le coordinate 
                    nei rispettivi files;
  int all_sel_der: valore che indica se si è operata la selezione globale
                   dei valori trovati per una classe derivata

  La funzione restituisce GS_GOOD se va a buon fine altrimenti GS_BAD.
-*/  
/*********************************************************************/
int gsc_extract_coord(C_RB_LIST *lista, int all_sel_der)
{
   int       tw, tw1, cnt, i;
   ads_point punto, punto1, p1, p2;
   C_STRING  filename;
   C_RB_LIST lista_coord[2];

   cnt = lista->GetCount()-1;
   tw = tw1 = 0;

   for (i=1; i<cnt; i++)                        
   {                                                          
      // imposto il nome del file da cui estrarre le coordinate che mi interessano
      filename = GS_CURRENT_WRK_SESSION->get_dir();
      filename += _T("\\CORD");
      filename += lista->getptr_at(i+1)->resval.rint;
      filename += _T(".TXT");

      if (gsc_GetAreaFromReport(filename.get_name(), punto, punto1) == GS_BAD) return GS_BAD;

		// leggo i dati e costruisco il rettangolo di massima
		lista_coord[i-1] << acutBuildList(RTLB, 0);
		lista_coord[i-1] += acutBuildList(RT3DPOINT, punto, 0);
      if (punto1[X] != 0.0 || punto1[Y] != 0.0)
         lista_coord[i-1] += acutBuildList(RT3DPOINT, punto1, 0);
      lista_coord[i-1] += acutBuildList(RTLE, 0);
   }

   // invoco la funzione che trova la minima distanza
   if (lista->GetCount() > 3)
   {  // opero su un gruppo che mi fornisce una coppia di valori di punti
      if (gsc_calc_min_dist(lista_coord, p1, p2) != GS_GOOD) return GS_BAD;
      if (all_sel_der == 0) tw = tw1 = 0;
      else 
      {
         tw = lista->getptr_at(2)->resval.rint;
         tw1 = lista->getptr_at(3)->resval.rint;
      } 
   } // opero su un gruppo che non fornisce valori per la classe derivata in relazione
     // al valore della primaria scelta
   else
   {
      if (gsc_calc_pnt_min_max(lista_coord, p1, p2) != GS_GOOD) return GS_BAD;
      tw = lista->getptr_at(2)->resval.rint;
   }

   // imposto la finestra con estremi i punti p1 e p2
   if (gsc_impost_win(p1, p2, tw, tw1) != GS_GOOD) return GS_BAD;
                        
   return GS_GOOD;
}