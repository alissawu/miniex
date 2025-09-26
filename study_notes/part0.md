Notes
- CMake: Configure, CMake: Build — steps for compiling and building main.cpp
- main.cpp builds to hello
- To run:
  ./build/hello

---

Questions

# What did the compiler, Make, and CMake each do in this build?
- CMake
  - Parses CMakeLists.txt (targets, settings, compiler/flags)
  - Generates Makefiles and other build system files in build/
  - Does not compile itself, only writes instructions

- Make
  - Reads Makefile + source files (.o files)
  - Decides what needs rebuilding (based on timestamps/dependencies)
  - Invokes tools (compiler, linker) in the correct order

- Compiler
  - Translates each .cpp → .o (object/machine code files)

- Linker (invoked by Make)
  - Combines .o files + libraries → final executable (build/hello)

- OS
  - Runs the executable when I type ./build/hello

Order of operations:
1. Me → provide CMakeLists.txt
2. CMake → generate build system (Makefile, CMakeCache.txt, etc.)
3. Make → decide what to rebuild, call tools in order
4. Compiler → .cpp → .o
5. Linker → .o + libs → executable (build/hello)
6. OS → executes hello

---

# What is the target here, and why did we make it?
- target = the executable hello
- We made it bc it's in machine code to run

---

# Which kit did you select, and why does choosing it explicitly matter?
- Apple Clang
- Choosing explicitly matters bc it locks in a specific compiler/version, aka ensures consistent features and behavior

---

# Where is the executable located, and why isn’t it in the project root?
- Located in build/ (bc we used out-of-source build)
- This keeps the source tree clean; build artifacts stay separate from project source files
