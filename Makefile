PG_CONFIG = pg_config

pg_version := $(word 2,$(shell $(PG_CONFIG) --version))
extension_version = 0

EXTENSION = pg_id
MODULE_big = pg_id
OBJS = inout.o magic.o
DATA_built = pg_id--$(extension_version).sql

REGRESS = init create_mgid mgid_typelen_check operators mgid_functions hash cast order_btree drop
REGRESS_OPTS = --inputdir=test

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

pg_id--$(extension_version).sql: pg_id.sql
	cat $^ >$@

