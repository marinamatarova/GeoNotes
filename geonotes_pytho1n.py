

Порядок: **Python → C++ → Java → C# → Go → Rust → JavaScript**.

---

### 1. `geonotes_python.py`

```python
# geonotes_python.py — карта заметок на Python (Tkinter + folium)

import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
import json
import os
import webbrowser
import folium
from geopy.geocoders import Nominatim
from geopy.distance import distance
import requests
import threading
import time

class Note:
    def __init__(self, note_id, text, lat, lon, category=""):
        self.id = note_id
        self.text = text
        self.lat = lat
        self.lon = lon
        self.category = category
        self.created = time.time()

    def to_dict(self):
        return {"id": self.id, "text": self.text, "lat": self.lat, "lon": self.lon,
                "category": self.category, "created": self.created}

    @classmethod
    def from_dict(cls, data):
        note = cls(data["id"], data["text"], data["lat"], data["lon"], data.get("category", ""))
        note.created = data.get("created", time.time())
        return note

class GeoNotesApp:
    def __init__(self, root):
        self.root = root
        self.root.title("🗺️ GeoNotes — Python")
        self.root.geometry("900x650")
        self.notes = []
        self.next_id = 1
        self.current_location = (55.7558, 37.6173)  # Москва по умолчанию
        self.geolocator = Nominatim(user_agent="geonotes")
        self.filename = "notes.json"
        self.load_data()
        self.create_widgets()
        self.refresh_list()

    def create_widgets(self):
        # Верхняя панель
        top = tk.Frame(self.root)
        top.pack(fill=tk.X, pady=5)
        tk.Button(top, text="Добавить", command=self.add_note).pack(side=tk.LEFT, padx=5)
        tk.Button(top, text="Удалить", command=self.delete_note).pack(side=tk.LEFT, padx=5)
        tk.Button(top, text="Показать карту", command=self.show_map).pack(side=tk.LEFT, padx=5)
        tk.Button(top, text="Экспорт", command=self.export_data).pack(side=tk.LEFT, padx=5)
        tk.Button(top, text="Импорт", command=self.import_data).pack(side=tk.LEFT, padx=5)
        tk.Button(top, text="Обновить геолокацию", command=self.update_location).pack(side=tk.LEFT, padx=5)

        # Поиск и фильтр
        search_frame = tk.Frame(self.root)
        search_frame.pack(fill=tk.X, pady=5)
        tk.Label(search_frame, text="Поиск:").pack(side=tk.LEFT, padx=5)
        self.search_entry = tk.Entry(search_frame, width=30)
        self.search_entry.pack(side=tk.LEFT, padx=5)
        tk.Button(search_frame, text="Найти", command=self.search_notes).pack(side=tk.LEFT, padx=5)
        tk.Label(search_frame, text="Радиус (км):").pack(side=tk.LEFT, padx=5)
        self.radius_entry = tk.Entry(search_frame, width=8)
        self.radius_entry.pack(side=tk.LEFT, padx=5)
        tk.Button(search_frame, text="Найти рядом", command=self.search_near).pack(side=tk.LEFT, padx=5)

        # Список заметок
        self.listbox = tk.Listbox(self.root, height=15)
        self.listbox.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)
        self.listbox.bind('<<ListboxSelect>>', self.on_select)

        # Текст заметки
        self.text_area = scrolledtext.ScrolledText(self.root, height=6)
        self.text_area.pack(fill=tk.X, padx=10, pady=5)

        # Статус
        self.status = tk.Label(self.root, text="Готов", anchor=tk.W)
        self.status.pack(fill=tk.X, padx=10)

        # Горячие клавиши
        self.root.bind("<Control-n>", lambda e: self.add_note())
        self.root.bind("<Delete>", lambda e: self.delete_note())

    def add_note(self, text=None, lat=None, lon=None):
        if text is None:
            text = tk.simpledialog.askstring("Новая заметка", "Введите текст заметки:")
            if not text:
                return
        if lat is None or lon is None:
            # Используем текущее местоположение
            lat, lon = self.current_location
        note = Note(self.next_id, text, lat, lon)
        self.notes.append(note)
        self.next_id += 1
        self.save_data()
        self.refresh_list()
        self.status.config(text=f"Добавлена заметка #{note.id}")

    def delete_note(self):
        selection = self.listbox.curselection()
        if not selection:
            return
        idx = selection[0]
        if idx < len(self.notes):
            note = self.notes[idx]
            if messagebox.askyesno("Удалить", f"Удалить заметку #{note.id}?"):
                del self.notes[idx]
                self.save_data()
                self.refresh_list()
                self.status.config(text=f"Удалена заметка #{note.id}")

    def on_select(self, event):
        selection = self.listbox.curselection()
        if selection:
            idx = selection[0]
            if idx < len(self.notes):
                note = self.notes[idx]
                self.text_area.delete(1.0, tk.END)
                self.text_area.insert(tk.END, note.text)
                self.status.config(text=f"Заметка #{note.id} ({note.lat:.5f}, {note.lon:.5f})")

    def refresh_list(self, filter_notes=None):
        self.listbox.delete(0, tk.END)
        display = filter_notes if filter_notes is not None else self.notes
        for note in display:
            self.listbox.insert(tk.END, f"#{note.id}: {note.text[:30]}... ({note.lat:.5f}, {note.lon:.5f})")

    def search_notes(self):
        query = self.search_entry.get().strip().lower()
        if not query:
            self.refresh_list()
            return
        filtered = [n for n in self.notes if query in n.text.lower()]
        self.refresh_list(filtered)
        self.status.config(text=f"Найдено {len(filtered)} заметок")

    def search_near(self):
        try:
            radius = float(self.radius_entry.get())
        except ValueError:
            messagebox.showerror("Ошибка", "Введите корректный радиус")
            return
        lat, lon = self.current_location
        filtered = []
        for n in self.notes:
            dist = distance((lat, lon), (n.lat, n.lon)).km
            if dist <= radius:
                filtered.append(n)
        self.refresh_list(filtered)
        self.status.config(text=f"Найдено {len(filtered)} заметок в радиусе {radius} км")

    def update_location(self):
        try:
            # Определяем по IP
            resp = requests.get('http://ipinfo.io/json', timeout=5)
            data = resp.json()
            loc = data.get('loc', '').split(',')
            if len(loc) == 2:
                lat, lon = float(loc[0]), float(loc[1])
                self.current_location = (lat, lon)
                self.status.config(text=f"Геолокация обновлена: {lat:.5f}, {lon:.5f}")
            else:
                self.status.config(text="Не удалось определить местоположение")
        except Exception as e:
            self.status.config(text=f"Ошибка: {e}")

    def show_map(self):
        if not self.notes:
            messagebox.showinfo("Информация", "Нет заметок для отображения")
            return
        # Создаем карту с центром в первой заметке
        center = (self.notes[0].lat, self.notes[0].lon)
        m = folium.Map(location=center, zoom_start=12)
        for note in self.notes:
            popup = f"{note.text}<br><b>#{note.id}</b><br>{note.lat:.5f}, {note.lon:.5f}"
            folium.Marker([note.lat, note.lon], popup=popup).add_to(m)
        # Сохраняем и открываем
        map_file = "map.html"
        m.save(map_file)
        webbrowser.open(map_file)
        self.status.config(text="Карта открыта в браузере")

    def export_data(self):
        filename = tk.filedialog.asksaveasfilename(defaultextension=".json", filetypes=[("JSON", "*.json")])
        if filename:
            data = [n.to_dict() for n in self.notes]
            with open(filename, 'w', encoding='utf-8') as f:
                json.dump(data, f, indent=2, ensure_ascii=False)
            self.status.config(text=f"Экспортировано в {filename}")

    def import_data(self):
        filename = tk.filedialog.askopenfilename(filetypes=[("JSON", "*.json")])
        if filename:
            with open(filename, 'r', encoding='utf-8') as f:
                data = json.load(f)
            for item in data:
                note = Note.from_dict(item)
                if note.id >= self.next_id:
                    self.next_id = note.id + 1
                self.notes.append(note)
            self.save_data()
            self.refresh_list()
            self.status.config(text=f"Импортировано из {filename}")

    def load_data(self):
        if os.path.exists(self.filename):
            try:
                with open(self.filename, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                for item in data:
                    note = Note.from_dict(item)
                    if note.id >= self.next_id:
                        self.next_id = note.id + 1
                    self.notes.append(note)
            except Exception as e:
                print("Ошибка загрузки:", e)

    def save_data(self):
        data = [n.to_dict() for n in self.notes]
        with open(self.filename, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2, ensure_ascii=False)

    def on_close(self):
        self.save_data()
        self.root.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = GeoNotesApp(root)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()
