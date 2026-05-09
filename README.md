# Programación III: UTEC STREAMING
### Proyecto #1

---

## Integrantes

| Nombre y Apellidos | Código   |
|--------------------|----------|
| Jasyr Valdez       | 202510474 |
| Andre Sanchez      | 202510063 |
| Renzo Liang        | 202320147 |

---

## Tabla de Contenidos

1. [Pre-procesamiento de los datos](#1-pre-procesamiento-de-los-datos)
2. [Pseudo-código de Inserción](#2-pseudo-código-de-inserción-suffix-trie)
3. [Estructura de Datos: Suffix Trie](#3-estructura-de-datos-suffix-trie)
4. [Avance de la Interfaz del Programa](#4-avance-de-la-interfaz-del-programa)
5. [Especificaciones Técnicas](#5-especificaciones-técnicas)

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

A diferencia de un Trie estándar —que solo permite búsquedas por prefijo (inicio de palabra)— el Suffix Trie indexa cada terminación posible de las palabras. Esto permite encontrar sub-palabras de manera eficiente: si se busca `"bar"`, el árbol conduce a los nodos que contienen esa secuencia independientemente de si formaba parte de `"barco"` o `"desembarcar"`.

### Diseño de los Nodos

Cada nodo (`NodoTrie`) contiene:

- **Contenedor asociativo:** `std::unordered_map<char, NodoTrie*>` para gestionar los hijos. Optimiza el uso de memoria al no reservar espacio para caracteres inexistentes.
- **Contenedor de resultados:** `std::unordered_set<Pelicula*>`. Almacena punteros únicos a las películas que contienen la secuencia de caracteres que llega a ese nodo. El uso de `set` evita duplicados en los resultados.

### Análisis de Complejidad (Big-O)

| Operación | Complejidad | Observación |
|-----------|-------------|-------------|
| Búsqueda | O(m) | `m` = longitud de la palabra buscada. No depende del total de películas. |
| Inserción | O(w²) | `w` = longitud de la palabra indexada, por la creación de sufijos. |
| Espacio | O(S²) | `S` = total de caracteres. Estructura pesada en RAM, optimizada para velocidad. |

---

## 4. Avance de la Interfaz del Programa

El programa se ejecuta íntegramente en la terminal mediante comandos de texto y menús numerados.

- **Menú principal:** Muestra el estado de carga de datos y un panel de bienvenida. Permite realizar búsquedas globales por título, sinopsis, director, entre otros.
- **Mecanismo de búsqueda:** El usuario ingresa una cadena; el sistema consulta el Trie y recupera el conjunto de punteros coincidentes.
- **Paginación:** Los resultados se presentan en bloques de 5. El usuario navega entre páginas con `S` (Siguiente) y `A` (Anterior).
- **Vista de detalle:** Al seleccionar un resultado (1–5), la pantalla muestra la sinopsis completa, director y casting, con la opción de marcar la película con "Like".
---

## 5. Especificaciones Técnicas

### Entorno de Desarrollo

| Parámetro            | Detalle                                                             |
|----------------------|---------------------------------------------------------------------|
| Hardware (Máquina 1) | Procesador: Intel(R) Core(TM) Ultra 5 125U (1.30 GHz), RAM: 16.0 GB |
| Sistema Operativo    | Completar: Windows 11                                               |
| Estándar de C++      | C++17                                                               |
| Librerías externas   | Ninguna — se utiliza exclusivamente la STL                          |

### Consumo de Memoria RAM (Estimado)

| Escenario | Consumo |
|-----------|---------|
| Carga parcial (500 películas) | ~45 MB |
| Carga completa (~35 000 películas) | ~450 MB – 600 MB |

> El alto consumo de RAM es inherente a la naturaleza del Suffix Trie, que prioriza velocidad de respuesta sobre uso de memoria.

---

## Compilación y Ejecución

### Requisitos

- Compilador compatible con **C++17** (`g++` o `clang++`)
- Archivo `wiki_movie_plots_deduped.csv` en la misma carpeta que el ejecutable

### Comandos

```bash
# Compilación con optimización de velocidad
g++ -O3 -std=c++17 main.cpp -o utec_streaming

# Ejecución
./utec_streaming
```

> **Nota:** Se recomienda usar la flag `-O3` para maximizar la velocidad de construcción del árbol durante la carga inicial del dataset.

---

*Proyecto desarrollado para el curso Programación III.*
