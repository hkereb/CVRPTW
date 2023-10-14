#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>

using namespace std;

class truck {

public:
    // inicjalizacja

};

//struktura wierzchołków
struct vertex {
    //nr wierzcholka (i=0 magazyn)
    int i;
    //koordynaty
    int x;
    int y;
    //zapotrzebowanie na towar
    int q;
    //początek i koniec okna
    int e;
    int l;
    //czas rozładunku
    int d;
};

//struktura pomocnicza do wczytywania pliku
struct all_data {
    int vehicle_number;
    int capacity;
    vector<vertex> vertexes;
};

// Funkcja do wczytywania danych
all_data readingData(string file_name) {
    all_data temp;
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
        //jeżeli nie jest jeszcze na "CUSTOMER", odczytuje pierwsze all_data
        else if (!readingCustomerData) {
            istringstream ss(line);
            ss >> temp.vehicle_number >> temp.capacity;
        }
        //wczytaj all_data do vertexes
        else {
            istringstream ss(line);
            vertex customer;
            if (ss >> customer.i >> customer.x >> customer.y
            >> customer.q >> customer.e >> customer.l >> customer.d) {
                temp.vertexes.push_back(customer);
            }
        }
    }
    example.close();
    return temp;
}

int main() {
    all_data data_set = readingData("cvrptw1.txt");
    cout << data_set.vehicle_number << " " << data_set.capacity << endl;
    int row_no = 0;
    cout << data_set.vertexes[row_no].i << " " << data_set.vertexes[row_no].x
         << " " << data_set.vertexes[row_no].y << " " << data_set.vertexes[row_no].q << " "
         << data_set.vertexes[row_no].e << " " << data_set.vertexes[row_no].l << " "
         << data_set.vertexes[row_no].d << endl;
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