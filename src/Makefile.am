bin_PROGRAMS=gst_playd

if HAVE_APPLE_MEDIA
APPLE_MEDIA_SOURCES = operations/audio_dev_osx.c
APPLE_MEDIA_LDFLAGS = -framework CoreFoundation -framework CoreAudio -framework Cocoa
else
APPLE_MEDIA_SOURCES =
APPLE_MEDIA_LDFLAGS =
endif

gst_playd_SOURCES= \
	gst_playd.c \
	gst-util.c \
	parser.c \
	pubsub.c \
	operations/control.c \
	operations/ping.c \
	operations/play.c \
	utility.c \
	uuencode.c \
	$(APPLE_MEDIA_SOURCES) \
	gst_playd.1 \
	aclocal.m4

#  uncomment the following if gst_playd requires the math library
gst_playd_CFLAGS = \
	-Wall \
	$(GLIB_CFLAGS) \
	$(LIBZMQ_CFLAGS) \
	$(GST_CFLAGS)

gst_playd_LDADD = \
	$(GLIB_LIBS) \
	$(LIBZMQ_LIBS) \
	$(GST_LIBS)

gst_playd_LDFLAGS = \
	$(APPLE_MEDIA_LDFLAGS)

#  install the man pages
man_MANS=gst_playd.1
