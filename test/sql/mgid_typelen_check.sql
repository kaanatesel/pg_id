select typlen from pg_type where oid = 'mgid'::regtype::oid;

select pg_column_size(mgid '00000000bcf86cd799439011');