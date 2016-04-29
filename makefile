
CPSW_DIR=.

include $(CPSW_DIR)/defs.mak

HEADERS = cpsw_api_user.h cpsw_api_builder.h cpsw_api_timeout.h cpsw_error.h

cpsw_SRCS = cpsw_entry.cc cpsw_hub.cc cpsw_path.cc
cpsw_SRCS+= cpsw_entry_adapt.cc
cpsw_SRCS+= cpsw_sval.cc
cpsw_SRCS+= cpsw_command.cc
cpsw_SRCS+= cpsw_mmio_dev.cc
cpsw_SRCS+= cpsw_mem_dev.cc
cpsw_SRCS+= cpsw_netio_dev.cc
cpsw_SRCS+= cpsw_buf.cc
cpsw_SRCS+= cpsw_bufq.cc
cpsw_SRCS+= cpsw_event.cc
cpsw_SRCS+= cpsw_enum.cc
cpsw_SRCS+= cpsw_obj_cnt.cc
cpsw_SRCS+= cpsw_rssi_proto.cc
cpsw_SRCS+= cpsw_rssi.cc
cpsw_SRCS+= cpsw_rssi_states.cc
cpsw_SRCS+= cpsw_rssi_timer.cc
cpsw_SRCS+= cpsw_proto_mod_depack.cc
cpsw_SRCS+= cpsw_proto_mod_udp.cc
cpsw_SRCS+= cpsw_proto_mod_srpmux.cc
cpsw_SRCS+= cpsw_proto_mod_tdestmux.cc
cpsw_SRCS+= cpsw_proto_mod_rssi.cc
cpsw_SRCS+= cpsw_thread.cc

DEP_HEADERS  = $(HEADERS)
DEP_HEADERS += cpsw_address.h
DEP_HEADERS += cpsw_buf.h
DEP_HEADERS += cpsw_event.h
DEP_HEADERS += cpsw_entry_adapt.h
DEP_HEADERS += cpsw_entry.h
DEP_HEADERS += cpsw_enum.h
DEP_HEADERS += cpsw_freelist.h
DEP_HEADERS += cpsw_hub.h
DEP_HEADERS += cpsw_mem_dev.h
DEP_HEADERS += cpsw_mmio_dev.h
DEP_HEADERS += cpsw_netio_dev.h
DEP_HEADERS += cpsw_obj_cnt.h
DEP_HEADERS += cpsw_path.h
DEP_HEADERS += cpsw_proto_mod_depack.h
DEP_HEADERS += cpsw_proto_mod.h
DEP_HEADERS += cpsw_proto_mod_bytemux.h
DEP_HEADERS += cpsw_proto_mod_srpmux.h
DEP_HEADERS += cpsw_proto_mod_tdestmux.h
DEP_HEADERS += cpsw_proto_mod_udp.h
DEP_HEADERS += cpsw_rssi_proto.h
DEP_HEADERS += cpsw_rssi.h
DEP_HEADERS += cpsw_rssi_timer.h
DEP_HEADERS += cpsw_proto_mod_rssi.h
DEP_HEADERS += cpsw_shared_obj.h
DEP_HEADERS += cpsw_sval.h
DEP_HEADERS += cpsw_thread.h
DEP_HEADERS += cpsw_mutex.h
DEP_HEADERS += crc32-le-tbl-4.h
DEP_HEADERS += udpsrv_regdefs.h
DEP_HEADERS += udpsrv_port.h
DEP_HEADERS += udpsrv_rssi_port.h
DEP_HEADERS += udpsrv_util.h

STATIC_LIBRARIES+=cpsw

tstaux_SRCS+= crc32-le-tbl-4.c

STATIC_LIBRARIES+=tstaux

udpsrv_SRCS = udpsrv.c
udpsrv_SRCS+= udpsrv_port.cc
udpsrv_SRCS+= udpsrv_util.cc
udpsrv_SRCS+= udpsrv_mod_mem.cc
udpsrv_SRCS+= udpsrv_mod_axiprom.cc

udpsrv_CXXFLAGS+= -DUDPSRV

udpsrv_LIBS = tstaux cpsw pthread rt

PROGRAMS   += udpsrv

cpsw_path_tst_SRCS       = cpsw_path_tst.cc
cpsw_path_tst_LIBS       = $(CPSW_LIBS)
TESTPROGRAMS            += cpsw_path_tst

cpsw_shared_obj_tst_SRCS = cpsw_shared_obj_tst.cc
cpsw_shared_obj_tst_LIBS = $(CPSW_LIBS)
TESTPROGRAMS            += cpsw_shared_obj_tst

cpsw_sval_tst_SRCS       = cpsw_sval_tst.cc
cpsw_sval_tst_LIBS       = $(CPSW_LIBS)
TESTPROGRAMS            += cpsw_sval_tst

cpsw_large_tst_SRCS      = cpsw_large_tst.cc
cpsw_large_tst_LIBS      = $(CPSW_LIBS)
TESTPROGRAMS            += cpsw_large_tst

cpsw_netio_tst_SRCS      = cpsw_netio_tst.cc
cpsw_netio_tst_LIBS      = $(CPSW_LIBS)
TESTPROGRAMS            += cpsw_netio_tst

cpsw_axiv_udp_tst_SRCS   = cpsw_axiv_udp_tst.cc
cpsw_axiv_udp_tst_LIBS   = $(CPSW_LIBS) $(YAML_LIBS)
TESTPROGRAMS            += cpsw_axiv_udp_tst

cpsw_buf_tst_SRCS        = cpsw_buf_tst.cc
cpsw_buf_tst_LIBS        = $(CPSW_LIBS)
TESTPROGRAMS            += cpsw_buf_tst

cpsw_stream_tst_SRCS     = cpsw_stream_tst.cc
cpsw_stream_tst_LIBS     = $(CPSW_LIBS)
cpsw_stream_tst_LIBS    += tstaux
TESTPROGRAMS            += cpsw_stream_tst

cpsw_srpmux_tst_SRCS     = cpsw_srpmux_tst.cc
cpsw_srpmux_tst_LIBS     = $(CPSW_LIBS)
TESTPROGRAMS            += cpsw_srpmux_tst

cpsw_enum_tst_SRCS       = cpsw_enum_tst.cc
cpsw_enum_tst_LIBS       = $(CPSW_LIBS)
TESTPROGRAMS            += cpsw_enum_tst

rssi_tst_SRCS            = rssi_tst.cc udpsrv_port.cc udpsrv_util.cc
rssi_tst_LIBS            = $(CPSW_LIBS)
TESTPROGRAMS            += rssi_tst

cpsw_srpv3_large_tst_SRCS += cpsw_srpv3_large_tst.cc
cpsw_srpv3_large_tst_LIBS += $(CPSW_LIBS)
TESTPROGRAMS              += cpsw_srpv3_large_tst

TEST_AXIV_YES=
TEST_AXIV_NO=cpsw_axiv_udp_tst
DISABLED_TESTPROGRAMS=$(TEST_AXIV_$(TEST_AXIV))

include $(CPSW_DIR)/rules.mak

# may override for individual test targets which will be
# executed with for all option sets, eg:
#
#    some_tst_run:RUN_OPTS='' '-a -b'
#
# (will run once w/o opts, a second time with -a -b)

# run for V2 and V1 and over TDEST demuxer (v2)
# NOTE: the t1 test will be slow as the packetized channel is scrambled
#       by udpsrv (and this increases average roundtrip time)
cpsw_netio_tst_run:     RUN_OPTS='' '-V1 -p8191' '-p8202 -r' '-p8203 -r -t1' '-p8190 -V3' '-p8189 -V3 -b'

cpsw_srpmux_tst_run:    RUN_OPTS='' '-V1 -p8191' '-p8202 -r'

cpsw_axiv_udp_tst_run:  RUN_OPTS='' '-S 30'

# error percentage should be >  value used for udpsrv (-L) times number
# of fragments (-f)
cpsw_stream_tst_run:    RUN_OPTS='-e 22' '-s8203 -R'

rssi_tst_run:           RUN_OPTS='-s500' '-n30000 -G2' '-n30000 -L1'

cpsw_netio_tst: udpsrv
