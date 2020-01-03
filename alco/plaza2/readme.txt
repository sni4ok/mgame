this folder containes parser for MOEX cgate library

how to install:
download latest version of cgate library from moex ftp://ftp.moex.com/pub/ClientsAPI/Spectra/CGate/
like
wget ftp://ftp.moex.com/pub/ClientsAPI/Spectra/CGate/cgate_linux_amd64-6.3.0.67.zip
unzip cgate_linux_amd64-6.3.0.67.zip

update scheme:
    make plaza_templater
    mkdir scheme
    ./plaza_templater cgate/scheme/SPECTRA63/ ../alco/plaza2/plaza.scheme scheme


