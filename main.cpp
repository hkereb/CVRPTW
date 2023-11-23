#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <iomanip>
#include <random>
#include <chrono>
#include <thread>
#include <algorithm>

using namespace std;

const double RCL = 10;
const double A = 0.00005;
const double B = 0.00005;
const double C = 100;
const double D = 100;
const double E = 0.00008;

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
    vector<vertex> visited; //odwiedzeni klienci
    int how_much_left; //ile pozostalo towaru
    double distance; //aktualny dystans
};
//struktura pomocnicza do wczytywania pliku
struct extracted_data {
    vector<vertex> vertexes; //lista wszystkich klientow
    int vehicle_number; //liczba ciezarowek
    int capacity; //ich zapotrzebowanie na towar
};
struct solution {
    vector<truck> trucks; //lista wszystkich uzytych ciezarowek
    int truck_no; //liczba ciezarowek
    double final_distance; //sumaryczny koszt
    bool acceptable; //czy dopuszczalne
};
struct by_value {
    inline bool operator() (const vertex& a, const vertex& b) {
        return (a.value < b.value);
    }
};

double getCurrentTime(const chrono::high_resolution_clock::time_point& start_time) {
    auto current_time = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::seconds>(current_time - start_time).count();
    return static_cast<double>(duration);
}

//wczytywanie danych
extracted_data readingData(const string file_name) {
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

vector<vector<double>> createDistanceMatrix(const extracted_data data_set) {
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
inline double countValue(const vector<vector<double>> distance_matrix, const vertex previous, const vertex next, const truck current_truck) {
    double capacity_violation = (current_truck.how_much_left < next.commodity_need) ? 1.0 : 0.0; //za malo towaru
    double window_end_violation = (current_truck.distance + distance_matrix[previous.vertex_no][next.vertex_no] > next.window_end) ? 1.0 : 0.0; //za pozno przyjechal
    double window_start_waiting = max( (next.window_start - current_truck.distance + distance_matrix[previous.vertex_no][next.vertex_no]), 0.0); //ile czeka

    double value = (A * next.unload_time) + (B * distance_matrix[previous.vertex_no][next.vertex_no])
     + (C * capacity_violation) + (D * window_end_violation) + (E * window_start_waiting);

    return value;
}

default_random_engine generator(random_device{}());

inline int chooseVertex(const vector<vertex> candidate_list) {
    vector<double> weights; //zamiana value na wagi z zakresu (0;1)
    for (const vertex& v : candidate_list) {
        weights.push_back(1.0 / v.value);
    }

    discrete_distribution<int> distribution(weights.begin(), weights.end()); //rozklad prawdopodobienstwa

    return distribution(generator); //wybrany indeks wierzcholka
}

inline truck updateTruckInfoPostShipment (truck truck, const vertex previous, const vertex next) {
    truck.visited.push_back(next);

    truck.how_much_left -= next.commodity_need;

    double distance_pc = distance(next.x, next.y, previous.x, previous.y);
    truck.distance += distance_pc;

    double waiting_time = max(0.0, next.window_start - truck.distance); //dopiero przyjechał, czy czeka?
    truck.distance += waiting_time + next.unload_time;

    return truck;
}

inline double distanceSimulation (const truck truck, const vertex previous, const vertex next) {
    double distance_pc = distance(next.x, next.y, previous.x, previous.y);
    double waiting_time = max(0.0, next.window_start - ( truck.distance + distance_pc) ); //dopiero przyjechał, czy czeka?
    double simulated_distance = truck.distance + distance_pc + waiting_time + next.unload_time;

    return simulated_distance;
}

inline double finalDistance(const vector<truck> trucks) {
    double distance = 0.0;
    for (int i = 0; i < trucks.size(); i++) {
        distance += trucks[i].distance;
    }
    return distance;
}

solution SingleGRASP (const extracted_data& data_set, const vector<vector<double>>& distance_matrix, const int time_limit) {
    int next_vertex_no = 0; //magazyn
    vertex full_previous_vertex = data_set.vertexes[0];
    vertex full_next_vertex = data_set.vertexes[0];

    truck current_truck;
    current_truck.distance = 0;
    current_truck.how_much_left = data_set.capacity;
    vector<truck> trucks;

    vector<vertex> candidate_list = data_set.vertexes;
    candidate_list.erase(candidate_list.begin()); //usuwamy magazyn
    vector<vertex> top_candidates;

    solution temp_solution;
    temp_solution.acceptable = false;
    solution best_solution;

    //GRASP main body
    while (!candidate_list.empty()) {
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
            int top_percent = RCL;
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

            current_truck = updateTruckInfoPostShipment(current_truck, full_previous_vertex, data_set.vertexes[0]);
            next_vertex_no = 0;
            full_previous_vertex = data_set.vertexes[0];
            trucks.push_back(current_truck);
            current_truck.distance = 0.0;
            current_truck.how_much_left = data_set.capacity;
            current_truck.visited.clear();
            continue;
        }

        current_truck = updateTruckInfoPostShipment(current_truck, full_previous_vertex, full_next_vertex);
        full_previous_vertex = full_next_vertex;
        candidate_list.erase(candidate_list.begin() + next_vertex_no);
    }
    current_truck = updateTruckInfoPostShipment(current_truck, full_previous_vertex, data_set.vertexes[0]);
    trucks.push_back(current_truck); //ostatnia ciezarowka (w przypadku braku klientow)

    temp_solution.trucks = trucks;
    temp_solution.truck_no = trucks.size();
    temp_solution.final_distance = finalDistance(trucks);
    temp_solution.acceptable = true;

    return temp_solution;
}

solution GRASP (const extracted_data data_set, const vector<vector<double>> distance_matrix, const int time_limit) {
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

void saveResultsToFile(const solution best_solution) {
    std::ofstream outputFile("wyniki.txt", std::ios::trunc);
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
void initialSaveResultsToFile() {
    ofstream outputFile("wyniki.txt");
    if (!outputFile.is_open()) {
        cerr << "Error opening output file." << std::endl;
        return;
    }
    outputFile << "-1";

    outputFile.close();
}

int main() {
//    cout.precision(5);
//    auto start_time = chrono::high_resolution_clock::now(); // Start stopera
//
//    extracted_data data_set = readingData("r1_2_1.txt");
//    /////////////////////////////////////////////////////////////////////////////////////////////////////////
//    vector<vector<double>> distance_matrix = createDistanceMatrix(data_set);
//
//    int time = 10;
//    solution best_solution = GRASP(data_set, distance_matrix, start_time, time);
//
//    double elapsed_time = getCurrentTime(start_time);
//    cout << "Elapsed Time: " << elapsed_time << " seconds" << endl;
//
//    saveResultsToFile(best_solution);

    auto start_time = chrono::steady_clock::now();  // Początkowy czas pomiaru

    initialSaveResultsToFile();
    cout.precision(5);
    extracted_data data_set = readingData("C101.txt");
    vector<vector<double>> distance_matrix = createDistanceMatrix(data_set);

    int time = 10;

    // Utwórz wątek do monitorowania czasu
    std::thread timerThread([&start_time, &time]() {
        auto current_time = chrono::steady_clock::now();
        auto elapsed_time = chrono::duration_cast<chrono::seconds>(current_time - start_time).count();

        while (elapsed_time <= time) {
            cout << "Elapsed time: " << elapsed_time << " seconds." << endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));  // Poczekaj 1 sekundę
            current_time = chrono::steady_clock::now();
            elapsed_time = chrono::duration_cast<chrono::seconds>(current_time - start_time).count();
        }

        cout << "Time limit exceeded." << endl;
        exit(0); // Lub inna odpowiednia akcja po przekroczeniu czasu
    });

    solution best_solution = GRASP(data_set, distance_matrix, time);
    saveResultsToFile(best_solution);

    // Zamknij wątek czasowy przed zakończeniem programu
    timerThread.join();

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
 * losowanie
 *
 */