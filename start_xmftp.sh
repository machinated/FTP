#!/bin/bash
JAILPATH=/ftp/ftproot/home
XMFTP=/ftp/bin/xmftp
LOGFILE=/ftp/log.txt
cd $JAILPATH
compartment --chroot $JAILPATH --cap CAP_NET_BIND_SERVICE $XMFTP>>$LOGFILE
