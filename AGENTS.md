# AGENTS.md

## Cursor Cloud specific instructions

### Обзор проекта

Прошивка для ESP8266 (Wemos D1 Mini) — Wi-Fi управляемая розетка с веб-интерфейсом и расписанием. Это embedded-проект на Arduino framework; физическое оборудование не требуется для сборки и статического анализа.

### Сборка и проверка

Проект собирается через PlatformIO CLI. PATH к `pio` — `~/.local/bin`.

```bash
export PATH="$HOME/.local/bin:$PATH"

# Сборка прошивки (ESP8266 d1_mini)
pio run -e d1_mini

# Статический анализ (cppcheck) — только проектный код
pio check -e d1_mini --skip-packages

# Полный статический анализ (включая зависимости)
pio check -e d1_mini
```

### Важные замечания

- **Нет автотестов**: проект не содержит unit/integration тестов. Проверка корректности — через успешную компиляцию и cppcheck.
- **Нет runtime**: прошивка предназначена для физического ESP8266; запустить её в VM невозможно. «Hello world» для этого проекта — успешная сборка `firmware.bin`.
- **cppcheck ложные срабатывания**: предупреждения `unreadVariable` для переменной `g` в скетче — ложные; `sets::Group` использует RAII конструктор/деструктор для группировки элементов UI.
- **`platformio.ini`** в корне репозитория: создан для CLI-сборки. Файл `src_dir = WIFIPowerManager` указывает PlatformIO на каталог скетча.
- Зависимости (Settings, GyverNTP и их транзитивные зависимости) устанавливаются автоматически при первой сборке через PlatformIO Library Manager в `.pio/libdeps/`.
- Локальные копии библиотек в `libraries/` — только для справки (см. README).
