#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <iomanip>

using namespace std;

class truck {

public:
    // inicjalizacja

};

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

int main() {
    extracted_data data_set = readingData("cvrptw1.txt");
    cout << data_set.vehicle_number << " " << data_set.capacity << endl;
    int row_no = 0;
    cout << data_set.vertexes[row_no].vertex_no << " " << data_set.vertexes[row_no].x
         << " " << data_set.vertexes[row_no].y << " " << data_set.vertexes[row_no].commodity_need << " "
         << data_set.vertexes[row_no].window_start << " " << data_set.vertexes[row_no].window_end << " "
         << data_set.vertexes[row_no].unload_time << endl;
    cout << endl;

    double distance_matrix[data_set.vertexes.size()][data_set.vertexes.size()];
    for (int w = 0; w < data_set.vertexes.size(); w++) {
        for (int k = 0; k < data_set.vertexes.size(); k++) {
            if (w == k) {
                distance_matrix[w][k] = 0;
                cout << distance_matrix[w][k] << " ";
                continue;
            }
            distance_matrix[w][k] = distance(data_set.vertexes[w].x, data_set.vertexes[w].y,
                                             data_set.vertexes[k].x, data_set.vertexes[k].y);
            cout << distance_matrix[w][k] << " ";
        }
        cout << endl;
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