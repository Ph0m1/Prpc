package gee

import (
	"net/http"
)

type HandlerFunc func(c *Context)

type Engine struct {
	router *Router
}
