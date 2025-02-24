package codec

import (
	"log"
)

var debug = true

func INFO(a ...any) {
	if debug {
		log.Println(a)
	}
}

func ERROR(a ...any) {
	if debug {
		log.Println(a)
	}
}
