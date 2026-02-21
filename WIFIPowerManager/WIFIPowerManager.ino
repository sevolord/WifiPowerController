// WIFI Power Manager — управляемая розетка (Wemos Mini + реле 220V)
// Веб-интерфейс и расписание через Settings, время через GyverNTP, настройки в LittleFS (GyverDB)

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <GyverDBFile.h>
#include <SettingsGyver.h>
#include <GyverNTP.h>

// ========== Константы ==========
const int RELAY_PIN = 5;           // GPIO 5 — реле (Wemos)
const int GMT_OFFSET = 3;          // Часовой пояс (часы), например 3 для Москвы
const char* AP_NAME = "WifiPowerManager";

// Время по умолчанию: 08:00 и 22:00 (секунды от полуночи)
const uint32_t DEFAULT_ON_SEC  = 8u * 3600u;
const uint32_t DEFAULT_OFF_SEC = 22u * 3600u;

// ========== База данных и Settings ==========
GyverDBFile db(&LittleFS, "/data.db");
SettingsGyver sett("WIFI Power Manager", &db);

DB_KEYS(
    kk,
    wifi_ssid,
    wifi_pass,
    wifi_restart,
    relay_state,
    tpl_on,
    tpl_off,
    apply_all,
    on_0, on_1, on_2, on_3, on_4, on_5, on_6,
    off_0, off_1, off_2, off_3, off_4, off_5, off_6
);

// Состояние реле (синхронизируется с пином и БД по id kk::relay_state)
bool relayState = false;

// Имена дней для UI (Пн..Вс)
static const char* DAY_NAMES[] = { "Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс" };

// Ключи расписания по дням (Пн=0 .. Вс=6)
static const size_t ON_KEYS[]  = { kk::on_0, kk::on_1, kk::on_2, kk::on_3, kk::on_4, kk::on_5, kk::on_6 };
static const size_t OFF_KEYS[] = { kk::off_0, kk::off_1, kk::off_2, kk::off_3, kk::off_4, kk::off_5, kk::off_6 };

// Последняя минута, в которую уже применили расписание (не дергать реле каждую секунду)
static int lastScheduleMinute = -1;

// Чтение времени из БД как uint32_t (секунды от полуночи)
static uint32_t getDbTimeSec(size_t key) {
    return (uint32_t)db[key].toInt();
}

// Проверка расписания и переключение реле по времени (вызывать при NTP.tick())
static void checkSchedule() {
    if (!NTP.online()) return;

    int h = NTP.hour();
    int m = NTP.minute();
    int currentMinute = h * 60 + m;
    if (currentMinute == lastScheduleMinute) return;
    lastScheduleMinute = currentMinute;

    Datime dt = NTP;
    int wd = (int)dt.weekDay();  // 1..7 Пн..Вс
    if (wd < 1 || wd > 7) return;
    int dayIndex = wd - 1;       // 0..6

    uint32_t currentSec = (uint32_t)(h * 3600 + m * 60 + NTP.second());
    uint32_t onSec  = getDbTimeSec(ON_KEYS[dayIndex]);
    uint32_t offSec = getDbTimeSec(OFF_KEYS[dayIndex]);

    if (currentSec == onSec) {
        relayState = true;
        digitalWrite(RELAY_PIN, HIGH);
        db[kk::relay_state] = relayState;
        db.update();
    } else if (currentSec == offSec) {
        relayState = false;
        digitalWrite(RELAY_PIN, LOW);
        db[kk::relay_state] = relayState;
        db.update();
    }
}

void build(sets::Builder& b) {
    // ----- Wi‑Fi -----
    {
        sets::Group g(b, "Wi‑Fi");
        b.Input(kk::wifi_ssid, "SSID");
        b.Pass(kk::wifi_pass, "Пароль");
        if (b.Button(kk::wifi_restart, "Сохранить и перезагрузить")) {
            db.update();
            ESP.restart();
        }
    }

    // ----- Реле -----
    {
        sets::Group g(b, "Реле");
        b.Label("Состояние", relayState ? "Вкл" : "Выкл");
        if (b.Switch(kk::relay_state, "Вкл/Выкл", &relayState)) {
            digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
            db[kk::relay_state] = relayState;
            db.update();
        }
    }

    // ----- Текущее время NTP -----
    {
        sets::Group g(b, "Время");
        b.Label("Сейчас (NTP)", NTP.online() ? NTP.toString() : F("— синхронизация..."));
    }

    // ----- Шаблон для всех дней -----
    {
        sets::Group g(b, "Шаблон для всех дней");
        b.Time(kk::tpl_on,  "Включить", nullptr);
        b.Time(kk::tpl_off, "Выключить", nullptr);
        if (b.Button(kk::apply_all, "Применить ко всем дням")) {
            uint32_t tplOn  = getDbTimeSec(kk::tpl_on);
            uint32_t tplOff = getDbTimeSec(kk::tpl_off);
            for (int d = 0; d < 7; d++) {
                db[ON_KEYS[d]]  = (int)tplOn;
                db[OFF_KEYS[d]] = (int)tplOff;
            }
            db.update();
        }
    }

    // ----- Расписание по дням -----
    {
        sets::Group g(b, "Расписание по дням");
        for (int d = 0; d < 7; d++) {
            b.Time(ON_KEYS[d],  String(DAY_NAMES[d]) + " вкл",  nullptr);
            b.Time(OFF_KEYS[d], String(DAY_NAMES[d]) + " выкл", nullptr);
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println(F("WIFI Power Manager"));

    pinMode(RELAY_PIN, OUTPUT);
    relayState = false;
    digitalWrite(RELAY_PIN, LOW);

#ifdef ESP32
    LittleFS.begin(true);
#else
    LittleFS.begin();
#endif
    db.begin();
    db.init(kk::wifi_ssid, "");
    db.init(kk::wifi_pass, "");
    db.init(kk::relay_state, false);
    db.init(kk::tpl_on,  (int)DEFAULT_ON_SEC);
    db.init(kk::tpl_off, (int)DEFAULT_OFF_SEC);
    for (int d = 0; d < 7; d++) {
        db.init(ON_KEYS[d],  (int)DEFAULT_ON_SEC);
        db.init(OFF_KEYS[d], (int)DEFAULT_OFF_SEC);
    }

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_NAME);
    Serial.print(F("AP: "));
    Serial.println(WiFi.softAPIP());

    String ssid = db[kk::wifi_ssid];
    String pass = db[kk::wifi_pass];
    if (ssid.length()) {
        WiFi.begin(ssid.c_str(), pass.c_str());
        Serial.print(F("STA "));
        int tries = 25;
        while (WiFi.status() != WL_CONNECTED && tries--) {
            delay(500);
            Serial.print('.');
        }
        Serial.println();
        if (WiFi.status() == WL_CONNECTED) {
            Serial.print(F("IP: "));
            Serial.println(WiFi.localIP());
        }
    }

    relayState = (bool)db[kk::relay_state];
    digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);

    NTP.begin(GMT_OFFSET);
    sett.begin();
    sett.onBuild(build);
}

void loop() {
    if (NTP.tick()) {
        checkSchedule();
    }
    sett.tick();
}
