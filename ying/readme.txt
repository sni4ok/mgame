this folder contains module for makoa that display
selected order_book state and last trades
to STDOUT by ncurses library
Usage in makoa_server.conf:

export ying [security[ refresh_rate_ms[ params for glasses[|params for trades]]]]
possible security:
    $0 first ticker from data,
    security_id
    security

params: t{0,1}m{0,1,2,3}c{0,1}
t print_time
m time_mode (0 sec, 1 ms, 2 us, 3 ns)
c print_count
e print exchange time for trades

examples:
export ying
export ying $0 100
export ying $0 100 t1m2c0
export ying $0 100 t0m0c1|t1m1c1e0

hotkeys:
a disable/enable autoscroll
arrow left, arrow right prev and next tickers for view
pgup, pgdown, arrow up, arrow down positioning for glass view when autoscroll disable
p pause

