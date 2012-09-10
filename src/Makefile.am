bin_PROGRAMS=gst_playd
gst_playd_SOURCES= \
	gst_playd.c 
	gst_playd.1 
	aclocal.m4

#  uncomment the following if gst_playd requires the math library
#gst_playd_LDADD=-lm

gst_playd_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(LIBZMQ_CFLAGS) \
	$(GST_CFLAGS)

gst_playd_LDFLAGS = \
	$(GLIB_LDFLAGS) \
	$(LIBZMQ_LDFLAGS) \
	$(GST_LDFLAGS)

#  install the man pages
man_MANS=gst_playd.1
