# VectorDB

**A high-performance, disk-backed vector database engine, built from scratch in C++ for AI embeddings.**

No off-the-shelf database libraries. Every layer is hand-built: memory, storage, indexing, concurrency, search.

***

## Ongoing Project

VectorDB is **under active development**. This repo is currently a **phase-wise dump of my learning, code and experiments**, uploaded as I build.

* Directory structure **changes frequently**
* Expect work-in-progress code and rough edges
* See **"thinking-system'** below for what I'm working on right now

***

## Core Features

* **Custom Memory Management**: hand-written smart pointers (`unique_ptr`-style), RAII, move semantics, zero-leak design verified with Valgrind *(completed)*
* **SmartVector**: custom dynamic array with automatic reallocation and `std::bad_alloc` handling *(completed)*
* **Automated Testing Wall**: GoogleTest suite and Valgrind checks to catch regressions *(completed, extended as new modules land)*
* **Binary Persistence**: dumping and recovering heap data through binary file I/O *(completed)*
* **Memory-Mapped Persistent Storage**: `mmap`-based disk storage for fast reads and writes *(completed)*
* **B+ Tree Indexing**: disk-based index with node splitting and Soft deletion *(in progress)*
* **Concurrency with Optimistic Latch Crabbing**: thread-safe B+ Tree using read/write locks, mutexes and deadlock avoidance *(in progress)*
*  **Vector Math Engine**: optimized dot product and cosine similarity *(planned)*
* **HNSW Graph Search**: approximate nearest-neighbour search over high-dimensional embeddings *(planned)*
* **Embedding Pipeline**: Python + HuggingFace embeddings piped into the C++ engine *(planned)*
* **Network API**: TCP / REST / gRPC layer so web apps can query it *(planned)*
* **Production Packaging**: Docker, GitHub Actions CI, cloud deployment *(planned)*

***

## Ongoing Notes

**Done so far**

* Memory architecture, custom smart pointers, RAII, move semantics
* SmartVector, binary file persistence, vector similarity functions
* GoogleTest suites and Valgrind-verified leak-free code
*  Memory-mapped file storage with `mmap`
* B+ Tree structure on disk: node splitting, deletion.

**Working on now: the persistent storage engine**

* Multithreading: mutexes, read/write locks, optimistic Crabbing, deadlocks, safe concurrent access

**Coming next**

* Embedding generation with HuggingFace
* HNSW graph search
* Network API layer

**Upload policy**

* I push each topic as I finish it, so the repo mirrors my learning order.

***

## Tech Stack

`C++` · `GoogleTest` · `Valgrind` · `POSIX mmap` · `POSIX Threads` · `Python + HuggingFace` · `Docker` · `GitHub Actions`

***

## Repository Structure

> Subject to frequent change.

```
VectorDB/
├── (folders added as I upload)
└── README.md
```

***

## Build and Run

Coming soon...

