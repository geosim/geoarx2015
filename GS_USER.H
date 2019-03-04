/*************************************************************************/
/*   GS_USER.H                                                           */
/*************************************************************************/


#ifndef _gs_user_h
#define _gs_user_h 1

#ifndef _gs_list_h
   #include "gs_list.h"
#endif


//-----------------------------------------------------------------------//
// Classe per informazioni di testa file GEOPWD //
//-----------------------------------------------------------------------//
class C_LOG_HEAD
{
   public:
      int log_len;
      int pwd_len;
      int mode_len;
      int flags_len;
      int tot_len;
      int max_usr;
      int num_usr;

      int write(int file);
      int read(int file);
      int from_geopwd(int file = NULL);
      
      C_LOG_HEAD()  { log_len=pwd_len=mode_len=flags_len=tot_len=0;
                      max_usr=num_usr=0; }
      ~C_LOG_HEAD() {}
};

//-----------------------------------------------------------------------//
// Classe informazioni utente //
//-----------------------------------------------------------------------//
class C_USER
{
   public:
     int code;
     TCHAR *log;
     TCHAR *pwd;
     BYTE *mode;
     BYTE *flags;

     C_USER()  { code=0; log=pwd=NULL; mode=flags=NULL; }
     ~C_USER() {  if(log!=NULL) free(log);
                  if(pwd!=NULL) free(pwd);
                  if(mode!=NULL) free(mode);
                  if(flags!=NULL) free(flags); }

     int alloc(int l_log,int l_pwd,int l_mode,int l_flags);
     int alloc_from_geopwd(int file = NULL);
     int set_login(C_USER &glob);
     bool is_superuser(void);
};


struct cmd_code
{
    TCHAR name[MAX_LEN_DESCR_CMD];   // Nome del comando
    short code;                      // Codice del comando
};


//-----------------------------------------------------------------------//
//  VERSIONE LISP  //
//-----------------------------------------------------------------------//

void gslogin(void);    	  		 // comando senza dcl
void gscreausr(void);
void gsdelusr(void);  
void gsmodpwd(void);    
int gs_login (void);             // funzione lisp
int gs_superuser(void);
int gs_chkusr(void);
int gs_creausr(void);
int gs_delusr    (void);
int gs_getusr    (void);
int gs_whoami    (void);
int gs_getusrcomm(void);
int gs_setusrcomm(void);
int gs_getgscomm (void);
int gs_getusrprj (void);
int gs_setusrprj (void);
int gs_getPrjPermissions(void);
int gs_getusrclass(void);                              
int gs_setusrclass(void);
int gs_getClassPermissions(void);
int gs_getfamily(void);
int gs_chkclsvis(void);


//-----------------------------------------------------------------------//
// FUNZIONI C++ CORRISPONDENTI A CHIAMATE LISP //
//-----------------------------------------------------------------------//

DllExport int gsc_login(const TCHAR *log, const TCHAR *pwd, int CheckSessions = GS_GOOD);
DllExport int gsc_getLastLogin(TCHAR login[MAX_LEN_LOGIN]);
DllExport int gsc_getCheckSessionOnLogin(int *Result);
int     gsc_chkusr(TCHAR *,C_INT_INT_STR*);
DllExport int gsc_whoami(C_INT_INT_STR *out);
DllExport int gsc_getusrlist(C_INT_INT_STR_LIST*, int file = NULL);
DllExport int gsc_creausr(const TCHAR *login, const TCHAR *passw, int UserType);
DllExport int gsc_delusr(int code, const TCHAR *Password, bool Destroy = false);
DllExport int gsc_getusrcomm(int code,C_INT_LIST*);
DllExport int gsc_setusrcomm(int,C_INT_LIST*);
DllExport int gsc_getgscomm(C_INT_STR_LIST*);

DllExport int gsc_getPersonalPrjPermissions(int usr,C_INT_INT_LIST *out);
DllExport int gsc_setPersonalPrjPermissions(int usr,C_INT_INT_LIST &in);
DllExport int gsc_getPrjPermissions(int usr, C_INT_INT_LIST &out);
DllExport int gsc_getInheritancePrjPermissions(int usr, C_INT_INT_LIST &out,
                                               C_STR_LIST *pLoadedUserNames = NULL);

DllExport int gsc_getPersonalClassPermissions(int usr,int prj, C_INT_INT_LIST *out);
DllExport int gsc_setPersonalClassPermissions(int usr, int prj, C_INT_INT_LIST &in);
DllExport int gsc_getClassPermissions(int usr, int prj, C_INT_INT_LIST &out);
DllExport int gsc_getInheritanceClsPermissions(int usr, int prj, C_INT_INT_LIST &out,
                                               C_STR_LIST *pLoadedUserNames = NULL);
int gsc_delClassPermissions(int prj, int cls);

DllExport int gsc_getPersonalSecPermissions(int usr,int prj, C_4INT_STR_LIST *out);
DllExport int gsc_setPersonalSecPermissions(int usr, int prj, C_4INT_STR_LIST &in);
DllExport int gsc_getSecPermissions(int usr, int prj, C_4INT_STR_LIST &out);
DllExport int gsc_getInheritanceSecPermissions(int usr, int prj, C_4INT_STR_LIST &out,
                                               C_STR_LIST *pLoadedUserNames = NULL);
int gsc_delSecPermissions(int prj, int cls, int sub, int sec);

DllExport int gsc_loadInheritanceUsers(int usr, C_STR_LIST &UserNames, int file = NULL);
DllExport int gsc_saveInheritanceUsers(int usr, C_STR_LIST &UserNames);

DllExport int gsc_getprjcls(int prj, C_INT_LIST *out);
DllExport int gsc_getfamily(C_NODE *pPrj,C_FAMILY_LIST *out);
int     gsc_getchildren(int parent,C_BIRELATION_LIST *in,
                    C_FAMILY *family,C_FAMILY_LIST *out, int cont);
int     gsc_chkClsPermissions(C_NODE *pPrj, C_INT_INT_LIST &vis, int flag);

//-----------------------------------------------------------------------//
//   FUNZIONI INTERNE  //
//-----------------------------------------------------------------------//

BYTE   gsc_bitmask(int);
int    gsc_byte2int(int,int,long);
int    gsc_int2byte(int,int,int,long);

int    gsc_open_geopwd(int shflag = UPDATEABLE);
int    gsc_open_usrprj(int usr, int mode);
int    gsc_open_usrclass(int usr, int prj, int mode);
int    gsc_open_usrSec(int usr, int prj, int mode);
void   gsInitGeopwd(void);

int    gsc_get_log(int file, int user, int tipo, TCHAR *out);
int    gsc_get_log(int file, int user, int tipo, BYTE *out);
int    gsc_set_log(int file, int user, int tipo, const TCHAR *in);
int    gsc_set_log(int file, int user, int tipo, BYTE *in);

int    gsc_kript_pos(long,long*);
int    gsc_kript_val(long,long,BYTE,BYTE*);
int    gsc_dekript_val(long,long,BYTE,BYTE*);

int    gsc_get_usrcode (const TCHAR *log_name);
int    gsc_get_user(int code, C_USER *user, int file = NULL);
int    gsc_checkpwd(int code, const TCHAR *pwd_name);

DllExport int gsc_superuser(void);         
DllExport int gsc_superuser(int code, int file = NULL);         
DllExport int gsc_check_op(int);
int    gsc_set_op(int,int,int);

void   gsPrintGeopwd(void);

extern int gs_modpwd (void);
DllExport int gsc_modpwd(const TCHAR *OldPassword, const TCHAR *NewPassword);
extern int printlistproject(void);
extern int printlistclasses(C_4INT_STR_LIST *pclasses,C_INT_INT_LIST *classusr);
extern int codabl_to_msg(int codice);
int gs_checkop(void);


//-----------------------------------------------------------------------//
//  VARIABILI GLOBALI //
//-----------------------------------------------------------------------//

extern C_USER GS_USER;


#endif
