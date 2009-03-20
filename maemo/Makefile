
pkg_packages := gtk+-2.0 gconf-2.0 hildon-1 hildon-fm-2 hildon-help gtk+-2.0 libosso gdk-2.0 gconf-2.0 gnome-vfs-2.0 gstreamer-0.10 libebook-1.2 libalarm bluez libgwobex libhildondesktop osso-gpsd
#osso-addressbook-1.0 gpsbt

PKG_CFLAGS := $(shell pkg-config --cflags $(pkg_packages))
PKG_LDFLAGS := $(shell pkg-config --libs $(pkg_packages))

#-DDEBUG -g 
ADD_CFLAGS := -Wall -O2 -DMAEMO -std=c99 \
  -march=armv6 \
  -I. -I.. \
  -I../../libogg-1.1.3/include -I../../liboggz-0.9.8/include -I../../speex-1.2rc1/include 

# for debug, C flags:  -DDEBUG -g

# combine the flags
CFLAGS  := $(PKG_CFLAGS) $(ADD_CFLAGS) $(CFLAGS)

# libgstinterfaces-0.10 is defined here to work around
# a bug in gstreamer's pkg-config
LDFLAGS := $(PKG_LDFLAGS) $(LDFLAGS) -lgstinterfaces-0.10 -lz


#CFLAGS= -Wall -pedantic $(shell pkg-config --cflags $(LIBRARIES))
#CDEBUG=-g

#ifdef DEBUG
#CFLAGS += $(CDEBUG)
#LDFLAGS += $(CDEBUG)
#endif

LIBEXAMPLES=\
	libapplet.so 

AUX_FILES=Makefile README README.gconf COPYING \
	  example_backup_client.desktop \
	  example_backup_client.service \
	  example_help_framework.service \
	  example_libosso.desktop \
	  example_libosso.service \
	  install_backup_client.sh \
	  install_controlpanel_applet.sh \
	  install_example_libosso.sh 

TWTW_EXEC=twtw
	
twtw: twtw-maemo.o twtw-maemo-canvas.o twtw-maemo-gfx.o twtw-filesystem-maemo.o twtw-graphicscache-maemo.o \
            ../twtw-fixedpoint.o ../twtw-curves.o ../twtw-document.o ../twtw-editing.o ../twtw-photo.o \
            ../skeleton.o ../twtw-audio-wavfile.o ../twtw-ogg.o ../twtw-audioconv.o \
            twtw-audio-maemo.o twtw-camera-maemo.o
#link:            
	cc -o $(TWTW_EXEC) \
	        twtw-maemo.o twtw-maemo-canvas.o twtw-maemo-gfx.o twtw-filesystem-maemo.o twtw-graphicscache-maemo.o \
            ../twtw-fixedpoint.o ../twtw-curves.o ../twtw-document.o ../twtw-editing.o ../twtw-photo.o \
            ../skeleton.o ../twtw-audio-wavfile.o ../twtw-ogg.o ../twtw-audioconv.o \
            twtw-audio-maemo.o twtw-camera-maemo.o \
            libogg.a liboggz.a libspeex.a libspeexdsp.a $(CFLAGS) $(LDFLAGS)

all: twtw

ICON_DIR=$(DESTDIR)`pkg-config osso-af-settings --variable=prefix`/share/icons/hicolor
SB_LIB_DIR=$(DESTDIR)`pkg-config osso-af-settings --variable=hildondesktoplibdir`
SB_DESKTOP_DIR=$(DESTDIR)`pkg-config osso-af-settings --variable=statusbardesktopentrydir`
BIN_DIR=$(DESTDIR)`pkg-config osso-af-settings --variable=prefix`/bin
DESKTOP_DIR=$(DESTDIR)`pkg-config osso-af-settings --variable=desktopentrydir`
DBUS_SERVICE_DIR=$(DESTDIR)`pkg-config osso-af-settings --variable=dbusservicedir`
CP_LIB_DIR=$(DESTDIR)`pkg-config hildon-control-panel --variable=pluginlibdir`
CP_DESKTOP_DIR=$(DESTDIR)`pkg-config hildon-control-panel --variable=plugindesktopentrydir`
MIME_DIR=$(DESTDIR)`pkg-config osso-af-settings --variable=prefix`/share/mime/packages

INSTALL_FILES=$(addsuffix .c, $(EXAMPLES))\
       		$(patsubst %.so, %.c, $(LIBEXAMPLES))\
       		$(AUX_FILES) \

INSTALL_DIR=$(DESTDIR)/usr/share/doc/maemo-examples
#HELP_INSTALL_DIR=$(DESTDIR)/usr/share/osso-help/$(LANG)
HELP_INSTALL_DIR=$(DESTDIR)/usr/share/osso-help/en_GB

install: install_help
	mkdir -p $(ICON_DIR)/40x40/hildon/
	mkdir -p $(ICON_DIR)/26x26/hildon/
	mkdir -p $(SB_LIB_DIR)
	mkdir -p $(SB_DESKTOP_DIR)
	mkdir -p $(BIN_DIR)
	mkdir -p $(DESKTOP_DIR)
	mkdir -p $(DBUS_SERVICE_DIR)
	mkdir -p $(CP_LIB_DIR)
	mkdir -p $(CP_DESKTOP_DIR)
	mkdir -p $(MIME_DIR)
	cp example_*.desktop $(DESKTOP_DIR)
	cp applet.desktop $(CP_DESKTOP_DIR)
	cp libapplet.so $(CP_LIB_DIR)
	cp $(EXAMPLES) $(BIN_DIR)
	cp *.service $(DBUS_SERVICE_DIR)
	cp example-mime.xml $(MIME_DIR)

.PHONY: install_help
install_help:	example.xml
	mkdir -p $(HELP_INSTALL_DIR)
	cp example.xml $(HELP_INSTALL_DIR)
	

# This rule uses GNU make implicit rules, so
# examples can be built eg make example_hildonprogram
# http://www.gnu.org/software/make/manual/make.html#Implicit-Rules
$(EXAMPLES): %: %.c

$(LIBEXAMPLES): %.so: %.c
	    $(CC) -shared $(CFLAGS) $(LDFLAGS) -o $@ $<
	    
clean:
#	-rm -f $(EXAMPLES) $(LIBEXAMPLES)
#	-rm -f example_bluetooth_marshallers.*
#	-rm -f example_alarm_dbus.h
	rm ../*.o ./*.o

dist: clean
	cd .. && \
	cp -a maemo_examples maemo-examples-$(VERSION) && \
	tar czf maemo-examples-$(VERSION).tar.gz \
		maemo-examples-$(VERSION) && \
	rm -rf maemo-examples-$(VERSION)
