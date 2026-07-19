// geonotes_java.java — карта заметок на Java (Swing + открытие браузера)

import javax.swing.*;
import javax.swing.event.*;
import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.List;
import com.google.gson.*;
import java.net.URI;
import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;

public class GeoNotesJava extends JFrame {
    private static final String DATA_FILE = "notes.json";
    private List<Note> notes = new ArrayList<>();
    private int nextId = 1;
    private double currentLat = 55.7558, currentLon = 37.6173;
    private DefaultListModel<String> listModel;
    private JList<String> list;
    private JTextArea textArea;
    private JTextField searchField, radiusField;
    private JLabel statusLabel;

    public GeoNotesJava() {
        setTitle("🗺️ GeoNotes — Java");
        setSize(900, 650);
        setDefaultCloseOperation(EXIT_ON_CLOSE);
        setLayout(new BorderLayout());
        loadData();
        createUI();
        refreshList();
    }

    private void createUI() {
        // Верхняя панель
        JPanel top = new JPanel(new FlowLayout());
        JButton addBtn = new JButton("Добавить");
        JButton delBtn = new JButton("Удалить");
        JButton mapBtn = new JButton("Показать карту");
        JButton exportBtn = new JButton("Экспорт");
        JButton importBtn = new JButton("Импорт");
        JButton locBtn = new JButton("Обновить геолокацию");
        top.add(addBtn);
        top.add(delBtn);
        top.add(mapBtn);
        top.add(exportBtn);
        top.add(importBtn);
        top.add(locBtn);
        add(top, BorderLayout.NORTH);

        // Поиск
        JPanel searchPanel = new JPanel(new FlowLayout());
        searchPanel.add(new JLabel("Поиск:"));
        searchField = new JTextField(20);
        searchPanel.add(searchField);
        JButton searchBtn = new JButton("Найти");
        searchPanel.add(searchBtn);
        searchPanel.add(new JLabel("Радиус (км):"));
        radiusField = new JTextField("5", 6);
        searchPanel.add(radiusField);
        JButton nearBtn = new JButton("Найти рядом");
        searchPanel.add(nearBtn);
        add(searchPanel, BorderLayout.NORTH); // второе место

        // Список и текст
        JSplitPane split = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT);
        listModel = new DefaultListModel<>();
        list = new JList<>(listModel);
        list.addListSelectionListener(e -> {
            if (!e.getValueIsAdjusting()) onSelect();
        });
        split.setLeftComponent(new JScrollPane(list));
        textArea = new JTextArea();
        textArea.setEditable(false);
        split.setRightComponent(new JScrollPane(textArea));
        add(split, BorderLayout.CENTER);

        // Статус
        statusLabel = new JLabel("Готов");
        add(statusLabel, BorderLayout.SOUTH);

        // Обработчики
        addBtn.addActionListener(e -> addNote());
        delBtn.addActionListener(e -> deleteNote());
        mapBtn.addActionListener(e -> showMap());
        exportBtn.addActionListener(e -> exportData());
        importBtn.addActionListener(e -> importData());
        locBtn.addActionListener(e -> updateLocation());
        searchBtn.addActionListener(e -> searchNotes());
        nearBtn.addActionListener(e -> searchNear());

        // Горячие клавиши
        getRootPane().registerKeyboardAction(e -> addNote(),
                KeyStroke.getKeyStroke(KeyEvent.VK_N, InputEvent.CTRL_MASK), JComponent.WHEN_IN_FOCUSED_WINDOW);
        getRootPane().registerKeyboardAction(e -> deleteNote(),
                KeyStroke.getKeyStroke(KeyEvent.VK_DELETE, 0), JComponent.WHEN_IN_FOCUSED_WINDOW);
    }

    private void addNote() {
        String text = JOptionPane.showInputDialog(this, "Введите текст заметки:");
        if (text == null || text.isEmpty()) return;
        // Используем текущие координаты
        Note n = new Note(nextId++, text, currentLat, currentLon);
        notes.add(n);
        saveData();
        refreshList();
        statusLabel.setText("Добавлена заметка #" + n.id);
    }

    private void deleteNote() {
        int idx = list.getSelectedIndex();
        if (idx < 0) return;
        if (JOptionPane.showConfirmDialog(this, "Удалить заметку?", "Подтверждение", JOptionPane.YES_NO_OPTION) == JOptionPane.YES_OPTION) {
            notes.remove(idx);
            saveData();
            refreshList();
            statusLabel.setText("Заметка удалена");
        }
    }

    private void onSelect() {
        int idx = list.getSelectedIndex();
        if (idx >= 0 && idx < notes.size()) {
            Note n = notes.get(idx);
            textArea.setText(n.text + "\n\nКоординаты: " + n.lat + ", " + n.lon);
            statusLabel.setText("Заметка #" + n.id);
        }
    }

    private void showMap() {
        if (notes.isEmpty()) {
            JOptionPane.showMessageDialog(this, "Нет заметок");
            return;
        }
        try {
            // Строим URL для OpenStreetMap с маркерами
            StringBuilder url = new StringBuilder("https://www.openstreetmap.org/?mlat=" + notes.get(0).lat + "&mlon=" + notes.get(0).lon + "&zoom=12");
            // Можно добавить параметры для маркеров, но OSM не поддерживает несколько маркеров через URL.
            // Вместо этого создадим HTML-страницу с Leaflet, как в других версиях.
            // Для простоты — откроем карту с центром на первой заметке.
            Desktop.getDesktop().browse(new URI(url.toString()));
        } catch (Exception e) {
            JOptionPane.showMessageDialog(this, "Ошибка открытия карты: " + e.getMessage());
        }
    }

    private void searchNotes() {
        String query = searchField.getText().trim().toLowerCase();
        if (query.isEmpty()) {
            refreshList();
            return;
        }
        List<Note> filtered = new ArrayList<>();
        for (Note n : notes) {
            if (n.text.toLowerCase().contains(query)) filtered.add(n);
        }
        refreshList(filtered);
        statusLabel.setText("Найдено " + filtered.size() + " заметок");
    }

    private void searchNear() {
        double radius;
        try {
            radius = Double.parseDouble(radiusField.getText().trim());
        } catch (NumberFormatException e) {
            JOptionPane.showMessageDialog(this, "Введите корректный радиус");
            return;
        }
        double lat = currentLat, lon = currentLon;
        List<Note> filtered = new ArrayList<>();
        for (Note n : notes) {
            double dist = haversine(lat, lon, n.lat, n.lon);
            if (dist <= radius) filtered.add(n);
        }
        refreshList(filtered);
        statusLabel.setText("Найдено " + filtered.size() + " заметок в радиусе " + radius + " км");
    }

    private void updateLocation() {
        try {
            // Используем ipinfo.io
            java.net.http.HttpClient client = java.net.http.HttpClient.newHttpClient();
            java.net.http.HttpRequest request = java.net.http.HttpRequest.newBuilder()
                    .uri(URI.create("http://ipinfo.io/json"))
                    .build();
            java.net.http.HttpResponse<String> response = client.send(request, java.net.http.HttpResponse.BodyHandlers.ofString());
            JsonObject obj = JsonParser.parseString(response.body()).getAsJsonObject();
            String loc = obj.get("loc").getAsString();
            String[] parts = loc.split(",");
            if (parts.length == 2) {
                currentLat = Double.parseDouble(parts[0]);
                currentLon = Double.parseDouble(parts[1]);
                statusLabel.setText("Геолокация обновлена: " + currentLat + ", " + currentLon);
            }
        } catch (Exception e) {
            statusLabel.setText("Ошибка определения геолокации");
        }
    }

    private void exportData() {
        JFileChooser chooser = new JFileChooser();
        if (chooser.showSaveDialog(this) == JFileChooser.APPROVE_OPTION) {
            File file = chooser.getSelectedFile();
            try (PrintWriter pw = new PrintWriter(file)) {
                Gson gson = new GsonBuilder().setPrettyPrinting().create();
                pw.write(gson.toJson(notes));
                statusLabel.setText("Экспортировано в " + file.getName());
            } catch (IOException e) {
                JOptionPane.showMessageDialog(this, "Ошибка экспорта");
            }
        }
    }

    private void importData() {
        JFileChooser chooser = new JFileChooser();
        if (chooser.showOpenDialog(this) == JFileChooser.APPROVE_OPTION) {
            File file = chooser.getSelectedFile();
            try (Reader reader = new FileReader(file)) {
                Gson gson = new Gson();
                Note[] arr = gson.fromJson(reader, Note[].class);
                for (Note n : arr) {
                    if (n.id >= nextId) nextId = n.id + 1;
                    notes.add(n);
                }
                saveData();
                refreshList();
                statusLabel.setText("Импортировано из " + file.getName());
            } catch (Exception e) {
                JOptionPane.showMessageDialog(this, "Ошибка импорта");
            }
        }
    }

    private void refreshList(List<Note> filtered) {
        listModel.clear();
        List<Note> display = filtered != null ? filtered : notes;
        for (Note n : display) {
            String s = String.format("#%d: %s... (%.5f, %.5f)", n.id, n.text.substring(0, Math.min(n.text.length(), 30)), n.lat, n.lon);
            listModel.addElement(s);
        }
    }

    private void refreshList() {
        refreshList(null);
    }

    private double haversine(double lat1, double lon1, double lat2, double lon2) {
        final double R = 6371.0;
        double dLat = Math.toRadians(lat2 - lat1);
        double dLon = Math.toRadians(lon2 - lon1);
        double a = Math.sin(dLat/2) * Math.sin(dLat/2) +
                   Math.cos(Math.toRadians(lat1)) * Math.cos(Math.toRadians(lat2)) *
                   Math.sin(dLon/2) * Math.sin(dLon/2);
        double c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1-a));
        return R * c;
    }

    private void loadData() {
        File file = new File(DATA_FILE);
        if (!file.exists()) return;
        try (Reader reader = new FileReader(file)) {
            Gson gson = new Gson();
            Note[] arr = gson.fromJson(reader, Note[].class);
            for (Note n : arr) {
                if (n.id >= nextId) nextId = n.id + 1;
                notes.add(n);
            }
        } catch (Exception e) { /* ignore */ }
    }

    private void saveData() {
        try (PrintWriter pw = new PrintWriter(new File(DATA_FILE))) {
            Gson gson = new GsonBuilder().setPrettyPrinting().create();
            pw.write(gson.toJson(notes));
        } catch (IOException e) { /* ignore */ }
    }

    static class Note {
        int id;
        String text;
        double lat, lon;
        String category;
        long created;

        Note(int id, String text, double lat, double lon) {
            this.id = id;
            this.text = text;
            this.lat = lat;
            this.lon = lon;
            this.category = "";
            this.created = System.currentTimeMillis();
        }
    }

    public static void main(String[] args) throws Exception {
        UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
        SwingUtilities.invokeLater(() -> new GeoNotesJava().setVisible(true));
    }
}
