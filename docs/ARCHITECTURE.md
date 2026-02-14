# Архитектура проекта

Проект — приложение для просмотра и отрисовки диаграмм (узлы и связи). Состоит из приложения-оболочки и набора библиотек с чётким разделением слоёв.

---

## Общая схема

```
┌─────────────────────────────────────────────────────────────┐
│  Приложение (main)                                          │
│  Окно ImGui, загрузка JSON, виджет канваса                  │
└─────────────────────────────┬───────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────┐
│  canvas — канвас с pan/zoom и сеткой                        │
│  Координаты world↔screen, ввод мыши, вызов placement+render│
└──────────────┬──────────────────────────────┬───────────────┘
               │                              │
┌──────────────▼──────────────┐  ┌───────────▼────────────────┐
│  diagram_render              │  │  diagram_placement          │
│  Отрисовка через ImDrawList  │  │  Размещение узлов и рёбер  │
└──────────────┬───────────────┘  └───────────┬───────────────┘
               │                              │
               └──────────────┬───────────────┘
                              │
               ┌──────────────▼───────────────┐
               │  diagram_model               │
               │  Node, Edge, Diagram         │
               └──────────────┬───────────────┘
                              │
               ┌──────────────▼───────────────┐
               │  diagram_loaders              │
               │  Загрузка из JSON и др.       │
               └──────────────────────────────┘
```

Поток данных: **источник (например JSON)** → **diagram_loaders** → **Diagram** (модель) → **diagram_placement** → размещённая диаграмма → **diagram_render** → отрисовка на **canvas** (с учётом pan/zoom и сетки).

---

## Каталоги и библиотеки

| Каталог | Библиотека | Назначение |
|---------|------------|------------|
| `src/libs/diagram_model/` | diagram_model | Структуры данных диаграммы: Node, Edge, Diagram. Без зависимостей от UI и источников. |
| `src/libs/diagram_loaders/` | diagram_loaders | Получение Diagram из внешних источников. Первый источник — JSON (nlohmann/json). |
| `src/libs/diagram_placement/` | diagram_placement | Расчёт позиций и размеров узлов/рёбер на канвасе (мировые координаты). |
| `src/libs/diagram_render/` | diagram_render | Рисование размещённой диаграммы через ImGui DrawList. |
| `src/libs/canvas/` | canvas | Область просмотра: преобразование координат, pan, zoom, сетка, вызов placement и render. |
| `src/apps/main/` | main (exe) | Окно, загрузка диаграммы из файла, виджет канваса. |

---

## Слой 1: diagram_model

- **Типы:** только стандартная библиотека (C++20).
- **Файлы:** `include/diagram_model/types.hpp`, INTERFACE-библиотека в CMake.

**Структуры:**

- **Node** — узел: `id`, `label`, `x`, `y`, `width`, `height`, `shape` (Rectangle / Ellipse).
- **Edge** — связь: `id`, `source_node_id`, `target_node_id`, `label`.
- **Diagram** — контейнер: `nodes`, `edges`, `name`, `canvas_width`, `canvas_height`.

Модель не знает об отрисовке и форматах хранения.

---

## Слой 2: diagram_loaders

- **Зависимости:** diagram_model, nlohmann/json (FetchContent в thirdparty).
- **API:** `load_diagram_from_json(std::istream&)`, `load_diagram_from_json_file(path)` → `std::optional<Diagram>`.

Расширение: новые источники (другой формат, сеть) добавляются новыми функциями/модулями, возвращающими Diagram.

---

## Слой 3: diagram_placement

- **Зависимости:** только diagram_model.
- **Вход:** Diagram, опционально размер области.
- **Выход:** PlacedDiagram — для каждого узла прямоугольник (PlacedNode + Rect), для каждого ребра — полилиния точек (PlacedEdge).

**Типы:** `Rect`, `PlacedNode`, `PlacedEdge`, `PlacedDiagram`.  
**Функция:** `place_diagram(diagram, view_width, view_height)` — использует координаты из модели или раскладывает узлы без позиции (например, в столбец), рёбра — отрезки между центрами узлов.

---

## Слой 4: diagram_render

- **Зависимости:** diagram_placement, imgui_impl.
- **API:** `render_diagram(draw_list, placed_diagram, offset_x, offset_y, zoom)`.

Рисует в мировых координатах; внутри переводит world → screen через переданные offset и zoom. Узлы — прямоугольники или круги (Ellipse), подписи по центру; рёбра — линии по точкам полилинии.

---

## Слой 5: canvas

- **Зависимости:** diagram_render, diagram_placement, diagram_model, imgui_impl.
- **Класс:** `DiagramCanvas`.

**Ответственность:**

- Хранение состояния вида: `offset_x`, `offset_y`, `zoom`, шаг сетки.
- Преобразование координат: `screen_to_world`, `world_to_screen`.
- Ввод: перетаскивание (pan) левой кнопкой мыши, масштабирование колёсиком к точке под курсором.
- Отрисовка: сначала сетка в мировых координатах, затем placement текущей диаграммы и вызов diagram_render.

Виджет вызывается из приложения в цикле кадра: передаётся размер области, внутри — `update_and_draw(width, height)`.

---

## Приложение (main)

- Инициализация SDL3, OpenGL, ImGui.
- Загрузка диаграммы при старте из `data/example_diagram.json` (или `example_diagram.json`).
- Полноэкранное окно ImGui с дочерней областью `BeginChild("canvas")`; в ней создаётся `DiagramCanvas`, вызывается `set_diagram()` и каждый кадр — `update_and_draw()`.

Каталог `data/` копируется в выходной каталог сборки (POST_BUILD), чтобы при запуске из `_build/bin/Debug` файл `data/example_diagram.json` был доступен.

---

## Зависимости между целями CMake

```
main
 ├── canvas
 │    ├── diagram_render
 │    │    ├── diagram_placement
 │    │    │    └── diagram_model
 │    │    └── imgui_impl
 │    ├── diagram_placement
 │    ├── diagram_model
 │    └── imgui_impl
 ├── diagram_loaders
 │    ├── diagram_model
 │    └── nlohmann_json::nlohmann_json
 └── imgui_impl
```

---

## Формат JSON диаграммы (текущий)

```json
{
  "nodes": [
    { "id": "n1", "label": "Start", "x": 100, "y": 50, "width": 80, "height": 40 },
    { "id": "n2", "label": "End", "x": 100, "y": 150, "width": 80, "height": 40 }
  ],
  "edges": [
    { "id": "e1", "source": "n1", "target": "n2" }
  ]
}
```

Опционально: `name`, `canvas_width`, `canvas_height`; у узла — `shape` (`"rectangle"` / `"ellipse"`); у ребра — `label`.
