#include<iostream>
void a() {
    int foo = 1;
}

void tester() {
    int foo = 2;
    a();
}

void c() {
    int foo = 3;
    tester();
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
