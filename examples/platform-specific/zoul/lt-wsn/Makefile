CONTIKI_PROJECT = lt-sensor lt-sink
PROJECT_SOURCEFILES += rtcc-tools.c

BOARD ?= remote-revb

all: $(CONTIKI_PROJECT)

CONTIKI = ../../../..

PLATFORMS_ONLY = zoul
BOARDS_ONLY = remote-revb


#CFLAGS += -DDATE_CONF_SECONDS="`date +%s`"

MAKE_MAC = MAKE_MAC_TSCH

include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_SERVICES_DIR)/shell


include $(CONTIKI)/Makefile.include
