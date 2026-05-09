#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iomanip>

using namespace std;

// ++++++++++++++++++++++++++++++++++++++++++
// 1. MODELO DE DATOS
// ++++++++++++++++++++++++++++++++++++++++++
class Pelicula {
public:
    string titulo;
    string sinopsis;
    string director;
    string genero;
    string casting;
    int anio;

    Pelicula() : anio(0) {}

    // Constructor con Semantica de Movimiento
    Pelicula(string&& t, string&& s, string&& d, string&& g, string&& c, int a)
        : titulo(move(t)), sinopsis(move(s)), director(move(d)), genero(move(g)), casting(move(c)), anio(a) {}

    // Sobrecarga de operador para imprimir sin tildes para evitar errores en Windows
    friend ostream& operator<<(ostream& os, const Pelicula& p) {
        os << "========================================\n"
           << " TITULO:   " << p.titulo << " (" << p.anio << ")\n"
           << " DIRECTOR: " << p.director << "\n"
           << " GENERO:   " << p.genero << "\n"
           << " CASTING:  " << p.casting << "\n"
           << "----------------------------------------\n"
           << " SINOPSIS: " << p.sinopsis << "\n"
           << "========================================";
        return os;
    }
};

class Usuario {
public:
    string nombre;
    unordered_set<Pelicula*> likes;
    vector<Pelicula*> verMasTarde;

    Usuario(string n) : nombre(n) {}
};

// ++++++++++++++++++++++++++++++++++++++++++
// 2. ESTRUCTURA DE DATOS: SUFFIX TRIE
// ++++++++++++++++++++++++++++++++++++++++++
class NodoTrie {
public:
    unordered_map<char, NodoTrie*> hijos;
    unordered_set<Pelicula*> peliculas;

    // Destructor recursivo para evitar Memory Leaks
    ~NodoTrie() {
        for (auto& par : hijos) {
            delete par.second;
        }
    }
};

class SuffixTrie {
private:
    NodoTrie* raiz;

    string limpiarTexto(string texto) {
        string resultado;
        for (char c : texto) {
            if (isalnum(c) || isspace(c)) {
                resultado += tolower(c);
            }
        }
        return resultado;
    }

    void insertarPalabraOSufijo(const string& texto, Pelicula* p) {
        NodoTrie* actual = raiz;
        for (char c : texto) {
            if (actual->hijos.find(c) == actual->hijos.end()) {
                actual->hijos[c] = new NodoTrie();
            }
            actual = actual->hijos[c];
            actual->peliculas.insert(p);
        }
    }

public:
    SuffixTrie() {
        raiz = new NodoTrie();
    }

    ~SuffixTrie() {
        delete raiz;
    }

    void indexar(Pelicula* p) {
        string textoCompleto = p->titulo + " " + p->sinopsis + " " + p->director + " " + p->genero + " " + p->casting;
        string textoLimpio = limpiarTexto(textoCompleto);

        stringstream ss(textoLimpio);
        string palabra;

        while (ss >> palabra) {
            for (size_t i = 0; i < palabra.length(); ++i) {
                insertarPalabraOSufijo(palabra.substr(i), p);
            }
        }
    }

    unordered_set<Pelicula*> buscar(const string& query) {
        string queryLimpia = limpiarTexto(query);
        NodoTrie* actual = raiz;

        for (char c : queryLimpia) {
            if (actual->hijos.find(c) == actual->hijos.end()) {
                return unordered_set<Pelicula*>();
            }
            actual = actual->hijos[c];
        }
        return actual->peliculas;
    }
};

// ++++++++++++++++++++++++++++++++++++++++++
// 3. MOTOR Y PLATAFORMA
// ++++++++++++++++++++++++++++++++++++++++++
class Plataforma {
private:
    vector<Pelicula*> baseDatos;
    SuffixTrie motorBusqueda;
    Usuario* usuarioActual;

    void limpiarPantalla() {
        #ifdef _WIN32
            system("cls");
        #else
            system("clear");
        #endif
    }

    vector<Pelicula*> rankearResultados(const unordered_set<Pelicula*>& resultados, const string& query) {
        vector<Pelicula*> ordenados(resultados.begin(), resultados.end());
        string qLower = query;
        transform(qLower.begin(), qLower.end(), qLower.begin(), ::tolower);

        auto rankingLambda = [&qLower](Pelicula* a, Pelicula* b) {
            string tA = a->titulo; string tB = b->titulo;
            transform(tA.begin(), tA.end(), tA.begin(), ::tolower);
            transform(tB.begin(), tB.end(), tB.begin(), ::tolower);

            bool enTituloA = tA.find(qLower) != string::npos;
            bool enTituloB = tB.find(qLower) != string::npos;

            if (enTituloA && !enTituloB) return true;
            if (!enTituloA && enTituloB) return false;

            return a->anio > b->anio;
        };

        sort(ordenados.begin(), ordenados.end(), rankingLambda);
        return ordenados;
    }

    // Parser avanzado para CSV
    vector<string> parsearLineaCSV(const string& linea) {
        vector<string> columnas;
        string valorActual = "";
        bool dentroDeComillas = false;

        for (char c : linea) {
            if (c == '"') {
                dentroDeComillas = !dentroDeComillas;
            } else if (c == ',' && !dentroDeComillas) {
                columnas.push_back(valorActual);
                valorActual = "";
            } else {
                valorActual += c;
            }
        }
        columnas.push_back(valorActual);
        return columnas;
    }

public:
    Plataforma() {
        usuarioActual = new Usuario("Admin");
    }

    ~Plataforma() {
        for (Pelicula* p : baseDatos) {
            delete p;
        }
        delete usuarioActual;
    }

    void cargarCSV(const string& ruta) {
        ifstream archivo(ruta);

        if (!archivo.is_open()) {
            cout << "Error critico: No se pudo encontrar el archivo '" << ruta << "'.\n";
            cout << "Asegurate de que el .csv este en la misma carpeta que el ejecutable.\n";
            cout << "Presiona Enter para salir...";
            cin.get();
            exit(1);
        }

        string linea;
        getline(archivo, linea); // Descartar cabecera

        string registroCompleto = "";
        bool dentroDeComillas = false;
        int contador = 0;

        while (getline(archivo, linea)) {
            registroCompleto += linea;

            for (char c : linea) {
                if (c == '"') dentroDeComillas = !dentroDeComillas;
            }

            if (dentroDeComillas) {
                registroCompleto += " ";
                continue;
            }

            vector<string> columnas = parsearLineaCSV(registroCompleto);
            registroCompleto = "";

            if (columnas.size() < 8) continue;

            // Extraemos basandonos en el orden real de tu archivo
            string anio_str = columnas[0];
            string titulo = columnas[1];
            string director = columnas[3];
            string casting = columnas[4];
            string genero = columnas[5];
            string sinopsis = columnas[7];

            int anio = 0;
            try {
                if (!anio_str.empty()) anio = stoi(anio_str);
            } catch (...) {
                anio = 0;
            }

            if (!titulo.empty()) {
                Pelicula* p = new Pelicula(move(titulo), move(sinopsis), move(director), move(genero), move(casting), anio);
                baseDatos.push_back(p);
                motorBusqueda.indexar(p);
                contador++;

                // 1. INDICADOR DE PROGRESO: Muestra en la misma linea cuantas peliculas van
                if (contador % 10 == 0) {
                    cout << "Indexando base de datos... " << contador << " peliculas procesadas.\r";
                }

                // 2. LIMITE DE PRUEBA: Frena en 500 para cargar rapido
                // (Borra este 'if' completo para tu presentacion final)
                if (contador >= 500) {
                    break;
                }
            }
        }

        archivo.close();
        cout << "\nExito: Base de datos cargada. Peliculas indexadas: " << contador << "\n";
    }

    void generarRecomendaciones() {
        cout << "\n--- RECOMENDADOS PARA TI ---\n";
        if (usuarioActual->likes.empty()) {
            cout << "Da 'Like' a peliculas para recibir recomendaciones.\n";
            return;
        }

        unordered_set<string> generosGustados;
        for (Pelicula* p : usuarioActual->likes) {
            generosGustados.insert(p->genero);
        }

        int contador = 0;
        for (Pelicula* p : baseDatos) {
            if (usuarioActual->likes.find(p) == usuarioActual->likes.end()) {
                if (generosGustados.find(p->genero) != generosGustados.end()) {
                    cout << "- " << p->titulo << " (" << p->genero << ")\n";
                    contador++;
                    if (contador == 3) break;
                }
            }
        }
    }

    void mostrarMenuPelicula(Pelicula* p) {
        while (true) {
            limpiarPantalla();
            cout << *p << "\n\n";
            cout << "1. Dar Like " << (usuarioActual->likes.count(p) ? "(Ya te gusta)" : "") << "\n";
            cout << "2. Anadir a Ver Mas Tarde\n";
            cout << "3. Volver a resultados\n";
            cout << "> ";
            string op_str;
            cin >> op_str;

            if (op_str == "1") {
                usuarioActual->likes.insert(p);
            } else if (op_str == "2") {
                usuarioActual->verMasTarde.push_back(p);
            } else if (op_str == "3") {
                break;
            }
        }
    }

    void iniciarUI() {
        string input;
        while (true) {
            limpiarPantalla();
            cout << "=== UTEC STREAMING ===\n";
            cout << "Hola, " << usuarioActual->nombre << "\n";
            generarRecomendaciones();

            cout << "\nVer mas tarde: " << usuarioActual->verMasTarde.size() << " peliculas.\n";
            cout << "\nIngrese su busqueda (o escriba 'exit' para salir): ";

            // Limpiar buffer si es necesario antes de getline
            if(cin.peek() == '\n') cin.ignore();
            getline(cin, input);

            if (input == "exit") break;
            if (input.empty()) continue;

            auto resultadosRaw = motorBusqueda.buscar(input);
            if (resultadosRaw.empty()) {
                cout << "No se encontraron resultados. Presione Enter para continuar...\n";
                cin.get();
                continue;
            }

            auto resultados = rankearResultados(resultadosRaw, input);
            int paginaActual = 0;
            int totalPaginas = (resultados.size() + 4) / 5;

            while (true) {
                limpiarPantalla();
                cout << "Resultados para '" << input << "' (Pagina " << paginaActual + 1 << "/" << totalPaginas << ")\n";

                int inicio = paginaActual * 5;
                int fin = min((int)resultados.size(), inicio + 5);

                for (int i = inicio; i < fin; ++i) {
                    cout << i + 1 << ". " << resultados[i]->titulo << " (" << resultados[i]->anio << ")\n";
                }

                cout << "\nOpciones:\n";
                cout << "[1-" << fin << "] Seleccionar pelicula\n";
                if (paginaActual < totalPaginas - 1) cout << "[S] Siguientes 5\n";
                if (paginaActual > 0) cout << "[A] Anteriores 5\n";
                cout << "[V] Volver al menu principal\n> ";

                string opcion;
                cin >> opcion;

                if (opcion == "V" || opcion == "v") break;
                else if ((opcion == "S" || opcion == "s") && paginaActual < totalPaginas - 1) paginaActual++;
                else if ((opcion == "A" || opcion == "a") && paginaActual > 0) paginaActual--;
                else {
                    try {
                        int idx = stoi(opcion) - 1;
                        if (idx >= inicio && idx < fin) {
                            mostrarMenuPelicula(resultados[idx]);
                        }
                    } catch (...) {
                        // Ignorar input invalido
                    }
                }
            }
        }
    }
};

int main() {
    Plataforma AppStreaming;
    // Carga utilizando la ruta relativa
    AppStreaming.cargarCSV("wiki_movie_plots_deduped.csv");

    // Pausa rapida para que veas cuantas peliculas cargaron antes de entrar a la UI
    cout << "Presiona Enter para iniciar la plataforma...";
    cin.get();

    AppStreaming.iniciarUI();
    return 0;
}