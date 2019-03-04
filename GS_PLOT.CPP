/*************************************************************************
Name: gs_plot.cpp

Module description: File funzioni di base per l'impostazione del plottaggio
            
Author: Caterina Gaeta

**************************************************************************/
      

/*************************************************************************/
/* INCLUDES                                                              */
/*************************************************************************/


#include "stdafx.h"         // MFC core and standard components

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")

#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>
#include <string.h>      /*  per strcat() strcmp()  */
#include <math.h>

#include "rxdefs.h"   
#include "adslib.h"   
#include "adsdlg.h"       // gestione DCL

#include "..\gs_def.h" // definizioni globali
#include "gs_error.h" 
#include "gs_init.h" 
#include "gs_list.h"		 //definizioni per C_STRING
#include "gs_ase.h"       // gestione links
#include "gs_class.h"
#include "gs_utily.h"
#include "gs_resbf.h"
#include "gs_prjct.h"
#include "gs_area.h"
#include "gs_graph.h"
#include "gs_plot.h" 	//definizioni di tipi

#include "d2hMap.h" // doc to help


/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/
/* ------------------------VARIABILI PER IL direttorio------------------ */
//variabile da prendere dal valore di GEOsimAppl::WORKDIR
static C_STRING DCL_FILE;	 //nome e percorso per il file DCL

static C_STRING INIPLOTFILE;  //nome e percorso del file per il caricamento dati

/* ------------------------VARIABILI PER LA FINESTRA------------------ */
static ads_point PT1={0,0,0},PT2={0,0,0}; //angoli della sessione selezionata (valori da GSDDPLOT.INI)

/* ------------------------VARIABILI PER LE UNITA' DI MISURA---------- */
static enum UNITS_SYSTEMS {METRIC, ENGLISH};
static enum MEASURE_UNITS {MM=0,CM,M,KM};
static enum EN_MEASURE_UNITS {POLLICI=0,PIEDI,YARDE,MIGLIA};	//Miglia terrestri
static int  UNITS_SYSTEM=METRIC;										//(valori da GSDDPLOT.INI)
static int  MEASURE_UNIT=M;										//(valori da GSDDPLOT.INI)

static double PLOT_UNITS=1.0; 									//(valori da GSDDPLOT.INI)
static double DRAWING_UNITS=1.0;								//(valori da GSDDPLOT.INI)

/* ---VARIABILI PER LE DIMENSIONI E IL LAYOUT DELL'AREA DI PLOTTAGGIO -------- */
static int PAPER_FIXED_MARGINS=0;		//margine fisso in mm dovuto alla stampante (valori da GSDDPLOT.INI)
static int PAPER_USER_MARGINS=0;	 	//margine tra foglio e disegno in mm =margine tra punto di inizio stampa
								//(valori da GSDDPLOT.INI)
static int     PAPER_MARGINS=0;	//margine utente+margine fisso dovuto alla stampante (calcolato)
static double LARGENESS=0.0;		//larghezza in unità disegno della finestra da plottare (calcolato)
static double HEIGHT=0.0;		   //altezza in unità disegno della finestra da plottare (calcolato)

//dimensioni reali in unità di misura reali - valori calcolati
static double PAPER_LARGENESS=0.0;
static double PAPER_HEIGHT=0.0;

//dimensioni totali disegno in mm compresi i margini - valori calcolati
static double PLOT_LARGENESS=0.0;
static double PLOT_HEIGHT=0.0;
static int    PAPER_LIMITS=0;     //stampa della cornice con le coordinate sui quattro vertici  (valori da GSDDPLOT.INI)
static int    LIMIT_TEXT_HEIGHT=0;//altezza testo limiti (valori da GSDDPLOT.INI)
static int    PAPER_GRID=0;       //stampa dei simboli di griglia  (valori da GSDDPLOT.INI)
static double GRID_STEPX=1;       //passo della griglia per x nell'unità scelta (valori da GSDDPLOT.INI)
static double GRID_STEPY=1;       //passo della griglia per y nell'unità scelta (valori da GSDDPLOT.INI)

enum       PLOT_LAYOUTS {BOTTOM_LEFT, TOP_LEFT, BOTTOM_RIGHT, TOP_RIGHT, CENTERED};
static int PLOT_LAYOUT=BOTTOM_LEFT;   //posizione disegno rispetto al foglio

/* ---VARIABILI PER GLI EVENTUALI CARTIGLIO E LEGENDA ------------------------ */
static        C_STRING CART_NAME;			 //nome cartiglio con il percorso (valori da GSDDPLOT.INI)
static        ads_point CART_POINT;       //valore calcolato  
static double CART_HEIGHT=0.0;		 //dimensioni reali del cartiglio in mm  (valori da GSDDPLOT.INI)
static double CART_BASE=0.0;
static enum   ATTR_TYPES {ATTR_VALUE,ATTR_DATE,ATTR_HOUR,ATTR_SCALE};//tipi di attributi del cartiglio
static short  ATTR_TYPE=ATTR_VALUE;
static        C_STRING CART_ATTR_VALUE;
static        ATTR_LIST USER_ATTR_LIST;   //lista attributi cartiglio


static double LEG_HEIGHT=0.0;		 //dimensioni reali legenda - valori calcolati
static double LEG_BASE=0.0;        //calcolata in base alle dim del cart. e della sessione da plottare
static int    EXIST_LEGEND=0;
static double LEG_TEXT_HEIGHT=0.0;		//altezza testo legenda in mm (valori da GSDDPLOT.INI)
static double LEG_TEXT_INTER_LINE=0.0;	//interlinea (valori da GSDDPLOT.INI)
static double LEG_SPACE_BLOCK=0.0;	   //spazio orizzontale disponibile per un blocco  (valori da GSDDPLOT.INI)
static size_t LEG_SPACE_TEXT=0;            //spazio orizzontale necessario per ogni scritta in numero di caratteri - valore calcolato
static int    LEG_ROWS_NUMBER=0;	         //numero righe necessarie per tutte le voci della legenda - valore calcolato
static int    LEG_AV_ROWS_NUMBER=0;        //numero righe disponibili - valore calcolato
static double LEG_BLOCK_SCALE=1.0;		//scala per i blocchi (valori da GSDDPLOT.INI)
static int    LEG_ENT=1;//raggruppamento nella legenda per entità (=1),raggruppamento nella legenda per blocchi(=0)-valore non variato
static int    LEG_MANUAL=0;               //legenda automatica o manuale (valori da GSDDPLOT.INI)
static        CLASS_LIST EXT_CLASS_LIST; 		//lista delle classi di entità estratte - calcolato


/* ---VARIABILI PER LA TIPOLOGIA (con/senza cartiglio, posizione, ecc..------- */
static enum   PLOT_TYPES {CART_LESS, INT_CART, EXBOTTOM_CART, EXRIGHT_CART, EXBOTTOMLEG_CART, EXRIGHTLEG_CART};
static int    PLOT_TYPE=CART_LESS; //tipo di plottata  (valori da GSDDPLOT.INI)- se non risulta una legenda e PLOT_TYPE=4 oppure 5
				             //viene settato  =2 oppure 3

//static double L_USER = 0.00;  //larghezza foglio se formato utente
//static double H_USER = 0.00;  //altezza   foglio se formato utente

/* ---VARIABILI PER L' ORIENTAMENTO e LE DIMENSIONI DEL FOGLIO --------------- */
//lista dimensioni dei fogli A4, A3, A2, A1, A0	in mm
static double DIMPAPERS[] = {
		l_A4,
		h_A4,
		l_A3,
		h_A3,
		l_A2,
		h_A2,
		l_A1,
		h_A1,
		l_A0,
		h_A0,
      0.00,
      0.00,
    NULL
};

static double UNIT_CONST; //costante di proporzionalità per il calcolo delle misure - calcolato
static enum   PAPER_TYPES {A4=0,A3=2,A2=4,A1=6,A0=8, FOGLIO_UTENTE=10};
static int    PAPER;           //memorizza il tipo di foglio scelto, nel caso di scala ottimizzata
static int    HPAPER, VPAPER;  //memorizza il tipo di foglio più piccolo utilizzabile - calcolato
static int    HUPAPER, VUPAPER; //memorizza il foglio scelto dall'utente a partire dal più piccolo
static enum   PAPER_ORIENTATIONS {HORIZONTAL, VERTICAL};
static int    PAPER_ORIENTATION=HORIZONTAL;	 //valori da GSDDPLOT.INI

/* ---VARIABILI PER LA SCELTA DELLA SCALA ----------------------------- */
static enum   SCALE_TYPES {UNO_CENTO=1, UNO_DUECENTO, UNO_CINQUECENTO, 
			     UNO_MILLE, UNO_DUEMILA, UNO_CINQUEMILA, OTTIMIZZATA, UTENTE};
static int    SCALE_TYPE=UTENTE; //contiene il tipo di scala scelto
static double PLOT_SCALE=1.0; //contiene la costante di proporzionalità per il calcolo delle misure del foglio (valori da GSDDPLOT.INI)

static        C_STRING PLOT_ERRORS; //contiene una stringa con l'elenco degli errori nelle impostazioni dell'utente


/*************************************************************************/
/*  LOCAL FUNCTIONS FORWARD DECLARATIONS                                 */
/*************************************************************************/
int gsddplot_wizard();	

int load_plot_values(const TCHAR *file_name = NULL, double x1=0.0, double y1=0.0, double x2=0.0, double y2=0.0);
int gsc_plot_preview(const TCHAR *file_name, double x1=0.0, double y1=0.0, double x2=0.0, double y2=0.0);

int valid_cart_name(TCHAR *name);
int set_extension(FILE *file);
int set_units(FILE *file);
int set_layout(FILE *file);
int set_cartleg(FILE *file); 
int set_type(FILE *file); 
int set_orientation(FILE *file); 
int set_scale(FILE *file); 
int save_plot_values(const TCHAR *file_name = NULL);
int get_real_value(FILE *file,TCHAR *sez,TCHAR *val,double *dbl);
int get_str_value(FILE *file,TCHAR *sez,TCHAR *val,TCHAR *res);
int get_extension(FILE *file);
int get_units(FILE *file);
int get_layout(FILE *file);
int get_cartleg(FILE *file); 
int get_type(FILE *file); 
int get_orientation(FILE *file); 
int get_scale(FILE *file); 
int init_values();

int geo_init();
int geo_ext(); 
int geo_units();  
int geo_view();
int geo_scale();
int geo_paper();
int geo_type();
int geo_cart_leg();
int geo_cart_attr();
int geo_userpaper();
int geo_cart_leg_correction();
int geo_layout();

int geo_user_dim_paper(int chk = 0);


int geo_user_scale();
int show_plot();
int find_geowork();
double scale_dim(double d);
int calculate_scale(); //solo per la scala ottimizzata
int calculate_draw_dim();
void skip_extern_class(resbuf *&rb);
int valid_class(resbuf *rb);
int insert_subclass_data(presbuf ent,short prj,short ext_cl_index,short cl_index,
                         C_STRING *ext_cl_name, C_STRING *cl_name);
int insert_class_data(presbuf ent,short prj,short ext_cl_index,short cl_index,
                      C_STRING *ext_cl_name, C_STRING *cl_name);
int insert_class(presbuf rt,presbuf ret,short ext_cl_index,short cl_index,
                 C_STRING *ext_cl_name, C_STRING *cl_name);
int calculate_legend_list();
int calculate_legend();
int calculate_leg_ent_space_text();
int calculate_leg_block_rows_number();
int correct_cart_leg_values();

int build_legend(ads_point leg_pt);

int create_gsddplot_layer(TCHAR *name);
int is_new_layer(TCHAR *name);
int is_point();
void get_area();
void dimen_calculate();
void unit_const_calculate();
int paper_scale_calculate();
int initialize_scale_list(ads_hdlg hdlg);
int initialize_paper_list(ads_hdlg hdlg);
int initialize_type_list(ads_hdlg hdlg);
int initialize_layout_list(ads_hdlg hdlg);
int initialize_user_paper_list(ads_hdlg hdlg);


int create_new_layer(TCHAR *name);
int set_default_layer();
int exist_linetype(TCHAR *name);
int exist_style(TCHAR *name);

static void select_ini_plot_file(ads_callback_packet *cpkt);

static void annulla(ads_callback_packet *cpkt);
static void conferma(ads_callback_packet *cpkt);
static void previous(ads_callback_packet *cpkt);
static void get_view(ads_callback_packet *cpkt);
static void get_user_area(ads_callback_packet *cpkt);
static void get_coordinates(ads_callback_packet *cpkt);
static void change_view(ads_callback_packet *cpkt);
static void EnglishUnitsList(ads_callback_packet *cpkt);
static void MetricUnitsList(ads_callback_packet *cpkt);
static void get_measure_unit(ads_callback_packet *cpkt);
static void get_plot_units(ads_callback_packet *cpkt);
static void get_drawing_units(ads_callback_packet *cpkt);
static void get_paper_orientation(ads_callback_packet *cpkt);
static int set_scale1(ads_callback_packet *cpkt);
static int set_scale2(ads_callback_packet *cpkt);
static int set_scale3(ads_callback_packet *cpkt);
static int set_scale4(ads_callback_packet *cpkt);
static int set_scale5(ads_callback_packet *cpkt);
static int set_scale6(ads_callback_packet *cpkt);
static int set_scale7(ads_callback_packet *cpkt);
static int set_scale8(ads_callback_packet *cpkt);

static int set_paper0(ads_callback_packet *cpkt);
static int set_paper1(ads_callback_packet *cpkt);
static int set_paper2(ads_callback_packet *cpkt);
static int set_paper3(ads_callback_packet *cpkt);
static int set_paper4(ads_callback_packet *cpkt);
static int set_paperUtente(ads_callback_packet *cpkt);

static void get_user_largeness(ads_callback_packet *cpkt);
static void get_user_height(ads_callback_packet *cpkt);


static void get_user_scale(ads_callback_packet *cpkt);
static int set_type1(ads_callback_packet *cpkt);
static int set_type2(ads_callback_packet *cpkt);
static int set_type3(ads_callback_packet *cpkt);
static int set_type4(ads_callback_packet *cpkt);
static int set_type5(ads_callback_packet *cpkt);
static int set_type6(ads_callback_packet *cpkt);
static void get_paper_user_margins(ads_callback_packet *cpkt);
static void get_paper_limits(ads_callback_packet *cpkt);
static void get_limit_text_height(ads_callback_packet *cpkt);
static void get_paper_grid(ads_callback_packet *cpkt);
static void get_user_grid_step_x(ads_callback_packet *cpkt);
static void get_user_grid_step_y(ads_callback_packet *cpkt);
static void browse(ads_callback_packet *cpkt);
static void get_cart_attr(ads_callback_packet *cpkt);
static void get_new_attr(ads_callback_packet *cpkt);
static void del_attr(ads_callback_packet *cpkt);
static void get_mod_attr(ads_callback_packet *cpkt);
static void attr_value_option(ads_callback_packet *cpkt);
static void attr_date_option(ads_callback_packet *cpkt);
static void attr_hour_option(ads_callback_packet *cpkt);
static void attr_scale_option(ads_callback_packet *cpkt);
static void insert_value_attr(ads_callback_packet *cpkt);
static void get_manual_attr(ads_callback_packet *cpkt);

static void get_cart_name(ads_callback_packet *cpkt);
static void get_cart_base_height(ads_callback_packet *cpkt);
static void get_leg_text_height(ads_callback_packet *cpkt);
static void get_leg_text_inter_line(ads_callback_packet *cpkt);
static void get_leg_space_block(ads_callback_packet *cpkt);
static void get_leg_space_text(ads_callback_packet *cpkt);
static void get_leg_rows_number(ads_callback_packet *cpkt);
static void get_leg_av_rows_number(ads_callback_packet *cpkt);
static void get_leg_block_scale(ads_callback_packet *cpkt);
static void get_leg_manual(ads_callback_packet *cpkt);
static void get_leg_ent(ads_callback_packet *cpkt);
static void get_leg_block(ads_callback_packet *cpkt);

static int set_userpaper0(ads_callback_packet *cpkt);
static int set_userpaper1(ads_callback_packet *cpkt);
static int set_userpaper2(ads_callback_packet *cpkt);
static int set_userpaper3(ads_callback_packet *cpkt);
static int set_userpaper4(ads_callback_packet *cpkt);
static int set_userpaper(ads_callback_packet *cpkt);

static void set_layout1(ads_callback_packet *cpkt);
static void set_layout2(ads_callback_packet *cpkt);
static void set_layout3(ads_callback_packet *cpkt);
static void set_layout4(ads_callback_packet *cpkt);
static void set_layout5(ads_callback_packet *cpkt);

static void save_ini_plot_file(ads_callback_packet *cpkt);



int gsc_rtrim_double(double *db);

void InsTextStandardTextStyle(ads_point pt, const TCHAR *HText, const TCHAR *Rot,
                              const TCHAR *Text);
void InsTextJustifyRightStandardTextStyle(ads_point pt, const TCHAR *HText, 
                                          const TCHAR *Rot, const TCHAR *Text);

/* ************************Funzioni*********************************** */


/* ******************************************************************* */
//    	funzioni della classe geosim_class e delle sottoclassi
/* ******************************************************************* */
//costruttore
geosim_class::geosim_class() {
		class_color.setByLayer();     //colore della classe 256=DALAYER=DEFAULT
		class_layer = _T("0");     	//piano default=layer "0"
		class_block = _T("GEOSIM");	//blocco default="GEOSIM"
		class_width = 0.0;				//larghezza
   	class_elevation=0.0;				//elevazione
		class_scale=1.0;					//scala
		class_rotation=0.0;				//rotazione
		class_text_height=1.0;			//altezza testo
		class_thickness=0.0;				//spessore
	}

//costruttore
CLASS_LIST_ITEM::CLASS_LIST_ITEM() {
	ext_class_index=0;
	class_index=0;
	class_ref=NULL;
}

//costruttore
CLASS_LIST_ITEM::CLASS_LIST_ITEM(short ext,short cl,TCHAR *ext_name,
                                 TCHAR *cl_name,geosim_class *p)
{
	ext_class_index=ext;
	class_index=cl;
	class_ref=p;
	ext_class_name.set_name(ext_name);
	class_name.set_name(cl_name);
}

//distruttore
CLASS_LIST_ITEM::~CLASS_LIST_ITEM() {
	if (class_ref)
      delete class_ref;
	class_ref=NULL;
}


void geosim_class::write_class_name(ads_point pt, TCHAR* val, C_STRING *name) 
{
	//controlla se esiste il layer della classe ed in caso negativo lo crea	
	create_new_layer(this->class_layer.get_name());

	//imposta il layer di default quello della classe 
	gsc_callCmd(_T("_.LAYER"), RTSTR, _T("_S"), RTSTR, this->class_layer.get_name(),
               RTSTR, GS_EMPTYSTR, 0);

	gsc_callCmd(_T("_.COLOR"), RTSTR, _T("7"), 0);

	//inserimento scritta in pt
   InsTextStandardTextStyle(pt, val, _T("0"), name->get_name()); 
  
   //imposta il layer di default 0	
	set_default_layer();
}

//costruttore
geosim_node::geosim_node(resbuf *rt)
{
   presbuf p;

   if ((p = gsc_CdrAssoc(_T("COLOR"), rt, FALSE)))
      class_color.setResbuf(p);

   if ((p = gsc_CdrAssoc(_T("LAYER"), rt, FALSE)))
      if (p->restype == RTSTR) class_layer = p->resval.rstring;

   if ((p = gsc_CdrAssoc(_T("BLOCK"), rt, FALSE)))
      if (p->restype == RTSTR) class_block = p->resval.rstring;

   if ((p = gsc_CdrAssoc(_T("BLOCK_SCALE"), rt, FALSE)))
      gsc_rb2Dbl(p, &class_scale);

   if ((p = gsc_CdrAssoc(_T("ROTATION"), rt, FALSE)))
      gsc_rb2Dbl(p, &class_rotation);
}

void geosim_node::draw_class_block(ads_point pt, TCHAR* scale, double larg)
{
	//inserimento blocco (se esiste) in  pt con la scala leg_block_scale	 (stessa per X e Y)
	//e la rotazione this->class_rotation
	//controlla se esiste il layer della classe ed in caso negativo lo crea	
	create_new_layer(this->class_layer.get_name());

	//imposta il layer di default quello della classe 
	gsc_callCmd(_T("_.LAYER"), RTSTR, _T("_S"), RTSTR, this->class_layer.get_name(),
               RTSTR, GS_EMPTYSTR, 0);

	//imposta il colore della classe come default
   gsc_set_CurrentColor(this->class_color);

   if (this->class_block.get_name())
		gsc_callCmd(_T("_.INSERT"), RTSTR, this->class_block.get_name() ,RTPOINT, pt,
       				RTSTR, scale, RTSTR, GS_EMPTYSTR, RTREAL, this->class_rotation, 0);
   //imposta il layer di default 0	
	set_default_layer();

}

//costruttore
geosim_text::geosim_text(presbuf rt)
{
   presbuf p;

   if ((p = gsc_CdrAssoc(_T("COLOR"), rt, FALSE)))
      class_color.setResbuf(p);

   if ((p = gsc_CdrAssoc(_T("TEXT_STYLE"), rt, FALSE)))
      if (p->restype == RTSTR) class_text_style = p->resval.rstring;

   if ((p = gsc_CdrAssoc(_T("LAYER"), rt, FALSE)))
      if (p->restype == RTSTR) class_layer = p->resval.rstring;

   if ((p = gsc_CdrAssoc(_T("H_TEXT"), rt, FALSE)))
      gsc_rb2Dbl(p, &class_text_height);
}

void geosim_text::write_class_name(ads_point pt, TCHAR* val, C_STRING *name)
{
	//controlla se esiste il layer della classe ed in caso negativo lo crea	
	create_new_layer(this->class_layer.get_name());

	//imposta il layer di default quello della classe 
	gsc_callCmd(_T("_.LAYER"), RTSTR, _T("_S"), RTSTR, this->class_layer.get_name(),
               RTSTR, GS_EMPTYSTR, 0);

	//imposta il colore della classe come default
   gsc_set_CurrentColor(this->class_color);

   double FixedHText;
   
   //inserimento scritta in pt - (stile, se valido)
	if (gsc_validtextstyle(this->class_text_style.get_name()) &&
       exist_style(this->class_text_style.get_name()))
   {
      // verifico se lo stile di testo ha una altezza fissa
      gsc_get_charact_textstyle(this->class_text_style.get_name(), &FixedHText);

      if (FixedHText == 0) // non è stata impostata altezza fissa
		   gsc_callCmd(_T("_.TEXT"), RTSTR, _T("_Style"), RTSTR, this->class_text_style.get_name(),
	                  RTPOINT, pt, RTSTR, val, RTSTR, _T("0"), RTSTR, name->get_name(), 0);
      else
		   gsc_callCmd(_T("_.TEXT"), RTSTR, _T("_Style"), RTSTR, this->class_text_style.get_name(),
	                  RTPOINT, pt, RTSTR, _T("0"), RTSTR, name->get_name(), 0);
   }
	else
   {
      resbuf rbTxtStyle;

      acedGetVar(_T("TEXTSTYLE"), &rbTxtStyle);

      // verifico se lo stile di testo ha una altezza fissa
      gsc_get_charact_textstyle(rbTxtStyle.resval.rstring, &FixedHText);

      if (FixedHText == 0) // non è stata impostata altezza fissa
   		gsc_callCmd(_T("_.TEXT"), RTPOINT, pt, RTSTR, val, RTSTR, _T("0"), RTSTR,name->get_name(), 0);
      else
   		gsc_callCmd(_T("_.TEXT"), RTPOINT, pt, RTSTR, _T("0"), RTSTR, name->get_name(), 0);
   }
	//imposta il layer di default 0	
	set_default_layer();
}

//costruttore
geosim_pol::geosim_pol(presbuf rt)
{
   presbuf p;

   if ((p = gsc_CdrAssoc(_T("COLOR"), rt, FALSE)))
      class_color.setResbuf(p);

   if ((p = gsc_CdrAssoc(_T("LINE_TYPE"), rt, FALSE)))
      if (p->restype == RTSTR) class_line_type = p->resval.rstring;

   if ((p = gsc_CdrAssoc(_T("LAYER"), rt, FALSE)))
      if (p->restype == RTSTR) class_layer = p->resval.rstring;

   if ((p = gsc_CdrAssoc(_T("WIDTH"), rt, FALSE)))
      gsc_rb2Dbl(p, &class_width);
}

void geosim_pol::draw_class_block(ads_point pt, TCHAR *scale, double larg) 
{ 	
	create_new_layer(this->class_layer.get_name());

	//imposta il layer di default quello della classe 
	gsc_callCmd(_T("_.LAYER"), RTSTR, _T("_S"), RTSTR, this->class_layer.get_name(), 
               RTSTR, GS_EMPTYSTR, 0);

	// imposta il colore della classe come default
   gsc_set_CurrentColor(this->class_color);

	//imposta il tipo linea della classe come default, se è valido ed esiste
	if ((gsc_validlinetype(this->class_line_type.get_name()))&&
       (exist_linetype(this->class_line_type.get_name())))
		gsc_callCmd(_T("_.LINETYPE"), RTSTR, _T("_S"),
                  RTSTR, this->class_line_type.get_name(), RTSTR, GS_EMPTYSTR, 0);

	ads_point pt1, pt2;
	pt1[0] = pt[0] - larg/2;
	pt1[1] = pt[1];
	pt1[2] = 0.0;

   pt2[0] = pt[0] + larg/2;
	pt2[1] = pt[1];     
 	pt2[2] = 0.0;

   double s;
   ads_distof(scale, -1, &s);  //conversione da stringa a double
   double l = (this->class_width) * s;
	gsc_callCmd(_T("_.PLINE"), RTPOINT, pt1, RTSTR, _T("_W"),
					RTREAL, l, RTREAL, l, RTPOINT, pt2, RTSTR, GS_EMPTYSTR, 0);
	//imposta il layer di default 0	
	set_default_layer();
} 

//costruttore
geosim_sup::geosim_sup(presbuf rt)
{
   presbuf p;

   if ((p = gsc_CdrAssoc(_T("COLOR"), rt, FALSE)))
      class_color.setResbuf(p);

   if ((p = gsc_CdrAssoc(_T("HATCH"), rt, FALSE)))
      if (p->restype == RTSTR) class_hatch = p->resval.rstring;

   if ((p = gsc_CdrAssoc(_T("LINE_TYPE"), rt, FALSE)))
      if (p->restype == RTSTR) class_line_type = p->resval.rstring;

   if ((p = gsc_CdrAssoc(_T("LAYER"), rt, FALSE)))
      if (p->restype == RTSTR) class_layer = p->resval.rstring;
}

void geosim_sup::draw_class_block(ads_point pt, TCHAR *scale, double larg)
{ 
	create_new_layer(this->class_layer.get_name());

	//imposta il layer di default quello della classe 
	gsc_callCmd(_T("_.LAYER"), RTSTR, _T("_S"), RTSTR, this->class_layer.get_name(), 
               RTSTR, GS_EMPTYSTR, 0);

	// imposta il colore della classe come default
   gsc_set_CurrentColor(this->class_color);

	//imposta il tipo linea della classe come default, se è valido ed esiste
	if (gsc_validlinetype(this->class_line_type.get_name()) && 
       exist_linetype(this->class_line_type.get_name()))
		gsc_callCmd(_T("_.LINETYPE"), RTSTR, _T("_S"), 
                  RTSTR, this->class_line_type.get_name(), RTSTR, GS_EMPTYSTR, 0);

	ads_point pt1,pt2,pt3,pt4;
	pt1[0] = pt[0] - larg/2;
	pt1[1] = pt[1] - larg/2;
	pt1[2] = 0.0;

   pt2[0] = pt[0] + larg/2;
	pt2[1] = pt1[1];
	pt2[2] = 0.0;

   pt3[0] = pt2[0];
	pt3[1] = pt[1] + larg/2;
	pt3[2] = 0.0;

	pt4[0] = pt1[0];
	pt4[1] = pt3[1];
	pt4[2] = 0.0;

   double s;
   ads_distof(scale, -1, &s);  //conversione da stringa a double
   double l = (this->class_width) * s;
	gsc_callCmd(_T("_.PLINE"), RTPOINT,pt1,RTSTR, _T("_W"),
					RTREAL,l,RTREAL,l,RTPOINT,pt2,RTPOINT,pt3,RTPOINT,pt4,RTPOINT,pt1,
               RTSTR, GS_EMPTYSTR, 0);

   set_default_layer();
}
  
//costruttore
geosim_spag::geosim_spag(presbuf rt)
{
   presbuf p;

   if ((p = gsc_CdrAssoc(_T("COLOR"), rt, FALSE)))
      class_color.setResbuf(p);

   if ((p = gsc_CdrAssoc(_T("LINE_TYPE"), rt, FALSE)))
      if (p->restype == RTSTR) class_line_type = p->resval.rstring;

   if ((p = gsc_CdrAssoc(_T("LAYER"), rt, FALSE)))
      if (p->restype == RTSTR) class_layer = p->resval.rstring;

   if ((p = gsc_CdrAssoc(_T("WIDTH"), rt, FALSE)))
      gsc_rb2Dbl(p, &class_width);
}

void geosim_spag::draw_class_block(ads_point pt, TCHAR *scale, double larg) 
{  
	create_new_layer(this->class_layer.get_name());

	//imposta il layer di default quello della classe 
	gsc_callCmd(_T("_.LAYER"), RTSTR, _T("_S"), RTSTR, this->class_layer.get_name(),
               RTSTR, GS_EMPTYSTR, 0);

	// imposta il colore della classe come default
   gsc_set_CurrentColor(this->class_color);

   //imposta il tipo linea della classe come default, se è valido ed esiste
	if ((gsc_validlinetype(this->class_line_type.get_name()))&&exist_linetype(this->class_line_type.get_name()) )
		gsc_callCmd(_T("_.LINETYPE"), RTSTR, _T("_S"), 
                  RTSTR, this->class_line_type.get_name(), RTSTR, GS_EMPTYSTR, 0);

	ads_point pt1, pt2;
	pt1[0] = pt[0] - larg/2;
	pt1[1] = pt[1];
	pt1[2] = 0.0;

	pt2[0] = pt[0]+larg/2;
	pt2[1] = pt[1];
	pt2[2] = 0.0;

   double s;
   ads_distof(scale, -1, &s);  //conversione da stringa a double
   double l = (this->class_width) * s;
	gsc_callCmd(_T("_.PLINE"), RTPOINT, pt, RTSTR, _T("_W"),
					RTREAL, l, RTREAL, l, RTPOINT, pt2, RTSTR, GS_EMPTYSTR, 0);
	//imposta il layer di default 0	
	set_default_layer();
}


/* ******************************************************************* */
//    	funzioni della classe attr_list e attr_list_item
/* ******************************************************************* */
//costruttore
ATTR_LIST_ITEM::ATTR_LIST_ITEM()
{
	attr_type = ATTR_VALUE;
	attr_value.set_name(GS_EMPTYSTR);
}

//costruttore
ATTR_LIST_ITEM::ATTR_LIST_ITEM(short type, TCHAR *value)
{
   attr_type = type;
	attr_value.set_name(value);
}

/* ******************************************************************* */
//    	valid_cart_name	  --  controlla se il nome e percorso del cartiglio è valido
/* ******************************************************************* */
int valid_cart_name(TCHAR *name)
{
   if (name == NULL) return GS_BAD;

   //controlla se il file name->get_name() esiste
   if (gsc_path_exist(name) == GS_GOOD) return GS_GOOD;

   return GS_BAD;
}

/* ******************************************************************* */
//    	set_extension	  --  memorizza i valori di PT1 e PT2 in GSDDPLOT.INI
/* ******************************************************************* */
int set_extension(FILE *file)
{
	TCHAR val[50];

   swprintf(val, 50, _T("%f"), PT1[X]);
	if (gsc_set_profile(file, _T("Extension"), _T("PT10"), val, 0) == GS_BAD)
   {
    	gsc_close_profile(file); 
    	return GS_BAD; 
   }

   swprintf(val, 50, _T("%f"), PT1[Y]);
	if (gsc_set_profile(file, _T("Extension"), _T("PT11"), val, 0) == GS_BAD)
   {
    	gsc_close_profile(file); 
    	return GS_BAD; 
   }

   swprintf(val, 50, _T("%f"), PT2[X]);
	if (gsc_set_profile(file, _T("Extension"), _T("PT20"), val, 0) == GS_BAD)
   {
    	gsc_close_profile(file); 
    	return GS_BAD; 
   }

   swprintf(val, 50, _T("%f"), PT2[Y]);
	if (gsc_set_profile(file, _T("Extension"), _T("PT21"), val, 0) == GS_BAD)
   {
   	gsc_close_profile(file); 
    	return GS_BAD; 
   }

	return GS_GOOD;
}

/* ******************************************************************* */
//    	set_units	  --  memorizza i valori di 	UNITS_SYSTEM	MEASURE_UNIT
//												PLOT_UNITS		DRAWING_UNITS
//						  in GSDDPLOT.INI
/* ******************************************************************* */
int set_units(FILE *file)
{
	TCHAR val[50];

	//conversione da int a stringa - base 10
	_itow_s(UNITS_SYSTEM, val, 10);
	if (gsc_set_profile(file, _T("Units"), _T("UNITS_SYSTEM"), val, 0) == GS_BAD)
   {
    	gsc_close_profile(file); 
    	return GS_BAD; 
   }

	_itow_s(MEASURE_UNIT, val, 10);
	if (gsc_set_profile(file, _T("Units"), _T("MEASURE_UNIT"), val, 0) == GS_BAD)
   {
    	gsc_close_profile(file); 
    	return GS_BAD; 
   }

   swprintf(val, 50, _T("%f"), PLOT_UNITS);
	if (gsc_set_profile(file, _T("Units"), _T("PLOT_UNITS"), val, 0) == GS_BAD)
   {
    	gsc_close_profile(file); 
    	return GS_BAD; 
   }

   swprintf(val, 50, _T("%f"), DRAWING_UNITS);
	if (gsc_set_profile(file, _T("Units"), _T("DRAWING_UNITS"), val, 0) == GS_BAD)
   {
    	gsc_close_profile(file); 
    	return GS_BAD; 
   }

	return GS_GOOD;
}

/* ******************************************************************* */
//    	set_layout	  --  memorizza i valori di	PAPER_FIXED_MARGINS	PAPER_USER_MARGINS   	
//											PAPER_LIMITS	LIMIT_TEXT_HEIGHT 
//                               PAPER_GRID	GRID_STEPX e Y
//                               PLOT_LAYOUT
//						  in GSDDPLOT.INI
/* ******************************************************************* */
int set_layout(FILE *file)
{
	TCHAR val[50];

	//conversione da int a stringa - base 10
	_itow_s(PAPER_FIXED_MARGINS, val, 10);
	if (gsc_set_profile(file, _T("Layout"), _T("PAPER_FIXED_MARGINS"), val, 0) == GS_BAD)
      { gsc_close_profile(file); return GS_BAD; }

	_itow_s(PAPER_USER_MARGINS, val, 10);
	if (gsc_set_profile(file, _T("Layout"), _T("PAPER_USER_MARGINS"), val, 0) == GS_BAD)
      { gsc_close_profile(file); return GS_BAD; }

	_itow_s(PAPER_LIMITS, val, 10);
	if (gsc_set_profile(file, _T("Layout"), _T("PAPER_LIMITS"), val, 0) == GS_BAD)
      { gsc_close_profile(file); return GS_BAD; }

	_itow_s(LIMIT_TEXT_HEIGHT, val, 10);
	if (gsc_set_profile(file, _T("Layout"), _T("LIMIT_TEXT_HEIGHT"), val, 0) == GS_BAD)
      { gsc_close_profile(file); return GS_BAD; }

	_itow_s(PAPER_GRID, val, 10);
	if (gsc_set_profile(file, _T("Layout"), _T("PAPER_GRID"), val, 0) == GS_BAD)
      { gsc_close_profile(file); return GS_BAD; }

   swprintf(val, 50, _T("%f"), GRID_STEPX);
	if (gsc_set_profile(file, _T("Layout"), _T("GRID_STEPX"), val, 0) == GS_BAD)
      { gsc_close_profile(file); return GS_BAD; } 

   swprintf(val, 50, _T("%f"), GRID_STEPY);
	if (gsc_set_profile(file, _T("Layout"), _T("GRID_STEPY"), val, 0) == GS_BAD)
      { gsc_close_profile(file); return GS_BAD; } 

   _itow_s(PLOT_LAYOUT, val, 10);
	if (gsc_set_profile(file, _T("Layout"), _T("PLOT_LAYOUT"), val, 0) == GS_BAD)
      { gsc_close_profile(file); return GS_BAD; } 

	return GS_GOOD;
}

/* ******************************************************************* */
//    	set_cartleg	  --  memorizza i valori di	CART_NAME	CART_HEIGHT		CART_BASE
//											LEG_TEXT_HEIGHT		LEG_TEXT_INTER_LINE
//											LEG_SPACE_BLOCK		LEG_BLOCK_SCALE		LEG_ENT
//											LEG_MANUAL
//						               in GSDDPLOT.INI
/* ******************************************************************* */
int set_cartleg(FILE *file)
{
	if (CART_NAME.len() == 0)
   {
   	if (gsc_set_profile(file, _T("CartLeg"), _T("CART_NAME"), GS_EMPTYSTR, 0) == GS_BAD)
         { gsc_close_profile(file); return GS_BAD; }
   }
	else
   	if (gsc_set_profile(file, _T("CartLeg"), _T("CART_NAME"), CART_NAME.get_name(), 0) == GS_BAD)
         { gsc_close_profile(file); return GS_BAD; }

	TCHAR val[50];

   swprintf(val, 50, _T("%f"), CART_HEIGHT);
	if (gsc_set_profile(file, _T("CartLeg"), _T("CART_HEIGHT"), val, 0) == GS_BAD)
      { gsc_close_profile(file); return GS_BAD; }

   swprintf(val, 50, _T("%f"), CART_BASE);
	if (gsc_set_profile(file, _T("CartLeg"), _T("CART_BASE"), val, 0) == GS_BAD)
      { gsc_close_profile(file); return GS_BAD; }

   swprintf(val, 50, _T("%f"), LEG_TEXT_HEIGHT);
	if (gsc_set_profile(file, _T("CartLeg"), _T("LEG_TEXT_HEIGHT"), val, 0) == GS_BAD)
      { gsc_close_profile(file); return GS_BAD; }

   swprintf(val, 50, _T("%f"), LEG_TEXT_INTER_LINE);
	if (gsc_set_profile(file, _T("CartLeg"), _T("LEG_TEXT_INTER_LINE"), val, 0) == GS_BAD)
      { gsc_close_profile(file); return GS_BAD; }

   swprintf(val, 50, _T("%f"), LEG_SPACE_BLOCK);
	if (gsc_set_profile(file, _T("CartLeg"), _T("LEG_SPACE_BLOCK"), val, 0) == GS_BAD)
      { gsc_close_profile(file); return GS_BAD; }

   swprintf(val, 50, _T("%f"), LEG_BLOCK_SCALE);
	if (gsc_set_profile(file, _T("CartLeg"), _T("LEG_BLOCK_SCALE"), val, 0) == GS_BAD)
      { gsc_close_profile(file); return GS_BAD; }

	_itow_s(LEG_ENT, val, 10);
	if (gsc_set_profile(file, _T("CartLeg"), _T("LEG_ENT"), val, 0) == GS_BAD)
      { gsc_close_profile(file); return GS_BAD; }

	_itow_s(LEG_MANUAL, val, 10);
	if (gsc_set_profile(file, _T("CartLeg"), _T("LEG_MANUAL"), val, 0) == GS_BAD)
      { gsc_close_profile(file); return GS_BAD; }

	return GS_GOOD;
}

/* ******************************************************************* */
//    	set_type	  --  memorizza il valore di PLOT_TYPE
//						  in GSDDPLOT.INI
/* ******************************************************************* */
int set_type(FILE *file)
{
	TCHAR val[50];

	_itow_s(PLOT_TYPE, val, 10);
	if (gsc_set_profile(file, _T("Type"), _T("PLOT_TYPE"), val, 0) == GS_BAD)
      { gsc_close_profile(file); return GS_BAD; }

	return GS_GOOD;
}

/* ******************************************************************* */
//    	set_orientation	  --  memorizza il valore di PAPER_ORIENTATION
//						  	            in GSDDPLOT.INI
/* ******************************************************************* */
int set_orientation(FILE *file)
{
	TCHAR val[50];

	_itow_s(PAPER_ORIENTATION, val, 10);
	if (gsc_set_profile(file, _T("Orientation"), _T("PAPER_ORIENTATION"), val, 0) == GS_BAD)
      { gsc_close_profile(file); return GS_BAD; }

	return GS_GOOD;
}

/* ******************************************************************* */
//    	set_scale	  --  memorizza il valore di PLOT_SCALE  e di PAPER 
//                         se la scala è OTTIMIZZATA
//						         in GSDDPLOT.INI
/* ******************************************************************* */
int set_scale(FILE *file)
{
	TCHAR val[50];

	_itow_s(SCALE_TYPE, val, 10);
	if (gsc_set_profile(file, _T("Scale"), _T("SCALE_TYPE"), val, 0) == GS_BAD)
      { gsc_close_profile(file); return GS_BAD; }

	if (SCALE_TYPE == OTTIMIZZATA)
   {
   	_itow_s(PAPER, val, 10);
   	if (gsc_set_profile(file, _T("Scale"), _T("PAPER"), val, 0) == GS_BAD)
         { gsc_close_profile(file); return GS_BAD; }
   }

   swprintf(val, 50, _T("%f"), PLOT_SCALE);
  	if (gsc_set_profile(file,  _T("Scale"),  _T("PLOT_SCALE"), val, 0) == GS_BAD)
      { gsc_close_profile(file); return GS_BAD; }

   swprintf(val, 50, _T("%f"), DIMPAPERS[FOGLIO_UTENTE]);
  	if (gsc_set_profile(file,  _T("Scale"),  _T("USERPAPERDIM1"), val, 0) == GS_BAD)
      { gsc_close_profile(file); return GS_BAD; }

   swprintf(val, 50, _T("%f"), DIMPAPERS[FOGLIO_UTENTE+1]);
  	if (gsc_set_profile(file,  _T("Scale"),  _T("USERPAPERDIM2"), val, 0) == GS_BAD)
      { gsc_close_profile(file); return GS_BAD; }
   
   return GS_GOOD;
}

/* ******************************************************************* */
//    	save_plot_values	  --  funzione per registrare 
//						  le variabili globali in GSDDPLOT.INI
/* ******************************************************************* */
int save_plot_values(const TCHAR * file_name)
{
	//OCCORRE SETTARE I VALORI NEL FILE GSDDPLOT.INI
	FILE *file;
   C_STRING path;

   if (file_name == NULL)
   {
	   path = GEOsimAppl::CURRUSRDIR;   //costruisce path per il file .INI
      path += _T("\\");
	   path += _T("GSDDPLOT.INI");		//aggiunge il nome del file .INI
   }
   else
	   path = file_name;		//file .INI scelto dall'utente

   if ((file = gsc_open_profile(path.get_name(), UPDATEABLE)) == NULL)
      return GS_BAD; 

   if (set_extension(file)==GS_BAD)	//valori di PT1 e PT2
   {
      if (gsc_close_profile(file)==GS_BAD) return GS_BAD;//CHIUSURA FILE
      return GS_BAD; 
	}
	if (set_units(file)==GS_BAD)		//VARIABILI PER LE UNITA' DI MISURA
   {
      if (gsc_close_profile(file)==GS_BAD) return GS_BAD; //CHIUSURA FILE
      return GS_BAD; 
	}
	if (set_layout(file)==GS_BAD)		//VARIABILI PER E IL LAYOUT DELL'AREA DI PLOTTAGGIO
   {
      if (gsc_close_profile(file)==GS_BAD) return GS_BAD; //CHIUSURA FILE
      return GS_BAD; 
	}
	if (set_cartleg(file)==GS_BAD)		//VARIABILI PER GLI EVENTUALI CARTIGLIO E LEGENDA
   {
      if (gsc_close_profile(file)==GS_BAD) return GS_BAD; //CHIUSURA FILE
      return GS_BAD; 
	}
	if (set_type(file)==GS_BAD)			//VARIABILI PER IL TIPO DI PLOTTATA
   {
      if (gsc_close_profile(file)==GS_BAD) return GS_BAD; //CHIUSURA FILE
      return GS_BAD; 
	}
	if (set_orientation(file)==GS_BAD)	//VARIABILI PER L' ORIENTAMENTO
   {
      if (gsc_close_profile(file)==GS_BAD) return GS_BAD; //CHIUSURA FILE
      return GS_BAD; 
	}
	if (set_scale(file)==GS_BAD)		//VARIABILI PER LA SCELTA DELLA SCALA
   {
      if (gsc_close_profile(file)==GS_BAD) return GS_BAD; //CHIUSURA FILE
      return GS_BAD; 
	}

   if (gsc_close_profile(file)==GS_BAD) return GS_BAD; //CHIUSURA FILE

	return GS_GOOD;
}

/* ******************************************************************* */
//    	get_real_value	  --  legge un valore reale in GSDDPLOT.INI
//							  di val nella sezione sez e lo memorizza in dbl
/* ******************************************************************* */
int get_real_value(FILE *file, TCHAR *sez, TCHAR *val, double *dbl)
{
	TCHAR *result = NULL;

	if (gsc_get_profile(file,sez,val,&result,0,0) == GS_BAD)
	{
		if (result) { free(result); result = NULL; }
    	return GS_BAD; 	  
	}
	ads_distof(result,-1,dbl);  //conversione da stringa a double
	if (result) { free(result); result = NULL; }
	return GS_GOOD;
}

/*********************************************************/
/*.doc get_int_value                           */
/*+
		Questa funzione ricava il valore intero del parametro "val"
      nella SEZIONE "SEZ" in un file INI
      Parametri:
      FILE         *file;   puntatore al file aperto in lettura
      const TCHAR  *sez;    nome sezione
      const TCHAR  *val;    nome parametro
      int          *int;    valore intero di val (out)

  Restituisce GS_BAD se non riesce a scrivere nel file 
-*/  
/*********************************************************/
int get_int_value(FILE *file, const TCHAR *sez, const TCHAR *val, int *in)
{
	TCHAR *result = NULL;

	if (gsc_get_profile(file,sez,val,&result,0,0) == GS_BAD)
	{
		if (result) { free(result); result = NULL; }
    	return GS_BAD; 	  
	}
	*in = _wtoi(result);	//conversione da stringa a int
	if (result) { free(result); result = NULL; }

	return GS_GOOD;
}


/* ******************************************************************* */
//    	get_str_value	  --  legge una stringa in GSDDPLOT.INI
//							  di val nella sezione sez
/* ******************************************************************* */
int get_str_value(FILE *file, TCHAR *sez, TCHAR *val, C_STRING &res)
{
	TCHAR *result=NULL;

	if (gsc_get_profile(file, sez, val, &result, 0, 0) == GS_BAD)
	{
		if (result) { free(result); result = NULL; }
    	return GS_BAD; 	  
	}

	res = result;
	if (result) { free(result); result = NULL; }

	return GS_GOOD;			
}

/* ******************************************************************* */
//    	get_extension	  --  legge i valori di PT1 e PT2 in GSDDPLOT.INI
/* ******************************************************************* */
int get_extension(FILE *file)
{
	double dbl; 

	if (get_real_value(file, _T("Extension"), _T("PT10"), &dbl)==GS_BAD) //valore di PT1[0]
		return GS_BAD;

	PT1[0]=dbl;
   //tronca PT1[0] alla seconda cifra decimale
   gsc_rtrim_double(&PT1[0]);
	if (get_real_value(file, _T("Extension"), _T("PT11"), &dbl)==GS_BAD) //valore di PT1[1]
		return GS_BAD;
	
   PT1[1] = dbl;
   //tronca PT1[1] alla seconda cifra decimale
   gsc_rtrim_double(&PT1[1]);
      if (get_real_value(file, _T("Extension"), _T("PT20"), &dbl)==GS_BAD) //valore di PT2[0]
		return GS_BAD;

	PT2[0] = dbl;
   //tronca PT2[0] alla seconda cifra decimale
   gsc_rtrim_double(&PT2[0]);
   
	if (get_real_value(file, _T("Extension"), _T("PT21"), &dbl)==GS_BAD) //valore di PT2[1]
		return GS_BAD;
	PT2[1]=dbl;
   //tronca PT2[1] alla seconda cifra decimale
   gsc_rtrim_double(&PT2[1]);

	return GS_GOOD;
}

/* ******************************************************************* */
//    	get_units	  --  legge i valori di 	UNITS_SYSTEM	MEASURE_UNIT
//												PLOT_UNITS		DRAWING_UNITS
//						  in GSDDPLOT.INI
/* ******************************************************************* */
int get_units(FILE *file)
{
	if (get_int_value(file, _T("Units"), _T("UNITS_SYSTEM"),&UNITS_SYSTEM)==GS_BAD)
		return GS_BAD;
	if (get_int_value(file, _T("Units"), _T("MEASURE_UNIT"),&MEASURE_UNIT)==GS_BAD)
		return GS_BAD;
	if (get_real_value(file, _T("Units"), _T("PLOT_UNITS"),&PLOT_UNITS)==GS_BAD)
		return GS_BAD;
	if (get_real_value(file, _T("Units"), _T("DRAWING_UNITS"),&DRAWING_UNITS)==GS_BAD)
		return GS_BAD;
	return GS_GOOD;
}

/* ******************************************************************* */
//    	get_layout	  --  legge i valori di	PAPER_FIXED_MARGINS	PAPER_USER_MARGINS   	
//											PAPER_LIMITS	LIMIT_TEXT_HEIGHT   PAPER_GRID	
//                               GRID_STEPX e Y
//                                    PLOT_LAYOUT
//						  in GSDDPLOT.INI
/* ******************************************************************* */
int get_layout(FILE *file)
{
	if (get_int_value(file, _T("Layout"), _T("PAPER_FIXED_MARGINS"),&PAPER_FIXED_MARGINS)==GS_BAD)
		return GS_BAD;
	if (get_int_value(file, _T("Layout"), _T("PAPER_USER_MARGINS"),&PAPER_USER_MARGINS)==GS_BAD)
		return GS_BAD;
	if (get_int_value(file, _T("Layout"), _T("PAPER_LIMITS"),&PAPER_LIMITS)==GS_BAD)
		return GS_BAD;
	if (get_int_value(file, _T("Layout"), _T("LIMIT_TEXT_HEIGHT"),&LIMIT_TEXT_HEIGHT)==GS_BAD)
		return GS_BAD;
	if (get_int_value(file, _T("Layout"), _T("PAPER_GRID"),&PAPER_GRID)==GS_BAD)
		return GS_BAD;
	if (get_real_value(file, _T("Layout"), _T("GRID_STEPX"),&GRID_STEPX)==GS_BAD)
		return GS_BAD;
	if (get_real_value(file, _T("Layout"), _T("GRID_STEPY"),&GRID_STEPY)==GS_BAD)
		return GS_BAD;
	if (get_int_value(file, _T("Layout"), _T("PLOT_LAYOUT"),&PLOT_LAYOUT)==GS_BAD)
		return GS_BAD;
	return GS_GOOD;
}

/* ******************************************************************* */
//    	get_cartleg	  --  legge i valori di	CART_NAME	CART_HEIGHT		CART_BASE
//											LEG_TEXT_HEIGHT		LEG_TEXT_INTER_LINE
//											LEG_SPACE_BLOCK		LEG_BLOCK_SCALE		LEG_ENT
//											LEG_MANUAL
//						  in GSDDPLOT.INI
/* ******************************************************************* */
int get_cartleg(FILE *file)
{
	C_STRING rs;
	if (get_str_value(file, _T("CartLeg"), _T("CART_NAME"), rs)==GS_BAD)
		return GS_BAD;
 
   int i=0, valid=0; 
   while (rs.get_chr(i) != _T('\0'))
   {
      if (rs.get_chr(i) != _T(' '))
      {
         valid=1;
         break;
      }
      else
         i++;
   }
   if (valid)
   {
      if (valid_cart_name(rs.get_name())==GS_GOOD)
         CART_NAME = rs;
   }
   else
      CART_NAME.clear();

	if (get_real_value(file, _T("CartLeg"), _T("CART_HEIGHT"), &CART_HEIGHT)==GS_BAD)
		return GS_BAD;
	if (get_real_value(file, _T("CartLeg"), _T("CART_BASE"), &CART_BASE)==GS_BAD)
		return GS_BAD;
	if (get_real_value(file, _T("CartLeg"), _T("LEG_TEXT_HEIGHT"), &LEG_TEXT_HEIGHT)==GS_BAD)
		return GS_BAD;
	if (get_real_value(file, _T("CartLeg"), _T("LEG_TEXT_INTER_LINE"), &LEG_TEXT_INTER_LINE)==GS_BAD)
		return GS_BAD;
	if (get_real_value(file, _T("CartLeg"), _T("LEG_SPACE_BLOCK"), &LEG_SPACE_BLOCK)==GS_BAD)
		return GS_BAD;
	if (get_real_value(file, _T("CartLeg"), _T("LEG_BLOCK_SCALE"), &LEG_BLOCK_SCALE)==GS_BAD)
		return GS_BAD;
	if (get_int_value(file, _T("CartLeg"), _T("LEG_ENT"), &LEG_ENT)==GS_BAD)
		return GS_BAD;
	if (get_int_value(file, _T("CartLeg"), _T("LEG_MANUAL"), &LEG_MANUAL)==GS_BAD)
		return GS_BAD;
	return GS_GOOD;
}

/* ******************************************************************* */
//    	get_type	  --  legge il valore di PLOT_TYPE
//						  in GSDDPLOT.INI
/* ******************************************************************* */
int get_type(FILE *file)
{
	if (get_int_value(file, _T("Type"), _T("PLOT_TYPE"), &PLOT_TYPE)==GS_BAD)
		return GS_BAD;
	return GS_GOOD;
}

/* ******************************************************************* */
//    	get_orientation	  --  legge il valore di PAPER_ORIENTATION
//						  	  in GSDDPLOT.INI
/* ******************************************************************* */
int get_orientation(FILE *file)
{
	if (get_int_value(file, _T("Orientation"), _T("PAPER_ORIENTATION"), &PAPER_ORIENTATION)==GS_BAD)
		return GS_BAD;
	return GS_GOOD;
}

/* ******************************************************************* */
//    	get_scale	  --  legge il valore di PLOT_SCALE
//						  in GSDDPLOT.INI
/* ******************************************************************* */
int get_scale(FILE *file)
{
	if (get_int_value(file, _T("Scale"), _T("SCALE_TYPE"),&SCALE_TYPE)==GS_BAD)
		return GS_BAD;

   double dim;
   PAPER = -1;
   if (SCALE_TYPE==OTTIMIZZATA)
   {
   	if (get_int_value(file, _T("Scale"), _T("PAPER"), &PAPER)==GS_BAD)
   		return GS_BAD;
   }

 	if (get_real_value(file, _T("Scale"), _T("USERPAPERDIM1"), &dim)==GS_BAD)
      dim = 0.00;
   DIMPAPERS[FOGLIO_UTENTE] = dim;
   
 	if (get_real_value(file, _T("Scale"), _T("USERPAPERDIM2"),&dim)==GS_BAD)
      dim = 0.00;
   DIMPAPERS[FOGLIO_UTENTE+1] = dim;

   switch (SCALE_TYPE)
   {
      case UNO_CENTO:
         PLOT_SCALE = 100.00;
         break;
      case UNO_DUECENTO:
         PLOT_SCALE = 200.00;
         break;
      case UNO_CINQUECENTO:
         PLOT_SCALE = 500.00;
         break;
      case UNO_MILLE:
         PLOT_SCALE = 1000.00;
         break;
      case UNO_DUEMILA:
         PLOT_SCALE = 2000.00;
         break;
      case UNO_CINQUEMILA:
         PLOT_SCALE = 5000.00;
         break;
      //case OTTIMIZZATA:
      //   break;
      default: //UTENTE o OTTIMIZZATA (solo come valore di riferimento)
  	      if (get_real_value(file, _T("Scale"), _T("PLOT_SCALE"), &PLOT_SCALE)==GS_BAD)
            return GS_BAD;
         break;
   }

	return GS_GOOD;
}

/* ******************************************************************* */
//    	init_values	  --  funzione inizializzazione variabili globali
/* ******************************************************************* */
int init_values()
{
   //inizializzazione variabili globali per il caso in cui il file .INI non esiste già
   PT1[0]=0;          
   PT1[1]=0;         
   PT2[0]=0;         
   PT2[1]=0;         
   UNITS_SYSTEM=0;        
   MEASURE_UNIT=M;        
   PLOT_UNITS=1;        
   DRAWING_UNITS=1;        
   PAPER_FIXED_MARGINS=0;   
   PAPER_USER_MARGINS=10;       
   PAPER_LIMITS=0;
   LIMIT_TEXT_HEIGHT=0;        
   PAPER_GRID=1;        
   GRID_STEPX=10;
   GRID_STEPY=10;
   PLOT_LAYOUT=CENTERED;      
   CART_NAME.clear();        
   CART_HEIGHT=50;       
   CART_BASE=130;       
   LEG_TEXT_HEIGHT=2;        
   LEG_TEXT_INTER_LINE=4;        
   LEG_SPACE_BLOCK=2;        
   LEG_BLOCK_SCALE=2;        
   LEG_ENT=1;        
   LEG_MANUAL=0;        
   PLOT_TYPE=0;        
   PAPER_ORIENTATION=0;
   SCALE_TYPE=UNO_CINQUECENTO;        
   PLOT_SCALE=500.0;
   PAPER=-1;      
	PAPER_MARGINS=PAPER_USER_MARGINS+PAPER_FIXED_MARGINS;
	EXIST_LEGEND=0;

	//costruisce DCL_FILE per il file DCL                  
   DCL_FILE = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GEOPLOT.DCL");
   
   load_plot_values(); 

   if (PAPER_LIMITS==0)
       LIMIT_TEXT_HEIGHT=0;
	PAPER_MARGINS=PAPER_USER_MARGINS+PAPER_FIXED_MARGINS+LIMIT_TEXT_HEIGHT;
	return GS_GOOD;
}

/* ******************************************************************* */
//       	gsddplot_wizard	  --  funzione per le DCL
/* ******************************************************************* */
int gsddplot_wizard()
{
   /*inizializzazione delle variabili per il controllo delle DCL*/
   int RESULT_DCL_GeoInit             = DLGOK;
   int RESULT_DCL_GeoExt              = DLGOK;
	int RESULT_DCL_GeoUnits            = DLGOK;
	int RESULT_DCL_GeoScale            = DLGOK;
	int RESULT_DCL_GeoType             = DLGOK;
	int RESULT_DCL_GeoCartLeg          = DLGOK;
	int RESULT_DCL_GeoUserPaper        = DLGOK;
	int RESULT_DCL_GeoCartLegCorrection= DLGOK;
	int RESULT_DCL_GeoLayout           = DLGOK;

   int Ok_GeoInit = 0;
   int Ok_GeoExt=0;
	int Ok_GeoUnits=0;
	int Ok_GeoScale=0;
	int Ok_GeoType=0;
	int Ok_GeoCartLeg=0;
	int Ok_GeoUserPaper=0;
   int Ok_GeoLayout=0;
   int Ok_last=0;	 
	do 												  //ciclo per la prima DCL 
	{			
      RESULT_DCL_GeoInit = geo_init();
      if (RESULT_DCL_GeoInit == DLGCANCEL) break;

      do                                    //ciclo seconda DCL
      {
                                               //(coordinate angoli zona da plottare) finchè Ok_GeoExt=1
		   RESULT_DCL_GeoExt=geo_ext(); 	   	  //prima DCL (l'area non è più ridotta a un punto)
		   if (RESULT_DCL_GeoExt==DLGCANCEL) break;
		   else if ((RESULT_DCL_GeoExt==DLGOK)||(RESULT_DCL_GeoExt==DLGSELECT_AREA))
	      {
            if (RESULT_DCL_GeoExt==DLGSELECT_AREA) //se l'utente vuole riselezionare l'area di stampa
		      {
               get_area();							      //prende i due angoli della sessione
				   Ok_GeoExt=1;
				   continue;         						//ricomincia il ciclo della prima DCL (GeoExt)
            }
			   if (is_point())                       //l'utente ha immesso le coordinate di due punti coincidenti
			   {
				   Ok_GeoExt=1;
				   continue;                             //ricomincia il ciclo della prima DCL
			   }
			   do   //ciclo per la terza DCL (GeoUnits)
			   {
               RESULT_DCL_GeoUnits=geo_units();
               if (RESULT_DCL_GeoUnits==DLGCANCEL)
               {
   				   Ok_GeoExt=0;
   				   break;
				   }
				   else if (RESULT_DCL_GeoUnits==DLGOK)
				   {
                  dimen_calculate();				  //calcolo larghezza e altezza della finestra da plottare
                  unit_const_calculate();			  //calcolo costante di proporzionalità per le misure dei fogli
					   do 								  //ciclo per la quarta DCL (GeoScale)
					   {	
                     RESULT_DCL_GeoScale=geo_scale();
                     if (RESULT_DCL_GeoScale==DLGCANCEL)
						   {
							   Ok_GeoExt=0;
							   Ok_GeoUnits=0;
							   break;
						   }
                     else if (RESULT_DCL_GeoScale==DLGOK)
                     {
                        do 							  //ciclo per la quinta DCL (GeoType)
                        {
                           RESULT_DCL_GeoType=geo_type();
		                	   if (RESULT_DCL_GeoType==DLGCANCEL)
                           {
                              Ok_GeoExt=0;
                              Ok_GeoUnits=0;
                              Ok_GeoScale=0;
                              break;
                           }
                           else if (RESULT_DCL_GeoType==DLGOK)
                           {
                              if (SCALE_TYPE==OTTIMIZZATA)
                              {
                                 calculate_scale();
                              }
                                 calculate_draw_dim();
                              do
                              {
                                 RESULT_DCL_GeoCartLeg=geo_cart_leg();  //ciclo sesta DCL (GeoCartLeg)
                          								                        //eventuale richiesta dei file contenenti
                              						                        //cartiglio e legenda e relative misure
                                 if (RESULT_DCL_GeoCartLeg==DLGCANCEL)
			                        {
								            Ok_GeoExt=0;
									         Ok_GeoUnits=0;
			                           Ok_GeoScale=0;
									         Ok_GeoType=0;
									         break;
                                 }
						               else if (RESULT_DCL_GeoCartLeg==DLGOK)
			                        {
										      //calcola le dimensioni del disegno stampato e i 
										      //fogli utilizzabili dopo aver richiesto gli eventuali
										      //cartiglio e legenda e verifica la correttezza dei parametri
                                    PLOT_ERRORS.clear();

                                    if ((!correct_cart_leg_values())||(!(paper_scale_calculate())))
   										   {
                                       do
                                       {
      											   RESULT_DCL_GeoCartLegCorrection=geo_cart_leg_correction();
      											   if (RESULT_DCL_GeoCartLegCorrection==DLGOK)
      											   {
      												   Ok_GeoCartLeg=1;
      												   break;
      											   }
      										   } while (1); 
   										   }
   										   else
   										   {  
                                       do
                                       {  
                                          if (SCALE_TYPE==OTTIMIZZATA)
                                             RESULT_DCL_GeoUserPaper=DLGOK;
                                          else
                                             RESULT_DCL_GeoUserPaper=geo_userpaper();
               		                	   if (RESULT_DCL_GeoUserPaper==DLGCANCEL)
                                          {
                                             Ok_GeoExt=0;
                                             Ok_GeoUnits=0;
      			                     		   Ok_GeoScale=0;
         											   Ok_GeoType=0;
      												   Ok_GeoCartLeg=0;
                                             break;
                                          }
               		                	   if (RESULT_DCL_GeoUserPaper==DLGOK)
                                          {
                                             do 							  //ciclo per la DCL (GeoLayout)
                                             {
                                                RESULT_DCL_GeoLayout=geo_layout();
                     		                	   if (RESULT_DCL_GeoLayout==DLGCANCEL)
                                                {
                                                   Ok_GeoExt=0;
                                                   Ok_GeoUnits=0;
            			                     		   Ok_GeoScale=0;
               											   Ok_GeoType=0;
            												   Ok_GeoCartLeg=0;
                                                   Ok_GeoUserPaper=0;
                                                   break;
                                                }
                     		                	   if (RESULT_DCL_GeoLayout==DLGOK)
                                                {
                                    
                                                   Ok_GeoExt=0;
                                                   Ok_GeoUnits=0;
            			                     		   Ok_GeoScale=0;
               											   Ok_GeoType=0;
            												   Ok_GeoCartLeg=0;
                                                   Ok_GeoUserPaper=0;
               											   Ok_last=1; //spostare sempre nell'ok dell'ultima dcl
                     							         break;
                                                }
                  									   else if (RESULT_DCL_GeoLayout==DLGPREV)
                  									   {
                                                   if (SCALE_TYPE==OTTIMIZZATA)
         				                              {   
         				                                 Ok_GeoCartLeg=1;
                  				                        Ok_GeoUserPaper=0;
                                                      break;
                                                   }
                                                   else
                                                   {
                  				                        Ok_GeoUserPaper=1;
               				                           break;
                                                   }
               			                  	   }
                                             } while (Ok_GeoLayout==1);//fine ciclo GeoLayout
                                          }
            									   else if (RESULT_DCL_GeoUserPaper==DLGPREV)
            									   {
         				                        Ok_GeoCartLeg=1;
         				                        break;
         			                  	   }
                                       } while (Ok_GeoUserPaper==1);//fine ciclo GeoUserPaper
   										   }
   									   }
   									   else if (RESULT_DCL_GeoCartLeg==DLGPREV)
   									   {
				                        Ok_GeoType=1;
				                        break;
			                  	   }
		                  	   }while(Ok_GeoCartLeg==1);
   							   }
   							   else if (RESULT_DCL_GeoType==DLGPREV)
   							   {
   								    Ok_GeoScale=1;
   								    break;
   							   }
   						   }while (Ok_GeoType==1); //fine ciclo per la quinta DCL
   					   }//fine RESULT_DCL_GeoScale==DLGOK
						   else if (RESULT_DCL_GeoScale==DLGPREV)
						   {
							   Ok_GeoUnits=1;
							   break;
						   }
					   }//fine ciclo GeoScale
					   while (Ok_GeoScale==1); //fine ciclo per la quarta DCL
				   }//fine RESULT_DCL_GeoUnits==DLGOK
				   else if (RESULT_DCL_GeoUnits==DLGPREV)
				   {
					   Ok_GeoExt=1;
					   break;
				   }
			   }while (Ok_GeoUnits==1); //fine ciclo terza DCL
	      } //fine if  RESULT_DCL_GeoExt=OK   //fine if Ok per la seconda DCL
   	   else if (RESULT_DCL_GeoExt==DLGPREV) 
         {
            Ok_GeoInit = 1;
            break; 
         }
      }while (Ok_GeoExt==1);//fine ciclo per la seconda DCL 

   }while (Ok_GeoInit == 1);//fine ciclo prima DCL

	if (Ok_last==1) return GS_GOOD;
	else return GS_BAD;
}

/* ******************************************************************* */
//       	gsddplot	  --  funzione principale per la creazione guidata
//                         di un preview di stampa
/* ******************************************************************* */
void gsddplot(void)
{
   int      result = GS_GOOD;
   int      result_GeoInit = DLGOK;
	short    EchoOld, MenuOld;

   GEOsimAppl::CMDLIST.StartCmd();

   // Registra e cambia i valori di CMDECHO e MENUECHO
	set_echo(&EchoOld);
   set_menu(&MenuOld);

   do
   {
      if (GS_CURRENT_WRK_SESSION == NULL)
         { GS_ERR_COD = eGSNotCurrentSession; result=GS_BAD; break; }


      //DA FARE FORSE
      //funzione di lettura da un file esterno di testo per DIMPAPERS (da 0 a 9)
      //i fogli sono pensati in orizzontale per la larghezza e l'altezza

		EXT_CLASS_LIST.remove_all();
      USER_ATTR_LIST.remove_all();


	   if ((init_values())!=GS_GOOD)
	   {                         
         result=GS_BAD; break;
	   }

      //apre la DCL di avvertimento per i valori iniziali
      /*
      if ((result_GeoInit = geo_init()) == RTERROR) { result=GS_BAD; break; }
      else if (result_GeoInit == DLGCANCEL) break;
      */

	   //ripristina i valori di CMDECHO e MENUECHO
	   if (reset_echo(EchoOld) != GS_GOOD) { result=GS_BAD; break; }
      if (reset_menu(MenuOld) != GS_GOOD) { result=GS_BAD; break; }


      //registra e cambia i valori di CMDECHO e MENUECHO
	   if (set_echo(&EchoOld) != GS_GOOD) { result=GS_BAD; break; }
      if (set_menu(&MenuOld) != GS_GOOD) { result=GS_BAD; break; }

      //costruisce la lista di elementi per la legenda EXT_CLASS_LIST
	   if (calculate_legend_list()==GS_GOOD) EXIST_LEGEND=1;
	   else EXIST_LEGEND=0;

	   if (gsddplot_wizard()==GS_GOOD)  //ciclo per le DCL
	   {	
	      gsdelplot(); // Cancella tutto nello spazio carta e scongela il layer GSDDPLOT se esiste

		   if (show_plot()==GS_GOOD)		//se riesce a costruire il preview nello spazio carta, memorizza i valori 
		   {
			   if (save_plot_values()==GS_BAD)	//delle variabili globali in GSDDPLOT.INI
			   {
				   EXT_CLASS_LIST.remove_all();
               USER_ATTR_LIST.remove_all();
               result=GS_BAD; break;
			   }
		   }
		   else 
		   {
         	EXT_CLASS_LIST.remove_all();
            USER_ATTR_LIST.remove_all();
            result=GS_BAD; break;
		   }
	   }
	   else
	   {
   	   EXT_CLASS_LIST.remove_all();
		   USER_ATTR_LIST.remove_all();
         result=GS_GOOD; break;
	   }

	   EXT_CLASS_LIST.remove_all(); 
      USER_ATTR_LIST.remove_all();

      /*
      if ((PLOT_TYPE!=CART_LESS)&&(CART_NAME.comp(NULL)!=0))//inserisce il cartiglio
		   //nome cartiglio, punto inserimento, scala, rotazione
         gsc_callCmd("_.INSERT", RTSTR,CART_NAME.get_name(),
      			   RTPOINT,CART_POINT,RTSTR,"1",RTSTR,"",RTSTR,"0",0);
      */
	}
   while(0);

   // Ripristina i valori di CMDECHO e MENUECHO
   reset_echo(EchoOld);
   reset_menu(MenuOld);
   
   if (result == GS_GOOD)
   {
      if ((PLOT_TYPE!=CART_LESS)&& CART_NAME.get_name() != NULL)//inserisce il cartiglio
		   //nome cartiglio, punto inserimento, scala, rotazione
         gsc_callCmd(_T("_.INSERT"), RTSTR,CART_NAME.get_name(),
      			      RTPOINT,CART_POINT,RTSTR, _T("1"), RTSTR, GS_EMPTYSTR, RTSTR, _T("0"), 0);
   }

   if (result == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();
   else if (result == GS_CAN) return GEOsimAppl::CMDLIST.CancelCmd();

   return GEOsimAppl::CMDLIST.EndCmd();
}

/* ******************************************************************* */
//		gsviewplot
// Passa allo spazio carta per visualizzare l'anteprima di stampa
/* ******************************************************************* */
void gsviewplot(void)
{
   resbuf *rb1=NULL;

   GEOsimAppl::CMDLIST.StartCmd();

	rb1=ads_newrb(RTSHORT);
	rb1->resval.rint = 0;

	if (acedSetVar(_T("TILEMODE"), rb1)!=RTNORM)/*spazio carta*/ 
	   { acutRelRb(rb1); return GEOsimAppl::CMDLIST.ErrorCmd(); }

	acutRelRb(rb1);

	if (gsc_callCmd(_T("_.PSPACE"), 0) != RTNORM) return GEOsimAppl::CMDLIST.ErrorCmd();

   // ZOOM di tutto
   gsc_callCmd(_T("_.ZOOM"), RTSTR, _T("_ALL"), 0);

   return GEOsimAppl::CMDLIST.EndCmd();
}

/* ******************************************************************* */
//		gsexitplot
// Passa allo spazio modello
/* ******************************************************************* */
void gsexitplot(void)
{
   resbuf *rb = NULL;

   GEOsimAppl::CMDLIST.StartCmd();

	rb=ads_newrb(RTSHORT);
	rb->resval.rint = 1;

	if (acedSetVar(_T("TILEMODE"), rb) != RTNORM) // spazio modello
	   { acutRelRb(rb); return GEOsimAppl::CMDLIST.ErrorCmd(); }

	acutRelRb(rb);

   return GEOsimAppl::CMDLIST.EndCmd();
}


/******************************************************************** */
//		gsdelplot
// Cancella la plottata creata nello spazio carta ed passa allo spazio modello. 
/******************************************************************** */
void gsdelplot()
{
   GEOsimAppl::CMDLIST.StartCmd();
   
   //registra e cambia i valori di CMDECHO e MENUECHO
	short EchoOld, MenuOld;

	if (set_echo(&EchoOld)!=GS_GOOD) return GEOsimAppl::CMDLIST.ErrorCmd();
   if (set_menu(&MenuOld)!=GS_GOOD) return GEOsimAppl::CMDLIST.ErrorCmd();

   gsviewplot(); // Entra nello spazio carta 

	// verifica l' esistenza del LAYER di lavoro per la view
	if (!(is_new_layer(_T("GSDDPLOT"))))
   {
		//lo scongela
      if (gsc_callCmd(_T("_.LAYER"), RTSTR, _T("_T"), RTSTR, _T("GSDDPLOT"), RTSTR, GS_EMPTYSTR, 0) != RTNORM)
		   return GEOsimAppl::CMDLIST.ErrorCmd();
      //setta il tipo linea e il colore

      //controlla se esiste il tipo linea "continuous"
      if (exist_linetype(_T("Continuous")))
         if (gsc_callCmd(_T("_.LAYER"), RTSTR, _T("_L"), RTSTR, _T("Continuous"),
                         RTSTR, _T("GSDDPLOT"), RTSTR, GS_EMPTYSTR, 0) != RTNORM)
	   	   return GEOsimAppl::CMDLIST.ErrorCmd();
      if (gsc_callCmd(_T("_.LAYER"), RTSTR, _T("_LW"), RTSTR, _T("Default"),
                      RTSTR, _T("GSDDPLOT"), RTSTR, GS_EMPTYSTR, 0) != RTNORM)
		   return GEOsimAppl::CMDLIST.ErrorCmd();
      if (gsc_callCmd(_T("_.LAYER"), RTSTR, _T("_C"), RTSTR, _T("7"), 
                      RTSTR, _T("GSDDPLOT"), RTSTR, GS_EMPTYSTR, 0) != RTNORM)
		   return GEOsimAppl::CMDLIST.ErrorCmd();
   }
   //CANCELLAZIONE DI TUTTO NELLO SPAZIO CARTA
   if (gsc_callCmd(_T("_.ERASE"), RTSTR, _T("_ALL"), RTSTR, GS_EMPTYSTR, 0) != RTNORM)
      return GEOsimAppl::CMDLIST.ErrorCmd();

   gsexitplot(); // Esce dallo spazio carta
   
	//ripristina i valori di CMDECHO e MENUECHO
	if (reset_echo(EchoOld)!=GS_GOOD) return GEOsimAppl::CMDLIST.ErrorCmd();
   if (reset_menu(MenuOld)!=GS_GOOD) return GEOsimAppl::CMDLIST.ErrorCmd();

	return GEOsimAppl::CMDLIST.EndCmd();
}

/* ******************************************************************* */
//		set_echo
/* ******************************************************************* */
int set_echo(short *val)
{
	struct resbuf ValOld;
   if (acedGetVar(_T("CMDECHO"), &ValOld) != RTNORM)
      return GS_BAD;
   *val=ValOld.resval.rint;

   presbuf new_val;
   if ((new_val = acutBuildList(RTSHORT, 0, 0)) == NULL) return GS_BAD;
   if (acedSetVar(_T("CMDECHO"), new_val) != RTNORM) return GS_BAD;

   return GS_GOOD;
}

/* ******************************************************************* */
//		reset_echo
/* ******************************************************************* */
int reset_echo(short val)
{
   presbuf new_val;
   if ((new_val = acutBuildList(RTSHORT, val, 0)) == NULL) return GS_BAD;
   if (acedSetVar(_T("CMDECHO"), new_val) != RTNORM) return GS_BAD;

   return GS_GOOD;
}

/* ******************************************************************* */
//		set_menu
/* ******************************************************************* */
int set_menu(short *val)
{
	struct resbuf ValOld;
   if (acedGetVar(_T("MENUECHO"), &ValOld) != RTNORM) return GS_BAD;
   *val=ValOld.resval.rint;

   presbuf new_val;
   if ((new_val = acutBuildList(RTSHORT, 2, 0)) == NULL) return GS_BAD;
   if (acedSetVar(_T("MENUECHO"), new_val) != RTNORM) return GS_BAD;

   //controllo da eliminare
   if (acedGetVar(_T("MENUECHO"), &ValOld) != RTNORM) return GS_BAD;

   return GS_GOOD;
}

/* ******************************************************************* */
//		reset_menu
/* ******************************************************************* */
int reset_menu(short val)
{
   presbuf new_val;
   if ((new_val = acutBuildList(RTSHORT, val, 0)) == NULL) return GS_BAD;
   if (acedSetVar(_T("MENUECHO"), new_val) != RTNORM) return GS_BAD;

   //controllo da eliminare
   if (acedGetVar(_T("MENUECHO"), new_val) != RTNORM) return GS_BAD;

   return GS_GOOD;
}

/* ******************************************************************* */
//		show_plot
/* ******************************************************************* */
int show_plot()
{
	struct resbuf ValOld;
   short osmode = 0;
   C_RB_LIST DescrEnt;
   int res = GS_GOOD;
   
   if (acedGetVar(_T("CMDECHO"), &ValOld) != RTNORM) return GS_BAD;
   create_gsddplot_layer(_T("GSDDPLOT"));//se il LAYER GSDDPLOT per gli oggetti nascosti non esiste già, lo crea
	struct resbuf *rb;
	rb=ads_newrb(RTSHORT);
	rb->resval.rint = 0;
	acedSetVar(_T("TILEMODE"), rb);
	acutRelRb(rb);rb=NULL;

   if (acedGetVar(_T("OSMODE"), &ValOld) != RTNORM) return GS_BAD;
   if (ValOld.restype == RTSHORT)
      osmode = ValOld.resval.rint;
   rb=ads_newrb(RTSHORT);
	rb->resval.rint = 0;
	acedSetVar(_T("OSMODE"), rb);
	acutRelRb(rb);rb=NULL;
   if (acedGetVar(_T("OSMODE"), &ValOld) != RTNORM) return GS_BAD;


	C_STRING paper_file;
	paper_file = GEOsimAppl::WORKDIR; 
	//costruzione limiti del disegno
	ads_point reference_point, first_point, second_point, third_point, last_point, cart_first_point, leg_point;
   reference_point[2]=0.0;
   first_point[2]=0.0;
   third_point[2]=0.0;
   last_point[2]=0.0;
   cart_first_point[2]=0.0;
   leg_point[2]=0.0;

   do
   {
   
      switch (PLOT_LAYOUT)   //COSTRUZIONE PUNTO DI RIFERIMENTO
      {
         case BOTTOM_LEFT:
            reference_point[0]=PAPER_MARGINS;
            reference_point[1]=PAPER_MARGINS;
            break;
         case TOP_LEFT:
            reference_point[0]=PAPER_MARGINS;
            if (PAPER_ORIENTATION==HORIZONTAL)
            { 
      		   if (SCALE_TYPE==OTTIMIZZATA)
                  reference_point[1]=DIMPAPERS[PAPER+1]-PLOT_HEIGHT+PAPER_MARGINS;
               else
                  reference_point[1]=DIMPAPERS[HUPAPER+1]-PLOT_HEIGHT+PAPER_MARGINS;
            }
            
            else
            {
      		   if (SCALE_TYPE==OTTIMIZZATA)
                  reference_point[1]=DIMPAPERS[PAPER]-PLOT_HEIGHT+PAPER_MARGINS;
               else
                  reference_point[1]=DIMPAPERS[VUPAPER]-PLOT_HEIGHT+PAPER_MARGINS;
            }
            
            break;
         case BOTTOM_RIGHT:
            reference_point[1]=PAPER_MARGINS;
            if (PAPER_ORIENTATION==HORIZONTAL) 
            {
      		   if (SCALE_TYPE==OTTIMIZZATA)
                  reference_point[0]=DIMPAPERS[PAPER]-PLOT_LARGENESS+PAPER_MARGINS;
               else
                  reference_point[0]=DIMPAPERS[HUPAPER]-PLOT_LARGENESS+PAPER_MARGINS;
            }
            
            else
            {
      		   if (SCALE_TYPE==OTTIMIZZATA)
                  reference_point[0]=DIMPAPERS[PAPER+1]-PLOT_LARGENESS+PAPER_MARGINS;
               else
                  reference_point[0]=DIMPAPERS[VUPAPER+1]-PLOT_LARGENESS+PAPER_MARGINS;
            }
            
            break;
         case TOP_RIGHT:
            if (PAPER_ORIENTATION==HORIZONTAL) 
            {
      		   if (SCALE_TYPE==OTTIMIZZATA)
               {
                  reference_point[0]=DIMPAPERS[PAPER]-PLOT_LARGENESS+PAPER_MARGINS;
                  reference_point[1]=DIMPAPERS[PAPER+1]-PLOT_HEIGHT+PAPER_MARGINS;
               }
               else
               {
                  reference_point[0]=DIMPAPERS[HUPAPER]-PLOT_LARGENESS+PAPER_MARGINS; 
                  reference_point[1]=DIMPAPERS[HUPAPER+1]-PLOT_HEIGHT+PAPER_MARGINS;
               }
            }
            
            else
            {
      		   if (SCALE_TYPE==OTTIMIZZATA)
               {
                  reference_point[0]=DIMPAPERS[PAPER+1]-PLOT_LARGENESS+PAPER_MARGINS;
                  reference_point[1]=DIMPAPERS[PAPER]-PLOT_HEIGHT+PAPER_MARGINS;
               }
               else
               {
                  reference_point[0]=DIMPAPERS[VUPAPER+1]-PLOT_LARGENESS+PAPER_MARGINS;
                  reference_point[1]=DIMPAPERS[VUPAPER]-PLOT_HEIGHT+PAPER_MARGINS;
               }
            }
            
            break;
         case CENTERED:
            if (PAPER_ORIENTATION==HORIZONTAL) 
            {
      		   if (SCALE_TYPE==OTTIMIZZATA)
               {
                  reference_point[0]=(DIMPAPERS[PAPER]-PLOT_LARGENESS)/2+PAPER_MARGINS;
                  reference_point[1]=(DIMPAPERS[PAPER+1]-PLOT_HEIGHT)/2+PAPER_MARGINS;
               }
               else
               {
                  reference_point[0]=(DIMPAPERS[HUPAPER]-PLOT_LARGENESS)/2+PAPER_MARGINS;
                  reference_point[1]=(DIMPAPERS[HUPAPER+1]-PLOT_HEIGHT)/2+PAPER_MARGINS;
               }
            }
            
            else
            {
      		   if (SCALE_TYPE==OTTIMIZZATA)
               {
                  reference_point[0]=(DIMPAPERS[PAPER+1]-PLOT_LARGENESS)/2+PAPER_MARGINS;
                  reference_point[1]=(DIMPAPERS[PAPER]-PLOT_HEIGHT)/2+PAPER_MARGINS;
               }
               else
               {
                  reference_point[0]=(DIMPAPERS[VUPAPER+1]-PLOT_LARGENESS)/2+PAPER_MARGINS;
                  reference_point[1]=(DIMPAPERS[VUPAPER]-PLOT_HEIGHT)/2+PAPER_MARGINS;
               }

            }
            
            break;
         default:
            return GS_BAD;
            break;

      }

      first_point[0]=reference_point[0];                                   
	   first_point[1]=PLOT_HEIGHT-2*PAPER_MARGINS+reference_point[1];

	   second_point[0]=PLOT_LARGENESS-2*PAPER_MARGINS+reference_point[0];
	   second_point[1]=first_point[1];                                   

	   third_point[0]=second_point[0];                                      
	   third_point[1]=reference_point[1];                     

	   last_point[0]=first_point[0];                     
	   last_point[1]=third_point[1];                     

	   
	   //ottiene temp1 e temp2 in una posizione oppurtuna nel caso di sistema di coordinate normale
	   //servono anche per indicare i limiti del disegno

	   //temp1->   *-----------------
	   //    		| 					 |
	   //		    	|					 |
	   //          |					 |
      //          |					 |
	   //          |					 |
	   //          -----------------* <-temp2
	   ads_point temp1={0.0,0.0,0.0}, temp2={0.0,0.0,0.0};
      temp1[0]=PT1[0];
	   temp1[1]=PT1[1];
	   temp2[0]=PT2[0];
	   temp2[1]=PT2[1];
	   if ( !( (temp1[0]<temp2[0])&&(temp1[1]>temp2[1]) ) )
		   if ( (temp1[0]<temp2[0])&&(temp1[1]<temp2[1]) )
		   { 
			   ads_real temp;
			   temp=temp2[1];
			   temp2[1]=temp1[1];
			   temp1[1]=temp;
		   }
		   else if ( (temp1[0]>temp2[0])&&(temp1[1]>temp2[1]) )
		   {
			   ads_real temp;
			   temp=temp2[0];
			   temp2[0]=temp1[0];
			   temp1[0]=temp;
		   }
		   else
		   {
			   ads_point tmp;
			   tmp[0]=temp1[0];
			   tmp[1]=temp1[1];
			   tmp[2]=0.0;

			   temp1[0]=temp2[0];
			   temp1[1]=temp2[1];
			   temp1[2]=0.0;

            temp2[0]=tmp[0];
			   temp2[1]=tmp[1];
			   temp2[2]=0.0;

		   }

      set_default_layer();//definisce il LAYER  0 come corrente per il rettangolo contenente tutto il disegno
      gsc_callCmd(_T("_.LAYER"), RTSTR, _T("_S"), RTSTR, _T("GSDDPLOT"), RTSTR, GS_EMPTYSTR, 0);//definisce il LAYER  per la view come default
	   ads_point cart_second_point={0.0,0.0,0.0}, cart_third_point={0.0,0.0,0.0}, cart_last_point={0.0,0.0,0.0};
	   ads_point leg_point2, leg_point3, leg_point4;
	   ads_point temp3={0.0,0.0,0.0}, temp4={0.0,0.0,0.0};
	   switch (PLOT_TYPE) 
	   {																	  
		   case CART_LESS:		  //SENZA CARTIGLIO
			   //definisce il LAYER per le view
			   gsc_callCmd(_T("_.LAYER"), RTSTR, _T("_S"), RTSTR, _T("GSDDPLOT"), RTSTR, GS_EMPTYSTR, 0);
			   break;
		   case INT_CART:			  //CARTIGLIO INTERNO
			   // inserisce l'altra view
			   cart_first_point[0]=third_point[0]-CART_BASE;
			   cart_first_point[1]=third_point[1]+CART_HEIGHT;
			   cart_first_point[2]=0.0;

            ads_point cart_second_point, cart_third_point;
			   cart_second_point[0]=second_point[0];
			   cart_second_point[1]=cart_first_point[1];
			   cart_second_point[2]=0.0;

			   cart_third_point[0]=cart_first_point[0];
			   cart_third_point[1]=last_point[1];
			   cart_third_point[2]=0.0;

            //viewport poligonale
            //1------------------2
            //|                  |
            //|                  |
            //|                  |
            //|                  |
            //|           c1---- c2
            //|           | 
            //|           | 
            //l_p---------c3

            gsc_callCmd(_T("_.MVIEW"), RTSTR, _T("_P"),
                        RTPOINT,first_point,
                        RTPOINT,second_point,
                        RTPOINT,cart_second_point,
                        RTPOINT,cart_first_point,                                               
                        RTPOINT,cart_third_point,   
                        RTPOINT,last_point,   
                        RTPOINT,first_point,
                        RTSTR, GS_EMPTYSTR,
                        0);
            //ZOOM di tutto
	         gsc_callCmd(_T("_.ZOOM"), RTSTR, _T("_ALL"), 0);
	         gsc_callCmd(_T("_.MSPACE"), 0);
	         //zoom nella view principale 
	         gsc_zoom(temp1, temp2);
	         gsc_callCmd(_T("_.PSPACE"), 0);
	         gsc_callCmd(_T("_.ZOOM"), RTSTR, _T("_ALL"), 0);

            //costruisce i punti temp3 e temp4 per lo zoom nella view, nel 
			   //la def. dei punti temp3 e temp4 non dipende dalla posizione
			   //delle due view rispetto al foglio
			   temp3[0]=temp1[0];
			   temp3[1]=temp2[1];
			   temp2[1]+=scale_dim(CART_HEIGHT);
			   temp4[0]=scale_dim(PLOT_LARGENESS-2*PAPER_MARGINS-CART_BASE)+temp3[0];
			   temp4[1]=temp2[1];

			   //costruzione rettangolo contenente il cartiglio
			   cart_second_point[0]=second_point[0];
			   cart_second_point[1]=cart_first_point[1];
			   cart_third_point[0]=cart_second_point[0];
			   cart_third_point[1]=third_point[1];
			   third_point[1]+=CART_HEIGHT;  //sposta in alto l'angolo in basso a destra del rettangolo principale
			   cart_last_point[0]=cart_first_point[0];
			   cart_last_point[1]=cart_third_point[1];
  			   CART_POINT[0]=third_point[0];
  			   CART_POINT[1]=third_point[1];
			   //definisce il LAYER 0 per il rettangolo del cartiglio
			   set_default_layer();
			   gsc_callCmd(_T("_.LINE"), RTPOINT, cart_first_point,
                        RTPOINT, cart_second_point, RTPOINT, cart_third_point, 
                        RTPOINT, cart_last_point, RTPOINT, cart_first_point,
                        RTSTR, GS_EMPTYSTR, 0);
			   break;
		   case EXBOTTOM_CART:							 //CARTIGLIO ESTERNO IN BASSO
			   third_point[1]+=CART_HEIGHT;   //sposta in alto l'angolo in basso a destra del rettangolo principale
			   //costruzione rettangolo contenente il cartiglio
			   if ((PAPER_LIMITS==1) && (LIMIT_TEXT_HEIGHT!=0))  //5 sett.
            {
   			   cart_first_point[0]=third_point[0]-CART_BASE+2*LIMIT_TEXT_HEIGHT;
   			   cart_first_point[1]=third_point[1]-CART_HEIGHT-2*LIMIT_TEXT_HEIGHT;
   			   cart_second_point[0]=third_point[0]+2*LIMIT_TEXT_HEIGHT;
   			   cart_second_point[1]=cart_first_point[1];
   			   cart_last_point[0]=cart_first_point[0];
   			   cart_last_point[1]=third_point[1]-2*LIMIT_TEXT_HEIGHT;
     			   cart_third_point[0]=cart_second_point[0];
         	   cart_third_point[1]=cart_last_point[1];
     			   CART_POINT[0]=cart_third_point[0];
     			   CART_POINT[1]=cart_third_point[1];
               ads_point t1,t2;
               t1[0]=first_point[0]-2*LIMIT_TEXT_HEIGHT;
               t1[1]=third_point[1]-2*LIMIT_TEXT_HEIGHT;
               t2[0]=third_point[0]+2*LIMIT_TEXT_HEIGHT;
               t2[1]=t1[1];
               t1[2]=0.0;
               t2[2]=0.0;
     			   set_default_layer();
               //divide il disegno dal cartiglio
	   		   gsc_callCmd(_T("_.LINE"), RTPOINT,t1,RTPOINT,t2, RTSTR, GS_EMPTYSTR, 0);
   		   }
            else
            {			
   			   cart_first_point[0]=third_point[0]-CART_BASE;
   			   cart_first_point[1]=third_point[1]-CART_HEIGHT;
   			   cart_second_point[0]=third_point[0];
   			   cart_second_point[1]=cart_first_point[1];
   			   cart_last_point[0]=cart_first_point[0];
     			   cart_last_point[1]=third_point[1];
     			   cart_third_point[0]=cart_second_point[0];
         	   cart_third_point[1]=cart_last_point[1];
     			   CART_POINT[0]=cart_third_point[0];
     			   CART_POINT[1]=cart_third_point[1];
			   }
			   //definisce il LAYER 0 per il rettangolo del cartiglio
			   set_default_layer();
			   gsc_callCmd(_T("_.LINE"), RTPOINT, cart_first_point, 
			               RTPOINT, cart_second_point, RTPOINT, cart_third_point, 
			               RTPOINT, cart_last_point,RTPOINT, cart_first_point,
                        RTSTR, GS_EMPTYSTR, 0);
			   break;
		   case EXRIGHT_CART:      					//CARTIGLIO ESTERNO A DESTRA
			   third_point[0]-=CART_BASE;
			   //costruzione rettangolo contenente il cartiglio
			   if ((PAPER_LIMITS==1) && (LIMIT_TEXT_HEIGHT!=0))  //5 sett.
            {
   			   cart_first_point[0]=third_point[0]+CART_BASE+2*LIMIT_TEXT_HEIGHT;
   			   cart_first_point[1]=third_point[1]+CART_HEIGHT-2*LIMIT_TEXT_HEIGHT;
   			   cart_second_point[0]=cart_first_point[0];
   			   cart_second_point[1]=third_point[1]-2*LIMIT_TEXT_HEIGHT;
   			   cart_last_point[0]=third_point[0]+2*LIMIT_TEXT_HEIGHT;
   			   cart_last_point[1]=cart_first_point[1];
     			   cart_third_point[0]=cart_last_point[0];
         	   cart_third_point[1]=cart_second_point[1];
     			   CART_POINT[0]=cart_first_point[0];
     			   CART_POINT[1]=cart_first_point[1];
               ads_point t1,t2;
               t1[0]=third_point[0]+2*LIMIT_TEXT_HEIGHT;
               t1[1]=first_point[1]+2*LIMIT_TEXT_HEIGHT;
               t2[0]=t1[0];
               t2[1]=third_point[1]-2*LIMIT_TEXT_HEIGHT;
               t1[2]=0.0;
               t2[2]=0.0;
     			   set_default_layer();
               //divide il disegno dal cartiglio
	   		   gsc_callCmd(_T("_.LINE"), RTPOINT, t1, RTPOINT, t2, RTSTR, GS_EMPTYSTR, 0);
			   }
            else
            {			
   			   cart_first_point[0]=third_point[0]+CART_BASE;
   			   cart_first_point[1]=third_point[1]+CART_HEIGHT;
   			   cart_second_point[0]=cart_first_point[0];
   			   cart_second_point[1]=third_point[1];
   			   cart_last_point[0]=third_point[0];
   			   cart_last_point[1]=cart_first_point[1];
     			   cart_third_point[0]=cart_last_point[0];
         	   cart_third_point[1]=cart_second_point[1];
     			   CART_POINT[0]=cart_first_point[0];
     			   CART_POINT[1]=cart_first_point[1];
            }
			   //definisce il LAYER 0 per il rettangolo del cartiglio 
			   set_default_layer();
			   gsc_callCmd(_T("_.LINE"), RTPOINT, cart_first_point, 
			               RTPOINT, cart_second_point, RTPOINT, cart_third_point, 
			               RTPOINT, cart_last_point,RTPOINT, cart_first_point,
                        RTSTR, GS_EMPTYSTR, 0);
			   break;
		   case EXBOTTOMLEG_CART:						//LEGENDA E CARTIGLIO IN BASSO
			   third_point[1]+=CART_HEIGHT;
			   //costruzione rettangolo contenente il cartiglio
			   if ((PAPER_LIMITS==1) && (LIMIT_TEXT_HEIGHT!=0))  //5 sett.
            {
   			   cart_first_point[0]=third_point[0]-CART_BASE+2*LIMIT_TEXT_HEIGHT;
   			   cart_first_point[1]=third_point[1]-CART_HEIGHT-2*LIMIT_TEXT_HEIGHT;
   			   cart_second_point[0]=third_point[0]+2*LIMIT_TEXT_HEIGHT;
   			   cart_second_point[1]=cart_first_point[1];
   			   cart_last_point[0]=cart_first_point[0];
   			   cart_last_point[1]=third_point[1]-2*LIMIT_TEXT_HEIGHT;
     			   cart_third_point[0]=cart_first_point[0]+CART_BASE;
         	   cart_third_point[1]=cart_first_point[1]+CART_HEIGHT;
     			   CART_POINT[0]=cart_third_point[0];
     			   CART_POINT[1]=cart_third_point[1];
   			   leg_point[0]=last_point[0];
   			   leg_point[1]=last_point[1]-2*LIMIT_TEXT_HEIGHT;
   			   leg_point2[0]=leg_point[0];
   			   leg_point2[1]=leg_point[1]+LEG_HEIGHT;
   			   leg_point3[0]=leg_point[0]+LEG_BASE+2*LIMIT_TEXT_HEIGHT;
   			   leg_point3[1]=leg_point2[1];
   			   leg_point4[0]=leg_point3[0];
   			   leg_point4[1]=leg_point[1];
               ads_point t1,t2;
               t1[0]=first_point[0]-2*LIMIT_TEXT_HEIGHT;
               t1[1]=third_point[1]-2*LIMIT_TEXT_HEIGHT;
               t2[0]=third_point[0]+2*LIMIT_TEXT_HEIGHT;
               t2[1]=t1[1];
               t1[2]=0.0;
               t2[2]=0.0;
     			   set_default_layer();
               //divide il disegno dal cartiglio
	   		   gsc_callCmd(_T("_.LINE"), RTPOINT, t1, RTPOINT, t2, RTSTR, GS_EMPTYSTR, 0);
   			   //inserisce il contorno della legenda
   			   gsc_callCmd(_T("_.LINE"), RTPOINT, leg_point3, 
   								RTPOINT, leg_point4, RTSTR, GS_EMPTYSTR, 0);
			   }
            else
            {			
   			   cart_first_point[0]=third_point[0]-CART_BASE;
   			   cart_first_point[1]=third_point[1]-CART_HEIGHT;
   			   cart_second_point[0]=third_point[0];
   			   cart_second_point[1]=cart_first_point[1];
   			   cart_last_point[0]=cart_first_point[0];
   			   cart_last_point[1]=third_point[1];
     			   cart_third_point[0]=cart_first_point[0]+CART_BASE;
         	   cart_third_point[1]=cart_first_point[1]+CART_HEIGHT;
     			   CART_POINT[0]=cart_third_point[0];
     			   CART_POINT[1]=cart_third_point[1];
   			   leg_point[0]=last_point[0];
   			   leg_point[1]=last_point[1];
   			   leg_point2[0]=leg_point[0];
   			   leg_point2[1]=leg_point[1]+LEG_HEIGHT;
   			   leg_point3[0]=leg_point[0]+LEG_BASE;
   			   leg_point3[1]=leg_point2[1];
   			   leg_point4[0]=leg_point3[0];
   			   leg_point4[1]=leg_point[1];
   			   set_default_layer();
   			   //inserisce il contorno della legenda
   			   gsc_callCmd(_T("_.LINE"), RTPOINT, leg_point,
                           RTPOINT,leg_point2, RTPOINT,leg_point3, 
   							   RTPOINT,leg_point4,RTPOINT,leg_point,
                           RTSTR, GS_EMPTYSTR, 0);
            }
			   //definisce il LAYER 0 per il rettangolo del cartiglio
			   set_default_layer();
			   gsc_callCmd(_T("_.LINE"), RTPOINT, cart_first_point, 
			               RTPOINT, cart_second_point, RTPOINT, cart_third_point, 
			               RTPOINT, cart_last_point,RTPOINT, cart_first_point,
                        RTSTR, GS_EMPTYSTR, 0);
            if (LEG_MANUAL!=1)
			      build_legend(leg_point);
			   break;
		   case EXRIGHTLEG_CART:   					//LEGENDA E CARTIGLIO A DESTRA
			   third_point[0]-=CART_BASE;
			   //costruzione rettangolo contenente il cartiglio
			   if ((PAPER_LIMITS==1) && (LIMIT_TEXT_HEIGHT!=0))  //5 sett.
            {
   			   cart_first_point[0]=third_point[0]+CART_BASE+2*LIMIT_TEXT_HEIGHT;
   			   cart_first_point[1]=third_point[1]+CART_HEIGHT-2*LIMIT_TEXT_HEIGHT;
   			   cart_second_point[0]=cart_first_point[0];
   			   cart_second_point[1]=third_point[1]-2*LIMIT_TEXT_HEIGHT;
   			   cart_last_point[0]=third_point[0]+2*LIMIT_TEXT_HEIGHT;
   			   cart_last_point[1]=cart_first_point[1];
     			   cart_third_point[0]=cart_last_point[0];
         	   cart_third_point[1]=cart_second_point[1];
   			   leg_point[0]=cart_last_point[0];
   			   leg_point[1]=cart_last_point[1];
   			   leg_point2[0]=leg_point[0];
   			   leg_point2[1]=leg_point[1]+LEG_HEIGHT+4*LIMIT_TEXT_HEIGHT;
   			   leg_point3[0]=leg_point[0]+LEG_BASE;
   			   leg_point3[1]=leg_point2[1];
   			   leg_point4[0]=leg_point3[0];
   			   leg_point4[1]=leg_point[1];
     			   CART_POINT[0]=cart_first_point[0];
     			   CART_POINT[1]=cart_first_point[1];
               ads_point t1,t2;
               t1[0]=third_point[0]+2*LIMIT_TEXT_HEIGHT;
               t1[1]=first_point[1]+2*LIMIT_TEXT_HEIGHT;
               t2[0]=t1[0];
               t2[1]=third_point[1]-2*LIMIT_TEXT_HEIGHT;
               t1[2]=0.0;
               t2[2]=0.0;
     			   set_default_layer();
               //divide il disegno dal cartiglio
	   		   gsc_callCmd(_T("_.LINE"), RTPOINT, t1, RTPOINT, t2, RTSTR, GS_EMPTYSTR, 0);
			   }
            else
            {			
   			   cart_first_point[0]=third_point[0]+CART_BASE;
   			   cart_first_point[1]=third_point[1]+CART_HEIGHT;
   			   cart_second_point[0]=cart_first_point[0];
   			   cart_second_point[1]=third_point[1];
   			   cart_last_point[0]=third_point[0];
   			   cart_last_point[1]=cart_first_point[1];
     			   cart_third_point[0]=cart_last_point[0];
         	   cart_third_point[1]=cart_second_point[1];
   			   leg_point[0]=cart_last_point[0];
   			   leg_point[1]=cart_last_point[1];
   			   leg_point2[0]=leg_point[0];
   			   leg_point2[1]=leg_point[1]+LEG_HEIGHT;
   			   leg_point3[0]=leg_point[0]+LEG_BASE;
   			   leg_point3[1]=leg_point2[1];
   			   leg_point4[0]=leg_point3[0];
   			   leg_point4[1]=leg_point[1];
     			   CART_POINT[0]=cart_first_point[0];
     			   CART_POINT[1]=cart_first_point[1];
            }
			   //definisce il LAYER 0 per il rettangolo del cartiglio
			   set_default_layer();
			   gsc_callCmd(_T("_.LINE"), RTPOINT, cart_first_point, 
			               RTPOINT, cart_second_point, RTPOINT, cart_third_point,
                        RTPOINT, cart_last_point,
			               RTPOINT, cart_first_point, RTSTR, GS_EMPTYSTR, 0);
			   //inserisce il contorno della legenda
			   gsc_callCmd(_T("_.LINE"), RTPOINT, leg_point, RTPOINT, leg_point2,
                        RTPOINT, leg_point3, 
							   RTPOINT, leg_point4, RTPOINT, leg_point, RTSTR, GS_EMPTYSTR, 0);
            if (LEG_MANUAL!=1)
   			   build_legend(leg_point);
			   break;
	   }

	   //indica i limiti del disegno se richiesto
	   if ((PAPER_LIMITS==1) && (LIMIT_TEXT_HEIGHT!=0))
	   {
		   //definisce il LAYER 0 come default
		   set_default_layer();
		   //vedere se è il caso di richiedere i valori in input
		   double text_height=LIMIT_TEXT_HEIGHT;	//altezza testo
		   double limit_width=2*LIMIT_TEXT_HEIGHT; //CORREGGERE I PUNTI DI INSERIMENTO
		   ads_point margins_first_point, margins_second_point, margins_third_point, margins_last_point;
		   TCHAR text_val[50];	//altezza testo	in stringa
		   TCHAR val[50];
		   ads_point text_pt; //punto di inserimento testi
		   //conversione da double a stringa
		   swprintf(text_val, 50, _T("%f"), text_height);
		   
		   margins_first_point[0]=first_point[0]-limit_width;
		   margins_first_point[1]=first_point[1]+limit_width;
		   margins_second_point[0]=second_point[0]+limit_width;
		   margins_second_point[1]=margins_first_point[1];
		   margins_third_point[0]=margins_second_point[0];

		   margins_first_point[2]=0.0;
		   margins_second_point[2]=0.0;
		   margins_third_point[2]=0.0;
		   margins_last_point[2]=0.0;
         
         //PARTE DA CORREGGERE
		   if ((PLOT_TYPE==INT_CART) ||(PLOT_TYPE==EXBOTTOM_CART)||(PLOT_TYPE==EXBOTTOMLEG_CART))
		      margins_third_point[1]=third_point[1]-CART_HEIGHT-limit_width;
         else
			   margins_third_point[1]=third_point[1]-limit_width;
		   margins_last_point[0]=last_point[0]-limit_width;
		   margins_last_point[1]=margins_third_point[1];
         margins_last_point[2]=0.0;
		   gsc_callCmd(_T("_.LINE"), RTPOINT, margins_first_point, 
                     RTPOINT, margins_second_point, 
					      RTPOINT, margins_third_point, RTPOINT, margins_last_point,
                     RTPOINT, margins_first_point, RTSTR, GS_EMPTYSTR, 0);

		   //inserimento prima coordinata temp1
		   wcscpy(val, _T("x= "));
		   wcscat(val, gsc_tostring(temp1[0]));//conversione da double a stringa
		   text_pt[0]=first_point[0];
		   text_pt[1]=first_point[1]+(limit_width-text_height)/2;            
         InsTextStandardTextStyle(text_pt, text_val, _T("0"), val); 

         //inserimento seconda coordinata temp1
		   wcscpy(val, _T("y= "));
		   wcscat(val, gsc_tostring(temp1[1]));//conversione da double a stringa
		   text_pt[0]=first_point[0]-(limit_width-text_height)/2;
		   text_pt[1]=first_point[1];
         
         InsTextJustifyRightStandardTextStyle(text_pt, text_val, _T("90"), val);
         
         //inserimento prima coordinata temp2
		   wcscpy(val, _T("x= "));
		   wcscat(val,gsc_tostring(temp2[0]));//conversione da double a stringa
		   if ((PLOT_TYPE==EXRIGHT_CART)||(PLOT_TYPE==EXRIGHTLEG_CART))   //5 sett.
   		   text_pt[0]=third_point[0];
   	   else
   		   text_pt[0]=second_point[0];
         if (PLOT_TYPE==INT_CART)
            text_pt[1]=last_point[1]-(limit_width-text_height)/2-text_height;
         else
            text_pt[1]=third_point[1]-(limit_width-text_height)/2-text_height; //5 sett
   	      //text_pt[1]=last_point[1]-(limit_width-text_height)/2-text_height;
         
         InsTextJustifyRightStandardTextStyle(text_pt, text_val, _T("0"), val);
                  
		   //inserimento seconda coordinata temp2
		   wcscpy(val, _T("y= "));
         if (PLOT_TYPE==INT_CART)
   		   wcscat(val,gsc_tostring(temp3[1]));//conversione da double a stringa
         else
		      wcscat(val,gsc_tostring(temp2[1]));//conversione da double a stringa
		   if ((PLOT_TYPE==EXRIGHT_CART)||(PLOT_TYPE==EXRIGHTLEG_CART))   //5 sett.
   		   text_pt[0]=third_point[0]+(limit_width-text_height)/2+text_height;
         else
   		   text_pt[0]=second_point[0]+(limit_width-text_height)/2+text_height;
         if (PLOT_TYPE==INT_CART)
            text_pt[1]=last_point[1];
         else
		      text_pt[1]=third_point[1];  //5 sett
		      //text_pt[1]=last_point[1];
		   
         InsTextStandardTextStyle(text_pt, text_val, _T("90"), val); 

		   //inserimento prima coordinata temp1
         wcscpy(val, _T("x= "));
		   wcscat(val, gsc_tostring(temp1[0]));//conversione da double a stringa
		   text_pt[0]=last_point[0];
		   
         if (PLOT_TYPE==INT_CART)
            text_pt[1]=last_point[1]-(limit_width-text_height)/2-text_height;
         else
            text_pt[1]=third_point[1]-(limit_width-text_height)/2-text_height;//5 sett.
		   //text_pt[1]=last_point[1]-(limit_width-text_height)/2-text_height;
		   
         InsTextStandardTextStyle(text_pt, text_val, _T("0"), val); 

         //inserimento seconda coordinata temp2 (o seconda di temp3 se il cartiglio è int)
		   wcscpy(val, _T("y= "));
         if (PLOT_TYPE==INT_CART)
   		   wcscat(val,gsc_tostring(temp3[1]));//conversione da double a stringa
         else
		      wcscat(val,gsc_tostring(temp2[1]));//conversione da double a stringa
		   text_pt[0]=last_point[0]-(limit_width-text_height)/2;
		   
         if (PLOT_TYPE==INT_CART)
            text_pt[1]=last_point[1];
         else
            text_pt[1]=third_point[1];//5 sett.
		   //text_pt[1]=last_point[1];
		   
         InsTextStandardTextStyle(text_pt, text_val, _T("90"), val); 

		   //inserimento prima coordinata temp2
		   wcscpy(val, _T("x= "));
		   wcscat(val, gsc_tostring(temp2[0]));//conversione da double a stringa
		   
         if ((PLOT_TYPE==EXRIGHT_CART)||(PLOT_TYPE==EXRIGHTLEG_CART))   //5 sett.
   		   text_pt[0]=third_point[0];
         else
   		   text_pt[0]=second_point[0];

		   text_pt[1]=second_point[1]+(limit_width-text_height)/2;

         InsTextJustifyRightStandardTextStyle(text_pt, text_val, _T("0"), val);
                  
		   //inserimento seconda coordinata temp1
		   wcscpy(val, _T("y= "));
		   wcscat(val, gsc_tostring(temp1[1]));//conversione da double a stringa
		   
         if (PLOT_TYPE==INT_CART)
            text_pt[0]=second_point[0]+(limit_width-text_height)/2+text_height;
         else
            text_pt[0]=third_point[0]+(limit_width-text_height)/2+text_height; //5 sett.
		   //text_pt[0]=second_point[0]+(limit_width-text_height)/2+text_height;
		   
		   text_pt[1]=second_point[1];
         
         InsTextJustifyRightStandardTextStyle(text_pt, text_val, _T("90"), val);
	   }

	   //inserimento griglia se richiesto
	   if ((PAPER_GRID==1) &&(GRID_STEPX>0) && (GRID_STEPY>0))
	   {   //CORREGGERE IL DISEGNO DELLA GRIGLIA e i calcoli
		   //definisce il LAYER 0 come default	
		   set_default_layer();
         //passi griglia riportati nello spazio modello
         double stepx = (GRID_STEPX*DRAWING_UNITS)/PLOT_UNITS;
         double stepy = (GRID_STEPY*DRAWING_UNITS)/PLOT_UNITS;
         ads_point p_grid;
         p_grid[2]=0.0;

         //punto riferimento griglia nello spazio modello
         int s,t;
         s=int (temp1[0]/stepx);
         t=int (temp1[1]/stepy);
         if ((s!=0) && (t!=0))  //disegna la griglia se questa non è "troppo grande"
         {
            p_grid[0]=s*stepx;
            p_grid[1]=t*stepy;
            //passi griglia riportati nello spazio carta
            stepx=stepx/DRAWING_UNITS*PLOT_UNITS/PLOT_SCALE*UNIT_CONST;
            stepy=stepy/DRAWING_UNITS*PLOT_UNITS/PLOT_SCALE*UNIT_CONST;

            //differenza tra temp1 ed il punto di riferimento temp1(nello spazio carta)
            double dist=((((p_grid[0]-temp1[0])/DRAWING_UNITS)*PLOT_UNITS)/PLOT_SCALE)*UNIT_CONST;
            //p_grid[0]=x del punto rif corrispondente a temp1   +dist;
            p_grid[0]=first_point[0]+dist;
            dist=((((p_grid[1]-temp1[1])/DRAWING_UNITS)*PLOT_UNITS)/PLOT_SCALE)*UNIT_CONST;
            p_grid[1]=first_point[1]+dist;

   		   int cols=(int)((third_point[0]-p_grid[0])/stepx);
   		   int rows=(int)((p_grid[1]-last_point[1])/stepy);

            if ((rows>0)&&(cols>0))   //disegna la griglia se possibile
            {
               for (int i=0; i<=cols;i++)
      		   {
      			   for (int j=0; j<=rows;j++)
      			   {
      				   ads_point temp;
      				   temp[0]=p_grid[0]+i*stepx;
      				   temp[1]=p_grid[1]-j*stepy;
                     temp[2]=0.0;
      				   ads_point temp1,temp2,temp3,temp4;
                     temp1[2]=0.0;
                     temp2[2]=0.0;
                     temp3[2]=0.0;
                     temp4[2]=0.0;
      				   //CASO INTERNO
      				   if (((temp[0]-1)>first_point[0])&&((temp[0]+1)<third_point[0])&&
      								   ((temp[1]-1)>last_point[1])&&((temp[1]+1)<first_point[1]))
      				   {
      					   temp1[0]=temp[0]-1;
      					   temp1[1]=temp[1];
      					   temp2[0]=temp[0]+1;
      					   temp2[1]=temp[1];
      					   temp3[0]=temp[0];
      					   temp3[1]=temp[1]+1;
      					   temp4[0]=temp[0];
      					   temp4[1]=temp[1]-1;
         				   if (PLOT_TYPE==INT_CART)
         				   {
         					   if ((temp2[0]<cart_first_point[0])||(temp4[1]>cart_first_point[1]))
         					   {
         						   gsc_callCmd(_T("_.LINE"), RTPOINT, temp1,
                                          RTPOINT, temp2, RTSTR, GS_EMPTYSTR, 0);
         						   gsc_callCmd(_T("_.LINE"), RTPOINT, temp3,
                                          RTPOINT, temp4, RTSTR, GS_EMPTYSTR, 0);
         					   } 
         				   }
           				   else if ((PLOT_TYPE==EXBOTTOM_CART) || (PLOT_TYPE==EXBOTTOMLEG_CART))
         				   {
         					   if (temp4[1]>third_point[1])
         					   {
         						   gsc_callCmd(_T("_.LINE"), RTPOINT, temp1, 
                                          RTPOINT, temp2, RTSTR, GS_EMPTYSTR, 0);
         						   gsc_callCmd(_T("_.LINE"), RTPOINT, temp3, 
                                          RTPOINT, temp4, RTSTR, GS_EMPTYSTR, 0);
         					   }
                        }
         				   else
         				   {
         					   gsc_callCmd(_T("_.LINE"), RTPOINT, temp1, 
                                       RTPOINT, temp2, RTSTR, GS_EMPTYSTR, 0);
         					   gsc_callCmd(_T("_.LINE"), RTPOINT, temp3, 
                                       RTPOINT, temp4, RTSTR, GS_EMPTYSTR, 0);
         				   } 
                     }
      			   }//fine 1 colonna
      		   }//fine 1 riga

               //DISEGNO GRIGLIA SUL LATO VERTICALE SINISTRO
      		   for (int h=0; h<=rows;h++)
      		   {
      			   ads_point temp,temp1;
      			   temp[0]=first_point[0];
      			   temp[1]=p_grid[1]-h*stepy;
                  temp[2]=0.0;
      			   temp1[0]=temp[0]+1;
      			   temp1[1]=temp[1];
                  temp1[2]=0.0;
   				   if ( (temp[1]<first_point[1])&&(temp[1]>last_point[1]))
                  {
                     if ((PLOT_TYPE==EXBOTTOMLEG_CART)||(PLOT_TYPE==EXBOTTOM_CART) )
                     {
                        if (temp[1]>third_point[1])
      				         gsc_callCmd(_T("_.LINE"), RTPOINT, temp1,
                                       RTPOINT, temp, RTSTR, GS_EMPTYSTR, 0);
                     }
                     else
      				      gsc_callCmd(_T("_.LINE"), RTPOINT, temp1,
                                    RTPOINT, temp, RTSTR, GS_EMPTYSTR, 0);
                  }
      		   }//fine colonna

      		   //DISEGNO GRIGLIA SUL LATO ORIZZONTALE IN ALTO
      		   for (int k=0; k<=cols;k++)
      		   {
      			   ads_point temp,temp1;
      			   temp[0]=p_grid[0]+k*stepx;
      			   temp[1]=first_point[1];
                  temp[2]=0.0;
      			   temp1[0]=temp[0];
      			   temp1[1]=temp[1]-1;  
                  temp1[2]=0.0;
     				   if ( (temp[0]>first_point[0])&&(temp[0]<second_point[0]))
                  {
        				   gsc_callCmd(_T("_.LINE"), RTPOINT, temp1,
                                 RTPOINT, temp, RTSTR, GS_EMPTYSTR, 0);
                  }
      		   }//fine riga

      		   //DISEGNO GRIGLIA SUL LATO VERTICALE DESTRO
      		   for (int h=0; h<=rows;h++)
      		   {
      			   ads_point temp,temp1;
      			   temp[0]=third_point[0];
      			   temp[1]=p_grid[1]-h*stepy;
                  temp[2]=0.0;
      			   temp1[0]=temp[0]-1;
      			   temp1[1]=temp[1];
                  temp1[2]=0.0;
   				   if ( (temp[1]<first_point[1])&&(temp[1]>last_point[1]))
                  {
         			   if (PLOT_TYPE==INT_CART)
         			   {
         				   if ((temp[1]>cart_first_point[1]))
         				   {
         					   gsc_callCmd(_T("_.LINE"), RTPOINT, temp1, 
                                       RTPOINT, temp, RTSTR, GS_EMPTYSTR, 0);
         				   }
         			   }
         			   else
         			   {
                        if ((PLOT_TYPE==EXBOTTOMLEG_CART)||(PLOT_TYPE==EXBOTTOM_CART) )
                        {
                           if (temp[1]>third_point[1])
         				         gsc_callCmd(_T("_.LINE"), RTPOINT, temp1,
                                          RTPOINT, temp, RTSTR, GS_EMPTYSTR, 0);
                        }
                        else
         				      gsc_callCmd(_T("_.LINE"), RTPOINT, temp1,
                                       RTPOINT, temp, RTSTR, GS_EMPTYSTR, 0);
         			   }
                  }
      		   }//fine colonna

      		   //DISEGNO GRIGLIA SUL LATO ORIZZONTALE IN BASSO
      		   for (int k=0; k<=cols;k++)
      		   {
      			   ads_point temp,temp1;
      			   temp[0]=p_grid[0]+k*stepx;
                  if ((PLOT_TYPE==EXBOTTOMLEG_CART)||(PLOT_TYPE==EXBOTTOM_CART) )
         			   temp[1]=third_point[1];
                  else
      			      temp[1]=last_point[1];
                  temp[2]=0.0;
      			   temp1[0]=temp[0];
      			   temp1[1]=temp[1]+1;
                  temp1[2]=0.0;
     				   if ( (temp[0]>first_point[0])&&(temp[0]<second_point[0]))
                  {
         			   if (PLOT_TYPE==INT_CART)
         			   {
         				   if ((temp[0]<cart_first_point[0]))
         				   {
         					   gsc_callCmd(_T("_.LINE"), RTPOINT, temp1, RTPOINT, temp,
                                       RTSTR, GS_EMPTYSTR, 0);
         				   }
         			   }
         			   else
         			   {
         				   gsc_callCmd(_T("_.LINE"), RTPOINT, temp1, 
                                    RTPOINT, temp, RTSTR, GS_EMPTYSTR, 0);
         			   }
                  }
      		   }//fine riga

            }//fine if
         }//fine if
	   }  //fine GRIGLIA
	   
	   //definisce il LAYER per le view nel caso di cartiglio interno
	   if (PLOT_TYPE==INT_CART)
		   gsc_callCmd(_T("_.LAYER"), RTSTR, _T("_S"), RTSTR, _T("GSDDPLOT"), RTSTR, GS_EMPTYSTR, 0);
	   //costruzione view principale
	   if (PLOT_TYPE!=INT_CART)
      {
         gsc_callCmd(_T("_.MVIEW"), RTPOINT, first_point, RTPOINT, third_point, 0);
         //ZOOM di tutto
	      gsc_callCmd(_T("_.ZOOM"), RTSTR, _T("_ALL"), 0);
	      gsc_callCmd(_T("_.MSPACE"), 0);
	      //zoom nella view principale 
	      gsc_zoom(temp1, temp2);
	      gsc_callCmd(_T("_.PSPACE"), 0);
	      gsc_callCmd(_T("_.ZOOM"), RTSTR, _T("_ALL"), 0);
      }	   
	   //definisce il LAYER 0 come default
	   set_default_layer();
      
  	   //inserimento foglio e impostazioni finali per il plottaggio
      first_point[0]=reference_point[0];                                   
	   first_point[1]=PLOT_HEIGHT-2*PAPER_MARGINS+reference_point[1];

	   second_point[0]=PLOT_LARGENESS-2*PAPER_MARGINS+reference_point[0];
	   second_point[1]=first_point[1];                                   

	   third_point[0]=second_point[0];                                      
	   third_point[1]=reference_point[1];                     

	   last_point[0]=first_point[0];                     
	   last_point[1]=third_point[1];                     

	   set_default_layer();
	   double text_height;
	   double space;
	   ads_point plot_point_setting;
      plot_point_setting[2]=0.0;

	   if (PAPER_ORIENTATION==HORIZONTAL) //CASO ORIENTAMENTO ORIZZONTALE
	   {
		   if (SCALE_TYPE==OTTIMIZZATA)
         {
            HPAPER=PAPER;
      	   HUPAPER=HPAPER;
		   }
		   text_height=DIMPAPERS[HUPAPER+1]/30;
		   space=DIMPAPERS[HUPAPER]/30;
   	   plot_point_setting[0]=DIMPAPERS[HUPAPER]+30;
	      plot_point_setting[1]=DIMPAPERS[HUPAPER+1]-text_height;
		   TCHAR text_val[50];

         swprintf(text_val, 50, _T("%f"), text_height);
         ads_point paper_point1, paper_point2, paper_point3, paper_point4;//limiti del foglio con orientamento orizzontale
         paper_point1[0]=-1;
         paper_point1[1]=-1; 
         paper_point1[2]=0.0; 
         paper_point2[0]=paper_point1[0];
         paper_point2[1]=DIMPAPERS[HUPAPER+1]+1; 
         paper_point2[2]=0.0; 
         paper_point3[0]=DIMPAPERS[HUPAPER]+1;
         paper_point3[1]=paper_point2[1]; 
         paper_point3[2]=0.0; 
         paper_point4[0]=paper_point3[0];
         paper_point4[1]=paper_point1[1]; 
         paper_point4[2]=0.0; 
   	   if (gsc_callCmd(_T("_.COLOR"), RTSTR, _T("_RED"), 0) == RTERROR) return GS_BAD;
		   gsc_callCmd(_T("_.LINE"), RTPOINT, paper_point1, RTPOINT, paper_point2 ,    //disegna i limiti del foglio
		               RTPOINT, paper_point3, RTPOINT, paper_point4,
                     RTPOINT, paper_point1, RTSTR, GS_EMPTYSTR, 0);
   	   set_default_layer();
		   plot_point_setting[0]=plot_point_setting[0]+space; //inserisce la descrizione orientamento
		   plot_point_setting[1]=plot_point_setting[1]-text_height;

         InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), gsc_msg(131)); // "ORIENTAMENTO ORIZZONTALE"

         plot_point_setting[1]=plot_point_setting[1]-2*text_height;   //descrizione foglio
		   switch (HUPAPER)
		   {
            case A4:
               InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), gsc_msg(317)); // "FOGLIO A4"
			      break;
            case A3:
               InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), gsc_msg(318)); // "FOGLIO A3"
			      break;
		      case A2:
               InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), gsc_msg(319)); // "FOGLIO A2"
			      break;
		      case A1:
               InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), gsc_msg(320)); // "FOGLIO A1"
			      break;
		      case A0:
               InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), gsc_msg(321)); // "FOGLIO A0"
			      break;
            case FOGLIO_UTENTE:

               break;
		   }
		   plot_point_setting[1]=plot_point_setting[1]-2*text_height; // descrizione finestra di plottaggio

         InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), gsc_msg(322)); // "AREA DI PLOTTAGGIO:"
		   plot_point_setting[1]=plot_point_setting[1]-1.5*text_height;
		   TCHAR plot_points[250];
         ads_point point1,point2;
   	   point1[0]=0.0;
   	   point1[1]=0.0;
         point1[2]=0.0;
   	   point2[0]=DIMPAPERS[HUPAPER];
   	   point2[1]=DIMPAPERS[HUPAPER+1];
         point2[2]=0.0;

         swprintf(plot_points, 250, _T("%f, %f;      %f, %f"), point1[X], point1[Y], point2[X], point2[Y]);
         InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), plot_points);

         plot_point_setting[1]=plot_point_setting[1]-2*text_height; // unità di misura
         InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), gsc_msg(323)); // "UNITA' DI MISURA: MM"

		   plot_point_setting[1]=plot_point_setting[1]-2*text_height; // rotazione
         InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), gsc_msg(448)); // "Rotazione: ved. preview"

         plot_point_setting[1]=plot_point_setting[1]-2*text_height; // unità di plottaggio=unità di disegno
         InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), gsc_msg(449)); // "Plotted MM=Drawing Units"

         plot_point_setting[1]=plot_point_setting[1]-1.5*text_height;
         InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), _T("                 <1=1>"));

         if (SCALE_TYPE==OTTIMIZZATA)
         {
   		   plot_point_setting[1]=plot_point_setting[1]-2*text_height; //scala ottimizzata
            TCHAR scale_val[100];
            wcscpy(scale_val, gsc_msg(34)); // "Scala ottimizzata 1:"
   		   wcscat(scale_val, gsc_tostring(PLOT_SCALE));
            InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), scale_val);
         }
         else
         {
   		   plot_point_setting[1]=plot_point_setting[1]-2*text_height; //scala normale
            TCHAR scale_val[100];
            wcscpy(scale_val, gsc_msg(36)); // "Scala 1:"
   		   wcscat(scale_val,gsc_tostring(PLOT_SCALE));
            InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), scale_val);
         }
	   }
	   else		 //CASO ORIENTAMENTO VERTICALE
	   {
		   if (SCALE_TYPE==OTTIMIZZATA)
		   {
            VPAPER=PAPER;
      	   VUPAPER=VPAPER;
		   }
		   text_height=DIMPAPERS[VUPAPER]/30;
		   space=DIMPAPERS[VUPAPER+1]/30;
   	   plot_point_setting[0]=DIMPAPERS[VUPAPER+1]+30;
	      plot_point_setting[1]=DIMPAPERS[VUPAPER]-text_height;
		   TCHAR text_val[50];
         swprintf(text_val, 50, _T("%f"), text_height);
         ads_point paper_point1, paper_point2, paper_point3, paper_point4;//limiti del foglio con orientamento orizzontale
         paper_point1[0]=-1;
         paper_point1[1]=-1; 
         paper_point1[2]=0.0; 

         paper_point2[0]=paper_point1[0];
         paper_point2[1]=DIMPAPERS[VUPAPER]+1; 
         paper_point2[2]=0.0; 

         paper_point3[0]=DIMPAPERS[VUPAPER+1]+1;
         paper_point3[1]=paper_point2[1]; 
         paper_point3[2]=0.0; 

         paper_point4[0]=paper_point3[0];
         paper_point4[1]=paper_point1[1]; 
         paper_point4[2]=0.0; 

   	   if (gsc_callCmd(_T("_.COLOR"), RTSTR, _T("_RED"), 0) == RTERROR) return GS_BAD;
		   gsc_callCmd(_T("_.LINE"), RTPOINT, paper_point1, RTPOINT, paper_point2,    //disegna i limiti del foglio
		               RTPOINT, paper_point3, RTPOINT, paper_point4,
                     RTPOINT, paper_point1, RTSTR, GS_EMPTYSTR, 0);
   	   set_default_layer();
		   plot_point_setting[0]=plot_point_setting[0]+space; //inserisce la descrizione orientamento
		   plot_point_setting[1]=plot_point_setting[1]-text_height;

         InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), gsc_msg(37)); // "ORIENTAMENTO VERTICALE"
		   
		   plot_point_setting[1]=plot_point_setting[1]-2*text_height;   //descrizione foglio
		   switch (VUPAPER)
		   {
            case A4:
               InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), gsc_msg(317)); // "FOGLIO A4"
			      break;
            case A3:
               InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), gsc_msg(318)); // "FOGLIO A3"
			      break;
		      case A2:
               InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), gsc_msg(319)); // "FOGLIO A2"
			      break;
		      case A1:
               InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), gsc_msg(320)); // "FOGLIO A1"
			      break;
		      case A0:
               InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), gsc_msg(321)); // "FOGLIO A0"
			      break;
            case FOGLIO_UTENTE:

               break;

		   }
		   plot_point_setting[1]=plot_point_setting[1]-2*text_height;//descrizione finestra di plottaggio
         
         InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), gsc_msg(322)); // "AREA DI PLOTTAGGIO:"

		   plot_point_setting[1]=plot_point_setting[1]-1.5*text_height;
		   TCHAR plot_points[250];
         ads_point point1,point2;
         point1[0]=0.0;
   	   point1[1]=0.0;
   	   point1[2]=0.0;

   	   point2[0]=DIMPAPERS[VUPAPER+1];
   	   point2[1]=DIMPAPERS[VUPAPER];
   	   point2[2]=0.0;

         swprintf(plot_points, 250, _T("%f, %f;      %f, %f"), point1[X], point1[Y], point2[X], point2[Y]);
         InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), plot_points);

		   plot_point_setting[1]=plot_point_setting[1]-2*text_height;   //unità di misura
         InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), gsc_msg(323)); // "UNITA' DI MISURA: MM"

		   plot_point_setting[1]=plot_point_setting[1]-2*text_height;   //rotazione
         InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), gsc_msg(38)); // "Rotazione: 0/90"

		   plot_point_setting[1]=plot_point_setting[1]-2*text_height; //unità di plottaggio=unità di disegno
         InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), gsc_msg(449)); // "Plotted MM=Drawing Units"

         plot_point_setting[1]=plot_point_setting[1]-1.5*text_height;
         InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), _T("                 <1=1>"));

         if (SCALE_TYPE == OTTIMIZZATA)
         {
   		   plot_point_setting[1]=plot_point_setting[1]-2*text_height; //scala ottimizzata
            TCHAR scale_val[100];
            wcscpy(scale_val, gsc_msg(34)); // "Scala ottimizzata 1:"
   		   wcscat(scale_val, gsc_tostring(PLOT_SCALE));
            InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), scale_val);
         }
         else
         {
   		   plot_point_setting[1]=plot_point_setting[1]-2*text_height; //scala normale
            TCHAR scale_val[100];
            wcscpy(scale_val,gsc_msg(36)); // "Scala 1:"
   		   wcscat(scale_val,gsc_tostring(PLOT_SCALE));
            InsTextStandardTextStyle(plot_point_setting, text_val, _T("0"), scale_val);
         }
	   }

   	   //rettangolo da ripristinare solo per alcuni casi
	   if (PLOT_TYPE==INT_CART)
	   gsc_callCmd(_T("_.LINE"), RTPOINT, first_point, RTPOINT, second_point,//disegna il rettangolo
	               RTPOINT, third_point, RTPOINT, last_point,
                  RTPOINT, first_point,RTSTR, GS_EMPTYSTR, 0);
	   
      if ((PLOT_TYPE==EXBOTTOM_CART)||(PLOT_TYPE==EXBOTTOMLEG_CART)||(PLOT_TYPE==EXRIGHT_CART))
			   if ((PAPER_LIMITS!=1) || (LIMIT_TEXT_HEIGHT==0))  //5 sett.
         	   gsc_callCmd(_T("_.LINE"), RTPOINT, first_point, RTPOINT, second_point,//disegna il rettangolo
         	               RTPOINT, third_point, RTPOINT, last_point,
                           RTPOINT, first_point,RTSTR,GS_EMPTYSTR, 0);
   
            
      gsc_callCmd(_T("_.ZOOM"), RTSTR, _T("_ALL"), 0);
      //ZOOM TUTTO

   }
   while (0);

   rb=ads_newrb(RTSHORT);
	rb->resval.rint = osmode;
	acedSetVar(_T("OSMODE"), rb);
	acutRelRb(rb);rb=NULL;

	return res;
}																	  

/* ******************************************************************* */
//		scale_dim	- aggiusta le dimensioni in base alla scala 
//						  e unità di misura scelte
/* ******************************************************************* */
double scale_dim (double d)
{
	return (((((d*DRAWING_UNITS)/PLOT_UNITS)*PLOT_SCALE)/UNIT_CONST));

}

/* ******************************************************************* */
//		set_default_layer	- questa funzione setta il layer 0 standard
/* ******************************************************************* */

int set_default_layer()
{
	//imposta il layer di default 0	- ripristinando il colore(white), il tipo linea(CONTINUOUS)
	if (gsc_callCmd(_T("_.LAYER"), RTSTR, _T("_S"), RTSTR, _T("0"), RTSTR, GS_EMPTYSTR, 0) == RTERROR)
		return GS_BAD;
	if (gsc_callCmd(_T("_.COLOR"), RTSTR, _T("_BYLAYER"), 0) == RTERROR)
		return GS_BAD;
	if (gsc_callCmd(_T("_.LINETYPE"), RTSTR, _T("_S"), RTSTR, _T("_BYLAYER"), RTSTR, GS_EMPTYSTR, 0) == RTERROR)
		return GS_BAD;
	return GS_GOOD;
}

/* ******************************************************************* */
//		create_gsddplot_layer	- questa funzione viene utilizzata una volta sola
/* ******************************************************************* */
int create_gsddplot_layer(TCHAR *name)
{
	//controllo se il LAYER "gsddplot" esiste già e creazione
	if (is_new_layer(name))
		gsc_callCmd(_T("_.LAYER"), RTSTR, _T("_N"), RTSTR, name, RTSTR, GS_EMPTYSTR, 0);
	return GS_GOOD; 
}

/* ******************************************************************* */
//	create_new_layer-questa funzione crea un nuovo layer se non esiste già 
/* ******************************************************************* */
int create_new_layer(TCHAR *name)
{
	if (is_new_layer(name))
		gsc_callCmd(_T("_.LAYER"), RTSTR, _T("_N"), RTSTR, name, RTSTR, GS_EMPTYSTR, 0);
	return GS_GOOD; 
}

/* ******************************************************************* */
//		is_new_layer	- cerca nella tavola dei layer il nome del layer da creare
/* ******************************************************************* */
int is_new_layer(TCHAR *name)
{
	return ( ads_tblsearch(_T("LAYER"), name, 0) == NULL );
}

/* ******************************************************************* */
//		exist_linetype	- cerca nella tavola dei linetype
/* ******************************************************************* */
int exist_linetype(TCHAR *name)
{
	return ( ads_tblsearch(_T("LTYPE"),name,0)!=NULL );
}

/* ******************************************************************* */
//		exist_style	- cerca nella tavola degli style
/* ******************************************************************* */
int exist_style(TCHAR *name)
{
	return ( ads_tblsearch(_T("STYLE"),name,0)!=NULL );
}

/* ******************************************************************* */
//		class_node controlla se la classe è di tipo nodo
//		il quinto elemento relativo alla classe 
// 	nella lista generale delle classi vale TYPE_NODE
/* ******************************************************************* */
int class_node(resbuf *rb)
{
   for (int i=1; i<extr_class_index-2; i++)
	{
		rb=rb->rbnext;
	}
	return ((rb->resval.rint==CAT_SIMPLEX)&&(rb->rbnext->resval.rint==TYPE_NODE));
}

/* ******************************************************************* */
//		class_text controlla se la classe è di tipo testo
//		il quinto elemento relativo alla classe 
// 	nella lista generale delle classi vale TYPE_TEXT
/* ******************************************************************* */
int class_text(resbuf *rb)
{
   for (int i=1; i<extr_class_index-2; i++)
	{
		rb=rb->rbnext;
	}
	return ((rb->resval.rint==CAT_SIMPLEX)&&(rb->rbnext->resval.rint==TYPE_TEXT));
}

/* ******************************************************************* */
//		class_pol controlla se la classe è di tipo polilinea
//		il quinto elemento relativo alla classe 
// 	nella lista generale delle classi vale TYPE_POLYLINE
/* ******************************************************************* */
int class_pol(resbuf *rb)
{
   for (int i=1; i<extr_class_index-2; i++)
	{
		rb=rb->rbnext;
	}
	return ((rb->resval.rint==CAT_SIMPLEX)&&(rb->rbnext->resval.rint==TYPE_POLYLINE));
}

/* ******************************************************************* */
//		class_sup controlla se la classe è di tipo polilinea
//		il quarto elemento relativo alla classe 
// 	nella lista generale delle classi non vale CAT_SPAGHETTI
//		il quinto elemento relativo alla classe 
// 	nella lista generale delle classi vale TYPE_SURFACE
/* ******************************************************************* */
int class_sup(resbuf *rb)
{
   for (int i=1; i<extr_class_index-2; i++)
	{
		rb=rb->rbnext;
	}
	return ((rb->resval.rint==CAT_SIMPLEX)&&(rb->rbnext->resval.rint==TYPE_SURFACE));
}

/* ******************************************************************* */
//		class_spag controlla se la classe è di tipo spaghetti
//		il quarto elemento relativo alla classe 
// 	nella lista generale delle classi vale CAT_SPAGHETTI
//		e il quinto elemento relativo alla classe 
// 	nella lista generale delle classi vale TYPE_SPAGHETTI
/* ******************************************************************* */
int class_spag(resbuf *rb)
{
   for (int i=1; i<extr_class_index-2; i++)
	{
		rb=rb->rbnext;
	}
	return ((rb->resval.rint==CAT_SPAGHETTI)&&(rb->rbnext->resval.rint==TYPE_SPAGHETTI));
}

/* ******************************************************************* */
//		extern_class controlla se la classe è di tipo simulazione
//		il sesto elemento è una parentesi aperta
/* ******************************************************************* */
int extern_class(resbuf *rb)
{
	for (int i=0; i<extr_class_index; i++)
	{
		rb=rb->rbnext;
	}
	return (rb->restype==RTLB); //parentensi tonda aperta
	                           //solo nel caso di una classe contenente sottoclassi
}

/* ******************************************************************* */
//		skip_extern_class salta tutte le informazioni 
//		relative alla classe spostando il puntatore alla fine 
//		della lista delle sottoclassi
/* ******************************************************************* */
void skip_extern_class(resbuf *&rb)
{
   while (rb->rbnext!=NULL)
   {
      rb=rb->rbnext;
      if (rb->restype==RTLE) //parentesi chiusa
         if ((rb->rbnext!=NULL)&&(rb->rbnext->restype==RTSHORT))
         {
            rb=rb->rbnext;//punta all'elemento intero contentente
                                   //l'informazione se la classe è estratta o no
            break;
         }
   }
   //ha trovato due parentesi chiuse
}

/* ******************************************************************* */
//		valid_class controlla se la classe è di tipo 
//		nodo 
//		polilinea
//		testo 
// 		spaghetti
//		superficie
/* ******************************************************************* */
int valid_class(resbuf *rb)
{
	return ((class_node(rb))||(class_text(rb))||(class_pol(rb))||(class_sup(rb))||(class_spag(rb)));
}

/* ******************************************************************* */
//		insert_class inserisce la classe nella lista
//		con tutti i suoi attributi 
/* ******************************************************************* */
int insert_class(resbuf *rt,resbuf *ret,short ext_cl_index,short cl_index,
                 C_STRING *ext_cl_name, C_STRING *cl_name)
{
	geosim_class *punc=NULL;
	
	if (class_node(rt))
	{
		punc=new geosim_node(ret);
	}
	else if (class_text(rt))
	{
		punc=new geosim_text(ret);
	}
	else if (class_pol(rt))
	{
		punc=new geosim_pol(ret);
	}
	else if (class_sup(rt))
	{
		punc=new geosim_sup(ret);
	}
	else if (class_spag(rt))
	{
		punc=new geosim_spag(ret);
	}
	else
		return GS_BAD;
	CLASS_LIST_ITEM *item=new CLASS_LIST_ITEM(ext_cl_index,cl_index,
	                              ext_cl_name->get_name(),cl_name->get_name(),punc);
    EXT_CLASS_LIST.add_tail(item);
	//EXT_CLASS_LIST->push(EXT_CLASS_LIST,ext_cl_index,cl_index,ext_cl_name,cl_name,punc);
	return GS_GOOD;
}


/* ******************************************************************* */
//		insert_subclass_data - inserisce una sottoclasse di una classe simulazione
//		nella lista EXT_CLASS_LIST delle classi estratte
/* ******************************************************************* */
int insert_subclass_data(resbuf *ent,short prj,short ext_cl_index,short cl_index,
                         C_STRING *ext_cl_name, C_STRING *cl_name)
{
	C_RB_LIST ret;
   C_CLASS   *pCls;

   if ((pCls = gsc_find_class(prj, ext_cl_index, cl_index)) == NULL) return GS_BAD;
   if ((ret << pCls->ptr_fas()->to_rb()) == NULL) return GS_BAD;

	return insert_class(ent,ret.get_head(),ext_cl_index,cl_index, ext_cl_name, cl_name);
}

/* ******************************************************************* */
//		insert_class_data - inserisce una classe 
//		nella lista EXT_CLASS_LIST delle classi estratte
/* ******************************************************************* */
int insert_class_data(resbuf *ent,short prj,short ext_cl_index,short cl_index,
                      C_STRING *ext_cl_name, C_STRING *cl_name)
{
	C_RB_LIST ret;
   C_CLASS   *pCls;	

   if ((pCls = gsc_find_class(prj, cl_index, 0)) == NULL) return GS_BAD;
   if ((ret << pCls->ptr_fas()->to_rb()) == NULL) return GS_BAD;
   ret.link_head(acutBuildList(RTLB, 0));
   ret += acutBuildList(RTLE, 0);

	return insert_class(ent, ret.get_head(),ext_cl_index,cl_index, ext_cl_name, cl_name);
}

/* ******************************************************************* */
//		calculate_legend_list	- costruzione lista elementi legenda 
//		se non è definibile una legenda, EXIST_LEGEND viene settata nulla
//		costruzione della lista delle classi selezionate
/* ******************************************************************* */
int calculate_legend_list()
{
   short prj;
   long  SessionCode;
   C_AREACLASS_LIST lista;

	LEG_ROWS_NUMBER = 0;

   if (GS_CURRENT_WRK_SESSION == NULL)
   {
		EXIST_LEGEND=0;//variabile che memorizza l'impossibilità di creare una legenda automatica
      GS_ERR_COD = eGSNotCurrentSession;
      return GS_BAD;
   }
   if (GS_CURRENT_WRK_SESSION->get_pPrj() == NULL)
   {
		EXIST_LEGEND=0;//variabile che memorizza l'impossibilità di creare una legenda automatica
      GS_ERR_COD = eGSNotCurrentPrj;
      return GS_BAD;
   }
   
   if (GS_CURRENT_WRK_SESSION->get_status() != WRK_SESSION_ACTIVE)
   {
		EXIST_LEGEND=0;//variabile che memorizza l'impossibilità di creare una legenda automatica
      GS_ERR_COD = eGSOpNotAble;
      return GS_BAD;
   }

	// costruzione dati progetto corrente
   prj = GS_CURRENT_WRK_SESSION->get_PrjId();
	// costruzione dati sessione di lavoro corrente
	SessionCode = GS_CURRENT_WRK_SESSION->get_id();

	presbuf ent;

   // ricavo le classi estratte della sessione
   if (GS_CURRENT_WRK_SESSION->get_class_list(lista) == GS_BAD) return RTERROR;
   ent = lista.to_rb();
   gsc_lspforrb(&ent);
   
	if (ent==NULL)
	{
		EXIST_LEGEND=0;
	   acutRelRb(ent);ent=NULL;
      return GS_BAD;
	}
	//utilizza la lista ent per costruire l'elenco delle classi di entità
	//estratte (settimo elemento=2 oppure 3)
	short cl_index=0, ext_cl_index=0;
	C_STRING cl_name;
	C_STRING ext_cl_name;
	int i;
	while(ent!=NULL)
	{
		//trova il primo int che identifica la classe di entità
		while ( (ent->restype!=RTSHORT)&&(ent->rbnext!=NULL) )
		{
			ent=ent->rbnext;
		}
		if (ent->rbnext==NULL)//caso fine lista
			break; 
		if (ent->rbnext->restype==RTSTR)  //allora è un elemento valido con un nome
		{
			cl_index=ent->resval.rint;
			cl_name=ent->rbnext->resval.rstring;
			if (extern_class(ent))
			{
				ext_cl_index=cl_index;  //registra il codice della classe principale
				ext_cl_name=cl_name.get_name(); //registra il nome della classe principale
				for (i=0; i<extr_class_index; i++)
				{
					ent=ent->rbnext;
				}
				//ent punta alla prima parentesi della prima sottoclasse
				while (ent->rbnext!=NULL)
				{
					ent=ent->rbnext;
					if ((ent->restype==RTSHORT)&&(ent->rbnext->restype==RTSTR))
					{	
						cl_index=ent->resval.rint;
						cl_name=ent->rbnext->resval.rstring;
						if (valid_class(ent)) //controlla se la classe è valida
						{
							if (insert_subclass_data(ent,prj,ext_cl_index,cl_index, &ext_cl_name, &cl_name))
							{		
								LEG_ROWS_NUMBER+=1; //occorre una riga per inserire la classe nella legenda
							}
						}
					}
					//uscita dalla lista delle sottoclassi
					if (ent->restype==RTLE) //parentesi chiusa
						if ((ent->rbnext!=NULL)&&(ent->rbnext->restype==RTSHORT))
						{
							ent=ent->rbnext;//punta all'elemento intero contentente
	                                               //l'informazione se la classe è estratta o no
	                        break;//esce dal while
						}			 
				}//fine while
	            ent=ent->rbnext;//quindi non vengono considerate le sottoclassi
				continue;
			} 
	        // fine caso classe simulazione
			if (valid_class(ent)) //controlla se la classe è valida
			{
				ext_cl_index=0;  //resetta il codice della classe principale
				ext_cl_name.clear(); //svuota il nome della classe principale - name=NULL	 
				if (insert_class_data(ent,prj,ext_cl_index,cl_index, &ext_cl_name, &cl_name))
				{		
					LEG_ROWS_NUMBER+=1; //occorre una riga per inserire la classe nella legenda
				}
			}
			ent=ent->rbnext;//ent punta a una parentesi chiusa,se la classe trovata non aveva sottoclassi
	                         //ent punta ad un intero,se la classe aveva sottoclassi
		}//fine if	(elemento valido)
		else
		{
			 ent=ent->rbnext;
			 continue;
		}
	}//fine while
	acutRelRb(ent);ent=NULL;
   	return GS_GOOD;
}

/* ******************************************************************* */
//		calculate_scale	- calcola scala nel caso scala OTTIMIZZATA
/* ******************************************************************* */
int calculate_scale()
{
   double hscale, vscale;
   if (PAPER_ORIENTATION==HORIZONTAL)
	{ 
      switch (PLOT_TYPE)
      {
         case CART_LESS:
            hscale=(PAPER_LARGENESS*UNIT_CONST)/(DIMPAPERS[PAPER]-2*PAPER_MARGINS);
            vscale=(PAPER_HEIGHT*UNIT_CONST)/(DIMPAPERS[PAPER+1]-2*PAPER_MARGINS);
            break;
         case INT_CART:
            hscale=(PAPER_LARGENESS*UNIT_CONST)/(DIMPAPERS[PAPER]-2*PAPER_MARGINS);
            vscale=(PAPER_HEIGHT*UNIT_CONST)/(DIMPAPERS[PAPER+1]-2*PAPER_MARGINS);
            break;
         case EXRIGHT_CART:
            hscale=(PAPER_LARGENESS*UNIT_CONST)/(DIMPAPERS[PAPER]-2*PAPER_MARGINS-CART_BASE);
            vscale=(PAPER_HEIGHT*UNIT_CONST)/(DIMPAPERS[PAPER+1]-2*PAPER_MARGINS);
            break;
         case EXBOTTOM_CART:
            hscale=(PAPER_LARGENESS*UNIT_CONST)/(DIMPAPERS[PAPER]-2*PAPER_MARGINS);
            vscale=(PAPER_HEIGHT*UNIT_CONST)/(DIMPAPERS[PAPER+1]-2*PAPER_MARGINS-CART_HEIGHT);
            break;
         case EXRIGHTLEG_CART:
            hscale=(PAPER_LARGENESS*UNIT_CONST)/(DIMPAPERS[PAPER]-2*PAPER_MARGINS-CART_BASE);
            vscale=PAPER_HEIGHT/(DIMPAPERS[PAPER+1]-2*PAPER_MARGINS);
            break;
         case EXBOTTOMLEG_CART:
            hscale=(PAPER_LARGENESS*UNIT_CONST)/(DIMPAPERS[PAPER]-2*PAPER_MARGINS);
            vscale=(PAPER_HEIGHT*UNIT_CONST)/(DIMPAPERS[PAPER+1]-2*PAPER_MARGINS-CART_HEIGHT);
            break;
         default:
            hscale=(PAPER_LARGENESS*UNIT_CONST)/(DIMPAPERS[PAPER]-2*PAPER_MARGINS);
            vscale=(PAPER_HEIGHT*UNIT_CONST)/(DIMPAPERS[PAPER+1]-2*PAPER_MARGINS);
            break;
      }
   }
   else   //ORIENTAMENTO VERTICALE
   {
      switch (PLOT_TYPE)
      {
         case CART_LESS:
            hscale=(PAPER_LARGENESS*UNIT_CONST)/(DIMPAPERS[PAPER+1]-2*PAPER_MARGINS);
            vscale=(PAPER_HEIGHT*UNIT_CONST)/(DIMPAPERS[PAPER]-2*PAPER_MARGINS);
            break;
         case INT_CART:
            hscale=(PAPER_LARGENESS*UNIT_CONST)/(DIMPAPERS[PAPER+1]-2*PAPER_MARGINS);
            vscale=(PAPER_HEIGHT*UNIT_CONST)/(DIMPAPERS[PAPER]-2*PAPER_MARGINS);
            break;
         case EXRIGHT_CART:
            hscale=(PAPER_LARGENESS*UNIT_CONST)/(DIMPAPERS[PAPER+1]-2*PAPER_MARGINS-CART_BASE);
            vscale=(PAPER_HEIGHT*UNIT_CONST)/(DIMPAPERS[PAPER]-2*PAPER_MARGINS);
            break;
         case EXBOTTOM_CART:
            hscale=(PAPER_LARGENESS*UNIT_CONST)/(DIMPAPERS[PAPER+1]-2*PAPER_MARGINS);
            vscale=(PAPER_HEIGHT*UNIT_CONST)/(DIMPAPERS[PAPER]-2*PAPER_MARGINS-CART_HEIGHT);
            break;
         case EXRIGHTLEG_CART:
            hscale=(PAPER_LARGENESS*UNIT_CONST)/(DIMPAPERS[PAPER+1]-2*PAPER_MARGINS-CART_BASE);
            vscale=(PAPER_HEIGHT*UNIT_CONST)/(DIMPAPERS[PAPER]-2*PAPER_MARGINS);
            break;
         case EXBOTTOMLEG_CART:
            hscale=(PAPER_LARGENESS*UNIT_CONST)/(DIMPAPERS[PAPER+1]-2*PAPER_MARGINS);
            vscale=(PAPER_HEIGHT*UNIT_CONST)/(DIMPAPERS[PAPER]-2*PAPER_MARGINS-CART_HEIGHT);
            break;
         default:
            hscale=(PAPER_LARGENESS*UNIT_CONST)/(DIMPAPERS[PAPER+1]-2*PAPER_MARGINS);
            vscale=(PAPER_HEIGHT*UNIT_CONST)/(DIMPAPERS[PAPER]-2*PAPER_MARGINS);
            break;
      }
   }
   if (hscale>vscale)
      PLOT_SCALE=hscale;
   else
      PLOT_SCALE=vscale;

   //tronco il numero a 6 cifre decimali
   TCHAR  val [50];
   double temp;
   swprintf(val, 50, _T("%f"), PLOT_SCALE);
   ads_distof(val,-1,&temp);  //conversione da stringa a double
   if (temp<PLOT_SCALE)
      PLOT_SCALE=PLOT_SCALE+0.000001;  //correzione altrimenti la scala rischia di essere minore del necessario


   return GS_GOOD;
}

/* ******************************************************************* */
//		calculate_draw_dim	- calcoli per costruzione delle dim del disegno
/* ******************************************************************* */
int calculate_draw_dim()
{
	//larghezza e altezza della sessione di disegno stampata in mm
	PLOT_LARGENESS=(PAPER_LARGENESS/PLOT_SCALE)*UNIT_CONST+2*PAPER_MARGINS;
	PLOT_HEIGHT=(PAPER_HEIGHT/PLOT_SCALE)*UNIT_CONST+2*PAPER_MARGINS;
	
   //tronco alla sesta cifra decimale
	TCHAR val[50];
   swprintf(val, 50, _T("%f"), PLOT_LARGENESS);
   //conversione da stringa a double
   ads_distof(val,-1,&PLOT_LARGENESS);  //conversione da stringa a double

   swprintf(val, 50, _T("%f"), PLOT_HEIGHT);
   //conversione da stringa a double
   ads_distof(val,-1,&PLOT_HEIGHT);  //conversione da stringa a double

   return GS_GOOD;
}

/* ******************************************************************* */
//		calculate_leg_ent_space_text	- calcolo spazio necessario per un 
//  								      elemento, per la legenda per entità
/* ******************************************************************* */
int calculate_leg_ent_space_text()
{
	C_STRING        name;
	size_t          space = 0, str = 0;
	CLASS_LIST_ITEM *punt = (CLASS_LIST_ITEM*) EXT_CLASS_LIST.get_head();

	while (punt)
	{
		if ((punt->ext_class_name.len())!=0) //caso sottoclasse
		{
			name = punt->ext_class_name.get_name();
			name += _T(" - ");
			name += punt->class_name.get_name();
		}
  		else
			name=punt->class_name.get_name();
		str=name.len();
		if (str>space)
			space=str;
		punt=(CLASS_LIST_ITEM *)EXT_CLASS_LIST.get_next();
	}
	
   LEG_SPACE_TEXT = space;

	return GS_GOOD;
} 

/* ******************************************************************* */
//		calculate_leg_block_rows_number_and_space_text	- calcolo righe necessarie 
//  										        per la legenda per blocchi
/* ******************************************************************* */
int calculate_leg_block_rows_number()
{
	return GS_GOOD;
} 

/* ******************************************************************* */
//		calculate_legend	- calcoli per costruzione legenda sul disegno
/* ******************************************************************* */
int calculate_legend()
{
	//controlla la var LEG_ENT. Se è =1 la legenda è per entità, quindi 
	//calcola il numero di righe necessarie e lo spazio max per il testo 
	//in base ai nomi delle classi
	if (LEG_ENT==1)
	{
		LEG_ROWS_NUMBER=EXT_CLASS_LIST.get_count();
      	calculate_leg_ent_space_text();		//calcolo LEG_SPACE_TEXT
	}
	else //se LEG_ENT=0, il numero di righe necessarie cambia
	//e lo spazio max per il testo in base ai nomi dei blocchi
	{
		//DA CAMBIARE per il calcolo della legenda per blocchi
		LEG_ROWS_NUMBER=EXT_CLASS_LIST.get_count();  
    	calculate_leg_block_rows_number();		//calcolo LEG_SPACE_TEXT
	}
  	//calcolo base e altezza legenda
  	if ( (PLOT_TYPE==EXRIGHTLEG_CART)||(PLOT_TYPE==EXBOTTOMLEG_CART) )
	{
		if (PLOT_TYPE==EXRIGHTLEG_CART)
		{
			LEG_BASE=CART_BASE;
		   LEG_HEIGHT=PLOT_HEIGHT-CART_HEIGHT-2*PAPER_MARGINS;
		}
		else
		{
			LEG_HEIGHT=CART_HEIGHT;
  			LEG_BASE=PLOT_LARGENESS-CART_BASE-2*PAPER_MARGINS;
		}
   }
   	//spazio orizzontale occupato da ogni elemento
	if (LEG_BLOCK_SCALE!=0)
	{
		double leg_item_space=((LEG_SPACE_BLOCK/LEG_BLOCK_SCALE)+(LEG_SPACE_TEXT+4)*LEG_TEXT_HEIGHT);
		if ( (leg_item_space!=0) && (LEG_TEXT_INTER_LINE!=0) )
		{
			//numero colonne in orizzontale disponibili in legenda=LEG_BASE/leg_item_space
			int columns=0; //se column2 non è un valore valido, columns rimane nullo
			int column2=(int)(LEG_BASE/(leg_item_space));
			if (column2>0)
			{	
				if ( (column2%2)==1)//column2 dispari
					columns=column2/2+1;
				else
					columns=column2/2;
			}
			//numero righe in verticale disponibili in legenda=LEG_HEIGHT/LEG_TEXT_INTER_LINE
			int rows=(int)((LEG_HEIGHT/(LEG_TEXT_INTER_LINE))-1);//CORRECTION
				if (rows<0)
					rows=0;
			LEG_AV_ROWS_NUMBER=rows*columns;
		}
	}
	return GS_GOOD;
}

/* ******************************************************************* */
//		build_legend	- costruzione legenda sul disegno
/* ******************************************************************* */
int build_legend(ads_point leg_pt)
{
	ads_point pt={leg_pt[0]+LEG_TEXT_HEIGHT*2,leg_pt[1]+LEG_HEIGHT-LEG_TEXT_INTER_LINE,0.0};
	double leg_item_space=((LEG_SPACE_BLOCK/LEG_BLOCK_SCALE)+(LEG_SPACE_TEXT+4)*LEG_TEXT_HEIGHT);
	int columns=0; 
	int column2=(int)(LEG_BASE/(leg_item_space));
	if ( (column2%2)==1)//column2 dispari
		columns=column2/2+1;
	else
		columns=column2/2;
	//numero righe in verticale disponibili in legenda=LEG_HEIGHT/LEG_TEXT_INTER_LINE
	int rows=(int)((LEG_HEIGHT/(LEG_TEXT_INTER_LINE))-1);//LINE_CORRECTION
	int i=0,j=0;
	C_STRING name;
	CLASS_LIST_ITEM *punct;
	punct=(CLASS_LIST_ITEM*)EXT_CLASS_LIST.get_head();
	TCHAR  val1[50], val2[50];
	double sc=(1.0/LEG_BLOCK_SCALE);
   
   swprintf(val1, 50, _T("%f"), sc); // scale blocco
   swprintf(val2, 50, _T("%f"), LEG_TEXT_HEIGHT); // altezza testo
	//imposta il layer di default 0
	set_default_layer();
	while ((i<columns)&&(punct!=NULL))
	{	
		while ((j<rows)&&(punct!=NULL))
		{
			//inserimento blocco in  pt con la scala LEG_BLOCK_SCALE	 (stessa per X e Y)
			ads_point temp={pt[0]+LEG_SPACE_BLOCK/2,pt[1]+LEG_SPACE_BLOCK/2, 0.0};
			punct->class_ref->draw_class_block(temp,val1,LEG_SPACE_BLOCK/LEG_BLOCK_SCALE); //vedere quali parametri passare
			//inserimento scritta in pt
			pt[0]+=LEG_SPACE_BLOCK/LEG_BLOCK_SCALE+LEG_TEXT_HEIGHT*2;
			if ((punct->ext_class_name.len())!=0) //caso sottoclasse
			{
				name =punct->ext_class_name.get_name();
				name += _T(" - ");
				name += punct->class_name.get_name();
			}
			else
				name = punct->class_name.get_name();

			punct->class_ref->write_class_name(pt,val2, &name);
			punct=(CLASS_LIST_ITEM *)EXT_CLASS_LIST.get_next();
			j++;
			pt[0]-=LEG_SPACE_BLOCK/LEG_BLOCK_SCALE+LEG_TEXT_HEIGHT*2;
			pt[1]-=LEG_TEXT_INTER_LINE;
		}
		j=0;
		pt[0]+=(LEG_SPACE_BLOCK/LEG_BLOCK_SCALE)+(LEG_SPACE_TEXT+4)*LEG_TEXT_HEIGHT;
		pt[1]=leg_pt[1]+LEG_HEIGHT-LEG_TEXT_INTER_LINE;
		i++;
	}
	//definisce il LAYER per le view  
	gsc_callCmd(_T("_.LAYER"), RTSTR, _T("_S"), RTSTR, _T("GSDDPLOT"), RTSTR, GS_EMPTYSTR, 0);
	punct=NULL;
	return GS_GOOD;
}

/* ******************************************************************* */
//		is_point
/* ******************************************************************* */
int is_point()
{
	return ( (PT1[0]==PT2[0])&&(PT1[1]==PT2[1])  );
}

/* ******************************************************************* */
//		get_area
/* ******************************************************************* */
void get_area()
{
	do
	{
		//prende i due angoli della sessione
		int tmp;
		if (acedGetPoint(NULL, gsc_msg(39), PT1)==RTNORM) // "\nInserisci il primo angolo\n"
			tmp=ads_getcorner(PT1, gsc_msg(40), PT2); // "\nInserisci il secondo angolo\n"
		else
			continue;
		if ( is_point() || tmp!=RTNORM)
			continue;
		else 
			break;
	} while(1); //fine ciclo per la scelta della sessione


   //modificare i valori di PT1 e PT2 troncando alla seconda cifra decimale
   gsc_rtrim_double(&PT1[0]);
   gsc_rtrim_double(&PT1[1]);
   gsc_rtrim_double(&PT2[0]);
   gsc_rtrim_double(&PT2[1]);
}

/* ******************************************************************* */
//		dimen_calculate	 -- calcola le dimensioni rispetto 
//                          all'unità di misura scelta
/* ******************************************************************* */
void dimen_calculate()
{
	LARGENESS=(PT1[0]-PT2[0]);
	if (LARGENESS<0)
		LARGENESS=-LARGENESS;
	HEIGHT=(PT1[1]-PT2[1]);
	if (HEIGHT<0)
		HEIGHT=-HEIGHT;
	PAPER_LARGENESS=(LARGENESS/DRAWING_UNITS)*PLOT_UNITS;	
	//dimensioni finestra nell'unità scelta
	PAPER_HEIGHT=(HEIGHT/DRAWING_UNITS)*PLOT_UNITS;
}

/* ******************************************************************* */
//		unit_const_calculate
/* ******************************************************************* */
void unit_const_calculate()
{
	if (UNITS_SYSTEM==METRIC)
		switch (MEASURE_UNIT)
		{
			case MM:
				UNIT_CONST=1.0;
				break;
			case CM:
				UNIT_CONST=10.0;
				break;
			case M:
				UNIT_CONST=1000.0;
				break;
			case KM:
				UNIT_CONST=1000000.0;
				break;
	   }
	else
		switch (MEASURE_UNIT)
		{
			case POLLICI:
				UNIT_CONST=25.4;
				break;
			case PIEDI:
				UNIT_CONST=304.8;
				break;
			case YARDE:
				UNIT_CONST=914.4;
				break;
			case MIGLIA:
				UNIT_CONST=1609344;
				break;
	   }
}

/* ******************************************************************* */
//		paper_scale_calculate
//    calcolo dei fogli più piccoli contenti il disegno
//    oppure della scala necessaria se OTTIMIZZATA al foglio PAPER
//    nei due casi di orientamento verticale e orizzontale del foglio
/* ******************************************************************* */
int paper_scale_calculate()
{
	switch (PLOT_TYPE) 
	{																	  
		case CART_LESS:
         if (SCALE_TYPE==OTTIMIZZATA)
            calculate_scale();
         calculate_draw_dim();   
			break;
		case INT_CART:
         if (SCALE_TYPE==OTTIMIZZATA)
            calculate_scale();
         calculate_draw_dim();   
			break;
		case EXBOTTOM_CART:
         if (SCALE_TYPE==OTTIMIZZATA)
            calculate_scale();
         calculate_draw_dim();   
		   PLOT_HEIGHT=PLOT_HEIGHT+CART_HEIGHT;
			break;
		case EXRIGHT_CART:      //opzione 4
         if (SCALE_TYPE==OTTIMIZZATA)
            calculate_scale();
         calculate_draw_dim();   
			PLOT_LARGENESS=PLOT_LARGENESS+CART_BASE;
			break;
		case EXBOTTOMLEG_CART:	//opzione 5   
         if (SCALE_TYPE==OTTIMIZZATA)
            calculate_scale();
         calculate_draw_dim();   
			PLOT_HEIGHT=PLOT_HEIGHT+CART_HEIGHT;
			//già calcolati nella DCL geo_cart_leg
			LEG_HEIGHT=CART_HEIGHT;
			LEG_BASE=PLOT_LARGENESS-CART_BASE-2*PAPER_MARGINS;
			break;
		case EXRIGHTLEG_CART:   //opzione 6
         if (SCALE_TYPE==OTTIMIZZATA)
            calculate_scale();
         calculate_draw_dim();   
			PLOT_LARGENESS=PLOT_LARGENESS+CART_BASE;
			LEG_HEIGHT=PLOT_HEIGHT-CART_HEIGHT-2*PAPER_MARGINS;
			LEG_BASE=CART_BASE;
			break;
	}
   if (SCALE_TYPE!=OTTIMIZZATA)
   {
   	HPAPER=-1;
   	VPAPER=-1;
   	if (PAPER_ORIENTATION==HORIZONTAL)
   	{
   		//calcolo dei fogli nel caso di orientamento orizzontale
   		int i=0; //indice per la lista delle dim dei fogli
   		do
   		{
   			if ( (DIMPAPERS[i]>=PLOT_LARGENESS)&&(DIMPAPERS[i+1]>=PLOT_HEIGHT) )
   			{
   				HPAPER=i;  //memorizza il tipo di foglio più piccolo utilizzabile
   				break;
   			}
   			else
   			{
   				i+=2;
   			}
   		}
   		while ( (DIMPAPERS[i]!=NULL)&&(DIMPAPERS[i+1]!=NULL));
   	}
   	else
   	{
   		//calcolo dei fogli nel caso di orientamento verticale
   		int j=0; //indice per la lista delle dim dei fogli
   		do
   		{
   			if ( (DIMPAPERS[j]>=PLOT_HEIGHT)&&(DIMPAPERS[j+1]>=PLOT_LARGENESS) )
   			{	
   				VPAPER=j;  //memorizza il tipo di foglio più piccolo utilizzabile
   				break;
   			}
   			else
   			{
   				j+=2;
   			}
   		}
   		while ((DIMPAPERS[j]!=NULL)&&(DIMPAPERS[j+1]!=NULL));
   	} //fine if PAPER_ORIENTATION
	
   	if ((HPAPER<0)&&(VPAPER<0))
      {
         if (PLOT_HEIGHT >= PLOT_LARGENESS)
         {
            DIMPAPERS[FOGLIO_UTENTE]     = PLOT_HEIGHT;
            DIMPAPERS[FOGLIO_UTENTE + 1] = PLOT_LARGENESS;
         }
         else
         {
            DIMPAPERS[FOGLIO_UTENTE]     = PLOT_LARGENESS;
            DIMPAPERS[FOGLIO_UTENTE + 1] = PLOT_HEIGHT;
         }

      	if (PAPER_ORIENTATION==HORIZONTAL)
         {
   	      HPAPER=FOGLIO_UTENTE;
         }
         else
         {
            if (PLOT_HEIGHT <= PLOT_LARGENESS)
            {
               DIMPAPERS[FOGLIO_UTENTE]     = PLOT_LARGENESS;
               DIMPAPERS[FOGLIO_UTENTE + 1] = PLOT_LARGENESS;
            }

            VPAPER=FOGLIO_UTENTE;
         }
         //PLOT_ERRORS += gsc_msg(41); // "Dimensioni area plottaggio troppo grandi\n"
      }


      return ((HPAPER>=0)||(VPAPER>=0));
   }//fine SCALE_TYPE non ottimizzata
   else 
      return GS_GOOD;
} //fine paper_scale_calculate
// ACTION TILE : click su tasto Help
static void CALLB ExtPlot_help(ads_callback_packet *dcl)
   { gsc_help(IDH_Ilcomandodistampa); } 

/* ******************************************************************* */
//		 -- GeoExt Dialog
/* ******************************************************************* */
int geo_ext()
{
	ads_hdlg hdlg;
	int dlg_status;
	int dcl_id;
	TCHAR val[50];
	
   if (gsc_load_dialog(DCL_FILE.get_name(), &dcl_id) == RTERROR)
   {
   	acutPrintf(gsc_msg(42), DCL_FILE.get_name()); // "Error loading %s \n"
   	return RTERROR;
   }
   ads_new_dialog(_T("GeoExt"), dcl_id,NULLCB,&hdlg);	/* Display the GeoExt dialog */
   ads_action_tile(hdlg, _T("UserArea"), (CLIENTFUNC) get_user_area);
   ads_action_tile(hdlg, _T("previous"), (CLIENTFUNC) previous);
   ads_action_tile(hdlg, _T("cancel"),   (CLIENTFUNC) annulla);
   ads_action_tile(hdlg, _T("accept"),   (CLIENTFUNC) conferma);
   ads_action_tile(hdlg, _T("CX1"),      (CLIENTFUNC) get_coordinates);
   ads_action_tile(hdlg, _T("CY1"),      (CLIENTFUNC) get_coordinates);
   ads_action_tile(hdlg, _T("CZ1"),      (CLIENTFUNC) get_coordinates);
   ads_action_tile(hdlg, _T("CX2"),      (CLIENTFUNC) get_coordinates);
   ads_action_tile(hdlg, _T("CY2"),      (CLIENTFUNC) get_coordinates);
   ads_action_tile(hdlg, _T("CZ2"),      (CLIENTFUNC) get_coordinates);
   ads_action_tile(hdlg, _T("help"),     (CLIENTFUNC) ExtPlot_help);

	//ads_mode_tile(hdlg,"previous",1);
   swprintf(val, 50, _T("%f"), PT1[X]);
	ads_set_tile(hdlg, _T("CX1"), val);
   swprintf(val, 50, _T("%f"), PT1[Y]);
	ads_set_tile(hdlg, _T("CY1"), val);
   swprintf(val, 50, _T("%f"), PT2[X]);
	ads_set_tile(hdlg, _T("CX2"), val);
   swprintf(val, 50, _T("%f"), PT2[Y]);
	ads_set_tile(hdlg, _T("CY2"), val);
   
   if (is_point())
   {
   	ads_set_tile(hdlg, _T("err_msg"), gsc_msg(43)); // "area non valida"
   }
	ads_start_dialog(hdlg, &dlg_status);	/* run the dialog */
	ads_unload_dialog(dcl_id);
	switch (dlg_status)
	{
		case DLGCANCEL:
			return dlg_status;
			break;
 		case DLGOK:
			return dlg_status;
		 	break;
		case DLGSELECT_AREA:
			return dlg_status;
			break;
		case DLGPREV:
			return dlg_status;
			break;
		default:
			return DLGWAIT;
			break; 
   }
}
// ACTION TILE : click su tasto Help
static void CALLB InitPlot_help(ads_callback_packet *dcl)
   { gsc_help(IDH_Ilcomandodistampa); } 


/* ******************************************************************* */
//		geo_init -- GeoInit Dialog
/* ******************************************************************* */
int geo_init()
{
   ads_hdlg hdlg;
   int dlg_status;
   int dcl_id;

   if (gsc_load_dialog(DCL_FILE.get_name(), &dcl_id) == RTERROR)
   {
		acutPrintf(gsc_msg(42), DCL_FILE.get_name()); // "Error loading %s \n"
		return RTERROR;
   }
   ads_new_dialog(_T("GeoInit"), dcl_id,NULLCB,&hdlg);	/* Display the GeoInit dialog */

   ads_action_tile(hdlg, _T("Load"), (CLIENTFUNC) select_ini_plot_file);
   
   ads_action_tile(hdlg, _T("Save"),   (CLIENTFUNC) save_ini_plot_file);
   
   ads_action_tile(hdlg, _T("cancel"), (CLIENTFUNC) annulla);
   ads_action_tile(hdlg, _T("accept"), (CLIENTFUNC) conferma);
   ads_action_tile(hdlg, _T("help"),   (CLIENTFUNC) InitPlot_help);

   C_STRING val;

   if (INIPLOTFILE.len() == 0)
   {
	   val = GEOsimAppl::CURRUSRDIR;      // costruisce path per il file .INI
	   val += _T("\\GSDDPLOT.INI");    //aggiunge il nome del file .INI
      INIPLOTFILE = val;
   }
   ads_set_tile(hdlg, _T("INIPlotFile"), INIPLOTFILE.get_name());

   if (UNITS_SYSTEM==0)
      val=DECIMAL_SYSTEM;
   else
      val=ENGLISH_SYSTEM;
	ads_set_tile(hdlg, _T("System"), val.get_name());

	if (UNITS_SYSTEM==METRIC)		/* inizializzazione opzione sistema e lista unità di misura */
	{
      switch (MEASURE_UNIT)
      {
         case MM:
            val = _T("mm");
            break;
         case CM:
            val = _T("cm");
            break;
         case M:
            val = _T("m");
            break;
         case KM:
            val = _T("km");
            break;
      }
	}
   else
   {
      switch (MEASURE_UNIT)
      {
			case POLLICI:
            val = gsc_msg(48); // "Pollici"
				break;
			case PIEDI:
				val = gsc_msg(49); // "Piedi"
				break;
			case YARDE:
				val = gsc_msg(50); // "Yarde"
				break;
			case MIGLIA:
				val = gsc_msg(51); // "Miglia"
				break;
      }
   }
	ads_set_tile(hdlg, _T("Unit"),val.get_name());

	val = PLOT_UNITS; //conversione da double a stringa
	ads_set_tile(hdlg, _T("Plot_units"), val.get_name());

   val=gsc_tostring(DRAWING_UNITS);//conversione da double a stringa
	ads_set_tile(hdlg, _T("Draw_units"), val.get_name());

	val = PAPER_USER_MARGINS; //conversione da int a stringa
	ads_set_tile(hdlg, _T("Margins"), val.get_name());

	val = GRID_STEPX; //conversione da double a stringa
	ads_set_tile(hdlg, _T("GridX"), val.get_name());

	val = GRID_STEPY; //conversione da double a stringa
	ads_set_tile(hdlg, _T("GridY"), val.get_name());

	val = CART_HEIGHT; //conversione da double a stringa
	ads_set_tile(hdlg, _T("Cart_height"), val.get_name());

	val = CART_BASE; //conversione da double a stringa
	ads_set_tile(hdlg, _T("Cart_base"), val.get_name());

	val = LEG_TEXT_HEIGHT; //conversione da double a stringa
	ads_set_tile(hdlg, _T("Text_height"), val.get_name());

	val = LEG_TEXT_INTER_LINE; //conversione da double a stringa
	ads_set_tile(hdlg, _T("Text_interline"), val.get_name());

	val = LEG_SPACE_BLOCK; //conversione da double a stringa
	ads_set_tile(hdlg, _T("Space_block"), val.get_name());

	val = LEG_BLOCK_SCALE; //conversione da double a stringa
	ads_set_tile(hdlg, _T("Block_scale"), val.get_name());

	val = PLOT_SCALE; //conversione da double a stringa
	ads_set_tile(hdlg, _T("Scale"), val.get_name());

   ads_start_dialog(hdlg, &dlg_status);	/* run the dialog */
   ads_unload_dialog(dcl_id);
	switch (dlg_status)
	{
 		case DLGOK:
			return dlg_status;
		 	break;
 		case DLGCANCEL:
			return dlg_status;
		 	break;
		default:
			return DLGWAIT;
			break; 
   }
}
// ACTION TILE : click su tasto Help
static void CALLB UnitsPlot_help(ads_callback_packet *dcl)
   { gsc_help(IDH_Ilcomandodistampa); } 

/* ******************************************************************* */
//		 -- GeoUnits Dialog
/* ******************************************************************* */
int geo_units()
{
	ads_hdlg hdlg;
	int dlg_status;
	int dcl_id;
	C_STRING val;

   if (gsc_load_dialog(DCL_FILE.get_name(), &dcl_id) == RTERROR)
   {
		acutPrintf(gsc_msg(42), DCL_FILE.get_name()); // "Error loading %s \n"
		return RTERROR;
   }
	ads_new_dialog(_T("GeoUnits"), dcl_id, NULLCB, &hdlg); // Display the GeoUnits dialog
	ads_action_tile(hdlg, _T("previous"),         (CLIENTFUNC) previous);
	ads_action_tile(hdlg, _T("cancel"),           (CLIENTFUNC) annulla);
	ads_action_tile(hdlg, _T("accept"),           (CLIENTFUNC) conferma);
	ads_action_tile(hdlg, _T("English"),          (CLIENTFUNC) EnglishUnitsList);
	ads_action_tile(hdlg, _T("Metric"),           (CLIENTFUNC) MetricUnitsList);
	ads_action_tile(hdlg, _T("MeasureUnitsList"), (CLIENTFUNC) get_measure_unit);
	ads_action_tile(hdlg, _T("PlotUnits"),        (CLIENTFUNC) get_plot_units);
	ads_action_tile(hdlg, _T("DrawingUnits"),     (CLIENTFUNC) get_drawing_units);
   ads_action_tile(hdlg, _T("help"),             (CLIENTFUNC) UnitsPlot_help);

   switch (UNITS_SYSTEM)		/* inizializzazione opzione sistema e lista unità di misura */
	{
		case METRIC:
			ads_start_list(hdlg, _T("MeasureUnitsList"), LIST_NEW, 0); // la lista viene cambiata, 3=LIST_NEW
			gsc_add_list(_T("mm"));
			gsc_add_list(_T("cm"));
			gsc_add_list(_T("m"));
			gsc_add_list(_T("km"));
			ads_end_list();
			val = MEASURE_UNIT;
			ads_set_tile(hdlg, _T("MeasureUnitsList"), val.get_name());
			ads_set_tile(hdlg, _T("Metric"), _T("1"));
			break;
		case ENGLISH:
			ads_start_list(hdlg, _T("MeasureUnitsList"), LIST_NEW, 0); // la lista viene cambiata, 3=LIST_NEW
			gsc_add_list(_T("Pollici"));
			gsc_add_list(_T("Piedi"));
			gsc_add_list(_T("Yarde"));
			gsc_add_list(_T("Miglia"));
			ads_end_list();
			val = MEASURE_UNIT;
			ads_set_tile(hdlg, _T("MeasureUnitsList"), val.get_name());
			ads_set_tile(hdlg, _T("English"), _T("1"));
			break;
	}
	/* inizializzazione valori per le unità di misura */
   val = PLOT_UNITS;
	ads_set_tile(hdlg, _T("PlotUnits"), val.get_name());
   val = DRAWING_UNITS;
	ads_set_tile(hdlg, _T("DrawingUnits"), val.get_name());
	ads_start_dialog(hdlg, &dlg_status);	/* run the dialog */
	ads_unload_dialog(dcl_id);
	switch (dlg_status)
	{
		case DLGCANCEL:
			return dlg_status;
			break;
		case DLGOK:	
			return dlg_status;
			break;
		case DLGPREV:
			return dlg_status;
			break;
		default:
			return DLGWAIT;
			break; 
	}
}
// ACTION TILE : click su tasto Help
static void CALLB ScalePlot_help(ads_callback_packet *dcl)
   { gsc_help(IDH_Impostazionedelfattorediscala); } 

/* ******************************************************************* */
//		geo_scale -- GeoScale Dialog
/* ******************************************************************* */
int geo_scale()
{
	ads_hdlg hdlg;
	int dlg_status;
	int dcl_id;

   if (gsc_load_dialog(DCL_FILE.get_name(), &dcl_id) == RTERROR)
   {
		acutPrintf(gsc_msg(42), DCL_FILE.get_name()); // "Error loading %s \n"
		return RTERROR;
   }
	ads_new_dialog(_T("GeoScale"), dcl_id, NULLCB, &hdlg); // Display the GeoScale dialog
	ads_action_tile(hdlg, _T("PaperOrientation"), (CLIENTFUNC) get_paper_orientation);
	ads_action_tile(hdlg, _T("previous"),         (CLIENTFUNC) previous);
	ads_action_tile(hdlg, _T("accept"),           (CLIENTFUNC) conferma);
	ads_action_tile(hdlg, _T("cancel"),           (CLIENTFUNC) annulla);
	ads_action_tile(hdlg, _T("S1"),               (CLIENTFUNC) set_scale1);
	ads_action_tile(hdlg, _T("S2"),               (CLIENTFUNC) set_scale2);
	ads_action_tile(hdlg, _T("S3"),               (CLIENTFUNC) set_scale3);
	ads_action_tile(hdlg, _T("S4"),               (CLIENTFUNC) set_scale4);
	ads_action_tile(hdlg, _T("S5"),               (CLIENTFUNC) set_scale5);
	ads_action_tile(hdlg, _T("S6"),               (CLIENTFUNC) set_scale6);
	ads_action_tile(hdlg, _T("S7"),               (CLIENTFUNC) set_scale7);
	ads_action_tile(hdlg, _T("S8"),               (CLIENTFUNC) set_scale8);
   ads_action_tile(hdlg, _T("help"),             (CLIENTFUNC) ScalePlot_help);

   if (PAPER_ORIENTATION == VERTICAL)
		ads_set_tile(hdlg, _T("VerticalPaperOrientation"), _T("1"));	
	else
		ads_set_tile(hdlg, _T("HorizontalPaperOrientation"), _T("1"));

	initialize_scale_list(hdlg);
	ads_start_dialog(hdlg, &dlg_status);
	ads_unload_dialog(dcl_id);
	switch (dlg_status)
	{
		case DLGCANCEL:
			return dlg_status;
			break;
		case DLGPREV:
			return dlg_status;
		 	break;
		case DLGOK:
			return dlg_status;
		 	break;
		default:
			return DLGWAIT;
			break; 
	}
}

/* ******************************************************************* */
//		geo_paper -- GeoPaper Dialog
/* ******************************************************************* */
int geo_paper()
{
	ads_hdlg hdlg;
	int dlg_status;
	int dcl_id;

   if (gsc_load_dialog(DCL_FILE.get_name(), &dcl_id) == RTERROR)
   {
      acutPrintf(gsc_msg(42), DCL_FILE.get_name()); // "Error loading %s \n"
      return RTERROR;
   }
	ads_new_dialog(_T("GeoPaper"), dcl_id, NULLCB, &hdlg);	/* Display the GeoPaper dialog */
	ads_action_tile(hdlg, _T("accept"), (CLIENTFUNC)conferma);
	ads_action_tile(hdlg, _T("A4"), (CLIENTFUNC)set_paper4);
	ads_action_tile(hdlg, _T("A3"), (CLIENTFUNC)set_paper3);
	ads_action_tile(hdlg, _T("A2"), (CLIENTFUNC)set_paper2);
	ads_action_tile(hdlg, _T("A1"), (CLIENTFUNC)set_paper1);
	ads_action_tile(hdlg, _T("A0"), (CLIENTFUNC)set_paper0);

   ads_action_tile(hdlg, _T("UTENTE"), (CLIENTFUNC)set_paperUtente);
   
   initialize_paper_list(hdlg);
   if (PAPER < 0)
      ads_set_tile(hdlg, _T("err_msg"), gsc_msg(44)); // "Occorre selezionare un foglio"
   
   ads_start_dialog(hdlg, &dlg_status);
	ads_unload_dialog(dcl_id);
	switch (dlg_status)
	{
		case DLGCANCEL:
			return dlg_status;
			break;
		case DLGPREV:
			return dlg_status;
		 	break;
		case DLGOK:
			return dlg_status;
		 	break;
		default:
			return DLGWAIT;
			break; 
	}
}

/* ******************************************************************* */
//		geo_user_dim_paper -- GeoUserDimPaper Dialog
/* ******************************************************************* */
int geo_user_dim_paper(int chk)
{
	ads_hdlg hdlg;
	int      dlg_status;
	int      dcl_id;
	C_STRING val;

   do
   {
      if (gsc_load_dialog(DCL_FILE.get_name(), &dcl_id) == RTERROR)
	   {
		   acutPrintf(gsc_msg(42), DCL_FILE.get_name()); // "Error loading %s \n"
		   return RTERROR;
	   }
	   ads_new_dialog(_T("GeoUserDimPaper"), dcl_id, NULLCB, &hdlg); // Display the GeoUserScale dialog
	   ads_action_tile(hdlg, _T("accept"), (CLIENTFUNC)conferma);
	   ads_action_tile(hdlg, _T("UserLargeness"), (CLIENTFUNC)get_user_largeness);
      ads_action_tile(hdlg, _T("UserHeight"), (CLIENTFUNC)get_user_height);

      val = DIMPAPERS[FOGLIO_UTENTE];
	   ads_set_tile(hdlg, _T("UserLargeness"), val.get_name());
	   
      val = DIMPAPERS[FOGLIO_UTENTE + 1];
	   ads_set_tile(hdlg, _T("UserHeight"), val.get_name());
      
      ads_start_dialog(hdlg, &dlg_status);  
      ads_unload_dialog(dcl_id);

      if (chk)
      {
        	if (PAPER_ORIENTATION == HORIZONTAL)
         {
            if ((DIMPAPERS[FOGLIO_UTENTE] >= PLOT_LARGENESS) &&
                (DIMPAPERS[FOGLIO_UTENTE + 1] >= PLOT_HEIGHT))
                chk = 0;
            else
               chk = 1;
         }      
         else
         {
            if ((DIMPAPERS[FOGLIO_UTENTE+1] >= PLOT_LARGENESS) &&
                (DIMPAPERS[FOGLIO_UTENTE] >= PLOT_HEIGHT))
                chk = 0;
            else
               chk = 1;
         }
      }

      if (chk)
      {
         gsc_ddalert(gsc_msg(293)); // Dimensioni insufficienti o orientamento foglio errato.

         if (PAPER_ORIENTATION == HORIZONTAL)
         {
            if (DIMPAPERS[FOGLIO_UTENTE] < PLOT_LARGENESS)
            {
               DIMPAPERS[FOGLIO_UTENTE] = PLOT_LARGENESS;
               val = DIMPAPERS[FOGLIO_UTENTE];
	            ads_set_tile(hdlg, _T("UserLargeness"), val.get_name());
            }

            if (DIMPAPERS[FOGLIO_UTENTE+1] < PLOT_HEIGHT)
            {
               DIMPAPERS[FOGLIO_UTENTE+1] = PLOT_HEIGHT;
               val = DIMPAPERS[FOGLIO_UTENTE+1];
	            ads_set_tile(hdlg, _T("UserHeight"), val.get_name());
            }
         }
         else
         {
            if (DIMPAPERS[FOGLIO_UTENTE+1] < PLOT_LARGENESS)
            {
               DIMPAPERS[FOGLIO_UTENTE+1] = PLOT_LARGENESS;
               val = DIMPAPERS[FOGLIO_UTENTE+1];
	            ads_set_tile(hdlg, _T("UserLargeness"), val.get_name());
            }

            if (DIMPAPERS[FOGLIO_UTENTE] < PLOT_HEIGHT)
            {
               DIMPAPERS[FOGLIO_UTENTE] = PLOT_HEIGHT;
               val = DIMPAPERS[FOGLIO_UTENTE];
	            ads_set_tile(hdlg, _T("UserHeight"), val.get_name());
            }
         }

         if (DIMPAPERS[FOGLIO_UTENTE] < DIMPAPERS[FOGLIO_UTENTE+1])
         {
            DIMPAPERS[FOGLIO_UTENTE] = DIMPAPERS[FOGLIO_UTENTE+1];
            val = DIMPAPERS[FOGLIO_UTENTE];
	         ads_set_tile(hdlg, _T("UserLargeness"), val.get_name());
         }
      }
   }
   while (chk);

   return dlg_status;
}


/* ******************************************************************* */
//		geo_user_scale -- GeoUserScale Dialog
/* ******************************************************************* */
int geo_user_scale()
{
	ads_hdlg hdlg;
	int      dlg_status;
	int      dcl_id;
	C_STRING val;

   if (gsc_load_dialog(DCL_FILE.get_name(), &dcl_id) == RTERROR)
	{
		acutPrintf(gsc_msg(42), DCL_FILE.get_name()); // "Error loading %s \n"
		return RTERROR;
	}
	ads_new_dialog(_T("GeoUserScale"), dcl_id, NULLCB, &hdlg);	// Display the GeoUserScale dialog
	ads_action_tile(hdlg, _T("accept"), (CLIENTFUNC)conferma);
	ads_action_tile(hdlg, _T("UserScale"), (CLIENTFUNC)get_user_scale);

   val = PLOT_SCALE;
	ads_set_tile(hdlg, _T("UserScale"), val.get_name());
	ads_start_dialog(hdlg, &dlg_status);
	ads_unload_dialog(dcl_id);
	return dlg_status;
}
// ACTION TILE : click su tasto Help
static void CALLB TypePlot_help(ads_callback_packet *dcl)
   { gsc_help(IDH_Sceltatipologiaplottata); } 


/* ******************************************************************* */
//		geo_type -- GeoType Dialog
/* ******************************************************************* */
int geo_type()
{
	ads_hdlg hdlg;
	int dlg_status;
	int dcl_id;

   if (gsc_load_dialog(/*MSG0*/DCL_FILE.get_name(), &dcl_id) == RTERROR)
   {
		acutPrintf(gsc_msg(42), DCL_FILE.get_name()); // "Error loading %s \n"
		return RTERROR;
   }
	ads_new_dialog(_T("GeoType"), dcl_id, NULLCB, &hdlg);	/* Display the GeoScale dialog */
	ads_action_tile(hdlg, _T("previous"), (CLIENTFUNC) previous);
	ads_action_tile(hdlg, _T("accept"),   (CLIENTFUNC) conferma);
	ads_action_tile(hdlg, _T("cancel"),   (CLIENTFUNC) annulla);
	ads_action_tile(hdlg, _T("T1"),       (CLIENTFUNC) set_type1);
	ads_action_tile(hdlg, _T("T2"),       (CLIENTFUNC) set_type2);
	ads_action_tile(hdlg, _T("T3"),       (CLIENTFUNC) set_type3);
	ads_action_tile(hdlg, _T("T4"),       (CLIENTFUNC) set_type4);
	ads_action_tile(hdlg, _T("T5"),       (CLIENTFUNC) set_type5);
	ads_action_tile(hdlg, _T("T6"),       (CLIENTFUNC) set_type6);
   ads_action_tile(hdlg, _T("help"),     (CLIENTFUNC) TypePlot_help);

   ads_mode_tile(hdlg, _T("T5"), 0);	//abilita di nuovo i tile in caso fossero stati disattivati
	ads_mode_tile(hdlg, _T("T6"), 0);
	initialize_type_list(hdlg);
   ads_set_tile(hdlg, _T("err_msg"), GS_EMPTYSTR);

	switch (EXIST_LEGEND) //se non esistono classi per costruire la legenda, vengono disattivate le due
							 //opzioni relative
	{
		case 0:
			ads_mode_tile(hdlg, _T("T5"), 1);
			ads_mode_tile(hdlg, _T("T6"), 1);
         ads_set_tile(hdlg,  _T("err_msg"), gsc_msg(45)); // "non esistono elementi per la legenda"

			break;
		case 1:
			ads_mode_tile(hdlg, _T("T5"), 0);
			ads_mode_tile(hdlg, _T("T6"), 0);
			break;
	}
	//inizializzazione in base al tipo di plottata precedente
	C_STRING type_file;
	short cx,cy;

	type_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\"); 

   switch (PLOT_TYPE)
	{
		case CART_LESS:
			ads_dimensions_tile(hdlg, _T("T1"), &cx,&cy);
			ads_start_image(hdlg, _T("T1"));
			ads_fill_image(0,0,cx,cy,7); //sfondo bianco
			type_file += _T("TYPE1.SLD"); 
			ads_slide_image(0,-25,cx,cy,type_file.get_name());
			ads_end_image();
			break;
		case INT_CART:
			ads_dimensions_tile(hdlg, _T("T2"), &cx,&cy);
			ads_start_image(hdlg, _T("T2"));
			ads_fill_image(0,0,cx,cy,7); //sfondo bianco
			type_file += _T("TYPE2.SLD"); 
			ads_slide_image(0,-25,cx,cy,type_file.get_name());
			ads_end_image();
			break;
		case EXBOTTOM_CART:
			ads_dimensions_tile(hdlg,_T("T3"),&cx,&cy);
			ads_start_image(hdlg, _T("T3"));
			ads_fill_image(0,0,cx,cy,7); //sfondo bianco
			type_file += _T("TYPE3.SLD"); 
			ads_slide_image(0,-25,cx,cy,type_file.get_name());
			ads_end_image();
			break;
		case EXRIGHT_CART: 
			ads_dimensions_tile(hdlg, _T("T4"),&cx,&cy);
			ads_start_image(hdlg, _T("T4"));
			ads_fill_image(0,0,cx,cy,7); //sfondo bianco
			type_file += _T("TYPE4.SLD"); 
			ads_slide_image(0,-25,cx,cy,type_file.get_name());
			ads_end_image();
			break;
		case EXBOTTOMLEG_CART:
         if (!EXIST_LEGEND)
         {
   			PLOT_TYPE = CART_LESS;
   			ads_dimensions_tile(hdlg, _T("T1"),&cx,&cy);
   			ads_start_image(hdlg, _T("T1"));
   			ads_fill_image(0,0,cx,cy,7); //sfondo bianco
   			type_file += _T("TYPE1.SLD"); 
   			ads_slide_image(0,-25,cx,cy,type_file.get_name());
   			ads_end_image();
   			break;
         }    
			ads_dimensions_tile(hdlg, _T("T5"),&cx,&cy);
			ads_start_image(hdlg, _T("T5"));
			ads_fill_image(0,0,cx,cy,7); //sfondo bianco
         type_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\TYPE5.SLD");
			ads_slide_image(0,-25,cx,cy,type_file.get_name());
			ads_end_image();
			break;
		case EXRIGHTLEG_CART:
         if (!EXIST_LEGEND)
         {
   			PLOT_TYPE = CART_LESS;
   			ads_dimensions_tile(hdlg, _T("T1"),&cx,&cy);
   			ads_start_image(hdlg, _T("T1"));
   			ads_fill_image(0,0,cx,cy,7); //sfondo bianco
   			type_file += _T("TYPE1.SLD"); 
   			ads_slide_image(0,-25,cx,cy,type_file.get_name());
   			ads_end_image();
   			break;
         }    
			type_file = GEOsimAppl::WORKDIR; 
			ads_dimensions_tile(hdlg, _T("T6"),&cx,&cy);
			ads_start_image(hdlg, _T("T6"));
			ads_fill_image(0,0,cx,cy,7); //sfondo bianco
         type_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\TYPE6.SLD"); 
			ads_slide_image(0,-25,cx,cy,type_file.get_name());
			ads_end_image();
			break;
		default:
			break;
	}

	ads_start_dialog(hdlg, &dlg_status);
	ads_unload_dialog(dcl_id);
	switch (dlg_status)
	{
		case DLGCANCEL:
			return dlg_status;
			break;
		case DLGPREV:
			return dlg_status;
		 	break;
		case DLGOK:
			return dlg_status;
		 	break;
		default:
			return DLGWAIT;
			break; 
	}
}
// ACTION TILE : click su tasto Help
static void CALLB OptionsPlot_help(ads_callback_packet *dcl)
   { gsc_help(IDH_Opzioniplottata); } 


/* ******************************************************************* */
//		geo_cart_leg -- GeoCartLeg Dialog
/* ******************************************************************* */
int geo_cart_leg()
{
	ads_hdlg hdlg;
	int dlg_status;
	int dcl_id;
	ads_load_dialog(/*MSG0*/DCL_FILE.get_name(), &dcl_id);
    
   if (dcl_id < 0)
   {
		acutPrintf(gsc_msg(42), DCL_FILE.get_name()); // "Error loading %s \n"
		return GS_BAD;
    }
	ads_new_dialog(_T("GeoCartLeg"), dcl_id, NULLCB, &hdlg);	/* Display the GeoCartLeg dialog */
	ads_action_tile(hdlg, _T("previous"),         (CLIENTFUNC) previous);
	ads_action_tile(hdlg, _T("accept"),           (CLIENTFUNC) conferma);
	ads_action_tile(hdlg, _T("cancel"),           (CLIENTFUNC) annulla);
	ads_action_tile(hdlg, _T("PaperUserMargins"), (CLIENTFUNC) get_paper_user_margins);
	ads_action_tile(hdlg, _T("PaperLimits"),      (CLIENTFUNC) get_paper_limits);
	ads_action_tile(hdlg, _T("PaperGrid"),        (CLIENTFUNC) get_paper_grid);
	ads_action_tile(hdlg, _T("UserGridStepX"),    (CLIENTFUNC) get_user_grid_step_x);
	ads_action_tile(hdlg, _T("UserGridStepY"),    (CLIENTFUNC) get_user_grid_step_y);
	ads_action_tile(hdlg, _T("Browse"),           (CLIENTFUNC) browse);
//	ads_action_tile(hdlg, _T("CartAttr"),         (CLIENTFUNC) get_cart_attr);
	ads_action_tile(hdlg, _T("CartName"),         (CLIENTFUNC) get_cart_name);
	ads_action_tile(hdlg, _T("CartBase"),         (CLIENTFUNC) get_cart_base_height);
	ads_action_tile(hdlg, _T("CartHeight"),       (CLIENTFUNC) get_cart_base_height);
	ads_action_tile(hdlg, _T("LegTextHeight"),    (CLIENTFUNC) get_leg_text_height);
	ads_action_tile(hdlg, _T("LegTextInterLine"), (CLIENTFUNC) get_leg_text_inter_line);
	ads_action_tile(hdlg, _T("LegSpaceBlock"),    (CLIENTFUNC) get_leg_space_block);
	ads_action_tile(hdlg, _T("LegSpaceText"),     (CLIENTFUNC) get_leg_space_text);
	ads_action_tile(hdlg, _T("LegRowsNumber"),    (CLIENTFUNC) get_leg_rows_number);
	ads_action_tile(hdlg, _T("LegAvRowsNumber"),  (CLIENTFUNC) get_leg_av_rows_number);
	ads_action_tile(hdlg, _T("LegBlockScale"),    (CLIENTFUNC) get_leg_block_scale);
	ads_action_tile(hdlg, _T("LegManual"),        (CLIENTFUNC) get_leg_manual);
   ads_action_tile(hdlg, _T("help"),             (CLIENTFUNC) OptionsPlot_help);
	//ads_action_tile(hdlg, _T("LegEnt"), (CLIENTFUNC)get_leg_ent);
	//ads_action_tile(hdlg, _T("LegBlock"), (CLIENTFUNC)get_leg_block);
    /* inizializzazione dei tile */

	C_STRING val;
	val = PAPER_USER_MARGINS;
	ads_set_tile(hdlg, _T("PaperUserMargins"), val.get_name());
	val = PAPER_LIMITS;
	ads_set_tile(hdlg, _T("PaperLimits"), val.get_name());
	val = PAPER_GRID;
	ads_set_tile(hdlg, _T("PaperGrid"), val.get_name());

   val = GRID_STEPX;
	ads_set_tile(hdlg, _T("UserGridStepX"), val.get_name());

   val = GRID_STEPY;
	ads_set_tile(hdlg, _T("UserGridStepY"), val.get_name());

   C_STRING grid_unit;
	if (UNITS_SYSTEM==METRIC)
		switch (MEASURE_UNIT)
		{
			case MM:
				grid_unit = _T("MM");
				break;
			case CM:
				grid_unit = _T("CM");
				break;
			case M:
				grid_unit = _T("M");
				break;
			case KM:
				grid_unit = _T("KM");
				break;
	   }
	else
		switch (MEASURE_UNIT)
		{
			case POLLICI:
            grid_unit = gsc_msg(48); // "Pollici"
				break;
			case PIEDI:
				grid_unit = gsc_msg(49); // "Piedi"
				break;
			case YARDE:
				grid_unit = gsc_msg(50); // "Yarde"
				break;
			case MIGLIA:
				grid_unit = gsc_msg(51); // "Miglia"
				break;
	   }

	ads_set_tile(hdlg, _T("GridUnit"), grid_unit.get_name());


	if (PLOT_TYPE==CART_LESS)
	{
	 	ads_mode_tile(hdlg, _T("Cart"),1);
	 	ads_mode_tile(hdlg, _T("Leg"),1);
		ads_mode_tile(hdlg, _T("LegManual"),1);
	}
	else if ( (PLOT_TYPE==EXBOTTOMLEG_CART) || (PLOT_TYPE==EXRIGHTLEG_CART) )
	{
	 	calculate_legend();
	 	ads_mode_tile(hdlg, _T("Cart"),0);
	 	ads_mode_tile(hdlg, _T("Leg"),0);
		ads_set_tile(hdlg, _T("CartName"), CART_NAME.get_name());

      val = CART_BASE;
		ads_set_tile(hdlg, _T("CartBase"), val.get_name());
      val = CART_HEIGHT;
		ads_set_tile(hdlg, _T("CartHeight"), val.get_name());
      val = LEG_TEXT_HEIGHT;
		ads_set_tile(hdlg, _T("LegTextHeight"), val.get_name());
      val = LEG_TEXT_INTER_LINE;
		ads_set_tile(hdlg, _T("LegTextInterLine"), val.get_name());
      val = LEG_SPACE_BLOCK;
		ads_set_tile(hdlg, _T("LegSpaceBlock"), val.get_name());
		val = (int) LEG_SPACE_TEXT; // conversione da int a stringa
		ads_set_tile(hdlg, _T("LegSpaceText"), val.get_name());
		val = LEG_ROWS_NUMBER; // conversione da int a stringa
		ads_set_tile(hdlg, _T("LegRowsNumber"), val.get_name());
		val = LEG_AV_ROWS_NUMBER; // conversione da int a stringa
		ads_set_tile(hdlg, _T("LegAvRowsNumber"), val.get_name());
      val = LEG_BLOCK_SCALE;
		ads_set_tile(hdlg, _T("LegBlockScale"), val.get_name());
		
		
		val = LEG_MANUAL; //conversione da int a stringa
		ads_set_tile(hdlg, _T("LegManual"), val.get_name());
      if (LEG_MANUAL == 1)
   	 	ads_mode_tile(hdlg, _T("Leg"), 1); // disabilita la legenda automatica
      else
		 	ads_mode_tile(hdlg, _T("Leg"), 0); // abilita la legenda automatica
	
		switch (LEG_ENT)
		{
			case 1:
				ads_set_tile(hdlg, _T("LegEnt"), _T("1"));
				break;
			case 0:
				ads_set_tile(hdlg, _T("LegBlock"), _T("1"));
				break;
		}
	}
	else
	{
	 	ads_mode_tile(hdlg, _T("Cart"), 0);
	 	ads_mode_tile(hdlg, _T("Leg"), 1);
		ads_mode_tile(hdlg, _T("LegManual"), 1);
		ads_set_tile(hdlg, _T("CartName"), CART_NAME.get_name());
      val = CART_BASE;
		ads_set_tile(hdlg, _T("CartBase"), val.get_name());
      val = CART_HEIGHT;
		ads_set_tile(hdlg, _T("CartHeight"), val.get_name());
      val = LEG_TEXT_HEIGHT;
	}
	ads_start_dialog(hdlg, &dlg_status);
	ads_unload_dialog(dcl_id);
	switch (dlg_status)
	{
		case DLGCANCEL:
			return dlg_status;
			break;
		case DLGPREV:
			return dlg_status;
		 	break;
		case DLGOK:
			return dlg_status;
			break;
		default:
			return DLGWAIT;
			break; 
	}
}

/* ******************************************************************* */
//		geo_cart_attr -- GeoCartAttr Dialog
/* ******************************************************************* */
int geo_cart_attr()
{
	ads_hdlg hdlg;
	int dlg_status;
	int dcl_id;
	ads_load_dialog(DCL_FILE.get_name(), &dcl_id);
	if (dcl_id < 0) {
		acutPrintf(gsc_msg(42), DCL_FILE.get_name()); // "Error loading %s \n"
		return RTERROR;
	}
	ads_new_dialog(_T("GeoCartAttr"), dcl_id, NULLCB, &hdlg);	/* Display the GeoUserScale dialog */
	ads_action_tile(hdlg, _T("AttrManual"), (CLIENTFUNC)get_manual_attr);
	ads_action_tile(hdlg, _T("insert"), (CLIENTFUNC)get_new_attr);
	ads_action_tile(hdlg, _T("delete"), (CLIENTFUNC)del_attr);
	ads_action_tile(hdlg, _T("modify"), (CLIENTFUNC)get_mod_attr);
	ads_action_tile(hdlg, _T("accept"), (CLIENTFUNC)conferma);

   ATTR_LIST_ITEM *item = new ATTR_LIST_ITEM(ATTR_TYPE,CART_ATTR_VALUE.get_name());
   USER_ATTR_LIST.add_tail(item);  //controllare se lo inserisce in fondo
   ads_start_list(hdlg, _T("ListAttr"), LIST_NEW, 0);  //la lista viene cambiata, 3=LIST_NEW
   //scorre la lista USER_ATTR_LIST
   item=(ATTR_LIST_ITEM*)USER_ATTR_LIST.get_head();
   while (item!=NULL)
   {
      gsc_add_list(item->attr_value.get_name());
    	item=(ATTR_LIST_ITEM *)USER_ATTR_LIST.get_next();
   }
   ads_end_list();


	//inizializzazione in base al valore di CART_ATTR_MANUAL
	//funzioni per i valori degli attributi  CART_ATTRn

	ads_start_dialog(hdlg, &dlg_status);
	ads_unload_dialog(dcl_id);
	return dlg_status;
}

/* ******************************************************************* */
//		geo_cart_attr_val -- GeoCartAttrVal Dialog
/* ******************************************************************* */
int geo_cart_attr_val()
{
	ads_hdlg hdlg;
	int dlg_status;
	int dcl_id;

   if (gsc_load_dialog(DCL_FILE.get_name(), &dcl_id) == RTERROR)
	{
		acutPrintf(gsc_msg(42), DCL_FILE.get_name()); // "Error loading %s \n"
		return RTERROR;
	}
	ads_new_dialog(_T("GeoCartAttrVal"), dcl_id, NULLCB, &hdlg);	/* Display the GeoUserScale dialog */
	ads_action_tile(hdlg, _T("Value"), (CLIENTFUNC)attr_value_option);
	ads_action_tile(hdlg, _T("Date"), (CLIENTFUNC)attr_date_option);
	ads_action_tile(hdlg, _T("Hour"), (CLIENTFUNC)attr_hour_option);
	ads_action_tile(hdlg, _T("Scale"), (CLIENTFUNC)attr_scale_option);
	ads_action_tile(hdlg, _T("ValIns"), (CLIENTFUNC)insert_value_attr);
	ads_action_tile(hdlg, _T("accept"), (CLIENTFUNC)conferma);
	
   //inizializzazione in base ai valori di ATTR_TYPE e CART_ATTR_VALUE
   switch (ATTR_TYPE)
   {
      case ATTR_VALUE:
         ads_set_tile(hdlg, _T("Value"), _T("1"));
         ads_mode_tile(hdlg, _T("ValIns"), 0);
         ads_set_tile(hdlg, _T("ValIns"), CART_ATTR_VALUE.get_name());
         break;
      case ATTR_DATE:
         ads_set_tile(hdlg, _T("Date"), _T("1"));
         ads_mode_tile(hdlg, _T("ValIns"), 1);
         break;
      case ATTR_HOUR:
         ads_set_tile(hdlg, _T("Hour"), _T("1"));
         ads_mode_tile(hdlg, _T("ValIns"), 1);
         break;
      case ATTR_SCALE:
         ads_set_tile(hdlg, _T("Scale"), _T("1"));
         ads_mode_tile(hdlg, _T("ValIns"), 1);
         break;
      default:
         break;
   }

	ads_start_dialog(hdlg, &dlg_status);
	ads_unload_dialog(dcl_id);
	return dlg_status;
}
// ACTION TILE : click su tasto Help
static void CALLB PaperPlot_help(ads_callback_packet *dcl)
   { gsc_help(IDH_Sceltadelformatodelfoglio); } 


/* ******************************************************************* */
//		geo_userpaper -- Geo Dialog    solo se la scala non è ottimizzata
/* ****************************UserPaper****************************** */
int geo_userpaper()
{
	ads_hdlg hdlg;
	int dlg_status;
	int dcl_id;

   if (gsc_load_dialog(DCL_FILE.get_name(), &dcl_id) == RTERROR)
   {
		acutPrintf(gsc_msg(42), DCL_FILE.get_name()); // "Error loading %s \n"
      return RTERROR;
   }
	ads_new_dialog(_T("GeoUserPaper"), dcl_id, NULLCB, &hdlg);	// Display the GeoPaper dialog
	
	ads_action_tile(hdlg, _T("previous"), (CLIENTFUNC) previous);
	ads_action_tile(hdlg, _T("accept"),   (CLIENTFUNC) conferma);
	ads_action_tile(hdlg, _T("cancel"),   (CLIENTFUNC) annulla);
	ads_action_tile(hdlg, _T("UA4"),      (CLIENTFUNC) set_userpaper4);
	ads_action_tile(hdlg, _T("UA3"),      (CLIENTFUNC) set_userpaper3);
	ads_action_tile(hdlg, _T("UA2"),      (CLIENTFUNC) set_userpaper2);
	ads_action_tile(hdlg, _T("UA1"),      (CLIENTFUNC) set_userpaper1);
	ads_action_tile(hdlg, _T("UA0"),      (CLIENTFUNC) set_userpaper0);

	ads_action_tile(hdlg, _T("UFOGLIO"),      (CLIENTFUNC) set_userpaper);
   
   ads_action_tile(hdlg, _T("help"),     (CLIENTFUNC) PaperPlot_help);
   
   calculate_draw_dim();
   paper_scale_calculate();
   HUPAPER=HPAPER;
   VUPAPER=VPAPER;
   initialize_user_paper_list(hdlg);

   
   ads_start_dialog(hdlg, &dlg_status);
	ads_unload_dialog(dcl_id);
	switch (dlg_status)
	{
		case DLGCANCEL:
			return dlg_status;
			break;
		case DLGPREV:
			return dlg_status;
		 	break;
		case DLGOK:
			return dlg_status;
		 	break;
		default:
			return DLGWAIT;
			break; 
	}
}

/* ******************************************************************* */
//		geo_limit_text_height -- GeoLimitTextHeight Dialog
/* ******************************************************************* */
int geo_limit_text_height()
{
	ads_hdlg hdlg;
	int      dlg_status;
	int      dcl_id;
	C_STRING val;

   if (gsc_load_dialog(DCL_FILE.get_name(), &dcl_id) == RTERROR)
	{
		acutPrintf(gsc_msg(42), DCL_FILE.get_name()); // "Error loading %s \n"
		return RTERROR;
	}
	ads_new_dialog(_T("GeoLimitTextHeight"), dcl_id, NULLCB, &hdlg); // Display the GeoUserScale dialog
	ads_action_tile(hdlg, _T("accept"), (CLIENTFUNC)conferma);
	ads_action_tile(hdlg, _T("LimitTextHeight"), (CLIENTFUNC)get_limit_text_height);    

	if (LIMIT_TEXT_HEIGHT == 0)
		LIMIT_TEXT_HEIGHT = 2; // valore di default
   val = LIMIT_TEXT_HEIGHT; // conversione da int a stringa

	ads_set_tile(hdlg, _T("LimitTextHeight"), val.get_name());
	
	ads_start_dialog(hdlg, &dlg_status);
	ads_unload_dialog(dcl_id);
	return dlg_status;
}


/* ******************************************************************* */
//		correct_cart_leg_values -- test sui valori inseriti dall'utente 
//											per le impostazioni della legenda
/* ******************************************************************* */
int correct_cart_leg_values()
{
	int v1=(CART_BASE>0);
	int v2=(CART_HEIGHT>0);
	int v3=(LEG_TEXT_HEIGHT>0);
	int v4=(LEG_TEXT_INTER_LINE>LEG_TEXT_HEIGHT);
	int v5=(LEG_SPACE_BLOCK>0);
	int v6=(LEG_SPACE_BLOCK<LEG_TEXT_INTER_LINE);
	int v7=(LEG_BLOCK_SCALE>0);
	int v8=(CART_BASE<(PLOT_LARGENESS-2*PAPER_MARGINS));
	int v9=(CART_HEIGHT<(PLOT_HEIGHT-2*PAPER_MARGINS));
   int v10=(((LEG_ROWS_NUMBER<=LEG_AV_ROWS_NUMBER)&&(LEG_MANUAL!=1))||(LEG_MANUAL==1));

	switch (PLOT_TYPE)
	{
		case CART_LESS:
			return GS_GOOD;
			break;
		case INT_CART:
         if (!v1)
            PLOT_ERRORS+=gsc_msg(565);  //"Base del cartiglio non valida\n"
         if (!v2)
            PLOT_ERRORS+=gsc_msg(566); //"Altezza del cartiglio non valida\n"
         if (!v8)
            PLOT_ERRORS+=gsc_msg(572);  //"Base cartiglio troppo grande\n"
         if (!v9)
            PLOT_ERRORS+=gsc_msg(573);  //"Altezza cartiglio troppo grande\n"
			return(v1 && v2 && v8 && v9);
			break;
		case EXBOTTOM_CART:
         if (!v1)
            PLOT_ERRORS+=gsc_msg(565);  //"Base del cartiglio non valida\n"
         if (!v2)
            PLOT_ERRORS+=gsc_msg(566); //"Altezza del cartiglio non valida\n"
         if (!v8)
            PLOT_ERRORS+=gsc_msg(572);  //"Base cartiglio troppo grande\n"
         if (!v9)
            PLOT_ERRORS+=gsc_msg(573);  //"Altezza cartiglio troppo grande\n"
			return(v1 && v2 && v8 && v9);
			break;
		case EXRIGHT_CART:
         if (!v1)
            PLOT_ERRORS+=gsc_msg(565); //"Base del cartiglio non valida\n"
         if (!v2)
            PLOT_ERRORS+=gsc_msg(566); //"Altezza del cartiglio non valida\n"
         if (!v8)
            PLOT_ERRORS+=gsc_msg(572);  //"Base cartiglio troppo grande\n"
         if (!v9)
            PLOT_ERRORS+=gsc_msg(573);  //"Altezza cartiglio troppo grande\n"
			return(v1 && v2 && v8 && v9);
			break;
		case EXBOTTOMLEG_CART:
         if (!v1)
            PLOT_ERRORS+=gsc_msg(565); //"Base del cartiglio non valida\n"
         if (!v2)
            PLOT_ERRORS+=gsc_msg(566); //"Altezza del cartiglio non valida\n"
         if (!v8)
            PLOT_ERRORS+=gsc_msg(572); //"Base cartiglio troppo grande\n"
         if (!v9)
            PLOT_ERRORS+=gsc_msg(573); //"Altezza cartiglio troppo grande\n"
         if (!LEG_MANUAL)
         {
            if (!v3)
            {
               PLOT_ERRORS+=gsc_msg(567); //"Altezza testo non valida"
               PLOT_ERRORS+= _T('\n');
            }
            if (!v4)
               PLOT_ERRORS+=gsc_msg(568); //"Altezza testo superiore all'interlinea\n"
            if (!v5)
            {
               PLOT_ERRORS+=gsc_msg(569); //"Spazio blocco non valido"
               PLOT_ERRORS+= _T('\n');
            }
            if (!v6)
               PLOT_ERRORS+=gsc_msg(570); //"Spazio blocco superiore all'interlinea\n"
            if (!v7)
            {
               PLOT_ERRORS+=gsc_msg(571); //"Scala blocco non valida"
               PLOT_ERRORS+= _T('\n');
            }
            if (!v10)
               PLOT_ERRORS+=gsc_msg(574); //"Spazio insufficiente per la legenda\n"
      		return(v1 && v2 && v3 && v4 && v5 && v6 && v7 && v8 && v9 && v10);
         }
         else
   			return(v1 && v2 && v8 && v9);
			break;
		case EXRIGHTLEG_CART:
         if (!v1)
            PLOT_ERRORS+=gsc_msg(565); //"Base del cartiglio non valida\n"
         if (!v2)
            PLOT_ERRORS+=gsc_msg(566); //"Altezza del cartiglio non valida\n"
         if (!v8)
            PLOT_ERRORS+=gsc_msg(572); //"Base cartiglio troppo grande\n"
         if (!v9)
            PLOT_ERRORS+=gsc_msg(573); //"Altezza cartiglio troppo grande\n"
         if (!LEG_MANUAL)
         {
            if (!v3)
            {
               PLOT_ERRORS+=gsc_msg(567); //"Altezza testo non valida"
               PLOT_ERRORS+= _T('\n');
            }
            if (!v4)
               PLOT_ERRORS+=gsc_msg(568); //"Altezza testo superiore all'interlinea\n"
            if (!v5)
            {
               PLOT_ERRORS+=gsc_msg(569); //"Spazio blocco non valido"
               PLOT_ERRORS+= _T('\n');
            }
            if (!v6)
               PLOT_ERRORS+=gsc_msg(570); //"Spazio blocco superiore all'interlinea\n"
            if (!v7)
            {
               PLOT_ERRORS+=gsc_msg(571); //"Scala blocco non valida"
               PLOT_ERRORS+= _T('\n');
            }
            if (!v10)
               PLOT_ERRORS+=gsc_msg(574); //"Spazio insufficiente per la legenda\n"
   			return(v1 && v2 && v3 && v4 && v5 && v6 && v7 && v8 && v9 && v10);
         }
         else
   			return(v1 && v2 && v8 && v9);
			break;
		default:
			return GS_GOOD;
			break;
	}
}

/* ******************************************************************* */
//		geo_cart_leg_correction -- GeoCartLegCorrection Dialog
/* ******************************************************************* */
int geo_cart_leg_correction()
{
	ads_hdlg hdlg;
	int dlg_status;
	int dcl_id;

   if (gsc_load_dialog(DCL_FILE.get_name(), &dcl_id) == RTERROR)
   {
		acutPrintf(gsc_msg(42), DCL_FILE.get_name()); // "Error loading %s \n"
		return RTERROR;
   }
	ads_new_dialog(_T("GeoCartLegCorrection"), dcl_id, NULLCB, &hdlg);	/* Display the GeoCartLegCorrection dialog */
	ads_action_tile(hdlg, _T("accept"), (CLIENTFUNC)conferma);	 
	//visualizza un messaggio diverso a seconda dell'errore nel tile message
	ads_set_tile(hdlg, _T("message"), PLOT_ERRORS.get_name());
	ads_start_dialog(hdlg, &dlg_status);
	ads_unload_dialog(dcl_id);
	return dlg_status;
}
// ACTION TILE : click su tasto Help
static void CALLB LayoutPlot_help(ads_callback_packet *dcl)
   { gsc_help(IDH_Layoutdipagina); } 


/* ******************************************************************* */
//		geo_layout -- GeoLayout Dialog
/* ******************************************************************* */
int geo_layout()
{
	ads_hdlg hdlg;
	int dlg_status;
	int dcl_id;

   if (gsc_load_dialog(DCL_FILE.get_name(), &dcl_id) == RTERROR)
   {
		acutPrintf(gsc_msg(42), DCL_FILE.get_name()); // "Error loading %s \n"
      return RTERROR;
   }
	ads_new_dialog(_T("GeoLayout"), dcl_id, NULLCB, &hdlg);	/* Display the GeoLayout dialog */
	ads_action_tile(hdlg, _T("previous"), (CLIENTFUNC) previous);
	ads_action_tile(hdlg, _T("accept"),   (CLIENTFUNC) conferma);
	ads_action_tile(hdlg, _T("cancel"),   (CLIENTFUNC) annulla);

   ads_action_tile(hdlg, _T("save"),   (CLIENTFUNC) save_ini_plot_file);
   
   ads_action_tile(hdlg, _T("BL"),       (CLIENTFUNC) set_layout1);
	ads_action_tile(hdlg, _T("TL"),       (CLIENTFUNC) set_layout2);
	ads_action_tile(hdlg, _T("BR"),       (CLIENTFUNC) set_layout3);
	ads_action_tile(hdlg, _T("TR"),       (CLIENTFUNC) set_layout4);
	ads_action_tile(hdlg, _T("CD"),       (CLIENTFUNC) set_layout5);
   ads_action_tile(hdlg, _T("help"),     (CLIENTFUNC) LayoutPlot_help);
   
   if (INIPLOTFILE.len() > 0)
      ads_set_tile(hdlg, _T("INIPlotFile"), INIPLOTFILE.get_name());   
      
   initialize_layout_list(hdlg);
   
   ads_start_dialog(hdlg, &dlg_status);
	ads_unload_dialog(dcl_id);
	switch (dlg_status)
	{
		case DLGCANCEL:
			return dlg_status;
			break;
		case DLGPREV:
			return dlg_status;
		 	break;
		case DLGOK:
			return dlg_status;
		 	break;
		default:
			return DLGWAIT;
			break; 
	}
}

/*************************************************************************/
/*  FUNZIONI accessorie                                                  */
/*************************************************************************/

/* ******************************************************************* */
//		initialize_scale_list
/* ******************************************************************* */
int initialize_scale_list(ads_hdlg hdlg)
{
	C_STRING scale_file;
	short cx,cy;

	ads_dimensions_tile(hdlg, _T("S1"), &cx,&cy);
	ads_start_image(hdlg, _T("S1"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   scale_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\SCALE1.SLD"); 
	ads_slide_image(0,-25,cx,cy,scale_file.get_name());
	ads_end_image();

	ads_dimensions_tile(hdlg, _T("S2"),&cx,&cy);
	ads_start_image(hdlg, _T("S2"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   scale_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\SCALE2.SLD"); 
	ads_slide_image(0,-25,cx,cy,scale_file.get_name());
	ads_end_image();

	ads_dimensions_tile(hdlg, _T("S3"),&cx,&cy);
	ads_start_image(hdlg, _T("S3"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   scale_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\SCALE3.SLD"); 
	ads_slide_image(0,-25,cx,cy,scale_file.get_name());
	ads_end_image();

	ads_dimensions_tile(hdlg, _T("S4"),&cx,&cy);
	ads_start_image(hdlg, _T("S4"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   scale_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\SCALE4.SLD"); 
	ads_slide_image(0,-25,cx,cy,scale_file.get_name());
	ads_end_image();

	ads_dimensions_tile(hdlg, _T("S5"),&cx,&cy);
	ads_start_image(hdlg, _T("S5"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   scale_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\SCALE5.SLD"); 
	ads_slide_image(0,-25,cx,cy,scale_file.get_name());
	ads_end_image();

	ads_dimensions_tile(hdlg, _T("S6"),&cx,&cy);
	ads_start_image(hdlg, _T("S6"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   scale_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\SCALE6.SLD"); 
	ads_slide_image(0,-25,cx,cy,scale_file.get_name());
	ads_end_image();

	ads_dimensions_tile(hdlg, _T("S7"),&cx,&cy);
	ads_start_image(hdlg, _T("S7"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   scale_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\SCALE7.SLD"); 
	ads_slide_image(0,-25,cx,cy,scale_file.get_name());
	ads_end_image();

	ads_dimensions_tile(hdlg, _T("S8"),&cx,&cy);
	ads_start_image(hdlg, _T("S8"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   scale_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\SCALE8.SLD"); 
	ads_slide_image(0,-25,cx,cy,scale_file.get_name());
	ads_end_image();

   scale_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\"); 
   switch (SCALE_TYPE)
   {
      case UNO_CENTO:
      	ads_dimensions_tile(hdlg, _T("S1"),&cx,&cy);
      	ads_start_image(hdlg, _T("S1"));
      	ads_fill_image(0,0,cx,cy,7); //sfondo bianco
      	scale_file += _T("SCALE1.SLD"); 
      	ads_slide_image(0,-25,cx,cy,scale_file.get_name());
      	ads_end_image();
         break;
      case UNO_DUECENTO:
      	ads_dimensions_tile(hdlg, _T("S2"),&cx,&cy);
      	ads_start_image(hdlg, _T("S2"));
      	ads_fill_image(0,0,cx,cy,7); //sfondo bianco
      	scale_file += _T("SCALE2.SLD"); 
      	ads_slide_image(0,-25,cx,cy,scale_file.get_name());
      	ads_end_image();
         break;
      case UNO_CINQUECENTO:
      	ads_dimensions_tile(hdlg, _T("S3"),&cx,&cy);
      	ads_start_image(hdlg, _T("S3"));
      	ads_fill_image(0,0,cx,cy,7); //sfondo bianco
      	scale_file += _T("SCALE3.SLD"); 
      	ads_slide_image(0,-25,cx,cy,scale_file.get_name());
      	ads_end_image();
         break;
      case UNO_MILLE:
      	ads_dimensions_tile(hdlg, _T("S4"),&cx,&cy);
      	ads_start_image(hdlg, _T("S4"));
      	ads_fill_image(0,0,cx,cy,7); //sfondo bianco
      	scale_file += _T("SCALE4.SLD"); 
      	ads_slide_image(0,-25,cx,cy,scale_file.get_name());
      	ads_end_image();
         break;
      case UNO_DUEMILA:
      	ads_dimensions_tile(hdlg, _T("S5"),&cx,&cy);
      	ads_start_image(hdlg, _T("S5"));
      	ads_fill_image(0,0,cx,cy,7); //sfondo bianco
      	scale_file += _T("SCALE5.SLD"); 
      	ads_slide_image(0,-25,cx,cy,scale_file.get_name());
      	ads_end_image();
         break;
      case UNO_CINQUEMILA:
      	ads_dimensions_tile(hdlg, _T("S6"),&cx,&cy);
      	ads_start_image(hdlg, _T("S6"));
      	ads_fill_image(0,0,cx,cy,7); //sfondo bianco
      	scale_file += _T("SCALE6.SLD"); 
      	ads_slide_image(0,-25,cx,cy,scale_file.get_name());
      	ads_end_image();
         break;
      case OTTIMIZZATA:
      	ads_dimensions_tile(hdlg, _T("S7"),&cx,&cy);
      	ads_start_image(hdlg, _T("S7"));
      	ads_fill_image(0,0,cx,cy,7); //sfondo bianco
      	scale_file += _T("SCALE7.SLD"); 
      	ads_slide_image(0,-25,cx,cy,scale_file.get_name());
      	ads_end_image();
         break;
      case UTENTE:
      	ads_dimensions_tile(hdlg, _T("S8"),&cx,&cy);
      	ads_start_image(hdlg, _T("S8"));
      	ads_fill_image(0,0,cx,cy,7); //sfondo bianco
      	scale_file += _T("SCALE8.SLD"); 
      	ads_slide_image(0,-25,cx,cy,scale_file.get_name());
      	ads_end_image();
         break;
      default:
         break;
   }

	return GS_GOOD;
}

/* ******************************************************************* */
//		initialize_paper_list
/* ******************************************************************* */
int initialize_paper_list(ads_hdlg hdlg)
{
	C_STRING paper_file;
	short cx,cy;

	ads_dimensions_tile(hdlg, _T("A0"),&cx,&cy);
	ads_start_image(hdlg, _T("A0"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   paper_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\A0.SLD"); 
	ads_slide_image(25,-40,cx,cy,paper_file.get_name());
	ads_end_image();

	ads_dimensions_tile(hdlg, _T("A1"),&cx,&cy);
	ads_start_image(hdlg, _T("A1"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   paper_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\A1.SLD"); 
	ads_slide_image(25,-40,cx,cy,paper_file.get_name());
	ads_end_image();

	ads_dimensions_tile(hdlg, _T("A2"),&cx,&cy);
	ads_start_image(hdlg, _T("A2"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   paper_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\A2.SLD"); 
	ads_slide_image(25,-40,cx,cy,paper_file.get_name());
	ads_end_image();

	ads_dimensions_tile(hdlg, _T("A3"),&cx,&cy);
	ads_start_image(hdlg, _T("A3"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   paper_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\A3.SLD"); 
	ads_slide_image(25,-40,cx,cy,paper_file.get_name());
	ads_end_image();

   ads_dimensions_tile(hdlg, _T("A4"),&cx,&cy);
	ads_start_image(hdlg, _T("A4"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   paper_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\40.SLD"); 
	ads_slide_image(25,-40,cx,cy,paper_file.get_name());
	ads_end_image();

   ads_dimensions_tile(hdlg, _T("UTENTE"),&cx,&cy);
	ads_start_image(hdlg, _T("UTENTE"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   paper_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\F_UTENTE.SLD"); 
	ads_slide_image(25,-40,cx,cy,paper_file.get_name());
	ads_end_image();

   paper_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\"); 

   switch (PAPER)
   {
      case A0:
      	ads_dimensions_tile(hdlg, _T("A0"),&cx,&cy);
      	ads_start_image(hdlg, _T("A0"));
      	ads_fill_image(0,0,cx,cy,7); //sfondo bianco
      	paper_file += _T("A0.SLD"); 
      	ads_slide_image(25,-40,cx,cy,paper_file.get_name());
      	ads_end_image();
         break;
      case A1:
      	ads_dimensions_tile(hdlg, _T("A1"),&cx,&cy);
      	ads_start_image(hdlg, _T("A1"));
      	ads_fill_image(0,0,cx,cy,7); //sfondo bianco
      	paper_file += _T("A1.SLD"); 
      	ads_slide_image(25,-40,cx,cy,paper_file.get_name());
      	ads_end_image();
         break;
      case A2:
      	ads_dimensions_tile(hdlg, _T("A2"),&cx,&cy);
      	ads_start_image(hdlg, _T("A2"));
      	ads_fill_image(0,0,cx,cy,7); //sfondo bianco
      	paper_file += _T("A2.SLD"); 
      	ads_slide_image(25,-40,cx,cy,paper_file.get_name());
      	ads_end_image();
         break;
      case A3:
      	ads_dimensions_tile(hdlg, _T("A3"),&cx,&cy);
      	ads_start_image(hdlg, _T("A3"));
      	ads_fill_image(0,0,cx,cy,7); //sfondo bianco
      	paper_file += _T("A3.SLD"); 
      	ads_slide_image(25,-40,cx,cy,paper_file.get_name());
      	ads_end_image();
         break;
      case A4:
      	ads_dimensions_tile(hdlg, _T("A4"),&cx,&cy);
      	ads_start_image(hdlg, _T("A4"));
      	ads_fill_image(0,0,cx,cy,7); //sfondo bianco
      	paper_file += _T("A4.SLD"); 
      	ads_slide_image(25,-40,cx,cy,paper_file.get_name());
      	ads_end_image();
         break;
      case FOGLIO_UTENTE:
      	ads_dimensions_tile(hdlg, _T("UTENTE"),&cx,&cy);
      	ads_start_image(hdlg, _T("UTENTE"));
      	ads_fill_image(0,0,cx,cy,7); //sfondo bianco
      	paper_file += _T("F_UTENTE.SLD"); 
      	ads_slide_image(25,-40,cx,cy,paper_file.get_name());
      	ads_end_image();
         break;
      default:
         break;
   }

   return GS_GOOD;
}

/* ******************************************************************* */
//		initialize_user_paper_list
/* ******************************************************************* */
int initialize_user_paper_list(ads_hdlg hdlg)
{
	C_STRING paper_file;
	short cx,cy;

	ads_dimensions_tile(hdlg, _T("UA0"),&cx,&cy);
	ads_start_image(hdlg, _T("UA0"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   paper_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\A0.SLD"); 
	ads_slide_image(25,-40,cx,cy,paper_file.get_name());
	ads_end_image();

	ads_dimensions_tile(hdlg, _T("UA1"),&cx,&cy);
	ads_start_image(hdlg, _T("UA1"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   paper_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\A1.SLD"); 
	ads_slide_image(25,-40,cx,cy,paper_file.get_name());
	ads_end_image();

	ads_dimensions_tile(hdlg, _T("UA2"),&cx,&cy);
	ads_start_image(hdlg, _T("UA2"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   paper_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\A2.SLD"); 
	ads_slide_image(25,-40,cx,cy,paper_file.get_name());
	ads_end_image();

	ads_dimensions_tile(hdlg, _T("UA3"),&cx,&cy);
	ads_start_image(hdlg, _T("UA3"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   paper_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\A3.SLD"); 
	ads_slide_image(25,-40,cx,cy,paper_file.get_name());
	ads_end_image();

	ads_dimensions_tile(hdlg, _T("UA4"),&cx,&cy);
	ads_start_image(hdlg, _T("UA4"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   paper_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\A4.SLD"); 
	ads_slide_image(25,-40,cx,cy,paper_file.get_name());
	ads_end_image();

	ads_dimensions_tile(hdlg, _T("UFOGLIO"),&cx,&cy);
	ads_start_image(hdlg, _T("UFOGLIO"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   paper_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\F_UTENTE.SLD"); 
	ads_slide_image(25,-40,cx,cy,paper_file.get_name());
	ads_end_image();

   int paper;
	if (PAPER_ORIENTATION==HORIZONTAL)
      paper=HPAPER;
   else
      paper=VPAPER;

   switch (paper)
   {
      case FOGLIO_UTENTE:

   	 	ads_mode_tile(hdlg, _T("UA0"),1);
   	 	ads_mode_tile(hdlg, _T("UA1"),1);
   	 	ads_mode_tile(hdlg, _T("UA2"),1);
   	 	ads_mode_tile(hdlg, _T("UA3"),1);
   	 	ads_mode_tile(hdlg, _T("UA4"),1);
         break;
      case A0:                  
   	 	ads_mode_tile(hdlg, _T("UA0"), 0);
   	 	ads_mode_tile(hdlg, _T("UA1"), 1);
   	 	ads_mode_tile(hdlg, _T("UA2"), 1);
   	 	ads_mode_tile(hdlg, _T("UA3"), 1);
   	 	ads_mode_tile(hdlg, _T("UA4"), 1);
         break;
      case A1:
   	 	ads_mode_tile(hdlg, _T("UA0"), 0);
   	 	ads_mode_tile(hdlg, _T("UA1"), 0);
   	 	ads_mode_tile(hdlg, _T("UA2"), 1);
   	 	ads_mode_tile(hdlg, _T("UA3"), 1);
   	 	ads_mode_tile(hdlg, _T("UA4"), 1);
         break;
      case A2:
   	 	ads_mode_tile(hdlg, _T("UA0"), 0);
   	 	ads_mode_tile(hdlg, _T("UA1"), 0);
   	 	ads_mode_tile(hdlg, _T("UA2"), 0);
   	 	ads_mode_tile(hdlg, _T("UA3"), 1);
   	 	ads_mode_tile(hdlg, _T("UA4"), 1);
         break;
      case A3:
   	 	ads_mode_tile(hdlg, _T("UA0"), 0);
   	 	ads_mode_tile(hdlg, _T("UA1"), 0);
   	 	ads_mode_tile(hdlg, _T("UA2"), 0);
   	 	ads_mode_tile(hdlg, _T("UA3"), 0);
   	 	ads_mode_tile(hdlg, _T("UA4"), 1);
         break;
      case A4:                  //attiva tutti i fogli
   	 	ads_mode_tile(hdlg, _T("UA0"), 0);
   	 	ads_mode_tile(hdlg, _T("UA1"), 0);
   	 	ads_mode_tile(hdlg, _T("UA2"), 0);
   	 	ads_mode_tile(hdlg, _T("UA3"), 0);
   	 	ads_mode_tile(hdlg, _T("UA4"), 0);
         break;
      default:
         break;
   } 

	if (PAPER_ORIENTATION==HORIZONTAL)
      paper=HUPAPER;
   else
      paper=VUPAPER;

   switch (paper)
   {
      case FOGLIO_UTENTE:
        	ads_dimensions_tile(hdlg, _T("UFOGLIO"), &cx,&cy);
      	ads_start_image(hdlg, _T("UFOGLIO"));
      	ads_fill_image(0,0,cx,cy,7); //sfondo bianco
         paper_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\F_UTENTE.SLD"); 
      	ads_slide_image(25,-40,cx,cy,paper_file.get_name());
      	ads_end_image();
         break;
      case A0:
        	ads_dimensions_tile(hdlg, _T("UA0"),&cx,&cy);
      	ads_start_image(hdlg, _T("UA0"));
      	ads_fill_image(0,0,cx,cy,7); //sfondo bianco
         paper_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\A0.SLD"); 
      	ads_slide_image(25,-40,cx,cy,paper_file.get_name());
      	ads_end_image();
         break;
      case A1:
        	ads_dimensions_tile(hdlg, _T("UA1"),&cx,&cy);
      	ads_start_image(hdlg, _T("UA1"));
      	ads_fill_image(0,0,cx,cy,7); //sfondo bianco
         paper_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\A1.SLD"); 
      	ads_slide_image(25,-40,cx,cy,paper_file.get_name());
      	ads_end_image();
         break;
      case A2:
        	ads_dimensions_tile(hdlg, _T("UA2"),&cx,&cy);
      	ads_start_image(hdlg, _T("UA2"));
      	ads_fill_image(0,0,cx,cy,7); //sfondo bianco
         paper_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\A2.SLD"); 
      	ads_slide_image(25,-40,cx,cy,paper_file.get_name());
      	ads_end_image();
         break;
      case A3:
        	ads_dimensions_tile(hdlg, _T("UA3"),&cx,&cy);
      	ads_start_image(hdlg, _T("UA3"));
      	ads_fill_image(0,0,cx,cy,7); //sfondo bianco
         paper_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\A3.SLD"); 
      	ads_slide_image(25,-40,cx,cy,paper_file.get_name());
      	ads_end_image();
         break;
      case A4:
        	ads_dimensions_tile(hdlg, _T("UA4"),&cx,&cy);
      	ads_start_image(hdlg, _T("UA4"));
      	ads_fill_image(0,0,cx,cy,7); //sfondo bianco
         paper_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\A4.SLD"); 
      	ads_slide_image(25,-40,cx,cy,paper_file.get_name());
      	ads_end_image();
         break;
      default:
         break;
   }

   return GS_GOOD;
}


/* ******************************************************************* */
//		initialize_layout_list
/* ******************************************************************* */
int initialize_layout_list(ads_hdlg hdlg)
{
	C_STRING layout_file;
	short cx,cy;

	ads_dimensions_tile(hdlg, _T("BL"),&cx,&cy);
	ads_start_image(hdlg, _T("BL"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   layout_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\BL.SLD"); 
	ads_slide_image(0,-25,cx,cy,layout_file.get_name());
	ads_end_image();

	ads_dimensions_tile(hdlg, _T("TL"),&cx,&cy);
	ads_start_image(hdlg, _T("TL"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   layout_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\TL.SLD"); 
	ads_slide_image(0,-25,cx,cy,layout_file.get_name());
	ads_end_image();

	ads_dimensions_tile(hdlg, _T("BR"),&cx,&cy);
	ads_start_image(hdlg, _T("BR"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   layout_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\BR.SLD"); 
	ads_slide_image(0,-25,cx,cy,layout_file.get_name());
	ads_end_image();

	ads_dimensions_tile(hdlg, _T("TR"),&cx,&cy);
	ads_start_image(hdlg, _T("TR"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   layout_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\TR.SLD"); 
	ads_slide_image(0,-25,cx,cy,layout_file.get_name());
	ads_end_image();

	ads_dimensions_tile(hdlg, _T("CD"),&cx,&cy);
	ads_start_image(hdlg, _T("CD"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   layout_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\CD.SLD"); 
	ads_slide_image(40,0,cx,cy,layout_file.get_name());
	ads_end_image();

   layout_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\"); 

   switch (PLOT_LAYOUT)
   {
      case BOTTOM_LEFT:
      	ads_dimensions_tile(hdlg, _T("BL"),&cx,&cy);
      	ads_start_image(hdlg, _T("BL"));
      	ads_fill_image(0,0,cx,cy,7); //sfondo bianco
      	layout_file += _T("BL.SLD"); 
      	ads_slide_image(0,-25,cx,cy,layout_file.get_name());
      	ads_end_image();
         break;
      case TOP_LEFT:
      	ads_dimensions_tile(hdlg, _T("TL"),&cx,&cy);
      	ads_start_image(hdlg, _T("TL"));
      	ads_fill_image(0,0,cx,cy,7); //sfondo bianco
      	layout_file += _T("TL.SLD");
      	ads_slide_image(0,-25,cx,cy,layout_file.get_name());
      	ads_end_image();
         break;
      case BOTTOM_RIGHT:
      	ads_dimensions_tile(hdlg, _T("BR"),&cx,&cy);
      	ads_start_image(hdlg, _T("BR"));
      	ads_fill_image(0,0,cx,cy,7); //sfondo bianco
      	layout_file += _T("BR.SLD"); 
      	ads_slide_image(0,-25,cx,cy,layout_file.get_name());
      	ads_end_image();
         break;
      case TOP_RIGHT:
      	ads_dimensions_tile(hdlg, _T("TR"),&cx,&cy);
      	ads_start_image(hdlg, _T("TR"));
      	ads_fill_image(0,0,cx,cy,7); //sfondo bianco
      	layout_file += _T("TR.SLD"); 
      	ads_slide_image(0,-25,cx,cy,layout_file.get_name());
      	ads_end_image();
         break;
      case CENTERED:
      	ads_dimensions_tile(hdlg, _T("CD"),&cx,&cy);
      	ads_start_image(hdlg, _T("CD"));
      	ads_fill_image(0,0,cx,cy,7); //sfondo bianco
      	layout_file += _T("CD.SLD"); 
      	ads_slide_image(40,0,cx,cy,layout_file.get_name());
      	ads_end_image();
         break;
      default:
         break;
   }

   return GS_GOOD;
}

/* ******************************************************************* */
//		initialize_type_list
/* ******************************************************************* */
int initialize_type_list(ads_hdlg hdlg)
{
	C_STRING type_file;
	short cx,cy;

	ads_dimensions_tile(hdlg, _T("T1"),&cx,&cy);
	ads_start_image(hdlg, _T("T1"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   type_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\TYPE1.SLD"); 
	ads_slide_image(0,-25,cx,cy,type_file.get_name());
	ads_end_image();

	ads_dimensions_tile(hdlg, _T("T2"),&cx,&cy);
	ads_start_image(hdlg, _T("T2"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   type_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\TYPE2.SLD"); 
	ads_slide_image(0,-25,cx,cy,type_file.get_name());
	ads_end_image();

	ads_dimensions_tile(hdlg, _T("T3"),&cx,&cy);
	ads_start_image(hdlg, _T("T3"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   type_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\TYPE3.SLD"); 
	ads_slide_image(0,-25,cx,cy,type_file.get_name());
	ads_end_image();

	ads_dimensions_tile(hdlg, _T("T4"),&cx,&cy);
	ads_start_image(hdlg, _T("T4"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   type_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\TYPE4.SLD"); 
	ads_slide_image(0,-25,cx,cy,type_file.get_name());
	ads_end_image();

	ads_dimensions_tile(hdlg, _T("T5"),&cx,&cy);
	ads_start_image(hdlg, _T("T5"));
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   type_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\TYPE5.SLD"); 
	ads_slide_image(0,-25,cx,cy,type_file.get_name());
	ads_end_image();

	ads_dimensions_tile(hdlg, _T("T6"),&cx,&cy);
	ads_start_image(hdlg, _T("T6") );
	ads_fill_image(0,0,cx,cy,0); //sfondo nero
   type_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\TYPE6.SLD"); 
	ads_slide_image(0,-25,cx,cy,type_file.get_name());
	ads_end_image();

	return GS_GOOD;
}

/*************************************************************************/
/*  CALLBACK FUNCTIONS                                                   */
/*************************************************************************/

/* ******************************************************************* */
//		funzioni CALLBACK per i bottoni OK(=AVANTI), INDIETRO, ANNULLA
/* ******************************************************************* */
static void 				 /*FCN*/
conferma(ads_callback_packet *cpkt)
{
    /* User pressed previous button to return to the previous dialog. */
   ads_done_dialog(cpkt->dialog,DLGOK);
}

static void 				 /*FCN*/
previous(ads_callback_packet *cpkt)
{
    /* User pressed previous button to return to the previous dialog. */
   ads_done_dialog(cpkt->dialog,DLGPREV);
}

static void              /*FCN*/
annulla(ads_callback_packet *cpkt)
{
    /* User pressed previous button to return to the previous dialog. */
	ads_done_dialog(cpkt->dialog, DLGCANCEL);
}

/* ******************************************************************* */
//		funzione CALLBACK per scegliere il file da caricare
/* ******************************************************************* */
static void              /*FCN*/
select_ini_plot_file(ads_callback_packet *cpkt)
{
   ads_hdlg hdlg = cpkt->dialog;
   C_STRING LastINIPlotFile, DefaultPath, filename;

   // Se non esiste alcun file precedente
   if (gsc_getPathInfoFromINI(_T("LastINIPlotFile"), LastINIPlotFile) == GS_BAD ||
       gsc_dir_exist(LastINIPlotFile) == GS_BAD)
      DefaultPath.clear();
   else
      if (gsc_dir_from_path(LastINIPlotFile, DefaultPath) == GS_BAD) DefaultPath = _T("c:");

   // "GEOsim - Seleziona file" da caricare
	if (gsc_GetFileD(gsc_msg(645), DefaultPath, _T("ini"), UI_LOADFILE_FLAGS, filename) == RTNORM)
	{
		if (load_plot_values(filename.get_name()) == GS_GOOD)
         ads_set_tile(hdlg, _T("INIPlotFile"), filename.get_name());
      else
         ads_set_tile(hdlg, _T("INIPlotFile"), GS_EMPTYSTR);
      // memorizzo la scelta in GEOSIM.INI per riproporla la prossima volta
      gsc_setPathInfoToINI(_T("LastINIPlotFile"), filename);
   }

   return;
}

/* ******************************************************************* */
//		funzioni CALLBACK per scegliere l'area da plottare
/* ******************************************************************* */
static void              /*FCN*/
get_view(ads_callback_packet *cpkt)
{
   ads_done_dialog(cpkt->dialog,DLGSELECT_AREA);
}

static void              /*FCN*/
get_user_area(ads_callback_packet *cpkt)
{
   ads_done_dialog(cpkt->dialog,DLGSELECT_AREA);
}

static void              /*FCN*/
change_view(ads_callback_packet *cpkt)
{
   ads_done_dialog(cpkt->dialog,DLGCHANGE_VIEW);
}

static void 				 /*FCN*/
get_coordinates(ads_callback_packet *cpkt)
{
	 TCHAR val[50];
	 ads_hdlg hdlg=cpkt->dialog;

    /* User change some coordinates values */
	 ads_get_tile(hdlg, _T("CX1"), val,50);
	 ads_distof(val,-1,&PT1[0]);  //conversione da stringa a double
    //ridurre PT1[0] alla seconda cifra decimale
    //settare il tile
    gsc_rtrim_double(&PT1[0]);
    swprintf(val, 50, _T("%f"), PT1[X]);
    ads_set_tile(hdlg, _T("CX1"), val);

	 ads_get_tile(hdlg, _T("CY1"), val, 50);
	 ads_distof(val,-1,&PT1[1]); 
    //ridurre PT1[1] alla seconda cifra decimale
    //settare il tile
    gsc_rtrim_double(&PT1[1]);
    swprintf(val, 50, _T("%f"), PT1[Y]);
    ads_set_tile(hdlg, _T("CY1"), val);
    
    ads_get_tile(hdlg, _T("CX2"), val, 50);
	 ads_distof(val,-1,&PT2[0]);   
    //ridurre PT2[0] alla seconda cifra decimale
    //settare il tile
    gsc_rtrim_double(&PT2[0]);
    swprintf(val, 50, _T("%f"), PT2[X]);
    ads_set_tile(hdlg, _T("CX2"), val);
    
    ads_get_tile(hdlg, _T("CY2"), val, 50);
	 ads_distof(val,-1,&PT2[1]); 
    //ridurre PT2[1] alla seconda cifra decimale
    //settare il tile
    gsc_rtrim_double(&PT2[1]);
    swprintf(val, 50, _T("%f"), PT2[Y]);
    ads_set_tile(hdlg, _T("CY2"), val);
}

/* ******************************************************************* */
//		funzioni CALLBACK per ottenere le impostazioni sulle unità di misura
/* ******************************************************************* */
static void 				 /*FCN*/
MetricUnitsList(ads_callback_packet *cpkt)
{
	ads_hdlg hdlg = cpkt->dialog;
	ads_start_list(hdlg, _T("MeasureUnitsList"), LIST_CHANGE,0);  //la lista viene cambiata, 1=LIST_CHANGE
	gsc_add_list( _T("mm"));
	ads_end_list();
	ads_start_list(hdlg, _T("MeasureUnitsList"), LIST_CHANGE,1);
	gsc_add_list( _T("cm"));
	ads_end_list();
	ads_start_list(hdlg, _T("MeasureUnitsList"), LIST_CHANGE,2);
	gsc_add_list( _T("m"));
	ads_end_list();
	ads_start_list(hdlg, _T("MeasureUnitsList"), LIST_CHANGE,3);
	gsc_add_list( _T("km"));
	ads_end_list();
	ads_set_tile(hdlg, _T("MeasureUnitsList"), _T("mm"));
	UNITS_SYSTEM=METRIC;
}

static void 				 /*FCN*/
EnglishUnitsList(ads_callback_packet *cpkt)
{
	ads_hdlg hdlg=cpkt->dialog;
	ads_start_list(hdlg, _T("MeasureUnitsList"), LIST_CHANGE,0);  //la lista viene cambiata, 1=LIST_CHANGE
	gsc_add_list(_T("Pollici"));
	ads_end_list();
	ads_start_list(hdlg,_T("MeasureUnitsList"), LIST_CHANGE,1); 
	gsc_add_list(_T("Piedi")); 
	ads_end_list();
	ads_start_list(hdlg,_T("MeasureUnitsList"), LIST_CHANGE,2); 
	gsc_add_list(_T("Yarde")); 
	ads_end_list();
	ads_start_list(hdlg,_T("MeasureUnitsList"), LIST_CHANGE,3); 
	gsc_add_list(_T("Miglia"));
	ads_end_list();
	ads_set_tile(hdlg, _T("MeasureUnitsList"), _T("Pollici"));
	UNITS_SYSTEM=ENGLISH;
}

static void 				 /*FCN*/
get_measure_unit(ads_callback_packet *cpkt)
{
	 TCHAR val[50];
	 ads_hdlg hdlg=cpkt->dialog;
     /* User change the units  */
	 ads_get_tile(hdlg, _T("MeasureUnitsList"), val, 50);
	 MEASURE_UNIT=_wtoi(val);  //conversione da stringa a int
}

static void 				 /*FCN*/
get_plot_units(ads_callback_packet *cpkt)
{
   TCHAR val[50];
   ads_hdlg hdlg=cpkt->dialog;

   /* User change the number of plot units */
   ads_get_tile(hdlg, _T("PlotUnits"), val,50);
   double val2;
   ads_distof(val,-1,&val2);//conversione da stringa a double
   if ((val2)==0.0)
   {
      C_STRING temp;
      
      temp = PLOT_UNITS;
      ads_set_tile(hdlg, _T("PlotUnits"), temp.get_name());
   }
   else
      ads_distof(val,-1,&PLOT_UNITS);//conversione da stringa a double
   
   C_STRING val1;
   val1 = PLOT_UNITS;
   if (val1.comp(val))
      ads_set_tile(hdlg, _T("err_msg"), gsc_msg(295)); // "Valore non valido"
   
   swprintf(val, 50, _T("%f"), PLOT_UNITS);
   ads_set_tile(hdlg, _T("PlotUnits"), val);
   ads_set_tile(hdlg, _T("err_msg"), GS_EMPTYSTR);
}

static void get_drawing_units(ads_callback_packet *cpkt) 				 /*FCN*/
{
	 TCHAR val[50];
	 ads_hdlg hdlg=cpkt->dialog;

     /* User change the number of drawing units */
	 ads_get_tile(hdlg, _T("DrawingUnits"), val,50);
	 double val2;
	 ads_distof(val,-1,&val2);//conversione da stringa a double
	 if ((val2)==0.0)
	 {
		C_STRING temp;
      temp = DRAWING_UNITS;
		ads_set_tile(hdlg, _T("DrawingUnits"), temp.get_name());
	 }
	 else
	   ads_distof(val,-1,&DRAWING_UNITS);//conversione da stringa a double
   
   C_STRING val1;
   val1 = DRAWING_UNITS;
   if (val1.comp(val))
      ads_set_tile(hdlg, _T("err_msg"), gsc_msg(295)); // "Valore non valido"
   
   swprintf(val, 50, _T("%f"), DRAWING_UNITS);
	ads_set_tile(hdlg, _T("DrawingUnits"), val);
   ads_set_tile(hdlg, _T("err_msg"), GS_EMPTYSTR);
}

/* ******************************************************************* */
//		funzioni CALLBACK per ottenere la scala e l'orientamento del foglio
/* ******************************************************************* */
static void get_paper_orientation(ads_callback_packet *cpkt)				 /*FCN*/
{
	TCHAR val[2];
	ads_hdlg hdlg=cpkt->dialog;

	// User selected the orientation
	ads_get_tile(hdlg, _T("VerticalPaperOrientation"), val, 2);

   //conversione da stringa a int
	if ((_wtoi(val))==1) PAPER_ORIENTATION=VERTICAL;  
	else PAPER_ORIENTATION=HORIZONTAL;

	initialize_scale_list(hdlg);
}

static int set_scale1(ads_callback_packet *cpkt)				 /*FCN*/
{
 	ads_hdlg hdlg=cpkt->dialog;

   PAPER=-1;
	SCALE_TYPE=UNO_CENTO;
	PLOT_SCALE=100;
	initialize_scale_list(hdlg);

	return GS_GOOD;
}

static int set_scale2(ads_callback_packet *cpkt) 				 /*FCN*/
{
 	ads_hdlg hdlg=cpkt->dialog;

   PAPER=-1;
	SCALE_TYPE=UNO_DUECENTO;
	PLOT_SCALE=200;
	initialize_scale_list(hdlg);

	return GS_GOOD;
}

static int set_scale3(ads_callback_packet *cpkt)				 /*FCN*/
{
 	ads_hdlg hdlg=cpkt->dialog;

   PAPER=-1;
	SCALE_TYPE=UNO_CINQUECENTO;
	PLOT_SCALE=500;
	initialize_scale_list(hdlg);

	return GS_GOOD;
}

static int set_scale4(ads_callback_packet *cpkt)				 /*FCN*/
{
 	ads_hdlg hdlg=cpkt->dialog;

   PAPER=-1;
	SCALE_TYPE=UNO_MILLE;
	PLOT_SCALE=1000;
	initialize_scale_list(hdlg);

	return GS_GOOD;
}

static int set_scale5(ads_callback_packet *cpkt)				 /*FCN*/
{
 	ads_hdlg hdlg=cpkt->dialog;

   PAPER=-1;
	SCALE_TYPE=UNO_DUEMILA;
	PLOT_SCALE=2000;
	initialize_scale_list(hdlg);

	return GS_GOOD;
}

static int set_scale6(ads_callback_packet *cpkt)				 /*FCN*/
{
 	ads_hdlg hdlg=cpkt->dialog;

   PAPER=-1;
	SCALE_TYPE=UNO_CINQUEMILA;
	PLOT_SCALE=5000;
	initialize_scale_list(hdlg);

	return GS_GOOD;
}

static int set_scale7(ads_callback_packet *cpkt)
{
	ads_hdlg hdlg=cpkt->dialog;

	SCALE_TYPE=OTTIMIZZATA;
	initialize_scale_list(hdlg);
	do {
		geo_paper();
	} while (PAPER<0); 

	return GS_GOOD;
}

static int set_scale8(ads_callback_packet *cpkt)				 /*FCN*/
{
 	ads_hdlg hdlg=cpkt->dialog;

	PAPER=-1;
	SCALE_TYPE=UTENTE;
	initialize_scale_list(hdlg);

	do {
		geo_user_scale();
	} while (PLOT_SCALE==0); 

	return GS_GOOD;
}

static int set_paperUtente(ads_callback_packet *cpkt)
{
 	ads_hdlg hdlg=cpkt->dialog;

   PAPER=FOGLIO_UTENTE;
   initialize_paper_list(hdlg);
   ads_set_tile(hdlg, _T("err_msg"), GS_EMPTYSTR);

   //DCL con la richiesta delle dimensioni
   do
   {
      geo_user_dim_paper(1);
   }
   while ((DIMPAPERS[FOGLIO_UTENTE] == 0) || (DIMPAPERS[FOGLIO_UTENTE+1] == 0));

   return GS_GOOD;
}


static int set_paper0(ads_callback_packet *cpkt)
{
 	ads_hdlg hdlg=cpkt->dialog;

   PAPER=A0;
   initialize_paper_list(hdlg);
   ads_set_tile(hdlg, _T("err_msg"), GS_EMPTYSTR);
   return GS_GOOD;
}

static int set_paper1(ads_callback_packet *cpkt)
{
 	ads_hdlg hdlg=cpkt->dialog;

   PAPER=A1;
   initialize_paper_list(hdlg);
   ads_set_tile(hdlg, _T("err_msg"), GS_EMPTYSTR);

   return GS_GOOD;
}

static int set_paper2(ads_callback_packet *cpkt)
{
 	ads_hdlg hdlg=cpkt->dialog;

   PAPER=A2;
   initialize_paper_list(hdlg);
   ads_set_tile(hdlg, _T("err_msg"), GS_EMPTYSTR);

   return GS_GOOD;
}

static int set_paper3(ads_callback_packet *cpkt)
{
 	ads_hdlg hdlg=cpkt->dialog;

   PAPER=A3;
   initialize_paper_list(hdlg);
   ads_set_tile(hdlg, _T("err_msg"), GS_EMPTYSTR);
   return GS_GOOD;
}

static int set_paper4(ads_callback_packet *cpkt)
{
 	ads_hdlg hdlg=cpkt->dialog;

   PAPER=A4;
   initialize_paper_list(hdlg);
   ads_set_tile(hdlg, _T("err_msg"), GS_EMPTYSTR);

   return GS_GOOD;
}

static void get_user_scale(ads_callback_packet *cpkt)				 /*FCN*/
{
	TCHAR val[50];
	ads_hdlg hdlg=cpkt->dialog;
	ads_get_tile(hdlg, _T("UserScale"), val, 50);
	
   double val2;
   acdbDisToF(val,-1,&val2); //conversione da stringa a double
   
   if ((val2)==0.0)
   {
      C_STRING temp;
      temp = PLOT_SCALE;
      ads_set_tile(hdlg, _T("UserScale"), temp.get_name());
   }
   else
      ads_distof(val,-1,&PLOT_SCALE);//conversione da stringa a double

   //tronco il numero a 6 cifre decimali
   swprintf(val, 50, _T("%f"), PLOT_SCALE);
   ads_distof(val,-1,&PLOT_SCALE);  //conversione da stringa a double

   ads_set_tile(hdlg, _T("UserScale"), val);
}

static void get_user_largeness(ads_callback_packet *cpkt)				 /*FCN*/
{
	TCHAR val[50];
	ads_hdlg hdlg=cpkt->dialog;
	ads_get_tile(hdlg, _T("UserLargeness"),val,50);
	
   double val2;
   ads_distof(val,-1,&val2);//conversione da stringa a double
   if ((val2)<0.0)
   {
      DIMPAPERS[FOGLIO_UTENTE] = 0.00;
      ads_set_tile(hdlg, _T("UserLargeness"), _T("0"));
   }
   else
      DIMPAPERS[FOGLIO_UTENTE] = val2;

   //tronco il numero a 6 cifre decimali
   swprintf(val, 50, _T("%f"), val2);
   ads_distof(val,-1,&val2);  //conversione da stringa a double

   DIMPAPERS[FOGLIO_UTENTE] = val2;

   ads_set_tile(hdlg, _T("UserLargeness"), val);
}

static void get_user_height(ads_callback_packet *cpkt)				 /*FCN*/
{
	TCHAR val[50];
	ads_hdlg hdlg=cpkt->dialog;
	ads_get_tile(hdlg, _T("UserHeight"), val,50);
	
   double val2;
   ads_distof(val,-1,&val2);//conversione da stringa a double
   if ((val2)<0.0)
   {
      DIMPAPERS[FOGLIO_UTENTE+1] = 0.00;
      ads_set_tile(hdlg, _T("UserHeight"), _T("0"));
   }
   else
      DIMPAPERS[FOGLIO_UTENTE+1] = val2;

   //tronco il numero a 6 cifre decimali
   swprintf(val, 50, _T("%f"), val2);
   ads_distof(val,-1,&val2);  //conversione da stringa a double

   DIMPAPERS[FOGLIO_UTENTE+1] = val2;

   ads_set_tile(hdlg, _T("UserHeight"), val);
}


/* ******************************************************************* */
//		funzioni CALLBACK per ottenere il tipo di plottata
/* ******************************************************************* */
static int set_type1(ads_callback_packet *cpkt)				 /*FCN*/
{
	C_STRING type_file;
	short    cx,cy;

	ads_hdlg hdlg=cpkt->dialog;
	initialize_type_list(hdlg);
	ads_dimensions_tile(hdlg, _T("T1"), &cx,&cy);
	ads_start_image(hdlg, _T("T1"));
	ads_fill_image(0,0,cx,cy,7); //sfondo bianco

   type_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\TYPE1.SLD"); 

	ads_slide_image(0,-25,cx,cy,type_file.get_name());
	ads_end_image();
	PLOT_TYPE=CART_LESS;

	return GS_GOOD;
}

static int set_type2(ads_callback_packet *cpkt)				 /*FCN*/
{
	C_STRING type_file;
	short cx,cy;
	ads_hdlg hdlg=cpkt->dialog;

	initialize_type_list(hdlg);
	ads_dimensions_tile(hdlg, _T("T2"),&cx,&cy);
	ads_start_image(hdlg, _T("T2"));
	ads_fill_image(0,0,cx,cy,7); //sfondo bianco

   type_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\TYPE2.SLD"); 

	ads_slide_image(0,-25,cx,cy,type_file.get_name());
	ads_end_image();
 	PLOT_TYPE=INT_CART;

	return GS_GOOD;
}

static int set_type3(ads_callback_packet *cpkt) 				 /*FCN*/
{
	C_STRING type_file;
	short cx,cy;
	ads_hdlg hdlg=cpkt->dialog;

	initialize_type_list(hdlg);
	ads_dimensions_tile(hdlg, _T("T3"),&cx,&cy);
	ads_start_image(hdlg, _T("T3"));
	ads_fill_image(0,0,cx,cy,7); //sfondo bianco

   type_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\TYPE3.SLD"); 

	ads_slide_image(0,-25,cx,cy,type_file.get_name());
	ads_end_image();
	PLOT_TYPE=EXBOTTOM_CART;

	return GS_GOOD;
}

static int set_type4(ads_callback_packet *cpkt) 				 /*FCN*/
{
	C_STRING type_file;
	short cx,cy;
	ads_hdlg hdlg=cpkt->dialog;

	initialize_type_list(hdlg);
	ads_dimensions_tile(hdlg, _T("T4"),&cx,&cy);
	ads_start_image(hdlg, _T("T4"));
	ads_fill_image(0,0,cx,cy,7); //sfondo bianco

   type_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\TYPE4.SLD"); 

	ads_slide_image(0,-25,cx,cy,type_file.get_name());
	ads_end_image();
	PLOT_TYPE=EXRIGHT_CART;

	return GS_GOOD;
}

static int set_type5(ads_callback_packet *cpkt)				 /*FCN*/
{
	ads_hdlg hdlg=cpkt->dialog;
   C_STRING type_file;
	short cx,cy;

	
	initialize_type_list(hdlg);
	ads_dimensions_tile(hdlg, _T("T5"),&cx,&cy);
	ads_start_image(hdlg, _T("T5"));
	ads_fill_image(0,0,cx,cy,7); //sfondo bianco

   type_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\TYPE5.SLD"); 

   ads_slide_image(0,-25,cx,cy,type_file.get_name());
	ads_end_image();
	PLOT_TYPE=EXBOTTOMLEG_CART;

	return GS_GOOD;
}

static int set_type6(ads_callback_packet *cpkt)				 /*FCN*/
{
	C_STRING type_file;
	short cx,cy;
	ads_hdlg hdlg=cpkt->dialog;

	initialize_type_list(hdlg);
	ads_dimensions_tile(hdlg, _T("T6"),&cx,&cy);
	ads_start_image(hdlg, _T("T6"));
	ads_fill_image(0,0,cx,cy,7); //sfondo bianco

   type_file = GEOsimAppl::WORKDIR + _T("\\") + GEOSLIDEDIR + _T("\\TYPE6.SLD"); 

	ads_slide_image(0,-25,cx,cy,type_file.get_name());
	ads_end_image();
	PLOT_TYPE=EXRIGHTLEG_CART;

	return GS_GOOD;
}

/* ******************************************************************* */
//		funzioni CALLBACK per i parametri finali
/* ******************************************************************* */
static void get_paper_user_margins(ads_callback_packet *cpkt)				 /*FCN*/
{
   TCHAR val[50];
   ads_hdlg hdlg=cpkt->dialog;

   ads_get_tile(hdlg, _T("PaperUserMargins"), val,50);
   PAPER_USER_MARGINS=_wtoi(val);       //conversione da stringa a int
	PAPER_MARGINS=PAPER_USER_MARGINS+PAPER_FIXED_MARGINS+LIMIT_TEXT_HEIGHT;

   if (SCALE_TYPE==OTTIMIZZATA) calculate_scale();

   calculate_draw_dim();
   _itow_s(PAPER_USER_MARGINS,val,10);//conversione da int a stringa
   ads_set_tile(hdlg, _T("PaperUserMargins"), val);
}

static void get_paper_limits(ads_callback_packet *cpkt)				 /*FCN*/
{
	TCHAR val[2];
	ads_hdlg hdlg=cpkt->dialog;

	ads_get_tile(hdlg, _T("PaperLimits"), val,2);
	PAPER_LIMITS=_wtoi(val);

   if (PAPER_LIMITS==1)
   	do {
   		geo_limit_text_height();
   	} while (LIMIT_TEXT_HEIGHT==0); 

   else LIMIT_TEXT_HEIGHT=0;

   PAPER_MARGINS=PAPER_USER_MARGINS+PAPER_FIXED_MARGINS+LIMIT_TEXT_HEIGHT; 

   if (SCALE_TYPE==OTTIMIZZATA) calculate_scale();

   calculate_draw_dim();
}

static void get_limit_text_height(ads_callback_packet *cpkt)				 /*FCN*/
{
	TCHAR val[50];
	ads_hdlg hdlg = cpkt->dialog;

	ads_get_tile(hdlg, _T("LimitTextHeight"), val, 50);
	LIMIT_TEXT_HEIGHT=_wtoi(val);  // conversione da stringa a int
}

static void get_paper_grid(ads_callback_packet *cpkt)				 /*FCN*/
{
	TCHAR val[2];
	ads_hdlg hdlg = cpkt->dialog;

	ads_get_tile(hdlg, _T("PaperGrid"), val, 2);
	PAPER_GRID=_wtoi(val);
}

static void get_user_grid_step_x(ads_callback_packet *cpkt)				 /*FCN*/
{
   TCHAR val[50];
   ads_hdlg hdlg=cpkt->dialog;

   ads_get_tile(hdlg, _T("UserGridStepX"), val,50);
	double temp;
	ads_distof(val, -1, &temp);  //conversione da stringa a double

   if (temp<0)
   {
      swprintf(val, 50, _T("%f"), GRID_STEPX);
   	ads_set_tile(hdlg, _T("UserGridStepX"), val);
   }

	GRID_STEPX = temp;  // conversione da stringa a double
//   _itow_s(GRID_STEP, val, 10); // conversione da int a stringa
//   ads_set_tile(hdlg, _T("UserGridStep"), val);
}

static void get_user_grid_step_y(ads_callback_packet *cpkt)				 /*FCN*/
{
   TCHAR val[50];
   ads_hdlg hdlg=cpkt->dialog;

   ads_get_tile(hdlg, _T("UserGridStepY"), val,50);
	double temp;
	ads_distof(val,-1,&temp);  //conversione da stringa a double
   if (temp<0)
   {
      swprintf(val, 50, _T("%f"), GRID_STEPY);
   	ads_set_tile(hdlg, _T("UserGridStepY"), val);
   }

	GRID_STEPY=temp;  //conversione da stringa a double
}

static void browse(ads_callback_packet *cpkt)				 /*FCN*/
{
   ads_hdlg hdlg = cpkt->dialog;
   C_STRING filename, LastCartFile, DefaultPath;
   
   // User pressed browse button to choose the file containing the cart.

   // Se non esiste alcun file precedente
   if (gsc_getPathInfoFromINI(_T("LastCartFile"), LastCartFile) == GS_BAD ||
       gsc_dir_exist(LastCartFile) == GS_BAD)
      DefaultPath.clear();
   else
      if (gsc_dir_from_path(LastCartFile, DefaultPath) == GS_BAD) return;

   // "GEOsim - Seleziona file" per il cartiglio
	if (gsc_GetFileD(gsc_msg(645), DefaultPath, _T("dwg"), 16, filename) == RTNORM)
	{
		ads_set_tile(hdlg, _T("CartName"), filename.get_name());

      if (valid_cart_name(filename.get_name()) == GS_GOOD)
      {
         CART_NAME = filename;

         // memorizzo la scelta in GEOSIM.INI per riproporla la prossima volta
         gsc_setPathInfoToINI(_T("LastCartFile"), filename);
      }
		else CART_NAME.clear();
	}
	ads_set_tile(hdlg, _T("CartName"), CART_NAME.get_name());
}

static void get_cart_name(ads_callback_packet *cpkt)				 /*FCN*/
{
	TCHAR val[TILE_STR_LIMIT];
	ads_hdlg hdlg=cpkt->dialog;
   int i=0, valid=0; 

	ads_get_tile(hdlg, _T("CartName"), val, TILE_STR_LIMIT);
  
   while (val[i]!= _T('\0'))
   {
      if (val[i]!= _T(' ')) { valid=1; break; }
      else i++;
   }
   if (valid)
   {
     if (valid_cart_name(val)==GS_GOOD)//controlla se l'utente ha immesso il nome di un file esistente
        CART_NAME=val;
     else   
      CART_NAME.clear();
   }
   else
	   CART_NAME.clear();
	ads_set_tile(hdlg, _T("CartName"), CART_NAME.get_name());
}

static void get_cart_attr(ads_callback_packet *cpkt)				 /*FCN*/
{
	geo_cart_attr();	
}

static void get_manual_attr(ads_callback_packet *cpkt)				 /*FCN*/
{
   //imposta la var globale MANUAL_CART_ATTR a 0 o 1
}

static void get_new_attr(ads_callback_packet *cpkt)				 /*FCN*/
{
	ads_hdlg hdlg=cpkt->dialog;

	//valore standard per inizializzazione DCL GeoCartAttrVal
   //da passare alla funzione geo_cart_attr_val 
   //tipo valore Valore e stringa vuota
   ATTR_TYPE=ATTR_VALUE;
   CART_ATTR_VALUE.set_name(GS_EMPTYSTR);
	geo_cart_attr_val();	 //sono settati i valori di short ATTR_TYPE e C_STRING CART_ATTR_VALUE
   //aggiunge il valore nella list ListAttr in fondo
   ATTR_LIST_ITEM *item=new ATTR_LIST_ITEM(ATTR_TYPE,CART_ATTR_VALUE.get_name());
   USER_ATTR_LIST.add_tail(item);  //controllare se lo inserisce in fondo
   ads_start_list(hdlg, _T("ListAttr"), LIST_NEW,0);  //la lista viene cambiata, 3=LIST_NEW
   //scorre la lista USER_ATTR_LIST
   item=(ATTR_LIST_ITEM*)USER_ATTR_LIST.get_head();
   while (item!=NULL)
   {
      gsc_add_list(item->attr_value.get_name());
    	item=(ATTR_LIST_ITEM *)USER_ATTR_LIST.get_next();
   }
   ads_end_list();
}

static void del_attr(ads_callback_packet *cpkt)				 /*FCN*/
{
   //legge la riga selezionata in ListAttr
   //controlla che corrisponda ad un elemento valido
	//elimina la riga nella list ListAttr
}

static void get_mod_attr(ads_callback_packet *cpkt)				 /*FCN*/
{
   //legge la riga selezionata in ListAttr
   //per passare i valori alla funzione geo_cart_val
   //tipo di valore ed eventuale stringa
	geo_cart_attr_val();	
	//aggiorna la riga nella list ListAttr
}

static void attr_value_option(ads_callback_packet *cpkt)				 /*FCN*/
{
	ads_hdlg hdlg=cpkt->dialog;

   ATTR_TYPE=ATTR_VALUE;
   ads_mode_tile(hdlg, _T("ValIns"), 0);
   //in questo caso la casella ValIns va attivata
   //la var globale ATTR_TYPE va settata a ATTR_VALUE
}

static void attr_date_option(ads_callback_packet *cpkt)				 /*FCN*/
{
	ads_hdlg hdlg=cpkt->dialog;

   ATTR_TYPE=ATTR_DATE;
   ads_mode_tile(hdlg, _T("ValIns"), 1);
   //in questo caso la casella ValIns va disattivata
   //la var globale ATTR_TYPE va settata a ATTR_DATE
}

static void attr_hour_option(ads_callback_packet *cpkt)				 /*FCN*/
{
	ads_hdlg hdlg=cpkt->dialog;

   ATTR_TYPE=ATTR_HOUR;
   ads_mode_tile(hdlg, _T("ValIns"), 1);
   //in questo caso la casella ValIns va disattivata
   //la var globale ATTR_TYPE va settata a ATTR_HOUR
}

static void attr_scale_option(ads_callback_packet *cpkt)				 /*FCN*/
{
	ads_hdlg hdlg=cpkt->dialog;

   ATTR_TYPE=ATTR_SCALE;
   ads_mode_tile(hdlg, _T("ValIns"), 1);
   //in questo caso la casella ValIns va disattivata
   //la var globale ATTR_TYPE va settata a ATTR_SCALE
}

static void insert_value_attr(ads_callback_packet *cpkt)				 /*FCN*/
{
   ads_hdlg hdlg=cpkt->dialog;
   TCHAR val[50];

   ads_get_tile(hdlg, _T("ValIns"), val,50);
   CART_ATTR_VALUE.set_name(val);
   //legge la stringa corrispondente al valore dell'attributo
   //setta la var globale CART_ATTR_VALUE
}

static void get_cart_base_height(ads_callback_packet *cpkt)				 /*FCN*/
{
	TCHAR val[50];
	ads_hdlg hdlg=cpkt->dialog;

	ads_get_tile(hdlg, _T("CartBase"), val,50);
	ads_distof(val,-1,&CART_BASE);  //conversione da stringa a double
	ads_get_tile(hdlg, _T("CartHeight"), val,50);
	ads_distof(val,-1,&CART_HEIGHT);  //conversione da stringa a double
   if (SCALE_TYPE==OTTIMIZZATA)
      calculate_scale();
   paper_scale_calculate();
	if ( (PLOT_TYPE==EXRIGHTLEG_CART) || (PLOT_TYPE==EXBOTTOMLEG_CART) )
	{
	 	calculate_legend();
		//aggiorna i valori calcolati
		_itow_s((int) LEG_SPACE_TEXT, val, 10); // conversione da int a stringa
		ads_set_tile(hdlg, _T("LegSpaceText"), val);
		_itow_s(LEG_ROWS_NUMBER, val, 10); // conversione da int a stringa
		ads_set_tile(hdlg, _T("LegRowsNumber"), val);
		_itow_s(LEG_AV_ROWS_NUMBER, val, 10); // conversione da int a stringa
		ads_set_tile(hdlg, _T("LegAvRowsNumber"), val);
	}

   swprintf(val, 50, _T("%f"), CART_BASE);
	ads_set_tile(hdlg, _T("CartBase"), val);
   swprintf(val, 50, _T("%f"), CART_HEIGHT);
	ads_set_tile(hdlg, _T("CartHeight"), val);

	if ((CART_BASE<=0)||(CART_HEIGHT<=0)||(CART_BASE>=(PLOT_LARGENESS-2*PAPER_MARGINS))
	                                   ||(CART_HEIGHT>=(PLOT_HEIGHT-2*PAPER_MARGINS)))
   	ads_set_tile(hdlg, _T("err_msg"), gsc_msg(46)); // "altezza e/o base cartiglio non valida"
   else
   	ads_set_tile(hdlg, _T("err_msg"), GS_EMPTYSTR);
}

static void get_leg_text_height(ads_callback_packet *cpkt)				 /*FCN*/
{
	TCHAR val[50];
	ads_hdlg hdlg=cpkt->dialog;

	ads_get_tile(hdlg, _T("LegTextHeight"), val,50);
	ads_distof(val, -1,&LEG_TEXT_HEIGHT);  //conversione da stringa a double
	calculate_legend();
	//aggiorna i valori calcolati
	_itow_s((int) LEG_SPACE_TEXT, val, 10);//conversione da int a stringa
	ads_set_tile(hdlg, _T("LegSpaceText"), val);
	_itow_s(LEG_ROWS_NUMBER, val,10);//conversione da int a stringa
	ads_set_tile(hdlg, _T("LegRowsNumber"), val);
	_itow_s(LEG_AV_ROWS_NUMBER, val,10);//conversione da int a stringa
	ads_set_tile(hdlg, _T("LegAvRowsNumber"), val);
   swprintf(val, 50, _T("%f"), LEG_TEXT_HEIGHT);
	ads_set_tile(hdlg, _T("LegTextHeight"), val);
   if ((LEG_TEXT_HEIGHT<=0) ||(LEG_TEXT_HEIGHT>=LEG_TEXT_INTER_LINE))
   	ads_set_tile(hdlg, _T("err_msg"), gsc_msg(567));	// "Altezza testo non valida"
   else
   	ads_set_tile(hdlg, _T("err_msg"), GS_EMPTYSTR);
}

static void get_leg_text_inter_line(ads_callback_packet *cpkt)				 /*FCN*/
{
	TCHAR val[50];
	ads_hdlg hdlg=cpkt->dialog;

	ads_get_tile(hdlg, _T("LegTextInterLine"), val,50);
	ads_distof(val,-1,&LEG_TEXT_INTER_LINE);  //conversione da stringa a double
	calculate_legend();
	//aggiorna i valori calcolati
	_itow_s((int) LEG_SPACE_TEXT,val,10);//conversione da int a stringa
	ads_set_tile(hdlg, _T("LegSpaceText"), val);
	_itow_s(LEG_ROWS_NUMBER,val,10);//conversione da int a stringa
	ads_set_tile(hdlg, _T("LegRowsNumber"), val);
	_itow_s(LEG_AV_ROWS_NUMBER,val,10);//conversione da int a stringa
	ads_set_tile(hdlg, _T("LegAvRowsNumber"), val);
   swprintf(val, 50, _T("%f"), LEG_TEXT_INTER_LINE);
	ads_set_tile(hdlg, _T("LegTextInterLine"), val);
   if ((LEG_SPACE_BLOCK>=LEG_TEXT_INTER_LINE)||(LEG_TEXT_HEIGHT>=LEG_TEXT_INTER_LINE)
                                             ||(LEG_TEXT_INTER_LINE==0))
      ads_set_tile(hdlg, _T("err_msg"), gsc_msg(47)); // "Interlinea non valida"
   else
   	ads_set_tile(hdlg, _T("err_msg"), GS_EMPTYSTR);
}

static void get_leg_space_block(ads_callback_packet *cpkt)				 /*FCN*/
{
	TCHAR val[50];
	ads_hdlg hdlg=cpkt->dialog;

	ads_get_tile(hdlg, _T("LegSpaceBlock"), val,50);
	ads_distof(val,-1,&LEG_SPACE_BLOCK);  //conversione da stringa a double
	calculate_legend();
	//aggiorna i valori calcolati
	_itow_s((int) LEG_SPACE_TEXT,val,10);//conversione da int a stringa
	ads_set_tile(hdlg, _T("LegSpaceText"), val);
	_itow_s(LEG_ROWS_NUMBER,val,10);//conversione da int a stringa
	ads_set_tile(hdlg, _T("LegRowsNumber"), val);
	_itow_s(LEG_AV_ROWS_NUMBER,val,10);//conversione da int a stringa
	ads_set_tile(hdlg, _T("LegAvRowsNumber"), val);
   swprintf(val, 50, _T("%f"), LEG_SPACE_BLOCK);
	ads_set_tile(hdlg, _T("LegSpaceBlock"), val);
   if ((LEG_SPACE_BLOCK<=0)||(LEG_SPACE_BLOCK>=LEG_TEXT_INTER_LINE))
      	ads_set_tile(hdlg, _T("err_msg"), gsc_msg(569)); // "Spazio blocco non valido"
   else
   	ads_set_tile(hdlg, _T("err_msg"), GS_EMPTYSTR);
}

static void get_leg_space_text(ads_callback_packet *cpkt)  				 /*FCN*/
{
	TCHAR val[50];
	ads_hdlg hdlg=cpkt->dialog;

	//è un valore calcolato, l'utente non può cambiarlo
	_itow_s((int) LEG_SPACE_TEXT, val, 10);//conversione da int a stringa
	ads_set_tile(hdlg, _T("LegSpaceText"), val);
}

static void get_leg_rows_number(ads_callback_packet *cpkt)				 /*FCN*/
{
	TCHAR val[50];
	ads_hdlg hdlg=cpkt->dialog;

	//è un valore calcolato, l'utente non può cambiarlo
	_itow_s(LEG_ROWS_NUMBER,val,10);//conversione da int a stringa
	ads_set_tile(hdlg, _T("LegRowsNumber"), val);
}

static void get_leg_av_rows_number(ads_callback_packet *cpkt)				 /*FCN*/
{
	TCHAR val[50];
	ads_hdlg hdlg=cpkt->dialog;

    //è un valore calcolato, l'utente non può cambiarlo
	_itow_s(LEG_AV_ROWS_NUMBER,val,10);//conversione da int a stringa
	ads_set_tile(hdlg, _T("LegAvRowsNumber"), val);
}

static void get_leg_block_scale(ads_callback_packet *cpkt)				 /*FCN*/
{
	TCHAR val[50];
	ads_hdlg hdlg=cpkt->dialog;

	ads_get_tile(hdlg, _T("LegBlockScale"), val,50);
	ads_distof(val,-1,&LEG_BLOCK_SCALE);  //conversione da stringa a double
	calculate_legend();
	//aggiorna i valori calcolati
	_itow_s((int) LEG_SPACE_TEXT,val,10);//conversione da int a stringa
	ads_set_tile(hdlg, _T("LegSpaceText"), val);
	_itow_s(LEG_ROWS_NUMBER,val,10);//conversione da int a stringa
	ads_set_tile(hdlg, _T("LegRowsNumber"), val);
	_itow_s(LEG_AV_ROWS_NUMBER,val,10);//conversione da int a stringa
	ads_set_tile(hdlg, _T("LegAvRowsNumber"), val);
   swprintf(val, 50, _T("%f"), LEG_BLOCK_SCALE);
	ads_set_tile(hdlg, _T("LegBlockScale"), val);

   if (LEG_BLOCK_SCALE<=0)
   	ads_set_tile(hdlg, _T("err_msg"), gsc_msg(571)); // "Scala blocco non valida"
   else
   	ads_set_tile(hdlg, _T("err_msg"), GS_EMPTYSTR);
}

static void get_leg_manual(ads_callback_packet *cpkt)				 /*FCN*/
{
	TCHAR val[2];
	ads_hdlg hdlg=cpkt->dialog;

	//se è selezionato la legenda viene lasciata vuota
	ads_get_tile(hdlg, _T("LegManual"), val, 2);
	LEG_MANUAL = _wtoi(val);

	if (LEG_MANUAL==1)
	 	ads_mode_tile(hdlg, _T("Leg"), 1); 	 	//disattiva le opzioni per la legenda
	else
	 	ads_mode_tile(hdlg, _T("Leg"), 0);	 	//attiva le opzioni per la legenda
}

static void get_leg_ent(ads_callback_packet *cpkt)				 /*FCN*/
{
	TCHAR val[2];
	ads_hdlg hdlg=cpkt->dialog;

	//scelta legenda per classi di entità
	//cambia tutti i calcoli
	LEG_ENT=1;
	calculate_legend();
	//aggiorna i valori calcolati
	_itow_s((int) LEG_SPACE_TEXT, val, 10);//conversione da int a stringa
	ads_set_tile(hdlg, _T("LegSpaceText"), val);
	_itow_s(LEG_ROWS_NUMBER,val,10);//conversione da int a stringa
	ads_set_tile(hdlg, _T("LegRowsNumber"), val);
	_itow_s(LEG_AV_ROWS_NUMBER,val,10);//conversione da int a stringa
	ads_set_tile(hdlg, _T("LegAvRowsNumber"), val);
}

static void get_leg_block(ads_callback_packet *cpkt)				 /*FCN*/
{
	TCHAR val[2];
	ads_hdlg hdlg=cpkt->dialog;

	//scelta legenda per blocchi
	//cambia tutti i calcoli
	LEG_ENT=0;
	calculate_legend();
	//aggiorna i valori calcolati
	_itow_s((int) LEG_SPACE_TEXT,val,10); // conversione da int a stringa
	ads_set_tile(hdlg, _T("LegSpaceText"), val);
	_itow_s(LEG_ROWS_NUMBER,val,10); // conversione da int a stringa
	ads_set_tile(hdlg, _T("LegRowsNumber"), val);
	_itow_s(LEG_AV_ROWS_NUMBER,val,10); // conversione da int a stringa
	ads_set_tile(hdlg, _T("LegAvRowsNumber"), val);
}

static int set_userpaper(ads_callback_packet *cpkt)
{
 	ads_hdlg hdlg=cpkt->dialog;

   //DCL con la richiesta delle dimensioni
   do
   {
      geo_user_dim_paper(1);
   }
   while ((DIMPAPERS[FOGLIO_UTENTE] == 0) || (DIMPAPERS[FOGLIO_UTENTE+1] == 0));


   if (PAPER_ORIENTATION==HORIZONTAL) HUPAPER=FOGLIO_UTENTE;
	else VUPAPER=FOGLIO_UTENTE;  

   initialize_user_paper_list(hdlg);

   return GS_GOOD;
}


static int set_userpaper0(ads_callback_packet *cpkt)
{
 	ads_hdlg hdlg=cpkt->dialog;

   if (PAPER_ORIENTATION==HORIZONTAL) HUPAPER=A0;
	else VUPAPER=A0;  

   initialize_user_paper_list(hdlg);

   return GS_GOOD;
}

static int set_userpaper1(ads_callback_packet *cpkt)
{
 	ads_hdlg hdlg=cpkt->dialog;

   if (PAPER_ORIENTATION==HORIZONTAL) HUPAPER=A1;
	else VUPAPER=A1;  

   initialize_user_paper_list(hdlg);

	return GS_GOOD;
}

static int set_userpaper2(ads_callback_packet *cpkt)
{
 	ads_hdlg hdlg=cpkt->dialog;

	if (PAPER_ORIENTATION==HORIZONTAL) HUPAPER=A2;
	else VUPAPER=A2;  

   initialize_user_paper_list(hdlg);

   return GS_GOOD;
}

static int set_userpaper3(ads_callback_packet *cpkt)
{
 	ads_hdlg hdlg=cpkt->dialog;

	if (PAPER_ORIENTATION==HORIZONTAL) HUPAPER=A3; 
	else VUPAPER=A3;  

   initialize_user_paper_list(hdlg);

   return GS_GOOD;
}

static int set_userpaper4(ads_callback_packet *cpkt) 				 
{
 	ads_hdlg hdlg=cpkt->dialog;

	if (PAPER_ORIENTATION==HORIZONTAL) HUPAPER=A4;
	else VUPAPER=A4;  

   initialize_user_paper_list(hdlg);

   return GS_GOOD;
}

static void set_layout1(ads_callback_packet *cpkt)				 /*FCN*/
{
	ads_hdlg hdlg=cpkt->dialog;

   PLOT_LAYOUT=BOTTOM_LEFT;
   initialize_layout_list(hdlg);
}

static void set_layout2(ads_callback_packet *cpkt)				 /*FCN*/
{
	ads_hdlg hdlg=cpkt->dialog;
   PLOT_LAYOUT=TOP_LEFT;
   initialize_layout_list(hdlg);
}

static void set_layout3(ads_callback_packet *cpkt)				 /*FCN*/
{
	ads_hdlg hdlg=cpkt->dialog;

   PLOT_LAYOUT=BOTTOM_RIGHT;
   initialize_layout_list(hdlg);
}

static void set_layout4(ads_callback_packet *cpkt)	         /*FCN*/
{
	ads_hdlg hdlg=cpkt->dialog;

   PLOT_LAYOUT=TOP_RIGHT;
   initialize_layout_list(hdlg);
}

static void set_layout5(ads_callback_packet *cpkt)			   /*FCN*/
{
	ads_hdlg hdlg=cpkt->dialog;

   PLOT_LAYOUT=CENTERED;
   initialize_layout_list(hdlg);
}

static void save_ini_plot_file(ads_callback_packet *cpkt)
{
	ads_hdlg hdlg=cpkt->dialog;
   C_STRING filename, LastINIPlotFile, DefaultPath;

   ads_set_tile(hdlg, _T("err_msg"), GS_EMPTYSTR); 

   // Se non esiste alcun file precedente
   if (gsc_getPathInfoFromINI(_T("LastINIPlotFile"), LastINIPlotFile) == GS_BAD ||
       gsc_dir_exist(LastINIPlotFile) == GS_BAD)
      DefaultPath.clear();
   else
      if (gsc_dir_from_path(LastINIPlotFile, DefaultPath) == GS_BAD) DefaultPath = _T("c:");

   // "GEOsim - Seleziona file" da salvare
	if (gsc_GetFileD(gsc_msg(645), DefaultPath, _T("ini"), 1+16, filename) == RTNORM)
	{
      ads_set_tile(hdlg, _T("err_msg"), gsc_msg(80)); 
		if (save_plot_values(filename.get_name()) == GS_GOOD)
      {
         ads_set_tile(hdlg, _T("err_msg"), gsc_msg(81)); 

         // memorizzo la scelta in GEOSIM.INI per riproporla la prossima volta
         gsc_setPathInfoToINI(_T("LastINIPlotFile"), filename);

         INIPLOTFILE = filename;
         ads_set_tile(hdlg, _T("INIPlotFile"), INIPLOTFILE.get_name()); 
      }
      else
         ads_set_tile(hdlg, _T("err_msg"), gsc_msg(82)); 
   }

   return;
}

/*************************END CALLBACK FUNCTIONS**************************/
/*************************************************************************/


/*************************************************************************/
/* Questa funzione tronca un double alla seconda cifra decimale          */
/* gsc_rtrim_double                    
/* parametri: db - puntatore al duoble da modificare                     */
/*            n - numero di cifre decimali                               */
/*************************************************************************/
int gsc_rtrim_double(double *db)
{
   TCHAR val[50];

   swprintf(val, 50, _T("%.2f"), *db);
   ads_distof(val, -1, db); // conversione da stringa a double

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc InsTextStandardTextStyle                                   <internal> */
/*+
  Questa funzione inserisce un tsto con stile standard verificando che l'altezza
  testo non sia fissa.
  Parametri:
  ads_point pt;
  const TCHAR *HText;   anche se sembra sbagliato funziona lo stesso
  const TCHAR *Rot;     anche se sembra sbagliato funziona lo stesso
  const TCHAR *Text;
   
  Restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/******************************************************************************/
void InsTextStandardTextStyle(ads_point pt, const TCHAR *HText, 
                              const TCHAR *Rot, const TCHAR *Text)
{
   double FixedHText;
   
   // verifico se lo stile di testo ha una altezza fissa
   if (gsc_get_charact_textstyle(_T("STANDARD"), &FixedHText) == GS_BAD) return;

   if (FixedHText == 0) // non è stata impostata altezza fissa
      gsc_callCmd(_T("_.TEXT"), RTSTR, _T("_S"), RTSTR, _T("STANDARD"), RTPOINT, pt,
   			      RTSTR, HText, RTSTR, Rot, RTSTR, Text, 0);
   else
      gsc_callCmd(_T("_.TEXT"), RTSTR, _T("_S"), RTSTR, _T("STANDARD"), RTPOINT, pt,
   			      RTSTR, Rot, RTSTR, Text, 0);
}


/*****************************************************************************/
/*.doc InsTextJustifyRightStandardTextStyle                       <internal> */
/*+
  Questa funzione inserisce un testo con stile standard giustificato a destra
  verificando che l'altezza testo non sia fissa.
  Parametri:
  ads_point pt;
  const TCHAR *HText;   anche se sembra sbagliato funziona lo stesso
  const TCHAR *Rot;     anche se sembra sbagliato funziona lo stesso
  const TCHAR *Text;
   
  Restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/******************************************************************************/
void InsTextJustifyRightStandardTextStyle(ads_point pt, const TCHAR *HText, 
                                          const TCHAR *Rot, const TCHAR *Text)
{
   double FixedHText;
   
   // verifico se lo stile di testo ha una altezza fissa
   if (gsc_get_charact_textstyle(_T("STANDARD"), &FixedHText) == GS_BAD) return;

   if (FixedHText == 0) // non è stata impostata altezza fissa
      gsc_callCmd(_T("_.TEXT"), RTSTR, _T("_S"), RTSTR, _T("STANDARD"), RTSTR, _T("_J"), RTSTR, _T("_R"),
                  RTPOINT, pt, RTSTR, HText, RTSTR, Rot, RTSTR, Text, 0);
   else
      gsc_callCmd(_T("_.TEXT"), RTSTR, _T("_S"),RTSTR, _T("STANDARD"), RTSTR, _T("_J"), RTSTR, _T("_R"),
                  RTPOINT, pt, RTSTR, Rot, RTSTR, Text, 0);
}


/*****************************************************************************/
/*.doc load_plot_values                                            <internal> */
/*+
  Questa funzione carica i dati 
  contenuti in un file .ini

  Parametri:
  const TCHAR *file_name
  double x1=0.0, double y1=0.0, double x2=0.0, double y2=0.0
   
  Restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/******************************************************************************/
int load_plot_values(const TCHAR *filename,  double x1, double y1, double x2, double y2)
{
   int      res = GS_GOOD;
   C_STRING file_name;
   FILE     *file = NULL;
   int      file_aperto = 0;

   do
   {
      if (filename == NULL)
      {
	      file_name = GEOsimAppl::CURRUSRDIR;   // costruisce path per il file .INI
	      file_name += _T("\\GSDDPLOT.INI");    // aggiunge il nome del file .INI
      }
      else
         file_name = filename;
      
      if (gsc_path_exist(file_name) == GS_BAD)
      {
         if (filename == NULL) save_plot_values(); // lo crea e inserisce i valori di default
         else
         {
            set_GS_ERR_COD(eGSFileNoExisting); 
            res = GS_BAD; 
         }
         break;
      }
      
      if ((file = gsc_open_profile(file_name, READONLY)) == NULL) 
         { set_GS_ERR_COD(eGSReadFile); res = GS_BAD; break; }
      file_aperto = 1;

      //inizializzazione
      PT1[0]=x1;          
      PT1[1]=y1;         
      PT2[0]=x2;         
      PT2[1]=y2;         
      if (is_point())
         if (get_extension(file) == GS_BAD)
         {
            PT1[0]=0;          
            PT1[1]=0;         
            PT2[0]=0;         
            PT2[1]=0;         
            acutPrintf(gsc_msg(857));//"\nImpossibile ottenere i limiti della sessione da plottare"
         }

      if (get_units(file) == GS_BAD)
	   {
         UNITS_SYSTEM=0;        //sistema italiano
         MEASURE_UNIT=1;        //metri
         PLOT_UNITS=1;          //1 m = 1 unità disegno
         DRAWING_UNITS=1;        
         acutPrintf(gsc_msg(858));//"\nImpossibile ottenere le impostazioni per le unità di misura della sessione da plottare"
      }

      if (get_layout(file) == GS_BAD)
	   {
         PAPER_FIXED_MARGINS=0;    //margine di correzione dovuto alla stampante  
         PAPER_USER_MARGINS=10;    //margine   
         PAPER_LIMITS=0;           //squadratura se = 1
         LIMIT_TEXT_HEIGHT=0;      //altezza testo dei limiti  
         PAPER_GRID=0;             //griglia se = 1
         GRID_STEPX=100;           //passo griglia orizzontale 
         GRID_STEPY=100;           //passo griglia verticale
         PLOT_LAYOUT=CENTERED;     //posizione della sessione plottata rispetto al foglio scelto 
         acutPrintf(gsc_msg(859));//"\nImpossibile ottenere le impostazioni di layout della sessione da plottare"
      }

      if (get_cartleg(file) == GS_BAD)
	   {
         CART_NAME.clear();         //cartiglio - percorso completo
         CART_HEIGHT=50;            //altezza
         CART_BASE=130;             //larghezza
         LEG_TEXT_HEIGHT=2;         //altezza testo legenda
         LEG_TEXT_INTER_LINE=4;     //interlinea testo legenda   
         LEG_SPACE_BLOCK=2;         //spazio per i blocchi
         LEG_BLOCK_SCALE=2;         //scala per i blocchi (fattore moltiplicativo)
         LEG_ENT=1;                 //
         LEG_MANUAL=0;              //legenda manuale se = 1
         acutPrintf(gsc_msg(860));  //"\nImpossibile ottenere le impostazioni per il cartiglio e la legenda"
      }
         
      if (get_type(file) == GS_BAD)
         PLOT_TYPE=CART_LESS;               //tipo plottaggio 
      //CART_LESS        =0 senza cartiglio
      //INT_CART         =1 con cartiglio interno
      //EXBOTTOM_CART    = 2 con cartiglio all'esterno e in basso
      //EXRIGHT_CART     = 3 con cartiglio all'esterno e a destra
      //EXBOTTOMLEG_CART = 4 cartiglio e legenda all'esterno in basso
      //EXRIGHTLEG_CART  = 5  cartiglio e legenda all'esterno a destra

      if (get_orientation(file) == GS_BAD)
         PAPER_ORIENTATION=HORIZONTAL;//HORIZONTAL=0, VERTICAL=1


      if (get_scale(file) == GS_BAD)
	   {
         SCALE_TYPE=UNO_CINQUECENTO;     
         //UNO_CENTO=1
         //UNO_DUECENTO=2
         //UNO_CINQUECENTO=3
         //UNO_MILLE=4
         //UNO_DUEMILA=5
         //UNO_CINQUEMILA=6
         //OTTIMIZZATA=7
         //UTENTE=8         
         
         PLOT_SCALE=500.0;
         PAPER=-1; //{A4=0,A3=2,A2=4,A1=6,A0=8, FOGLIO_UTENTE = 10}
         acutPrintf(gsc_msg(861));//"\nImpossibile ottenere le impostazioni per la scala"
      }

      INIPLOTFILE = file_name;

   }
   while (0);

   if (file_aperto)
      gsc_close_profile(file);

   return res;
}


/*****************************************************************************/
/*.doc gsc_plot_preview                                            <internal> */
/*+
  Questa funzione costruisce il preview di stampa a partire dai dati 
  contenuti in un file .ini
  corregge i valori mediante calcoli
  Parametri:
  const TCHAR *file_name
   
  Restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/******************************************************************************/
int gsc_plot_preview(const TCHAR *file_name, double x1, double y1, double x2, double y2)
{
   int res = GS_GOOD;

   do
   {
      if (GS_CURRENT_WRK_SESSION == NULL)
         { GS_ERR_COD = eGSNotCurrentSession; res = GS_BAD; break; }

      if (load_plot_values(file_name, x1, y1, x2, y2) == GS_BAD)
         { res = GS_BAD; break; }
      
	   if (PAPER_LIMITS==0)
          LIMIT_TEXT_HEIGHT=0;
	   PAPER_MARGINS=PAPER_USER_MARGINS+PAPER_FIXED_MARGINS+LIMIT_TEXT_HEIGHT;
      //fine inizializzazione      

	   if (calculate_legend_list()==GS_GOOD) EXIST_LEGEND=1;
	   else EXIST_LEGEND=0;

      //calcoli per rendere coerenti i dati in input
      dimen_calculate();
      unit_const_calculate();
      if (SCALE_TYPE == OTTIMIZZATA)
         calculate_scale();
      calculate_draw_dim();
      calculate_legend();
      paper_scale_calculate();

	   gsdelplot();   // Cancella tutto nello spazio carta e scongela il layer GSDDPLOT se esiste

      if (show_plot()==GS_BAD)		//se riesce a costruire il preview nello spazio carta, memorizza i valori 
         res = GS_BAD;
   }
   while (0);


   if (res == GS_GOOD)
   {
      if ((PLOT_TYPE!=CART_LESS)&& CART_NAME.get_name() != NULL) //inserisce il cartiglio
		   //nome cartiglio, punto inserimento, scala, rotazione
         gsc_callCmd(_T("_.INSERT"), RTSTR, CART_NAME.get_name(),
      			      RTPOINT, CART_POINT, RTSTR, _T("1"), RTSTR, GS_EMPTYSTR, RTSTR, _T("0"), 0);
   }

   return res;
}

int gs_plot_preview(void)
{
   presbuf arg = acedGetArgs();
   double x1=0.0, y1=0.0, x2=0.0, y2=0.0;
   C_STRING file_name;

   if (!arg || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   file_name = arg->resval.rstring;

   do
   {
      //area plottaggio se data in input
      arg = arg->rbnext;
      //cerca la prima coordinata o il primo punto
      while ((arg != NULL) && (arg->restype != RTREAL)  && (arg->restype != RTLONG)
              && (arg->restype != RTSHORT)  && (arg->restype != RTPOINT))
         arg = arg->rbnext;
   
      if (arg != NULL)
      {
         switch(arg->restype)
         {
            case RTREAL:
               x1 = arg->resval.rreal;
               break;
            case RTLONG:
               x1 = arg->resval.rlong;
               break;
            case RTSHORT:
               x1 = arg->resval.rint;
               break;
            case RTPOINT:
               x1 = arg->resval.rpoint[0];
               y1 = arg->resval.rpoint[1];
               break;
            default:
               break;
         }
      }

      if (arg == NULL) { x1 = 0.0; y1 = 0.0; x2 = 0.0; y2 = 0.0; break; }
   
      if (arg->restype != RTPOINT) //cerco la seconda coordinata
      {
         arg = arg->rbnext;

         while ((arg != NULL) && (arg->restype != RTREAL)  && (arg->restype != RTLONG)
                 && (arg->restype != RTSHORT)  && (arg->restype != RTPOINT))
            arg = arg->rbnext;
   
         if (arg != NULL)
         {
            switch(arg->restype)
            {
               case RTREAL:
                  y1 = arg->resval.rreal;
                  break;
               case RTLONG:
                  y1 = arg->resval.rlong;
                  break;
               case RTSHORT:
                  y1 = arg->resval.rint;
                  break;
               default:
                  break;
            }
         }
         if (arg == NULL) { x1 = 0.0; y1 = 0.0; x2 = 0.0; y2 = 0.0; break; }
      }

      //secondo punto
      arg = arg->rbnext;
      while ((arg != NULL) && (arg->restype != RTREAL)  && (arg->restype != RTLONG)
              && (arg->restype != RTSHORT)  && (arg->restype != RTPOINT))
         arg = arg->rbnext;
   
      if (arg != NULL)
      {
         switch(arg->restype)
         {
            case RTREAL:
               x2 = arg->resval.rreal;
               break;
            case RTLONG:
               x2 = arg->resval.rlong;
               break;
            case RTSHORT:
               x2 = arg->resval.rint;
               break;
            case RTPOINT:
               x2 = arg->resval.rpoint[0];
               y2 = arg->resval.rpoint[1];
               break;
            default:
               break;
         }
      }

      if (arg == NULL) { x1 = 0.0; y1 = 0.0; x2 = 0.0; y2 = 0.0; break; }
   
      if (arg->restype != RTPOINT) //cerco la seconda coordinata
      {
         arg = arg->rbnext;

         while ((arg != NULL) && (arg->restype != RTREAL)  && (arg->restype != RTLONG)
                 && (arg->restype != RTSHORT)  && (arg->restype != RTPOINT))
            arg = arg->rbnext;
   
         if (arg != NULL)
         {
            switch(arg->restype)
            {
               case RTREAL:
                  y2 = arg->resval.rreal;
                  break;
               case RTLONG:
                  y2 = arg->resval.rlong;
                  break;
               case RTSHORT:
                  y2 = arg->resval.rint;
                  break;
               default:
                  break;
            }
         }
         if (arg == NULL) { x1 = 0.0; y1 = 0.0; x2 = 0.0; y2 = 0.0; break; }
      }
   }
   while (0);
      
   
   if (gsc_plot_preview(file_name.get_name(), x1, y1, x2, y2) == GS_BAD) return RTERROR;

   return RTNORM;
}