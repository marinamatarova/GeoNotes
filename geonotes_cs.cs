// geonotes_cs.cs — карта заметок на C# (WPF + WebView2)

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using Microsoft.Web.WebView2.Wpf;

namespace GeoNotesWPF
{
    public partial class MainWindow : Window
    {
        private List<Note> notes = new List<Note>();
        private int nextId = 1;
        private double currentLat = 55.7558, currentLon = 37.6173;
        private ListBox listBox;
        private TextBox textBox, searchBox, radiusBox;
        private WebView2 webView;
        private Label statusLabel;
        private string dataFile = "notes.json";

        public MainWindow()
        {
            InitializeComponent();
            LoadData();
            CreateUI();
            RefreshList();
        }

        private void CreateUI()
        {
            Title = "🗺️ GeoNotes — C#";
            Width = 900;
            Height = 700;

            var grid = new Grid();
            grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
            grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
            grid.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1, GridUnitType.Star) });
            grid.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1, GridUnitType.Star) });
            grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });

            // Верхняя панель
            var topPanel = new StackPanel { Orientation = Orientation.Horizontal };
            var addBtn = new Button { Content = "Добавить", Width = 80 };
            var delBtn = new Button { Content = "Удалить", Width = 80 };
            var mapBtn = new Button { Content = "Показать карту", Width = 100 };
            var exportBtn = new Button { Content = "Экспорт", Width = 80 };
            var importBtn = new Button { Content = "Импорт", Width = 80 };
            var locBtn = new Button { Content = "Обновить геолокацию", Width = 120 };
            topPanel.Children.Add(addBtn);
            topPanel.Children.Add(delBtn);
            topPanel.Children.Add(mapBtn);
            topPanel.Children.Add(exportBtn);
            topPanel.Children.Add(importBtn);
            topPanel.Children.Add(locBtn);
            Grid.SetRow(topPanel, 0);
            grid.Children.Add(topPanel);

            // Поиск
            var searchPanel = new StackPanel { Orientation = Orientation.Horizontal, Margin = new Thickness(5) };
            searchPanel.Children.Add(new Label { Content = "Поиск:" });
            searchBox = new TextBox { Width = 200 };
            searchPanel.Children.Add(searchBox);
            var searchBtn = new Button { Content = "Найти" };
            searchPanel.Children.Add(searchBtn);
            searchPanel.Children.Add(new Label { Content = "Радиус (км):" });
            radiusBox = new TextBox { Width = 60, Text = "5" };
            searchPanel.Children.Add(radiusBox);
            var nearBtn = new Button { Content = "Найти рядом" };
            searchPanel.Children.Add(nearBtn);
            Grid.SetRow(searchPanel, 1);
            grid.Children.Add(searchPanel);

            // Список и текст
            var split = new Grid();
            split.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
            split.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
            listBox = new ListBox();
            listBox.SelectionChanged += (s, e) => OnSelect();
            Grid.SetColumn(listBox, 0);
            split.Children.Add(listBox);
            textBox = new TextBox { IsReadOnly = true, TextWrapping = TextWrapping.Wrap, VerticalScrollBarVisibility = ScrollBarVisibility.Auto };
            Grid.SetColumn(textBox, 1);
            split.Children.Add(textBox);
            Grid.SetRow(split, 2);
            grid.Children.Add(split);

            // Карта (WebView2)
            webView = new WebView2();
            webView.Visibility = Visibility.Collapsed;
            Grid.SetRow(webView, 3);
            grid.Children.Add(webView);

            // Статус
            statusLabel = new Label { Content = "Готов" };
            Grid.SetRow(statusLabel, 4);
            grid.Children.Add(statusLabel);

            Content = grid;

            addBtn.Click += (s, e) => AddNote();
            delBtn.Click += (s, e) => DeleteNote();
            mapBtn.Click += (s, e) => ShowMap();
            exportBtn.Click += (s, e) => ExportData();
            importBtn.Click += (s, e) => ImportData();
            locBtn.Click += (s, e) => UpdateLocation();
            searchBtn.Click += (s, e) => SearchNotes();
            nearBtn.Click += (s, e) => SearchNear();

            // Горячие клавиши
            this.KeyDown += (s, e) => {
                if (e.Key == Key.N && Keyboard.Modifiers == ModifierKeys.Control) AddNote();
                if (e.Key == Key.Delete) DeleteNote();
            };
        }

        private void AddNote()
        {
            var dialog = new InputDialog("Введите текст заметки:");
            if (dialog.ShowDialog() == true && !string.IsNullOrEmpty(dialog.Answer))
            {
                var note = new Note { Id = nextId++, Text = dialog.Answer, Lat = currentLat, Lon = currentLon, Created = DateTimeOffset.UtcNow.ToUnixTimeSeconds() };
                notes.Add(note);
                SaveData();
                RefreshList();
                statusLabel.Content = $"Добавлена заметка #{note.Id}";
            }
        }

        private void DeleteNote()
        {
            int idx = listBox.SelectedIndex;
            if (idx < 0) return;
            if (MessageBox.Show("Удалить заметку?", "Подтверждение", MessageBoxButton.YesNo) == MessageBoxResult.Yes)
            {
                notes.RemoveAt(idx);
                SaveData();
                RefreshList();
                statusLabel.Content = "Заметка удалена";
            }
        }

        private void OnSelect()
        {
            int idx = listBox.SelectedIndex;
            if (idx >= 0 && idx < notes.Count)
            {
                var n = notes[idx];
                textBox.Text = n.Text + "\n\nКоординаты: " + n.Lat + ", " + n.Lon;
                statusLabel.Content = $"Заметка #{n.Id}";
            }
        }

        private async void ShowMap()
        {
            if (notes.Count == 0) { MessageBox.Show("Нет заметок"); return; }
            // Создаем HTML с Leaflet
            string html = @"<!DOCTYPE html><html><head><meta charset='utf-8'/><title>Map</title>
<link rel='stylesheet' href='https://unpkg.com/leaflet@1.9.4/dist/leaflet.css'/>
<script src='https://unpkg.com/leaflet@1.9.4/dist/leaflet.js'></script>
<style>body{margin:0}#map{height:100vh}</style></head><body>
<div id='map'></div><script>
var map=L.map('map').setView([" + notes[0].Lat + "," + notes[0].Lon + "], 12);
L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png').addTo(map);";
            foreach (var n in notes)
            {
                html += "L.marker([" + n.Lat + "," + n.Lon + "]).bindPopup('" + n.Text + "<br><b>#" + n.Id + "</b>').addTo(map);";
            }
            html += "</script></body></html>";
            string tempFile = Path.GetTempFileName() + ".html";
            File.WriteAllText(tempFile, html);
            webView.Visibility = Visibility.Visible;
            await webView.EnsureCoreWebView2Async(null);
            webView.CoreWebView2.Navigate(tempFile);
            statusLabel.Content = "Карта загружена";
        }

        private void SearchNotes()
        {
            string query = searchBox.Text.Trim().ToLower();
            if (string.IsNullOrEmpty(query)) { RefreshList(); return; }
            var filtered = notes.Where(n => n.Text.ToLower().Contains(query)).ToList();
            RefreshList(filtered);
            statusLabel.Content = $"Найдено {filtered.Count} заметок";
        }

        private void SearchNear()
        {
            if (!double.TryParse(radiusBox.Text, out double radius)) { MessageBox.Show("Введите корректный радиус"); return; }
            double lat = currentLat, lon = currentLon;
            var filtered = notes.Where(n => Haversine(lat, lon, n.Lat, n.Lon) <= radius).ToList();
            RefreshList(filtered);
            statusLabel.Content = $"Найдено {filtered.Count} заметок в радиусе {radius} км";
        }

        private void UpdateLocation()
        {
            try
            {
                using (var client = new System.Net.Http.HttpClient())
                {
                    var response = client.GetStringAsync("http://ipinfo.io/json").Result;
                    var json = JsonDocument.Parse(response);
                    var loc = json.RootElement.GetProperty("loc").GetString();
                    var parts = loc.Split(',');
                    if (parts.Length == 2)
                    {
                        currentLat = double.Parse(parts[0]);
                        currentLon = double.Parse(parts[1]);
                        statusLabel.Content = $"Геолокация обновлена: {currentLat}, {currentLon}";
                    }
                }
            }
            catch { statusLabel.Content = "Ошибка определения геолокации"; }
        }

        private void ExportData()
        {
            var dialog = new Microsoft.Win32.SaveFileDialog { Filter = "JSON (*.json)|*.json" };
            if (dialog.ShowDialog() == true)
            {
                string json = JsonSerializer.Serialize(notes, new JsonSerializerOptions { WriteIndented = true });
                File.WriteAllText(dialog.FileName, json);
                statusLabel.Content = $"Экспортировано в {dialog.FileName}";
            }
        }

        private void ImportData()
        {
            var dialog = new Microsoft.Win32.OpenFileDialog { Filter = "JSON (*.json)|*.json" };
            if (dialog.ShowDialog() == true)
            {
                string json = File.ReadAllText(dialog.FileName);
                var imported = JsonSerializer.Deserialize<List<Note>>(json);
                if (imported != null)
                {
                    foreach (var n in imported)
                    {
                        if (n.Id >= nextId) nextId = n.Id + 1;
                        notes.Add(n);
                    }
                    SaveData();
                    RefreshList();
                    statusLabel.Content = $"Импортировано из {dialog.FileName}";
                }
            }
        }

        private void RefreshList(List<Note> filtered = null)
        {
            listBox.Items.Clear();
            var display = filtered ?? notes;
            foreach (var n in display)
            {
                listBox.Items.Add($"#{n.Id}: {n.Text.Substring(0, Math.Min(n.Text.Length, 30))}... ({n.Lat:F5}, {n.Lon:F5})");
            }
        }

        private double Haversine(double lat1, double lon1, double lat2, double lon2)
        {
            const double R = 6371.0;
            double dLat = (lat2 - lat1) * Math.PI / 180;
            double dLon = (lon2 - lon1) * Math.PI / 180;
            double a = Math.Sin(dLat / 2) * Math.Sin(dLat / 2) +
                       Math.Cos(lat1 * Math.PI / 180) * Math.Cos(lat2 * Math.PI / 180) *
                       Math.Sin(dLon / 2) * Math.Sin(dLon / 2);
            double c = 2 * Math.Atan2(Math.Sqrt(a), Math.Sqrt(1 - a));
            return R * c;
        }

        private void LoadData()
        {
            if (File.Exists(dataFile))
            {
                string json = File.ReadAllText(dataFile);
                notes = JsonSerializer.Deserialize<List<Note>>(json) ?? new List<Note>();
                foreach (var n in notes) if (n.Id >= nextId) nextId = n.Id + 1;
            }
        }

        private void SaveData()
        {
            string json = JsonSerializer.Serialize(notes, new JsonSerializerOptions { WriteIndented = true });
            File.WriteAllText(dataFile, json);
        }

        public class Note
        {
            public int Id { get; set; }
            public string Text { get; set; }
            public double Lat { get; set; }
            public double Lon { get; set; }
            public string Category { get; set; }
            public long Created { get; set; }
        }

        public class InputDialog : Window
        {
            public string Answer { get; private set; }
            public InputDialog(string prompt)
            {
                var stack = new StackPanel();
                stack.Children.Add(new Label { Content = prompt });
                var box = new TextBox { Width = 300 };
                stack.Children.Add(box);
                var ok = new Button { Content = "OK", Width = 80, Margin = new Thickness(5) };
                ok.Click += (s, e) => { Answer = box.Text; DialogResult = true; Close(); };
                var cancel = new Button { Content = "Отмена", Width = 80 };
                cancel.Click += (s, e) => { DialogResult = false; Close(); };
                var btns = new StackPanel { Orientation = Orientation.Horizontal };
                btns.Children.Add(ok);
                btns.Children.Add(cancel);
                stack.Children.Add(btns);
                Content = stack;
                Width = 400;
                Height = 150;
            }
        }

        [STAThread]
        static void Main()
        {
            var app = new Application();
            app.Run(new MainWindow());
        }
    }
}
