// geonotes_rs.rs — карта заметок на Rust (консоль + генерация HTML)

use std::collections::HashMap;
use std::fs;
use std::io::{self, Write, BufRead};
use std::process::Command;
use std::time::{SystemTime, UNIX_EPOCH};
use serde::{Deserialize, Serialize};
use serde_json;

#[derive(Serialize, Deserialize, Clone)]
struct Note {
    id: i32,
    text: String,
    lat: f64,
    lon: f64,
    category: String,
    created: u64,
}

struct App {
    notes: Vec<Note>,
    next_id: i32,
    current_lat: f64,
    current_lon: f64,
    filename: String,
}

impl App {
    fn new() -> Self {
        App {
            notes: Vec::new(),
            next_id: 1,
            current_lat: 55.7558,
            current_lon: 37.6173,
            filename: "notes.json".to_string(),
        }
    }

    fn load(&mut self) {
        if let Ok(data) = fs::read_to_string(&self.filename) {
            if let Ok(notes) = serde_json::from_str::<Vec<Note>>(&data) {
                self.notes = notes;
                for n in &self.notes {
                    if n.id >= self.next_id { self.next_id = n.id + 1; }
                }
            }
        }
    }

    fn save(&self) {
        let data = serde_json::to_string_pretty(&self.notes).unwrap();
        fs::write(&self.filename, data).unwrap();
    }

    fn add(&mut self, text: &str, lat: f64, lon: f64) {
        let note = Note {
            id: self.next_id,
            text: text.to_string(),
            lat,
            lon,
            category: "".to_string(),
            created: SystemTime::now().duration_since(UNIX_EPOCH).unwrap().as_secs(),
        };
        self.notes.push(note);
        self.next_id += 1;
        self.save();
        println!("Добавлена заметка #{}", note.id);
    }

    fn list(&self) {
        for n in &self.notes {
            println!("#{}: {} ({:.5}, {:.5})", n.id, n.text, n.lat, n.lon);
        }
    }

    fn delete(&mut self, id: i32) {
        let pos = self.notes.iter().position(|n| n.id == id);
        if let Some(i) = pos {
            self.notes.remove(i);
            self.save();
            println!("Заметка #{} удалена", id);
        } else {
            println!("Заметка #{} не найдена", id);
        }
    }

    fn show_map(&self) {
        if self.notes.is_empty() {
            println!("Нет заметок для отображения");
            return;
        }
        let mut html = String::from(r#"<!DOCTYPE html><html><head><meta charset="utf-8"/><title>GeoNotes Map</title>
<link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css"/>
<script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
<style>body{margin:0}#map{height:100vh}</style></head><body>
<div id="map"></div><script>
var map = L.map('map').setView(["#);
        html.push_str(&format!("{}, {}", self.notes[0].lat, self.notes[0].lon));
        html.push_str(r#"], 12);
L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png').addTo(map);"#);
        for n in &self.notes {
            html.push_str(&format!("L.marker([{}, {}]).bindPopup('{}<br><b>#{}</b>').addTo(map);", n.lat, n.lon, n.text, n.id));
        }
        html.push_str(r#"</script></body></html>"#);
        fs::write("map.html", html).unwrap();
        println!("Карта сохранена в map.html, открываю в браузере...");
        Command::new("xdg-open").arg("map.html").status().unwrap();
    }

    fn search(&self, query: &str) {
        let found: Vec<&Note> = self.notes.iter()
            .filter(|n| n.text.to_lowercase().contains(&query.to_lowercase()))
            .collect();
        if found.is_empty() {
            println!("Ничего не найдено");
            return;
        }
        for n in found {
            println!("#{}: {} ({:.5}, {:.5})", n.id, n.text, n.lat, n.lon);
        }
    }

    fn search_near(&self, lat: f64, lon: f64, radius: f64) {
        let found: Vec<&Note> = self.notes.iter()
            .filter(|n| haversine(lat, lon, n.lat, n.lon) <= radius)
            .collect();
        if found.is_empty() {
            println!("Нет заметок в радиусе {:.2} км", radius);
            return;
        }
        for n in found {
            let dist = haversine(lat, lon, n.lat, n.lon);
            println!("#{}: {} ({:.5}, {:.5}) расстояние: {:.2} км", n.id, n.text, n.lat, n.lon, dist);
        }
    }

    fn update_location(&mut self) {
        // Упрощённо: ввод вручную
        print!("Введите широту (или оставьте пустым для автоматического определения): ");
        io::stdout().flush().unwrap();
        let mut input = String::new();
        io::stdin().read_line(&mut input).unwrap();
        let input = input.trim();
        if !input.is_empty() {
            let lat: f64 = input.parse().unwrap();
            print!("Введите долготу: ");
            io::stdout().flush().unwrap();
            let mut lon_str = String::new();
            io::stdin().read_line(&mut lon_str).unwrap();
            let lon: f64 = lon_str.trim().parse().unwrap();
            self.current_lat = lat;
            self.current_lon = lon;
            println!("Геолокация установлена: {:.5}, {:.5}", self.current_lat, self.current_lon);
        } else {
            // Авто через ipinfo (упрощённо — заглушка)
            println!("Автоопределение не реализовано в консольной версии");
        }
    }
}

fn haversine(lat1: f64, lon1: f64, lat2: f64, lon2: f64) -> f64 {
    const R: f64 = 6371.0;
    let d_lat = (lat2 - lat1).to_radians();
    let d_lon = (lon2 - lon1).to_radians();
    let a = (d_lat / 2.0).sin().powi(2) +
            lat1.to_radians().cos() * lat2.to_radians().cos() *
            (d_lon / 2.0).sin().powi(2);
    let c = 2.0 * a.sqrt().atan2((1.0 - a).sqrt());
    R * c
}

fn main() {
    let mut app = App::new();
    app.load();
    println!("🗺️ GeoNotes — Rust Edition");
    println!("Команды: add, list, delete <id>, map, search <текст>, near <lat> <lon> <radius>, update, exit");
    let stdin = io::stdin();
    let mut reader = stdin.lock();
    loop {
        print!("> ");
        io::stdout().flush().unwrap();
        let mut line = String::new();
        if reader.read_line(&mut line).is_err() { break; }
        let line = line.trim();
        if line.is_empty() { continue; }
        let parts: Vec<&str> = line.splitn(2, ' ').collect();
        let cmd = parts[0];
        let arg = if parts.len() > 1 { parts[1] } else { "" };
        match cmd {
            "add" => {
                print!("Текст заметки: ");
                io::stdout().flush().unwrap();
                let mut text = String::new();
                reader.read_line(&mut text).unwrap();
                let text = text.trim();
                let (lat, lon) = (app.current_lat, app.current_lon);
                print!("Введите широту (Enter для текущей): ");
                io::stdout().flush().unwrap();
                let mut lat_str = String::new();
                reader.read_line(&mut lat_str).unwrap();
                let lat_str = lat_str.trim();
                let lat = if lat_str.is_empty() { lat } else { lat_str.parse().unwrap() };
                print!("Введите долготу: ");
                io::stdout().flush().unwrap();
                let mut lon_str = String::new();
                reader.read_line(&mut lon_str).unwrap();
                let lon = lon_str.trim().parse().unwrap_or(lon);
                app.add(text, lat, lon);
            }
            "list" => app.list(),
            "delete" => {
                if let Ok(id) = arg.parse::<i32>() { app.delete(id); }
                else { println!("Укажите ID"); }
            }
            "map" => app.show_map(),
            "search" => {
                if arg.is_empty() { println!("Укажите текст для поиска"); }
                else { app.search(arg); }
            }
            "near" => {
                let args: Vec<&str> = arg.split_whitespace().collect();
                if args.len() < 3 {
                    println!("Использование: near lat lon radius");
                } else {
                    let lat: f64 = args[0].parse().unwrap();
                    let lon: f64 = args[1].parse().unwrap();
                    let radius: f64 = args[2].parse().unwrap();
                    app.search_near(lat, lon, radius);
                }
            }
            "update" => app.update_location(),
            "exit" => {
                app.save();
                println!("До свидания!");
                break;
            }
            _ => println!("Неизвестная команда"),
        }
    }
}
