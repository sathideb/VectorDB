
#include<iostream>
using namespace std;

int main(){

    cout<<"System booting..." << endl;

    int* brokenpointer=new int[100] ; // asking for 400 byte of space

    brokenpointer[0]=99;

    cout<<" program ending . memory leak initiated. " <<endl;
    return 0;


}