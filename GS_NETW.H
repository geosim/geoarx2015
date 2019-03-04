/*************************************************************************/
/*   GS_NETW.H                                                           */
/*************************************************************************/


#ifndef _gs_netw_h
#define _gs_netw_h 1


class C_ALIAS : public C_NODE
{
   friend class C_ALIAS_LIST;

   protected :  
      TCHAR *path;    // es. C:
      TCHAR *host;    // es. \\GEPPETTO\TUTTO_C oppure <GEPPETTO>
      enum OperatingSystemsEnum op_sys;
   public :
DllExport C_ALIAS() : C_NODE() { path = NULL; host = NULL; op_sys = GS_OS_WINDOWS_NT_2000; }
DllExport virtual ~C_ALIAS()   { if (path != NULL) free(path); path = NULL;
                                 if (host != NULL) free(host); host = NULL; }

DllExport TCHAR *get_path() { return path; }
DllExport int   set_path(const char *in); 
DllExport int   set_path(const TCHAR *in); 
DllExport TCHAR *get_host() { return host; }
DllExport int   set_host(const char *in); 
DllExport int   set_host(const TCHAR *in); 
DllExport enum OperatingSystemsEnum get_op_sys() { return op_sys; }
DllExport int   set_op_sys(enum OperatingSystemsEnum in) { op_sys = in; return GS_GOOD; }
DllExport int   is_valid();
DllExport int   is_valid_path();
DllExport int   is_valid_host();
      int   save(FILE *file, bool Unicode = false);
      int   del(FILE *file, bool Unicode = false);
};


//-----------------------------------------------------------------------//   
class C_ALIAS_LIST : public C_LIST
{
   public :
DllExport C_ALIAS_LIST() : C_LIST() { }
DllExport virtual ~C_ALIAS_LIST(){ } // chiama ~C_LIST

DllExport C_ALIAS *search_host(const char *in, int CaseSensitive = GS_BAD);
DllExport C_ALIAS *search_host(const TCHAR *in, int CaseSensitive = GS_BAD);
DllExport C_ALIAS *search_path(const char *in, int CaseSensitive = GS_GOOD);
DllExport C_ALIAS *search_path(const TCHAR *in, int CaseSensitive = GS_GOOD);
      C_ALIAS *search_host_xptr(const char *in, C_ALIAS *pAlias,
                                int CaseSensitive = GS_BAD); // ricerca saltando un puntatore
      C_ALIAS *search_host_xptr(const TCHAR *in, C_ALIAS *pAlias,
                                int CaseSensitive = GS_BAD); // ricerca saltando un puntatore
      C_ALIAS *search_path_xptr(const char *in, C_ALIAS *pAlias,
                                int CaseSensitive = GS_GOOD); // ricerca saltando un puntatore
      C_ALIAS *search_path_xptr(const TCHAR *in, C_ALIAS *pAlias,
                                int CaseSensitive = GS_GOOD); // ricerca saltando un puntatore
      int copy(C_ALIAS_LIST *out);
DllExport int load();
DllExport int save();
DllExport int add_to_mem(C_ALIAS *pAlias);
DllExport int del_to_mem(C_ALIAS *pAlias);
DllExport int upd_to_mem(C_ALIAS *pOrig, C_ALIAS *pAlias);

DllExport int loadHostsFromSrv();
DllExport int saveHostsToSrv();
};


DllExport int gsc_nethost2drive(C_STRING &path, int PrintError = GS_GOOD);
DllExport int gsc_nethost2drive(char *path, int maxlen, int PrintError = GS_GOOD );
DllExport int gsc_nethost2drive(TCHAR *path, int maxlen, int PrintError = GS_GOOD );
int gs_drive2nethost(void);
DllExport int gsc_drive2nethost(C_STRING &path);
DllExport int gsc_drive2nethost(char *path, int len_str);
DllExport int gsc_drive2nethost(TCHAR *path, int len_str);
DllExport int gsc_ADEdrv2nethost(const char *path_input, C_STRING &path_output);
DllExport int gsc_ADEdrv2nethost(const TCHAR *path_input, C_STRING &path_output);
DllExport const TCHAR* gsc_OSCode2OSName(enum OperatingSystemsEnum OSCode);
DllExport enum OperatingSystemsEnum gsc_OSName2OSCode(const TCHAR *OSName);

int gs_conv_alias(void);

                                                                           
#endif