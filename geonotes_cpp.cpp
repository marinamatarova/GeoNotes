// geonotes_cpp.cpp — карта заметок на C++ (Qt + WebEngineView)

#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QListWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QUrl>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonParseError>
#include <QDir>

struct Note {
    int id;
    QString text;
    double lat;
    double lon;
    QString category;
    qint64 created;

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["text"] = text;
        obj["lat"] = lat;
        obj["lon"] = lon;
        obj["category"] = category;
        obj["created"] = created;
        return obj;
    }

    static Note fromJson(const QJsonObject &obj) {
        Note n;
        n.id = obj["id"].toInt();
        n.text = obj["text"].toString();
        n.lat = obj["lat"].toDouble();
        n.lon = obj["lon"].toDouble();
        n.category = obj["category"].toString();
        n.created = obj["created"].toInt64();
        return n;
    }
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("🗺️ GeoNotes — C++");
        resize(900, 700);
        loadData();
        createUI();
        refreshList();
    }

private slots:
    void addNote() {
        QString text = QInputDialog::getText(this, "Новая заметка", "Введите текст:");
        if (text.isEmpty()) return;
        // Используем текущую геолокацию (по умолчанию Москва)
        double lat = currentLat, lon = currentLon;
        Note n;
        n.id = nextId++;
        n.text = text;
        n.lat = lat;
        n.lon = lon;
        n.category = "";
        n.created = QDateTime::currentSecsSinceEpoch();
        notes.append(n);
        saveData();
        refreshList();
        statusLabel->setText("Добавлена заметка #" + QString::number(n.id));
    }

    void deleteNote() {
        int row = listWidget->currentRow();
        if (row < 0) return;
        if (QMessageBox::question(this, "Удалить", "Удалить заметку?") == QMessageBox::Yes) {
            notes.removeAt(row);
            saveData();
            refreshList();
            statusLabel->setText("Заметка удалена");
        }
    }

    void showMap() {
        if (notes.isEmpty()) {
            QMessageBox::information(this, "Информация", "Нет заметок");
            return;
        }
        // Создаем HTML с картой Leaflet
        QString html = "<!DOCTYPE html><html><head><meta charset='utf-8'/><title>GeoNotes Map</title>"
                       "<link rel='stylesheet' href='https://unpkg.com/leaflet@1.9.4/dist/leaflet.css'/>"
                       "<script src='https://unpkg.com/leaflet@1.9.4/dist/leaflet.js'></script>"
                       "<style>body { margin:0; } #map { height:100vh; }</style></head><body>"
                       "<div id='map'></div><script>"
                       "var map = L.map('map').setView([" +
                       QString::number(notes[0].lat) + ", " + QString::number(notes[0].lon) + "], 12);"
                       "L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png').addTo(map);";
        for (const Note &n : notes) {
            html += "L.marker([" + QString::number(n.lat) + ", " + QString::number(n.lon) + "])"
                    ".bindPopup('" + n.text + "<br><b>#" + QString::number(n.id) + "</b>').addTo(map);";
        }
        html += "</script></body></html>";
        // Сохраняем во временный файл
        QString mapFile = QDir::tempPath() + "/geonotes_map.html";
        QFile file(mapFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(html.toUtf8());
            file.close();
            webView->load(QUrl::fromLocalFile(mapFile));
            webView->show();
        }
        statusLabel->setText("Карта загружена");
    }

    void searchNotes() {
        QString query = searchEdit->text().trimmed().toLower();
        if (query.isEmpty()) {
            refreshList();
            return;
        }
        QList<Note> filtered;
        for (const Note &n : notes) {
            if (n.text.toLower().contains(query)) filtered.append(n);
        }
        refreshList(filtered);
        statusLabel->setText("Найдено " + QString::number(filtered.size()) + " заметок");
    }

    void searchNear() {
        bool ok;
        double radius = radiusEdit->text().toDouble(&ok);
        if (!ok || radius <= 0) {
            QMessageBox::warning(this, "Ошибка", "Введите корректный радиус");
            return;
        }
        // Используем текущие координаты (по умолчанию Москва)
        double lat = currentLat, lon = currentLon;
        QList<Note> filtered;
        for (const Note &n : notes) {
            double dist = haversine(lat, lon, n.lat, n.lon);
            if (dist <= radius) filtered.append(n);
        }
        refreshList(filtered);
        statusLabel->setText("Найдено " + QString::number(filtered.size()) + " заметок в радиусе " + QString::number(radius) + " км");
    }

    void updateLocation() {
        // Определяем через IP
        QNetworkAccessManager *manager = new QNetworkAccessManager(this);
        connect(manager, &QNetworkAccessManager::finished, this, &MainWindow::onLocationReply);
        manager->get(QNetworkRequest(QUrl("http://ipinfo.io/json")));
    }

    void onLocationReply(QNetworkReply *reply) {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject obj = doc.object();
            QString loc = obj["loc"].toString();
            QStringList parts = loc.split(',');
            if (parts.size() == 2) {
                currentLat = parts[0].toDouble();
                currentLon = parts[1].toDouble();
                statusLabel->setText("Геолокация обновлена: " + loc);
            }
        } else {
            statusLabel->setText("Ошибка определения геолокации");
        }
        reply->deleteLater();
    }

    void exportData() {
        QString filename = QFileDialog::getSaveFileName(this, "Экспорт JSON", "", "JSON (*.json)");
        if (filename.isEmpty()) return;
        QJsonArray arr;
        for (const Note &n : notes) arr.append(n.toJson());
        QJsonDocument doc(arr);
        QFile file(filename);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(doc.toJson());
            statusLabel->setText("Экспортировано в " + filename);
        }
    }

    void importData() {
        QString filename = QFileDialog::getOpenFileName(this, "Импорт JSON", "", "JSON (*.json)");
        if (filename.isEmpty()) return;
        QFile file(filename);
        if (!file.open(QIODevice::ReadOnly)) return;
        QByteArray data = file.readAll();
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError) {
            QMessageBox::warning(this, "Ошибка", "Неверный JSON");
            return;
        }
        QJsonArray arr = doc.array();
        for (const QJsonValue &v : arr) {
            QJsonObject obj = v.toObject();
            Note n = Note::fromJson(obj);
            if (n.id >= nextId) nextId = n.id + 1;
            notes.append(n);
        }
        saveData();
        refreshList();
        statusLabel->setText("Импортировано из " + filename);
    }

private:
    QList<Note> notes;
    int nextId = 1;
    double currentLat = 55.7558, currentLon = 37.6173;
    QListWidget *listWidget;
    QTextEdit *textEdit;
    QLineEdit *searchEdit, *radiusEdit;
    QWebEngineView *webView;
    QLabel *statusLabel;

    void createUI() {
        QWidget *central = new QWidget(this);
        setCentralWidget(central);
        QVBoxLayout *mainLayout = new QVBoxLayout(central);

        // Верхняя панель
        QHBoxLayout *topLayout = new QHBoxLayout();
        QPushButton *addBtn = new QPushButton("Добавить");
        QPushButton *delBtn = new QPushButton("Удалить");
        QPushButton *mapBtn = new QPushButton("Показать карту");
        QPushButton *exportBtn = new QPushButton("Экспорт");
        QPushButton *importBtn = new QPushButton("Импорт");
        QPushButton *locBtn = new QPushButton("Обновить геолокацию");
        topLayout->addWidget(addBtn);
        topLayout->addWidget(delBtn);
        topLayout->addWidget(mapBtn);
        topLayout->addWidget(exportBtn);
        topLayout->addWidget(importBtn);
        topLayout->addWidget(locBtn);
        mainLayout->addLayout(topLayout);

        // Поиск
        QHBoxLayout *searchLayout = new QHBoxLayout();
        searchLayout->addWidget(new QLabel("Поиск:"));
        searchEdit = new QLineEdit();
        searchLayout->addWidget(searchEdit);
        QPushButton *searchBtn = new QPushButton("Найти");
        searchLayout->addWidget(searchBtn);
        searchLayout->addWidget(new QLabel("Радиус (км):"));
        radiusEdit = new QLineEdit("5");
        radiusEdit->setFixedWidth(60);
        searchLayout->addWidget(radiusEdit);
        QPushButton *nearBtn = new QPushButton("Найти рядом");
        searchLayout->addWidget(nearBtn);
        mainLayout->addLayout(searchLayout);

        // Список и текст
        QHBoxLayout *centerLayout = new QHBoxLayout();
        listWidget = new QListWidget();
        centerLayout->addWidget(listWidget, 1);
        textEdit = new QTextEdit();
        textEdit->setReadOnly(true);
        centerLayout->addWidget(textEdit, 1);
        mainLayout->addLayout(centerLayout);

        // Карта (скрыта по умолчанию)
        webView = new QWebEngineView(this);
        webView->setVisible(false);
        mainLayout->addWidget(webView, 2);

        // Статус
        statusLabel = new QLabel("Готов");
        mainLayout->addWidget(statusLabel);

        connect(addBtn, &QPushButton::clicked, this, &MainWindow::addNote);
        connect(delBtn, &QPushButton::clicked, this, &MainWindow::deleteNote);
        connect(mapBtn, &QPushButton::clicked, this, &MainWindow::showMap);
        connect(exportBtn, &QPushButton::clicked, this, &MainWindow::exportData);
        connect(importBtn, &QPushButton::clicked, this, &MainWindow::importData);
        connect(locBtn, &QPushButton::clicked, this, &MainWindow::updateLocation);
        connect(searchBtn, &QPushButton::clicked, this, &MainWindow::searchNotes);
        connect(nearBtn, &QPushButton::clicked, this, &MainWindow::searchNear);
        connect(listWidget, &QListWidget::currentRowChanged, this, &MainWindow::onSelectNote);
    }

    void onSelectNote(int row) {
        if (row < 0 || row >= notes.size()) return;
        const Note &n = notes[row];
        textEdit->setText(n.text + "\n\nКоординаты: " + QString::number(n.lat, 'f', 5) + ", " + QString::number(n.lon, 'f', 5));
        statusLabel->setText("Заметка #" + QString::number(n.id));
    }

    void refreshList(const QList<Note> &filtered = QList<Note>()) {
        listWidget->clear();
        const QList<Note> &display = filtered.isEmpty() ? notes : filtered;
        for (const Note &n : display) {
            QString item = QString("#%1: %2... (%3, %4)")
                           .arg(n.id)
                           .arg(n.text.left(30))
                           .arg(n.lat, 0, 'f', 5)
                           .arg(n.lon, 0, 'f', 5);
            listWidget->addItem(item);
        }
    }

    double haversine(double lat1, double lon1, double lat2, double lon2) {
        const double R = 6371.0;
        double dLat = (lat2 - lat1) * M_PI / 180.0;
        double dLon = (lon2 - lon1) * M_PI / 180.0;
        double a = sin(dLat/2) * sin(dLat/2) +
                   cos(lat1 * M_PI / 180.0) * cos(lat2 * M_PI / 180.0) *
                   sin(dLon/2) * sin(dLon/2);
        double c = 2 * atan2(sqrt(a), sqrt(1-a));
        return R * c;
    }

    void loadData() {
        QFile file("notes.json");
        if (!file.open(QIODevice::ReadOnly)) return;
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isArray()) return;
        QJsonArray arr = doc.array();
        for (const QJsonValue &v : arr) {
            QJsonObject obj = v.toObject();
            Note n = Note::fromJson(obj);
            if (n.id >= nextId) nextId = n.id + 1;
            notes.append(n);
        }
    }

    void saveData() {
        QJsonArray arr;
        for (const Note &n : notes) arr.append(n.toJson());
        QJsonDocument doc(arr);
        QFile file("notes.json");
        if (file.open(QIODevice::WriteOnly)) {
            file.write(doc.toJson());
        }
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    return app.exec();
}

#include "geonotes_cpp.moc"
