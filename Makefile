# pg_aa/Makefile

MODULE_big = pg_aa
OBJS = pg_aa.o test.o
EXTENSION = pg_aa
DATA = pg_aa--1.0.sql
SHLIB_LINK = -lgd -laa -lcaca
REGRESS = pg_aa

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/pg_aa
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
