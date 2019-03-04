/*************************************************************************
Name: gs_plot

Module description: File per le definizioni dei tipi
            
Author: Caterina Gaeta

**************************************************************************/

/*************************************************************************
ONLY ITALIAN indica stringhe eventualmente da tradurre 
**************************************************************************/
      
/*************************************************************************/

/*  GS_PLOT.H                                                            */

/*************************************************************************/


#ifndef _gs_plot_h
#define _gs_plot_h 1


/*************************************************************************/
/*  DEFINES                                                              */
/*************************************************************************/

/* -STRINGHE PER LA GESTIONE DELLE IMPOSTAZIONI DI DEFAULT DEL PLOTTAGGIO- */
#define DECIMAL_SYSTEM _T("Metrico")  //ONLY ITALIAN
#define ENGLISH_SYSTEM _T("Inglese")   //ONLY ITALIAN

/* ------------------------VALORI RESTITUITI DALLE DCL------------------ */

#define DLGPREV 10
#define DLGSELECT_AREA 40
#define DLGCHANGE_VIEW 50
#define DLGWAIT 60

/* --------------------DIMENSIONI DEI FOGLI IN mm ------------------ */
//i valori corrispondono al plotter DJ 650C con Map2000i
//i secondi valori corrispondono al plotter HP 650 con Map2 (i terzi sono le dim reali dei fogli)
#define l_A4 261.41//285.0//297.0
#define h_A4 198.41//198.0//210.0 
#define l_A3 408.41//396.0//420.0
#define h_A3 261.41//273.0//297.0 
#define l_A2 558.41//570.0//594.0
#define h_A2 408.41//396.0//420.0 
#define l_A1 829.41//817.0//841.0
#define h_A1 558.41//570.0//594.0 
#define l_A0 1153.41//1165.0//1189.0
#define h_A0 829.41//817.0//841.0 
#define l_U  0.00
#define h_U  0.00 

/* -----COSTANTE PER LA CORREZIONE DEL CALCOLO DELLE RIGHE IN LEGENDA--- */

#define LINE_CORRECTION 1.5

/* --------COSTANTI PER LA GESTIONE DELLE CLASSI DI ENTITA'------------- */

#define extr_class_index 6  //posizione elemento sottolista dell'elemento
									//nella lista della classi selezionate
									//che indica se la classe è estratta

/*************************************************************************/
/*  TYPEDEFS                                                             */
/*************************************************************************/

class geosim_class
{
public:
	C_COLOR class_color;					// colore della classe
	C_STRING class_hatch; 				// riempimento
	C_STRING class_line_type; 			// tipo linea
	C_STRING class_text_style; 		// stile testo
	C_STRING class_layer;      		// piano
	C_STRING class_block;				// blocco
	double class_width;					// larghezza
	double class_elevation;				// elevazione
	double class_scale;					// scala
	double class_rotation;				// rotazione
	double class_text_height;			// altezza testo
	double class_thickness;				// spessore

   geosim_class();
	~geosim_class() {
	}

	virtual void draw_class_block(ads_point pt, TCHAR *scale, double larg) {}

	virtual void write_class_name(ads_point pt, TCHAR *val, C_STRING *name);
};								 


class geosim_node:public geosim_class
{
public:
	geosim_node(resbuf *rt);
	void draw_class_block(ads_point pt, TCHAR *scale,double larg);//IN FASE DI CONTROLLO
};

class geosim_text:public geosim_class {
public:
	geosim_text(presbuf rt);
	void write_class_name(ads_point pt,TCHAR* val, C_STRING *name);
};

class geosim_pol:public geosim_class {
public:
	geosim_pol(presbuf rt);
	void draw_class_block(ads_point pt,TCHAR* scale,double larg);
};

class geosim_sup:public geosim_class {
public:
	geosim_sup(presbuf rt);
	void draw_class_block(ads_point pt,TCHAR* scale,double larg);
};

class geosim_spag:public geosim_class {
public:
	geosim_spag(presbuf rt);
	void draw_class_block(ads_point pt,TCHAR* scale,double larg);
};


/*************************************************************************/
/*								CLASSE CLASS_LIST_ITEM                   */
/*************************************************************************/
//lista classi di entità (indice, nome, nome blocco)
class CLASS_LIST_ITEM:public C_NODE
{
friend class CLASS_LIST;
public:
	short ext_class_index;
	short class_index;
	C_STRING ext_class_name;
	C_STRING class_name;	

	geosim_class * class_ref; //punta alla lista delle caratteristiche
	
	//CLASS_LIST *clnext;   //è già in C_NODE next

	CLASS_LIST_ITEM();

	CLASS_LIST_ITEM(short ext,short cl,TCHAR *ext_name,TCHAR *cl_name,geosim_class *p);

	~CLASS_LIST_ITEM();
};


class CLASS_LIST:public C_LIST
{
public:   
   CLASS_LIST(){
   }
   virtual ~CLASS_LIST(){
   }
};


/*************************************************************************/
/*								CLASSE ATTR_LIST_ITEM                            */
/*************************************************************************/
//lista attributi cartiglio
class ATTR_LIST_ITEM:public C_NODE
{
friend class ATTR_LIST;
public:
	short attr_type;
	C_STRING attr_value;	

	ATTR_LIST_ITEM();
	ATTR_LIST_ITEM(short type, TCHAR *value);

	~ATTR_LIST_ITEM() {
	}
};


class ATTR_LIST:public C_LIST
{
public:   
   ATTR_LIST(){
   }
   virtual ~ATTR_LIST(){
   }
};

//funzioni per l'utente del plottaggio
void gsddplot(void);
void gsdelplot(void);
void gsviewplot(void);
void gsexitplot(void);


int gs_plot_preview();


//funzioni di servizio
DllExport	int get_int_value(FILE *file,const TCHAR *sez,const TCHAR *val,int *in);
DllExport	int set_echo(short *val);
DllExport	int set_menu(short *val);
DllExport	int reset_echo(short val);
DllExport	int reset_menu(short val);
DllExport	int extern_class(resbuf *rb);
DllExport	int class_node(resbuf *rb);
DllExport	int class_text(resbuf *rb);
DllExport	int class_pol(resbuf *rb);
DllExport	int class_sup(resbuf *rb);
DllExport	int class_spag(resbuf *rb);

                                                                           
#endif