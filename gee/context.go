package gee

import (
	"fmt"
	"net/http"
)

const ContentType = "Content-Type"

type H map[string]interface{}

type Context struct {
	// R & W
	Writer http.ResponseWriter
	Req    *http.Request

	// message
	Path   string
	Method string

	// return
	StatusCode int
}

func NewContext(w http.ResponseWriter, r *http.Request) *Context {
	return &Context{
		Writer: w,
		Req:    r,
		Path:   r.URL.Path,
		Method: r.Method,
	}
}

// Get request value
func (c *Context) GetPostFormValue(key string) string {
	return c.Req.FormValue(key)
}
func (c *Context) GetQueryVal(key string) string {
	return c.Req.URL.Query().Get(key)
}

// set return value
func (c *Context) SetStatusCode(code int) {
	c.StatusCode = code
	c.Writer.WriteHeader(code)
}
func (c *Context) SetHeader(key, value string) {
	c.Req.Header.Set(key, value)
}

// set type of return value
func (c *Context) String(code int, format string, values ...interface{}) {
	c.SetHeader(ContentType, "text/plain")
	c.SetStatusCode(code)
	if _, err := c.Writer.Write([]byte(fmt.Sprintf(format, values))); err != nil {

	}
}

func (c *Context) HTML(code int, html string) {

}

func (c *Context) Data(code int, data []byte) {
	c.SetStatusCode(code)
	if _, err := c.Writer.Write(data); err != nil {
		codec.INFO("context data write error:", err)
	}
}

func (c *Context) JSON(code int, data interface{}) {

}
