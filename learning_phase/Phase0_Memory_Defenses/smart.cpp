#include <iostream>

class SmartPointer {
private:
    // The dangerous raw pointer is locked inside the private shell.
    // Nobody outside this class can touch it or mess it up.
    int* ptr; 

public:
    // 1. THE CONSTRUCTOR (Acquisition)
    // When the robot is created, we hand it a piece of Heap memory.
    explicit SmartPointer(int* p = nullptr) {
        ptr = p;
        std::cout << "[Robot] Waking up. Taking control of Heap memory." << std::endl;
    }

    // 2. THE DESTRUCTOR (The RAII Magic)
    // The exact millisecond this robot falls off the Stack and dies, 
    // the OS automatically calls this function. 
    ~SmartPointer() {
        std::cout << "[Robot] Dying. Automatically cleaning up the Heap..." << std::endl;
        delete ptr; 
    }

    // 3. OPERATOR OVERLOADING
    // We want our robot to feel and act exactly like a normal pointer.
    // This lets us use the * symbol to read/write the actual memory.
    int& operator*() {
        return *ptr;
    }
};

int main() {
    std::cout << "--- System Booting ---" << std::endl;

    // We create an artificial scope (a mini-stack) using curly braces
    { 
        std::cout << "Entering isolated scope..." << std::endl;

        // We ask the OS for memory on the Heap, but instead of holding it 
        // ourselves, we hand the key directly to our new Stack robot.
        SmartPointer robot(new int(99)); 

        // We can manipulate the Heap memory through the robot
        std::cout << "Value stored in Heap: " << *robot << std::endl;
        
        *robot = 100; // Changing the value
        std::cout << "Updated value in Heap: " << *robot << std::endl;

        std::cout << "Exiting isolated scope..." << std::endl;
    } // CRITICAL MOMENT: The scope ends here. The robot is destroyed.

    // Notice we NEVER typed the word 'delete' in our main function!

    std::cout << "--- System Shutting Down ---" << std::endl;
    return 0;
}
/*
int&        operator*        ()        { ... }
^^^^        ^^^^^^^^^^^       ^^        ^^^^^
return      function          params    body
type        NAME

int& — this is the return type, full stop. Nothing special. It means "this function returns a reference to an int." Same as if you wrote int& getValue() for any ordinary function.
operator* — this whole thing, together, is the function's name. operator is a keyword that tells the compiler "what follows is the symbol being overloaded," and * says which symbol. operator* is one name, the same way getValue is one name.
() — parameter list. Empty because (per Question 3 from before) unary member operators take zero explicit params — *this supplies the operand.
{ ... } — body, same as any function.

So the & has nothing to do with operator or *. It's not glued to the operator symbol. It's just an ordinary return-by-reference, exactly like:
ReturnType operator SYMBOL (Parameters) { body }
*/