# Copyright (c) 2026 Lenka Koskova Triskova
#
# SPDX-License-Identifier: Apache-2.0
# https://www.apache.org/licenses/LICENSE-2.0

cmake_minimum_required(VERSION 3.20.0)

# Pulled in unconditionally, merged on top of whichever of
# prj.conf/prj_release.conf FILE_SUFFIX selects for CONF_FILE -
# EXTRA_CONF_FILE is never subject to the suffix replace-not-merge
# behavior CONF_FILE has, so this is where the lines shared between
# the dev and release builds live, instead of being duplicated in
# both.
set(EXTRA_CONF_FILE common.conf)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(release)

target_sources(app PRIVATE src/main.c)
