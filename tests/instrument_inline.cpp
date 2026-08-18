#include <iostream>

class Widget {
public:
    int method_in_class_body(int value) {
        std::cout << value << std::endl;
        return value;
    }
};

inline int free_fn_marked_inline() {
    return 1234;
}

int main() {
    Widget w;
    w.method_in_class_body(1);
    std::cout << free_fn_marked_inline() << std::endl;
    return 0;
}
