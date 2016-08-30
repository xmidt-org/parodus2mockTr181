lib_LTLIBRARIES=libwal.la
libwal_la_SOURCES=wal.c ssp_messagebus_interface.c ssp_main.c ssp_action.c cosa_Webpa_dml.c plugin_main.c
libwal_la_CFLAGS = -I$(top_srcdir)/wal/include $(DBUS_CFLAGS)
libwal_la_LDFLAGS = -lccsp_common -lm -lpthread -llog4c -lbase64
