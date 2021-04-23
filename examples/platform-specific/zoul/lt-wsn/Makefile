CONTIKI_PROJECT = lt-sensor lt-sink rt
PROJECT_SOURCEFILES += rtcc-tools.c
PROJECT_SOURCEFILES += lt-data.c lt-data-zoul.c

CONTIKI_TARGET_SOURCEFILES += sht25.c bmpx8x.c dht22.c adc-sensors.c tsl256x.c motion-sensor.c

BOARD ?= remote-revb

all: $(CONTIKI_PROJECT)

CONTIKI = ../../../..

PLATFORMS_ONLY = zoul
BOARDS_ONLY = remote-revb

CFLAGS += -DCURRENT_CONF_DATE="`date +%s`"

MAKE_MAC = MAKE_MAC_TSCH



include Makefile.sensors
include $(CONTIKI)/Makefile.dir-variables

MODULES += $(CONTIKI_NG_SERVICES_DIR)/shell


include $(CONTIKI)/Makefile.include
