#include <iostream>
using namespace std;

int main() {
    cout << "Print 1" << endl;
    cout << "Print 2" << endl;
    cout << "Print 3" << endl;
    cout << "Print 4" << endl;
    cout << "Print 5" << endl;

    int* brokenpointer = new int[100]; // Your 1 leak

    cout << "Print 6" << endl;
    cout << "Print 7" << endl;
    cout << "Print 8" << endl;
    cout << "Print 9" << endl;
    cout << "Print 10" << endl;

    return 0;
}
