/*-------------------------------------------------------------------------
 *
 * xact.h
 *	  postgres transaction system definitions
 *
 *
 * Portions Copyright (c) 1996-2021, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/access/xact.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef XACT_H
#define XACT_H

#include "access/transam.h"
#include "access/xlogreader.h"
#include "datatype/timestamp.h"
#include "lib/stringinfo.h"
#include "nodes/pg_list.h"
#include "storage/relfilenode.h"
#include "storage/sinval.h"
#include "storage/dbdirnode.h"
#include "utils/datetime.h"

#include "cdb/cdbpublic.h"
#include "cdb/cdbtm.h"

/*
 * Maximum size of Global Transaction ID (including '\0').
 *
 * Note that the max value of GIDSIZE must fit in the uint16 gidlen,
 * specified in TwoPhaseFileHeader.
 */
#define GIDSIZE 200

/*
 * Xact isolation levels
 */
#define XACT_READ_UNCOMMITTED	0
#define XACT_READ_COMMITTED		1
#define XACT_REPEATABLE_READ	2
#define XACT_SERIALIZABLE		3

extern int	DefaultXactIsoLevel;
extern PGDLLIMPORT int XactIsoLevel;

/*
 * We implement three isolation levels internally.
 * The two stronger ones use one snapshot per database transaction;
 * the others use one snapshot per statement.
 * Serializable uses predicate locks in addition to snapshots.
 * These macros should be used to check which isolation level is selected.
 */
#define IsolationUsesXactSnapshot() (XactIsoLevel >= XACT_REPEATABLE_READ)
#define IsolationIsSerializable() (XactIsoLevel == XACT_SERIALIZABLE)

/* Xact read-only state */
extern bool DefaultXactReadOnly;
extern bool XactReadOnly;

/* flag for logging statements in this transaction */
extern bool xact_is_sampled;

/*
 * Xact is deferrable -- only meaningful (currently) for read only
 * SERIALIZABLE transactions
 */
extern bool DefaultXactDeferrable;
extern bool XactDeferrable;

typedef enum
{
	SYNCHRONOUS_COMMIT_OFF,		/* asynchronous commit */
	SYNCHRONOUS_COMMIT_LOCAL_FLUSH, /* wait for local flush only */
	SYNCHRONOUS_COMMIT_REMOTE_WRITE,	/* wait for local flush and remote
										 * write */
	SYNCHRONOUS_COMMIT_REMOTE_FLUSH,	/* wait for local and remote flush */
	SYNCHRONOUS_COMMIT_REMOTE_APPLY /* wait for local and remote flush and
									 * remote apply */
}			SyncCommitLevel;

/* Define the default setting for synchronous_commit */
#define SYNCHRONOUS_COMMIT_ON	SYNCHRONOUS_COMMIT_REMOTE_FLUSH

/* Synchronous commit level */
extern int	synchronous_commit;

/* used during logical streaming of a transaction */
extern PGDLLIMPORT TransactionId CheckXidAlive;
extern PGDLLIMPORT bool bsysscan;

/*
 * Miscellaneous flag bits to record events which occur on the top level
 * transaction. These flags are only persisted in MyXactFlags and are intended
 * so we remember to do certain things later in the transaction. This is
 * globally accessible, so can be set from anywhere in the code which requires
 * recording flags.
 */
extern int	MyXactFlags;

/* Disabled in GPDB as per comment in PrepareTransaction(). */
#if 0
/*
 * XACT_FLAGS_ACCESSEDTEMPNAMESPACE - set when a temporary object is accessed.
 * We don't allow PREPARE TRANSACTION in that case.
 */
#define XACT_FLAGS_ACCESSEDTEMPNAMESPACE		(1U << 0)
#endif

/*
 * XACT_FLAGS_ACQUIREDACCESSEXCLUSIVELOCK - records whether the top level xact
 * logged any Access Exclusive Locks.
 */
#define XACT_FLAGS_ACQUIREDACCESSEXCLUSIVELOCK	(1U << 1)

/*
 * XACT_FLAGS_NEEDIMMEDIATECOMMIT - records whether the top level statement
 * is one that requires immediate commit, such as CREATE DATABASE.
 */
#define XACT_FLAGS_NEEDIMMEDIATECOMMIT			(1U << 2)

/*
 *	start- and end-of-transaction callbacks for dynamically loaded modules
 */
typedef enum
{
	XACT_EVENT_COMMIT,
	XACT_EVENT_PARALLEL_COMMIT,
	XACT_EVENT_ABORT,
	XACT_EVENT_PARALLEL_ABORT,
	XACT_EVENT_PREPARE,
	XACT_EVENT_PRE_COMMIT,
	XACT_EVENT_PARALLEL_PRE_COMMIT,
	XACT_EVENT_PRE_PREPARE
} XactEvent;

typedef void (*XactCallback) (XactEvent event, void *arg);

typedef enum
{
	SUBXACT_EVENT_START_SUB,
	SUBXACT_EVENT_COMMIT_SUB,
	SUBXACT_EVENT_ABORT_SUB,
	SUBXACT_EVENT_PRE_COMMIT_SUB
} SubXactEvent;

typedef void (*SubXactCallback) (SubXactEvent event, SubTransactionId mySubid,
								 SubTransactionId parentSubid, void *arg);


/* ----------------
 *		transaction-related XLOG entries
 * ----------------
 */

/*
 * XLOG allows to store some information in high 4 bits of log record xl_info
 * field. We use 3 for the opcode, and one about an optional flag variable.
 */
/*
* GPDB use the high 4 bits for the opcode, XLOG_XACT_HAS_INFO not needed anymore.
* xinfo field default owned by xl_xact_commit and xl_xact_abort.
*/
#define XLOG_XACT_COMMIT			0x00
#define XLOG_XACT_PREPARE			0x10
#define XLOG_XACT_ABORT				0x20
#define XLOG_XACT_COMMIT_PREPARED	0x30
#define XLOG_XACT_ABORT_PREPARED	0x40
#define XLOG_XACT_ASSIGNMENT		0x50
#define XLOG_XACT_INVALIDATIONS		0x60
#define XLOG_XACT_DISTRIBUTED_COMMIT 0x70
#define XLOG_XACT_DISTRIBUTED_FORGET 0x80

/* mask for filtering opcodes out of xl_info */
#define XLOG_XACT_OPMASK			0xF0

/* does this record have a 'xinfo' field or not */
#define XLOG_XACT_HAS_INFO			0x80

/*
 * The following flags, stored in xinfo, determine which information is
 * contained in commit/abort records.
 */
#define XACT_XINFO_HAS_DBINFO			(1U << 0)
#define XACT_XINFO_HAS_SUBXACTS			(1U << 1)
#define XACT_XINFO_HAS_RELFILENODES		(1U << 2)
#define XACT_XINFO_HAS_INVALS			(1U << 3)
#define XACT_XINFO_HAS_TWOPHASE			(1U << 4)
#define XACT_XINFO_HAS_ORIGIN			(1U << 5)
#define XACT_XINFO_HAS_AE_LOCKS			(1U << 6)
#define XACT_XINFO_HAS_GID				(1U << 7)
#define XACT_XINFO_HAS_DISTRIB			(1U << 8)
#define XACT_XINFO_HAS_DELDBS			(1U << 9)

/*
 * Also stored in xinfo, these indicating a variety of additional actions that
 * need to occur when emulating transaction effects during recovery.
 *
 * They are named XactCompletion... to differentiate them from
 * EOXact... routines which run at the end of the original transaction
 * completion.
 */
#define XACT_COMPLETION_APPLY_FEEDBACK			(1U << 29)
#define XACT_COMPLETION_UPDATE_RELCACHE_FILE	(1U << 30)
#define XACT_COMPLETION_FORCE_SYNC_COMMIT		(1U << 31)

/* Access macros for above flags */
#define XactCompletionApplyFeedback(xinfo) \
	((xinfo & XACT_COMPLETION_APPLY_FEEDBACK) != 0)
#define XactCompletionRelcacheInitFileInval(xinfo) \
	((xinfo & XACT_COMPLETION_UPDATE_RELCACHE_FILE) != 0)
#define XactCompletionForceSyncCommit(xinfo) \
	((xinfo & XACT_COMPLETION_FORCE_SYNC_COMMIT) != 0)

typedef struct xl_xact_assignment
{
	TransactionId xtop;			/* assigned XID's top-level XID */
	int			nsubxacts;		/* number of subtransaction XIDs */
	TransactionId xsub[FLEXIBLE_ARRAY_MEMBER];	/* assigned subxids */
} xl_xact_assignment;

#define MinSizeOfXactAssignment offsetof(xl_xact_assignment, xsub)

/*
 * Commit and abort records can contain a lot of information. But a large
 * portion of the records won't need all possible pieces of information. So we
 * only include what's needed.
 *
 * A minimal commit/abort record only consists of a xl_xact_commit/abort
 * struct. The presence of additional information is indicated by bits set in
 * 'xl_xact_xinfo->xinfo'. The presence of the xinfo field itself is default
 * owned by xl_xact_commit and xl_xact_abort.
 *
 * NB: All the individual data chunks should be sized to multiples of
 * sizeof(int) and only require int32 alignment. If they require bigger
 * alignment, they need to be copied upon reading.
 */

/* sub-records for commit/abort */

typedef struct xl_xact_xinfo
{
	/*
	 * Even though we right now only require 1 byte of space in xinfo we use
	 * four so following records don't have to care about alignment. Commit
	 * records can be large, so copying large portions isn't attractive.
	 */
	uint32		xinfo;
} xl_xact_xinfo;

typedef struct xl_xact_distrib
{
	DistributedTransactionId distrib_xid;
} xl_xact_distrib;

typedef struct xl_xact_dbinfo
{
	Oid			dbId;			/* MyDatabaseId */
	Oid			tsId;			/* MyDatabaseTableSpace */
} xl_xact_dbinfo;

typedef struct xl_xact_subxacts
{
	int			nsubxacts;		/* number of subtransaction XIDs */
	TransactionId subxacts[FLEXIBLE_ARRAY_MEMBER];
} xl_xact_subxacts;
#define MinSizeOfXactSubxacts offsetof(xl_xact_subxacts, subxacts)

typedef struct xl_xact_relfilenodes
{
	int			nrels;			/* number of subtransaction XIDs */
	RelFileNodePendingDelete xnodes[FLEXIBLE_ARRAY_MEMBER];
} xl_xact_relfilenodes;
#define MinSizeOfXactRelfilenodes offsetof(xl_xact_relfilenodes, xnodes)

typedef struct xl_xact_invals
{
	int			nmsgs;			/* number of shared inval msgs */
	SharedInvalidationMessage msgs[FLEXIBLE_ARRAY_MEMBER];
} xl_xact_invals;
#define MinSizeOfXactInvals offsetof(xl_xact_invals, msgs)

typedef struct xl_xact_deldbs
{
	int			ndeldbs;		/* number of DbDirNodes */
	DbDirNode	deldbs[FLEXIBLE_ARRAY_MEMBER];
} xl_xact_deldbs;
#define MinSizeOfXactDelDbs offsetof(xl_xact_deldbs, deldbs)

typedef struct xl_xact_twophase
{
	TransactionId xid;
} xl_xact_twophase;

typedef struct xl_xact_origin
{
	XLogRecPtr	origin_lsn;
	TimestampTz origin_timestamp;
} xl_xact_origin;

typedef struct xl_xact_commit
{
	TimestampTz xact_time;		/* time of commit */
	Oid	tablespace_oid_to_delete_on_commit;

	/* xl_xact_xinfo follows default */
	/* xl_xact_dbinfo follows if XINFO_HAS_DBINFO */
	/* xl_xact_subxacts follows if XINFO_HAS_SUBXACT */
	/* xl_xact_relfilenodes follows if XINFO_HAS_RELFILENODES */
	/* xl_xact_invals follows if XINFO_HAS_INVALS */
	/* xl_xact_deldbs follows if XACT_XINFO_HAS_DELDBS */
	/* xl_xact_twophase follows if XINFO_HAS_TWOPHASE */
	/* twophase_gid follows if XINFO_HAS_GID. As a null-terminated string. */
	/* xl_xact_origin follows if XINFO_HAS_ORIGIN, stored unaligned! */
} xl_xact_commit;
#define MinSizeOfXactCommit sizeof(xl_xact_commit) 

typedef struct xl_xact_abort
{
	TimestampTz xact_time;		/* time of abort */
	Oid tablespace_oid_to_delete_on_abort;

	/* xl_xact_xinfo follows default */
	/* xl_xact_dbinfo follows if XINFO_HAS_DBINFO */
	/* xl_xact_subxacts follows if XINFO_HAS_SUBXACT */
	/* xl_xact_relfilenodes follows if XINFO_HAS_RELFILENODES */
	/* xl_xact_deldbs follows if XINFO_HAS_DELDBS */
	/* No invalidation messages needed. */
	/* xl_xact_twophase follows if XINFO_HAS_TWOPHASE */
	/* twophase_gid follows if XINFO_HAS_GID. As a null-terminated string. */
	/* xl_xact_origin follows if XINFO_HAS_ORIGIN, stored unaligned! */
} xl_xact_abort;
#define MinSizeOfXactAbort sizeof(xl_xact_abort)

typedef struct xl_xact_prepare
{
	uint32		magic;			/* format identifier */
	uint32		total_len;		/* actual file length */
	TransactionId xid;			/* original transaction XID */
	Oid			database;		/* OID of database it was in */
	TimestampTz prepared_at;	/* time of preparation */
	Oid			owner;			/* user running the transaction */
	int32		nsubxacts;		/* number of following subxact XIDs */
	int32		ncommitrels;	/* number of delete-on-commit rels */
	int32		nabortrels;		/* number of delete-on-abort rels */
	int32		ninvalmsgs;		/* number of cache invalidation messages */
	bool		initfileinval;	/* does relcache init file need invalidation? */
	uint16		gidlen;			/* length of the GID - GID follows the header */
	XLogRecPtr	origin_lsn;		/* lsn of this record at origin node */
	TimestampTz origin_timestamp;	/* time of prepare at origin node */
} xl_xact_prepare;

/*
 * Commit/Abort records in the above form are a bit verbose to parse, so
 * there's a deconstructed versions generated by ParseCommit/AbortRecord() for
 * easier consumption.
 */
typedef struct xl_xact_parsed_commit
{
	TimestampTz xact_time;
	uint32		xinfo;

	Oid			tablespace_oid_to_delete_on_commit;

	Oid			dbId;			/* MyDatabaseId */
	Oid			tsId;			/* MyDatabaseTableSpace */

	int			nsubxacts;
	TransactionId *subxacts;

	int			nrels;
	RelFileNodePendingDelete *xnodes;

	int			nmsgs;
	SharedInvalidationMessage *msgs;

	int			ndeldbs;
	DbDirNode	*deldbs;

	TransactionId twophase_xid; /* only for 2PC */
	char		twophase_gid[GIDSIZE];	/* only for 2PC */
	int			nabortrels;		/* only for 2PC */
	RelFileNodePendingDelete *abortnodes;	/* only for 2PC */

	XLogRecPtr	origin_lsn;
	TimestampTz origin_timestamp;

	DistributedTransactionId        distribXid;
} xl_xact_parsed_commit;

typedef xl_xact_parsed_commit xl_xact_parsed_prepare;

typedef struct xl_xact_parsed_abort
{
	TimestampTz xact_time;
	uint32		xinfo;

	Oid         tablespace_oid_to_delete_on_abort;
	Oid			dbId;			/* MyDatabaseId */
	Oid			tsId;			/* MyDatabaseTableSpace */

	int			nsubxacts;
	TransactionId *subxacts;

	int			nrels;
	RelFileNodePendingDelete *xnodes;

	int			ndeldbs;
	DbDirNode	*deldbs;

	TransactionId twophase_xid; /* only for 2PC */
	char		twophase_gid[GIDSIZE];	/* only for 2PC */

	XLogRecPtr	origin_lsn;
	TimestampTz origin_timestamp;
} xl_xact_parsed_abort;

/* 
 * xl_xact_distributed_forget - moved to cdb/cdbtm.h 
 */
typedef struct xl_xact_distributed_forget
{
	DistributedTransactionId gxid;
} xl_xact_distributed_forget;

/* ----------------
 *		extern definitions
 * ----------------
 */

/* Apache Cloudberry specific */ 
extern void SetSharedTransactionId_writer(DtxContext distributedTransactionContext);
extern void SetSharedTransactionId_reader(FullTransactionId xid, CommandId cid, DtxContext distributedTransactionContext);
extern bool IsTransactionState(void);
extern bool IsAbortInProgress(void);
extern bool IsTransactionPreparing(void);
extern bool IsAbortedTransactionBlockState(void);
extern bool TransactionDidWriteXLog(void);
extern bool TopXactExecutorDidWriteXLog(void);
extern void GetAllTransactionXids(
	DistributedTransactionId	*distribXid,
	TransactionId				*localXid,
	TransactionId				*subXid);
extern TransactionId GetTopTransactionId(void);
extern TransactionId GetTopTransactionIdIfAny(void);
extern TransactionId GetCurrentTransactionId(void);
extern TransactionId GetCurrentTransactionIdIfAny(void);
extern TransactionId GetStableLatestTransactionId(void);
extern SubTransactionId GetCurrentSubTransactionId(void);
extern FullTransactionId GetTopFullTransactionId(void);
extern FullTransactionId GetTopFullTransactionIdIfAny(void);
extern FullTransactionId GetCurrentFullTransactionId(void);
extern FullTransactionId GetCurrentFullTransactionIdIfAny(void);
extern void MarkCurrentTransactionIdLoggedIfAny(void);
extern void MarkTopTransactionWriteXLogOnExecutor(void);
extern bool SubTransactionIsActive(SubTransactionId subxid);
extern CommandId GetCurrentCommandId(bool used);
extern void SetParallelStartTimestamps(TimestampTz xact_ts, TimestampTz stmt_ts);
extern TimestampTz GetCurrentTransactionStartTimestamp(void);
extern TimestampTz GetCurrentStatementStartTimestamp(void);
extern TimestampTz GetCurrentTransactionStopTimestamp(void);
extern void SetCurrentStatementStartTimestamp(void);
extern int	GetCurrentTransactionNestLevel(void);
extern bool TransactionIdIsCurrentTransactionId(TransactionId xid);
extern void CommandCounterIncrement(void);
extern void ForceSyncCommit(void);
extern void StartTransactionCommand(void);
extern void SaveTransactionCharacteristics(void);
extern void RestoreTransactionCharacteristics(void);
extern void CommitTransactionCommand(void);
extern void AbortCurrentTransaction(void);
extern void BeginTransactionBlock(void);
extern bool EndTransactionBlock(bool chain);
extern bool PrepareTransactionBlock(const char *gid);
extern void UserAbortTransactionBlock(bool chain);
extern void BeginImplicitTransactionBlock(void);
extern void EndImplicitTransactionBlock(void);
extern void ReleaseSavepoint(const char *name);
extern void DefineSavepoint(const char *name);
extern void DefineDispatchSavepoint(char *name);
extern void RollbackToSavepoint(const char *name);
extern void BeginInternalSubTransaction(const char *name);
extern void ReleaseCurrentSubTransaction(void);
extern void RollbackAndReleaseCurrentSubTransaction(void);
extern bool IsSubTransaction(void);
extern Size EstimateTransactionStateSpace(void);
extern void SerializeTransactionState(Size maxsize, char *start_address);
extern void StartParallelWorkerTransaction(char *tstatespace);
extern void EndParallelWorkerTransaction(void);
extern bool IsTransactionBlock(void);
extern bool IsTransactionOrTransactionBlock(void);
extern void ExecutorMarkTransactionUsesSequences(void);
extern void ExecutorMarkTransactionDoesWrites(void);
extern bool ExecutorSaysTransactionDoesWrites(void);
extern char TransactionBlockStatusCode(void);
extern void AbortOutOfAnyTransaction(void);
extern void PreventInTransactionBlock(bool isTopLevel, const char *stmtType);
extern void RequireTransactionBlock(bool isTopLevel, const char *stmtType);
extern void WarnNoTransactionBlock(bool isTopLevel, const char *stmtType);
extern bool IsInTransactionBlock(bool isTopLevel);
extern void RegisterXactCallback(XactCallback callback, void *arg);
extern void UnregisterXactCallback(XactCallback callback, void *arg);
extern void RegisterXactCallbackOnce(XactCallback callback, void *arg);
extern void UnregisterXactCallbackOnce(XactCallback callback, void *arg);
extern void RegisterSubXactCallback(SubXactCallback callback, void *arg);
extern void UnregisterSubXactCallback(SubXactCallback callback, void *arg);

extern XLogRecPtr RecordDistributedForgetCommitted(DistributedTransactionId gxid);
extern bool IsSubTransactionAssignmentPending(void);
extern void MarkSubTransactionAssigned(void);

extern int	xactGetCommittedChildren(TransactionId **ptr);

extern XLogRecPtr XactLogCommitRecord(TimestampTz commit_time,
									  Oid tablespace_oid_to_delete_on_commit,
									  int nsubxacts, TransactionId *subxacts,
									  int nrels, RelFileNodePendingDelete *rels,
									  int nmsgs, SharedInvalidationMessage *msgs,
									  int ndeldbs, DbDirNode *deldbs,
									  bool relcacheInval,
									  int xactflags,
									  TransactionId twophase_xid,
									  const char *twophase_gid);

extern XLogRecPtr XactLogAbortRecord(TimestampTz abort_time,
									 Oid tablespace_oid_to_abort,
									 int nsubxacts, TransactionId *subxacts,
									 int nrels, RelFileNodePendingDelete *rels,
									 int ndeldbs, DbDirNode *deldbs,
									 int xactflags, TransactionId twophase_xid,
									 const char *twophase_gid);
extern void xact_redo(XLogReaderState *record);

/* xactdesc.c */
extern void xact_desc(StringInfo buf, XLogReaderState *record);
extern const char *xact_identify(uint8 info);

/* also in xactdesc.c, so they can be shared between front/backend code */
extern void ParseCommitRecord(uint8 info, xl_xact_commit *xlrec, xl_xact_parsed_commit *parsed);
extern void ParseAbortRecord(uint8 info, xl_xact_abort *xlrec, xl_xact_parsed_abort *parsed);
extern void ParsePrepareRecord(uint8 info, xl_xact_prepare *xlrec, xl_xact_parsed_prepare *parsed);

extern void EnterParallelMode(void);
extern void ExitParallelMode(void);
extern bool IsInParallelMode(void);

/* GPDB: Used for logging */
extern const char *IsoLevelAsUpperString(int IsoLevel);

#endif							/* XACT_H */
