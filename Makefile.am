ACLOCAL_AMFLAGS = -I m4
CFLAGS = $(shell pkg-config --cflags gstreamer-1.0)
CXXFLAGS = $(CFLAGS)
lib_LTLIBRARIES = libjanus_rtmp.la
libjanus_rtmp_la_SOURCES = src/libjanus_rtmp.c
libjanus_rtmp_la_LDFLAGS = -version-info 0:0:0 $(shell pkg-config --libs gstreamer-1.0)

LIBS = $(shell pkg-config --libs gstreamer-1.0)

confdir = $(exec_prefix)/etc/janus
libdir = $(exec_prefix)/lib/janus/plugins
conf_DATA = conf/janus.plugin.rtmp.cfg.sample

##
# Configuration
##

config:
	$(MKDIR_P) $(DESTDIR)$(confdir)
	$(foreach config,$(conf_DATA),cp "$(CURDIR)/$(config)" "$(DESTDIR)$(confdir)/$(notdir $(basename $(config) .sample))";)
