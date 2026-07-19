// geonotes_go.go — карта заметок на Go (консоль + генерация HTML-карты)

package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"math"
	"os"
	"os/exec"
	"strconv"
	"strings"
	"time"
)

type Note struct {
	ID       int     `json:"id"`
	Text     string  `json:"text"`
	Lat      float64 `json:"lat"`
	Lon      float64 `json:"lon"`
	Category string  `json:"category"`
	Created  int64   `json:"created"`
}

type App struct {
	notes      []Note
	nextID     int
	currentLat float64
	currentLon float64
	filename   string
}

func NewApp() *App {
	return &App{
		notes:      []Note{},
		nextID:     1,
		currentLat: 55.7558,
		currentLon: 37.6173,
		filename:   "notes.json",
	}
}

func (a *App) load() {
	data, err := ioutil.ReadFile(a.filename)
	if err != nil {
		return
	}
	var notes []Note
	if err := json.Unmarshal(data, &notes); err != nil {
		return
	}
	a.notes = notes
	for _, n := range a.notes {
		if n.ID >= a.nextID {
			a.nextID = n.ID + 1
		}
	}
}

func (a *App) save() {
	data, _ := json.MarshalIndent(a.notes, "", "  ")
	ioutil.WriteFile(a.filename, data, 0644)
}

func (a *App) add(text string, lat, lon float64) {
	note := Note{
		ID:       a.nextID,
		Text:     text,
		Lat:      lat,
		Lon:      lon,
		Created:  time.Now().Unix(),
		Category: "",
	}
	a.notes = append(a.notes, note)
	a.nextID++
	a.save()
	fmt.Printf("Добавлена заметка #%d\n", note.ID)
}

func (a *App) list() {
	for _, n := range a.notes {
		fmt.Printf("#%d: %s (%.5f, %.5f)\n", n.ID, n.Text, n.Lat, n.Lon)
	}
}

func (a *App) delete(id int) {
	for i, n := range a.notes {
		if n.ID == id {
			a.notes = append(a.notes[:i], a.notes[i+1:]...)
			a.save()
			fmt.Printf("Заметка #%d удалена\n", id)
			return
		}
	}
	fmt.Printf("Заметка #%d не найдена\n", id)
}

func (a *App) showMap() {
	if len(a.notes) == 0 {
		fmt.Println("Нет заметок для отображения")
		return
	}
	html := `<!DOCTYPE html><html><head><meta charset="utf-8"/><title>GeoNotes Map</title>
<link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css"/>
<script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
<style>body{margin:0}#map{height:100vh}</style></head><body>
<div id="map"></div><script>
var map = L.map('map').setView([` +
		fmt.Sprint(a.notes[0].Lat) + "," + fmt.Sprint(a.notes[0].Lon) +
		`], 12);
L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png').addTo(map);`
	for _, n := range a.notes {
		html += "L.marker([" + fmt.Sprint(n.Lat) + "," + fmt.Sprint(n.Lon) + "]).bindPopup('" + n.Text + "<br><b>#" + fmt.Sprint(n.ID) + "</b>').addTo(map);"
	}
	html += `</script></body></html>`
	filename := "map.html"
	ioutil.WriteFile(filename, []byte(html), 0644)
	fmt.Println("Карта сохранена в map.html, открываю в браузере...")
	exec.Command("xdg-open", filename).Start()
}

func (a *App) search(query string) {
	found := []Note{}
	for _, n := range a.notes {
		if strings.Contains(strings.ToLower(n.Text), strings.ToLower(query)) {
			found = append(found, n)
		}
	}
	if len(found) == 0 {
		fmt.Println("Ничего не найдено")
		return
	}
	for _, n := range found {
		fmt.Printf("#%d: %s (%.5f, %.5f)\n", n.ID, n.Text, n.Lat, n.Lon)
	}
}

func (a *App) searchNear(lat, lon, radius float64) {
	found := []Note{}
	for _, n := range a.notes {
		if haversine(lat, lon, n.Lat, n.Lon) <= radius {
			found = append(found, n)
		}
	}
	if len(found) == 0 {
		fmt.Printf("Нет заметок в радиусе %.2f км\n", radius)
		return
	}
	for _, n := range found {
		fmt.Printf("#%d: %s (%.5f, %.5f) расстояние: %.2f км\n", n.ID, n.Text, n.Lat, n.Lon, haversine(lat, lon, n.Lat, n.Lon))
	}
}

func (a *App) updateLocation() {
	// Используем ipinfo.io
	// Упрощённо: можно вручную ввести координаты
	fmt.Print("Введите широту (или оставьте пустым для автоматического определения): ")
	scanner := bufio.NewScanner(os.Stdin)
	scanner.Scan()
	latStr := scanner.Text()
	if latStr != "" {
		lat, _ := strconv.ParseFloat(latStr, 64)
		fmt.Print("Введите долготу: ")
		scanner.Scan()
		lonStr := scanner.Text()
		lon, _ := strconv.ParseFloat(lonStr, 64)
		a.currentLat = lat
		a.currentLon = lon
		fmt.Printf("Геолокация установлена: %.5f, %.5f\n", a.currentLat, a.currentLon)
	} else {
		// Авто через ipinfo
		resp, err := http.Get("http://ipinfo.io/json")
		if err != nil {
			fmt.Println("Ошибка определения геолокации")
			return
		}
		defer resp.Body.Close()
		body, _ := ioutil.ReadAll(resp.Body)
		var data map[string]interface{}
		json.Unmarshal(body, &data)
		loc := data["loc"].(string)
		parts := strings.Split(loc, ",")
		if len(parts) == 2 {
			a.currentLat, _ = strconv.ParseFloat(parts[0], 64)
			a.currentLon, _ = strconv.ParseFloat(parts[1], 64)
			fmt.Printf("Геолокация обновлена: %.5f, %.5f\n", a.currentLat, a.currentLon)
		}
	}
}

func haversine(lat1, lon1, lat2, lon2 float64) float64 {
	const R = 6371.0
	dLat := (lat2 - lat1) * math.Pi / 180
	dLon := (lon2 - lon1) * math.Pi / 180
	a := math.Sin(dLat/2)*math.Sin(dLat/2) +
		math.Cos(lat1*math.Pi/180)*math.Cos(lat2*math.Pi/180)*
			math.Sin(dLon/2)*math.Sin(dLon/2)
	c := 2 * math.Atan2(math.Sqrt(a), math.Sqrt(1-a))
	return R * c
}

func main() {
	app := NewApp()
	app.load()
	scanner := bufio.NewScanner(os.Stdin)
	fmt.Println("🗺️ GeoNotes — Go Edition")
	fmt.Println("Команды: add, list, delete <id>, map, search <текст>, near <lat> <lon> <radius>, update, exit")
	for {
		fmt.Print("> ")
		if !scanner.Scan() {
			break
		}
		line := strings.TrimSpace(scanner.Text())
		if line == "" {
			continue
		}
		parts := strings.SplitN(line, " ", 2)
		cmd := parts[0]
		arg := ""
		if len(parts) > 1 {
			arg = parts[1]
		}
		switch cmd {
		case "add":
			fmt.Print("Текст заметки: ")
			scanner.Scan()
			text := scanner.Text()
			lat, lon := app.currentLat, app.currentLon
			fmt.Print("Введите широту (Enter для текущей): ")
			scanner.Scan()
			latStr := scanner.Text()
			if latStr != "" {
				lat, _ = strconv.ParseFloat(latStr, 64)
				fmt.Print("Введите долготу: ")
				scanner.Scan()
				lonStr := scanner.Text()
				lon, _ = strconv.ParseFloat(lonStr, 64)
			}
			app.add(text, lat, lon)
		case "list":
			app.list()
		case "delete":
			if arg == "" {
				fmt.Println("Укажите ID")
			} else {
				id, _ := strconv.Atoi(arg)
				app.delete(id)
			}
		case "map":
			app.showMap()
		case "search":
			if arg == "" {
				fmt.Println("Укажите текст для поиска")
			} else {
				app.search(arg)
			}
		case "near":
			// формат: near lat lon radius
			args := strings.Fields(arg)
			if len(args) < 3 {
				fmt.Println("Использование: near lat lon radius")
			} else {
				lat, _ := strconv.ParseFloat(args[0], 64)
				lon, _ := strconv.ParseFloat(args[1], 64)
				radius, _ := strconv.ParseFloat(args[2], 64)
				app.searchNear(lat, lon, radius)
			}
		case "update":
			app.updateLocation()
		case "exit":
			app.save()
			fmt.Println("До свидания!")
			return
		default:
			fmt.Println("Неизвестная команда")
		}
	}
}
