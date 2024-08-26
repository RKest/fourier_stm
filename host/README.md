### The host part of the project

#### Setting up

Set the desired baud rate, and disable software flow control
```bash
stty -F /dev/YOUR_DEVICE 115200 -ixon
```
Compile the app
```bash
cmake -S . -B build -D CMAKE_BUILD_TYPE=Release && cmake --build build -j`nproc`
```

#### Running the app

After compiling, first run the app that signals to the board to start sending data
```bash
build/micro_proj > /dev/YOUR_DEVICE
```

Then use the script included to visualize the received data
```bash
./a.sh /dev/YOUR_DEVICE
```
