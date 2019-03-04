#pragma once
#include "afxwin.h"
#include "afxcmn.h"
#include "afxdialogex.h"

#include "GSResource.h"

#ifndef _gs_utily_h
   #include "gs_utily.h"
#endif


/*************************************************************************/
/*.doc class WaitDlgComponent                                            */
/*+
  Classe che gestisce il componente di attesa che è composto da una etichetta
  che fa da titolo e sotto una progress bar con una etichetta sovrapposta 
  (che sono gli elementi che cambiano durante il ciclo di elaborazione).
  La progresss bar può essere normale quando si conosce il totale dell'elaborazione
  o MarqueeBar quando non si conosce il totale. In questo caso la barra va avanti e indietro.
-*/  
/*************************************************************************/
class WaitDlgComponent
{
protected:
   bool Visible;    // se il componente è visibile nella finestra di attesa (uso interno)

public:
	WaitDlgComponent(void);
   WaitDlgComponent(WaitDlgComponent &item);
   WaitDlgComponent(C_STRING &_LabelText);
   WaitDlgComponent(C_STRING &_LabelText, bool _PercVis, bool _EstimatedTimeVis);
	virtual ~WaitDlgComponent(void);

   C_STRING     LabelText;    // etichetta titolo del componente
   C_STRING     BarText;      // etichetta che si sovrappone alla barra
   bool         MarqueeBar;   // se vero la barra va avanti e indietro perchè non si conosce il totale
                              // altrimenti è una progress bar normale di cui si conosce il totale

   int          Total;        // numero totale di iterazioni (usato se MarqueeBar=false)
   COleDateTime InitialTime;  // tempo iniziale per calcolare quanto tempo manca alla fine (usato se MarqueeBar=false)
   int          Prcnt;        // percentuale sul totale (usato se MarqueeBar=false)
   bool         PercVis;      // se vero il testo sulla barra mostrerà la percentuale di avanzamento (usato se MarqueeBar=false)
   bool         EstimatedTimeVis; // se vero il testo sulla barra mostrerà il tempo mancante (usato se MarqueeBar=false)

   C_STRING     Format;     // stringa di formattazione (es. "%ld entità GEOsim elaborate.", usato se MarqueeBar=true)

   int          Threshold; // se MarqueeBar=false, è il tempo soglia (sec) oltre il quale il componente deve essere
                           // reso visibile (per le attese brevi non vale neanche la pena di visualizzare l'attesa) 
                           // se MarqueeBar=true, usando la funzione SetBar(long i) è il passo di visualizzazione del contatore
                           // (es. ogni 100)

   void operator = (WaitDlgComponent &in);

   void End(void);
   bool SetBar_Text(C_STRING &Msg);
   bool SetBar(long i);
   bool isToUpdateBarTextByPerc(int NewPrcnt);
   COleDateTimeSpan getEstimatedTime(void);
   COleDateTimeSpan getTotalEstimatedTime(void);
   void getUpdatedBarText(C_STRING &UpdatedBarText);

   void SetProgressBarStyle(C_STRING &_LabelText, bool _PercVis, bool _EstimatedTimeVis);
   void Init(long _Total);
   void SetMarqueeBarStyle(C_STRING &_LabelText);

   bool getVisible(void);
   void ForceVisible(void);
};


/*************************************************************************/
/*.doc class CWaitDlg                                                    */
/*+
  Classe che definisce la finestra di dialogo CWaitDlg.
-*/  
/*************************************************************************/
class CWaitDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CWaitDlg)

public:
	CWaitDlg(CWnd* pParent = NULL);   // costruttore standard
	virtual ~CWaitDlg();

// Dati della finestra di dialogo
	enum { IDD = IDD_WAIT_DLG };

protected:
   C_INT_VOIDPTR_LIST m_WaitDlgComponentList;
   C_INT_VOIDPTR_LIST m_LabelList;            // lista dei controlli label di tipo CStatic
   C_INT_VOIDPTR_LIST m_ProgressList;         // lista dei controlli progress bar di tipo CProgressCtrl

	virtual void DoDataExchange(CDataExchange* pDX);    // Supporto DDX/DDV
	virtual BOOL OnInitDialog();

	int getWaitDlgComponentHeight(WaitDlgComponent &item);
   void RefreshPrevBarTextOnTimeChanged(void);

	DECLARE_MESSAGE_MAP()
public:
   int  AppendDlgComponent(WaitDlgComponent &WaitComponent);
   bool RefreshCtrls(void);
   bool RefreshCtrl(C_INT_VOIDPTR *p, int *Y);
   int RemoveLastDlgComponent(void);
   int getWaitDlgComponentCount(void);

   void Set(C_STRING &Msg);
   void Set(long i);
   void SetPerc(int NewPrcnt);
};


//----------------------------------------------------------------------------//
//    class C_WAIT_DLG_INSTANCE_JOB                                           //
//----------------------------------------------------------------------------//
class C_WAIT_DLG_INSTANCE_JOB  : public C_INT_LONG 
{
   public :
      C_WAIT_DLG_INSTANCE_JOB();
      virtual ~C_WAIT_DLG_INSTANCE_JOB();

      WaitDlgComponent DlgComponent;
      C_STRING Msg;
};


/*************************************************************************/
/*.doc class C_WAIT_DLG_INSTANCE                                         */
/*+
  Classe che rappresenta il thread usato per visualizzare la finestra di dialogo CWaitDlg.
-*/  
/*************************************************************************/
class C_WAIT_DLG_INSTANCE : public CWinThread
{
   DECLARE_DYNCREATE(C_WAIT_DLG_INSTANCE)

	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
   virtual BOOL PreTranslateMessage(MSG* pMsg);
         
   DECLARE_MESSAGE_MAP()

   public:

	CWaitDlg *pWaitDlg;   // puntatore alla finestra di attesa

   int AppendDlgComponent(WaitDlgComponent &item);
   int RemoveLastDlgComponent(void);
   void Set(C_STRING &Msg);
   void Set(long i);
   void SetPerc(int NewPrcnt);
};


class C_STATUSBAR_PROGRESSMETER
{   
   private:
      WaitDlgComponent m_WaitDlgComponent;
      bool initialized;
      void internalInitialization(C_STRING &Title);

   public :
DllExport C_STATUSBAR_PROGRESSMETER(C_STRING &Title);
DllExport C_STATUSBAR_PROGRESSMETER(const TCHAR *format = NULL, ...);
DllExport virtual ~C_STATUSBAR_PROGRESSMETER(); 
DllExport void Set(long i);
DllExport void Set_Perc(int Prcnt);
DllExport void Init(long _Total);
DllExport void End(const TCHAR *formatEndTitle = NULL, ...);
};


class C_STATUSLINE_MESSAGE
{   
   private:
      WaitDlgComponent m_WaitDlgComponent;
      bool initialized;
      void internalInitialization(C_STRING &Title);

   public :
DllExport C_STATUSLINE_MESSAGE(C_STRING &Title);
DllExport C_STATUSLINE_MESSAGE(const TCHAR *format = NULL, ...);
DllExport virtual ~C_STATUSLINE_MESSAGE(); 
DllExport void Set(const TCHAR *format, ...);
DllExport void Set(C_STRING &Msg);
DllExport void Set(long i);
DllExport void Init(const TCHAR *Format = NULL, int OptimizedEvery = 1);
DllExport void End(const TCHAR *formatEndTitle = NULL, ...);
};
