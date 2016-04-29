# In this file the desired target architectures
# as well as toolchain paths and prefixes are
# defined.
#
# The names for target architectures can be picked
# arbitrarily but should not characters besides
# [0-9][a-z][A-Z][.][-][_]

# By default the 'host' architecture is built. But
# you can redefine the name of the host architecture:
# HARCH = www
HARCH=linux-x86_64

# Assuming you also want 'xxx' and 'yyy' then you'd
# add these to the 'ARCHES' variable:

# ARCHES += xxx yyy

ARCHES += linuxRT-x86_64

# Next, you need to define prefixes (which may include
# absolute paths) for the tools, e.g.,:
BR_VERSION=2015.02

BR_ARCH_linuxRT_x86_64=x86_64
BR_ARCH_linuxRT_x86=i686
BR_ARCH_linuxRT_zynq=arm
BR_ARCH=$(BR_ARCH_$(TARNM))

BR_TOOLDIR=/afs/slac/package/linuxRT/buildroot-$(BR_VERSION)/host/$(HARCH)/$(BR_ARCH)/usr/bin

# CROSS_xxx = path_to_xxx_tools/xxx-
# CROSS_yyy = path_to_yyy_tools/yyy-
#
# host build needs no prefix
CROSS_linux_x86_64=
CROSS_linuxRT_x86_64=$(BR_TOOLDIR)/$(BR_ARCH)-linux-

# so that e.g., $(CROSS_xxx)gcc can be used as a C compiler

# (you can override 'gcc' by another compiler by setting CC_<arch>)


# CPSW requires 'boost'. If this installed at a non-standard
# location (where it cannot be found automatically by the tools)
# then you need to set one of the variables

# boostinc_DIR_xxx, boostinc_DIR_default, boostinc_DIR
BOOST_VERSION=1.57.0

BOOST_ARCH_linux_x86_64=linux-x86_64
BOOST_ARCH_linuxRT_x86_64=linuxRT_glibc-x86_64

BOOST_ARCH=$(BOOST_ARCH_$(TARNM))
BOOST_DIR=/afs/slac/g/lcls/package/boost/$(BOOST_VERSION)/$(BOOST_ARCH)
boostinc_DIR=$(addsuffix /include,$(BOOST_DIR))
boostlib_DIR=$(addsuffix /lib,$(BOOST_DIR))

# to the directory where boost headers are installed.
# These aforementioned variables are tried in the listed
# order and the first one which is defined is used.
# Thus, e.g.,
#
# boostinc_DIR_xxx = /a/b/c
# boostinc_DIR     = /e/f
#
# will use /a/b/c for architecture 'xxx' and /e/f for
# any other one (including the 'host').

# Likewise, you must set
# boostlib_DIR_xxx, boostlib_DIR_default, boostlib_DIR
# if 'boost' libraries cannot be found automatically by
# the (cross) tools.
# Note 'boostlib_DIR' (& friends) must be absolute paths
# or relative to $(CPSW_DIR).

# yaml-cpp is located the same way
YAML_CPP_VERSION             = yaml-cpp-0.5.3
YAML_CPP_ARCH_linux_x86_64   = rhel6-x86_64
YAML_CPP_ARCH_linuxRT_x86_64 = buildroot-2015.02-x86_64
YAML_CPP_ARCH                = $(YAML_CPP_ARCH_$(TARNM))
YAML_CPP_DIR                 = /afs/slac/g/lcls/package/yaml-cpp/$(YAML_CPP_VERSION)/$(YAML_CPP_ARCH)

# use 'addsuffix'
yaml_cpplib_DIR          = $(addsuffix /lib,$(YAML_CPP_DIR))
yaml_cppinc_DIR          = $(addsuffix /include,$(YAML_CPP_DIR))
