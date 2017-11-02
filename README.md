# ox
An RPC library for C++

## Features
- Header-only (except external libraries)  
  `#include <ox/ox.hpp>`

```cpp
using function_type = void(int, std::function<void(int)>);

// server
ox::server<function_type> server([](auto x, auto f) {
  f(x + 1);
});

// client
ox::client<function_type> client("localhost");

client(1, [&](auto x) {
  std::cout << x << std::endl;
});
```

## Requirements

### Supported Compilers
- Clang (>=3.8.1)

### External Libraries
- Boost C++ Libraries (>= 1.65.1)
- [cereal](https://github.com/USCiLab/cereal)
- [Catch](https://github.com/philsquared/Catch) (for test)

## License
The BSD 3-Clause License (see [LICENSE](LICENSE))
