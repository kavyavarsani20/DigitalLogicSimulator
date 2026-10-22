#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <fstream>
#include <iomanip>

// ============================================================================
// 1. DATA TYPES & CUSTOM EXCEPTION HIERARCHY
// ============================================================================

enum LogicState {
    LOW = 0,
    HIGH = 1,
    UNKNOWN = -1
};

inline std::string stateToString(LogicState state) {
    if (state == HIGH) return "1";
    if (state == LOW) return "0";
    return "X";
}

class CircuitException : public std::runtime_error {
public:
    explicit CircuitException(const std::string& msg) : std::runtime_error(msg) {}
};

class InvalidConnectionException : public CircuitException {
public:
    explicit InvalidConnectionException(const std::string& msg) 
        : CircuitException("Invalid Connection Error: " + msg) {}
};

class EvaluationException : public CircuitException {
public:
    explicit EvaluationException(const std::string& msg) 
        : CircuitException("Evaluation Error: " + msg) {}
};

// ============================================================================
// 2. ABSTRACT BASE CLASS: Component
// ============================================================================

class Component {
protected:
    std::string name;
    std::vector<LogicState> inputs;
    std::vector<LogicState> outputs;
    double propagationDelayNs;

public:
    Component(std::string name, size_t numInputs, size_t numOutputs, double delay = 1.0)
        : name(std::move(name)), 
          inputs(numInputs, LogicState::LOW), 
          outputs(numOutputs, LogicState::LOW),
          propagationDelayNs(delay) {}

    virtual ~Component() = default;

    // Pure virtual method (Runtime Polymorphism)
    virtual void evaluate() = 0;

    virtual void setInput(size_t index, LogicState state) {
        if (index >= inputs.size()) {
            throw InvalidConnectionException("Input pin out of range for " + name);
        }
        inputs[index] = state;
    }

    LogicState getOutput(size_t index = 0) const {
        if (index >= outputs.size()) {
            throw InvalidConnectionException("Output pin out of range for " + name);
        }
        return outputs[index];
    }

    std::string getName() const { return name; }
    double getDelay() const { return propagationDelayNs; }
    size_t getNumInputs() const { return inputs.size(); }
    size_t getNumOutputs() const { return outputs.size(); }

    // Operator Overloading for stream insertion
    friend std::ostream& operator<<(std::ostream& os, const Component& comp) {
        os << "[" << comp.name << " | Delay: " << comp.propagationDelayNs << "ns | Out: " 
           << stateToString(comp.outputs[0]) << "]";
        return os;
    }
};

// ============================================================================
// 3. COMBINATIONAL GATES (Inheritance & Polymorphism)
// ============================================================================

class AndGate : public Component {
public:
    AndGate(std::string name, double delay = 1.2) : Component(std::move(name), 2, 1, delay) {}

    void evaluate() override {
        outputs[0] = (inputs[0] == HIGH && inputs[1] == HIGH) ? HIGH : LOW;
    }
};

class OrGate : public Component {
public:
    OrGate(std::string name, double delay = 1.5) : Component(std::move(name), 2, 1, delay) {}

    void evaluate() override {
        outputs[0] = (inputs[0] == HIGH || inputs[1] == HIGH) ? HIGH : LOW;
    }
};

class NotGate : public Component {
public:
    NotGate(std::string name, double delay = 0.8) : Component(std::move(name), 1, 1, delay) {}

    void evaluate() override {
        outputs[0] = (inputs[0] == HIGH) ? LOW : HIGH;
    }
};

class XorGate : public Component {
public:
    XorGate(std::string name, double delay = 1.8) : Component(std::move(name), 2, 1, delay) {}

    void evaluate() override {
        outputs[0] = (inputs[0] != inputs[1]) ? HIGH : LOW;
    }
};

class NandGate : public Component {
public:
    NandGate(std::string name, double delay = 1.2) : Component(std::move(name), 2, 1, delay) {}

    void evaluate() override {
        outputs[0] = (inputs[0] == HIGH && inputs[1] == HIGH) ? LOW : HIGH;
    }
};

// ============================================================================
// 4. SEQUENTIAL ELEMENTS (Clock & D Flip-Flop)
// ============================================================================

class Clock : public Component {
private:
    double halfPeriodNs;

public:
    Clock(std::string name, double periodNs) 
        : Component(std::move(name), 0, 1, 0.0), halfPeriodNs(periodNs / 2.0) {
        outputs[0] = LOW;
    }

    void evaluate() override {
        outputs[0] = (outputs[0] == LOW) ? HIGH : LOW;
    }

    double getHalfPeriod() const { return halfPeriodNs; }
};

class DFlipFlop : public Component {
private:
    LogicState previousClock;
    LogicState storedValue;

public:
    // Pin 0: D (Data), Pin 1: CLK (Clock)
    // Out 0: Q, Out 1: Q_bar
    DFlipFlop(std::string name, double delay = 2.5) 
        : Component(std::move(name), 2, 2, delay), 
          previousClock(LOW), 
          storedValue(LOW) {
        outputs[0] = LOW;
        outputs[1] = HIGH;
    }

    void evaluate() override {
        LogicState currentClock = inputs[1];
        LogicState dInput = inputs[0];

        // Rising-edge trigger condition (0 -> 1 transition)
        if (previousClock == LOW && currentClock == HIGH) {
            storedValue = dInput;
        }

        outputs[0] = storedValue;
        outputs[1] = (storedValue == HIGH) ? LOW : HIGH;
        previousClock = currentClock;
    }
};

// ============================================================================
// 5. CIRCUIT & NETLIST MANAGER
// ============================================================================

struct Connection {
    std::shared_ptr<Component> source;
    size_t sourcePin;
    std::shared_ptr<Component> destination;
    size_t destPin;
};

class Circuit {
private:
    std::string circuitName;
    std::vector<std::shared_ptr<Component>> components;
    std::vector<Connection> connections;

public:
    explicit Circuit(std::string name) : circuitName(std::move(name)) {}

    void addComponent(const std::shared_ptr<Component>& comp) {
        components.push_back(comp);
    }

    void connect(const std::shared_ptr<Component>& src, size_t srcPin,
                 const std::shared_ptr<Component>& dest, size_t destPin) {
        if (!src || !dest) {
            throw InvalidConnectionException("Null component encountered in netlist.");
        }
        if (srcPin >= src->getNumOutputs()) {
            throw InvalidConnectionException("Source pin index out of range for " + src->getName());
        }
        if (destPin >= dest->getNumInputs()) {
            throw InvalidConnectionException("Destination pin index out of range for " + dest->getName());
        }
        connections.push_back({src, srcPin, dest, destPin});
    }

    void propagate() {
        // Phase 1: Forward signals through wires
        for (const auto& conn : connections) {
            LogicState outState = conn.source->getOutput(conn.sourcePin);
            conn.destination->setInput(conn.destPin, outState);
        }

        // Phase 2: Polymorphic evaluation of all components
        for (auto& comp : components) {
            comp->evaluate();
        }
    }

    double calculateTotalCriticalPathDelay() const {
        double maxDelay = 0.0;
        for (const auto& comp : components) {
            maxDelay += comp->getDelay();
        }
        return maxDelay;
    }
};

// ============================================================================
// 6. INTERACTIVE CONSOLE ROUTINES
// ============================================================================

void runManualGateTester() {
    std::cout << "\n--------------------------------------------\n";
    std::cout << "         MANUAL LOGIC GATE TESTER           \n";
    std::cout << "--------------------------------------------\n";
    std::cout << "1. AND Gate\n";
    std::cout << "2. OR Gate\n";
    std::cout << "3. NOT Gate\n";
    std::cout << "4. XOR Gate\n";
    std::cout << "5. NAND Gate\n";
    std::cout << "Select a gate to test (1-5): ";

    int choice;
    if (!(std::cin >> choice)) return;

    std::shared_ptr<Component> gate;
    if (choice == 1)      gate = std::make_shared<AndGate>("User_AND");
    else if (choice == 2) gate = std::make_shared<OrGate>("User_OR");
    else if (choice == 3) gate = std::make_shared<NotGate>("User_NOT");
    else if (choice == 4) gate = std::make_shared<XorGate>("User_XOR");
    else if (choice == 5) gate = std::make_shared<NandGate>("User_NAND");
    else {
        std::cout << "[Error] Invalid selection.\n";
        return;
    }

    if (choice == 3) {
        int in0;
        std::cout << "Enter Input (0 or 1): ";
        std::cin >> in0;
        gate->setInput(0, in0 ? HIGH : LOW);
    } else {
        int in0, in1;
        std::cout << "Enter Input 1 (0 or 1): ";
        std::cin >> in0;
        std::cout << "Enter Input 2 (0 or 1): ";
        std::cin >> in1;
        gate->setInput(0, in0 ? HIGH : LOW);
        gate->setInput(1, in1 ? HIGH : LOW);
    }

    gate->evaluate();

    std::cout << "\n[Evaluated Output Pin 0]: " << stateToString(gate->getOutput(0)) << "\n";
    std::cout << "[Component Debug Stream]: " << *gate << "\n";
}

void runInteractiveFullAdder() {
    std::cout << "\n--------------------------------------------\n";
    std::cout << "    1-BIT FULL ADDER (MANUAL USER INPUT)    \n";
    std::cout << "--------------------------------------------\n";

    Circuit fullAdder("FullAdder");

    auto xor1 = std::make_shared<XorGate>("XOR1", 1.8);
    auto xor2 = std::make_shared<XorGate>("XOR2_SUM", 1.8);
    auto and1 = std::make_shared<AndGate>("AND1", 1.2);
    auto and2 = std::make_shared<AndGate>("AND2", 1.2);
    auto or1  = std::make_shared<OrGate>("OR_COUT", 1.5);

    fullAdder.addComponent(xor1);
    fullAdder.addComponent(xor2);
    fullAdder.addComponent(and1);
    fullAdder.addComponent(and2);
    fullAdder.addComponent(or1);

    fullAdder.connect(xor1, 0, xor2, 0);
    fullAdder.connect(xor1, 0, and2, 0);
    fullAdder.connect(and2, 0, or1, 0);
    fullAdder.connect(and1, 0, or1, 1);

    int a, b, cin;
    std::cout << "Enter binary bit A   (0 or 1): ";
    std::cin >> a;
    std::cout << "Enter binary bit B   (0 or 1): ";
    std::cin >> b;
    std::cout << "Enter Carry-In (Cin) (0 or 1): ";
    std::cin >> cin;

    xor1->setInput(0, a ? HIGH : LOW);
    xor1->setInput(1, b ? HIGH : LOW);
    and1->setInput(0, a ? HIGH : LOW);
    and1->setInput(1, b ? HIGH : LOW);

    xor2->setInput(1, cin ? HIGH : LOW);
    and2->setInput(1, cin ? HIGH : LOW);

    fullAdder.propagate();
    fullAdder.propagate();

    std::cout << "\n============================================\n";
    std::cout << "  CALCULATION RESULTS (Dynamic Settling):\n";
    std::cout << "  Inputs : A=" << a << ", B=" << b << ", Cin=" << cin << "\n";
    std::cout << "  SUM    : " << stateToString(xor2->getOutput(0)) << "\n";
    std::cout << "  CARRY  : " << stateToString(or1->getOutput(0)) << "\n";
    std::cout << "  Total Path Latency : " << fullAdder.calculateTotalCriticalPathDelay() << " ns\n";
    std::cout << "============================================\n";
}

void runInteractiveClockDFF() {
    std::cout << "\n--------------------------------------------\n";
    std::cout << "     STEP-BY-STEP D FLIP-FLOP & CLOCK       \n";
    std::cout << "--------------------------------------------\n";

    Circuit seqCircuit("SequentialTimer");

    auto clk = std::make_shared<Clock>("CLK", 10.0);
    auto dff = std::make_shared<DFlipFlop>("DFF1", 2.5);

    seqCircuit.addComponent(clk);
    seqCircuit.addComponent(dff);
    seqCircuit.connect(clk, 0, dff, 1);

    int totalSteps;
    std::cout << "Enter number of clock steps to simulate: ";
    if (!(std::cin >> totalSteps) || totalSteps <= 0) totalSteps = 4;

    std::cout << "\nStep | CLK | D_in | Q | Q_bar | State Action\n";
    std::cout << "--------------------------------------------------\n";

    for (int step = 1; step <= totalSteps; ++step) {
        int dVal;
        std::cout << "Step " << step << " - Enter input D (0 or 1): ";
        std::cin >> dVal;

        dff->setInput(0, dVal ? HIGH : LOW);
        clk->evaluate();
        seqCircuit.propagate();

        std::cout << "   " << step << " |  "
                  << stateToString(clk->getOutput(0)) << "  |   "
                  << dVal << "  | "
                  << stateToString(dff->getOutput(0)) << " |   "
                  << stateToString(dff->getOutput(1)) << "   | "
                  << (clk->getOutput(0) == HIGH ? "Rising Edge -> LATCHED" : "Falling/Low -> HELD")
                  << "\n";
    }
}

// ============================================================================
// 7. MAIN DRIVER
// ============================================================================

int main() {
    int option = 0;
    while (true) {
        std::cout << "\n============================================\n";
        std::cout << "   DIGITAL LOGIC SIMULATOR & TIMING ENGINE  \n";
        std::cout << "============================================\n";
        std::cout << "1. Test Single Gate (Manual Inputs)\n";
        std::cout << "2. Simulate 1-Bit Full Adder (Manual Inputs)\n";
        std::cout << "3. Step-by-Step Clock & D-Flip-Flop\n";
        std::cout << "4. Exit\n";
        std::cout << "Enter your choice (1-4): ";

        if (!(std::cin >> option)) {
            std::cin.clear();
            std::string discard;
            std::cin >> discard;
            continue;
        }

        try {
            if (option == 1) {
                runManualGateTester();
            } else if (option == 2) {
                runInteractiveFullAdder();
            } else if (option == 3) {
                runInteractiveClockDFF();
            } else if (option == 4) {
                std::cout << "\nTerminating simulator. Done.\n";
                break;
            } else {
                std::cout << "\n[Error] Invalid option. Enter a number between 1 and 4.\n";
            }
        } catch (const CircuitException& ex) {
            std::cerr << "\n[Circuit Exception Caught]: " << ex.what() << "\n";
        } catch (const std::exception& ex) {
            std::cerr << "\n[System Exception]: " << ex.what() << "\n";
        }
    }
    return 0;
}
