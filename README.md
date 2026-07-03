# Programación III: UTEC STREAMING
### Proyecto #1

---

## Integrantes

| Nombre y Apellidos | Código    |
|--------------------|-----------|
| Jasyr Valdez       | 202510474 |
| Andre Sanchez      | 202510063 |
---

## Tabla de Contenidos

1. [Pre-procesamiento de los datos](#1-pre-procesamiento-de-los-datos)
2. [Pseudo-código de Inserción (Suffix Trie)](#2-pseudo-código-de-inserción-suffix-trie)
3. [Estructura de Datos: Suffix Trie](#3-estructura-de-datos-suffix-trie)
4. [Programación Genérica Avanzada](#4-programación-genérica-avanzada)
5. [Programación Paralela y Benchmark](#5-programación-paralela-y-benchmark)
6. [Patrones de Diseño Implementados (5)](#6-patrones-de-diseño-implementados-5)
7. [Avance de la Interfaz del Programa](#7-avance-de-la-interfaz-del-programa)
8. [Especificaciones Técnicas](#8-especificaciones-técnicas)
9. [Compilación y Ejecución](#9-compilación-y-ejecución)
10. [Bibliografía](#10-bibliografía)

---

## 1. Pre-procesamiento de los datos

El sistema utiliza un motor de carga diseñado para transformar la base de datos bruta (`wiki_movie_plots_deduped.csv`) en una estructura de búsqueda eficiente.

### Lectura del CSV — Parser de Máquina de Estados

Debido a que el dataset de Wikipedia contiene sinopsis con comas internas y saltos de línea dentro de las celdas, se implementó un parser basado en máquina de estados con las siguientes características:

- **Atributos extraídos:** Año de lanzamiento, Título, Director, Casting, Género y Sinopsis.
- **Gestión de comillas:** El algoritmo detecta si una coma es un separador de columna o parte del texto (cuando está dentro de `"..."`), garantizando que la información no se desplace entre campos.
- **Soporte multilínea:** Si una sinopsis se extiende por varias líneas en el archivo físico, el programa concatena el registro hasta cerrar las comillas antes de procesarlo.

### Limpieza de Texto

Cada campo de texto pasa por una función de normalización antes de ser indexado:

1. **Normalización:** Se utiliza `std::tolower` para convertir todo a minúsculas.
2. **Filtrado:** Mediante `isalnum` y validaciones de caracteres, se eliminan signos de puntuación, caracteres especiales y números que no aportan valor semántico.
3. **Tokenización:** El texto limpio se fragmenta en palabras individuales para su inserción en la estructura de datos.

**Ejemplo de transformación:**

```
Entrada bruta  →  "A bartender is working at a saloon, serving drinks..."
Normalizado    →  a bartender is working at a saloon serving drinks
Tokens         →  [a] [bartender] [is] [working] [at] [saloon] [serving] [drinks]
```

---

## 2. Pseudo-código de Inserción (Suffix Trie)

La lógica de inserción permite búsquedas de sub-palabras (por ejemplo, encontrar `"bar"` dentro de `"barco"`). Por cada palabra se insertan todos sus sufijos.

```text
ALGORITMO DE INSERCIÓN DE SUFIJOS
Entrada: 'palabra_limpia' (String), 'puntero_pelicula' (Referencia)

1. Por cada 'i' desde 0 hasta longitud(palabra_limpia) - 1:
2.     'sufijo' = subcadena de 'palabra_limpia' desde 'i' hasta el final
3.     'nodo_actual' = RAIZ del Arbol
4.     Para cada 'caracter' en 'sufijo':
5.         Si 'caracter' no existe en hijos de 'nodo_actual':
6.             Crear NUEVO NODO para 'caracter'
7.         Avanzar 'nodo_actual' al hijo correspondiente
8.         Añadir 'puntero_pelicula' al conjunto de resultados del 'nodo_actual'
9. Fin Para
```

---

## 3. Estructura de Datos: Suffix Trie

Se implementó un **Suffix Trie** (Árbol de Sufijos) desde cero, utilizando punteros y memoria dinámica.

### Justificación Teórica

Esta estructura fue implementada desde cero usando punteros y memoria dinámica. Cada nodo del trie guarda dos cosas principales: primero, un unordered_map que conecta cada carácter con su siguiente nodo; y segundo, un unordered_set de punteros a películas.
Usamos unordered_map porque no queremos reservar espacio para todos los caracteres posibles, sino solo para los que realmente aparecen. Eso ayuda a manejar mejor la memoria. Y usamos unordered_set porque una misma película podría coincidir varias veces, pero no queremos que aparezca duplicada en los resultados.
Una diferencia importante frente a un trie normal es que el trie normal sirve sobre todo para buscar prefijos, es decir, el inicio de una palabra. En cambio, el Suffix Trie permite buscar subpalabras. Esto hace que el sistema sea más flexible, porque el usuario puede encontrar resultados aunque escriba solo una parte del término.
Además, el índice no se manejó como una sola estructura gigante, sino que se dividió en shards. Un shard es como una parte independiente del índice. Cada shard tiene sus propios tries para distintos campos: uno para título y sinopsis, otro para director, otro para casting y otro para género.
Esto también ayuda más adelante con la búsqueda paralela, porque cada shard puede trabajar de manera independiente. Aunque esa parte la explicará mi compañero más adelante, la base está en que el índice ya fue diseñado pensando en dividir el trabajo.
Sobre la complejidad, la búsqueda es eficiente porque depende principalmente del tamaño de la palabra que el usuario está buscando, no de la cantidad total de películas. Es decir, buscar una palabra corta no obliga al programa a recorrer toda la base de datos. En cambio, la inserción sí es más pesada, porque por cada palabra se insertan sus sufijos. Pero esa carga se hace al construir el índice, y luego las búsquedas quedan mucho más rápidas.

### Análisis de Complejidad (Big-O)

| Operación  | Complejidad | Observación |
|------------|-------------|-------------|
| Búsqueda   | O(m)        | `m` = longitud de la palabra buscada. No depende del total de películas. |
| Inserción  | O(w²)       | `w` = longitud de la palabra indexada, por la creación de sufijos. |
| Espacio    | O(S²)       | `S` = total de caracteres. Estructura pesada en RAM, optimizada para velocidad. |

---

## 4. Programación Genérica Avanzada

template <typename T, size_t DefaultPageSize = 5>
class Paginador { ... };

template <typename T,
          typename = enable_if_t<is_arithmetic_v<T>>>
struct Estadisticas {
    T      minimo, maximo;
    double promedio;
    size_t cantidad;

template <typename Container>
    static Estadisticas calcular(const Container& c);
};

// Uso en benchmark:
auto stat = Estadisticas<double>::calcular(tiemposSeq);
cout << stat.promedio << "\n";
// Uso en buscarPeliculas():
Paginador<const Pelicula*, 5> paginador(resultados);


template <typename Resultado, typename Container, typename MapFn, typename ReduceFn>
Resultado parallelReduce(const Container& datos, size_t numHilos,
                         MapFn mapFn, ReduceFn reduceFn, Resultado valorInicial);


  template <typename T>
class ResultadoOp {
    static ResultadoOp exito(T v);
    static ResultadoOp falla(string msg);
    bool valido() const;
    const T& valor() const;
    explicit operator bool() const;
};

En esta parte usamos templates de C++ para que varias herramientas del proyecto sean reutilizables y no estén amarradas a un solo tipo de dato.
Primero está el Paginador<T, DefaultPageSize>. Esta clase sirve para mostrar resultados por páginas. Por ejemplo, si una búsqueda devuelve muchas películas, no se muestran todas de golpe, sino en grupos. Lo interesante es que usa un parámetro genérico T, así que podría paginar películas, punteros u otros tipos de datos. Además, el tamaño de página puede definirse en tiempo de compilación con un valor por defecto.
Luego está Estadisticas<T>, que permite calcular mínimo, máximo, promedio y cantidad sobre datos numéricos. Para evitar que se use con tipos que no tienen sentido, como strings, usamos enable_if e is_arithmetic. Eso restringe la estructura solo a tipos numéricos, como int, float o double.
También usamos parallelReduce, que es una función genérica inspirada en la idea de MapReduce. Esta función divide una colección en partes, aplica una función a cada parte y luego combina los resultados. Lo bueno es que recibe lambdas como parámetros, así que no está limitada a una sola operación. Se puede reutilizar para distintos cálculos, especialmente en tareas paralelas.
Por último, tenemos ResultadoOp<T>, que es un wrapper genérico para manejar operaciones que pueden salir bien o fallar. En vez de depender siempre de excepciones, esta clase permite devolver un resultado exitoso o un mensaje de error.


---

## 5. Programación Paralela y Benchmark

La estructura concurrente divide el Suffix Trie en sub-índices independientes (**Shards**) para aprovechar al máximo la arquitectura multinúcleo, usando `std::async` y `std::future` en dos fases críticas.

### Fase 1 — Indexación paralela

`MotorBusqueda::construir()` distribuye las películas entre shards mediante round-robin. Cada hilo construye su propio shard sin necesidad de `std::mutex`, eliminando condiciones de carrera.

```
Dataset (P películas)
    ↓ round-robin
  Shard 0   Shard 1   ...  Shard N-1
  (hilo 0)  (hilo 1)       (hilo N-1)
    ↓           ↓               ↓
   async       async           async
    └───────────┴───────────────┘
               join (future::get)
```

### Fase 2 — Búsqueda paralela

`MotorBusqueda::buscar()` lanza un `std::async` por cada shard. El hilo principal fusiona los conjuntos de resultados parciales.

```
Query tokens
    ↓ (broadcast a todos los shards)
  Shard 0   Shard 1   ...  Shard N-1
  buscar()  buscar()       buscar()
  (async)   (async)        (async)
    └───────────┴───────────────┘
         fusión en hilo principal
              → resultados finales
```

### Reporte de Benchmark

| Etapa          | Modo        | Hilos | Tiempo Promedio | Speedup      |
|:---------------|:------------|:-----:|:---------------:|:------------:|
| **Indexación** | Secuencial  |   1   |    ~ 1.250 s    |    1.00x     |
| **Indexación** | Paralelo    |   8   |    ~ 0.280 s    | **~ 4.46x**  |
| **Búsqueda**   | Secuencial  |   1   |    ~ 0.050 s    |    1.00x     |
| **Búsqueda**   | Paralelo    |   8   |    ~ 0.012 s    | **~ 4.16x**  |

> Valores obtenidos con 10 000 películas en una máquina con Intel Core Ultra 5 125U (8 hilos). El benchmark también calcula estadísticas (mínimo, máximo, promedio) usando el template `Estadisticas<double>`.

---

## 6. Patrones de Diseño Implementados (5)

La arquitectura respeta los principios SOLID incorporando explícitamente 5 patrones de la *Gang of Four* (GoF):

### 1. Builder — `PeliculaBuilder`

Garantiza la construcción paso a paso de objetos `Pelicula`, separando la representación del ensamblaje y evitando constructores con exceso de parámetros posicionales.

```cpp
auto pelicula = PeliculaBuilder()
    .titulo("Metropolis")
    .director("Fritz Lang")
    .anio(1927)
    .genero("Sci-Fi")
    .sinopsis("...")
    .build();
```

### 2. Factory Method — `IPeliculaFactory` / `PeliculaCSVFactory`

Desacopla la lógica de parseo crudo de la creación en memoria. A futuro, soportar formatos JSON o bases SQL requiere únicamente inyectar una nueva fábrica, sin modificar `CSVParser`.

```
IPeliculaFactory            ← interfaz (método virtual puro)
    └── PeliculaCSVFactory  ← concreta para wiki_movie_plots
         usa PeliculaBuilder internamente
```

### 3. Singleton — `Configuracion`

Asegura un punto global y único para el estado de arranque (hilos, límites, rutas). Se implementó de manera segura deshabilitando los constructores de copia y movimiento (`= delete`). La inicialización del static local es thread-safe por especificación del estándar (C++11 §6.7).

```cpp
static Configuracion& instancia() {
    static Configuracion cfg;
    return cfg;
}
```

### 4. Observer — `IUsuarioObserver`

El sujeto (`Usuario`) notifica de manera reactiva sus cambios de estado a dos observadores concretos:

| Observador       | Responsabilidad |
|:-----------------|:----------------|
| `SesionObserver` | Auto-guarda en disco los *Likes* y la lista "Ver más tarde". |
| `LogObserver`    | Registra auditorías de uso en `actividad.log` con timestamp, usando `std::mutex` para acceso hilo-seguro. |

### 5. Strategy — `IRankingStrategy` / `IRecommendationStrategy`

Aísla los algoritmos y permite cambiarlos en tiempo de ejecución desde el menú "Cambiar estrategias", sin tocar el Motor de Búsqueda.

**Ranking:**

| Clase                  | Criterio |
|:-----------------------|:---------|
| `RankingPorRelevancia` | Score ponderado: título(100) + director(40) + género(35) + casting(25) + sinopsis(10) |
| `RankingPorAnio`       | Año descendente (más reciente primero) |

**Recomendación:**

| Clase                      | Criterio |
|:---------------------------|:---------|
| `RecomendacionPorAfinidad` | Frecuencia de géneros, directores y actores en los likes del usuario |
| `RecomendacionPorAnio`     | Películas del período más cercano al promedio de años de los likes |

---

## 7. Avance de la Interfaz del Programa

El programa se ejecuta íntegramente en la terminal mediante comandos de texto y menús numerados.

- **Menú principal:** Muestra el estado de carga, el usuario activo, las estrategias activas y un panel con "Ver más tarde" y recomendaciones personalizadas.
- **Mecanismo de búsqueda:** El usuario selecciona el campo (todo, título, director, casting, género) e ingresa una cadena; el sistema consulta el Trie y recupera el conjunto de punteros coincidentes.
- **Paginación:** Los resultados se presentan en bloques de 5. El usuario navega entre páginas con `S` (Siguiente) y `A` (Anterior).
- **Vista de detalle:** Al seleccionar un resultado (1–5), la pantalla muestra título, año, director, género, casting y sinopsis completa, con opciones de "Like" y "Añadir a Ver más tarde".
- **Cambio de estrategias:** Desde el menú principal se puede alternar entre algoritmos de ranking y recomendación en tiempo de ejecución.
- **Persistencia automática:** Al dar Like o guardar una película, la sesión se escribe en disco sin intervención del usuario.

---

## 8. Especificaciones Técnicas

### Entorno de Desarrollo

| Parámetro            | Detalle                                                              |
|----------------------|----------------------------------------------------------------------|
| Hardware (Máquina 1) | Procesador: Intel(R) Core(TM) Ultra 5 125U (1.30 GHz), RAM: 16.0 GB |
| Sistema Operativo    | Windows 11                                                           |
| Estándar de C++      | C++17                                                                |
| Librerías externas   | Ninguna — se utiliza exclusivamente la STL                           |

### Estructura del código

El archivo `main.cpp` está organizado en 14 secciones numeradas:

```
Sección  1 — TextoUtil         namespace con limpiarTexto() y tokenizar()
Sección  2 — Programación Genérica
             2-A  Paginador<T, DefaultPageSize>    (clase template + NTTP)
             2-B  parallelReduce<Res,C,Map,Reduce>  (función template)
             2-C  ResultadoOp<T>                   (wrapper éxito/error)
             2-D  Estadisticas<T>                  (agrega numéricos + traits)
Sección  3 — Pelicula          entidad de dominio
Sección  4 — Patrones de Diseño
             4-A  Factory Method  (IPeliculaFactory / PeliculaCSVFactory)
             4-B  Builder         (PeliculaBuilder — fluent interface)
             4-C  Singleton       (Configuracion)
             4-D  Observer        (IUsuarioObserver / SesionObserver / LogObserver)
             4-E  Strategy        (IRankingStrategy × 2, IRecommendationStrategy × 2)
Sección  5 — Persistencia      lectura/escritura de sesión en disco
Sección  6 — Observer          implementaciones concretas de IUsuarioObserver
Sección  7 — Usuario           sujeto observable; mutaciones controladas
Sección  8 — CSVParser         parser RFC 4180 con soporte multilínea
Sección  9 — SuffixTrie + IndiceShard + CampoBusqueda
Sección 10 — MotorBusqueda     paralelismo en indexación y búsqueda
Sección 11 — Estrategias       dos rankings y dos recomendadores concretos
Sección 12 — ResultadoBenchmark struct de métricas
Sección 13 — Plataforma        controlador de UI + método ejecutarBenchmark()
Sección 14 — main()
```

### Consumo de Memoria RAM (Estimado)

| Escenario                          | Consumo          |
|------------------------------------|------------------|
| Carga parcial (500 películas)      | ~ 45 MB          |
| Carga completa (~ 35 000 películas)| ~ 450 MB – 600 MB|

> El alto consumo de RAM es inherente a la naturaleza del Suffix Trie, que prioriza velocidad de respuesta sobre uso de memoria.

### Dataset esperado

El CSV debe tener al menos 8 columnas con este orden:

| Col | Campo    |
|:---:|:---------|
| 0   | Año      |
| 1   | Título   |
| 2   | Origen   |
| 3   | Director |
| 4   | Casting  |
| 5   | Género   |
| 6   | Wiki URL |
| 7   | Sinopsis |

Dataset recomendado: [wiki_movie_plots_deduped.csv](https://www.kaggle.com/datasets/jrobischon/wikipedia-movie-plots) (~ 35 000 películas).

---

## 9. Compilación y Ejecución

### Requisitos

- Compilador compatible con **C++17** (`g++` o `clang++`)
- Archivo `wiki_movie_plots_deduped.csv` en la misma carpeta que el ejecutable

### Comandos

```bash
# Compilación con optimización de velocidad
g++ -O3 -std=c++17 -pthread main.cpp -o utec_streaming

# Ejecución normal
./utec_streaming

# Opciones de línea de comandos
./utec_streaming --csv   wiki_movie_plots_deduped.csv  # ruta al CSV
                 --limit 5000                          # límite de películas a cargar
                 --threads 8                           # número de hilos
                 --benchmark 3000                      # modo benchmark con N películas

# Modo evaluación (test de rendimiento multihilo)
./utec_streaming --benchmark 10000 --threads 8
```

> **Nota:** Se recomienda el flag `-O3` para maximizar la velocidad de construcción del Suffix Trie durante la carga inicial.

### Sesión de usuario

Al iniciar, el programa crea/carga automáticamente `<usuario>_sesion.txt`. Las acciones de Like y "Ver más tarde" se persisten mediante el `SesionObserver`. El `LogObserver` registra cada cambio en `actividad.log` con timestamp.

---

## 10. Bibliografía

- Gamma, E., Helm, R., Johnson, R., & Vlissides, J. (1994). *Design patterns: elements of reusable object-oriented software*. Addison-Wesley.
- Knuth, D. E., Morris, (Jr) J. H., & Pratt, V. R. (1977). Fast pattern matching in strings. *SIAM Journal on Computing*, 6(2), 323–350.
- Stroustrup, B. (2013). *The C++ Programming Language* (4th ed.). Addison-Wesley Professional.
- Ukkonen, E. (1995). On-line construction of suffix trees. *Algorithmica*, 14(3), 249–260.
- Williams, A. (2012). *C++ Concurrency in Action: Practical Multithreading*. Manning Publications.

---

*Proyecto desarrollado para el curso Programación III — UTEC.*
