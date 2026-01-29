Follow the steps below to execute the projects.

1. Setup sandbox environment for external library dependencies (see `Examples Setup` in `DETAILS.md`). 
2. Execute CMake command in `pub` and `sub` directories.  
   `cmake -B build .`
3. Build client and server applications within the `build` directory.
4. Start `delegate_pun_app` first.
5. Start `delegate_sub_app` second.
6. Client and server communicate and output debug data to the console.

