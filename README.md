# ImGui + SDL3 + OpenGL3

Минимальное приложение на C++20 с Dear ImGui, SDL3 и OpenGL 3 для отрисовки интерфейса. Сборка через CMake, зависимости подтягиваются через FetchContent.

---

## Требования

- **CMake** 3.20+
- **Компилятор** с поддержкой C++20 (на Windows — Visual Studio 2022, генератор "Visual Studio 17 2022", x64)
- **OpenGL** (драйвер и заголовки; на Windows — через системный OpenGL)

---

## Сборка

### Windows (Visual Studio)

```batch
build_vs.bat
```

- Генерация: `cmake -S . -B _build -G "Visual Studio 17 2022" -A x64`
- Сборка: `cmake --build _build --config Debug`
- Исполняемый файл: `_build\bin\Debug\main.exe` (или `_build\bin\Release\main.exe` для Release)

### Другая конфигурация

```bash
cmake -S . -B _build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build _build
```

Бинарник: `_build/bin/main` (или `main.exe` на Windows).

---

## Структура проекта

```
├── CMakeLists.txt          # Корневой CMake, C++20, выходы в _build
├── build_vs.bat            # Сборка под Visual Studio (каталог _build)
├── data/                   # Примеры данных (копируются в bin при сборке)
│   └── example_diagram.json
├── docs/
│   └── ARCHITECTURE.md     # Описание архитектуры и слоёв (см. ниже)
├── thirdparty/
│   └── CMakeLists.txt      # FetchContent: SDL3, ImGui, nlohmann/json; imgui_impl
├── src/
│   ├── apps/
│   │   └── main/           # Приложение просмотра диаграмм (main.cpp)
│   ├── libs/               # Библиотеки диаграмм
│   │   ├── diagram_model/  # Структуры Node, Edge, Diagram
│   │   ├── diagram_loaders/# Загрузка из JSON и др.
│   │   ├── diagram_placement/ # Размещение узлов и рёбер на канвасе
│   │   ├── diagram_render/ # Отрисовка через ImDrawList
│   │   └── canvas/         # Канвас: pan, zoom, сетка
│   └── tests/              # Тесты (enable_testing + add_subdirectory)
└── _build/                 # Каталог сборки (создаётся при конфигурации)
```

- **Третьесторонние библиотеки** — в `thirdparty/`; подтягиваются CMake (FetchContent).
- **Приложение** — в `src/apps/main/`; библиотеки диаграмм — в `src/libs/`, тесты — в `src/tests/`.
- **Архитектура** — подробное описание слоёв, зависимостей и потока данных см. в [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

---

## Зависимости (FetchContent)

Все зависимости загружаются при первом запуске CMake в `_build/_deps/`.

| Библиотека | Репозиторий | Версия/тег | Как используется |
|------------|-------------|------------|-------------------|
| **SDL3** | libsdl-org/SDL | `release-3.2.30` | Окно, события, OpenGL-контекст; статическая сборка (`SDL_STATIC ON`). |
| **ImGui** | ocornut/imgui | ветка `docking` | Ядро + бэкенды SDL3 и OpenGL3; статическая библиотека `imgui_impl`. |
| **nlohmann/json** | nlohmann/json | v3.11.3 | Парсинг JSON при загрузке диаграмм (`diagram_loaders`). |

У ImGui нет корневого `CMakeLists.txt`, поэтому используется `FetchContent_Populate(imgui)` и свой целевой статический таргет `imgui_impl` с нужными исходниками и путями включения.

---

## Технические особенности приложения

### Статическая линковка SDL3 на Windows

- Точка входа — свой `main()`, не SDL_main.
- Перед любыми включениями SDL задаётся **`SDL_MAIN_HANDLED`**.
- Подключается **`<SDL3/SDL_main.h>`** и перед `SDL_Init()` вызывается **`SDL_SetMainReady()`**.
- В SDL3 **`SDL_Init()`** при успехе возвращает **`true`**, при ошибке — **`false`** (в отличие от SDL2).

### Окно и OpenGL

- Окно создаётся с флагами `SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY`.
- **HiDPI:** флаг `SDL_WINDOW_HIGH_PIXEL_DENSITY` запрашивает буфер с учётом плотности пикселей (на HiDPI-мониторах — больше пикселей при тех же логических размерах).
- Контекст OpenGL: Core Profile 3.0 (`SDL_GL_CONTEXT_MAJOR_VERSION 3`, `SDL_GL_CONTEXT_PROFILE_CORE`).
- Рендер: ImGui рисуется через бэкенд OpenGL3, в конце кадра — `SDL_GL_SwapWindow()`.

### HiDPI и масштабирование

- **ImGui:** включены `ImGuiConfigFlags_DpiEnableScaleFonts` и `ImGuiConfigFlags_DpiEnableScaleViewports` — масштабирование шрифтов и вьюпортов по DPI.
- Бэкенд ImGui для SDL3 выставляет `io.DisplaySize` и `io.DisplayFramebufferScale` (из `SDL_GetWindowSizeInPixels` и масштаба окна).
- **Viewport OpenGL** задаётся в пикселях: `fb_w = DisplaySize.x * DisplayFramebufferScale.x`, `fb_h = DisplaySize.y * DisplayFramebufferScale.y`, затем `glViewport(0, 0, fb_w, fb_h)`.

### Шрифты

- Вместо встроенного пиксельного шрифта ImGui загружается системный TTF:
  - **Windows:** по очереди пробуются `C:\Windows\Fonts\segoeui.ttf` (Segoe UI), `seguiui.ttf`, `arial.ttf`.
  - **Linux:** DejaVu Sans / Liberation Sans в типичных путях (`/usr/share/fonts/...`).
- Параметры загрузки: размер 19 px, `OversampleH/OversampleV = 2`, `PixelSnapH = true` для более чёткого и читаемого текста.

### События и выход

- Обработка событий через `SDL_PollEvent`; все события передаются в `ImGui_ImplSDL3_ProcessEvent`.
- Выход при `SDL_EVENT_QUIT` или `SDL_EVENT_WINDOW_CLOSE_REQUESTED` для текущего окна (сравнение по `SDL_GetWindowID`).

---

## Выходы сборки

- **Библиотеки (статические):** `_build/lib/<Config>/` (например `SDL3-static.lib`, `imgui_impl.lib`).
- **Исполняемые файлы:** `_build/bin/<Config>/` (например `main.exe` в Debug/Release).

Для мультиконфигурационных генераторов (Visual Studio) подставьте `Debug` или `Release` вместо `<Config>`.

---

## Тесты

- В корневом `CMakeLists.txt` вызываются `enable_testing()` и `add_subdirectory(src/tests)`.
- В `src/tests/CMakeLists.txt` можно добавлять цели тестов и `add_test(...)`.

---

## Стиль и документация

- Код на **C++**, бэкенд нативный (C++20).
- Сложная логика помечается комментариями в коде.
