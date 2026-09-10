#! /bin/bash
# Used to move boot.firm to my new 3ds XL using ftpd
SRC="./boot.firm"
FTP_URL="ftp://192.168.1.165:5000/boot.firm"

curl -T "$SRC" "$FTP_URL"
