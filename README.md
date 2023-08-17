# pg_id - Emulation of Unique ID data types for different database systems

This extension provides additional Unique ID data types for PostgreSQL
- `mgid` (Mongo Object Id)
### Installation
This extension was specially developed for PostgreSQL 15. Currently only available for 64-bit operating systems.

To build and install:
```
make
sudo make install
```
And finally inside the database:
```
CREATE EXTENSION pg_id;
```
### Using
You can use the data types provided by extension as regular data types in PostgreSQL.

`mgid` example:
```
CREATE TABLE test_table (
    a mgid,
    name text
);
INSERT INTO test_table VALUES ('507f191e810c19729de860ea', 'some text');
SELECT a FROM test_table;
```
Operator support is added to the type provided by pg_id. However, some pieces are missing feel free to let me know or contribute. 
### Running tests
We use PostgreSQL Regression Tests to pg_id. [More information](https://www.postgresql.org/docs/current/regress.html).

How to run regression tests:
```
make installcheck
```
### How to use internal functions 
There are some internal functions like hash_mgid or mgid_hash_extended which are required for index support. You can use them as follows:
```
SELECT mgid_hash('507f1f77bcf86cd799439011');
```
Output should look like:
```
 mgid_hash 
-----------
 585932317
(1 row)
```
### Quick Demo
After properly installing and starting a PostgreSQL 15 instance. 
```
sudo su - postgres
psql
CREATE USER {username} SUPERUSER;
```
Let's create a database and give ownership to the user we just created.
```
CREATE DATABASE demo OWNER {username}
```
Connect to the database.
```
\c demo
CREATE EXTENSION pg_id
```
Now we can create a table and use the datatypes provided by pg_id.
```
CREATE TABLE test_mgid1 (a mgid);
INSERT INTO test_mgid1 VALUES ('507f191e810c19729de860ea');
SELECT a FROM test_mgid1;
```