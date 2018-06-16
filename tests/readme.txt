/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

makoa with ying exporter should running in personal console

test zero:
generate configs:
    lua make_zero.lua 25

     M    MY
      \  /
p ---> M - M
      / \
     M   M

in first console:
./run_zeroY.sh
in second:
./run_zero.sh

test one:
generate configs:
    lua make_one.lua 25

P - M - M - M - MY

how to run: start 2 consoles:
in first console:
./run_oneY.sh
and in second:
./run_one.sh


