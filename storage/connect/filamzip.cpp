/*********** File AM Zip C++ Program Source Code File (.CPP) ***********/
/* PROGRAM NAME: FILAMZIP                                              */
/* -------------                                                       */
/*  Version 1.4                                                        */
/*                                                                     */
/* COPYRIGHT:                                                          */
/* ----------                                                          */
/*  (C) Copyright to the author Olivier BERTRAND          2005-2013    */
/*                                                                     */
/* WHAT THIS PROGRAM DOES:                                             */
/* -----------------------                                             */
/*  This program are the ZLIB compressed files classes.                */
/*                                                                     */
/***********************************************************************/

/***********************************************************************/
/*  Include relevant MariaDB header file.                  */
/***********************************************************************/
#include "my_global.h"
#if defined(WIN32)
#include <io.h>
#include <fcntl.h>
#if defined(__BORLANDC__)
#define __MFC_COMPAT__                   // To define min/max as macro
#endif
//#include <windows.h>
#else   // !WIN32
#if defined(UNIX)
#include <errno.h>
#else   // !UNIX
#include <io.h>
#endif
#include <fcntl.h>
#endif  // !WIN32

/***********************************************************************/
/*  Include application header files:                                  */
/*  global.h    is header containing all global declarations.          */
/*  plgdbsem.h  is header containing the DB application declarations.  */
/*  tabdos.h    is header containing the TABDOS class declarations.    */
/***********************************************************************/
#include "global.h"
#include "plgdbsem.h"
//#include "catalog.h"
//#include "reldef.h"
//#include "xobject.h"
//#include "kindex.h"
#include "filamtxt.h"
#include "tabdos.h"
#if defined(UNIX)
#include "osutil.h"
#endif

/***********************************************************************/
/*  This define prepares ZLIB function declarations.                   */
/***********************************************************************/
//#define ZLIB_DLL

#include "filamzip.h"

/***********************************************************************/
/*  DB static variables.                                               */
/***********************************************************************/
extern int num_read, num_there, num_eq[];                 // Statistics

/* ------------------------------------------------------------------- */

/***********************************************************************/
/*  Implementation of the ZIPFAM class.                                */
/***********************************************************************/
ZIPFAM::ZIPFAM(PZIPFAM txfp) : TXTFAM(txfp)
  {
  Zfile = txfp->Zfile;
  Zpos = txfp->Zpos;
  } // end of ZIPFAM copy constructor

/***********************************************************************/
/*  Zerror: Error function for gz calls.                               */
/*  gzerror returns the error message for the last error which occurred*/
/*  on the given compressed file. errnum is set to zlib error number.  */
/*  If an error occurred in the file system and not in the compression */
/*  library, errnum is set to Z_ERRNO and the application may consult  */
/*  errno to get the exact error code.                                 */
/***********************************************************************/
int ZIPFAM::Zerror(PGLOBAL g)
  {
  int errnum;

  strcpy(g->Message, gzerror(Zfile, &errnum));

  if (errnum == Z_ERRNO)
#if defined(WIN32)
    sprintf(g->Message, MSG(READ_ERROR), To_File, strerror(NULL));
#else   // !WIN32
    sprintf(g->Message, MSG(READ_ERROR), To_File, strerror(errno));
#endif  // !WIN32

    return (errnum == Z_STREAM_END) ? RC_EF : RC_FX;
  } // end of Zerror

/***********************************************************************/
/*  Reset: reset position values at the beginning of file.             */
/***********************************************************************/
void ZIPFAM::Reset(void)
  {
  TXTFAM::Reset();
//gzrewind(Zfile);                  // Useful ?????
  Zpos = 0;
  } // end of Reset

/***********************************************************************/
/*  ZIP GetFileLength: returns an estimate of what would be the        */
/*  uncompressed file size in number of bytes.                         */
/***********************************************************************/
int ZIPFAM::GetFileLength(PGLOBAL g)
  {
  int len = TXTFAM::GetFileLength(g);

  if (len > 0)
    // Estimate size reduction to a max of 6
    len *= 6;

  return len;
  } // end of GetFileLength

/***********************************************************************/
/*  ZIP Access Method opening routine.                                 */
/***********************************************************************/
bool ZIPFAM::OpenTableFile(PGLOBAL g)
  {
  char    opmode[4], filename[_MAX_PATH];
  MODE    mode = Tdbp->GetMode();

  switch (mode) {
    case MODE_READ:
      strcpy(opmode, "r");
      break;
    case MODE_UPDATE:
      /*****************************************************************/
      /* Updating ZIP files not implemented yet.                       */
      /*****************************************************************/
      strcpy(g->Message, MSG(UPD_ZIP_NOT_IMP));
      return true;
    case MODE_DELETE:
      if (!Tdbp->GetNext()) {
        // Store the number of deleted lines
        DelRows = Cardinality(g);

        // This will erase the entire file
        strcpy(opmode, "w");
//      Block = 0;                // For ZBKFAM
//      Last = Nrec;              // For ZBKFAM
        Tdbp->ResetSize();
      } else {
        sprintf(g->Message, MSG(NO_PART_DEL), "ZIP");
        return true;
      } // endif filter

      break;
    case MODE_INSERT:
      strcpy(opmode, "a+");
      break;
    default:
      sprintf(g->Message, MSG(BAD_OPEN_MODE), mode);
      return true;
    } // endswitch Mode

  /*********************************************************************/
  /*  Open according to logical input/output mode required.            */
  /*  Use specific zlib functions.                                     */
  /*  Treat files as binary.                                           */
  /*********************************************************************/
  strcat(opmode, "b");
  Zfile = gzopen(PlugSetPath(filename, To_File, Tdbp->GetPath()), opmode);

  if (Zfile == NULL) {
    sprintf(g->Message, MSG(GZOPEN_ERROR),
            opmode, (int)errno, filename);
    strcat(strcat(g->Message, ": "), strerror(errno));
    return (mode == MODE_READ && errno == ENOENT)
            ? PushWarning(g, Tdbp) : true;
    } // endif Zfile

  /*********************************************************************/
  /*  Something to be done here. >>>>>>>> NOT DONE <<<<<<<<            */
  /*********************************************************************/
//To_Fb = dbuserp->Openlist;     // Keep track of File block

  /*********************************************************************/
  /*  Allocate the line buffer.                                        */
  /*********************************************************************/
  return AllocateBuffer(g);
  } // end of OpenTableFile

/***********************************************************************/
/*  Allocate the line buffer. For mode Delete a bigger buffer has to   */
/*  be allocated because is it also used to move lines into the file.  */
/***********************************************************************/
bool ZIPFAM::AllocateBuffer(PGLOBAL g)
  {
  MODE mode = Tdbp->GetMode();

  Buflen = Lrecl + 2;                     // Lrecl does not include CRLF
//Buflen *= ((Mode == MODE_DELETE) ? DOS_BUFF_LEN : 1);    NIY

#ifdef DEBTRACE
 htrc("SubAllocating a buffer of %d bytes\n", Buflen);
#endif

  To_Buf = (char*)PlugSubAlloc(g, NULL, Buflen);

  if (mode == MODE_INSERT) {
    /*******************************************************************/
    /*  For Insert buffer must be prepared.                            */
    /*******************************************************************/
    memset(To_Buf, ' ', Buflen);
    To_Buf[Buflen - 2] = '\n';
    To_Buf[Buflen - 1] = '\0';
    } // endif Insert

  return false;
  } // end of AllocateBuffer

/***********************************************************************/
/*  GetRowID: return the RowID of last read record.                    */
/***********************************************************************/
int ZIPFAM::GetRowID(void)
  {
  return Rows;
  } // end of GetRowID

/***********************************************************************/
/*  GetPos: return the position of last read record.                   */
/***********************************************************************/
int ZIPFAM::GetPos(void)
  {
  return (int)Zpos;
  } // end of GetPos

/***********************************************************************/
/*  GetNextPos: return the position of next record.                    */
/***********************************************************************/
int ZIPFAM::GetNextPos(void)
  {
  return gztell(Zfile);
  } // end of GetNextPos

/***********************************************************************/
/*  SetPos: Replace the table at the specified position.               */
/***********************************************************************/
bool ZIPFAM::SetPos(PGLOBAL g, int pos)
  {
  sprintf(g->Message, MSG(NO_SETPOS_YET), "ZIP");
  return true;
#if 0
  Fpos = pos;

  if (fseek(Stream, Fpos, SEEK_SET)) {
    sprintf(g->Message, MSG(FSETPOS_ERROR), Fpos);
    return true;
    } // endif

  Placed = true;
  return false;
#endif // 0
  } // end of SetPos

/***********************************************************************/
/*  Record file position in case of UPDATE or DELETE.                  */
/***********************************************************************/
bool ZIPFAM::RecordPos(PGLOBAL g)
  {
  Zpos = gztell(Zfile);
  return false;
  } // end of RecordPos

/***********************************************************************/
/*  Skip one record in file.                                           */
/***********************************************************************/
int ZIPFAM::SkipRecord(PGLOBAL g, bool header)
  {
  // Skip this record
  if (gzeof(Zfile))
    return RC_EF;
  else if (gzgets(Zfile, To_Buf, Buflen) == Z_NULL)
    return Zerror(g);

  if (header)
    RecordPos(g);

  return RC_OK;
  } // end of SkipRecord

/***********************************************************************/
/*  ReadBuffer: Read one line from a compressed text file.             */
/***********************************************************************/
int ZIPFAM::ReadBuffer(PGLOBAL g)
  {
  char *p;
  int   rc;

  if (!Zfile)
    return RC_EF;

  if (!Placed) {
    /*******************************************************************/
    /*  Record file position in case of UPDATE or DELETE.              */
    /*******************************************************************/
#if defined(BLK_INDX)
   next:
#endif   // BLK_INDX
    if (RecordPos(g))
      return RC_FX;

    CurBlk = Rows++;                        // Update RowID

#if defined(BLK_INDX)
    /*******************************************************************/
    /*  Check whether optimization on ROWID                            */
    /*  can be done, as well as for join as for local filtering.       */
    /*******************************************************************/
    switch (Tdbp->TestBlock(g)) {
      case RC_EF:
        return RC_EF;
      case RC_NF:
        // Skip this record
        if ((rc = SkipRecord(g, FALSE)) != RC_OK)
          return rc;

        goto next;
      } // endswitch rc
#endif   // BLK_INDX
  } else
    Placed = false;

  if (gzeof(Zfile)) {
    rc = RC_EF;
  } else if (gzgets(Zfile, To_Buf, Buflen) != Z_NULL) {
    p = To_Buf + strlen(To_Buf) - 1;

    if (*p == '\n')
      *p = '\0';              // Eliminate ending new-line character

    if (*(--p) == '\r')
      *p = '\0';              // Eliminate eventuel carriage return

    strcpy(Tdbp->GetLine(), To_Buf);
    IsRead = true;
    rc = RC_OK;
    num_read++;
  } else
    rc = Zerror(g);

#ifdef DEBTRACE
 htrc(" Read: '%s' rc=%d\n", To_Buf, rc);
#endif
  return rc;
  } // end of ReadBuffer

/***********************************************************************/
/*  WriteDB: Data Base write routine for ZDOS access method.           */
/*  Update is not possible without using a temporary file (NIY).       */
/***********************************************************************/
int ZIPFAM::WriteBuffer(PGLOBAL g)
  {
  /*********************************************************************/
  /*  Prepare the write buffer.                                        */
  /*********************************************************************/
  strcat(strcpy(To_Buf, Tdbp->GetLine()), CrLf);

  /*********************************************************************/
  /*  Now start the writing process.                                   */
  /*********************************************************************/
  if (gzputs(Zfile, To_Buf) < 0)
    return Zerror(g);

  return RC_OK;
  } // end of WriteBuffer

/***********************************************************************/
/*  Data Base delete line routine for ZDOS access method.  (NIY)       */
/***********************************************************************/
int ZIPFAM::DeleteRecords(PGLOBAL g, int irc)
  {
  strcpy(g->Message, MSG(NO_ZIP_DELETE));
  return RC_FX;
  } // end of DeleteRecords

/***********************************************************************/
/*  Data Base close routine for DOS access method.                     */
/***********************************************************************/
void ZIPFAM::CloseTableFile(PGLOBAL g)
  {
  int rc = gzclose(Zfile);

#ifdef DEBTRACE
 htrc("ZIP CloseDB: closing %s rc=%d\n", To_File, rc);
#endif

  Zfile = NULL;            // So we can know whether table is open
//To_Fb->Count = 0;        // Avoid double closing by PlugCloseAll
  } // end of CloseTableFile

/***********************************************************************/
/*  Rewind routine for ZIP access method.                              */
/***********************************************************************/
void ZIPFAM::Rewind(void)
  {
  gzrewind(Zfile);
  } // end of Rewind

/* ------------------------------------------------------------------- */

/***********************************************************************/
/*  Constructors.                                                      */
/***********************************************************************/
ZBKFAM::ZBKFAM(PDOSDEF tdp) : ZIPFAM(tdp)
  {
  Blocked = true;
  Block = tdp->GetBlock();
  Last = tdp->GetLast();
  Nrec = tdp->GetElemt();
  CurLine = NULL;
  NxtLine = NULL;
  Closing = false;
#if defined(BLK_INDX)
  BlkPos = tdp->GetTo_Pos();
#else   // !BLK_INDX
  BlkPos = NULL;
#endif  // !BLK_INDX
  } // end of ZBKFAM standard constructor

ZBKFAM::ZBKFAM(PZBKFAM txfp) : ZIPFAM(txfp)
  {
  CurLine = txfp->CurLine;
  NxtLine = txfp->NxtLine;
  Closing = txfp->Closing;
  } // end of ZBKFAM copy constructor

/***********************************************************************/
/*  Use BlockTest to reduce the table estimated size.                  */
/***********************************************************************/
int ZBKFAM::MaxBlkSize(PGLOBAL g, int s)
  {
  int rc = RC_OK, savcur = CurBlk;
  int size;

  // Roughly estimate the table size as the sum of blocks
  // that can contain good rows
  for (size = 0, CurBlk = 0; CurBlk < Block; CurBlk++)
#if defined(BLK_INDX)
    if ((rc = Tdbp->TestBlock(g)) == RC_OK)
      size += (CurBlk == Block - 1) ? Last : Nrec;
    else if (rc == RC_EF)
      break;
#else   // !BLK_INDX
    size += (CurBlk == Block - 1) ? Last : Nrec;
#endif  // !BLK_INDX

  CurBlk = savcur;
  return size;
  } // end of MaxBlkSize

/***********************************************************************/
/*  ZBK Cardinality: returns table cardinality in number of rows.      */
/*  This function can be called with a null argument to test the       */
/*  availability of Cardinality implementation (1 yes, 0 no).          */
/***********************************************************************/
int ZBKFAM::Cardinality(PGLOBAL g)
  {
  // Should not be called in this version
  return (g) ? -1 : 0;
//return (g) ? (int)((Block - 1) * Nrec + Last) : 1;
  } // end of Cardinality

/***********************************************************************/
/*  Allocate the line buffer. For mode Delete a bigger buffer has to   */
/*  be allocated because is it also used to move lines into the file.  */
/***********************************************************************/
bool ZBKFAM::AllocateBuffer(PGLOBAL g)
  {
  Buflen = Nrec * (Lrecl + 2);
  CurLine = To_Buf = (char*)PlugSubAlloc(g, NULL, Buflen);

  if (Tdbp->GetMode() == MODE_INSERT) {
    // Set values so Block and Last can be recalculated
    if (Last == Nrec) {
      CurBlk = Block;
      Rbuf = Nrec;                   // To be used by WriteDB
    } else {
      // The last block must be completed
      CurBlk = Block - 1;
      Rbuf = Nrec - Last;            // To be used by WriteDB
    } // endif Last

    } // endif Insert

  return false;
  } // end of AllocateBuffer

/***********************************************************************/
/*  GetRowID: return the RowID of last read record.                    */
/***********************************************************************/
int ZBKFAM::GetRowID(void)
  {
  return CurNum + Nrec * CurBlk + 1;
  } // end of GetRowID

/***********************************************************************/
/*  GetPos: return the position of last read record.                   */
/***********************************************************************/
int ZBKFAM::GetPos(void)
  {
  return CurNum + Nrec * CurBlk;            // Computed file index
  } // end of GetPos

/***********************************************************************/
/*  Record file position in case of UPDATE or DELETE.                  */
/*  Not used yet for fixed tables.                                     */
/***********************************************************************/
bool ZBKFAM::RecordPos(PGLOBAL g)
  {
//strcpy(g->Message, "RecordPos not implemented for zip blocked tables");
//return true;
  return RC_OK;
  } // end of RecordPos

/***********************************************************************/
/*  Skip one record in file.                                           */
/***********************************************************************/
int ZBKFAM::SkipRecord(PGLOBAL g, bool header)
  {
//strcpy(g->Message, "SkipRecord not implemented for zip blocked tables");
//return RC_FX;
  return RC_OK;
  } // end of SkipRecord

/***********************************************************************/
/*  ReadBuffer: Read one line from a compressed text file.             */
/***********************************************************************/
int ZBKFAM::ReadBuffer(PGLOBAL g)
  {
#if defined(BLK_INDX)
  int     n, skip, rc = RC_OK;

  /*********************************************************************/
  /*  Sequential reading when Placed is not true.                      */
  /*********************************************************************/
  if (++CurNum < Rbuf) {
    CurLine = NxtLine;

    // Get the position of the next line in the buffer
    while (*NxtLine++ != '\n') ;

    // Set caller line buffer
    n = NxtLine - CurLine - Ending;
    memcpy(Tdbp->GetLine(), CurLine, n);
    Tdbp->GetLine()[n] = '\0';
    return RC_OK;
  } else if (Rbuf < Nrec && CurBlk != -1)
    return RC_EF;

  /*********************************************************************/
  /*  New block.                                                       */
  /*********************************************************************/
  CurNum = 0;
  skip = 0;

 next:
  if (++CurBlk >= Block)
    return RC_EF;

  /*********************************************************************/
  /*  Before using the new block, check whether block optimization     */
  /*  can be done, as well as for join as for local filtering.         */
  /*********************************************************************/
  switch (Tdbp->TestBlock(g)) {
    case RC_EF:
      return RC_EF;
    case RC_NF:
      skip++;
      goto next;
    } // endswitch rc

  if (skip)
    // Skip blocks rejected by block optimization
    for (int i = CurBlk - skip; i < CurBlk; i++) {
      BlkLen = BlkPos[i + 1] - BlkPos[i];

      if (gzseek(Zfile, (z_off_t)BlkLen, SEEK_CUR) < 0)
        return Zerror(g);

      } // endfor i

  BlkLen = BlkPos[CurBlk + 1] - BlkPos[CurBlk];

  if (!(n = gzread(Zfile, To_Buf, BlkLen))) {
    rc = RC_EF;
  } else if (n > 0) {
    // Get the position of the current line
    CurLine = To_Buf;

    // Now get the position of the next line
    for (NxtLine = CurLine; *NxtLine++ != '\n';) ;

    // Set caller line buffer
    n = NxtLine - CurLine - Ending;
    memcpy(Tdbp->GetLine(), CurLine, n);
    Tdbp->GetLine()[n] = '\0';
    Rbuf = (CurBlk == Block - 1) ? Last : Nrec;
    IsRead = true;
    rc = RC_OK;
    num_read++;
  } else
    rc = Zerror(g);

  return rc;
#else   // !BLK_POS
  strcpy(g->Message, "This AM cannot be used in this version");
  return RC_FX;
#endif  // !BLK_POS
  } // end of ReadBuffer

/***********************************************************************/
/*  WriteDB: Data Base write routine for ZDOS access method.           */
/*  Update is not possible without using a temporary file (NIY).       */
/***********************************************************************/
int ZBKFAM::WriteBuffer(PGLOBAL g)
  {
  /*********************************************************************/
  /*  Prepare the write buffer.                                        */
  /*********************************************************************/
  if (!Closing)
    strcat(strcpy(CurLine, Tdbp->GetLine()), CrLf);

  /*********************************************************************/
  /*  In Insert mode, blocs are added sequentialy to the file end.     */
  /*  Note: Update mode is not handled for zip files.                  */
  /*********************************************************************/
  if (++CurNum == Rbuf) {
    /*******************************************************************/
    /*  New block, start the writing process.                          */
    /*******************************************************************/
    BlkLen = CurLine + strlen(CurLine) - To_Buf;

    if (gzwrite(Zfile, To_Buf, BlkLen) != BlkLen ||
        gzflush(Zfile, Z_FULL_FLUSH)) {
      Closing = true;
      return Zerror(g);
      } // endif gzwrite

    Rbuf = Nrec;
    CurBlk++;
    CurNum = 0;
    CurLine = To_Buf;
  } else
    CurLine += strlen(CurLine);

  return RC_OK;
  } // end of WriteBuffer

/***********************************************************************/
/*  Data Base delete line routine for ZBK access method.               */
/*  Implemented only for total deletion of the table, which is done    */
/*  by opening the file in mode "wb".                                  */
/***********************************************************************/
int ZBKFAM::DeleteRecords(PGLOBAL g, int irc)
  {
  if (irc == RC_EF) {
    LPCSTR  name = Tdbp->GetName();
    PDOSDEF defp = (PDOSDEF)Tdbp->GetDef();
    PCATLG  cat = PlgGetCatalog(g);

    defp->SetBlock(0);
    defp->SetLast(Nrec);

    if (!cat->SetIntCatInfo("Blocks", 0) ||
        !cat->SetIntCatInfo("Last", 0)) {
      sprintf(g->Message, MSG(UPDATE_ERROR), "Header");
      return RC_FX;
    } else
      return RC_OK;

  } else
    return irc;

  } // end of DeleteRecords

/***********************************************************************/
/*  Data Base close routine for ZBK access method.                     */
/***********************************************************************/
void ZBKFAM::CloseTableFile(PGLOBAL g)
  {
  int rc = RC_OK;

  if (Tdbp->GetMode() == MODE_INSERT) {
    PCATLG  cat = PlgGetCatalog(g);
    LPCSTR  name = Tdbp->GetName();
    PDOSDEF defp = (PDOSDEF)Tdbp->GetDef();

    if (CurNum && !Closing) {
      // Some more inserted lines remain to be written
      Last = (Nrec - Rbuf) + CurNum;
      Block = CurBlk + 1;
      Rbuf = CurNum--;
      Closing = true;
      rc = WriteBuffer(g);
    } else if (Rbuf == Nrec) {
      Last = Nrec;
      Block = CurBlk;
    } // endif CurNum

    if (rc != RC_FX) {
      defp->SetBlock(Block);
      defp->SetLast(Last);
      cat->SetIntCatInfo("Blocks", Block);
      cat->SetIntCatInfo("Last", Last);
      } // endif

    gzclose(Zfile);
  } else if (Tdbp->GetMode() == MODE_DELETE) {
    rc = DeleteRecords(g, RC_EF);
    gzclose(Zfile);
  } else
    rc = gzclose(Zfile);

#ifdef DEBTRACE
 htrc("ZIP CloseDB: closing %s rc=%d\n", To_File, rc);
#endif

  Zfile = NULL;            // So we can know whether table is open
//To_Fb->Count = 0;        // Avoid double closing by PlugCloseAll
  } // end of CloseTableFile

/***********************************************************************/
/*  Rewind routine for ZBK access method.                              */
/***********************************************************************/
void ZBKFAM::Rewind(void)
  {
  gzrewind(Zfile);
  CurBlk = -1;
  CurNum = Rbuf;
  } // end of Rewind

/* ------------------------------------------------------------------- */

/***********************************************************************/
/*  Constructors.                                                      */
/***********************************************************************/
ZIXFAM::ZIXFAM(PDOSDEF tdp) : ZBKFAM(tdp)
  {
//Block = tdp->GetBlock();
//Last = tdp->GetLast();
  Nrec = (tdp->GetElemt()) ? tdp->GetElemt() : DOS_BUFF_LEN;
  Blksize = Nrec * Lrecl;
  } // end of ZIXFAM standard constructor

/***********************************************************************/
/*  ZIX Cardinality: returns table cardinality in number of rows.      */
/*  This function can be called with a null argument to test the       */
/*  availability of Cardinality implementation (1 yes, 0 no).          */
/***********************************************************************/
int ZIXFAM::Cardinality(PGLOBAL g)
  {
  if (Last)
    return (g) ? (int)((Block - 1) * Nrec + Last) : 1;
  else  // Last and Block not defined, cannot do it yet
    return 0;

  } // end of Cardinality

/***********************************************************************/
/*  Allocate the line buffer. For mode Delete a bigger buffer has to   */
/*  be allocated because is it also used to move lines into the file.  */
/***********************************************************************/
bool ZIXFAM::AllocateBuffer(PGLOBAL g)
  {
  Buflen = Blksize;
  To_Buf = (char*)PlugSubAlloc(g, NULL, Buflen);

  if (Tdbp->GetMode() == MODE_INSERT) {
    /*******************************************************************/
    /*  For Insert the buffer must be prepared.                        */
    /*******************************************************************/
    memset(To_Buf, ' ', Buflen);

    if (Tdbp->GetFtype() < 2)
      // if not binary, the file is physically a text file
      for (int len = Lrecl; len <= Buflen; len += Lrecl) {
#if defined(WIN32)
        To_Buf[len - 2] = '\r';
#endif   // WIN32
        To_Buf[len - 1] = '\n';
        } // endfor len

    // Set values so Block and Last can be recalculated
    if (Last == Nrec) {
      CurBlk = Block;
      Rbuf = Nrec;                   // To be used by WriteDB
    } else {
      // The last block must be completed
      CurBlk = Block - 1;
      Rbuf = Nrec - Last;            // To be used by WriteDB
    } // endif Last

    } // endif Insert

  return false;
  } // end of AllocateBuffer

/***********************************************************************/
/*  ReadBuffer: Read one line from a compressed text file.             */
/***********************************************************************/
int ZIXFAM::ReadBuffer(PGLOBAL g)
  {
  int n, rc = RC_OK;

  /*********************************************************************/
  /*  Sequential reading when Placed is not true.                      */
  /*********************************************************************/
  if (++CurNum < Rbuf) {
    Tdbp->IncLine(Lrecl);                // Used by DOSCOL functions
    return RC_OK;
  } else if (Rbuf < Nrec && CurBlk != -1)
    return RC_EF;

  /*********************************************************************/
  /*  New block.                                                       */
  /*********************************************************************/
  CurNum = 0;
  Tdbp->SetLine(To_Buf);

#if defined(BLK_INDX)
  int skip = 0;

 next:
  if (++CurBlk >= Block)
    return RC_EF;

  /*********************************************************************/
  /*  Before using the new block, check whether block optimization     */
  /*  can be done, as well as for join as for local filtering.         */
  /*********************************************************************/
  switch (Tdbp->TestBlock(g)) {
    case RC_EF:
      return RC_EF;
    case RC_NF:
      skip++;
      goto next;
    } // endswitch rc

  if (skip)
    // Skip blocks rejected by block optimization
    for (int i = 0; i < skip; i++) {
      if (gzseek(Zfile, (z_off_t)Buflen, SEEK_CUR) < 0)
        return Zerror(g);

      } // endfor i
#endif   // BLK_INDX

  if (!(n = gzread(Zfile, To_Buf, Buflen))) {
    rc = RC_EF;
  } else if (n > 0) {
    Rbuf = n / Lrecl;
    IsRead = true;
    rc = RC_OK;
    num_read++;
  } else
    rc = Zerror(g);

  return rc;
  } // end of ReadBuffer

/***********************************************************************/
/*  WriteDB: Data Base write routine for ZDOS access method.           */
/*  Update is not possible without using a temporary file (NIY).       */
/***********************************************************************/
int ZIXFAM::WriteBuffer(PGLOBAL g)
  {
  /*********************************************************************/
  /*  In Insert mode, blocs are added sequentialy to the file end.     */
  /*  Note: Update mode is not handled for zip files.                  */
  /*********************************************************************/
  if (++CurNum == Rbuf) {
    /*******************************************************************/
    /*  New block, start the writing process.                          */
    /*******************************************************************/
    BlkLen = Rbuf * Lrecl;

    if (gzwrite(Zfile, To_Buf, BlkLen) != BlkLen ||
        gzflush(Zfile, Z_FULL_FLUSH)) {
      Closing = true;
      return Zerror(g);
      } // endif gzwrite

    Rbuf = Nrec;
    CurBlk++;
    CurNum = 0;
    Tdbp->SetLine(To_Buf);
  } else
    Tdbp->IncLine(Lrecl);            // Used by FIXCOL functions

  return RC_OK;
  } // end of WriteBuffer

#if defined(BLK_INDX)
/* --------------------------- Class ZLBFAM -------------------------- */

/***********************************************************************/
/*  Constructors.                                                      */
/***********************************************************************/
ZLBFAM::ZLBFAM(PDOSDEF tdp) : BLKFAM(tdp)
  {
  Zstream = NULL;
  Zbuffer = NULL;
  Zlenp = NULL;
  Optimized = tdp->IsOptimized();
  } // end of ZLBFAM standard constructor

ZLBFAM::ZLBFAM(PZLBFAM txfp) : BLKFAM(txfp)
  {
  Zstream = txfp->Zstream;
  Zbuffer = txfp->Zbuffer;
  Zlenp = txfp->Zlenp;
  Optimized = txfp->Optimized;
  } // end of ZLBFAM (dummy?) copy constructor

/***********************************************************************/
/*  ZLB GetFileLength: returns an estimate of what would be the        */
/*  uncompressed file size in number of bytes.                         */
/***********************************************************************/
int ZLBFAM::GetFileLength(PGLOBAL g)
  {
  int len = (Optimized) ? BlkPos[Block] : BLKFAM::GetFileLength(g);

  if (len > 0)
    // Estimate size reduction to a max of 5
    len *= 5;

  return len;
  } // end of GetFileLength

/***********************************************************************/
/*  Allocate the line buffer. For mode Delete a bigger buffer has to   */
/*  be allocated because is it also used to move lines into the file.  */
/***********************************************************************/
bool ZLBFAM::AllocateBuffer(PGLOBAL g)
  {
  char *msg;
  int   n, zrc;

#if 0
  if (!Optimized && Tdbp->NeedIndexing(g)) {
    strcpy(g->Message, MSG(NOP_ZLIB_INDEX));
    return TRUE;
    } // endif indexing
#endif // 0

#if defined(NOLIB)
  if (!zlib && LoadZlib()) {
    sprintf(g->Message, MSG(DLL_LOAD_ERROR), GetLastError(), "zlib.dll");
    return TRUE;
    } // endif zlib
#endif

  BLKFAM::AllocateBuffer(g);
//Buflen = Nrec * (Lrecl + 2);
//Rbuf = Nrec;

  // Allocate the compressed buffer
  n = Buflen + 16;             // ?????????????????????????????????
  Zlenp = (int*)PlugSubAlloc(g, NULL, n);
  Zbuffer = (Byte*)(Zlenp + 1);

  // Allocate and initialize the Z stream
  Zstream = (z_streamp)PlugSubAlloc(g, NULL, sizeof(z_stream));
  Zstream->zalloc = (alloc_func)0;
  Zstream->zfree = (free_func)0;
  Zstream->opaque = (voidpf)0;
  Zstream->next_in = NULL;
  Zstream->avail_in = 0;

  if (Tdbp->GetMode() == MODE_READ) {
    msg = "inflateInit";
    zrc = inflateInit(Zstream);
  } else {
    msg = "deflateInit";
    zrc = deflateInit(Zstream, Z_DEFAULT_COMPRESSION);
  } // endif Mode

  if (zrc != Z_OK) {
    if (Zstream->msg)
      sprintf(g->Message, "%s error: %s", msg, Zstream->msg);
    else
      sprintf(g->Message, "%s error: %d", msg, zrc);

    return TRUE;
    } // endif zrc

  if (Tdbp->GetMode() == MODE_INSERT) {
    // Write the file header block
    if (Last == Nrec) {
      CurBlk = Block;
      CurNum = 0;

      if (!GetFileLength(g)) {
        // Write the zlib header as an extra block
        strcpy(To_Buf, "PlugDB");
        BlkLen = strlen("PlugDB") + 1;

        if (WriteCompressedBuffer(g))
          return TRUE;

        } // endif void file

    } else {
      // In mode insert, if Last != Nrec, last block must be updated
      CurBlk = Block - 1;
      CurNum = Last;

      strcpy(g->Message, MSG(NO_PAR_BLK_INS));
      return TRUE;
    } // endif Last

  } else { // MODE_READ
    // First thing to do is to read the header block
    void *rdbuf;

    if (Optimized) {
      BlkLen = BlkPos[0];
      rdbuf = Zlenp;
    } else {
      // Get the stored length from the file itself
      if (fread(Zlenp, sizeof(int), 1, Stream) != 1)
        return FALSE;             // Empty file

      BlkLen = *Zlenp;
      rdbuf = Zbuffer;
    } // endif Optimized

    switch (ReadCompressedBuffer(g, rdbuf)) {
      case RC_EF:
        return FALSE;
      case RC_FX:
#if defined(UNIX)
        sprintf(g->Message, MSG(READ_ERROR), To_File, strerror(errno));
#else
        sprintf(g->Message, MSG(READ_ERROR), To_File, _strerror(NULL));
#endif
      case RC_NF:
        return TRUE;
      } // endswitch

    // Some old tables can have PlugDB in their header
    if (strcmp(To_Buf, "PlugDB")) {
      sprintf(g->Message, MSG(BAD_HEADER), Tdbp->GetFile(g));
      return TRUE;
      } // endif strcmp

  } // endif Mode

  return FALSE;
  } // end of AllocateBuffer

/***********************************************************************/
/*  GetPos: return the position of last read record.                   */
/***********************************************************************/
int ZLBFAM::GetPos(void)
  {
  return (Optimized) ? (CurNum + Nrec * CurBlk) : Fpos;
  } // end of GetPos

/***********************************************************************/
/*  GetNextPos: should not be called for this class.                   */
/***********************************************************************/
int ZLBFAM::GetNextPos(void)
  {
  if (Optimized) {
    assert(FALSE);
    return 0;
  } else
    return ftell(Stream);

  } // end of GetNextPos

/***********************************************************************/
/*  ReadBuffer: Read one line for a text file.                         */
/***********************************************************************/
int ZLBFAM::ReadBuffer(PGLOBAL g)
  {
  int   n;
  void *rdbuf;

  /*********************************************************************/
  /*  Sequential reading when Placed is not true.                      */
  /*********************************************************************/
  if (Placed) {
    Placed = FALSE;
  } else if (++CurNum < Rbuf) {
    CurLine = NxtLine;

    // Get the position of the next line in the buffer
    if (Tdbp->GetFtype() == RECFM_VAR)
      while (*NxtLine++ != '\n') ;
    else
      NxtLine += Lrecl;

    // Set caller line buffer
    n = NxtLine - CurLine - ((Tdbp->GetFtype() == RECFM_BIN) ? 0 : Ending);
    memcpy(Tdbp->GetLine(), CurLine, n);
    Tdbp->GetLine()[n] = '\0';
    return RC_OK;
  } else if (Rbuf < Nrec && CurBlk != -1) {
    CurNum--;         // To have a correct Last value when optimizing
    return RC_EF;
  } else {
    /*******************************************************************/
    /*  New block.                                                     */
    /*******************************************************************/
    CurNum = 0;

   next:
    if (++CurBlk >= Block)
      return RC_EF;

    /*******************************************************************/
    /*  Before reading a new block, check whether block optimization   */
    /*  can be done, as well as for join as for local filtering.       */
    /*******************************************************************/
    if (Optimized) switch (Tdbp->TestBlock(g)) {
      case RC_EF:
        return RC_EF;
      case RC_NF:
        goto next;
      } // endswitch rc

  } // endif's

  if (OldBlk == CurBlk)
    goto ok;         // Block is already there

  if (Optimized) {
    // Store the position of next block
    Fpos = BlkPos[CurBlk];

    // fseek is required only in non sequential reading
    if (CurBlk != OldBlk + 1)
      if (fseek(Stream, Fpos, SEEK_SET)) {
        sprintf(g->Message, MSG(FSETPOS_ERROR), Fpos);
        return RC_FX;
        } // endif fseek

    // Calculate the length of block to read
    BlkLen = BlkPos[CurBlk + 1] - Fpos;
    rdbuf = Zlenp;
  } else {                     // !Optimized
    if (CurBlk != OldBlk + 1) {
      strcpy(g->Message, MSG(INV_RAND_ACC));
      return RC_FX;
    } else
      Fpos = ftell(Stream);    // Used when optimizing

    // Get the stored length from the file itself
    if (fread(Zlenp, sizeof(int), 1, Stream) != 1) {
      if (feof(Stream))
        return RC_EF;

      goto err;
      } // endif fread

    BlkLen = *Zlenp;
    rdbuf = Zbuffer;
  } // endif Optimized

  // Read the next block
  switch (ReadCompressedBuffer(g, rdbuf)) {
    case RC_FX: goto err;
    case RC_NF: return RC_FX;
    case RC_EF: return RC_EF;
    default: Rbuf = (CurBlk == Block - 1) ? Last : Nrec;
    } // endswitch ReadCompressedBuffer

 ok:
  if (Tdbp->GetFtype() == RECFM_VAR) {
    int i;

    // Get the position of the current line
    for (i = 0, CurLine = To_Buf; i < CurNum; i++)
      while (*CurLine++ != '\n') ;      // What about Unix ???

    // Now get the position of the next line
    for (NxtLine = CurLine; *NxtLine++ != '\n';) ;

    // Set caller line buffer
    n = NxtLine - CurLine - Ending;
  } else {
    CurLine = To_Buf + CurNum * Lrecl;
    NxtLine = CurLine + Lrecl;
    n = Lrecl - ((Tdbp->GetFtype() == RECFM_BIN) ? 0 : Ending);
  } // endif Ftype

  memcpy(Tdbp->GetLine(), CurLine, n);
  Tdbp->GetLine()[n] = '\0';

  OldBlk = CurBlk;         // Last block actually read
  IsRead = TRUE;           // Is read indeed
  return RC_OK;

 err:
#if defined(UNIX)
  sprintf(g->Message, MSG(READ_ERROR), To_File, strerror(errno));
#else
  sprintf(g->Message, MSG(READ_ERROR), To_File, _strerror(NULL));
#endif
  return RC_FX;
  } // end of ReadBuffer

/***********************************************************************/
/*  Read and decompress a block from the stream.                       */
/***********************************************************************/
int ZLBFAM::ReadCompressedBuffer(PGLOBAL g, void *rdbuf)
  {
  if (fread(rdbuf, 1, (size_t)BlkLen, Stream) == (unsigned)BlkLen) {
    int zrc;

    num_read++;

    if (Optimized && BlkLen != signed(*Zlenp + sizeof(int))) {
      sprintf(g->Message, MSG(BAD_BLK_SIZE), CurBlk + 1);
      return RC_NF;
      } // endif BlkLen

    // HERE WE MUST INFLATE THE BLOCK
    Zstream->next_in = Zbuffer;
    Zstream->avail_in = (uInt)(*Zlenp);
    Zstream->next_out = (Byte*)To_Buf;
    Zstream->avail_out = Buflen;
    zrc = inflate(Zstream, Z_SYNC_FLUSH);

    if (zrc != Z_OK) {
      if (Zstream->msg)
        sprintf(g->Message, MSG(FUNC_ERR_S), "inflate", Zstream->msg);
      else
        sprintf(g->Message, MSG(FUNCTION_ERROR), "inflate", (int)zrc);

      return RC_NF;
      } // endif zrc

  } else if (feof(Stream)) {
    return RC_EF;
  } else
    return RC_FX;

  return RC_OK;
  } // end of ReadCompressedBuffer

/***********************************************************************/
/*  WriteBuffer: File write routine for DOS access method.             */
/*  Update is directly written back into the file,                     */
/*         with this (fast) method, record size cannot change.         */
/***********************************************************************/
int ZLBFAM::WriteBuffer(PGLOBAL g)
  {
  assert (Tdbp->GetMode() == MODE_INSERT);

  /*********************************************************************/
  /*  Prepare the write buffer.                                        */
  /*********************************************************************/
  if (!Closing) {
    if (Tdbp->GetFtype() == RECFM_BIN)
      memcpy(CurLine, Tdbp->GetLine(), Lrecl);
    else
      strcat(strcpy(CurLine, Tdbp->GetLine()), CrLf);

#if defined(_DEBUG)
    if (Tdbp->GetFtype() == RECFM_FIX &&
      (signed)strlen(CurLine) != Lrecl + (signed)strlen(CrLf)) {
      strcpy(g->Message, MSG(BAD_LINE_LEN));
      Closing = TRUE;
      return RC_FX;
      } // endif Lrecl
#endif   // _DEBUG
    } // endif Closing

  /*********************************************************************/
  /*  In Insert mode, blocs are added sequentialy to the file end.     */
  /*********************************************************************/
  if (++CurNum != Rbuf) {
    if (Tdbp->GetFtype() == RECFM_VAR)
      CurLine += strlen(CurLine);
    else
      CurLine += Lrecl;

    return RC_OK;                    // We write only full blocks
    } // endif CurNum

  // HERE WE MUST DEFLATE THE BLOCK
  if (Tdbp->GetFtype() == RECFM_VAR)
    NxtLine = CurLine + strlen(CurLine);
  else
    NxtLine = CurLine + Lrecl;

  BlkLen = NxtLine - To_Buf;

  if (WriteCompressedBuffer(g)) {
    Closing = TRUE;      // To tell CloseDB about a Write error
    return RC_FX;
    } // endif WriteCompressedBuffer

  CurBlk++;
  CurNum = 0;
  CurLine = To_Buf;
  return RC_OK;
  } // end of WriteBuffer

/***********************************************************************/
/*  Compress the buffer and write the deflated output to stream.       */
/***********************************************************************/
bool ZLBFAM::WriteCompressedBuffer(PGLOBAL g)
  {
  int zrc;

  Zstream->next_in = (Byte*)To_Buf;
  Zstream->avail_in = (uInt)BlkLen;
  Zstream->next_out = Zbuffer;
  Zstream->avail_out = Buflen + 16;
  Zstream->total_out = 0;
  zrc = deflate(Zstream, Z_FULL_FLUSH);

  if (zrc != Z_OK) {
    if (Zstream->msg)
      sprintf(g->Message, MSG(FUNC_ERR_S), "deflate", Zstream->msg);
    else
      sprintf(g->Message, MSG(FUNCTION_ERROR), "deflate", (int)zrc);

    return TRUE;
  } else
    *Zlenp = Zstream->total_out;

  //  Now start the writing process.
  BlkLen = *Zlenp + sizeof(int);

  if (fwrite(Zlenp, 1, BlkLen, Stream) != (size_t)BlkLen) {
    sprintf(g->Message, MSG(FWRITE_ERROR), strerror(errno));
    return TRUE;
    } // endif size

  return FALSE;
  } // end of WriteCompressedBuffer

/***********************************************************************/
/*  Table file close routine for DOS access method.                    */
/***********************************************************************/
void ZLBFAM::CloseTableFile(PGLOBAL g)
  {
  int rc = RC_OK;

  if (Tdbp->GetMode() == MODE_INSERT) {
    PCATLG  cat = PlgGetCatalog(g);
    LPCSTR  name = Tdbp->GetName();
    PDOSDEF defp = (PDOSDEF)Tdbp->GetDef();

    // Closing is True if last Write was in error
    if (CurNum && !Closing) {
      // Some more inserted lines remain to be written
      Last = (Nrec - Rbuf) + CurNum;
      Block = CurBlk + 1;
      Rbuf = CurNum--;
      Closing = TRUE;
      rc = WriteBuffer(g);
    } else if (Rbuf == Nrec) {
      Last = Nrec;
      Block = CurBlk;
    } // endif CurNum

    if (rc != RC_FX) {
      defp->SetBlock(Block);
      defp->SetLast(Last);
      cat->SetIntCatInfo("Blocks", Block);
      cat->SetIntCatInfo("Last", Last);
      } // endif

    fclose(Stream);
  } else
    rc = fclose(Stream);

#ifdef DEBTRACE
 htrc("ZLB CloseTableFile: closing %s mode=%d rc=%d\n",
  To_File, Tdbp->GetMode(), rc);
#endif

  Stream = NULL;           // So we can know whether table is open
  To_Fb->Count = 0;        // Avoid double closing by PlugCloseAll

  if (Tdbp->GetMode() == MODE_READ)
    rc = inflateEnd(Zstream);
  else
    rc = deflateEnd(Zstream);

  } // end of CloseTableFile

/***********************************************************************/
/*  Rewind routine for ZLIB access method.                             */
/***********************************************************************/
void ZLBFAM::Rewind(void)
  {
  // We must be positioned after the header block
  if (CurBlk >= 0) {   // Nothing to do if no block read yet
    if (!Optimized) {  // If optimized, fseek will be done in ReadBuffer
      rewind(Stream);
      fread(Zlenp, sizeof(int), 1, Stream);
      fseek(Stream, *Zlenp + sizeof(int), SEEK_SET);
      OldBlk = -1;
      } // endif Optimized

    CurBlk = -1;
    CurNum = Rbuf;
    } // endif CurBlk

//OldBlk = -1;
//Rbuf = 0;        commented out in case we reuse last read block
  } // end of Rewind
#endif   // BLK_INDX
/* ------------------------ End of ZipFam ---------------------------- */
