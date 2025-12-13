#include <cstddef>
#include <cstdlib>
#include <cstring>

struct IntBuffer {
    int* data;
    size_t size;
    size_t capacity;

    IntBuffer() : data(nullptr), size(0), capacity(0) {}

    ~IntBuffer() {
        if (data) free(data);
    }

    void push_back(int val) {
        if (size == capacity) {
            size_t new_cap = (capacity == 0) ? 8 : capacity * 2;
            int* new_data = (int*)realloc(data, new_cap * sizeof(int));
            if (!new_data) return;
            data = new_data;
            capacity = new_cap;
        }
        data[size++] = val;
    }

    void clear() { size = 0; }

    static int compare_ints(const void* a, const void* b) { return (*(int*)a - *(int*)b); }

    void sort() {
        if (size > 1) {
            qsort(data, size, sizeof(int), compare_ints);
        }
    }

    void unique() {
        if (size <= 1) return;
        size_t write_idx = 0;
        for (size_t i = 1; i < size; ++i) {
            if (data[write_idx] != data[i]) {
                write_idx++;
                data[write_idx] = data[i];
            }
        }
        size = write_idx + 1;
    }
};

struct Node {
    char* token;
    IntBuffer doc_ids;
    Node* next;

    Node(const char* t) : next(nullptr) { token = strdup(t); }
    ~Node() {
        free(token);
        if (next) delete next;
    }
};