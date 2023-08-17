#include <postgres.h>
#include <fmgr.h>
#include <utils/builtins.h>
#include <lib/stringinfo.h>
#include <lib/hyperloglog.h>
#include <libpq/pqformat.h>
#include <common/hashfn.h>
#include <utils/sortsupport.h>
#include <pg_config_manual.h>
#include <utils/guc.h>

#include "pg_id.h"
#include "mgid.h"

/* sortsupport for mgid */
typedef struct
{
	int64		input_count;	/* number of non-null values seen */
	bool		estimating;		/* true if estimating cardinality */

	hyperLogLogState abbr_card; /* cardinality estimator */
} mgid_sortsupport_state;

static void string_to_mgid(const char *source, pg_mgid_t *mgid);
static int mgid_internal_cmp(const pg_mgid_t *arg1, const pg_mgid_t *arg2);
static int mgid_fast_cmp(Datum x, Datum y, SortSupport ssup);
static bool mgid_abbrev_abort(int memtupcount, SortSupport ssup);
static Datum mgid_abbrev_convert(Datum original, SortSupport ssup);

/*
 * We allow mgid's as a series of 24 hexadecimal digits.
 */
static void
string_to_mgid(const char *source, pg_mgid_t *mgid)
{
	const char *src = source;
	int			i;

	for (i = 0; i < MGID_LEN; i++)
	{
		char		str_buf[3];

		if (src[0] == '\0' || src[1] == '\0')
			goto syntax_error;
		memcpy(str_buf, src, 2);
		if (!isxdigit((unsigned char) str_buf[0]) ||
			!isxdigit((unsigned char) str_buf[1]))
			goto syntax_error;

		str_buf[2] = '\0';
		mgid->data[i] = (unsigned char) strtoul(str_buf, NULL, 16);
		src += 2;
	}

	if (*src != '\0')
		goto syntax_error;
	
	return;
syntax_error:
	ereport(ERROR,
			(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
			 errmsg("invalid input syntax for type %s: \"%s\"",
					"mgid", source)));
}

static int 
mgid_internal_cmp(const pg_mgid_t *arg1, const pg_mgid_t *arg2)
{
	return memcmp(arg1->data, arg2->data, MGID_LEN);
}

/*
 * SortSupport comparison func
 */
static int
mgid_fast_cmp(Datum x, Datum y, SortSupport ssup)
{
	pg_mgid_t  *arg1 = DatumGetMGIDP(x);
	pg_mgid_t  *arg2 = DatumGetMGIDP(y);

	return mgid_internal_cmp(arg1, arg2);
}

/*
 * Callback for estimating effectiveness of abbreviated key optimization.
 *
 * We pay no attention to the cardinality of the non-abbreviated data, because
 * there is no equality fast-path within authoritative mgid comparator.
 */
static bool
mgid_abbrev_abort(int memtupcount, SortSupport ssup)
{
	mgid_sortsupport_state *mss = ssup->ssup_extra;
	double		abbr_card;

	if (memtupcount < 10000 || mss->input_count < 10000 || !mss->estimating)
		return false;

	abbr_card = estimateHyperLogLog(&mss->abbr_card);

	/*
	 * If we have >100k distinct values, then even if we were sorting many
	 * billion rows we'd likely still break even, and the penalty of undoing
	 * that many rows of abbrevs would probably not be worth it.  Stop even
	 * counting at that point.
	 */
	if (abbr_card > 100000.0)
	{
#ifdef TRACE_SORT
		if (trace_sort)
			elog(LOG,
				 "mgid_abbrev: estimation ends at cardinality %f"
				 " after " INT64_FORMAT " values (%d rows)",
				 abbr_card, mss->input_count, memtupcount);
#endif
		mss->estimating = false;
		return false;
	}

	/*
	 * Target minimum cardinality is 1 per ~2k of non-null inputs.  0.5 row
	 * fudge factor allows us to abort earlier on genuinely pathological data
	 * where we've had exactly one abbreviated value in the first 2k
	 * (non-null) rows.
	 */
	if (abbr_card < mss->input_count / 2000.0 + 0.5)
	{
#ifdef TRACE_SORT
		if (trace_sort)
			elog(LOG,
				 "mgid_abbrev: aborting abbreviation at cardinality %f"
				 " below threshold %f after " INT64_FORMAT " values (%d rows)",
				 abbr_card, mss->input_count / 2000.0 + 0.5, mss->input_count,
				 memtupcount);
#endif
		return true;
	}

#ifdef TRACE_SORT
	if (trace_sort)
		elog(LOG,
			 "mgid_abbrev: cardinality %f after " INT64_FORMAT
			 " values (%d rows)", abbr_card, mss->input_count, memtupcount);
#endif

	return false;
}

/*
 * Conversion routine for sortsupport.  Converts original mgid representation
 * to abbreviated key representation.  Our encoding strategy is simple -- pack
 * the first `sizeof(Datum)` bytes of mgid data into a Datum (on little-endian
 * machines, the bytes are stored in reverse order), and treat it as an
 * unsigned integer.
 */
static Datum
mgid_abbrev_convert(Datum original, SortSupport ssup)
{
	mgid_sortsupport_state *mss = ssup->ssup_extra;
	pg_mgid_t  *authoritative = DatumGetMGIDP(original);
	Datum		res;

	memcpy(&res, authoritative->data, sizeof(Datum));
	mss->input_count += 1;

	if (mss->estimating)
	{
		uint32		tmp;

#if SIZEOF_DATUM == 8
		tmp = (uint32) res ^ (uint32) ((uint64) res >> 32);
#else							/* SIZEOF_DATUM != 8 */
		tmp = (uint32) res;
#endif

		addHyperLogLog(&mss->abbr_card, DatumGetUInt32(hash_uint32(tmp)));
	}

	/*
	 * Byteswap on little-endian machines.
	 *
	 * This is needed so that ssup_datum_unsigned_cmp() (an unsigned integer
	 * 3-way comparator) works correctly on all platforms.  If we didn't do
	 * this, the comparator would have to call memcmp() with a pair of
	 * pointers to the first byte of each abbreviated key, which is slower.
	 */
	res = DatumBigEndianToNative(res);

	return res;
}

/* mgid type functions functions */
PG_FUNCTION_INFO_V1(mg_id_in);
Datum
mg_id_in(PG_FUNCTION_ARGS)
{
	char	   *mgid_str = PG_GETARG_CSTRING(0);
	pg_mgid_t  *mgid;

	mgid = (pg_mgid_t *) palloc(sizeof(*mgid));
	string_to_mgid(mgid_str, mgid);
	PG_RETURN_MGID_P(mgid);
}

PG_FUNCTION_INFO_V1(mg_id_out);
Datum
mg_id_out(PG_FUNCTION_ARGS)
{
	pg_mgid_t  *mgid = PG_GETARG_MGID_P(0);
	static const char hex_chars[] = "0123456789abcdef";
	StringInfoData buf;
	int			i;

	initStringInfo(&buf);
	for (i = 0; i < MGID_LEN; i++)
	{
		int			hi;
		int			lo;

		hi = mgid->data[i] >> 4;
		lo = mgid->data[i] & 0x0F;

		appendStringInfoChar(&buf, hex_chars[hi]);
		appendStringInfoChar(&buf, hex_chars[lo]);
	}

	PG_RETURN_CSTRING(buf.data);
}

PG_FUNCTION_INFO_V1(mgid_recv);
Datum
mgid_recv(PG_FUNCTION_ARGS)
{
	StringInfo	buffer = (StringInfo) PG_GETARG_POINTER(0);
	pg_mgid_t  *mgid;

	mgid = (pg_mgid_t *) palloc(MGID_LEN);
	memcpy(mgid->data, pq_getmsgbytes(buffer, MGID_LEN), MGID_LEN);
	PG_RETURN_POINTER(mgid);
}

PG_FUNCTION_INFO_V1(mgid_send);
Datum
mgid_send(PG_FUNCTION_ARGS)
{
	pg_mgid_t  *mgid = PG_GETARG_MGID_P(0);
	StringInfoData buffer;

	pq_begintypsend(&buffer);
	pq_sendbytes(&buffer, (char *) mgid->data, MGID_LEN);
	PG_RETURN_BYTEA_P(pq_endtypsend(&buffer));
}

/* mgid operator functions */
PG_FUNCTION_INFO_V1(mgid_eq);
Datum
mgid_eq(PG_FUNCTION_ARGS)
{
	pg_mgid_t  *arg1 = PG_GETARG_MGID_P(0);
	pg_mgid_t  *arg2 = PG_GETARG_MGID_P(1);

	PG_RETURN_BOOL(mgid_internal_cmp(arg1, arg2) == 0);
}

PG_FUNCTION_INFO_V1(mgid_ne);
Datum
mgid_ne(PG_FUNCTION_ARGS)
{
	pg_mgid_t  *arg1 = PG_GETARG_MGID_P(0);
	pg_mgid_t  *arg2 = PG_GETARG_MGID_P(1);

	PG_RETURN_BOOL(mgid_internal_cmp(arg1, arg2) != 0);
}

PG_FUNCTION_INFO_V1(mgid_lt);
Datum
mgid_lt(PG_FUNCTION_ARGS)
{
	pg_mgid_t  *arg1 = PG_GETARG_MGID_P(0);
	pg_mgid_t  *arg2 = PG_GETARG_MGID_P(1);

	PG_RETURN_BOOL(mgid_internal_cmp(arg1, arg2) < 0);
}

PG_FUNCTION_INFO_V1(mgid_gt);
Datum
mgid_gt(PG_FUNCTION_ARGS)
{
	pg_mgid_t  *arg1 = PG_GETARG_MGID_P(0);
	pg_mgid_t  *arg2 = PG_GETARG_MGID_P(1);

	PG_RETURN_BOOL(mgid_internal_cmp(arg1, arg2) > 0);
}

PG_FUNCTION_INFO_V1(mgid_le);
Datum
mgid_le(PG_FUNCTION_ARGS)
{
	pg_mgid_t  *arg1 = PG_GETARG_MGID_P(0);
	pg_mgid_t  *arg2 = PG_GETARG_MGID_P(1);

	PG_RETURN_BOOL(mgid_internal_cmp(arg1, arg2) <= 0);
}

PG_FUNCTION_INFO_V1(mgid_ge);
Datum
mgid_ge(PG_FUNCTION_ARGS)
{
	pg_mgid_t  *arg1 = PG_GETARG_MGID_P(0);
	pg_mgid_t  *arg2 = PG_GETARG_MGID_P(1);

	PG_RETURN_BOOL(mgid_internal_cmp(arg1, arg2) >= 0);
}

/* handler for btree index operator */
PG_FUNCTION_INFO_V1(mgid_cmp);
Datum
mgid_cmp(PG_FUNCTION_ARGS)
{
	pg_mgid_t  *arg1 = PG_GETARG_MGID_P(0);
	pg_mgid_t  *arg2 = PG_GETARG_MGID_P(1);

	PG_RETURN_INT32(mgid_internal_cmp(arg1, arg2));
}

/* hash index support */
PG_FUNCTION_INFO_V1(mgid_hash);
Datum
mgid_hash(PG_FUNCTION_ARGS)
{
	pg_mgid_t  *key = PG_GETARG_MGID_P(0);

	return hash_any(key->data, MGID_LEN);
}

PG_FUNCTION_INFO_V1(mgid_hash_extended);
Datum
mgid_hash_extended(PG_FUNCTION_ARGS)
{
	pg_mgid_t  *key = PG_GETARG_MGID_P(0);

	return hash_any_extended(key->data, MGID_LEN, PG_GETARG_INT64(1));
}

/* user functions */
// TODO
PG_FUNCTION_INFO_V1(mgid_get_timestamp);
Datum 
mgid_get_timestamp(PG_FUNCTION_ARGS)
{
	pg_mgid_t  *mgid = PG_GETARG_MGID_P(0);
	static const char hex_chars[] = "0123456789abcdef";
	StringInfoData buf;
	int			i;

	initStringInfo(&buf);
	for (i = 0; i < MGID_TIMESTAMP_INDEX; i++)
	{
		int			hi;
		int			lo;

		hi = mgid->data[i] >> 4;
		lo = mgid->data[i] & 0x0F;

		appendStringInfoChar(&buf, hex_chars[hi]);
		appendStringInfoChar(&buf, hex_chars[lo]);
	}

	PG_RETURN_CSTRING(buf.data);
}

PG_FUNCTION_INFO_V1(mgid_get_process_unique);
Datum 
mgid_get_process_unique(PG_FUNCTION_ARGS)
{
	pg_mgid_t  *mgid = PG_GETARG_MGID_P(0);
	static const char hex_chars[] = "0123456789abcdef";
	StringInfoData buf;
	int			i;

	initStringInfo(&buf);
	for (i = MGID_TIMESTAMP_INDEX; i < MGID_TIMESTAMP_INDEX + MGID_PROCESS_UNIQUE; i++)
	{
		int			hi;
		int			lo;

		hi = mgid->data[i] >> 4;
		lo = mgid->data[i] & 0x0F;

		appendStringInfoChar(&buf, hex_chars[hi]);
		appendStringInfoChar(&buf, hex_chars[lo]);
	}

	PG_RETURN_CSTRING(buf.data);
}

PG_FUNCTION_INFO_V1(mgid_get_counter);
Datum 
mgid_get_counter(PG_FUNCTION_ARGS)
{
	pg_mgid_t  *mgid = PG_GETARG_MGID_P(0);
	static const char hex_chars[] = "0123456789abcdef";
	StringInfoData buf;
	int			i;

	initStringInfo(&buf);
	for (i = MGID_TIMESTAMP_INDEX + MGID_PROCESS_UNIQUE; i < MGID_LEN; i++)
	{
		int			hi;
		int			lo;

		hi = mgid->data[i] >> 4;
		lo = mgid->data[i] & 0x0F;

		appendStringInfoChar(&buf, hex_chars[hi]);
		appendStringInfoChar(&buf, hex_chars[lo]);
	}

	PG_RETURN_CSTRING(buf.data);
}

