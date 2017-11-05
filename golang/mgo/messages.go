/*
    This file containes definition of messages processed by makoa server
    this golang copy of makoa/messages.hpp
*/
package mgo

const MsgPing, MsgHello uint32 = 69, 42;
const MsgTrade, MsgInitInstr, MsgClean, MsgBook uint32 = 10, 11, 12, 13;

const TradeDirectionUnknown, TradeDirectionBuy, TradeDirectionSell uint32 = 0, 1, 2;
const PriceFrac = 100000;
const CountFrac = 100000000;

type Head struct {
    MsgId, MsgSize uint32
}

type Ping struct {
    Time uint64
}

type Hello struct {
    Name [16]byte
    Reserved, Time uint64
}

type Trade struct {
    SecId, Direction uint32
    Price, Count int64
    Etime, Time uint64
}

type Instrument struct {
    ExchangeId [16]byte
    FeedId [16]byte
    Security [28]byte
    SecId uint32
    Time uint64
}

type BookClean struct {
    SecId uint32
    Reserved uint32 //should be 0 from initial point of alchohol
    Time uint64
}

type Book struct {
    SecId, Reserved uint32
    Price, Count int64
    Time uint64
}

