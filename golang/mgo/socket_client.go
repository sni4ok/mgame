package mgo

import (
    "encoding/binary"
    "net"
    "unsafe"
    "hash/crc32"
)

type SocketClient struct {
    conn net.Conn
    log_makoa bool
}

func (s SocketClient) send(MsgId uint32, MsgSize uint32, msg interface{}) error {
    head := Head{}
    head.MsgId = MsgId
    head.MsgSize = MsgSize

    err := binary.Write(s.conn, binary.LittleEndian, &head)
    if(err != nil) {
        return err
    }
    err = binary.Write(s.conn, binary.LittleEndian, msg)
    if(err != nil) {
        return err
    }
    return err
}

func (s SocketClient) SendTrade(trade Trade) error {
    err := s.send(MsgTrade, uint32(unsafe.Sizeof(trade)), trade)
    if s.log_makoa {
        Log(">trade|", trade.SecId, "|", trade.Direction, "|", Price(trade.Price),
        "|", Count(trade.Count), "|", Time(trade.Etime), "|", Time(trade.Time), "|")
    }
    return err
}

var crc32t *crc32.Table
func crcInstrument(sec Instrument) uint32 {
    b := *(*[60]byte)(unsafe.Pointer(&sec))
    return crc32.Checksum(b[:], crc32t);
}

func (s SocketClient) InitInstrument(sec Instrument) (uint32, error) {
    sec.SecId = crcInstrument(sec);
    err := s.send(MsgInitInstr, uint32(unsafe.Sizeof(sec)), sec)
    if s.log_makoa {
        Log(">instr|", string(sec.ExchangeId[:16]), "|", string(sec.FeedId[:16]), "|",
        string(sec.Security[:28]), "|", sec.SecId, "|")
    }
    return sec.SecId, err
}

func (s SocketClient) SendBook(book Book) error {
    err := s.send(MsgBook, uint32(unsafe.Sizeof(book)), book)
    if s.log_makoa {
        Log(">book|", book.SecId, "|", Price(book.Price), "|", Count(book.Count), "|", Time(book.Time), "|")
    }
    return err
}

func (s SocketClient) Clean(bc BookClean) error {
    err := s.send(MsgClean, uint32(unsafe.Sizeof(bc)), bc)
    if s.log_makoa {
        Log(">clean|", bc.SecId, "|", bc.Time, "|")
    }
    return err
}

func NewSocketClient(host string, log_makoa bool) (SocketClient, error) {
    if(crc32t == nil) {
        crc32t = crc32.MakeTable(0xedb88320)
    }
    s := SocketClient{}
    s.conn, err = net.Dial("tcp", host)
    s.log_makoa = log_makoa
    if(err != nil) {
        return s, err
    }
    return s, err
}

func (s SocketClient) SendHello(hello Hello) error {
    err := s.send(MsgHello, uint32(unsafe.Sizeof(hello)), hello)
    if s.log_makoa {
        Log(">hello|", hello.Name, "|", Time(hello.Time), "|")
    }
    return err
}

