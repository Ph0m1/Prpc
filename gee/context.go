package gee

import (
	"encoding/json"
	"fmt"
	"logger"
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
		logger.INFO("set string return type error(context set return value):", err)
	}
}

func (c *Context) HTML(code int, html string) {
	c.SetHeader(ContentType, "text/html")
	c.SetStatusCode(code)
	if _, err := c.Writer.Write([]byte(html)); err != nil {
		logger.INFO("set html return type error(context set return value):", err)
	}
}

func (c *Context) Data(code int, data []byte) {
	c.SetStatusCode(code)
	if _, err := c.Writer.Write(data); err != nil {
		logger.INFO("set data return type error(context set return value):", err)
	}
}

func (c *Context) JSON(code int, data interface{}) {
	c.SetHeader(ContentType, "application/json")
	c.SetStatusCode(code)
	encoder := json.NewEncoder(c.Writer)
	if err := encoder.Encode(data); err != nil {
		logger.INFO("set json return type error(context set return value):", err)
		http.Error(c.Writer, err.Error(), 500)
	}
}
