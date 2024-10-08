/*-------------------------------------------------------------------------
 *
 * nodeFunctionscan.h
 *
 *
 *
 * Portions Copyright (c) 1996-2021, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/executor/nodeFunctionscan.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODEFUNCTIONSCAN_H
#define NODEFUNCTIONSCAN_H

#include "executor/executor.h"

extern FunctionScanState *ExecInitFunctionScan(FunctionScan *node, EState *estate, int eflags);
extern void ExecEndFunctionScan(FunctionScanState *node);
extern void ExecReScanFunctionScan(FunctionScanState *node);
extern void ExecSquelchFunctionScan(FunctionScanState *node, bool force);

#endif							/* NODEFUNCTIONSCAN_H */
