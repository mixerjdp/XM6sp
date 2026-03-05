#if !defined(host_fs_h)
#define host_fs_h

#include "os.h"
#include "xm6.h"

class Memory;

#define FS_INVALIDFUNC           0xffffffff
#define FS_FILENOTFND            0xfffffffe
#define FS_DIRNOTFND             0xfffffffd
#define FS_OVEROPENED            0xfffffffc
#define FS_CANTACCESS            0xfffffffb
#define FS_NOTOPENED             0xfffffffa
#define FS_INVALIDMEM            0xfffffff9
#define FS_OUTOFMEM              0xfffffff8
#define FS_INVALIDPTR            0xfffffff7
#define FS_INVALIDENV            0xfffffff6
#define FS_ILLEGALFMT            0xfffffff5
#define FS_ILLEGALMOD            0xfffffff4
#define FS_INVALIDPATH           0xfffffff3
#define FS_INVALIDPRM            0xfffffff2
#define FS_INVALIDDRV            0xfffffff1
#define FS_DELCURDIR             0xfffffff0
#define FS_NOTIOCTRL             0xffffffef
#define FS_LASTFILE              0xffffffee
#define FS_CANTWRITE             0xffffffed
#define FS_DIRALREADY            0xffffffec
#define FS_CANTDELETE            0xffffffeb
#define FS_CANTRENAME            0xffffffea
#define FS_DISKFULL              0xffffffe9
#define FS_DIRFULL               0xffffffe8
#define FS_CANTSEEK              0xffffffe7
#define FS_SUPERVISOR            0xffffffe6
#define FS_THREADNAME            0xffffffe5
#define FS_BUFWRITE              0xffffffe4
#define FS_BACKGROUND            0xffffffe3
#define FS_OUTOFLOCK             0xffffffe0
#define FS_LOCKED                0xffffffdf
#define FS_DRIVEOPENED           0xffffffde
#define FS_LINKOVER              0xffffffdd
#define FS_FILEEXIST             0xffffffb0

#define FS_FATAL_INVALIDUNIT     0xFFFFFFA0
#define FS_FATAL_INVALIDCOMMAND  0xFFFFFFA1
#define FS_FATAL_WRITEPROTECT    0xFFFFFFA2
#define FS_FATAL_MEDIAOFFLINE    0xFFFFFFA3

#define HUMAN68K_MAX_PATH 96
class Human68k {
public:
    enum {
        AT_READONLY  = 0x01,
        AT_HIDDEN    = 0x02,
        AT_SYSTEM    = 0x04,
        AT_VOLUME    = 0x08,
        AT_DIRECTORY = 0x10,
        AT_ARCHIVE   = 0x20,
        AT_ALL       = 0xFF,
    };

    enum {
        OP_READ      = 0,
        OP_WRITE     = 1,
        OP_READWRITE = 2,
    };

    enum {
        SK_BEGIN   = 0,
        SK_CURRENT = 1,
        SK_END     = 2,
    };

    typedef struct {
        BYTE wildcard;
        BYTE drive;
        BYTE path[65];
        BYTE name[8];
        BYTE ext[3];
        BYTE add[10];

        void FASTCALL GetCopyPath(BYTE* szPath) const;
        void FASTCALL GetCopyFilename(BYTE* szFilename) const;
    } namests_t;

    typedef struct {
        BYTE fatr;
        BYTE drive;
        DWORD sector;
        WORD offset;
        BYTE attr;
        WORD time;
        WORD date;
        DWORD size;
        BYTE full[23];
    } files_t;

    typedef struct {
        DWORD fileptr;
        WORD mode;
        BYTE attr;
        WORD time;
        WORD date;
        DWORD size;
    } fcb_t;

    typedef struct {
        WORD free;
        WORD clusters;
        WORD sectors;
        WORD bytes;
    } capacity_t;

    typedef struct {
        BYTE status;
    } ctrldrive_t;

    typedef struct {
        WORD sector_size;
        BYTE cluster_size;
        BYTE shift;
        WORD fat_sector;
        BYTE fat_max;
        BYTE fat_size;
        WORD file_max;
        WORD data_sector;
        WORD cluster_max;
        WORD root_sector;
        BYTE media;
    } dpb_t;

    typedef struct {
        BYTE name[8];
        BYTE ext[3];
        BYTE attr;
        BYTE add[10];
        WORD time;
        WORD date;
        WORD cluster;
        DWORD size;
    } dirent_t;

    typedef union {
        BYTE buffer[8];
        DWORD param;
        WORD media;
    } ioctrl_t;
};

class WindrvContext
{
public:
    virtual ~WindrvContext() {}
    virtual DWORD FASTCALL GetUnit() const = 0;
    virtual Memory* FASTCALL GetMemory() const = 0;
    virtual void FASTCALL LockXM() = 0;
    virtual void FASTCALL UnlockXM() = 0;
    virtual void FASTCALL Ready() = 0;
};

class FileSys
{
public:
    virtual DWORD FASTCALL Init(WindrvContext* ps, DWORD nDriveMax, const BYTE* pOption) = 0;
    virtual void FASTCALL Reset() = 0;

    virtual int FASTCALL CheckDir(WindrvContext* ps, const Human68k::namests_t* pNamests) = 0;
    virtual int FASTCALL MakeDir(WindrvContext* ps, const Human68k::namests_t* pNamests) = 0;
    virtual int FASTCALL RemoveDir(WindrvContext* ps, const Human68k::namests_t* pNamests) = 0;
    virtual int FASTCALL Rename(WindrvContext* ps, const Human68k::namests_t* pNamests, const Human68k::namests_t* pNamestsNew) = 0;
    virtual int FASTCALL Delete(WindrvContext* ps, const Human68k::namests_t* pNamests) = 0;
    virtual int FASTCALL Attribute(WindrvContext* ps, const Human68k::namests_t* pNamests, DWORD nHumanAttribute) = 0;
    virtual int FASTCALL Files(WindrvContext* ps, const Human68k::namests_t* pNamests, DWORD nKey, Human68k::files_t* info) = 0;
    virtual int FASTCALL NFiles(WindrvContext* ps, DWORD nKey, Human68k::files_t* info) = 0;
    virtual int FASTCALL Create(WindrvContext* ps, const Human68k::namests_t* pNamests, DWORD nKey, Human68k::fcb_t* pFcb, DWORD nAttribute, BOOL bForce) = 0;
    virtual int FASTCALL Open(WindrvContext* ps, const Human68k::namests_t* pNamests, DWORD nKey, Human68k::fcb_t* pFcb) = 0;
    virtual int FASTCALL Close(WindrvContext* ps, DWORD nKey, Human68k::fcb_t* pFcb) = 0;
    virtual int FASTCALL Read(WindrvContext* ps, DWORD nKey, Human68k::fcb_t* pFcb, DWORD nAddress, DWORD nSize) = 0;
    virtual int FASTCALL Write(WindrvContext* ps, DWORD nKey, Human68k::fcb_t* pFcb, DWORD nAddress, DWORD nSize) = 0;
    virtual int FASTCALL Seek(WindrvContext* ps, DWORD nKey, Human68k::fcb_t* pFcb, DWORD nMode, int nOffset) = 0;
    virtual DWORD FASTCALL TimeStamp(WindrvContext* ps, DWORD nKey, Human68k::fcb_t* pFcb, WORD nFatDate, WORD nFatTime) = 0;
    virtual int FASTCALL GetCapacity(WindrvContext* ps, Human68k::capacity_t* cap) = 0;
    virtual int FASTCALL CtrlDrive(WindrvContext* ps, Human68k::ctrldrive_t* pCtrlDrive) = 0;
    virtual int FASTCALL GetDPB(WindrvContext* ps, Human68k::dpb_t* pDpb) = 0;
    virtual int FASTCALL DiskRead(WindrvContext* ps, DWORD nAddress, DWORD nSector, DWORD nSize) = 0;
    virtual int FASTCALL DiskWrite(WindrvContext* ps, DWORD nAddress, DWORD nSector, DWORD nSize) = 0;
    virtual int FASTCALL IoControl(WindrvContext* ps, Human68k::ioctrl_t* pIoctrl, DWORD nFunction) = 0;
    virtual int FASTCALL Flush(WindrvContext* ps) = 0;
    virtual int FASTCALL CheckMedia(WindrvContext* ps) = 0;
    virtual int FASTCALL Lock(WindrvContext* ps) = 0;
};

#endif // host_fs_h
