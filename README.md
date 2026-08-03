# 🔄 Multi-Threaded Producer-Consumer Pipeline & Bounded Buffer

## 📌 Overview
This repository contains a solution for coursework in **Operating Systems** at Bar-Ilan University. The project implements a complete **Producer-Consumer pipeline** architecture with thread synchronization, dynamic message dispatching, and thread-safe bounded buffers in C.

---

## 🏗️ Architecture & System Design

The system operates as a multi-stage concurrent pipeline consisting of multiple dedicated threads:

* **Producers (`producer.c`):** Generate structured message items across multiple categories and insert them into assigned bounded buffers[cite: 9].
* **Bounded Buffer (`bounded_buffer.c`):** A thread-safe queue implementation guarded by Mutexes and Semaphores to handle race conditions, producer blocking on full buffers, and consumer blocking on empty buffers[cite: 9].
* **Dispatcher (`dispatcher.c`):** Reads messages from producer buffers via non-blocking/round-robin polling and routes them to category-specific co-editor queues[cite: 9].
* **Co-Editors (`coeditor.c`):** Process incoming categorized messages and pass them forward to the screen manager[cite: 9].
* **Screen Manager (`screen_manager.c`):** Receives fully processed messages and prints the formatted system output safely[cite: 9].

---

## 🛠️ Tech Stack & Concepts
* **Language:** C (POSIX Threads / `pthreads`)[cite: 9]
* **Build System:** Makefile[cite: 9]
* **Concepts:** Concurrency, Producer-Consumer Pattern, Thread Synchronization, Mutexes, Semaphores, Bounded Buffers, Pipeline Architecture, Thread Safety

---

## 🚀 How to Run

1. Clone the repository:
   ```bash
   git clone [https://github.com/fatimas11/producer-consumer-bounded-buffer-os.git](https://github.com/fatimas11/producer-consumer-bounded-buffer-os.git)
