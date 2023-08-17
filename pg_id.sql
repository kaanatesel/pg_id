CREATE TYPE mgid;

CREATE FUNCTION mg_id_in(cstring) RETURNS mgid
    IMMUTABLE STRICT LANGUAGE C AS '$libdir/pg_id', 'mg_id_in';

CREATE FUNCTION mg_id_out(mgid) RETURNS cstring
    IMMUTABLE STRICT LANGUAGE C AS '$libdir/pg_id', 'mg_id_out';

CREATE FUNCTION mgid_recv(internal) RETURNS mgid
    IMMUTABLE STRICT LANGUAGE C AS '$libdir/pg_id', 'mgid_recv';

CREATE FUNCTION mgid_send(mgid) RETURNS bytea
    IMMUTABLE STRICT LANGUAGE C AS '$libdir/pg_id', 'mgid_send';

CREATE TYPE mgid (
    INPUT = mg_id_in,
    OUTPUT = mg_id_out,
    RECEIVE = mgid_recv,
    SEND = mgid_send,
    INTERNALLENGTH = 12,
    CATEGORY = 'U',
    ALIGNMENT = char
);

CREATE FUNCTION mgid_eq(mgid, mgid) RETURNS boolean
    IMMUTABLE STRICT LANGUAGE C AS '$libdir/pg_id', 'mgid_eq';

CREATE FUNCTION mgid_ne(mgid, mgid) RETURNS boolean 
    IMMUTABLE STRICT LANGUAGE C AS '$libdir/pg_id', 'mgid_ne';

CREATE FUNCTION mgid_lt(mgid, mgid) RETURNS boolean
    IMMUTABLE STRICT LANGUAGE C AS '$libdir/pg_id', 'mgid_lt';

CREATE FUNCTION mgid_gt(mgid, mgid) RETURNS boolean
    IMMUTABLE STRICT LANGUAGE C AS '$libdir/pg_id', 'mgid_gt';

CREATE FUNCTION mgid_ge(mgid, mgid) RETURNS boolean
    IMMUTABLE STRICT LANGUAGE C AS '$libdir/pg_id', 'mgid_ge';

CREATE FUNCTION mgid_le(mgid, mgid) RETURNS boolean
    IMMUTABLE STRICT LANGUAGE C AS '$libdir/pg_id', 'mgid_le';

/* Index Support Functions */
CREATE FUNCTION mgid_cmp(mgid, mgid) RETURNS integer
    IMMUTABLE STRICT LANGUAGE C AS '$libdir/pg_id', 'mgid_cmp';

CREATE FUNCTION mgid_hash(mgid) RETURNS int4
    IMMUTABLE STRICT LANGUAGE C AS '$libdir/pg_id', 'mgid_hash';

CREATE FUNCTION mgid_hash_extended(mgid) RETURNS int4
    IMMUTABLE STRICT LANGUAGE C AS '$libdir/pg_id', 'mgid_hash_extended';

/* User Functions */
CREATE FUNCTION mgid_get_timestamp(mgid) RETURNS cstring
    IMMUTABLE STRICT LANGUAGE C AS '$libdir/pg_id', 'mgid_get_timestamp';

CREATE FUNCTION mgid_get_process_unique(mgid) RETURNS cstring
    IMMUTABLE STRICT LANGUAGE C AS '$libdir/pg_id', 'mgid_get_process_unique';

CREATE FUNCTION mgid_get_counter(mgid) RETURNS cstring
    IMMUTABLE STRICT LANGUAGE C AS '$libdir/pg_id', 'mgid_get_counter';

/* Operators */ 
CREATE OPERATOR = (
    LEFTARG = mgid,
    RIGHTARG = mgid,
    COMMUTATOR = =,
    NEGATOR = <>,
    RESTRICT = eqsel,
    JOIN = eqjoinsel,
    HASHES,
    MERGES,
    PROCEDURE = mgid_eq
);

CREATE OPERATOR <> (
    LEFTARG = mgid,
    RIGHTARG = mgid,
    COMMUTATOR = <>,
    NEGATOR = =,
    RESTRICT = neqsel,
    JOIN = neqjoinsel,
    HASHES,
    MERGES,
    PROCEDURE = mgid_ne
);

CREATE OPERATOR < (
    LEFTARG = mgid,
    RIGHTARG = mgid,
    COMMUTATOR = >,
    NEGATOR = >=,
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel,
    PROCEDURE = mgid_lt
);

CREATE OPERATOR > (
    LEFTARG = mgid,
    RIGHTARG = mgid,
    COMMUTATOR = <,
    NEGATOR = <=,
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel,
    PROCEDURE = mgid_gt
);

CREATE OPERATOR >= (
    LEFTARG = mgid,
    RIGHTARG = mgid,
    COMMUTATOR = <=,
    NEGATOR = <,
    RESTRICT = scalargesel,
    JOIN = scalargejoinsel,
    PROCEDURE = mgid_ge
);

CREATE OPERATOR <= (
    LEFTARG = mgid,
    RIGHTARG = mgid,
    COMMUTATOR = >=,
    NEGATOR = >,
    RESTRICT = scalarlesel,
    JOIN = scalarlejoinsel,
    PROCEDURE = mgid_le
);

CREATE OPERATOR FAMILY pg_id_ops USING btree;

CREATE OPERATOR CLASS mg_id_ops
DEFAULT FOR TYPE mgid USING btree FAMILY pg_id_ops AS
  -- standard mgid comparisons
  OPERATOR 1 < ,
  OPERATOR 2 <= ,
  OPERATOR 3 = ,
  OPERATOR 4 >= ,
  OPERATOR 5 > ,
  FUNCTION 1 mgid_cmp(mgid, mgid)
;