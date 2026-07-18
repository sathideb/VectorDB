

#include<iostream>
using namespace std;

int main(){

// Create an array of 5 pointers (Rows)
int** array = new int*[5];

for (int i = 0; i < 5; ++i) {
    array[i] = new int[3];
}

for (int i = 0; i < 5; ++i) {
    delete[] array[i]; // Delete columns
}
delete[] array;        // Delete rows




}