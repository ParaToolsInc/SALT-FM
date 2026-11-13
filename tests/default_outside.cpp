// Issue #119: a special member function defaulted outside its class has
// no body in braces, so it gets no TAU calls, as one defaulted in its
// class gets none. main must be instrumented.
struct Defaulted {
    Defaulted();
    ~Defaulted();
    Defaulted &operator=(const Defaulted &);
    int v;
};
Defaulted::Defaulted() = default;
Defaulted::~Defaulted() = default;
Defaulted &Defaulted::operator=(const Defaulted &) = default;

int main() {
    Defaulted s;
    Defaulted t;
    t = s;
    return 0;
}
