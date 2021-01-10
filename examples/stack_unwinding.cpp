#include<iostream>
using namespace std;

void a() {
    int foo = 1;
}

void tester( int arg) {
    int foo = 2;
    cout<<"1: "<<foo<<endl;
    cout<<"2: "<<foo<<endl;
    cout<<"3: "<<foo<<endl;
    cout<<"4: "<<foo<<endl;
    cout<<"5: "<<foo<<endl;
    cout<<"argument "<<arg<<endl;
    a();
}

void c() {
    int foo = 3;
    tester( 5);
}

void d() {
    int foo = 4;
    c();
}

void e() {
    int foo = 5;
    d();
}

void f() {
    int foo = 6;
    e();
}

int main() {
    f();
}
