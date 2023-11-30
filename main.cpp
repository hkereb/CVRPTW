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
    double waiting_time_to_get_there; // ile czasu czekala ciezarowka na otwarcie okna
    double current_distance;
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
struct possibility {
    int id_i;
    int id_j;
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
inline double countValue(const vector<vector<double>>& distance_matrix, const vertex previous, const vertex next, const truck current_truck) {
    double capacity_violation = (current_truck.how_much_left < next.commodity_need) ? 1.0 : 0.0; //za malo towaru
    double window_end_violation = (current_truck.distance + distance_matrix[previous.vertex_no][next.vertex_no] > next.window_end) ? 1.0 : 0.0; //za pozno przyjechal
    double window_start_waiting = max( (next.window_start - current_truck.distance + distance_matrix[previous.vertex_no][next.vertex_no]), 0.0); //ile czeka

    double value = (A * next.unload_time) + (B * distance_matrix[previous.vertex_no][next.vertex_no])
     + (C * capacity_violation) + (D * window_end_violation) + (E * window_start_waiting);

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

inline truck updateTruckInfoPostShipment (truck& truck, const vertex previous, vertex next) {
    truck.how_much_left -= next.commodity_need;

    double distance_pc = distance(next.x, next.y, previous.x, previous.y);
    truck.distance += distance_pc;

    double waiting_time = max(0.0, next.window_start - truck.distance); //dopiero przyjechał, czy czeka?
    truck.distance += waiting_time + next.unload_time;

    next.current_distance = truck.distance;
    next.waiting_time_to_get_there = waiting_time;
    truck.visited.push_back(next);

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

solution singleGRASP (const extracted_data& data_set, const vector<vector<double>>& distance_matrix, const int time_limit) {
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

    int new_truck = 0;

    //GRASP main body
    while (!candidate_list.empty()) {
        if (candidate_list.size() == 1) {
            next_vertex_no = 0; //bo ten jeden ma id 0
            full_next_vertex = candidate_list[next_vertex_no];
        }
        else {
            //obliczanie miar dobroci wzgledem aktualnego wierzcholka
            transform(candidate_list.begin(), candidate_list.end(), candidate_list.begin(),
                       [&](vertex& candidate) {
                           candidate.value = countValue(distance_matrix, full_previous_vertex, candidate, current_truck);
                           return candidate;
                       }
            );
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

            if (new_truck > 0) {
                cout << "Exited, no feasible solution." << endl;
                exit(0);
            }
            else {
                new_truck++;
                current_truck = updateTruckInfoPostShipment(current_truck, full_previous_vertex, data_set.vertexes[0]);
                next_vertex_no = 0;
                full_previous_vertex = data_set.vertexes[0];
                trucks.push_back(current_truck);
                current_truck.distance = 0.0;
                current_truck.how_much_left = data_set.capacity;
                current_truck.visited.clear();
                continue;
            }
        }
        new_truck = 0;
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

void saveResultsToFile(const solution& best_solution) {
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

solution GRASP (const extracted_data& data_set, const vector<vector<double>> distance_matrix, const int time_limit) {
    solution best_solution;
    best_solution.truck_no = data_set.vehicle_number * 5; //dla pierwszego porownania z temp_solution
    best_solution.acceptable = false;
    solution temp_solution;
    for (auto start = chrono::steady_clock::now(), now = start; now < start + chrono::seconds{time_limit}; now = chrono::steady_clock::now()) {
        temp_solution = singleGRASP(data_set, distance_matrix, time_limit);
        if ((temp_solution.acceptable) && (best_solution.truck_no > temp_solution.truck_no)) {
            best_solution = temp_solution;
            saveResultsToFile(best_solution);
        }
    }
    return best_solution;
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

double distanceSimulationLS(const extracted_data& data_set, vector<vector<double>> distance_matrix, double distance, int i, int s_i, int s_j, int j) {
    //odejmij dystans z lukow ktore maja zostac zamienione (REALNE ID Z DATA SET)
    distance -= (distance_matrix[i][s_i] - data_set.vertexes[s_i].unload_time - data_set.vertexes[s_i].waiting_time_to_get_there)
            - (distance_matrix[j][s_j] - data_set.vertexes[s_j].unload_time - data_set.vertexes[s_j].waiting_time_to_get_there);

    //dodaj dystans lukow ktory dodajesz w miejsce starych
    double waiting_time_1 = max(0.0, data_set.vertexes[j].window_start - (data_set.vertexes[i].current_distance + distance_matrix[i][j]) );
    double waiting_time_2= max(0.0, data_set.vertexes[s_j].window_start - (data_set.vertexes[s_i].current_distance + distance_matrix[i][j]) );
    distance += (distance_matrix[i][j] + waiting_time_1 + data_set.vertexes[j].unload_time)
                + (distance_matrix[s_i][s_j] + waiting_time_2 + data_set.vertexes[s_j].unload_time);
    return distance;
}

vector<possibility> findPossibilities(const solution& solution, const extracted_data& data_set, vector<vector<double>> distance_matrix) { // per trasa
    vector<possibility> possibilities;
    for (int truck_id = 0; truck_id < solution.trucks.size(); ++truck_id) {
        for (int i = 0; i < solution.trucks[truck_id].visited.size(); ++i) {
                for (int j = i + 1; j < solution.trucks[truck_id].visited.size(); ++j) {
                //sprawdzanie czy dwa luki moga byc zmodyfikowane

                //symulacja final distance

                double simulated_distance; // dopisz reszte
                if (distanceSimulationLS(data_set, distance_matrix, solution.trucks[truck_id].distance, solution.trucks[truck_id].visited[i].vertex_no, solution.trucks[truck_id].visited[j].vertex_no, solution.trucks[truck_id].visited[i + 1].vertex_no, solution.trucks[truck_id].visited[j + 1].vertex_no) < solution.trucks[truck_id].distance) {
                    //podmien dystans
                }
            }
        }
    }
    return possibilities;
}

solution localSearch(const solution init, vector<vector<double>> distance_matrix) {
    solution temp, best_solution;
    int i = 0;
    int last_i = 0;
    bool no_improvement = false;

    while(!no_improvement) { // i wrocil sie bez zadnej poprawy

    }
    return best_solution;
}

int main() {
    string filename = "cvrptw1.txt";
/////////////////////LINUX///////////////////////
/*int main(int argc, char* argv[]) {
    if (argc != 2) {
        cout << "Wrong amount of arguments" << endl;
        cout << "./program_name.out input_name.txt" << endl;
        return 1;
    }
    string filename = argv[1];*/
/////////////////////////////////////////////////
    auto start_time = chrono::steady_clock::now();  // start pomiaru czasu

    initialSaveResultsToFile();
    cout.precision(5);
    extracted_data data_set = readingData(filename);
    vector<vector<double>> distance_matrix = createDistanceMatrix(data_set);

    /////////////////////////////////////////////////////////////////////////
    const int time = 300; // TU NALEZY ZMIENIC CZAS [SEKUNDY]
    ////////////////////////////////////////////////////////////////////////

    std::thread timerThread([&start_time, &time]() {
        auto current_time = chrono::steady_clock::now();
        auto elapsed_time = chrono::duration_cast<chrono::seconds>(current_time - start_time).count();

        while (elapsed_time <= time) {
            //cout << "Elapsed time: " << elapsed_time << " seconds." << endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));  // Poczekaj 1 sekundę
            current_time = chrono::steady_clock::now();
            elapsed_time = chrono::duration_cast<chrono::seconds>(current_time - start_time).count();
        }

        //cout << "Time limit exceeded." << endl;
        exit(0);
    });

    solution best_solution = GRASP(data_set, distance_matrix, time);
    saveResultsToFile(best_solution); //best_solution jest na biezaco nadpisywane w pliku przy kazdym znalezieniu polepszajacego wyniku (czyli rzadko)

    timerThread.join();

    return 0;
}
