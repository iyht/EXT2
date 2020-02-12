make
cp libext2fsal.so ../bin/
clear
cp /student/meharabd/Downloads/emptydisk.img ../img/

../bin/ext2kmfs $1
