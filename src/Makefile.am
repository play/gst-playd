bin_PROGRAMS=gst_playd

gst_playd_SOURCES= \
	gst_playd.c \
	parser.c \
	pubsub.c \
	operations.c \
	utility.c \
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

#  install the man pages
man_MANS=gst_playd.1
