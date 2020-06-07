// Package p contains an HTTP Cloud Function.
package p

import (
	"cloud.google.com/go/bigquery"
	"context"
	"encoding/json"
	"fmt"
	"html"
	"log"
	"net/http"
	"os"
	"time"
)

type MeasurementRequest struct {
		ApiSecret string `json:"apiSecret"`
		LocalIpAddress string `json:"localIpAddress"` 
		SensorId string `json:"sensorId"`
		MeasurementType string `json:"measurementType"`
		Measurement float32 `json:"measurement"`
}

func (i *MeasurementRequest) Save() (map[string]bigquery.Value, string, error) {
        return map[string]bigquery.Value{
                "localIpAddress": i.LocalIpAddress,
                "sensorId":       i.SensorId,
				"measurementType": i.MeasurementType,
				"measurement": i.Measurement,
				"reported": time.Now(),
        }, "", nil
}

var client *bigquery.Client

func init() {
	ctx := context.Background()
	var err error
	client, err = bigquery.NewClient(ctx, "climate-station")
	if err != nil {
		log.Printf("Failed to initialise function: %v", err)
	}
}

func Measure(w http.ResponseWriter, r *http.Request) {
	ctx := context.Background()
	d := &MeasurementRequest{}
	if err := json.NewDecoder(r.Body).Decode(d); err != nil {
        log.Printf("Failed to decode payload")
		return
	}
	if d.ApiSecret != os.Getenv("apiSecret") {
		fmt.Fprint(w, "No soup for you!")
		return
	}
	log.Printf("Received measurement. sensorId: %s, measurementType: %s, measurement: %v", d.SensorId, d.MeasurementType, d.Measurement)
	
	reportsDs := client.Dataset("reports")
	measurementsTable := reportsDs.Table("measurements")
	inserter := measurementsTable.Inserter()
	items := []*MeasurementRequest{
                // Item implements the ValueSaver interface.
                d,
        }
    if err := inserter.Put(ctx, items); err != nil {
            log.Printf("Failed to store row: %v", err)
    }

	fmt.Fprint(w, html.EscapeString("OK\n"))
}
