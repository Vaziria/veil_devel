cd depends
make HOST=x86_64-pc-linux-gnu -j$(nproc)
cd ..
./autogen.sh
CONFIG_SITE=$PWD/depends/x86_64-pc-linux-gnu/share/config.site ./configure --prefix=/
make -j$(nproc)