SOURCE=http://www.libsdl.org/release/SDL-1.2.15.tar.gz
wget -c $SOURCE


tar xzf `ls SDL-*.tar.gz`
cd SDL-*

./configure --prefix=/mingw
make -j 4
make install
#evil workaround
rm /mingw/lib/libSDLmain.la