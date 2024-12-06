# PMS - Pack My Sh*t
#
# Developer: cowmonk
# Contributor(s): kirwano, localspook
#

VERSION = 0.9.9-preview

### Compiler Flags & default C compiler

# CFLAGS = -std=c99 -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE=700L -DVERSION=\"${VERSION}\" -s -pedantic -Wall -Os
CFLAGS = -std=c99 -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE=700L -DVERSION=\"${VERSION}\" -g -pedantic -Wall -Os
# INCLUDES =
# LDFLAGS =
CC=cc

### Install Directory
PREFIX=/usr

#### Target name
TARGET="pms"
