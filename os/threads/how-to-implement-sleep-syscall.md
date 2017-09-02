# How to implement `sleep` system call

- If there are other threads that can be scheduled
  - Schedule that thread
- If no threads can be scheduled
  - Schedule `idle` kernel thread
  - `idle` kernel thread either:
    - executes a `while(1)` loop until an interrupts occurs(busy loop)
    - executes a `hlt` instruction until an interrupts occurs(idle loop)

