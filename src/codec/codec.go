package codec

import (
	"io"
)

type Header struct {
	ServiceMethod string // Service.Method
	Seq           uint64 // From client
	Error         string
}

type Codec interface {
	io.Closer
	ReadHeader(*Header) error
	ReadBody(interface{}) error
	Write(*Header, interface{}) error
}

type NewCodecFunc func(io.ReadWriteCloser) Codec

type Type string

const ()
