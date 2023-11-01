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

using namespace std;

//struktura wierzchołków
struct vertex {
    //nr wierzcholka (vertex_no=0 magazyn)
    int vertex_no;
    //koordynaty
    int x;
    int y;
    //zapotrzebowanie na towar
    int commodity_need;
    //początek i koniec okna
    int window_start;
    int window_end;
    //czas rozładunku
    int unload_time;
    double value;
};

struct truck {
    int vehicle_no;
    int how_much_left;
    double distance;
    vector<vertex> visited;
};

//struktura pomocnicza do wczytywania pliku
struct extracted_data {
    int vehicle_number;
    int capacity;
    vector<vertex> vertexes;
};

// Funkcja do wczytywania danych
extracted_data readingData(string file_name) {
    extracted_data temp;
    fstream example;
    example.open(file_name, ios::in);
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
        //jeżeli nie jest jeszcze na "CUSTOMER", odczytuje pierwsze extracted_data
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
double distance (double x, double y, double a, double b) {
    return sqrt(pow(x - a, 2) + pow(y - b, 2));
}
double roundToFiveDecimalPlaces(double distance) {
    //dystans zawsze z 5 miejscami po przecinku
    //cout << fixed << setprecision(5) << distance << endl;
    return distance;
}

//im mniejsze value tym wieksze prawdopodobienstwo na wylosowanie przez GRASP
double countValue(vertex current, vertex next, double current_time) {
    double value = distance(current.x, current.y, next.x, next.y)
            + next.unload_time + (next.window_start - current_time);

    return value;
}
struct by_value {
    inline bool operator() (const vertex& a, const vertex& b)
    {
        return (a.value < b.value);
    }
};
//vertex chooseVertex(const vector<vertex>& candidate_list) {
//    random_device rd;
//    default_random_engine generator(rd());
//
//    vector<double> weights;
//    for (const vertex& v : candidate_list) {
//        weights.push_back(1.0 / v.value);
//    }
//
//    discrete_distribution<int> distribution(weights.begin(), weights.end());
//
//    int selected_index = distribution(generator);
//
//    return candidate_list[selected_index];
//}
int chooseVertex(const vector<vertex>& candidate_list) {
    random_device rd;
    default_random_engine generator(rd());

    vector<double> weights;
    for (const vertex& v : candidate_list) {
        weights.push_back(1.0 / v.value);
    }

    discrete_distribution<int> distribution(weights.begin(), weights.end());

    int selected_index = distribution(generator);


    return selected_index;
}
void updateTruckInfoPostShipment (truck& truck, vertex previous, vertex current) {
    truck.visited.push_back(current);
    truck.how_much_left = (truck.how_much_left) - (current.commodity_need);
    double distance_pc = distance(current.x, current.y, previous.x, previous.y);
    truck.distance = truck.distance + distance_pc; //tutaj dopiero przyjechał, czy czeka?
    double waiting_time = current.window_start - truck.distance;
    truck.distance = truck.distance + waiting_time + current.unload_time;
}
int main() {
    srand(time(0));

    cout.precision(5);
    extracted_data data_set = readingData("cvrptw1.txt");
    cout << data_set.vehicle_number << " " << data_set.capacity << endl;
    int row_no = 0;
    cout << data_set.vertexes[row_no].vertex_no << " " << data_set.vertexes[row_no].x
         << " " << data_set.vertexes[row_no].y << " " << data_set.vertexes[row_no].commodity_need << " "
         << data_set.vertexes[row_no].window_start << " " << data_set.vertexes[row_no].window_end << " "
         << data_set.vertexes[row_no].unload_time << endl;
    cout << endl;
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    vector<vector<double>> distance_matrix;

    for (int w = 0; w < data_set.vertexes.size(); w++) {
        vector<double> temp;
        for (int k = 0; k < data_set.vertexes.size(); k++) {
            if (w == k) {
                temp.push_back(0);
                //cout << temp[k] << " ";
                continue;
            }
            temp.push_back(distance(data_set.vertexes[w].x, data_set.vertexes[w].y,
                                    data_set.vertexes[k].x, data_set.vertexes[k].y));
            //cout << temp[k] << " ";
        }
        distance_matrix.push_back(temp);
        //cout << endl;
    }
//    the delivery time for a customer can be defined as the maximum of:
//    -the earliest allowed delivery time for this customer
//    -the departure time from the previous customer plus the travel time to the current customer


    //     step 1 dla graspa

    int previous_vertex = 0;
    int current_vertex_no = 0; //magazyn
    vertex full_previous_vertex = data_set.vertexes[0];
    vertex full_current_vertex = data_set.vertexes[0];
    truck current_truck;
    current_truck.vehicle_no = 0;
    current_truck.distance = 0;
    current_truck.how_much_left = data_set.capacity;
    current_truck.visited.push_back(data_set.vertexes[current_vertex_no]);
    double current_time = 0; //czas startowy
    vector<vertex> candidate_list = data_set.vertexes;
    candidate_list.erase(candidate_list.begin());
    vector<truck> trucks;
    trucks.push_back(current_truck);
    extracted_data data_set_copy = data_set;
    while(!candidate_list.empty() || (trucks.size() > data_set.vehicle_number)) {

        for (int i = 0; i < candidate_list.size(); i++) {
            candidate_list[i].value = countValue(data_set.vertexes[current_vertex_no], candidate_list[i], 0);
        }
        sort(candidate_list.begin(), candidate_list.end(), by_value());
        current_vertex_no = chooseVertex(candidate_list);
        full_current_vertex = data_set_copy.vertexes[candidate_list[current_vertex_no].vertex_no];

        if (current_truck.how_much_left < full_current_vertex.commodity_need) {
            current_vertex_no = 0;
            previous_vertex = 0;
            trucks.push_back(current_truck);
            continue;
        }
        updateTruckInfoPostShipment(current_truck, full_previous_vertex, full_current_vertex);
        //candidate_list.erase(current_vertex_no);
    }

    return 0;
}

/*
0. cvrptw1
1.
2. VEHICLE   -> ile mamy ciężarówek do maksymalnego wykorzystania
3. NUMBER     CAPACITY   -> pojemność jednej ciężarówki
4.  10        1000
5.
6. CUSTOMER
7. CUST NO.  XCOORD.  YCOORD.    DEMAND   READY TIME  DUE DATE   SERVICE TIME
8.
9.     0     0        0          0        0           100          0
 .   1     0        10         10       200         300         10
 .   2     0        20         10       10          120         10
 .   3     0        30         10       20          180         10
 */


/*
 * liczbatras(Liczba ciężarówek) całkowitadługośćwszystkichtras(Droga+Rozładunek+Oczekiwanie)\n
    nr_1._wierzchołka_w_1.trasie    nr_2._wierzchołka_w_1.trasie ...\n, <- Wierzchołki dowiedzone przez ciężarówke np.: 0 -> 2 -> 4 -> 3 -> 1 -> 0
    ...
    nr_1._wierzchołka_w_ntej.trasie nr_2._wierzchołka_w_ntej.trasie ...\n
 */