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

const double DEMOLISH = 50;

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
    double how_much_left_in_truck;
};
struct truck {
    vector<vertex> visited; //odwiedzeni klienci
    int how_much_left; //ile pozostalo towaru
    double distance; //aktualny dystans
    bool acceptable;
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
    int customer_no;
    double cost_f;
};
struct by_value {
    inline bool operator() (const vertex& a, const vertex& b) {
        return (a.value < b.value);
    }
};
struct by_window_end {
    inline bool operator() (const vertex& a, const vertex& b) {
        return (a.window_end < b.window_end);
    }
};
struct possibility {
    int id_i;
    int id_j;
    int id_s_i;
    int id_s_j;
    int truck_id;
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

    truck.visited.push_back(next);

    truck.visited[truck.visited.size() - 1].current_distance = truck.distance;
    truck.visited[truck.visited.size() - 1].waiting_time_to_get_there = waiting_time;
    truck.visited[truck.visited.size() - 1].how_much_left_in_truck = truck.how_much_left;

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

solution singleGRASP (const extracted_data& data_set, const vector<vector<double>>& distance_matrix) {
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
            //sort(candidate_list.begin(), candidate_list.end(), by_value());
            sort(candidate_list.begin(), candidate_list.end(), by_window_end());

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
    temp_solution.customer_no = data_set.vertexes.size() - 1;

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
        temp_solution = singleGRASP(data_set, distance_matrix);
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

//////////////////////////////////////////// LOCAL SEARCH ////////////////////////////////////////////////////////////////////////////

bool performExchangeMove(solution& temp_solution, int i, int j, int t_i, int t_j) {
    // nie wymieniaj magazynów
    if (temp_solution.trucks[t_i].visited[i].vertex_no != 0 && temp_solution.trucks[t_j].visited[j].vertex_no != 0) {
        swap(temp_solution.trucks[t_i].visited[i], temp_solution.trucks[t_j].visited[j]);
        return true;
    }

    return false;
}

bool isRouteValid(solution& temp_solution, int starting_index, const vector<vector<double>> distance_matrix, int truck_id, int capacity) {
    //czy ciezarowka zdazy do kazdego wierzcholka? (wlacznie z magazynem), czy zgadza sie ilosc towaru?
    for (int i = starting_index; i < temp_solution.trucks[truck_id].visited.size(); i++) {
        temp_solution.trucks[truck_id].visited[i].current_distance = 0;
        vertex previous;
        if (i == 0) {
            previous.vertex_no = 0;
            previous.current_distance = 0;
            previous.how_much_left_in_truck = capacity;
        }
        else {
            previous = temp_solution.trucks[truck_id].visited[i - 1];

        }
        vertex current = temp_solution.trucks[truck_id].visited[i];

        if (previous.how_much_left_in_truck < current.commodity_need) {
            return false;
        }
        temp_solution.trucks[truck_id].visited[i].how_much_left_in_truck = previous.how_much_left_in_truck - current.commodity_need;
        double temp_distance = previous.current_distance + distance_matrix[previous.vertex_no][current.vertex_no];
        if (temp_distance > current.window_end) {
            return false;
        }
        double waiting_time =  max(0.0, current.window_start - temp_distance);
        temp_solution.trucks[truck_id].visited[i].current_distance = temp_distance + waiting_time + current.unload_time;
    }

    return true;
}

solution localSearch(const solution& init, const vector<vector<double>> distance_matrix, const extracted_data& data_set) {
    solution temp_solution = init;
    solution best_solution = init;
    bool improvement_found = true;

    while (improvement_found) {
        improvement_found = false;

        for (int t_i = 0; t_i < best_solution.trucks.size(); t_i++) {
            for (int i = 0; i < best_solution.trucks[t_i].visited.size() - 1; i++) {
                for (int t_j = t_i + 1; t_j < best_solution.trucks.size(); t_j++) {
                    for (int j = 0; j < best_solution.trucks[t_j].visited.size() - 1; j++) {
                        if (t_i != t_j) {
                            if (performExchangeMove(temp_solution, i, j, t_i, t_j) ) {
                                if (isRouteValid(temp_solution, i, distance_matrix, t_i, data_set.capacity) &&
                                    isRouteValid(temp_solution, j, distance_matrix, t_j, data_set.capacity) ) {
                                    temp_solution.trucks[t_i].distance = temp_solution.trucks[t_i].visited.back().current_distance;
                                    temp_solution.trucks[t_j].distance = temp_solution.trucks[t_j].visited.back().current_distance;
                                    temp_solution.final_distance = finalDistance(temp_solution.trucks);
                                    temp_solution.truck_no = temp_solution.trucks.size();

                                    if (temp_solution.final_distance < best_solution.final_distance) {
                                        // znalazl polepszenie
                                        best_solution = temp_solution;
                                        improvement_found = true;
                                        saveResultsToFile(best_solution);
                                    }
                                }
                                // nie znalazl polepszenia
                                temp_solution = best_solution;
                            }
                        }
                        else {
                            break;
                        }
                    }
                }
            }
        }
    }

    return best_solution;
}

////////////////////////////////////// DEMOLISH & REBUILD /////////////////////////////////////////////////////////////////////////////////

void updateTruckInfoPostRemoval (truck& truck, const vertex& before_to_remove) {
    truck.how_much_left = before_to_remove.how_much_left_in_truck;
    truck.distance = before_to_remove.current_distance;
}

vector<vertex> partlyDemolishSolution (solution& init) {
    vector<vertex> removed;
    int how_many = init.customer_no * DEMOLISH / 100; // DEMOLISH to wartosc const

    for (int t = init.trucks.size() - 1; t >= 0; t--) {
        for (int v = init.trucks[t].visited.size() - 1; v >= 0; v--) {
            if (init.trucks[t].visited[v].vertex_no != 0) {
                if (v - 1 >= 0) {
                    updateTruckInfoPostRemoval(init.trucks[t], init.trucks[t].visited[v - 1]);
                }
                removed.push_back(init.trucks[t].visited[v]);
                init.trucks[t].visited.erase(init.trucks[t].visited.begin() + v);
                how_many--;
                if (how_many == 0) {
                    init.truck_no = init.trucks.size();
                    if (init.trucks[t].visited.empty()) {
                        init.trucks.erase(init.trucks.begin() + t);
                    }
                    return removed;
                }
            }
            else {
                init.trucks[t].visited.erase(init.trucks[t].visited.begin() + v);
            }
        }
        if (init.trucks[t].visited.empty()) {
            init.trucks.erase(init.trucks.begin() + t);
        }
    }
}

solution singleRebuildByGreedy(const solution& incomplete_solution, const vector<vertex>& removed_vertexes, const extracted_data& data_set, const vector<vector<double>>& distance_matrix) {
    solution temp_solution = incomplete_solution;
    temp_solution.acceptable = false;

    int next_vertex_no;
    vertex full_previous_vertex = temp_solution.trucks.back().visited.back();
    vertex full_next_vertex;

    truck current_truck = temp_solution.trucks.back();
    vector<truck> trucks = temp_solution.trucks;
    trucks.pop_back();

    vector<vertex> candidate_list = removed_vertexes;

    vector<vertex> top_candidates;

    int new_truck = 0;

    while (!candidate_list.empty()) {
        if (candidate_list.size() == 1) {
            next_vertex_no = 0; // bo ten jeden ma id 0
            full_next_vertex = candidate_list[next_vertex_no];
        }
        else {
            transform(candidate_list.begin(), candidate_list.end(), candidate_list.begin(),
                      [&](vertex& candidate) {
                          candidate.value = countValue(distance_matrix, full_previous_vertex, candidate, current_truck);
                          return candidate;
                      }
            );
            sort(candidate_list.begin(), candidate_list.end(), by_value());

            next_vertex_no = 0;

            next_vertex_no = 0;
            full_next_vertex = candidate_list[next_vertex_no];
        }

        if ((distanceSimulation(current_truck, full_previous_vertex, full_next_vertex) +
             distance_matrix[full_next_vertex.vertex_no][0] >
             data_set.vertexes[0].window_end) ||

            (current_truck.how_much_left < full_next_vertex.commodity_need) ||

            (current_truck.distance + distance_matrix[full_previous_vertex.vertex_no][full_next_vertex.vertex_no] >
             full_next_vertex.window_end)) {

            if (new_truck > 0) {
                cout << "Exited, no feasible solution." << endl;
                exit(0);
            } else {
                new_truck++;
                current_truck = updateTruckInfoPostShipment(current_truck, full_previous_vertex,
                                                            data_set.vertexes[0]);
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
    trucks.push_back(current_truck);

    temp_solution.trucks = trucks;
    temp_solution.truck_no = trucks.size();
    temp_solution.final_distance = finalDistance(trucks);
    temp_solution.acceptable = true;

    return temp_solution;
}

solution rebuildByGreedy(const solution& init, const solution& incomplete_solution, vector<vertex> removed_vertexes, const extracted_data& data_set, const vector<vector<double>>& distance_matrix) {
    solution best_solution = init;
    saveResultsToFile(best_solution);

    for (int i = 0; i < 1; i++) {
        solution temp_solution = singleRebuildByGreedy(incomplete_solution, removed_vertexes, data_set, distance_matrix);

        if (temp_solution.truck_no < best_solution.truck_no || temp_solution.final_distance < best_solution.final_distance) {
            best_solution = temp_solution;
            saveResultsToFile(best_solution);
        }
    }

    return best_solution;
}


int main() {
    string filename = "RC2_410.txt";
///////////////////// LINUX ///////////////////////
/*int main(int argc, char* argv[]) {
    if (argc != 2) {
        cout << "Wrong amount of arguments" << endl;
        cout << "./program_name.out input_name.txt" << endl;
        return 1;
    }
    string filename = argv[1];*/
///////////////////////////////////////////////////
    auto start_time = chrono::steady_clock::now();  // start pomiaru czasu

    initialSaveResultsToFile();
    cout.precision(10);
    extracted_data data_set = readingData(filename);
    vector<vector<double>> distance_matrix = createDistanceMatrix(data_set);

    /////////////////////////////////////////////////////////////////////////
    const int time = 1000; // TU NALEZY ZMIENIC CZAS [SEKUNDY]
    ////////////////////////////////////////////////////////////////////////

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
        exit(0);
    });

    ////////////////////////////// WYWOLANIE GRASP /////////////////////////////////////////////////////
    solution GRASP_solution = singleGRASP(data_set, distance_matrix);
    //solution GRASP_solution = GRASP(data_set, distance_matrix, time);
    saveResultsToFile(GRASP_solution); //GRASP_solution jest na biezaco nadpisywane w pliku przy kazdym znalezieniu polepszajacego wyniku
    cout << GRASP_solution.truck_no << " " << GRASP_solution.final_distance << endl;
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    /////////////////////////// WYWOLANIE LOCAL SEARCH /////////////////////////////////////////////////
    //solution LS_solution = localSearch(GRASP_solution, distance_matrix, data_set);
    //cout << LS_solution.truck_no << " " << LS_solution.final_distance << endl;
    //saveResultsToFile(LS_solution);
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    /////////////////////////// WYWOLANIE DEMOLISH & REBUILD /////////////////////////////////////////////////////////////////////////
    solution GRASP_og = GRASP_solution;
    vector<vertex> removed = partlyDemolishSolution(GRASP_solution);
    solution REBUILT_solution = rebuildByGreedy(GRASP_og, GRASP_solution, removed, data_set, distance_matrix);
    saveResultsToFile(REBUILT_solution);
    //cout << REBUILT_solution.truck_no << " " << REBUILT_solution.final_distance << endl;

//    solution LS_solution = localSearch(REBUILT_solution, distance_matrix, data_set);
//    saveResultsToFile(LS_solution);

//    cout << LS_solution.truck_no << " " << LS_solution.final_distance << endl;



    timerThread.join();

    return 0;
}
