this module for makoa allows us to transmit market data
from one makoa server to another (or from some modules to makoa)

Usage in makoa_server.conf example:
export viktor sight localhost:10010

viktor can works in two modes:

normal mode: (normal)
viktor have orders_book state, so in case of problems with consumer
he accumulates orders_book and automatically reinitialize state for orders_book
when it necessary and without affects any others modules

iron sights mode: (sight)
viktor don't have orders_book state, so
he can't send message_instr and initialize order_book by self,
in case of problems with send*(any exceptions from tyra library)
connection with market data provider will be closed,
this will affect all others makoa consumers with valid state,
so should be used only with very trusted consumers
or for test purposes


