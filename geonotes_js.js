// geonotes_js.js — карта заметок на JavaScript (Electron + Leaflet)

const { app, BrowserWindow, Menu, dialog, ipcMain, shell } = require('electron');
const fs = require('fs');
const path = require('path');
const { exec } = require('child_process');

let mainWindow;
let notes = [];
let nextId = 1;
let currentLat = 55.7558;
let currentLon = 37.6173;
const dataFile = path.join(app.getPath('userData'), 'notes.json');

function loadData() {
    try {
        if (fs.existsSync(dataFile)) {
            const data = fs.readFileSync(dataFile, 'utf8');
            notes = JSON.parse(data);
            notes.forEach(n => { if (n.id >= nextId) nextId = n.id + 1; });
        }
    } catch (e) { /* ignore */ }
}

function saveData() {
    fs.writeFileSync(dataFile, JSON.stringify(notes, null, 2));
}

function createWindow() {
    mainWindow = new BrowserWindow({
        width: 900,
        height: 700,
        webPreferences: {
            nodeIntegration: true,
            contextIsolation: false
        }
    });
    mainWindow.loadFile('index.html'); // HTML с интерфейсом
    Menu.setApplicationMenu(Menu.buildFromTemplate([
        { label: 'Файл', submenu: [{ role: 'quit' }] }
    ]));

    loadData();

    // IPC handlers
    ipcMain.handle('get-notes', () => notes);

    ipcMain.handle('add-note', (event, text, lat, lon) => {
        if (!text) return { error: 'Текст не может быть пустым' };
        const note = {
            id: nextId++,
            text,
            lat: lat || currentLat,
            lon: lon || currentLon,
            category: '',
            created: Date.now()
        };
        notes.push(note);
        saveData();
        return note;
    });

    ipcMain.handle('delete-note', (event, id) => {
        const index = notes.findIndex(n => n.id === id);
        if (index >= 0) {
            notes.splice(index, 1);
            saveData();
            return { success: true };
        }
        return { success: false };
    });

    ipcMain.handle('search-notes', (event, query) => {
        if (!query) return notes;
        return notes.filter(n => n.text.toLowerCase().includes(query.toLowerCase()));
    });

    ipcMain.handle('search-near', (event, lat, lon, radius) => {
        return notes.filter(n => haversine(lat, lon, n.lat, n.lon) <= radius);
    });

    ipcMain.handle('update-location', async () => {
        try {
            const response = await fetch('http://ipinfo.io/json');
            const data = await response.json();
            const loc = data.loc.split(',');
            if (loc.length === 2) {
                currentLat = parseFloat(loc[0]);
                currentLon = parseFloat(loc[1]);
                return { lat: currentLat, lon: currentLon };
            }
        } catch (e) {}
        return null;
    });

    ipcMain.handle('export-data', async (event) => {
        const result = await dialog.showSaveDialog(mainWindow, { filters: [{ name: 'JSON', extensions: ['json'] }] });
        if (!result.canceled) {
            fs.writeFileSync(result.filePath, JSON.stringify(notes, null, 2));
            return { success: true };
        }
        return { success: false };
    });

    ipcMain.handle('import-data', async () => {
        const result = await dialog.showOpenDialog(mainWindow, { filters: [{ name: 'JSON', extensions: ['json'] }] });
        if (!result.canceled) {
            const data = fs.readFileSync(result.filePaths[0], 'utf8');
            const imported = JSON.parse(data);
            imported.forEach(n => {
                if (n.id >= nextId) nextId = n.id + 1;
                notes.push(n);
            });
            saveData();
            return { success: true };
        }
        return { success: false };
    });

    ipcMain.handle('show-map', () => {
        if (notes.length === 0) {
            dialog.showMessageBox(mainWindow, { message: 'Нет заметок для отображения' });
            return;
        }
        // Генерируем HTML с картой и открываем в браузере
        let html = `<!DOCTYPE html><html><head><meta charset="utf-8"/><title>GeoNotes Map</title>
<link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css"/>
<script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
<style>body{margin:0}#map{height:100vh}</style></head><body>
<div id="map"></div><script>
var map = L.map('map').setView([${notes[0].lat}, ${notes[0].lon}], 12);
L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png').addTo(map);`;
        notes.forEach(n => {
            html += `L.marker([${n.lat}, ${n.lon}]).bindPopup('${n.text}<br><b>#${n.id}</b>').addTo(map);`;
        });
        html += `</script></body></html>`;
        const mapFile = path.join(app.getPath('temp'), 'geonotes_map.html');
        fs.writeFileSync(mapFile, html);
        shell.openPath(mapFile);
    });

    function haversine(lat1, lon1, lat2, lon2) {
        const R = 6371;
        const dLat = (lat2 - lat1) * Math.PI / 180;
        const dLon = (lon2 - lon1) * Math.PI / 180;
        const a = Math.sin(dLat/2)**2 + Math.cos(lat1 * Math.PI/180) * Math.cos(lat2 * Math.PI/180) * Math.sin(dLon/2)**2;
        const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1-a));
        return R * c;
    }

    mainWindow.on('closed', () => {
        mainWindow = null;
    });
}

app.whenReady().then(createWindow);

app.on('window-all-closed', () => {
    if (process.platform !== 'darwin') app.quit();
});
