#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <iomanip>
#include <cstdlib>
#include <regex>
#include <random>
#include <chrono>

using namespace std;

//struktura wierzchołków
struct vertex {
    int vertex_no; //nr klienta (vertex_no = 0 magazyn)
    int x; //koordynaty
    int y;
    int commodity_need; //zapotrzebowanie na towar
    int window_start; //początek okna
    int window_end; //koniec okna
    int unload_time; //czas rozładunku
    double value; //miara dobroci
};
struct truck {
    int how_much_left; //ile pozostalo towaru
    double distance; //aktualny dystans
    vector<vertex> visited; //odwiedzeni klienci
};
//struktura pomocnicza do wczytywania pliku
struct extracted_data {
    int vehicle_number; //liczba ciezarowek
    int capacity; //ich zapotrzebowanie na towar
    vector<vertex> vertexes; //lista wszystkich klientow
};
struct solution {
    bool acceptable; //czy dopuszczalne
    vector<truck> trucks; //lista wszystkich uzytych ciezarowek
    int truck_no; //liczba ciezarowek
    double final_distance; //sumaryczny koszt
};
struct by_value {
    inline bool operator() (const vertex& a, const vertex& b) {
        return (a.value < b.value);
    }
};

//wczytywanie danych
extracted_data readingData(const string& file_name) {
    extracted_data temp;
    ifstream example(file_name);
    if (!example.good()) {
        cout << "File error! Can't open file." << endl;
        exit(1);
    }

    string line;
    bool readingCustomerData = false; //czy wczytuje vertexes

    while (getline(example, line)) {
        if (line.empty() || line.find_first_not_of(" \t") == string::npos) {
            continue; // Pomiń puste linie lub takie z samymi słowami
        }
        //jeżeli trafi na "CUSTOMER", dostosowuje tryb odczytu do formatu vertexes
        if (line.find("CUSTOMER") != string::npos) {
            readingCustomerData = true;
        }
        //jeżeli nie jest jeszcze na "CUSTOMER", odczytuje pierwsze informacje z pliku
        else if (!readingCustomerData) {
            istringstream ss(line);
            ss >> temp.vehicle_number >> temp.capacity;
        }
        //wczytaj extracted_data do vertexes
        else {
            istringstream ss(line);
            vertex customer;
            if (ss >> customer.vertex_no >> customer.x >> customer.y
                   >> customer.commodity_need >> customer.window_start >> customer.window_end >> customer.unload_time) {
                temp.vertexes.push_back(customer);
            }
        }
    }
    example.close();

    return temp;
}

//obliczanie dystansu miedzy dwoma wierzchołkami (pitagoras)
inline double distance (double x, double y, double a, double b) {
    return sqrt(pow(x - a, 2) + pow(y - b, 2));
}

vector<vector<double>> createDistanceMatrix(const extracted_data& data_set) {
    vector<vector<double>> distance_matrix;

    for (const auto& w : data_set.vertexes) {
        vector<double> temp;
        for (const auto& k : data_set.vertexes) {
            if (&w == &k) {
                temp.push_back(0);
                continue;
            }
            temp.push_back(distance(w.x, w.y, k.x, k.y));
        }
        distance_matrix.push_back(temp);
    }

    return distance_matrix;
}

//im mniejsze value tym wieksze prawdopodobienstwo na wylosowanie przez GRASP
inline double countValue(const vector<vector<double>>& distance_matrix, const vertex& previous, const vertex& next, const truck& current_truck) {
    double capacity_violation = (current_truck.how_much_left < next.commodity_need) ? 1.0 : 0.0; //za malo towaru
    double window_end_violation = (current_truck.distance + distance_matrix[previous.vertex_no][next.vertex_no] > next.window_end) ? 1.0 : 0.0; //za pozno przyjechal
    double window_start_waiting = max( (next.window_start - current_truck.distance + distance_matrix[previous.vertex_no][next.vertex_no]), 0.0 );

    double value = (0.0000001 * next.unload_time) + (0.0000003 * distance_matrix[previous.vertex_no][next.vertex_no])
     + (1.0 * capacity_violation) + (1.0 * window_end_violation) + (0.00009 * window_start_waiting);
//0.0001 0.8 0.004 0.0 naj 135 454872
//0.0000001 0.0000003 0.9 1.0 0.0
//0.005 0.005 0.3 0.5 0.8 oryginal
    return value;
}

default_random_engine generator(random_device{}());
inline int chooseVertex(const vector<vertex>& candidate_list) {
    vector<double> weights; //zamiana value na wagi z zakresu (0;1)
    for (const vertex& v : candidate_list) {
        weights.push_back(1.0 / v.value);
    }

    discrete_distribution<int> distribution(weights.begin(), weights.end()); //rozklad prawdopodobienstwa

    return distribution(generator); //wybrany indeks wierzcholka
}

inline void updateTruckInfoPostShipment (truck& truck, const vertex& previous, const vertex& next) {
    truck.visited.push_back(next);

    truck.how_much_left -= next.commodity_need;

    double distance_pc = distance(next.x, next.y, previous.x, previous.y);
    truck.distance += distance_pc;

    double waiting_time = max(0.0, next.window_start - truck.distance); //dopiero przyjechał, czy czeka?
    truck.distance += waiting_time + next.unload_time;
}

inline double distanceSimulation (const truck& truck, const vertex& previous, const vertex& next) {
    double distance_pc = distance(next.x, next.y, previous.x, previous.y);
    double waiting_time = max(0.0, next.window_start - truck.distance); //dopiero przyjechał, czy czeka?
    double simulated_distance = truck.distance + distance_pc + waiting_time + next.unload_time;

    return simulated_distance;
}

inline double finalDistance(const vector<truck>& trucks) {
    double distance = 0.0;
    for (int i = 0; i < trucks.size(); i++) {
        distance += trucks[i].distance;
    }
    return distance;
}

solution SingleGRASP (const extracted_data& data_set, const vector<vector<double>>& distance_matrix, const int time_limit) {
    auto start_time = chrono::high_resolution_clock::now(); //start stoper

    int next_vertex_no = 0; //magazyn
    vertex full_previous_vertex = data_set.vertexes[0];
    vertex full_next_vertex = data_set.vrtexes[0];

    truck current_truck;
    current_truck.distance = 0;
    current_truck.how_much_left = data_set.capacity;
    vector<truck> trucks;

    vector<vertex> candidate_list = data_set.vertexes;
    candidate_list.erase(candidate_list.begin()); //usuwamy magazyn
    vector<vertex> top_candidates;

    solution temp_solution;
    temp_solution.acceptable = true;
    solution best_solution;

    //GRASP main body
    while (!candidate_list.empty()) {
        //przerwanie pracy po przekroczeniu czasu
        auto current_time = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::seconds>(current_time - start_time).count();
        if (duration > time_limit) {
            temp_solution.acceptable = false;
            break;
        }

        if (candidate_list.size() == 1) {
            next_vertex_no = 0; //bo ten jeden ma id 0
            full_next_vertex = candidate_list[next_vertex_no];
        }
        else {
            //obliczanie miar dobroci wzgledem aktualnego wierzcholka
            for (auto& candidate : candidate_list) {
                candidate.value = countValue(distance_matrix, full_previous_vertex, candidate, current_truck);
            }
            sort(candidate_list.begin(), candidate_list.end(), by_value());

            //wyznaczenie top x% kandydatow z ktorych bedzie losowany kolejny klient
            int top_percent = 10;
            int last_top_id = candidate_list.size() * top_percent / 100;
            top_candidates.clear();
            top_candidates.assign(candidate_list.begin(), candidate_list.begin() + last_top_id + 1); //+1 bo last top to int i chodzi o zaokraglenie

            //losowanie kolejnego klienta
            next_vertex_no = chooseVertex(top_candidates);
            full_next_vertex = candidate_list[next_vertex_no];
        }

        //warunki na zakonczenie pracy aktualnej ciezarowki
        if ((distanceSimulation(current_truck, full_previous_vertex, full_next_vertex) + distance_matrix[full_next_vertex.vertex_no][0]
             > data_set.vertexes[0].window_end) //nie wroci do magazynu po kolejnym wierzcholku (ciezarowka konczy prace, za nia wchodzi kolejna)

            || (current_truck.how_much_left < full_next_vertex.commodity_need) //nie ma wystarczajaco duzo towaru (ciezarowka konczy prace, za nia wchodzi kolejna)

            || (current_truck.distance + distance_matrix[full_previous_vertex.vertex_no][full_next_vertex.vertex_no]
                > full_next_vertex.window_end) ) { //nie dojedzie do klienta przed zamknieciem okna dostawy (ciezarowka konczy prace, za nia wchodzi kolejna)

            updateTruckInfoPostShipment(current_truck, full_previous_vertex, data_set.vertexes[0]);
            next_vertex_no = 0;
            full_previous_vertex = data_set.vertexes[0];
            trucks.push_back(current_truck);
            current_truck.distance = 0.0;
            current_truck.how_much_left = data_set.capacity;
            current_truck.visited.clear();
            continue;
        }

        updateTruckInfoPostShipment(current_truck, full_previous_vertex, full_next_vertex);
        full_previous_vertex = full_next_vertex;
        candidate_list.erase(candidate_list.begin() + next_vertex_no);
    }
    updateTruckInfoPostShipment(current_truck, full_previous_vertex, data_set.vertexes[0]);
    trucks.push_back(current_truck); //ostatnia ciezarowka (w przypadku braku klientow)

    temp_solution.trucks = trucks;
    temp_solution.truck_no = trucks.size();
    temp_solution.final_distance = finalDistance(trucks);

    return temp_solution;
}

solution GRASP (const extracted_data& data_set, const vector<vector<double>>& distance_matrix, const int time_limit){
    solution best_solution;
    best_solution.truck_no = data_set.vehicle_number * 5; //dla pierwszego porownania z temp_solution
    best_solution.acceptable = false;
    solution temp_solution;
    for (auto start = chrono::steady_clock::now(), now = start; now < start + chrono::seconds{time_limit}; now = chrono::steady_clock::now()) {
        temp_solution = SingleGRASP(data_set, distance_matrix, time_limit);
        if ((temp_solution.acceptable) && (best_solution.truck_no > temp_solution.truck_no)) {
            best_solution = temp_solution;
        }
    }

    return best_solution;
}

void saveResultsToFile(const solution& best_solution) {
    ofstream outputFile("wyniki.txt");
    if (!outputFile.is_open()) {
        cerr << "Error opening output file." << std::endl;
        return;
    }

    if (best_solution.acceptable) {
        outputFile << fixed << setprecision(5);
        outputFile << best_solution.truck_no << " " << best_solution.final_distance << endl;
        for (int g = 0; g < best_solution.trucks.size(); g++) {
            for (int y = 0; y < best_solution.trucks[g].visited.size(); y++) {
                if (best_solution.trucks[g].visited[y].vertex_no != 0) {
                    outputFile << best_solution.trucks[g].visited[y].vertex_no << " ";
                }
            }
            if (g < best_solution.trucks.size() - 1) {
                outputFile << endl;
            }
        }
    }
    else {
        outputFile << "-1";
    }

    outputFile.close();
}

int main() {
    cout.precision(5);
    extracted_data data_set = readingData("RC2_4_4.txt");
    /////////////////////////////////////////////////////////////////////////////////////////////////////////
    vector<vector<double>> distance_matrix = createDistanceMatrix(data_set);

    int time = 5;
    solution best_solution = GRASP(data_set, distance_matrix, time);
    saveResultsToFile(best_solution);


    return 0;
}

// Wartości zmiennej time:
// 1min = 60s
// 2min = 120s
// 3min = 180s
// 4min = 240s
// 5min = 300s

//NOTATNIK BY ANTEK
/*
 * ZROBIONE - Brak uwzględnienia w przyjeździe ciężarówki po czasie L do wierzchołka
 * ZROBIONE - Wyplucie ostatecznego wyniku po 3 lub 5 minutach
 * ZROBIONE - Zamkniecie pliku
 * ZROBIONE - na etapie porównywania rozwiązań w poszukiwaniu najlepszego należy zingnorować rozwiązanie któego ostatnia ciężarówka ma wartość -1 w dystans (warunek, że jak wszystkie są -1)
 *           Liczenie ile jest -1 i porównanie z ilością rozwiązań, jak tyle samo to rozwiązania nie ma
 * Obsluga bledow
 * ZROBIONE - greedy
 * zaokraglenia
 * zliczanie ilości rozwiąń
 * ZROBIONE - napisać instrukcje jak korzystać z programu
 *
 *
 */