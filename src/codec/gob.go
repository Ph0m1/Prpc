package codec

import (
	"bufio"
	"encoding/gob"
	"io"
)

type GobCodec struct {
	conn io.ReadWriteCloser
	dec  *gob.GobDecoder
	enc  *gob.GobEncoder
	buf  *bufio.Writer
}

var _ Codec = (*GobCodec)(nil)

func NewGoCodec(conn io.ReadWriteCloser) Codec {
	buf := bufio.NewWriter(conn)
	return &GobCodec{
		conn: conn,
	}
}
