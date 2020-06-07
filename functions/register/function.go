// Package p contains an HTTP Cloud Function.
package p

import (
	"encoding/json"
	"fmt"
	"html"
	"net/http"
	"log"
	"os"
)

func Register(w http.ResponseWriter, r *http.Request) {
	var d struct {
		ApiSecret string `json:"apiSecret"`
		LocalIpAddress string `json:"localIpAddress"` 
	}
	if err := json.NewDecoder(r.Body).Decode(&d); err != nil {
        log.Printf("Failed to decode payload")
		return
	}
	if d.ApiSecret != os.Getenv("apiSecret") {
		fmt.Fprint(w, "No soup for you!")
		return
	}
	log.Printf("Registered climate device %s from %s", d.LocalIpAddress, r.RemoteAddr)
	fmt.Fprint(w, html.EscapeString("OK\n"))
}
