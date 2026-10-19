
#include <gtest/gtest.h>

// Our Robot Class (The one you built!)
class SmartPointer {
private:
    int* ptr;
public:
    explicit SmartPointer(int* p = nullptr) : ptr(p) {}
    ~SmartPointer() { delete ptr; }
    int& operator*() { return *ptr; }
};

// The Test: Proving the robot works
TEST(RobotTests, HandlesMemoryCorrectly) {
    // We put a value into the Robot
    SmartPointer* robot = new SmartPointer(new int(50));
    
    // We verify the value is 50
    EXPECT_EQ(**robot, 50);
    
    // We clean up the robot itself (not the int, the robot does that)
    delete robot;
}
/*
1. What is nullptr?
In Java/Python, you use null. In C++, NULL used to be a macro (a legacy trick), but nullptr is the modern, type-safe keyword introduced in C++11.

It is a literal that represents "this pointer points to nothing."

Using nullptr prevents a specific class of bugs where the compiler gets confused between the number 0 and a memory address. Think of it as: "I am intentionally pointing at empty space."

explicit SmartPointer(int* p = nullptr) : ptr(p) {}
By default, C++ is "helpful" in a dangerous way: it will automatically convert an int* into a SmartPointer if it thinks it can. explicit tells the compiler: "Stop! Do not do any automatic conversions." If you didn't have explicit, someone could accidentally do this: SmartPointer myPtr = new int(10); and the compiler would silently create a SmartPointer for them. explicit forces you to be intentional: SmartPointer myPtr(new int(10));. It saves you from thousands of hours of debugging "magic" compiler errors.

3. What are we actually doing in that class?
We are implementing the RAII (Resource Acquisition Is Initialization) pattern. Here is the life cycle:

The Initialization (ptr = p): When you create the SmartPointer robot, you give it a raw memory address (the p). We store that address in our private ptr variable. Now the robot "owns" that memory.

The Access (operator*): We overload the * symbol. This is just a fancy way of saying: "If someone uses *myRobot, give them the actual int value that I am protecting."

The "Tear-down" (~SmartPointer): This is the magic. Because the robot owns the memory, when the robot dies (goes out of scope), the Destructor (~) is automatically called by the system. Inside that, we write delete ptr.
*/