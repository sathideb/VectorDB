class SmartPointer {
private:
    int* ptr;

public:
    // 1. Standard Constructor
    explicit SmartPointer(int* p = nullptr) : ptr(p) {}

    // 2. Destructor (The Cleanup Crew)
    ~SmartPointer() { delete[] ptr; }

    // ==========================================
    // 3. THE MOVE CONSTRUCTOR (The Handoff)
    // The '&&' means we are stealing from an object that is giving up its rights.
    // 'noexcept' tells the compiler this steal is 100% safe and won't throw errors.
    // ==========================================
    SmartPointer(SmartPointer&& other) noexcept {
        this->ptr = other.ptr;  // Step 1: You steal the map
        other.ptr = nullptr;    // Step 2: You deactivate Robot A's map
    }

    // ==========================================
    // 4. THE MOVE ASSIGNMENT OPERATOR
    // What if Robot B already had a map, and we want it to take Robot A's map instead?
    // ==========================================
    SmartPointer& operator=(SmartPointer&& other) noexcept {
        // We must check: Is Robot A trying to hand the map to itself? If so, do nothing!
        if (this != &other) { 
            delete[] this->ptr;    // Step 1: Robot B burns its OLD warehouse first!
            this->ptr = other.ptr; // Step 2: Robot B steals Robot A's map
            other.ptr = nullptr;   // Step 3: We deactivate Robot A's map
        }
        return *this;
    }

    // 5. THE LOCKDOWN
    // We explicitly delete the Copy methods so no one can accidentally duplicate the map.
    SmartPointer(const SmartPointer&) = delete;
    SmartPointer& operator=(const SmartPointer&) = delete;
};

int main() {
    // 1. We allocate 4000 bytes on the Heap. Robot A gets the map.
    SmartPointer robotA(new int[1000]); 
    
    // 2. We build Robot B, and hand off the map from A to B.
    // std::move strips A's protection, triggering our Move Constructor!
    SmartPointer robotB = std::move(robotA); 
    
    // RESULT:
    // robotA's pointer is now nullptr (deactivated).
    // robotB now owns the 4000 bytes.
    // When main() ends, only robotB will clean up the heap. No double-free crash!
}