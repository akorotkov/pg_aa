CREATE EXTENSION pg_aa;

CREATE TABLE logo (name text NOT NULL, data bytea NOT NULL);
\copy logo FROM 'data/logo.data'

SELECT aa_out(data, 20) FROM logo ORDER BY name;
SELECT aa_out(data, 40) FROM logo ORDER BY name;

SELECT caca_out(data, 20) FROM logo ORDER BY name;
SELECT caca_out(data, 40) FROM logo ORDER BY name;