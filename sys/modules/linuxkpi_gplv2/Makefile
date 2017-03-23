# $FreeBSD$
.PATH:	${SRCTOP}/sys/compat/linuxkpi/gplv2/src

KMOD=	linuxkpi-gplv2
SRCS=	linux_kmod_gplv2.c	\
	linux_fs_attr.c

SRCS+=	bus_if.h		\
	device_if.h		\
	pci_if.h		\
	vnode_if.h

CFLAGS+= -I${SRCTOP}/sys/compat/linuxkpi/gplv2/include
CFLAGS+= -I${SRCTOP}/sys/compat/linuxkpi/common/include

.include <bsd.kmod.mk>
CWARNFLAGS+=	-Wno-pointer-arith
