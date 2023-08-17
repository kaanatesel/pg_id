/*-------------------------------------------------------------------------
 *
 * mgid.h
 *	  Header file for the "mgid" ADT. In C, we use the name pg_mgid_t,
 *	  to avoid conflicts with any mgid_t type that might be defined by
 *	  the system headers.
 *
 *-------------------------------------------------------------------------
 */
#ifndef MGID_H
#define MGID_H

/* uuid size in bytes */
#define MGID_LEN 12
#define MGID_TIMESTAMP_INDEX 4
#define MGID_PROCESS_UNIQUE 5
#define MGID_COUNTER 3

typedef struct pg_mgid_t
{
	unsigned char data[MGID_LEN];
} pg_mgid_t;

/* fmgr interface macros */
#define MGIDPGetDatum(X)		PointerGetDatum(X)
#define PG_RETURN_MGID_P(X)		return MGIDPGetDatum(X)
#define DatumGetMGIDP(X)		((pg_mgid_t *) DatumGetPointer(X))
#define PG_GETARG_MGID_P(X)		DatumGetMGIDP(PG_GETARG_DATUM(X))

#endif							/* MGID_H */
