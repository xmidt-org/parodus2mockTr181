bin_PROGRAMS=webpa
webpa_SOURCES=main.c webpa_rpc.c webpa_client_connector.c $(top_srcdir)/app/rpcgen/webpa_rpc_clnt.c  $(top_srcdir)/app/rpcgen/webpa_rpc_server_stubs.c  $(top_srcdir)/app/rpcgen/webpa_rpc_xdr.c
webpa_CFLAGS = -I$(top_srcdir)/app/rpcgen
webpa_LDFLAGS =  -lwebpacore -lbase64
webpa_LDADD = $(top_builddir)/wal/libwal.la
