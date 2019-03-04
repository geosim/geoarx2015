/*************************************************************************/
/*   GS_UTILY.H                                                          */
/*************************************************************************/
#ifndef _gs_utily_h
#define _gs_utily_h 1

#ifndef _adsdlg_h
   #include "adsdlg.h"   
#endif

#ifndef _gs_list_h
   #include "gs_list.h"
#endif

#ifndef _gs_selst_h
   #include "gs_selst.h"
#endif

#include <ATLComTime.h>
//#ifndef __AFXWIN_H__
//   #include <afxwin.h>
//#endif

//-----------------------------------------------------------------------//
//  CLASSE GESTIONE EED APPLICAZIONE "GEOsim" //
//-----------------------------------------------------------------------//   
class C_EED
{
   public :
   ads_name ent;         // Nome entita' Autocad corrispondente
   int      cls;         // Code 1070 primo   obbligatorio
   int      sub;         // Code 1070 secondo (usato solo per le simulazioni)
   int      num_el;      // Code 1071 terzo   (usato solo per n. aggregazioni > 1)
   // Dati usati solo per le simulazioni con tipo polilinea
   long     initial_node; // (Code 1002 "{") + (Code 1071 nodo-iniziale) +
   long     final_node;  // (Code 1071 nodo-finale) + (Code 1002 "}")
   int      vis;         // flag di visibilità usato per blocchi attributi (VISIBLE e INVISIBLE)
   long     gs_id;       // Code 1040 (real) usato per memorizzare il codice gs_id
                         // (usato solo per oggetti con link a DB)

DllExport C_EED();
    C_EED(ads_name name);
   ~C_EED() {}

DllExport int load(ads_name name);
   int load(const AcDbObject *dbObj);
   int load(presbuf lst);
   int save(int AddEnt2SaveSS = GS_GOOD);
   int save(AcDbObject *pObj);
DllExport int save(ads_name name, int AddEnt2SaveSS = GS_GOOD);
   int save_selset(C_SELSET &SelSet);
   int save_selset(ads_name selset);
   int save_aggr(int n);
   int save_aggr(C_SELSET &SelSet, int n);
   int save_aggr(ads_name name, int n);
   int save_vis(int flag);
   int save_vis(ads_name name, int flag);

DllExport int clear();
   int clear(AcDbObjectId &objId);
DllExport int clear(ads_name name);

   int set_cls(int in);   
   int set_sub(int in);   
   int set_num_el(int in);
   int set_initial_node(long in);
   int set_final_node(long in);
DllExport int set_initialfinal_node(long init, long final);

   int set_cls(ads_name name,int in);   
   int set_sub(ads_name name,int in);   
   int set_num_el(ads_name name,int in);
   int set_initial_node(ads_name name,long in);
   int set_final_node(ads_name name,long in);
DllExport int set_initialfinal_node(ads_name name, long init, long final);

   int operator = (C_EED &eed);
};


          int   gsc_getstringmask(int cronly, TCHAR *prompt, TCHAR *result, TCHAR mask);
DllExport int   gsc_path_conv(C_STRING &path);
DllExport int   gsc_path_conv(TCHAR path[], int maxlen);
          int   gsc_validdir(C_STRING &dir);
          int   gs_create_dir(void);
          int   gs_dir_exist (void);
DllExport int   gsc_dir_exist(C_STRING &path, int retest = MORETESTS);
DllExport int   gsc_dir_exist(TCHAR *path, int retest = MORETESTS);
          int   gs_path_exist(void);
          int   gsc_drive_exist(const TCHAR *path);
DllExport int   gsc_path_exist(C_STRING &path, int PrintError = GS_GOOD);
DllExport int   gsc_path_exist(const TCHAR *path, int PrintError = GS_GOOD);
DllExport int   gsc_mkdir(C_STRING &dirname);
DllExport int   gsc_mkdir(const TCHAR *dirname);
DllExport int   gsc_mkdir_from_path(C_STRING &path, int retest = MORETESTS);
DllExport int   gsc_mkdir_from_path(const TCHAR *path, int retest = MORETESTS);
          int   gsc_rmdir(C_STRING &dirname, int retest = MORETESTS);
          int   gsc_rmdir(const TCHAR *dirname, int retest = MORETESTS);
          int   gsc_rmdir_from_path(C_STRING &path, int retest = MORETESTS);
          int   gsc_rmdir_from_path(const TCHAR *path, int retest = MORETESTS);
          int   gs_get_tmp_filename(void);
DllExport int   gsc_get_tmp_filename(const TCHAR *dir_rif, const TCHAR *prefix, const TCHAR *ext,
                                     TCHAR **filetemp);
DllExport int   gsc_get_tmp_filename(const TCHAR *dir_rif, const TCHAR *prefix, const TCHAR *ext,
                                     C_STRING &filetemp);
          int   gsc_SetFileTime(C_STRING &Path, CONST FILETIME *lpTime = NULL);
          int   gsc_geosimdir (C_STRING &dir);
          int   gsc_geoworkdir(C_STRING &dir);
DllExport int   gsc_copyfile(C_STRING &Source, C_STRING &Target, int retest = MORETESTS);
DllExport int   gsc_copyfile (const TCHAR *Source, const TCHAR *Target, int retest = MORETESTS);
          int   gsc_appendfile(C_STRING &Source, C_STRING &Target);
          bool  gsc_isUnicodeFile(FILE *File);
DllExport int   gsc_sopen(const TCHAR *path, int oflag, int shflag, int pmode = -1, 
                          int retest = MORETESTS);
DllExport FILE *gsc_fopen(C_STRING &path, const TCHAR *mode, int retest = MORETESTS,
                          bool *Unicode = NULL);
DllExport FILE *gsc_fopen(const TCHAR *path, const TCHAR *mode, int retest = MORETESTS,
                          bool *Unicode = NULL);
DllExport int   gsc_fclose(FILE *file);
          long  gsc_filesize(FILE *stream);
          int   gsc_identicalFiles(C_STRING &File1, C_STRING &File2);
          int   gsc_identicalFiles(const TCHAR *File1, const TCHAR *File2);
DllExport TCHAR gsc_readchar(FILE *fp, bool Unicode = false);
          size_t gsc_fread(TCHAR *buffer, size_t count, FILE *fp, bool Unicode = false);
DllExport int   gsc_fputs(C_STRING &Buffer, FILE *fp, bool Unicode = false);
          int   gsc_fputs(TCHAR *Buffer, FILE *fp, bool Unicode = false);
          int   gsc_fputc(TCHAR Buffer, FILE *fp, bool Unicode = false);
DllExport int   gsc_readline(FILE *fp, TCHAR **rigaletta, bool Unicode = false);
DllExport int   gsc_readline(FILE *fp, C_STRING &buffer, bool Unicode = false);

DllExport int   gsc_load_dialog(C_STRING &dclfile, int *dcl_id);
DllExport int   gsc_load_dialog(const TCHAR *dclfile, int *dcl_id);
          int   gsc_copydir(TCHAR *source_dir, TCHAR *what, TCHAR *dest_dir,
                            int recursive = RECURSIVE, TCHAR *except = NULL);
          int   gs_delfile (void);
DllExport int   gsc_delfile(C_STRING &path, int retest = MORETESTS);
DllExport int   gsc_delfile (const TCHAR *path, int retest = MORETESTS);
DllExport int   gsc_delfiles(const TCHAR *mask, int retest = MORETESTS);
          int   gsc_renfile(TCHAR *OldName, TCHAR *NewName);
          int   gsc_renfile(C_STRING &OldName, C_STRING &NewName);
          long  gsc_filedim (TCHAR*);
          int   gs_dir_from_path(void);
DllExport int   gsc_dir_from_path(C_STRING &path, C_STRING &dir);
DllExport int   gsc_dir_from_path(const TCHAR *path, C_STRING &dir);

DllExport TCHAR *gsc_tostring(short);
DllExport TCHAR *gsc_tostring(int);
DllExport TCHAR *gsc_tostring(long);
DllExport TCHAR *gsc_tostring(float);
DllExport TCHAR *gsc_tostring(float, int digits, int dec = -1);
DllExport TCHAR *gsc_tostring(double);
DllExport TCHAR *gsc_tostring(double, int digits, int dec = -1);
DllExport TCHAR *gsc_tostring(const TCHAR *in);
DllExport TCHAR *gsc_tostring(ads_point pt);
DllExport TCHAR *gsc_tostring(ads_point pt, int digits, int dec = -1);
DllExport TCHAR *gsc_tostring_format(double, int digits, int dec = -1);

int gsc_Str2Pt(TCHAR *string, ads_point pt);

DllExport TCHAR *gsc_strcat (TCHAR *first, const TCHAR *second);
          TCHAR *gsc_strcat (TCHAR *first, TCHAR second);
DllExport TCHAR *gsc_strtran(TCHAR *source, TCHAR *sub, TCHAR *con, int sensitive = TRUE);
DllExport int   gsc_strsep (TCHAR*, TCHAR ch, TCHAR ric = _T(','));

          TCHAR *gsc_ltrim  (TCHAR *stringa);
DllExport TCHAR *gsc_rtrim  (TCHAR *stringa, TCHAR charToTrim = _T(' '));
DllExport TCHAR *gsc_alltrim(TCHAR *stringa);

DllExport TCHAR gsc_CharToUnicode(const char CharCharacter);
TCHAR *gsc_CharToUnicode(const char *CharString);
DllExport char gsc_UnicodeToChar(const TCHAR UnicodeCharacter);
char *gsc_UnicodeToChar(const TCHAR *UnicodeString);

          void  gsc_wait     (int);
DllExport int   gsc_toupper(TCHAR *stringa); 
          int   gsc_toupper(char *stringa); 
DllExport int   gsc_tolower(TCHAR *stringa);

DllExport long  gsc_adir(const TCHAR *path, C_STR_LIST *pFileNameList = NULL, 
                         C_LONG_LIST *pFileSizeList = NULL,
                         C_STR_LIST *pFileDatetimeList = NULL, 
                         C_STR_LIST *pFileAttrList = NULL, 
                         bool OnlyFiles = true);
DllExport long  gsc_adir(const TCHAR *path, presbuf *filename = NULL, presbuf *filesize = NULL,
                         presbuf *filedatetime = NULL, presbuf *fileattr = NULL);
DllExport int   gsc_delall(TCHAR *dir, int recursive, int removeDir = TRUE, int retest = GS_GOOD);
extern    int   gs_delall(void);

DllExport TCHAR *gsc_strstr(TCHAR *str1, const TCHAR *str2, int sensitive = TRUE);
TCHAR *gsc_strrstr(TCHAR *str1, const TCHAR *str2, int sensitive = TRUE);
DllExport int   gsc_strcmp(const TCHAR *str1, const TCHAR *str2, int sensitive = TRUE);
DllExport size_t gsc_strlen(const TCHAR *str);
DllExport TCHAR *gsc_strcpy(TCHAR *dest, const TCHAR *source, size_t dim);
DllExport char *gsc_strcpy(char *dest, const char *source, size_t dim);

DllExport int gsc_read_profile(const TCHAR *Path, C_PROFILE_SECTION_BTREE &Sections, bool *Unicode = NULL);
DllExport int gsc_read_profile(C_STRING &Path, C_PROFILE_SECTION_BTREE &Sections, bool *Unicode = NULL);
DllExport int gsc_write_profile(const TCHAR *Path, C_PROFILE_SECTION_BTREE &Sections, bool Unicode = false);
DllExport int gsc_write_profile(C_STRING &Path, C_PROFILE_SECTION_BTREE &Sections, bool Unicode = false);

DllExport FILE *gsc_open_profile(C_STRING &pathfile, int mode = UPDATEABLE,
                                 int retest = MORETESTS, bool *Unicode = NULL);
DllExport FILE *gsc_open_profile(const TCHAR *pathfile, int mode = UPDATEABLE,
                                 int retest = MORETESTS, bool *Unicode = NULL);
DllExport int   gsc_close_profile(FILE *file);

          int   gsc_get_profile(TCHAR *filename, const TCHAR *sez, const TCHAR *entry,
                                TCHAR **target, size_t dim, long start);
DllExport int   gsc_get_profile(FILE *file, const TCHAR *sez, const TCHAR *entry,
                                TCHAR **target, size_t dim, long start, bool Unicode = false);
int gsc_get_profile(C_STRING &Path, const TCHAR *sez, const TCHAR *entry, C_STRING &target);
int gsc_get_profile(FILE *file, long FileSectionPos, const TCHAR *entry,
                    C_STRING &target, bool Unicode = false);
DllExport int gsc_get_profile(FILE *file, const TCHAR *sez, const TCHAR *entry,
                              C_STRING &target, bool Unicode = false);
DllExport int gsc_get_profile(FILE *file, const TCHAR *sez, C_2STR_LIST &EntryList,
                              bool Unicode = false);
DllExport int gsc_get_profile(FILE *file, C_STR_LIST &SezList, bool Unicode = false);

          int gsc_set_profile(C_STRING &filename, const TCHAR *sez, const TCHAR *entry,
                              int value, bool Unicode = false);
          int gsc_set_profile(C_STRING &filename, const TCHAR *sez, const TCHAR *entry,
                              long value, bool Unicode = false);
          int gsc_set_profile(C_STRING &filename, const TCHAR *sez, const TCHAR *entry,
                              double value, bool Unicode = false);
          int gsc_set_profile(C_STRING &filename, const TCHAR *sez, const TCHAR *entry,
                              const TCHAR *value, bool Unicode = false);
          int gsc_set_profile(TCHAR *filename, const TCHAR *sez, const TCHAR *entry,
                              const TCHAR *value, bool Unicode = false);
DllExport int gsc_set_profile(FILE *file, const TCHAR *sez, const TCHAR *entry,
                              const TCHAR *source, long start = 0,
                              bool Unicode = false);
DllExport int gsc_set_profile(FILE *file, const TCHAR *sez, C_2STR_LIST &EntryList,
                              bool Unicode = false);

DllExport int   gsc_find_sez(FILE *file, const TCHAR *sez, size_t len, long *pos = NULL,
                             bool Unicode = false);
DllExport int   gsc_find_entry(FILE *file, const TCHAR *entry, size_t *len_profile, 
                               long *pos = NULL, bool Unicode = false);

DllExport int   gsc_append_profile(FILE *file, const TCHAR *sez, const TCHAR *entry,
                                   const TCHAR *source, size_t len, bool Unicode = false);
          int   gsc_insert_entry(FILE *file, const TCHAR *entry, const TCHAR *source, size_t len, 
                                 int flag, bool Unicode = false);

          int   gsc_delete_sez(TCHAR *filename, const TCHAR *sez);
DllExport int   gsc_delete_sez(FILE *file, const TCHAR *sez, bool Unicode = false);

          int   gsc_delete_entry(C_STRING &filename, const TCHAR *sez, const TCHAR *entry);
          int   gsc_delete_entry(TCHAR *filename, const TCHAR *sez, const TCHAR *entry);
DllExport int   gsc_delete_entry(FILE *file, const TCHAR *sez, const TCHAR *entry,
                                 bool Unicode = false);

int gsc_getInfoFromINI(const TCHAR *entry, TCHAR *value, int dim_value, C_STRING *IniFile = NULL);
DllExport int gsc_getInfoFromINI(const TCHAR *entry, C_STRING &value, C_STRING *IniFile = NULL);
int gsc_setInfoToINI(const TCHAR *entry, TCHAR *value, C_STRING *IniFile = NULL);
DllExport int gsc_setInfoToINI(const TCHAR *entry, C_STRING &value, C_STRING *IniFile = NULL);

int gs_getPathInfoFromINI(void);
DllExport int gsc_getPathInfoFromINI(const TCHAR *entry, C_STRING &value,
                                     C_STRING *IniFile = NULL);
int gs_setPathInfoToINI(void);
DllExport int gsc_setPathInfoToINI(const TCHAR *entry, C_STRING &value,
                                   C_STRING *IniFile = NULL);
  
DllExport int gsc_goto_next_line(FILE *file, bool Unicode = false);

          int gsc_write_log(C_STRING &Msg);    // scrittura messaggi nel file di log
          int gsc_write_log(const TCHAR *Msg); // scrittura messaggi nel file di log
          int gsc_get_NotValidGraphObjMsg(AcDbEntity *pObj, C_STRING &Msg);
          int gsc_get_NotValidGraphObjMsg(ads_name ent, C_STRING &Msg);
          int gsc_NotValidGraphObj_write_log(ads_name ent); // scrittura messaggi di oggetto non valido nel file di log

DllExport double gsc_get_profile_curr_time(void);
DllExport int gsc_profile_int_func(double t1, int result, double t2, const char *fName);
DllExport void gsc_clear_profiling(void);
void gsc_write_profiling(const char *FunctionName, double Timing);

#if defined(GSPROFILEFILE)
   #define PROFILE_INT_FUNC(x, ...) gsc_profile_int_func(gsc_get_profile_curr_time(), x(__VA_ARGS__), gsc_get_profile_curr_time(), #x)
#else
   #define PROFILE_INT_FUNC(x, ...) x(__VA_ARGS__)
#endif

DllExport double gsc_grd2rad(double grd);
DllExport double gsc_rad2grd(double rad);

DllExport double gsc_gons2rad(double gons);
DllExport double gsc_rad2gons(double rad);

          TCHAR *gsc_getRotMeasureUnitsDescription(RotationMeasureUnitsEnum rotation_unit);
          TCHAR *gsc_getColorFormatDescription(GSFormatColorEnum color_format);

          int   gsc_getconfirm(const TCHAR *prompt, int *result, int rispdefault = GS_BAD,
                               int SecurityFlag = GS_BAD);
          int   gs_getconfirm(void);
DllExport int   gsc_ddgetconfirm(const TCHAR *prompt, int *result, int rispdefault = GS_BAD,
                                 int SecurityFlag = GS_BAD, int TastoNO = GS_BAD, int ScrollBar = GS_BAD);
          int   gs_ddgetconfirm(void);
          int   gs_ddalert(void);
DllExport void  gsc_ddalert(TCHAR *prompt, HWND hWnd = NULL);
          int   gs_ddinput(void);
DllExport int   gsc_ddinput(const TCHAR *Msg, C_STRING &Out, const TCHAR *Def = NULL,
                            int isSQLParam = TRUE, int WithSuggestionButtons = TRUE);
DllExport presbuf gsc_splitIntoNumRows(const TCHAR *stringa, int maxriga);

DllExport int gsc_radix2decimal(TCHAR *Source, int SourceRadix, long *Decimal);
DllExport int gsc_decimal2radix(long Decimal, int DestRadix, TCHAR **Dest);
DllExport int   gsc_is_numeric(const TCHAR *string);

          double gsc_round(double n, int dec = -1);
          double gsc_truncate(double n, int dec = -1);

DllExport int   gsc_base362long(TCHAR* number_base36, long* number); 
DllExport int   gsc_long2base36(TCHAR** number_base36, long number, size_t lenStr); 
          int   gsc_setfilenamecode(int prj, int cl, TCHAR* dir, TCHAR** number_base36); 
          int   gsc_tempfilebase36(const TCHAR *dir_rif, const TCHAR *prefix, 
                                   TCHAR *ext, TCHAR **filetempbase36);

          double gsc_get_UnitCoordConvertionFactor(GSSRIDUnitEnum Source, GSSRIDUnitEnum Dest);

DllExport int gsc_mainentnext(ads_name ent, ads_name next);

DllExport int gsc_FindSupportFiles(const TCHAR *AttName, int prj, int cls, int sub, int sec,
                                   C_STRING &OutPath, ValuesListTypeEnum *type);
int gsc_FindSupportFiles(const TCHAR *dir, const TCHAR *AttName,
                         int prj, int cls, int sub, int sec,
                         C_STRING &OutPath, ValuesListTypeEnum *type);
int gsc_CopySupportFile(const TCHAR *AttName, int SrcPrj, int SrcCls, int SrcSub, int SrcSec,
                        int DstPrj, int DstCls, int DstSub, int DstSec);
int gsc_DelSupportFile(const TCHAR *AttName, int Prj, int Cls, int Sub, int Sec);

DllExport int gsc_ssget(const TCHAR *str, const void *pt1, const void *pt2,
                        const struct resbuf *filter, ads_name ss);
DllExport int gsc_ssget(const TCHAR *str, const void *pt1, const void *pt2,
                        const struct resbuf *filter, C_SELSET &ss);
int gs_AskSelSet(void);


//-----------------------------------------------------------------------//
//  RIDEFINIZIONE FUNZIONI STANDARD PER COMPATIBILTA' UNIX-DOS           //
//-----------------------------------------------------------------------//


DllExport TCHAR *gsc_itoa(int value, TCHAR *string, int radix = 10);
DllExport TCHAR *gsc_ltoa(int value, TCHAR *string, int radix = 10);
DllExport void gsc_makepath(C_STRING &path, const TCHAR *drive, const TCHAR *dir, 
                            const TCHAR *file, const TCHAR *ext);
          void gsc_makepath(TCHAR *path, TCHAR *drive, TCHAR *dir, TCHAR *file, TCHAR *ext);
DllExport void gsc_splitpath(TCHAR *path, TCHAR *drive, TCHAR *dir, TCHAR *file, TCHAR *ext);
DllExport void gsc_splitpath(C_STRING &path, C_STRING *drive = NULL, C_STRING *dir = NULL,
                             C_STRING *file = NULL, C_STRING *ext = NULL);
DllExport void gsc_splitpath(const TCHAR *path, C_STRING *drive = NULL, C_STRING *dir = NULL,
                             C_STRING *file = NULL, C_STRING *ext = NULL);
          int gs_splitpath(void);

          TCHAR *gsc_fullpath(TCHAR *buffer, const TCHAR *path, size_t maxlen);
          TCHAR *gsc_fullpath(C_STRING &absPath, C_STRING &relPath);

          int gs_tostring (void);

// DATE
int gs_conv_DateTime_2_WinFmt(void);
int gsc_conv_DateTime_2_WinFmt(const TCHAR *DateTimeSource, C_STRING &DateTimeDest);
int gsc_conv_DateTime_2_WinFmt(presbuf DateTimeSource, C_STRING &DateTimeDest);
int gs_conv_DateTime_2_WinShortFmt(void);
// ORA
int gs_conv_Time_2_WinFmt(void);
int gsc_conv_Time_2_WinFmt(const TCHAR *TimeSource, C_STRING &TimeDest);
int gsc_conv_Time_2_WinFmt(presbuf TimeSource, C_STRING &TimeDest);

DllExport int gsc_getWinShortDateTime(const TCHAR *DateTime, C_STRING &ShortDateTime);
int gsc_getWinShortDateTime4DgtYear(const TCHAR *DateTime, C_STRING &ShortDateTime);
DllExport int gsc_getGEOsimDateTime(const TCHAR *DateTime, C_STRING &GEOsimDateTime);
DllExport int gsc_getSQLDateTime(const TCHAR *DateTime, C_STRING &MAPSQLDateTime);
DllExport int gsc_getSQLDate(const TCHAR *Date, C_STRING &MAPSQLDate);
DllExport int gsc_getSQLTime(const TCHAR *Date, C_STRING &MAPSQLDate);

DllExport void gsc_current_DateTime(C_STRING &DateTime);
DllExport void gsc_current_Date(C_STRING &Date);
DllExport void gsc_current_Time(C_STRING &Time);
void WINAPI gsc_ddChooseDate(C_STRING &DateTime);
DllExport int gsc_conv_date(const TCHAR *DateSource, const TCHAR *DrvSource, int FormatSource,
                            TCHAR *DateDest, const TCHAR *DrvDest, int FormatDest);
int gsc_getYearDateTime(const TCHAR *DateTime);
int gsc_getMonthDateTime(const TCHAR *DateTime);
int gsc_getDayDateTime(const TCHAR *DateTime);
void gsc_getDateTimeSpanFormat(COleDateTimeSpan &Span, C_STRING &Msg);

// NUMERI
int gs_conv_Number(void);
DllExport int gsc_conv_Number(double NumberSource, C_STRING &NumberDest);
DllExport int gsc_conv_Number(const TCHAR *NumberSource, double *NumberDest);
int gsc_conv_Number(presbuf NumberSource, C_STRING &NumberDest);
int gsc_conv_Number(double NumberSource, C_STRING &NumberDest, int len, int dec);
int gsc_conv_Number(presbuf NumberSource, C_STRING &NumberDest, int len, int dec);

// VALUTA
int gs_conv_Currency(void);
int gsc_conv_Currency(double CurrencySource, C_STRING &CurrencyDest);
int gsc_conv_Currency(TCHAR *CurrencySource, double *CurrencyDest);
int gsc_conv_Currency(presbuf CurrencySource, C_STRING &CurrencyDest);

// BOOLEANI
int gs_conv_Bool(void);
DllExport int gsc_conv_Bool(int BoolSource, C_STRING &BoolDest);
DllExport int gsc_conv_Bool(TCHAR *BoolSource, int *BoolDest);
DllExport int gsc_conv_Bool(presbuf BoolSource, C_STRING &BoolDest);

// PUNTI
int gsc_conv_Point(ads_point PtSource, C_STRING &PtDest);

DllExport int gsc_ddSuggestDBValue(C_STRING &Value, DataTypeEnum DataType);

DllExport int gsc_GetAcadLanguage(int *lan);
DllExport int gsc_GetAcadLocation(C_STRING &Dir);

DllExport int gsc_callCmd(TCHAR *pCmdName, ...);
DllExport int gsc_callCmd(TCHAR *pCmdName, struct resbuf *pParms);

int gsc_getLocaleDecimalSep(C_STRING &str);
int gsc_getLocaleListSep(C_STRING &str);

int gsc_getLocaleShortDateFormat(C_STRING &str);
int gsc_getLocaleLongDateFormat(C_STRING &str);
int gsc_getLocaleDateSep(C_STRING &str);

int gsc_getLocaleTimeFormat(C_STRING &str);
int gsc_getLocaleTimeSep(C_STRING &str);


int gsc_getClsTypeDescr(int Type, C_STRING &StrType);
int gsc_getSecTypeDescr(SecondaryTableTypesEnum Type, C_STRING &StrType);
int getClsCategoryDescr(int Category, C_STRING &StrCategory);
DllExport int gsc_getClsCategoryTypeDescr(int category, int type, C_STRING &tp_class);
void gsc_getConnectDescr(int Connect, C_STRING &StrConnect);

int gs_help(void);
DllExport void gsc_help(int iTopic, HWND WndCaller = NULL, TCHAR *szFilename = NULL);

int gs_dd_get_existingDir(void);
DllExport int gsc_dd_get_existingDir(const TCHAR *Title, const TCHAR *CurrentDir,
                                     C_STRING &OutDir);
int gsc_getGEOsimUsrAppDataPath(C_STRING &Value);

DllExport int gsc_DocumentExecute(const TCHAR *Document, int Sync = TRUE);

// GESTIONE FILES .UDL
DllExport TCHAR *gsc_LoadOleDBIniStrFromFile(const TCHAR *Path);
DllExport int gsc_WriteOleDBIniStrToFile(const TCHAR *ConnStr, const TCHAR *Path);
TCHAR *gsc_SearchACADUDLFile(const TCHAR *Prefix, const TCHAR *ConnStr);
TCHAR *gsc_CreateACADUDLFile(const TCHAR *Prefix, const TCHAR *ConnStr);

// NOTIFICHE IN RETE
int gsc_getComputerName(C_STRING &ComputerName);
int gsc_setWaitForExtraction(int prj, int cls);
DllExport int gsc_setWaitForExtraction(int prj, C_INT_LIST &ClsList);
int gsc_NotifyWaitForExtraction(int prj, int cls, const TCHAR *msg);
int gsc_NotifyWaitForExtraction(int prj, C_INT_LIST &ClsList, const TCHAR *msg);
int gsc_isWaitingForExtraction(int prj, int cls);
DllExport int gsc_isWaitingForExtraction(int prj, C_INT_LIST &ClsList);
int gsc_setWaitForSave(int prj, int cls);
int gsc_setWaitForSave(int prj, C_INT_LIST &ClsList);
int gsc_isWaitingForSave(int prj, int cls);
int gsc_isWaitingForSave(int prj, C_INT_LIST &ClsList);
int gsc_NotifyWaitForSave(int prj, int cls, const TCHAR *msg);
int gsc_NotifyWaitForSave(int prj, C_INT_LIST &ClsList, const TCHAR *msg);
int gsc_Notify(const TCHAR *computer, const TCHAR *msg);

// FINESTRA DI PRESENTAZIONE
void WINAPI gsc_DisplaySplash(void);

// GESTIONE BITMAP
int gsc_WriteDIB(const TCHAR *Path, HANDLE hDIB);
HANDLE DDBToDIB(CBitmap& bitmap, DWORD dwCompression = BI_RGB, CPalette* pPal = NULL);
void gsc_CaptureWindowScreen(HWND hWnd, CBitmap &srcImage);

// FUNZIONI PER LICENZE IN RETE
void gsc_Kript(TCHAR *str, size_t str_len, const TCHAR *PassWord = NULL);
void gsc_DeKript(TCHAR *str, size_t str_len, const TCHAR *PassWord = NULL);
int gsc_KriptFile(C_STRING &Source, C_STRING &Target, const TCHAR *PassWord = NULL);
int gsc_DeKriptFile(C_STRING &Source, C_STRING &Target, const TCHAR *PassWord = NULL);
int gs_KriptFile(void);
int gs_DeKriptFile(void);

int gs_setMaxLicNum(void);
int gs_getMaxLicNum(void);
int gsc_getMaxLicNum(void);
int gs_getActualGEOsimUsrList(void);
int gsc_startAutorization(void);

// FUNZIONI PER PROFILI AUTOCAD
int gsc_getGEOsimProfileRegistryKey(C_STRING &CurrProfile);
int gsc_getGEOsimQNEWTEMPLATERegistryKey(C_STRING &Value);

DllExport bool gsc_isCurrentDWGModified(void);

DllExport int gsc_GetFileD(const ACHAR *title, C_STRING &defawlt, const ACHAR *ext,
                           int flags, C_STRING &Path);
DllExport int gsc_GetFileD(const ACHAR *title, const ACHAR *defawlt, const ACHAR *ext,
                           int flags, C_STRING &Path);

// DCL
DllExport int gsc_add_list(const ACHAR *item);
DllExport int gsc_add_list(C_STRING &item);

void gsc_print_mem_status(bool extendexInfo = false);

void gsc_set_variant_t(_variant_t &Value, bool NewValue);
void gsc_set_variant_t(_variant_t &Value, short NewValue);
void gsc_set_variant_t(_variant_t &Value, int NewValue);
void gsc_set_variant_t(_variant_t &Value, long NewValue);
void gsc_set_variant_t(_variant_t &Value, double NewValue);
void gsc_set_variant_t(_variant_t &Value, const TCHAR *NewValue);
void gsc_set_variant_t(_variant_t &Value, C_STRING &NewValue);
void gsc_set_variant_t(_variant_t &Value, COleDateTime NewValue);

int gs_ChangeEEDAggr(void);
int gs_ChangeEEDCls(void);
int gs_ChangeODCls(void);
int gs_ClearGSMarker(void);
int gs_ClearAdeLock(void);
int gs_get_eed(void);

DllExport int gsc_ChangeColorToBmp(HBITMAP hBitmap, COLORREF crFrom, COLORREF crTo);
DllExport int gsc_get_Bitmap(GSBitmapTypeEnum BitmapType, CBitmap &CBMP);

int gsc_close_AcDbEntities(AcDbEntityPtrArray &EntArray);

void gsc_CenterDialog(CWnd *MyDialogPtr);

// GOOGLE
int gsc_getAddressFromLL(double _lat, double _long, C_STRING &GoogleKey, C_STRING &Address);
int gs_getAddressFromLL(void);
int gsc_getLLFromAddress(C_STRING &Address, C_STRING &GoogleKey, double &_lat, double &_long);
int gs_getLLFromAddress(void);
int gsc_getLLDirectionFromPts(C_POINT_LIST &Pts, C_STRING &GoogleKey, C_POINT_LIST &result);
int gs_getLLDirectionFromPts(void);

int gsc_getGUID(C_STRING &Res);
int gs_getGUID(void);

extern HINSTANCE g_hInst;

#endif


