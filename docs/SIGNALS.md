# Signal — Publish / Subscribe

`Signal<Sig>` is the recommended way to implement publish/subscribe within a process. It returns a `ScopedConnection` handle from `Connect()` that automatically disconnects when it goes out of scope — no manual unsubscribe needed.

---

## Table of Contents

- [Basic Usage](#basic-usage)
- [Lambda Slots](#lambda-slots)
- [Mixed Sync and Async Slots](#mixed-sync-and-async-slots)
- [When to use `MulticastDelegateSafe` instead](#when-to-use-multicastdelegatesafe-instead)

---

## Basic Usage

**Publisher** — declare `Signal<>` as a plain class member and call it to emit:

```cpp
class Button
{
public:
    dmq::Signal<void(int buttonId)> OnPressed;  // plain member, no shared_ptr needed

    void Press(int id) { OnPressed(id); }       // emit to all connected slots
};
```

**Subscriber** — connect with `MakeDelegate`, store the `ScopedConnection`:

```cpp
class UI
{
public:
    UI(Button& btn) : m_thread("UIThread")
    {
        m_thread.CreateThread();

        // Connect: callback will run on m_thread context
        m_conn = btn.OnPressed.Connect(
            MakeDelegate(this, &UI::HandlePress, m_thread)
        );
    }
    // No destructor needed — m_conn disconnects automatically when UI is destroyed

private:
    void HandlePress(int buttonId)
    {
        std::cout << "Button " << buttonId << " pressed\n";
    }

    Thread m_thread;
    dmq::ScopedConnection m_conn;  // RAII: disconnects on destruction
};

// Usage
Button btn;
{
    UI ui(btn);
    btn.Press(1);   // UI::HandlePress called on UIThread
}                   // ui destroyed -> m_conn disconnects -> no more callbacks
btn.Press(2);       // safe: no subscribers, nothing happens
```

---

## Lambda Slots

```cpp
dmq::Signal<void(int)> OnData;

// Stateless lambda
dmq::ScopedConnection c1 = OnData.Connect(MakeDelegate([](int v) {
    std::cout << "Got: " << v << "\n";
}));

// Capturing lambda — no std::function wrapper needed
int factor = 3;
dmq::ScopedConnection c2 = OnData.Connect(MakeDelegate(
    [factor](int v) { std::cout << v * factor; }
));

OnData(10);  // both slots called
```

---

## Mixed Sync and Async Slots

Unlike most signal libraries, DelegateMQ lets each subscriber independently choose its execution context. The publisher doesn't need to know:

```cpp
Button btn;

// Subscriber A: synchronous (called on the emitting thread)
dmq::ScopedConnection connA = btn.OnPressed.Connect(
    MakeDelegate([](int id) { std::cout << "Sync: " << id; })
);

// Subscriber B: asynchronous (called on workerThread)
dmq::ScopedConnection connB = btn.OnPressed.Connect(
    MakeDelegate([](int id) { std::cout << "Async: " << id; }, workerThread)
);

btn.Press(1);  // connA called synchronously, connB queued on workerThread
```

---

## When to use `MulticastDelegateSafe` instead

Use `Signal` by default. Reach for `MulticastDelegateSafe` only when you need explicit control over subscription timing.

| | `Signal<Sig>` | `MulticastDelegateSafe<Sig>` |
| --- | --- | --- |
| **Subscription** | `Connect()` → returns `ScopedConnection` | `operator+=` → no return value |
| **Unsubscription** | Automatic when `ScopedConnection` is destroyed | Manual `operator-=` |
| **Lifetime safety** | Safe — disconnects on scope exit, even if Signal is already destroyed | Caller responsible; missed `-=` leaves a dangling subscriber |
| **Mixed sync/async slots** | Yes — each subscriber independently chooses its thread | Yes |
| **Prefer when** | Observer pattern, component events, any long-lived subscription | Subscription lifetime is fully explicit and controlled by the caller |
| **Avoid when** | You need to control the exact moment of disconnect | Subscriber lifetime is hard to predict or tied to complex ownership |

```cpp
// MulticastDelegateSafe — manual subscription management
MulticastDelegateSafe<void(int)> OnData;
OnData += MakeDelegate(&obj, &MyClass::Handle, workerThread);
OnData(42);
OnData -= MakeDelegate(&obj, &MyClass::Handle, workerThread);  // must not forget this
```
