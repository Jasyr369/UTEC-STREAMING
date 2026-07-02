#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace std;

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  SECCIÓN 1 — UTILIDADES DE TEXTO
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
namespace TextoUtil {
    string limpiarTexto(const string& texto) {
        string resultado;
        resultado.reserve(texto.size());
        for (unsigned char c : texto) {
            if (isalnum(c) || isspace(c))
                resultado += static_cast<char>(tolower(c));
            else
                resultado += ' ';
        }
        return resultado;
    }

    vector<string> tokenizar(const string& texto) {
        vector<string> tokens;
        tokens.reserve(texto.size() / 6);
        string tokenActual;
        tokenActual.reserve(32);
        for (unsigned char c : texto) {
            if (isalnum(c)) {
                tokenActual += static_cast<char>(tolower(c));
            } else if (!tokenActual.empty()) {
                tokens.push_back(move(tokenActual));
                tokenActual.clear();
            }
        }
        if (!tokenActual.empty()) tokens.push_back(move(tokenActual));
        return tokens;
    }
}

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  SECCIÓN 2 — PROGRAMACIÓN GENÉRICA
//
//  2-A  Paginador<T, PageSize>  — clase template con parámetro
//       de tipo y parámetro de valor (NTTP).
//  2-B  ParallelReduce<T>       — función template para reducir
//       colecciones en paralelo (std::future).
//  2-C  ResultadoOp<T>          — wrapper genérico de resultado /
//       error (similar a std::expected, disponible en C++23).
//  2-D  Estadisticas<T>         — agrega min/max/sum de cualquier
//       tipo numérico usando traits de la STL.
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// 2-C — Resultado genérico éxito / error
template <typename T>
class ResultadoOp {
    bool ok_;
    T    valor_;
    string error_;
public:
    static ResultadoOp exito(T v)          { ResultadoOp r; r.ok_=true;  r.valor_=move(v); return r; }
    static ResultadoOp falla(string msg)   { ResultadoOp r; r.ok_=false; r.error_=move(msg); return r; }

    bool     valido()  const { return ok_; }
    const T& valor()   const { return valor_; }
    const string& error() const { return error_; }
    explicit operator bool() const { return ok_; }
private:
    ResultadoOp() = default;
};

// 2-D — Estadísticas sobre cualquier colección de tipo numérico
template <typename T,
          typename = enable_if_t<is_arithmetic_v<T>>>
struct Estadisticas {
    T  minimo;
    T  maximo;
    double promedio;
    size_t cantidad;

    template <typename Container>
    static Estadisticas calcular(const Container& c) {
        Estadisticas s{};
        if (c.empty()) return s;
        s.minimo = s.maximo = *c.begin();
        double suma = 0.0;
        for (const T& v : c) {
            if (v < s.minimo) s.minimo = v;
            if (v > s.maximo) s.maximo = v;
            suma += static_cast<double>(v);
            ++s.cantidad;
        }
        s.promedio = suma / static_cast<double>(s.cantidad);
        return s;
    }
};

// 2-B — Reducción paralela genérica
//   Divide la colección en N tareas, aplica 'mapFn' a cada chunk
//   y combina resultados con 'reduceFn'.
template <typename Resultado, typename Container, typename MapFn, typename ReduceFn>
Resultado parallelReduce(const Container& datos,
                         size_t           numHilos,
                         MapFn            mapFn,
                         ReduceFn         reduceFn,
                         Resultado        valorInicial)
{
    if (datos.empty()) return valorInicial;
    numHilos = max<size_t>(1, min(numHilos, datos.size()));

    size_t total     = datos.size();
    size_t chunkSize = (total + numHilos - 1) / numHilos;

    vector<future<Resultado>> tareas;
    tareas.reserve(numHilos);

    auto it = datos.begin();
    for (size_t t = 0; t < numHilos; ++t) {
        auto inicio = it;
        size_t avance = min(chunkSize, static_cast<size_t>(datos.end() - it));
        advance(it, static_cast<ptrdiff_t>(avance));
        auto fin = it;

        tareas.push_back(async(launch::async, [inicio, fin, &mapFn]() {
            return mapFn(inicio, fin);
        }));
    }

    Resultado acum = valorInicial;
    for (auto& f : tareas) acum = reduceFn(move(acum), f.get());
    return acum;
}

// 2-A — Paginador genérico con parámetro de valor (NTTP)
template <typename T, size_t DefaultPageSize = 5>
class Paginador {
    static_assert(DefaultPageSize > 0, "DefaultPageSize debe ser mayor que cero");

    vector<T> items_;
    size_t    porPagina_;

public:
    // Constructor con page-size dinámico (sobreescribe el NTTP)
    explicit Paginador(vector<T> items, size_t porPagina = DefaultPageSize)
        : items_(move(items)), porPagina_(porPagina > 0 ? porPagina : DefaultPageSize) {}

    size_t totalPaginas() const {
        if (items_.empty()) return 0;
        return (items_.size() + porPagina_ - 1) / porPagina_;
    }

    size_t tamanio() const { return items_.size(); }

    // Devuelve la página (sin excepción: página fuera de rango → vacío)
    vector<T> obtenerPagina(size_t numPagina) const {
        size_t inicio = numPagina * porPagina_;
        if (inicio >= items_.size()) return {};
        size_t fin = min(items_.size(), inicio + porPagina_);
        return vector<T>(items_.begin() + static_cast<ptrdiff_t>(inicio),
                         items_.begin() + static_cast<ptrdiff_t>(fin));
    }

    // Iteradores para permitir rango-for sobre los items completos
    auto begin() const { return items_.begin(); }
    auto end()   const { return items_.end(); }
};

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  SECCIÓN 3 — ENTIDAD Pelicula
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class Pelicula {
public:
    string titulo;
    string sinopsis;
    string director;
    string genero;
    string casting;
    int    anio;

    Pelicula() : anio(0) {}

    Pelicula(string t, string sin, string dir,
             string gen, string cast, int yr)
        : titulo(move(t)), sinopsis(move(sin)),
          director(move(dir)), genero(move(gen)),
          casting(move(cast)), anio(yr) {}

    friend ostream& operator<<(ostream& os, const Pelicula& p) {
        os << "========================================\n"
           << " TITULO:   " << p.titulo << " (" << p.anio << ")\n"
           << " DIRECTOR: " << p.director << "\n"
           << " GENERO:   " << p.genero << "\n"
           << " CASTING:  " << p.casting << "\n"
           << "----------------------------------------\n"
           << " SINOPSIS:\n" << p.sinopsis << "\n"
           << "========================================";
        return os;
    }
};

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  SECCIÓN 4 — PATRONES DE DISEÑO
//
//  4-A  Factory Method  — IPeliculaFactory + PeliculaCSVFactory
//       (jerarquía de fábricas con interfaz, no solo método estático)
//  4-B  Builder         — PeliculaBuilder para construcción paso a paso
//  4-C  Singleton       — Configuracion (corrección: + move deleted)
//  4-D  Observer        — IUsuarioObserver / SesionObserver / LogObserver
//  4-E  Strategy        — IRankingStrategy / IRecommendationStrategy
//       (dos familias de estrategias, intercambiables en runtime)
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// -------  4-B Builder  -------
class PeliculaBuilder {
    string titulo_, sinopsis_, director_, genero_, casting_;
    int    anio_ = 0;
public:
    PeliculaBuilder& titulo(string v)   { titulo_   = move(v); return *this; }
    PeliculaBuilder& sinopsis(string v) { sinopsis_ = move(v); return *this; }
    PeliculaBuilder& director(string v) { director_ = move(v); return *this; }
    PeliculaBuilder& genero(string v)   { genero_   = move(v); return *this; }
    PeliculaBuilder& casting(string v)  { casting_  = move(v); return *this; }
    PeliculaBuilder& anio(int v)        { anio_     = v;       return *this; }

    unique_ptr<Pelicula> build() {
        if (titulo_.empty()) throw invalid_argument("Pelicula sin titulo");
        return make_unique<Pelicula>(move(titulo_), move(sinopsis_),
                                     move(director_), move(genero_),
                                     move(casting_), anio_);
    }
};

// -------  4-A Factory Method  -------
class IPeliculaFactory {
public:
    virtual ~IPeliculaFactory() = default;
    virtual unique_ptr<Pelicula> crear(const vector<string>& columnas) const = 0;
};

// Fábrica concreta para el CSV wiki_movie_plots_deduped
// Columnas: 0:Año 1:Título 2:Origin 3:Director 4:Cast 5:Género 6:Wiki 7:Plot
class PeliculaCSVFactory : public IPeliculaFactory {
public:
    unique_ptr<Pelicula> crear(const vector<string>& col) const override {
        if (col.size() < 8) return nullptr;
        const string& titulo = col[1];
        if (titulo.empty() || titulo == "unknown") return nullptr;

        int anio = 0;
        try {
            if (!col[0].empty() && col[0] != "unknown") anio = stoi(col[0]);
        } catch (...) { anio = 0; }

        return PeliculaBuilder()
            .titulo(titulo)
            .sinopsis(col[7])
            .director(col[3])
            .genero(col[5])
            .casting(col[4])
            .anio(anio)
            .build();
    }
};

// -------  4-C Singleton (Configuracion)  -------
class Configuracion {
    string rutaCSV_         = "wiki_movie_plots_deduped.csv";
    size_t limiteCarga_     = 0;
    size_t hilos_           = max(2u, thread::hardware_concurrency());
    bool   modoBenchmark_   = false;
    size_t limiteBenchmark_ = 3000;

    Configuracion() = default;
public:
    // Eliminar copia Y movimiento para garantizar unicidad
    Configuracion(const Configuracion&)            = delete;
    Configuracion& operator=(const Configuracion&) = delete;
    Configuracion(Configuracion&&)                 = delete;
    Configuracion& operator=(Configuracion&&)      = delete;

    static Configuracion& instancia() {
        static Configuracion cfg;
        return cfg;
    }

    const string& rutaCSV()       const { return rutaCSV_; }
    size_t        limiteCarga()   const { return limiteCarga_; }
    size_t        hilos()         const { return hilos_; }
    bool          modoBenchmark() const { return modoBenchmark_; }
    size_t        limiteBenchmark() const { return limiteBenchmark_; }

    void setRutaCSV(string ruta)       { rutaCSV_  = move(ruta); }
    void setLimiteCarga(size_t limite) { limiteCarga_ = limite; }
    void setHilos(size_t h)            { hilos_ = max<size_t>(1, h); }
    void activarBenchmark(size_t lim)  { modoBenchmark_ = true; limiteBenchmark_ = lim; }
};

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  SECCIÓN 5 — PERSISTENCIA
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class Persistencia {
public:
    static string rutaArchivo(const string& nombreUsuario) {
        return nombreUsuario + "_sesion.txt";
    }

    static void guardar(const string& nombreUsuario,
                        const unordered_set<string>& titulosLikes,
                        const vector<string>& titulosVerMasTarde) {
        ofstream f(rutaArchivo(nombreUsuario));
        if (!f.is_open()) return;
        f << "[likes]\n";
        for (const string& t : titulosLikes) f << t << "\n";
        f << "[ver_mas_tarde]\n";
        for (const string& t : titulosVerMasTarde) f << t << "\n";
    }

    static void cargar(const string& nombreUsuario,
                       unordered_set<string>& titulosLikes,
                       vector<string>& titulosVerMasTarde) {
        ifstream f(rutaArchivo(nombreUsuario));
        if (!f.is_open()) return;
        string linea;
        int seccion = 0;
        while (getline(f, linea)) {
            if (linea == "[likes]")         { seccion = 1; continue; }
            if (linea == "[ver_mas_tarde]") { seccion = 2; continue; }
            if (linea.empty()) continue;
            if (seccion == 1) titulosLikes.insert(linea);
            else if (seccion == 2) titulosVerMasTarde.push_back(linea);
        }
    }
};

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  SECCIÓN 6 — PATRÓN OBSERVER (4-D)
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class IUsuarioObserver {
public:
    virtual ~IUsuarioObserver() = default;
    virtual void onModificado(const string& nombre,
                              const unordered_set<const Pelicula*>& likes,
                              const vector<const Pelicula*>& vmt) = 0;
};

// Observer 1: persiste la sesión en disco
class SesionObserver : public IUsuarioObserver {
public:
    void onModificado(const string& nombre,
                      const unordered_set<const Pelicula*>& likes,
                      const vector<const Pelicula*>& vmt) override {
        unordered_set<string> titulosLikes;
        for (const Pelicula* p : likes) titulosLikes.insert(p->titulo);
        vector<string> titVMT;
        for (const Pelicula* p : vmt) titVMT.push_back(p->titulo);
        Persistencia::guardar(nombre, titulosLikes, titVMT);
    }
};

// Observer 2: registra en log de actividad (segundo observador concreto)
class LogObserver : public IUsuarioObserver {
    string archivoLog_;
    mutex  mtx_;
public:
    explicit LogObserver(string archivoLog = "actividad.log")
        : archivoLog_(move(archivoLog)) {}

    void onModificado(const string& nombre,
                      const unordered_set<const Pelicula*>& likes,
                      const vector<const Pelicula*>& /*vmt*/) override {
        lock_guard<mutex> lock(mtx_);
        ofstream f(archivoLog_, ios::app);
        if (!f.is_open()) return;
        auto now = chrono::system_clock::now();
        auto t   = chrono::system_clock::to_time_t(now);
        f << "[" << put_time(localtime(&t), "%F %T") << "] "
          << nombre << " — likes: " << likes.size() << "\n";
    }
};

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  SECCIÓN 7 — USUARIO (sujeto observable)
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class Usuario {
    vector<IUsuarioObserver*> observadores_;

    void notificar() {
        for (auto* obs : observadores_)
            obs->onModificado(nombre, likes, verMasTarde);
    }

public:
    string nombre;
    // Acceso controlado: los campos permanecen públicos para la
    // compatibilidad con los observers, pero toda mutación pasa
    // por métodos que garantizan consistencia y notificación.
    unordered_set<const Pelicula*> likes;
    vector<const Pelicula*>        verMasTarde;

    explicit Usuario(string n) : nombre(move(n)) {}

    void agregarObservador(IUsuarioObserver* obs) {
        observadores_.push_back(obs);
    }

    bool leGusta(const Pelicula* p) const {
        return likes.count(p) > 0;
    }

    bool estaEnVerMasTarde(const Pelicula* p) const {
        for (const Pelicula* q : verMasTarde) if (q == p) return true;
        return false;
    }

    void darLike(const Pelicula* p) {
        if (!leGusta(p)) { likes.insert(p); notificar(); }
    }

    void anadirVerMasTarde(const Pelicula* p) {
        if (!estaEnVerMasTarde(p)) { verMasTarde.push_back(p); notificar(); }
    }

    void cargarSesion(const vector<unique_ptr<Pelicula>>& baseDatos) {
        unordered_set<string> titulosLikes;
        vector<string>        titulosVMT;
        Persistencia::cargar(nombre, titulosLikes, titulosVMT);

        unordered_map<string, const Pelicula*> indice;
        indice.reserve(baseDatos.size());
        for (const auto& p : baseDatos) indice[p->titulo] = p.get();

        for (const string& t : titulosLikes) {
            auto it = indice.find(t);
            if (it != indice.end()) likes.insert(it->second);
        }
        for (const string& t : titulosVMT) {
            auto it = indice.find(t);
            if (it != indice.end() && !estaEnVerMasTarde(it->second))
                verMasTarde.push_back(it->second);
        }
    }
};

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  SECCIÓN 8 — PARSER CSV
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class CSVParser {
    static vector<string> parsearLinea(const string& linea) {
        vector<string> cols;
        cols.reserve(8);
        string val;
        val.reserve(128);
        bool enComillas = false;
        for (size_t i = 0; i < linea.size(); ++i) {
            char c = linea[i];
            if (c == '"') {
                if (enComillas && i+1 < linea.size() && linea[i+1] == '"') {
                    val += '"'; ++i;
                } else {
                    enComillas = !enComillas;
                }
            } else if (c == ',' && !enComillas) {
                cols.push_back(move(val)); val.clear();
            } else {
                val += c;
            }
        }
        cols.push_back(move(val));
        return cols;
    }

    static bool comillasAbiertas(const string& s) {
        bool dentro = false;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '"') {
                if (dentro && i+1 < s.size() && s[i+1] == '"') ++i;
                else dentro = !dentro;
            }
        }
        return dentro;
    }

public:
    static vector<unique_ptr<Pelicula>> cargarPeliculas(
        const string& ruta,
        size_t limite = 0,
        const IPeliculaFactory* factory = nullptr)
    {
        // Fábrica por defecto si no se inyecta una
        PeliculaCSVFactory defFactory;
        if (!factory) factory = &defFactory;

        ifstream archivo(ruta);
        if (!archivo.is_open())
            throw runtime_error("No se pudo abrir CSV: " + ruta);

        vector<unique_ptr<Pelicula>> peliculas;
        string linea, registro;
        registro.reserve(4096);
        getline(archivo, linea); // descartar cabecera
        size_t contador = 0;

        while (getline(archivo, linea)) {
            if (!linea.empty() && linea.back() == '\r') linea.pop_back();
            if (!registro.empty()) registro += '\n';
            registro += linea;
            if (comillasAbiertas(registro)) continue;

            auto columnas = parsearLinea(registro);
            registro.clear();

            auto pelicula = factory->crear(columnas);
            if (!pelicula) continue;

            peliculas.push_back(move(pelicula));
            ++contador;

            if (contador % 1000 == 0) {
                cout << "Leyendo CSV... " << contador << " peliculas.\r";
                cout.flush();
            }
            if (limite > 0 && contador >= limite) break;
        }
        cout << "\nCSV cargado. Peliculas leidas: " << peliculas.size() << "\n";
        return peliculas;
    }
};

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  SECCIÓN 9 — ÍNDICE: Trie de sufijos
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class NodoTrie {
public:
    unordered_map<char, NodoTrie*>  hijos;
    unordered_set<const Pelicula*>  peliculas;

    ~NodoTrie() {
        for (auto& [c, n] : hijos) delete n;
    }
};

class SuffixTrie {
    NodoTrie* raiz_;

    void insertarSufijo(const string& suf, const Pelicula* p) {
        NodoTrie* actual = raiz_;
        for (char c : suf) {
            auto [it, inserted] = actual->hijos.emplace(c, nullptr);
            if (inserted) it->second = new NodoTrie();
            actual = it->second;
            actual->peliculas.insert(p);
        }
    }
public:
    SuffixTrie()  : raiz_(new NodoTrie()) {}
    ~SuffixTrie() { delete raiz_; }

    // No copiable (los punteros raw son del trie)
    SuffixTrie(const SuffixTrie&)            = delete;
    SuffixTrie& operator=(const SuffixTrie&) = delete;

    void indexarTexto(const string& texto, const Pelicula* p, bool soloPrefijos = false) {
        for (const string& palabra : TextoUtil::tokenizar(texto)) {
            if (palabra.size() > 40) continue;
            size_t lim = soloPrefijos ? 1 : palabra.size();
            for (size_t i = 0; i < lim; ++i)
                insertarSufijo(palabra.substr(i), p);
        }
    }

    unordered_set<const Pelicula*> buscarToken(const string& token) const {
        string limpio = TextoUtil::limpiarTexto(token);
        if (limpio.empty()) return {};
        const NodoTrie* actual = raiz_;
        for (char c : limpio) {
            if (isspace(static_cast<unsigned char>(c))) continue;
            auto it = actual->hijos.find(c);
            if (it == actual->hijos.end()) return {};
            actual = it->second;
        }
        return actual->peliculas;
    }
};

enum class CampoBusqueda { Todo = 0, TituloSinopsis, Director, Casting, Genero };

class IndiceShard {
    SuffixTrie tituloSinopsis_;
    SuffixTrie director_;
    SuffixTrie casting_;
    SuffixTrie genero_;
public:
    void indexar(const vector<const Pelicula*>& peliculas) {
        for (const Pelicula* p : peliculas) {
            tituloSinopsis_.indexarTexto(p->titulo,   p, false);
            tituloSinopsis_.indexarTexto(p->sinopsis, p, true);
            director_.indexarTexto(p->director,       p, false);
            casting_.indexarTexto(p->casting,         p, false);
            genero_.indexarTexto(p->genero,           p, false);
        }
    }

    unordered_set<const Pelicula*> buscarTokens(
        const vector<string>& tokens, CampoBusqueda campo) const
    {
        unordered_set<const Pelicula*> acum;
        auto buscarEnTrie = [&](const SuffixTrie& trie) {
            for (const string& tok : tokens) {
                auto parcial = trie.buscarToken(tok);
                acum.insert(parcial.begin(), parcial.end());
            }
        };
        switch (campo) {
            case CampoBusqueda::TituloSinopsis: buscarEnTrie(tituloSinopsis_); break;
            case CampoBusqueda::Director:       buscarEnTrie(director_);       break;
            case CampoBusqueda::Casting:        buscarEnTrie(casting_);        break;
            case CampoBusqueda::Genero:         buscarEnTrie(genero_);         break;
            default:
                buscarEnTrie(tituloSinopsis_); buscarEnTrie(director_);
                buscarEnTrie(casting_);        buscarEnTrie(genero_);
        }
        return acum;
    }
};

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  SECCIÓN 10 — MOTOR DE BÚSQUEDA (Programación Paralela)
//
//  Paralelismo en dos etapas:
//   A) Indexación: cada shard se construye en un hilo separado.
//   B) Búsqueda:  cada shard se consulta en paralelo; los resultados
//      parciales se fusionan en el hilo principal.
//  Además se mide y expone el speedup real de cada etapa.
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class MotorBusqueda {
    vector<unique_ptr<IndiceShard>> shards_;
    size_t cantHilos_;

public:
    explicit MotorBusqueda(size_t hilos = 1)
        : cantHilos_(max<size_t>(1, hilos)) {}

    // --- Indexación paralela (etapa A) ---
    void construir(const vector<unique_ptr<Pelicula>>& peliculas) {
        if (peliculas.empty()) return;
        cantHilos_ = min(cantHilos_, peliculas.size());
        shards_.clear();
        shards_.reserve(cantHilos_);
        for (size_t i = 0; i < cantHilos_; ++i)
            shards_.push_back(make_unique<IndiceShard>());

        // Distribuir películas entre buckets (round-robin para equilibrar carga)
        vector<vector<const Pelicula*>> buckets(cantHilos_);
        for (size_t i = 0; i < peliculas.size(); ++i)
            buckets[i % cantHilos_].push_back(peliculas[i].get());

        cout << "Indexando con " << cantHilos_ << " hilo(s)...\n";

        // Lanzar una tarea async por shard (cada una trabaja en datos disjuntos → sin mutex)
        vector<future<void>> tareas;
        tareas.reserve(cantHilos_);
        for (size_t i = 0; i < cantHilos_; ++i) {
            tareas.push_back(async(launch::async,
                [this, i, &buckets]() { shards_[i]->indexar(buckets[i]); }));
        }
        for (auto& t : tareas) t.get();
    }

    // --- Búsqueda paralela (etapa B) ---
    unordered_set<const Pelicula*> buscar(
        const string& query, CampoBusqueda campo = CampoBusqueda::Todo) const
    {
        vector<string> tokens = TextoUtil::tokenizar(query);
        if (tokens.empty()) return {};

        // Cada shard se consulta concurrentemente
        vector<future<unordered_set<const Pelicula*>>> tareas;
        tareas.reserve(shards_.size());
        for (const auto& shard : shards_) {
            tareas.push_back(async(launch::async,
                [&shard, &tokens, campo]() {
                    return shard->buscarTokens(tokens, campo);
                }));
        }

        // Fusión de resultados parciales
        unordered_set<const Pelicula*> resultados;
        for (auto& t : tareas) {
            auto parcial = t.get();
            resultados.insert(parcial.begin(), parcial.end());
        }
        return resultados;
    }

    size_t hilosUsados() const { return cantHilos_; }
};

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  SECCIÓN 11 — ESTRATEGIAS (Patrón Strategy, 4-E)
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// --- Estrategia de ranking ---
class IRankingStrategy {
public:
    virtual ~IRankingStrategy() = default;
    virtual vector<const Pelicula*> rankear(
        const unordered_set<const Pelicula*>& resultados,
        const string& query) const = 0;
    virtual string nombre() const = 0;
};

class RankingPorRelevancia : public IRankingStrategy {
    static bool contiene(const string& hay, const string& aguja) {
        if (aguja.empty()) return true;
        return search(hay.begin(), hay.end(), aguja.begin(), aguja.end(),
            [](unsigned char a, unsigned char b){ return tolower(a) == tolower(b); })
               != hay.end();
    }
    static int calcularScore(const Pelicula* p, const vector<string>& toks) {
        int s = 0;
        for (const string& t : toks) {
            if (contiene(p->titulo,   t)) s += 100;
            if (contiene(p->director, t)) s += 40;
            if (contiene(p->genero,   t)) s += 35;
            if (contiene(p->casting,  t)) s += 25;
            if (contiene(p->sinopsis, t)) s += 10;
        }
        return s;
    }
public:
    string nombre() const override { return "Relevancia"; }

    vector<const Pelicula*> rankear(
        const unordered_set<const Pelicula*>& resultados,
        const string& query) const override
    {
        vector<string> toks = TextoUtil::tokenizar(query);
        vector<const Pelicula*> ord(resultados.begin(), resultados.end());
        sort(ord.begin(), ord.end(), [&toks](const Pelicula* a, const Pelicula* b) {
            int sa = calcularScore(a, toks), sb = calcularScore(b, toks);
            if (sa != sb) return sa > sb;
            if (a->anio != b->anio) return a->anio > b->anio;
            return a->titulo < b->titulo;
        });
        return ord;
    }
};

// Segunda estrategia concreta de ranking: orden cronológico descendente
class RankingPorAnio : public IRankingStrategy {
public:
    string nombre() const override { return "Año (reciente primero)"; }

    vector<const Pelicula*> rankear(
        const unordered_set<const Pelicula*>& resultados,
        const string& /*query*/) const override
    {
        vector<const Pelicula*> ord(resultados.begin(), resultados.end());
        sort(ord.begin(), ord.end(), [](const Pelicula* a, const Pelicula* b) {
            if (a->anio != b->anio) return a->anio > b->anio;
            return a->titulo < b->titulo;
        });
        return ord;
    }
};

// --- Estrategia de recomendación ---
class IRecommendationStrategy {
public:
    virtual ~IRecommendationStrategy() = default;
    virtual vector<const Pelicula*> recomendar(
        const Usuario& usuario,
        const vector<unique_ptr<Pelicula>>& baseDatos,
        size_t cantidad) const = 0;
    virtual string nombre() const = 0;
};

class RecomendacionPorAfinidad : public IRecommendationStrategy {
    static void sumarPalabras(const string& texto, unordered_map<string,int>& mapa) {
        for (const string& t : TextoUtil::tokenizar(texto))
            if (t.size() > 2) mapa[t]++;
    }
public:
    string nombre() const override { return "Afinidad por gustos"; }

    vector<const Pelicula*> recomendar(
        const Usuario& usuario,
        const vector<unique_ptr<Pelicula>>& baseDatos,
        size_t cantidad) const override
    {
        if (usuario.likes.empty()) return {};
        unordered_map<string,int> generos, directores, actores;
        for (const Pelicula* p : usuario.likes) {
            sumarPalabras(p->genero,   generos);
            sumarPalabras(p->director, directores);
            sumarPalabras(p->casting,  actores);
        }
        vector<pair<int, const Pelicula*>> candidatos;
        candidatos.reserve(baseDatos.size() / 4);
        for (const auto& pp : baseDatos) {
            const Pelicula* p = pp.get();
            if (usuario.leGusta(p)) continue;
            int score = 0;
            for (const string& t : TextoUtil::tokenizar(p->genero))   score += generos[t]    * 50;
            for (const string& t : TextoUtil::tokenizar(p->director))  score += directores[t] * 25;
            for (const string& t : TextoUtil::tokenizar(p->casting))   score += actores[t]    * 10;
            if (score > 0) candidatos.push_back({score, p});
        }
        sort(candidatos.begin(), candidatos.end(), [](const auto& a, const auto& b){
            if (a.first != b.first) return a.first > b.first;
            return a.second->anio > b.second->anio;
        });
        vector<const Pelicula*> rec;
        rec.reserve(cantidad);
        for (const auto& [s, p] : candidatos) {
            rec.push_back(p);
            if (rec.size() == cantidad) break;
        }
        return rec;
    }
};

// Segunda estrategia concreta de recomendación: películas del mismo año
class RecomendacionPorAnio : public IRecommendationStrategy {
public:
    string nombre() const override { return "Mismo período"; }

    vector<const Pelicula*> recomendar(
        const Usuario& usuario,
        const vector<unique_ptr<Pelicula>>& baseDatos,
        size_t cantidad) const override
    {
        if (usuario.likes.empty()) return {};
        // Calcular el año promedio de los likes
        int sumaAnios = 0, conteo = 0;
        for (const Pelicula* p : usuario.likes)
            if (p->anio > 0) { sumaAnios += p->anio; ++conteo; }
        if (conteo == 0) return {};
        int anioRef = sumaAnios / conteo;

        vector<pair<int, const Pelicula*>> candidatos;
        for (const auto& pp : baseDatos) {
            const Pelicula* p = pp.get();
            if (usuario.leGusta(p) || p->anio == 0) continue;
            int diff = abs(p->anio - anioRef);
            candidatos.push_back({diff, p});
        }
        sort(candidatos.begin(), candidatos.end(), [](const auto& a, const auto& b){
            return a.first < b.first;
        });
        vector<const Pelicula*> rec;
        rec.reserve(cantidad);
        for (const auto& [d, p] : candidatos) {
            rec.push_back(p);
            if (rec.size() == cantidad) break;
        }
        return rec;
    }
};

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  SECCIÓN 12 — BENCHMARK (Programación Paralela)
//
//  Mide speedup de INDEXACIÓN y de BÚSQUEDA por separado.
//  Usa parallelReduce<> (template genérico) para calcular
//  estadísticas de tiempos de búsqueda.
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
struct ResultadoBenchmark {
    double tIndexSec  = 0.0; // indexación 1 hilo
    double tIndexPar  = 0.0; // indexación N hilos
    double tBuscSec   = 0.0; // búsqueda   1 hilo (promedio)
    double tBuscPar   = 0.0; // búsqueda   N hilos (promedio)
    size_t hilosUsados = 0;
    size_t peliculas   = 0;
};

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  SECCIÓN 13 — PLATAFORMA (controlador principal)
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class Plataforma {
    vector<unique_ptr<Pelicula>>      baseDatos_;
    unique_ptr<MotorBusqueda>         motorBusqueda_;
    unique_ptr<IRankingStrategy>      rankingStrategy_;
    unique_ptr<IRecommendationStrategy> recStrategy_;
    Usuario                           usuario_;
    SesionObserver                    sesionObs_;
    LogObserver                       logObs_;

    using Reloj = chrono::high_resolution_clock;
    static double seg(const Reloj::time_point& ini) {
        return chrono::duration<double>(Reloj::now() - ini).count();
    }

    static void limpiarPantalla() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
#pragma GCC diagnostic pop
    }

    static void esperarEnter() {
        cout << "\nPresiona Enter para continuar...";
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }

    static int leerEntero(const string& msg, int lo, int hi) {
        while (true) {
            cout << msg;
            string e;
            if (!getline(cin, e)) exit(0);
            try {
                int v = stoi(e);
                if (v >= lo && v <= hi) return v;
            } catch (...) {}
            cout << "Opcion invalida. Intenta de nuevo.\n";
        }
    }

    static CampoBusqueda leerCampo() {
        cout << "\nBuscar en:\n1. Todo\n2. Titulo y sinopsis\n"
                "3. Director\n4. Casting\n5. Genero\n";
        switch (leerEntero("> ", 1, 5)) {
            case 2: return CampoBusqueda::TituloSinopsis;
            case 3: return CampoBusqueda::Director;
            case 4: return CampoBusqueda::Casting;
            case 5: return CampoBusqueda::Genero;
            default: return CampoBusqueda::Todo;
        }
    }

    void mostrarCabecera() const {
        cout << "=== UTEC STREAMING ===\n"
             << "Usuario: " << usuario_.nombre << "\n"
             << "Peliculas: " << baseDatos_.size()
             << " | Hilos: " << motorBusqueda_->hilosUsados()
             << " | Ranking: " << rankingStrategy_->nombre()
             << " | Recomend: " << recStrategy_->nombre() << "\n";
    }

    void mostrarVerMasTarde() const {
        cout << "\n--- VER MAS TARDE ---\n";
        if (usuario_.verMasTarde.empty()) {
            cout << "No hay peliculas guardadas.\n"; return;
        }
        size_t lim = min<size_t>(5, usuario_.verMasTarde.size());
        for (size_t i = 0; i < lim; ++i)
            cout << i+1 << ". " << usuario_.verMasTarde[i]->titulo << "\n";
    }

    void mostrarRecomendaciones() const {
        cout << "\n--- RECOMENDADOS PARA TI ---\n";
        auto rec = recStrategy_->recomendar(usuario_, baseDatos_, 5);
        if (rec.empty()) {
            cout << "Da Like a una pelicula para activar recomendaciones.\n"; return;
        }
        for (const Pelicula* p : rec)
            cout << "- " << p->titulo << " (" << p->genero << ", " << p->anio << ")\n";
    }

    void menuPelicula(const Pelicula* p) {
        while (true) {
            limpiarPantalla();
            cout << *p << "\n\n";
            cout << "1. Dar Like "
                 << (usuario_.leGusta(p) ? "(Ya te gusta)" : "") << "\n";
            cout << "2. Anadir a Ver Mas Tarde "
                 << (usuario_.estaEnVerMasTarde(p) ? "(Ya esta guardada)" : "") << "\n";
            cout << "3. Volver\n";
            int op = leerEntero("> ", 1, 3);
            if (op == 1) { usuario_.darLike(p); cout << "Like registrado.\n"; esperarEnter(); }
            else if (op == 2) { usuario_.anadirVerMasTarde(p); cout << "Guardada.\n"; esperarEnter(); }
            else break;
        }
    }

    void buscarPeliculas() {
        CampoBusqueda campo = leerCampo();
        cout << "\nIngrese palabra, frase o sub-palabra: ";
        string query; getline(cin, query);
        if (query.empty()) return;

        auto ini = Reloj::now();
        auto resultados = rankingStrategy_->rankear(
            motorBusqueda_->buscar(query, campo), query);
        double tBusq = seg(ini);

        if (resultados.empty()) {
            cout << "\nNo se encontraron resultados.\n"; esperarEnter(); return;
        }

        // Usar Paginador<T, 5> (template con NTTP)
        Paginador<const Pelicula*, 5> paginador(resultados);
        size_t paginaActual = 0;

        while (true) {
            limpiarPantalla();
            cout << "Resultados para '" << query << "' | tiempo: "
                 << fixed << setprecision(4) << tBusq << " s\n"
                 << "Pagina " << paginaActual+1 << "/" << paginador.totalPaginas() << "\n\n";

            auto pagina = paginador.obtenerPagina(paginaActual);
            for (size_t i = 0; i < pagina.size(); ++i)
                cout << i+1 << ". " << pagina[i]->titulo
                     << " (" << pagina[i]->anio << ")\n";

            cout << "\n1-" << pagina.size() << ". Seleccionar";
            if (paginaActual + 1 < paginador.totalPaginas()) cout << "  |  S. Siguientes";
            if (paginaActual > 0)                             cout << "  |  A. Anteriores";
            cout << "  |  V. Volver\n> ";

            string op; getline(cin, op);
            if (op == "V" || op == "v") break;
            if ((op == "S" || op == "s") && paginaActual+1 < paginador.totalPaginas()) ++paginaActual;
            else if ((op == "A" || op == "a") && paginaActual > 0) --paginaActual;
            else {
                try {
                    int idx = stoi(op) - 1;
                    if (idx >= 0 && static_cast<size_t>(idx) < pagina.size())
                        menuPelicula(pagina[static_cast<size_t>(idx)]);
                } catch (...) {}
            }
        }
    }

    void verListaMasTarde() {
        while (true) {
            limpiarPantalla();
            cout << "=== VER MAS TARDE ===\n\n";
            if (usuario_.verMasTarde.empty()) {
                cout << "Todavia no guardaste peliculas.\n"; esperarEnter(); return;
            }
            for (size_t i = 0; i < usuario_.verMasTarde.size(); ++i)
                cout << i+1 << ". " << usuario_.verMasTarde[i]->titulo << "\n";
            cout << "\n0. Volver\n";
            int op = leerEntero("> ", 0, static_cast<int>(usuario_.verMasTarde.size()));
            if (op == 0) break;
            menuPelicula(usuario_.verMasTarde[static_cast<size_t>(op - 1)]);
        }
    }

    // Menú para cambiar estrategias en runtime (demuestra Strategy intercambiable)
    void menuEstrategias() {
        limpiarPantalla();
        cout << "=== CAMBIAR ESTRATEGIAS ===\n\n"
             << "Ranking actual: " << rankingStrategy_->nombre() << "\n"
             << "1. Ranking por Relevancia\n"
             << "2. Ranking por Anio (reciente primero)\n\n"
             << "Recomendacion actual: " << recStrategy_->nombre() << "\n"
             << "3. Recomendacion por Afinidad\n"
             << "4. Recomendacion por Periodo\n"
             << "5. Volver\n";
        int op = leerEntero("> ", 1, 5);
        switch (op) {
            case 1: rankingStrategy_ = make_unique<RankingPorRelevancia>(); break;
            case 2: rankingStrategy_ = make_unique<RankingPorAnio>();       break;
            case 3: recStrategy_     = make_unique<RecomendacionPorAfinidad>(); break;
            case 4: recStrategy_     = make_unique<RecomendacionPorAnio>();     break;
            default: break;
        }
    }

public:
    explicit Plataforma(string nombreUsuario)
        : motorBusqueda_(make_unique<MotorBusqueda>(1)),
          rankingStrategy_(make_unique<RankingPorRelevancia>()),
          recStrategy_(make_unique<RecomendacionPorAfinidad>()),
          usuario_(move(nombreUsuario)),
          logObs_("actividad.log")
    {
        usuario_.agregarObservador(&sesionObs_);
        usuario_.agregarObservador(&logObs_);
    }

    void cargarCSV(const string& ruta, size_t limite, size_t hilos) {
        PeliculaCSVFactory factory;
        auto iL = Reloj::now();
        baseDatos_ = CSVParser::cargarPeliculas(ruta, limite, &factory);
        double tL = seg(iL);

        auto iI = Reloj::now();
        motorBusqueda_ = make_unique<MotorBusqueda>(hilos);
        motorBusqueda_->construir(baseDatos_);
        double tI = seg(iI);

        cout << fixed << setprecision(3)
             << "Lectura: " << tL << " s | Indexacion: " << tI << " s\n";
        usuario_.cargarSesion(baseDatos_);
    }

    void iniciarUI() {
        while (true) {
            limpiarPantalla();
            mostrarCabecera();
            mostrarVerMasTarde();
            mostrarRecomendaciones();
            cout << "\nMENU PRINCIPAL\n"
                    "1. Buscar peliculas\n"
                    "2. Ver Mas Tarde\n"
                    "3. Cambiar estrategias\n"
                    "4. Salir\n";
            int op = leerEntero("> ", 1, 4);
            if      (op == 1) buscarPeliculas();
            else if (op == 2) verListaMasTarde();
            else if (op == 3) menuEstrategias();
            else              break;
        }
    }

    // --------------------------------------------------------
    //  Benchmark completo:
    //   - Indexación 1 hilo vs N hilos (speedup)
    //   - Búsqueda   1 hilo vs N hilos (speedup)
    //   - Estadísticas de tiempos usando template Estadisticas<double>
    // --------------------------------------------------------
    static void ejecutarBenchmark(const string& ruta, size_t limite, size_t hilosParalelos)  {
        cout << "=== BENCHMARK UTEC STREAMING ===\n"
             << "Peliculas: " << (limite ? to_string(limite) : "todas")
             << " | Hilos paralelos: " << hilosParalelos << "\n\n";

        PeliculaCSVFactory factory;
        auto peliculas = CSVParser::cargarPeliculas(ruta, limite, &factory);
        const size_t N = peliculas.size();
        if (N == 0) { cerr << "No se cargaron peliculas.\n"; return; }

        // ---- Benchmark indexación ----
        double tIdxSec = 0.0, tIdxPar = 0.0;
        {
            auto ini = Reloj::now();
            MotorBusqueda mS(1); mS.construir(peliculas);
            tIdxSec = chrono::duration<double>(Reloj::now() - ini).count();
        }
        {
            auto ini = Reloj::now();
            MotorBusqueda mP(hilosParalelos); mP.construir(peliculas);
            tIdxPar = chrono::duration<double>(Reloj::now() - ini).count();
        }

        // ---- Benchmark búsqueda ----
        const vector<string> queries = {
            "action", "love", "war", "comedy", "drama",
            "murder", "family", "new york", "escape", "adventure"
        };

        // Motor con 1 hilo para búsqueda secuencial
        MotorBusqueda mSec(1);  mSec.construir(peliculas);
        // Motor con N hilos para búsqueda paralela
        MotorBusqueda mPar(hilosParalelos); mPar.construir(peliculas);

        vector<double> tiemposSeq, tiemposPar;
        tiemposSeq.reserve(queries.size());
        tiemposPar.reserve(queries.size());

        for (const string& q : queries) {
            {
                auto ini = Reloj::now();
                mSec.buscar(q);
                tiemposSeq.push_back(chrono::duration<double>(Reloj::now() - ini).count());
            }
            {
                auto ini = Reloj::now();
                mPar.buscar(q);
                tiemposPar.push_back(chrono::duration<double>(Reloj::now() - ini).count());
            }
        }

        // Estadísticas usando el template Estadisticas<double>
        auto statSeq = Estadisticas<double>::calcular(tiemposSeq);
        auto statPar = Estadisticas<double>::calcular(tiemposPar);

        // ---- Reporte ----
        cout << fixed << setprecision(4);
        cout << "┌─────────────────────────────────────────────┐\n";
        cout << "│              INDEXACION                     │\n";
        cout << "├──────────────┬──────────┬───────────────────┤\n";
        cout << "│ Modo         │  Hilos   │  Tiempo (s)       │\n";
        cout << "├──────────────┼──────────┼───────────────────┤\n";
        cout << "│ Secuencial   │    1     │  " << setw(8) << tIdxSec << "         │\n";
        cout << "│ Paralelo     │  " << setw(4) << hilosParalelos
             << "    │  " << setw(8) << tIdxPar  << "         │\n";
        cout << "│ Speedup      │    —     │  " << setw(8) << (tIdxSec/max(tIdxPar,1e-9)) << "x        │\n";
        cout << "└──────────────┴──────────┴───────────────────┘\n\n";

        cout << "┌─────────────────────────────────────────────┐\n";
        cout << "│    BUSQUEDA (" << queries.size() << " queries promediadas)        │\n";
        cout << "├──────────────┬──────────┬───────────────────┤\n";
        cout << "│ Modo         │  Hilos   │  Promedio (s)     │\n";
        cout << "├──────────────┼──────────┼───────────────────┤\n";
        cout << "│ Secuencial   │    1     │  " << setw(8) << statSeq.promedio << "         │\n";
        cout << "│ Paralelo     │  " << setw(4) << hilosParalelos
             << "    │  " << setw(8) << statPar.promedio  << "         │\n";
        cout << "│ Speedup      │    —     │  "
             << setw(8) << (statSeq.promedio / max(statPar.promedio, 1e-9)) << "x        │\n";
        cout << "├──────────────┴──────────┴───────────────────┤\n";
        cout << "│ Busqueda seq  min=" << setw(7) << statSeq.minimo
             << "  max=" << setw(7) << statSeq.maximo << "    │\n";
        cout << "│ Busqueda par  min=" << setw(7) << statPar.minimo
             << "  max=" << setw(7) << statPar.maximo << "    │\n";
        cout << "└─────────────────────────────────────────────┘\n";
        cout << "Peliculas indexadas: " << N << "\n";
    }
};

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  SECCIÓN 14 — MAIN
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
static string pedirNombreUsuario() {
    string nombre;
    cout << "========================================\n"
            "       Bienvenido a UTEC STREAMING      \n"
            "========================================\n";
    while (true) {
        cout << "Ingresa tu nombre de usuario: ";
        if (!getline(cin, nombre)) exit(0);
        size_t ini = nombre.find_first_not_of(" \t\r\n");
        size_t fin = nombre.find_last_not_of(" \t\r\n");
        if (ini != string::npos) {
            nombre = nombre.substr(ini, fin - ini + 1);
            break;
        }
    }
    return nombre;
}

int main(int argc, char* argv[]) {
    auto& cfg = Configuracion::instancia();
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if      (arg == "--csv"       && i+1 < argc) cfg.setRutaCSV(argv[++i]);
        else if (arg == "--limit"     && i+1 < argc) cfg.setLimiteCarga(stoull(argv[++i]));
        else if (arg == "--threads"   && i+1 < argc) cfg.setHilos(stoull(argv[++i]));
        else if (arg == "--benchmark" && i+1 < argc) cfg.activarBenchmark(stoull(argv[++i]));
    }
    try {
        if (cfg.modoBenchmark()) {
            Plataforma::ejecutarBenchmark(
                cfg.rutaCSV(), cfg.limiteBenchmark(), cfg.hilos());
            return 0;
        }
        string u = pedirNombreUsuario();
        Plataforma app(u);
        app.cargarCSV(cfg.rutaCSV(), cfg.limiteCarga(), cfg.hilos());
        app.iniciarUI();
    } catch (const exception& e) {
        cerr << "Error critico: " << e.what() << "\n";
        return 1;
    }
    return 0;
}