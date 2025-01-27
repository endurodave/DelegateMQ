/// @file
/// @brief Implement a observer pattern using delegates. 

#include "Observer.h"
#include "DelegateLib.h"
#include <iostream>

using namespace DelegateLib;
using namespace std;

namespace Example
{
    // Observer Interface
    class Observer {
    public:
        virtual ~Observer() = default;
        virtual void update(int state) = 0;  // update method called when the state changes
    };

    // Concrete Observer 1
    class ConcreteObserverA : public Observer {
    public:
        void update(int state) override {
            std::cout << "ConcreteObserverA: State updated to " << state << std::endl;
        }
    };

    // Concrete Observer 2
    class ConcreteObserverB : public Observer {
    public:
        void update(int state) override {
            std::cout << "ConcreteObserverB: State updated to " << state << std::endl;
        }
    };

    // Subject Interface
    class Subject {
    public:
        virtual ~Subject() = default;
        virtual void attach(const Delegate<void(int)>& observer) = 0;
        virtual void detach(const Delegate<void(int)>& observer) = 0;
        virtual void notify() = 0;
    };

    // Concrete Subject
    class ConcreteSubject : public Subject {
    private:
        int state;
        MulticastDelegate<void(int)> observers;

    public:
        ConcreteSubject(int initialState) : state(initialState) {}

        // Attach observer
        void attach(const Delegate<void(int)>& observer) override {
            observers += observer;
        }

        // Detach observer
        void detach(const Delegate<void(int)>& observer) override {
            observers -= observer;
        }

        // Notify all observers
        void notify() override {
            observers(state);
        }

        // Set state and notify observers
        void setState(int newState) {
            state = newState;
            notify();  // Notify all observers of the state change
        }

        // Get current state
        int getState() const {
            return state;
        }
    };

    void SubjectFree(int state) {
        std::cout << "ConcreteSubjectFree: State updated to " << state << std::endl;
    }

    auto SubjectLambda = +[](int state) {
        std::cout << "ConcreteSubjectLambda: State updated to " << state << std::endl;
    };

    auto SubjectLambda2 = [](int state) {
        std::cout << "ConcreteSubjectLambda: State updated to " << state << std::endl;
    };

    void ObserverExample()
    {
        // Create concrete observers
        std::shared_ptr<ConcreteObserverA> observerA = std::make_shared<ConcreteObserverA>();
        std::shared_ptr<ConcreteObserverB> observerB = std::make_shared<ConcreteObserverB>();

        // Create a concrete subject
        ConcreteSubject subject(10);  // Initial state is 10

        // Attach observers to the subject. Unlike traditional observer pattern,
        // delegates accept any target function: member, free, or lambda.
        subject.attach(MakeDelegate(observerA, &ConcreteObserverA::update));
        subject.attach(MakeDelegate(observerB, &ConcreteObserverB::update));
        subject.attach(MakeDelegate(SubjectFree));
        subject.attach(MakeDelegate(SubjectLambda));
        subject.attach(MakeDelegate(std::function(SubjectLambda2)));

        // Change the subject's state and notify observers
        std::cout << "Setting state to 20:" << std::endl;
        subject.setState(20);  // All observers will be notified

        // Detach observerB and change state again
        subject.detach(MakeDelegate(observerB, &ConcreteObserverB::update));
        std::cout << "\nSetting state to 30 (ObserverB detached):" << std::endl;
        subject.setState(30);  // observerB not notified
    }
}
