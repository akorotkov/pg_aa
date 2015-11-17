/* pg_aa/pg_aa--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_aa" to load this file. \quit

--
--  PostgreSQL code for pg_aa.
--

CREATE FUNCTION aa_out(bytea, int4)
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION caca_out(bytea, int4)
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C IMMUTABLE STRICT;
