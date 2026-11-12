# Copyright (c) 2026 Lenka Koskova Triskova
#
# SPDX-License-Identifier: Apache-2.0
# https://www.apache.org/licenses/LICENSE-2.0
#
# One project, two device roles - FILE_SUFFIX picks which one, the
# same mechanism 18-release uses to pick dev vs. release. Here it
# picks which main() gets built instead: -DFILE_SUFFIX=a or
# -DFILE_SUFFIX=b, each pulling in its own prj_<suffix>.conf (see
# common.conf for what both roles share, via EXTRA_CONF_FILE).

cmake_minimum_required(VERSION 3.20.0)

set(EXTRA_CONF_FILE common.conf)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(two_roles)

if(FILE_SUFFIX STREQUAL "b")
    target_sources(app PRIVATE src/main_b.c)
else()
    target_sources(app PRIVATE src/main_a.c)
endif()
