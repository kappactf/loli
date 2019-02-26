<img height="150" align="left" style="float: left; margin: 0 10px 0 0;" alt="Loli logo" src="https://raw.githubusercontent.com/kappactf/loli/master/loli.png">

## Loli
Embedded programming language 


```rust
fn range(start: Integer, end: *Integer = -1): List[Integer]
{
    var result: List[Integer] = []
    if end == -1: {
        end = start
        start = 0
    }
    for n in start...end:
    {
        result.push(n)
    }
    return result
}
```

## Make everything, it will work everywhere
Loli applications can run in any operation system and architecture. It can be embedded into HTML code and be used to make wonderfull websites. 

Write once. Run everywhere!

## Building from sources

Compiling depends ``make`` and ``CMake`` 3.6 or later

```bash
mkdir ./build               # make build directory
cmake -S ./ -B ./build      # generate make file to build directory
cd build && make            # compile loli
```

Then in the build directory you can find files: ``loli`` & ``libloli.so`` (``loli.exe`` & ``libloli.dll`` on Windows)

