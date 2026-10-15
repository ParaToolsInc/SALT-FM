// A routine name with a '[': tests/sif/operator_index.tau excludes
// Vec::operator[] by name.
struct Vec {
    int data[2] = {1, 2};
    int &operator[](int i);
};

int &Vec::operator[](int i) {
    return data[i];
}

int main() {
    Vec v;
    return v[0] - 1;
}
